// Plain number fields of the Edit Condition dialog (Rise, Run, Display Size): DLG-PREC-01 (text must round-trip the
// stored value; iOS showed %g, six significant digits, and re-saved from that text) and DLG-FINITE-01 (nan / inf /
// overflow are rejected).
//
// Windows oracle: presentation/dialogs/edit_condition_dialog.py _load_condition (str(cond.rise) if cond.rise else "",
// str(abs(cond.rise)) for the Area style, str(int(cond.display_size)) if cond.display_size else "") and
// _validate_and_build_dto (float(text); isfinite), plus Fixtures/formatting_oracle.txt (PLAIN and PLAINPARSE records
// produced by running exactly those Python expressions).
#include "FormattingTestSupport.hpp"

#include "Domain/DimensionFormat.hpp"

#include <cmath>
#include <limits>
#include <string>

using namespace ost_domain;

namespace
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();

    // text -> value, failing the test when the text is not a plain finite number
    double parsed(const std::string &text)
    {
        double value = -1.0;
        const PlainNumberStatus status = parse_plain_number(text, value);
        CHECK(status == PlainNumberStatus::Ok);
        return value;
    }
}

TEST(Prec01_PlainNumberText, TheTextIsPythonsReprSoItRoundTripsTheStoredValue)
{
    // values computed by running Windows (str(float)); iOS showed 3.14159 / 0.333333 / 7.12346 / 12
    struct Row
    {
        double value;
        const char *text;
    };
    const Row rows[] = {
        {3.14159265, "3.14159265"}, {0.333333333333, "0.333333333333"}, {12.0, "12.0"},     {2.5, "2.5"},
        {1e-7, "1e-07"},            {7.123456789, "7.123456789"},       {12.000001, "12.000001"}, {1e16, "1e+16"},
        {1e15, "1000000000000000.0"}, {-0.5, "-0.5"},                    {100.0, "100.0"},   {0.1, "0.1"},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("value " + ost_test::detail::exact_float(row.value));
        CHECK_STR_EQ(plain_number_text(row.value), row.text);
        CHECK_EQ(parsed(plain_number_text(row.value)), row.value);
    }
}

TEST(Prec01_PlainNumberText, ZeroIsAnEmptyField)
{
    CHECK_STR_EQ(plain_number_text(0.0), "");
    CHECK_STR_EQ(plain_number_text(-0.0), "");
    CHECK_STR_EQ(display_size_text(0.0), "");
    CHECK_STR_EQ(display_size_text(-0.0), "");
}

TEST(Prec01_PlainNumberText, TheAreaStyleShowsTheMagnitude)
{
    // Windows: str(abs(cond.rise)) if cond.rise else ""
    CHECK_STR_EQ(plain_number_text(std::fabs(-3.5)), "3.5");
    CHECK_STR_EQ(plain_number_text(std::fabs(-0.0)), "");
    CHECK_STR_EQ(plain_number_text(std::fabs(-7.123456789)), "7.123456789");
}

TEST(Prec01_PlainNumberText, DisplaySizeIsTheTruncatedInteger)
{
    // Windows: str(int(cond.display_size)) if cond.display_size else ""
    CHECK_STR_EQ(display_size_text(100.0), "100");
    CHECK_STR_EQ(display_size_text(100.7), "100");
    CHECK_STR_EQ(display_size_text(99.99), "99");
    CHECK_STR_EQ(display_size_text(-250.5), "-250");
    CHECK_STR_EQ(display_size_text(500.0), "500");
    CHECK_STR_EQ(display_size_text(0.5), "0");
    CHECK_STR_EQ(display_size_text(-0.5), "0");
    CHECK_STR_EQ(display_size_text(1e22), "10000000000000000000000");
    // int(nan) / int(inf) raise in Windows; the field simply stays empty here
    CHECK_STR_EQ(display_size_text(kNaN), "");
    CHECK_STR_EQ(display_size_text(kInf), "");
    CHECK_STR_EQ(display_size_text(-kInf), "");
}

TEST(Prec01_PlainNumberText, ASavedValueSurvivesTheDialogUnchanged)
{
    // Dialog level (COND-M02): rise / run shown and re-parsed without the user touching them keep the stored value
    for (double stored : {7.123456789, 12.000001, 0.3333333333, 3.14159265, 0.333333333333, 1e-7, 2.5, 12.0})
    {
        SCOPED_CONTEXT("stored " + ost_test::detail::exact_float(stored));
        CHECK_EQ(parsed(plain_number_text(stored)), stored);
        CHECK_EQ(parsed(plain_number_text(std::fabs(stored))), std::fabs(stored));
    }
}

