// RPV-04, RPV-05, RPV-06, RPV-07, RPV-13, RPV-20 (aliases SNP-02/03/06/07, GEO-12/13): the placement snap and acceptance rules of
// the Windows plan view, extracted from PlanView+Placement.mm (placeSnapOst / placeSnapAngle / the placement tap and pan
// handlers) into Core/Geometry/placement_snap.hpp.
//
// Windows oracle: presentation/components/plan_view/components/placement_mode.py (_snap_angle, _snap_angle_for_placement,
// _snap_placement_distance, _grid_snap_from_cursor, _meets_placement_minimum, handle_place_press) and drag_handler.py snap_ost.
// The literal tables below were produced by running those Windows functions (read-only, python -B, offscreen Qt) with a
// harness that only sets _snap_increments / _mouse_*_snap_angle; each table row is {inputs..., Windows result}.
// Windows computes with libm sin/cos, so results that go through an angle are compared with 1e-9, exact roundings with ==.
#include "TestHarness.hpp"

#include "Geometry/placement_snap.hpp"

#include <cmath>
#include <string>

namespace
{
    using ost_placement::SnapKind;

    std::string label(const char *name, int index)
    {
        return std::string(name) + " row " + std::to_string(index);
    }
}

// Port of test_drag_handler.py::test_snap_ost_rounds_to_the_increment_and_ignores_non_positive_increments plus the
// banker's-rounding rows of RPV-07 (Python round(): ties to even; C round(): ties away from zero).
TEST(PlacementSnapOst, MatchesPythonRoundHalfToEven)
{
    struct Row
    {
        double inc, value, expected;
    };
    const Row rows[] = {
        {0.0, 12.4, 12.4},    {-5.0, 12.4, 12.4},  {5.0, 12.4, 10.0},   {5.0, 12.6, 15.0},   {5.0, -12.6, -15.0},
        {5.0, 0.0, 0.0},      {0.25, 1.1, 1.0},    {0.25, 1.2, 1.25},   {1.0, 0.5, 0.0},     {1.0, 1.5, 2.0},
        {1.0, 2.5, 2.0},      {1.0, 3.5, 4.0},     {1.0, 10.5, 10.0},   {1.0, 20.5, 20.0},   {1.0, -0.6, -1.0},
        {1.0, -0.5, 0.0},     {1.0, -1.5, -2.0},   {1.0, -2.5, -2.0},   {0.5, 1.25, 1.0},    {0.25, 0.125, 0.0},
        {0.25, 0.625, 0.5},   {0.5, 0.75, 1.0},    {0.5, 0.25, 0.0},    {2.0, 3.0, 4.0},     {2.0, 5.0, 4.0},
        {10.0, 15.0, 20.0},   {10.0, 25.0, 20.0},
    };
    int index = 0;
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(label("snap_ost", index++));
        CHECK_NEAR(ost_placement::snap_ost(row.value, row.inc), row.expected, 1e-9);
    }
}

TEST(PlacementSnapOst, TiesGoToTheEvenNeighbourExactly)
{
    // RPV-07 plan: inc 1 -> 0.5, 1.5, 2.5, 3.5, 10.5, 20.5 give 0, 2, 2, 4, 10, 20 (exactly, not within a tolerance).
    CHECK_EQ(ost_placement::snap_ost(0.5, 1.0), 0.0);
    CHECK_EQ(ost_placement::snap_ost(1.5, 1.0), 2.0);
    CHECK_EQ(ost_placement::snap_ost(2.5, 1.0), 2.0);
    CHECK_EQ(ost_placement::snap_ost(3.5, 1.0), 4.0);
    CHECK_EQ(ost_placement::snap_ost(10.5, 1.0), 10.0);
    CHECK_EQ(ost_placement::snap_ost(20.5, 1.0), 20.0);
    CHECK_EQ(ost_placement::snap_ost(-2.5, 1.0), -2.0);
}

