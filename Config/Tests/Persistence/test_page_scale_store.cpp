// Set Scale, the store writer (plan 5.4, tests-first): ost_store::save_page_scale / save_page_scales, one transaction, ported from the Windows page operations.
// Windows source of truth: page_operations.py:28-217 (save_page_scale and the rewrite), project_write_service.py:6306-6339 and :6371-6433 (the service rules).
// Ported Windows tests (each cites file:line): tests/infrastructure/mdb/components/test_page_operations.py (PageOperationsCoverSheetPathTests :192-319,
// PageOperationsGuardTests :499-670, PageScaleChangeContentTests :1781-2006, PageOperationsRealContractTests :1401-1514), tests/integration/persistence/
// test_page_scale_geometry.py, tests/application/services/test_project_write_service.py:781-912. The Windows tests that use mocks (a fake cursor, a patched epsilon) are
// ported as END-STATE tests on a real SQLite database; the data-safety cases are in test_page_scale_store_safety.cpp.
// Deliberate differences, each in the name or the comment of the test: ALL-OR-NOTHING for a batch (SS-11), the legend is rewritten in place, the TEXT storage class.
#include "PageScaleTestSupport.hpp"

#include "Domain/PositionParser.hpp"

#include <cmath>
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

    // The Windows fixtures use the bid with UID 1 and the pages 20 and 21.
    bool save(const TempDb &db, const string &page, double sf1, double sf2, PageScaleOutcome *outcome = nullptr)
    {
        return ost_store::save_page_scale(db.path(), "1", page, sf1, sf2, outcome);
    }

    // The positions of a row, parsed (an unparseable or NULL cell is []).
    vector<double> positions(const TempDb &db, const string &table, long long uid)
    {
        const string text = ost_ptest::position_of(db, table, uid);
        return text == "NULL" || text == "<none>" ? vector<double>() : ost_domain::parse_position(text);
    }
}

// ---- tests/infrastructure/mdb/components/test_page_operations.py: PageOperationsCoverSheetPathTests ------------------------------------------------------

// :193 test_page_scale_save_uses_shared_content_rescale: (0.125, 12) -> (0.25, 12) is the factor 0.5 (the Windows mock asserts rescale_calls [(11, 0.5)] and the UPDATE
// args (0.25, 12.0, 11)); the end state: the page holds (0.25, 12.0) and its takeoff is halved, the three-decimal text.
TEST(SetScale_Store, PageScaleSaveUsesTheSharedContentRescale)
{
    TempDb db("scale_store_shared", false);
    ost_ptest::make_windows_scale_db(db);
    PageScaleOutcome outcome;
    ScaleLogCapture log;
    REQUIRE(save(db, "20", 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "None");
    CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.25, 12.0)));
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "5;10;15;20\n");
    CHECK_EQ(ost_ptest::scale_values(db, 21), (std::pair<double, double>(0.125, 12.0))); // the other page is not touched
}

// :207 test_page_scale_save_with_unchanged_calibration_skips_content_rescale: (0.125, 12) -> (0.125, 12): the scale is still written, no content is rewritten (the text is
// byte for byte what it was), and there is no warning (no table was read).
TEST(SetScale_Store, AnUnchangedCalibrationSkipsTheContentRescale)
{
    TempDb db("scale_store_unchanged", false);
    ost_ptest::make_windows_scale_db(db);
    const string before = db.snapshot_all();
    ScaleLogCapture log;
    REQUIRE(save(db, "20", 0.125, 12.0));
    CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.125, 12.0)));
    CHECK_EQ(db.snapshot_all(), before); // the same values were written: nothing differs
    CHECK(log.warnings().empty());
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "10;20;30;40"); // not even re-encoded
}

// :220 test_page_scale_save_rejects_invalid_overlay_calibration: a stored (0, 12) with the new (0.125, 12), and a stored (0.125, 12) with the new (0, 12): false, no UPDATE,
// nothing changed; the reason names the page ("Page 20 requires finite positive scale factors", page_operations.py:75-77).
TEST(SetScale_Store, AnInvalidStoredOrNewCalibrationIsRefused)
{
    {
        TempDb db("scale_store_bad_stored", false);
        ost_ptest::make_windows_scale_db(db);
        db.must("UPDATE BidPages SET ScaleFactor1 = 0.0 WHERE UID = 20");
        const string before = db.snapshot_all();
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, "20", 0.125, 12.0, &outcome));
        CHECK_STR_EQ(refusal_name(outcome.refusal), "BadStoredScale");
        CHECK_STR_EQ(outcome.error, "Page 20 requires finite positive scale factors");
        CHECK_EQ(db.snapshot_all(), before);
    }
    {
        TempDb db("scale_store_bad_new", false);
        ost_ptest::make_windows_scale_db(db);
        const string before = db.snapshot_all();
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, "20", 0.0, 12.0, &outcome));
        CHECK_STR_EQ(refusal_name(outcome.refusal), "BadNewScale");
        CHECK_STR_EQ(outcome.error, "Page 20 requires finite positive scale factors");
        CHECK_EQ(db.snapshot_all(), before);
        CHECK(save(db, "20", 0.25, 12.0)); // the control: the same page with a valid pair
    }
}

