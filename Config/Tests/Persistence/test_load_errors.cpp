// DBP-M02: load_project / load_hierarchy must report a failed read instead of returning an empty or partial model.
// Windows oracle: bid_data_reader.py (schema.require_column raises, a failing read re-raises, optional tables are
// skipped) and the two contract tests
//   tests/integration/persistence/test_takeoff_read_contract.py::test_deleted_record_read_error_never_becomes_empty_takeoff_family
//   tests/integration/persistence/test_scoped_family_read_contract.py::test_condition_folder_query_failure_cannot_publish_incomplete_family
// (the error propagates and no empty family is published). The legacy 2-argument load_project / 1-argument load_hierarchy
// return an EMPTY project / hierarchy for a failure (callers already treat empty() as "unreadable").
#include "StoreTestSupport.hpp"

#include <string>

using ost_ptest::TempDb;

namespace
{
    // Bid 10 with a page, a condition and two takeoffs, a layer, a condition folder, a page folder and a cdn type.
    void seed(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (10, 2, 'Job', 10)");
        db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (90, 10, 'pf')");
        db.must("INSERT INTO BidPages (UID, BidUID, Name, BidPageFolderUID) VALUES (100, 10, 'P1', 90)");
        db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (80, 10, 'cf')");
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (70, 10, 'L1', -1, 1)");
        db.must("INSERT INTO CdnTypes (UID, Name) VALUES (5, 'Type')");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, CdnTypeUID, BidConditionFolderUID) "
                "VALUES (30, 10, 1, 'Cdn', 0, 5, 80)");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (1, 10, 30, 100, '1;2;3;4')");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (2, 10, 30, 100, '5;6;7;8')");
    }

    struct Outcome
    {
        bool ok;
        ost_data::ProjectData project;
        std::string error;
    };

    Outcome load_checked(const TempDb &db, const std::string &bid = "10")
    {
        Outcome out;
        out.ok = ost_store::load_project(db.path(), bid, out.project, &out.error);
        return out;
    }

    // The failure contract: false, an error text, an empty project; the legacy call gives an empty project too.
    void check_failed(const TempDb &db, const char *what)
    {
        SCOPED_CONTEXT(std::string("case: ") + what);
        const Outcome out = load_checked(db);
        CHECK_FALSE(out.ok);
        CHECK(out.project.empty());
        CHECK(out.project.takeoffs.empty());
        CHECK(out.project.pages.empty());
        CHECK(out.project.conditions.empty());
        CHECK_FALSE(out.error.empty());
        CHECK(ost_store::load_project(db.path(), "10").empty());
    }
} // namespace

// Control: the seeded bid loads completely, and the checked and the legacy call agree.
TEST(Dbpm02_LoadErrors, ControlLoadsTheWholeBid)
{
    TempDb db("load_err_control");
    seed(db);
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    CHECK(out.error.empty());
    CHECK_STR_EQ(out.project.bid.uid, "10");
    CHECK_STR_EQ(out.project.project_name, "Folder A");
    CHECK_EQ(out.project.pages.size(), static_cast<size_t>(1));
    CHECK_EQ(out.project.layers.size(), static_cast<size_t>(1));
    CHECK_EQ(out.project.cdn_types.size(), static_cast<size_t>(1));
    CHECK_EQ(out.project.condition_folders.size(), static_cast<size_t>(1));
    CHECK_EQ(out.project.conditions.size(), static_cast<size_t>(1));
    CHECK_EQ(out.project.takeoffs.size(), static_cast<size_t>(2));
    CHECK_EQ(out.project.folders.size(), static_cast<size_t>(1));
    const ost_data::ProjectData legacy = ost_store::load_project(db.path(), "10");
    CHECK_FALSE(legacy.empty());
    CHECK_EQ(legacy.takeoffs.size(), static_cast<size_t>(2));
}

// Test plan: build a bid with takeoffs, then DROP TABLE BidTakeoffs (and separately BidPages, BidConditions): the load
// must fail, not return the bid with an empty family (Windows: schema.require_column raises "Missing BidTakeoffs.UID").
TEST(Dbpm02_LoadErrors, AMissingRequiredTableFailsTheLoad)
{
    for (const char *table : {"BidTakeoffs", "BidPages", "BidConditions", "Bids"})
    {
        TempDb db(std::string("load_err_drop_") + table);
        seed(db);
        db.must(std::string("DROP TABLE ") + table);
        check_failed(db, table);
    }
}

