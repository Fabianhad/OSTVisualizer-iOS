// UOM code table, labels, conversion divisors, valid-unit lists and condition-type support.
//
// Windows oracle: ost_visualizer/domain/services/uom_service.py and
// tests/domain/services/test_uom_service.py (UomCodeTableTests, UomValidUnitTests,
// UomConversionTests, UomConditionTypeSupportTests), tests/integration/quantities/
// test_scalar_conversion_contract.py.
//
// Items: QTY-05 (metric UOM codes 11-16), QTY-16 (labels 'IN^2' / 'SQ', roofing squares in the area
// list), QTY-17 (unsupported condition types: options, is_supported_condition_type, throw).
//
// Superscripts are written as the UTF-8 bytes Windows uses: ^2 = C2 B2, ^3 = C2 B3.
#include "QtyHelpers.hpp"

#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

using namespace qty;

namespace
{
    const std::string SQ_IN = "IN\xC2\xB2";
    const std::string MM2 = "mm\xC2\xB2";
    const std::string M2 = "m\xC2\xB2";
    const std::string MM3 = "mm\xC2\xB3";
    const std::string M3 = "m\xC2\xB3";

    using UomList = std::vector<UomOption>;
    const UomList COUNT_UOMS = {{0, "EA"}};
    const UomList LENGTH_UOMS = {{2, "LF"}, {3, "LY"}, {1, "IN"}};
    const UomList AREA_UOMS = {{5, "SF"}, {6, "SY"}, {7, "SQ"}, {4, SQ_IN}};
    const UomList VOLUME_UOMS = {{8, "CF"}, {9, "CY"}};
    const UomList LENGTH_UOMS_M = {{13, "m"}, {11, "mm"}};
    const UomList AREA_UOMS_M = {{14, M2}, {12, MM2}};
    const UomList VOLUME_UOMS_M = {{16, M3}, {15, MM3}};

    std::string lower(std::string s)
    {
        for (char &c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
} // namespace

// ============================================================================================
// QTY-05: metric UOM codes 11-16
// ============================================================================================

// Port of tests/domain/services/test_uom_service.py::UomCodeTableTests::test_uom_and_calculation_codes_are_the_stored_database_values
TEST(Qty05_MetricUom, CodesAreTheStoredDatabaseValues)
{
    const std::vector<int> uoms = {UOM_EACH, UOM_INCHES, UOM_LINEAR_FEET, UOM_LINEAR_YARDS, UOM_SQUARE_INCHES,
                                   UOM_SQUARE_FEET, UOM_SQUARE_YARDS, UOM_SQUARE_ROOFING, UOM_CUBIC_FEET,
                                   UOM_CUBIC_YARDS, UOM_MM, UOM_MM2, UOM_M, UOM_M2, UOM_MM3, UOM_M3};
    CHECK(uoms == std::vector<int>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16}));
    const std::vector<int> calcs = {CALC_TOP_BOTTOM, CALC_TOP_AND_BOTTOM, CALC_VOLUME, CALC_COUNT,
                                    CALC_HEIGHT_OR_LENGTH, CALC_WIDTH_SIDE, CALC_BOTH_WIDTH_SIDES,
                                    CALC_DEPTH_SIDE, CALC_BOTH_DEPTH_SIDES, CALC_ALL_SIDES,
                                    CALC_ALL_SIDES_PLUS_TB};
    CHECK(calcs == std::vector<int>({5, 6, 20, 23, 24, 25, 26, 27, 28, 29, 30}));
}

// Port of tests/domain/services/test_uom_service.py::UomCodeTableTests::test_labels_cover_every_uom_and_unknown_codes_have_no_label
TEST(Qty05_MetricUom, LabelsCoverEveryUomAndUnknownCodesHaveNoLabel)
{
    struct L
    {
        int code;
        std::string label;
    };
    const std::vector<L> expected = {
        {0, "EA"}, {1, "IN"}, {2, "LF"}, {3, "LY"}, {4, SQ_IN}, {5, "SF"}, {6, "SY"}, {7, "SQ"},
        {8, "CF"}, {9, "CY"}, {11, "mm"}, {12, MM2}, {13, "m"}, {14, M2}, {15, MM3}, {16, M3},
    };
    for (const L &l : expected)
    {
        SCOPED_CONTEXT("code " + std::to_string(l.code));
        CHECK_STR_EQ(get_uom_label(l.code), l.label);
    }
    for (int code : {10, 17, -1, 99})
    {
        SCOPED_CONTEXT("unknown code " + std::to_string(code));
        CHECK_STR_EQ(get_uom_label(code), "");
    }
}

