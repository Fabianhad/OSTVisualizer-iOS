// Quantity text of the Conditions list: QTY-12 (two decimals for metric units below 100, locale independent
// grouping), QTY-22 (a zero quantity reads "0 <label>"), COND-METRIC-01 (2-decimal text of metric bids).
//
// Windows oracle: ost_visualizer/application/utils/quantity_display.py with
// tests/application/utils/test_quantity_display.py (ported below), presentation/components/conditions_sidebar.py
// update_quantities (three slots per row, a missing entry is (0, 0, 0)) and Fixtures/formatting_oracle.txt (QTY records
// produced by running format_quantity_number / format_quantity_with_uom read-only).
#include "FormattingTestSupport.hpp"

#include "Domain/QuantityDisplay.hpp"

#include <limits>
#include <string>
#include <vector>

using namespace ost_domain;

namespace
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();
}

// ---------------------------------------------------------------------------------------------------------------------
// QTY-12
// ---------------------------------------------------------------------------------------------------------------------

TEST(Qty12_QuantityNumber, FacadeReplacementPreservesExactFormatting)
{
    // Port of test_quantity_display.py::QuantityDisplayFormattingTests::test_quantity_display_facade_replacement_preserves_exact_formatting
    struct Row
    {
        double value;
        int uom;
        const char *text;
    };
    const Row rows[] = {
        {0.0, UOM_LINEAR_FEET, ""},      {12.4, UOM_LINEAR_FEET, "12"},   {12.6, UOM_LINEAR_FEET, "13"},
        {-12.6, UOM_LINEAR_FEET, "-13"}, {12.345, UOM_M, "12.35"},         {-12.345, UOM_M, "-12.35"},
        {100.0, UOM_M, "100"},           {1234.0, UOM_CUBIC_YARDS, "1,234"},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("value " + std::to_string(row.value) + " uom " + std::to_string(row.uom));
        CHECK_STR_EQ(format_quantity_number(row.value, row.uom), row.text);
    }
    const UomLabelFn label = [](int code) -> std::string
    {
        if (code == UOM_LINEAR_FEET)
            return "LF";
        if (code == UOM_M)
            return "M";
        return "";
    };
    CHECK_STR_EQ(format_quantity_with_uom(0.0, UOM_LINEAR_FEET, label), "0 LF");
    CHECK_STR_EQ(format_quantity_with_uom(12.345, UOM_M, label), "12.35 M");
    CHECK_STR_EQ(format_quantity_with_uom(-12.6, 999, label), "-13");
}

TEST(Qty12_QuantityNumber, AllMetricDimensionsSharePrecisionAndAbsoluteThreshold)
{
    // Port of test_all_metric_dimensions_share_precision_and_absolute_threshold
    struct Row
    {
        double value;
        const char *text;
    };
    const Row rows[] = {{12.345, "12.35"}, {-12.345, "-12.35"}, {99.99, "99.99"}, {-99.99, "-99.99"},
                        {100.0, "100"},    {-100.0, "-100"},     {1234.6, "1,235"}};
    for (int uom : {UOM_M, UOM_M2, UOM_M3, UOM_MM, UOM_MM2, UOM_MM3})
    {
        for (const Row &row : rows)
        {
            SCOPED_CONTEXT("uom " + std::to_string(uom) + " value " + std::to_string(row.value));
            CHECK_STR_EQ(format_quantity_number(row.value, uom), row.text);
        }
    }
}

TEST(Qty12_QuantityNumber, ZeroPlaceholderAndMissingLabelDoNotInventUnits)
{
    // Port of test_zero_placeholder_and_missing_label_do_not_invent_units
    const UomLabelFn none = [](int) { return std::string(); };
    for (double value : {0.0, -0.0})
    {
        CHECK_STR_EQ(format_quantity_number(value, UOM_M, "none"), "none");
        CHECK_STR_EQ(format_quantity_with_uom(value, 999, none), "");
    }
}