// :252 test_page_scale_change_rescales_overlay_rect_and_offsets: stored (0.1875, 12) and the rectangle "-1.103146,0.000000,2686.161423,1919.474692", new (0.125, 12): factor
// 1.5, the rectangle "-1.654719,0.000000,4029.242135,2879.212038", the offsets -1.654719 and 0.0, the positions of the page rewritten by 1.5.
TEST(SetScale_Store, ScaleChangeRescalesTheOverlayRectAndTheOffsets)
{
    TempDb db("scale_store_overlay", false);
    ost_ptest::make_windows_scale_db(db);
    db.must("UPDATE BidPages SET ScaleFactor1 = 0.1875, OverlayRect = '-1.103146,0.000000,2686.161423,1919.474692', OverlayOffsetX = 5.0, OverlayOffsetY = 6.0 WHERE UID = 20");
    ScaleLogCapture log;
    REQUIRE(save(db, "20", 0.125, 12.0));
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 20"), "-1.654719,0.000000,4029.242135,2879.212038");
    CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 20"), "-1.654719");
    CHECK_EQ(db.text("SELECT OverlayOffsetY FROM BidPages WHERE UID = 20"), "0.0");
    CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.125, 12.0)));
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "15;30;45;60\n"); // 10..40 x 1.5
}

// :279 test_page_scale_change_preserves_native_empty_overlay_marker: "*" is kept, the positions are rescaled.
TEST(SetScale_Store, TheNativeEmptyOverlayMarkerIsPreserved)
{
    TempDb db("scale_store_marker", false);
    ost_ptest::make_windows_scale_db(db);
    db.must("UPDATE BidPages SET ScaleFactor1 = 0.1875, OverlayRect = '*', OverlayOffsetX = 1.5, OverlayOffsetY = 2.5 WHERE UID = 20");
    ScaleLogCapture log;
    REQUIRE(save(db, "20", 0.125, 12.0));
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 20"), "*");
    CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 20"), "1.5");
    CHECK_EQ(db.text("SELECT OverlayOffsetY FROM BidPages WHERE UID = 20"), "2.5");
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "15;30;45;60\n");
}

// :285 test_page_scale_change_leaves_absent_or_zero_size_overlay_rect_untouched: NULL, "", "0.0,0.0,0.0,0.0", "5,5,0,100", "5,5,100,0": the rectangle and the offsets stay
// as they were (the Windows UPDATE list is empty), the positions are rescaled.
TEST(SetScale_Store, AbsentOrZeroSizeOverlayRectanglesAreLeftUntouched)
{
    for (const char *rect : {"NULL", "''", "'0.0,0.0,0.0,0.0'", "'5,5,0,100'", "'5,5,100,0'"})
    {
        SCOPED_CONTEXT(string("OverlayRect = ") + rect);
        TempDb db("scale_store_zero_rect", false);
        ost_ptest::make_windows_scale_db(db);
        db.must(string("UPDATE BidPages SET ScaleFactor1 = 0.1875, OverlayRect = ") + rect + ", OverlayOffsetX = 7.0, OverlayOffsetY = 8.0 WHERE UID = 20");
        const string rect_before = db.text("SELECT ifnull(OverlayRect, 'NULL') FROM BidPages WHERE UID = 20");
        ScaleLogCapture log;
        REQUIRE(save(db, "20", 0.125, 12.0));
        CHECK_EQ(db.text("SELECT ifnull(OverlayRect, 'NULL') FROM BidPages WHERE UID = 20"), rect_before);
        CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 20"), "7.0");
        CHECK_EQ(db.text("SELECT OverlayOffsetY FROM BidPages WHERE UID = 20"), "8.0");
        CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "15;30;45;60\n");
    }
}