// The Windows default (Config.DEFAULT_MOUSE_UNPRESSED_SNAP_ANGLE) and the other constants named by the decisions.
TEST(PlacementConstants, MatchTheWindowsDefaults)
{
    CHECK_EQ(ost_placement::kDefaultSnapAngleDegrees, 15.0);
    CHECK_EQ(ost_placement::kGridSnapThresholdPx, 8.0);
    CHECK_EQ(ost_placement::kPolygonCloseThresholdPx, 12.0);
    CHECK_EQ(ost_placement::kFallbackMinimumPlacementLength, 1e-6);
}

// Port of test_placement_mode.py::test_default_mouse_snap_angles_are_15_unpressed_and_off_when_pressed (15 degrees:
// x = length * 0.9659258263, y = length * 0.2588190451), ::test_zero_mouse_snap_angle_disables_angle_snap and
// ::test_configured_pressed_mouse_snap_angle_uses_shift_state (90 degrees, target (3, 10) -> (0, 10.44031)); the other rows
// are Windows _snap_angle outputs for the angles of RPV-04 (iOS used 22.5 degrees: (9.645585, 3.995332) for (10, 3)).
TEST(PlacementSnapAngle, MatchesTheWindowsSnapAngle)
{
    struct Row
    {
        double angle, ox, oy, tx, ty, ex, ey;
    };
    const Row rows[] = {
        {15.0, 0.0, 0.0, 10.0, 3.0, 10.084561691330562, 2.7021501612138605},
        {15.0, 0.0, 0.0, 9.902680687415703, 1.3917310096006543, 9.659258262890683, 2.5881904510252074},
        {15.0, 0.0, 0.0, 7.0710678118654755, 7.0710678118654755, 7.0710678118654755, 7.0710678118654755},
        {15.0, 0.0, 0.0, 6.427876096865393, 7.66044443118978, 7.0710678118654755, 7.0710678118654755},
        {0.0, 0.0, 0.0, 10.0, 3.0, 10.0, 3.0},
        {90.0, 0.0, 0.0, 3.0, 10.0, 6.392843974127292e-16, 10.44030650891055},
        {15.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        {15.0, 2.0, 3.0, 2.0, 3.0, 2.0, 3.0},
        {15.0, 2.0, 3.0, 2.0000000005, 3.0, 2.0000000005, 3.0},
        {15.0, 5.0, 5.0, -5.0, -2.0, -5.571187255932985, -1.1032778078668555},
        {15.0, -3.0, 4.0, -13.0, 10.0, -13.099504938362077, 9.830951894845304},
        {15.0, 0.0, 0.0, -10.0, 3.0, -10.08456169133056, 2.7021501612138636},
        {15.0, 0.0, 0.0, 10.0, -3.0, 10.084561691330562, -2.7021501612138605},
        {15.0, 0.0, 0.0, -10.0, -3.0, -10.08456169133056, -2.7021501612138636},
        {45.0, 1.0, 1.0, 4.0, 2.0, 4.16227766016838, 1.0},
        {30.0, 0.0, 0.0, 10.0, 3.6397023426620234, 9.216049851068764, 5.32088886237956},
        {5.0, 0.0, 0.0, 10.0, 1.0, 10.01163281024212, 0.8759043742803736},
        {10.0, 0.0, 0.0, -4.0, 9.0, -3.368507756964434, 9.254898999517522},
        {15.0, 0.0, 0.0, 9.914448613738104, 1.3052619222005157, 10.0, 0.0},
        {15.0, 0.0, 0.0, 9.238795325112868, 3.826834323650898, 8.660254037844387, 4.999999999999999},
        {15.0, 0.0, 0.0, 9.914448613738104, -1.3052619222005157, 10.0, 0.0},
        {15.0, 0.0, 0.0, -10.0, 0.0, -10.0, 1.2246467991473533e-15},
        {15.0, 0.0, 0.0, -10.0, 1e-09, -10.0, 1.2246467991473533e-15},
        {15.0, 0.0, 0.0, -10.0, -1e-09, -10.0, -1.2246467991473533e-15},
        {90.0, 0.0, 0.0, -3.0, -10.0, 6.392843974127292e-16, -10.44030650891055},
        {-15.0, 0.0, 0.0, 10.0, 3.0, 10.0, 3.0},
    };
    int index = 0;
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(label("snap_angle", index++));
        const auto r = ost_placement::snap_angle(row.ox, row.oy, row.tx, row.ty, row.angle);
        CHECK_NEAR(r[0], row.ex, 1e-9);
        CHECK_NEAR(r[1], row.ey, 1e-9);
    }
}

