// CF-1, CF-2, CF-3, CF-4, CF-7 (plan 5.2): ost_store::delete_condition_folders on a real SQLite file (a TempDb): the ported Windows persistence,
// contract, delete-safety and ownership tests, the real-Access scenario with the in-use rule flipped to the service rule (a folder with a used
// descendant is refused), and the data-safety cases of the plan (chunk edges, foreign folders, rollbacks, bound-variable limit, lock).
//
// Windows sources (HEAD 49a2d59):
//   tests/infrastructure/mdb/components/test_condition_folder_operations.py   :54-392 persistence, :445-593 contract, :607-813 delete safety,
//                                                                             :823-1068 the real-Access scenario (flipped, see CF-1)
//   tests/integration/mdb/test_ownership_and_atomicity.py                     :51 cross-bid batch, :154 later-chunk rollback, :279 large delete, :475 orphan
//   ost_visualizer/infrastructure/mdb/components/condition_folder_operations.py:87-142 (delete), application/services/project_write_service.py:866-953
//
// CF-1: the Windows Access layer refuses only a folder that is DIRECTLY used (condition_folder_operations.py:106-123: its scenario deletes `Mid`
// although `Leaf` holds a condition); the service layer, which the app calls, refuses every ancestor of a used folder (project_write_service.py:936-953).
// The iOS store follows the service rule, from rows inside the transaction: the Access-layer expectation is flipped where it appears.
//
// ADAPTATION of the ported fixtures: the minimal Bids tables have the extra column JobStatusUID (see ConditionFoldersTestSupport.hpp).
#include "ConditionFoldersTestSupport.hpp"

#include "Persistence/OstDatabase.hpp"

#include <functional>
#include <set>

using ost_ftest::add_condition;
using ost_ftest::add_folder;
using ost_ftest::add_folder_range;
using ost_ftest::file_exists;
using ost_ftest::folder_rows;
using ost_ftest::lock_bid;
using ost_ftest::seed_bids;
using ost_ftest::TempDb;
using ost_ftest::uid_range;
using ost_ftest::windows_folder_db;
using R = ost_store::ConditionFolderRefusal;

namespace
{
    ost_store::ConditionFolderResult del(const TempDb &db, const std::vector<std::string> &uids, const std::string &bid = "1")
    {
        return ost_store::delete_condition_folders(db.path(), bid, uids);
    }

    std::string join(const std::vector<std::string> &items) { return ost_ftest::join(items); }

    // The Windows `_folder_db` of the delete tests with a request of `count` plain root folders (UID, BidUID 1, 'F', -1).
    void many_folders(const TempDb &db, int count = 120)
    {
        windows_folder_db(db);
        db.must("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < " + std::to_string(count) +
                ") INSERT INTO BidConditionFolders (UID, BidUID, ParentUID, Name, ExpandState) SELECT i, 1, NULL, 'F', -1 FROM n");
    }

    // The iOS-schema world of the safety tests: bid 1 has the chain 1 'Top' > 2 'Mid' > 3 'Leaf', the root folder 4 'Spare', conditions 30 (no
    // folder) and 31 (in folder 3: Leaf); bid 2 has the folder 20 'Other' and condition 40 (in 20).
    void seed_chain(const TempDb &db)
    {
        seed_bids(db);
        add_folder(db, 1, 1, "Top");
        add_folder(db, 2, 1, "Mid", "1");
        add_folder(db, 3, 1, "Leaf", "2");
        add_folder(db, 4, 1, "Spare");
        add_folder(db, 20, 2, "Other");
        add_condition(db, 30, 1);
        add_condition(db, 31, 1, "3");
        add_condition(db, 40, 2, "20");
    }
}

// =================================================================================================================================
// the ported Windows persistence tests (test_condition_folder_operations.py:54-392)
// =================================================================================================================================

// Port of :54 test_delete_condition_folder_refuses_folder_used_by_conditions: folders 1 'Used', 2 'Unused', condition 10 in folder 1, NO foreign key
// (only the in-use guard can refuse): delete ["1","2"] is refused, both rows remain. (The table has no ParentUID column.)
TEST(ConditionFolderDelete, AFolderUsedByAConditionRefusesTheWholeBatch)
{
    TempDb db("fdel_used", false);
    windows_folder_db(db, "UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT", {}, "CREATE TABLE BidConditions (UID INTEGER PRIMARY KEY, BidConditionFolderUID INTEGER)");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (1, 1, 'Used')");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (2, 1, 'Unused')");
    db.must("INSERT INTO BidConditions (UID, BidConditionFolderUID) VALUES (10, 1)");
    const std::string before = db.snapshot_all();

    const auto result = del(db, {"1", "2"});
    CHECK_FOLDER_REFUSED(result, R::InUse);
    CHECK_STR_EQ(join(result.blocked), "1");
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.list("SELECT UID || ':' || Name FROM BidConditionFolders ORDER BY UID"), "1:Used,2:Unused");
}

// Port of :91 test_delete_condition_folder_in_use_check_covers_every_uid_chunk: 120 folders, one condition in folder 120: deleting all is refused,
// 120 rows remain.
TEST(ConditionFolderDelete, TheInUseCheckCoversEveryFolderOfALargeBatch)
{
    TempDb db("fdel_every_chunk", false);
    windows_folder_db(db, "UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT", {}, "CREATE TABLE BidConditions (UID INTEGER PRIMARY KEY, BidConditionFolderUID INTEGER)");
    db.must("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 120) INSERT INTO BidConditionFolders SELECT i, 1, 'F' FROM n");
    db.must("INSERT INTO BidConditions VALUES (10, 120)");

    const auto result = del(db, uid_range(1, 120));
    CHECK_FOLDER_REFUSED(result, R::InUse);
    CHECK_STR_EQ(join(result.blocked), "120");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 120);
}

