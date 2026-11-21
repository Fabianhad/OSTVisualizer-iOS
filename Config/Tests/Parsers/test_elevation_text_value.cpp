// COND-ELEV-01: ost_domain::elevation_text_inches, the Core port of Windows elevation_text.py::elevation_text_inches (b9902e6): the one
// function that reads an elevation text. The loader (extract_z_value_from_name, position_parser.py:23-33) and the Edit Condition
// dialog (parse_elevation_z) both call it, so a text the dialog accepts is a text a reload reads to the same number.
//
// Windows oracle: tests/domain/services/test_elevation_text.py::ElevationTextValueTests (:525-572). Rows marked "run" are not in the
// Windows tests: they are values computed by running the Windows elevation_text.py (Python 3.11.4, executed in memory, no file
// written).
#include "ParsersTestSupport.hpp"

#include "Domain/PositionParser.hpp"

#include <cmath>
#include <string>

using namespace ost_domain;

namespace
{
    struct ValueRow
    {
        const char *text;
        bool has_value; // false: elevation_text_inches is None (NotAnElevation)
        double inches;
    };

    ElevationTextStatus read(const std::string &text, double &inches, std::string *error = nullptr)
    {
        return elevation_text_inches(text, inches, error);
    }

    bool is_status(const std::string &text, ElevationTextStatus want)
    {
        double inches = 0.0;
        return read(text, inches) == want;
    }
}

// Port of test_values_match_the_loader_and_unreadable_text_is_none: the 18 Windows rows. Each text also goes through the loader's
// function with the name "C @T <text>": the same number and Top, or (0.0, false) for the texts that are no elevation.
TEST(ElevationTextValue, ValuesMatchTheLoaderAndUnreadableTextIsNone)
{
    const ValueRow rows[] = {
        {"10' 6\"", true, 126.0},
        {" 10' 6\" ", true, 126.0},
        {"-2' 3 1/2\"", true, -27.5},
        {"6\"", true, 6.0},
        {"5'x", true, 60.0},
        {"3.2 m", true, 320.0 / 2.54},
        {"320 cm", true, 320.0 / 2.54},
        {"3m 20cm", true, 320.0 / 2.54},
        {"-1m+5cm", true, -105.0 / 2.54},
        {"3,5 m", true, 350.0 / 2.54},
        {"3200", false, 0.0},
        {"3200mm", false, 0.0},
        {"126", false, 0.0},
        {"10' 6", false, 0.0},
        {"5'", false, 0.0},
        {"10' 6\" 2m", false, 0.0},
        {"", false, 0.0},
        {"later", false, 0.0},
    };
    CHECK_EQ(sizeof(rows) / sizeof(rows[0]), size_t(18));
    for (const ValueRow &row : rows)
    {
        SCOPED_CONTEXT(std::string("text=[") + row.text + "]");
        double inches = 777.0;
        std::string error;
        const ElevationTextStatus status = read(row.text, inches, &error);

        double z = 555.0;
        bool top = !row.has_value;
        std::string loader_error;
        CHECK(extract_z_value_from_name(std::string("C @T ") + row.text, z, top, &loader_error));
        if (row.has_value)
        {
            CHECK(status == ElevationTextStatus::Value);
            CHECK_NEAR(inches, row.inches, ost_parsers_test::kAlmost9);
            CHECK_EQ(z, inches); // the loader gives exactly the number of the text
            CHECK(top);
        }
        else
        {
            CHECK(status == ElevationTextStatus::NotAnElevation);
            CHECK_EQ(inches, 777.0); // nothing is written for a text that is no elevation
            CHECK_EQ(z, 0.0);
            CHECK_FALSE(top);
        }
        CHECK(error.empty());
        CHECK(loader_error.empty());
    }
}

// Port of test_a_zero_denominator_raises_like_the_loader: the Windows function raises ValueError("zero denominator in fraction 1/0")
// and the loader lets it through (the MDB reader catches it and stores (0.0, False)). Here: the status Raises with the Windows text.
TEST(ElevationTextValue, AZeroDenominatorRaisesLikeTheLoader)
{
    for (const char *text : {"1/0\"", "5' 6 1/0\"", "3 1/0\"", "-1/0\""})
    {
        SCOPED_CONTEXT(std::string("text=[") + text + "]");
        double inches = 777.0;
        std::string error;
        CHECK(read(text, inches, &error) == ElevationTextStatus::Raises);
        CHECK_STR_EQ(error, "zero denominator in fraction 1/0");
        CHECK_EQ(inches, 777.0);

        // the loader function reports the same failure, with the same text and untouched outputs
        double z = 555.0;
        bool top = true;
        std::string loader_error;
        CHECK_FALSE(extract_z_value_from_name(std::string("Wall @T ") + text, z, top, &loader_error));
        CHECK_STR_EQ(loader_error, "zero denominator in fraction 1/0");
        CHECK_EQ(z, 0.0);
        CHECK_FALSE(top);
    }
    // without a destination for the message the status is the same
    double inches = 0.0;
    CHECK(elevation_text_inches("1/0\"", inches) == ElevationTextStatus::Raises);
}

