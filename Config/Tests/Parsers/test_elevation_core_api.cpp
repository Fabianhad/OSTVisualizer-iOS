// Core API of Domain/PositionParser.hpp: ports of the helper-level cases of
// tests/infrastructure/parsers/test_position_parser.py and tests/domain/services/test_elevation.py
// (parse_elevation), plus the Python float() / repr(float) behaviour the ports rely on.
#include "ParsersTestSupport.hpp"

#include "Domain/PositionParser.hpp"

#include <cmath>
#include <limits>

using namespace ost_domain;

namespace
{
    double frac(const std::string &text)
    {
        double v = 0.0;
        std::string error;
        SCOPED_CONTEXT("fractional text=" + text);
        REQUIRE(parse_fractional_inches_text(text, v, &error));
        return v;
    }

    bool frac_fails(const std::string &text, std::string *error = nullptr)
    {
        double v = 0.0;
        return !parse_fractional_inches_text(text, v, error);
    }

    double imperial(const std::string &text)
    {
        double v = -1.0;
        std::string error;
        SCOPED_CONTEXT("imperial text=" + text);
        REQUIRE(parse_imperial_elevation(text, v, &error));
        return v;
    }

    double metric(const std::string &text) { return parse_metric_elevation(text); }

    std::string replace_all(std::string s, const std::string &from, const std::string &to)
    {
        for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size())
            s.replace(pos, from.size(), to);
        return s;
    }

    std::string replace_first(std::string s, const std::string &from, const std::string &to)
    {
        const size_t pos = s.find(from);
        if (pos != std::string::npos)
            s.replace(pos, from.size(), to);
        return s;
    }
}

// ---------------------------------------------------------------------------------------------------------
// ElevationTextValidityCharacterizationTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationCore, EveryAcceptedElevationTextShape)
{
    // Port of ...::ElevationTextValidityCharacterizationTests::test_every_accepted_elevation_text_shape
    for (const char *text : {"745' 0\"", "12' 6 1/2\"", "12'-6\"", "5' 0\"", "5'x", "6\"", "1/2\"", "3 1/2\"", "1m 20cm",
                             "1m and 20cm", "1m+20cm", "2,5 m", "3 meters", "30 cm", "30 centimeters", "-4' 2\""})
    {
        SCOPED_CONTEXT(std::string("text=") + text);
        CHECK(is_elevation_text(text));
    }
}

TEST(ElevationCore, EveryRejectedElevationTextShape)
{
    // Port of ...::test_every_rejected_elevation_text_shape
    for (const char *text : {"", "later", "5pm", "abc 5'", "'", "m", "@T 5'", "5'", "-4'", "5' wall"})
    {
        SCOPED_CONTEXT(std::string("text=") + text);
        CHECK_FALSE(is_elevation_text(text));
    }
}

TEST(ElevationCore, TextThatStartsLikeOneUnitIsStillAValidGate)
{
    // Port of ...::MixedUnitAndCacheTests (the gate half: "a quote or an m starts a valid elevation")
    for (const std::string &tail : {std::string("5'cm"), std::string("5" OST_U_PRIME "cm"), std::string("5'm"),
                                    std::string("6\"m"), std::string("5m'"), std::string("3 meters'")})
    {
        SCOPED_CONTEXT("tail=" + tail);
        CHECK(is_elevation_text(tail));
    }
}

// ---------------------------------------------------------------------------------------------------------
// FractionalInchesParsingTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationCore, WholeFractionalAndMixedInches)
{
    // Port of ...::FractionalInchesParsingTests::test_whole_fractional_and_mixed_inches
    CHECK_EQ(frac("6"), 6.0);
    CHECK_EQ(frac("1/2"), 0.5);
    CHECK_EQ(frac("3/4"), 0.75);
    CHECK_EQ(frac("3 1/2"), 3.5);
    CHECK_EQ(frac("3 3/4"), 3.75);
    CHECK_EQ(frac("3 0.5"), 3.5);
    CHECK_EQ(frac("  6 "), 6.0);
}