// Port of :121 test_delete_condition_folder_allows_unused_folder: a condition column that REFERENCES the folder table (foreign keys on in Windows'
// connection): deleting the unused folder 1 succeeds and removes it.
TEST(ConditionFolderDelete, AnUnusedFolderIsDeletedEvenWithAForeignKeyToIt)
{
    TempDb db("fdel_unused_fk", false);
    windows_folder_db(db, "UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT", {},
                      "CREATE TABLE BidConditions (UID INTEGER PRIMARY KEY, BidConditionFolderUID INTEGER REFERENCES BidConditionFolders(UID))");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (1, 1, 'Unused')");

    CHECK_FOLDER_OK(del(db, {"1"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 0);
}

// Port of :148 test_delete_condition_folder_reparents_surviving_child_to_root: 1 'Parent', 2 'Child' of 1; delete ["1"] -> ok; rows [(2, NULL)]. (The
// condition table has no BidConditionFolderUID column at all.)
TEST(ConditionFolderDelete, ASurvivingChildMovesToTheRoot)
{
    TempDb db("fdel_reparent", false);
    windows_folder_db(db, "UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT, ParentUID INTEGER", {}, "CREATE TABLE BidConditions (UID INTEGER PRIMARY KEY)");
    db.must("INSERT INTO BidConditionFolders VALUES (1, 1, 'Parent', NULL)");
    db.must("INSERT INTO BidConditionFolders VALUES (2, 1, 'Child', 1)");

    CHECK_FOLDER_OK(del(db, {"1"}));
    CHECK_EQ(db.list("SELECT UID || ':' || ifnull(ParentUID, 'NULL') FROM BidConditionFolders ORDER BY UID"), "2:NULL");
}

// Port of :350 test_delete_condition_folders_empty_request_is_a_noop_success: [] -> true, the row remains; the table has only a UID column and the
// database has no Bids table: nothing is read. Windows opens no connection for an empty list (:88-89): a database path that does not exist works too
// and is not created.
TEST(ConditionFolderDelete, AnEmptyRequestIsASuccessThatTouchesNothing)
{
    TempDb db("fdel_empty", false);
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER)");
    db.must("INSERT INTO BidConditionFolders VALUES (1)");
    const std::string before = db.snapshot_all();

    const auto result = del(db, {});
    CHECK_FOLDER_OK(result);
    CHECK(result.blocked.empty());
    CHECK_EQ(db.snapshot_all(), before);

    const std::string missing = ost_test::temp_path("folders_empty_request_" + ost_ctest::run_token() + ".db");
    CHECK_FOLDER_OK(ost_store::delete_condition_folders(missing, "1", {}));
    CHECK_FALSE(file_exists(missing));
}

// Port of :359 test_delete_condition_folders_rejects_invalid_uid_without_deleting: ["1","bad"] -> refused, the row remains.
TEST(ConditionFolderDelete, AnInvalidUidRefusesTheWholeRequest)
{
    TempDb db("fdel_invalid", false);
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER)");
    db.must("INSERT INTO BidConditionFolders VALUES (1)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(del(db, {"1", "bad"}), R::InvalidUid);
    CHECK_EQ(db.snapshot_all(), before);
}

// Port of :372 test_delete_condition_folders_rejects_folders_from_different_bids: folders (1, bid 1) and (2, bid 2): ["1","2"] is refused, both stay.
// (Windows names the batch incoherent; iOS names the folder that is not the bid's.)
TEST(ConditionFolderDelete, FoldersOfDifferentBidsRefuseTheBatch)
{
    TempDb db("fdel_two_bids", false);
    windows_folder_db(db, "UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT", {2});
    db.must("INSERT INTO BidConditionFolders VALUES (1, 1, 'F')");
    db.must("INSERT INTO BidConditionFolders VALUES (2, 2, 'F')");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(del(db, {"1", "2"}), R::FolderOfAnotherBid);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.list("SELECT UID FROM BidConditionFolders ORDER BY UID"), "1,2");
}

// =================================================================================================================================
// the ported contract tests (:445-593)
// =================================================================================================================================

// Port of :523 test_rename_and_delete_need_the_columns_they_write (the delete half): a table without UID refuses the delete.
TEST(ConditionFolderDelete, ATableWithoutUidRefusesTheDelete)
{
    TempDb db("fdel_no_uid_column", false);
    windows_folder_db(db, "BidUID INTEGER, Name TEXT");
    const std::string before = db.snapshot_all();
    CHECK_FOLDER_REFUSED(del(db, {"5"}), R::SchemaUnsupported);
    CHECK_EQ(db.snapshot_all(), before);
}

// Port of :549 test_failures_return_false_and_follow_the_error_policy (the delete half): ["x"] -> refused (InvalidUid), ["99"] -> refused (the
// folder does not exist). The strict-mode raise of the Windows test is Windows-only.
TEST(ConditionFolderDelete, FailuresAreRefusalsNotExceptions)
{
    TempDb db("fdel_failures", false);
    windows_folder_db(db);
    CHECK_FOLDER_REFUSED(del(db, {"x"}), R::InvalidUid);
    CHECK_FOLDER_REFUSED(del(db, {"99"}), R::FolderNotFound);
}

// Port of :570 test_a_folder_in_use_is_refused_with_false: folder 5 'Used' with a condition in it: refused, one folder row remains.
TEST(ConditionFolderDelete, AFolderInUseIsRefused)
{
    TempDb db("fdel_in_use", false);
    windows_folder_db(db);
    db.must("INSERT INTO BidConditionFolders VALUES (5, 1, NULL, 'Used', -1)");
    db.must("INSERT INTO BidConditions VALUES (1, 5)");

    const auto result = del(db, {"5"});
    CHECK_FOLDER_REFUSED(result, R::InUse);
    CHECK_STR_EQ(join(result.blocked), "5");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 1);
}