// A required column that is gone fails the load and names the column.
TEST(Dbpm02_LoadErrors, AMissingRequiredColumnFailsTheLoadAndNamesIt)
{
    TempDb db("load_err_column");
    seed(db);
    db.must("ALTER TABLE BidTakeoffs DROP COLUMN Position");
    const Outcome out = load_checked(db);
    CHECK_FALSE(out.ok);
    CHECK(out.project.empty());
    CHECK(out.error.find("Position") != std::string::npos);
}

// Windows: BidLayers, CdnTypes, BidConditionFolders and BidPageFolders are optional tables (schema.optional_table_missing):
// without them the bid loads with an empty layer / type / folder family. (iOS also tolerates a missing BidProjects table:
// the project name falls back to the bid name.)
TEST(Dbpm02_LoadErrors, AMissingOptionalTableIsSkipped)
{
    struct Case
    {
        const char *table;
        size_t pages, layers, cdn_types, condition_folders, folders;
    };
    const Case cases[] = {
        {"BidLayers", 1, 0, 1, 1, 1},
        {"CdnTypes", 1, 1, 0, 1, 1},
        {"BidConditionFolders", 1, 1, 1, 0, 1},
        {"BidPageFolders", 1, 1, 1, 1, 0},
        {"BidProjects", 1, 1, 1, 1, 1},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string(c.table));
        TempDb db(std::string("load_err_optional_") + c.table);
        seed(db);
        db.must(std::string("DROP TABLE ") + c.table);
        const Outcome out = load_checked(db);
        REQUIRE(out.ok);
        CHECK(out.error.empty());
        CHECK_EQ(out.project.pages.size(), c.pages);
        CHECK_EQ(out.project.layers.size(), c.layers);
        CHECK_EQ(out.project.cdn_types.size(), c.cdn_types);
        CHECK_EQ(out.project.condition_folders.size(), c.condition_folders);
        CHECK_EQ(out.project.folders.size(), c.folders);
        CHECK_EQ(out.project.conditions.size(), static_cast<size_t>(1));
        CHECK_EQ(out.project.takeoffs.size(), static_cast<size_t>(2));
        if (std::string(c.table) == "BidProjects")
            CHECK_STR_EQ(out.project.project_name, "Job"); // falls back to the bid name
    }
}

// The statement fails while the rows are being read: the rows read so far must not become the takeoff family. A view named
// BidTakeoffs whose Position computation overflows for takeoff 2 (abs(INT64_MIN) is an SQL error at step time) gives
// takeoff 1 and then an error.
TEST(Dbpm02_LoadErrors, AReadThatFailsInTheMiddleFailsTheLoad)
{
    TempDb db("load_err_mid_read");
    seed(db);
    db.must("ALTER TABLE BidTakeoffs RENAME TO BidTakeoffsData");
    db.must("CREATE VIEW BidTakeoffs AS SELECT UID, BidUID, BidConditionUID, BidPageUID, BidAreaUID, Rotation, Curve, "
            "ParentUID, IsNegativeQuantity, "
            "CASE WHEN UID = 2 THEN abs(-9223372036854775807 - 1) ELSE Position END AS Position FROM BidTakeoffsData");
    // the first row alone reads fine, the second one errors
    CHECK_EQ(db.count("SELECT length(Position) FROM BidTakeoffs WHERE UID = 1"), 7);
    CHECK_EQ(db.count("SELECT SUM(length(Position)) FROM BidTakeoffs"), -1);
    check_failed(db, "error while stepping BidTakeoffs");
}

