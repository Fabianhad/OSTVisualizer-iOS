// The closing-inch-mark rule of the Elevation field of the Edit Condition dialog (item 1 of job 6).
//
// After the shared parser (elevation_text_inches, the loader's own) accepted the text, the dialog refuses it when ANY non-blank
// character follows a foot mark (' or U+2032), with or without a blank, unless that is a lone x or X ("5'x", which may be followed by
// white space or by U+180E U+200B U+200C U+200D U+2060 U+FEFF), and unless the text starts with a real feet-and-inches value (5'6",
// the inch marks U+2033 and U+201D read like '"'). So "5'6", "10'6", "5'6x", "5'abc", "-2'3", "5'x\"", "5'6x\"", "10'6.3\"" and "5'e-acute"
// are refused with the usual message and the hint of the bid (before, the loader's grammar read "5'6" as 5 feet and dropped the rest,
// and the dialog saved 5 feet without a word), while "5'x", "5' 6\"", "5'6\"", "5'6<U+2033>" and "10'6 1/2\"" are accepted. The loader
// and the shared parser still read a stored name "Footing @T 5'6" as 60 inches, top (and "5'6<U+2033>" as 66). The decision is
// ost_domain::elevation_text_needs_closing_inch_mark (ConditionFormRules.hpp), called by parse_elevation_z.
//
// Windows source: Windows HEAD d26893c (0a24294), presentation/dialogs/edit_condition_dialog.py :184-188
// (_TEXT_AFTER_FOOT_MARK) and :1620-1646 (_validate_and_build_dto: strip, elevation_text_inches, the rule on the stripped text unless
// IMPERIAL_FEET_INCHES matches the inch-normalised text, one refusal for None and a non-finite number); Windows tests
// tests/presentation/dialogs/test_edit_condition_dialog.py EditConditionElevationRuleTests (:1774-1873), :1376-1423
// (test_text_right_after_the_foot_mark_needs_a_closing_inch_mark, test_feet_with_x_and_feet_with_closed_inches_are_still_accepted,
// test_the_loader_still_reads_a_stored_feet_digit_name_as_feet). Every other row was RECOMPUTED for Windows d26893c by running the
// Windows elevation_text.py and the real regex constants in memory (Python 3.11.4), applying _validate_and_build_dto exactly; the
// column "rule" is `not IMPERIAL_FEET_INCHES.match(normalize_inch_marks(text)) and RE.search(text)` on the text as written, whatever
// the shared parser says. 45 rows changed against the 49a2d59 table (35 of them flip acceptance, the rest only the column "rule" of
// a refused row). The owner decision of job 7 (the stray quote and the ASCII-only class "ported as is", COND-ELEV-01) is withdrawn:
// Windows fixed both in 0a24294.
//
// The groups below run through parse_elevation_z in a metric AND an imperial bid: acceptance does not depend on the bid, only the
// hint of a refusal does.
#include "TestHarness.hpp"

#include "Domain/ConditionFormRules.hpp"
#include "Domain/PositionParser.hpp"
#include "Import/TextRules.hpp"
#include "Persistence/ProjectStore/ProjectStore_Internal.hpp"

#include <cmath>
#include <cstddef>
#include <string>

using ost_domain::parse_elevation_z;

namespace
{
    // The Windows wording, spelled out (not taken from the constants under test)
    const char *const kWindowsImperialHint = "Use feet and inches with unit marks, for example 10' 6\".";
    const char *const kWindowsMetricHint = "Use meters or centimeters, for example \"3.2 m\" or \"320 cm\".";

    // A text typed into the Elevation field and what the Windows dialog does with it.
    struct Row
    {
        const char *text;
        bool accepted;  // the Windows dialog result for the text as typed (it strips it first)
        double inches;  // the number it saves when accepted; 0.0 is a placeholder in a refused row
        bool rule;      // the rule alone on the text AS WRITTEN: no ASCII '"' and the regex finds a match
    };

    // A text for the rule alone (no dialog result): the free function is applied to the text exactly as written.
    struct RuleRow
    {
        const char *text;
        bool rule;
    };

    // The text with every byte outside printable ASCII written as \xHH, so that a failure names the invisible characters.
    std::string shown(const std::string &text)
    {
        static const char digits[] = "0123456789ABCDEF";
        std::string out;
        for (const char ch : text)
        {
            const unsigned char byte = static_cast<unsigned char>(ch);
            if (byte >= 0x20 && byte < 0x7F)
                out += ch;
            else
            {
                out += "\\x";
                out += digits[byte >> 4];
                out += digits[byte & 0x0F];
            }
        }
        return out;
    }

