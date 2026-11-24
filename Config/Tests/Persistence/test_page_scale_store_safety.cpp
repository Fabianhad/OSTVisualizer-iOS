// Set Scale, DATA SAFETY of the store writer (plan 5.4, tests-first; the highest data risk of M1: an irreversible bulk rewrite of every coordinate of one or all pages).
// Every refusal leaves the WHOLE database byte-identical (ost_ptest::TempDb::snapshot_all, every table), every refusal has its own typed reason, every test that refuses ends with a
// CONTROL run (the same call without the fault succeeds), so a writer that simply refuses everything cannot pass. Failures are injected with TempDb::block (a RAISE(ABORT) trigger).
// Windows source of truth: page_operations.py:28-217 (scope check, stored scale, factor, overlay, the 17 tables, the final UPDATE, one transaction), bid_owned_identity.py:205-235.
// Deliberate differences from Windows, each in the name or the comment of the test (register section 8): SS-1 stored scale refused (kept from Windows), SS-2 overflow / underflow
// refused (Windows writes "inf"), SS-11 all-or-nothing batch, SS-12 a page of another bid refused, the bid lock (4.1) refused inside the transaction.
#include "PageScaleTestSupport.hpp"

#include "Domain/PositionParser.hpp"

#include <cstdio>
#include <string>
#include <vector>

using ost_ptest::ScaleLogCapture;
using ost_ptest::TempDb;
using ost_store::PageScaleOutcome;
using ost_store::PageScaleRefusal;
using std::string;
using std::vector;

namespace
{
    const char *refusal_name(PageScaleRefusal refusal)
    {
        switch (refusal)
        {
        case PageScaleRefusal::None: return "None";
        case PageScaleRefusal::MalformedUid: return "MalformedUid";
        case PageScaleRefusal::EmptyBatch: return "EmptyBatch";
        case PageScaleRefusal::NoSuchBid: return "NoSuchBid";
        case PageScaleRefusal::NoSuchPage: return "NoSuchPage";
        case PageScaleRefusal::ForeignBid: return "ForeignBid";
        case PageScaleRefusal::Locked: return "Locked";
        case PageScaleRefusal::MissingColumns: return "MissingColumns";
        case PageScaleRefusal::BadStoredScale: return "BadStoredScale";
        case PageScaleRefusal::BadNewScale: return "BadNewScale";
        case PageScaleRefusal::NonFiniteResult: return "NonFiniteResult";
        case PageScaleRefusal::Underflow: return "Underflow";
        case PageScaleRefusal::BadOverlayRect: return "BadOverlayRect";
        case PageScaleRefusal::BadLegend: return "BadLegend";
        case PageScaleRefusal::StatementFailed: return "StatementFailed";
        }
        return "?";
    }

    // The default call of the world: the open bid is 10, the new scale (0.25, 12) is the factor 0.5 for a page at (0.125, 12).
    bool save(const TempDb &db, const vector<string> &pages, double sf1, double sf2, PageScaleOutcome *outcome, const string &bid = "10")
    {
        return ost_store::save_page_scales(db.path(), bid, pages, sf1, sf2, outcome);
    }

    // A refusal must name its reason, leave the whole database untouched and say why in the outcome's text.
    void check_refused(const TempDb &db, const string &before, const vector<string> &pages, double sf1, double sf2, const char *expected_refusal, const string &bid = "10")
    {
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, pages, sf1, sf2, &outcome, bid));
        CHECK_STR_EQ(refusal_name(outcome.refusal), expected_refusal);
        CHECK(!outcome.error.empty());
        CHECK_EQ(db.snapshot_all(), before);
    }

    string text_of_row(const TempDb &db, const string &table, int page_index)
    {
        return ost_ptest::position_of(db, table, ost_ptest::world_row_uid(table, page_index));
    }

    // The text a world row has after a factor-0.5 change (page 100: (0.125, 12) -> (0.25, 12)): hardcoded from the real Windows functions (python -B -I, serialization.py,
    // legend_position.py), not computed with the code under test.
    string halved(const string &table)
    {
        if (table == "BidLegends")
            return "<Legends dX=\"15.402\" dY=\"511.339\" Visible=\"255\"><Legend ConditionUID=\"11334\" dX=\"15.402\" dY=\"15.402\" Visible=\"1\"/></Legends>";
        if (table == "BidTakeoffs" || table == "BidComments" || table == "BidTypGroupViews")
            return "5;10;15;20\n";
        if (table == "BidTexts")
            return "5.0;10.0;15.0;20.0;0.5\n"; // the rotation slot keeps its value
        if (table == "BidAnnoInk")
            return "0.5;5.0;10.0;15.0;20.0\n"; // index 0 is the rotation of an ink
        return "5.0;10.0;15.0;20.0\n";
    }
}

// ---- identity: uids, bids, pages (Windows bid_owned_identity.py) --------------------------------------------------------------------------------------

// S1: a uid that is not a positive ASCII decimal ("abc", "0", "-5", " 7", "7 ", "1.0", 2^63) refuses the whole batch before anything is read; a valid page next to a malformed
// one does not save: ONE bad uid refuses ALL. The bid uid is held to the same rule.
TEST(SetScale_StoreSafety, MalformedUidsRefuseTheWholeBatch)
{
    TempDb db("scale_safe_malformed");
    ost_ptest::seed_scale_world(db);
    const string before = db.snapshot_all();
    for (const char *uid : {"abc", "0", "-5", " 7", "7 ", "1.0", "9223372036854775808", "0x10", "1e2", "+5", "\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90"})
    {
        SCOPED_CONTEXT(string("page uid '") + uid + "'");
        check_refused(db, before, {uid}, 0.25, 12.0, "MalformedUid");
        check_refused(db, before, {"100", uid}, 0.25, 12.0, "MalformedUid"); // the good page does not get written
        check_refused(db, before, {uid, "100"}, 0.25, 12.0, "MalformedUid");
    }
    for (const char *bid : {"", "abc", "0", "-5", "10 ", " 10", "1e1", "10.0"})
    {
        SCOPED_CONTEXT(string("bid uid '") + bid + "'");
        check_refused(db, before, {"100"}, 0.25, 12.0, "MalformedUid", bid);
    }
    PageScaleOutcome outcome;
    CHECK(save(db, {"100"}, 0.25, 12.0, &outcome)); // the control
    CHECK_STR_EQ(refusal_name(outcome.refusal), "None");
}