TEST(PlacementSnapAngle, FifteenDegreesGivesTheWindowsDefaultResultNotTwentyTwoAndAHalf)
{
    // RPV-04 plan: snap_angle(0,0,10,3) with 15 degrees -> (len*0.9659258263, len*0.2588190451) = (10.084562, 2.702150);
    // the 22.5 degree iOS result was (9.645585, 3.995332).
    const auto r = ost_placement::snap_angle(0.0, 0.0, 10.0, 3.0, ost_placement::kDefaultSnapAngleDegrees);
    const double length = std::hypot(10.0, 3.0);
    CHECK_NEAR(r[0], length * 0.9659258263, 1e-6);
    CHECK_NEAR(r[1], length * 0.2588190451, 1e-6);
    CHECK(std::fabs(r[0] - 9.645585) > 0.1);
}

// Angle ties go to the even neighbour like Python's round. With a 90 degree step the direction (1, 1) has the exact quotient
// atan2(1, 1) / radians(90) = 0.5 (and (-1, 1) has 1.5): Python round(0.5) = 0 puts (1, 1) on the +x axis, while C round(0.5) = 1
// would put it on the +y axis. Windows results (read-only run of _snap_angle with a 90 degree step):
TEST(PlacementSnapAngle, ExactAngleTiesAreSnappedToTheEvenNeighbour)
{
    struct Row
    {
        double tx, ty, ex, ey;
    };
    const Row rows[] = {
        {1.0, 1.0, 1.4142135623730951, 0.0},
        {1.0, -1.0, 1.4142135623730951, 0.0},
        {-1.0, 1.0, -1.4142135623730951, 1.7319121124709868e-16},
        {-1.0, -1.0, -1.4142135623730951, -1.7319121124709868e-16},
        {2.0, 2.0, 2.8284271247461903, 0.0},
        {3.0, 3.0, 4.242640687119285, 0.0},
    };
    int index = 0;
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(label("angle tie", index++));
        const auto r = ost_placement::snap_angle(0.0, 0.0, row.tx, row.ty, 90.0);
        CHECK_NEAR(r[0], row.ex, 1e-9);
        CHECK_NEAR(r[1], row.ey, 1e-9);
    }
}

// Port of test_grid_distance_snap_rounds_to_increment_and_collapses_short_drags (inc 5: (0,0)->(12,0) gives (10,0); (13,0) gives
// (15,0), Windows 14.999999999999998; origin (2,3) to (4,3) collapses to the origin; zero-length and increment 0 unchanged)
// plus Windows _snap_placement_distance rows for other lengths.
TEST(PlacementSnapDistance, MatchesTheWindowsLengthSnap)
{
    struct Row
    {
        double inc, ox, oy, tx, ty, ex, ey;
    };
    const Row rows[] = {
        {5.0, 0.0, 0.0, 12.0, 0.0, 10.0, 0.0},
        {5.0, 0.0, 0.0, 13.0, 0.0, 14.999999999999998, 0.0},
        {5.0, 2.0, 3.0, 4.0, 3.0, 2.0, 3.0},
        {5.0, 2.0, 3.0, 2.0, 3.0, 2.0, 3.0},
        {0.0, 0.0, 0.0, 12.0, 0.0, 12.0, 0.0},
        {-1.0, 0.0, 0.0, 12.0, 0.0, 12.0, 0.0},
        {1.0, 0.0, 0.0, 9.0, 4.0, 9.138115486202572, 4.061384660534476},
        {1.0, 0.0, 0.0, 3.0, 4.0, 3.0, 4.0},
        {0.1, 0.0, 0.0, 0.30000000000000004, 0.0, 0.30000000000000004, 0.0},
        {2.0, 1.0, 1.0, 1.0000000001, 1.0, 1.0000000001, 1.0},
        {5.0, 0.0, 0.0, 2.5, 0.0, 0.0, 0.0},
        {5.0, 0.0, 0.0, 7.5, 0.0, 10.0, 0.0},
        {5.0, -3.0, -3.0, -14.0, -3.0, -13.0, -3.0},
    };
    int index = 0;
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(label("snap_placement_distance", index++));
        const auto r = ost_placement::snap_placement_distance(row.ox, row.oy, row.tx, row.ty, row.inc);
        CHECK_NEAR(r[0], row.ex, 1e-9);
        CHECK_NEAR(r[1], row.ey, 1e-9);
    }
}

