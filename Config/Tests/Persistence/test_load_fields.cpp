// Field mapping of load_project against the Windows readers (database-persistence parity):
//   DBP-04 (+SNP-01, LP-19, LOAD-01)          Bids.TakeoffIncrements NULL loads as 1.0 (hierarchy_reader.py:126); a stored 0 stays 0
//                                             (DBP-05: Windows disagrees with itself; kept by user decision 2026-11-19)
//   QTY-13 (+DBP-02, LOAD-M01)                BidTakeoffs.Curve NULL loads as -1 (bid_data_reader.py:1131-1135)
//   DBP-03 (+MSH-M03)                         BidConditions.DisplaySize <= 0 / NULL loads as 100.0 (_normalize_display_size)
//   LP-20 (+DBP-28, DBP-29, BOOL-01)          BidPages: Show, Rotation % 360, flip / flag parsing, Sequence, SheetNo
// The expected values come from the Windows tests (cited per test) or from running the Windows reader read-only on the
// same rows (the literal is embedded with a comment).
#include "StoreTestSupport.hpp"

#include "Domain/UomService.hpp"

#include <string>

using ost_ptest::TempDb;

namespace
{
    // Bid 10 (project 2) with the given MeasureBase / TakeoffIncrements SQL values.
    void seed_bid(const TempDb &db, const std::string &measure_base, const std::string &increments)
    {
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase, TakeoffIncrements) VALUES "
                "(10, 2, 'Job', 10, " + measure_base + ", " + increments + ")");
    }

    ost_data::ProjectData load(const TempDb &db, const std::string &bid_uid = "10")
    {
        return ost_store::load_project(db.path(), bid_uid);
    }

    const ost_domain::Page &page_of(const ost_data::ProjectData &project, const std::string &uid)
    {
        auto it = project.pages.find(uid);
        if (it == project.pages.end())
            FAIL_TEST("page " + uid + " was not loaded");
        return it->second;
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// DBP-04
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/infrastructure/mdb/components/test_hierarchy_reader.py:534-548 (orphan bid: NULL TakeoffIncrements ->
// 1.0) and test_minimal_schema_hierarchy_uses_documented_defaults (a missing value is 1 inch).
TEST(Dbp04_TakeoffIncrements, NullLoadsAsOneInch)
{
    TempDb db("dbp04_null");
    seed_bid(db, "NULL", "NULL");
    const ost_data::ProjectData project = load(db);
    REQUIRE_EQ(project.bid.uid, std::string("10"));
    CHECK_EQ(project.bid.takeoff_increments, 1.0);
    CHECK_EQ(project.bid.measure_base, 0);
    CHECK_EQ(project.bid.snap_increments_ost(), 1.0);
}

// DBP-05 (kept by user decision 2026-11-19, see PORTING_BLOCKERS.md): a stored 0 is not turned into 1.0. Windows disagrees
// with itself here: hierarchy_reader.py:126 (`float(TakeoffIncrements or 1.0)`) maps 0 to 1.0 while settings_reader.py:151 and
// the cover sheet dialog keep 0 and the plan view treats `<= 0` as "snapping off"; no Windows test pins 0. The iOS behavior
// stays (0 disables snapping, like a negative value). The test name keeps its old wording, it was not renamed.
TEST(Dbp05_TakeoffIncrementsZero, ZeroKeepsSnappingDisabledUntilTheOwnerDecides)
{
    int index = 0;
    for (const char *stored : {"0", "0.0"})
    {
        SCOPED_CONTEXT(std::string("TakeoffIncrements = ") + stored);
        TempDb db("dbp05_zero_" + std::to_string(index++));
        seed_bid(db, "0", stored);
        const ost_data::ProjectData project = load(db);
        REQUIRE_EQ(project.bid.uid, std::string("10"));
        CHECK_EQ(project.bid.takeoff_increments, 0.0);
        CHECK_EQ(project.bid.snap_increments_ost(), 0.0);
    }
}

// The snap increment the plan view derives (plan_view/view.py set_snap_settings): metric -> mm / 25.4, else inches;
// <= 0 disables snapping. SNP-01 / LP-19 plans.
TEST(Dbp04_TakeoffIncrements, SnapIncrementFollowsTheMeasurementSystem)
{
    struct Case
    {
        const char *measure_base;
        const char *increments;
        double stored;
        double snap;
    };
    const Case cases[] = {
        {"1", "10", 10.0, 10.0 / 25.4},   // 0.3937007874015748 (Python 10 / 25.4; the SNP-01 plan quotes 10 * (1 / 25.4))
        {"1", "0.5", 0.5, 0.5 / 25.4},
        {"1", "50.8", 50.8, 50.8 / 25.4}, // 2.0 (LP-19)
        {"0", "5.0", 5.0, 5.0},
        {"2", "2.0", 2.0, 2.0},           // only MeasureBase 1 is metric
        {"0", "0.5", 0.5, 0.5},
        {"0", "-2", -2.0, 0.0},           // negative stays negative, snapping is disabled
        {"1", "-2", -2.0, 0.0},
        // Values below come from running the Windows MdbReader._parse_hierarchy (tests' sqlite shim, read-only) and
        // plan_view set_snap_settings on the same rows (scratch driver diff_snap.py):
        {"1", "NULL", 1.0, 1.0 / 25.4},   // NULL is 1.0 whatever MeasureBase is (not the dialog's 10 for metric): 0.03937007874015748
        {"1", "1.0", 1.0, 1.0 / 25.4},    // 0.03937007874015748
        {"0", "1e-9", 1e-9, 1e-9},        // any positive value is kept, however small
    };
    int index = 0;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("MeasureBase ") + c.measure_base + ", increments " + c.increments);
        TempDb db("dbp04_snap_" + std::to_string(index++));
        seed_bid(db, c.measure_base, c.increments);
        const ost_data::ProjectData project = load(db);
        REQUIRE_EQ(project.bid.uid, std::string("10"));
        CHECK_EQ(project.bid.takeoff_increments, c.stored);
        CHECK_EQ(project.bid.snap_increments_ost(), c.snap);
    }
    CHECK_EQ(10.0 / 25.4, 0.3937007874015748); // the value Python's 10 / 25.4 prints
}