// Port of :582 test_an_unused_folder_is_deleted_and_its_children_move_to_the_root: 5 'Gone', 6 'Kid' of 5: delete ["5"] -> ok; [(6, NULL)].
TEST(ConditionFolderDelete, AnUnusedFolderIsDeletedAndItsChildrenMoveToTheRoot)
{
    TempDb db("fdel_unused", false);
    windows_folder_db(db);
    db.must("INSERT INTO BidConditionFolders VALUES (5, 1, NULL, 'Gone', -1)");
    db.must("INSERT INTO BidConditionFolders VALUES (6, 1, 5, 'Kid', -1)");

    CHECK_FOLDER_OK(del(db, {"5"}));
    CHECK_EQ(db.list("SELECT UID || ':' || ifnull(ParentUID, 'NULL') FROM BidConditionFolders"), "6:NULL");
}

// =================================================================================================================================
// the ported delete-safety tests (:607-813)
// =================================================================================================================================

// Port of :608 test_a_uid_collection_that_cannot_be_read_is_refused_before_a_connection_opens (the ["5","bad"] case; a scalar 5 has no C++
// counterpart): refused, row 5 kept, and no database is opened: with a database path that does not exist the answer is the same and no file appears.
TEST(ConditionFolderDelete, AnUnreadableUidListIsRefusedBeforeTheDatabaseIsOpened)
{
    TempDb db("fdel_unreadable_list", false);
    windows_folder_db(db);
    db.must("INSERT INTO BidConditionFolders VALUES (5, 1, NULL, 'Keep', -1)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(del(db, {"5", "bad"}), R::InvalidUid);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.list("SELECT UID FROM BidConditionFolders"), "5");

    const std::string missing = ost_test::temp_path("folders_unreadable_" + ost_ctest::run_token() + ".db");
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(missing, "1", {"5", "bad"}), R::InvalidUid);
    CHECK_FALSE(file_exists(missing));
}

// Port of :631 test_refusal_and_failure_logs_name_the_operation_the_uids_and_the_database (the Python log text is not portable: the OUTCOMES are):
// delete ["99","98"] on an empty table -> refused (no folder); ["x"] -> InvalidUid; ["6","5","6"] with 5 used and 6 free -> refused as in use: the
// duplicate is dropped (Windows logs "[6, 5]") and the blocked folder is 5 only.
TEST(ConditionFolderDelete, TheOutcomesOfTheRefusalLogCases)
{
    {
        TempDb db("fdel_log_missing", false);
        windows_folder_db(db);
        CHECK_FOLDER_REFUSED(del(db, {"99", "98"}), R::FolderNotFound);
        CHECK_FOLDER_REFUSED(del(db, {"x"}), R::InvalidUid);
    }
    {
        TempDb db("fdel_log_dedup", false);
        windows_folder_db(db);
        db.must("INSERT INTO BidConditionFolders VALUES (5, 1, NULL, 'Used', -1)");
        db.must("INSERT INTO BidConditionFolders VALUES (6, 1, NULL, 'Free', -1)");
        db.must("INSERT INTO BidConditions VALUES (1, 5)");
        const std::string before = db.snapshot_all();

        const auto result = del(db, {"6", "5", "6"});
        CHECK_FOLDER_REFUSED(result, R::InUse);
        CHECK_STR_EQ(join(result.blocked), "5");
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// Port of :680 test_a_folder_in_use_blocks_the_delete_wherever_it_sits_in_a_batch: 120 folders, a condition in uid 1/49/50/51/75/100/101/120 each time:
// the batch is refused and all 120 rows remain. (The chunk boundaries are an Access detail; the cases also test any chunking of the iOS store.)
// Windows names every folder of the batch in its log; the iOS result names the folders in use: the one folder.
TEST(ConditionFolderDelete, AFolderInUseBlocksTheDeleteWhereverItSitsInABatch)
{
    for (const int in_use : {1, 49, 50, 51, 75, 100, 101, 120})
    {
        SCOPED_CONTEXT("in use " + std::to_string(in_use));
        TempDb db("fdel_in_use_position", false);
        many_folders(db);
        db.must("INSERT INTO BidConditions VALUES (1, " + std::to_string(in_use) + ")");
        const std::string before = db.snapshot_all();

        const auto result = del(db, uid_range(1, 120));
        CHECK_FOLDER_REFUSED(result, R::InUse);
        CHECK_STR_EQ(join(result.blocked), std::to_string(in_use));
        CHECK_EQ(db.snapshot_all(), before);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 120);
    }
}

// Port of :704 test_children_of_every_deleted_folder_move_to_the_root_across_chunks: parents 1..60 and children 101..160 (child uid+100 of parent
// uid); delete 1..60 -> ok; the children remain with a NULL parent.
TEST(ConditionFolderDelete, ChildrenOfEveryDeletedFolderMoveToTheRootAcrossChunks)
{
    TempDb db("fdel_children_chunks", false);
    windows_folder_db(db);
    db.must("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 60) INSERT INTO BidConditionFolders SELECT i, 1, NULL, 'P', -1 FROM n");
    db.must("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 60) INSERT INTO BidConditionFolders SELECT i + 100, 1, i, 'C', -1 FROM n");

    CHECK_FOLDER_OK(del(db, uid_range(1, 60)));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 60);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID BETWEEN 101 AND 160 AND ParentUID IS NULL"), 60);
}

// Replacement of :725 test_every_statement_of_a_large_delete_binds_at_most_one_chunk_of_uids (the Access chunk size does not port): a delete of more
// folders than SQLite's bound-variable limit succeeds completely (all-or-nothing), and one used folder among them refuses all of them.
TEST(ConditionFolderDelete, ADeleteLargerThanTheBoundVariableLimitIsCompleteOrNothing)
{
    TempDb db("fdel_huge");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder_range(db, 1, 40000, 1);
    add_folder(db, 50000, 1, "Child of the last", "40000");
    add_condition(db, 30, 1, "25000");
    const std::string before = db.snapshot_all();

    const auto refused = del(db, uid_range(1, 40000));
    CHECK_FOLDER_REFUSED(refused, R::InUse);
    CHECK_STR_EQ(join(refused.blocked), "25000");
    CHECK_EQ(db.snapshot_all(), before);

    db.must("UPDATE BidConditions SET BidConditionFolderUID = NULL WHERE UID = 30");
    CHECK_FOLDER_OK(del(db, uid_range(1, 40000)));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 1);
    CHECK_EQ(db.text("SELECT UID || ':' || ifnull(ParentUID, 'NULL') FROM BidConditionFolders"), "50000:NULL");
}

