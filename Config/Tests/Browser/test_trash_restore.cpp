// BRW-10 / BRW-11 / BRW-09: what a bid remembers when it goes to "Deleted Bids" (Bids.OrigBidProjectUID) and what Restore does with it.
// Windows oracle:
//   ost_visualizer/presentation/handlers/project_write_handler.py _delete_bids (the trash group of a bid is the project it is in: a bid
//     whose BidProjectUID is NULL, 0 or no project is an orphan and gets orig_project_uid=None) and restore_bids (groups by
//     OrigBidProjectUID, no original -> orphan)
//   ost_visualizer/infrastructure/mdb/components/project_operations.py move_bids_to_project (OrigBidProjectUID is written only when a
//     restore project is given, which must exist) / orphan_bids (clears BidProjectUID only)
//   tests/presentation/handlers/test_project_write_handler.py::test_unassigned_bids_share_one_trash_group_whatever_their_missing_project_is,
//     test_restore_bids_groups_by_original_project_and_uses_the_trash_marker
//   tests/infrastructure/mdb/components/test_project_operations.py::test_move_without_a_restore_project_leaves_the_restore_column_alone,
//     test_move_assigns_the_project_and_the_restore_project, test_orphaning_clears_the_project_only,
//     test_move_bids_assigns_project_and_restore_reference_to_every_batch_member
#include "BrowserTestSupport.hpp"

#include <string>

using ost_btest::TempDb;

namespace
{
    // BidProjects 1 (trash, from create_schema), 3, 5, 6.
    void seed_folders(const TempDb &db)
    {
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (3, 'Three')");
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (5, 'Five')");
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (6, 'Six')");
    }

    void add_bid(const TempDb &db, int uid, const std::string &project, const std::string &orig)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (" + std::to_string(uid) + ", " +
                project + ", " + orig + ", 'bid" + std::to_string(uid) + "', " + std::to_string(uid) + ")");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-10: only a folder that exists is remembered as the restore target
// ---------------------------------------------------------------------------------------------------------------

// Item test plan: BidProjects 1 (trash), 3; Bids (1, BidProjectUID 99, Orig NULL), (2, BidProjectUID 0, Orig 7), (3, BidProjectUID 3).
// Trash each: Orig stays NULL for bid 1 (99 is no project), stays 7 for bid 2 (0 is no project, nothing is written, as Windows does)
// and becomes 3 for bid 3. Before the fix bid 1 got 99 and bid 2 got 0.
// Port of test_unassigned_bids_share_one_trash_group_whatever_their_missing_project_is (orig None for None / "" / unknown, the project
// uid for a known project) + test_move_without_a_restore_project_leaves_the_restore_column_alone.
TEST(Brw10_TrashOrigin, ADanglingOrZeroProjectIsNotRecordedAsTheRestoreTarget)
{
    TempDb db("trash_dangling");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 1, "99", "NULL");
    add_bid(db, 2, "0", "7");
    add_bid(db, 3, "3", "NULL");

    CHECK(ost_store::delete_project(db.path(), "1"));
    CHECK(ost_store::delete_project(db.path(), "2"));
    CHECK(ost_store::delete_project(db.path(), "3"));
    CHECK_EQ(db.bids(), std::string("1:1/N 2:1/7 3:1/3"));
}

// The same for the other orphan shapes: NULL project, and a project that was deleted (a dangling reference).
TEST(Brw10_TrashOrigin, EveryOrphanShapeLeavesTheRestoreColumnAlone)
{
    TempDb db("trash_orphans");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 10, "NULL", "NULL");
    add_bid(db, 11, "NULL", "5");
    add_bid(db, 12, "0", "NULL");
    add_bid(db, 13, "42", "6");

    for (const char *uid : {"10", "11", "12", "13"})
        CHECK(ost_store::delete_project(db.path(), uid));
    CHECK_EQ(db.bids(), std::string("10:1/N 11:1/5 12:1/N 13:1/6"));
}

