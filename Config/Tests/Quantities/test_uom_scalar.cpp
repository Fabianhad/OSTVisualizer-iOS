// Per-takeoff quantity math: rounding (QTY-04), area slope magnitude (QTY-10), polygon area about
// the first vertex (QTY-14), plus regression ports of the per-calc-type tables.
//
// Windows oracle: ost_visualizer/domain/services/uom_service.py (convert_and_round_quantity,
// _calc_slope_factor, calculate_polygon_area, calculate_condition_quantities) and
// tests/domain/services/test_uom_service.py, tests/integration/geometry/test_quantity_pivot_precision.py.
#include "QtyHelpers.hpp"

#include <cmath>
#include <vector>

using namespace qty;

using Pt = std::pair<double, double>;
using Poly = std::vector<Pt>;

// ============================================================================================
// QTY-04: round_quantity / round_up
// ============================================================================================

// Port of tests/domain/services/test_uom_service.py::UomConversionTests::test_rounding_goes_up_to_the_next_increment_expressed_in_the_same_unit
TEST(Qty04_Rounding, RoundingGoesUpToTheNextIncrementExpressedInTheSameUnit)
{
    // 30 in = 2.5 LF; a 12 in increment is 1 LF.
    CHECK_NEAR(convert_and_round_quantity(30.0, UOM_LINEAR_FEET), 2.5, TOL7);
    CHECK_NEAR(convert_and_round_quantity(30.0, UOM_LINEAR_FEET, true, 12.0), 3.0, TOL7);
    CHECK_NEAR(convert_and_round_quantity(24.0, UOM_LINEAR_FEET, true, 12.0), 2.0, TOL7);
    CHECK_NEAR(convert_and_round_quantity(13.0, UOM_INCHES, true, 12.0), 24.0, TOL7);
    CHECK_NEAR(convert_and_round_quantity(13.0, UOM_INCHES, true, 0.5), 13.0, TOL7);
    CHECK_NEAR(convert_and_round_quantity(13.2, UOM_INCHES, true, 0.5), 13.5, TOL7);
}

// Port of tests/domain/services/test_uom_service.py::UomConversionTests::test_rounding_is_skipped_without_the_flag_or_with_a_non_positive_increment
TEST(Qty04_Rounding, RoundingIsSkippedWithoutTheFlagOrWithANonPositiveIncrement)
{
    struct C
    {
        bool flag;
        double increment;
    };
    for (const C &c : {C{false, 12.0}, C{true, 0.0}, C{true, -12.0}, C{false, 0.0}})
    {
        SCOPED_CONTEXT(std::string("flag ") + (c.flag ? "true" : "false") + " increment " + std::to_string(c.increment));
        CHECK_NEAR(convert_and_round_quantity(13.0, UOM_INCHES, c.flag, c.increment), 13.0, TOL7);
    }
    CHECK_NEAR(convert_and_round_quantity(13.0, UOM_INCHES, false, 12.0), 13.0, TOL7); // round_up only
    CHECK_NEAR(convert_and_round_quantity(13.5, UOM_INCHES, true), 13.5, TOL7);        // round_quantity only
}

// Port of tests/domain/services/test_uom_service.py::UomRoundingDefaultsTests::test_an_increment_that_underflows_to_zero_in_the_target_unit_is_ignored
TEST(Qty04_Rounding, AnIncrementThatUnderflowsToZeroInTheTargetUnitIsIgnored)
{
    // 5e-324 inches divided by 14400 square inches per square is 0.0: no division by a zero increment.
    CHECK_NEAR(convert_and_round_quantity(14400.0, UOM_SQUARE_ROOFING, true, 5e-324), 1.0, TOL7);
}

// Port of tests/domain/services/test_uom_service.py::UomRoundingDefaultsTests::test_an_increment_without_the_rounding_flag_never_rounds_but_with_it_rounds_up
// and UomSecondReviewGapTests::test_rounding_needs_an_increment_as_well_as_the_flag.
TEST(Qty04_Rounding, CalculateConditionQuantitiesAppliesTheRoundingFlagAndIncrement)
{
    QArgs a;
    a.position = {0, 0, 13.5, 0};
    a.round_up = 12.0;
    CHECK_EQ(quantities(ConditionType::Linear, 1, 1, a).q1, 13.5); // increment without the flag
    a.round_quantity = true;
    CHECK_EQ(quantities(ConditionType::Linear, 1, 1, a).q1, 24.0);
    a.round_quantity = false;
    CHECK_EQ(quantities(ConditionType::Linear, 1, 1, a).q1, 13.5);
    a.round_quantity = true;
    a.round_up = 0.0;
    CHECK_EQ(quantities(ConditionType::Linear, 1, 1, a).q1, 13.5); // flag without an increment
}