// S2: a valid uid with no row (99, 2147483647, 2147483648, 9223372036854775807) is NoSuchPage; one missing page in a batch refuses the whole batch.
TEST(SetScale_StoreSafety, UnknownPagesAreRefused)
{
    TempDb db("scale_safe_unknown");
    ost_ptest::seed_scale_world(db);
    const string before = db.snapshot_all();
    for (const char *uid : {"99", "2147483647", "2147483648", "9223372036854775807", "007"})
    {
        SCOPED_CONTEXT(string("page uid ") + uid);
        check_refused(db, before, {uid}, 0.25, 12.0, "NoSuchPage");
        check_refused(db, before, {"100", uid}, 0.25, 12.0, "NoSuchPage");
        check_refused(db, before, {uid, "100"}, 0.25, 12.0, "NoSuchPage");
    }
    CHECK(save(db, {"100"}, 0.25, 12.0, nullptr)); // the control (nullptr outcome is allowed)
}

// The 2147483647 limits: Set Scale allocates no uid, so a page whose uid is the largest Access Long (2147483647), above it (2147483648) or the largest 63-bit value is rescaled
// like any other page (the uid is parsed and bound as a 64-bit integer: nothing wraps).
TEST(SetScale_StoreSafety, PageUidsAtTheLimitsAreRescaledLikeAnyOther)
{
    TempDb db("scale_safe_uid_limits");
    ost_ptest::seed_scale_world(db);
    const vector<long long> uids = {2147483647LL, 2147483648LL, 9223372036854775807LL};
    long long takeoff = 700000;
    for (const long long uid : uids)
    {
        db.must("INSERT INTO BidPages (UID, BidUID, Name, ScaleFactor1, ScaleFactor2, Sequence) VALUES (" + std::to_string(uid) + ", 10, 'Big', 0.125, 12.0, 9)");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (" + std::to_string(takeoff++) + ", 10, 30, " + std::to_string(uid) +
                ", '10;20;30;40')");
    }
    const string before_other = db.text("SELECT Position FROM BidTakeoffs WHERE UID = 10000");
    for (const long long uid : uids)
    {
        SCOPED_CONTEXT("page uid " + std::to_string(uid));
        PageScaleOutcome outcome;
        REQUIRE(save(db, {std::to_string(uid)}, 0.25, 12.0, &outcome));
        CHECK_STR_EQ(refusal_name(outcome.refusal), "None");
        CHECK_EQ(ost_ptest::scale_values(db, uid), (std::pair<double, double>(0.25, 12.0)));
    }
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 700000"), "5;10;15;20\n");
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 700001"), "5;10;15;20\n");
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 700002"), "5;10;15;20\n");
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 10000"), before_other); // another page: untouched
}

// S3: a bid that does not exist is refused (NoSuchBid); a page with BidUID NULL or of a bid that has no row is not a page of the open bid (ForeignBid): nothing is written.
TEST(SetScale_StoreSafety, ABidThatDoesNotExistAndPagesWithoutAnOwnerAreRefused)
{
    TempDb db("scale_safe_no_owner");
    ost_ptest::seed_scale_world(db);
    db.must("INSERT INTO BidPages (UID, BidUID, Name, ScaleFactor1, ScaleFactor2) VALUES (300, NULL, 'No bid', 0.125, 12.0)");
    db.must("INSERT INTO BidPages (UID, BidUID, Name, ScaleFactor1, ScaleFactor2) VALUES (301, 77, 'Bid 77 has no row', 0.125, 12.0)");
    const string before = db.snapshot_all();
    check_refused(db, before, {"100"}, 0.25, 12.0, "NoSuchBid", "88");  // the open bid does not exist
    check_refused(db, before, {"301"}, 0.25, 12.0, "NoSuchBid", "77");  // its page names bid 77, which has no Bids row
    check_refused(db, before, {"300"}, 0.25, 12.0, "ForeignBid");       // BidUID NULL: Windows IncoherentBidOwnedScopeError
    check_refused(db, before, {"301"}, 0.25, 12.0, "ForeignBid");       // a page of a bid that is not the open one
    check_refused(db, before, {"100", "300"}, 0.25, 12.0, "ForeignBid");
    CHECK(save(db, {"100"}, 0.25, 12.0, nullptr));
}

// S4 / SS-12: a page of ANOTHER existing bid is refused when the open bid is not its bid (Windows derives the bid from the page and would rescale it); a mixed batch refuses
// entirely and the valid page is not changed.
TEST(SetScale_StoreSafety, APageOfAnotherBidIsRefused_Hardening)
{
    TempDb db("scale_safe_foreign");
    ost_ptest::seed_scale_world(db);
    const string before = db.snapshot_all();
    check_refused(db, before, {"200"}, 0.25, 12.0, "ForeignBid");          // page 200 belongs to bid 20
    check_refused(db, before, {"100"}, 0.25, 12.0, "ForeignBid", "20");    // page 100 belongs to bid 10
    check_refused(db, before, {"100", "200"}, 0.25, 12.0, "ForeignBid");
    check_refused(db, before, {"200", "100"}, 0.25, 12.0, "ForeignBid");
    CHECK(save(db, {"200"}, 0.25, 12.0, nullptr, "20")); // the control: the right bid
}

