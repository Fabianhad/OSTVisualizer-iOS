// Differential test of the condition persistence against the Windows MdbWriter (COND-DEL-01, COND-DUP-01, COND-UPD-01,
// COND-UPD-02, COND-M05 and the update transaction of COND-M03).
//
// Fixtures/condition_oracle.txt was produced by running the Windows condition operations (condition_operations.py
// delete_conditions / update_condition / duplicate_conditions through tests/helpers/mdb/real_write_contract.py
// RealContractOps on SQLite) on random worlds (seed 20261116) and dumping the tables afterwards. Here the same world is
// built, the same call goes through ost_store, and the tables are compared line by line, together with the result.
//
// Fixture format, one record per line:
//   DDL <set> <CREATE TABLE ...>          the schema of the scenarios of <set> (delete | update; duplicate uses update)
//   TABLES <set> a,b,c                    the tables dumped for the delete scenarios
//   SCENARIO <delete|update|duplicate> ... END
//     BID <uid> / COND <uid> / OK <0|1> / NEWUID <uid or empty> / CHANGE <field>=<value> / SQL <insert> / EXP <line>
// An EXP line is "[Table]" or one row: every non-NULL column as "name=value,", rows by UID.
#include "ConditionsTestSupport.hpp"

#include <map>
#include <regex>
#include <sstream>

using ost_ctest::TempDb;
using ost_domain::ConditionChanges;
using ost_domain::ConditionField;

namespace
{
    struct Scenario
    {
        std::string kind;
        std::string bid;
        std::string condition;
        bool ok = false;
        std::string new_uid;
        std::vector<std::pair<std::string, std::string>> changes;
        std::vector<std::string> inserts;
        std::vector<std::string> expected;
    };

    struct Oracle
    {
        std::map<std::string, std::vector<std::string>> ddl;
        std::map<std::string, std::vector<std::string>> tables;
        std::vector<Scenario> scenarios;
    };

