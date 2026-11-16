// Ports of the extract_z_value_from_name cases of tests/infrastructure/parsers/test_position_parser.py
// (ELV-01..ELV-06). Every test drives ost_store::extract_z_value, the function ProjectStore+Load.cpp calls for
// each condition name. Where the Windows function raises ValueError (zero denominator) the Windows MDB reader
// stores (0.0, False) (infrastructure/mdb/components/bid_data_reader.py:896-906), which is the contract of
// extract_z_value here.
#include "ParsersTestSupport.hpp"

#include "Persistence/ProjectStore/ProjectStore_Internal.hpp"

#include <cmath>

namespace
{
    using Z = std::pair<double, bool>;

    Z ez(const std::string &name) { return ost_store::extract_z_value(name); }

    void expect_z(const std::string &name, double inches, bool is_top)
    {
        SCOPED_CONTEXT("name=" + name);
        const Z got = ez(name);
        CHECK_EQ(got.first, inches);
        CHECK_EQ(got.second, is_top);
    }

    void expect_none(const std::string &name)
    {
        SCOPED_CONTEXT("name=" + name);
        CHECK_EQ(ez(name), std::make_pair(0.0, false));
    }

    // assertAlmostEqual(value, centimetres / 2.54, places=9) plus the top/bottom flag
    void expect_cm(const std::string &name, double centimetres, bool is_top)
    {
        SCOPED_CONTEXT("name=" + name);
        const Z got = ez(name);
        CHECK_NEAR(got.first, centimetres / 2.54, ost_parsers_test::kAlmost9);
        CHECK_EQ(got.second, is_top);
    }
}

// ---------------------------------------------------------------------------------------------------------
// Port of tests/infrastructure/parsers/test_position_parser.py::ExtractZValueCharacterizationTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, TopAndBottomImperialValues)
{
    // Port of ...::ExtractZValueCharacterizationTests::test_top_and_bottom_imperial_values
    expect_z("F9 @T 410' 3\"", 4923.0, true);
    expect_z("Wall @B 8' 0\"", 96.0, false);
    expect_z("Slab @T 12' 6 1/2\"", 150.5, true);
}

TEST(ElevationPort, MetricValuesAreConvertedToInches)
{
    // Port of ...::test_metric_values_are_converted_to_inches (assertAlmostEqual places=6)
    const Z got = ez("Wall @T 2.54 m");
    CHECK(got.second);
    CHECK_NEAR(got.first, 100.0, 5e-7);
}

TEST(ElevationPort, NamesWithoutAValidElevationHaveNone)
{
    // Port of ...::test_names_without_a_valid_elevation_have_none
    for (const char *name : {"", "Wall", "Wall @T later", "Wall @T 5pm", "Wall @T 5' 2 m"})
        expect_none(name);
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::ZeroDenominatorElevationTests (reader level: ValueError -> (0.0, False))
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, ZeroDenominatorIsUnreadableAndStoresNoElevation)
{
    // Port of ...::test_extracting_an_elevation_with_a_zero_denominator_is_a_value_error
    for (const char *name : {"Wall @B 1/0\"", "Slab @T 6 1/0\"", "Slab @T 12' 6 1/0\""})
        expect_none(name);
}

TEST(ElevationPort, AFailedLookupDoesNotAffectValidNames)
{
    // Port of ...::test_a_failed_lookup_is_not_cached_and_valid_names_are_unaffected
    expect_none("Wall @B 1/0\"");
    expect_none("Wall @B 1/0\"");
    expect_z("Wall @B 6 1/2\"", 6.5, false);
}