// S5: a batch is all or nothing: page 100 is fine, the second page is missing / foreign / has an unusable stored scale, and NEITHER page changes.
TEST(SetScale_StoreSafety, ABatchWithOneBadPageChangesNeitherPage)
{
    for (const char *fault : {"missing", "foreign", "stored scale NULL", "stored scale zero", "bad overlay rect", "bad legend", "non-numeric stays"})
    {
        SCOPED_CONTEXT(string("fault: ") + fault);
        TempDb db("scale_safe_batch_fault");
        ost_ptest::seed_scale_world(db);
        string second = "101";
        const char *expected = "NoSuchPage";
        const string kind = fault;
        if (kind == "missing")
            second = "99";
        else if (kind == "foreign")
        {
            second = "200";
            expected = "ForeignBid";
        }
        else if (kind == "stored scale NULL")
        {
            db.must("UPDATE BidPages SET ScaleFactor2 = NULL WHERE UID = 101");
            expected = "BadStoredScale";
        }
        else if (kind == "stored scale zero")
        {
            db.must("UPDATE BidPages SET ScaleFactor1 = 0 WHERE UID = 101");
            expected = "BadStoredScale";
        }
        else if (kind == "bad overlay rect")
        {
            db.must("UPDATE BidPages SET OverlayRect = '1,2,x,4' WHERE UID = 101");
            expected = "BadOverlayRect";
        }
        else if (kind == "bad legend")
        {
            db.must("UPDATE BidLegends SET Position = '<Legends' WHERE BidPageUID = 101");
            expected = "BadLegend";
        }
        if (kind == "non-numeric stays")
        {
            // control of the rule that a non-numeric Position is NOT a reason to refuse (SS-13): the batch writes, the row stays
            db.must("UPDATE BidDimensions SET Position = 'abc' WHERE BidPageUID = 101");
            ScaleLogCapture log;
            REQUIRE(save(db, {"100", "101"}, 0.25, 12.0, nullptr));
            CHECK_EQ(text_of_row(db, "BidDimensions", 1), "abc");
            CHECK_EQ(text_of_row(db, "BidDimensions", 0), halved("BidDimensions"));
            CHECK_EQ(log.warnings().size(), static_cast<size_t>(1));
            continue;
        }
        const string before = db.snapshot_all();
        check_refused(db, before, {"100", second}, 0.25, 12.0, expected);
        check_refused(db, before, {second, "100"}, 0.25, 12.0, expected);
        // the control: without the fault the same batch writes
        TempDb good("scale_safe_batch_ok");
        ost_ptest::seed_scale_world(good);
        REQUIRE(save(good, {"100", "101"}, 0.25, 12.0, nullptr));
        CHECK_EQ(ost_ptest::scale_values(good, 100), (std::pair<double, double>(0.25, 12.0)));
        CHECK_EQ(ost_ptest::scale_values(good, 101), (std::pair<double, double>(0.25, 12.0)));
    }
}

// S6: ["100", "", "100", "101"] writes page 100 once and page 101 once (the empty and the repeated uid are dropped; a page written twice would be scaled twice); the same page
// spelled "7" and "007" is ONE page; ["", ""] and [] are EmptyBatch.
TEST(SetScale_StoreSafety, EmptyAndRepeatedUidsAreDroppedAndAPageIsScaledOnce)
{
    TempDb db("scale_safe_dedupe");
    ost_ptest::seed_scale_world(db);
    PageScaleOutcome outcome;
    REQUIRE(save(db, {"100", "", "100", "101", "0100", "101"}, 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "None");
    CHECK_EQ(text_of_row(db, "BidTakeoffs", 0), "5;10;15;20\n"); // x 0.5 once, never x 0.25
    CHECK_EQ(text_of_row(db, "BidTakeoffs", 1), "5;10;15;20\n");
    CHECK_EQ(text_of_row(db, "BidTakeoffs", 2), "10;20;30;40"); // page 200 of the other bid
    for (const vector<string> &pages : {vector<string>{"", ""}, vector<string>{}, vector<string>{""}})
    {
        PageScaleOutcome empty;
        const string before = db.snapshot_all();
        CHECK_FALSE(save(db, pages, 0.5, 12.0, &empty));
        CHECK_STR_EQ(refusal_name(empty.refusal), "EmptyBatch");
        CHECK(!empty.error.empty());
        CHECK_EQ(db.snapshot_all(), before);
    }
    // the same page under two spellings, with a page uid that is a plain 7
    db.must("INSERT INTO BidPages (UID, BidUID, Name, ScaleFactor1, ScaleFactor2) VALUES (7, 10, 'Seven', 0.125, 12.0)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (7001, 10, 30, 7, '10;20;30;40')");
    REQUIRE(save(db, {"7", "007"}, 0.25, 12.0, nullptr));
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 7001"), "5;10;15;20\n");
}

// Each page of a batch is rescaled by ITS OWN stored scale: page 100 is (0.125, 12) (96 units per inch), page 101 is (0.25, 12) (48); both go to (1, 192) (192 units per inch):
// the factors are 2 and 4, and both pages end with the new pair exactly.
TEST(SetScale_StoreSafety, EachPageOfABatchUsesItsOwnStoredScale)
{
    TempDb db("scale_safe_own_scale");
    ost_ptest::seed_scale_world(db);
    db.must("UPDATE BidPages SET ScaleFactor1 = 0.25 WHERE UID = 101");
    REQUIRE(save(db, {"100", "101"}, 1.0, 192.0, nullptr));
    CHECK_EQ(text_of_row(db, "BidTakeoffs", 0), "20;40;60;80\n");   // x 2
    CHECK_EQ(text_of_row(db, "BidTakeoffs", 1), "40;80;120;160\n"); // x 4
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(1.0, 192.0)));
    CHECK_EQ(ost_ptest::scale_values(db, 101), (std::pair<double, double>(1.0, 192.0)));
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 100"), "20.000000,40.000000,60.000000,80.000000");
}

// ---- what is rewritten and what is not (SS-8, SS-9, the 17 tables, TEXT) ---------------------------------------------------------------------------------