// ---------------------------------------------------------------------------------------------------------------
// QTY-13
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    void seed_takeoff_world(const TempDb &db)
    {
        seed_bid(db, "0", "1");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (20, 10, 'P1')");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 10, 1, 'C', 0)");
    }

    const ost_domain::Takeoff &takeoff_of(const ost_data::ProjectData &project, const std::string &uid)
    {
        for (const ost_domain::Takeoff &t : project.takeoffs)
            if (t.uid == uid)
                return t;
        FAIL_TEST("takeoff " + uid + " was not loaded");
        return project.takeoffs.front();
    }
} // namespace

// Port of tests/infrastructure/mdb/components/test_bid_data_reader.py::TakeoffValueEdgeTests::
// test_explicit_zero_and_null_markers_are_roots_without_area_or_curve_loss: Curve 0 stays 0, NULL is -1 (straight),
// 3 stays 3; the area and parent markers 0 / NULL are "0".
TEST(Qty13_TakeoffCurve, NullCurveIsStraightAndExplicitCurvesAreKept)
{
    TempDb db("qty13_curve");
    seed_takeoff_world(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, BidAreaUID, Position, Rotation, Curve, "
            "ParentUID) VALUES (1, 10, 10, 20, 0, '1;2;3;4', 0, 0, 0)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, BidAreaUID, Position, Rotation, Curve, "
            "ParentUID) VALUES (2, 10, 10, 20, NULL, '1;2;3;4', NULL, NULL, NULL)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, BidAreaUID, Position, Rotation, Curve, "
            "ParentUID) VALUES (3, 10, 10, 20, 5, '1;2;3;4', -90.5, 3, 2)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position, Curve) "
            "VALUES (4, 10, 10, 20, '1;2;3;4', '-1')");

    const ost_data::ProjectData project = load(db);
    REQUIRE_EQ(project.takeoffs.size(), static_cast<size_t>(4));
    const ost_domain::Takeoff &explicit_zero = takeoff_of(project, "1");
    const ost_domain::Takeoff &null_row = takeoff_of(project, "2");
    const ost_domain::Takeoff &hole = takeoff_of(project, "3");
    const ost_domain::Takeoff &minus_one = takeoff_of(project, "4");

    CHECK_EQ(explicit_zero.curve, 0);
    CHECK_EQ(null_row.curve, -1);
    CHECK_EQ(hole.curve, 3);
    CHECK_EQ(minus_one.curve, -1);
    CHECK_STR_EQ(explicit_zero.area_uid, "0");
    CHECK_STR_EQ(null_row.area_uid, "0");
    CHECK_STR_EQ(hole.area_uid, "5");
    CHECK_STR_EQ(explicit_zero.parent_uid, "0");
    CHECK_STR_EQ(null_row.parent_uid, "0");
    CHECK_STR_EQ(hole.parent_uid, "2");
    CHECK_EQ(explicit_zero.rotation, 0.0);
    CHECK_EQ(null_row.rotation, 0.0);
    CHECK_EQ(hole.rotation, -90.5);
}

