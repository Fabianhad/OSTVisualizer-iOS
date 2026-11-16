// DBP-11 (aliases BRW-07, COND-M04): a failed SQL statement must fail the whole ProjectStore operation.
//
// Windows contract (ost_visualizer/infrastructure/mdb/mdb_writer.py:63-105 _connection): every cursor.execute error
// raises; the connection scope rolls the whole transaction back and the write method returns its documented failure
// value (False / None / ''). A COMMIT error is an error too. Nothing is reported as success after a failed statement.
//
// The failures are provoked with BEFORE <event> triggers that RAISE(ABORT), the same device the Windows tests use
// (tests/integration/mdb/test_ownership_and_atomicity.py::test_later_chunk_delete_failure_rolls_back_page_condition_and_folder_batches,
// tests/infrastructure/mdb/components/test_condition_operations.py::ConditionDeleteCascadeContractTests).
#include "PersistenceTestSupport.hpp"

using ost_ptest::TempDb;

namespace
{
    ost_import::OstRawData make_raw_import()
    {
        ost_import::OstRawData raw;
        raw.bid_row = {{"UID", "900"}, {"JobName", "Imported"}, {"BidNo", "1"}};
        raw.bid_tables["BidAreas"] = {{{"UID", "950"}, {"BidUID", "900"}, {"Name", "Area"}}};
        raw.bid_tables["BidConditions"] = {
            {{"UID", "960"}, {"BidUID", "900"}, {"RefNo", "1"}, {"Name", "Cdn"}, {"Type", "0"}}};
        raw.page_tables["BidTakeoffs"] = {
            {{"UID", "700"}, {"BidUID", "900"}, {"BidConditionUID", "960"}, {"Position", "1;2"}}};
        raw.global_tables["CdnTypes"] = {{{"UID", "5"}, {"Name", "Walls"}}};
        return raw;
    }

