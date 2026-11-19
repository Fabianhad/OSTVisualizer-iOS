// DBP-12 (+BRW-04): permanently_delete_project removes every row Windows delete_bids removes, and no more.
// Windows oracle: ost_visualizer/infrastructure/mdb/components/bid_operations.py::_delete_bids_cascade_chunk
// (tests/infrastructure/mdb/components/test_bid_operations.py). DeleteCascadeOracleData.hpp is the real Windows cascade run
// read-only on a dataset in which every table has one row linked to the deleted bid ONLY through each parent column and one
// linked only by BidUID, plus bystander rows of another bid; the survivors per table are the expected result here.
#include "StoreTestSupport.hpp"

#include "DeleteCascadeOracleData.hpp"

#include <algorithm>
#include <string>
#include <vector>

using ost_ptest::TempDb;

namespace
{
    std::string uids_of(const TempDb &db, const std::string &table)
    {
        std::string out = db.text("SELECT group_concat(UID, ' ') FROM (SELECT UID FROM [" + table + "] ORDER BY UID)");
        return out == "<none>" ? std::string() : out;
    }

    // Applies the oracle dataset on top of create_schema: bid 10 is the target in 'Deleted Bids' (project 1), bid 20 the bystander.
    void load_oracle_dataset(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        for (const char *sql : ost_ptest::kDeleteCascadeInserts)
            db.must(sql);
    }

    const char *label_of(const std::string &table, int uid)
    {
        for (const ost_ptest::DeleteCascadeRow &row : ost_ptest::kDeleteCascadeRows)
            if (table == row.table && uid == row.uid)
                return row.label;
        return "";
    }
} // namespace

// Windows oracle dataset: after the cascade every table holds exactly the rows Windows keeps (the rows of the bystander bid
// and the few Windows leaves behind: BidPercents linked only via BidLaborActivityUID, BidDPCSubscribers.BidEmployeeUID,
// BidHotLinks.BidPageUID, BidNamedViews.BidPageUID, ...), the Bids row is gone and the 'Deleted Bids' project stays.
TEST(Dbp12_PermanentDelete, LeavesExactlyTheRowsTheWindowsCascadeLeaves)
{
    TempDb db("dbp12_oracle");
    load_oracle_dataset(db);
    REQUIRE(ost_store::permanently_delete_project(db.path(), "10"));

    int mismatched_tables = 0;
    for (const ost_ptest::DeleteCascadeSurvivors &expected : ost_ptest::kDeleteCascadeSurvivors)
    {
        const std::string actual = uids_of(db, expected.table);
        if (actual == expected.uids)
            continue;
        ++mismatched_tables;
        // name the rows that are in the wrong state
        std::string detail;
        auto split = [](const std::string &text)
        {
            std::vector<int> out;
            size_t from = 0;
            while (from < text.size())
            {
                size_t space = text.find(' ', from);
                if (space == std::string::npos)
                    space = text.size();
                out.push_back(std::stoi(text.substr(from, space - from)));
                from = space + 1;
            }
            return out;
        };
        const std::vector<int> want = split(expected.uids);
        const std::vector<int> have = split(actual);
        for (int uid : have)
            if (std::find(want.begin(), want.end(), uid) == want.end())
                detail += " kept-but-Windows-deletes:" + std::to_string(uid) + "[" + label_of(expected.table, uid) + "]";
        for (int uid : want)
            if (std::find(have.begin(), have.end(), uid) == have.end())
                detail += " deleted-but-Windows-keeps:" + std::to_string(uid) + "[" + label_of(expected.table, uid) + "]";
        SCOPED_CONTEXT(std::string(expected.table) + detail);
        CHECK_STR_EQ(actual, expected.uids);
    }
    CHECK_EQ(mismatched_tables, 0);

    // BidSettings.BidPageSelectedUID of another bid that pointed at a page of the deleted bid is cleared; one that points at
    // a page of a surviving bid stays (cross-bid selected page).
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidSettings WHERE UID = 900 AND BidPageSelectedUID IS NULL"),
             ost_ptest::kDeleteCascadeSelectedPageAfter900IsNull);
    CHECK_EQ(db.count("SELECT BidPageSelectedUID FROM BidSettings WHERE UID = 901"), ost_ptest::kDeleteCascadeSelectedPageAfter901);
}