TEST(ElevationPort, ZeroDenominatorMessagesStillGiveNoElevation)
{
    // Port of ...::ElevationValueBoundaryTests::test_the_zero_denominator_error_names_the_offending_fraction
    // (the message itself is checked in test_elevation_core_api.cpp)
    for (const char *name : {"Wall @B 1/0\"", "Wall @B 6 1/00\"", "Wall @T 12' 6 3/0\"", "Wall @B 0/0\""})
        expect_none(name);
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::UnicodeWhitespaceInElevationNameTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, ANonAsciiSpaceInAMixedInchValueReadsLikeAPlainSpace)
{
    // Port of ...::test_a_non_ascii_space_in_a_mixed_inch_value_reads_like_a_plain_space
    const std::pair<const char *, std::string> seps[] = {
        {"nbsp", OST_U_NBSP}, {"narrow nbsp", OST_U_NNBSP}, {"tab", "\t"}};
    for (const auto &s : seps)
    {
        SCOPED_CONTEXT(std::string("separator=") + s.first);
        expect_z("Wall @B 6" + s.second + "1/2\"", 6.5, false);
        expect_z("Slab @T 12'" + s.second + "6" + s.second + "1/2\"", 150.5, true);
        expect_z("Wall @B -6" + s.second + "1/2\"", -6.5, false);
    }
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::MixedUnitAndCacheTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, TextThatStartsLikeOneUnitButCarriesTheOtherHasNoElevation)
{
    // Port of ...::test_text_that_starts_like_one_unit_but_carries_the_other_has_no_elevation
    const std::string tails[] = {"5'cm", "5" OST_U_PRIME "cm", "5'm", "6\"m", "5m'", "3 meters'"};
    for (const std::string &tail : tails)
    {
        expect_none("Wall @T " + tail);
        expect_none("Wall @B " + tail);
    }
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::ElevationFormattingAndLookupTests (the extract part)
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, EveryValidSpellingYieldsInchesAndTheTopOrBottomFlag)
{
    // Port of ...::test_every_valid_spelling_yields_inches_and_the_top_or_bottom_flag
    expect_z("Wall @T 5'x", 60.0, true);
    expect_z("Wall @T 5" OST_U_PRIME "x", 60.0, true);
    expect_z("Wall @B 6\"", 6.0, false);
    expect_z("Wall @T 12' 6\"", 150.0, true);
    expect_cm("Wall @T 3 meters", 300.0, true);
    expect_cm("Wall @B 30 cm", 30.0, false);
}

TEST(ElevationPort, ANameWithAnAtSignButNoElevationHasNone)
{
    // Port of ...::test_a_name_with_an_at_sign_but_no_elevation_has_none
    for (const char *name : {"Wall @T ", "Wall @", "Wall @T"})
        expect_none(name);
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::UnicodeWhitespaceBoundaryTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, EveryUnicodeWhitespaceSeparatesInchFractions)
{
    // Port of ...::test_every_unicode_whitespace_separates_inch_fractions_in_both_entry_points (extract half)
    for (const auto &s : ost_parsers_test::unicode_separators())
    {
        SCOPED_CONTEXT(std::string("separator=") + s.label);
        expect_z("Wall @B 6" + s.text + "1/2\"", 6.5, false);
        expect_z("Wall @T -6" + s.text + "1/2\"", -6.5, true);
        expect_z("Slab @T 12'" + s.text + "6" + s.text + "1/2\"", 150.5, true);
        expect_z("Slab @T 12'" + s.text + "6\"", 150.0, true);
    }
}

TEST(ElevationPort, ARunOfMixedWhitespaceIsOneSeparator)
{
    // Port of ...::test_a_run_of_mixed_whitespace_is_one_separator
    const std::string mixed = " " OST_U_NBSP "\t" OST_U_NNBSP " ";
    expect_z("Slab @T 12'" + mixed + "6" + mixed + "1/2\"", 150.5, true);
}

TEST(ElevationPort, WhitespaceAroundTheFractionBarIsNotAnElevation)
{
    // Port of ...::test_whitespace_around_the_fraction_bar_is_not_an_elevation (extract half)
    for (const auto &s : ost_parsers_test::unicode_separators())
    {
        SCOPED_CONTEXT(std::string("separator=") + s.label);
        expect_none("Wall @B 6 1" + s.text + "/2\"");
        expect_none("Wall @B 6 1/" + s.text + "2\"");
    }
}

