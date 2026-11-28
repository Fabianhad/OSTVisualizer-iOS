// Set Scale, store writer: tests ADDED BY THE INDEPENDENT REVIEW for the mutants a lazy implementation would survive (plan 4.4: the order of the checks, plan 5.4: what a change
// whose factor is within the epsilon may and may not read). Windows source of truth: page_operations.py:57-82 (the content is read and rewritten only when
// abs(factor - 1.0) > SCALE_EPSILON), plan 4.4 item 2 (parse the uids, bid exists, rows belong to the OPEN bid, bid lock, name / value rules, the writes).
// Every refusal leaves the WHOLE database byte-identical (snapshot_all) and ends with a control run.
#include "PageScaleTestSupport.hpp"

#include <string>
#include <vector>

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

    string refusal_of(const TempDb &db, const vector<string> &pages, double sf1, double sf2, const string &bid = "10")
    {
        PageScaleOutcome outcome;
        if (ost_store::save_page_scales(db.path(), bid, pages, sf1, sf2, &outcome))
            return "written";
        return refusal_name(outcome.refusal);
    }
}

// Plan 4.4 item 2: the order is uids, bid, pages of the open bid, LOCK, values. On a LOCKED bid every earlier problem still names itself (a malformed uid, an empty batch, a bid that
// does not exist, a page that does not exist, a page of another bid) and the lock hides every LATER one (a stored scale that is NULL, a new scale that is not usable, a malformed
// overlay rectangle, a broken legend): the user is told that the bid is locked, not about a value rule of a bid they may not edit. The whole database is unchanged each time.
TEST(SetScale_StoreReview, TheLockComesAfterTheOwnershipChecksAndBeforeTheValueRules)
{
    TempDb db("scale_review_order");
    ost_ptest::seed_scale_world(db);
    ost_ptest::seed_locked_bid(db, 10);
    // the value faults of the pages of the locked bid 10
    db.must("UPDATE BidPages SET ScaleFactor2 = NULL WHERE UID = 101");
    db.must("UPDATE BidPages SET OverlayRect = '1,2,x,4' WHERE UID = 100");
    db.must("UPDATE BidLegends SET Position = '<Legends' WHERE BidPageUID = 100");
    const string before = db.snapshot_all();

    // before the lock
    CHECK_EQ(refusal_of(db, {"abc"}, 0.25, 12.0), "MalformedUid");
    CHECK_EQ(refusal_of(db, {"", ""}, 0.25, 12.0), "EmptyBatch");
    CHECK_EQ(refusal_of(db, {"100"}, 0.25, 12.0, "88"), "NoSuchBid"); // bid 88 has no row
    CHECK_EQ(refusal_of(db, {"999"}, 0.25, 12.0), "NoSuchPage");
    CHECK_EQ(refusal_of(db, {"200"}, 0.25, 12.0), "ForeignBid");      // a page of bid 20 on the locked bid 10
    CHECK_EQ(refusal_of(db, {"100", "200"}, 0.25, 12.0), "ForeignBid");
    // the lock, whatever value fault the pages have
    CHECK_EQ(refusal_of(db, {"100"}, 0.25, 12.0), "Locked");          // malformed rectangle + broken legend behind it
    CHECK_EQ(refusal_of(db, {"101"}, 0.25, 12.0), "Locked");          // stored ScaleFactor2 NULL behind it
    CHECK_EQ(refusal_of(db, {"100", "101"}, 0.25, 12.0), "Locked");
    CHECK_EQ(refusal_of(db, {"100"}, 0.0, 12.0), "Locked");           // a new scale that is not usable
    CHECK_EQ(refusal_of(db, {"100"}, 1e-320, 1e300), "Locked");
    CHECK_EQ(db.snapshot_all(), before);

    // the control: the same faults on an UNLOCKED bid name their own reasons (so the lock above really came first)
    db.must("UPDATE Bids SET JobStatusUID = NULL WHERE UID = 10");
    CHECK_EQ(refusal_of(db, {"101"}, 0.25, 12.0), "BadStoredScale");
    CHECK_EQ(refusal_of(db, {"100"}, 0.0, 12.0), "BadNewScale");
    CHECK_EQ(refusal_of(db, {"100"}, 0.25, 12.0), "BadOverlayRect");
    db.must("UPDATE BidPages SET OverlayRect = '10,20,30,40' WHERE UID = 100");
    CHECK_EQ(refusal_of(db, {"100"}, 0.25, 12.0), "BadLegend");
}