// The 17 tables of the iOS schema, one row each for the three pages: the rows of page 100 are rewritten (three decimals for BidTakeoffs / BidComments / BidTypGroupViews, the exact
// repr for the 13 annotation tables with the rotation slot kept, the legend in place), the rows of page 101 and of bid 20 are byte-identical, every OTHER table is byte-identical
// (snapshot of everything but the 17 tables and BidPages), and BidPages changes ONLY ScaleFactor1 / 2, OverlayRect and the offsets of page 100 (SS-8: ScaleStyle, IsCustomScale,
// Scale stay; SS-9: Quantity, the grid offsets stay).
TEST(SetScale_StoreSafety, TheSeventeenTablesOfThePageAreRewrittenAndNothingElseChanges)
{
    TempDb db("scale_safe_seventeen");
    ost_ptest::seed_scale_world(db);
    const long long takeoff = ost_ptest::world_row_uid("BidTakeoffs", 0);
    db.must("UPDATE BidTakeoffs SET Quantity = 123.5, Count = 4, GridOffsetX = 4.5, GridOffsetY = 5.5, Rotation = 0.25 WHERE UID = " + std::to_string(takeoff));
    vector<string> excluded = ost_ptest::scale_position_tables();
    excluded.push_back("BidPages");
    const string others_before = ost_ptest::snapshot_except(db, excluded);
    const string pages_before = db.text("SELECT group_concat(UID || '|' || ifnull(BidUID,'') || '|' || ifnull(Name,'') || '|' || ifnull(ScaleStyle,'') || '|' || ifnull(IsCustomScale,'') || '|' || "
                                        "ifnull(Scale,'') || '|' || ifnull(Width,'') || '|' || ifnull(Height,''), char(10)) FROM BidPages");
    const string takeoff_before = db.text("SELECT quote(Quantity) || quote(Count) || quote(GridOffsetX) || quote(GridOffsetY) || quote(Rotation) FROM BidTakeoffs WHERE UID = " +
                                          std::to_string(takeoff));
    PageScaleOutcome outcome;
    REQUIRE(save(db, {"100"}, 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "None");
    for (const string &table : ost_ptest::scale_position_tables())
    {
        SCOPED_CONTEXT("table " + table);
        CHECK_EQ(text_of_row(db, table, 0), halved(table));
        CHECK_EQ(ost_ptest::type_of(db, table, "Position", ost_ptest::world_row_uid(table, 0)), "text"); // TEXT, never a blob (plan 4.5)
        CHECK_EQ(text_of_row(db, table, 1), ost_ptest::world_position(table)); // page 101: not even re-encoded
        CHECK_EQ(text_of_row(db, table, 2), ost_ptest::world_position(table)); // bid 20
    }
    CHECK_EQ(ost_ptest::snapshot_except(db, excluded), others_before);
    CHECK_EQ(db.text("SELECT group_concat(UID || '|' || ifnull(BidUID,'') || '|' || ifnull(Name,'') || '|' || ifnull(ScaleStyle,'') || '|' || ifnull(IsCustomScale,'') || '|' || "
                     "ifnull(Scale,'') || '|' || ifnull(Width,'') || '|' || ifnull(Height,''), char(10)) FROM BidPages"),
             pages_before);
    CHECK_EQ(db.text("SELECT quote(Quantity) || quote(Count) || quote(GridOffsetX) || quote(GridOffsetY) || quote(Rotation) FROM BidTakeoffs WHERE UID = " + std::to_string(takeoff)),
             takeoff_before);
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.25, 12.0)));
    CHECK_EQ(ost_ptest::scale_values(db, 101), (std::pair<double, double>(0.125, 12.0)));
    CHECK_EQ(ost_ptest::scale_values(db, 200), (std::pair<double, double>(0.125, 12.0)));
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 100"), "5.000000,10.000000,15.000000,20.000000");
    CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 100"), "5.0");
    CHECK_EQ(db.text("SELECT OverlayOffsetY FROM BidPages WHERE UID = 100"), "10.0");
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 900"), "1;2;3;4"); // the seeded takeoff of bid 20
}

// The scale columns hold the new pair EXACTLY as given, as REAL (not rounded, not text): 0.1 and 1/3 are stored bit for bit.
TEST(SetScale_StoreSafety, TheScaleColumnsHoldTheNewPairExactlyAsGiven)
{
    TempDb db("scale_safe_exact_pair");
    ost_ptest::seed_scale_world(db);
    REQUIRE(save(db, {"100"}, 0.1, 1.0 / 3.0, nullptr));
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.1, 1.0 / 3.0)));
    CHECK_EQ(ost_ptest::type_of(db, "BidPages", "ScaleFactor1", 100), "real");
    CHECK_EQ(ost_ptest::type_of(db, "BidPages", "ScaleFactor2", 100), "real");
    REQUIRE(save(db, {"100"}, 12345678.901234567, 0.000123456789012345, nullptr));
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(12345678.901234567, 0.000123456789012345)));
}

// ---- the bid lock (plan 4.1: inside the transaction) -------------------------------------------------------------------------------------------------

// A bid whose job status is Locked refuses the change inside the transaction; a status that cannot be resolved is locked too (fail closed); an unlocked status and no status write.
TEST(SetScale_StoreSafety, ALockedBidIsRefusedAndAnUnresolvedStatusIsLocked)
{
    {
        TempDb db("scale_safe_locked");
        ost_ptest::seed_scale_world(db);
        ost_ptest::seed_locked_bid(db, 10);
        const string before = db.snapshot_all();
        check_refused(db, before, {"100"}, 0.25, 12.0, "Locked");
        check_refused(db, before, {"100", "101"}, 0.25, 12.0, "Locked");
        CHECK(save(db, {"200"}, 0.25, 12.0, nullptr, "20")); // bid 20 is not locked
    }
    {
        TempDb db("scale_safe_unresolved");
        ost_ptest::seed_scale_world(db);
        ost_ptest::seed_bid_with_status(db, 10, "99"); // a status uid that no JobStatuses row has
        const string before = db.snapshot_all();
        check_refused(db, before, {"100"}, 0.25, 12.0, "Locked");
    }
    {
        TempDb db("scale_safe_open");
        ost_ptest::seed_scale_world(db);
        ost_ptest::seed_open_bid(db, 10);
        REQUIRE(save(db, {"100"}, 0.25, 12.0, nullptr));
        CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.25, 12.0)));
    }
    {
        TempDb db("scale_safe_no_status");
        ost_ptest::seed_scale_world(db);
        db.must("UPDATE Bids SET JobStatusUID = NULL WHERE UID = 10");
        REQUIRE(save(db, {"100"}, 0.25, 12.0, nullptr));
    }
}