// The measured consequence (LOAD-M01 / the corrected plan of QTY-13): a NULL Curve is straight, so a multi-segment
// polyline measures every segment; Curve 0 would measure only the first curved piece. Windows values computed with
// uom_service._calc_linear_length (read-only run): ([0,0,100,0,0,0,0,100,0,100,50,0,0,0], -1) = 150.0,
// (same, 0) = 100.0, ([0,0,100,0,50,40], -1) = 100.0, (same, 3) = 138.26834959014053.
TEST(Qty13_TakeoffCurve, ALoadedNullCurveMeasuresEverySegmentOfThePolyline)
{
    TempDb db("qty13_length");
    seed_takeoff_world(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position, Curve) VALUES "
            "(1, 10, 10, 20, '0;0;100;0;0;0;0;100;0;100;50;0;0;0', NULL)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position, Curve) VALUES "
            "(2, 10, 10, 20, '0;0;100;0;0;0;0;100;0;100;50;0;0;0', 0)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position, Curve) VALUES "
            "(3, 10, 10, 20, '0;0;100;0;50;40', NULL)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position, Curve) VALUES "
            "(4, 10, 10, 20, '0;0;100;0;50;40', 3)");

    const ost_data::ProjectData project = load(db);
    REQUIRE_EQ(project.takeoffs.size(), static_cast<size_t>(4));
    auto length = [&](const char *uid)
    {
        const ost_domain::Takeoff &t = takeoff_of(project, uid);
        return ost_domain::calc_linear_length(t.position, t.curve);
    };
    CHECK_EQ(takeoff_of(project, "1").curve, -1);
    CHECK_EQ(length("1"), 150.0);
    CHECK_EQ(length("2"), 100.0);
    CHECK_EQ(length("3"), 100.0);
    CHECK_NEAR(length("4"), 138.26834959014053, 138.26834959014053 * 1e-12);
}

// ---------------------------------------------------------------------------------------------------------------
// DBP-03
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/infrastructure/mdb/components/test_bid_data_reader.py::ConditionDisplaySizeTests (None / 0 / "0" / -5
// -> 100.0; 75, "125", 0.5 unchanged) plus the MSH-M03 case (-50) and 150.5 (LOAD-01).
TEST(Dbp03_DisplaySize, NonPositiveAndMissingDisplaySizeLoadsAsOneHundredPercent)
{
    struct Case
    {
        const char *stored;
        double expected;
    };
    const Case cases[] = {
        {"NULL", 100.0}, {"0", 100.0}, {"'0'", 100.0}, {"-5", 100.0}, {"-50", 100.0}, {"-0.5", 100.0}, {"-0.0", 100.0},
        {"75", 75.0},    {"'125'", 125.0}, {"0.5", 0.5}, {"150.5", 150.5}, {"1e-9", 1e-9}, {"100", 100.0},
    };
    TempDb db("dbp03_display_size");
    seed_takeoff_world(db);
    int uid = 100;
    for (const Case &c : cases)
    {
        const std::string text = std::to_string(uid);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, DisplaySize) VALUES (" + text + ", 10, " +
                text + ", 'D', 2, " + c.stored + ")");
        ++uid;
    }

    const ost_data::ProjectData project = load(db);
    uid = 100;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("DisplaySize = ") + c.stored);
        auto it = project.conditions.find(std::to_string(uid));
        ++uid;
        REQUIRE(it != project.conditions.end());
        CHECK_EQ(it->second.display_size, c.expected);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// LP-20
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    // DDL of the iOS BidPages table with OverlayResized declared WITHOUT type affinity, so a numeric flag stays numeric
    // (the Windows test declares the column the same way: "keep every value exactly as stored").
    void make_overlay_resized_untyped(const TempDb &db)
    {
        const std::string ddl = db.text("SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'BidPages'");
        const std::string typed = "OverlayResized TEXT";
        const size_t at = ddl.find(typed);
        REQUIRE(at != std::string::npos);
        std::string untyped = ddl;
        untyped.replace(at, typed.size(), "OverlayResized");
        db.must("DROP TABLE BidPages");
        db.must(untyped);
    }
} // namespace