// Windows page_operations.py:78-82: a change whose factor is within the epsilon of 1 neither reads nor rewrites any content, so content that could not be rescaled (a malformed
// overlay rectangle, a legend that is not XML, a coordinate that is not finite, a row that is not numeric) is NOT a reason to refuse it, raises no warning, and every content table
// stays byte-identical; only the two scale columns change. (0.125, 12) -> (0.25, 24) is the same ratio, (0.125, 12 * (1 + 1e-12)) is a factor below the epsilon.
TEST(SetScale_StoreReview, AChangeWithinTheEpsilonNeverReadsTheContent)
{
    const struct
    {
        double sf1, sf2;
    } same_ratio[] = {{0.25, 24.0}, {0.125, 12.0 * (1.0 + 1e-12)}, {0.125, 12.0}};
    for (const auto &pair : same_ratio)
    {
        SCOPED_CONTEXT("new pair (" + ost_test::detail::exact_float(pair.sf1) + ", " + ost_test::detail::exact_float(pair.sf2) + ")");
        TempDb db("scale_review_within_epsilon");
        ost_ptest::seed_scale_world(db);
        db.must("UPDATE BidPages SET OverlayRect = '0,0,abc,1920', OverlayOffsetX = 3.5, OverlayOffsetY = 4.5 WHERE UID = 100");
        db.must("UPDATE BidLegends SET Position = '<Legends' WHERE BidPageUID = 100");
        db.must("UPDATE BidTakeoffs SET Position = 'inf;1' WHERE BidPageUID = 100");
        db.must("UPDATE BidDimensions SET Position = 'not numeric' WHERE BidPageUID = 100");
        const string content_before = ost_ptest::snapshot_except(db, {"BidPages"});
        const string rect_before = db.text("SELECT OverlayRect || '|' || OverlayOffsetX || '|' || OverlayOffsetY FROM BidPages WHERE UID = 100");
        ost_ptest::ScaleLogCapture log;
        PageScaleOutcome outcome;
        REQUIRE(ost_store::save_page_scales(db.path(), "10", {"100"}, pair.sf1, pair.sf2, &outcome));
        CHECK_STR_EQ(refusal_name(outcome.refusal), "None");
        CHECK(log.warnings().empty());
        CHECK_EQ(ost_ptest::snapshot_except(db, {"BidPages"}), content_before);
        CHECK_EQ(db.text("SELECT OverlayRect || '|' || OverlayOffsetX || '|' || OverlayOffsetY FROM BidPages WHERE UID = 100"), rect_before);
        CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(pair.sf1, pair.sf2)));
        // the same content IS refused as soon as the factor is real (the control: the faults are real faults)
        CHECK_FALSE(ost_store::save_page_scales(db.path(), "10", {"100"}, 0.5, 12.0, nullptr));
    }
}

// The factor is computed per page from ITS stored scale: in a batch a page that is already at the new scale is a no-op that must not read its (broken) content, while the other page
// is rescaled. (Page 101 is at the new pair (0.25, 12) with a broken legend; page 100 is at (0.125, 12).)
TEST(SetScale_StoreReview, APageAlreadyAtTheNewScaleInABatchIsNotReadAndTheOtherPageIsRescaled)
{
    TempDb db("scale_review_batch_noop");
    ost_ptest::seed_scale_world(db);
    db.must("UPDATE BidPages SET ScaleFactor1 = 0.25, ScaleFactor2 = 12.0 WHERE UID = 101");
    db.must("UPDATE BidLegends SET Position = '<Legends' WHERE BidPageUID = 101");
    db.must("UPDATE BidPages SET OverlayRect = '1,2,x,4' WHERE UID = 101");
    const string page101_content_before =
        db.text("SELECT group_concat(Position, '|') FROM BidTakeoffs WHERE BidPageUID = 101") + db.text("SELECT Position FROM BidLegends WHERE BidPageUID = 101");
    ost_store::PageScaleOutcome outcome;
    REQUIRE(ost_store::save_page_scales(db.path(), "10", {"100", "101"}, 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "None");
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", ost_ptest::world_row_uid("BidTakeoffs", 0)), "5;10;15;20\n");
    CHECK_EQ(db.text("SELECT group_concat(Position, '|') FROM BidTakeoffs WHERE BidPageUID = 101") + db.text("SELECT Position FROM BidLegends WHERE BidPageUID = 101"),
             page101_content_before);
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 101"), "1,2,x,4");
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.25, 12.0)));
    CHECK_EQ(ost_ptest::scale_values(db, 101), (std::pair<double, double>(0.25, 12.0)));
}

