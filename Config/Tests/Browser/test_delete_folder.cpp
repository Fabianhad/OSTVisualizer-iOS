// BRW-05 / BRW-08 (decided): deleting a folder. Only EMPTY folders are deleted (refused with the Windows text otherwise), and a delete
// clears every bid reference to the deleted folder, Bids.BidProjectUID and Bids.OrigBidProjectUID.
// Windows oracle:
//   ost_visualizer/presentation/handlers/project_write_handler.py:931-992 (_delete_projects: the system project is skipped, a project with
//     bids is not deletable and gets the warning 'Cannot Delete Project' / 'Some selected projects still have bids inside.\nOnly empty
//     projects can be deleted.', the confirmation is 'Delete Projects' / 'Permanently delete this empty project?\nThis cannot be undone.')
//   ost_visualizer/infrastructure/mdb/components/project_operations.py:157-208 (delete_projects: BidProjectUID and OrigBidProjectUID
//     are set NULL wherever they equal a deleted project, then the rows go; an absent project is a success)
//   ost_visualizer/domain/services/project_data_service.py project_has_bids (counts the bids of the project in the hierarchy only: a bid
//     that merely has OrigBidProjectUID = the project does not block)
//   tests/presentation/handlers/test_project_write_handler.py::test_delete_projects_skips_the_trash_and_projects_with_bids,
//     test_delete_projects_confirmation_text_counts_several_projects
//   tests/infrastructure/mdb/components/test_project_operations.py::test_delete_project_clears_deleted_bid_restore_reference,
//     test_deleting_projects_detaches_their_bids_and_restore_references,
//     test_deleting_several_projects_detaches_and_removes_every_one_of_them,
//     test_deleting_or_orphaning_a_project_or_bid_that_is_absent_changes_nothing
// The Windows writer fixtures put bids INSIDE the deleted project; the handler refuses that before the writer runs (BRW-08), so the ports
// below keep the restore-reference parts of those fixtures and use an empty project (or an absent one for the BidProjectUID part).
#include "BrowserTestSupport.hpp"

#include <string>

using ost_btest::TempDb;

namespace
{
    void add_folder(const TempDb &db, int uid, const char *name)
    {
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (" + std::to_string(uid) + ", '" + name + "')");
    }