// :293 test_page_scale_change_rejects_malformed_overlay_rect_before_any_write: "0,0,2688", "0,0,abc,1920", "0,0,-1,1920" (and the non-finite "nan,0,1,1"): false BEFORE any write, the
// whole database stays byte-identical (the Windows test asserts no position rescale, no overlay update and no UPDATE of the scale).
TEST(SetScale_Store, AMalformedOverlayRectIsRefusedBeforeAnyWrite)
{
    for (const char *rect : {"0,0,2688", "0,0,abc,1920", "0,0,-1,1920", "nan,0,1,1"})
    {
        SCOPED_CONTEXT(string("OverlayRect = ") + rect);
        TempDb db("scale_store_bad_rect", false);
        ost_ptest::make_windows_scale_db(db);
        db.must(string("UPDATE BidPages SET ScaleFactor1 = 0.1875, OverlayRect = '") + rect + "' WHERE UID = 20");
        const string before = db.snapshot_all();
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, "20", 0.125, 12.0, &outcome));
        CHECK_STR_EQ(refusal_name(outcome.refusal), "BadOverlayRect");
        CHECK(!outcome.error.empty());
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// :309 test_page_scale_position_failure_aborts_scale_update: a failing position write (an UPDATE that aborts) leaves the scale unchanged: false, the database is exactly as it was.
TEST(SetScale_Store, AFailingPositionWriteAbortsTheScaleUpdate)
{
    TempDb db("scale_store_position_failure", false);
    ost_ptest::make_windows_scale_db(db);
    const string before = db.snapshot_all();
    const string trigger = db.block("BidTakeoffs", "UPDATE");
    ScaleLogCapture log;
    PageScaleOutcome outcome;
    CHECK_FALSE(save(db, "20", 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "StatementFailed");
    CHECK_EQ(db.snapshot_all(), before);
    db.unblock(trigger);
    CHECK(save(db, "20", 0.25, 12.0)); // the control run: the same call without the trigger writes
    CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.25, 12.0)));
}

// ---- PageOperationsGuardTests (:499-670) -------------------------------------------------------------------------------------------------------------

// :522 test_a_page_scale_save_needs_the_scale_columns_and_exactly_one_page_row (the missing-column half; iOS UIDs are INTEGER PRIMARY KEY, so the duplicate half has no
// fixture): a BidPages without ScaleFactor2 refuses ("Missing BidPages.ScaleFactor2"), nothing is written.
TEST(SetScale_Store, ABidPagesWithoutTheScaleColumnsIsRefused)
{
    TempDb db("scale_store_missing_column", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL)");
    db.must("INSERT INTO BidPages VALUES (20, 1, 0.125)");
    const string before = db.snapshot_all();
    PageScaleOutcome outcome;
    CHECK_FALSE(save(db, "20", 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "MissingColumns");
    CHECK(outcome.error.find("ScaleFactor2") != string::npos);
    CHECK_EQ(db.snapshot_all(), before);
}

// :579 test_a_page_scale_save_rescales_only_the_content_of_that_page: pages 20 and 21 both (0.125, 12); takeoffs "1;2;3;4" (page 20), "" (page 20), "1;2;3;4" (page 21); a BidLegends
// without Position is skipped; save page 20 -> (0.25, 12): [0.5, 1, 1.5, 2], [], [1, 2, 3, 4]; BidPages (20, 0.25, 12.0) and (21, 0.125, 12.0).
TEST(SetScale_Store, ASaveRescalesOnlyTheContentOfThatPage)
{
    TempDb db("scale_store_only_page", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL, ScaleFactor2 REAL)");
    db.must("INSERT INTO BidPages VALUES (20, 1, 0.125, 12.0), (21, 1, 0.125, 12.0)");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, Position BLOB)");
    db.must("INSERT INTO BidTakeoffs VALUES (1, 1, 20, '1;2;3;4'), (2, 1, 20, ''), (3, 1, 21, '1;2;3;4')");
    db.must("CREATE TABLE BidLegends (UID INTEGER, BidPageUID INTEGER)");
    db.must("INSERT INTO BidLegends VALUES (9, 20)");
    REQUIRE(save(db, "20", 0.25, 12.0));
    CHECK_EQ(positions(db, "BidTakeoffs", 1), vector<double>({0.5, 1.0, 1.5, 2.0}));
    CHECK_EQ(positions(db, "BidTakeoffs", 2), vector<double>());
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 2), ""); // an empty text stays empty
    CHECK_EQ(positions(db, "BidTakeoffs", 3), vector<double>({1.0, 2.0, 3.0, 4.0}));
    CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.25, 12.0)));
    CHECK_EQ(ost_ptest::scale_values(db, 21), (std::pair<double, double>(0.125, 12.0)));
}

// :606 test_a_scale_change_that_keeps_the_ratio_leaves_page_content_untouched: (0.125, 12) -> (0.25, 24) is the same ratio: the text "1;2;3;4" is untouched byte for byte.
TEST(SetScale_Store, AScaleChangeThatKeepsTheRatioLeavesTheContentUntouched)
{
    TempDb db("scale_store_same_ratio", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL, ScaleFactor2 REAL)");
    db.must("INSERT INTO BidPages VALUES (20, 1, 0.125, 12.0)");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, Position BLOB)");
    db.must("INSERT INTO BidTakeoffs VALUES (1, 1, 20, '1;2;3;4')");
    REQUIRE(save(db, "20", 0.25, 24.0));
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "1;2;3;4");
    CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.25, 24.0)));
}