    // Refused in the given bid: "Invalid Value", '"<stripped text>" is not a valid elevation. <hint of the bid>', z left untouched.
    void expect_refused(const std::string &text, bool metric)
    {
        SCOPED_CONTEXT("refused? text=[" + shown(text) + "] metric=" + (metric ? "1" : "0"));
        double z = -1.0;
        const auto issue = parse_elevation_z(text, metric, z);
        if (!issue)
        {
            CHECK(issue.has_value()); // the text was accepted
            return;
        }
        CHECK_STR_EQ(issue->title, "Invalid Value");
        CHECK_STR_EQ(issue->message, "\"" + ost_import::text_rules::py_strip(text) + "\" is not a valid elevation. " +
                                         (metric ? kWindowsMetricHint : kWindowsImperialHint));
        CHECK_EQ(z, -1.0); // a refusal leaves the output untouched
    }

    // Accepted in both bids, with no issue and z == inches; and the reload of the name the dialog would save (the stripped text
    // verbatim after " @T " / " @B ") gives the same number and the same segment (Windows: the saved value is the reloaded value).
    void expect_accepted(const std::string &text, double inches)
    {
        const std::string stripped = ost_import::text_rules::py_strip(text);
        for (const bool metric : {true, false})
        {
            for (const bool bottom : {false, true})
            {
                SCOPED_CONTEXT("accepted? text=[" + shown(text) + "] metric=" + (metric ? "1" : "0") + " bottom=" + (bottom ? "1" : "0"));
                double z = -1.0;
                CHECK_FALSE(parse_elevation_z(text, metric, z).has_value());
                CHECK_NEAR(z, inches, 1e-9);

                const std::string name = std::string("Footing") + (bottom ? " @B " : " @T ") + stripped;
                double reloaded_z = 123.0;
                bool reloaded_top = bottom;
                ost_domain::derive_elevation_from_name(name, reloaded_z, reloaded_top);
                CHECK_EQ(reloaded_z, z);
                CHECK(reloaded_top == !bottom);
            }
        }
    }

    template <size_t N>
    void check_rows(const Row (&rows)[N])
    {
        for (const Row &row : rows)
        {
            if (row.accepted)
                expect_accepted(row.text, row.inches);
            else
            {
                expect_refused(row.text, true);
                expect_refused(row.text, false);
            }
        }
    }

    // A_refused_both_modes
    const Row kGroupA[] = {
        {"5'6", false, 0.0, true},
        {"5'6x", false, 0.0, true},
        {"10'6", false, 0.0, true},
        {"5'abc", false, 0.0, true},
        {"-2'3", false, 0.0, true},
        {"5\xE2\x80\xB2" "6", false, 0.0, true},
    };

    // B_accepted
    const Row kGroupB[] = {
        {"5'x", true, 60.0, false},
        {"5'X", true, 60.0, false},
        {"-4'x", true, -48.0, false},
        {"5' 6\"", true, 66.0, false},
        {"5'6\"", true, 66.0, false},
        {"10'6 1/2\"", true, 126.5, false},
    };

    // C_whitespace
    const Row kGroupC[] = {
        {"5'x ", true, 60.0, false},
        {"5'x\n", true, 60.0, false},
        {"5'x\n\n", true, 60.0, false},
        {"5'x \t", true, 60.0, false},
        {"5'x\xC2\xA0", true, 60.0, false},
        {"5'x\xE2\x80\x89", true, 60.0, false},
        {"5'x\xE3\x80\x80", true, 60.0, false},
        {"5'x\xC2\x85", true, 60.0, false},
        {"5'x\x1C", true, 60.0, false},
        {" 5'x", true, 60.0, false},
        {"\xC2\xA0" "5'x", true, 60.0, false},
        {" 5'x ", true, 60.0, false},
        {"5 'x", true, 60.0, false},
        {"5'x\xE2\x80\x8B", true, 60.0, false},
        {"5'x\xEF\xBB\xBF", true, 60.0, false},
        {"5'x\xE1\xA0\x8E", true, 60.0, false},
        {"5'x y", false, 0.0, true},
        {"5'x\x1Cy", false, 0.0, true},
        {"5' x", false, 0.0, true},
        {"5' \t6", false, 0.0, true},
        {"5'\xC2\xA0" "6", false, 0.0, true},
        {"5'\xE2\x80\x89" "6", false, 0.0, true},
        {"5'\xE3\x80\x80" "6", false, 0.0, true},
        {"5'\xC2\x85" "6", false, 0.0, true},
        {"5'\x1C" "6", false, 0.0, true},
        {"5'\x1Cx", false, 0.0, true},
        {"5'\xC2\xA0x", false, 0.0, true},
        {"5 '6", false, 0.0, true},
        {"- 5'6", false, 0.0, true},
        {"\xC2\xA0" "5'6\xC2\xA0", false, 0.0, true},
        {"5'6 hello", false, 0.0, true},
        {"5'x hello", false, 0.0, true},
        {"5'6\xC2\xA0", false, 0.0, true},
    };