// Port of tests/domain/services/test_uom_service.py::UomAreaQuantityTests::test_area_converts_to_the_requested_unit_and_rounds_up
TEST(Qty04_Rounding, AreaConvertsToTheRequestedUnitAndRoundsUp)
{
    QArgs a;
    a.position = SQUARE_12;
    a.holes = {HOLE_6};
    a.thickness = 2.0;
    a.attachment_footprint = 10.0;
    a.attachment_perimeter = 14.0;
    a.grid_size1 = 3.0;
    a.grid_size2 = 4.0;
    a.gap = 1.0;
    CHECK_NEAR(quantities(ConditionType::Area, 11, UOM_SQUARE_FEET, a).q1, 108.0 / 144, TOL7);
    CHECK_NEAR(quantities(ConditionType::Area, 11, UOM_SQUARE_YARDS, a).q1, 108.0 / 1296, TOL7);
    a.round_quantity = true;
    a.round_up = 144.0;
    CHECK_NEAR(quantities(ConditionType::Area, 11, UOM_SQUARE_FEET, a).q1, 1.0, TOL7);
}

// Windows calculate_condition_quantities(Linear, calc 1, uom 2 (LF), [0,0,100,0], round, 12.0):
// 100 in = 8.3333 LF -> 9 x (12 in = 1 LF) = 9.0.   (COND-QTYROUND-01 plan)
TEST(Qty04_Rounding, LinearFeetRoundUpToTheNextFoot)
{
    QArgs a;
    a.position = {0, 0, 100, 0};
    a.round_quantity = true;
    a.round_up = 12.0;
    CHECK_EQ(quantities(ConditionType::Linear, 1, UOM_LINEAR_FEET, a).q1, 9.0);
}

// Windows calculate_condition_quantities(Linear, calc (1, 3, 20), uom (LF, IN2, CF), height 7, thickness 3,
// [0,0,13,0], round_quantity, round_up 12): every one of the three slots is rounded in its own unit:
// (2.0, 96.0, 0.1597222222222222).
TEST(Qty04_Rounding, EverySlotIsRoundedInItsOwnUnit)
{
    QArgs a;
    a.position = {0, 0, 13, 0};
    a.height = 7.0;
    a.thickness = 3.0;
    a.calc2 = 3; // length * height = 91 in2
    a.uom2 = UOM_SQUARE_INCHES;
    a.calc3 = CALC_VOLUME; // 13 * 7 * 3 = 273 in3
    a.uom3 = UOM_CUBIC_FEET;
    a.round_quantity = true;
    a.round_up = 12.0;
    QuantityResult q = quantities(ConditionType::Linear, 1, UOM_LINEAR_FEET, a);
    CHECK_EQ(q.q1, 2.0);  // 13/12 = 1.083 LF, increment 1 LF
    CHECK_EQ(q.q2, 96.0); // 91 in2, increment 12 -> 96
    CHECK_REL(q.q3, 0.1597222222222222, 1e-12); // 273/1728 CF, increment 12/1728 CF -> 23 steps
}

// ============================================================================================
// QTY-10: Area slopes use the absolute rise and run; linear slopes need strictly positive values.
// ============================================================================================

// Port of tests/domain/services/test_uom_service.py::UomGeometryHelperTests::test_slope_factor_is_the_hypotenuse_over_run_and_ignores_flat_or_unset_slopes
TEST(Qty10_AbsoluteSlope, SlopeFactorIsTheHypotenuseOverRunAndIgnoresFlatOrUnsetSlopes)
{
    CHECK_EQ(calc_slope_factor(3, 4), 1.25);
    CHECK_EQ(calc_slope_factor(0, 4), 1.0);
    CHECK_EQ(calc_slope_factor(3, 0), 1.0);
    CHECK_EQ(calc_slope_factor(-3, 4), 1.0);
    CHECK_EQ(calc_slope_factor(3, -4), 1.0);
    CHECK_EQ(calc_slope_factor(-3, -4), 1.0);
    CHECK_EQ(calc_slope_factor(-3, 4, true), 1.25);
    CHECK_EQ(calc_slope_factor(3, -4, true), 1.25);
    CHECK_EQ(calc_slope_factor(-3, -4, true), 1.25);
    CHECK_EQ(calc_slope_factor(0, 0, true), 1.0);
}

