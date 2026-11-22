// IMP-09: the validation of a parsed OST file before anything is written (Core/Import/RawBidIntegrity.hpp).
// Windows oracle: ost_visualizer/infrastructure/mdb/raw_bid_integrity.py and ost_importer.py (_validated_raw_data,
// _discard_stale_named_views, _validate_page_references); tests/infrastructure/mdb/importers/test_ost_importer.py
// (test_ost_import_skips_orphaned_takeoffs_and_cascading_children, test_ost_import_rejects_duplicate_takeoff_uid_before_remapping,
// test_ost_import_rejects_duplicate_page_uid_before_backend_remapping, test_ost_import_rejects_malformed_bid_owned_uid_before_backend_remapping,
// test_ost_import_rejects_takeoff_parent_cycle, test_ost_import_rejects_area_parent_cycle_before_writer,
// test_ost_import_rejects_takeoff_missing_required_condition, test_ost_import_clears_missing_annotation_takeoff_attachments,
// test_ost_import_clears_missing_bid_settings_selected_page, test_ost_import_rejects_non_numeric_selected_page_uid,
// test_ost_import_drops_only_stale_named_views_and_their_hotlinks, test_ost_import_rejects_named_views_and_hotlinks_for_missing_pages,
// test_ost_import_rejects_multiple_bid_settings_rows) and tests/infrastructure/mdb/test_raw_bid_integrity.py.
//
// The fixture Fixtures/import_validation_oracle.txt holds 219 raw projects (the item's test plan, the parity test, random consistent
// graphs with random corruption and random chaos) with the outcome of the REAL Windows _validated_raw_data: its ValueError text, its
// warning / error log records and the tables it changed. Generated read-only by scratchpad objc-U5-import-browser/gen_validation_oracle.py
// (python -B, Windows HEAD 6d4786f).
#include "Import/RawBidIntegrity.hpp"
#include "Import/TextRules.hpp"
#include "OspTestSupport.hpp"
#include "TestHarness.hpp"

#include <algorithm>
#include <string>
#include <vector>

using ost_import::IntegrityIssue;
using ost_import::OstRawData;
using ost_import::Row;
using ost_import::RowList;
using ost_osptest::LogCapture;