TEST(ElevationPort, AZeroWidthSpaceIsNotWhitespace)
{
    // Port of ...::test_a_zero_width_space_is_not_whitespace (extract half)
    expect_none("Wall @B 6" OST_U_ZWSP "1/2\"");
    expect_none("Wall @B 12'" OST_U_ZWSP "6\"");
}

TEST(ElevationPort, WhitespaceAroundTheWholeValueIsIgnored)
{
    // Port of ...::test_whitespace_around_the_whole_value_is_ignored
    for (const auto &s : ost_parsers_test::unicode_separators())
    {
        SCOPED_CONTEXT(std::string("separator=") + s.label);
        expect_z("Wall @B" + s.text + "6 1/2\"" + s.text, 6.5, false);
    }
}

TEST(ElevationPort, ZeroDenominatorsWithAnyWhitespaceFlavourGiveNoElevation)
{
    // Port of ...::test_zero_denominators_stay_value_errors_with_any_whitespace_flavour (reader-level half)
    for (const auto &s : ost_parsers_test::unicode_separators())
    {
        SCOPED_CONTEXT(std::string("separator=") + s.label);
        expect_none("Wall @B 6" + s.text + "1/0\"");
        expect_none("Wall @B 12'" + s.text + "6" + s.text + "1/0\"");
        expect_none("Wall @B -6" + s.text + "0/0\"");
    }
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::ElevationValueBoundaryTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, ADashBetweenFeetAndInchesIsASeparatorNotASign)
{
    // Port of ...::test_a_dash_between_feet_and_inches_is_a_separator_not_a_sign
    expect_z("Wall @T 12'-6\"", 150.0, true);
    expect_z("Wall @T 12' - 6\"", 150.0, true);
    expect_z("Wall @T 12'6\"", 150.0, true);
    expect_z("Wall @T -12'-6\"", -150.0, true);
    expect_z("Wall @B 12'-6 1/2\"", 150.5, false);
}

TEST(ElevationPort, ZeroAndNegativeValuesKeepTheirTopOrBottomFlag)
{
    // Port of ...::test_zero_and_negative_values_keep_their_top_or_bottom_flag
    expect_z("Wall @T 0\"", 0.0, true);
    expect_z("Wall @B 0\"", 0.0, false);
    expect_z("Wall @T 0' 0\"", 0.0, true);
    expect_z("Wall @T 0'x", 0.0, true);
    expect_z("Wall @T 0 cm", 0.0, true);
    expect_z("Wall @B 0/4\"", 0.0, false);
    expect_z("Wall @T -1/2\"", -0.5, true);
    expect_z("Wall @B - 6\"", -6.0, false);
    expect_z("Wall @T 5' 6 0/4\"", 66.0, true);
}

TEST(ElevationPort, AZeroDenominatorAfterFeetWithoutWholeInchesIsNotAnElevation)
{
    // Port of ...::test_a_zero_denominator_after_feet_without_whole_inches_is_not_an_elevation
    expect_none("Wall @T 5' 1/0\"");
    expect_none("Wall @T 5' 1/2\"");
}

TEST(ElevationPort, AVeryLargeDenominatorGivesATinyValueNotZeroOrAnError)
{
    // Port of ...::test_a_very_large_denominator_gives_a_tiny_value_not_zero_or_an_error
    expect_z("Wall @B 1/99999999999999999999\"", 1e-20, false);
}

TEST(ElevationPort, VeryLargeValuesAreExactOrFloatRoundedNotRejected)
{
    // Port of ...::test_very_large_values_are_exact_or_float_rounded_not_rejected
    expect_z("Wall @T 1000000000' 6\"", 12000000006.0, true);
    expect_z("Wall @T -1000000000' 6\"", -12000000006.0, true);
    expect_z("Wall @T 99999999999999999999' 0\"", 1.2e21, true);
    expect_z("Wall @B 99999999999999999999/1\"", 1e20, false);
}