// :621 test_overlay_rectangles_of_any_positive_size_follow_a_scale_change: rectangle 0.5 x 0.5 at (0.5, 0.5), offsets 0.5 / 0.5, (0.125, 12) -> (0.25, 12): every value and both
// offsets become 0.25.
TEST(SetScale_Store, OverlayRectanglesOfAnyPositiveSizeFollowAScaleChange)
{
    TempDb db("scale_store_tiny_rect", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL, ScaleFactor2 REAL, OverlayRect TEXT, OverlayOffsetX REAL, OverlayOffsetY REAL)");
    db.must("INSERT INTO BidPages VALUES (20, 1, 0.125, 12.0, '0.500000,0.500000,0.500000,0.500000', 0.5, 0.5)");
    REQUIRE(save(db, "20", 0.25, 12.0));
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 20"), "0.250000,0.250000,0.250000,0.250000");
    CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 20"), "0.25");
    CHECK_EQ(db.text("SELECT OverlayOffsetY FROM BidPages WHERE UID = 20"), "0.25");
}

// ---- PageScaleChangeContentTests (:1781-2006) ---------------------------------------------------------------------------------------------------------

// :1854 test_scale_change_rewrites_only_the_requested_pages_content_tables: page 20 (0.125, 12) -> (0.0625, 12), factor 2. In each of the 12 tables: row 1 -> [20, 40, 60, 80]
// (BidTexts [10, 12, 14, 16, 0.5]), row 2 (page 21) unchanged, row 3 (NULL) untouched, row 4 ('not-numeric') untouched with ONE warning per table, BidTexts row 5 ->
// [2, 4, 6, 8, 0.5, 9, 10]; BidHotLinks (no BidPageUID) and BidNamedViews (no Position) are skipped; the legends 1 and 4 get dX 61.608 / dY 2045.358, legend 2 (page 21) and 3
// (NULL) stay; BidPages (20, 0.0625, 12) and (21, 0.125, 12). ADAPTED: the legend text is rewritten in place, the three-decimal / exact forms are TEXT.
TEST(SetScale_Store, ScaleChangeRewritesOnlyTheRequestedPagesContentTables)
{
    TempDb db("scale_store_content_tables", false);
    ost_ptest::make_windows_scale_db(db);
    ScaleLogCapture log;
    REQUIRE(save(db, "20", 0.0625, 12.0));
    vector<string> expected_warnings;
    for (const string &table : ost_ptest::windows_scaled_tables())
        expected_warnings.push_back("Skipping page-scale rescale for " + table + " UID 4 because Position is not numeric");
    vector<string> warnings = log.warnings();
    std::sort(warnings.begin(), warnings.end());
    std::sort(expected_warnings.begin(), expected_warnings.end());
    CHECK_EQ(warnings, expected_warnings);
    for (const string &table : ost_ptest::windows_scaled_tables())
    {
        SCOPED_CONTEXT("table " + table);
        const bool texts = table == "BidTexts";
        CHECK_EQ(positions(db, table, 1), texts ? vector<double>({10.0, 12.0, 14.0, 16.0, 0.5}) : vector<double>({20.0, 40.0, 60.0, 80.0}));
        CHECK_EQ(positions(db, table, 2), texts ? vector<double>({5.0, 6.0, 7.0, 8.0, 0.5}) : vector<double>({10.0, 20.0, 30.0, 40.0}));
        CHECK_EQ(ost_ptest::position_of(db, table, 2), texts ? "5;6;7;8;0.5" : "10;20;30;40"); // the other page: not even re-encoded
        CHECK_EQ(ost_ptest::position_of(db, table, 3), "NULL");
        CHECK_EQ(ost_ptest::type_of(db, table, "Position", 3), "null");
        CHECK_EQ(ost_ptest::position_of(db, table, 4), "not-numeric");
        CHECK_EQ(ost_ptest::type_of(db, table, "Position", 1), "text"); // the memo / blob columns are TEXT on iOS (plan 4.5)
        if (texts)
            CHECK_EQ(positions(db, table, 5), vector<double>({2.0, 4.0, 6.0, 8.0, 0.5, 9.0, 10.0}));
    }
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "20;40;60;80\n");          // BidTakeoffs: the three-decimal form
    CHECK_EQ(ost_ptest::position_of(db, "BidDimensions", 1), "20.0;40.0;60.0;80.0\n"); // an annotation table: the exact form
    CHECK_EQ(ost_ptest::position_of(db, "BidTexts", 1), "10.0;12.0;14.0;16.0;0.5\n");
    CHECK_EQ(db.text("SELECT Position FROM BidHotLinks WHERE UID = 1"), "1;2;3;4");
    const string scaled_legend = "<Legends dX=\"61.608\" dY=\"2045.358\" Visible=\"255\"><Legend ConditionUID=\"11334\" dX=\"61.608\" dY=\"61.608\" Visible=\"1\"/></Legends>";
    CHECK_EQ(ost_ptest::position_of(db, "BidLegends", 1), scaled_legend);
    CHECK_EQ(ost_ptest::position_of(db, "BidLegends", 4), scaled_legend);
    CHECK_EQ(ost_ptest::position_of(db, "BidLegends", 2), ost_ptest::windows_legend_xml());
    CHECK_EQ(ost_ptest::position_of(db, "BidLegends", 3), "NULL");
    CHECK_EQ(ost_ptest::type_of(db, "BidLegends", "Position", 1), "text");
    CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.0625, 12.0)));
    CHECK_EQ(ost_ptest::scale_values(db, 21), (std::pair<double, double>(0.125, 12.0)));
}