// Port of test_bid_data_reader.py::BidPageReaderTests::test_every_page_column_is_mapped_from_its_own_field. The
// RasterDrawMethod column (1) must NOT be read as the show mode (Show = 2 is).
TEST(Lp20_PageFields, EveryPageColumnIsMappedFromItsOwnField)
{
    TempDb db("lp20_page_fields");
    seed_bid(db, "0", "1");
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (11, 10, 'F')");
    db.must("INSERT INTO BidPages (UID, BidUID, Name, BidPageFolderUID, SheetNo, Sequence, ImagePath, Width, Height, "
            "ScaleFactor1, ScaleFactor2, Rotation, FlipX, FlipY, Index1, Show, RasterDrawMethod, OverlayImagePath, "
            "OverlayOffsetX, OverlayOffsetY, OverlayRotation, OverlayRect, OverlayResized, DeskewRotationOverlay) VALUES "
            "(7, 10, 'Plan A', 11, 'A-101', 4, 'a.png', 42.0, 30.0, 0.25, 0.5, 450, -1, 1, 3, 2, 1, 'o.png', 1.5, 2.5, "
            "3.5, '1,2,3,4', -1, 0.75)");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (8, 20, 'other bid')");

    const ost_data::ProjectData project = load(db);
    REQUIRE_EQ(project.pages.size(), static_cast<size_t>(1));
    const ost_domain::Page &page = page_of(project, "7");
    CHECK_STR_EQ(page.name, "Plan A");
    CHECK_STR_EQ(page.folder_uid, "11");
    CHECK_STR_EQ(page.sheet_no, "A-101");
    CHECK_EQ(page.sequence, 4);
    CHECK_STR_EQ(page.image_path, "a.png");
    CHECK_EQ(page.width, 42.0);
    CHECK_EQ(page.height, 30.0);
    CHECK_EQ(page.scale_factor1, 0.25);
    CHECK_EQ(page.scale_factor2, 0.5);
    CHECK_EQ(page.rotation, 90);            // 450 % 360
    CHECK(page.flip_x);                     // -1
    CHECK(page.flip_y);                     // 1
    CHECK_EQ(page.page_index, 2);           // Index1 3
    CHECK_EQ(page.image_show_mode, 2);      // Show, not RasterDrawMethod
    CHECK_STR_EQ(page.overlay_image_path, "o.png");
    CHECK_EQ(page.overlay_offset_x, 1.5);
    CHECK_EQ(page.overlay_offset_y, 2.5);
    CHECK_EQ(page.overlay_rotation, 3.5);
    CHECK(page.overlay_resized);
    CHECK_EQ(page.deskew_rotation_overlay, 0.75);
}

// DBP-28: Show = 2 with RasterDrawMethod = 1 gives 2; a NULL Show is 0 (`int(row.Show) if row.Show is not None else 0`),
// whatever RasterDrawMethod holds. Values of a read-only Windows run of _parse_bid_pages_for_bid.
TEST(Lp20_PageFields, ShowIsTheImageShowModeNotRasterDrawMethod)
{
    struct Case
    {
        const char *show;
        int raster;
        int expected;
    };
    const Case cases[] = {{"2", 1, 2}, {"NULL", 1, 0}, {"NULL", 0, 0}, {"1.9", 3, 1}, {"-1", 0, -1}, {"0", 2, 0}, {"3", 0, 3}};
    TempDb db("lp20_show");
    seed_bid(db, "0", "1");
    int uid = 1;
    for (const Case &c : cases)
        db.must("INSERT INTO BidPages (UID, BidUID, Show, RasterDrawMethod) VALUES (" + std::to_string(uid++) + ", 10, " +
                c.show + ", " + std::to_string(c.raster) + ")");
    const ost_data::ProjectData project = load(db);
    uid = 1;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("Show = ") + c.show + ", RasterDrawMethod = " + std::to_string(c.raster));
        CHECK_EQ(page_of(project, std::to_string(uid++)).image_show_mode, c.expected);
    }
}