TEST(ElevationCore, ALeadingMinusNegatesTheWholeValueIncludingItsFraction)
{
    // Port of ...::test_a_leading_minus_negates_the_whole_value_including_its_fraction
    CHECK_EQ(frac("-6"), -6.0);
    CHECK_EQ(frac("-12"), -12.0);
    CHECK_EQ(frac("- 6"), -6.0);
    CHECK_EQ(frac("-1/2"), -0.5);
    CHECK_EQ(frac("-3 1/2"), -3.5);
}

TEST(ElevationCore, TheHelperToleratesSpacesAroundTheSlashOfAMixedNumber)
{
    // Port of ...::test_the_helper_tolerates_spaces_around_the_slash_of_a_mixed_number
    CHECK_EQ(frac("3 1 / 2"), 3.5);
    CHECK_EQ(frac("-3 1 /2"), -3.5);
}

TEST(ElevationCore, AnUnreadableWholePartCountsAsZero)
{
    // Port of ...::test_an_unreadable_whole_part_counts_as_zero
    CHECK_EQ(frac("x 1/2"), 0.5);
    CHECK_EQ(frac("x 0.5"), 0.5);
}

TEST(ElevationCore, UnreadableValuesAreRefused)
{
    // Port of ...::test_unreadable_values_are_refused (ValueError)
    for (const char *text : {"", "abc", "1/0x", "1/2/3"})
    {
        SCOPED_CONTEXT(std::string("text=") + text);
        CHECK(frac_fails(text));
    }
}

TEST(ElevationCore, AnyUnicodeWhitespaceSeparatesAMixedNumberLikeASpace)
{
    // Port of ...::test_any_unicode_whitespace_separates_a_mixed_number_like_a_space
    for (const auto &s : ost_parsers_test::unicode_separators())
    {
        SCOPED_CONTEXT(std::string("separator=") + s.label);
        CHECK_EQ(frac("3" + s.text + "1/2"), 3.5);
        CHECK_EQ(frac("3" + s.text + "3/4"), 3.75);
        CHECK_EQ(frac("-3" + s.text + "1/2"), -3.5);
        CHECK_EQ(frac("-" + s.text + "3" + s.text + "1/2"), -3.5);
    }
}

TEST(ElevationCore, UnicodeWhitespaceDoesNotMakeAMalformedFractionReadable)
{
    // Port of ...::test_unicode_whitespace_does_not_make_a_malformed_fraction_readable
    for (const char *text : {"1 /2/3", "abc ", "1 1/0x", " "})
    {
        SCOPED_CONTEXT(std::string("text=") + text);
        CHECK(frac_fails(text));
    }
}

// ---------------------------------------------------------------------------------------------------------
// ZeroDenominatorElevationTests / ElevationValueBoundaryTests (helper level and messages)
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationCore, TheFractionHelperRefusesAZeroDenominator)
{
    // Port of ...::ZeroDenominatorElevationTests::test_the_fraction_helper_refuses_a_zero_denominator_with_a_value_error
    for (const char *text : {"1/0", "-1/0", "3 1/0", "-3 1/0", "3 1/00", "0/0"})
    {
        SCOPED_CONTEXT(std::string("text=") + text);
        std::string error;
        CHECK(frac_fails(text, &error));
        CHECK(error.find("zero denominator") != std::string::npos);
    }
}

TEST(ElevationCore, AZeroNumeratorOverARealDenominatorIsStillZero)
{
    // Port of ...::test_a_zero_numerator_over_a_real_denominator_is_still_zero
    CHECK_EQ(frac("0/4"), 0.0);
    CHECK_EQ(frac("3 0/4"), 3.0);
}