TEST(ElevationPort, TheLastElevationMarkerWinsAndALegacyBareMarkerIsABottom)
{
    // Port of ...::test_the_last_elevation_marker_wins_and_a_legacy_bare_marker_is_a_bottom
    expect_z("Wall @T 5' 6\" @B 3' 0\"", 36.0, false);
    expect_z("Wall @B 3' 0\" @T 5' 6\"", 66.0, true);
    expect_z("A @T 1/2\" @T 3/4\"", 0.75, true);
    expect_z("Wall @ 6\"", 6.0, false);
    expect_z("Wall @ 6 1/2\"", 6.5, false);
}

TEST(ElevationPort, DecimalInchesAndNestedFractionsAreOutsideTheGrammar)
{
    // Port of ...::test_decimal_inches_and_nested_fractions_are_outside_the_grammar
    for (const char *name : {"Wall @B 6.5\"", "Wall @T 6 1/2/3\"", "Wall @B 6 1/2/3\""})
        expect_none(name);
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::MetricSpellingBoundaryTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, MetricUnitsAreCaseInsensitive)
{
    // Port of ...::test_metric_units_are_case_insensitive_in_every_entry_point (extract part, places=9)
    expect_cm("Wall @T 3 M", 300.0, true);
    expect_cm("Wall @B 30 CM", 30.0, false);
    expect_cm("Wall @T 1M 20CM", 120.0, true);
    expect_cm("Wall @B 3 Meters", 300.0, false);
    expect_cm("Wall @T 2 CENTIMETERS", 2.0, true);
}

TEST(ElevationPort, DecimalMetresAndDecimalCentimetresInACompoundValue)
{
    // Port of ...::test_decimal_metres_and_decimal_centimetres_in_a_compound_value
    expect_cm("Wall @T 1.5m 20cm", 170.0, true);
    expect_cm("Wall @T 1,5 m 20 cm", 170.0, true);
    expect_cm("Wall @T 1m 2.5cm", 102.5, true);
    expect_cm("Wall @T 1m and 2,5 cm", 102.5, true);
}

TEST(ElevationPort, WhitespaceAroundTheValueIsIgnored)
{
    // Port of ...::test_whitespace_around_the_value_is_ignored_by_both_entry_points (extract part)
    expect_z("Wall @T  -6\"", -6.0, true);
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::CompoundMetricConversionAndEmptyBaseNameTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, AnElevationMarkerWithAnEmptyBaseNameStillHasAnElevation)
{
    // Port of ...::test_an_elevation_marker_with_an_empty_base_name_still_has_an_elevation
    expect_z(" @T 6\"", 6.0, true);
    expect_z(" @B 6\"", 6.0, false);
    expect_z("   @T 6\"", 6.0, true);
}

TEST(ElevationPort, AMarkerWithoutTheLeadingSpaceIsNotAnElevationMarker)
{
    // Port of ...::test_a_marker_without_the_leading_space_is_not_an_elevation_marker
    expect_none("@T 6\"");
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::DetachedMinusTests (entry-point half)
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, TheEntryPointsAgreeForADetachedMinus)
{
    // Port of ...::test_the_entry_points_agree_for_a_detached_minus
    expect_z("Wall @T - 5' 6\"", -66.0, true);
    expect_z("Wall @B - 5' 6\"", -66.0, false);
    expect_z("Wall @T - 6\"", -6.0, true);
    expect_z("Wall @T - 1 m", -100.0 / 2.54, true);
    expect_z("Wall @T - 20cm", -20.0 / 2.54, true);
    // ... and each equals the same name with the minus attached ("- " replaced by "-" once)
    expect_z("Wall @T -5' 6\"", -66.0, true);
    expect_z("Wall @T -6\"", -6.0, true);
    expect_z("Wall @T -1 m", -100.0 / 2.54, true);
    expect_z("Wall @T -20cm", -20.0 / 2.54, true);
}