// Port of test_angle_snap_distance_increment_applies_only_to_grid_snap (target (10.4, 0), inc 1: GRID -> (10, 0), another kind ->
// (10.4, 0)) and the worked RPV-06 example (inc 1, 15 degrees, origin (0,0), target (10.3, 3.9): GRID (10.625184, 2.847009),
// NONE (10.638348, 2.850537); iOS gave (10.0, 4.0)).
TEST(PlacementSnapForPlacement, LengthIsRoundedOnlyForGridPointsAndTheAnglePreserved)
{
    struct Row
    {
        bool grid;
        double inc, angle, ox, oy, tx, ty, ex, ey;
    };
    const Row rows[] = {
        {true, 1.0, 15.0, 0.0, 0.0, 10.4, 0.0, 10.0, 0.0},
        {false, 1.0, 15.0, 0.0, 0.0, 10.4, 0.0, 10.4, 0.0},
        {true, 1.0, 15.0, 0.0, 0.0, 10.3, 3.9, 10.625184089179752, 2.847009496127728},
        {false, 1.0, 15.0, 0.0, 0.0, 10.3, 3.9, 10.638347650811484, 2.8505366618364683},
        {true, 1.0, 15.0, 0.0, 0.0, 9.0, 4.0, 8.660254037844386, 4.999999999999999},
        {true, 0.0, 15.0, 0.0, 0.0, 10.3, 3.9, 10.638347650811484, 2.8505366618364683},
        {true, 1.0, 0.0, 0.0, 0.0, 10.3, 3.9, 10.287255099263795, 3.895174260886291},
        {false, 1.0, 0.0, 0.0, 0.0, 10.3, 3.9, 10.3, 3.9},
        {true, 5.0, 15.0, 2.0, 3.0, 4.0, 3.0, 2.0, 3.0},
        {true, 5.0, 15.0, 2.0, 3.0, 2.0, 3.0, 2.0, 3.0},
        {true, 2.0, 45.0, 1.0, 1.0, 7.2, 4.6, 6.656854249492381, 6.656854249492381},
        {true, 0.5, 15.0, -4.0, 2.0, -9.9, 6.3, -10.495190528383288, 5.750000000000002},
    };
    int index = 0;
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(label("snap_angle_for_placement", index++));
        const auto r = ost_placement::snap_angle_for_placement(row.ox, row.oy, row.tx, row.ty,
                                                               row.grid ? SnapKind::Grid : SnapKind::None, row.inc,
                                                               row.angle);
        CHECK_NEAR(r[0], row.ex, 1e-9);
        CHECK_NEAR(r[1], row.ey, 1e-9);
    }
}

// The snapped ray is kept exactly: the angle of the result equals the angle snap's, only the length is rounded.
TEST(PlacementSnapForPlacement, GridResultStaysOnTheSnappedRay)
{
    const auto angled = ost_placement::snap_angle(0.0, 0.0, 10.3, 3.9, 15.0);
    const auto grid = ost_placement::snap_angle_for_placement(0.0, 0.0, 10.3, 3.9, SnapKind::Grid, 1.0, 15.0);
    CHECK_NEAR(std::atan2(grid[1], grid[0]), std::atan2(angled[1], angled[0]), 1e-12);
    const double length = std::hypot(grid[0], grid[1]);
    CHECK_NEAR(length, std::nearbyint(length), 1e-12); // a whole number of increments (1.0)
}

