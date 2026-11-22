// Dimension text of the Edit Condition dialog: FMT-01 (banker's rounding of the eighths), DLG-FINITE-01 (non-finite and
// overflowing text), COND-METRIC-01 (millimetre text of metric bids).
//
// Windows oracle: ost_visualizer/domain/services/dimension_format_service.py with
// tests/domain/services/test_dimension_format_service.py (ported below, same inputs and expected outputs) and
// tests/application/use_cases/project/test_condition_summary_service.py::test_metric_summary_formats_condition_height_in_millimetres.
// On top of the ported tests every record of Fixtures/formatting_oracle.txt (thousands of calls of the real Windows
// functions, read-only) is compared exactly.
#include "FormattingTestSupport.hpp"

#include "Domain/DimensionFormat.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <string>

using namespace ost_domain;

namespace
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();

    // counts every mismatch of a corpus loop; the first eight are reported in detail (note() is true), the total is
    // asserted at the end
    struct MismatchLog
    {
        int count = 0;
        bool note() { return ++count <= 8; }
    };

    // display_to_inches(text, metric) must be a number equal to `expected`
#define CHECK_PARSES(text, metric, expected)                                                                      \
    do                                                                                                            \
    {                                                                                                             \
        const std::optional<double> ost_parsed_ = display_to_inches((text), (metric));                            \
        if (!ost_parsed_.has_value())                                                                             \
            ::ost_test::report_failure(__FILE__, __LINE__, std::string("display_to_inches(\"") + (text) +         \
                                                              "\") is None, expected " + std::to_string(expected)); \
        else                                                                                                      \
            CHECK_EQ(*ost_parsed_, (expected));                                                                   \
    } while (0)

#define CHECK_REJECTED(text, metric)                                                                              \
    do                                                                                                            \
    {                                                                                                             \
        const std::optional<double> ost_parsed_ = display_to_inches((text), (metric));                            \
        if (ost_parsed_.has_value())                                                                              \
            ::ost_test::report_failure(__FILE__, __LINE__,                                                        \
                                       std::string("display_to_inches(\"") + fmt_test::show(text) +               \
                                           "\") accepted, expected None; got " + std::to_string(*ost_parsed_));   \
    } while (0)
}

// ---------------------------------------------------------------------------------------------------------------------
// FMT-01
// ---------------------------------------------------------------------------------------------------------------------

// Port of tests/domain/services/test_dimension_format_service.py::DimensionDisplayTableTests and the odd-sixteenth ties
// (values computed by running the Windows function: Python round() is round-half-even).
TEST(Fmt01_EighthRounding, OddSixteenthsRoundToTheEvenEighth)
{
    struct Row
    {
        double inches;
        const char *text;
    };
    const Row rows[] = {
        {0.0625, ""},          {-0.0625, ""},         {0.1875, "1/4\""},     {0.3125, "1/4\""},
        {0.4375, "1/2\""},     {0.5625, "1/2\""},     {0.6875, "3/4\""},     {0.8125, "3/4\""},
        {0.9375, "1\""},       {1.0625, "1\""},       {1.3125, "1 1/4\""},   {6.0625, "6\""},
        {12.0625, "1' 0\""},   {24.5625, "2' 0 1/2\""}, {11.9375, "1' 0\""}, {5.0625, "5\""},
        {17.0625, "1' 5\""},   {100.0625, "8' 4\""},  {95.9375, "8' 0\""},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("inches " + std::to_string(row.inches));
        CHECK_STR_EQ(inches_to_display(row.inches), row.text);
    }
}

TEST(Fmt01_EighthRounding, NegativeTiesMirrorThePositiveOnes)
{
    CHECK_STR_EQ(inches_to_display(-0.3125), "-1/4\"");
    CHECK_STR_EQ(inches_to_display(-0.5625), "-1/2\"");
    CHECK_STR_EQ(inches_to_display(-12.0625), "-1' 0\"");
    CHECK_STR_EQ(inches_to_display(-1.3125), "-1 1/4\"");
}