// A batch is also all-or-nothing when the LAST page of the batch is locked-out by a late value fault and the first page was already fully written: the first page's overlay rectangle,
// offsets, positions and scale come back (they are the only things the first page changes), checked field by field on top of the snapshot.
TEST(SetScale_StoreReview, ALateRefusalOnTheSecondPageRestoresEveryFieldOfTheFirstPage)
{
    TempDb db("scale_review_late_refusal");
    ost_ptest::seed_scale_world(db);
    db.must("UPDATE BidTypGroupViews SET Position = '1;1e300' WHERE BidPageUID = 101"); // the 17th table of the second page overflows at factor 1e10
    const string before = db.snapshot_all();
    const string overlay_before = db.text("SELECT OverlayRect || '|' || OverlayOffsetX || '|' || OverlayOffsetY FROM BidPages WHERE UID = 100");
    PageScaleOutcome outcome;
    CHECK_FALSE(ost_store::save_page_scales(db.path(), "10", {"100", "101"}, 1.0, 9.6e11, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "NonFiniteResult");
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.text("SELECT OverlayRect || '|' || OverlayOffsetX || '|' || OverlayOffsetY FROM BidPages WHERE UID = 100"), overlay_before);
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.125, 12.0)));
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", ost_ptest::world_row_uid("BidTakeoffs", 0)), ost_ptest::world_position("BidTakeoffs"));
    CHECK_EQ(ost_ptest::position_of(db, "BidLegends", ost_ptest::world_row_uid("BidLegends", 0)), ost_ptest::world_position("BidLegends"));
    // the control: without the fault the same batch writes
    db.must("UPDATE BidTypGroupViews SET Position = '1;2' WHERE BidPageUID = 101");
    CHECK(ost_store::save_page_scales(db.path(), "10", {"100", "101"}, 1.0, 9.6e11, nullptr));
}

namespace
{
    // The upper-case hex of the bytes (what SQLite's hex() prints).
    string hex_of(const string &bytes)
    {
        static const char digits[] = "0123456789ABCDEF";
        string out;
        for (const unsigned char c : bytes)
        {
            out += digits[c >> 4];
            out += digits[c & 15];
        }
        return out;
    }
}