TEST(ElevationPort, ADetachedMinusBeforeAFeetOnlyValueIsAccepted)
{
    // Port of ...::ElevationValidityGateSpacingAndBoundaryTests::test_a_detached_minus_before_a_feet_only_value_is_accepted
    expect_z("Wall @T - 5'x", -60.0, true);
    expect_z("Wall @B - 12 'x", -144.0, false);
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::MetricSignAppliesToTheWholeValueTests (name entry points)
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, TheMetricSignAppliesToTheWholeValue)
{
    // Port of ...::test_the_name_entry_points_apply_the_sign_to_the_whole_value
    expect_cm("Wall @T -1m 20cm", -120.0, true);
    expect_cm("Wall @B -1m 20cm", -120.0, false);
    expect_cm("Wall @T - 1m 20cm", -120.0, true);
    expect_cm("Wall @T -1m -20cm", -120.0, true);
    expect_cm("Wall @T 1m -20cm", 80.0, true);
}

TEST(ElevationPort, MetricAndImperialSignsAgreeForTheSameLength)
{
    // Port of ...::test_metric_and_imperial_signs_agree_for_the_same_length (extract half)
    CHECK_EQ(ez("Wall @T -1m 20cm").first, -ez("Wall @T 1m 20cm").first);
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::RegexInternalDigitRunAndSpacingTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, FeetAndInchesWithTwoDigitAndSixteenthParts)
{
    // Port of ...::test_feet_and_inches_with_two_digit_and_sixteenth_parts
    expect_z("Wall @T 5' 10\"", 70.0, true);
    expect_z("Wall @T 5' 11\"", 71.0, true);
    expect_z("Wall @T 12' 3\"", 147.0, true);
    expect_z("Wall @T 10' 10\"", 130.0, true);
    expect_z("Wall @T 5' 6 11/16\"", 66.6875, true);
    expect_z("Wall @T 5' 6 1/16\"", 66.0625, true);
    expect_z("Wall @T 5' 6 15/16\"", 66.9375, true);
}

TEST(ElevationPort, ASpaceBeforeThePrimeOrBeforeTheClosingQuoteIsAllowed)
{
    // Port of ...::test_a_space_before_the_prime_or_before_the_closing_quote_is_allowed
    expect_z("Wall @T 5 ' 6\"", 66.0, true);
    expect_z("Wall @T 5' 6 \"", 66.0, true);
    expect_z("Wall @T 5 ' 6 \"", 66.0, true);
    expect_z("Wall @T -5 ' 6\"", -66.0, true);
    expect_z("Wall @T 5' 6 1/2 \"", 66.5, true);
}

TEST(ElevationPort, ABareFractionDirectlyAfterTheFeetMarkIsTheInchesPart)
{
    // Port of ...::test_a_bare_fraction_directly_after_the_feet_mark_is_the_inches_part
    expect_z("Wall @T 5'3/4\"", 60.75, true);
    expect_z("Wall @T 5'3/16\"", 60.1875, true);
    expect_z("Wall @T 5'13/16\"", 60.8125, true);
    expect_z("Wall @T -5'3/4\"", -60.75, true);
}

TEST(ElevationPort, FeetOnlyValuesWithTwoDigitsOrASpaceBeforeThePrime)
{
    // Port of ...::test_feet_only_values_with_two_digits_or_a_space_before_the_prime
    expect_z("Wall @T 20'x", 240.0, true);
    expect_z("Wall @T 11 'x", 132.0, true);
    expect_z("Wall @T 10'x", 120.0, true);
    expect_z("Wall @T -12 'x", -144.0, true);
}

TEST(ElevationPort, InchesOnlyValuesWithTwoDigitsASpaceOrASixteenthFraction)
{
    // Port of ...::test_inches_only_values_with_two_digits_a_space_or_a_sixteenth_fraction
    expect_z("Wall @T 12\"", 12.0, true);
    expect_z("Wall @T 6 \"", 6.0, true);
    expect_z("Wall @T 11 1/2\"", 11.5, true);
    expect_z("Wall @T 1 7/16\"", 1.4375, true);
    expect_z("Wall @T 6 11/16\"", 6.6875, true);
    expect_z("Wall @T 11/16\"", 0.6875, true);
    expect_z("Wall @T 5/16 \"", 0.3125, true);
    expect_z("Wall @T -11 1/2\"", -11.5, true);
}