TEST(Fmt01_EighthRounding, EveryEighthHasItsFractionTextBelowOneInch)
{
    // Port of DimensionDisplayTableTests::test_every_eighth_has_its_fraction_text_below_one_inch
    const char *texts[] = {"", "1/8\"", "1/4\"", "3/8\"", "1/2\"", "5/8\"", "3/4\"", "7/8\""};
    for (int eighths = 0; eighths < 8; ++eighths)
    {
        SCOPED_CONTEXT("eighths " + std::to_string(eighths));
        const std::string text = texts[eighths];
        CHECK_STR_EQ(inches_to_display(eighths / 8.0), text);
        CHECK_STR_EQ(inches_to_display(-eighths / 8.0), text.empty() ? std::string() : "-" + text);
    }
}

TEST(Fmt01_EighthRounding, InchesAndFeetLayoutsForEveryCombinationOfParts)
{
    // Port of DimensionDisplayTableTests::test_inches_and_feet_layouts_for_every_combination_of_parts
    struct Row
    {
        double inches;
        const char *text;
    };
    const Row rows[] = {
        {5, "5\""},          {11, "11\""},          {5.5, "5 1/2\""},     {11.875, "11 7/8\""},
        {12.125, "1' 0 1/8\""}, {12.0, "1' 0\""},   {13, "1' 1\""},       {13.375, "1' 1 3/8\""},
        {24, "2' 0\""},      {100, "8' 4\""},       {-5, "-5\""},         {-5.5, "-5 1/2\""},
        {-13, "-1' 1\""},    {-12.5, "-1' 0 1/2\""}, {-12, "-1' 0\""},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("inches " + std::to_string(row.inches));
        CHECK_STR_EQ(inches_to_display(row.inches), row.text);
    }
}

TEST(Fmt01_EighthRounding, RoundingGoesToTheNearestEighthAndCanCarryIntoFeet)
{
    // Port of DimensionDisplayTableTests::test_rounding_goes_to_the_nearest_eighth_and_can_carry_into_feet
    CHECK_STR_EQ(inches_to_display(0.06), "");
    CHECK_STR_EQ(inches_to_display(0.07), "1/8\"");
    CHECK_STR_EQ(inches_to_display(11.99), "1' 0\"");
    CHECK_STR_EQ(inches_to_display(-0.06), "");
    CHECK_STR_EQ(inches_to_display(-0.07), "-1/8\"");
    CHECK_STR_EQ(inches_to_display(0.0), "");
    CHECK_STR_EQ(inches_to_display(-0.0), "");
}

TEST(Fmt01_EighthRounding, ValidImperialDimensionsRoundTrip)
{
    // Port of DimensionFormattingTests::test_valid_imperial_and_metric_dimensions_have_exact_semantics (imperial half)
    struct Row
    {
        double inches;
        const char *text;
    };
    const Row rows[] = {{0, ""}, {0.125, "1/8\""}, {1.5, "1 1/2\""}, {12, "1' 0\""}, {-14.25, "-1' 2 1/4\""}};
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("inches " + std::to_string(row.inches));
        CHECK_STR_EQ(inches_to_display(row.inches), row.text);
        CHECK_PARSES(row.text, false, row.inches);
    }
}

TEST(Fmt01_EighthRounding, DisplayAndParseRoundTripEveryEighthUpToThirtySixFeet)
{
    // Port of test_display_and_parse_round_trip_every_eighth_up_to_three_feet (the item's plan widens it to 36 ft)
    for (int eighths = 0; eighths <= 8 * 36 * 12; ++eighths)
    {
        const double inches = eighths / 8.0;
        SCOPED_CONTEXT("inches " + std::to_string(inches));
        CHECK_PARSES(inches_to_display(inches), false, inches);
        CHECK_PARSES(inches_to_display(-inches), false, -inches);
    }
}

TEST(Fmt01_EighthRounding, NonFiniteInputGivesABlankText)
{
    // Port of DimensionFormattingTests::test_dimension_text_rejects_non_finite_values_before_formatting (display half)
    for (double value : {kNaN, kInf, -kInf})
    {
        SCOPED_CONTEXT("value " + std::to_string(value));
        CHECK_STR_EQ(inches_to_display(value), "");
        CHECK_STR_EQ(inches_to_display(value, true), "");
        CHECK_STR_EQ(mm_to_display(value), "");
    }
}