// A bid that does not exist is a failure with a cause (Windows get_condition_family: require_existing_unique_bid_owned_
// uid_matches raises "Bids has no row for UID 99"); a bid without any rows in its families is not.
TEST(Dbpm02_LoadErrors, ANonexistentBidFailsAndAnEmptyBidLoads)
{
    TempDb db("load_err_missing_bid");
    seed(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (11, 2, 'Empty job', 11)");
    {
        const Outcome out = load_checked(db, "99");
        CHECK_FALSE(out.ok);
        CHECK(out.project.empty());
        CHECK(out.error.find("99") != std::string::npos);
        CHECK(ost_store::load_project(db.path(), "99").empty());
    }
    {
        const Outcome out = load_checked(db, "11");
        CHECK(out.ok);
        CHECK_STR_EQ(out.project.bid.uid, "11");
        CHECK(out.project.takeoffs.empty());
        CHECK(out.project.pages.empty());
        CHECK_FALSE(ost_store::load_project(db.path(), "11").empty());
    }
}

// A database that cannot be opened (a directory is no database file) is a failure, not an empty project.
TEST(Dbpm02_LoadErrors, ADatabaseThatCannotBeOpenedFailsTheLoad)
{
    const std::string directory = ost_test::temp_path("load_err_directory");
    std::filesystem::create_directories(directory);
    ost_data::ProjectData project;
    std::string error;
    CHECK_FALSE(ost_store::load_project(directory, "10", project, &error));
    CHECK(project.empty());
    CHECK_FALSE(error.empty());
    CHECK(ost_store::load_project(directory, "10").empty());
    ost_domain::DatabaseHierarchy hierarchy;
    std::string hierarchy_error;
    CHECK_FALSE(ost_store::load_hierarchy(directory, hierarchy, &hierarchy_error));
    CHECK_FALSE(hierarchy_error.empty());
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
}

// ---------------------------------------------------------------------------------------------------------------
// load_hierarchy
// ---------------------------------------------------------------------------------------------------------------

TEST(Dbpm02_LoadErrors, HierarchyControlListsFoldersAndBids)
{
    TempDb db("load_err_hierarchy_control");
    seed(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (11, NULL, 'Orphan', 11)");
    ost_domain::DatabaseHierarchy hierarchy;
    std::string error;
    REQUIRE(ost_store::load_hierarchy(db.path(), hierarchy, &error));
    CHECK(error.empty());
    CHECK_EQ(hierarchy.folders.size(), static_cast<size_t>(2)); // 'Deleted Bids' and 'Folder A'
    CHECK_EQ(hierarchy.orphan_bids.size(), static_cast<size_t>(1));
    const ost_domain::DatabaseHierarchy legacy = ost_store::load_hierarchy(db.path());
    CHECK_EQ(legacy.folders.size(), static_cast<size_t>(2));
    CHECK_EQ(legacy.orphan_bids.size(), static_cast<size_t>(1));
}

// Windows _parse_hierarchy requires BidProjects and Bids: a dropped table fails the load and the result is an empty hierarchy,
// never the half that was readable.
TEST(Dbpm02_LoadErrors, HierarchyWithAMissingTableFails)
{
    for (const char *table : {"Bids", "BidProjects"})
    {
        SCOPED_CONTEXT(std::string("dropped table: ") + table);
        TempDb db(std::string("load_err_hierarchy_drop_") + table);
        seed(db);
        db.must(std::string("DROP TABLE ") + table);
        ost_domain::DatabaseHierarchy hierarchy;
        std::string error;
        CHECK_FALSE(ost_store::load_hierarchy(db.path(), hierarchy, &error));
        CHECK(hierarchy.folders.empty());
        CHECK(hierarchy.orphan_bids.empty());
        CHECK_FALSE(error.empty());
        const ost_domain::DatabaseHierarchy legacy = ost_store::load_hierarchy(db.path());
        CHECK(legacy.folders.empty());
        CHECK(legacy.orphan_bids.empty());
    }
}

// The bid list fails while it is read (a view over Bids that overflows for bid 20): the project folders read before it must not
// be returned as the hierarchy.
TEST(Dbpm02_LoadErrors, HierarchyReadThatFailsInTheMiddleFails)
{
    TempDb db("load_err_hierarchy_mid");
    seed(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (20, 2, 'Twenty', 20)");
    db.must("ALTER TABLE Bids RENAME TO BidsData");
    db.must("CREATE VIEW Bids AS SELECT UID, BidProjectUID, BidNo, "
            "CASE WHEN UID = 10 THEN abs(-9223372036854775807 - 1) ELSE JobName END AS JobName FROM BidsData");
    ost_domain::DatabaseHierarchy hierarchy;
    std::string error;
    CHECK_FALSE(ost_store::load_hierarchy(db.path(), hierarchy, &error));
    CHECK(hierarchy.folders.empty());
    CHECK(hierarchy.orphan_bids.empty());
    CHECK_FALSE(error.empty());
    const ost_domain::DatabaseHierarchy legacy = ost_store::load_hierarchy(db.path());
    CHECK(legacy.folders.empty());
    CHECK(legacy.orphan_bids.empty());
}