TEST(ElevationCore, ADenominatorThatIsZeroByValueIsRefused)
{
    // Port of ...::ElevationValueBoundaryTests::test_a_denominator_that_is_zero_by_value_is_refused
    for (const char *text : {"1/0.0", "1/-0", "1/0e0", "3 1/0.0"})
    {
        SCOPED_CONTEXT(std::string("text=") + text);
        std::string error;
        CHECK(frac_fails(text, &error));
        CHECK(error.find("zero denominator") != std::string::npos);
    }
}

TEST(ElevationCore, OnlyAZeroDenominatorIsRefusedNotANegativeOne)
{
    // Port of ...::test_only_a_zero_denominator_is_refused_not_a_negative_one
    CHECK_EQ(frac("1/-1"), -1.0);
    CHECK_EQ(frac("-1/-4"), 0.25);
}

TEST(ElevationCore, TheFractionHelperReadsDecimalPartsAndTinyDenominators)
{
    // Port of ...::PrivateParserSearchSemanticsTests::test_the_fraction_helper_reads_decimal_parts_and_tiny_denominators
    CHECK_EQ(frac("1.5/2"), 0.75);
    CHECK_EQ(frac("6.5 1/2"), 7.0);
    CHECK_EQ(frac("1/0.0000001"), 1e7);
    CHECK_EQ(frac("1/1e-12"), 1e12);
    // ...::test_a_very_large_denominator_gives_a_tiny_value_not_zero_or_an_error
    CHECK_EQ(frac("1/1000000"), 1e-06);
}

TEST(ElevationCore, TheZeroDenominatorErrorNamesTheOffendingFraction)
{
    // Port of ...::test_the_zero_denominator_error_names_the_offending_fraction (assertRaisesRegex ^zero denominator in fraction X$)
    const std::pair<const char *, const char *> cases[] = {
        {"Wall @B 1/0\"", "1/0"}, {"Wall @B 6 1/00\"", "1/00"}, {"Wall @T 12' 6 3/0\"", "3/0"}, {"Wall @B 0/0\"", "0/0"}};
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT(std::string("name=") + c.first);
        double z = 7.0;
        bool top = true;
        std::string error;
        CHECK_FALSE(extract_z_value_from_name(c.first, z, top, &error));
        CHECK_STR_EQ(error, std::string("zero denominator in fraction ") + c.second);
        // a failed lookup reports (0.0, false) in the out parameters
        CHECK_EQ(z, 0.0);
        CHECK_EQ(top, false);
    }
}

TEST(ElevationCore, ZeroDenominatorWithAnyWhitespaceFlavourFails)
{
    // Port of ...::UnicodeWhitespaceBoundaryTests::test_zero_denominators_stay_value_errors_with_any_whitespace_flavour
    for (const auto &s : ost_parsers_test::unicode_separators())
    {
        for (const std::string &text : {"6" + s.text + "1/0\"", "12'" + s.text + "6" + s.text + "1/0\"", "-6" + s.text + "0/0\""})
        {
            SCOPED_CONTEXT(std::string("separator=") + s.label + " text=" + text);
            double z = 0.0;
            bool top = false;
            CHECK_FALSE(extract_z_value_from_name("Wall @B " + text, z, top));
        }
    }
}

TEST(ElevationCore, NamesWithAValidElevationSucceed)
{
    double z = 0.0;
    bool top = false;
    CHECK(extract_z_value_from_name("Slab @T 12' 6 1/2\"", z, top));
    CHECK_EQ(z, 150.5);
    CHECK_EQ(top, true);
    CHECK(extract_z_value_from_name("Wall @B 5' 1/2\"", z, top)); // outside the grammar: no elevation, not an error
    CHECK_EQ(z, 0.0);
    CHECK_EQ(top, false);
}