// A bid in a folder is trashed with that folder as the restore target, replacing an older value
// (test_move_assigns_the_project_and_the_restore_project: move to project 2 with restore project 1).
TEST(Brw10_TrashOrigin, ABidInAFolderRecordsThatFolder)
{
    TempDb db("trash_known");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 20, "5", "NULL");
    add_bid(db, 21, "6", "3");

    CHECK(ost_store::delete_project(db.path(), "20"));
    CHECK(ost_store::delete_project(db.path(), "21"));
    CHECK_EQ(db.bids(), std::string("20:1/5 21:1/6"));
}

// A bid that is already in the trash is left alone (its restore target is kept); a bid that does not exist is refused.
TEST(Brw10_TrashOrigin, ABidInTheTrashStaysAndAMissingBidIsRefused)
{
    TempDb db("trash_twice");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 30, "1", "5");

    CHECK(ost_store::delete_project(db.path(), "30"));
    CHECK_EQ(db.bids(), std::string("30:1/5"));
    CHECK_FALSE(ost_store::delete_project(db.path(), "999"));
    CHECK_EQ(db.bids(), std::string("30:1/5"));
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-11 (decided): trashing an orphan leaves OrigBidProjectUID untouched
// ---------------------------------------------------------------------------------------------------------------

// Item host repro: bid 80 (BidProjectUID NULL, OrigBidProjectUID 7) -> (1, 7); before the fix (1, NULL).
// Port of test_move_without_a_restore_project_leaves_the_restore_column_alone (UPDATE Bids SET OrigBidProjectUID = 7; move bid 10 to
// project 2 with no restore project -> [(10, 2, 7), (11, NULL, 7)]).
TEST(Brw11_OrphanTrash, AnOrphanKeepsItsStoredRestoreTarget)
{
    TempDb db("orphan_keeps_orig");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 80, "NULL", "7");
    add_bid(db, 81, "NULL", "NULL");

    CHECK(ost_store::delete_project(db.path(), "80"));
    CHECK(ost_store::delete_project(db.path(), "81"));
    CHECK_EQ(db.bids(), std::string("80:1/7 81:1/N"));
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-09: Restore goes back to OrigBidProjectUID
// ---------------------------------------------------------------------------------------------------------------

// Item test plan: trash folder 1 and folders 5 and 6; trash bids with Orig 5, 6, NULL and 0. Restore gives BidProjectUID 5, 6, NULL, NULL
// and the Orig values stay as they were. Port of test_restore_bids_groups_by_original_project_and_uses_the_trash_marker (the groups
// are the original projects; no original -> orphan_bids) for one bid at a time.
TEST(Brw09_Restore, GoesBackToTheOriginalFolderAndKeepsTheOriginalReference)
{
    TempDb db("restore_orig");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 41, "1", "5");
    add_bid(db, 42, "1", "6");
    add_bid(db, 43, "1", "NULL");
    add_bid(db, 44, "1", "0");

    CHECK(ost_store::restore_bid(db.path(), "41"));
    CHECK(ost_store::restore_bid(db.path(), "42"));
    CHECK(ost_store::restore_bid(db.path(), "43"));
    CHECK(ost_store::restore_bid(db.path(), "44"));
    CHECK_EQ(db.bids(), std::string("41:5/5 42:6/6 43:N/N 44:N/0"));
}

// The whole round trip: trash from folder 5, restore -> back in folder 5 (the restore reference survives the restore, as on Windows).
TEST(Brw09_Restore, ATrashedBidComesBackToItsFolder)
{
    TempDb db("restore_round_trip");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 50, "5", "NULL");

    CHECK(ost_store::delete_project(db.path(), "50"));
    CHECK_EQ(db.bids(), std::string("50:1/5"));
    CHECK(ost_store::restore_bid(db.path(), "50"));
    CHECK_EQ(db.bids(), std::string("50:5/5"));
    CHECK(ost_store::delete_project(db.path(), "50"));
    CHECK_EQ(db.bids(), std::string("50:1/5"));
}