TEST(Qty12_QuantityNumber, ImperialUnitsNeverShowDecimals)
{
    // item plan values (computed by running Windows); half-way values go to the even integer
    CHECK_STR_EQ(format_quantity_number(12.5, UOM_LINEAR_FEET), "12");
    CHECK_STR_EQ(format_quantity_number(13.5, UOM_LINEAR_FEET), "14");
    CHECK_STR_EQ(format_quantity_number(0.5, UOM_SQUARE_FEET), "0");
    CHECK_STR_EQ(format_quantity_number(1.5, UOM_SQUARE_FEET), "2");
    CHECK_STR_EQ(format_quantity_number(-0.4, UOM_LINEAR_FEET), "-0");
    CHECK_STR_EQ(format_quantity_number(1234567.891, UOM_LINEAR_FEET), "1,234,568");
    CHECK_STR_EQ(format_quantity_number(-1234567.891, UOM_LINEAR_FEET), "-1,234,568");
    CHECK_STR_EQ(format_quantity_number(999.5, UOM_EACH), "1,000"); // 999.5 -> 1000 (even), then grouped
    CHECK_STR_EQ(format_quantity_number(1e22, UOM_LINEAR_FEET), "10,000,000,000,000,000,000,000");
}

TEST(Qty12_QuantityNumber, MetricRoundingTiesFollowTheExactBinaryValue)
{
    // values computed by running Windows: f"{v:,.2f}" rounds the exact binary value
    // exactly representable ties go to the even digit (not away from zero like C's printf on some runtimes)
    CHECK_STR_EQ(format_quantity_number(0.125, UOM_M), "0.12");
    CHECK_STR_EQ(format_quantity_number(0.375, UOM_M), "0.38");
    CHECK_STR_EQ(format_quantity_number(0.625, UOM_M), "0.62");
    CHECK_STR_EQ(format_quantity_number(0.875, UOM_M), "0.88");
    CHECK_STR_EQ(format_quantity_number(99.875, UOM_M3), "99.88");
    CHECK_STR_EQ(format_quantity_number(-0.125, UOM_M), "-0.12");
    CHECK_STR_EQ(format_quantity_number(0.005, UOM_M), "0.01");   // 0.005 is slightly above
    CHECK_STR_EQ(format_quantity_number(0.015, UOM_M), "0.01");   // 0.015 is slightly below
    CHECK_STR_EQ(format_quantity_number(99.995, UOM_M), "100.00"); // below 100 but rounds up to it
    CHECK_STR_EQ(format_quantity_number(-99.995, UOM_M), "-100.00");
    CHECK_STR_EQ(format_quantity_number(99.999, UOM_M2), "100.00");
    CHECK_STR_EQ(format_quantity_number(-0.004, UOM_M), "-0.00");
    CHECK_STR_EQ(format_quantity_number(0.4, UOM_M), "0.40");
    CHECK_STR_EQ(format_quantity_number(100.4, UOM_M), "100");
    CHECK_STR_EQ(format_quantity_number(1.234, UOM_M), "1.23");
    CHECK_STR_EQ(format_quantity_number(123.4, UOM_M), "123");
}

TEST(Qty12_QuantityNumber, NonFiniteQuantitiesKeepPythonsSpelling)
{
    CHECK_STR_EQ(format_quantity_number(kNaN, UOM_LINEAR_FEET), "nan");
    CHECK_STR_EQ(format_quantity_number(kInf, UOM_M), "inf");
    CHECK_STR_EQ(format_quantity_number(-kInf, UOM_LINEAR_FEET), "-inf");
    CHECK_STR_EQ(format_quantity_number(-kNaN, UOM_M), "nan");
}

TEST(Qty12_QuantityNumber, TheTextDoesNotDependOnTheCLocale)
{
    // NSNumberFormatter followed the device locale (grouping separators); the Core formatter is fixed to "," and "."
    fmt_test::ScopedDecimalCommaLocale locale;
    if (!locale.active())
    {
        // no decimal-comma locale installed on this machine: nothing to switch to, the checks below still hold
    }
    CHECK_STR_EQ(format_quantity_number(12.345, UOM_M), "12.35");
    CHECK_STR_EQ(format_quantity_number(-99.99, UOM_M3), "-99.99");
    CHECK_STR_EQ(format_quantity_number(1234567.891, UOM_LINEAR_FEET), "1,234,568");
    CHECK_STR_EQ(format_quantity_with_uom(12.345, UOM_M), "12.35 m");
    CHECK_STR_EQ(format_quantity_with_uom(1234.0, UOM_CUBIC_YARDS), "1,234 CY");
}