TEST(ElevationCore, AFeetRunOfMoreThan4300DigitsIsAValueErrorLeadingZerosCounting)
{
    // Not in the Windows tests: values computed by running Windows extract_z_value_from_name (Python 3.11.4).
    // int(feet_str) refuses more than 4300 digits (sys.int_info.default_max_str_digits) with a ValueError, leading
    // zeros included; the Windows MDB reader catches it and stores (0.0, False). 4300 digits are still read.
    const std::string at_limit(4299, '0');
    const std::string over_limit(4300, '0');
    double z = 0.0;
    bool top = false;
    CHECK(extract_z_value_from_name("Wall @T " + at_limit + "5' 0\"", z, top));
    CHECK_EQ(z, 60.0);
    CHECK_EQ(top, true);
    CHECK(extract_z_value_from_name("Wall @T " + at_limit + "5'x", z, top));
    CHECK_EQ(z, 60.0);
    CHECK_EQ(top, true);
    CHECK_FALSE(extract_z_value_from_name("Wall @T " + over_limit + "5' 0\"", z, top));
    CHECK_EQ(z, 0.0);
    CHECK_EQ(top, false);
    CHECK_FALSE(extract_z_value_from_name("Wall @T " + over_limit + "5'x", z, top));
    CHECK_FALSE(extract_z_value_from_name("Wall @T -" + over_limit + "5'x", z, top));
    // only int() has the limit: float() parts (inches, fractions) and the metric values do not
    CHECK(extract_z_value_from_name("Wall @T 5' " + over_limit + "6\"", z, top));
    CHECK_EQ(z, 66.0);
    CHECK(extract_z_value_from_name("Wall @T " + over_limit + "6\"", z, top));
    CHECK_EQ(z, 6.0);
    CHECK(extract_z_value_from_name("Wall @T " + over_limit + "6 m", z, top));
    CHECK_EQ(z, 236.2204724409449);
}

// ---------------------------------------------------------------------------------------------------------
// ImperialElevationParsingTests / MetricElevationParsingTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationCore, ImperialFeetAndInches)
{
    // Port of ...::ImperialElevationParsingTests::test_feet_and_inches
    CHECK_EQ(imperial("12' 6\""), 150.0);
    CHECK_EQ(imperial("12' 6 1/2\""), 150.5);
    CHECK_EQ(imperial("5" OST_U_PRIME " 6\""), 66.0);
    CHECK_EQ(imperial("0' 6\""), 6.0);
}

TEST(ElevationCore, ImperialNegativeFeetMakeTheWholeElevationNegative)
{
    // Port of ...::test_negative_feet_make_the_whole_elevation_negative
    CHECK_EQ(imperial("-4' 2\""), -50.0);
    CHECK_EQ(imperial("-0' 6\""), -6.0);
}

TEST(ElevationCore, ImperialFeetOnly)
{
    // Port of ...::test_feet_only
    CHECK_EQ(imperial("5'x"), 60.0);
    CHECK_EQ(imperial("5" OST_U_PRIME "x"), 60.0);
    CHECK_EQ(imperial("-4'x"), -48.0);
}

TEST(ElevationCore, ImperialInchesOnly)
{
    // Port of ...::test_inches_only
    CHECK_EQ(imperial("6\""), 6.0);
    CHECK_EQ(imperial("-6\""), -6.0);
    CHECK_EQ(imperial("3 1/2\""), 3.5);
    CHECK_EQ(imperial("1/2\""), 0.5);
}

TEST(ElevationCore, ImperialUnrecognisedTextIsZero)
{
    // Port of ...::test_unrecognised_text_is_zero
    for (const char *text : {"garbage", "", "5"})
    {
        SCOPED_CONTEXT(std::string("text=") + text);
        CHECK_EQ(imperial(text), 0.0);
    }
}