// Port of tests/domain/services/test_uom_service.py::UomCodeTableTests::test_only_the_millimetre_and_metre_families_are_metric
TEST(Qty05_MetricUom, OnlyTheMillimetreAndMetreFamiliesAreMetric)
{
    for (int code = 0; code < 20; ++code)
    {
        SCOPED_CONTEXT("code " + std::to_string(code));
        CHECK_EQ(is_metric_uom(code), code >= 11 && code <= 16);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomConversionTests::test_every_unit_divides_inches_by_its_size_in_inches
// and tests/integration/quantities/test_scalar_conversion_contract.py (1 sq/cu inch in metres).
TEST(Qty05_MetricUom, EveryUnitDividesInchesByItsSizeInInches)
{
    struct E
    {
        int code;
        double per_unit;
    };
    const std::vector<E> expectations = {
        {UOM_EACH, 1.0},
        {UOM_INCHES, 1.0},
        {UOM_LINEAR_FEET, 1.0 / 12},
        {UOM_LINEAR_YARDS, 1.0 / 36},
        {UOM_SQUARE_INCHES, 1.0},
        {UOM_SQUARE_FEET, 1.0 / 144},
        {UOM_SQUARE_YARDS, 1.0 / 1296},
        {UOM_SQUARE_ROOFING, 1.0 / 14400},
        {UOM_CUBIC_FEET, 1.0 / 1728},
        {UOM_CUBIC_YARDS, 1.0 / 46656},
        {UOM_MM, 25.4},
        {UOM_M, 0.0254},
        {UOM_MM2, 25.4 * 25.4},
        {UOM_M2, 25.4 * 25.4 / 1000000.0},
        {UOM_MM3, 25.4 * 25.4 * 25.4},
        {UOM_M3, 25.4 * 25.4 * 25.4 / 1000000000.0},
    };
    for (const E &e : expectations)
    {
        SCOPED_CONTEXT("code " + std::to_string(e.code));
        CHECK_NEAR(convert_to_uom(1.0, e.code) / e.per_unit, 1.0, 1e-12);
        CHECK_NEAR(convert_to_uom(3456.0, e.code) / (3456.0 * e.per_unit), 1.0, 1e-12);
    }
    CHECK_EQ(convert_to_uom(5.0, 99), 5.0);
    CHECK_EQ(convert_to_uom(0.0, UOM_SQUARE_FEET), 0.0);
    CHECK_EQ(convert_to_uom(-144.0, UOM_SQUARE_FEET), -1.0);
}

// Windows convert_to_uom run with raw 1.0 / 3456.0 / -1.0 for every metric code
// (divisors raw/(1/25.4), raw/(1000/25.4), raw/(1/25.4^2), raw/(1e6/25.4^2), raw/(1/25.4^3),
// raw/(1e9/25.4^3), i.e. inches*25.4, *0.0254, *645.16, *0.00064516, *16387.064, *1.6387064e-5).
// Windows is plain IEEE division here, so the values are compared exactly.
TEST(Qty05_MetricUom, MetricConversionsMatchTheWindowsValuesExactly)
{
    struct E
    {
        int code;
        double one, many, minus_one;
    };
    const std::vector<E> windows = {
        {11, 25.4, 87782.4, -25.4},
        {12, 645.16, 2229672.96, -645.16},
        {13, 0.0254, 87.7824, -0.0254},
        {14, 0.00064516, 2.22967296, -0.00064516},
        {15, 16387.064, 56633693.18399999, -16387.064},
        {16, 1.6387064e-05, 0.056633693183999995, -1.6387064e-05},
    };
    for (const E &e : windows)
    {
        SCOPED_CONTEXT("code " + std::to_string(e.code));
        CHECK_EQ(convert_to_uom(1.0, e.code), e.one);
        CHECK_EQ(convert_to_uom(3456.0, e.code), e.many);
        CHECK_EQ(convert_to_uom(-1.0, e.code), e.minus_one);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomLinearQuantityTests::test_linear_quantity_converts_to_the_requested_unit
// plus the QTY-05 plan: a linear takeoff [0,0,100,0] with calc 1 and uom 13 measures 2.54 m.
TEST(Qty05_MetricUom, LinearQuantityConvertsToTheRequestedUnit)
{
    QArgs a;
    a.position = {0, 0, 24, 0};
    CHECK_NEAR(quantities(ConditionType::Linear, 1, UOM_LINEAR_FEET, a).q1, 2.0, TOL7);
    a.position = {0, 0, 100, 0};
    CHECK_NEAR(quantities(ConditionType::Linear, 1, UOM_M, a).q1, 2.54, TOL7);
}

// Port of tests/domain/services/test_uom_service.py::UomValidUnitTests::test_every_calculation_code_selects_its_unit_family_in_both_systems
TEST(Qty05_MetricUom, EveryCalculationCodeSelectsItsUnitFamilyInBothSystems)
{
    struct Family
    {
        std::vector<int> codes;
        const UomList *imperial;
        const UomList *metric;
    };
    const std::vector<Family> families = {
        {{0, 23, 2, 17, 18, 19}, &COUNT_UOMS, &COUNT_UOMS},
        {{1, 24, 13, 14, 15, 16, 33}, &LENGTH_UOMS, &LENGTH_UOMS_M},
        {{20, 21, 22, 35}, &VOLUME_UOMS, &VOLUME_UOMS_M},
        {{5, 6, 25, 26, 27, 28, 29, 30, 3, 4, 7, 8, 9, 11, 12, 34}, &AREA_UOMS, &AREA_UOMS_M},
        {{10, 31, 32, 36, 99, -1}, &LENGTH_UOMS, &LENGTH_UOMS_M},
    };
    for (const Family &f : families)
    {
        for (int code : f.codes)
        {
            SCOPED_CONTEXT("calc_type " + std::to_string(code));
            CHECK(get_valid_uoms_for_calc_type(code) == *f.imperial);
            CHECK(get_valid_uoms_for_calc_type(code, true) == *f.metric);
            CHECK(get_valid_uoms_for_calc_type(code, false) == *f.imperial);
        }
    }
}

// Port of tests/domain/services/test_uom_service.py::UomCodeTableTests::test_every_offered_calculation_maps_to_the_unit_family_its_label_measures
TEST(Qty05_MetricUom, EveryOfferedCalculationMapsToTheUnitFamilyItsLabelMeasures)
{
    struct Pair
    {
        const UomList *imperial;
        const UomList *metric;
    };
    auto expected = [](const std::string &label) -> Pair
    {
        // Independent of the production tables: what does the label measure?
        std::string low = lower(label);
        if (low == "count" || low == "segment count" || low == "area counts" || low.rfind("tile count", 0) == 0)
            return {&COUNT_UOMS, &COUNT_UOMS};
        if (low.find("volume") != std::string::npos)
            return {&VOLUME_UOMS, &VOLUME_UOMS_M};
        if (low.find("surface area") != std::string::npos || low.rfind("area", 0) == 0)
            return {&AREA_UOMS, &AREA_UOMS_M};
        return {&LENGTH_UOMS, &LENGTH_UOMS_M}; // length, perimeter, total height, grid length
    };
    size_t seen = 0;
    for (ConditionType type : {ConditionType::Count, ConditionType::Linear, ConditionType::Area})
    {
        std::vector<QtyOption> options = get_quantity_options_for_type(type);
        for (const QtyOption &opt : options)
        {
            ++seen;
            SCOPED_CONTEXT("calc_type " + std::to_string(opt.first) + " label " + opt.second);
            Pair p = expected(opt.second);
            CHECK(get_valid_uoms_for_calc_type(opt.first) == *p.imperial);
            CHECK(get_valid_uoms_for_calc_type(opt.first, true) == *p.metric);
        }
    }
    CHECK_EQ(seen, static_cast<size_t>(33));
    CHECK(get_valid_uoms_for_calc_type(0) == COUNT_UOMS);
    CHECK(get_valid_uoms_for_calc_type(0, true) == COUNT_UOMS);
}

// ============================================================================================
// QTY-16: labels 'IN^2' / 'SQ' and the area UOM list (SF, SY, SQ, IN^2)
// ============================================================================================

// Port of tests/domain/services/test_uom_service.py::UomCodeTableTests::test_labels_cover_every_uom_and_unknown_codes_have_no_label
// (the two labels that differed on iOS: 4 'SQ IN' -> 'IN^2', 7 'ROOF' -> 'SQ').
TEST(Qty16_UomLabels, SquareInchesAndRoofingSquareLabels)
{
    CHECK_STR_EQ(get_uom_label(4), "IN\xC2\xB2");
    CHECK_STR_EQ(get_uom_label(4), SQ_IN);
    CHECK_EQ(get_uom_label(4).size(), static_cast<size_t>(4)); // 'I' 'N' 0xC2 0xB2
    CHECK_STR_EQ(get_uom_label(7), "SQ");
    for (int code : {10, -1})
        CHECK_STR_EQ(get_uom_label(code), "");
}

// Port of tests/domain/services/test_uom_service.py::UomValidUnitTests (area family) and
// tests/integration/quantities/test_condition_uom_parity.py (COND-UOMPICK-01 plan):
// every area calc code offers SF, SY, SQ, IN^2 in that order.
TEST(Qty16_UomLabels, EveryAreaCalcOffersFeetYardsRoofingSquaresAndSquareInches)
{
    for (int calc : {3, 4, 5, 6, 7, 8, 9, 11, 12, 25, 26, 27, 28, 29, 30, 34})
    {
        SCOPED_CONTEXT("calc_type " + std::to_string(calc));
        CHECK(get_valid_uoms_for_calc_type(calc) == AREA_UOMS);
    }
    CHECK(get_valid_uoms_for_calc_type(11) == UomList({{5, "SF"}, {6, "SY"}, {7, "SQ"}, {4, "IN\xC2\xB2"}}));
    // the other families are unchanged
    CHECK(get_valid_uoms_for_calc_type(13) == UomList({{2, "LF"}, {3, "LY"}, {1, "IN"}}));
    CHECK(get_valid_uoms_for_calc_type(20) == UomList({{8, "CF"}, {9, "CY"}}));
    CHECK(get_valid_uoms_for_calc_type(23) == UomList({{0, "EA"}}));
    CHECK(get_valid_uoms_for_calc_type(0) == UomList({{0, "EA"}}));
    for (int calc : {10, 31, 32, 36, 99, -1})
    {
        SCOPED_CONTEXT("unknown calc_type " + std::to_string(calc));
        CHECK(get_valid_uoms_for_calc_type(calc) == LENGTH_UOMS);
    }
}

// Port of tests/integration/quantities/test_condition_uom_parity.py::ConditionUomConsistencyTests::test_imperial_roofing_uom_remains_available_and_quantity_is_not_rescaled
// (120 x 120 area, calc 11, uom 7 -> 1.0 labeled 'SQ').
TEST(Qty16_UomLabels, RoofingSquareQuantityIsOneAndLabeledSq)
{
    QArgs a;
    a.position = {0.0, 0.0, 120.0, 0.0, 120.0, 120.0, 0.0, 120.0};
    CHECK_EQ(quantities(ConditionType::Area, 11, UOM_SQUARE_ROOFING, a).q1, 1.0);
    CHECK_STR_EQ(get_uom_label(UOM_SQUARE_ROOFING), "SQ");
}

// ============================================================================================
// QTY-17: unsupported condition types
// ============================================================================================

namespace
{
    const std::vector<QtyOption> COUNT_OPTIONS = {
        {23, "Count"},
        {24, "Total Height"},
        {13, "Perimeter"},
        {5, "Surface area (top or bottom)"},
        {6, "Surface area (top and bottom)"},
        {25, "Surface area (single width side)"},
        {26, "Surface area (both width sides)"},
        {29, "Surface area (all sides)"},
        {30, "Surface area (all sides + top and bottom)"},
        {20, "Volume"},
    };
    const std::vector<QtyOption> LINEAR_OPTIONS = {
        {1, "Length"},
        {2, "Segment count"},
        {3, "Surface area (single side)"},
        {4, "Surface area (both sides)"},
        {5, "Surface area (top or bottom)"},
        {6, "Surface area (top and bottom)"},
        {7, "Surface area (single end)"},
        {8, "Surface area (both ends)"},
        {9, "Surface area (all side/duct)"},
        {20, "Volume"},
    };
    const std::vector<QtyOption> AREA_OPTIONS = {
        {11, "Area"},
        {34, "Area (Ignore Backout Areas)"},
        {12, "Area (minus Attachments)"},
        {15, "Perimeter"},
        {14, "Perimeter (without Backout Perimeters)"},
        {33, "Perimeter (plus Attachments Perimeter)"},
        {16, "Grid length (visible)"},
        {17, "Tile count (average)"},
        {18, "Tile count (visible)"},
        {19, "Area counts"},
        {21, "Volume"},
        {35, "Volume (Ignore Backout Volume)"},
        {22, "Volume (minus Attachments)"},
    };
} // namespace

// Port of tests/domain/services/test_uom_service.py::UomCodeTableTests::test_quantity_option_lists_are_the_dialog_contract
TEST(Qty17_UnsupportedTypes, QuantityOptionListsAreTheDialogContract)
{
    CHECK(get_quantity_options_for_type(ConditionType::Count) == COUNT_OPTIONS);
    CHECK(get_quantity_options_for_type(ConditionType::Linear) == LINEAR_OPTIONS);
    CHECK(get_quantity_options_for_type(ConditionType::Area) == AREA_OPTIONS);
}

// Port of tests/domain/services/test_uom_service.py::UomCodeTableTests::test_options_depend_on_condition_type_and_unknown_types_get_count_options
TEST(Qty17_UnsupportedTypes, UnknownAndAttachmentTypesGetTheCountOptions)
{
    for (int other : {3, 9, -1})
    {
        SCOPED_CONTEXT("condition_type " + std::to_string(other));
        CHECK(get_quantity_options_for_type(static_cast<ConditionType>(other)) == COUNT_OPTIONS);
    }
    CHECK(get_quantity_options_for_type(ConditionType::Attachment) == COUNT_OPTIONS);
}

// Port of tests/domain/services/test_uom_service.py::UomConditionTypeSupportTests::test_the_supported_types_are_exactly_linear_area_count_and_attachment
TEST(Qty17_UnsupportedTypes, TheSupportedTypesAreExactlyLinearAreaCountAndAttachment)
{
    CHECK_EQ(static_cast<int>(ConditionType::Linear), 0);
    CHECK_EQ(static_cast<int>(ConditionType::Area), 1);
    CHECK_EQ(static_cast<int>(ConditionType::Count), 2);
    CHECK_EQ(static_cast<int>(ConditionType::Attachment), 3);
    for (int type = -2; type < 8; ++type)
    {
        SCOPED_CONTEXT("condition_type " + std::to_string(type));
        CHECK_EQ(is_supported_condition_type(static_cast<ConditionType>(type)), type >= 0 && type <= 3);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomConditionTypeSupportTests::test_every_supported_type_is_calculated_without_error
TEST(Qty17_UnsupportedTypes, EverySupportedTypeIsCalculatedWithoutError)
{
    for (int type = 0; type <= 3; ++type)
    {
        SCOPED_CONTEXT("condition_type " + std::to_string(type));
        QArgs a;
        a.position = {0, 0, 12, 0, 12, 12, 0, 12};
        bool threw = false;
        try
        {
            quantities(static_cast<ConditionType>(type), CALC_COUNT, 0, a);
        }
        catch (...)
        {
            threw = true;
        }
        CHECK_FALSE(threw);
    }
}

// Port of tests/domain/services/test_uom_service.py::UomConditionTypeSupportTests::test_a_type_outside_the_supported_set_is_a_value_error
// and ::test_an_unsupported_type_is_refused_even_without_geometry and
// UomWaveThreeReworkTests::test_an_unsupported_condition_type_message_names_the_type.
// Windows raises UnsupportedConditionTypeError (a ValueError) with the message
// 'Unsupported condition type <n>'; the C++ equivalent is a std::invalid_argument subclass.
TEST(Qty17_UnsupportedTypes, ATypeOutsideTheSupportedSetIsRefused)
{
    for (int type : {-1, 4, 7, 99})
    {
        SCOPED_CONTEXT("condition_type " + std::to_string(type));
        QArgs a;
        a.position = {0, 0, 12, 0, 12, 12, 0, 12};
        bool caught = false;
        try
        {
            quantities(static_cast<ConditionType>(type), 11, 0, a);
        }
        catch (const UnsupportedConditionTypeError &e)
        {
            caught = true;
            CHECK_STR_EQ(std::string(e.what()), "Unsupported condition type " + std::to_string(type));
        }
        CHECK(caught);
    }
    // refused even without geometry
    bool caught = false;
    try
    {
        quantities(static_cast<ConditionType>(5), 0, 0);
    }
    catch (const std::invalid_argument &)
    {
        caught = true;
    }
    CHECK(caught);
    // the message names the type
    caught = false;
    try
    {
        quantities(static_cast<ConditionType>(7), 0, 0);
    }
    catch (const UnsupportedConditionTypeError &e)
    {
        caught = true;
        CHECK_STR_EQ(std::string(e.what()), "Unsupported condition type 7");
    }
    CHECK(caught);
}