// :1914 test_scale_change_scales_the_overlay_rectangle_and_translation_together: "10,20,30,40" with the offsets 10 / 20 -> "20.000000,40.000000,60.000000,80.000000", 20.0, 40.0;
// page 21 ("", 0, 0) is unchanged.
TEST(SetScale_Store, ScaleChangeScalesTheOverlayRectangleAndTranslationTogether)
{
    TempDb db("scale_store_rect_translation", false);
    ost_ptest::make_windows_scale_db(db);
    ScaleLogCapture log;
    REQUIRE(save(db, "20", 0.0625, 12.0));
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 20"), "20.000000,40.000000,60.000000,80.000000");
    CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 20"), "20.0");
    CHECK_EQ(db.text("SELECT OverlayOffsetY FROM BidPages WHERE UID = 20"), "40.0");
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 21"), "");
    CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 21"), "0.0");
    CHECK_EQ(db.text("SELECT OverlayOffsetY FROM BidPages WHERE UID = 21"), "0.0");
}

// :1929 test_scale_change_without_overlay_columns_still_rewrites_positions: a BidPages without the three overlay columns: (0.125, 12) -> (0.25, 12) still rewrites the positions
// ([5, 10, 15, 20]), nothing is skipped with an error.
TEST(SetScale_Store, ScaleChangeWithoutOverlayColumnsStillRewritesPositions)
{
    TempDb db("scale_store_no_overlay_columns", false);
    ost_ptest::WindowsScaleOptions options;
    options.overlay_columns = false;
    ost_ptest::make_windows_scale_db(db, options);
    ScaleLogCapture log;
    PageScaleOutcome outcome;
    REQUIRE(save(db, "20", 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "None");
    CHECK_EQ(positions(db, "BidTakeoffs", 1), vector<double>({5.0, 10.0, 15.0, 20.0}));
}

// :1938 test_scale_change_below_the_epsilon_does_not_rewrite_content: (0.125, 12) -> (0.125, 12 * (1 + 1e-12)): no position, no overlay rectangle and no warning; ScaleFactor2 is stored
// exactly as given (the bit pattern of 12 * (1 + 1e-12)).
TEST(SetScale_Store, AChangeBelowTheEpsilonDoesNotRewriteContentButStoresTheNewValues)
{
    TempDb db("scale_store_below_epsilon", false);
    ost_ptest::make_windows_scale_db(db);
    ScaleLogCapture log;
    const double new_sf2 = 12.0 * (1 + 1e-12);
    REQUIRE(save(db, "20", 0.125, new_sf2));
    CHECK_EQ(positions(db, "BidTakeoffs", 1), vector<double>({10.0, 20.0, 30.0, 40.0}));
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "10;20;30;40"); // not rewritten, not even re-encoded
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 20"), "10,20,30,40");
    CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.125, new_sf2))); // exact
    CHECK(new_sf2 != 12.0);
    CHECK(log.warnings().empty()); // nothing was read: the not-numeric rows were never looked at
}

// :1957 test_rescale_threshold_is_exclusive_at_the_epsilon_itself: the Windows test patches the constant; the C++ store cannot, so the exclusive comparison is pinned on the pure
// function (SetScale_Epsilon.TheThresholdIsExclusiveAtTheEpsilonItself, Geometry). What the store adds: the factor exactly 1.5 ((0.125, 12) -> (0.125, 18): 144 / 96) is far above the
// default epsilon, so the positions ARE rewritten, [15, 30, 45, 60] (the Windows 0.4999 half).
TEST(SetScale_Store, AFactorOfExactlyOnePointFiveRewritesWithTheDefaultEpsilon)
{
    TempDb db("scale_store_factor_one_half", false);
    ost_ptest::make_windows_scale_db(db);
    ScaleLogCapture log;
    REQUIRE(save(db, "20", 0.125, 18.0));
    CHECK_EQ(positions(db, "BidTakeoffs", 1), vector<double>({15.0, 30.0, 45.0, 60.0}));
}