    // D_non_ascii_after_mark
    const Row kGroupD[] = {
        {"5'\xC3\xA9", false, 0.0, true},
        {"5'\xC3\x89", false, 0.0, true},
        {"5'\xC3\x9F", false, 0.0, true},
        {"5'\xC3\xA9\xC3\xA9", false, 0.0, true},
        {"5'\xD9\xA3", false, 0.0, true},
        {"5'\xDB\xB3", false, 0.0, true},
        {"5'\xEF\xBC\x96", false, 0.0, true},
        {"5'\xC2\xB2", false, 0.0, true},
        {"5'_", false, 0.0, true},
        {"5'_6", false, 0.0, true},
        {"5'\xC5\xBF", false, 0.0, true},
        {"5'\xE2\x84\xAA", false, 0.0, true},
        {"5'\xC4\xB1", false, 0.0, true},
        {"5'\xC2\xAA", false, 0.0, true},
        {"5'\xE4\xB8\xAD", false, 0.0, true},
        {"5'\xE2\x85\xA0", false, 0.0, true},
        {"5'\xF0\x9D\x9F\x98", false, 0.0, true},
        {"\xD9\xA5'x", true, 60.0, false},
        {"\xD9\xA5'\xD9\xA6", false, 0.0, true},
        {"\xD9\xA5'6", false, 0.0, true},
        {"\xD9\xA5" "5'6", false, 0.0, true},
    };

    // E_quotes
    const Row kGroupE[] = {
        {"5'6\xE2\x80\x9D", true, 66.0, false},
        {"5'6\xE2\x80\xB3", true, 66.0, false},
        {"5'6\xE2\x80\x9C", false, 0.0, true},
        {"5'6\xE2\x80\x99", false, 0.0, true},
        {"5'x\xE2\x80\xB3", false, 0.0, true},
        {"5'x\xE2\x80\x9D", false, 0.0, true},
        {"5'6\xE2\x80\xB3\"", true, 66.0, false},
        {"5'x\"", false, 0.0, true},
        {"5'6x\"", false, 0.0, true},
        {"5'abc\"", false, 0.0, true},
        {"10'6.3\"", false, 0.0, true},
        {"6\"5'7", false, 0.0, true},
        {"5'x1/2\"", false, 0.0, true},
        {"5'6 \"", true, 66.0, false},
        {"5'6\"x", true, 66.0, false},
        {"5'6\"abc", true, 66.0, false},
        {"5'\"6", false, 0.0, true},
    };

    // F_multiple_marks
    const Row kGroupF[] = {
        {"5'6'7", false, 0.0, true},
        {"5'6''", false, 0.0, true},
        {"5'x'", false, 0.0, true},
        {"5'x'6", false, 0.0, true},
        {"5'x'x", false, 0.0, true},
        {"5'x'xy", false, 0.0, true},
        {"5'_'6", false, 0.0, true},
        {"5'_'x", false, 0.0, true},
        {"5'_'xy", false, 0.0, true},
        {"5'_'", false, 0.0, true},
        {"5'_' ", false, 0.0, true},
        {"5'\xC3\xA9'6", false, 0.0, true},
        {"5'\xC3\xA9'x", false, 0.0, true},
        {"5'\xC3\xA9'x ", false, 0.0, true},
        {"5'\xC3\xA9'xy", false, 0.0, true},
        {"5'\xC3\xA9' 6", false, 0.0, true},
        {"5'\xC3\xA9' x", false, 0.0, true},
        {"5'6\"'7", true, 66.0, false},
        {"5'6\"7'8", true, 66.0, false},
    };

    // G_prime
    const Row kGroupG[] = {
        {"5\xE2\x80\xB2x", true, 60.0, false},
        {"5\xE2\x80\xB2X", true, 60.0, false},
        {"-4\xE2\x80\xB2x", true, -48.0, false},
        {"5\xE2\x80\xB2 6\"", true, 66.0, false},
        {"5\xE2\x80\xB2" "6\"", true, 66.0, false},
        {"5\xE2\x80\xB2" "abc", false, 0.0, true},
        {"5\xE2\x80\xB2x6", false, 0.0, true},
        {"5\xE2\x80\xB2" "6", false, 0.0, true},
        {"5\xE2\x80\xB2x ", true, 60.0, false},
        {"5\xE2\x80\xB2", false, 0.0, false},
        {"5\xE2\x80\xB2 ", false, 0.0, false},
    };