// The store-level twin of the Core scanner test: a legend BLOB that holds a LONE latin-1 byte (0xE9, "caf" + E9 is NOT valid UTF-8; what the Windows writer stores) is rescaled in place,
// the 0xE9 survives byte for byte and the cell is TEXT afterwards (the older store test of this name uses the UTF-8 spelling of the letter, which is valid UTF-8). A legend whose
// ASCII part is broken is still refused with the same bytes around it, so "opaque high bytes" never turns into "accept anything".
TEST(SetScale_StoreReview, ALegendBlobWithALoneLatin1ByteIsRescaledAndTheByteSurvives)
{
    TempDb db("scale_review_latin1_legend");
    ost_ptest::seed_scale_world(db);
    const long long legend = ost_ptest::world_row_uid("BidLegends", 0);
    const string stored = "<Legends dX=\"10\" Name=\"caf\xE9\"><Legend dX=\"20\"/></Legends>";
    const string expected = "<Legends dX=\"5\" Name=\"caf\xE9\"><Legend dX=\"10\"/></Legends>";
    db.must("UPDATE BidLegends SET Position = X'" + hex_of(stored) + "' WHERE UID = " + std::to_string(legend));
    CHECK_EQ(ost_ptest::type_of(db, "BidLegends", "Position", legend), "blob");
    REQUIRE(ost_store::save_page_scales(db.path(), "10", {"100"}, 0.25, 12.0, nullptr));
    CHECK_EQ(db.text("SELECT hex(Position) FROM BidLegends WHERE UID = " + std::to_string(legend)), hex_of(expected));
    CHECK_EQ(ost_ptest::type_of(db, "BidLegends", "Position", legend), "text");

    TempDb broken("scale_review_latin1_broken");
    ost_ptest::seed_scale_world(broken);
    const string damaged = "<Legends dX=\"10\" Name=\"caf\xE9\"><Legend dX=\"20\"></Legends>"; // the Legend element is never closed
    broken.must("UPDATE BidLegends SET Position = X'" + hex_of(damaged) + "' WHERE UID = " + std::to_string(legend));
    const string before = broken.snapshot_all();
    PageScaleOutcome outcome;
    CHECK_FALSE(ost_store::save_page_scales(broken.path(), "10", {"100"}, 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "BadLegend");
    CHECK_EQ(broken.snapshot_all(), before);
}

// page_operations.py:112-118: the offsets are written "each only if the column exists". Three BidPages shapes with the rectangle column and only ONE of the two offset columns (or
// none): the rectangle is rescaled, the offset column that exists follows the scaled rectangle, and the missing column is no failure (an UPDATE that named it would fail and
// the whole change would be refused).
TEST(SetScale_StoreReview, EachOverlayOffsetColumnIsWrittenOnlyWhenItExists)
{
    const struct
    {
        const char *columns;
        const char *values;
        bool has_x, has_y;
    } shapes[] = {
        {"OverlayRect TEXT, OverlayOffsetX REAL", "'10,20,30,40', 7.0", true, false},
        {"OverlayRect TEXT, OverlayOffsetY REAL", "'10,20,30,40', 8.0", false, true},
        {"OverlayRect TEXT", "'10,20,30,40'", false, false},
    };
    for (const auto &shape : shapes)
    {
        SCOPED_CONTEXT(string("columns: ") + shape.columns);
        TempDb db("scale_review_offset_columns", false);
        db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
        db.must("INSERT INTO Bids (UID) VALUES (1)");
        db.must(string("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL, ScaleFactor2 REAL, ") + shape.columns + ")");
        db.must(string("INSERT INTO BidPages VALUES (20, 1, 0.125, 12.0, ") + shape.values + ")");
        PageScaleOutcome outcome;
        REQUIRE(ost_store::save_page_scales(db.path(), "1", {"20"}, 0.25, 12.0, &outcome));
        CHECK_STR_EQ(refusal_name(outcome.refusal), "None");
        CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 20"), "5.000000,10.000000,15.000000,20.000000");
        if (shape.has_x)
            CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 20"), "5.0");
        if (shape.has_y)
            CHECK_EQ(db.text("SELECT OverlayOffsetY FROM BidPages WHERE UID = 20"), "10.0");
        CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.25, 12.0)));
    }
}

// A refused change does not leak into the NEXT change: after a refusal the same connection-free call sequence behaves as on a fresh database (no state is cached between calls):
// a refusal, then a success, then the same refusal again.
TEST(SetScale_StoreReview, NoStateSurvivesBetweenCalls)
{
    TempDb db("scale_review_no_state");
    ost_ptest::seed_scale_world(db);
    for (int round = 0; round < 3; ++round)
    {
        SCOPED_CONTEXT("round " + std::to_string(round));
        const string before = db.snapshot_all();
        CHECK_EQ(refusal_of(db, {"200"}, 0.25, 12.0), "ForeignBid");
        CHECK_EQ(refusal_of(db, {"100", "100"}, 0.0, 12.0), "BadNewScale");
        CHECK_EQ(db.snapshot_all(), before);
        // 0.125 -> 0.25 -> 0.125 ...: each call is factor 0.5 / 2 from the then-current stored scale
        const double sf1 = round % 2 == 0 ? 0.25 : 0.125;
        CHECK_EQ(refusal_of(db, {"100"}, sf1, 12.0), "written");
        CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(sf1, 12.0)));
    }
}
