// UND-02 / DBP-35: one undo or redo step changes SEVERAL takeoffs (a deleted area with its holes comes back, a delete is redone):
// the rows of the step are written in ONE transaction, all or nothing, so a failure never leaves half of a step behind.
// Windows oracle: infrastructure/mdb/components/takeoff_operations.py
//   delete_takeoffs (334-370) / _run_delete_takeoffs (372-419): uids must be positive integers, an empty list is True, ONE
//   transaction with require_single_bid_scope_for_uids over all of them (all exist, one bid), then the cleanup and the delete of the
//   whole set; any failure rolls everything back and returns False;
//   insert_takeoffs (421-): ONE transaction, every spec validated (condition / page / area / parent of the bid), all inserted or none
//   ([] on failure, [] for nothing to insert).
// and tests/infrastructure/mdb/components/test_takeoff_operations.py (the bulk delete / insert failure tests). The iOS addition: the
// parent of a takeoff may be an EARLIER takeoff of the same list (a restore re-inserts the parent and its holes together with
// their old uids).
#include "StoreTestSupport.hpp"

#include <string>
#include <vector>

using ost_ptest::TempDb;
using ost_ptest::make_takeoff;
using ost_ptest::seed_two_bids;
using Uids = std::vector<std::string>;

namespace
{
    void seed(const TempDb &db)
    {
        seed_two_bids(db);
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position) VALUES (500, 10, 30, 100, 0, '0;0;10;0;10;10;0;10')");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position) VALUES (501, 10, 30, 100, 500, '1;1;2;1;2;2;1;2')");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position) VALUES (502, 10, 31, 100, 0, '5;5;6;6')");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position) VALUES (503, 10, 31, 101, 0, '7;7;8;8')");
    }

    std::string remaining(const TempDb &db) { return ost_ptest::list(db, "SELECT UID FROM BidTakeoffs ORDER BY UID"); }
}

// ---------------------------------------------------------------------------------------------------------------
// delete_takeoffs
// ---------------------------------------------------------------------------------------------------------------

// The whole set goes together (a parent with its hole), the other takeoffs and the other bid stay.
TEST(Und02_DeleteTakeoffs, TheWholeSetIsDeleted)
{
    TempDb db("batch_delete");
    seed(db);
    CHECK(ost_store::delete_takeoffs(db.path(), Uids{"500", "501"}));
    CHECK_STR_EQ(remaining(db), "502,503,900");
}

// A failure on the second takeoff keeps the first one (a BEFORE DELETE trigger refuses 501); without the trigger both go.
TEST(Und02_DeleteTakeoffs, AFailureOnTheSecondTakeoffRollsBackTheFirst)
{
    TempDb db("batch_delete_atomic");
    seed(db);
    db.must("CREATE TRIGGER boom BEFORE DELETE ON BidTakeoffs WHEN OLD.UID = 501 BEGIN SELECT RAISE(ABORT, 'boom'); END");
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK_FALSE(ost_store::delete_takeoffs(db.path(), Uids{"500", "501"}));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
    db.must("DROP TRIGGER boom");
    CHECK(ost_store::delete_takeoffs(db.path(), Uids{"500", "501"}));
    CHECK_STR_EQ(remaining(db), "502,503,900");
}