    // H_unaffected
    const Row kGroupH[] = {
        {"3.2 m", true, 125.98425196850394, false},
        {"320 cm", true, 125.98425196850394, false},
        {"3m20cm", true, 125.98425196850394, false},
        {"3m 20cm", true, 125.98425196850394, false},
        {"3,5 m", true, 137.79527559055117, false},
        {"-1m+5cm", true, -41.338582677165356, false},
        {"3 meters", true, 118.11023622047244, false},
        {"30 centimeters", true, 11.811023622047244, false},
        {"6\"", true, 6.0, false},
        {"10' 6\"", true, 126.0, false},
        {"-2' 3 1/2\"", true, -27.5, false},
        {"2 m", true, 78.74015748031496, false},
        {"3'", false, 0.0, false},
        {"5'", false, 0.0, false},
        {"5' ", false, 0.0, false},
        {"3200", false, 0.0, false},
        {"5' 6", false, 0.0, true},
        {"10' 6", false, 0.0, true},
        {"3'6 m", false, 0.0, true},
        {"5'6m", false, 0.0, true},
        {"5'6 mm", false, 0.0, true},
        {"5'x m", false, 0.0, true},
        {"5'6 foam", false, 0.0, true},
        {"5'foam", false, 0.0, true},
        {"5' foam", false, 0.0, true},
        {"5'xx m", false, 0.0, true},
    };

    // I_xx_family
    const Row kGroupI[] = {
        {"5'xx", false, 0.0, true},
        {"5'x6", false, 0.0, true},
        {"5'xX", false, 0.0, true},
        {"5'xy", false, 0.0, true},
        {"5'x.", false, 0.0, true},
        {"5'x-", false, 0.0, true},
        {"5'x1/2", false, 0.0, true},
        {"5'x\t6", false, 0.0, true},
        {"5'x'", false, 0.0, true},
    };

    // J_decimal_zero
    const Row kGroupJ[] = {
        {"5.5'6", false, 0.0, true},
        {"5.5'x", false, 0.0, false},
        {"5,5'6", false, 0.0, true},
        {"0'6", false, 0.0, true},
        {"00'6", false, 0.0, true},
        {"5'06", false, 0.0, true},
        {"-0'6", false, 0.0, true},
        {"007  '6  1/2", false, 0.0, true},
    };

    // K_foreign_marks
    const Row kGroupK[] = {
        {"5\xE2\x80\xB2\xE2\x80\xB2" "6", false, 0.0, true},
        {"5'\xE2\x80\xB2" "6", false, 0.0, true},
        {"5''6", false, 0.0, true},
        {"'6", false, 0.0, true},
        {"x'6", false, 0.0, true},
        {"-'6", false, 0.0, true},
        {"1_0'6", false, 0.0, true},
        {"5\xE2\x80\x99" "6", false, 0.0, false},
        {"5\xCA\xBC" "6", false, 0.0, false},
        {"5\xC2\xB4" "6", false, 0.0, false},
        {"'x", false, 0.0, false},
        {"'", false, 0.0, false},
        {"''", false, 0.0, true},
        {"-", false, 0.0, false},
        {"5", false, 0.0, false},
        {"' 6\"", false, 0.0, true},
        {"'6\"", false, 0.0, true},
    };

    // L_ascii_class_edges
    const Row kGroupL[] = {
        {"5'0", false, 0.0, true},
        {"5'9", false, 0.0, true},
        {"5'A", false, 0.0, true},
        {"5'Z", false, 0.0, true},
        {"5'a", false, 0.0, true},
        {"5'z", false, 0.0, true},
        {"5'X hello", false, 0.0, true},
        {"5'Xy", false, 0.0, true},
        {"5'ABC", false, 0.0, true},
        {"5'X6", false, 0.0, true},
        {"5'/", false, 0.0, true},
        {"5':", false, 0.0, true},
        {"5'@", false, 0.0, true},
        {"5'[", false, 0.0, true},
        {"5'`", false, 0.0, true},
        {"5'{", false, 0.0, true},
    };
}

// ---------------------------------------------------------------------------------------------------------------------
// What the Windows tests check
// ---------------------------------------------------------------------------------------------------------------------

// Port of test_text_right_after_the_foot_mark_needs_a_closing_inch_mark (the Windows rows run in one bid each; here every row runs
// in both): any non-blank character after the foot mark with no closing inch mark is refused (Windows 0a24294; was letters and digits only), whatever follows, U+2032 included.
TEST(CondElev02_ClosingInchMark, TextRightAfterTheFootMarkNeedsAClosingInchMark)
{
    check_rows(kGroupA);
}

// Port of test_feet_with_x_and_feet_with_closed_inches_are_still_accepted: the lone x, and any text with a closing inch mark.
TEST(CondElev02_ClosingInchMark, FeetWithXAndFeetWithClosedInchesAreStillAccepted)
{
    check_rows(kGroupB);
}

