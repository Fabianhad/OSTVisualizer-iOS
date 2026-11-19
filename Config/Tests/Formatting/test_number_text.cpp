// The exact number-to-text primitives behind the dimension and quantity text (Domain/NumberText.hpp):
// Python's round() (ties to even), str(int(x)) for any magnitude and a printf that never writes a locale decimal point.
// Expected values come from running CPython 3.11 (round(x), int(x)).
#include "FormattingTestSupport.hpp"

#include "Domain/NumberText.hpp"

#include <string>

using namespace ost_domain;

TEST(NumberText, RoundHalfEvenGoesToTheEvenNeighbourOnATie)
{
    CHECK_EQ(round_half_even(0.0), 0.0);
    CHECK_EQ(round_half_even(0.5), 0.0);
    CHECK_EQ(round_half_even(1.5), 2.0);
    CHECK_EQ(round_half_even(2.5), 2.0);
    CHECK_EQ(round_half_even(3.5), 4.0);
    CHECK_EQ(round_half_even(0.49999999999999994), 0.0);
    CHECK_EQ(round_half_even(0.5000000000000001), 1.0);
    CHECK_EQ(round_half_even(4503599627370495.5), 4503599627370496.0); // 2^52 - 0.5 -> the even 2^52
    CHECK_EQ(round_half_even(4503599627370494.5), 4503599627370494.0);
    CHECK_EQ(round_half_even(4503599627370496.0), 4503599627370496.0); // already an integer
    CHECK_EQ(round_half_even(1e300), 1e300);
    CHECK_EQ(round_half_even(12.4), 12.0);
    CHECK_EQ(round_half_even(12.6), 13.0);
}

TEST(NumberText, IntegerDigitsAreExactForEveryMagnitude)
{
    struct Row
    {
        double value;
        const char *digits;
    };
    const Row rows[] = {
        {0.0, "0"},
        {1.0, "1"},
        {12.0, "12"},
        {999999999999.0, "999999999999"},
        {0x1.0000000000000p+53, "9007199254740992"},
        {0x1.b69b4ba630f35p+56, "123456789012345680"},
        {0x1.0000000000000p+63, "9223372036854775808"},
        {0x1.158e460913d00p+63, "10000000000000000000"},
        {0x1.0000000000000p+64, "18446744073709551616"},
        {0x1.0000000000001p+64, "18446744073709555712"},
        {0x1.0f0cf064dd592p+73, "10000000000000000000000"},
        {0x1.0000000000000p+100, "1267650600228229401496703205376"},
        {0x1.0000000000000p+200, "1606938044258990275541962092341162602522202993782792835301376"},
        {0x1.fffffffffffffp+1023,
         "179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953"
         "514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236"
         "903222948165808559332123348274797826204144723168738177180919299881250404026184124858368"},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("value " + ost_test::detail::exact_float(row.value));
        CHECK_STR_EQ(integer_digits(row.value), row.digits);
    }
}

TEST(NumberText, FormatDoubleWritesADotWhateverTheCLocaleSays)
{
    fmt_test::ScopedDecimalCommaLocale locale;
    CHECK_STR_EQ(format_double("%.2f", 12.345), "12.35");
    CHECK_STR_EQ(format_double("%g", 25.4), "25.4");
    CHECK_STR_EQ(format_double("%g", 1234567.5), "1.23457e+06");
    CHECK_STR_EQ(format_double("%g", -0.000123), "-0.000123");
    CHECK_STR_EQ(format_double("%.2f", 0.0), "0.00");
}