TEST(Fmt01_EighthRounding, HugeValuesKeepEveryFeetDigit)
{
    // Python ints are exact: 1e20 inches = 8333333333333333333' 4" (values computed by running Windows)
    CHECK_STR_EQ(inches_to_display(1e20), "8333333333333333333' 4\"");
    CHECK_STR_EQ(inches_to_display(1e17), "8333333333333333' 4\"");
    CHECK_STR_EQ(inches_to_display(1.2345678901234568e17), "10288065751028806' 8\"");
    CHECK_STR_EQ(inches_to_display(2500000000000000.0), "208333333333333' 4\"");
    CHECK_STR_EQ(inches_to_display(-1e20), "-8333333333333333333' 4\"");
}

TEST(Fmt01_EighthRounding, TheValueWhoseEighthsOverflowADoubleGivesABlankText)
{
    // Windows raises OverflowError (round(inf)); the dialog must not crash, so iOS shows no text (cannot be reproduced)
    CHECK_STR_EQ(inches_to_display(std::numeric_limits<double>::max()), "");
    CHECK_STR_EQ(inches_to_display(-std::numeric_limits<double>::max()), "");
    const fmt_test::Oracle &oracle = fmt_test::oracle();
    REQUIRE(oracle.loaded);
    CHECK_EQ(oracle.overflow.size(), 2u);
    for (double value : oracle.overflow)
        CHECK_STR_EQ(inches_to_display(value), "");
}

TEST(Fmt01_EighthRounding, EveryWindowsOracleRecordOfInchesToDisplayMatches)
{
    const fmt_test::Oracle &oracle = fmt_test::oracle();
    REQUIRE(oracle.loaded);
    REQUIRE(oracle.inches.size() > 5000u);
    MismatchLog log;
    for (const fmt_test::InchesCase &c : oracle.inches)
    {
        const std::string imperial = inches_to_display(c.inches);
        const std::string metric = inches_to_display(c.inches, true);
        if ((imperial != c.imperial || metric != c.metric) && log.note())
        {
            SCOPED_CONTEXT("inches " + ost_test::detail::exact_float(c.inches));
            CHECK_STR_EQ(imperial, c.imperial);
            CHECK_STR_EQ(metric, c.metric);
        }
    }
    CHECK_EQ(log.count, 0);
}

// ---------------------------------------------------------------------------------------------------------------------
// DLG-FINITE-01
// ---------------------------------------------------------------------------------------------------------------------

TEST(Finite01_DimensionParsing, NonFiniteTextIsRejectedInEveryMode)
{
    // Port of DimensionFormattingTests::test_dimension_text_rejects_non_finite_values_before_formatting (parse half)
    for (const char *text : {"nan", "inf", "-inf", "Infinity", "-Infinity"})
    {
        SCOPED_CONTEXT(std::string("text ") + text);
        CHECK_REJECTED(text, false);
        CHECK_REJECTED(text, true);
        CHECK(!display_to_mm(text).has_value());
    }
    CHECK_REJECTED(std::string(5000, '9') + "'", false);
}

TEST(Finite01_DimensionParsing, OverflowingDigitRunsAreRejectedInEveryLayout)
{
    // Port of DimensionParsingTableTests::test_overflowing_digit_runs_are_rejected_in_every_layout
    const std::string huge(5000, '9');
    const std::string texts[] = {
        huge + "'",         huge + "' 3",       "3' " + huge,     huge + "' 1/2",
        huge + "' 3 1/2",   "3' " + huge + " 1/2", huge + " 1/2", huge,
        "-" + huge,
    };
    for (const std::string &text : texts)
    {
        SCOPED_CONTEXT("text " + text.substr(0, 12));
        CHECK_REJECTED(text, false);
    }
}