// require_single_bid_scope_for_uids: a missing takeoff, a malformed uid and takeoffs of two bids refuse the whole set.
TEST(Und02_DeleteTakeoffs, AnyRefusedUidRefusesTheWholeSet)
{
    TempDb db("batch_delete_refused");
    seed(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK_FALSE(ost_store::delete_takeoffs(db.path(), Uids{"500", "9999"}));
    CHECK_FALSE(ost_store::delete_takeoffs(db.path(), Uids{"500", "abc"}));
    CHECK_FALSE(ost_store::delete_takeoffs(db.path(), Uids{"500", ""}));
    CHECK_FALSE(ost_store::delete_takeoffs(db.path(), Uids{"500", "900"})); // bid 10 and bid 20
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

// An empty list is a success that writes nothing; a uid listed twice is deleted once.
TEST(Und02_DeleteTakeoffs, AnEmptyListSucceedsAndRepeatedUidsCountOnce)
{
    TempDb db("batch_delete_empty");
    seed(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK(ost_store::delete_takeoffs(db.path(), Uids{}));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
    CHECK(ost_store::delete_takeoffs(db.path(), Uids{"502", "502"}));
    CHECK_STR_EQ(remaining(db), "500,501,503,900");
}

// The cleanup of delete_takeoff applies to the set: the survivors that referenced a deleted takeoff are detached (ParentUID NULL).
TEST(Und02_DeleteTakeoffs, SurvivorsThatReferencedADeletedTakeoffAreDetached)
{
    TempDb db("batch_delete_detach");
    seed(db);
    CHECK(ost_store::delete_takeoffs(db.path(), Uids{"500"})); // 501 is NOT in the set
    CHECK_STR_EQ(remaining(db), "501,502,503,900");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT ifnull(ParentUID, 'N') FROM BidTakeoffs WHERE UID = 501"), "N");
}

// ---------------------------------------------------------------------------------------------------------------
// insert_takeoffs
// ---------------------------------------------------------------------------------------------------------------

// Delete a set and restore it in one step: the rows come back with their uids, the hole under its parent.
TEST(Und02_InsertTakeoffs, ASetIsRestoredWithItsParentLinks)
{
    TempDb db("batch_insert_restore");
    seed(db);
    ost_domain::Takeoff parent = make_takeoff("30", "100", "0", "0", "500");
    parent.position = {0, 0, 10, 0, 10, 10, 0, 10};
    ost_domain::Takeoff hole = make_takeoff("30", "100", "0", "500", "501");
    hole.position = {1, 1, 2, 1, 2, 2, 1, 2};
    REQUIRE(ost_store::delete_takeoffs(db.path(), Uids{"500", "501"}));
    REQUIRE_EQ(remaining(db), "502,503,900");

    Uids uids;
    CHECK(ost_store::insert_takeoffs(db.path(), "10", {parent, hole}, uids)); // the parent first: the hole names it
    REQUIRE_EQ(uids.size(), static_cast<size_t>(2));
    CHECK_STR_EQ(uids[0], "500");
    CHECK_STR_EQ(uids[1], "501");
    CHECK_STR_EQ(remaining(db), "500,501,502,503,900");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT ParentUID FROM BidTakeoffs WHERE UID = 501"), "500");
    CHECK_STR_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 501"), "1.0;1.0;2.0;1.0;2.0;2.0;1.0;2.0\n");
}

// All or nothing: the second takeoff names a condition the bid does not have, so the first is not inserted either.
TEST(Und02_InsertTakeoffs, ARefusedTakeoffRollsBackTheOnesBeforeIt)
{
    TempDb db("batch_insert_refused");
    seed(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    const ost_domain::Takeoff good = make_takeoff("30", "100", "0", "0", "600");
    const ost_domain::Takeoff bad = make_takeoff("777", "100", "0", "0", "601"); // condition 777 does not exist
    Uids uids{"stale"};
    CHECK_FALSE(ost_store::insert_takeoffs(db.path(), "10", {good, bad}, uids));
    CHECK(uids.empty());
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

// A failure of a statement of the second row (a BEFORE INSERT trigger) rolls back the first as well; without the trigger both are
// inserted.
TEST(Und02_InsertTakeoffs, AFailedStatementRollsBackTheBatch)
{
    TempDb db("batch_insert_atomic");
    seed(db);
    db.must("CREATE TRIGGER boom BEFORE INSERT ON BidTakeoffs WHEN NEW.UID = 601 BEGIN SELECT RAISE(ABORT, 'boom'); END");
    const std::string before = db.snapshot({"BidTakeoffs"});
    const ost_domain::Takeoff first = make_takeoff("30", "100", "0", "0", "600");
    const ost_domain::Takeoff second = make_takeoff("30", "100", "0", "0", "601");
    Uids uids;
    CHECK_FALSE(ost_store::insert_takeoffs(db.path(), "10", {first, second}, uids));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
    db.must("DROP TRIGGER boom");
    CHECK(ost_store::insert_takeoffs(db.path(), "10", {first, second}, uids));
    CHECK_STR_EQ(remaining(db), "500,501,502,503,600,601,900");
}

// An explicit uid that is taken refuses the batch (the single insert's rule), whatever comes first.
TEST(Und02_InsertTakeoffs, ADuplicateExplicitUidRefusesTheBatch)
{
    TempDb db("batch_insert_duplicate");
    seed(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    Uids uids;
    CHECK_FALSE(ost_store::insert_takeoffs(db.path(), "10", {make_takeoff("30", "100", "0", "0", "600"), make_takeoff("30", "100", "0", "0", "500")}, uids));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
    // the same uid twice inside the batch
    CHECK_FALSE(ost_store::insert_takeoffs(db.path(), "10", {make_takeoff("30", "100", "0", "0", "600"), make_takeoff("30", "100", "0", "0", "600")}, uids));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

// A parent on another page is refused (Windows: "A Takeoff and its parent must belong to the same Page"), the batch with it.
TEST(Und02_InsertTakeoffs, AParentOnAnotherPageRefusesTheBatch)
{
    TempDb db("batch_insert_parent_page");
    seed(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    Uids uids;
    CHECK_FALSE(ost_store::insert_takeoffs(db.path(), "10", {make_takeoff("30", "100", "0", "0", "600"), make_takeoff("30", "100", "0", "503", "601")}, uids));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

// Without explicit uids the new rows get consecutive uids above the largest one.
TEST(Und02_InsertTakeoffs, NewTakeoffsGetFreshUidsInOrder)
{
    TempDb db("batch_insert_fresh");
    seed(db);
    Uids uids;
    CHECK(ost_store::insert_takeoffs(db.path(), "10", {make_takeoff("30", "100"), make_takeoff("31", "100")}, uids));
    CHECK_STR_EQ(remaining(db), "500,501,502,503,900,901,902");
    REQUIRE_EQ(uids.size(), static_cast<size_t>(2));
    CHECK_STR_EQ(uids[0], "901");
    CHECK_STR_EQ(uids[1], "902");
}

// Nothing to insert succeeds without touching the database.
TEST(Und02_InsertTakeoffs, AnEmptyListSucceeds)
{
    TempDb db("batch_insert_empty");
    seed(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    Uids uids{"stale"};
    CHECK(ost_store::insert_takeoffs(db.path(), "10", {}, uids));
    CHECK(uids.empty());
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

// ---------------------------------------------------------------------------------------------------------------
// set_takeoffs_negative (UND-09): one user action sets the flag of several takeoffs, one undo restores their old values
// ---------------------------------------------------------------------------------------------------------------
// Windows: takeoff_operations.py set_takeoffs_negative -> _update_selected_takeoffs_value (106-166): uids normalised (invalid ->
// False), empty -> True, ONE transaction with the scope check over all uids; plan_view_action_handler.py on_set_negative
// (2072-2116) writes it for every selected takeoff and records the old values for undo.

namespace
{
    using Negatives = std::vector<std::pair<std::string, bool>>;
    std::string flags(const TempDb &db) { return ost_ptest::list(db, "SELECT UID || ':' || ifnull(IsNegativeQuantity, 'N') FROM BidTakeoffs WHERE BidUID = 10 ORDER BY UID"); }
}

// Several takeoffs change together, each to its own value (an undo writes the old, possibly mixed, values back).
TEST(Und09_SetNegatives, EveryListedTakeoffIsWrittenWithItsOwnValue)
{
    TempDb db("neg_batch_write");
    seed(db);
    CHECK(ost_store::set_takeoffs_negative(db.path(), "10", Negatives{{"500", true}, {"502", true}}));
    CHECK_STR_EQ(flags(db), "500:1,501:0,502:1,503:0");
    CHECK(ost_store::set_takeoffs_negative(db.path(), "10", Negatives{{"500", false}, {"502", true}, {"503", true}}));
    CHECK_STR_EQ(flags(db), "500:0,501:0,502:1,503:1");
}

// All or nothing: a failure on the second row (BEFORE UPDATE trigger) keeps the first unwritten.
TEST(Und09_SetNegatives, AFailureOnTheSecondRowRollsBackTheFirst)
{
    TempDb db("neg_batch_atomic");
    seed(db);
    db.must("CREATE TRIGGER boom BEFORE UPDATE ON BidTakeoffs WHEN NEW.UID = 502 BEGIN SELECT RAISE(ABORT, 'boom'); END");
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK_FALSE(ost_store::set_takeoffs_negative(db.path(), "10", Negatives{{"500", true}, {"502", true}}));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
    db.must("DROP TRIGGER boom");
    CHECK(ost_store::set_takeoffs_negative(db.path(), "10", Negatives{{"500", true}, {"502", true}}));
    CHECK_STR_EQ(flags(db), "500:1,501:0,502:1,503:0");
}

// require_single_bid_scope_for_uids: a missing takeoff, a malformed uid and takeoffs of two bids refuse the whole list.
TEST(Und09_SetNegatives, AnyRefusedUidRefusesTheWholeList)
{
    TempDb db("neg_batch_refused");
    seed(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK_FALSE(ost_store::set_takeoffs_negative(db.path(), "10", Negatives{{"500", true}, {"9999", true}}));
    CHECK_FALSE(ost_store::set_takeoffs_negative(db.path(), "10", Negatives{{"500", true}, {"abc", true}}));
    CHECK_FALSE(ost_store::set_takeoffs_negative(db.path(), "10", Negatives{{"500", true}, {"900", true}}));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

// An empty list is a success that writes nothing.
TEST(Und09_SetNegatives, AnEmptyListSucceeds)
{
    TempDb db("neg_batch_empty");
    seed(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK(ost_store::set_takeoffs_negative(db.path(), "10", Negatives{}));
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}