namespace
{
    std::string decode(const std::string &text)
    {
        std::string out;
        for (size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '%' && i + 2 < text.size())
            {
                out += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16));
                i += 2;
            }
            else
                out += text[i];
        }
        return out;
    }

    Row parse_row(const std::string &line, size_t from)
    {
        Row row;
        size_t pos = from;
        while (pos < line.size())
        {
            size_t end = line.find(' ', pos);
            if (end == std::string::npos)
                end = line.size();
            const std::string token = line.substr(pos, end - pos);
            pos = end + 1;
            if (token.empty())
                continue;
            const size_t eq = token.find('=');
            row[token.substr(0, eq)] = decode(token.substr(eq + 1));
        }
        return row;
    }

    std::string row_text(const Row &row)
    {
        std::vector<std::string> keys;
        for (const auto &entry : row)
            keys.push_back(entry.first);
        std::sort(keys.begin(), keys.end());
        std::string out;
        for (const std::string &key : keys)
            out += (out.empty() ? "" : " ") + key + "=" + row.at(key);
        return out;
    }

    std::string rows_text(const RowList &rows)
    {
        std::string out;
        for (const Row &row : rows)
            out += "[" + row_text(row) + "]";
        return out;
    }

    struct TableRows
    {
        char kind;
        std::string table;
        RowList rows;
    };

    struct Scenario
    {
        int index = 0;
        Row bid;
        std::vector<TableRows> tables;
        bool ok = false;
        std::string error;
        std::vector<std::pair<std::string, std::string>> logs;
        std::vector<TableRows> out_tables;
        Row out_bid;
    };

    std::vector<Scenario> load_scenarios()
    {
        std::string text;
        if (!ost_test::read_file(ost_test::fixture_path("import_validation_oracle.txt"), text))
            FAIL_TEST("cannot read the validation oracle fixture");
        std::vector<Scenario> scenarios;
        size_t pos = 0;
        bool in_output = false;
        while (pos < text.size())
        {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos)
                end = text.size();
            const std::string line = text.substr(pos, end - pos);
            pos = end + 1;
            if (line.empty() || line[0] == '#')
                continue;
            if (line.compare(0, 9, "SCENARIO ") == 0)
            {
                scenarios.emplace_back();
                scenarios.back().index = std::stoi(line.substr(9));
                in_output = false;
                continue;
            }
            Scenario &s = scenarios.back();
            if (line.compare(0, 4, "BID ") == 0)
                s.bid = parse_row(line, 4);
            else if (line.compare(0, 6, "TABLE ") == 0)
            {
                s.tables.push_back({line[6], line.substr(8), {}});
            }
            else if (line.compare(0, 4, "ROW ") == 0)
                s.tables.back().rows.push_back(parse_row(line, 4));
            else if (line.compare(0, 7, "RESULT ") == 0)
            {
                const std::string rest = line.substr(7);
                s.ok = rest.compare(0, 2, "OK") == 0;
                const size_t space = rest.find(' ');
                s.error = space == std::string::npos ? std::string() : decode(rest.substr(space + 1));
            }
            else if (line.compare(0, 4, "LOG ") == 0)
            {
                const size_t space = line.find(' ', 4);
                s.logs.emplace_back(line.substr(4, space - 4), decode(line.substr(space + 1)));
            }
            else if (line.compare(0, 9, "OUTTABLE ") == 0)
            {
                in_output = true;
                s.out_tables.push_back({line[9], line.substr(11), {}});
            }
            else if (line.compare(0, 7, "OUTROW ") == 0)
                s.out_tables.back().rows.push_back(parse_row(line, 7));
            else if (line.compare(0, 7, "OUTBID ") == 0)
                s.out_bid = parse_row(line, 7);
            else if (line == "END")
                continue;
            else if (line == "BID")
                continue;
            (void)in_output;
        }
        return scenarios;
    }

    OstRawData build_raw(const Scenario &s)
    {
        OstRawData raw;
        raw.bid_row = s.bid;
        raw.has_bid = !s.bid.empty();
        for (const TableRows &t : s.tables)
        {
            ost_import::TableMap &target = t.kind == 'B' ? raw.bid_tables : (t.kind == 'P' ? raw.page_tables : raw.global_tables);
            ost_import::RowList &rows = target[t.table];
            rows.insert(rows.end(), t.rows.begin(), t.rows.end());
        }
        return raw;
    }

    // The tables whose rows differ from the input, in table order (B, P, G; insertion order inside), as the generator lists them.
    std::vector<TableRows> changed_tables(const Scenario &s, const OstRawData &raw)
    {
        std::vector<TableRows> out;
        struct Source
        {
            char kind;
            const ost_import::TableMap *map;
        };
        const Source sources[] = {{'B', &raw.bid_tables}, {'P', &raw.page_tables}, {'G', &raw.global_tables}};
        for (const Source &source : sources)
        {
            for (const auto &entry : *source.map)
            {
                RowList input;
                for (const TableRows &t : s.tables)
                    if (t.kind == source.kind && t.table == entry.first)
                        input.insert(input.end(), t.rows.begin(), t.rows.end());
                if (rows_text(entry.second) == rows_text(input))
                    continue;
                out.push_back({source.kind, entry.first, entry.second});
            }
        }
        return out;
    }

    OstRawData raw_with(const std::vector<std::pair<std::pair<char, std::string>, RowList>> &tables, const Row &bid = Row{{"UID", "1"}})
    {
        OstRawData raw;
        raw.bid_row = bid;
        raw.has_bid = !bid.empty();
        for (const auto &t : tables)
        {
            ost_import::TableMap &target = t.first.first == 'B' ? raw.bid_tables : (t.first.first == 'P' ? raw.page_tables : raw.global_tables);
            target[t.first.second] = t.second;
        }
        return raw;
    }

    std::string join_logs(const LogCapture &log)
    {
        std::string out;
        for (const auto &record : log.records)
            out += std::string(record.level == ost_log::Level::Error ? "ERROR" : "WARNING") + ": " + record.message + "\n";
        return out;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The differential test against the real Windows validation
// ---------------------------------------------------------------------------------------------------------------

TEST(Imp09_Oracle, ValidationMatchesTheWindowsImporterOnGeneratedProjects)
{
    const std::vector<Scenario> scenarios = load_scenarios();
    REQUIRE(scenarios.size() >= 200);
    size_t accepted = 0;
    size_t rejected = 0;
    for (const Scenario &s : scenarios)
    {
        SCOPED_CONTEXT("scenario " + std::to_string(s.index));
        OstRawData raw = build_raw(s);
        LogCapture log;
        std::string error;
        const bool ok = ost_import::validate_raw_data(raw, error);
        CHECK_EQ(ok, s.ok);
        CHECK_STR_EQ(error, s.error);
        ok ? ++accepted : ++rejected;

        REQUIRE_EQ(log.records.size(), s.logs.size());
        for (size_t i = 0; i < s.logs.size(); ++i)
        {
            CHECK_STR_EQ(log.records[i].level == ost_log::Level::Error ? "ERROR" : "WARNING", s.logs[i].first);
            CHECK_STR_EQ(log.records[i].message, s.logs[i].second);
        }

        const std::vector<TableRows> changed = changed_tables(s, raw);
        REQUIRE_EQ(changed.size(), s.out_tables.size());
        for (size_t i = 0; i < changed.size(); ++i)
        {
            CHECK_EQ(changed[i].kind, s.out_tables[i].kind);
            CHECK_STR_EQ(changed[i].table, s.out_tables[i].table);
            CHECK_STR_EQ(rows_text(changed[i].rows), rows_text(s.out_tables[i].rows));
        }
        CHECK_STR_EQ(row_text(raw.bid_row), row_text(s.out_bid));
    }
    // the fixture exercises both outcomes
    CHECK(accepted >= 40);
    CHECK(rejected >= 100);
}

// ---------------------------------------------------------------------------------------------------------------
// The item's test plan (results of the Windows importer on <Bid UID=1> with one BidCondition UID=10, pages as noted)
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    const Row COND = {{"UID", "10"}, {"BidUID", "1"}};
    const Row PAGE20 = {{"UID", "20"}, {"BidUID", "1"}};

    Row takeoff(const char *uid, const char *page, const char *condition, const char *parent = nullptr)
    {
        Row row = {{"UID", uid}, {"BidUID", "1"}, {"BidPageUID", page}, {"BidConditionUID", condition}};
        if (parent)
            row["ParentUID"] = parent;
        return row;
    }

    // Runs the validation; returns "<ok|error>" and the joined log text through `logs`.
    std::string validate(OstRawData &raw, std::string *logs = nullptr)
    {
        LogCapture log;
        std::string error;
        const bool ok = ost_import::validate_raw_data(raw, error);
        if (logs)
            *logs = join_logs(log);
        return ok ? std::string("accepted") : error;
    }
}