// Port of test_the_loader_still_reads_a_stored_feet_digit_name_as_feet: the rule lives in the dialog only. The loader's functions
// (and the shared parser behind them) still read "5'6" as 5 feet, so a name that is stored (or typed whole into the Name field)
// loads as before.
TEST(CondElev02_ClosingInchMark, TheLoaderStillReadsAStoredFeetDigitNameAsFeet)
{
    double z = 0.0;
    bool is_top = false;
    ost_domain::derive_elevation_from_name("Footing @T 5'6", z, is_top);
    CHECK_EQ(z, 60.0);
    CHECK(is_top);
    z = 0.0;
    is_top = true;
    ost_domain::derive_elevation_from_name("Footing @B 10'6", z, is_top);
    CHECK_EQ(z, 120.0);
    CHECK_FALSE(is_top);

    z = 0.0;
    is_top = false;
    CHECK(ost_domain::extract_z_value_from_name("Footing @T 5'6", z, is_top));
    CHECK_EQ(z, 60.0);
    CHECK(is_top);
    z = 0.0;
    is_top = true;
    CHECK(ost_domain::extract_z_value_from_name("Footing @B 10'6", z, is_top));
    CHECK_EQ(z, 120.0);
    CHECK_FALSE(is_top);

    const std::pair<double, bool> top = ost_store::extract_z_value("Footing @T 5'6");
    CHECK_EQ(top.first, 60.0);
    CHECK(top.second);
    const std::pair<double, bool> bottom = ost_store::extract_z_value("Footing @B 10'6");
    CHECK_EQ(bottom.first, 120.0);
    CHECK_FALSE(bottom.second);

    // the shared parser itself is unchanged: it accepts both texts, feet only
    double inches = 0.0;
    CHECK(ost_domain::elevation_text_inches("5'6", inches) == ost_domain::ElevationTextStatus::Value);
    CHECK_EQ(inches, 60.0);
    CHECK(ost_domain::elevation_text_inches("10'6", inches) == ost_domain::ElevationTextStatus::Value);
    CHECK_EQ(inches, 120.0);

    // the rule only looks at the Elevation field: the same text typed whole into the Name field is a valid name
    std::string name;
    CHECK_FALSE(ost_domain::parse_condition_name("Footing @T 5'6", name).has_value());
    CHECK_STR_EQ(name, "Footing @T 5'6");
}

// ---------------------------------------------------------------------------------------------------------------------
// The refusal
// ---------------------------------------------------------------------------------------------------------------------

// The refusal is the existing one: the title, the wording of a refusal by the shared parser, and the hint of the bid; the text in
// the message is the STRIPPED text (as Windows quotes elev_value), U+2032 verbatim.
TEST(CondElev02_ClosingInchMark, TheRefusalIsTheExistingMessageWithTheHintOfTheBid)
{
    double z = -1.0;
    const auto imperial = parse_elevation_z("5'6", false, z);
    REQUIRE(imperial.has_value());
    CHECK_STR_EQ(imperial->title, "Invalid Value");
    CHECK_STR_EQ(imperial->message, "\"5'6\" is not a valid elevation. Use feet and inches with unit marks, for example 10' 6\".");
    CHECK_EQ(z, -1.0);

    const auto metric = parse_elevation_z("5'6", true, z);
    REQUIRE(metric.has_value());
    CHECK_STR_EQ(metric->title, "Invalid Value");
    CHECK_STR_EQ(metric->message, "\"5'6\" is not a valid elevation. Use meters or centimeters, for example \"3.2 m\" or \"320 cm\".");
    CHECK_EQ(z, -1.0);

    // the text is quoted as stripped (Unicode white space included), inner characters as typed
    const auto stripped = parse_elevation_z("\xC2\xA0" "  5'6x\t", false, z);
    REQUIRE(stripped.has_value());
    CHECK_STR_EQ(stripped->message, "\"5'6x\" is not a valid elevation. " + std::string(kWindowsImperialHint));
    CHECK_EQ(z, -1.0);

    // the prime mark is quoted verbatim
    const auto prime = parse_elevation_z("5\xE2\x80\xB2" "6", true, z);
    REQUIRE(prime.has_value());
    CHECK_STR_EQ(prime->message, "\"5\xE2\x80\xB2" "6\" is not a valid elevation. " + std::string(kWindowsMetricHint));
    CHECK_EQ(z, -1.0);

    // the very wording a refusal by the shared parser gives: only the quoted text differs
    const auto shared = parse_elevation_z("3200", true, z);
    REQUIRE(shared.has_value());
    CHECK_STR_EQ(shared->title, metric->title);
    CHECK_STR_EQ(shared->message, "\"3200\" is not a valid elevation. " + std::string(kWindowsMetricHint));
    CHECK_STR_EQ(shared->message.substr(shared->message.find("is not a valid elevation.")),
                 metric->message.substr(metric->message.find("is not a valid elevation.")));
    const auto shared_imperial = parse_elevation_z("126", false, z);
    REQUIRE(shared_imperial.has_value());
    CHECK_STR_EQ(shared_imperial->message.substr(shared_imperial->message.find("is not a valid elevation.")),
                 imperial->message.substr(imperial->message.find("is not a valid elevation.")));
    CHECK_EQ(z, -1.0);
}