    const Oracle &oracle()
    {
        static Oracle data;
        static bool loaded = false;
        if (loaded)
            return data;
        loaded = true;
        std::string text;
        if (!ost_test::read_file(ost_test::fixture_path("condition_oracle.txt"), text))
            return data;
        std::istringstream in(text);
        std::string line;
        Scenario current;
        bool in_scenario = false;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto rest = [&line](std::size_t prefix) { return line.substr(prefix); };
            if (line.rfind("DDL ", 0) == 0)
            {
                const std::string body = rest(4);
                const std::size_t space = body.find(' ');
                data.ddl[body.substr(0, space)].push_back(body.substr(space + 1));
            }
            else if (line.rfind("TABLES ", 0) == 0)
            {
                const std::string body = rest(7);
                const std::size_t space = body.find(' ');
                std::istringstream names(body.substr(space + 1));
                std::string name;
                while (std::getline(names, name, ','))
                    data.tables[body.substr(0, space)].push_back(name);
            }
            else if (line.rfind("SCENARIO ", 0) == 0)
            {
                current = Scenario();
                current.kind = rest(9);
                in_scenario = true;
            }
            else if (!in_scenario)
            {
                continue;
            }
            else if (line.rfind("BID ", 0) == 0)
                current.bid = rest(4);
            else if (line.rfind("COND ", 0) == 0)
                current.condition = rest(5);
            else if (line.rfind("OK ", 0) == 0)
                current.ok = rest(3) == "1";
            else if (line.rfind("NEWUID", 0) == 0)
                current.new_uid = line.size() > 7 ? rest(7) : "";
            else if (line.rfind("CHANGE ", 0) == 0)
            {
                const std::string body = rest(7);
                const std::size_t eq = body.find('=');
                current.changes.push_back({body.substr(0, eq), body.substr(eq + 1)});
            }
            else if (line.rfind("SQL ", 0) == 0)
                current.inserts.push_back(rest(4));
            else if (line.rfind("EXP ", 0) == 0)
                current.expected.push_back(rest(4));
            else if (line == "END")
            {
                data.scenarios.push_back(current);
                in_scenario = false;
            }
        }
        return data;
    }

    // The tables as the fixture lists them: "[Table]" then one line per row (non-NULL columns "name=value,"), by UID.
    std::vector<std::string> dump_tables(const TempDb &db, const std::vector<std::string> &tables,
                                         const std::string &skip_column = "")
    {
        std::vector<std::string> lines;
        for (const std::string &table : tables)
        {
            lines.push_back("[" + table + "]");
            std::vector<std::string> columns;
            {
                ost_db::OstDatabase conn;
                if (!conn.open(db.path()))
                    return {"<cannot open>"};
                ost_db::Statement info = conn.prepare("SELECT name FROM pragma_table_info(?)");
                info.bind_text(1, table);
                while (info.step())
                    if (info.column_text(0) != skip_column)
                        columns.push_back(info.column_text(0));
            }
            std::string expr = "''";
            for (const std::string &column : columns)
                expr += " || CASE WHEN [" + column + "] IS NULL THEN '' ELSE '" + column + "=' || CAST([" + column +
                        "] AS TEXT) || ',' END";
            ost_db::OstDatabase conn;
            if (!conn.open(db.path()))
                return {"<cannot open>"};
            ost_db::Statement rows = conn.prepare("SELECT " + expr + " FROM [" + table + "] ORDER BY UID, rowid");
            if (!rows.valid())
                return {"<dump failed: " + table + ">"};
            while (rows.step())
                lines.push_back(rows.column_text(0));
        }
        return lines;
    }

    // First differing line of two dumps, for a readable failure.
    std::string first_difference(const std::vector<std::string> &got, const std::vector<std::string> &want)
    {
        for (std::size_t i = 0; i < std::max(got.size(), want.size()); ++i)
        {
            const std::string g = i < got.size() ? got[i] : "<missing>";
            const std::string w = i < want.size() ? want[i] : "<missing>";
            if (g != w)
                return "line " + std::to_string(i) + "\n  got:      " + g + "\n  expected: " + w;
        }
        return "";
    }

    void build_world(const TempDb &db, const std::string &set, const Scenario &s)
    {
        for (const std::string &ddl : oracle().ddl.at(set))
            db.must(ddl);
        for (const std::string &sql : s.inserts)
            db.must(sql);
    }

    void check_tables(const TempDb &db, const std::vector<std::string> &tables, const Scenario &s,
                      const std::string &skip_column = "")
    {
        const std::vector<std::string> got = dump_tables(db, tables, skip_column);
        if (got != s.expected)
            ost_test::report_failure(__FILE__, __LINE__, "tables differ from the Windows result: " + first_difference(got, s.expected));
    }

    bool to_bool(const std::string &v) { return v == "true"; }

    // One field of a Windows UpdateConditionDto as a ConditionChanges entry.
    bool apply_change(ConditionChanges &changes, const std::string &field, const std::string &value)
    {
        ost_domain::Condition &c = changes.values;
        const int number = value.empty() || value == "true" || value == "false" ? 0 : static_cast<int>(std::strtol(value.c_str(), nullptr, 10));
        const double real = value.empty() ? 0.0 : std::strtod(value.c_str(), nullptr);
        auto mark = [&changes](ConditionField f) { changes.mark(f); };
        if (field == "name") { c.name = value; mark(ConditionField::Name); }
        else if (field == "condition_type") { c.condition_type = static_cast<ost_domain::ConditionType>(number); mark(ConditionField::ConditionType); }
        else if (field == "backout") { c.backout = to_bool(value); mark(ConditionField::Backout); }
        else if (field == "ref_no") { c.ref_no = number; mark(ConditionField::RefNo); }
        else if (field == "cdn_type_uid") { c.cdn_type_uid = value; mark(ConditionField::CdnTypeUid); }
        else if (field == "layer_uid") { c.layer_uid = value; mark(ConditionField::LayerUid); }
        else if (field == "folder_uid") { c.condition_folder_uid = value; mark(ConditionField::FolderUid); }
        else if (field == "height") { c.height = real; mark(ConditionField::Height); }
        else if (field == "width") { c.width = real; mark(ConditionField::Width); }
        else if (field == "depth") { c.depth = real; mark(ConditionField::Depth); }
        else if (field == "thickness") { c.thickness = real; mark(ConditionField::Thickness); }
        else if (field == "rise") { c.rise = real; mark(ConditionField::Rise); }
        else if (field == "run") { c.run = real; mark(ConditionField::Run); }
        else if (field == "display_size") { c.display_size = real; mark(ConditionField::DisplaySize); }
        else if (field == "shape") { c.shape = static_cast<ost_domain::ShapeId>(number); mark(ConditionField::Shape); }
        else if (field == "pattern") { c.pattern = static_cast<ost_domain::Pattern>(number); mark(ConditionField::Pattern); }
        else if (field == "spacing") { c.spacing = real; mark(ConditionField::Spacing); }
        else if (field == "color_fill") { c.color_fill = number; mark(ConditionField::ColorFill); }
        else if (field == "uom1") { c.uom1 = number; mark(ConditionField::Uom1); }
        else if (field == "uom2") { c.uom2 = number; mark(ConditionField::Uom2); }
        else if (field == "uom3") { c.uom3 = number; mark(ConditionField::Uom3); }
        else if (field == "calc_type1") { c.calc_type1 = number; mark(ConditionField::CalcType1); }
        else if (field == "calc_type2") { c.calc_type2 = number; mark(ConditionField::CalcType2); }
        else if (field == "calc_type3") { c.calc_type3 = number; mark(ConditionField::CalcType3); }
        else if (field == "notes") { c.notes = value; mark(ConditionField::Notes); }
        else if (field == "drop_run") { c.drop_run = to_bool(value); mark(ConditionField::DropRun); }
        else if (field == "drop_value") { c.drop_value = real; mark(ConditionField::DropValue); }
        else if (field == "round_quantity") { c.round_quantity = to_bool(value); mark(ConditionField::RoundQuantity); }
        else if (field == "round_up") { c.round_up = real; mark(ConditionField::RoundUp); }
        else if (field == "trim") { c.trim = to_bool(value); mark(ConditionField::Trim); }
        else if (field == "is_curved_segment") { c.is_curved_segment = to_bool(value); mark(ConditionField::IsCurvedSegment); }
        else if (field == "grid") { c.grid = to_bool(value); mark(ConditionField::Grid); }
        else if (field == "grid_size1") { c.grid_size1 = real; mark(ConditionField::GridSize1); }
        else if (field == "grid_size2") { c.grid_size2 = real; mark(ConditionField::GridSize2); }
        else if (field == "gap") { c.gap = real; mark(ConditionField::Gap); }
        else if (field == "display_dimension") { c.display_dimension = to_bool(value); mark(ConditionField::DisplayDimension); }
        else if (field == "display_name") { c.display_name = to_bool(value); mark(ConditionField::DisplayName); }
        else if (field == "display_grid_while_drawing") { c.display_grid_while_drawing = to_bool(value); mark(ConditionField::DisplayGridWhileDrawing); }
        else return false;
        return true;
    }

    std::size_t count_kind(const std::string &kind)
    {
        std::size_t n = 0;
        for (const Scenario &s : oracle().scenarios)
            n += s.kind == kind ? 1 : 0;
        return n;
    }
}