// Port of test_bid_operations.py::test_delete_bid_removes_bid_scoped_children_without_direct_links, on the iOS schema. Rows
// that carry no BidUID and no page: they hang off the bid's takeoff, area, typical area, labor code or time card state.
TEST(Dbp12_PermanentDelete, RemovesChildrenWithoutDirectLinksAndKeepsTheSiblingBid)
{
    TempDb db("dbp12_no_direct_links");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'deleted')");
    db.must("INSERT INTO BidAreas (UID, BidUID) VALUES (30, 1)");
    db.must("INSERT INTO BidTypAreas (UID, BidUID) VALUES (40, 1)");
    db.must("INSERT INTO BidLaborCostCodes (UID, BidUID) VALUES (50, 1)");
    db.must("INSERT INTO BidTimeCardStates (UID, BidUID) VALUES (60, 1)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID) VALUES (10, 1, NULL)");
    db.must("INSERT INTO BidDimensions (UID, BidUID, BidPageUID, BidTakeoffFromUID) VALUES (20, 1, NULL, 10)");
    db.must("INSERT INTO BidTimeCards (UID, BidTimeCardStateUID, BidEmployeeUID, BidAreaUID, BidTypicalAreaUID, "
            "BidLaborCostCodeUID) VALUES (70, 60, NULL, 30, 40, 50)");
    db.must("INSERT INTO BidPercents (UID, BidTakeoffUID, BidLaborCostCodeUID, BidTimeCardStateUID) VALUES (80, NULL, 50, 60)");
    db.must("INSERT INTO BidTypAreaCounts (UID, BidAreaUID, BidTypAreaUID) VALUES (90, 30, 40)");
    db.must("INSERT INTO BidPageSettings (UID, BidPageUID, BidAreaUID, BidTypAreaUID) VALUES (100, NULL, 30, 40)");
    // A sibling bid whose rows must survive the cascade.
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (2, 1, 'sibling')");
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (11, 2)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID) VALUES (12, 2, 11)");
    db.must("INSERT INTO BidAreas (UID, BidUID) VALUES (31, 2)");
    db.must("INSERT INTO BidTimeCards (UID, BidTimeCardStateUID, BidEmployeeUID, BidAreaUID, BidTypicalAreaUID, "
            "BidLaborCostCodeUID) VALUES (71, NULL, NULL, 31, NULL, NULL)");
    db.must("INSERT INTO BidPercents (UID, BidTakeoffUID, BidLaborCostCodeUID, BidTimeCardStateUID) VALUES (81, 12, NULL, NULL)");

    REQUIRE(ost_store::permanently_delete_project(db.path(), "1"));
    CHECK_STR_EQ(uids_of(db, "Bids"), "2");
    const struct
    {
        const char *table;
        const char *survivors;
    } expected[] = {{"BidPages", "11"},  {"BidTakeoffs", "12"},   {"BidDimensions", ""},      {"BidTimeCards", "71"},
                    {"BidPercents", "81"}, {"BidTypAreaCounts", ""}, {"BidPageSettings", ""},    {"BidAreas", "31"},
                    {"BidTypAreas", ""},   {"BidLaborCostCodes", ""}, {"BidTimeCardStates", ""}};
    for (const auto &row : expected)
    {
        SCOPED_CONTEXT(std::string(row.table));
        CHECK_STR_EQ(uids_of(db, row.table), row.survivors);
    }
}

// Port of test_delete_bid_removes_annotation_linked_only_by_to_takeoff (the 'To' end alone links the annotation to the bid).
TEST(Dbp12_PermanentDelete, RemovesAnnotationsLinkedOnlyByTheToTakeoff)
{
    TempDb db("dbp12_to_takeoff");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'deleted')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (2, 1, 'other')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID) VALUES (10, 1)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID) VALUES (11, 2)");
    db.must("INSERT INTO BidDimensions (UID, BidUID, BidPageUID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (20, NULL, NULL, NULL, 10)");
    db.must("INSERT INTO BidDimensions (UID, BidUID, BidPageUID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (21, NULL, NULL, NULL, 11)");

    REQUIRE(ost_store::permanently_delete_project(db.path(), "1"));
    CHECK_STR_EQ(uids_of(db, "BidDimensions"), "21");
    CHECK_STR_EQ(uids_of(db, "BidTakeoffs"), "11");
    CHECK_STR_EQ(uids_of(db, "Bids"), "2");
}