// :1979 test_failed_content_rewrite_rolls_back_every_earlier_write: legend 1 is the blob "<Legends" (broken): the save is refused and EVERY table, ScaleFactor1, OverlayRect and
// OverlayOffsetX are as before (the takeoffs, the annotations and the overlay were rewritten before the legend failed, then rolled back).
TEST(SetScale_Store, AFailedContentRewriteRollsBackEveryEarlierWrite)
{
    TempDb db("scale_store_rollback_legend", false);
    ost_ptest::WindowsScaleOptions options;
    options.broken_legend = true;
    ost_ptest::make_windows_scale_db(db, options);
    const string before = db.snapshot_all();
    ScaleLogCapture log;
    PageScaleOutcome outcome;
    CHECK_FALSE(save(db, "20", 0.0625, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "BadLegend");
    CHECK(!outcome.error.empty());
    for (const string &table : ost_ptest::windows_scaled_tables())
    {
        SCOPED_CONTEXT("table " + table);
        CHECK_EQ(positions(db, table, 1), table == "BidTexts" ? vector<double>({5.0, 6.0, 7.0, 8.0, 0.5}) : vector<double>({10.0, 20.0, 30.0, 40.0}));
    }
    CHECK_EQ(db.text("SELECT OverlayRect FROM BidPages WHERE UID = 20"), "10,20,30,40");
    CHECK_EQ(db.text("SELECT OverlayOffsetX FROM BidPages WHERE UID = 20"), "10.0");
    CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.125, 12.0)));
    CHECK_EQ(db.snapshot_all(), before); // the WHOLE database
}

// ---- PageOperationsRealContractTests (:1232-1530): the scale half ------------------------------------------------------------------------------------

// :1401 test_every_page_save_logs_its_own_failure_and_changes_nothing (save_page_scale on page 99) and :1476 test_fatal_mutation_errors_propagate_instead_of_returning_false (the
// scale half: Windows raises MissingBidOwnedUidError "BidPages has no row for UID 99"; iOS answers false with the reason in the outcome): a page that does not exist is refused
// and the database is exactly as before (BidPages and BidPageSettings included).
TEST(SetScale_Store, ASaveOfAPageThatDoesNotExistChangesNothing)
{
    TempDb db("scale_store_page_99", false);
    ost_ptest::make_windows_scale_db(db);
    db.must("CREATE TABLE BidPageSettings (UID INTEGER, BidPageUID INTEGER, BidAreaUID INTEGER, BidAreaSelected INTEGER)");
    const string before = db.snapshot_all();
    PageScaleOutcome outcome;
    CHECK_FALSE(save(db, "99", 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "NoSuchPage");
    CHECK(outcome.error.find("99") != string::npos);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPageSettings"), 0);
}

// :1500 test_page_saves_reject_malformed_page_uids_before_touching_rows ("abc", "0", "-5", ""): refused before any row is touched. A single "" is no page at all (the batch rule
// of project_write_service.py:6885-6892 drops it: EmptyBatch); the other three are malformed uids.
TEST(SetScale_Store, MalformedPageUidsAreRefusedBeforeTouchingRows)
{
    TempDb db("scale_store_malformed_uid", false);
    ost_ptest::make_windows_scale_db(db);
    const string before = db.snapshot_all();
    for (const char *uid : {"abc", "0", "-5"})
    {
        SCOPED_CONTEXT(string("page uid '") + uid + "'");
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, uid, 0.25, 12.0, &outcome));
        CHECK_STR_EQ(refusal_name(outcome.refusal), "MalformedUid");
    }
    PageScaleOutcome empty;
    CHECK_FALSE(save(db, "", 0.25, 12.0, &empty));
    CHECK_STR_EQ(refusal_name(empty.refusal), "EmptyBatch");
    CHECK_EQ(db.snapshot_all(), before);
    CHECK(save(db, "20", 0.25, 12.0)); // the control
}

// ---- tests/application/services/test_project_write_service.py (the service rules, group H) ---------------------------------------------------------

// :781 test_scale_save_projects_only_affected_page_without_database_reload (the store half): pages 42 (1 : 48) and 43 (1 : 24); save 42 -> (1 : 96) writes page 42 and leaves page 43
// (and its content) alone.
TEST(SetScale_Store, ASavedScaleTouchesOnlyTheAffectedPage)
{
    TempDb db("scale_store_one_page", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (7)");
    db.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL, ScaleFactor2 REAL)");
    db.must("INSERT INTO BidPages VALUES (42, 7, 1.0, 48.0), (43, 7, 1.0, 24.0)");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, Position BLOB)");
    db.must("INSERT INTO BidTakeoffs VALUES (1, 7, 42, '48;96'), (2, 7, 43, '24;48')");
    REQUIRE(ost_store::save_page_scale(db.path(), "7", "42", 1.0, 96.0));
    CHECK_EQ(ost_ptest::scale_values(db, 42), (std::pair<double, double>(1.0, 96.0)));
    CHECK_EQ(ost_ptest::scale_values(db, 43), (std::pair<double, double>(1.0, 24.0)));
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "96;192\n"); // 96 / 48 = 2
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 2), "24;48");
}

