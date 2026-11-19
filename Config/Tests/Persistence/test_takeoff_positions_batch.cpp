// DBP-35 (drag part, alias COND-M03 is the condition part handled elsewhere): the positions one gesture changes (a
// multi-selection drag, an area with its children) are written in ONE transaction.
// Windows oracle: infrastructure/mdb/components/takeoff_operations.py save_takeoff_positions (one connection, one scope check
// require_single_bid_scope_for_uids over every uid, then one UPDATE per row; any exception rolls the whole write back) and
// tests/infrastructure/mdb/components/test_takeoff_operations.py::TakeoffFieldSavePersistenceTests
// (test_bulk_positions_are_saved_unrounded_for_the_listed_takeoffs, test_bulk_position_save_of_nothing_succeeds_without_scope_
// validation, test_bulk_position_and_rotation_failures_roll_back_and_log). iOS before the fix called save_takeoff_position once per
// takeoff: a failure on the second row left the first one written.
#include "StoreTestSupport.hpp"

#include <string>
#include <utility>
#include <vector>

using ost_ptest::TempDb;
using ost_ptest::seed_two_bids;

namespace
{
    using Entries = std::vector<std::pair<std::string, std::vector<double>>>;

    void seed(const TempDb &db)
    {
        seed_two_bids(db);
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (500, 10, 30, 100, '1;2;3;4')");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (501, 10, 31, 100, '5;6;7;8')");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (502, 10, 31, 100, '9;10;11;12')");
    }
}

// Port of test_bulk_positions_are_saved_unrounded_for_the_listed_takeoffs: the listed takeoffs get their full-precision position
// (encode_position with every index preserved, trailing newline), the others are untouched.
TEST(Dbp35_SavePositions, ListedTakeoffsAreWrittenUnrounded)
{
    TempDb db("pos_batch_write");
    seed(db);
    CHECK(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"500", {10.0, 20.123456}}, {"501", {30.5, 40.0}}}));
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 500"), "10.0;20.123456\n");
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 501"), "30.5;40.0\n");
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 502"), "9;10;11;12");
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 900"), "1;2;3;4"); // another bid
}

// Port of test_bulk_position_save_of_nothing_succeeds_without_scope_validation.
TEST(Dbp35_SavePositions, NothingToSaveSucceedsAndWritesNothing)
{
    TempDb db("pos_batch_empty");
    seed(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK(ost_store::save_takeoff_positions(db.path(), "10", Entries{}));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

// DBP-35 test plan: inject a failure on the second row and assert that nothing was applied (a BEFORE UPDATE trigger aborts the
// update of takeoff 501); without the trigger both rows are written.
TEST(Dbp35_SavePositions, AFailureOnTheSecondRowRollsBackTheFirst)
{
    TempDb db("pos_batch_atomic");
    seed(db);
    db.must("CREATE TRIGGER boom BEFORE UPDATE ON BidTakeoffs WHEN NEW.UID = 501 BEGIN SELECT RAISE(ABORT, 'boom'); END");
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK_FALSE(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"500", {7.0, 8.0}}, {"501", {9.0, 10.0}}}));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
    db.must("DROP TRIGGER boom");
    CHECK(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"500", {7.0, 8.0}}, {"501", {9.0, 10.0}}}));
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 500"), "7.0;8.0\n");
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 501"), "9.0;10.0\n");
}

// Port of test_bulk_position_and_rotation_failures_roll_back_and_log ("positions of a missing takeoff": [(1, [1, 2]), (99, [3, 4])]
// returns False and the rows are unchanged) plus the other refusals of require_single_bid_scope_for_uids: a malformed uid, a takeoff
// without a bid, a takeoff of a bid that does not exist, takeoffs of two different bids. Nothing is written in any of them.
TEST(Dbp35_SavePositions, AnyRefusedRowRefusesTheWholeBatchWithoutWriting)
{
    TempDb db("pos_batch_refused");
    seed(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (503, NULL, 30, 100, '1;1')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (504, 77, 30, 100, '2;2')");
    const std::string before = db.snapshot({"BidTakeoffs"});
    const std::vector<double> moved = {1.0, 2.0};
    CHECK_FALSE(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"500", moved}, {"9999", moved}}));  // missing takeoff
    CHECK_FALSE(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"500", moved}, {"abc", moved}}));   // malformed uid
    CHECK_FALSE(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"500", moved}, {"", moved}}));
    CHECK_FALSE(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"500", moved}, {"503", moved}}));   // no bid
    CHECK_FALSE(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"500", moved}, {"504", moved}}));   // bid 77 has no row
    CHECK_FALSE(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"500", moved}, {"900", moved}}));   // two bids (10 and 20)
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

// Windows derives the bid from the takeoffs and never compares it with an argument: a batch that lies wholly in another existing
// bid is written (no stricter scoping than Windows, as for the single write); a repeated uid is fine and the later position wins.
TEST(Dbp35_SavePositions, TheBidArgumentIsNotComparedAndRepeatedUidsAreAllowed)
{
    TempDb db("pos_batch_scope");
    seed(db);
    CHECK(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"900", {3.0, 4.0}}}));
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 900"), "3.0;4.0\n");
    CHECK(ost_store::save_takeoff_positions(db.path(), "10", Entries{{"500", {1.0, 1.0}}, {"500", {2.0, 2.0}}}));
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 500"), "2.0;2.0\n");
}