TEST(ElevationCore, EveryMetricSpellingReturnsCentimetres)
{
    // Port of ...::MetricElevationParsingTests::test_every_metric_spelling_returns_centimetres
    CHECK_EQ(metric("1m 20cm"), 120.0);
    CHECK_EQ(metric("1m and 20cm"), 120.0);
    CHECK_EQ(metric("1m+20cm"), 120.0);
    CHECK_EQ(metric("-1m 20cm"), -120.0);
    CHECK_EQ(metric("2,5 m"), 250.0);
    CHECK_EQ(metric("2.5m"), 250.0);
    CHECK_EQ(metric("3 meters"), 300.0);
    CHECK_EQ(metric("30 cm"), 30.0);
    CHECK_EQ(metric("30 centimeters"), 30.0);
    CHECK_EQ(metric("junk"), 0.0);
}

TEST(ElevationCore, MetricUnitsAreCaseInsensitiveInTheHelper)
{
    // Port of ...::MetricSpellingBoundaryTests (helper half)
    CHECK_EQ(metric("1M 20CM"), 120.0);
    CHECK_EQ(metric("3 METERS"), 300.0);
    CHECK_EQ(metric("5 Cm"), 5.0);
    CHECK_EQ(metric("1.5m 20cm"), 170.0);
    CHECK_EQ(metric("1m 2.5cm"), 102.5);
    CHECK_NEAR(cm_to_inches(254.0), 100.0, 1e-9);
}

// ---------------------------------------------------------------------------------------------------------
// PrivateParserSearchSemanticsTests / DetachedMinusTests / MetricSignAppliesToTheWholeValueTests (helpers)
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationCore, AFootMarkNeedsAWordBoundaryAfterIt)
{
    // Port of ...::PrivateParserSearchSemanticsTests::test_a_foot_mark_needs_a_word_boundary_after_it
    CHECK_EQ(imperial("5'"), 0.0);
    CHECK_EQ(imperial("5' x"), 0.0);
    CHECK_EQ(imperial("5'x"), 60.0);
}

TEST(ElevationCore, TheValueIsFoundAfterLeadingNoise)
{
    // Port of ...::test_the_value_is_found_after_leading_noise
    CHECK_EQ(imperial("x5' 6\""), 66.0);
    CHECK_EQ(imperial("x5'6"), 60.0);
    CHECK_EQ(imperial("x 6\""), 6.0);
    CHECK_EQ(metric("x 3 m"), 300.0);
    CHECK_EQ(metric("x 30 cm"), 30.0);
    CHECK_EQ(metric("x 1m 20cm"), 120.0);
}

TEST(ElevationCore, ADetachedMinusNegatesFeetAndInchesTogether)
{
    // Port of ...::DetachedMinusTests::test_a_detached_minus_negates_feet_and_inches_together
    const std::pair<std::string, double> table[] = {
        {"- 5' 6\"", -66.0},      {"-  5' 6\"", -66.0},     {"- 5' 6 1/2\"", -66.5}, {"- 5'-6\"", -66.0},
        {"- 5' - 6\"", -66.0},    {"- 0' 6\"", -6.0},       {"- 5" OST_U_PRIME " 6\"", -66.0}, {"- 5'x", -60.0},
        {"- 4' 2\"", -50.0}};
    for (const auto &row : table)
    {
        SCOPED_CONTEXT("text=" + row.first);
        CHECK_EQ(imperial(row.first), row.second);
        CHECK_EQ(imperial(row.first), imperial(replace_first(row.first, "- ", "-")));
    }
}

TEST(ElevationCore, AttachedAndAbsentSignsReadAsBefore)
{
    // Port of ...::test_attached_and_absent_signs_read_as_before
    CHECK_EQ(imperial("5' 6\""), 66.0);
    CHECK_EQ(imperial("-5' 6\""), -66.0);
    CHECK_EQ(imperial("5'-6\""), 66.0);
    CHECK_EQ(imperial("-5' -6\""), -66.0);
    CHECK_EQ(imperial("- 6\""), -6.0);
    CHECK_EQ(imperial("-5'x"), -60.0);
}