// Port of tests/domain/services/test_uom_service.py::UomDefaultArgumentTests::test_slope_factor_with_fractional_rise_and_run
TEST(Qty10_AbsoluteSlope, SlopeFactorWithFractionalRiseAndRun)
{
    CHECK_NEAR(calc_slope_factor(0.5, 0.5), std::sqrt(2.0), TOL7);
    CHECK_NEAR(calc_slope_factor(0.3, 0.4), 1.25, TOL7);
    CHECK_EQ(calc_slope_factor(0.0, 0.5), 1.0);
    CHECK_NEAR(calc_slope_factor(0.5, 0.0), 1.0, TOL7);
}

// Port of tests/domain/services/test_uom_service.py::UomWaveThreeSlopeAndGridDefaultTests::test_a_small_negative_rise_or_run_is_not_a_slope
// (Linear: only strictly positive rise AND run form a slope).
TEST(Qty10_AbsoluteSlope, ASmallNegativeRiseOrRunIsNotALinearSlope)
{
    struct RR
    {
        double rise, run;
    };
    for (const RR &rr : {RR{-0.5, 4.0}, RR{3.0, -0.5}, RR{-0.5, -0.5}, RR{0.0, 4.0}, RR{3.0, 0.0}})
    {
        SCOPED_CONTEXT("rise " + std::to_string(rr.rise) + " run " + std::to_string(rr.run));
        CHECK_EQ(calc_slope_factor(rr.rise, rr.run), 1.0);
        QArgs a;
        a.position = {0, 0, 12, 0};
        a.rise = rr.rise;
        a.run = rr.run;
        CHECK_EQ(quantities(ConditionType::Linear, 1, 0, a).q1, 12.0);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomLinearQuantityTests::test_slope_scales_linear_length_but_negative_rise_is_not_a_slope
TEST(Qty10_AbsoluteSlope, SlopeScalesLinearLengthButNegativeRiseIsNotASlope)
{
    QArgs base;
    base.position = {0, 0, 12, 0};
    base.rise = 3;
    base.run = 4;
    CHECK_EQ(quantities(ConditionType::Linear, 1, 0, base).q1, 15.0);
    QArgs flat = base;
    flat.rise = -3;
    CHECK_EQ(quantities(ConditionType::Linear, 1, 0, flat).q1, 12.0);
}

// Port of tests/domain/services/test_uom_service.py::AreaSlopeBehaviorTests::test_area_quantity_uses_absolute_slope_magnitude
TEST(Qty10_AbsoluteSlope, AreaQuantityUsesAbsoluteSlopeMagnitude)
{
    QArgs a;
    a.position = {0, 0, 120, 0, 120, 120, 0, 120};
    a.rise = -3;
    a.run = 4;
    CHECK_NEAR(quantities(ConditionType::Area, 11, UOM_SQUARE_FEET, a).q1, 125.0, TOL7);
}

// Port of tests/domain/services/test_uom_service.py::UomWaveThreeSlopeAndGridDefaultTests::test_small_negative_rise_or_run_has_magnitude_for_areas
TEST(Qty10_AbsoluteSlope, SmallNegativeRiseOrRunHasMagnitudeForAreas)
{
    const double expected = 144.0 * std::sqrt(0.5 * 0.5 + 4.0 * 4.0) / 4.0;
    struct RR
    {
        double rise, run;
    };
    for (const RR &rr : {RR{-0.5, 4.0}, RR{0.5, -4.0}, RR{-0.5, -4.0}})
    {
        SCOPED_CONTEXT("rise " + std::to_string(rr.rise) + " run " + std::to_string(rr.run));
        QArgs a;
        a.position = SQUARE_12;
        a.rise = rr.rise;
        a.run = rr.run;
        CHECK_NEAR(quantities(ConditionType::Area, 11, 0, a).q1, expected, TOL7);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomAreaQuantityTests::test_slope_scales_area_perimeter_and_hole_measures_and_uses_magnitudes
// extended with the Windows values for every slope-affected calc code (SQUARE_12 with HOLE_6,
// thickness 2, grid 2 x 2, footprint 10, attachment perimeter 14), for the four rise/run sign
// combinations: calc 11 -> 135, 14 -> 60, 15 -> 90, 34 -> 180, 21 -> 270, 17 -> 33.75, 18 -> 36
// (the bounding box is not scaled), 12 -> 125, 22 -> 250, 33 -> 104, 35 -> 360, 16 -> 144.
TEST(Qty10_AbsoluteSlope, SlopeScalesAreaPerimeterAndHoleMeasuresForEverySignCombination)
{
    struct E
    {
        int calc;
        double expected;
    };
    const E table[] = {{11, 135.0}, {14, 60.0}, {15, 90.0}, {34, 180.0}, {21, 270.0}, {17, 33.75},
                       {18, 36.0},  {12, 125.0}, {22, 250.0}, {33, 104.0}, {35, 360.0}, {16, 144.0}};
    struct RR
    {
        double rise, run;
    };
    for (const RR &rr : {RR{-3, -4}, RR{3, -4}, RR{-3, 4}, RR{3, 4}})
    {
        for (const E &e : table)
        {
            SCOPED_CONTEXT("rise " + std::to_string(rr.rise) + " run " + std::to_string(rr.run) + " calc " +
                           std::to_string(e.calc));
            QArgs a;
            a.position = SQUARE_12;
            a.holes = {HOLE_6};
            a.thickness = 2.0;
            a.attachment_footprint = 10.0;
            a.attachment_perimeter = 14.0;
            a.grid_size1 = 2.0;
            a.grid_size2 = 2.0;
            a.rise = rr.rise;
            a.run = rr.run;
            CHECK_NEAR(quantities(ConditionType::Area, e.calc, 0, a).q1, e.expected, TOL7);
        }
    }
}

// Windows calculate_condition_quantities(Area, calc 11 / 34, SQUARE_12) with fractional rise/run
// (-0.5, 4), (0.5, -4), (-0.5, -4), (0.5, 4): calc 11 on SQUARE_12 minus HOLE_6 = 108.84047960203041,
// calc 34 = 145.12063946937388.
TEST(Qty10_AbsoluteSlope, FractionalAreaSlopesMatchTheWindowsValues)
{
    struct RR
    {
        double rise, run;
    };
    for (const RR &rr : {RR{-0.5, 4}, RR{0.5, -4}, RR{-0.5, -4}, RR{0.5, 4}})
    {
        SCOPED_CONTEXT("rise " + std::to_string(rr.rise) + " run " + std::to_string(rr.run));
        QArgs a;
        a.position = SQUARE_12;
        a.holes = {HOLE_6};
        a.rise = rr.rise;
        a.run = rr.run;
        CHECK_REL(quantities(ConditionType::Area, 11, 0, a).q1, 108.84047960203041, 1e-12);
        CHECK_REL(quantities(ConditionType::Area, 34, 0, a).q1, 145.12063946937388, 1e-12);
    }
}

// ============================================================================================
// QTY-14: polygon area about the first vertex
// ============================================================================================

// Port of tests/domain/services/test_uom_service.py::UomGeometryHelperTests::test_polygon_area_is_absolute_orientation_free_and_needs_three_vertices
TEST(Qty14_PolygonAreaOrigin, PolygonAreaIsAbsoluteOrientationFreeAndNeedsThreeVertices)
{
    CHECK_EQ(calc_polygon_area(Poly{{0, 0}, {4, 0}, {0, 3}}), 6.0);
    CHECK_EQ(calc_polygon_area(Poly{{0, 0}, {0, 3}, {4, 0}}), 6.0);
    CHECK_EQ(calc_polygon_area(Poly{{0, 0}, {4, 0}}), 0.0);
    CHECK_EQ(calc_polygon_area(Poly{}), 0.0);
    CHECK_EQ(calc_polygon_area(Poly{{0, 0}, {12, 0}, {12, 12}, {0, 12}}), 144.0);
    // A far-away origin must not cost precision: 3 x 4 rectangle at 1e7 (places=6 -> 5e-7).
    CHECK_NEAR(calc_polygon_area(Poly{{1e7, 1e7}, {1e7 + 3, 1e7}, {1e7 + 3, 1e7 + 4}, {1e7, 1e7 + 4}}), 12.0, 5e-7);
    CHECK_EQ(calc_polygon_area(Poly{{0, 0}, {4, 0}, {4, 0}, {0, 3}}), 6.0);
}

// Port of tests/domain/services/test_uom_service.py::UomWaveThreeReworkTests::test_polygon_area_of_a_small_polygon_a_billion_inches_from_the_origin
TEST(Qty14_PolygonAreaOrigin, SmallPolygonABillionInchesFromTheOrigin)
{
    const double far = 1e9;
    std::vector<double> square = {far, far, far + 10, far, far + 10, far + 10, far, far + 10};
    CHECK_NEAR(calc_polygon_area(vertices_from_position(square)), 100.0, 1e-6);
    // Windows calculate_polygon_area: 1 x 1 at 1e9 -> 1.0 and 2 x 10 at 1e9 -> 20.0 (iOS gave 0).
    CHECK_EQ(calc_polygon_area(Poly{{1e9, 1e9}, {1e9 + 1, 1e9}, {1e9 + 1, 1e9 + 1}, {1e9, 1e9 + 1}}), 1.0);
    CHECK_EQ(calc_polygon_area(Poly{{1e9, 1e9}, {1e9 + 2, 1e9}, {1e9 + 2, 1e9 + 10}, {1e9, 1e9 + 10}}), 20.0);
    // 2 x 0.1 sliver at 1e8: Windows 0.19999998807907104 (the sliver height is 1e8+0.1 - 1e8).
    CHECK_EQ(calc_polygon_area(Poly{{1e8, 1e8}, {1e8 + 2, 1e8}, {1e8 + 2, 1e8 + 0.1}, {1e8, 1e8 + 0.1}}),
             0.19999998807907104);
}

// Port of tests/integration/geometry/test_quantity_pivot_precision.py::TakeoffLifecyclePrecisionTests::test_area_and_rotation_pivot_are_translation_independent_for_slivers
// (the polygon-area part; the centroid / signed_area parts are not in UomService).
TEST(Qty14_PolygonAreaOrigin, AreaIsTranslationIndependentForSlivers)
{
    for (double size : {0.1, 0.0001})
    {
        for (double offset : {0.0, 1e8})
        {
            SCOPED_CONTEXT("size " + std::to_string(size) + " offset " + std::to_string(offset));
            // Exactly representable dimensions after translation as the reference: this tests
            // cancellation, not input rounding.
            double left = offset, right = offset + 2;
            double low = offset, high = offset + size;
            Poly vertices = {{left, low}, {right, low}, {right, high}, {left, high}};
            double expected = (right - left) * (high - low);
            CHECK_NEAR(calc_polygon_area(vertices), expected, expected * 1e-12);
        }
    }
}

// Port of tests/domain/services/test_uom_service.py::AreaSlopeBehaviorTests::test_area_quantity_retains_fractional_origin_precision
TEST(Qty14_PolygonAreaOrigin, AreaQuantityRetainsFractionalOriginPrecision)
{
    QArgs a;
    a.position = {891.672, 507.397, 435.672, 507.397, 435.672, 860.397,
                  593.672, 860.397, 593.672, 702.397, 893.672, 702.397};
    // Independent decomposition after subtracting the fractional origin: 458 x 195 rectangle, minus
    // the 2 x 195 / 2 right-edge triangle, plus the 158 x 158 lower-left rectangle (square inches
    // converted to square feet), to 10 places.
    const double expected_square_feet = (458 * 195 - 2 * 195 / 2.0 + 158 * 158) / 144.0;
    CHECK_NEAR(quantities(ConditionType::Area, 11, UOM_SQUARE_FEET, a).q1, expected_square_feet, 5e-11);
}

// ============================================================================================
// Regression ports (no divergence expected) of the per-calc tables of test_uom_service.py
// ============================================================================================

// Port of tests/domain/services/test_uom_service.py::UomGeometryHelperTests (vertices, bounding box, perimeter)
TEST(Qty00_GeometryRegression, VerticesBoundingBoxAndPerimeter)
{
    CHECK(vertices_from_position({1, 2, 3, 4}) == Poly({{1, 2}, {3, 4}}));
    CHECK(vertices_from_position({1, 2, 3}) == Poly({{1, 2}}));
    CHECK(vertices_from_position({1}) == Poly());
    CHECK(vertices_from_position({}) == Poly());
    CHECK(calc_bounding_box({}) == Pt(0.0, 0.0));
    CHECK(calc_bounding_box({3, 4}) == Pt(0, 0));
    CHECK(calc_bounding_box({1, 2, 5, 9, -1, 4}) == Pt(6, 7));
    CHECK_EQ(calc_polygon_perimeter(SQUARE_12), 48.0);
    CHECK_EQ(calc_polygon_perimeter({0, 0, 3, 4}), 10.0);
    CHECK_EQ(calc_polygon_perimeter({0, 0}), 0.0);
    CHECK_EQ(calc_polygon_perimeter({}), 0.0);
    CHECK_EQ(calc_polygon_area_sq_inches({0, 0, 4, 0}), 0.0);
    CHECK_EQ(calc_polygon_area_sq_inches({0, 0, 4, 0, 0, 3}), 6.0);
    CHECK_EQ(calc_polygon_area(Poly{{0, 0}, {1, 0}, {0, 1}}), 0.5);
    CHECK_EQ(calc_polygon_area(Poly{{0, 0}, {0, 1}, {1, 0}}), 0.5);
}

// Port of tests/domain/services/test_uom_service.py::UomCountQuantityTests::test_every_count_calculation_uses_the_condition_dimensions
TEST(Qty00_GeometryRegression, EveryCountCalculationUsesTheConditionDimensions)
{
    struct E
    {
        int calc;
        double expected;
    };
    const E table[] = {{0, 0.0},  {CALC_COUNT, 1.0},      {CALC_HEIGHT_OR_LENGTH, 7.0}, {13, 16.0},
                       {CALC_TOP_BOTTOM, 15.0},           {CALC_TOP_AND_BOTTOM, 30.0},  {CALC_VOLUME, 105.0},
                       {CALC_WIDTH_SIDE, 21.0},           {CALC_BOTH_WIDTH_SIDES, 42.0}, {CALC_DEPTH_SIDE, 35.0},
                       {CALC_BOTH_DEPTH_SIDES, 70.0},     {CALC_ALL_SIDES, 112.0},      {CALC_ALL_SIDES_PLUS_TB, 142.0},
                       {99, 0.0}};
    for (const E &e : table)
    {
        for (ConditionType type : {ConditionType::Count, ConditionType::Attachment})
        {
            SCOPED_CONTEXT("calc_type " + std::to_string(e.calc) + " condition_type " +
                           std::to_string(static_cast<int>(type)));
            QArgs a;
            a.width = 3.0;
            a.height = 7.0;
            a.depth = 5.0;
            CHECK_EQ(quantities(type, e.calc, 0, a).q1, e.expected);
        }
    }
}

// Port of tests/domain/services/test_uom_service.py::UomCountQuantityTests::test_all_three_quantity_slots_are_computed_independently_with_their_own_units
TEST(Qty00_GeometryRegression, AllThreeQuantitySlotsAreComputedIndependentlyWithTheirOwnUnits)
{
    QuantityResult r = calculate_condition_quantities(
        ConditionType::Count, CALC_WIDTH_SIDE, CALC_VOLUME, CALC_COUNT,
        UOM_SQUARE_FEET, UOM_CUBIC_FEET, UOM_EACH, 30.0, 12.0, 6.0, 0.0, {});
    CHECK_NEAR(r.q1, 30.0 * 12 / 144, TOL7);
    CHECK_NEAR(r.q2, 30.0 * 6 * 12 / 1728, TOL7);
    CHECK_EQ(r.q3, 1.0);
    CHECK_NE(r.q1, r.q2);
}

// Port of tests/domain/services/test_uom_service.py::UomLinearQuantityTests::test_every_linear_calculation_uses_length_height_and_thickness
// and ::test_a_linear_condition_ignores_calculation_codes_of_other_condition_types.
TEST(Qty00_GeometryRegression, EveryLinearCalculationUsesLengthHeightAndThickness)
{
    struct E
    {
        int calc;
        double expected;
    };
    const E table[] = {{0, 0.0}, {1, 5.0}, {2, 1.0}, {3, 35.0}, {4, 70.0}, {5, 15.0}, {6, 30.0},
                       {7, 21.0}, {8, 42.0}, {9, 100.0}, {20, 105.0}, {99, 0.0}};
    for (const E &e : table)
    {
        SCOPED_CONTEXT("calc_type " + std::to_string(e.calc));
        QArgs a;
        a.position = {0, 0, 3, 4};
        a.height = 7.0;
        a.thickness = 3.0;
        CHECK_EQ(quantities(ConditionType::Linear, e.calc, 0, a).q1, e.expected);
    }
    for (int calc : {10, 11, 12, 13, 15, 17, 19, 21, 23, 24, 29, 30})
    {
        SCOPED_CONTEXT("other calc_type " + std::to_string(calc));
        QArgs a;
        a.position = {0, 0, 3, 4};
        a.height = 7.0;
        a.thickness = 3.0;
        QuantityResult r = quantities(ConditionType::Linear, calc, 0, a);
        CHECK_EQ(r.q1, 0.0);
        CHECK_EQ(r.q2, 0.0);
        CHECK_EQ(r.q3, 0.0);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomLinearQuantityTests::test_segment_lengths_are_summed_every_seven_coordinates
// and ::test_a_missing_position_yields_zero.
TEST(Qty00_GeometryRegression, SegmentLengthsAreSummedEverySevenCoordinates)
{
    CHECK_EQ(calc_linear_length({0, 0, 3, 4}), 5.0);
    CHECK_EQ(calc_linear_length({0, 0, 3, 4, 9, 9, 9, 10, 10, 13, 14}), 10.0);
    CHECK_EQ(calc_linear_length({0, 0, 3, 4, 9, 9, 9, 10, 10, 13}), 5.0);
    CHECK_EQ(calc_linear_length({0, 0, 3}), 0.0);
    CHECK_EQ(calc_linear_length({}), 0.0);
    CHECK_EQ(calc_linear_length({0, 0, 3, 4, 1, 2}, -1), 5.0);
    CHECK_EQ(calc_linear_length({1, 2, 4, 6}), 5.0);
    CHECK_EQ(calc_linear_length({1, 2, 4, 6, 0, 0, 0, 20, 30, 23, 34}), 10.0);
    for (std::vector<double> position : {std::vector<double>{}, std::vector<double>{1.0}})
    {
        QArgs a;
        a.position = position;
        CHECK_EQ(quantities(ConditionType::Linear, 1, 0, a).q1, 0.0);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomAreaQuantityTests::test_every_area_calculation_combines_polygon_holes_attachments_and_grid
// and ::test_an_area_condition_ignores_calculation_codes_of_other_condition_types.
TEST(Qty00_GeometryRegression, EveryAreaCalculationCombinesPolygonHolesAttachmentsAndGrid)
{
    struct E
    {
        int calc;
        double expected;
    };
    const E table[] = {{0, 0.0},   {11, 108.0}, {12, 98.0},  {14, 48.0},  {15, 72.0},  {16, 72.0},
                       {17, 5.4},  {18, 6.0},   {19, 1.0},   {21, 216.0}, {22, 196.0}, {23, 1.0},
                       {33, 86.0}, {34, 144.0}, {35, 288.0}, {99, 0.0}};
    auto area = [](int calc)
    {
        QArgs a;
        a.position = SQUARE_12;
        a.holes = {HOLE_6};
        a.thickness = 2.0;
        a.attachment_footprint = 10.0;
        a.attachment_perimeter = 14.0;
        a.grid_size1 = 3.0;
        a.grid_size2 = 4.0;
        a.gap = 1.0;
        return quantities(ConditionType::Area, calc, 0, a).q1;
    };
    for (const E &e : table)
    {
        SCOPED_CONTEXT("calc_type " + std::to_string(e.calc));
        CHECK_NEAR(area(e.calc), e.expected, TOL7);
    }
    for (int calc : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 13, 20, 24, 25, 29, 30, 31, 32, 36})
    {
        SCOPED_CONTEXT("other calc_type " + std::to_string(calc));
        CHECK_EQ(area(calc), 0.0);
    }
}