    const std::vector<std::string> &import_tables()
    {
        static const std::vector<std::string> tables = {"Bids", "BidAreas", "BidConditions", "BidTakeoffs",
                                                         "CdnTypes"};
        return tables;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// create_schema / delete_project / permanently_delete_project (ProjectStore.cpp)
// ---------------------------------------------------------------------------------------------------------------

// Windows: create fails -> raise -> rollback -> failure value. The 'Deleted Bids' insert inside create_schema was
// executed with the result ignored, so create_schema reported success without the system project.
TEST(StoreErrors, CreateSchemaFailsWhenTheDeletedBidsProjectCannotBeInserted)
{
    TempDb db("create_schema_blocked", /*with_schema=*/false);
    db.must("CREATE TABLE BidProjects (UID INTEGER PRIMARY KEY, Name TEXT, Description BLOB)");
    db.block("BidProjects", "INSERT");

    CHECK_FALSE(ost_store::create_schema(db.path()));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidProjects"), 0);
}

TEST(StoreErrors, DeleteProjectRollsBackWhenTheUpdateFails)
{
    TempDb db("delete_project_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("Bids", "UPDATE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_project(db.path(), "11"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, DeleteProjectControlMovesTheBidToDeletedBids)
{
    TempDb db("delete_project_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::delete_project(db.path(), "11"));
    CHECK_EQ(db.text("SELECT BidProjectUID || '/' || OrigBidProjectUID FROM Bids WHERE UID = 11"), "1/2");
}

// The merge of duplicate 'Deleted Bids' projects runs inside the delete transaction: a failure there fails the delete
// and the moved bids go back (resolve_deleted_bids_project_uid ignored both step results).
TEST(StoreErrors, DeleteProjectRollsBackWhenMergingDuplicateDeletedBidsProjectsFails)
{
    TempDb db("delete_project_merge_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (7, 'Deleted Bids')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (12, 7, 'In duplicate', 12)");
    db.block("BidProjects", "DELETE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_project(db.path(), "11"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

// Port of tests/integration/mdb/test_ownership_and_atomicity.py::test_later_chunk_delete_failure_rolls_back_page_condition_and_folder_batches
// (a delete that fails part-way leaves every table untouched). Trigger on BidPages as in the item's host repro.
TEST(StoreErrors, PermanentDeleteFailingMidCascadeLeavesEveryTableUntouched)
{
    TempDb db("perm_delete_mid");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidPages", "DELETE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::permanently_delete_project(db.path(), "10"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, PermanentDeleteFailingOnTheFirstStatementLeavesEveryTableUntouched)
{
    TempDb db("perm_delete_first");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidPercents", "DELETE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::permanently_delete_project(db.path(), "10"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, PermanentDeleteFailingOnTheBidRowLeavesEveryTableUntouched)
{
    TempDb db("perm_delete_last");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("Bids", "DELETE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::permanently_delete_project(db.path(), "10"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, PermanentDeleteControlRemovesTheBidAndKeepsTheOthers)
{
    TempDb db("perm_delete_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::permanently_delete_project(db.path(), "10"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids WHERE UID = 10"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids WHERE UID = 11"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidDimensions"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPercents"), 0);
}

// ---------------------------------------------------------------------------------------------------------------
// Folders (ProjectStore+Folders.cpp)  -  alias BRW-07
// Port of tests/infrastructure/mdb/components/test_project_operations.py::test_failures_return_the_documented_value_and_follow_the_error_policy
// (create -> None, rename -> False, move -> False, orphan -> False, delete -> False when the table cannot be written)
// ---------------------------------------------------------------------------------------------------------------

TEST(StoreErrors, CreateFolderReturnsEmptyWhenTheInsertFails)
{
    TempDb db("create_folder_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidProjects", "INSERT");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_STR_EQ(ost_store::create_folder(db.path(), "New folder"), "");
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, CreateFolderControlInsertsTheFolder)
{
    TempDb db("create_folder_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    const std::string uid = ost_store::create_folder(db.path(), "New folder");
    CHECK_NE(uid, std::string());
    CHECK_EQ(db.text("SELECT Name FROM BidProjects WHERE UID = " + uid), "New folder");
}

TEST(StoreErrors, RenameFolderReturnsFalseWhenTheUpdateFails)
{
    TempDb db("rename_folder_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidProjects", "UPDATE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::rename_folder(db.path(), "2", "Renamed"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, RenameFolderControlRenames)
{
    TempDb db("rename_folder_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::rename_folder(db.path(), "2", "Renamed"));
    CHECK_EQ(db.text("SELECT Name FROM BidProjects WHERE UID = 2"), "Renamed");
}

TEST(StoreErrors, AssignBidToFolderReturnsFalseWhenTheUpdateFails)
{
    TempDb db("assign_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("Bids", "UPDATE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), "11", "1"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, AssignBidToFolderControlMovesTheBid)
{
    TempDb db("assign_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::assign_bid_to_folder(db.path(), "11", "1"));
    CHECK_EQ(db.text("SELECT BidProjectUID FROM Bids WHERE UID = 11"), "1");
}

TEST(StoreErrors, RemoveBidFromFolderReturnsFalseWhenTheUpdateFails)
{
    TempDb db("remove_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("Bids", "UPDATE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::remove_bid_from_folder(db.path(), "11"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, RemoveBidFromFolderControlOrphansTheBid)
{
    TempDb db("remove_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::remove_bid_from_folder(db.path(), "11"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids WHERE UID = 11 AND BidProjectUID IS NULL"), 1);
}

// delete_folder runs two statements in one transaction; a failure of the second must undo the first.
TEST(StoreErrors, DeleteFolderRollsBackTheUnassignWhenTheFolderDeleteFails)
{
    TempDb db("delete_folder_blocked_delete");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidProjects", "DELETE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, DeleteFolderReturnsFalseWhenTheUnassignFails)
{
    TempDb db("delete_folder_blocked_update");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("Bids", "UPDATE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, DeleteFolderControlOrphansItsBidsAndRemovesTheFolder)
{
    TempDb db("delete_folder_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidProjects WHERE UID = 2"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids WHERE UID = 11 AND BidProjectUID IS NULL"), 1);
}

// ---------------------------------------------------------------------------------------------------------------
// Layers / takeoffs (ProjectStore+Takeoffs.cpp)
// Port of tests/infrastructure/mdb/components/test_takeoff_operations.py::test_delete_takeoffs_non_resource_failure_rolls_back_without_retry
// and ...::test_delete_failures_leave_every_table_untouched (delete failure -> False, every table untouched).
// ---------------------------------------------------------------------------------------------------------------

TEST(StoreErrors, SetLayerVisibilityReturnsFalseWhenTheUpdateFails)
{
    TempDb db("layer_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidLayers", "UPDATE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "10", "70", false));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, SetLayerVisibilityControlWritesTheFlag)
{
    TempDb db("layer_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::set_layer_visibility(db.path(), "10", "70", false));
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 70"), 0);
    CHECK(ost_store::set_layer_visibility(db.path(), "10", "70", true));
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 70"), -1);
}

TEST(StoreErrors, SaveTakeoffPositionReturnsFalseWhenTheUpdateFails)
{
    TempDb db("tk_position_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidTakeoffs", "UPDATE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::save_takeoff_position(db.path(), "10", "500", {5.0, 6.0}));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, SaveTakeoffPositionControlWritesThePosition)
{
    TempDb db("tk_position_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::save_takeoff_position(db.path(), "10", "500", {5.0, 6.0}));
    // Windows encode_position with preserve_indices for every index (takeoff_operations.py): full repr + newline (DBP-07).
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 500"), "5.0;6.0\n");
}

TEST(StoreErrors, SetTakeoffNegativeReturnsFalseWhenTheUpdateFails)
{
    TempDb db("tk_negative_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidTakeoffs", "UPDATE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::set_takeoff_negative(db.path(), "10", "500", true));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, SetTakeoffNegativeControlWritesTheFlag)
{
    TempDb db("tk_negative_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::set_takeoff_negative(db.path(), "10", "500", true));
    CHECK_EQ(db.count("SELECT IsNegativeQuantity FROM BidTakeoffs WHERE UID = 500"), 1);
}

TEST(StoreErrors, InsertTakeoffReturnsEmptyWhenTheInsertFails)
{
    TempDb db("tk_insert_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidTakeoffs", "INSERT");
    const std::string before = db.snapshot(TempDb::world_tables());

    ost_domain::Takeoff takeoff;
    takeoff.condition_uid = "30";
    takeoff.page_uid = "100";
    takeoff.position = {1.0, 2.0};
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", takeoff), "");
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, InsertTakeoffControlInsertsTheRow)
{
    TempDb db("tk_insert_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    ost_domain::Takeoff takeoff;
    takeoff.condition_uid = "30";
    takeoff.page_uid = "100";
    takeoff.position = {1.0, 2.0};
    const std::string uid = ost_store::insert_takeoff(db.path(), "10", takeoff);
    REQUIRE(!uid.empty());
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = " + uid + " AND BidUID = 10"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 3);
}

// delete_takeoff: the child cleanups run first; a failing takeoff delete must bring them back.
TEST(StoreErrors, DeleteTakeoffRollsBackTheChildCleanupWhenTheTakeoffDeleteFails)
{
    TempDb db("tk_delete_blocked_row");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidTakeoffs", "DELETE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_takeoff(db.path(), "500"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, DeleteTakeoffReturnsFalseWhenAChildCleanupFails)
{
    TempDb db("tk_delete_blocked_child");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidPercents", "DELETE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_takeoff(db.path(), "500"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, DeleteTakeoffControlRemovesTheTakeoffAndItsLinks)
{
    TempDb db("tk_delete_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::delete_takeoff(db.path(), "500"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 500"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 501"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidDimensions"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPercents"), 0);
}

// ---------------------------------------------------------------------------------------------------------------
// Conditions (ProjectStore+Conditions.cpp)  -  alias COND-M04
// ---------------------------------------------------------------------------------------------------------------

TEST(StoreErrors, UpdateConditionReturnsFalseWhenTheUpdateFails)
{
    TempDb db("cdn_update_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidConditions", "UPDATE");
    const std::string before = db.snapshot(TempDb::world_tables());

    ost_domain::Condition condition;
    condition.uid = "30";
    condition.name = "Renamed";
    CHECK_FALSE(ost_store::update_condition(db.path(), condition));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, UpdateConditionControlWritesTheName)
{
    TempDb db("cdn_update_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    ost_domain::Condition condition;
    condition.uid = "30";
    condition.name = "Renamed";
    CHECK(ost_store::update_condition(db.path(), condition));
    CHECK_EQ(db.text("SELECT Name FROM BidConditions WHERE UID = 30"), "Renamed");
}

TEST(StoreErrors, ShiftConditionRefNosReturnsFalseWhenTheUpdateFails)
{
    TempDb db("cdn_shift_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidConditions", "UPDATE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::shift_condition_ref_nos(db.path(), "10", 1, "30"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, ShiftConditionRefNosControlShiftsTheOthers)
{
    TempDb db("cdn_shift_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::shift_condition_ref_nos(db.path(), "10", 1, "30"));
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 30"), 1);
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 31"), 3);
}

// duplicate_condition returned the new uid even when the INSERT failed (host repro of the item).
TEST(StoreErrors, DuplicateConditionReturnsEmptyWhenTheInsertFails)
{
    TempDb db("cdn_dup_blocked");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidConditions", "INSERT");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "10", "30"), "");
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, DuplicateConditionControlInsertsTheCopy)
{
    TempDb db("cdn_dup_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    const std::string uid = ost_store::duplicate_condition(db.path(), "10", "30");
    REQUIRE(!uid.empty());
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = " + uid + " AND Name = 'Cdn'"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 3);
}

// COND-M04: Seed BidConditions (30) and BidTakeoffs (500, cond 30); a BEFORE DELETE trigger that raises on BidTakeoffs.
// delete_condition must return false and leave the condition, the takeoff and all dependents in place.
TEST(StoreErrors, DeleteConditionReturnsFalseAndRollsBackWhenTheTakeoffDeleteFails)
{
    TempDb db("cdn_delete_blocked_takeoffs");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidTakeoffs", "DELETE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_condition(db.path(), "10", "30"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, DeleteConditionReturnsFalseAndRollsBackWhenTheConditionDeleteFails)
{
    TempDb db("cdn_delete_blocked_row");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidConditions", "DELETE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_condition(db.path(), "10", "30"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, DeleteConditionReturnsFalseWhenADependentCleanupFails)
{
    TempDb db("cdn_delete_blocked_dependent");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("BidLaborActivity", "DELETE");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_condition(db.path(), "10", "30"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

TEST(StoreErrors, DeleteConditionControlRemovesTheConditionAndEveryDependentOfThatCondition)
{
    TempDb db("cdn_delete_control");
    REQUIRE(db.schema_ok());
    db.seed_world();

    CHECK(ost_store::delete_condition(db.path(), "10", "30"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 30"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 31"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 500"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 501"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidDimensions"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPercents"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLaborActivity"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM ConditionSetStyles"), 0);
}

// Port of tests/infrastructure/mdb/components/test_condition_operations.py::ConditionDeleteCascadeContractTests::
// test_a_failing_style_cleanup_is_reported_but_never_stops_the_delete (ConditionSetStyles failure is only logged).
TEST(StoreErrors, DeleteConditionContinuesWhenTheStyleCleanupFails)
{
    TempDb db("cdn_delete_blocked_styles");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.block("ConditionSetStyles", "DELETE");

    CHECK(ost_store::delete_condition(db.path(), "10", "30"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 30"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 31"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 500"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLaborActivity"), 0);
    // the style row itself stays: its delete failed (and only that statement was undone)
    CHECK_EQ(db.count("SELECT COUNT(*) FROM ConditionSetStyles"), 1);
}

// No Windows analog (ODBC has no such failure): SQLite can end the transaction by itself (here RAISE(ROLLBACK)). The
// tolerated style failure must then NOT let the delete carry on in autocommit mode and drop the condition alone.
TEST(StoreErrors, DeleteConditionFailsWhenTheStyleCleanupRollsTheTransactionBack)
{
    TempDb db("cdn_delete_styles_rollback");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.must("CREATE TRIGGER blk_styles_rollback BEFORE DELETE ON ConditionSetStyles "
            "BEGIN SELECT RAISE(ROLLBACK, 'blocked'); END");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_condition(db.path(), "10", "30"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

// Port of ...::test_dependent_tables_that_cannot_be_matched_are_left_alone: a dependent table the schema lacks (or that
// lacks the matching column) is skipped; the delete still succeeds and the table keeps its rows.
TEST(StoreErrors, DeleteConditionSkipsDependentTablesThatAreMissing)
{
    const char *tables[] = {"BidDimensions", "BidALines", "BidArrows", "BidPercents", "BidLaborActivity",
                            "ConditionSetStyles"};
    for (const char *table : tables)
    {
        SCOPED_CONTEXT(std::string("missing table ") + table);
        TempDb db(std::string("cdn_delete_missing_") + table);
        REQUIRE(db.schema_ok());
        db.seed_world();
        db.must(std::string("DROP TABLE [") + table + "]");

        CHECK(ost_store::delete_condition(db.path(), "10", "30"));
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 30"), 0);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 500"), 0);
    }
}

TEST(StoreErrors, DeleteConditionSkipsDependentTablesWithoutTheMatchingColumn)
{
    const char *tables[] = {"BidDimensions", "BidALines", "BidArrows", "BidPercents", "BidLaborActivity",
                            "ConditionSetStyles"};
    for (const char *table : tables)
    {
        SCOPED_CONTEXT(std::string("table without matching column ") + table);
        TempDb db(std::string("cdn_delete_nocolumn_") + table);
        REQUIRE(db.schema_ok());
        db.seed_world();
        db.must(std::string("DROP TABLE [") + table + "]");
        db.must(std::string("CREATE TABLE [") + table + "] (UID INTEGER)");
        db.must(std::string("INSERT INTO [") + table + "] VALUES (1)");

        CHECK(ost_store::delete_condition(db.path(), "10", "30"));
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 30"), 0);
        CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM [") + table + "]"), 1);
    }
}

// Windows: BidTakeoffs is optional as a table, but once present it must carry BidConditionUID/BidUID
// ("BidTakeoffs" case of test_dependent_tables_that_cannot_be_matched_are_left_alone -> False).
TEST(StoreErrors, DeleteConditionFailsWhenTheTakeoffTableLacksTheConditionColumns)
{
    TempDb db("cdn_delete_takeoffs_nocolumn");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.must("DROP TABLE BidTakeoffs");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER)");
    db.must("INSERT INTO BidTakeoffs VALUES (1)");
    const std::string before = db.snapshot(TempDb::world_tables());

    CHECK_FALSE(ost_store::delete_condition(db.path(), "10", "30"));
    CHECK_EQ(db.snapshot(TempDb::world_tables()), before);
}

// Windows (condition_operations.py _delete_condition_cascade_chunk): `if not schema.optional_table_missing("BidTakeoffs")`
// guards the takeoff delete, so a schema without BidTakeoffs (and without the tables whose delete sub-selects it) still
// deletes the condition and its other dependents.
TEST(StoreErrors, DeleteConditionSkipsAMissingTakeoffTable)
{
    TempDb db("cdn_delete_takeoffs_missing");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.must("DROP TABLE BidTakeoffs");
    db.must("DROP TABLE BidDimensions");
    db.must("DROP TABLE BidALines");
    db.must("DROP TABLE BidArrows");
    db.must("DROP TABLE BidPercents");

    CHECK(ost_store::delete_condition(db.path(), "10", "30"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 30"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 31"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLaborActivity"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM ConditionSetStyles"), 0);
}

// ---------------------------------------------------------------------------------------------------------------
// Render config (ProjectStore+Conditions.cpp, set_pref in ProjectStore_Internal.hpp)
// ---------------------------------------------------------------------------------------------------------------

TEST(StoreErrors, SaveRenderConfigReturnsFalseWhenTheWriteFails)
{
    TempDb db("render_cfg_blocked");
    REQUIRE(db.schema_ok());
    db.block("_AppPreferences", "INSERT");

    ost_domain::RenderConfig config;
    config.color_mode = ost_domain::RenderConfig::MODE_OST;
    config.grayscale_enabled = true;
    CHECK_FALSE(ost_store::save_render_config(db.path(), config));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM _AppPreferences"), 0);
}

// The two preferences are one operation: when the second write fails the first is rolled back.
TEST(StoreErrors, SaveRenderConfigIsAllOrNothing)
{
    TempDb db("render_cfg_half");
    REQUIRE(db.schema_ok());
    db.must("CREATE TRIGGER blk_gray BEFORE INSERT ON _AppPreferences WHEN NEW.Key = 'grayscale_enabled' "
            "BEGIN SELECT RAISE(ABORT, 'blocked'); END");

    ost_domain::RenderConfig config;
    config.color_mode = ost_domain::RenderConfig::MODE_OST;
    config.grayscale_enabled = true;
    CHECK_FALSE(ost_store::save_render_config(db.path(), config));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM _AppPreferences"), 0);
}

TEST(StoreErrors, SaveRenderConfigReturnsFalseWhenThePreferencesTableIsMissing)
{
    TempDb db("render_cfg_notable");
    REQUIRE(db.schema_ok());
    db.must("DROP TABLE _AppPreferences");

    ost_domain::RenderConfig config;
    CHECK_FALSE(ost_store::save_render_config(db.path(), config));
}

TEST(StoreErrors, SaveRenderConfigControlRoundTrips)
{
    TempDb db("render_cfg_control");
    REQUIRE(db.schema_ok());

    ost_domain::RenderConfig config;
    config.color_mode = ost_domain::RenderConfig::MODE_TRANSPARENT;
    config.grayscale_enabled = true;
    CHECK(ost_store::save_render_config(db.path(), config));
    ost_domain::RenderConfig loaded = ost_store::load_render_config(db.path());
    CHECK_STR_EQ(loaded.color_mode, std::string(ost_domain::RenderConfig::MODE_TRANSPARENT));
    CHECK(loaded.grayscale_enabled);
}

// ---------------------------------------------------------------------------------------------------------------
// import_project (ProjectStore+Import.cpp)
// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rolls_back_every_write_when_a_bid_row_fails
// and ::test_a_page_table_failure_rolls_back_every_earlier_table_and_is_logged (any failed write rolls the whole import
// back and import returns False).
// ---------------------------------------------------------------------------------------------------------------

TEST(StoreErrors, ImportControlWritesEveryTable)
{
    TempDb db("import_control");
    REQUIRE(db.schema_ok());

    CHECK(ost_store::import_project(db.path(), make_raw_import()));
    // Import remaps every UID (IMP-06, as the Windows importer does), so the rows are counted, not looked up by the
    // UIDs of the raw input.
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidAreas"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM CdnTypes"), 1);
}

TEST(StoreErrors, ImportFailureInABidTableRollsBackEverything)
{
    TempDb db("import_blocked_bid_table");
    REQUIRE(db.schema_ok());
    db.block("BidAreas", "INSERT");
    const std::string before = db.snapshot(import_tables());

    CHECK_FALSE(ost_store::import_project(db.path(), make_raw_import()));
    CHECK_EQ(db.snapshot(import_tables()), before);
}

TEST(StoreErrors, ImportFailureInAPageTableRollsBackEveryEarlierTable)
{
    TempDb db("import_blocked_page_table");
    REQUIRE(db.schema_ok());
    db.block("BidTakeoffs", "INSERT");
    const std::string before = db.snapshot(import_tables());

    CHECK_FALSE(ost_store::import_project(db.path(), make_raw_import()));
    CHECK_EQ(db.snapshot(import_tables()), before);
}

TEST(StoreErrors, ImportFailureOnTheBidRowRollsBackTheMasterData)
{
    TempDb db("import_blocked_bid_row");
    REQUIRE(db.schema_ok());
    db.block("Bids", "INSERT");
    const std::string before = db.snapshot(import_tables());

    CHECK_FALSE(ost_store::import_project(db.path(), make_raw_import()));
    CHECK_EQ(db.snapshot(import_tables()), before);
}

TEST(StoreErrors, ImportFailureOnAMasterDataInsertRollsBackEverything)
{
    TempDb db("import_blocked_master");
    REQUIRE(db.schema_ok());
    db.block("CdnTypes", "INSERT");
    const std::string before = db.snapshot(import_tables());

    CHECK_FALSE(ost_store::import_project(db.path(), make_raw_import()));
    CHECK_EQ(db.snapshot(import_tables()), before);
}

// import_project ignored the result of commit(): a failing COMMIT (here a deferred foreign key violated by a trigger
// at the very end) was reported as success.
TEST(StoreErrors, ImportReturnsFalseWhenTheCommitFails)
{
    TempDb db("import_commit_fails");
    REQUIRE(db.schema_ok());
    db.must("CREATE TABLE fk_parent (id INTEGER PRIMARY KEY)");
    db.must("CREATE TABLE fk_child (pid INTEGER REFERENCES fk_parent(id) DEFERRABLE INITIALLY DEFERRED)");
    db.must("CREATE TRIGGER fk_violation AFTER INSERT ON BidAreas BEGIN INSERT INTO fk_child VALUES (999); END");
    const std::string before = db.snapshot(import_tables());

    CHECK_FALSE(ost_store::import_project(db.path(), make_raw_import()));
    CHECK_EQ(db.snapshot(import_tables()), before);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM fk_child"), 0);
}