TEST(ElevationCore, ADetachedMinusInMetricTextIsReadNotRefused)
{
    // Port of ...::test_a_detached_minus_in_metric_text_is_read_not_refused
    const std::pair<std::string, double> table[] = {{"- 1m", -100.0},       {"- 1 m", -100.0},       {"- 1,5 m", -150.0},
                                                    {"- 20cm", -20.0},      {"- 3 meters", -300.0},  {"- 1m 20cm", -120.0},
                                                    {"1m - 20cm", 80.0},    {"- 1m - 20cm", -120.0}};
    for (const auto &row : table)
    {
        SCOPED_CONTEXT("text=" + row.first);
        CHECK_EQ(metric(row.first), row.second);
        CHECK_EQ(metric(row.first), metric(replace_all(row.first, "- ", "-")));
    }
}

TEST(ElevationCore, ALeadingMinusNegatesMetresAndCentimetresTogether)
{
    // Port of ...::MetricSignAppliesToTheWholeValueTests::test_a_leading_minus_negates_metres_and_centimetres_together
    const std::pair<std::string, double> table[] = {{"-1m 20cm", -120.0},   {"-1m and 20cm", -120.0}, {"-1m+20cm", -120.0},
                                                    {"-1.5m 20cm", -170.0}, {"-1,5 m 20 cm", -170.0}, {"-1m 2.5cm", -102.5},
                                                    {"-0m 20cm", -20.0},    {"- 1m 20cm", -120.0},    {"x -1m 20cm", -120.0}};
    for (const auto &row : table)
    {
        SCOPED_CONTEXT("text=" + row.first);
        CHECK_EQ(metric(row.first), row.second);
    }
}

TEST(ElevationCore, AMinusOnTheCentimetresAloneStillSubtracts)
{
    // Port of ...::test_a_minus_on_the_centimetres_alone_still_subtracts
    CHECK_EQ(metric("1m -20cm"), 80.0);
    CHECK_EQ(metric("1m and -20cm"), 80.0);
    CHECK_EQ(metric("1m+-20cm"), 80.0);
    CHECK_EQ(metric("1m - 20cm"), 80.0);
    CHECK_EQ(metric("1m -2.5cm"), 97.5);
}

TEST(ElevationCore, BothPartsSignedStayNegativeAndSingleUnitValuesAreUnchanged)
{
    // Port of ...::test_both_parts_signed_stay_negative and ::test_unsigned_and_single_unit_values_are_unchanged
    for (const char *text : {"-1m -20cm", "- 1m - 20cm", "-1m and -20cm"})
        CHECK_EQ(metric(text), -120.0);
    CHECK_EQ(metric("1m 20cm"), 120.0);
    CHECK_EQ(metric("-1m"), -100.0);
    CHECK_EQ(metric("-20cm"), -20.0);
    CHECK_EQ(metric("-3 meters"), -300.0);
    CHECK_EQ(metric("-30 centimeters"), -30.0);
}

TEST(ElevationCore, PrivateMetricHelperNeedsAWordBoundaryAfterEachUnit)
{
    // Port of ...::RegexInternalDigitRunAndSpacingTests::test_private_metric_helper_needs_a_word_boundary_after_each_unit
    CHECK_EQ(metric("5cmx"), 0.0);
    CHECK_EQ(metric("5mx"), 0.0);
    CHECK_EQ(metric("5 cm 2mm"), 5.0);
}

TEST(ElevationCore, PrivateMetricHelperSearchDoesNotBridgeABrokenNumber)
{
    // Port of ...::RunsOfWhitespaceAndDecimalPointTests::test_private_metric_helper_search_does_not_bridge_a_broken_number
    CHECK_EQ(metric("2x5m"), 500.0);
    CHECK_EQ(metric("2x5cm"), 5.0);
    CHECK_EQ(metric("1x2m 3cm"), 203.0);
    CHECK_EQ(metric("1.m 2cm"), 2.0);
    CHECK_EQ(metric("2.m"), 0.0);
    CHECK_EQ(metric("2.cm"), 0.0);
    CHECK_EQ(metric("1m 2.cm"), 100.0);
}