TEST(ElevationPort, MetricValuesWithTwoDigitMetresOrCentimetresAndDecimals)
{
    // Port of ...::test_metric_values_with_two_digit_metres_or_centimetres_and_decimals
    expect_cm("Wall @T 11m 20cm", 1120.0, true);
    expect_cm("Wall @T 1m 25cm", 125.0, true);
    expect_cm("Wall @T 1.25m 20cm", 145.0, true);
    expect_cm("Wall @T 1m 2.50cm", 102.5, true);
    expect_cm("Wall @T 11 m", 1100.0, true);
    expect_cm("Wall @T 11.5 m", 1150.0, true);
    expect_cm("Wall @T 12 meters", 1200.0, true);
    expect_cm("Wall @T 25 cm", 25.0, true);
    expect_cm("Wall @T 12.5 cm", 12.5, true);
    expect_cm("Wall @T 12 centimeters", 12.0, true);
}

TEST(ElevationPort, AMetricUnitIsNotTakenFromTheStartOfALongerWord)
{
    // Port of ...::test_a_metric_unit_is_not_taken_from_the_start_of_a_longer_word
    expect_cm("Wall @T 5 cm 2mm", 5.0, true);
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::ElevationValidityGateSpacingAndBoundaryTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, InchesGluedToAFractionAreNotAReadableElevation)
{
    // Port of ...::test_inches_glued_to_a_fraction_are_not_a_readable_elevation
    expect_none("Wall @T 5' 611/16\"");
}

TEST(ElevationPort, ACentimetreUnitGluedToMoreLettersIsNotAnElevation)
{
    // Port of ...::test_a_centimetre_unit_glued_to_more_letters_is_not_an_elevation
    for (const char *name : {"Wall @T 5cmx", "Wall @T 5 centimetersx", "Wall @B 12 cmx"})
        expect_none(name);
}

// ---------------------------------------------------------------------------------------------------------
// Port of ...::RunsOfWhitespaceAndDecimalPointTests
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, ImperialValuesAcceptRunsOfSpacesAroundEveryMark)
{
    // Port of ...::test_imperial_values_accept_runs_of_spaces_around_every_mark
    expect_z("Wall @T 5  ' 6\"", 66.0, true);
    expect_z("Wall @T 5' 6  \"", 66.0, true);
    expect_z("Wall @T 5   '   6   \"", 66.0, true);
    expect_z("Wall @T 5'  -  6\"", 66.0, true);
    expect_z("Wall @T 5' -  6\"", 66.0, true);
    expect_z("Wall @T -  5'x", -60.0, true);
    expect_z("Wall @T 5  'x", 60.0, true);
    expect_z("Wall @T -  6\"", -6.0, true);
    expect_z("Wall @T 6  \"", 6.0, true);
    expect_z("Wall @T 1/2  \"", 0.5, true);
    expect_z("Wall @T 11  1/2  \"", 11.5, true);
}

TEST(ElevationPort, MetricValuesAcceptRunsOfSpacesAroundEveryMark)
{
    // Port of ...::test_metric_values_accept_runs_of_spaces_around_every_mark
    expect_cm("Wall @T -  1m 20cm", -120.0, true);
    expect_cm("Wall @T 1  m 20  cm", 120.0, true);
    expect_cm("Wall @T 1m  and  20cm", 120.0, true);
    expect_cm("Wall @T 1m  +  20cm", 120.0, true);
    expect_cm("Wall @T -  1 m", -100.0, true);
    expect_cm("Wall @T 1  m", 100.0, true);
    expect_cm("Wall @T -  20 cm", -20.0, true);
    expect_cm("Wall @T 20  cm", 20.0, true);
    expect_cm("Wall @T 1   m   20   cm", 120.0, true);
    expect_cm("Wall @T 1m and  -  20cm", 80.0, true);
    expect_cm("Wall @T 1m -  20cm", 80.0, true);
}