// Port of test_zero_grid_threshold_disables_grid_snap, test_non_positive_snap_increment_disables_grid_snap,
// test_disabling_snap_sources_returns_unsnapped_cursor_position (cursor (10.4, 20.6), increment 1): the harness maps one OST
// unit to one viewport pixel there. With the RPV-05 plan's 8 pixels per OST unit the nearest node (10, 21) is 4.5 px away.
TEST(PlacementGridSnap, SnapsOnlyWhenEnabledPositiveAndWithinTheThreshold)
{
    using ost_placement::grid_snap;
    // distance of the nearest node (10, 21): hypot(0.4, 0.4) = 0.566 OST = 4.5 px at 8 px per unit
    auto near = grid_snap(10.4, 20.6, 1.0, 8.0, 8.0, true);
    CHECK(near.kind == SnapKind::Grid);
    CHECK_EQ(near.point[0], 10.0);
    CHECK_EQ(near.point[1], 21.0);
    // threshold 0 -> the raw cursor, kind None
    auto zero_threshold = grid_snap(10.4, 20.6, 1.0, 8.0, 0.0, true);
    CHECK(zero_threshold.kind == SnapKind::None);
    CHECK_EQ(zero_threshold.point[0], 10.4);
    CHECK_EQ(zero_threshold.point[1], 20.6);
    // increment 0 and a negative increment -> raw
    for (double inc : {0.0, -1.0})
    {
        auto none = grid_snap(10.4, 20.6, inc, 8.0, 8.0, true);
        CHECK(none.kind == SnapKind::None);
        CHECK_EQ(none.point[0], 10.4);
        CHECK_EQ(none.point[1], 20.6);
    }
    // snapping disabled -> raw
    auto off = grid_snap(10.4, 20.6, 1.0, 8.0, 8.0, false);
    CHECK(off.kind == SnapKind::None);
    CHECK_EQ(off.point[0], 10.4);
    CHECK_EQ(off.point[1], 20.6);
}

// Port of test_grid_snap_is_skipped_when_snapped_point_is_beyond_pixel_threshold: increment 10, cursor (12.0, 20.6), the nearest
// node (10, 20) is hypot(2, 0.6) = 2.088 px away (one pixel per unit in that harness): threshold 1 -> raw coordinates, kind
// NONE; threshold 3 -> (10, 20) GRID.
TEST(PlacementGridSnap, NodeBeyondThePixelThresholdIsSkipped)
{
    using ost_placement::grid_snap;
    auto raw = grid_snap(12.0, 20.6, 10.0, 1.0, 1.0, true);
    CHECK(raw.kind == SnapKind::None);
    CHECK_EQ(raw.point[0], 12.0);
    CHECK_EQ(raw.point[1], 20.6);
    auto snapped = grid_snap(12.0, 20.6, 10.0, 1.0, 3.0, true);
    CHECK(snapped.kind == SnapKind::Grid);
    CHECK_EQ(snapped.point[0], 10.0);
    CHECK_EQ(snapped.point[1], 20.0);
}

// Windows rejects a snapped point with `dist_px > threshold_px`, so a distance exactly at the threshold is still snapped.
TEST(PlacementGridSnap, TheThresholdIsInclusive)
{
    using ost_placement::grid_snap;
    // cursor (10.5, 20.0), increment 1: ties to even -> (10, 20), 0.5 unit away = 4.0 px at 8 px per unit
    auto at = grid_snap(10.5, 20.0, 1.0, 8.0, 4.0, true);
    CHECK(at.kind == SnapKind::Grid);
    CHECK_EQ(at.point[0], 10.0);
    auto below = grid_snap(10.5, 20.0, 1.0, 8.0, 3.9, true);
    CHECK(below.kind == SnapKind::None);
    CHECK_EQ(below.point[0], 10.5);
}