// ---------------------------------------------------------------------------------------------------------
// elevation.py::parse_elevation (tests/domain/services/test_elevation.py)
// ---------------------------------------------------------------------------------------------------------
namespace
{
    void expect_parts(const std::string &name, const std::string &base, int type, const std::string &value)
    {
        SCOPED_CONTEXT("name=" + name);
        const ElevationParts p = parse_elevation(name);
        CHECK_EQ(p.base_name, base);
        CHECK_EQ(p.type, type);
        CHECK_EQ(p.value, value);
    }
}

TEST(ElevationParts, LastExplicitMarkerWinsAndLegacyMarkerMeansBottom)
{
    // Port of tests/domain/services/test_elevation.py::ParseElevationTests::test_last_explicit_marker_wins_and_legacy_marker_means_bottom
    expect_parts("Wall @T old @B 4'", "Wall @T old", 1, "4'");
    expect_parts("Wall @ 2'", "Wall", 1, "2'");
}

TEST(ElevationParts, EmptyAndUnmarkedNamesDoNotInventAnElevation)
{
    // Port of ...::test_empty_and_unmarked_names_do_not_invent_an_elevation
    CHECK_EQ(parse_elevation("").value, std::string());
    CHECK_EQ(parse_elevation("Wall").base_name, std::string("Wall"));
}

TEST(ElevationParts, EachMarkerFormSplitsIntoBaseTypeAndValue)
{
    // Port of ...::ParseElevationTableTests::test_each_marker_form_splits_into_base_type_and_value
    expect_parts("Wall @T 10'", "Wall", 0, "10'");
    expect_parts("Wall @B 10'", "Wall", 1, "10'");
    expect_parts("Wall @T10'", "Wall", 0, "10'");
    expect_parts("Wall @B10'", "Wall", 1, "10'");
    expect_parts("Wall @T", "Wall", 0, "");
    expect_parts("Wall @B", "Wall", 1, "");
    expect_parts("Wall @T ", "Wall", 0, "");
    expect_parts("Wall @ 2'", "Wall", 1, "2'");
    expect_parts("Wall @2'", "Wall", 1, "2'");
    expect_parts("Wall @", "Wall", 1, "");
    expect_parts("Wall @ ", "Wall", 1, "");
    expect_parts("Wall", "Wall", 0, "");
    expect_parts("", "", 0, "");
    expect_parts("@T 5'", "@T 5'", 0, "");
    expect_parts(" @T 5'", "", 0, "5'");
}

TEST(ElevationParts, TheMarkerThatAppearsLastWinsAndAnExplicitMarkerBeatsTheLegacyOne)
{
    // Port of ...::test_the_marker_that_appears_last_wins_and_an_explicit_marker_beats_the_legacy_one
    expect_parts("A @B 1' @T 2'", "A @B 1'", 0, "2'");
    expect_parts("A @T 1' @B 2'", "A @T 1'", 1, "2'");
    expect_parts("A @ 1' @T 2'", "A @ 1'", 0, "2'");
    expect_parts("A @T 2' @ 3'", "A", 0, "2' @ 3'");
    expect_parts("A @ 2' @x", "A", 1, "2' @x");
}

TEST(ElevationParts, ANameThatStartsWithOneMarkerStillResolvesTheLastOfBothMarkers)
{
    // Port of ...::test_a_name_that_starts_with_one_marker_still_resolves_the_last_of_both_markers
    expect_parts(" @T 5 @B 6", " @T 5", 1, "6");
    expect_parts(" @B 5 @T 6", " @B 5", 0, "6");
    expect_parts(" @T 5 @T 6", " @T 5", 0, "6");
}