// ---------------------------------------------------------------------------------------------------------------------
// The decision on its own
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
    // Texts for the rule alone that are not dialog rows. Robustness of the iOS port: an invalid UTF-8 byte reads as U+FFFD, which is
    // not white space, so it counts as a non-blank character like any other (Windows has no such text: a QLineEdit holds valid UTF-16).
    const RuleRow kRuleOnly[] = {
        {"", false},
        {"'", false},
        {"''", true},                 // the second mark is a character after the first
        {"5", false},
        {"5'6\xFF", true},            // the six after the mark counts, the byte after it does not matter
        {"5'\xFF" "6", true},         // the invalid byte sits right after the mark: not white space, so it counts (U+FFFD)
        {"5'x\xFF", true},            // the x is not the last non-blank character
        {"\xFF" "5'x", false},        // nothing about a byte before the feet
        {"5'\n6", true},              // a line feed is white space between the mark and the character (the regex \s)
        {"5'\t\t x", true},           // a blank run between the mark and an x that is NOT directly after it: no exemption
    };

    template <size_t N>
    void check_rule_rows(const Row (&rows)[N], size_t &checked)
    {
        for (const Row &row : rows)
        {
            SCOPED_CONTEXT("rule text=[" + shown(row.text) + "]");
            CHECK(ost_domain::elevation_text_needs_closing_inch_mark(row.text) == row.rule);
            ++checked;
        }
    }
}

