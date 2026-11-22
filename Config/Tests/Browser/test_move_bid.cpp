// BRW-12: assign_bid_to_folder checks that the bid and the target folder exist and writes nothing otherwise.
// Windows oracle: ost_visualizer/infrastructure/mdb/components/project_operations.py:15-81 (move_bids_to_project: every bid UID must exist exactly once,
// the target project must exist; otherwise False and nothing in the batch changes) and
//   tests/infrastructure/mdb/components/test_project_operations.py::test_move_bid_rejects_missing_project_owner_before_mutation,
//   test_move_bids_rejects_missing_batch_member_before_mutation,
//   test_move_bids_assigns_project_and_restore_reference_to_every_batch_member,
//   test_moving_bids_into_or_out_of_the_system_project_is_unaffected (DeletedBidsProjectGuardTests)
//   tests/infrastructure/mdb/components/test_project_operations.py::test_orphan_bids_clears_project_and_rejects_duplicate_bid_uid_before_mutation,
//   test_deleting_or_orphaning_a_project_or_bid_that_is_absent_changes_nothing (orphan_bids: an absent bid is a success that changes nothing).
// Already fixed by the persistence step (DBP-17); these Browser ports pin it with the item's own fixtures.
#include "BrowserTestSupport.hpp"

#include <string>

using ost_btest::TempDb;

// Port of test_move_bid_rejects_missing_project_owner_before_mutation: BidProjects without 99, Bids (11, NULL): moving 11 to '99' is refused and the bid
// stays unassigned (before the fix the call returned true and stored the dangling 99).
TEST(Brw12_AssignBid, AMissingTargetFolderIsRefusedAndNothingIsWritten)
{
    TempDb db("assign_missing_folder");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (11, NULL, NULL, 'eleven', 11)");
    const std::string before = db.snapshot();

    CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), "11", "99"));
    CHECK_EQ(db.snapshot(), before);
    CHECK_EQ(db.bids(), std::string("11:N/N"));
}

// Port of test_move_bids_rejects_missing_batch_member_before_mutation for one bid: assign(missing bid '77', '1') was a success before the fix; now it is
// refused, and the existing bid 11 stays in project 1.
TEST(Brw12_AssignBid, AMissingBidIsRefusedAndNothingIsWritten)
{
    TempDb db("assign_missing_bid");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Target')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (11, 1, NULL, 'eleven', 11)");
    const std::string before = db.snapshot();

    CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), "77", "1"));
    CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), "77", "2"));
    CHECK_EQ(db.snapshot(), before);
}

// Port of test_move_bids_assigns_project_and_restore_reference_to_every_batch_member for single moves: BidProjects (1 Source), (2 Target), (3 Restore);
// Bids 11, 12, 13 in project 1; moving 11 and 12 to project 2 gives [(11, 2, NULL), (12, 2, NULL), (13, 1, NULL)]; the restore column is not written.
TEST(Brw12_AssignBid, AnExistingBidMovesToAnExistingFolderAndTheRestoreColumnIsLeftAlone)
{
    TempDb db("assign_ok");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Target')");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (3, 'Restore')");
    for (const int uid : {11, 12, 13})
        db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (" + std::to_string(uid) + ", 1, NULL, 'b', " +
                std::to_string(uid) + ")");

    CHECK(ost_store::assign_bid_to_folder(db.path(), "11", "2"));
    CHECK(ost_store::assign_bid_to_folder(db.path(), "12", "2"));
    CHECK_EQ(db.bids(), std::string("11:2/N 12:2/N 13:1/N"));

    db.must("UPDATE Bids SET OrigBidProjectUID = 7");
    CHECK(ost_store::assign_bid_to_folder(db.path(), "13", "3"));
    CHECK_EQ(db.bids(), std::string("11:2/7 12:2/7 13:3/7"));
}

// The system folder is no special target for the writer (test_moving_bids_into_or_out_of_the_system_project_is_unaffected: Windows keeps
// that rule in its tree, which offers no drop on 'Deleted Bids'); the move into and out of UID 1 works.
TEST(Brw12_AssignBid, TheWriterDoesNotSpecialCaseTheSystemFolder)
{
    TempDb db("assign_system");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (11, 2, 1, 'eleven', 11)");

    CHECK(ost_store::assign_bid_to_folder(db.path(), "11", "1"));
    CHECK_EQ(db.bids(), std::string("11:1/1"));
    CHECK(ost_store::assign_bid_to_folder(db.path(), "11", "2"));
    CHECK_EQ(db.bids(), std::string("11:2/1"));
}

// Malformed uids are refused (require_valid_unique_bid_owned_uids): nothing is written for them.
TEST(Brw12_AssignBid, MalformedUidsAreRefused)
{
    TempDb db("assign_malformed");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Target')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (11, NULL, NULL, 'eleven', 11)");
    const std::string before = db.snapshot();

    for (const char *uid : {"", "abc", "0", "-1", "2.5"})
    {
        SCOPED_CONTEXT(std::string("uid '") + uid + "'");
        CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), "11", uid));
        CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), uid, "2"));
    }
    CHECK_EQ(db.snapshot(), before);
}

// Orphaning (remove_bid_from_folder = orphan_bids) clears the folder only; an absent bid is a success that changes nothing, as on Windows.
TEST(Brw12_AssignBid, RemoveFromFolderClearsTheFolderOnlyAndAMissingBidIsASilentSuccess)
{
    TempDb db("remove_from_folder");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (11, 2, 7, 'eleven', 11)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (12, 1, 7, 'twelve', 12)");

    CHECK(ost_store::remove_bid_from_folder(db.path(), "11"));
    CHECK_EQ(db.bids(), std::string("11:N/7 12:1/7"));
    const std::string before = db.snapshot();
    CHECK(ost_store::remove_bid_from_folder(db.path(), "999"));
    CHECK_EQ(db.snapshot(), before);
}

// Reviewer addition. Port of test_unusable_uid_lists_are_refused_with_the_table_named_when_the_policy_is_strict (orphan_bids(["x"]) is
// refused) and of the orphan_bids uid rules (_normalize_int_uids, require_unique_bid_owned_uid_matches -> require_valid_unique_bid_owned_uids:
// '', '0' and '-1' are no bid uid): a uid that is no bid uid is refused and nothing is written; before, the call reported success.
TEST(Brw12_AssignBid, RemoveFromFolderRefusesAUidThatIsNoBidUid)
{
    TempDb db("remove_from_folder_malformed");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName, BidNo) VALUES (11, 2, 7, 'eleven', 11)");
    const std::string before = db.snapshot();

    for (const char *uid : {"", "abc", "0", "-11", "11x", "1.5"})
    {
        SCOPED_CONTEXT(std::string("uid '") + uid + "'");
        CHECK_FALSE(ost_store::remove_bid_from_folder(db.path(), uid));
    }
    CHECK_EQ(db.snapshot(), before);
    CHECK(ost_store::remove_bid_from_folder(db.path(), "011")); // the same number as 11, like int("011") on Windows
    CHECK_EQ(db.bids(), std::string("11:N/7"));
}