TEST(ConditionOracle, TheFixtureLoads)
{
    REQUIRE(!oracle().scenarios.empty());
    CHECK_EQ(count_kind("delete"), size_t(50));
    CHECK_EQ(count_kind("update"), size_t(110));
    CHECK_EQ(count_kind("duplicate"), size_t(30));
    CHECK(oracle().ddl.count("delete") == 1 && oracle().ddl.count("update") == 1);
    CHECK(oracle().tables.count("delete") == 1);
}

// 50 random worlds (conditions of three bids, takeoffs with random parent / typical-group references, annotations in both
// directions, labor activities, percents, type group views, totals, condition users and styles); a third of the calls
// name the wrong bid or an unknown condition.
TEST(ConditionOracle, DeleteMatchesTheWindowsCascade)
{
    int index = 0;
    for (const Scenario &s : oracle().scenarios)
    {
        if (s.kind != "delete")
            continue;
        SCOPED_CONTEXT("delete scenario " + std::to_string(index++) + " (bid " + s.bid + ", condition " + s.condition + ")");
        TempDb db("oracle_delete", false);
        build_world(db, "delete", s);
        const bool ok = ost_store::delete_condition(db.path(), s.bid, s.condition);
        CHECK_EQ(ok, s.ok);
        check_tables(db, oracle().tables.at("delete"), s);
    }
}

// 110 random update calls: 1-6 random fields with random values (cleared and foreign layers / folders, taken reference
// numbers, booleans, floats, notes) on a world with conditions of two bids; a few name the wrong bid or an unknown
// condition. Same result and same tables as Windows.
TEST(ConditionOracle, UpdateMatchesTheWindowsUpdate)
{
    int index = 0;
    for (const Scenario &s : oracle().scenarios)
    {
        if (s.kind != "update")
            continue;
        SCOPED_CONTEXT("update scenario " + std::to_string(index++) + " (bid " + s.bid + ", condition " + s.condition + ")");
        ConditionChanges changes;
        for (const auto &change : s.changes)
            REQUIRE(apply_change(changes, change.first, change.second));
        TempDb db("oracle_update", false);
        build_world(db, "update", s);
        const bool ok = ost_store::update_condition(db.path(), s.bid, s.condition, changes);
        CHECK_EQ(ok, s.ok);
        check_tables(db, {"BidConditions"}, s);
    }
}

// 30 random duplicate calls: the copy has the Windows uid, reference number and every other column except the GUID (not
// compared: random), a Windows-format GUID that differs from the source's; refused calls change nothing.
TEST(ConditionOracle, DuplicateMatchesTheWindowsDuplicate)
{
    static const std::regex guid_pattern("^\\{[0-9A-F]{8}(-[0-9A-F]{4}){3}-[0-9A-F]{12}\\}$");
    int index = 0;
    for (const Scenario &s : oracle().scenarios)
    {
        if (s.kind != "duplicate")
            continue;
        SCOPED_CONTEXT("duplicate scenario " + std::to_string(index++) + " (bid " + s.bid + ", condition " + s.condition + ")");
        TempDb db("oracle_duplicate", false);
        build_world(db, "update", s);
        const std::string uid = ost_store::duplicate_condition(db.path(), s.bid, s.condition);
        CHECK_EQ(uid, s.new_uid);
        check_tables(db, {"BidConditions"}, s, "GUID");
        if (!uid.empty())
        {
            const std::string guid = db.text("SELECT GUID FROM BidConditions WHERE UID = " + uid);
            CHECK(std::regex_match(guid, guid_pattern));
            CHECK_NE(guid, db.text("SELECT GUID FROM BidConditions WHERE UID = " + s.condition));
        }
    }
}