TEST(Prec01_PlainNumberText, EveryWindowsOracleRecordMatches)
{
    const fmt_test::Oracle &oracle = fmt_test::oracle();
    REQUIRE(oracle.loaded);
    REQUIRE(oracle.plain.size() > 1000u);
    int mismatches = 0;
    for (const fmt_test::PlainCase &c : oracle.plain)
    {
        const std::string text = plain_number_text(c.value);
        const std::string abs_text = plain_number_text(std::fabs(c.value));
        const std::string size = display_size_text(c.value);
        const std::string want_size = c.display_size_ok ? c.display_size : std::string();
        if (text != c.text || abs_text != c.abs_text || size != want_size)
        {
            if (++mismatches <= 8)
            {
                SCOPED_CONTEXT("value " + ost_test::detail::exact_float(c.value));
                CHECK_STR_EQ(text, c.text);
                CHECK_STR_EQ(abs_text, c.abs_text);
                CHECK_STR_EQ(size, want_size);
            }
        }
        // finite non-zero values must read back exactly (the point of DLG-PREC-01)
        if (std::isfinite(c.value) && c.value != 0.0)
        {
            double back = 0.0;
            if (parse_plain_number(text, back) != PlainNumberStatus::Ok || back != c.value)
            {
                if (++mismatches <= 8)
                    ::ost_test::report_failure(__FILE__, __LINE__, "text \"" + text + "\" does not read back as " +
                                                                       ost_test::detail::exact_float(c.value));
            }
        }
    }
    CHECK_EQ(mismatches, 0);
}

TEST(Finite01_PlainNumbers, NonFiniteAndOverflowingTextIsRejected)
{
    // Windows: edit_condition_dialog.py _validate_and_build_dto: float(text) then isfinite (two different messages)
    double value = 0.0;
    for (const char *text : {"nan", "NaN", "-nan", "inf", "-inf", "+inf", "Infinity", "-Infinity", "infinity", "1e400",
                             "-1e400", "1e309"})
    {
        SCOPED_CONTEXT(std::string("text ") + text);
        CHECK(parse_plain_number(text, value) == PlainNumberStatus::NotFinite);
    }
    CHECK(parse_plain_number(std::string(5000, '9'), value) == PlainNumberStatus::NotFinite);
    CHECK(parse_plain_number("-" + std::string(5000, '9'), value) == PlainNumberStatus::NotFinite);
    CHECK_EQ(parsed("1e308"), 1e308);
    CHECK_EQ(parsed(std::string(300, '9')), 1e300);
}

TEST(Finite01_PlainNumbers, TextThatIsNotAFloatIsNotANumber)
{
    double value = 0.0;
    for (const char *text : {"", " ", "abc", "1/2", "3 4", "--3", "3-", "0x10", "1__0", "_1", "1_", "3'", "3\"", "5%",
                             "1e", "e1", "+", "-", ".", "3,5", "1 000", "1e+", "1e-"})
    {
        SCOPED_CONTEXT(std::string("text \"") + text + "\"");
        CHECK(parse_plain_number(text, value) == PlainNumberStatus::NotANumber);
    }
}

TEST(Finite01_PlainNumbers, TextThatIsAFloatIsAccepted)
{
    // float() strips surrounding whitespace itself and knows underscores, exponents and a bare leading / trailing dot
    CHECK_EQ(parsed("3"), 3.0);
    CHECK_EQ(parsed("-3"), -3.0);
    CHECK_EQ(parsed("+3"), 3.0);
    CHECK_EQ(parsed(" 3 "), 3.0);
    CHECK_EQ(parsed("\t3\n"), 3.0);
    CHECK_EQ(parsed("3.14159265"), 3.14159265);
    CHECK_EQ(parsed("1e2"), 100.0);
    CHECK_EQ(parsed("1E2"), 100.0);
    CHECK_EQ(parsed(".5"), 0.5);
    CHECK_EQ(parsed("5."), 5.0);
    CHECK_EQ(parsed("-.5"), -0.5);
    CHECK_EQ(parsed("1_000"), 1000.0);
    CHECK_EQ(parsed("\xD9\xA3"), 3.0); // ARABIC-INDIC DIGIT THREE
    CHECK_EQ(parsed("3\xC2\xA0"), 3.0); // NBSP is whitespace for float()
    CHECK_EQ(parsed("1e-400"), 0.0);    // underflow is 0.0, not an error
}

TEST(Finite01_PlainNumbers, EveryWindowsOracleRecordOfFloatMatches)
{
    const fmt_test::Oracle &oracle = fmt_test::oracle();
    REQUIRE(oracle.loaded);
    REQUIRE(oracle.plain_parse.size() > 400u);
    int mismatches = 0;
    for (const fmt_test::PlainParseCase &c : oracle.plain_parse)
    {
        double value = -12345.0;
        const PlainNumberStatus status = parse_plain_number(c.text, value);
        const int got = status == PlainNumberStatus::Ok ? 0 : status == PlainNumberStatus::NotANumber ? 1 : 2;
        if (got != c.status || (got == 0 && value != c.value))
        {
            if (++mismatches <= 8)
            {
                SCOPED_CONTEXT("text \"" + fmt_test::show(c.text) + "\"");
                CHECK_EQ(got, c.status);
                if (got == 0 && c.status == 0)
                    CHECK_EQ(value, c.value);
            }
        }
    }
    CHECK_EQ(mismatches, 0);
}