// Port of test_delete_bid_removes_bid_settings_before_bid.
TEST(Dbp12_PermanentDelete, RemovesTheBidSettingsOfTheBidOnly)
{
    TempDb db("dbp12_bid_settings");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'deleted')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (4, 1, 'other')");
    db.must("INSERT INTO BidSettings (UID, BidUID) VALUES (2, 1)");
    db.must("INSERT INTO BidSettings (UID, BidUID) VALUES (5, 4)");

    REQUIRE(ost_store::permanently_delete_project(db.path(), "1"));
    CHECK_STR_EQ(uids_of(db, "BidSettings"), "5");
    CHECK_STR_EQ(uids_of(db, "Bids"), "4");
}

// Port of test_delete_bid_clears_cross_bid_selected_page_before_page_delete.
TEST(Dbp12_PermanentDelete, ClearsASelectedPageOfAnotherBidThatPointsIntoTheDeletedBid)
{
    TempDb db("dbp12_selected_page");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'deleted')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (2, 1, 'second')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (3, 1, 'third')");
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (10, 1)");
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (30, 3)");
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (20, 2, 10)");
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (21, 3, 30)");

    REQUIRE(ost_store::permanently_delete_project(db.path(), "1"));
    CHECK_STR_EQ(uids_of(db, "Bids"), "2 3");
    CHECK_STR_EQ(uids_of(db, "BidPages"), "30");
    CHECK_EQ(db.count("SELECT BidPageSelectedUID FROM BidSettings WHERE UID = 21"), 30);
    CHECK_EQ(db.count("SELECT BidUID FROM BidSettings WHERE UID = 20"), 2);
    CHECK_STR_EQ(db.text("SELECT typeof(BidPageSelectedUID) FROM BidSettings WHERE UID = 20"), "null");
}

// Windows skips a cascade step whose table or column the schema lacks (optional_table_missing / column_exists guards) instead of
// failing the delete. The tables are dropped from the iOS schema one group at a time: the delete still completes, the rows of the
// tables that remain are cleaned and the sibling's rows stay.
TEST(Dbp12_PermanentDelete, SkipsCascadeTablesTheSchemaLacks)
{
    const std::vector<std::vector<std::string>> dropped_groups = {
        {"BidPageSettings", "BidTypAreaCounts"},
        {"BidHotLinks", "BidNamedViews", "BidComments", "BidLegends"},
        {"BidTimeCards", "BidTimeCardStates", "BidLaborCostCodes", "BidEmployees"},
        {"BidPages", "BidPageFolders", "BidSettings"},
        {"ConditionSetStyles", "BidConditionUser", "BidPlanRooms", "BidNotes", "BidTransactionsHistory", "STSTransactionHistory"},
    };
    int index = 0;
    for (const std::vector<std::string> &group : dropped_groups)
    {
        std::string names;
        for (const std::string &t : group)
            names += t + " ";
        SCOPED_CONTEXT("schema without " + names);
        TempDb db("dbp12_skip_" + std::to_string(index++));
        load_oracle_dataset(db);
        for (const std::string &table : group)
            db.must("DROP TABLE [" + table + "]");

        REQUIRE(ost_store::permanently_delete_project(db.path(), "10"));
        CHECK_STR_EQ(uids_of(db, "Bids"), "20");
        // the tables that are still there are cleaned (none of the groups drops these)
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE BidUID = 10"), 0);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE BidUID = 10"), 0);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidAreas WHERE BidUID = 10"), 0);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE BidUID = 20"), 1);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE BidUID = 20"), 1);
    }
}

// The 'Deleted Bids' rule of the iOS app stays: only a bid of the trash can be deleted for good; nothing is touched otherwise.
TEST(Dbp12_PermanentDelete, OnlyABidOfTheTrashIsDeletedAndAnythingElseChangesNothing)
{
    TempDb db("dbp12_trash_rule");
    db.seed_world();
    const std::string before = db.snapshot(TempDb::world_tables());
    CHECK_FALSE(ost_store::permanently_delete_project(db.path(), "11")); // bid 11 lives in 'Folder A'
    CHECK_FALSE(ost_store::permanently_delete_project(db.path(), "999")); // no such bid
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
    CHECK(ost_store::permanently_delete_project(db.path(), "10"));
    CHECK_STR_EQ(uids_of(db, "Bids"), "11");
}