TEST(ElevationPort, ADecimalPointWithoutDigitsIsNotPartOfTheNumber)
{
    // Port of ...::test_a_decimal_point_without_digits_is_not_part_of_the_number
    expect_cm("Wall @T 1m 2.cm", 100.0, true);
    expect_cm("Wall @T 1m 2x3cm", 100.0, true);
    expect_cm("Wall @T 5 cm 2.m", 5.0, true);
}

// ---------------------------------------------------------------------------------------------------------
// Phase 1 plan cases (ELV-01..06 test plans; values verified by running the Windows code)
// ---------------------------------------------------------------------------------------------------------
TEST(ElevationPort, Elv01FractionalAndMixedNumberInches)
{
    expect_z("Slab @T 12' 6 1/2\"", 150.5, true);
    expect_z("Wall @B 6 1/2\"", 6.5, false);
    expect_z("Wall @T 1 7/16\"", 1.4375, true);
    expect_z("Wall @T 11/16\"", 0.6875, true);
    expect_z("Wall @T 5' 6 11/16\"", 66.6875, true);
    expect_z("Wall @T 5' 6 1/2 \"", 66.5, true);
    expect_z("Wall @T 5'3/4\"", 60.75, true);
    expect_z("Wall @B 12'-6 1/2\"", 150.5, false);
    expect_z("Wall @T -11 1/2\"", -11.5, true);
    expect_z("Wall @T 0\"", 0.0, true);
    expect_z("Wall @B 0/4\"", 0.0, false);
    expect_z("Slab @T 10' - 3 1/2\"", 123.5, true);
    expect_z("Wall @B -6 1/2\"", -6.5, false);
    expect_z("Wall @B 3/4\"", 0.75, false);
    // zero denominators: Windows raises, the loader stores (0.0, false)
    expect_none("Wall @B 1/0\"");
    expect_none("Slab @T 6 1/0\"");
    expect_none("Slab @T 12' 6 1/0\"");
}

TEST(ElevationPort, Elv02MetricCompoundAndSpellings)
{
    // inches = cm / 2.54 computed with the same IEEE operations as cm_to_inches
    expect_z("Wall @T 1m 20cm", 120.0 / 2.54, true);
    expect_z("Wall @T 1.5m 20cm", 170.0 / 2.54, true);
    expect_z("Wall @T 1,5 m 20 cm", 170.0 / 2.54, true);
    expect_z("Wall @T 1m 2.5cm", 102.5 / 2.54, true);
    expect_z("Wall @T 1m and 2,5 cm", 102.5 / 2.54, true);
    expect_z("Wall @T 3m+20cm", 320.0 / 2.54, true);
    expect_z("Wall @T 2,5 m", 250.0 / 2.54, true);
    expect_z("Wall @T 2 CENTIMETERS", 2.0 / 2.54, true);
    expect_z("Wall @T 1M 20CM", 120.0 / 2.54, true);
    expect_z("Wall @T -1m 20cm", -120.0 / 2.54, true);
    expect_z("Wall @T 1m -20cm", 80.0 / 2.54, true);
    expect_z("Wall @T 11m 20cm", 1120.0 / 2.54, true);
    expect_z("Wall @T 3,5 m", 350.0 / 2.54, true);
    expect_z("Wall @T 2 m 50 cm", 250.0 / 2.54, true);
    for (const char *name : {"Wall @T 5cmx", "Wall @B 12 cmx", "Wall @T 5 centimetersx", "Wall @T 3mm"})
        expect_none(name);
}

