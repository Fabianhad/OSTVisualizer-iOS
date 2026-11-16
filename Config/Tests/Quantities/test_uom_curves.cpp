// Three-point arc length (curved linear takeoffs): perp_slope, arc sweep through the control point,
// straight chord when the control point coincides with an end point.
//
// Windows oracle: ost_visualizer/domain/services/uom_service.py (_perp_slope, _control_is_at_an_end,
// _straight_chord_points, _advanced_curve_points, _calc_linear_length) and
// tests/domain/services/test_uom_service.py (UomServiceTests, UomLinearQuantityTests,
// UomCurvedArcTests, UomTranslatedArcTests, UomArcControlPointSideTests,
// UomArcSideAndStraightChordTests, UomArcOrientationRegressionTests, UomWaveThreeArcOracleTests,
// UomWaveThreeReworkTests, UomSecondReviewGapTests).
//
// Items: QTY-07 (perp_slope dx/dy swapped), QTY-08 (arc side = arc through the control point),
// QTY-09 (control point at an end point -> straight chord).
//
// Tolerances: where the Windows test uses assertAlmostEqual the same delta/places are used.  Where
// Windows uses assertEqual on a computed length (sum of hypot steps) the C++ check is 1e-12 relative:
// Python's math.hypot is not the C library hypot, so exact bit equality is not portable.
#include "QtyHelpers.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <vector>

using namespace qty;

namespace
{
    using Pt = std::pair<double, double>;
    using Pos = std::vector<double>;

    double arc_len(const Pos &position) { return calc_linear_length(position, 0); }

    Pt circle_point(double radius, double degrees, Pt center = {0.0, 0.0})
    {
        double angle = radians(degrees);
        return {center.first + radius * std::cos(angle), center.second + radius * std::sin(angle)};
    }

    // position = [*p1, *p2, *control] (start, end, through)
    Pos arc_position(Pt p1, Pt p2, Pt c) { return {p1.first, p1.second, p2.first, p2.second, c.first, c.second}; }

    // Windows _circumcircle_arc_length: length of the arc p1 -> control -> p2 of the circle through the
    // three points (closed-form determinant circumcentre, direction from the orientation of the triple).
    double circumcircle_arc_length(Pt p1, Pt control, Pt p2)
    {
        double ox = p1.first, oy = p1.second;
        double ax = 0.0, ay = 0.0;
        double bx = control.first - ox, by = control.second - oy;
        double cx = p2.first - ox, cy = p2.second - oy;
        double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
        double ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d;
        double uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d;
        double radius = std::hypot(ax - ux, ay - uy);
        double start = std::atan2(ay - uy, ax - ux);
        double end = std::atan2(cy - uy, cx - ux);
        bool counter_clockwise = (bx - ax) * (cy - by) - (by - ay) * (cx - bx) > 0;
        double full_turn = 2.0 * M_PI;
        double sweep = counter_clockwise ? py_mod(end - start, full_turn) : py_mod(start - end, full_turn);
        return radius * sweep;
    }

    // UomArcSideAndStraightChordTests.assert_arc (radius 100, center (500, 500) unless given)
    void assert_arc(double start, double through, double end, double radius = 100.0, Pt center = {500.0, 500.0},
                    double rel = 0.005)
    {
        Pos position = arc_position(circle_point(radius, start, center), circle_point(radius, end, center),
                                    circle_point(radius, through, center));
        double measured = arc_len(position);
        double expected = arc_length_oracle(radius, start, through, end);
        CHECK_NEAR(measured, expected, expected * rel);
    }
} // namespace

// ============================================================================================
// QTY-07: perp_slope tests dy == 0 first, then dx == 0
// ============================================================================================

// Port of tests/domain/services/test_uom_service.py::UomLinearQuantityTests::test_curve_helpers_agree_with_closed_forms (perp_slope part)
TEST(Qty07_PerpSlope, PerpSlopeAgreesWithTheClosedForms)
{
    const double inf = std::numeric_limits<double>::infinity();
    CHECK_EQ(perp_slope(1.0, 0.0), inf);
    CHECK_EQ(perp_slope(0.0, 1.0), 0.0);
    CHECK_EQ(perp_slope(0.0, 0.0), inf);
    CHECK_EQ(perp_slope(2.0, 4.0), -0.5);
    CHECK_EQ(perp_slope(1.0, 2.0), -0.5);
    CHECK_EQ(perp_slope(-2.0, 4.0), 0.5);
}