// A failed read of the lock state fails closed: with a status the bid cannot be resolved against (the JobStatuses table is gone) the change is refused and nothing is written.
TEST(SetScale_StoreSafety, AFailedLockReadFailsClosed)
{
    TempDb db("scale_safe_lock_read");
    ost_ptest::seed_scale_world(db);
    ost_ptest::seed_open_bid(db, 10);
    db.must("DROP TABLE JobStatuses");
    const string before = db.snapshot_all();
    PageScaleOutcome outcome;
    CHECK_FALSE(save(db, {"100"}, 0.25, 12.0, &outcome));
    CHECK(outcome.refusal == PageScaleRefusal::Locked || outcome.refusal == PageScaleRefusal::StatementFailed);
    CHECK_EQ(db.snapshot_all(), before);
    // the control: a bid WITHOUT a status never reads JobStatuses, so the missing table does not matter and the change is written
    db.must("UPDATE Bids SET JobStatusUID = NULL WHERE UID = 10");
    PageScaleOutcome control;
    REQUIRE(save(db, {"100"}, 0.25, 12.0, &control));
    CHECK_STR_EQ(refusal_name(control.refusal), "None");
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.25, 12.0)));
}

// ---- the scale pair (SS-1, SS-8) -----------------------------------------------------------------------------------------------------------------------

// S8 / SS-1: a stored ScaleFactor1 or 2 that is NULL, 0, negative, +-infinity or text refuses every Set Scale on the page (kept from Windows: no repair), nothing is written,
// even when the other side is valid and the new pair is fine.
TEST(SetScale_StoreSafety, AStoredScaleThatIsNotUsableRefusesTheChange)
{
    for (const char *column : {"ScaleFactor1", "ScaleFactor2"})
    {
        for (const char *value : {"NULL", "0", "0.0", "-0.0", "-1", "-0.125", "9e999", "-9e999", "'abc'", "''"})
        {
            SCOPED_CONTEXT(string(column) + " = " + value);
            TempDb db("scale_safe_stored_scale");
            ost_ptest::seed_scale_world(db);
            db.must(string("UPDATE BidPages SET ") + column + " = " + value + " WHERE UID = 100");
            const string before = db.snapshot_all();
            PageScaleOutcome outcome;
            CHECK_FALSE(save(db, {"100"}, 0.25, 12.0, &outcome));
            CHECK_STR_EQ(refusal_name(outcome.refusal), "BadStoredScale");
            CHECK_STR_EQ(outcome.error, "Page 100 requires finite positive scale factors");
            CHECK_EQ(db.snapshot_all(), before);
            // a repair is not offered: even the pair the page already shows is refused (nothing is repaired by a save)
            CHECK_FALSE(save(db, {"100"}, 0.125, 12.0, nullptr));
            CHECK_EQ(db.snapshot_all(), before);
        }
    }
}

// S8: a new pair with sf1 <= 0, sf2 <= 0, NaN or an infinity, a ratio that overflows to infinity or underflows to 0 (1e-320 over 1e300, 1e300 over 1e-320), is refused even when the
// other side is valid; and so is a change whose FACTOR (new ratio / old ratio) overflows or underflows.
TEST(SetScale_StoreSafety, ANewScaleThatIsNotUsableIsRefused)
{
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const struct
    {
        double sf1, sf2;
    } bad[] = {{0.0, 12.0}, {-0.0, 12.0}, {0.125, 0.0}, {-0.125, 12.0}, {0.125, -12.0}, {nan, 12.0}, {0.125, inf}, {inf, 12.0}, {0.125, nan}, {-inf, 12.0},
               {1e-320, 1e300}, {1e300, 1e-320}, {0.0, 0.0}};
    TempDb db("scale_safe_new_scale");
    ost_ptest::seed_scale_world(db);
    const string before = db.snapshot_all();
    for (const auto &pair : bad)
    {
        SCOPED_CONTEXT("new (" + ost_test::detail::exact_float(pair.sf1) + ", " + ost_test::detail::exact_float(pair.sf2) + ")");
        check_refused(db, before, {"100"}, pair.sf1, pair.sf2, "BadNewScale");
    }
    CHECK(save(db, {"100"}, 0.25, 12.0, nullptr)); // the control
}

// S8 / SS-2 (hardening): a pair whose RATIO OF RATIOS is not finite or underflows to 0 is refused as a bad new scale although both ratios are valid on their own (stored 1e-300 units per
// inch with a new 1e300: factor 1e600 = infinity; stored 1e300 with a new 1e-300: factor 1e-600 = 0). Nothing is written (whole-database snapshot), then a finite factor writes.
TEST(SetScale_StoreSafety, AFactorThatOverflowsOrUnderflowsIsRefused)
{
    {
        TempDb db("scale_safe_factor_overflow");
        ost_ptest::seed_scale_world(db);
        db.must("UPDATE BidPages SET ScaleFactor1 = 1.0, ScaleFactor2 = 1e-300 WHERE UID = 100"); // 1e-300 units per inch
        const string before = db.snapshot_all();
        check_refused(db, before, {"100"}, 1.0, 1e300, "BadNewScale"); // factor 1e600 = infinity
        CHECK(save(db, {"100"}, 1.0, 1e-290, nullptr));                // the control: a finite factor
    }
    {
        TempDb db("scale_safe_factor_underflow");
        ost_ptest::seed_scale_world(db);
        db.must("UPDATE BidPages SET ScaleFactor1 = 1.0, ScaleFactor2 = 1e300 WHERE UID = 100");
        const string before = db.snapshot_all();
        check_refused(db, before, {"100"}, 1.0, 1e-300, "BadNewScale"); // factor 1e-600 = 0
        CHECK(save(db, {"100"}, 1.0, 1e290, nullptr));
    }
}

// ---- content that cannot be rescaled (SS-2, SS-10, SS-13) ----------------------------------------------------------------------------------------------