// Port of placement_mode._meets_placement_minimum (length >= minimum or math.isclose(length, minimum)): RPV-13 plan
// (0.09999999999999998, 0.1) true; (0.099, 0.1) false; (0.1, 0.1) true; the minimum is 1e-6 when the increment is <= 0.
TEST(PlacementMinimum, AcceptsLengthsWithinIsCloseOfTheMinimum)
{
    using ost_placement::meets_placement_minimum;
    CHECK(meets_placement_minimum(0.09999999999999998, 0.1));
    CHECK_FALSE(meets_placement_minimum(0.099, 0.1));
    CHECK(meets_placement_minimum(0.1, 0.1));
    CHECK(meets_placement_minimum(0.5, 0.1));
    CHECK_FALSE(meets_placement_minimum(0.0, 0.1));
    // math.isclose rel_tol = 1e-9: 1e-9 relative is accepted, 1e-8 relative is not
    CHECK(meets_placement_minimum(0.1 * (1.0 - 5e-10), 0.1));
    CHECK_FALSE(meets_placement_minimum(0.1 * (1.0 - 2e-8), 0.1));
    CHECK_EQ(ost_placement::placement_minimum_length(0.1), 0.1);
    CHECK_EQ(ost_placement::placement_minimum_length(0.0), 1e-6);
    CHECK_EQ(ost_placement::placement_minimum_length(-2.0), 1e-6);
}

// RPV-13 note: with increment 0.1, a drag from grid index k to k + 1 has |d| = 0.09999999999999998 for k in
// {3, 4, 6, 7, 8, 9, 12, 14, 17, 19, 24, 29, 34, 39} (computed in Python for k in 0..39) and must still commit.
TEST(PlacementMinimum, OneIncrementDragsThatFloatRoundingLandsBelowTheIncrementCommit)
{
    const int below[] = {3, 4, 6, 7, 8, 9, 12, 14, 17, 19, 24, 29, 34, 39};
    int strictly_below = 0;
    for (int k = 0; k < 40; ++k)
    {
        const double a = k * 0.1, b = (k + 1) * 0.1;
        const double d = std::fabs(b - a);
        const bool lands_below = d < 0.1;
        bool expected_below = false;
        for (int v : below)
            expected_below = expected_below || v == k;
        CHECK_EQ(lands_below, expected_below); // the Windows enumeration is reproduced
        strictly_below += lands_below ? 1 : 0;
        CHECK(ost_placement::meets_placement_minimum(d, ost_placement::placement_minimum_length(0.1)));
    }
    CHECK_EQ(strictly_below, 14);
}

// Port of test_minimum_fractional_linear_and_area_release_preserves_preview / test_subminimum_geometry_does_not_commit:
// increment 0.1, linear (0.2,0.2)->(0.3,0.2) commits, an area (0.2,0.2)->(0.3,0.3) commits (each axis meets the minimum),
// (0.2,0.2)->(0.24,0.24) does not.
TEST(PlacementMinimum, FractionalLinearAndAreaReleases)
{
    using ost_placement::meets_placement_minimum;
    const double minimum = ost_placement::placement_minimum_length(0.1);
    CHECK(meets_placement_minimum(std::hypot(0.3 - 0.2, 0.2 - 0.2), minimum));
    CHECK(meets_placement_minimum(std::fabs(0.3 - 0.2), minimum));
    CHECK(meets_placement_minimum(std::fabs(0.3 - 0.2), minimum));
    CHECK_FALSE(meets_placement_minimum(std::fabs(0.24 - 0.2), minimum));
}

// RPV-20: Windows closes the polygon when the raw click is at most 12 px from the first point and the area has at least three
// points; the boundary is inclusive. (The snapped tap point is irrelevant; the caller measures the raw click.)
TEST(PlacementClose, ClosesWithinTwelvePixelsInclusiveWithThreePoints)
{
    using ost_placement::polygon_close_hit;
    CHECK(polygon_close_hit(3, 0.0));
    CHECK(polygon_close_hit(4, 11.99));
    CHECK(polygon_close_hit(4, 12.0)); // inclusive
    CHECK_FALSE(polygon_close_hit(4, 12.0001));
    CHECK_FALSE(polygon_close_hit(2, 0.0)); // fewer than three points: the click adds a point instead
    CHECK_FALSE(polygon_close_hit(1, 5.0));
    CHECK_FALSE(polygon_close_hit(0, 5.0));
}