TEST(ElevationPort, Elv03MarkerDetection)
{
    expect_z("Wall @T 5' 6\" @B 3' 0\"", 36.0, false);
    expect_z("Wall @B 3' 0\" @T 5' 6\"", 66.0, true);
    expect_z("A @T 1/2\" @T 3/4\"", 0.75, true);
    expect_z("Wall @ 6\"", 6.0, false);
    expect_z("Wall @ 6 1/2\"", 6.5, false);
    expect_z(" @T 6\"", 6.0, true);
    expect_none("@T 6\"");
    expect_none("Wall@T 5' 0\"");
    expect_none("Wall @t 5' 0\"");
    expect_z("Wall email@x.com @T 5' 0\"", 60.0, true);
    expect_z("A @ B @T 8' 0\"", 96.0, true);
    expect_z("Wall A @T 10' 0\" and @B 4' 0\"", 48.0, false);
    expect_z("Wall @ Level 2 @T 10' - 0\"", 120.0, true);
    expect_z("CMU 8\" @ 16\" o.c. @B 4' - 0\"", 48.0, false);
}

TEST(ElevationPort, Elv04ValidityGate)
{
    for (const char *tail : {"5'", "5' ", "5' 6", "5' 6.5\"", "6.5\"", "1.5\"", "6 1/2/3\"", "5.5'x", "5' 1/2\"", "5' 1/0\"",
                             "5' 611/16\"", "5' 2 m"})
    {
        expect_none(std::string("Wall @T ") + tail);
    }
    expect_none("Wall @B 5'");
    // lookalike foot marks: only ' and U+2032 are foot marks
    for (const char *mark : {OST_U_ENDASH, OST_U_EURO, OST_U_DPRIME})
        expect_none(std::string("Wall @T 5") + mark + " 6\"");
    expect_z("Wall @T 5'x", 60.0, true);
    expect_z("Wall @T 5" OST_U_PRIME "x", 60.0, true);
    expect_z("Wall @T 5" OST_U_PRIME " 6\"", 66.0, true);
    expect_z("Wall @T 12'-6\"", 150.0, true);
    expect_z("Wall @T 5 ' 6 \"", 66.0, true);
    expect_z("Wall @T 12' 6\"", 150.0, true);
    expect_z("Wall @T 12' - 6\"", 150.0, true);
}

TEST(ElevationPort, Elv04LegacyScannerOverAcceptedThese)
{
    // The old scanner read these as values; Windows rejects them (Windows-verified).
    for (const char *name : {"Wall @T 10'", "Wall@T 10'", "Wall @T -4'", "Wall @T 5'cm", "Wall @T 5' 6.5\"", "Pier @T 100.5\"",
                             "Wall @B 6.5\"", "wall @t 10' 6\"", "Wall @T 10' 6.5\"", "Wall @T 5' 2 m"})
        expect_none(name);
    expect_z("Wall @T 10'6\"", 126.0, true);
    expect_z("Wall @T 10' 6\"", 126.0, true);
    expect_z("Wall @T 10'-6\"", 126.0, true);
}

TEST(ElevationPort, Elv05DetachedMinus)
{
    expect_z("Wall @T - 5' 6\"", -66.0, true);
    expect_z("Wall @T -  5'x", -60.0, true);
    expect_z("Wall @B - 6\"", -6.0, false);
    expect_z("Wall @T - 1 m", -100.0 / 2.54, true);
    expect_z("Wall @T - 20cm", -20.0 / 2.54, true);
    expect_z("Wall @T - 1m 20cm", -120.0 / 2.54, true);
    expect_z("Wall @T- 5' 6\"", -66.0, true);
}

TEST(ElevationPort, Elv06UnicodeWhitespaceSeparators)
{
    for (const auto &s : ost_parsers_test::unicode_separators())
    {
        SCOPED_CONTEXT(std::string("separator=") + s.label);
        expect_z("Wall @B 6" + s.text + "1/2\"", 6.5, false);
        expect_z("Wall @T -6" + s.text + "1/2\"", -6.5, true);
        expect_z("Slab @T 12'" + s.text + "6" + s.text + "1/2\"", 150.5, true);
        expect_z("Slab @T 12'" + s.text + "6\"", 150.0, true);
        expect_z("Wall @B" + s.text + "6 1/2\"" + s.text, 6.5, false);
    }
    // NBSP between the marker and the value
    expect_z("Wall @T" OST_U_NBSP "5' 0\"", 60.0, true);
}