// S14 / SS-2 (hardening): a coordinate that overflows to infinity or a stored non-finite coordinate refuses the WHOLE change; nothing is written, then a sane factor writes.
TEST(SetScale_StoreSafety, ACoordinateThatOverflowsOrIsNotFiniteRefusesTheWholeChange_Hardening)
{
    struct Fault
    {
        const char *table;
        const char *position;
        double factor_sf2; // the new sf2 (sf1 = 1): units = sf2, the old units are 96
    };
    const Fault faults[] = {
        {"BidTakeoffs", "1e300;1", 9.6e11},       // 1e300 x 1e10 overflows
        {"BidTexts", "1e300;1;2;3", 9.6e11},
        {"BidAnnoInk", "1;1e300", 9.6e11},
        {"BidComments", "1e300;1", 9.6e11},
        {"BidTakeoffs", "inf;1", 192.0},           // a stored infinity
        {"BidDimensions", "1;nan", 192.0},         // a stored NaN
        {"BidTypGroupViews", "1;-inf", 192.0},
    };
    for (const Fault &fault : faults)
    {
        SCOPED_CONTEXT(string(fault.table) + " '" + fault.position + "'");
        TempDb db("scale_safe_overflow");
        ost_ptest::seed_scale_world(db);
        db.must(string("UPDATE ") + fault.table + " SET Position = '" + fault.position + "' WHERE UID = " + std::to_string(ost_ptest::world_row_uid(fault.table, 0)));
        const string before = db.snapshot_all();
        check_refused(db, before, {"100"}, 1.0, fault.factor_sf2, "NonFiniteResult");
        check_refused(db, before, {"101", "100"}, 1.0, fault.factor_sf2, "NonFiniteResult"); // the earlier page of the batch is rolled back too
        db.must(string("UPDATE ") + fault.table + " SET Position = '1;2;3;4' WHERE UID = " + std::to_string(ost_ptest::world_row_uid(fault.table, 0)));
        CHECK(save(db, {"100"}, 1.0, 192.0, nullptr)); // the control run
    }
}

// SS-2 (hardening): a non-zero coordinate that underflows to exactly 0 refuses the change: in a position table, in a legend and in the overlay rectangle. The factor here is 0.5
// ((0.125, 12) -> (0.25, 12)) and the stored value is the smallest double (5e-324).
TEST(SetScale_StoreSafety, ACoordinateThatUnderflowsToZeroRefusesTheChange_Hardening)
{
    struct Fault
    {
        const char *table;
        string value;
        PageScaleRefusal refusal;
    };
    const Fault faults[] = {
        {"BidTakeoffs", "5e-324;1", PageScaleRefusal::Underflow},
        {"BidAnnotationRects", "1;5e-324;2;3", PageScaleRefusal::Underflow},
        {"BidLegends", "<Legends dX=\"5e-324\"/>", PageScaleRefusal::Underflow},
    };
    for (const Fault &fault : faults)
    {
        SCOPED_CONTEXT(string(fault.table));
        TempDb db("scale_safe_underflow");
        ost_ptest::seed_scale_world(db);
        db.must(string("UPDATE ") + fault.table + " SET Position = '" + fault.value + "' WHERE UID = " + std::to_string(ost_ptest::world_row_uid(fault.table, 0)));
        const string before = db.snapshot_all();
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, {"100"}, 0.25, 12.0, &outcome));
        CHECK(outcome.refusal == fault.refusal || outcome.refusal == PageScaleRefusal::BadLegend); // a legend may report its own reason
        CHECK(!outcome.error.empty());
        CHECK_EQ(db.snapshot_all(), before);
    }
    TempDb db("scale_safe_underflow_rect");
    ost_ptest::seed_scale_world(db);
    db.must("UPDATE BidPages SET OverlayRect = '5e-324,0,10,10' WHERE UID = 100");
    const string before = db.snapshot_all();
    check_refused(db, before, {"100"}, 0.25, 12.0, "Underflow");
    db.must("UPDATE BidPages SET OverlayRect = '5,5,10,10' WHERE UID = 100");
    CHECK(save(db, {"100"}, 0.25, 12.0, nullptr)); // the control
}

// S12 / SS-10: a legend that is not well-formed ("<Legends"), has another root, has a dX that is NaN or not a number is BadLegend and rolls back everything written before it
// (the takeoffs, the annotations and the overlay are rewritten first: the legend is the 13th of the 17 tables). The legend of a UTF-8 name (letters outside latin-1) is
// rewritten without failing (Windows fails the whole page change on a non-latin-1 character).
TEST(SetScale_StoreSafety, ABrokenLegendRollsBackTheWholeChangeAndUtf8IsNotAFailure)
{
    for (const char *broken : {"<Legends", "<Other dX=\"1\"/>", "<Legends dX=\"NaN\"/>", "<Legends dX=\"abc\"/>", "", "plain text", "<Legends dX=\"1\"><Legend dX=\"2\"/>"})
    {
        SCOPED_CONTEXT(string("legend '") + broken + "'");
        TempDb db("scale_safe_legend");
        ost_ptest::seed_scale_world(db);
        const long long legend = ost_ptest::world_row_uid("BidLegends", 0);
        db.must(string("UPDATE BidLegends SET Position = '") + broken + "' WHERE UID = " + std::to_string(legend));
        const string before = db.snapshot_all();
        PageScaleOutcome outcome;
        const bool ok = save(db, {"100"}, 0.25, 12.0, &outcome);
        if (string(broken).empty())
        {
            // an empty text is an empty cell: skipped like any empty Position (Windows `if not raw_position`), the change succeeds
            CHECK(ok);
            CHECK_EQ(ost_ptest::position_of(db, "BidLegends", legend), "");
            continue;
        }
        CHECK_FALSE(ok);
        CHECK_STR_EQ(refusal_name(outcome.refusal), "BadLegend");
        CHECK(!outcome.error.empty());
        CHECK_EQ(db.snapshot_all(), before);
    }
    TempDb db("scale_safe_legend_utf8");
    ost_ptest::seed_scale_world(db);
    const string xml = "<Legends dX=\"10\" dY=\"20\" Name=\"\xC5\x82\xC3\xA9 \xE2\x82\xAC \xE4\xB8\xAD \xF0\x9F\x98\x80\"><Legend dX=\"30\"/></Legends>";
    db.must("UPDATE BidLegends SET Position = '" + xml + "' WHERE UID = " + std::to_string(ost_ptest::world_row_uid("BidLegends", 0)));
    REQUIRE(save(db, {"100"}, 0.25, 12.0, nullptr));
    CHECK_EQ(text_of_row(db, "BidLegends", 0), "<Legends dX=\"5\" dY=\"10\" Name=\"\xC5\x82\xC3\xA9 \xE2\x82\xAC \xE4\xB8\xAD \xF0\x9F\x98\x80\"><Legend dX=\"15\"/></Legends>");
    CHECK_EQ(ost_ptest::type_of(db, "BidLegends", "Position", ost_ptest::world_row_uid("BidLegends", 0)), "text");
}