// DBP-29: `int(Rotation or 0) % 360` (Python floored modulo of the truncated value). Values of a read-only Windows run
// of _parse_bid_pages_for_bid on a REAL column: -90 -> 270, 450 -> 90, 90.7 -> 90, NULL -> 0, -450 -> 270, 360 -> 0,
// 725 -> 5, -0.5 -> 0, -90.7 -> 270, 719.9 -> 359, 0 -> 0, 359.99 -> 359, -360 -> 0, -1 -> 359, 1e10 -> 280.
TEST(Lp20_PageFields, RotationIsTruncatedAndWrappedIntoZeroTo359)
{
    struct Case
    {
        const char *stored;
        int expected;
    };
    const Case cases[] = {{"-90", 270},  {"450", 90},   {"90.7", 90},  {"NULL", 0},     {"-450", 270}, {"360", 0},
                          {"725", 5},    {"-0.5", 0},   {"-90.7", 270}, {"719.9", 359}, {"0", 0},      {"359.99", 359},
                          {"-360", 0},   {"-1", 359},   {"1e10", 280},  {"90", 90},     {"359", 359}};
    TempDb db("lp20_rotation");
    seed_bid(db, "0", "1");
    int uid = 1;
    for (const Case &c : cases)
        db.must("INSERT INTO BidPages (UID, BidUID, Rotation) VALUES (" + std::to_string(uid++) + ", 10, " + c.stored + ")");
    const ost_data::ProjectData project = load(db);
    uid = 1;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("Rotation = ") + c.stored);
        CHECK_EQ(page_of(project, std::to_string(uid++)).rotation, c.expected);
    }
}

// `FlipX in (-1, True)`: only -1 and 1 flip (also 1.0 / -1.0); 2 does not, text does not. Windows run on an INTEGER column:
// (FlipX, FlipY) = (2, -1) -> (False, True); (1, 1) -> (True, True); (-1.0, 1.0) -> (True, True); (0, 0) -> (False, False);
// (-1, 0) -> (True, False); (3, -3) -> (False, False); ('true', 'x') -> (False, False).
TEST(Lp20_PageFields, FlipFlagsAreSetOnlyByMinusOneAndOne)
{
    struct Case
    {
        const char *x;
        const char *y;
        bool flip_x;
        bool flip_y;
    };
    const Case cases[] = {{"2", "-1", false, true},   {"1", "1", true, true},       {"-1.0", "1.0", true, true},
                          {"0", "0", false, false},   {"-1", "0", true, false},     {"3", "-3", false, false},
                          {"'true'", "'x'", false, false}, {"NULL", "NULL", false, false}, {"1.5", "-1.5", false, false}};
    TempDb db("lp20_flip");
    seed_bid(db, "0", "1");
    int uid = 1;
    for (const Case &c : cases)
        db.must("INSERT INTO BidPages (UID, BidUID, FlipX, FlipY) VALUES (" + std::to_string(uid++) + ", 10, " + c.x + ", " +
                c.y + ")");
    const ost_data::ProjectData project = load(db);
    uid = 1;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("FlipX = ") + c.x + ", FlipY = " + c.y);
        const ost_domain::Page &page = page_of(project, std::to_string(uid++));
        CHECK_EQ(page.flip_x, c.flip_x);
        CHECK_EQ(page.flip_y, c.flip_y);
    }
}

