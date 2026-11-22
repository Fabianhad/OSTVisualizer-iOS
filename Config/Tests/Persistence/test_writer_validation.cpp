// DBP-17 (+UND-04, LP-M03, BRW-12): the writers check existence and ownership like the Windows ones and report a rejected write as
// failure without writing anything.
// Windows oracle: bid_owned_identity.py::require_* used by layer_operations.update_layer_show (require_single_bid_scope_for_uids),
// takeoff_operations (set_takeoffs_negative / save_takeoff_positions / delete_takeoffs: require_single_bid_scope_for_uids) and
// project_operations.move_bids_to_project (require_existing_unique_bid_owned_uid_matches for the bids AND the target project);
//   tests/infrastructure/mdb/components/test_layer_operations.py::test_bid_layer_show_rejects_missing_and_unowned_layers_without_writing
//   tests/infrastructure/mdb/components/test_project_operations.py::test_move_bid_rejects_missing_project_owner_before_mutation,
//   test_move_bids_rejects_missing_batch_member_before_mutation, test_move_bids_assigns_project_and_restore_reference_to_every_batch_member
//   tests/infrastructure/mdb/components/test_takeoff_operations.py (set_takeoffs_negative / delete: duplicate or missing UID rejected).
// What Windows does NOT check stays unchecked (rename of an absent project / folder is a silent success, a layer is not compared with the
// bid that is open).
#include "StoreTestSupport.hpp"

#include <string>

using ost_ptest::TempDb;
using ost_ptest::seed_two_bids;

// ---------------------------------------------------------------------------------------------------------------
// set_layer_visibility
// ---------------------------------------------------------------------------------------------------------------

// Port of test_bid_layer_show_rejects_missing_and_unowned_layers_without_writing: layer 99 does not exist, layer 3 is a template layer
// of no bid (BidUID NULL): both are refused and nothing changes. Item host repro: set_layer_visibility('20', '1', false) changed the
// template layer 1 and returned true.
TEST(Dbp17_LayerVisibility, MissingAndOwnerlessLayersAreRejectedWithoutWriting)
{
    TempDb db("val_layer_reject");
    seed_two_bids(db);
    const std::string before = db.snapshot({"BidLayers"});
    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "10", "99", true));
    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "10", "3", false)); // template layer of no bid
    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "20", "1", false)); // the item's host repro
    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "10", "abc", false));
    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "10", "", false));
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
}

// A layer of a bid that has no Bids row is not an authoritative owner either (require_existing_unique_bid_owned_uid_matches).
TEST(Dbp17_LayerVisibility, LayerOfAMissingBidIsRejected)
{
    TempDb db("val_layer_missing_bid");
    seed_two_bids(db);
    db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (95, 777, 'L95', -1, 1)");
    const std::string before = db.snapshot({"BidLayers"});
    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "10", "95", false));
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
}

// The write itself: a bid layer is shown (-1) or hidden (0). Windows does not compare the layer's bid with the open one, so a layer of
// another existing bid is written too (the verifier's note on DBP-17: do not add stricter scoping than Windows).
TEST(Dbp17_LayerVisibility, BidLayersAreWritten)
{
    TempDb db("val_layer_write");
    seed_two_bids(db);
    CHECK(ost_store::set_layer_visibility(db.path(), "10", "70", false));
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 70"), 0);
    CHECK(ost_store::set_layer_visibility(db.path(), "10", "70", true));
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 70"), -1);
    CHECK(ost_store::set_layer_visibility(db.path(), "10", "90", false)); // a layer of bid 20 (Windows: no comparison with the open bid)
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 90"), 0);
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 3"), -1);   // the template layers were never touched
}

// ---------------------------------------------------------------------------------------------------------------
// takeoff writers
// ---------------------------------------------------------------------------------------------------------------