    void add_bid(const TempDb &db, int uid, const std::string &project, const std::string &orig)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (" + std::to_string(uid) + ", " +
                project + ", " + orig + ", 'bid" + std::to_string(uid) + "', " + std::to_string(uid) + ")");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The Windows texts
// ---------------------------------------------------------------------------------------------------------------

TEST(Brw08_DeleteFolderTexts, AreTheWindowsStrings)
{
    CHECK_STR_EQ(ost_domain::CANNOT_DELETE_FOLDER_TITLE, "Cannot Delete Project");
    CHECK_STR_EQ(ost_domain::CANNOT_DELETE_FOLDER_MESSAGE,
                 "Some selected projects still have bids inside.\nOnly empty projects can be deleted.");
    CHECK_STR_EQ(ost_domain::DELETE_FOLDER_CONFIRM_TITLE, "Delete Projects");
    CHECK_STR_EQ(ost_domain::DELETE_FOLDER_CONFIRM_MESSAGE, "Permanently delete this empty project?\nThis cannot be undone.");
    CHECK_STR_EQ(ost_domain::DELETED_BIDS_FOLDER_UID, "1");
    CHECK_STR_EQ(ost_domain::DELETED_BIDS_FOLDER_NAME, "Deleted Bids");
    CHECK(ost_domain::is_deleted_bids_folder_uid("1"));
    CHECK_FALSE(ost_domain::is_deleted_bids_folder_uid("01"));
    CHECK_FALSE(ost_domain::is_deleted_bids_folder_uid("2"));
    CHECK_FALSE(ost_domain::is_deleted_bids_folder_uid(""));
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-08: a folder with bids is refused
// ---------------------------------------------------------------------------------------------------------------

// Port of test_delete_projects_skips_the_trash_and_projects_with_bids for a single folder: 'busy' has a bid -> the warning text and
// nothing is written; 'empty' is deleted. Before the fix the bid was unassigned and the folder deleted.
TEST(Brw08_DeleteFolder, AFolderWithBidsIsRefusedWithTheWindowsMessageAndNothingIsWritten)
{
    TempDb db("del_busy");
    REQUIRE(db.schema_ok());
    add_folder(db, 2, "busy");
    add_folder(db, 3, "empty");
    add_bid(db, 41, "2", "NULL");
    const std::string before = db.snapshot();

    std::string error = "stale";
    CHECK_FALSE(ost_store::delete_folder(db.path(), "2", &error));
    CHECK_STR_EQ(error, "Some selected projects still have bids inside.\nOnly empty projects can be deleted.");
    CHECK_EQ(db.snapshot(), before);

    std::string reason;
    CHECK_FALSE(ost_store::can_delete_folder(db.path(), "2", &reason));
    CHECK_STR_EQ(reason, ost_domain::CANNOT_DELETE_FOLDER_MESSAGE);
    CHECK_EQ(db.snapshot(), before);

    // the same call without an error out parameter refuses the same way
    CHECK_FALSE(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.snapshot(), before);

    CHECK(ost_store::can_delete_folder(db.path(), "3"));
    error = "stale";
    CHECK(ost_store::delete_folder(db.path(), "3", &error));
    CHECK(error.empty());
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,2:busy"));
    CHECK_EQ(db.bids(), std::string("41:2/N"));
}

// Several bids, bids in the trash are not "in" the folder, and a bid that only REMEMBERS the folder (OrigBidProjectUID) does not block:
// Windows counts project_info.bids, the bids whose BidProjectUID is the project.
TEST(Brw08_DeleteFolder, OnlyBidsThatAreInTheFolderBlockTheDelete)
{
    TempDb db("del_blockers");
    REQUIRE(db.schema_ok());
    add_folder(db, 2, "Two");
    add_folder(db, 3, "Three");
    add_bid(db, 51, "2", "NULL");
    add_bid(db, 52, "2", "2");
    add_bid(db, 53, "1", "3");  // trashed from folder 3: it remembers folder 3 but is not in it
    add_bid(db, 54, "NULL", "3");
    const std::string before = db.snapshot();

    CHECK_FALSE(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.snapshot(), before);
    CHECK(ost_store::can_delete_folder(db.path(), "3"));
    CHECK(ost_store::delete_folder(db.path(), "3"));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,2:Two"));
    CHECK_EQ(db.bids(), std::string("51:2/N 52:2/2 53:1/N 54:N/N"));
}

// A folder that does not exist has no bids (project_has_bids is False for a project that is not in the hierarchy): deleting it is a
// success that changes nothing (test_deleting_or_orphaning_a_project_or_bid_that_is_absent_changes_nothing: delete_projects(['99']) is True).
TEST(Brw08_DeleteFolder, AnAbsentFolderIsASuccessThatChangesNothing)
{
    TempDb db("del_absent");
    REQUIRE(db.schema_ok());
    add_folder(db, 2, "Two");
    add_bid(db, 10, "1", "2");
    add_bid(db, 11, "2", "NULL");
    const std::string before = db.snapshot();

    CHECK(ost_store::can_delete_folder(db.path(), "99"));
    CHECK(ost_store::delete_folder(db.path(), "99"));
    CHECK_EQ(db.snapshot(), before);
}

// A value that is no project uid is refused and nothing is written (_normalize_int_uids / require_valid_unique_bid_owned_uids).
TEST(Brw08_DeleteFolder, AMalformedFolderUidIsRefused)
{
    TempDb db("del_malformed");
    REQUIRE(db.schema_ok());
    add_folder(db, 2, "Two");
    const std::string before = db.snapshot();

    for (const char *uid : {"", "abc", "0", "-2", "2x", " 2", "2.0"})
    {
        SCOPED_CONTEXT(std::string("uid '") + uid + "'");
        CHECK_FALSE(ost_store::delete_folder(db.path(), uid));
        CHECK_FALSE(ost_store::can_delete_folder(db.path(), uid));
    }
    CHECK_EQ(db.snapshot(), before);
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-05: the delete clears every reference to the folder
// ---------------------------------------------------------------------------------------------------------------

// Port of test_delete_project_clears_deleted_bid_restore_reference with the deleted project 2 emptied first (BRW-08): BidProjects (1 trash),
// (2),(3); Bids (10, in 3, restore 2), (11, in the trash, restore 2) -> after the delete the restore references are gone and the other
// project and the trash membership are untouched. Before the fix OrigBidProjectUID stayed 2 and a later new folder 2 would have been
// treated as the original folder of both bids.
TEST(Brw05_DeleteFolder, ClearsTheRestoreReferencesToTheDeletedFolder)
{
    TempDb db("del_clears_orig");
    REQUIRE(db.schema_ok());
    add_folder(db, 2, "Two");
    add_folder(db, 3, "Three");
    add_bid(db, 10, "3", "2");
    add_bid(db, 11, "1", "2");

    CHECK(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.bids(), std::string("10:3/N 11:1/N"));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,3:Three"));
}

// Item alias DBP-39 (Bids (10, 1, Orig 2), (20, 2, NULL)): 20 lives in folder 2, so the delete is refused now (BRW-08); with the bid moved
// out, the delete clears the reference of bid 10 and the trash stays.
TEST(Brw05_DeleteFolder, TheTrashAndItsBidsStayAndTheirReferenceToTheFolderGoes)
{
    TempDb db("del_trash_bystander");
    REQUIRE(db.schema_ok());
    add_folder(db, 2, "Two");
    add_bid(db, 10, "1", "2");
    add_bid(db, 20, "2", "NULL");

    CHECK_FALSE(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.bids(), std::string("10:1/2 20:2/N"));
    CHECK(ost_store::remove_bid_from_folder(db.path(), "20"));
    CHECK(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.bids(), std::string("10:1/N 20:N/N"));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids"));
    CHECK_FALSE(ost_store::delete_folder(db.path(), "1"));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids"));
}

// Port of test_deleting_projects_detaches_their_bids_and_restore_references for the part that does not need a bid inside the project:
// the references to a project that no longer exists (dangling BidProjectUID and OrigBidProjectUID) go with the delete, in both columns;
// the system project and its members keep theirs. Windows: delete_projects(['2']) -> [(10, NULL, NULL), (11, 1, NULL)].
TEST(Brw05_DeleteFolder, DanglingReferencesInBothColumnsAreCleared)
{
    TempDb db("del_dangling");
    REQUIRE(db.schema_ok());
    add_folder(db, 3, "Three");
    add_bid(db, 10, "99", "99");
    add_bid(db, 11, "1", "99");
    add_bid(db, 12, "3", "3");

    CHECK(ost_store::delete_folder(db.path(), "99"));
    CHECK_EQ(db.bids(), std::string("10:N/N 11:1/N 12:3/3"));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,3:Three"));
}

// Port of test_deleting_several_projects_detaches_and_removes_every_one_of_them for the references: projects 1 (trash), 2, 3 are empty after the
// bids moved elsewhere; deleting 2 and then 3 clears both columns for both, and the bystander project 1 and the bid that points at it keep
// theirs. Windows: [(10, NULL, 1), (11, NULL, NULL), (12, 1, NULL)] for Bids (10, 2, 1), (11, 3, 2), (12, 1, 3) with the bids inside.
TEST(Brw05_DeleteFolder, EachDeleteClearsBothColumnsAndTheBystanderKeepsItsReferences)
{
    TempDb db("del_several");
    REQUIRE(db.schema_ok());
    add_folder(db, 2, "Two");
    add_folder(db, 3, "Three");
    add_folder(db, 4, "Four");
    add_bid(db, 10, "4", "1");  // a live bid that remembers the trash
    add_bid(db, 11, "1", "2");
    add_bid(db, 12, "1", "3");

    CHECK(ost_store::delete_folder(db.path(), "2"));
    CHECK(ost_store::delete_folder(db.path(), "3"));
    CHECK_EQ(db.bids(), std::string("10:4/1 11:1/N 12:1/N"));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,4:Four"));
}

// The three statements are one transaction: a failing step leaves the references and the folder as they were.
TEST(Brw05_DeleteFolder, AFailureRollsEveryStatementBack)
{
    TempDb db("del_rollback");
    REQUIRE(db.schema_ok());
    add_folder(db, 2, "Two");
    add_bid(db, 10, "1", "2");
    const std::string before = db.snapshot();

    db.must("CREATE TRIGGER blk_delete BEFORE DELETE ON BidProjects BEGIN SELECT RAISE(ABORT, 'blocked'); END");
    CHECK_FALSE(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.snapshot(), before);
    db.must("DROP TRIGGER blk_delete");

    db.must("CREATE TRIGGER blk_update BEFORE UPDATE ON Bids BEGIN SELECT RAISE(ABORT, 'blocked'); END");
    CHECK_FALSE(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.snapshot(), before);
    db.must("DROP TRIGGER blk_update");

    CHECK(ost_store::delete_folder(db.path(), "2"));
    CHECK_EQ(db.bids(), std::string("10:1/N"));
}