// :836 test_failed_scale_write_preserves_model_and_does_not_reload_or_publish (the store half): a failed write leaves the database exactly as it was (the memory half is in
// test_page_scale_memory_twin.cpp).
TEST(SetScale_Store, AFailedWriteLeavesTheDatabaseAsItWas)
{
    TempDb db("scale_store_failed_write", false);
    ost_ptest::make_windows_scale_db(db);
    const string before = db.snapshot_all();
    const string trigger = db.block("BidPages", "UPDATE");
    PageScaleOutcome outcome;
    ScaleLogCapture log;
    CHECK_FALSE(save(db, "20", 0.25, 12.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "StatementFailed");
    CHECK_EQ(db.snapshot_all(), before);
    db.unblock(trigger);
    CHECK(save(db, "20", 0.25, 12.0));
}

// :873 test_bulk_scale_projects_only_saved_pages_and_reports_partial_failure: uids ["42", "", "42", "43"]: the empty and the repeated uid are dropped and each page is written ONCE, in
// order (the positions are scaled exactly once, not twice). ADAPTED (SS-11): the Windows test uses a mocked writer that fails page 43 and pins a PARTIAL result (42 saved);
// iOS is ALL-OR-NOTHING: when page 43 fails (an invalid stored scale) page 42 is NOT changed either.
TEST(SetScale_Store, ABatchWritesEachPageOnceAndIsAllOrNothing_Adapted)
{
    TempDb db("scale_store_batch", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (7)");
    db.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL, ScaleFactor2 REAL)");
    db.must("INSERT INTO BidPages VALUES (42, 7, 1.0, 48.0), (43, 7, 1.0, 24.0)");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, Position BLOB)");
    db.must("INSERT INTO BidTakeoffs VALUES (1, 7, 42, '48;96'), (2, 7, 43, '24;48')");
    const string before = db.snapshot_all();
    // page 43 has an unusable stored scale: the whole batch is refused and page 42 is untouched
    db.must("UPDATE BidPages SET ScaleFactor2 = 0.0 WHERE UID = 43");
    const string bad_before = db.snapshot_all();
    PageScaleOutcome outcome;
    CHECK_FALSE(ost_store::save_page_scales(db.path(), "7", {"42", "", "42", "43"}, 1.0, 96.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "BadStoredScale");
    CHECK_EQ(db.snapshot_all(), bad_before); // NEITHER page changed
    db.must("UPDATE BidPages SET ScaleFactor2 = 24.0 WHERE UID = 43");
    CHECK_EQ(db.snapshot_all(), before);
    // the control: both pages are good, each is written once
    CHECK(ost_store::save_page_scales(db.path(), "7", {"42", "", "42", "43"}, 1.0, 96.0));
    CHECK_EQ(ost_ptest::scale_values(db, 42), (std::pair<double, double>(1.0, 96.0)));
    CHECK_EQ(ost_ptest::scale_values(db, 43), (std::pair<double, double>(1.0, 96.0)));
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "96;192\n"); // 96 / 48 = 2, once
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 2), "96;192\n"); // 96 / 24 = 4, once
}

// :906 test_empty_bulk_scale_is_a_noop_without_persistence_or_events: ["", ""] is false and the database is not even opened (a path that does not exist is not created); [] too.
TEST(SetScale_Store, AnEmptyBatchIsRefusedWithoutOpeningTheDatabase)
{
    const string path = ost_test::temp_path("scale_store_never_opened_" + ost_ptest::run_token() + ".db");
    std::remove(path.c_str());
    PageScaleOutcome outcome;
    CHECK_FALSE(ost_store::save_page_scales(path, "7", {"", ""}, 1.0, 96.0, &outcome));
    CHECK_STR_EQ(refusal_name(outcome.refusal), "EmptyBatch");
    CHECK_FALSE(std::ifstream(path).good()); // sqlite3_open would have created the file
    PageScaleOutcome none;
    CHECK_FALSE(ost_store::save_page_scales(path, "7", {}, 1.0, 96.0, &none));
    CHECK_STR_EQ(refusal_name(none.refusal), "EmptyBatch");
    CHECK_FALSE(std::ifstream(path).good());
    std::remove(path.c_str());
}

// ---- tests/integration/persistence/test_page_scale_geometry.py (the store halves) -----------------------------------------------------------------