// Port of test_bid_data_reader.py::test_overlay_resized_reads_every_stored_spelling_of_the_flag (and the TEXT-column and
// bytes variants), restricted to the spellings a TEXT column can hold; BOOL-01 plan ('-1', '1', 'true', 'TRUE ' -> true;
// '0', '2', 'yes' -> false). The Unicode-whitespace and bad-letter rows are values of a read-only Windows run:
// ' 1 ' -> True, ' true' -> True, '\x1c1' -> True, 'truе' (Cyrillic e) -> False, '1.0' -> False,
// '+1' -> False, '01' -> False.
TEST(Lp20_PageFields, OverlayResizedReadsEveryStoredSpellingOfTheFlag)
{
    struct Case
    {
        std::string stored; // SQL literal
        bool expected;
    };
    const std::vector<Case> cases = {
        {"'-1'", true},        {"'1'", true},         {"'True'", true},       {"'true'", true},     {"'TRUE '", true},
        {"' -1 '", true},      {"'0'", false},        {"''", false},          {"NULL", false},      {"'False'", false},
        {"'false'", false},    {"'2'", false},        {"'-2'", false},        {"'yes'", false},     {"'1.0'", false},
        {"'+1'", false},       {"'01'", false},       {"'\xC2\xA0" "1\xC2\xA0'", true},  {"'\xE2\x80\x83true'", true},
        {"'tru\xD0\xB5'", false}, {"-1", true},       {"1", true},            {"0", false},         {"2", false},
        {"X'2D31'", true},     {"X'30'", false},      {"'  '", false},        {"'TrUe'", true},
    };
    TempDb db("lp20_overlay_resized");
    seed_bid(db, "0", "1");
    int uid = 1;
    for (const Case &c : cases)
        db.must("INSERT INTO BidPages (UID, BidUID, OverlayResized) VALUES (" + std::to_string(uid++) + ", 10, " + c.stored + ")");
    const ost_data::ProjectData project = load(db);
    uid = 1;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT("OverlayResized = " + c.stored);
        CHECK_EQ(page_of(project, std::to_string(uid++)).overlay_resized, c.expected);
    }
}

// The numeric spellings of the same Windows test when the stored value keeps its numeric type: True / -1 / 1 / -1.0 / 1.0
// -> True; 0.0 / 0 / 2 -> False.
TEST(Lp20_PageFields, OverlayResizedKeepsNumericStorageSemantics)
{
    struct Case
    {
        const char *stored;
        bool expected;
    };
    const Case cases[] = {{"-1", true}, {"1", true}, {"-1.0", true}, {"1.0", true}, {"0.0", false}, {"0", false},
                          {"2", false}, {"-2", false}, {"1.5", false}, {"NULL", false}};
    TempDb db("lp20_overlay_resized_numeric");
    seed_bid(db, "0", "1");
    make_overlay_resized_untyped(db);
    int uid = 1;
    for (const Case &c : cases)
        db.must("INSERT INTO BidPages (UID, BidUID, OverlayResized) VALUES (" + std::to_string(uid++) + ", 10, " + c.stored + ")");
    const ost_data::ProjectData project = load(db);
    REQUIRE_EQ(project.pages.size(), sizeof(cases) / sizeof(cases[0]));
    uid = 1;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("OverlayResized = ") + c.stored);
        CHECK_EQ(page_of(project, std::to_string(uid++)).overlay_resized, c.expected);
    }
}

// Sequence is `int(Sequence or 0)` (truncated), SheetNo is `decode_value(SheetNo)` ("" when NULL). Windows run on a REAL
// Sequence column: 7 -> 7, 7.9 -> 7, NULL -> 0, -3 -> -3, -3.9 -> -3, 0 -> 0; SheetNo NULL -> '', '' -> '', 'S1' -> 'S1',
// '  a ' -> '  a ' (not trimmed).
TEST(Lp20_PageFields, SequenceAndSheetNumberAreLoaded)
{
    struct Case
    {
        const char *sequence;
        const char *sheet_no;
        int expected_sequence;
        const char *expected_sheet_no;
    };
    const Case cases[] = {{"7", "'S1'", 7, "S1"},        {"7.9", "NULL", 7, ""}, {"NULL", "''", 0, ""},
                          {"-3", "'  a '", -3, "  a "}, {"-3.9", "'S-2'", -3, "S-2"}, {"0", "'A-101'", 0, "A-101"}};
    TempDb db("lp20_sequence");
    seed_bid(db, "0", "1");
    int uid = 1;
    for (const Case &c : cases)
        db.must("INSERT INTO BidPages (UID, BidUID, Sequence, SheetNo) VALUES (" + std::to_string(uid++) + ", 10, " +
                c.sequence + ", " + c.sheet_no + ")");
    const ost_data::ProjectData project = load(db);
    uid = 1;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("Sequence = ") + c.sequence + ", SheetNo = " + c.sheet_no);
        const ost_domain::Page &page = page_of(project, std::to_string(uid++));
        CHECK_EQ(page.sequence, c.expected_sequence);
        CHECK_STR_EQ(page.sheet_no, c.expected_sheet_no);
    }
}