// Run on Windows (not in its tests): a feet run beyond the float range raises OverflowError ("int too large to convert to float")
// and one beyond Python's 4300-digit int() limit raises ValueError. Both are Raises here; the dialog (parse_elevation_z) refuses them
// with its normal message instead of copying the uncaught OverflowError of the Windows dialog.
TEST(ElevationTextValue, FeetOverTheFloatRangeOrDigitLimitRaises)
{
    const std::string over_float = "1" + std::string(404, '0') + "'x"; // 405 digits: OverflowError on Windows
    const std::string over_limit = "1" + std::string(4399, '0') + "'x"; // 4400 digits: ValueError on Windows
    double inches = 777.0;
    std::string error;
    CHECK(read(over_float, inches, &error) == ElevationTextStatus::Raises);
    CHECK_STR_EQ(error, "int too large to convert to float");
    CHECK(read(over_limit, inches) == ElevationTextStatus::Raises);
    CHECK(read("-" + over_float, inches) == ElevationTextStatus::Raises);
    CHECK(read("1" + std::string(404, '0') + "' 6\"", inches) == ElevationTextStatus::Raises);
    CHECK_EQ(inches, 777.0);

    // the loader function raises on the same names
    double z = 0.0;
    bool top = false;
    CHECK_FALSE(extract_z_value_from_name("Wall @T " + over_float, z, top));
    CHECK_FALSE(extract_z_value_from_name("Wall @T " + over_limit, z, top));

    // 307 digits of feet still fit (1e306 feet are 1.2e307 inches, below the float maximum 1.8e308); Windows gives 1.2e+307
    CHECK(read("1" + std::string(306, '0') + "'x", inches) == ElevationTextStatus::Value);
    CHECK(std::isfinite(inches));
    CHECK_EQ(inches, 1.2e307);
}

// Run on Windows: metres beyond the float range give inf ("1" + 400 zeros + " m" -> inf, no exception); the dialog refuses a
// non-finite number (isfinite), the function itself reports the Windows value.
TEST(ElevationTextValue, MetresBeyondTheFloatRangeAreInfinite)
{
    double inches = 0.0;
    CHECK(read("1" + std::string(400, '0') + " m", inches) == ElevationTextStatus::Value);
    CHECK(std::isinf(inches));
    CHECK(inches > 0.0);
    CHECK(read("-1" + std::string(400, '0') + " m", inches) == ElevationTextStatus::Value);
    CHECK(std::isinf(inches));
    CHECK(inches < 0.0);
}

// Run on Windows (not in its tests): the quirks of the shared gate. "5'" alone is no elevation (the feet-only pattern needs a word
// boundary after the foot mark: "5'", "5' " and "-4'" fail) but "5'x" and "5'6" are 5 feet and the rest is dropped; "10'6" is 120.0
// (feet only), not 126.0; decimal inches, an inch fraction after feet without a mark, a letter m next to an inch or foot mark and a
// bare number are no elevation. Two different refusals share the letter m: "5' foam" never passes the gate (the feet-only pattern
// needs a word boundary right after the foot mark and a blank follows it), "5'foam" and "6\" mm" pass the gate and are then mixed
// units (an imperial mark and a letter m: None). Windows b9902e6, elevation_text_inches run in memory: "5' foam" gate False / None,
// "5'foam" gate True / None, "6\" mm" gate True / None.
TEST(ElevationTextValue, TheGateKeepsTheLoaderQuirks)
{
    struct Row
    {
        const char *text;
        bool has_value;
        double inches;
    };
    const Row rows[] = {
        {"5'6", true, 60.0},
        {"5'x", true, 60.0},
        {"10'6", true, 120.0},
        {"-4' 6\"", true, -54.0},
        {"5'", false, 0.0},
        {"5' ", false, 0.0},
        {"-4'", false, 0.0},
        {"5", false, 0.0},
        {"10' 6.3\"", false, 0.0},
        {"6.5\"", false, 0.0},
        {"5' 1/2\"", false, 0.0},
        {"5' foam", false, 0.0}, // refused by the gate, not as mixed units
        {"5'foam", false, 0.0},  // gate true, then mixed units
        {"6\" mm", false, 0.0},  // gate true, then mixed units
        {"3200 mm", false, 0.0},
        {"nan", false, 0.0},
        {"1e2", false, 0.0},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(std::string("text=[") + row.text + "]");
        double inches = 777.0;
        const ElevationTextStatus status = read(row.text, inches);
        if (row.has_value)
        {
            CHECK(status == ElevationTextStatus::Value);
            CHECK_EQ(inches, row.inches);
        }
        else
        {
            CHECK(status == ElevationTextStatus::NotAnElevation);
            CHECK_EQ(inches, 777.0);
        }
    }
    CHECK(is_status("5'", ElevationTextStatus::NotAnElevation));
}

// Python str.strip() around the text (elevation_text.py:132): Unicode white space and the C0 separators go, as the loader and the
// dialog both see the text.
TEST(ElevationTextValue, TheTextIsStrippedLikePythonStrip)
{
    double inches = 0.0;
    CHECK(read(OST_U_NBSP "3.2 m" OST_U_EM, inches) == ElevationTextStatus::Value);
    CHECK_NEAR(inches, 320.0 / 2.54, ost_parsers_test::kAlmost9);
    CHECK(read("\t10' 6\"\n", inches) == ElevationTextStatus::Value);
    CHECK_EQ(inches, 126.0);
    // a zero width space is not white space
    CHECK(is_status(OST_U_ZWSP "3.2 m", ElevationTextStatus::NotAnElevation));
}