// elevation_text_needs_closing_inch_mark applied to the text exactly as written (no strip, unlike the dialog): the value of the
// column "rule" of every row of the groups above. It mirrors the regex, so "5'x\n" and "5'x" + NBSP are safe (the lookahead allows
// white space up to the end), "5' x" and "5'6" + NBSP are not, and a text that starts with a real feet-and-inches value ("5'6\"x")
// is never refused, whatever follows it.
TEST(CondElev02_ClosingInchMark, TheRuleOnTheFreeFunction)
{
    size_t checked = 0;
    check_rule_rows(kGroupA, checked);
    check_rule_rows(kGroupB, checked);
    check_rule_rows(kGroupC, checked);
    check_rule_rows(kGroupD, checked);
    check_rule_rows(kGroupE, checked);
    check_rule_rows(kGroupF, checked);
    check_rule_rows(kGroupG, checked);
    check_rule_rows(kGroupH, checked);
    check_rule_rows(kGroupI, checked);
    check_rule_rows(kGroupJ, checked);
    check_rule_rows(kGroupK, checked);
    check_rule_rows(kGroupL, checked);
    CHECK_EQ(checked, size_t(189));

    for (const RuleRow &row : kRuleOnly)
    {
        SCOPED_CONTEXT("rule text=[" + shown(row.text) + "]");
        CHECK(ost_domain::elevation_text_needs_closing_inch_mark(row.text) == row.rule);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The edges of the rule (every row computed with the Windows functions in memory)
// ---------------------------------------------------------------------------------------------------------------------

// White space: what follows the x is stripped before the rule sees it (and the lookahead tolerates it anyway), what sits between
// the mark and the character cannot be reached on a first mark (the shared parser refuses it first), other characters that look
// like white space (U+200B, U+FEFF, U+180E) are neither stripped nor white space but the lone x tolerates them (Windows 0a24294:
// U+180E U+200B U+200C U+200D U+2060 U+FEFF after the x), and the C0 separators U+001C..U+001F and U+0085 ARE white space (Python's
// str.strip() and \s). (The test name is kept: PARITY_DIVERGENCES.json cites it.)
TEST(CondElev02_ClosingInchMark, WhiteSpaceAfterTheXIsStrippedButOtherCharactersAreNot)
{
    check_rows(kGroupC);
}

// ANY non-blank character after the mark counts since Windows 0a24294 (the class is \S, no longer [0-9A-Za-z]): accented and CJK
// letters, non-ASCII digits, a superscript, the underscore, U+017F (long s) and U+212A (Kelvin sign) are refused like the ASCII
// ones (the 18 rows of this group that the 49a2d59 rule accepted as is). A feet digit may be non-ASCII itself; the text is refused
// as before when the ASCII six follows it. (The test name is kept: PARITY_DIVERGENCES.json cites it.)
TEST(CondElev02_ClosingInchMark, OnlyAsciiLettersAndDigitsCountAfterTheFootMark)
{
    check_rows(kGroupD);
}

// A double quote no longer switches the rule off (Windows 0a24294): 5'x", 5'6x", 5'abc", 5'x1/2", 10'6.3" and 6"5'7 are refused.
// The rule is skipped only when the text starts with a real feet-and-inches value, with U+2033 and U+201D read as inch marks
// (5'6<U+2033> and 5'6<U+201D> are 66, 5'6<U+2033>" is 66 and no longer 60); U+201C is no inch mark. 5'x<U+2033> stays refused.
// (The test name is kept: PARITY_DIVERGENCES.json cites it.)
TEST(CondElev02_ClosingInchMark, OnlyTheAsciiDoubleQuoteSwitchesTheRuleOff)
{
    check_rows(kGroupE);
}

// Every foot mark of the text is checked, not only the first: a safe mark does not protect a later one, and the lone-x exemption
// needs the x to be the last non-blank character of the whole text.
TEST(CondElev02_ClosingInchMark, EveryFootMarkOfTheTextIsChecked)
{
    check_rows(kGroupF);
}

// U+2032 (prime) is a foot mark like the apostrophe: the lone x and a closing inch mark are accepted, any other non-blank character is refused (Windows 0a24294).
TEST(CondElev02_ClosingInchMark, ThePrimeMarkBehavesLikeTheApostrophe)
{
    check_rows(kGroupG);
}

// Texts without a foot mark (metric forms, inches only: the rule never fires) and texts the shared parser refuses on its own stay
// as they were, with the same message.
TEST(CondElev02_ClosingInchMark, TextsWithoutAFootMarkAndTextsTheSharedParserRefusesAreUnchanged)
{
    check_rows(kGroupH);
}

// More of the x family and the decimal and zero feet: refused (a second letter, a digit, a dot, a dash, an inch fraction without
// the closing mark after the x; a decimal or comma feet figure is no elevation; a zero feet figure is still feet).
TEST(CondElev02_ClosingInchMark, TheXFamilyAndTheDecimalAndZeroRowsOfTheOracle)
{
    check_rows(kGroupI);
    check_rows(kGroupJ);
}

// Marks that start no readable feet, other apostrophe-like characters and the shapes the shared parser refuses anyway: all stay
// refused, and the free function leaves the foreign marks alone (U+2019, U+02BC and U+00B4 are NOT foot marks, only U+0027 and
// U+2032 are).
TEST(CondElev02_ClosingInchMark, ForeignMarksAndMarksWithoutReadableFeetStayRefused)
{
    check_rows(kGroupK);

    // A feet run beyond the float range, with a lone x (or X) after the mark. The closing-inch-mark rule does NOT refuse these (no
    // digit or letter follows the mark but the final x, so the rule column is F; the CHECK_FALSE below pins that), so only the shared
    // parser's overflow handling can: Windows 49a2d59 elevation_text_inches raises OverflowError ("int too large to convert to
    // float", elevation_text.py :101: float(feet_val * 12) of a 405-digit feet run) and the dialog (edit_condition_dialog.py :1440-1443)
    // catches only ValueError, so there it is an uncaught exception in the Qt slot; iOS refuses it with the normal message. (A
    // run of digits followed by a digit or letter, "...'6", is refused by the rule whatever the parser says: it would pin nothing.)
    // The same text with 404 zeros and the ASCII mark is in test_condition_elevation_dialog.cpp (CondElev01_Validation).
    const std::string overflow_texts[] = {
        "1" + std::string(404, '0') + "\xE2\x80\xB2" "x", // U+2032 and a lone x
        "-1" + std::string(404, '0') + "'X  ",             // negative, an upper case X and blanks after it (stripped first)
    };
    for (const std::string &text : overflow_texts)
    {
        SCOPED_CONTEXT("overflow text=[" + shown(text.substr(0, 4)) + "...(" + std::to_string(text.size()) + " bytes)..." +
                       shown(text.substr(text.size() - 6)) + "]");
        CHECK_FALSE(ost_domain::elevation_text_needs_closing_inch_mark(ost_import::text_rules::py_strip(text)));
        expect_refused(text, true);
        expect_refused(text, false);
    }
}

// The edges of the old ASCII class [0-9A-Za-z]: the first and last digit and letter of each range are refused, and so is an upper case
// letter that is not a lone final X ("5'Xy", "5'X hello"); the characters just outside the ranges ('/' ':' '@' '[' '`' '{') are
// refused as well since Windows 0a24294 (any non-blank character counts; the shared parser refused them anyway, so only the rule
// column of these rows changed). (The test name is kept: PARITY_DIVERGENCES.json cites it.)
TEST(CondElev02_ClosingInchMark, TheEdgesOfTheAsciiClassAndAnUpperCaseLetterAfterTheMark)
{
    check_rows(kGroupL);
}

// ---------------------------------------------------------------------------------------------------------------------
// Port of tests/presentation/dialogs/test_edit_condition_dialog.py::EditConditionElevationRuleTests (Windows 0a24294, :1774-1873).
// Each Windows text runs in a metric and an imperial bid (the Windows class runs one bid); an accepted text must also reload (as the
// name suffix) to the number the dialog saved.
// ---------------------------------------------------------------------------------------------------------------------

TEST(CondElev03_WindowsElevationRuleTests, AStrayQuoteDoesNotHideTextAfterTheFootMark)
{
    for (const char *text : {"5'x\"", "5'6x\"", "5'abc\"", "5'x1/2\"", "10'6.3\"", "6\"5'7"})
    {
        expect_refused(text, true);
        expect_refused(text, false);
    }
}

TEST(CondElev03_WindowsElevationRuleTests, AnyNonBlankCharacterAfterTheFootMarkIsRefused)
{
    for (const char *text : {"5'\xC3\xA9",         // e acute
                             "5'_",
                             "5'\xD9\xA3",         // U+0663 Arabic-Indic digit three
                             "5'\xEF\xBC\x96",     // U+FF16 fullwidth six
                             "5'\xC5\xBF",         // U+017F long s
                             "5'\xE2\x84\xAA",     // U+212A Kelvin sign
                             "5'\xE4\xB8\x80",     // U+4E00
                             "5'_'x"})
    {
        expect_refused(text, true);
        expect_refused(text, false);
    }
}

TEST(CondElev03_WindowsElevationRuleTests, DoublePrimeAndRightDoubleQuoteAreInchMarks)
{
    for (const char *text : {"5'6\xE2\x80\xB3", "5'6\xE2\x80\x9D", "5'6\"", "5' 6\xE2\x80\xB3"})
        expect_accepted(text, 66.0);
    // the other typographic marks stay what they were: U+201C is no inch mark, and a mark after the x is not the invisible kind
    expect_refused("5'6\xE2\x80\x9C", true);
    expect_refused("5'x\xE2\x80\xB3", false);
    expect_refused("5'x\xE2\x80\x9D", false);
}

TEST(CondElev03_WindowsElevationRuleTests, ALoneXMayBeFollowedByInvisibleFormatCharacters)
{
    expect_accepted("5'x", 60.0);
    expect_accepted("5'X", 60.0);
    expect_accepted("-4'x", -48.0);
    for (const char *invisible : {"\xE2\x80\x8B", "\xEF\xBB\xBF", "\xE1\xA0\x8E", "\xE2\x80\x8C", "\xE2\x80\x8D", "\xE2\x81\xA0"})
        expect_accepted(std::string("5'x") + invisible, 60.0); // U+200B U+FEFF U+180E U+200C U+200D U+2060
}

TEST(CondElev03_WindowsElevationRuleTests, AValueBeyondTheFloatRangeIsRefusedWithTheNormalMessage)
{
    for (const std::string &text : {std::string(400, '9') + "'x", std::string(400, '9') + "'6\"", "1" + std::string(400, '0') + "\"",
                                    "1" + std::string(400, '0') + " m", "-1" + std::string(400, '0') + " cm"})
    {
        SCOPED_CONTEXT("length=" + std::to_string(text.size()));
        expect_refused(text, true);
        expect_refused(text, false);
    }
}

TEST(CondElev03_WindowsElevationRuleTests, ValidFeetAndInchesAreStillAccepted)
{
    expect_accepted("5' 6\"", 66.0);
    expect_accepted("10'6 1/2\"", 126.5);
    expect_accepted("-2' 3\"", -27.0);
    expect_accepted("1/2\"", 0.5);
    // a real feet-and-inches value switches the rule off whatever follows it
    expect_accepted("5'6\"x", 66.0);
    expect_accepted("5'6\"abc", 66.0);
    expect_accepted("5'6\xE2\x80\xB3\"", 66.0);
}

// The free function on the Windows lists (the rule alone, text as written): true = refuse.
TEST(CondElev03_WindowsElevationRuleTests, TheFreeFunctionOnTheWindowsLists)
{
    for (const char *text : {"5'x\"", "5'6x\"", "5'abc\"", "5'x1/2\"", "10'6.3\"", "6\"5'7", "5'\xC3\xA9", "5'_", "5'\xD9\xA3",
                             "5'\xEF\xBC\x96", "5'\xC5\xBF", "5'\xE2\x84\xAA", "5'\xE4\xB8\x80", "5'_'x", "5'x\xE2\x80\xB3", "5'6",
                             "5'6x", "10'6", "5'abc", "-2'3"})
    {
        SCOPED_CONTEXT("rule text=[" + shown(text) + "]");
        CHECK(ost_domain::elevation_text_needs_closing_inch_mark(text));
    }
    for (const char *text : {"5'6\xE2\x80\xB3", "5'6\xE2\x80\x9D", "5'6\"", "5' 6\xE2\x80\xB3", "5'x", "5'X", "-4'x",
                             "5'x\xE2\x80\x8B", "5'x\xEF\xBB\xBF", "5'x\xE1\xA0\x8E", "5'x\xE2\x80\x8C", "5'x\xE2\x80\x8D",
                             "5'x\xE2\x81\xA0", "5' 6\"", "10'6 1/2\"", "-2' 3\"", "1/2\""})
    {
        SCOPED_CONTEXT("rule text=[" + shown(text) + "]");
        CHECK_FALSE(ost_domain::elevation_text_needs_closing_inch_mark(text));
    }
}