// Port of test_ost_import_rejects_duplicate_page_uid_before_backend_remapping: two BidPage UID=20.
TEST(Imp09_Plan, TwoPagesWithTheSameUidAreRejected)
{
    OstRawData raw = raw_with({{{'B', "BidConditions"}, {COND}}, {{'B', "BidPages"}, {PAGE20, PAGE20}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "The imported project contains invalid references.");
    CHECK_STR_EQ(logs, "ERROR: Rejecting OST import because it contains invalid database references: BidPages.UID=20 occurs 2 times\n");
}

// Port of test_ost_import_rejects_multiple_bid_settings_rows: 'BidSettings has 2 rows for Bids.UID=1; expected at most 1'.
TEST(Imp09_Plan, TwoBidSettingsRowsAreRejected)
{
    OstRawData raw = raw_with({{{'B', "BidSettings"}, {Row{{"UID", "30"}, {"BidUID", "1"}}, Row{{"UID", "31"}, {"BidUID", "1"}}}},
                               {{'B', "BidConditions"}, {COND}},
                               {{'B', "BidPages"}, {PAGE20}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "The imported project contains invalid references.");
    CHECK_STR_EQ(logs, "ERROR: Rejecting OST import because it contains invalid database references: BidSettings has 2 rows for "
                       "Bids.UID=1; expected at most 1\n");
}

// Port of test_ost_import_rejects_takeoff_missing_required_condition: takeoff 40 with BidConditionUID=77.
TEST(Imp09_Plan, ATakeoffOfAMissingConditionIsRejected)
{
    OstRawData raw = raw_with({{{'B', "BidConditions"}, {COND}}, {{'B', "BidPages"}, {PAGE20}}, {{'P', "BidTakeoffs"}, {takeoff("40", "20", "77")}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "The imported project contains invalid references.");
    CHECK_STR_EQ(logs, "ERROR: Rejecting OST import because it contains invalid database references: BidTakeoffs.UID=40 "
                       "BidConditionUID=77 missing BidConditions.UID\n");
}

// Port of test_ost_import_rejects_takeoff_parent_cycle: takeoffs 40 and 41 with parents 41 and 40 (fatal before the project check).
TEST(Imp09_Plan, ATakeoffParentCycleIsFatal)
{
    OstRawData raw = raw_with({{{'B', "BidConditions"}, {COND}},
                               {{'B', "BidPages"}, {PAGE20}},
                               {{'P', "BidTakeoffs"}, {takeoff("40", "20", "10", "41"), takeoff("41", "20", "10", "40")}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "The imported takeoff graph is invalid: BidTakeoffs.UID=40 participates in a ParentUID cycle; "
                                       "BidTakeoffs.UID=41 participates in a ParentUID cycle");
    CHECK(logs.empty());
}

// Port of test_ost_import_rejects_duplicate_takeoff_uid_before_remapping.
TEST(Imp09_Plan, ADuplicateTakeoffUidIsFatal)
{
    OstRawData raw = raw_with({{{'B', "BidConditions"}, {COND}},
                               {{'B', "BidPages"}, {PAGE20}},
                               {{'P', "BidTakeoffs"}, {takeoff("40", "20", "10"), takeoff("40", "20", "10")}}});
    CHECK_STR_EQ(validate(raw), "The imported takeoff graph is invalid: BidTakeoffs.UID=40 occurs 2 times");
}

// Port of test_ost_import_rejects_area_parent_cycle_before_writer.
TEST(Imp09_Plan, AnAreaParentCycleIsRejected)
{
    OstRawData raw = raw_with({{{'B', "BidAreas"},
                                {Row{{"UID", "5"}, {"BidUID", "1"}, {"ParentUID", "6"}}, Row{{"UID", "6"}, {"BidUID", "1"}, {"ParentUID", "5"}}}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "The imported project contains invalid references.");
    CHECK_STR_EQ(logs, "ERROR: Rejecting OST import because it contains invalid database references: BidAreas.UID=5 participates in a "
                       "ParentUID cycle; BidAreas.UID=6 participates in a ParentUID cycle\n");
}

// A condition whose layer / bid is missing, a malformed condition UID and a takeoff whose parent lies on another page (plan list).
TEST(Imp09_Plan, MissingLayerBidMalformedUidAndAParentOnAnotherPage)
{
    {
        OstRawData raw = raw_with({{{'B', "BidLayers"}, {Row{{"UID", "50"}, {"BidUID", "1"}}}},
                                   {{'B', "BidConditions"}, {Row{{"UID", "10"}, {"BidUID", "1"}, {"BidLayerUID", "55"}}}}});
        std::string logs;
        CHECK_STR_EQ(validate(raw, &logs), "The imported project contains invalid references.");
        CHECK_STR_EQ(logs, "ERROR: Rejecting OST import because it contains invalid database references: BidConditions.UID=10 "
                           "BidLayerUID=55 missing BidLayers.UID\n");
    }
    {
        OstRawData raw = raw_with({{{'B', "BidConditions"}, {Row{{"UID", "10"}, {"BidUID", "9"}}}}});
        std::string logs;
        CHECK_STR_EQ(validate(raw, &logs), "The imported project contains invalid references.");
        CHECK_STR_EQ(logs, "ERROR: Rejecting OST import because it contains invalid database references: BidConditions.UID=10 BidUID=9 "
                           "missing Bids.UID\n");
    }
    {
        OstRawData raw = raw_with({{{'B', "BidConditions"}, {Row{{"UID", "abc"}, {"BidUID", "1"}}}}});
        std::string logs;
        CHECK_STR_EQ(validate(raw, &logs), "The imported project contains invalid references.");
        CHECK_STR_EQ(logs, "ERROR: Rejecting OST import because it contains invalid database references: BidConditions.UID=abc has "
                           "malformed UID=abc\n");
    }
    {
        OstRawData raw = raw_with({{{'B', "BidConditions"}, {COND}},
                                   {{'B', "BidPages"}, {PAGE20, Row{{"UID", "21"}, {"BidUID", "1"}}}},
                                   {{'P', "BidTakeoffs"}, {takeoff("40", "20", "10"), takeoff("41", "21", "10", "40")}}});
        std::string logs;
        CHECK_STR_EQ(validate(raw, &logs), "The imported project contains invalid references.");
        CHECK_STR_EQ(logs, "ERROR: Rejecting OST import because it contains invalid database references: BidTakeoffs.UID=41 has a parent "
                           "on another Page\n");
    }
}

// Port of test_ost_import_clears_missing_bid_settings_selected_page: BidPageSelectedUID=999 becomes '0'.
TEST(Imp09_Plan, AMissingSelectedPageIsCleared)
{
    OstRawData raw = raw_with({{{'B', "BidSettings"}, {Row{{"UID", "30"}, {"BidUID", "1"}, {"BidPageSelectedUID", "999"}}}},
                               {{'B', "BidConditions"}, {COND}},
                               {{'B', "BidPages"}, {PAGE20}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "accepted");
    CHECK_STR_EQ(raw.bid_tables.find("BidSettings")->second[0]["BidPageSelectedUID"], "0");
    CHECK_STR_EQ(logs, "WARNING: Cleared 1 missing selected-page reference(s) during OST import: BidSettings.UID=30 "
                       "BidPageSelectedUID=999 missing BidPages.UID\n");
}

// Port of test_ost_import_rejects_non_numeric_selected_page_uid: 'not-a-uid' is not cleared, the import is rejected.
TEST(Imp09_Plan, ANonNumericSelectedPageIsRejected)
{
    OstRawData raw = raw_with({{{'B', "BidSettings"}, {Row{{"UID", "30"}, {"BidUID", "1"}, {"BidPageSelectedUID", "not-a-uid"}}}},
                               {{'B', "BidConditions"}, {COND}},
                               {{'B', "BidPages"}, {PAGE20}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "The imported project contains invalid references.");
    CHECK_STR_EQ(logs, "ERROR: Rejecting OST import because it contains invalid database references: BidSettings.UID=30 "
                       "BidPageSelectedUID=not-a-uid missing BidPages.UID\n");
}

// Port of test_ost_import_clears_missing_annotation_takeoff_attachments: BidTakeoffFromUID=999 is cleared to '0'.
TEST(Imp09_Plan, AMissingAnnotationTakeoffIsCleared)
{
    OstRawData raw = raw_with({{{'B', "BidConditions"}, {COND}},
                               {{'B', "BidPages"}, {PAGE20}},
                               {{'P', "BidTakeoffs"}, {takeoff("40", "20", "10")}},
                               {{'P', "BidDimensions"},
                                {Row{{"UID", "60"}, {"BidUID", "1"}, {"BidTakeoffFromUID", "999"}, {"BidTakeoffToUID", "40"}}}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "accepted");
    const Row &dimension = raw.page_tables.find("BidDimensions")->second[0];
    CHECK_STR_EQ(dimension.at("BidTakeoffFromUID"), "0");
    CHECK_STR_EQ(dimension.at("BidTakeoffToUID"), "40");
    CHECK_STR_EQ(logs, "WARNING: Cleared 1 missing annotation takeoff attachment reference(s) during OST import: BidDimensions.UID=60 "
                       "BidTakeoffFromUID=999 missing BidTakeoffs.UID\n");
}

// Port of test_ost_import_drops_only_stale_named_views_and_their_hotlinks (view 60 on missing page 999 + link 70 dropped, view 61 and link 71 stay).
TEST(Imp09_Plan, OnlyStaleNamedViewsAndTheirHotLinksAreDropped)
{
    OstRawData raw = raw_with(
        {{{'B', "BidConditions"}, {COND}},
         {{'B', "BidPages"}, {PAGE20}},
         {{'B', "BidNamedViews"},
          {Row{{"UID", "60"}, {"BidUID", "1"}, {"BidPageUID", "999"}}, Row{{"UID", "61"}, {"BidUID", "1"}, {"BidPageUID", "20"}}}},
         {{'B', "BidHotLinks"},
          {Row{{"UID", "70"}, {"BidUID", "1"}, {"BidPageViewUID", "60"}}, Row{{"UID", "71"}, {"BidUID", "1"}, {"BidPageViewUID", "61"}}}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "accepted");
    CHECK_STR_EQ(rows_text(raw.bid_tables.find("BidNamedViews")->second), "[BidPageUID=20 BidUID=1 UID=61]");
    CHECK_STR_EQ(rows_text(raw.bid_tables.find("BidHotLinks")->second), "[BidPageViewUID=61 BidUID=1 UID=71]");
    CHECK_STR_EQ(logs, "WARNING: Discarded 1 stale Named View(s) and 1 dependent Hot Link(s) during OST import for Bid 1; absent Page "
                       "references: BidNamedViews.UID=60 BidPageUID=999 missing BidPages.UID\n");
}

// Port of test_ost_import_rejects_named_views_and_hotlinks_for_missing_pages: a hot link that points at a missing view makes the project
// invalid in another way, so nothing is discarded and the whole import is rejected.
TEST(Imp09_Plan, NothingIsDiscardedWhenSomethingElseIsInvalid)
{
    OstRawData raw = raw_with({{{'B', "BidConditions"}, {COND}},
                               {{'B', "BidPages"}, {PAGE20}},
                               {{'B', "BidNamedViews"}, {Row{{"UID", "60"}, {"BidUID", "1"}, {"BidPageUID", "999"}}}},
                               {{'B', "BidHotLinks"}, {Row{{"UID", "70"}, {"BidUID", "1"}, {"BidPageViewUID", "999"}}}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "The imported project contains invalid references.");
    CHECK_STR_EQ(logs, "ERROR: Rejecting OST import because it contains invalid database references: BidNamedViews.UID=60 BidPageUID=999 "
                       "missing BidPages.UID; BidHotLinks.UID=70 BidPageViewUID=999 missing BidNamedViews.UID\n");
    CHECK_EQ(raw.bid_tables.find("BidNamedViews")->second.size(), 1u);
}

// Port of test_ost_import_skips_orphaned_takeoffs_and_cascading_children and the parity test (test_backend_graph_parity): Child(31, parent
// 30), Parent(30), Orphan(32, parent 999), Descendant(33, parent 32), Early(34), Late(35, parent 34) import as exactly Child, Parent, Early,
// Late in file order, with one warning 'Skipping 2 invalid takeoff(s)'.
TEST(Imp09_Plan, OrphanedTakeoffsAndTheirDescendantsAreSkipped)
{
    OstRawData raw = raw_with({{{'B', "BidConditions"}, {COND}},
                               {{'B', "BidPages"}, {PAGE20}},
                               {{'P', "BidTakeoffs"},
                                {takeoff("31", "20", "10", "30"), takeoff("30", "20", "10"), takeoff("32", "20", "10", "999"),
                                 takeoff("33", "20", "10", "32"), takeoff("34", "20", "10"), takeoff("35", "20", "10", "34")}}});
    std::string logs;
    CHECK_STR_EQ(validate(raw, &logs), "accepted");
    std::vector<std::string> uids;
    for (const Row &row : raw.page_tables.find("BidTakeoffs")->second)
        uids.push_back(row.at("UID"));
    CHECK(uids == (std::vector<std::string>{"31", "30", "34", "35"}));
    CHECK_STR_EQ(logs, "WARNING: Skipping 2 invalid takeoff(s) during OST import; missing-parent roots: BidTakeoffs.UID=32 ParentUID=999 "
                       "missing BidTakeoffs.UID; skipped dependent descendants: BidTakeoffs.UID=33 ParentUID=32 missing BidTakeoffs.UID\n");
}

// ---------------------------------------------------------------------------------------------------------------
// Details of the Windows rules
// ---------------------------------------------------------------------------------------------------------------

// RAW_BID_RELATIONSHIPS: 144 relationships of which 96 name tables of a raw file and are not reconciled by the import itself (Windows run).
TEST(Imp09_Rules, TheRawRelationshipsAreTheWindowsSet)
{
    CHECK_EQ(ost_import::raw_bid_relationships().size(), 96u);
    size_t required = 0;
    for (const auto &relationship : ost_import::raw_bid_relationships())
    {
        if (relationship.required)
            ++required;
        CHECK_FALSE(relationship.child_table == "Bids" && (relationship.child_column == "JobStatusUID" || relationship.child_column == "EstimatorUID" ||
                                                           relationship.child_column == "PrManagerUID" || relationship.child_column == "JobSiteManagerUID"));
    }
    CHECK_EQ(required, 2u); // BidTakeoffs.BidPageUID and BidTakeoffs.BidConditionUID
}

// Port of format_integrity_issues(limit=10) and the issue texts (Windows run).
TEST(Imp09_Rules, TheIssueTexts)
{
    std::vector<IntegrityIssue> issues;
    for (int i = 0; i < 13; ++i)
    {
        IntegrityIssue issue;
        issue.kind = IntegrityIssue::Kind::DuplicateUid;
        issue.table = "T";
        issue.uid = std::to_string(i);
        issue.count = 2;
        issues.push_back(issue);
    }
    CHECK_STR_EQ(ost_import::format_integrity_issues(issues),
                 "T.UID=0 occurs 2 times; T.UID=1 occurs 2 times; T.UID=2 occurs 2 times; T.UID=3 occurs 2 times; T.UID=4 occurs 2 times; "
                 "T.UID=5 occurs 2 times; T.UID=6 occurs 2 times; T.UID=7 occurs 2 times; T.UID=8 occurs 2 times; T.UID=9 occurs 2 times; +3 more");
    CHECK_STR_EQ(ost_import::format_integrity_issues(issues, 2), "T.UID=0 occurs 2 times; T.UID=1 occurs 2 times; +11 more");
    issues.resize(10);
    CHECK(ost_import::format_integrity_issues(issues).find("more") == std::string::npos);
    CHECK_STR_EQ(ost_import::format_integrity_issues({}), "");

    IntegrityIssue missing;
    missing.kind = IntegrityIssue::Kind::MissingReference;
    missing.table = "T";
    missing.column = "C";
    missing.value = "v";
    missing.parent_table = "P";
    CHECK_STR_EQ(missing.format(), "T.UID=<no UID> C=v missing P.UID");
    IntegrityIssue malformed;
    malformed.kind = IntegrityIssue::Kind::MalformedUid;
    malformed.table = "T";
    malformed.column = "UID";
    CHECK_STR_EQ(malformed.format(), "T.UID=<no UID> has malformed UID=<missing>");
    IntegrityIssue cardinality;
    cardinality.kind = IntegrityIssue::Kind::Cardinality;
    cardinality.table = "BidSettings";
    cardinality.owner_table = "Bids";
    cardinality.owner_uid = "1";
    cardinality.count = 3;
    cardinality.maximum = 1;
    CHECK_STR_EQ(cardinality.format(), "BidSettings has 3 rows for Bids.UID=1; expected at most 1");
}

// Python's int() refuses more than 4300 digits (leading zeros count): the validation raises and the import fails (Windows run: 4300 digits
// are fine, 4301 raise ValueError "Exceeds the limit (4300 digits) for integer string conversion: value has 4301 digits; use
// sys.set_int_max_str_digits() to increase the limit").
TEST(Imp09_Rules, AUidOfMoreThan4300DigitsRaises)
{
    {
        OstRawData raw = raw_with({{{'B', "BidPages"}, {Row{{"UID", std::string(4300, '1')}, {"BidUID", "1"}}}}});
        CHECK_STR_EQ(validate(raw), "accepted");
    }
    {
        OstRawData raw = raw_with({{{'B', "BidPages"}, {Row{{"UID", std::string(4301, '1')}, {"BidUID", "1"}}}}});
        CHECK_STR_EQ(validate(raw), "Exceeds the limit (4300 digits) for integer string conversion: value has 4301 digits; use "
                                    "sys.set_int_max_str_digits() to increase the limit");
    }
    {
        OstRawData raw = raw_with({{{'B', "BidPages"}, {Row{{"UID", "0" + std::string(4300, '1')}, {"BidUID", "1"}}}}});
        CHECK_FALSE(validate(raw) == "accepted");
    }
    {
        // a takeoff UID of that size is judged by resolve_takeoff_graph first
        OstRawData raw = raw_with({{{'P', "BidTakeoffs"}, {Row{{"UID", std::string(4301, '9')}}}}});
        CHECK(validate(raw).find("Exceeds the limit (4300 digits)") == 0);
    }
}

// str.isdecimal() (the repair of selected pages / annotation references): Unicode decimal digits count as numbers, so an Arabic-Indic '3'
// that names no page is cleared like an ASCII one (Windows run: BidPageSelectedUID='٣' -> '0').
TEST(Imp09_Rules, UnicodeDecimalDigitsCountAsNumbersWhenClearing)
{
    CHECK(ost_import::text_rules::py_isdecimal("123"));
    CHECK(ost_import::text_rules::py_isdecimal("\xD9\xA3"));     // U+0663
    CHECK(ost_import::text_rules::py_isdecimal("\xEF\xBC\x91")); // U+FF11 fullwidth one
    CHECK_FALSE(ost_import::text_rules::py_isdecimal(""));
    CHECK_FALSE(ost_import::text_rules::py_isdecimal("1.0"));
    CHECK_FALSE(ost_import::text_rules::py_isdecimal("-4"));
    CHECK_FALSE(ost_import::text_rules::py_isdecimal(" 7"));
    CHECK_FALSE(ost_import::text_rules::py_isdecimal("\xC2\xB2")); // superscript two: a digit but not decimal
    CHECK_FALSE(ost_import::text_rules::py_isdecimal("\xFF"));     // not UTF-8

    OstRawData raw = raw_with({{{'B', "BidSettings"}, {Row{{"UID", "30"}, {"BidUID", "1"}, {"BidPageSelectedUID", "\xD9\xA3"}}}}});
    CHECK_STR_EQ(validate(raw), "accepted");
    CHECK_STR_EQ(raw.bid_tables.find("BidSettings")->second[0]["BidPageSelectedUID"], "0");
}

// A takeoff UID is compared as the stored text by resolve_takeoff_graph ("007" and "7" are two takeoffs there) but as a number by the
// duplicate check of the bid tables ("007" and "7" of one table occur twice): both from the Windows code.
TEST(Imp09_Rules, TakeoffUidsAreTextButBidOwnedUidsAreNumbers)
{
    {
        OstRawData raw = raw_with({{{'B', "BidConditions"}, {COND}},
                                   {{'B', "BidPages"}, {PAGE20}},
                                   {{'P', "BidTakeoffs"}, {takeoff("7", "20", "10"), takeoff("007", "20", "10")}}});
        ost_import::TakeoffGraphResolution graph = ost_import::resolve_takeoff_graph(raw);
        CHECK(graph.fatal_issues.empty());
        CHECK_EQ(raw.page_tables.find("BidTakeoffs")->second.size(), 2u);
    }
    {
        OstRawData raw = raw_with({{{'B', "BidPages"}, {Row{{"UID", "7"}, {"BidUID", "1"}}, Row{{"UID", "007"}, {"BidUID", "1"}}}}});
        std::vector<IntegrityIssue> issues = ost_import::validate_raw_bid_integrity(raw);
        REQUIRE_EQ(issues.size(), 1u);
        CHECK_STR_EQ(issues[0].format(), "BidPages.UID=7 occurs 2 times");
    }
}