TEST(Finite01_DimensionParsing, HugeFeetThatOverflowWhenMultipliedAreRejected)
{
    // 308 digits are a finite double (1e308) but feet * 12 is not; 307 digits (1e307 * 12 = 1.2e308) still fit
    // (both cases are in the Windows oracle as well)
    CHECK_REJECTED(std::string(308, '9') + "'", false);
    const std::string feet307(307, '9');
    CHECK_PARSES(feet307 + "'", false, std::strtod(feet307.c_str(), nullptr) * 12.0);
    CHECK_PARSES("1e308", false, 1e308);
    CHECK_REJECTED("1e400", false);
    CHECK_REJECTED("-1e400", false);
}

TEST(Finite01_DimensionParsing, UnparseableTextAndUnsupportedFractionsAreRejected)
{
    // Port of DimensionParsingTableTests::test_unparseable_text_and_unsupported_fractions_are_rejected
    for (const char *text : {"abc", "1/3", "2 1/3", "3' 1/3", "3' 2 1/3", "1/0", "3' 4 5", "3 ft", "1 2 3", "--5",
                             "5'' 3", "3' 4 1/2 3"})
    {
        SCOPED_CONTEXT(std::string("text ") + text);
        CHECK_REJECTED(text, false);
        CHECK_REJECTED(text, true);
    }
}

TEST(Finite01_DimensionParsing, TrailingTextAfterEveryLayoutIsRejectedNotIgnored)
{
    // Port of DimensionParsingTableTests::test_trailing_text_after_every_layout_is_rejected_not_ignored
    struct Layout
    {
        const char *valid;
        const char *junk;
    };
    const Layout layouts[] = {{"3' 4 1/2", "x"}, {"3' 4", "x"},     {"3' 1/2", "x"}, {"3'", "x"},
                              {"2 1/2", " junk"}, {"1/2", "x"},     {"4", "x"}};
    for (const Layout &layout : layouts)
    {
        SCOPED_CONTEXT(std::string("layout ") + layout.valid);
        CHECK(display_to_inches(layout.valid).has_value());
        CHECK_REJECTED(std::string(layout.valid) + layout.junk, false);
        std::string junk = layout.junk;
        while (!junk.empty() && junk.front() == ' ')
            junk.erase(junk.begin());
        CHECK_REJECTED(std::string(layout.valid) + "\" " + junk, false);
    }
}

TEST(Finite01_DimensionParsing, EverySupportedImperialLayoutParsesToInches)
{
    // Port of DimensionParsingTableTests::test_every_supported_imperial_layout_parses_to_inches
    struct Row
    {
        const char *text;
        double inches;
    };
    const Row rows[] = {
        {"5", 5.0},          {"5.5", 5.5},          {"5\"", 5.0},           {"  5 \" ", 5.0},      {"-5", -5.0},
        {"- 5.25", -5.25},   {"1/2", 0.5},          {"1/8\"", 0.125},       {"-3/4", -0.75},       {"2 1/2", 2.5},
        {"2 3/8\"", 2.375},  {"-2 5/8", -2.625},    {"3'", 36.0},           {"-3'", -36.0},        {"0'", 0.0},
        {"3' 4", 40.0},      {"3'4", 40.0},         {"3'-4", 40.0},         {"3' 4.5\"", 40.5},    {"-3' 4", -40.0},
        {"3' 1/2", 36.5},    {"3'1/2\"", 36.5},     {"-3' 7/8", -36.875},   {"1' 2 1/4\"", 14.25}, {"1'-2 1/4", 14.25},
        {"-1' 2 3/4\"", -14.75}, {"10' 11 7/8\"", 10 * 12 + 11.875},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(std::string("text ") + row.text);
        CHECK_PARSES(row.text, false, row.inches);
    }
}

TEST(Finite01_DimensionParsing, EveryEighthFractionParsesInEachFractionLayout)
{
    // Port of DimensionParsingTableTests::test_every_eighth_fraction_parses_in_each_fraction_layout
    for (int numerator = 1; numerator < 8; ++numerator)
    {
        const double frac = numerator / 8.0;
        std::string text = std::to_string(numerator) + "/8";
        if (numerator == 2)
            text = "1/4";
        else if (numerator == 4)
            text = "1/2";
        else if (numerator == 6)
            text = "3/4";
        SCOPED_CONTEXT("fraction " + text);
        CHECK_PARSES(text, false, frac);
        CHECK_PARSES("2 " + text, false, 2 + frac);
        CHECK_PARSES("2' " + text, false, 24 + frac);
        CHECK_PARSES("2' 3 " + text, false, 27 + frac);
        CHECK_PARSES("-2' 3 " + text, false, -(27 + frac));
        CHECK_PARSES("-" + text, false, -frac);
    }
}