// Test plan: delete_takeoff / set_takeoff_negative for a uid that is not a takeoff return false and write nothing. Windows
// require_single_bid_scope_for_uids: the target must exist and belong to ONE existing bid (a takeoff without a bid, or whose bid
// has no Bids row, is refused); there is no bid argument to compare, so a takeoff of another existing bid is written (the bid_uid
// of the iOS call is not compared: no stricter scoping than Windows).
TEST(Dbp17_TakeoffWriters, UnknownAndOwnerlessTakeoffsAreRejected)
{
    TempDb db("val_takeoff_writers");
    seed_two_bids(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (500, 10, 30, 100, '1;2;3;4')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (501, NULL, 30, 100, '1;2;3;4')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (502, 77, 30, 100, '1;2;3;4')");
    const std::string before = db.snapshot({"BidTakeoffs"});

    // negative flag
    CHECK_FALSE(ost_store::set_takeoff_negative(db.path(), "10", "9999", true)); // does not exist
    CHECK_FALSE(ost_store::set_takeoff_negative(db.path(), "10", "501", true));  // no bid
    CHECK_FALSE(ost_store::set_takeoff_negative(db.path(), "10", "502", true));  // bid 77 does not exist
    CHECK_FALSE(ost_store::set_takeoff_negative(db.path(), "10", "abc", true));
    CHECK_FALSE(ost_store::set_takeoff_negative(db.path(), "10", "", true));
    CHECK_FALSE(ost_store::set_takeoff_negative(db.path(), "10", "-500", true));
    // position
    const std::vector<double> moved = {7.0, 8.0, 9.0, 10.0};
    CHECK_FALSE(ost_store::save_takeoff_position(db.path(), "10", "9999", moved));
    CHECK_FALSE(ost_store::save_takeoff_position(db.path(), "10", "501", moved));
    CHECK_FALSE(ost_store::save_takeoff_position(db.path(), "10", "502", moved));
    CHECK_FALSE(ost_store::save_takeoff_position(db.path(), "10", "abc", moved));
    // delete
    CHECK_FALSE(ost_store::delete_takeoff(db.path(), "9999"));
    CHECK_FALSE(ost_store::delete_takeoff(db.path(), "501"));
    CHECK_FALSE(ost_store::delete_takeoff(db.path(), "502"));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

TEST(Dbp17_TakeoffWriters, TakeoffsOfTheBidAreWritten)
{
    TempDb db("val_takeoff_writers_ok");
    seed_two_bids(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (500, 10, 30, 100, '1;2;3;4')");
    CHECK(ost_store::set_takeoff_negative(db.path(), "10", "500", true));
    CHECK_EQ(db.count("SELECT IsNegativeQuantity FROM BidTakeoffs WHERE UID = 500"), 1);
    CHECK(ost_store::set_takeoff_negative(db.path(), "10", "500", false));
    CHECK_EQ(db.count("SELECT IsNegativeQuantity FROM BidTakeoffs WHERE UID = 500"), 0);
    CHECK(ost_store::save_takeoff_position(db.path(), "10", "500", {7.0, 8.0, 9.0, 10.0}));
    const ost_data::ProjectData project = ost_store::load_project(db.path(), "10");
    REQUIRE_EQ(project.takeoffs.size(), static_cast<size_t>(1));
    CHECK_EQ(project.takeoffs[0].position, (std::vector<double>{7.0, 8.0, 9.0, 10.0}));
    // a takeoff of another existing bid is written too: Windows derives the bid from the takeoff and never compares it
    CHECK(ost_store::set_takeoff_negative(db.path(), "10", "900", true));
    CHECK_EQ(db.count("SELECT IsNegativeQuantity FROM BidTakeoffs WHERE UID = 900"), 1);
    CHECK(ost_store::delete_takeoff(db.path(), "500"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 500"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 900"), 1); // the other bid's takeoff stays
}

// ---------------------------------------------------------------------------------------------------------------
// assign_bid_to_folder (move_bids_to_project)
// ---------------------------------------------------------------------------------------------------------------

// Port of test_move_bid_rejects_missing_project_owner_before_mutation (Bids (11, NULL); move 11 to project 99 -> False, the bid stays
// unassigned) and test_move_bids_rejects_missing_batch_member_before_mutation (a missing bid).
TEST(Dbp17_AssignBidToFolder, MissingFolderOrBidIsRejectedWithoutWriting)
{
    TempDb db("val_assign_reject");
    seed_two_bids(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (11, NULL, 'loose', 11)");
    const std::string before = db.snapshot({"Bids"});
    CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), "11", "99"));   // BRW-12 plan: assign(bid, '99') -> false
    CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), "11", "77"));   // folder 77 does not exist
    CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), "77", "2"));    // bid 77 does not exist
    CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), "abc", "2"));
    CHECK_FALSE(ost_store::assign_bid_to_folder(db.path(), "11", "abc"));
    CHECK_EQ(db.snapshot({"Bids"}), before);
    CHECK_STR_EQ(db.text("SELECT typeof(BidProjectUID) FROM Bids WHERE UID = 11"), "null");
}

// Port of test_move_bids_assigns_project_and_restore_reference_to_every_batch_member for one bid: an existing bid moves to an
// existing project.
TEST(Dbp17_AssignBidToFolder, ExistingBidMovesToAnExistingFolder)
{
    TempDb db("val_assign_ok");
    seed_two_bids(db);
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (3, 'Other')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (11, NULL, 'loose', 11)");
    CHECK(ost_store::assign_bid_to_folder(db.path(), "11", "3"));
    CHECK_EQ(db.count("SELECT BidProjectUID FROM Bids WHERE UID = 11"), 3);
    CHECK(ost_store::assign_bid_to_folder(db.path(), "11", "2"));
    CHECK_EQ(db.count("SELECT BidProjectUID FROM Bids WHERE UID = 11"), 2);
    CHECK_EQ(db.count("SELECT BidProjectUID FROM Bids WHERE UID = 10"), 2); // untouched
}

// Windows pins the silent successes: renaming an absent project (rename_project: require_unique_bid_owned_uid_matches does not require
// a row) is a success that changes nothing; iOS keeps that for rename_folder.
TEST(Dbp17_AssignBidToFolder, RenameOfAnAbsentFolderStaysASilentSuccess)
{
    TempDb db("val_rename_absent");
    seed_two_bids(db);
    const std::string before = db.snapshot({"BidProjects"});
    CHECK(ost_store::rename_folder(db.path(), "999", "Renamed"));
    CHECK_EQ(db.snapshot({"BidProjects"}), before);
    CHECK(ost_store::rename_folder(db.path(), "2", "Renamed"));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidProjects WHERE UID = 2"), "Renamed");
}