TEST(ElevationParts, OnlyOneSeparatingSpaceAfterTheMarkerIsDropped)
{
    // Port of ...::test_only_one_separating_space_after_the_marker_is_dropped
    expect_parts("A @T  2'", "A", 0, " 2'");
    expect_parts("A @B   x", "A", 1, "  x");
    const std::string separators[] = {"\t", "\n", "\r", "\x0b", std::string(1, '\0'), OST_U_NBSP};
    for (const std::string &sep : separators)
    {
        expect_parts("A @T" + sep + "2'", "A", 0, sep + "2'");
        expect_parts("A @B" + sep + "2'", "A", 1, sep + "2'");
    }
}

// ---------------------------------------------------------------------------------------------------------
// Python float() and repr(float)
// ---------------------------------------------------------------------------------------------------------
TEST(PythonFloat, ParsesLikeFloat)
{
    const std::pair<const char *, double> ok[] = {{"1", 1.0},       {" 1 ", 1.0},        {"-1.5", -1.5},     {"+.5", 0.5},
                                                  {"5.", 5.0},      {"1e5", 1e5},        {"1E-3", 1e-3},     {"1_000", 1000.0},
                                                  {"1e1_0", 1e10},  {"007", 7.0},        {"1e400", HUGE_VAL}, {"-1e400", -HUGE_VAL},
                                                  {"1e-400", 0.0},  {"\xD9\xA1\xD9\xA2", 12.0}, {"\xEF\xBC\x93.\xEF\xBC\x95", 3.5}};
    for (const auto &c : ok)
    {
        SCOPED_CONTEXT(std::string("text=") + c.first);
        double v = -99.0;
        REQUIRE(parse_python_float(c.first, v));
        CHECK_EQ(v, c.second);
    }
    for (const char *bad : {"", " ", "abc", "1,5", "1.5abc", "0x10", "1 2", "1e", "e5", ".", "+", "-", "1__0", "_1", "1_", "1_.5", "1._5",
                            "- 5", "nan(1)", "infx", "1.2.3", "1-2"})
    {
        SCOPED_CONTEXT(std::string("text=") + bad);
        double v = 0.0;
        CHECK_FALSE(parse_python_float(bad, v));
    }
    double v = 0.0;
    REQUIRE(parse_python_float("-InFiNiTy", v));
    CHECK_EQ(v, -std::numeric_limits<double>::infinity());
    REQUIRE(parse_python_float("NaN", v));
    CHECK(std::isnan(v));
}

TEST(PythonFloat, ReprIsTheShortestRoundTripWithPythonLayout)
{
    // Python repr(float) values (Python 3.11.4)
    const std::pair<double, const char *> table[] = {
        {1.0, "1.0"},
        {0.0, "0.0"},
        {-0.0, "-0.0"},
        {100.0, "100.0"},
        {1.5, "1.5"},
        {0.1, "0.1"},
        {0.1 + 0.2, "0.30000000000000004"},
        {1e-05, "1e-05"},
        {0.0001, "0.0001"},
        {1e16, "1e+16"},
        {1e15, "1000000000000000.0"},
        {123456789012345.0, "123456789012345.0"},
        {1e22, "1e+22"},
        {1.5e300, "1.5e+300"},
        {5e-324, "5e-324"},
        {1.7976931348623157e308, "1.7976931348623157e+308"},
        {9007199254740992.0, "9007199254740992.0"},
        {12345.678901234, "12345.678901234"},
        {1234.567890123456, "1234.567890123456"},
        {0.00012345, "0.00012345"},
        {-2.5, "-2.5"},
        {std::numeric_limits<double>::infinity(), "inf"},
        {-std::numeric_limits<double>::infinity(), "-inf"},
        {std::numeric_limits<double>::quiet_NaN(), "nan"},
    };
    for (const auto &c : table)
    {
        SCOPED_CONTEXT(std::string("expected=") + c.second);
        CHECK_STR_EQ(python_float_repr(c.first), c.second);
    }
}
