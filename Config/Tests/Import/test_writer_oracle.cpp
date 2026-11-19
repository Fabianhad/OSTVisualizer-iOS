// Differential oracle for the import WRITER: IMP-01 (bid number), IMP-05 (typed values), IMP-06 (UID floor + remap),
// IMP-07 (source-database references), IMP-08 (atomic import, unknown attributes), IMP-10 / IMP-11 (master data).
//
// Fixtures/import_writer_oracle.txt was produced by running the REAL Windows code (ImportOperationsMixin.import_ost_data
// + OstImporter._parse_ost_xml/_transform, CPython 3.11.4, read-only, python -B) on a SQLite database with the iOS schema
// for seeded random pre-states and random OST files, the way the Windows tests drive it (tests/helpers/mdb/
// import_export_support.py _SqliteMdbWriter): scratchpad/import-pass1/gen_writer_oracle.py. The Access catalog type names
// that SQLite cannot express are supplied as Access reports them (UID autonumber = COUNTER, the 85 YESNO columns of
// database_creator._TABLE_DDL = YESNO, its 14 DATETIME columns = DATETIME, its 30 IMAGE columns = LONGBINARY). Every scenario
// records whether the import succeeded and every row of every table afterwards; a failed import must leave the database
// exactly as it was (rollback), which the recorded rows prove.
// IMP-20 (the datetime / memo conversion) is part of this oracle with two documented deviations the user decided: a valid
// six part date keeps its raw text (Windows stores a datetime), and memo/IMAGE text is compared as the UTF-8 text of the bytes
// Windows stores (the Windows latin-1 encoding of BidTexts/BidCallOuts.Name is not ported). The raw-graph validation (IMP-09)
// is not part of this oracle.
//
// Fixture lines (UTF-8, LF): DDL <sql> | SCENARIO <n> | SQL <setup sql> | XML <ost xml> | RESULT OK|FAIL | ROW <table>TAB<row> | END
#include "ImportTestSupport.hpp"

#include <algorithm>
#include <string>
#include <vector>

using ost_import::OstRawData;
using ost_itest::ScratchDb;

namespace
{
    struct Scenario
    {
        int number = 0;
        std::vector<std::string> setup;
        std::string xml;
        bool ok = false;
        std::vector<std::string> rows; // "table\trow"
    };

    struct Oracle
    {
        std::vector<std::string> ddl;
        std::vector<Scenario> scenarios;
    };

    bool load_oracle(const std::string &path, Oracle &out)
    {
        std::string content;
        if (!ost_test::read_file(path, content))
            return false;
        size_t pos = 0;
        while (pos < content.size())
        {
            size_t eol = content.find('\n', pos);
            if (eol == std::string::npos)
                eol = content.size();
            std::string line = content.substr(pos, eol - pos);
            pos = eol + 1;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.rfind("DDL ", 0) == 0)
                out.ddl.push_back(line.substr(4));
            else if (line.rfind("SCENARIO ", 0) == 0)
            {
                out.scenarios.emplace_back();
                out.scenarios.back().number = std::stoi(line.substr(9));
            }
            else if (line.rfind("SQL ", 0) == 0)
                out.scenarios.back().setup.push_back(line.substr(4));
            else if (line.rfind("XML ", 0) == 0)
                out.scenarios.back().xml = line.substr(4);
            else if (line.rfind("RESULT ", 0) == 0)
                out.scenarios.back().ok = (line.substr(7) == "OK");
            else if (line.rfind("ROW ", 0) == 0)
                out.scenarios.back().rows.push_back(line.substr(4));
        }
        return true;
    }

    std::vector<std::string> split_lines(const std::string &text)
    {
        std::vector<std::string> lines;
        size_t start = 0;
        while (start <= text.size())
        {
            const size_t eol = text.find('\n', start);
            if (eol == std::string::npos)
            {
                if (start < text.size())
                    lines.push_back(text.substr(start));
                break;
            }
            lines.push_back(text.substr(start, eol - start));
            start = eol + 1;
        }
        return lines;
    }

    // Every row of every table, tables by name, rows by rowid: "table<TAB>rowid: col=quote(value) ...".
    std::vector<std::string> dump_all(const ScratchDb &db)
    {
        std::vector<std::string> out;
        const std::string names = db.rows("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name", 1);
        size_t start = 0;
        while (start <= names.size() && !names.empty())
        {
            const size_t semi = names.find(';', start);
            const std::string table = names.substr(start, semi == std::string::npos ? std::string::npos : semi - start);
            const std::string text = db.dump(table);
            if (text != "<missing>" && !text.empty())
                for (const std::string &row : split_lines(text))
                    out.push_back(table + "\t" + row);
            if (semi == std::string::npos)
                break;
            start = semi + 1;
        }
        return out;
    }
}

TEST(ImportWriterOracle, Imp01_Imp05_Imp06_Imp08_Imp10_Imp11_ImportMatchesTheWindowsWriterOnRandomScenarios)
{
    Oracle oracle;
    REQUIRE(load_oracle(ost_test::fixture_path("import_writer_oracle.txt"), oracle));
    REQUIRE_EQ(oracle.scenarios.size(), 120u);
    REQUIRE(!oracle.ddl.empty());

    std::string ddl_batch = "BEGIN;";
    for (const std::string &statement : oracle.ddl)
        ddl_batch += statement + ";";
    ddl_batch += "COMMIT;";

    size_t imported = 0;
    size_t rolled_back = 0;
    for (const Scenario &scenario : oracle.scenarios)
    {
        SCOPED_CONTEXT("oracle scenario " + std::to_string(scenario.number));
        ScratchDb db("writer_oracle");
        REQUIRE(db.exec(ddl_batch));
        for (const std::string &sql : scenario.setup)
            db.must(sql);

        const OstRawData raw = ost_import::parse_ost_xml(scenario.xml);
        REQUIRE(!raw.empty());
        ost_store::ImportResult result;
        const bool ok = ost_store::import_project(db.path(), raw, &result);
        CHECK_EQ(ok, scenario.ok);
        if (ok)
            ++imported;
        else
            ++rolled_back;

        const std::vector<std::string> actual = dump_all(db);
        CHECK_EQ(actual.size(), scenario.rows.size());
        const size_t common = std::min(actual.size(), scenario.rows.size());
        for (size_t i = 0; i < common; ++i)
        {
            if (actual[i] != scenario.rows[i])
            {
                CHECK_STR_EQ(actual[i], scenario.rows[i]);
                break; // the first difference is enough, the rest is noise
            }
        }
    }
    // the scenarios cover both outcomes
    CHECK(imported > 30);
    CHECK(rolled_back > 15);
}
