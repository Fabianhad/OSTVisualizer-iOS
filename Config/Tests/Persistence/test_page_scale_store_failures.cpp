// Set Scale, store writer: tests ADDED BY THE SECOND INDEPENDENT REVIEW (label scale-r) for the mutants that the whole-table triggers of the earlier tests cannot tell apart from the
// real thing. Windows source of truth: page_operations.py:28-217 (ONE connection, ONE transaction: any failed statement ends the call with a rollback, nothing is committed).
//
// Why these tests exist. The earlier failure tests block a whole table (TempDb::block(table, "UPDATE")). For BidPages that makes the FIRST BidPages write of the call fail, and on the
// pages of the world that is always the overlay rectangle write, so the later writes of BidPages are never the one that fails. A writer that IGNORED the result of the final
// "UPDATE BidPages SET ScaleFactor1 = ?, ScaleFactor2 = ?" would still be refused by those tests, although it would answer "saved" with the content rewritten and the scale unchanged
// (the page would then hold coordinates in the new units under the old scale: silent corruption), and the same holds for the overlay write when only the overlay columns fail.
// SQLite lets a trigger watch single columns (BEFORE UPDATE OF <column>), so each statement of the writer is made to fail ALONE here, with every other statement allowed.
// Every test checks the OUTCOME (StatementFailed), the WHOLE database against a snapshot (snapshot_all) and ends with a control run (the same call without the fault writes).
#include "PageScaleTestSupport.hpp"

#include <string>
#include <utility>
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

    bool save(const TempDb &db, const vector<string> &pages, double sf1, double sf2, PageScaleOutcome *outcome)
    {
        return ost_store::save_page_scales(db.path(), "10", pages, sf1, sf2, outcome);
    }

    // The statement that must fail ALONE: a trigger that aborts an UPDATE of BidPages which sets `column` (an UPDATE that does not set it is not affected), optionally only for one page.
    string fail_column(const TempDb &db, const string &name, const string &column, const string &only_page = "")
    {
        db.must("CREATE TRIGGER " + name + " BEFORE UPDATE OF " + column + " ON BidPages" + (only_page.empty() ? string() : " WHEN NEW.UID = " + only_page) +
                " BEGIN SELECT RAISE(ABORT, 'boom'); END");
        return name;
    }

    // A refused change: StatementFailed, a reason in the outcome, and not one byte of the database changed.
    void check_failed(const TempDb &db, const string &before, const vector<string> &pages, double sf1, double sf2)
    {
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, pages, sf1, sf2, &outcome));
        CHECK_STR_EQ(refusal_name(outcome.refusal), "StatementFailed");
        CHECK(!outcome.error.empty());
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// The last statement of every page, "UPDATE BidPages SET ScaleFactor1 = ?, ScaleFactor2 = ?", fails ALONE (a trigger on ScaleFactor1, then on ScaleFactor2): the call is refused and the
// WHOLE database is as before, whether the page has an overlay rectangle (page 100: its overlay write succeeds first and is rolled back), has none (page 101: the content tables are the only
// earlier writes) or the new scale has the same ratio (0.25 : 24: no content is read or written at all, this UPDATE is the ONLY write of the call). Without the trigger the same calls write.
TEST(SetScale_StoreFailure, TheFinalScaleWriteFailingAloneRollsEverythingBack)
{
    for (const char *column : {"ScaleFactor1", "ScaleFactor2"})
    {
        SCOPED_CONTEXT(string("only the write of ") + column + " fails");
        TempDb db("scale_fail_final");
        ost_ptest::seed_scale_world(db);
        const string before = db.snapshot_all();
        const string trigger = fail_column(db, "boom_scale", column);
        check_failed(db, before, {"100"}, 0.25, 12.0);        // overlay rectangle and content rewritten first
        check_failed(db, before, {"101"}, 0.25, 12.0);        // no rectangle: only the content tables before it
        check_failed(db, before, {"100", "101"}, 0.25, 12.0);
        check_failed(db, before, {"101"}, 0.25, 24.0);        // same ratio: this UPDATE is the only write
        check_failed(db, before, {"100"}, 0.125, 12.0);       // the pair the page already has: the same
        db.must("DROP TRIGGER " + trigger);
        PageScaleOutcome control;
        REQUIRE(save(db, {"101"}, 0.25, 12.0, &control)); // the control: the same call without the fault writes
        CHECK_STR_EQ(refusal_name(control.refusal), "None");
        CHECK_EQ(ost_ptest::scale_values(db, 101), (std::pair<double, double>(0.25, 12.0)));
        CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", ost_ptest::world_row_uid("BidTakeoffs", 1)), "5;10;15;20\n");
    }
}