TEST(Qty12_QuantityNumber, EveryWindowsOracleRecordOfTheNumberMatches)
{
    const fmt_test::Oracle &oracle = fmt_test::oracle();
    REQUIRE(oracle.loaded);
    REQUIRE(oracle.quantity.size() > 2000u);
    int mismatches = 0;
    for (const fmt_test::QuantityCase &c : oracle.quantity)
    {
        const std::string number = format_quantity_number(c.value, c.uom);
        const std::string zero_none = format_quantity_number(c.value, c.uom, "none");
        if (number != c.number || zero_none != c.number_zero_none)
        {
            if (++mismatches <= 8)
            {
                SCOPED_CONTEXT("value " + ost_test::detail::exact_float(c.value) + " uom " + std::to_string(c.uom));
                CHECK_STR_EQ(number, c.number);
                CHECK_STR_EQ(zero_none, c.number_zero_none);
            }
        }
    }
    CHECK_EQ(mismatches, 0);
}

// ---------------------------------------------------------------------------------------------------------------------
// QTY-22
// ---------------------------------------------------------------------------------------------------------------------

TEST(Qty22_ZeroQuantity, AZeroWithALabelReadsZeroAndTheUnit)
{
    // Port of test_quantity_display_facade_replacement_preserves_exact_formatting (with-uom half) and the plan of QTY-22
    CHECK_STR_EQ(format_quantity_with_uom(0.0, UOM_LINEAR_FEET), "0 LF");
    CHECK_STR_EQ(format_quantity_with_uom(0.0, UOM_EACH), "0 EA");
    CHECK_STR_EQ(format_quantity_with_uom(4.0, UOM_EACH), "4 EA");
    CHECK_STR_EQ(format_quantity_with_uom(-0.0, UOM_SQUARE_FEET), "0 SF");
    CHECK_STR_EQ(format_quantity_with_uom(0.0, UOM_M), "0 m");
}

TEST(Qty22_ZeroQuantity, AZeroWithoutALabelStaysEmpty)
{
    CHECK_STR_EQ(format_quantity_with_uom(0.0, 999), "");
    CHECK_STR_EQ(format_quantity_with_uom(0.0, -1), "");
    CHECK_STR_EQ(format_quantity_with_uom(12.6, -1), "13");
    // test_zero_placeholder_and_missing_label_do_not_invent_units: the label function may also answer "no label"
    CHECK_STR_EQ(format_quantity_with_uom(0.0, UOM_LINEAR_FEET, [](int) { return std::string(); }), "");
}

TEST(Qty22_ZeroQuantity, AnEmptyLabelLeavesTheBareNumber)
{
    // Port of QuantityDisplayLabelTests::test_an_empty_label_leaves_the_bare_number_with_no_trailing_space_or_zero
    const UomLabelFn no_label = [](int) { return std::string(); };
    CHECK_STR_EQ(format_quantity_with_uom(12.6, UOM_LINEAR_FEET, no_label), "13");
    CHECK_STR_EQ(format_quantity_with_uom(12.345, UOM_M, no_label), "12.35");
    CHECK_STR_EQ(format_quantity_with_uom(0.0, UOM_LINEAR_FEET, no_label), "");
}

TEST(Qty22_ZeroQuantity, TheLabelFunctionIsAskedForTheUnitExactlyOnce)
{
    // Port of QuantityDisplayLabelTests::test_the_label_function_is_asked_for_the_unit_of_the_quantity_exactly_once
    std::vector<int> asked;
    const UomLabelFn label = [&asked](int code)
    {
        asked.push_back(code);
        return std::string("SF");
    };
    CHECK_STR_EQ(format_quantity_with_uom(1234.0, UOM_M2, label), "1,234 SF");
    CHECK_EQ(asked.size(), 1u);
    if (!asked.empty())
        CHECK_EQ(asked[0], static_cast<int>(UOM_M2));
}