TEST(Finite01_DimensionParsing, BlankInputIsZeroAndPlainNumbersFallBackToFloatParsing)
{
    // Port of DimensionParsingTableTests::test_blank_input_is_zero_and_plain_numbers_fall_back_to_float_parsing
    for (const char *text : {"", "   ", "\t"})
    {
        CHECK_PARSES(text, false, 0.0);
        CHECK_PARSES(text, true, 0.0);
        const std::optional<double> mm = display_to_mm(text);
        CHECK(mm.has_value());
        if (mm)
            CHECK_EQ(*mm, 0.0);
    }
    CHECK_PARSES("1e2", false, 100.0);
    CHECK_PARSES(".5", false, 0.5);
    CHECK_PARSES("+7", false, 7.0);
}

TEST(Finite01_DimensionParsing, PythonFloatSyntaxDecidesTheFallback)
{
    // float() accepts underscores between digits, a trailing dot and Unicode digits / spaces (values computed by
    // running Windows); every other shape is rejected
    CHECK_PARSES("1_000", false, 1000.0);
    CHECK_PARSES("5.", false, 5.0);
    CHECK_PARSES("1.5e3", false, 1500.0);
    CHECK_PARSES("5\xC2\xA0", false, 5.0);               // trailing NBSP is stripped
    CHECK_PARSES("3'\xC2\xA0" "4", false, 40.0);          // NBSP is the regex \s
    CHECK_PARSES("\xD9\xA3'", false, 36.0);               // ARABIC-INDIC DIGIT THREE is the regex \d
    CHECK_PARSES("\xD9\xA1\xD9\xA2\xD9\xA3", false, 123.0);
    CHECK_REJECTED("1__0", false);
    CHECK_REJECTED("_1", false);
    CHECK_REJECTED("0x10", false);
    CHECK_REJECTED("5e", false);
    CHECK_REJECTED("3' 4 \xC2\xBD", false); // VULGAR FRACTION ONE HALF is not a supported fraction
}

TEST(Finite01_DimensionParsing, EveryWindowsOracleRecordOfTheParserMatches)
{
    const fmt_test::Oracle &oracle = fmt_test::oracle();
    REQUIRE(oracle.loaded);
    REQUIRE(oracle.parse.size() > 3000u);
    MismatchLog log;
    for (const fmt_test::ParseCase &c : oracle.parse)
    {
        const std::optional<double> imperial = display_to_inches(c.text);
        const std::optional<double> metric = display_to_inches(c.text, true);
        const std::optional<double> mm = display_to_mm(c.text);
        auto differs = [](const std::optional<double> &got, const fmt_test::OptDouble &want)
        { return got.has_value() != want.ok || (got.has_value() && *got != want.value); };
        if (differs(imperial, c.imperial) || differs(metric, c.metric) || differs(mm, c.mm))
        {
            if (log.note())
            {
                SCOPED_CONTEXT("text \"" + fmt_test::show(c.text) + "\"");
                CHECK_EQ(imperial.has_value(), c.imperial.ok);
                CHECK_EQ(metric.has_value(), c.metric.ok);
                CHECK_EQ(mm.has_value(), c.mm.ok);
                if (imperial && c.imperial.ok)
                    CHECK_EQ(*imperial, c.imperial.value);
                if (metric && c.metric.ok)
                    CHECK_EQ(*metric, c.metric.value);
                if (mm && c.mm.ok)
                    CHECK_EQ(*mm, c.mm.value);
            }
        }
    }
    CHECK_EQ(log.count, 0);
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-METRIC-01 (millimetre text)
// ---------------------------------------------------------------------------------------------------------------------

TEST(Metric01_MillimetreText, ValidMetricDimensionsHaveExactSemantics)
{
    // Port of DimensionFormattingTests::test_valid_imperial_and_metric_dimensions_have_exact_semantics (metric half)
    CHECK_EQ(DIM_MM_PER_INCH, 25.4);
    CHECK_EQ(inches_to_mm(1), 25.4);
    CHECK_EQ(mm_to_inches(25.4), 1.0);
    CHECK_STR_EQ(inches_to_display(1, true), "25.4");
    CHECK_PARSES("25.4 mm", true, 1.0);
    const std::optional<double> mm = display_to_mm("25.4 mm");
    REQUIRE(mm.has_value());
    CHECK_EQ(*mm, 25.4);
    CHECK_STR_EQ(mm_to_display(-12.5), "-12.5");
}

TEST(Metric01_MillimetreText, MetricDisplayDropsATrailingZeroAndKeepsSixSignificantDigits)
{
    // Port of DimensionDisplayTableTests::test_metric_display_drops_a_trailing_zero_and_keeps_six_significant_digits
    struct Row
    {
        double mm;
        const char *text;
    };
    const Row rows[] = {{0.0, ""},       {-0.0, ""},        {5.0, "5"},          {-5.0, "-5"},
                        {25.4, "25.4"},  {-12.5, "-12.5"},  {0.5, "0.5"},        {1234567.0, "1234567"},
                        {0.000123, "0.000123"}, {1.23456789, "1.23457"}};
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("mm " + std::to_string(row.mm));
        CHECK_STR_EQ(mm_to_display(row.mm), row.text);
    }
    CHECK_STR_EQ(inches_to_display(2.0, true), "50.8");
    CHECK_STR_EQ(inches_to_display(0.0, true), "");
    CHECK_STR_EQ(inches_to_display(1.0, false), "1\"");
}