// Port of :748 test_a_folder_of_another_bid_in_a_late_chunk_refuses_the_whole_batch: 120 folders, uid 110 belongs to bid 2: refused, 120 rows remain.
// Extended to every chunk edge: the foreign folder at uid 1, 50, 51, 100, 101, 110, 120 each refuses the batch and no row is touched.
TEST(ConditionFolderDelete, AFolderOfAnotherBidAnywhereInABatchRefusesTheWholeBatch)
{
    for (const int foreign : {1, 50, 51, 100, 101, 110, 120})
    {
        SCOPED_CONTEXT("foreign folder " + std::to_string(foreign));
        TempDb db("fdel_foreign_chunk", false);
        windows_folder_db(db, ost_ftest::kWindowsFolderColumns, {2});
        db.must("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 120) INSERT INTO BidConditionFolders SELECT i, 1, NULL, 'F', -1 FROM n");
        db.must("UPDATE BidConditionFolders SET BidUID = 2 WHERE UID = " + std::to_string(foreign));
        const std::string before = db.snapshot_all();

        CHECK_FOLDER_REFUSED(del(db, uid_range(1, 120)), R::FolderOfAnotherBid);
        CHECK_EQ(db.snapshot_all(), before);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 120);
    }
}

// A uid that is not a folder at all, deep in a large list, refuses the whole batch too.
TEST(ConditionFolderDelete, AnUnknownUidDeepInABatchRefusesTheWholeBatch)
{
    TempDb db("fdel_unknown_deep", false);
    many_folders(db);
    const std::string before = db.snapshot_all();
    std::vector<std::string> request = uid_range(1, 120);
    request.insert(request.begin() + 100, "9999");

    CHECK_FOLDER_REFUSED(del(db, request), R::FolderNotFound);
    CHECK_EQ(db.snapshot_all(), before);
}

// Port of :762 test_a_refused_delete_leaves_the_folder_hierarchy_untouched: Top(1), Child(2 of 1), Used(3 with a condition); delete ["1","3"] is
// refused; rows, parents and names are unchanged.
TEST(ConditionFolderDelete, ARefusedDeleteLeavesTheHierarchyUntouched)
{
    TempDb db("fdel_refused_hierarchy", false);
    windows_folder_db(db);
    db.must("INSERT INTO BidConditionFolders VALUES (1, 1, NULL, 'Top', -1)");
    db.must("INSERT INTO BidConditionFolders VALUES (2, 1, 1, 'Child', -1)");
    db.must("INSERT INTO BidConditionFolders VALUES (3, 1, NULL, 'Used', -1)");
    db.must("INSERT INTO BidConditions VALUES (1, 3)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(del(db, {"1", "3"}), R::InUse);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.list("SELECT UID || ':' || ifnull(ParentUID, 'NULL') || ':' || Name FROM BidConditionFolders ORDER BY UID"), "1:NULL:Top,2:1:Child,3:NULL:Used");
}

// Port of :781 test_folders_can_be_deleted_from_a_file_without_a_condition_table: no BidConditions table; delete ["5"] -> ok; child 6 gets a NULL
// parent. Extended: a conditions table without the folder column and a folder table without ParentUID also delete.
TEST(ConditionFolderDelete, FoldersAreDeletedFromAFileWithoutAConditionTable)
{
    {
        TempDb db("fdel_no_condition_table", false);
        windows_folder_db(db, "UID INTEGER, BidUID INTEGER, ParentUID INTEGER, Name TEXT", {}, "");
        db.must("INSERT INTO BidConditionFolders VALUES (5, 1, NULL, 'Gone')");
        db.must("INSERT INTO BidConditionFolders VALUES (6, 1, 5, 'Kid')");

        CHECK_FOLDER_OK(del(db, {"5"}));
        CHECK_EQ(db.list("SELECT UID || ':' || ifnull(ParentUID, 'NULL') FROM BidConditionFolders"), "6:NULL");
    }
    {
        TempDb db("fdel_no_parent_column", false);
        windows_folder_db(db, "UID INTEGER, BidUID INTEGER, Name TEXT", {}, "CREATE TABLE BidConditions (UID INTEGER)");
        db.must("INSERT INTO BidConditionFolders VALUES (5, 1, 'Gone')");
        db.must("INSERT INTO BidConditionFolders VALUES (6, 1, 'Stay')");
        CHECK_FOLDER_OK(del(db, {"5"}));
        CHECK_EQ(db.list("SELECT UID FROM BidConditionFolders"), "6");
    }
}

// Port of :798 test_deleting_a_parent_and_its_grandchild_together_keeps_the_child: Top(1), Mid(2 of 1), Leaf(3 of 2); delete ["1","3"] -> ok; only
// (2, NULL, 'Mid') remains.
TEST(ConditionFolderDelete, DeletingAParentAndItsGrandchildTogetherKeepsTheChild)
{
    TempDb db("fdel_parent_and_grandchild", false);
    windows_folder_db(db);
    db.must("INSERT INTO BidConditionFolders VALUES (1, 1, NULL, 'Top', -1)");
    db.must("INSERT INTO BidConditionFolders VALUES (2, 1, 1, 'Mid', -1)");
    db.must("INSERT INTO BidConditionFolders VALUES (3, 1, 2, 'Leaf', -1)");

    CHECK_FOLDER_OK(del(db, {"1", "3"}));
    CHECK_EQ(db.list("SELECT UID || ':' || ifnull(ParentUID, 'NULL') || ':' || Name FROM BidConditionFolders"), "2:NULL:Mid");
}

// =================================================================================================================================
// ownership and atomicity (tests/integration/mdb/test_ownership_and_atomicity.py)
// =================================================================================================================================

// Port of :51 test_bid_owned_bulk_deletes_reject_cross_bid_batches (the folder fixture `BidConditionFolders (UID, BidUID, ParentUID)`, rows (7, bid 1) and
// (8, bid 2), both bids exist): delete ["7","8"] is refused and both stay; the control ["7"] is accepted and only 8 remains.
TEST(ConditionFolderDelete, ACrossBidBatchIsRefusedAndTheSameBidControlIsAccepted)
{
    TempDb db("fdel_cross_bid", false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("INSERT INTO Bids (UID) VALUES (2)");
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER, BidUID INTEGER, ParentUID INTEGER)");
    db.must("INSERT INTO BidConditionFolders VALUES (7, 1, NULL)");
    db.must("INSERT INTO BidConditionFolders VALUES (8, 2, NULL)");

    CHECK_FOLDER_REFUSED(del(db, {"7", "8"}), R::FolderOfAnotherBid);
    CHECK_EQ(db.list("SELECT UID FROM BidConditionFolders ORDER BY UID"), "7,8");
    CHECK_FOLDER_OK(del(db, {"7"}));
    CHECK_EQ(db.list("SELECT UID FROM BidConditionFolders ORDER BY UID"), "8");
}

// Port of :154 test_later_chunk_delete_failure_rolls_back_page_condition_and_folder_batches (the folder part, :242-277): folders 1..51 and folder 100
// (a child of 1); a trigger aborts the DELETE of uid 51; deleting 1..51 is refused, 52 rows remain, folder 100 still has parent 1 (the re-parenting
// was rolled back); without the trigger the same batch is accepted and leaves [(100, NULL)].
TEST(ConditionFolderDelete, ALaterFailureRollsBackTheReparentingToo)
{
    TempDb db("fdel_later_chunk", false);
    windows_folder_db(db, "UID INTEGER PRIMARY KEY, BidUID INTEGER, ParentUID INTEGER");
    db.must("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 51) INSERT INTO BidConditionFolders (UID, BidUID) SELECT i, 1 FROM n");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, ParentUID) VALUES (100, 1, 1)");
    db.must("CREATE TRIGGER fail_second_BidConditionFolders BEFORE DELETE ON BidConditionFolders WHEN OLD.UID = 51 BEGIN SELECT RAISE(ABORT, 'later chunk'); END");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(del(db, uid_range(1, 51)), R::WriteFailed);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 52);
    CHECK_EQ(db.count("SELECT ParentUID FROM BidConditionFolders WHERE UID = 100"), 1);
    CHECK_EQ(db.snapshot_all(), before);

    // control: without the failing trigger the same batch commits, clears the reference and keeps only the unrelated row
    db.must("DROP TRIGGER fail_second_BidConditionFolders");
    CHECK_FOLDER_OK(del(db, uid_range(1, 51)));
    CHECK_EQ(db.list("SELECT UID || ':' || ifnull(ParentUID, 'NULL') FROM BidConditionFolders"), "100:NULL");
}