TEST(Qty22_ZeroQuantity, TheLabelsAreThoseOfTheUomService)
{
    CHECK_STR_EQ(format_quantity_with_uom(12.35, UOM_M2), "12.35 m\xC2\xB2");
    CHECK_STR_EQ(format_quantity_with_uom(0.0, UOM_M3), "0 m\xC2\xB3");
    CHECK_STR_EQ(format_quantity_with_uom(5.0, UOM_MM3), "5.00 mm\xC2\xB3");
    CHECK_STR_EQ(format_quantity_with_uom(7.0, UOM_SQUARE_INCHES), "7 IN\xC2\xB2");
    CHECK_STR_EQ(format_quantity_with_uom(7.0, UOM_SQUARE_ROOFING), "7 SQ");
}

TEST(Qty22_ZeroQuantity, ARowWithoutAQuantityEntryReadsZeroInEverySlotWhoseUnitHasALabel)
{
    // conditions_sidebar.update_quantities: qtys = quantities.get(cond_uid, (0.0, 0.0, 0.0)) for every row; an empty
    // text means the unit has no label. Windows reads UOM_EACH (0) as "EA".
    const std::array<std::string, 3> slots = format_condition_quantity_texts(nullptr, UOM_LINEAR_FEET, UOM_EACH, -1);
    CHECK_STR_EQ(slots[0], "0 LF");
    CHECK_STR_EQ(slots[1], "0 EA");
    CHECK_STR_EQ(slots[2], "");
}

TEST(Qty22_ZeroQuantity, ARowWithAQuantityEntryFormatsEachSlotWithItsOwnUnit)
{
    QuantityResult q;
    q.q1 = 12.6;
    q.q2 = 0.0;
    q.q3 = 1234.0;
    const std::array<std::string, 3> slots = format_condition_quantity_texts(&q, UOM_LINEAR_FEET, UOM_SQUARE_FEET, UOM_CUBIC_YARDS);
    CHECK_STR_EQ(slots[0], "13 LF");
    CHECK_STR_EQ(slots[1], "0 SF");
    CHECK_STR_EQ(slots[2], "1,234 CY");
    // a metric row
    q.q1 = 12.345;
    q.q2 = 3.0;
    q.q3 = 150.0;
    const std::array<std::string, 3> metric = format_condition_quantity_texts(&q, UOM_M, UOM_M2, UOM_M3);
    CHECK_STR_EQ(metric[0], "12.35 m");
    CHECK_STR_EQ(metric[1], "3.00 m\xC2\xB2");
    CHECK_STR_EQ(metric[2], "150 m\xC2\xB3");
}

TEST(Qty22_ZeroQuantity, EveryWindowsOracleRecordOfTheLabelledTextMatches)
{
    const fmt_test::Oracle &oracle = fmt_test::oracle();
    REQUIRE(oracle.loaded);
    int mismatches = 0;
    for (const fmt_test::QuantityCase &c : oracle.quantity)
    {
        const std::string text = format_quantity_with_uom(c.value, c.uom);
        if (text != c.with_label)
        {
            if (++mismatches <= 8)
            {
                SCOPED_CONTEXT("value " + ost_test::detail::exact_float(c.value) + " uom " + std::to_string(c.uom));
                CHECK_STR_EQ(text, c.with_label);
            }
        }
    }
    CHECK_EQ(mismatches, 0);
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-METRIC-01 (2-decimal quantity text)
// ---------------------------------------------------------------------------------------------------------------------

TEST(Metric01_QuantityText, MetricBidsShowTwoDecimalsBelowOneHundred)
{
    // item plan: format_quantity_number(1.234, 13) == '1.23', (123.4, 13) == '123'
    CHECK_STR_EQ(format_quantity_number(1.234, 13), "1.23");
    CHECK_STR_EQ(format_quantity_number(123.4, 13), "123");
    // the same rule applies to the unit codes 11..16 and to no other code
    for (int uom : {11, 12, 13, 14, 15, 16})
        CHECK_STR_EQ(format_quantity_number(5.5, uom), "5.50");
    for (int uom : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 17, 999, -1})
        CHECK_STR_EQ(format_quantity_number(5.5, uom), "6");
}