// S10: a malformed or non-finite overlay rectangle ("0,0,2688", "0,0,abc,1920", "0,0,-1,1920", "nan,0,1,1", "0,0,inf,1") is BadOverlayRect BEFORE any write; the offsets of a page
// whose rectangle is empty are not rewritten.
TEST(SetScale_StoreSafety, ABadOverlayRectangleIsRefusedBeforeAnyWrite)
{
    for (const char *rect : {"0,0,2688", "0,0,abc,1920", "0,0,-1,1920", "0,0,1,-1", "nan,0,1,1", "0,0,inf,1", "1,2,3,4,5", ",,,"})
    {
        SCOPED_CONTEXT(string("OverlayRect '") + rect + "'");
        TempDb db("scale_safe_rect");
        ost_ptest::seed_scale_world(db);
        db.must(string("UPDATE BidPages SET OverlayRect = '") + rect + "' WHERE UID = 100");
        const string before = db.snapshot_all();
        check_refused(db, before, {"100"}, 0.25, 12.0, "BadOverlayRect");
        check_refused(db, before, {"101", "100"}, 0.25, 12.0, "BadOverlayRect");
    }
    TempDb db("scale_safe_rect_empty");
    ost_ptest::seed_scale_world(db);
    db.must("UPDATE BidPages SET OverlayRect = '*', OverlayOffsetX = 3.5, OverlayOffsetY = 4.5 WHERE UID = 100");
    REQUIRE(save(db, {"100"}, 0.25, 12.0, nullptr));
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 100"), "*");
    CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 100"), "3.5");
    CHECK_EQ(db.text("SELECT OverlayOffsetY FROM BidPages WHERE UID = 100"), "4.5");
}

// S11 / SS-13 (parity): a row whose Position is not numeric is skipped with ONE warning while the scale changes anyway; the good rows of the same table are rewritten.
TEST(SetScale_StoreSafety, ANonNumericRowIsSkippedWithAWarningAndTheRestIsRewritten)
{
    TempDb db("scale_safe_non_numeric");
    ost_ptest::seed_scale_world(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (777, 10, 30, 100, 'abc;1')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (778, 10, 30, 100, '4;8')");
    ScaleLogCapture log;
    REQUIRE(save(db, {"100"}, 0.25, 12.0, nullptr));
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 777"), "abc;1");
    CHECK_EQ(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 778"), "2;4\n");
    CHECK_EQ(text_of_row(db, "BidTakeoffs", 0), "5;10;15;20\n");
    CHECK_EQ(log.warnings(), vector<string>({"Skipping page-scale rescale for BidTakeoffs UID 777 because Position is not numeric"}));
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.25, 12.0)));
}

// ---- rollback and the transaction (S13, S15) -------------------------------------------------------------------------------------------------------------

// S13: an UPDATE that fails in ANY of the 17 tables, or in BidPages (the overlay write and the final scale write), rolls back everything written before: the whole database
// equals the snapshot, the outcome is StatementFailed; without the trigger the same call writes (control run).
TEST(SetScale_StoreSafety, AFailingUpdateInAnyTableRollsBackTheWholeDatabase)
{
    vector<string> tables = ost_ptest::scale_position_tables();
    tables.push_back("BidPages");
    for (const string &table : tables)
    {
        SCOPED_CONTEXT("blocked table " + table);
        TempDb db("scale_safe_block");
        ost_ptest::seed_scale_world(db);
        const string before = db.snapshot_all();
        const string trigger = db.block(table, "UPDATE");
        ScaleLogCapture log;
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, {"100"}, 0.25, 12.0, &outcome));
        CHECK_STR_EQ(refusal_name(outcome.refusal), "StatementFailed");
        CHECK(!outcome.error.empty());
        CHECK_EQ(db.snapshot_all(), before);
        db.unblock(trigger);
        REQUIRE(save(db, {"100"}, 0.25, 12.0, nullptr)); // the control run
        CHECK_EQ(text_of_row(db, "BidTakeoffs", 0), "5;10;15;20\n");
        CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.25, 12.0)));
    }
}