// :21 test_legend_transform_keeps_extensions_and_null_position (the NULL half and the positive control): a legend row with a NULL Position is untouched and not warned about; a
// populated legend is rewritten. ADAPTED (in-place text): <Legends dX="2" dY="4"><Legend dX="6" dY="8"/></Legends>.
TEST(SetScale_Store, ANullLegendPositionIsUntouchedAndAPopulatedOneIsRewritten_Adapted)
{
    TempDb db("scale_store_legend_null", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL, ScaleFactor2 REAL)");
    db.must("INSERT INTO BidPages VALUES (3, 1, 1.0, 1.0)");
    db.must("CREATE TABLE BidLegends (UID INTEGER, BidPageUID INTEGER, Position BLOB)");
    db.must("INSERT INTO BidLegends VALUES (7, 3, NULL)");
    db.must("INSERT INTO BidLegends VALUES (8, 3, '<Legends dX=\"1\" dY=\"2\"><Legend dX=\"3\" dY=\"4\"/></Legends>')");
    ScaleLogCapture log;
    REQUIRE(save(db, "3", 1.0, 2.0)); // ratio 1 -> 2: factor 2
    CHECK_EQ(ost_ptest::position_of(db, "BidLegends", 7), "NULL");
    CHECK_EQ(ost_ptest::position_of(db, "BidLegends", 8), "<Legends dX=\"2\" dY=\"4\"><Legend dX=\"6\" dY=\"8\"/></Legends>");
    CHECK(log.warnings().empty());
}

// :63 test_repeated_round_trips_obey_storage_precision (the store form): 20 pairs of (x0.3, x1/0.3) through the real writer. The factors are the ratios of (1, 100) -> (1, 30) and
// back (30 / 100 and 100 / 30). A takeoff, a legend dX and an overlay rectangle stay within 0.0022, 0.0022 and 0.0000022 of 1234.567 (the Windows tolerances: the takeoffs are
// re-quantised to three decimals on every step, register SS-6).
TEST(SetScale_Store, RepeatedRoundTripsObeyTheStoragePrecision)
{
    TempDb db("scale_store_round_trips", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL, ScaleFactor2 REAL, OverlayRect TEXT, OverlayOffsetX REAL, OverlayOffsetY REAL)");
    db.must("INSERT INTO BidPages VALUES (3, 1, 1.0, 100.0, '1234.567,2345.678,100.123456,200.123456', 1234.567, 2345.678)");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, Position BLOB)");
    db.must("INSERT INTO BidTakeoffs VALUES (7, 1, 3, '1234.567;2345.678\n')");
    db.must("CREATE TABLE BidLegends (UID INTEGER, BidPageUID INTEGER, Position BLOB)");
    db.must("INSERT INTO BidLegends VALUES (7, 3, '<Legends dX=\"1234.567\" dY=\"2345.678\"/>')");
    // one step first: 1234.567 x 0.3 = 370.3701 and 2345.678 x 0.3 = 703.7034, stored to three decimals
    {
        TempDb single("scale_store_single_step", false);
        single.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
        single.must("INSERT INTO Bids (UID) VALUES (1)");
        single.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL, ScaleFactor2 REAL)");
        single.must("INSERT INTO BidPages VALUES (3, 1, 1.0, 100.0)");
        single.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, Position BLOB)");
        single.must("INSERT INTO BidTakeoffs VALUES (7, 1, 3, '1234.567;2345.678\n')");
        REQUIRE(save(single, "3", 1.0, 30.0));
        CHECK_EQ(ost_ptest::position_of(single, "BidTakeoffs", 7), "370.37;703.703\n");
    }
    for (int i = 0; i < 20; ++i)
    {
        REQUIRE(save(db, "3", 1.0, 30.0));
        REQUIRE(save(db, "3", 1.0, 100.0));
    }
    const vector<double> final_takeoff = positions(db, "BidTakeoffs", 7);
    REQUIRE_EQ(final_takeoff.size(), static_cast<size_t>(2));
    CHECK_NEAR(final_takeoff[0], 1234.567, 0.0022);
    const string legend = ost_ptest::position_of(db, "BidLegends", 7);
    const size_t start = legend.find("dX=\"") + 4;
    double legend_x = 0.0;
    REQUIRE(ost_domain::parse_python_float(legend.substr(start, legend.find('"', start) - start), legend_x));
    CHECK_NEAR(legend_x, 1234.567, 0.0022);
    const string rect = db.text("SELECT OverlayRect FROM BidPages WHERE UID = 3");
    double rect_x = 0.0;
    REQUIRE(ost_domain::parse_python_float(rect.substr(0, rect.find(',')), rect_x));
    CHECK_NEAR(rect_x, 1234.567, 0.0000022);
    CHECK_EQ(ost_ptest::scale_values(db, 3), (std::pair<double, double>(1.0, 100.0)));
}