// An orphan trashed and restored comes back as an orphan (orphan_bids).
TEST(Brw09_Restore, ATrashedOrphanComesBackUnassigned)
{
    TempDb db("restore_orphan");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 51, "NULL", "NULL");

    CHECK(ost_store::delete_project(db.path(), "51"));
    CHECK(ost_store::restore_bid(db.path(), "51"));
    CHECK_EQ(db.bids(), std::string("51:N/N"));
}

// BRW-05 + BRW-09: the folder a bid was trashed from is deleted afterwards (it is empty, so it may be): the bid has no restore target
// any more and comes back unassigned instead of pointing at a folder number that a later new folder could take.
TEST(Brw09_Restore, AfterTheOriginalFolderWasDeletedTheBidComesBackUnassigned)
{
    TempDb db("restore_after_folder_delete");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 52, "5", "NULL");
    CHECK(ost_store::delete_project(db.path(), "52"));
    CHECK_EQ(db.bids(), std::string("52:1/5"));

    CHECK(ost_store::delete_folder(db.path(), "5"));
    CHECK_EQ(db.bids(), std::string("52:1/N"));
    CHECK(ost_store::restore_bid(db.path(), "52"));
    CHECK_EQ(db.bids(), std::string("52:N/N"));
}

// A bid that does not exist is a success that changes nothing (orphan_bids of an absent bid); a value that is no bid uid is refused.
TEST(Brw09_Restore, AMissingBidChangesNothingAndAMalformedUidIsRefused)
{
    TempDb db("restore_missing");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 60, "1", "5");
    const std::string before = db.snapshot();

    CHECK(ost_store::restore_bid(db.path(), "999"));
    CHECK_FALSE(ost_store::restore_bid(db.path(), "abc"));
    CHECK_FALSE(ost_store::restore_bid(db.path(), ""));
    CHECK_FALSE(ost_store::restore_bid(db.path(), "0"));
    CHECK_FALSE(ost_store::restore_bid(db.path(), "-60"));
    CHECK_EQ(db.snapshot(), before);
}

// Kept by user decision 2026-11-19 (BRW-09, see PORTING_BLOCKERS.md): Windows fails the whole restore group with no change when
// the original project no longer exists (move_bids_to_project requires the project: "Restore Error"). That is only reachable
// with foreign data (the Windows writer clears the reference when a project is deleted, BRW-05), and it leaves the bid stuck in
// the trash; iOS keeps what its Restore action always did: the bid comes back unassigned. This test pins that kept behavior
// (the "Interim" in its name is the old wording, the name was not changed).
TEST(Brw09_Restore, InterimADanglingOriginalFolderRestoresTheBidUnassigned)
{
    TempDb db("restore_dangling");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 70, "1", "99");

    CHECK(ost_store::restore_bid(db.path(), "70"));
    CHECK_EQ(db.bids(), std::string("70:N/99"));
}

// The write is one statement per call: a failing write is reported and leaves the bid where it was.
TEST(Brw09_Restore, AFailingWriteIsReportedAndChangesNothing)
{
    TempDb db("restore_blocked");
    REQUIRE(db.schema_ok());
    seed_folders(db);
    add_bid(db, 80, "1", "5");
    db.must("CREATE TRIGGER blk_restore BEFORE UPDATE ON Bids BEGIN SELECT RAISE(ABORT, 'blocked'); END");
    const std::string before = db.snapshot();

    CHECK_FALSE(ost_store::restore_bid(db.path(), "80"));
    CHECK_EQ(db.snapshot(), before);
    db.must("DROP TRIGGER blk_restore");
    CHECK(ost_store::restore_bid(db.path(), "80"));
    CHECK_EQ(db.bids(), std::string("80:5/5"));
}