// A failure on the LAST table of the LAST page of a batch rolls back the first page completely and the earlier tables of the second (BidTypGroupViews is the 17th table).
TEST(SetScale_StoreSafety, ALateFailureOnTheSecondPageRollsBackTheFirstPage)
{
    TempDb db("scale_safe_late");
    ost_ptest::seed_scale_world(db);
    const string before = db.snapshot_all();
    db.must("CREATE TRIGGER boom BEFORE UPDATE ON BidTypGroupViews WHEN NEW.BidPageUID = 101 BEGIN SELECT RAISE(ABORT, 'boom'); END");
    PageScaleOutcome outcome;
    CHECK_FALSE(save(db, {"100", "101"}, 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "StatementFailed");
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(text_of_row(db, "BidTakeoffs", 0), ost_ptest::world_position("BidTakeoffs")); // page 100 was fully rewritten before: it is back
    CHECK_EQ(text_of_row(db, "BidTakeoffs", 1), ost_ptest::world_position("BidTakeoffs"));
    CHECK(save(db, {"100"}, 0.25, 12.0, nullptr)); // page 100 alone does not touch the blocked row
    db.must("DROP TRIGGER boom");
    CHECK(save(db, {"101"}, 0.25, 12.0, nullptr));
}

// The database is not left inside a transaction: after a refusal and after a rolled-back failure another connection can write at once.
TEST(SetScale_StoreSafety, NoTransactionIsLeftOpenAfterARefusalOrAFailure)
{
    TempDb db("scale_safe_no_dangling");
    ost_ptest::seed_scale_world(db);
    CHECK_FALSE(save(db, {"99"}, 0.25, 12.0, nullptr));
    CHECK(db.exec("INSERT INTO BidAreas (UID, BidUID, Name) VALUES (555, 10, 'after refusal')"));
    const string trigger = db.block("BidTakeoffs", "UPDATE");
    CHECK_FALSE(save(db, {"100"}, 0.25, 12.0, nullptr));
    CHECK(db.exec("INSERT INTO BidAreas (UID, BidUID, Name) VALUES (556, 10, 'after failure')"));
    db.unblock(trigger);
    CHECK(save(db, {"100"}, 0.25, 12.0, nullptr));
    CHECK(db.exec("INSERT INTO BidAreas (UID, BidUID, Name) VALUES (557, 10, 'after success')"));
}

// S15: a database that cannot be opened (a directory that does not exist, a file that is not a database) is StatementFailed, nothing is created or written.
TEST(SetScale_StoreSafety, ADatabaseThatCannotBeOpenedIsRefused)
{
    PageScaleOutcome outcome;
    const string missing_dir = ost_test::temp_path("scale_safe_no_such_dir_" + ost_ptest::run_token()) + "/inner/none.db";
    CHECK_FALSE(ost_store::save_page_scale(missing_dir, "10", "100", 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "StatementFailed");
    CHECK(!outcome.error.empty());
    const string junk = ost_test::temp_path("scale_safe_junk_" + ost_ptest::run_token() + ".db");
    {
        std::ofstream file(junk, std::ios::binary);
        file << "this is not a sqlite database, it is only text that is long enough to look like a header ................................................";
    }
    PageScaleOutcome junk_outcome;
    CHECK_FALSE(ost_store::save_page_scale(junk, "10", "100", 0.25, 12.0, &junk_outcome));
    CHECK_STR_EQ(refusal_name(junk_outcome.refusal), "StatementFailed");
    std::remove(junk.c_str());
    // the control: the same call on a real database writes
    TempDb db("scale_safe_open_control");
    ost_ptest::seed_scale_world(db);
    PageScaleOutcome control;
    REQUIRE(ost_store::save_page_scale(db.path(), "10", "100", 0.25, 12.0, &control));
    CHECK_STR_EQ(refusal_name(control.refusal), "None");
}

// S15: a database that another connection holds locked for longer than the busy timeout (5 s) is refused, nothing is written and nothing is left behind; once the holder lets go, the
// same call writes. (This test waits for the busy timeout.)
TEST(SetScale_StoreSafety, ABusyDatabaseIsRefusedAndNothingIsWritten)
{
    TempDb db("scale_safe_busy");
    ost_ptest::seed_scale_world(db);
    const string before = db.snapshot_all();
    {
        ost_db::OstDatabase holder;
        REQUIRE(holder.open(db.path()));
        REQUIRE(holder.begin_transaction()); // BEGIN IMMEDIATE: the write lock
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, {"100"}, 0.25, 12.0, &outcome));
        CHECK_STR_EQ(refusal_name(outcome.refusal), "StatementFailed");
        CHECK(!outcome.error.empty());
        holder.rollback();
    }
    CHECK_EQ(db.snapshot_all(), before);
    CHECK(save(db, {"100"}, 0.25, 12.0, nullptr)); // the control
}

// The outcome is optional and its text is empty on success; every refusal above carried a reason (checked in check_refused).
TEST(SetScale_StoreSafety, TheOutcomeIsOptionalAndEmptyOnSuccess)
{
    TempDb db("scale_safe_outcome");
    ost_ptest::seed_scale_world(db);
    CHECK_FALSE(save(db, {"99"}, 0.25, 12.0, nullptr)); // no outcome: no crash
    PageScaleOutcome outcome;
    outcome.refusal = PageScaleRefusal::StatementFailed; // stale values are overwritten
    outcome.error = "stale";
    REQUIRE(save(db, {"100"}, 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "None");
    CHECK(outcome.error.empty());
    PageScaleOutcome single;
    CHECK(ost_store::save_page_scale(db.path(), "10", "101", 0.25, 12.0, &single)); // the one-page wrapper
    CHECK_STR_EQ(refusal_name(single.refusal), "None");
}

// A failed read: a database without the tables the writer reads (a fresh empty file; a Bids table without BidPages) is refused, nothing is created or written and nothing crashes;
// the same call on a real database writes.
TEST(SetScale_StoreSafety, ADatabaseWithoutTheTablesIsRefused)
{
    {
        TempDb db("scale_safe_empty_db", false);
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, {"100"}, 0.25, 12.0, &outcome));
        CHECK(outcome.refusal == PageScaleRefusal::StatementFailed || outcome.refusal == PageScaleRefusal::NoSuchBid || outcome.refusal == PageScaleRefusal::NoSuchPage);
        CHECK(!outcome.error.empty());
        CHECK_EQ(db.count("SELECT COUNT(*) FROM sqlite_master"), 0); // nothing was created
    }
    {
        TempDb db("scale_safe_no_pages_table", false);
        db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
        db.must("INSERT INTO Bids (UID) VALUES (10)");
        const string before = db.snapshot_all();
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, {"100"}, 0.25, 12.0, &outcome));
        CHECK(!outcome.error.empty());
        CHECK_EQ(db.snapshot_all(), before);
    }
    TempDb good("scale_safe_tables_control");
    ost_ptest::seed_scale_world(good);
    PageScaleOutcome control;
    REQUIRE(save(good, {"100"}, 0.25, 12.0, &control));
    CHECK_STR_EQ(refusal_name(control.refusal), "None");
}