// The final write of the SECOND page of a batch fails (page 100 is rewritten completely before it: its content, its overlay rectangle and its scale): everything of page 100 is rolled
// back too. Page 101 is the second page, so only its UPDATE of the scale columns fails; page 100 alone is written when the batch no longer names page 101.
TEST(SetScale_StoreFailure, TheFinalScaleWriteOfTheSecondPageFailingRollsBackTheFirstPage)
{
    TempDb db("scale_fail_final_second");
    ost_ptest::seed_scale_world(db);
    const string before = db.snapshot_all();
    const string trigger = fail_column(db, "boom_second", "ScaleFactor2", "101");
    check_failed(db, before, {"100", "101"}, 0.25, 12.0);
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.125, 12.0))); // the first page is back
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", ost_ptest::world_row_uid("BidTakeoffs", 0)), ost_ptest::world_position("BidTakeoffs"));
    REQUIRE(save(db, {"100"}, 0.25, 12.0, nullptr)); // page 100 alone does not touch the failing row
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.25, 12.0)));
    CHECK_EQ(ost_ptest::scale_values(db, 101), (std::pair<double, double>(0.125, 12.0)));
    db.must("DROP TRIGGER " + trigger);
    REQUIRE(save(db, {"101"}, 0.25, 12.0, nullptr));
    CHECK_EQ(ost_ptest::scale_values(db, 101), (std::pair<double, double>(0.25, 12.0)));
}

// The overlay write fails ALONE (a trigger on OverlayRect, on OverlayOffsetX, on OverlayOffsetY): the scale-column write would succeed, so a writer that ignored this failure would save
// the new scale with the rectangle (or one of its offsets) left in the old units. The call is refused and the whole database is as before; a page without an overlay rectangle never
// writes those columns, so the same trigger does not stop it (the fault really is confined to the overlay write); a change below the epsilon does not write them either.
TEST(SetScale_StoreFailure, TheOverlayWriteFailingAloneRollsEverythingBack)
{
    for (const char *column : {"OverlayRect", "OverlayOffsetX", "OverlayOffsetY"})
    {
        SCOPED_CONTEXT(string("only the write of ") + column + " fails");
        TempDb db("scale_fail_overlay");
        ost_ptest::seed_scale_world(db);
        const string before = db.snapshot_all();
        const string trigger = fail_column(db, "boom_overlay", column);
        check_failed(db, before, {"100"}, 0.25, 12.0);
        check_failed(db, before, {"101", "100"}, 0.25, 12.0); // the page before it in the batch is rolled back too
        // the fault is confined to the overlay write: a change that writes no overlay (same ratio) and a page with no rectangle go through
        REQUIRE(save(db, {"100"}, 0.25, 24.0, nullptr));
        CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.25, 24.0)));
        CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 100"), "10,20,30,40"); // untouched: the same ratio
        REQUIRE(save(db, {"101"}, 0.25, 12.0, nullptr));
        CHECK_EQ(ost_ptest::scale_values(db, 101), (std::pair<double, double>(0.25, 12.0)));
        db.must("DROP TRIGGER " + trigger);
        // the control: page 100 holds (0.25, 24) = 96 units per inch; (0.25, 12) is 48, a factor of 0.5, so without the trigger the rectangle and its offsets are rescaled
        PageScaleOutcome control;
        REQUIRE(save(db, {"100"}, 0.25, 12.0, &control));
        CHECK_STR_EQ(refusal_name(control.refusal), "None");
        CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 100"), "5.000000,10.000000,15.000000,20.000000");
        CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 100"), "5.0");
        CHECK_EQ(db.text("SELECT OverlayOffsetY FROM BidPages WHERE UID = 100"), "10.0");
    }
}

// A position UPDATE fails ALONE for ONE row of ONE table (a trigger on Position of the third table of the page, only for the row of page 100): the earlier tables of the page were
// written, the change is refused and ALL of them are back; the same table on page 101 is not affected.
TEST(SetScale_StoreFailure, APositionWriteFailingForOneRowRollsBackTheTablesBeforeIt)
{
    for (const string &table : {string("BidTakeoffs"), string("BidAnnotationRects"), string("BidLegends"), string("BidTypGroupViews")})
    {
        SCOPED_CONTEXT("table " + table);
        TempDb db("scale_fail_position_row");
        ost_ptest::seed_scale_world(db);
        const string before = db.snapshot_all();
        db.must("CREATE TRIGGER boom_row BEFORE UPDATE OF Position ON [" + table + "] WHEN NEW.UID = " + std::to_string(ost_ptest::world_row_uid(table, 0)) +
                " BEGIN SELECT RAISE(ABORT, 'boom'); END");
        check_failed(db, before, {"100"}, 0.25, 12.0);
        check_failed(db, before, {"101", "100"}, 0.25, 12.0);
        REQUIRE(save(db, {"101"}, 0.25, 12.0, nullptr)); // the other page does not touch the failing row
        CHECK_EQ(ost_ptest::scale_values(db, 101), (std::pair<double, double>(0.25, 12.0)));
        db.must("DROP TRIGGER boom_row");
        REQUIRE(save(db, {"100"}, 0.25, 12.0, nullptr)); // the control
        CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.25, 12.0)));
    }
}
