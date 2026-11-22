// BRW-02: the system folder "Deleted Bids" is the BidProjects row with UID 1, whatever its name; nothing is renamed, merged,
// reserved or deleted by NAME.
// Windows oracle:
//   ost_visualizer/domain/entities/project_constants.py (DELETED_BIDS_PROJECT_UID = "1", is_deleted_bids_project_uid)
//   ost_visualizer/infrastructure/mdb/components/project_operations.py (create_project / rename_project take any name, duplicate names
//     are allowed; delete_projects refuses UID 1)
//   ost_visualizer/presentation/components/project_tree_view.py:1785-1791 (_can_rename_context: Rename is disabled for UID 1)
//   tests/infrastructure/mdb/components/test_project_operations.py::DeletedBidsProjectGuardTests
//   tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_full_hierarchy_maps_every_bid_field_and_orders_bids_and_projects
//     (two projects named 'Alpha' are two projects)
#include "BrowserTestSupport.hpp"

#include <string>

using ost_btest::TempDb;

namespace
{
    // Port of DeletedBidsProjectGuardTests._database (UID 1 is the system project; the Windows fixture also renames it to
    // 'Deleted Bids'): project 2 and bids 10 (in the trash, restore target 2) and 11 (in project 2, restore target 1).
    void seed_guard_database(const TempDb &db)
    {
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
        db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (10, 1, 2, 'ten', 10)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (11, 2, 1, 'eleven', 11)");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// delete: the system folder is refused by UID
// ---------------------------------------------------------------------------------------------------------------

// Port of test_the_system_project_is_refused_and_every_row_is_left_untouched ("1", "01" of the uid variants [1], [" 1 "], ["01"]) and
// the strict-policy message ^Cannot delete the system project 'Deleted Bids' \(UID 1\)$ (the writer's ValueError).
TEST(Brw02_SystemFolder, DeleteIsRefusedForTheSystemFolderAndLeavesEveryRowUntouched)
{
    for (const char *uid : {"1", "01"})
    {
        SCOPED_CONTEXT(std::string("uid ") + uid);
        TempDb db(std::string("sys_delete_") + uid);
        REQUIRE(db.schema_ok());
        seed_guard_database(db);
        const std::string before = db.snapshot();

        std::string error;
        CHECK_FALSE(ost_store::delete_folder(db.path(), uid, &error));
        CHECK_STR_EQ(error, "Cannot delete the system project 'Deleted Bids' (UID 1)");
        CHECK_EQ(db.snapshot(), before);
    }
    // " 1 " is no valid project uid at all (iOS parses ASCII digits only): refused as well, nothing written
    TempDb db("sys_delete_padded");
    REQUIRE(db.schema_ok());
    seed_guard_database(db);
    const std::string before = db.snapshot();
    CHECK_FALSE(ost_store::delete_folder(db.path(), " 1 "));
    CHECK_EQ(db.snapshot(), before);
}

// The system project is the row with UID 1 whatever its name (the Windows fixture renames UID 1; the name is not the key).
TEST(Brw02_SystemFolder, DeleteIsRefusedForUidOneEvenWhenItsNameIsDifferent)
{
    TempDb db("sys_delete_renamed");
    REQUIRE(db.schema_ok());
    seed_guard_database(db);
    db.must("UPDATE BidProjects SET Name = 'Foo' WHERE UID = 1");
    const std::string before = db.snapshot();

    CHECK_FALSE(ost_store::delete_folder(db.path(), "1"));
    CHECK_EQ(db.snapshot(), before);
    CHECK_FALSE(ost_store::can_delete_folder(db.path(), "1"));
}

// Reviewer addition. The system folder is refused by UID alone, also while it is EMPTY (so no 'still has bids' reason can hide a missing
// system-folder rule): can_delete_folder and delete_folder both say no with the Windows writer text, for every spelling of the uid.
TEST(Brw02_SystemFolder, AnEmptySystemFolderIsStillRefusedByUid)
{
    TempDb db("sys_delete_empty");
    REQUIRE(db.schema_ok());
    const std::string before = db.snapshot();

    for (const char *uid : {"1", "01", "0001"})
    {
        SCOPED_CONTEXT(std::string("uid ") + uid);
        std::string can_error;
        CHECK_FALSE(ost_store::can_delete_folder(db.path(), uid, &can_error));
        CHECK_STR_EQ(can_error, "Cannot delete the system project 'Deleted Bids' (UID 1)");
        std::string delete_error;
        CHECK_FALSE(ost_store::delete_folder(db.path(), uid, &delete_error));
        CHECK_STR_EQ(delete_error, "Cannot delete the system project 'Deleted Bids' (UID 1)");
    }
    CHECK_EQ(db.snapshot(), before);
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids"));
}

// A project that merely carries the NAME 'Deleted Bids' is an ordinary project (Windows keys on the UID only): it can be deleted.
TEST(Brw02_SystemFolder, AnotherFolderNamedDeletedBidsIsAnOrdinaryFolder)
{
    TempDb db("sys_delete_namesake");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (5, 'Deleted Bids')");

    CHECK(ost_store::can_delete_folder(db.path(), "5"));
    CHECK(ost_store::delete_folder(db.path(), "5"));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids"));
}

// ---------------------------------------------------------------------------------------------------------------
// rename: the system folder keeps its name, every other folder may take any name
// ---------------------------------------------------------------------------------------------------------------

// Item test plan (1): rename_folder(db, '1', 'Foo') must be refused. Before the fix it returned true and the next create_schema
// (it runs on every loadHierarchy) added a NEW 'Deleted Bids' row, turning the trash into an ordinary folder 'Foo'.
TEST(Brw02_SystemFolder, RenameIsRefusedAndCreateSchemaAddsNoSecondTrash)
{
    for (const char *uid : {"1", "01"})
    {
        SCOPED_CONTEXT(std::string("uid ") + uid);
        TempDb db(std::string("sys_rename_") + uid);
        REQUIRE(db.schema_ok());
        seed_guard_database(db);
        const std::string before = db.snapshot();

        CHECK_FALSE(ost_store::rename_folder(db.path(), uid, "Foo"));
        CHECK_EQ(db.snapshot(), before);
        REQUIRE(ost_store::create_schema(db.path()));
        CHECK_EQ(db.folders(), std::string("1:Deleted Bids,2:Folder A"));
        CHECK_EQ(db.bids(), std::string("10:1/2 11:2/1"));
    }
}

// Item (d): renaming another folder to 'Deleted Bids' was refused; Windows allows it (rename_project takes any name).
TEST(Brw02_SystemFolder, AnotherFolderMayBeRenamedToDeletedBids)
{
    TempDb db("sys_rename_namesake");
    REQUIRE(db.schema_ok());
    seed_guard_database(db);

    CHECK(ost_store::rename_folder(db.path(), "2", "Deleted Bids"));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,2:Deleted Bids"));
    // the namesake is still an ordinary folder: UID 1 stays the trash
    REQUIRE(ost_store::create_schema(db.path()));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,2:Deleted Bids"));
    CHECK_EQ(db.bids(), std::string("10:1/2 11:2/1"));
}

// The rename of any other folder is unaffected (test_project_rename_updates_only_the_target_project).
TEST(Brw02_SystemFolder, RenameOfAnotherFolderStillWorks)
{
    TempDb db("sys_rename_other");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (7, 'First')");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (8, 'Second')");

    CHECK(ost_store::rename_folder(db.path(), "7", "Renamed"));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,7:Renamed,8:Second"));
}

// ---------------------------------------------------------------------------------------------------------------
// create_schema: no merge, no renumbering by name
// ---------------------------------------------------------------------------------------------------------------

// Item test plan (2): BidProjects (5, 'Deleted Bids') and Bids (60, BidProjectUID 5): create_schema must leave project 5 and the
// bid in it alone. Before the fix the project was deleted and the bid moved into the trash.
TEST(Brw02_SystemFolder, CreateSchemaDoesNotMergeAFolderThatCarriesTheName)
{
    TempDb db("sys_schema_merge");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (5, 'Deleted Bids')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (60, 5, 'sixty', 60)");

    REQUIRE(ost_store::create_schema(db.path()));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,5:Deleted Bids"));
    CHECK_EQ(db.bids(), std::string("60:5/N"));
}

// A trash folder that was renamed by an older build (UID 1 called 'Foo') stays the trash: no second 'Deleted Bids' row appears.
TEST(Brw02_SystemFolder, CreateSchemaKeepsTheSystemFolderWhateverItsName)
{
    TempDb db("sys_schema_renamed");
    REQUIRE(db.schema_ok());
    seed_guard_database(db);
    db.must("UPDATE BidProjects SET Name = 'Foo' WHERE UID = 1");

    REQUIRE(ost_store::create_schema(db.path()));
    CHECK_EQ(db.folders(), std::string("1:Foo,2:Folder A"));
    CHECK_EQ(db.bids(), std::string("10:1/2 11:2/1"));
}

// The system folder is created (UID 1, 'Deleted Bids') when the database has none, as the Windows database creator does.
TEST(Brw02_SystemFolder, CreateSchemaCreatesTheSystemFolderWhenItIsMissing)
{
    TempDb db("sys_schema_missing");
    REQUIRE(db.schema_ok());
    db.must("DELETE FROM BidProjects WHERE UID = 1");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (5, 'Deleted Bids')");

    REQUIRE(ost_store::create_schema(db.path()));
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,5:Deleted Bids"));
}

// ---------------------------------------------------------------------------------------------------------------
// create_folder: duplicate names are allowed, also 'Deleted Bids'
// ---------------------------------------------------------------------------------------------------------------

// Item test plan (3): create_folder('Deleted Bids') must insert a new row with the next UID (Windows allows duplicate names;
// test_hierarchy_reader has two 'Alpha' projects). Before the fix it returned the existing UID '1'.
TEST(Brw02_SystemFolder, CreateFolderNamedDeletedBidsInsertsANewRow)
{
    TempDb db("sys_create");
    REQUIRE(db.schema_ok());

    CHECK_STR_EQ(ost_store::create_folder(db.path(), "Deleted Bids"), "2");
    CHECK_STR_EQ(ost_store::create_folder(db.path(), "Deleted Bids"), "3");
    CHECK_STR_EQ(ost_store::create_folder(db.path(), "Alpha"), "4");
    CHECK_STR_EQ(ost_store::create_folder(db.path(), "Alpha"), "5");
    CHECK_EQ(db.folders(), std::string("1:Deleted Bids,2:Deleted Bids,3:Deleted Bids,4:Alpha,5:Alpha"));
}

// ---------------------------------------------------------------------------------------------------------------
// trash and permanent delete: UID 1 only
// ---------------------------------------------------------------------------------------------------------------

// "Trash resolution should use UID '1' only": with a namesake folder (5) and the system folder renamed, a deleted bid still goes to
// UID 1 (project_write_handler._delete_bids moves to _DELETED_BIDS_PROJECT_UID).
TEST(Brw02_SystemFolder, DeleteBidMovesToUidOneEvenWhenAnotherFolderCarriesTheName)
{
    TempDb db("sys_trash_target");
    REQUIRE(db.schema_ok());
    seed_guard_database(db);
    db.must("UPDATE BidProjects SET Name = 'Foo' WHERE UID = 1");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (5, 'Deleted Bids')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (12, 2, 'twelve', 12)");

    CHECK(ost_store::delete_project(db.path(), "12"));
    CHECK_EQ(db.bids(), std::string("10:1/2 11:2/1 12:1/2"));
    CHECK_EQ(db.folders(), std::string("1:Foo,2:Folder A,5:Deleted Bids"));
}

// Only a bid of UID 1 is deleted for good; a bid in a namesake folder is a live bid and is refused (nothing is touched).
TEST(Brw02_SystemFolder, PermanentDeleteOnlyTakesBidsOfTheSystemFolder)
{
    TempDb db("sys_perm_delete");
    REQUIRE(db.schema_ok());
    seed_guard_database(db);
    db.must("UPDATE BidProjects SET Name = 'Foo' WHERE UID = 1");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (5, 'Deleted Bids')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (12, 5, 'in namesake', 12)");
    const std::string before = db.snapshot();

    CHECK_FALSE(ost_store::permanently_delete_project(db.path(), "12"));
    CHECK_EQ(db.snapshot(), before);
    CHECK(ost_store::permanently_delete_project(db.path(), "10")); // UID 1, named 'Foo'
    CHECK_EQ(db.bids(), std::string("11:2/1 12:5/N"));
}