// Port of tests/domain/services/test_uom_service.py::UomServiceTests::test_axis_aligned_circular_curve_uses_its_arc_length
// (headline test: pi / sqrt(2) +- 0.03) and the Windows value 2.196152422706632.
TEST(Qty07_PerpSlope, AxisAlignedCircularCurveUsesItsArcLength)
{
    QArgs a;
    a.position = {1.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    a.curve = 0;
    double quantity = quantities(ConditionType::Linear, 1, UOM_INCHES, a).q1;
    CHECK_NEAR(quantity, M_PI / std::sqrt(2.0), 0.03);
    CHECK_REL(quantity, 2.196152422706632, 1e-9);
}

// Port of tests/domain/services/test_uom_service.py::UomCurvedArcTests::test_arcs_with_axis_aligned_chord_segments_use_the_dedicated_center_formulas
TEST(Qty07_PerpSlope, ArcsWithAxisAlignedChordSegmentsUseTheDedicatedCenterFormulas)
{
    const double radius = 100.0;
    const double s = radius * std::sqrt(0.5);
    struct C
    {
        Pt p1, c, p2;
        double start, through, end;
    };
    // first segment horizontal, second segment horizontal, vertical segments
    const std::vector<C> cases = {
        {{-s, s}, {s, s}, {0.0, -radius}, 135, 45, 270},
        {{0.0, -radius}, {-s, s}, {s, s}, 270, 135, 45},
        {{s, s}, {s, -s}, {-s, -s}, 45, 315, 225},
        {{-s, -s}, {s, -s}, {s, s}, 225, 315, 45},
    };
    for (const C &c : cases)
    {
        SCOPED_CONTEXT("arc (" + std::to_string(c.start) + ", " + std::to_string(c.through) + ", " + std::to_string(c.end) + ")");
        double expected = arc_length_oracle(radius, c.start, c.through, c.end);
        CHECK_NEAR(arc_len(arc_position(c.p1, c.p2, c.c)), expected, expected * 0.004);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomCurvedArcTests::test_arc_length_matches_the_circle_for_the_listed_orientations_and_chord_directions
TEST(Qty07_PerpSlope, ArcLengthMatchesTheCircleForTheListedOrientationsAndChordDirections)
{
    const double radius = 100.0;
    const std::array<std::array<double, 3>, 14> cases = {{
        {0, 45, 90}, {90, 45, 0}, {0, 90, 180}, {180, 90, 0}, {180, 270, 0}, {0, 270, 180}, {30, 120, 250},
        {250, 120, 30}, {135, 45, 270}, {270, 135, 45}, {45, 315, 135}, {45, 135, 225}, {10, 20, 30}, {200, 100, 20},
    }};
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT("arc (" + std::to_string(c[0]) + ", " + std::to_string(c[1]) + ", " + std::to_string(c[2]) + ")");
        Pos position = arc_position(circle_point(radius, c[0]), circle_point(radius, c[2]), circle_point(radius, c[1]));
        double expected = arc_length_oracle(radius, c[0], c[1], c[2]);
        CHECK_NEAR(arc_len(position), expected, expected * 0.004);
    }
}

// Windows _calc_linear_length(pos, curve=0) for control points whose first or second segment is axis
// aligned (CURVE-01 plan): [0,0,10,10,10,0] -> 22.0284485899 (iOS 16.198), [27,26,4,15,27,27] -> 57.856213
// (iOS 25.558), [9,34,7,36,9,43] -> 28.664197 (iOS 8.596), plus values that were already right.
TEST(Qty07_PerpSlope, ArcLengthsWithAxisAlignedSegmentsMatchTheWindowsValues)
{
    struct C
    {
        Pos pos;
        double windows;
    };
    const std::vector<C> cases = {
        {{0, 0, 10, 10, 10, 0}, 22.028448589906333},
        {{27, 26, 4, 15, 27, 27}, 57.85621286019014},
        {{9, 34, 7, 36, 9, 43}, 28.66419650095284},
        {{0, 0, 10, 0, 5, 5}, 15.576465376942007},
        {{0, 0, 10, 0, 5, 5, 3}, 12.18336592166496},
        {{0, 0, 100, 0, 50, 30}, 122.45980862598586},
    };
    for (const C &c : cases)
    {
        SCOPED_CONTEXT("position[0..5] " + std::to_string(c.pos[0]) + "," + std::to_string(c.pos[1]) + "," +
                       std::to_string(c.pos[2]) + "," + std::to_string(c.pos[3]) + "," + std::to_string(c.pos[4]) +
                       "," + std::to_string(c.pos[5]));
        CHECK_REL(arc_len(c.pos), c.windows, 1e-9);
    }
}

// ============================================================================================
// QTY-08: arc side = the arc through the control point (any sweep)
// ============================================================================================

// Port of tests/domain/services/test_uom_service.py::UomArcControlPointSideTests
TEST(Qty08_ArcSweep, ControlPointCloseToTheEndOfAMajorArcIsMeasuredOnTheMajorArc)
{
    auto measured = [](double start, double through, double end)
    {
        const double radius = 100.0;
        return arc_len(arc_position(circle_point(radius, start), circle_point(radius, end), circle_point(radius, through)));
    };
    double expected = arc_length_oracle(100.0, 0, 250, 251);
    CHECK_NEAR(measured(0, 250, 251), expected, expected * 0.01); // ~438.1 (was 190.2)
    expected = arc_length_oracle(100.0, 0, 110, 100);
    CHECK_NEAR(measured(0, 110, 100), expected, expected * 0.01); // ~453.8 (was 174.5)
    // Pins the corrected values (tessellated into at most 64 segments).
    CHECK_NEAR(measured(0, 250, 251), 438.1, 0.5);
    CHECK_NEAR(measured(0, 110, 100), 453.8, 0.5);
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_the_ost_confirmed_arc_measures_the_251_degree_route_through_the_service_and_the_public_path
TEST(Qty08_ArcSweep, TheOstConfirmedArcMeasuresThe251DegreeRoute)
{
    const double radius = 100.0;
    Pt center = {500.0, 500.0};
    Pos position = arc_position(circle_point(radius, 0, center), circle_point(radius, 251, center),
                                circle_point(radius, 250, center));
    double expected = radius * radians(251.0);
    CHECK_NEAR(expected, 438.08, 0.005);
    CHECK_NEAR(arc_len(position), expected, expected * 0.005);
    QArgs a;
    a.position = position;
    a.curve = 0;
    CHECK_NEAR(quantities(ConditionType::Linear, 1, UOM_INCHES, a).q1, expected, expected * 0.005);
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_the_arc_direction_is_the_side_of_the_chord_the_control_is_on
TEST(Qty08_ArcSweep, TheArcDirectionIsTheSideOfTheChordTheControlIsOn)
{
    const double arcs[][3] = {{251, 250, 0}, {0, -110, -251}, {0, 110, 100}, {100, 110, 0}, {0, -250, -251}};
    for (const auto &c : arcs)
    {
        SCOPED_CONTEXT("arc (" + std::to_string(c[0]) + ", " + std::to_string(c[1]) + ", " + std::to_string(c[2]) + ")");
        assert_arc(c[0], c[1], c[2]);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_major_arcs_with_the_control_point_near_either_end
TEST(Qty08_ArcSweep, MajorArcsWithTheControlPointNearEitherEnd)
{
    for (double sweep : {200.0, 250.0, 300.0})
        for (double fraction : {0.01, 0.03, 0.05, 0.5, 0.95, 0.97, 0.99})
            for (double direction : {1.0, -1.0})
                for (double start : {0.0, 100.0, 200.0, 310.0})
                {
                    SCOPED_CONTEXT("sweep " + std::to_string(sweep) + " fraction " + std::to_string(fraction) +
                                   " direction " + std::to_string(direction) + " start " + std::to_string(start));
                    assert_arc(start, start + direction * sweep * fraction, start + direction * sweep);
                }
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_arcs_whose_angles_wrap_past_plus_or_minus_180_degrees
TEST(Qty08_ArcSweep, ArcsWhoseAnglesWrapPastPlusOrMinus180Degrees)
{
    for (double start : {-175.0, 170.0, 175.0, 179.5, 180.0, 185.0, 359.5})
        for (double sweep : {90.0, 200.0, 260.0})
            for (double fraction : {0.04, 0.5, 0.96})
                for (double direction : {1.0, -1.0})
                {
                    SCOPED_CONTEXT("start " + std::to_string(start) + " sweep " + std::to_string(sweep) +
                                   " fraction " + std::to_string(fraction) + " direction " + std::to_string(direction));
                    assert_arc(start, start + direction * sweep * fraction, start + direction * sweep);
                }
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_sweeps_up_to_180_degrees_are_measured_on_the_arc_through_the_control_point
TEST(Qty08_ArcSweep, SweepsUpTo180DegreesAreMeasuredOnTheArcThroughTheControlPoint)
{
    for (double sweep : {20.0, 60.0, 120.0, 170.0})
        for (double fraction : {0.05, 0.3, 0.5, 0.7, 0.95})
            for (double direction : {1.0, -1.0})
            {
                SCOPED_CONTEXT("sweep " + std::to_string(sweep) + " fraction " + std::to_string(fraction) +
                               " direction " + std::to_string(direction));
                assert_arc(30.0, 30.0 + direction * sweep * fraction, 30.0 + direction * sweep);
            }
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_a_control_point_on_the_bisector_gives_the_value_the_legacy_rule_gave
// (the legacy-rule oracle assertion is a test of the old formula and is not ported; the arc length
// itself is what must stay equal to the circle, up to 350 degrees).
TEST(Qty08_ArcSweep, AControlPointOnTheBisectorGivesTheCircleUpTo350Degrees)
{
    for (double sweep : {30.0, 90.0, 179.0, 181.0, 250.0, 300.0, 350.0})
        for (double direction : {1.0, -1.0})
        {
            SCOPED_CONTEXT("sweep " + std::to_string(sweep) + " direction " + std::to_string(direction));
            double start = 40.0;
            assert_arc(start, start + direction * sweep / 2.0, start + direction * sweep);
        }
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_a_control_point_that_is_near_but_not_equal_to_an_end_still_follows_the_arc
TEST(Qty08_ArcSweep, AControlPointNearButNotEqualToAnEndStillFollowsTheArc)
{
    // Control 0.2 degrees (0.35 units, 2e-3 of the chord) after the start of a 250 degree arc.
    const double arcs[][3] = {{0, 0.2, 250}, {0, 249.8, 250}, {90, 90.2, 340}, {0, -0.2, -250},
                              {0, 0.01, 250}, {0, 1e-5, 250},  {0, 249.99999, 250}, {0, -1e-5, -250}};
    for (const auto &c : arcs)
    {
        SCOPED_CONTEXT("arc (" + std::to_string(c[0]) + ", " + std::to_string(c[1]) + ", " + std::to_string(c[2]) + ")");
        assert_arc(c[0], c[1], c[2]);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomArcOrientationRegressionTests
TEST(Qty08_ArcSweep, LargeAndSmallArcsWithAFarOriginAreMeasuredOnTheArcThroughTheControlPoint)
{
    struct C
    {
        double radius;
        Pt center;
        double start, through, end;
    };
    const std::vector<C> cases = {
        {100.0, {6.0, 17.5}, 33.1, 235.4, 73.3},
        {100.0, {-431.2, 220.2}, 69.8, 343.2, 90.4},
        {100.0, {237.1, -37.4}, 332.9, 96.9, 294.2},
        {5.0, {-445.3, -358.3}, 61.9, 263.4, 285.0},
    };
    for (const C &c : cases)
    {
        SCOPED_CONTEXT("arc (" + std::to_string(c.radius) + ", " + std::to_string(c.start) + ", " +
                       std::to_string(c.through) + ", " + std::to_string(c.end) + ")");
        Pos position = arc_position(circle_point(c.radius, c.start, c.center), circle_point(c.radius, c.end, c.center),
                                    circle_point(c.radius, c.through, c.center));
        double expected = arc_length_oracle(c.radius, c.start, c.through, c.end);
        CHECK_NEAR(arc_len(position), expected, expected * 0.01 + 0.2);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomWaveThreeArcOracleTests::test_arbitrary_three_point_arcs_match_the_circumcircle_in_length
TEST(Qty08_ArcSweep, ArbitraryThreePointArcsMatchTheCircumcircleInLength)
{
    struct C
    {
        Pt p1, control, p2;
    };
    const std::vector<C> cases = {
        {{0, 0}, {80, 30}, {20, 70}},       {{0, 0}, {80, 30}, {10, -40}},   {{100, 100}, {160, 100}, {110, 40}},
        {{-200, 50}, {-150, -100}, {30, 20}}, {{0, 0}, {3, -4}, {100, 10}},    {{0, 0}, {-60, 40}, {90, 10}},
        {{50, 50}, {0, 0}, {100, 0}},       {{10, 0}, {90, 60}, {0, 10}},    {{0, 0}, {0.5, 70}, {100, 0.5}},
        {{0, 0}, {100, 1}, {50, 60}},       {{0, 0}, {120, 5}, {60, -80}},   {{300, 200}, {250, 260}, {380, 210}},
    };
    int index = 0;
    for (const C &c : cases)
    {
        SCOPED_CONTEXT("case " + std::to_string(index++));
        double expected = circumcircle_arc_length(c.p1, c.control, c.p2);
        CHECK_NEAR(arc_len(arc_position(c.p1, c.p2, c.control)), expected, expected * 0.003);
        // Swapping the end points walks the same circle the other way round and must pass through the
        // same control point: same arc length.
        CHECK_NEAR(arc_len(arc_position(c.p2, c.p1, c.control)), expected, expected * 0.003);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomWaveThreeArcOracleTests::test_mirroring_an_arc_does_not_change_its_length
TEST(Qty08_ArcSweep, MirroringAnArcDoesNotChangeItsLength)
{
    struct C
    {
        Pt p1, control, p2;
    };
    const std::vector<C> cases = {
        {{0, 0}, {80, 30}, {20, 70}},
        {{50, 50}, {0, 0}, {100, 0}},
        {{0, 0}, {120, 5}, {60, -80}},
    };
    int index = 0;
    for (const C &c : cases)
    {
        SCOPED_CONTEXT("case " + std::to_string(index++));
        double straight = arc_len(arc_position(c.p1, c.p2, c.control));
        double mirrored = arc_len(arc_position({-c.p1.first, c.p1.second}, {-c.p2.first, c.p2.second},
                                               {-c.control.first, c.control.second}));
        double flipped = arc_len(arc_position({c.p1.first, -c.p1.second}, {c.p2.first, -c.p2.second},
                                              {c.control.first, -c.control.second}));
        CHECK_NEAR(mirrored, straight, straight * 1e-9);
        CHECK_NEAR(flipped, straight, straight * 1e-9);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomWaveThreeArcOracleTests::test_segments_whose_x_step_is_exactly_minus_one_still_find_the_circle
TEST(Qty08_ArcSweep, SegmentsWhoseXStepIsExactlyMinusOneStillFindTheCircle)
{
    struct C
    {
        Pt p1, control, p2;
    };
    const std::vector<C> cases = {
        {{0, 0}, {-1, 50}, {40, 80}},
        {{0, 0}, {10, 20}, {9, 60}},
        {{0, 0}, {-1, -50}, {-30, -80}},
        {{0, 0}, {1, 50}, {40, 80}},
    };
    int index = 0;
    for (const C &c : cases)
    {
        SCOPED_CONTEXT("case " + std::to_string(index++));
        double expected = circumcircle_arc_length(c.p1, c.control, c.p2);
        CHECK_NEAR(arc_len(arc_position(c.p1, c.p2, c.control)), expected, expected * 0.003);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomWaveThreeReworkTests::test_a_huge_circle_keeps_the_short_sweep_when_control_and_end_angles_tie
// (curve=0 position [0,0,100,0,100-1.5e-7,-4.50354391908547e-16] -> 100.0 +- 0.01).
TEST(Qty08_ArcSweep, AHugeCircleKeepsTheShortSweepWhenControlAndEndAnglesTie)
{
    CHECK_NEAR(arc_len({0, 0, 100, 0, 100 - 1.5e-7, -4.50354391908547e-16}), 100.0, 0.01);
}

// Port of tests/domain/services/test_uom_service.py::UomSecondReviewGapTests::test_numerically_degenerate_circles_fall_back_to_the_quadratic_path
TEST(Qty08_ArcSweep, NumericallyDegenerateCirclesFallBackToTheQuadraticPath)
{
    const std::vector<Pos> triples = {
        {-15.05766633024992, 102.25967331254812, 27072.542757617703, -2214.0656632762107, 20067.03261395505, -1615.0077775681077},
        {3.2626032666664124, 369.36683081795644, -70452.7258311968, -30699.856722606186, -15689.96504820314, -6550.930634790724},
        {-179.919479464134, 60.15739467281006, 54892.68210123489, 7084.611892722189, 20583.415063180953, 2708.4996654084894},
        // Triples for which radius1 - radius2 (the next two) or radius1 - radius_c (the last two) comes
        // out NEGATIVE by more than the 0.1 tolerance: only an absolute-value comparison rejects them.
        {296.0400574287478, -161.74591924037009, 99222.97200211308, 3244.546671967867, 53411.49945461752, 1667.1473081719344},
        {449.75051153891013, 217.53465753309206, -47229.77133589611, 4848.248563504065, -42052.1321720158, 4345.387696856169},
        {205.39841524991493, 18.092321669440935, -34543.942674898884, 13400.592997815456, -20955.700756124465, 8167.554011437886},
        {148.10713847092245, 121.8096587122202, 86192.14185719546, -26713.382355153783, 33808.93792628607, -10376.24358153916},
    };
    int index = 0;
    for (const Pos &triple : triples)
    {
        SCOPED_CONTEXT("triple " + std::to_string(index++));
        double chord = std::hypot(triple[2] - triple[0], triple[3] - triple[1]);
        CHECK_NEAR(arc_len(triple), chord, 5e-4); // places=3
    }
}

// Windows _calc_linear_length(pos, curve=0) on 87 three-point arcs: 24 random 3-decimal arcs in +-100,
// 36 integer-grid arcs (many with axis-aligned chord segments), 10 arcs given by a seventh (offset)
// value and the fixed cases of the CURVE-02 / QTY-09 plans.  Seeded with random.Random(20261115).
// Compared at 1e-9 relative (iOS before the fix differed on about a fifth of them).
TEST(Qty08_ArcSweep, DifferentialAgainstTheWindowsArcLengths)
{
    struct C
    {
        Pos pos;
        double windows;
    };
    const std::vector<C> cases = {
        {{-39.833, -4.861, 53.251, 32.218, 21.002, -29.676}, 148.65094796574155}, // random3dp
        {{39.485, 55.882, -9.543, -61.529, 89.739, 98.439}, 1169.5433914593164}, // random3dp
        {{75.734, -52.038, 36.383, -69.99, 8.753, -1.013}, 219.2402404173536}, // random3dp
        {{74.148, 65.029, 43.594, -32.697, 70.356, 54.672}, 102.44017402397476}, // random3dp
        {{-27.019, -26.689, 25.757, 8.954, 22.145, -98.355}, 269.02505869905025}, // random3dp
        {{85.097, -40.129, 52.141, 79.123, -29.301, 88.553}, 448.0869734858976}, // random3dp
        {{61.735, -82.583, -58.011, 53.882, -93.124, 27.855}, 359.5029419059826}, // random3dp
        {{-11.029, 92.702, 93.175, 48.3, 71.764, -52.929}, 412.43120247299737}, // random3dp
        {{-77.407, -38.112, 23.768, 60.335, -29.973, -99.801}, 367.6246443052483}, // random3dp
        {{-47.397, -37.525, 35.192, 67.862, 87.129, 15.677}, 288.12997723286264}, // random3dp
        {{37.643, -37.027, 25.992, 7.895, -67.169, 5.625}, 317.16907224717113}, // random3dp
        {{-75.416, 65.207, -8.143, 21.386, -41.741, 14.433}, 102.81781650354283}, // random3dp
        {{33.23, 85.728, 56.66, 43.205, -22.714, -76.231}, 557.8256482002331}, // random3dp
        {{-88.852, -11.053, -70.72, -62.284, 71.785, 57.004}, 529.5906591997333}, // random3dp
        {{52.328, 76.81, 95.024, 90.495, -44.077, -61.15}, 1021.8906742989312}, // random3dp
        {{-20.368, 42.982, 69.106, 11.726, 9.551, 86.185}, 184.01838400388385}, // random3dp
        {{20.91, 20.839, -49.67, -12.823, 62.006, -12.618}, 303.32891249943896}, // random3dp
        {{-80.979, -19.406, 23.135, -95.506, -52.665, 45.01}, 364.9206030158011}, // random3dp
        {{-94.409, 94.747, -0.881, 29.268, 7.38, 24.233}, 6005.333375527005}, // random3dp
        {{36.958, -99.803, 89.091, 18.965, -18.492, 53.289}, 364.55918658515066}, // random3dp
        {{-1.399, 50.932, -10.537, 36.823, -89.702, -51.708}, 2739.685604239575}, // random3dp
        {{37.372, -82.324, 39.388, 77.651, -40.482, 66.603}, 325.40956124895104}, // random3dp
        {{-63.273, 75.767, -66.95, -86.944, -56.366, -3.941}, 163.99171764377573}, // random3dp
        {{82.419, 8.52, -16.496, -3.684, 17.936, 82.662}, 229.32078001756742}, // random3dp
        {{34.0, 10.0, 38.0, 21.0, 28.0, 36.0}, 90.9051583001954}, // intgrid
        {{21.0, 3.0, 46.0, 48.0, 48.0, 3.0}, 83.06943309043564}, // intgrid
        {{37.0, 40.0, 45.0, 21.0, 39.0, 14.0}, 67.40444627473235}, // intgrid
        {{9.0, 23.0, 29.0, 19.0, 19.0, 6.0}, 42.55119575649145}, // intgrid
        {{2.0, 15.0, 34.0, 32.0, 46.0, 35.0}, 586.5708778021209}, // intgrid
        {{40.0, 29.0, 4.0, 28.0, 21.0, 44.0}, 51.64910739007931}, // intgrid
        {{4.0, 50.0, 44.0, 33.0, 18.0, 15.0}, 79.90261441413679}, // intgrid
        {{32.0, 10.0, 5.0, 25.0, 46.0, 22.0}, 102.80424776450847}, // intgrid
        {{2.0, 5.0, 15.0, 10.0, 22.0, 2.0}, 51.720945925007115}, // intgrid
        {{14.0, 13.0, 47.0, 39.0, 20.0, 3.0}, 88.34113427235512}, // intgrid
        {{46.0, 8.0, 13.0, 9.0, 28.0, 29.0}, 60.296580772614}, // intgrid
        {{35.0, 6.0, 13.0, 39.0, 18.0, 13.0}, 46.87422852505101}, // intgrid
        {{6.0, 6.0, 33.0, 17.0, 48.0, 24.0}, 2825.5759739463524}, // intgrid
        {{25.0, 9.0, 23.0, 49.0, 22.0, 31.0}, 40.29075808444451}, // intgrid
        {{12.0, 35.0, 44.0, 20.0, 6.0, 8.0}, 85.27678963712532}, // intgrid
        {{50.0, 15.0, 28.0, 26.0, 36.0, 40.0}, 60.157910114974406}, // intgrid
        {{50.0, 45.0, 37.0, 9.0, 37.0, 26.0}, 40.634385247519354}, // intgrid
        {{42.0, 7.0, 18.0, 5.0, 19.0, 7.0}, 29.64200552418467}, // intgrid
        {{28.0, 31.0, 30.0, 23.0, 15.0, 13.0}, 64.60340006185926}, // intgrid
        {{22.0, 44.0, 1.0, 11.0, 0.0, 6.0}, 335.2841322094963}, // intgrid
        {{27.0, 40.0, 29.0, 48.0, 3.0, 38.0}, 83.29607436339406}, // intgrid
        {{22.0, 41.0, 33.0, 10.0, 23.0, 33.0}, 33.3331812847254}, // intgrid
        {{46.0, 22.0, 9.0, 9.0, 22.0, 34.0}, 60.78479091338033}, // intgrid
        {{2.0, 30.0, 46.0, 50.0, 43.0, 16.0}, 90.00654520932156}, // intgrid
        {{48.0, 44.0, 4.0, 43.0, 2.0, 33.0}, 96.53845148896117}, // intgrid
        {{8.0, 42.0, 5.0, 31.0, 49.0, 33.0}, 126.61655615587021}, // intgrid
        {{3.0, 15.0, 10.0, 26.0, 10.0, 7.0}, 47.24083053117242}, // intgrid
        {{26.0, 8.0, 19.0, 45.0, 10.0, 28.0}, 47.54639245402468}, // intgrid
        {{7.0, 1.0, 42.0, 19.0, 11.0, 31.0}, 72.9802912327136}, // intgrid
        {{5.0, 49.0, 11.0, 17.0, 34.0, 28.0}, 76.33988658097866}, // intgrid
        {{38.0, 37.0, 44.0, 23.0, 21.0, 43.0}, 113.11260624011707}, // intgrid
        {{27.0, 30.0, 26.0, 12.0, 47.0, 8.0}, 74.35562858059646}, // intgrid
        {{33.0, 42.0, 1.0, 7.0, 39.0, 47.0}, 1220.9311265535969}, // intgrid
        {{37.0, 31.0, 28.0, 6.0, 14.0, 42.0}, 92.30909821307792}, // intgrid
        {{50.0, 25.0, 46.0, 14.0, 36.0, 44.0}, 106.4289176557752}, // intgrid
        {{9.0, 13.0, 43.0, 16.0, 49.0, 28.0}, 118.90334110396577}, // intgrid
        {{-74.47, -6.96, -30.98, 63.32, 0.0, 0.0, 49.91}, 147.84004589175845}, // offset7
        {{70.26, 99.6, 71.63, -55.19, 0.0, 0.0, 28.52}, 168.43697136021848}, // offset7
        {{-62.58, -81.07, 60.09, -92.19, 0.0, 0.0, 42.05}, 158.41789158009502}, // offset7
        {{12.57, 72.64, 8.48, -47.39, 0.0, 0.0, -50.27}, 170.0212335558541}, // offset7
        {{-9.21, 62.1, 82.7, 23.96, 0.0, 0.0, -9.59}, 101.94972287620715}, // offset7
        {{9.02, 97.0, 33.22, -74.18, 0.0, 0.0, -55.34}, 216.78218126459493}, // offset7
        {{-20.85, 36.94, -54.87, 19.73, 0.0, 0.0, -56.11}, 155.4356132530547}, // offset7
        {{-68.09, 87.1, -41.24, 13.59, 0.0, 0.0, -31.33}, 108.22807461386932}, // offset7
        {{57.08, -1.83, 38.19, 20.28, 0.0, 0.0, -28.05}, 77.4877365255099}, // offset7
        {{23.11, 49.96, -43.89, 61.92, 0.0, 0.0, -17.19}, 79.05789793958404}, // offset7
        {{9.0, 25.0, 41.0, 3.0, 4.0, 25.0}, 194.46729111811797}, // fixed
        {{27.0, 26.0, 4.0, 15.0, 27.0, 27.0}, 57.85621286019014}, // fixed
        {{36.0, 7.0, 14.0, 40.0, 3.0, 40.0}, 131.14882168051852}, // fixed
        {{25.0, 3.0, 14.0, 2.0, 35.0, 8.0}, 173.17793968330537}, // fixed
        {{37.0, 36.0, 40.0, 12.0, 37.0, 35.0}, 24.252832255323824}, // fixed
        {{0.0, 0.0, 10.0, 10.0, 10.0, 0.0}, 22.028448589906333}, // fixed
        {{9.0, 34.0, 7.0, 36.0, 9.0, 43.0}, 28.66419650095284}, // fixed
        {{0.0, 0.0, 10.0, 0.0, 5.0, 5.0}, 15.576465376942007}, // fixed
        {{0.0, 0.0, 10.0, 0.0, 5.0, 5.0, 3.0}, 12.18336592166496}, // fixed
        {{0.0, 0.0, 100.0, 0.0, 50.0, 30.0}, 122.45980862598586}, // fixed
        {{1.0, 0.0, 0.0, 1.0, 1.0, 1.0}, 2.196152422706632}, // fixed
        {{2.806, -0.005, 3.249, -0.853, -4.437, 9.953}, 275.4636365546571}, // fixed
        {{0.0, 0.0, 60.0, 80.0, 60.0, 80.0}, 100.0}, // fixed
        {{0.0, 0.0, 60.0, 80.0, 0.0, 0.0}, 100.0}, // fixed
        {{0.0, 0.0, 100.0, 50.0, 100.0, 50.0}, 111.8033988749895}, // fixed
        {{0.0, 0.0, 60.0, 80.0, 1e-12, -1e-12}, 100.0}, // fixed
        {{5.0, 5.0, 5.0, 5.0, 5.0, 5.0}, 0.0}, // fixed
    };
    CHECK_EQ(cases.size(), static_cast<size_t>(87));
    int index = 0;
    for (const C &c : cases)
    {
        SCOPED_CONTEXT("case " + std::to_string(index++));
        CHECK_REL(arc_len(c.pos), c.windows, 1e-9);
    }
}

// ============================================================================================
// QTY-09: a control point at an end point measures the straight chord
// ============================================================================================

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_a_control_point_equal_to_an_end_point_is_a_straight_chord
TEST(Qty09_StraightChord, AControlPointEqualToAnEndPointIsAStraightChord)
{
    for (int rotation = 0; rotation < 360; rotation += 15)
    {
        for (double chord : {0.5, 100.0, 5000.0})
        {
            double angle = radians(rotation);
            Pt p1 = {500.0, -300.0};
            Pt p2 = {p1.first + chord * std::cos(angle), p1.second + chord * std::sin(angle)};
            for (int which = 0; which < 2; ++which)
            {
                Pt control = which == 0 ? p1 : p2;
                SCOPED_CONTEXT("rotation " + std::to_string(rotation) + " chord " + std::to_string(chord) +
                               (which == 0 ? " control=start" : " control=end"));
                double length = arc_len(arc_position(p1, p2, control));
                CHECK_NEAR(length, chord, chord * 1e-9);
                int segments = calc_curve_segments(p1.first, p1.second, p2.first, p2.second, control.first, control.second);
                auto points = advanced_curve_points(p1.first, p1.second, p2.first, p2.second, control.first,
                                                    control.second, segments);
                CHECK_EQ(points.size(), static_cast<size_t>(segments) + 1);
                if (points.size() == static_cast<size_t>(segments) + 1)
                {
                    CHECK(points.front() == p1);
                    CHECK(points.back() == p2);
                    for (size_t i = 0; i < points.size(); ++i)
                    {
                        double t = static_cast<double>(i) / segments;
                        CHECK_NEAR(points[i].first, p1.first + t * (p2.first - p1.first), chord * 1e-9);
                        CHECK_NEAR(points[i].second, p1.second + t * (p2.second - p1.second), chord * 1e-9);
                    }
                }
            }
        }
    }
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_a_coincident_control_is_a_chord_through_the_seven_value_and_closed_loop_edges
// (and the QTY-09 plan: iOS gave 115.87 for the first two).
TEST(Qty09_StraightChord, ACoincidentControlIsAChordThroughTheClosedLoopEdges)
{
    CHECK_REL(arc_len({0, 0, 60, 80, 60, 80}), 100.0, 1e-12);
    CHECK_REL(arc_len({0, 0, 60, 80, 0, 0}), 100.0, 1e-12);
    // Start == end == control: a zero-length chord, no failure.
    CHECK_EQ(arc_len({5, 5, 5, 5, 5, 5}), 0.0);
    // Windows values: [0,0,100,50,100,50] -> 111.8033988749895 (iOS 115.90).
    CHECK_REL(arc_len({0, 0, 100, 50, 100, 50}), 111.8033988749895, 1e-12);
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_a_control_point_within_a_tiny_relative_epsilon_of_an_end_is_still_that_end
TEST(Qty09_StraightChord, AControlPointWithinATinyRelativeEpsilonOfAnEndIsStillThatEnd)
{
    const double chord = 100.0;
    for (double offset : {0.0, 1e-12, 1e-10})
    {
        SCOPED_CONTEXT("offset " + std::to_string(offset));
        CHECK_NEAR(arc_len({0, 0, 60, 80, offset, -offset}), chord, 1e-6);
        CHECK_NEAR(arc_len({0, 0, 60, 80, 60 + offset, 80 - offset}), chord, 1e-6);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_the_end_point_epsilon_is_relative_to_the_chord_length
TEST(Qty09_StraightChord, TheEndPointEpsilonIsRelativeToTheChordLength)
{
    // 1e-4 away from an end of a 1e6 chord is 1e-10 of it (inside the 1e-9 epsilon): the points are the
    // evenly spaced straight chord, not the quadratic fallback.
    Pt p1 = {0.0, 0.0}, p2 = {6e5, 8e5};
    for (Pt control : {Pt{1e-4, 0.0}, Pt{6e5 - 1e-4, 8e5}})
    {
        SCOPED_CONTEXT("control " + std::to_string(control.first) + "," + std::to_string(control.second));
        int segments = calc_curve_segments(p1.first, p1.second, p2.first, p2.second, control.first, control.second);
        auto points = advanced_curve_points(p1.first, p1.second, p2.first, p2.second, control.first, control.second, segments);
        REQUIRE_EQ(points.size(), static_cast<size_t>(segments) + 1);
        for (size_t i = 0; i < points.size(); ++i)
        {
            CHECK_NEAR(points[i].first, p2.first * static_cast<double>(i) / segments, 1e-3);
            CHECK_NEAR(points[i].second, p2.second * static_cast<double>(i) / segments, 1e-3);
        }
    }
}

// Port of tests/domain/services/test_uom_service.py::UomWaveThreeArcOracleTests::test_the_straight_chord_epsilon_scales_with_the_chord_when_the_ends_cancel
TEST(Qty09_StraightChord, TheStraightChordEpsilonScalesWithTheChordWhenTheEndsCancel)
{
    struct C
    {
        Pt p1, p2;
        std::vector<Pt> controls;
    };
    const std::vector<C> cases = {
        {{-6e5, 0.0}, {6e5, 0.0}, {{-6e5 + 1e-4, 0.0}, {6e5 - 1e-4, 0.0}}},
        {{0.0, -6e5}, {0.0, 6e5}, {{0.0, -6e5 + 1e-4}, {0.0, 6e5 - 1e-4}}},
    };
    for (const C &c : cases)
    {
        for (Pt control : c.controls)
        {
            SCOPED_CONTEXT("control " + std::to_string(control.first) + "," + std::to_string(control.second));
            int segments = calc_curve_segments(c.p1.first, c.p1.second, c.p2.first, c.p2.second, control.first, control.second);
            auto points = advanced_curve_points(c.p1.first, c.p1.second, c.p2.first, c.p2.second, control.first,
                                                control.second, segments);
            REQUIRE_EQ(points.size(), static_cast<size_t>(segments) + 1);
            for (size_t i = 0; i < points.size(); ++i)
            {
                double t = static_cast<double>(i) / segments;
                CHECK_NEAR(points[i].first, c.p1.first + t * (c.p2.first - c.p1.first), 1e-3);
                CHECK_NEAR(points[i].second, c.p1.second + t * (c.p2.second - c.p1.second), 1e-3);
            }
        }
    }
}

// Port of tests/domain/services/test_uom_service.py::UomWaveThreeReworkTests::test_the_straight_chord_band_scales_with_the_chord_not_with_the_coordinates
// and ::UomWaveThreeArcOracleTests::test_a_control_point_a_little_off_a_far_away_short_chord_is_an_arc
TEST(Qty09_StraightChord, TheStraightChordBandScalesWithTheChordNotWithTheCoordinates)
{
    const double far = 1e6;
    // A 100 long chord (60, 80) far from the origin, control 5e-4 from its start: 5e-6 of the chord,
    // outside the 1e-9 band, so it is a real arc and not the straight chord.
    for (Pt origin : {Pt{far, far}, Pt{far, 0.0}, Pt{0.0, far}})
    {
        SCOPED_CONTEXT("origin " + std::to_string(origin.first) + "," + std::to_string(origin.second));
        Pt p1 = origin, p2 = {origin.first + 60.0, origin.second + 80.0}, control = {origin.first + 5e-4, origin.second};
        double expected = circumcircle_arc_length({0.0, 0.0}, {5e-4, 0.0}, {60.0, 80.0});
        double length = arc_len(arc_position(p1, p2, control));
        CHECK(length > 110.0);
        CHECK_NEAR(length, expected, expected * 0.01);
    }
    // control 1e-3 from an end of a 100 chord located at 1e6 is a real arc too
    {
        Pt p1 = {far, far}, p2 = {far + 60.0, far + 80.0}, control = {far + 1e-3, far};
        double expected = circumcircle_arc_length({0.0, 0.0}, {1e-3, 0.0}, {60.0, 80.0});
        CHECK(expected > 110.0);
        CHECK_NEAR(arc_len(arc_position(p1, p2, control)), expected, expected * 0.01);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_collinear_controls_keep_the_quadratic_fallback
TEST(Qty09_StraightChord, CollinearControlsKeepTheQuadraticFallback)
{
    struct C
    {
        Pos pos;
        double expected;
    };
    const std::vector<C> cases = {
        {{0, 0, 100, 0, 50, 0}, 100.0},
        {{0, 0, 100, 0, 150, 0}, 124.943},
        {{0, 0, 100, 0, -50, 0}, 124.943},
        {{0, 0, 0, 100, 0, 50}, 100.0},
    };
    for (const C &c : cases)
    {
        SCOPED_CONTEXT("position[4] " + std::to_string(c.pos[4]));
        CHECK_NEAR(arc_len(c.pos), c.expected, 5e-3); // places=2
    }
}

// ============================================================================================
// Regression ports (no divergence expected) of the remaining curve tests.
// ============================================================================================

// Port of tests/domain/services/test_uom_service.py::UomLinearQuantityTests::test_curve_helpers_agree_with_closed_forms
// (quadratic points, curved position, segment counts).
TEST(Qty00_CurveRegression, CurveHelpersAgreeWithClosedForms)
{
    auto points = quadratic_curve_points(0, 0, 10, 0, 5, 10, 2);
    CHECK(points == std::vector<Pt>({{0.0, 0.0}, {5.0, 5.0}, {10.0, 0.0}}));
    auto same = process_curved_position({1, 2, 3, 4, 5, 6});
    CHECK((same == std::array<double, 6>({1, 2, 3, 4, 5, 6})));
    auto p = process_curved_position({0, 0, 10, 0, 99, 99, -4});
    CHECK_EQ(p[0], 0.0);
    CHECK_EQ(p[1], 0.0);
    CHECK_EQ(p[2], 10.0);
    CHECK_EQ(p[3], 0.0);
    CHECK_NEAR(p[4], 5.0, TOL7);
    CHECK_NEAR(p[5], 4.0, TOL7);
    auto degenerate = process_curved_position({2, 2, 2, 2, 7, 8, -4});
    CHECK((degenerate == std::array<double, 6>({2, 2, 2, 2, 7, 8})));
    CHECK_EQ(calc_curve_segments(0, 0, 10, 0, 5, 0), 6);
    CHECK_EQ(calc_curve_segments(0, 0, 1000, 0, 500, 0), 64);
    CHECK_EQ(calc_curve_segments(0, 0, 100, 0, 50, 0), 20);
    CHECK_EQ(calc_curve_segments(0, 0, 100, 0, 50, 50), 32);
    CHECK_EQ(calc_curve_segments(5, 5, 5, 5, 5, 5), 6);
    CHECK_EQ(calc_curve_segments(5, 5, 5, 5, 5, 105), 32);
}

// Port of tests/domain/services/test_uom_service.py::UomCurvedArcTests (segment counts) and
// UomSecondReviewGapTests::test_a_curve_with_a_control_offset_uses_more_than_the_minimum_segments.
TEST(Qty00_CurveRegression, SegmentCountAddsTheControlPointOffsetFromTheChordClampedToTheChord)
{
    CHECK_EQ(calc_curve_segments(0, 0, 60, 80, 10, 50), 24);
    CHECK_EQ(calc_curve_segments(0, 0, 100, 0, 150, 30), 44);
    CHECK_EQ(calc_curve_segments(0, 0, 100, 0, -50, 30), 44);
    CHECK_EQ(calc_curve_segments(0, 0, 100, 0, 50, 30), 26);
    CHECK_EQ(calc_curve_segments(0, 0, 100, 0, 50, 5), 21);
    CHECK_EQ(calc_curve_segments(0, 0, 100, 0, 50, 4.9), 20);
    CHECK_EQ(calc_curve_segments(0, 0, 10, 0, 5, 5), 7);
    CHECK_EQ(calc_curve_segments(0, 0, 0.5, 0, 0.25, 50), 26);
    CHECK_EQ(calc_curve_segments(0, 0, 1000, 0, 500, 500), 64);
    CHECK_EQ(calc_curve_segments(0, 0, 100, 0, 50, 1000), 64);
    CHECK_EQ(calc_curve_segments(0, 0, 30, 40, 30, 0), 15);
    CHECK_EQ(calc_curve_segments(0, 0, 120, 50, 20, 90), 46);
}

// Windows _calc_curve_segments / _calc_linear_length (curve=0) on chords far beyond the int range: Python
// integers are unbounded and the count is clamped to [6, 64], so the count is 64 (a plain cast of the double to
// int gave 6 on x86-64, and an arc measured with 6 segments is 0.5 - 1.6% short).  Values computed by running
// the Windows functions on these positions.
TEST(Qty00_CurveRegression, SegmentCountSaturatesForChordsBeyondTheIntRange)
{
    CHECK_EQ(calc_curve_segments(0, 0, 1e11, 0, 5e10, 3e10), 64);
    CHECK_EQ(calc_curve_segments(0, 0, 10, 0, 5, 1e11), 64);
    CHECK_EQ(calc_curve_segments(0, 0, 1e12, 1e12, 0, 1e12), 64);
    CHECK_EQ(calc_curve_segments(0, 0, 1e300, 0, 5e299, 3e299), 64);
    CHECK_REL(arc_len({0, 0, 1e11, 0, 5e10, 3e10}), 122489264043.04756, 1e-9);
    CHECK_REL(arc_len({0, 0, 3e10, 4e10, 0, 5e10}), 99731442265.42494, 1e-9);
    CHECK_REL(arc_len({0, 0, 1e12, 1e12, 0, 1e12}), 2221218445721.6, 1e-9);
    CHECK_REL(arc_len({0, 0, 1e300, 0, 5e299, 3e299}), 1.2248926404304747e+300, 1e-9);
}

// Port of tests/domain/services/test_uom_service.py::UomCurvedArcTests::test_a_seventh_value_also_works_for_tilted_chords_and_short_chords
TEST(Qty00_CurveRegression, ASeventhValueWorksForTiltedChordsAndShortChords)
{
    auto p = process_curved_position({1, 2, 7, 10, 0, 0, 5});
    CHECK_EQ(p[0], 1.0);
    CHECK_EQ(p[1], 2.0);
    CHECK_EQ(p[2], 7.0);
    CHECK_EQ(p[3], 10.0);
    CHECK_NEAR(p[4], 8.0, TOL7);
    CHECK_NEAR(p[5], 3.0, TOL7);
    p = process_curved_position({0, 0, 0, 10, 99, 99, -4});
    CHECK_NEAR(p[4], -4.0, TOL7);
    CHECK_NEAR(p[5], 5.0, TOL7);
    p = process_curved_position({0, 0, 0.5, 0, 99, 99, -2});
    CHECK_NEAR(p[4], 0.25, TOL7);
    CHECK_NEAR(p[5], 2.0, TOL7);
}

// Port of tests/domain/services/test_uom_service.py::UomLinearQuantityTests (curve cases) and
// UomSecondReviewGapTests (seventh value, closed-loop edge, coincident end points).
TEST(Qty00_CurveRegression, CurveFlagControlPointAndSeventhValueBehavior)
{
    // collinear points measure the straight distance
    CHECK_NEAR(arc_len({0, 0, 100, 0, 50, 0}), 100.0, 5e-7);
    CHECK_NEAR(arc_len({0, 0, 0, 100, 0, 50}), 100.0, 5e-7);
    // a circle quarter and its reverse, a half circle through the top point and below
    const double radius = 100.0;
    Pos quarter = {radius, 0.0, 0.0, radius, radius * std::cos(M_PI / 4), radius * std::sin(M_PI / 4)};
    CHECK_NEAR(arc_len(quarter), radius * M_PI / 2, 0.05);
    Pos reversed_quarter = {0.0, radius, radius, 0.0, quarter[4], quarter[5]};
    CHECK_NEAR(arc_len(reversed_quarter), radius * M_PI / 2, 0.05);
    CHECK_NEAR(arc_len({radius, 0.0, -radius, 0.0, 0.0, radius}), radius * M_PI, 0.1);
    CHECK_NEAR(arc_len({radius, 0.0, -radius, 0.0, 0.0, -radius}), radius * M_PI, 0.1);
    // seventh value places the arc by signed offset from the chord
    CHECK_NEAR(arc_len({0, 0, 100, 0, 0, 0, -50}), 50 * M_PI, 0.1);
    CHECK_NEAR(arc_len({0, 0, 100, 0, 0, 0, 50}), 50 * M_PI, 0.1);
    CHECK_NEAR(arc_len({0, 0, 100, 0, 0, 0, 0}), 100.0, 5e-7);
    CHECK_NEAR(arc_len({5, 5, 5, 5, 5, 5, 20}), 0.0, 5e-10);
    CHECK_NEAR(arc_len({0, 0, 100, 0, 0, 0, -50, 1, 1, 2, 2}), 50 * M_PI, 0.1);
    // curve needs a control point and a non-negative curve flag
    CHECK_EQ(calc_linear_length({0, 0, 100, 0, 50, 40}, -1), 100.0);
    CHECK_EQ(calc_linear_length({0, 0, 100, 0, 50}, 0), 100.0);
    double bent = calc_linear_length({0, 0, 100, 0, 50, 40}, 0);
    CHECK(bent > 100.0);
    CHECK_EQ(calc_linear_length({0, 0, 100, 0, 50, 40}, 3), bent);
    // collinear control points fall back to the quadratic and measure its path
    CHECK_NEAR(arc_len({0, 0, 100, 0, 150, 0}), 125.0, 0.1);
    CHECK_NEAR(arc_len({0, 0, 0, 100, 0, 150}), 125.0, 0.1);
    CHECK_NEAR(arc_len({0, 0, 60, 80, 90, 120}), 125.0, 0.1);
    // an off-chord control point measures the circle through all three points
    // (0,0), (50,40), (100,0): radius 51.25 centred 11.25 below the chord
    CHECK_NEAR(arc_len({0, 0, 100, 0, 50, 40}), 51.25 * (2 * std::atan2(50.0, 11.25)), 0.3);
    // coincident end points with a far control point: out and back, 5 + 5
    CHECK_NEAR(arc_len({-5, 0, -5, 0, 5, 0}), 10.0, 0.5);
}

// Port of tests/domain/services/test_uom_service.py::UomTranslatedArcTests::test_arc_lengths_do_not_depend_on_where_the_circle_is
// and ::test_arc_points_start_and_end_on_the_chord_ends_pass_the_control_point_and_stay_on_the_circle
TEST(Qty00_CurveRegression, ArcLengthsAndPointsDoNotDependOnWhereTheCircleIs)
{
    const double radius = 100.0;
    const std::array<Pt, 3> centers = {{{0.0, 0.0}, {300.0, -200.0}, {-1234.5, 777.25}}};
    const std::array<std::array<double, 3>, 11> cases = {{
        {0, 45, 90}, {90, 45, 0}, {0, 90, 180}, {180, 270, 0}, {30, 120, 250}, {250, 120, 30},
        {135, 45, 270}, {270, 135, 45}, {45, 315, 135}, {10, 20, 30}, {200, 100, 20},
    }};
    for (const Pt &center : centers)
    {
        for (const auto &c : cases)
        {
            SCOPED_CONTEXT("center " + std::to_string(center.first) + "," + std::to_string(center.second) + " arc " +
                           std::to_string(c[0]) + "," + std::to_string(c[1]) + "," + std::to_string(c[2]));
            Pt p1 = circle_point(radius, c[0], center), pc = circle_point(radius, c[1], center),
               p2 = circle_point(radius, c[2], center);
            double expected = arc_length_oracle(radius, c[0], c[1], c[2]);
            CHECK_NEAR(arc_len(arc_position(p1, p2, pc)), expected, expected * 0.004);

            int segments = calc_curve_segments(p1.first, p1.second, p2.first, p2.second, pc.first, pc.second);
            auto points = advanced_curve_points(p1.first, p1.second, p2.first, p2.second, pc.first, pc.second, segments);
            REQUIRE_EQ(points.size(), static_cast<size_t>(segments) + 1);
            CHECK_NEAR(points.front().first, p1.first, 1e-6);
            CHECK_NEAR(points.front().second, p1.second, 1e-6);
            CHECK_NEAR(points.back().first, p2.first, 1e-6);
            CHECK_NEAR(points.back().second, p2.second, 1e-6);
            double nearest = std::numeric_limits<double>::infinity();
            for (const Pt &q : points)
            {
                CHECK_NEAR(std::hypot(q.first - center.first, q.second - center.second), radius, 1e-6);
                nearest = std::min(nearest, std::hypot(q.first - pc.first, q.second - pc.second));
            }
            double sweep = expected / radius;
            double step = radius * sweep / segments;
            CHECK(nearest <= step);
        }
    }
}

// Port of tests/domain/services/test_uom_service.py::UomTranslatedArcTests::test_translating_a_curve_does_not_change_its_segment_count
// and ::test_collinear_curves_are_unaffected_by_translation.
TEST(Qty00_CurveRegression, TranslatingACurveDoesNotChangeItsSegmentCountOrCollinearLength)
{
    const std::vector<std::array<double, 6>> cases = {
        {0, 0, 60, 80, 10, 50}, {0, 0, 100, 0, 150, 30}, {0, 0, 100, 0, -50, 30}, {0, 0, 100, 0, 50, 30},
        {0, 0, 100, 0, 150, 0}, {0, 0, 0.5, 0, 0.25, 50}, {0, 0, 1000, 0, 500, 500}, {0, 0, 30, 40, 30, 0},
        {0, 0, 120, 50, 20, 90},
    };
    for (const auto &c : cases)
    {
        int base = calc_curve_segments(c[0], c[1], c[2], c[3], c[4], c[5]);
        for (Pt d : {Pt{1000.0, -500.0}, Pt{-37.5, 12.25}})
        {
            SCOPED_CONTEXT("shift " + std::to_string(d.first) + "," + std::to_string(d.second) + " base " + std::to_string(base));
            CHECK_EQ(calc_curve_segments(c[0] + d.first, c[1] + d.second, c[2] + d.first, c[3] + d.second,
                                         c[4] + d.first, c[5] + d.second),
                     base);
        }
    }
    for (Pt d : {Pt{0.0, 0.0}, Pt{500.0, 300.0}, Pt{-77.0, 123.0}})
    {
        SCOPED_CONTEXT("shift " + std::to_string(d.first) + "," + std::to_string(d.second));
        CHECK_NEAR(arc_len({d.first, d.second, 100 + d.first, d.second, 150 + d.first, d.second}), 125.0, 0.1);
        CHECK_NEAR(arc_len({d.first, d.second, 60 + d.first, 80 + d.second, 90 + d.first, 120 + d.second}), 125.0, 0.1);
    }
}

// ============================================================================================
// accurate_hypot (used for every length in the arc code)
// ============================================================================================

// Python's math.hypot (used by Windows for every arc length, radius and segment count) is accurate to
// the last bit; std::hypot is not (it differs from it in about one call in six).  The arc guard
// |radius1 - radius2| < 0.1 on a huge circle flips on that last bit, so the C++ port must round like
// math.hypot.  Windows math.hypot(x, y) for 129 operand pairs (hexadecimal floating literals are exact),
// including zero, denormal and huge operands.
TEST(Qty08_ArcSweep, AccurateHypotMatchesPythonMathHypot)
{
    struct H
    {
        double x, y, windows;
    };
    const std::vector<H> pairs = {
        {0x1.8000000000000p+1, 0x1.0000000000000p+2, 0x1.4000000000000p+2},
        {0x1.e000000000000p+5, 0x1.4000000000000p+6, 0x1.9000000000000p+6},
        {0x1.4000000000000p+2, 0x1.8000000000000p+3, 0x1.a000000000000p+3},
        {0x1.56e1fc2f8f359p-997, 0x1.56e1fc2f8f359p-997, 0x1.e4e8d12762225p-997},
        {0x1.7e43c8800759cp+996, 0x1.7e43c8800759cp+996, 0x1.0e4d50f99b211p+997},
        {0x0.0p+0, 0x0.0p+0, 0x0.0p+0},
        {0x0.0p+0, 0x1.4000000000000p+2, 0x1.4000000000000p+2},
        {0x1.4000000000000p+2, 0x0.0p+0, 0x1.4000000000000p+2},
        {0x1.0000000000000p+0, 0x1.0000000000000p+0, 0x1.6a09e667f3bcdp+0},
        {-0x1.8000000000000p+1, 0x1.0000000000000p+2, 0x1.4000000000000p+2},
        {0x0.00000000007e8p-1022, 0x0.00000000007e8p-1022, 0x0.0000000000b2ep-1022},
        {0x0.0000000000001p-1022, 0x0.0p+0, 0x0.0000000000001p-1022},
        {0x1.421f5f40d8376p-23, 0x1.036847569cd7ap-51, 0x1.421f5f40d8376p-23},
        {-0x1.4e7f3dbdfae4dp+49, 0x1.7b456906ce11fp+50, 0x1.9e8298d0a90e7p+50},
        {-0x1.4e7f3dbd7148dp+49, 0x1.7b456906ec693p+50, 0x1.9e8298d0a90e6p+50},
        {-0x1.4e7f3dbddc3e3p+49, 0x1.7b456906d4d40p+50, 0x1.9e8298d0a90e7p+50},
        {0x1.864f80310c082p+49, -0x1.7e82ac24964b5p+52, 0x1.819bff8fa8c29p+52},
        {0x1.864f8030a077ep+49, -0x1.7e82ac2498026p+52, 0x1.819bff8fa8c2ap+52},
        {0x1.864f8030e37a8p+49, -0x1.7e82ac2496f0dp+52, 0x1.819bff8fa8c29p+52},
        {0x1.3958fb44f4e0dp-39, 0x1.45e5f1ada85e5p-60, 0x1.3958fb44f50b3p-39},
        {-0x1.b451a4ce01e4ap-25, 0x1.18ecc0333d614p-51, 0x1.b451a4ce01e4ap-25},
        {0x1.34a183cc89fb0p-35, -0x1.f09a4a4f7a729p-48, 0x1.34a184306bdc5p-35},
        {-0x1.f4a086b8342cbp-10, -0x1.fa292509b78fdp-25, 0x1.f4a086bc33af8p-10},
        {-0x1.a574ae68ad0c1p+40, -0x1.366de18db237cp+13, 0x1.a574ae68ad0c1p+40},
        {-0x1.7e2175ca3bf27p-49, -0x1.6e587d6444054p-56, 0x1.7e24343499929p-49},
        {0x1.983ebc28c82f8p-57, 0x1.ebdbabe8ed4a4p-70, 0x1.983ebc72db41dp-57},
        {0x1.d79fcaef3574bp+53, 0x1.89d6c0355a37ap+47, 0x1.d7aa11e264ee0p+53},
        {0x1.58a9aff96aee8p-16, -0x1.a6120226f1ac8p-21, 0x1.58ea4587d336bp-16},
        {-0x1.cf47deb5dcb1bp-15, -0x1.4588c2ddd3987p-11, 0x1.46d1c4b61e93dp-11},
        {-0x1.6ab2a15e988e0p-64, -0x1.3b5eb93d83503p-84, 0x1.6ab2a15e99172p-64},
        {-0x1.1c511a0ac2f25p-36, -0x1.0197352a0c7c3p-52, 0x1.1c511a0b37a29p-36},
        {-0x1.90b22284f5b79p+4, 0x1.f5a81f0efdafep-12, 0x1.90b222862fbecp+4},
        {-0x1.8db3c8bc497f1p+55, -0x1.b55e5f7974ceep+57, 0x1.c086ffc7f080bp+57},
        {0x1.6977077638da3p-1, -0x1.4a6bc4128c796p-25, 0x1.6977077638dacp-1},
        {0x1.6783d3e725eefp-12, 0x1.8501f7ddb64d8p-18, 0x1.6790fb056b449p-12},
        {0x1.5dcff75ac32b3p-21, -0x1.52a645bcb0caap-48, 0x1.5dcff75ac32b3p-21},
        {0x1.5ec17e84de8e2p+48, 0x1.cb79d980ea9b2p+26, 0x1.5ec17e84dea0fp+48},
        {0x1.8acdac60afe34p-61, -0x1.beb6f08c92fddp-73, 0x1.8acdad5d69aacp-61},
        {0x1.5ae3946dc7a21p+6, 0x1.2c7eb3079cf4ap+1, 0x1.5b041caff2771p+6},
        {-0x1.13516008bdd39p+53, -0x1.c14ca494be37ep+41, 0x1.135161775a9e8p+53},
        {0x1.97585a6b1264bp+16, -0x1.7822e7c2b4632p+6, 0x1.975865459ece5p+16},
        {0x1.82648a296859cp-29, -0x1.162d5f7cc9466p-42, 0x1.82648a4270f75p-29},
        {-0x1.99648a8ba41c6p+24, 0x1.91c7e14ea8a12p+11, 0x1.99648abcee0adp+24},
        {0x1.cf32169cc463bp+10, 0x1.17a75872582bap+13, 0x1.1d95a515cca80p+13},
        {0x1.e32a3e5eb1ecap+54, -0x1.61eefd9e0735ep+45, 0x1.e32a5ec73fd1ep+54},
        {0x1.222dc5e501eb2p-10, 0x1.ef97bd82d94cep-25, 0x1.222dc5eb9ec07p-10},
        {-0x1.616bb2e37ab80p-36, -0x1.31a7be9e16de1p-48, 0x1.616bb367a6edcp-36},
        {0x1.b595d9d1c02bdp+63, 0x1.65bf07cdb0d41p+58, 0x1.b5ba67762c8f2p+63},
        {-0x1.52290788b6e04p+8, -0x1.4fd1991d7dbb3p-17, 0x1.52290788b6e07p+8},
        {-0x1.c47eebead0b26p-13, 0x1.8a5aad2bf7e08p-29, 0x1.c47eebeb7c89dp-13},
        {0x1.123aab1287315p+52, 0x1.3c7f61ba331b1p+36, 0x1.123aab133dd55p+52},
        {0x1.77c1e706e382ap-15, 0x1.6c1636736b353p-26, 0x1.77c1e9c87222bp-15},
        {0x1.40aee40b8b25dp+15, 0x1.66fe53d160004p+8, 0x1.40b207ca6bbd9p+15},
        {-0x1.4ce4ca8dbe601p+7, -0x1.3b6d813c663e6p-18, 0x1.4ce4ca8dbe603p+7},
        {0x1.4ce3cfa43979cp+61, 0x1.5c27a5f18632fp+45, 0x1.4ce3cfa4ef891p+61},
        {0x1.f470f6a302192p-54, -0x1.ebb4a424d384dp-69, 0x1.f470f6a6c857dp-54},
        {-0x1.3bb67e5918eadp+22, 0x1.63474247c45a9p+1, 0x1.3bb67e59191cdp+22},
        {-0x1.02ed56089c1fep+54, 0x1.dda96f3a5afb7p+37, 0x1.02ed56090a459p+54},
        {-0x1.cd8db65adb440p-69, 0x1.04cfbf7bf1311p-88, 0x1.cd8db65adc6acp-69},
        {-0x1.2b30a8ee99c88p+47, -0x1.a4b10ec81b8d1p+48, 0x1.be7f640727d7cp+48},
        {0x1.631fee5e2104cp+49, -0x1.ba64a993b7c58p+47, 0x1.73f2c5aab52a6p+49},
        {0x1.50f10d2958a78p+49, 0x1.7b2b734e3c714p+49, 0x1.fb3f2f23c4742p+49},
        {-0x1.34ce8681d00a8p+47, -0x1.367c1c8f9bf18p+49, 0x1.3ff05d9c8c6eep+49},
        {0x1.b8048cc27fca0p+47, 0x1.ad4bad861a36cp+48, 0x1.e26372cd41764p+48},
        {-0x1.6da1ab2eef300p+49, 0x1.58a5814ab1520p+44, 0x1.6dca44a35bb63p+49},
        {-0x1.60db9f77ad118p+49, -0x1.4729a7344f432p+48, 0x1.84ee69dd063c3p+49},
        {-0x1.2d95c1c41dea1p+49, -0x1.a023ee8187af5p+49, 0x1.00f752366c072p+50},
        {-0x1.858f95e472850p+49, 0x1.4329890b4bdb8p+49, 0x1.fa2755a656076p+49},
        {0x1.f66fb9470d00cp+48, -0x1.c6a2bdba24f4cp+47, 0x1.13bbc4073c7ccp+49},
        {-0x1.3f4cfd758904ep+49, 0x1.02e10321f826ap+49, 0x1.9b0fa8292e5b6p+49},
        {0x1.93e915fd4bfb8p+47, -0x1.8574f8340475ep+48, 0x1.b6b4a287a2c40p+48},
        {-0x1.870cb2d64b8cap+49, 0x1.28a516722d57cp+49, 0x1.ead57e3c38413p+49},
        {-0x1.3e29ce415fa6cp+49, -0x1.71dc813462434p+49, 0x1.e7e0dcd0c4751p+49},
        {-0x1.ad934b047d5c4p+49, 0x1.6f24c793708f0p+45, 0x1.ae3012f021230p+49},
        {0x1.47b47ae0ee160p+48, 0x1.13ccd3f510c40p+46, 0x1.4ee14d70e8dd2p+48},
        {0x1.1c7ee6b66eff0p+48, -0x1.67b3139809280p+48, 0x1.ca9bb7fabfa3bp+48},
        {0x1.0152de6a6463cp+49, -0x1.a3a89524be668p+47, 0x1.15e3b5bdc749fp+49},
        {0x1.f252423486ce8p+47, -0x1.229eb212dc6e4p+49, 0x1.3c3249cd14824p+49},
        {-0x1.42b70304096a8p+47, -0x1.012ddad71a695p+49, 0x1.0d8974333e9f0p+49},
        {-0x1.a7b98419331b3p+49, 0x1.5c11e72496c56p+49, 0x1.122dae7cc19c4p+50},
        {-0x1.af7ab125da188p+47, 0x1.ec3afe55e6360p+48, 0x1.0cb771b266b06p+49},
        {0x1.50da1cb14d9a6p+49, 0x1.131906ca1826ap+49, 0x1.b2e95aaec8021p+49},
        {-0x1.c75710b81ef78p+47, -0x1.a94bec484c0fcp+47, 0x1.378880eba7ee3p+48},
        {0x1.2acebfc75564ep+49, -0x1.701ea9140e7ddp+49, 0x1.da20fb677a354p+49},
        {-0x1.208f2e0a175a0p+49, 0x1.0b325a2ee5b84p+49, 0x1.8944dfceb7a81p+49},
        {-0x1.82252afa70460p+47, -0x1.746686697cdcap+48, 0x1.a3798fccf21d3p+48},
        {-0x1.6bb55a54f52f9p+49, -0x1.9e8e887367800p+38, 0x1.6bb55e05fc55bp+49},
        {0x1.1f0ae178be0d0p+48, 0x1.96cef25451c04p+48, 0x1.f1e1c917c998ep+48},
        {-0x1.bf7df9f604914p+47, 0x1.0d748a1ab9d5cp+49, 0x1.23c1980eb52f8p+49},
        {-0x1.523f85686b3bfp+49, -0x1.f735dcef849dcp+47, 0x1.68e2992db0bc1p+49},
        {-0x1.b7fa523697d84p+47, -0x1.82a264d3b82dep+49, 0x1.91f9eb2ce4a6bp+49},
        {0x1.4fc1138f3d116p+49, -0x1.0b699f074e0a8p+49, 0x1.ad3b78a10254ap+49},
        {0x1.37a50f7a64a50p+49, 0x1.593f5d1c16a80p+48, 0x1.6442a4fa40ed2p+49},
        {-0x1.7b228060ef08ep+49, -0x1.4b4ec87f07478p+47, 0x1.84139e7ea6bc9p+49},
        {-0x1.6193bad9bc4d0p+48, 0x1.008dc9d901642p+49, 0x1.37912eb9b02afp+49},
        {0x1.bcbb0512487a8p+48, -0x1.dcd08c129ff68p+47, 0x1.f89a251ac8a68p+48},
        {0x1.85a0e05a7470ep+49, 0x1.c3458be92231cp+49, 0x1.2a19d025b0f14p+50},
        {-0x1.4795b402356e0p+44, -0x1.72b405f34e296p+48, 0x1.7344a763f5c4dp+48},
        {-0x1.bc8340159faafp+49, -0x1.791cad08209ecp+48, 0x1.e2d9cebbe827fp+49},
        {-0x1.449472e643240p+1, 0x1.0087bcf5d7cc0p+5, 0x1.0154c23481bb9p+5},
        {-0x1.cc5f5d3a9f004p+4, 0x1.42909ba683ba0p+3, 0x1.e7ce626ddf494p+4},
        {-0x1.0e37390b51e4ep+5, -0x1.25a89f4e0a44ap+6, 0x1.433f99c0c486ep+6},
        {-0x1.315593c686084p+5, -0x1.07d789147b204p+6, 0x1.30d3d90cf0ddbp+6},
        {0x1.45396e9d5a884p+6, 0x1.72049f8e0d784p+5, 0x1.762a291c903e9p+6},
        {0x1.83f39f2a4a72cp+5, 0x1.5c4111d99b684p+4, 0x1.a93cbcd0a6a6ap+5},
        {0x1.b6f44b45f9074p+4, -0x1.410269db4d640p+3, 0x1.d360e23c4d1bcp+4},
        {-0x1.8d59375f5be50p+4, 0x1.62852db797ec2p+6, 0x1.702cab8ead3a8p+6},
        {-0x1.666164415c178p+4, -0x1.4dfd6bff23336p+6, 0x1.59cc661c79087p+6},
        {-0x1.159a2e8794da4p+5, -0x1.786a3ffc48a2cp+5, 0x1.d3b5649cd0800p+5},
        {-0x1.a5913f6b7bd8bp+5, 0x1.6819abf78cb48p+3, 0x1.af12943525629p+5},
        {-0x1.a1b8dfa0109a0p+4, 0x1.f06d07f175728p+4, 0x1.446551aa2ac90p+5},
        {0x1.a7933cc5a360cp+5, -0x1.627ad63de6b11p+6, 0x1.9cedb0aa68e9bp+6},
        {-0x1.71750e3b1b170p+6, -0x1.81bde92e0a8b0p+6, 0x1.0b10b543e184bp+7},
        {-0x1.d9ed85e62e65fp+5, 0x1.bed700d515070p+5, 0x1.45ae6ecac722ap+6},
        {0x1.b2fff6d9560d0p+5, 0x1.a8d08a4df5650p+5, 0x1.3002fb73309f6p+6},
        {0x1.02c2cd1e67198p+6, 0x1.7a8ec1e59fef8p+6, 0x1.ca8b6883e4ae8p+6},
        {-0x1.8eca62673e010p+3, -0x1.a2b5091a49a00p-2, 0x1.8f015268f3358p+3},
        {0x1.4cc5339173eecp+5, 0x1.b17641c0e2ff8p+5, 0x1.113bad5f1b061p+6},
        {-0x1.78c3101d4d7b4p+5, -0x1.c972f2b11fa44p+4, 0x1.b8c12322e8002p+5},
        {0x1.94a60874a802cp+4, -0x1.d602c50cf4ee1p+5, 0x1.ffb54cd7be428p+5},
        {0x1.0232c806ab70cp+6, -0x1.78e40b8216687p+6, 0x1.c8d9ebfbc36a3p+6},
        {-0x1.6c0eed8c5e264p+4, -0x1.e4f158a178ffap+5, 0x1.02fd8a56785d4p+6},
        {-0x1.b189a16a44d51p+5, -0x1.d73c050c55d05p+5, 0x1.4029aac148e0ep+6},
        {-0x1.918090ca1c910p+3, 0x1.1c28d3b969c20p+6, 0x1.208eb7785feb1p+6},
        {-0x1.3d2139f390b47p+5, 0x1.1544420edaa3ep+6, 0x1.3f67b400e8788p+6},
        {0x1.553162faf39a0p+2, 0x1.620611dc01dbcp+4, 0x1.6c279415ea222p+4},
        {0x1.dc930be8ef0b0p+3, 0x1.4d2752f649c18p+3, 0x1.22bcf1beb2af3p+4},
        {0x1.496991ba4f458p+6, -0x1.4396b41797f02p+5, 0x1.6f00529f2eab8p+6},
        {-0x1.473438ff448a0p+5, 0x1.930241bc80a00p-2, 0x1.473819b9bbbaep+5},
    };
    for (const H &h : pairs)
    {
        SCOPED_CONTEXT("hypot(" + std::to_string(h.x) + ", " + std::to_string(h.y) + ")");
        CHECK_EQ(accurate_hypot(h.x, h.y), h.windows);
        CHECK_EQ(accurate_hypot(h.y, h.x), h.windows);
    }
    CHECK_EQ(accurate_hypot(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()),
             std::numeric_limits<double>::infinity());
    CHECK(std::isnan(accurate_hypot(std::numeric_limits<double>::quiet_NaN(), 1.0)));
}