TEST(Metric01_MillimetreText, ExponentFormsFollowPythonsGFormat)
{
    // values computed by running Windows: format(mm, "g") gives two-digit exponents like C
    CHECK_STR_EQ(mm_to_display(1e-7), "1e-07");
    CHECK_STR_EQ(mm_to_display(1234567.5), "1.23457e+06");
    CHECK_STR_EQ(mm_to_display(123456.7), "123457");
    CHECK_STR_EQ(mm_to_display(5e-324), "4.94066e-324");
    CHECK_STR_EQ(mm_to_display(1e20), "100000000000000000000");
}

TEST(Metric01_MillimetreText, AnExactTieAtTheSixthSignificantDigitGoesToTheEvenDigit)
{
    // values computed by running Windows: format(mm, "g") rounds the exact binary value, an exactly representable
    // tie (seven significant digits ending in 5) goes to the even digit, in positive and negative values and in
    // fixed and small-magnitude notation. The text comes from the C library's "%g", which must round the same way.
    struct Row
    {
        double mm;
        const char *text;
    };
    const Row rows[] = {{100000.5, "100000"},   {100001.5, "100002"},   {100002.5, "100002"},
                        {123456.5, "123456"},   {123457.5, "123458"},   {-100000.5, "-100000"},
                        {131072.5, "131072"},   {131073.5, "131074"},   {2048.125, "2048.12"},
                        {2048.375, "2048.38"},  {-2048.125, "-2048.12"}, {0.0009765625, "0.000976562"},
                        {-0.0009765625, "-0.000976562"}};
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("mm " + ost_test::detail::exact_float(row.mm));
        CHECK_STR_EQ(mm_to_display(row.mm), row.text);
    }
}

TEST(Metric01_MillimetreText, TheTextDoesNotDependOnTheCLocale)
{
    // printf would write "25,4" under a decimal-comma locale; the dialog text is always "25.4"
    fmt_test::ScopedDecimalCommaLocale locale;
    CHECK_STR_EQ(mm_to_display(25.4), "25.4");
    CHECK_STR_EQ(mm_to_display(-12.5), "-12.5");
    CHECK_STR_EQ(mm_to_display(1.23456789), "1.23457");
    CHECK_STR_EQ(inches_to_display(1.0, true), "25.4");
    CHECK_STR_EQ(inches_to_display(1.0), "1\"");
    CHECK_STR_EQ(mm_to_display(1e20), "100000000000000000000");
}