// Port of :279 test_large_condition_and_folder_deletes_stay_below_access_parameter_limit (the folder part; the Access parameter-limit double does not
// port): 256 folders are deleted completely.
TEST(ConditionFolderDelete, TwoHundredFiftySixFoldersAreDeletedCompletely)
{
    TempDb db("fdel_256", false);
    windows_folder_db(db, "UID INTEGER, BidUID INTEGER, ParentUID INTEGER");
    db.must("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 256) INSERT INTO BidConditionFolders SELECT i, 1, NULL FROM n");

    CHECK_FOLDER_OK(del(db, uid_range(1, 256)));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 0);
}

// Port of :475 test_orphan_layer_and_folder_mutations_reject_before_write (the delete half): folder 8 belongs to bid 99, which has no Bids row: the
// delete is refused (Windows "Bids has no row for UID 99") and the row stays; once the owning bid exists it is deleted.
TEST(ConditionFolderDelete, AFolderOfABidWithoutARowIsRefusedBeforeTheWrite)
{
    TempDb db("fdel_orphan", false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER, BidUID INTEGER, Name TEXT, ParentUID INTEGER)");
    db.must("INSERT INTO BidConditionFolders VALUES (8, 99, 'Orphan folder', NULL)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(del(db, {"8"}, "99"), R::BidNotFound);
    CHECK_EQ(db.snapshot_all(), before);

    db.must("INSERT INTO Bids (UID) VALUES (99)");
    CHECK_FOLDER_OK(del(db, {"8"}, "99"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 0);
}

// =================================================================================================================================
// the real-Access scenario (:823-1046), sequential, with the Access-layer expectation flipped (CF-1)
// =================================================================================================================================

// Port of ConditionFolderOperationsRealAccessTests._exercise_real_access: bids 201 and 202; folders Top > Mid > Leaf (bid 201), Other and Spare (bid 202);
// conditions 301 in Leaf, 302 in Other, 303 without folder. Refused inserts add nothing; rename works, rename of 999 is refused; refused deletes
// [Leaf], [Mid, Leaf], [Top, Spare] (two bids) and ["999"]; an injected failure rolls the re-parenting back; a batch of 120 with a used folder at the
// first or last uid is refused and the two children keep their parents; after the condition is gone the batch deletes.
// FLIPPED (CF-1, :961-968): Windows' Access layer deletes `Mid` although `Leaf` holds a condition; the iOS store refuses [Mid] and [Top] while Leaf
// holds condition 301. The scenario then moves the condition out (Move to Folder) and goes on as Windows does.
TEST(ConditionFolderDelete, TheRealAccessScenarioWithTheFlippedInUseRule)
{
    TempDb db("fdel_scenario");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (201, 1, 'a', 201)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (202, 1, 'b', 202)");

    auto created = [&db](const std::string &bid, const std::string &name, const std::string &parent = "")
    {
        const auto result = ost_store::create_condition_folder(db.path(), bid, name, parent);
        CHECK_FOLDER_OK(result);
        return result.uid;
    };
    const std::string top = created("201", "Top");
    const std::string mid = created("201", "Mid", top);
    const std::string leaf = created("201", "Leaf", mid);
    const std::string other = created("202", "Other");
    const std::string spare = created("202", "Spare");
    CHECK_EQ(std::set<std::string>({top, mid, leaf, other, spare}).size(), size_t(5)); // distinct folder uids
    add_condition(db, 301, 201, leaf);
    add_condition(db, 302, 202, other);
    add_condition(db, 303, 201);

    auto state = [&db]()
    {
        return folder_rows(db) + "#" + db.list("SELECT UID || ':' || quote(BidConditionFolderUID) FROM BidConditions ORDER BY UID");
    };
    const std::string seed = state();

    // inserts that name an absent bid, an absent parent or a parent of another bid are refused and add nothing
    CHECK_FOLDER_REFUSED(ost_store::create_condition_folder(db.path(), "999", "N", ""), R::BidNotFound);
    CHECK_FOLDER_REFUSED(ost_store::create_condition_folder(db.path(), "201", "N", "999"), R::ParentNotFound);
    CHECK_FOLDER_REFUSED(ost_store::create_condition_folder(db.path(), "201", "N", other), R::ParentOfAnotherBid);
    CHECK_EQ(state(), seed);

    CHECK_FOLDER_OK(ost_store::rename_condition_folder(db.path(), "201", top, "Renamed"));
    CHECK_FOLDER_REFUSED(ost_store::rename_condition_folder(db.path(), "201", "999", "X"), R::FolderNotFound);
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = " + top), "Renamed");
    const std::string after_rename = state();

    // refusals: a folder holding conditions, a batch containing one, a batch spanning two bids and an absent folder delete nothing
    CHECK_FOLDER_REFUSED(del(db, {leaf}, "201"), R::InUse);
    CHECK_FOLDER_REFUSED(del(db, {mid, leaf}, "201"), R::InUse);
    CHECK_FOLDER_REFUSED(del(db, {top, spare}, "201"), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(del(db, {"999"}, "201"), R::FolderNotFound);
    CHECK_EQ(state(), after_rename);

    // FLIPPED (CF-1): Windows deletes Mid here (True); the service rule refuses it because Leaf holds condition 301
    const auto mid_refused = del(db, {mid}, "201");
    CHECK_FOLDER_REFUSED(mid_refused, R::InUse);
    CHECK_STR_EQ(join(mid_refused.blocked), mid);
    CHECK_FOLDER_REFUSED(del(db, {top}, "201"), R::InUse);
    CHECK_EQ(state(), after_rename);

    // Move to Folder takes the condition out of Leaf; a failure after that, in the delete of Mid, rolls the re-parenting back
    CHECK_FOLDER_OK(ost_store::move_condition_to_folder(db.path(), "201", "301", ""));
    const std::string moved = state();
    const std::string trigger = db.block("BidConditionFolders", "DELETE");
    CHECK_FOLDER_REFUSED(del(db, {mid}, "201"), R::WriteFailed);
    CHECK_EQ(state(), moved);
    db.unblock(trigger);

    // deleting a folder removes only it: the child folder, the grandchild and every condition survive, the child moves to the root
    CHECK_FOLDER_OK(del(db, {mid}, "201"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID = " + mid), 0);
    CHECK_EQ(db.text("SELECT quote(ParentUID) FROM BidConditionFolders WHERE UID = " + leaf), "NULL");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 3);
    CHECK_FOLDER_OK(del(db, {top}, "201"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID = " + top), 0);

    // a batch larger than one chunk is checked and applied completely: 120 bulk folders, children under the first and the last one
    db.must("WITH RECURSIVE n(i) AS (SELECT 5000 UNION ALL SELECT i + 1 FROM n WHERE i < 5119) INSERT INTO BidConditionFolders (UID, BidUID, Name) SELECT i, 201, 'bulk' FROM n");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, ParentUID, Name) VALUES (6000, 201, 5000, 'child')");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, ParentUID, Name) VALUES (6001, 201, 5119, 'child')");
    const std::vector<std::string> batch = uid_range(5000, 5119);
    for (const int position : {5000, 5119})
    {
        SCOPED_CONTEXT("bulk in use at " + std::to_string(position));
        add_condition(db, 399, 201, std::to_string(position));
        const auto refused = del(db, batch, "201");
        CHECK_FOLDER_REFUSED(refused, R::InUse);
        CHECK_STR_EQ(join(refused.blocked), std::to_string(position));
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID >= 5000"), 122);
        CHECK_EQ(db.list("SELECT ParentUID FROM BidConditionFolders WHERE UID IN (6000, 6001) ORDER BY UID"), "5000,5119"); // children stay put when refused
        db.must("DELETE FROM BidConditions WHERE UID = 399");
    }
    CHECK_FOLDER_OK(del(db, batch, "201"));
    CHECK_EQ(db.list("SELECT UID || ':' || quote(ParentUID) FROM BidConditionFolders WHERE UID >= 5000 ORDER BY UID"), "6000:NULL,6001:NULL");
}

// =================================================================================================================================
// the in-use rule from database rows (CF-1, CF-2; plan 5.2), the re-parenting and what is never touched
// =================================================================================================================================

// CF-1: a folder is in use when it holds a condition or when ANY folder below it does (the service rule). The result names the blocked folders in
// request order; nothing is deleted.
TEST(ConditionFolderDelete, AFolderWithAUsedDescendantIsRefused)
{
    TempDb db("fdel_descendant");
    REQUIRE(db.schema_ok());
    seed_chain(db); // condition 31 sits in Leaf (3), Leaf is below Mid (2), Mid is below Top (1)
    const std::string before = db.snapshot_all();

    struct Row
    {
        std::vector<std::string> request;
        std::string blocked;
    };
    const std::vector<Row> rows = {
        {{"3"}, "3"},          {{"2"}, "2"},          {{"1"}, "1"},          {{"1", "2", "3"}, "1,2,3"}, {{"3", "2", "1"}, "3,2,1"},
        {{"2", "1"}, "2,1"},   {{"4", "1"}, "1"},     {{"1", "4", "3"}, "1,3"}, {{"3", "3"}, "3"},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("request [" + join(row.request) + "]");
        const auto result = del(db, row.request);
        CHECK_FOLDER_REFUSED(result, R::InUse);
        CHECK_STR_EQ(join(result.blocked), row.blocked);
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// The descendants of the condition's own folder are not in use, and a batch that mixes a deletable and a blocked folder is refused as a whole by the
// store (the service deletes the deletable ones after the plan; the store is all-or-nothing, Access layer): the condition is in Mid (2): Leaf (3)
// can go alone, Top (1) and Mid are blocked.
TEST(ConditionFolderDelete, TheDescendantsOfTheConditionsOwnFolderAreNotInUse)
{
    TempDb db("fdel_descendant_free");
    REQUIRE(db.schema_ok());
    seed_chain(db);
    db.must("UPDATE BidConditions SET BidConditionFolderUID = 2 WHERE UID = 31");
    const std::string before = db.snapshot_all();

    const auto mixed = del(db, {"1", "3"});
    CHECK_FOLDER_REFUSED(mixed, R::InUse);
    CHECK_STR_EQ(join(mixed.blocked), "1");
    CHECK_EQ(db.snapshot_all(), before);

    CHECK_FOLDER_OK(del(db, {"3"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID = 3"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 31"), 1); // the condition is never touched
    CHECK_EQ(db.text("SELECT BidConditionFolderUID FROM BidConditions WHERE UID = 31"), "2");
}

// CF-2: the in-use rule reads the DATABASE rows of the call, not a screen's memory: a condition moved into a folder behind the screen's back (an
// import, another connection) blocks the delete, and a condition moved out unblocks it.
TEST(ConditionFolderDelete, TheInUseRuleReadsTheDatabaseNotTheScreen)
{
    TempDb db("fdel_reads_database");
    REQUIRE(db.schema_ok());
    seed_chain(db);

    CHECK_FOLDER_OK(del(db, {"4"})); // Spare is empty: deletable
    db.must("UPDATE BidConditions SET BidConditionFolderUID = 1 WHERE UID = 30"); // a condition appears in Top
    const std::string before = db.snapshot_all();
    CHECK_FOLDER_REFUSED(del(db, {"1"}), R::InUse);
    CHECK_EQ(db.snapshot_all(), before);
    db.must("UPDATE BidConditions SET BidConditionFolderUID = NULL WHERE UID IN (30, 31)");
    CHECK_FOLDER_OK(del(db, {"1"}));
}

// A condition of ANOTHER bid whose folder uid names a requested folder also blocks it (a stray reference; a safe superset of both Windows checks, the
// Access layer reads BidConditions of every bid: condition_folder_operations.py:113-117).
TEST(ConditionFolderDelete, AConditionOfAnotherBidReferencingTheFolderBlocksIt)
{
    TempDb db("fdel_other_bid_reference");
    REQUIRE(db.schema_ok());
    seed_chain(db);
    db.must("UPDATE BidConditions SET BidConditionFolderUID = 4 WHERE UID = 40"); // bid 2's condition points at bid 1's folder 4
    const std::string before = db.snapshot_all();

    const auto result = del(db, {"4"});
    CHECK_FOLDER_REFUSED(result, R::InUse);
    CHECK_STR_EQ(join(result.blocked), "4");
    CHECK_EQ(db.snapshot_all(), before);
}

// CF-3: the direct children of a deleted folder get ParentUID NULL (the TOP level, not the deleted folder's own parent); grandchildren keep their
// parent; a folder of ANOTHER bid whose ParentUID names a deleted folder is set to NULL too (Windows' update is not bid-scoped, :124-131); the
// conditions and every other table are untouched. Top(1) > Mid(2) > Leaf(3); a stray folder 30 of bid 2 has ParentUID 1.
TEST(ConditionFolderDelete, OnlyTheDirectChildrenMoveToTheTopLevelAndNothingElseChanges)
{
    TempDb db("fdel_reparent_direct");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 1, 1, "Top");
    add_folder(db, 2, 1, "Mid", "1");
    add_folder(db, 3, 1, "Leaf", "2");
    add_folder(db, 5, 1, "Sibling of Mid", "1");
    add_folder(db, 6, 1, "Under sibling", "5");
    add_folder(db, 30, 2, "Stray", "1");
    add_folder(db, 31, 2, "Stray child", "30");
    add_condition(db, 30, 1);
    const std::string conditions_before = db.dump("BidConditions");
    const std::string bids_before = db.dump("Bids");

    CHECK_FOLDER_OK(del(db, {"1"}));
    CHECK_EQ(db.list("SELECT UID || ':' || quote(ParentUID) FROM BidConditionFolders ORDER BY UID"), "2:NULL,3:2,5:NULL,6:5,30:NULL,31:30");
    CHECK_EQ(db.dump("BidConditions"), conditions_before);
    CHECK_EQ(db.dump("Bids"), bids_before);
    // the surviving rows keep every other column
    CHECK_EQ(db.row("BidConditionFolders", "2", {"ParentUID"}), "UID=2 BidUID=1 Name='Mid' Description=NULL ExpandState=-1");
}

// Duplicates in the request are dropped and uids are compared as numbers ("007" is 7), order kept; the folder is deleted once and the call succeeds.
TEST(ConditionFolderDelete, DuplicatesInTheRequestAreDroppedAndUidsCompareAsNumbers)
{
    TempDb db("fdel_dedup");
    REQUIRE(db.schema_ok());
    seed_chain(db);

    CHECK_FOLDER_OK(del(db, {"4", "4", "004"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID = 4"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 4);

    const auto blocked = del(db, {"3", "1", "3", "2", "001"});
    CHECK_FOLDER_REFUSED(blocked, R::InUse);
    CHECK_STR_EQ(join(blocked.blocked), "3,1,2"); // request order, each once
}

// Every uid of the request must be a positive decimal integer: a bad one anywhere refuses the call before the database is opened; so does a bad bid.
TEST(ConditionFolderDelete, ABadUidAnywhereInTheRequestIsRefused)
{
    TempDb db("fdel_bad_uids");
    REQUIRE(db.schema_ok());
    seed_chain(db);
    const std::string before = db.snapshot_all();

    for (const std::string bad : {"", "0", "-4", " 4", "4 ", "abc", "4.0", "+4", "9223372036854775808", "99999999999999999999"})
    {
        SCOPED_CONTEXT("bad uid " + ost_test::detail::quote(bad));
        CHECK_FOLDER_REFUSED(del(db, {"4", bad}), R::InvalidUid);
        CHECK_FOLDER_REFUSED(del(db, {bad, "4"}), R::InvalidUid);
        CHECK_FOLDER_REFUSED(del(db, {"4"}, bad), R::InvalidUid);
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// Ownership: a missing folder, a folder of another bid (also among folders of the bid) and a missing bid refuse the whole batch.
TEST(ConditionFolderDelete, TheFoldersMustBelongToTheOpenBid)
{
    TempDb db("fdel_ownership");
    REQUIRE(db.schema_ok());
    seed_chain(db);
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(del(db, {"20"}), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(del(db, {"4", "20"}), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(del(db, {"4"}, "2"), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(del(db, {"4", "999"}), R::FolderNotFound);
    CHECK_FOLDER_REFUSED(del(db, {"4"}, "99"), R::BidNotFound);
    CHECK_EQ(db.snapshot_all(), before);
}

// =================================================================================================================================
// lock, rollback, failed writes
// =================================================================================================================================

// A locked bid refuses the delete, also a large batch, and the lock comes before the in-use rule (the order of plan 4.4: ownership, lock, rules).
TEST(ConditionFolderDelete, ALockedBidRefusesTheDeleteBeforeTheInUseRule)
{
    TempDb db("fdel_locked");
    REQUIRE(db.schema_ok());
    seed_chain(db);
    add_folder_range(db, 100, 219, 1);
    lock_bid(db, 1);
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(del(db, {"4"}), R::BidLocked);
    CHECK_FOLDER_REFUSED(del(db, uid_range(100, 219)), R::BidLocked);
    CHECK_FOLDER_REFUSED(del(db, {"3"}), R::BidLocked); // Leaf is in use: BidLocked, not InUse
    CHECK_FOLDER_REFUSED(del(db, {"20"}), R::FolderOfAnotherBid); // ownership first
    CHECK_FOLDER_REFUSED(del(db, {"999"}), R::FolderNotFound);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_FOLDER_OK(del(db, {}));
}

// Failure injection, the re-parenting statement: a BEFORE UPDATE trigger aborts the update of the children's ParentUID: refused, nothing changes (the
// folder is not deleted either), the write lock is released.
TEST(ConditionFolderDelete, AFailedReparentingRollsBackTheWholeDelete)
{
    TempDb db("fdel_rollback_update");
    REQUIRE(db.schema_ok());
    seed_chain(db);
    db.must("UPDATE BidConditions SET BidConditionFolderUID = NULL WHERE UID = 31");
    const std::string before = db.snapshot_all();

    const std::string trigger = db.block("BidConditionFolders", "UPDATE");
    CHECK_FOLDER_REFUSED(del(db, {"1"}), R::WriteFailed); // Mid is a child of Top: the update runs and fails
    CHECK_EQ(db.snapshot_all(), before);
    CHECK(db.exec("UPDATE Bids SET JobName = 'still writable' WHERE UID = 2"));
    db.must("UPDATE Bids SET JobName = 'Bid two' WHERE UID = 2");
    db.unblock(trigger);
    CHECK_FOLDER_OK(del(db, {"1"}));
}

// Failure injection, the delete statement, after the re-parenting UPDATE ran: a BEFORE DELETE trigger aborts it: the re-parenting is rolled back
// (the plan's "rollback of the ParentUID update"), the database is byte-identical.
TEST(ConditionFolderDelete, AFailedDeleteRollsBackTheReparenting)
{
    TempDb db("fdel_rollback_delete");
    REQUIRE(db.schema_ok());
    seed_chain(db);
    db.must("UPDATE BidConditions SET BidConditionFolderUID = NULL WHERE UID = 31");
    const std::string before = db.snapshot_all();

    const std::string trigger = db.block("BidConditionFolders", "DELETE");
    CHECK_FOLDER_REFUSED(del(db, {"1", "4"}), R::WriteFailed);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.text("SELECT ParentUID FROM BidConditionFolders WHERE UID = 2"), "1"); // Mid still hangs under Top
    CHECK(db.exec("UPDATE Bids SET JobName = 'still writable' WHERE UID = 2"));
    db.must("UPDATE Bids SET JobName = 'Bid two' WHERE UID = 2");
    db.unblock(trigger);
    CHECK_FOLDER_OK(del(db, {"1", "4"}));
    CHECK_EQ(db.list("SELECT UID || ':' || quote(ParentUID) FROM BidConditionFolders WHERE BidUID = 1 ORDER BY UID"), "2:NULL,3:2");
}

// A busy database: another connection holds the write lock for longer than the busy timeout: the delete is refused and nothing changes; afterwards it
// works.
TEST(ConditionFolderDelete, ABusyDatabaseRefusesTheDeleteAndChangesNothing)
{
    TempDb db("fdel_busy");
    REQUIRE(db.schema_ok());
    seed_chain(db);
    const std::string before = db.snapshot_all();

    ost_db::OstDatabase holder;
    REQUIRE(holder.open(db.path()));
    REQUIRE(holder.begin_transaction());
    CHECK_FOLDER_REFUSED(del(db, {"4"}), R::WriteFailed);
    REQUIRE(holder.rollback());
    holder.close();
    CHECK_EQ(db.snapshot_all(), before);

    CHECK_FOLDER_OK(del(db, {"4"}));
}

// A delete that finds nothing to read (a database without the folder table) is refused as an unsupported schema, nothing is created.
TEST(ConditionFolderDelete, ADatabaseWithoutTheFolderTableRefusesTheDelete)
{
    TempDb db("fdel_no_table", false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(del(db, {"5"}), R::SchemaUnsupported);
    CHECK_EQ(db.snapshot_all(), before);
}