TEST(Metric01_MillimetreText, TheSummaryHeightOfA24InchConditionIs609Point6)
{
    // Port of test_condition_summary_service.py::test_metric_summary_formats_condition_height_in_millimetres
    CHECK_STR_EQ(inches_to_display(24.0, true), "609.6");
}

TEST(Metric01_MillimetreText, MetricInputAcceptsAnMmSuffixWithAnyCaseAndSpaces)
{
    // Port of DimensionParsingTableTests::test_metric_input_accepts_an_mm_suffix_with_any_case_and_spaces
    struct Row
    {
        const char *text;
        double mm;
    };
    const Row rows[] = {{"25.4", 25.4}, {"25.4mm", 25.4}, {"25.4 MM", 25.4}, {"25.4 Mm", 25.4}, {"  -3  m ", -3.0}, {"1e1", 10.0}};
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(std::string("text ") + row.text);
        const std::optional<double> mm = display_to_mm(row.text);
        REQUIRE(mm.has_value());
        CHECK_EQ(*mm, row.mm);
        const std::optional<double> inches = display_to_inches(row.text, true);
        REQUIRE(inches.has_value());
        CHECK_NEAR(*inches, row.mm / 25.4, 5e-8);
    }
    CHECK(!display_to_mm("12 cm").has_value());
    CHECK(!display_to_mm("mm mm x").has_value());
}

TEST(Metric01_MillimetreText, OnlyATrailingMUnitIsStrippedFromMetricText)
{
    // Port of DimensionParsingTableTests::test_only_a_trailing_m_unit_is_stripped_from_metric_text
    for (const char *text : {"5X", "5x", "5 mX", "5 Xm X"})
    {
        SCOPED_CONTEXT(std::string("text ") + text);
        CHECK(!display_to_mm(text).has_value());
    }
    const std::optional<double> five = display_to_mm("5mmm");
    REQUIRE(five.has_value());
    CHECK_EQ(*five, 5.0);
    // rstrip("m") then rstrip("M"): an "m" after an "M" stays and makes the number invalid (computed by running Windows)
    CHECK(!display_to_mm("5mMm").has_value());
    CHECK(!display_to_mm("mm").has_value());
}

TEST(Metric01_MillimetreText, DisplayAndParseRoundTripMillimetres)
{
    // Port of test_display_and_parse_round_trip_every_eighth_up_to_three_feet (metric half)
    for (double mm : {0.0, 1.0, -7.5, 250.25})
    {
        SCOPED_CONTEXT("mm " + std::to_string(mm));
        const std::optional<double> back = display_to_mm(mm_to_display(mm));
        REQUIRE(back.has_value());
        CHECK_EQ(*back, mm);
        const std::optional<double> inches = display_to_inches(inches_to_display(mm_to_inches(mm), true), true);
        REQUIRE(inches.has_value());
        CHECK_NEAR(*inches, mm_to_inches(mm), 5e-8);
    }
}

TEST(Metric01_MillimetreText, UnitConversionHelpersAreInverseAndLinear)
{
    // Port of DimensionParsingTableTests::test_unit_conversion_helpers_are_inverse_and_linear
    CHECK_NEAR(mm_to_inches(50.8), 2.0, 5e-8);
    CHECK_NEAR(inches_to_mm(2.0), 50.8, 5e-8);
    CHECK_EQ(mm_to_inches(0), 0.0);
    CHECK_EQ(inches_to_mm(-1), -25.4);
    CHECK_NEAR(mm_to_inches(inches_to_mm(3.3)), 3.3, 5e-8);
}

TEST(Metric01_MillimetreText, EveryWindowsOracleRecordOfMmToDisplayMatches)
{
    const fmt_test::Oracle &oracle = fmt_test::oracle();
    REQUIRE(oracle.loaded);
    REQUIRE(oracle.mm.size() > 900u);
    MismatchLog log;
    for (const fmt_test::MmCase &c : oracle.mm)
    {
        const std::string got = mm_to_display(c.mm);
        if (got != c.text && log.note())
        {
            SCOPED_CONTEXT("mm " + ost_test::detail::exact_float(c.mm));
            CHECK_STR_EQ(got, c.text);
        }
    }
    CHECK_EQ(log.count, 0);
}
