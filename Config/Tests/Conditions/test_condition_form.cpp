// The field rules of the Edit Condition dialog (ConditionFormRules.hpp): COND-REF-01 (condition number is validated, not
// coerced), DLG-FINITE-01 (the condition number is capped at 2147483647 with a message of its own, and the save-failed alert
// carries the store's reason), COND-VAL-01 (Display Size and plain numbers), DLG-RISE-01 (Area rise / run are absolute), UI-MSG-01
// (alert wording), COND-ELEV-01 (the elevation text is kept verbatim and validated with the loader's parser, elevation_text_inches; the
// saved z / is_top come from the name: test_condition_elevation_dialog.cpp, test_condition_elevation.cpp), COND-TRIM-01 (Trim
// and Curved Segment exclude each other), COL-PICK-01 (the colour writes ColorFill only).
//
// Windows oracle: ost_visualizer/presentation/dialogs/edit_condition_dialog.py (_validate_and_build_dto, _parse_dimension_field,
// _on_color_changed, _sync_linear_trim_curved_state) with tests/presentation/dialogs/test_edit_condition_dialog.py. Python
// values (int(), float(), display_to_inches) were computed by running the Windows code.
#include "TestHarness.hpp"

#include "Domain/ConditionChanges.hpp"
#include "Domain/ConditionFormRules.hpp"
#include "Domain/ConditionStyleChange.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <string>

using namespace ost_domain;

namespace
{
    constexpr const char *kRefNoMessage = "Condition number must be a positive whole number.";
    constexpr const char *kRangeMessage = "Condition number must be between 1 and 2147483647.";

    // text -> the alert it is refused with, or "" when it is accepted
    std::string ref_no_issue(const std::string &text, int &value)
    {
        const auto issue = parse_condition_number(text, value);
        if (!issue)
            return "";
        CHECK_STR_EQ(issue->title, "Invalid Value");
        return issue->message;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-REF-01
// ---------------------------------------------------------------------------------------------------------------------

// Port of tests/presentation/dialogs/test_edit_condition_dialog.py::test_edit_condition_rejects_invalid_condition_numbers
// ("", "not-a-number", "1.5", "0", "-1") plus the cases of the item's test plan ("12abc"); iOS coerced each one to 1 (or 12).
TEST(CondRef01_ConditionNumber, InvalidTextsAreRefusedNotCoerced)
{
    const char *texts[] = {"", "not-a-number", "1.5", "0", "-1", "12abc", "   ", "0x10", "1e3", "- 1", "+0", "1__0", "_1", "1_"};
    for (const char *text : texts)
    {
        SCOPED_CONTEXT(std::string("text '") + text + "'");
        int value = 4711;
        CHECK_STR_EQ(ref_no_issue(text, value), kRefNoMessage);
        CHECK_EQ(value, 4711); // untouched on a refusal
    }
}

// Port of ...::test_edit_condition_accepts_valid_changed_condition_number (' 8 ' -> 8) and the Python int() grammar
TEST(CondRef01_ConditionNumber, ValidTextsAreAcceptedLikePythonInt)
{
    struct Row
    {
        const char *text;
        int expected;
    };
    const Row rows[] = {
        {" 8 ", 8},     {"7", 7},   {"+7", 7},           {"1_0", 10},        {"008", 8},
        {"\xD9\xA8", 8},                                          // ARABIC-INDIC DIGIT EIGHT: int('٨') == 8
        {"\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x99", 9},               // FULLWIDTH 009
        {"\xC2\xA0" "5" "\xC2\xA0", 5},                           // no-break spaces are whitespace for str.strip()
        {"2147483647", 2147483647},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(std::string("text '") + row.text + "'");
        int value = 0;
        CHECK_STR_EQ(ref_no_issue(row.text, value), "");
        CHECK_EQ(value, row.expected);
    }
}

// str.strip() removes the C0 separators U+001C..U+001F, so int(text.strip()) reads them around a number (computed with the Windows
// code: int("\x1f100".strip()) == 100, int("\x1c 7 \x1d".strip()) == 7); an inner one is a ValueError like any other character
TEST(CondRef01_ConditionNumber, TheC0SeparatorsAroundANumberAreStrippedLikePythonStrip)
{
    int value = 0;
    CHECK_STR_EQ(ref_no_issue("\x1f" "100", value), "");
    CHECK_EQ(value, 100);
    CHECK_STR_EQ(ref_no_issue("\x1c 7 \x1d", value), "");
    CHECK_EQ(value, 7);
    value = 4711;
    CHECK_STR_EQ(ref_no_issue("1\x1f" "2", value), kRefNoMessage);
    CHECK_EQ(value, 4711);
}

// A number above the highest condition number cannot be stored (an Access Long, a Condition's int): refused with the RANGE message,
// not the 'positive whole number' one (DLG-FINITE-01, Windows b9902e6). Was ANumberBeyondTheStorableRangeIsRefused, which pinned
// the old message for 2147483648 and 99999999999999999999 and the wrong premise that Windows accepts up to 64 bits.
// Port of tests/presentation/dialogs/test_edit_condition_dialog.py::test_edit_condition_caps_the_condition_number_at_the_access_limit
// ('2147483648' and ' 99999999999999999999 '); the other rows are iOS cases of the same refusal (a number that parses as 64 bit, a sign,
// underscores, Unicode digits, leading zeros, the largest and the first too large 64 bit number).
TEST(CondRef01_ConditionNumber, ANumberAboveTheHighestConditionNumberIsRefusedWithTheRangeMessage)
{
    const char *texts[] = {"2147483648",
                           " 99999999999999999999 ",
                           "99999999999",
                           "+2147483648",
                           "2_147_483_648",
                           "0002147483648",
                           "9223372036854775807",
                           "9223372036854775808",
                           "\xD9\xA2\xD9\xA1\xD9\xA4\xD9\xA7\xD9\xA4\xD9\xA8\xD9\xA3\xD9\xA6\xD9\xA4\xD9\xA8"}; // ARABIC-INDIC 2147483648
    for (const char *text : texts)
    {
        SCOPED_CONTEXT(std::string("text '") + text + "'");
        int value = 4711;
        CHECK_STR_EQ(ref_no_issue(text, value), kRangeMessage);
        CHECK_EQ(value, 4711); // untouched on a refusal
    }
}

// The order of Windows' checks: text that is not a whole number and a value below 1 come first, so a NEGATIVE number, however
// large, keeps the 'positive whole number' message; only a number above the highest condition number gets the range message.
// (-99999999999999999999 and -9223372036854775809 do not fit in 64 bits: the parser reports an overflow without a sign.)
TEST(DlgFinite01_ConditionNumber, ANegativeNumberKeepsThePositiveWholeNumberMessageHoweverLarge)
{
    const char *texts[] = {"-99999999999999999999", "  -99999999999999999999  ", "-9223372036854775809", "-9223372036854775808",
                           "-2147483649",           "-2147483648",               "-1",                   "0",
                           "",                      "   ",                      "abc",                  "1.5",
                           "-",                     "+",                         "2147483648x",          "- 2147483648"};
    for (const char *text : texts)
    {
        SCOPED_CONTEXT(std::string("text '") + text + "'");
        int value = 4711;
        CHECK_STR_EQ(ref_no_issue(text, value), kRefNoMessage);
        CHECK_EQ(value, 4711);
    }
}

// Port of ...::test_edit_condition_caps_the_condition_number_at_the_access_limit (the third part): the highest number itself is
// accepted and the change set of a condition with number 7 is exactly {ref_no: 2147483647}; so is the number just below it.
TEST(DlgFinite01_ConditionNumber, TheHighestConditionNumberIsAcceptedAndIsTheOnlyChange)
{
    Condition stored;
    stored.uid = "1";
    stored.name = "Condition 1";
    stored.condition_type = ConditionType::Linear;
    stored.ref_no = 7;
    stored.is_top = true;
    stored.uom1 = stored.uom2 = stored.uom3 = -1;

    for (const char *text : {"2147483647", " 2147483647 ", "2_147_483_647"})
    {
        SCOPED_CONTEXT(std::string("text '") + text + "'");
        Condition form = stored;
        int ref_no = 0;
        CHECK_FALSE(parse_condition_number(text, ref_no).has_value());
        CHECK_EQ(ref_no, 2147483647);
        form.ref_no = ref_no;
        const ConditionChanges changes = collect_condition_changes(stored, condition_to_save(stored, form, false));
        CHECK(changes.has(ConditionField::RefNo));
        CHECK_EQ(changes.fields.count(), 1u);
        CHECK_EQ(changes.values.ref_no, 2147483647);
    }
    int below = 0;
    CHECK_FALSE(parse_condition_number("2147483646", below).has_value());
    CHECK_EQ(below, 2147483646);
}

// update_condition_dto.py MAX_CONDITION_NUMBER and the two texts of the field, character for character.
TEST(DlgFinite01_ConditionNumber, TheLimitAndTheTwoMessagesAreTheWindowsOnes)
{
    CHECK_EQ(kMaxConditionNumber, 2147483647);
    CHECK_EQ(kMaxConditionNumber, std::numeric_limits<int>::max());
    CHECK_STR_EQ(kConditionNumberPositiveMessage, "Condition number must be a positive whole number.");
    CHECK_STR_EQ(kConditionNumberRangeMessage, "Condition number must be between 1 and 2147483647.");
    CHECK_STR_EQ(kConditionNumberRangeMessage,
                 "Condition number must be between 1 and " + std::to_string(kMaxConditionNumber) + ".");
}

// With condition ref 7 and the text ' 8 ' the change set is exactly {ref_no: 8}
// (Port of ...::test_edit_condition_accepts_valid_changed_condition_number: dto.get_changes() == {"ref_no": 8})
TEST(CondRef01_ConditionNumber, TheChangeSetOfARenumberIsExactlyTheRefNo)
{
    Condition stored;
    stored.uid = "1";
    stored.name = "Condition 1";
    stored.condition_type = ConditionType::Linear;
    stored.ref_no = 7;
    stored.is_top = true;
    stored.uom1 = stored.uom2 = stored.uom3 = -1;

    Condition form = stored;
    int ref_no = 0;
    CHECK_FALSE(parse_condition_number(" 8 ", ref_no).has_value());
    form.ref_no = ref_no;
    const ConditionChanges changes = collect_condition_changes(stored, condition_to_save(stored, form, false));
    CHECK(changes.has(ConditionField::RefNo));
    CHECK_EQ(changes.fields.count(), 1u);
    CHECK_EQ(changes.values.ref_no, 8);
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-VAL-01, UI-MSG-01
// ---------------------------------------------------------------------------------------------------------------------

TEST(CondVal01_DisplaySize, AnEmptyFieldIsTheDefault100)
{
    double value = 0.0;
    CHECK_FALSE(parse_display_size("", value).has_value());
    CHECK_EQ(value, 100.0);
    value = 0.0;
    CHECK_FALSE(parse_display_size("   ", value).has_value());
    CHECK_EQ(value, 100.0);
}

// The test plan: with the stored size 150 and the field emptied the change set is {display_size: 100.0}
TEST(CondVal01_DisplaySize, AnEmptiedFieldChangesTheStoredSizeTo100)
{
    Condition stored;
    stored.uid = "1";
    stored.name = "Pier";
    stored.condition_type = ConditionType::Count;
    stored.ref_no = 1;
    stored.uom1 = stored.uom2 = stored.uom3 = -1; // the Windows fixtures: a slot without a quantity has the unit -1
    stored.display_size = 150.0;
    Condition form = stored;
    CHECK_FALSE(parse_display_size("", form.display_size).has_value());
    const ConditionChanges changes = collect_condition_changes(stored, condition_to_save(stored, form, false));
    CHECK_EQ(changes.fields.count(), 1u);
    CHECK(changes.has(ConditionField::DisplaySize));
    CHECK_EQ(changes.values.display_size, 100.0);
}

TEST(CondVal01_DisplaySize, TheBoundsAre10And500Inclusive)
{
    struct Row
    {
        const char *text;
        bool accepted;
        double value;
    };
    const Row rows[] = {{"5", false, 0},    {"501", false, 0},   {"10", true, 10.0},   {"500", true, 500.0},
                        {"9.999999999999999", false, 0},          {"500.0000000000001", false, 0},
                        {" 50 ", true, 50.0}, {"1_0", true, 10.0}, {"250.5", true, 250.5}};
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(std::string("text '") + row.text + "'");
        double value = -1.0;
        const auto issue = parse_display_size(row.text, value);
        if (row.accepted)
        {
            CHECK_FALSE(issue.has_value());
            CHECK_EQ(value, row.value);
        }
        else
        {
            REQUIRE(issue.has_value());
            CHECK_STR_EQ(issue->title, "Validation Error");
            CHECK_STR_EQ(issue->message, "Display Size must be between 10% and 500%.");
            CHECK_EQ(value, -1.0);
        }
    }
}

// Port of ...::test_edit_condition_rejects_non_finite_numeric_values ("display size": '"nan" is not a valid number for Display
// Size.') and the other texts the same message covers; NSScanner let 'nan' slip past the range comparison on iOS.
TEST(CondVal01_DisplaySize, NotANumberAndNonFiniteTextAreRefusedWithTheNumberMessage)
{
    const char *texts[] = {"nan", "inf", "-inf", "abc", "1e400", "5%", "1,5", "NaN", "Infinity"};
    for (const char *text : texts)
    {
        SCOPED_CONTEXT(std::string("text '") + text + "'");
        double value = -1.0;
        const auto issue = parse_display_size(text, value);
        REQUIRE(issue.has_value());
        CHECK_STR_EQ(issue->title, "Invalid Value");
        CHECK_STR_EQ(issue->message, std::string("\"") + text + "\" is not a valid number for Display Size.");
        CHECK_EQ(value, -1.0);
    }
}

// Port of ...::test_edit_condition_rejects_non_finite_numeric_values ("linear rise": 'inf' -> 'Rise must be a finite number.',
// "area run": '-inf' -> 'Run must be a finite number.') and the not-a-number texts ('Rise must be a number.')
TEST(CondVal01_RiseRun, NonFiniteAndNonNumericTextHaveTheirWindowsMessages)
{
    struct Row
    {
        const char *text;
        const char *label;
        bool absolute;
        const char *message;
    };
    const Row rows[] = {
        {"inf", "Rise", false, "Rise must be a finite number."},  {"-inf", "Run", true, "Run must be a finite number."},
        {"nan", "Rise", false, "Rise must be a finite number."},  {"1e400", "Run", false, "Run must be a finite number."},
        {"abc", "Rise", false, "Rise must be a number."},         {"1,5", "Run", true, "Run must be a number."},
        {"3/4", "Rise", true, "Rise must be a number."},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(std::string(row.label) + " '" + row.text + "'");
        double value = -1.0;
        const auto issue = parse_slope_value(row.text, row.label, row.absolute, value);
        REQUIRE(issue.has_value());
        CHECK_STR_EQ(issue->title, "Invalid Value");
        CHECK_STR_EQ(issue->message, row.message);
        CHECK_EQ(value, -1.0);
    }
}

// The Rise / Run field is not stripped by the dialog: float("\x1f100") is a ValueError (float() skips only ASCII white space and the
// Unicode spaces, not the C0 separators that str.strip() removes), float(" 100 ") and float("\xc2\xa0" "100") are 100.0
TEST(CondVal01_RiseRun, OnlyFloatWhiteSpaceSurroundsANumber)
{
    double value = -1.0;
    const auto issue = parse_slope_value("\x1f" "100", "Rise", false, value);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->message, "Rise must be a number.");
    CHECK_EQ(value, -1.0);
    CHECK_FALSE(parse_slope_value(" 100 ", "Rise", false, value).has_value());
    CHECK_EQ(value, 100.0);
    CHECK_FALSE(parse_slope_value("\xC2\xA0" "100", "Rise", false, value).has_value());
    CHECK_EQ(value, 100.0);
}

// DLG-FINITE-01: only the EMPTY field is blank (float(text or "0")); a field of white space is not (next test)
TEST(CondVal01_RiseRun, AnEmptyFieldIsZeroAndANumberIsTheNumber)
{
    double value = -1.0;
    CHECK_FALSE(parse_slope_value("", "Rise", false, value).has_value());
    CHECK_EQ(value, 0.0);
    value = -1.0;
    CHECK_FALSE(parse_slope_value("", "Run", true, value).has_value());
    CHECK_EQ(value, 0.0);
    CHECK_FALSE(parse_slope_value("3.5", "Rise", false, value).has_value());
    CHECK_EQ(value, 3.5);
    CHECK_FALSE(parse_slope_value(" 12 ", "Run", false, value).has_value());
    CHECK_EQ(value, 12.0);
}

// ---------------------------------------------------------------------------------------------------------------------
// DLG-FINITE-01 (Rise / Run half; decided 2026-11-19: follow Windows)
// ---------------------------------------------------------------------------------------------------------------------

// edit_condition_dialog.py :1578 / :1590 (Linear) and :1608 / :1620 (Area, abs()): float(self._rise_edit.text() or "0"). The field
// is not stripped and only an EMPTY text is blank, so a text of white space reaches float(). Python 3.11.4 (python -B, builtins):
// float(t) is a ValueError for ' ', '   ', '\t', '\xa0', '\u2003', '\u3000', ' \n', '\r', '\x0b', '\x0c', '\x1c', '\x1f',
// '\x85' and '\u2028' (str.strip() removes every one of them, float() has no number left), shown as "Rise must be a number." /
// "Run must be a number." (show_warning "Invalid Value"). Core read such a field as blank (0) before.
TEST(DlgFinite01_RiseRun, AWhiteSpaceOnlyFieldIsNotANumberLikeWindows)
{
    const char *texts[] = {
        " ",                  // one space
        "   ",                // several spaces
        "\t",                 // TAB
        "\xC2\xA0",           // U+00A0 no-break space
        "\xE2\x80\x83",       // U+2003 em space
        "\xE3\x80\x80",       // U+3000 ideographic space
        " \n",                // space and line feed
        "\r",                 // carriage return
        "\x0b",               // vertical tab
        "\x0c",               // form feed
        "\x1c",               // U+001C, a C0 separator: str.strip() removes it, float() does not skip it
        "\x1f",               // U+001F
        "\xC2\x85",           // U+0085 next line
        "\xE2\x80\xA8",       // U+2028 line separator
        " \xC2\xA0\t",        // a mix
    };
    const char *labels[] = {"Rise", "Run"};
    const bool absolutes[] = {false, true}; // Linear keeps the sign, Area takes abs()
    for (const char *label : labels)
    {
        for (const bool absolute : absolutes)
        {
            for (size_t index = 0; index < sizeof texts / sizeof texts[0]; ++index)
            {
                const char *text = texts[index];
                SCOPED_CONTEXT(std::string(label) + (absolute ? " (Area)" : " (Linear)") + " text #" + std::to_string(index));
                double value = -1.0;
                const auto issue = parse_slope_value(text, label, absolute, value);
                CHECK(issue.has_value()); // not REQUIRE: every wrong row of the table is reported
                CHECK_EQ(value, -1.0);    // refused: the output is untouched
                if (!issue)
                    continue;
                CHECK_STR_EQ(issue->title, "Invalid Value");
                CHECK_STR_EQ(issue->message, std::string(label) + " must be a number.");
            }
        }
    }
}

// The wording of the whitespace-only refusal is the NotANumber text, not the finite-number text, and the neighbouring cases stay as
// they were: float() skips white space around a number (' 1.5 ' is 1.5, '\t7\n' is 7.0, U+2003 around '-2.25'), accepts digit
// underscores ('1_0' is 10.0) and reads nan / inf as non-finite even with spaces around; the C0 separators around a number are
// still a ValueError ('\x1c1\x1c'). Python 3.11.4 results.
TEST(DlgFinite01_RiseRun, WhiteSpaceAroundANumberAndTheOtherRefusalsStayAsTheyWere)
{
    double value = -1.0;
    CHECK_FALSE(parse_slope_value(" 1.5 ", "Rise", false, value).has_value());
    CHECK_EQ(value, 1.5);
    CHECK_FALSE(parse_slope_value("\t7\n", "Run", false, value).has_value());
    CHECK_EQ(value, 7.0);
    CHECK_FALSE(parse_slope_value("\xE2\x80\x83-2.25\xE2\x80\x83", "Rise", false, value).has_value());
    CHECK_EQ(value, -2.25);
    CHECK_FALSE(parse_slope_value("\xE2\x80\x83-2.25\xE2\x80\x83", "Rise", true, value).has_value());
    CHECK_EQ(value, 2.25); // the Area style: abs()
    CHECK_FALSE(parse_slope_value(" -3 ", "Run", true, value).has_value());
    CHECK_EQ(value, 3.0);
    CHECK_FALSE(parse_slope_value("1_0", "Rise", false, value).has_value());
    CHECK_EQ(value, 10.0);

    value = -1.0;
    auto issue = parse_slope_value(" nan ", "Rise", false, value);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->message, "Rise must be a finite number.");
    issue = parse_slope_value(" inf ", "Run", true, value);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->message, "Run must be a finite number.");
    issue = parse_slope_value("\x1c" "1" "\x1c", "Rise", false, value);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->message, "Rise must be a number.");
    issue = parse_slope_value("7 7", "Run", false, value);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->message, "Run must be a number.");
    CHECK_EQ(value, -1.0);
}

// ---------------------------------------------------------------------------------------------------------------------
// DLG-RISE-01
// ---------------------------------------------------------------------------------------------------------------------

// Test plan: Area condition with rise text '-3' run '12' -> saved rise 3.0 run 12.0; Linear condition with rise '-3' -> -3.0
// (edit_condition_dialog.py :1601-1630, the branch of every style but Linear: abs(float(...)) at :1608 / :1620).
TEST(DlgRise01_AreaSlope, AreaRiseAndRunAreAbsoluteLinearKeepsTheSign)
{
    double area_rise = 0.0, area_run = 0.0, linear_rise = 0.0;
    CHECK_FALSE(parse_slope_value("-3", "Rise", true, area_rise).has_value());
    CHECK_FALSE(parse_slope_value("12", "Run", true, area_run).has_value());
    CHECK_FALSE(parse_slope_value("-3", "Rise", false, linear_rise).has_value());
    CHECK_EQ(area_rise, 3.0);
    CHECK_EQ(area_run, 12.0);
    CHECK_EQ(linear_rise, -3.0);
    // -0.0 is a plain zero
    double zero = 1.0;
    CHECK_FALSE(parse_slope_value("-0", "Rise", true, zero).has_value());
    CHECK_EQ(zero, 0.0);
    CHECK_FALSE(std::signbit(zero));
}

// ---------------------------------------------------------------------------------------------------------------------
// Dimension / elevation / name texts (UI-MSG-01, COND-ELEV-01)
// ---------------------------------------------------------------------------------------------------------------------

// Port of ...::test_edit_condition_invalid_spacing_blocks_the_entire_update and the "spacing" case of
// test_edit_condition_rejects_non_finite_numeric_values; the alert names the field ('... for Height.'), not '... a valid Height.'
TEST(Msg01_Dimension, TheAlertNamesTheDimensionField)
{
    double inches = -1.0;
    auto issue = parse_dimension_value("not-a-dimension", "Spacing", false, inches);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->title, "Invalid Value");
    CHECK_STR_EQ(issue->message, "\"not-a-dimension\" is not a valid dimension for Spacing.");
    issue = parse_dimension_value("nan", "Spacing", false, inches);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->message, "\"nan\" is not a valid dimension for Spacing.");
    issue = parse_dimension_value("  inf ", "Height", false, inches);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->message, "\"inf\" is not a valid dimension for Height."); // the stripped text
    CHECK_EQ(inches, -1.0);
}

TEST(Msg01_Dimension, BlankIsZeroAndTextIsTheWindowsParse)
{
    double inches = -1.0;
    CHECK_FALSE(parse_dimension_value("", "Height", false, inches).has_value());
    CHECK_EQ(inches, 0.0);
    CHECK_FALSE(parse_dimension_value("5' 6\"", "Height", false, inches).has_value());
    CHECK_EQ(inches, 66.0); // Windows display_to_inches("5' 6\"") == 66.0
    // metric: millimetres (display_to_inches("3200", True) == 125.98425196850394, "25.4 mm" -> 1.0)
    CHECK_FALSE(parse_dimension_value("3200", "Height", true, inches).has_value());
    CHECK_EQ(inches, 125.98425196850394);
    CHECK_FALSE(parse_dimension_value("25.4 mm", "Height", true, inches).has_value());
    CHECK_EQ(inches, 1.0);
    // the imperial grammar does not know "mm"
    CHECK(parse_dimension_value("25.4 mm", "Height", false, inches).has_value());
}

// Port of ...::test_edit_condition_rejects_non_finite_numeric_values ("elevation": 'nan' is refused, with the imperial hint since
// b9902e6) and of the COND-ELEV-01 rule of b9902e6: the text is read with the loader's parser (elevation_text_inches), not with
// display_to_inches. So decimal inches are no elevation (display_to_inches gave 126.3), a bare metric number is refused (it was read as
// millimetres) and metres are accepted; empty is 0.0, never validated. "10'6" is feet only, 120.0, for the LOADER (display_to_inches
// gave 126.0); since Windows 49a2d59 (edit_condition_dialog.py :72, :1444-1445) the DIALOG refuses
// "10'6" (a digit after the foot mark and no closing inch mark: it would be saved as written and reloaded as 120.0), while "10'6\""
// is 126.0. The whole table, both modes, Top / Bottom and the reload: test_condition_elevation_dialog.cpp; the rule:
// test_condition_elevation_foot_mark.cpp.
TEST(CondElev01_Elevation, TheTextGivesZAndNanIsRefused)
{
    const std::string imperial_hint = "Use feet and inches with unit marks, for example 10' 6\".";
    const std::string metric_hint = "Use metres or centimetres, for example \"3.2 m\" or \"320 cm\".";

    double z = -1.0;
    const auto closing = parse_elevation_z("10'6", false, z);
    REQUIRE(closing.has_value());
    CHECK_STR_EQ(closing->title, "Invalid Value");
    CHECK_STR_EQ(closing->message, "\"10'6\" is not a valid elevation. " + imperial_hint);
    CHECK_EQ(z, -1.0); // a refusal leaves the output untouched
    CHECK_FALSE(parse_elevation_z("10'6\"", false, z).has_value());
    CHECK_EQ(z, 126.0); // read with the loader's parser, not display_to_inches: the closing inch mark makes it feet and inches
    z = -1.0;
    const auto decimal_inches = parse_elevation_z("10' 6.3\"", false, z);
    REQUIRE(decimal_inches.has_value());
    CHECK_STR_EQ(decimal_inches->title, "Invalid Value");
    CHECK_STR_EQ(decimal_inches->message, "\"10' 6.3\"\" is not a valid elevation. " + imperial_hint); // the text 10' 6.3" in quotes
    CHECK_EQ(z, -1.0);
    CHECK_FALSE(parse_elevation_z("", false, z).has_value());
    CHECK_EQ(z, 0.0);
    z = -1.0;
    const auto issue = parse_elevation_z("nan", false, z);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->title, "Invalid Value");
    CHECK_STR_EQ(issue->message, "\"nan\" is not a valid elevation. " + imperial_hint);
    CHECK_EQ(z, -1.0);
    // a metric bid: a bare number is refused with the metric hint (it was millimetres), metres are read
    const auto bare = parse_elevation_z("3200", true, z);
    REQUIRE(bare.has_value());
    CHECK_STR_EQ(bare->message, "\"3200\" is not a valid elevation. " + metric_hint);
    CHECK_EQ(z, -1.0);
    CHECK_FALSE(parse_elevation_z("3.2 m", true, z).has_value());
    CHECK_NEAR(z, 320.0 / 2.54, 1e-9);
}

TEST(Msg01_Name, AnEmptyNameIsAValidationError)
{
    std::string name = "keep";
    for (const char *text : {"", "   ", "\t \xC2\xA0"})
    {
        const auto issue = parse_condition_name(text, name);
        REQUIRE(issue.has_value());
        CHECK_STR_EQ(issue->title, "Validation Error");
        CHECK_STR_EQ(issue->message, "Condition name cannot be empty.");
    }
    CHECK_STR_EQ(name, "keep");
    CHECK_FALSE(parse_condition_name("  Wall A \xC2\xA0", name).has_value());
    CHECK_STR_EQ(name, "Wall A");
}

TEST(Msg01_SaveFailed, TheWindowsWording)
{
    const FormIssue issue = condition_save_failed_issue();
    CHECK_STR_EQ(issue.title, "Save Failed");
    CHECK_STR_EQ(issue.message, "Failed to save condition to database.");
}

// DLG-FINITE-01: the dialog shows result.error under "Save Failed" (edit_condition_dialog.py :1810-1817); the use case's error is the
// overflow text for a ConditionNumberOverflowError and the generic text for every other failure, so an empty detail is the generic one.
TEST(DlgFinite01_SaveFailed, ADetailReplacesTheGenericTextUnderTheSameTitle)
{
    const FormIssue detailed = condition_save_failed_issue(condition_number_overflow_message(2));
    CHECK_STR_EQ(detailed.title, "Save Failed");
    CHECK_STR_EQ(detailed.message,
                 "Condition number 2 cannot be used: the conditions numbered 2 and up would move up by one, and one of them is "
                 "already 2147483647, the highest condition number.");

    const FormIssue free_text = condition_save_failed_issue("Some other reason.");
    CHECK_STR_EQ(free_text.title, "Save Failed");
    CHECK_STR_EQ(free_text.message, "Some other reason.");

    const FormIssue empty = condition_save_failed_issue(std::string());
    CHECK_STR_EQ(empty.title, "Save Failed");
    CHECK_STR_EQ(empty.message, "Failed to save condition to database.");
}

// condition_operations.py :650-655, with the number of the renumber filled in twice and the limit once (Windows tests: N = 2 and 3).
TEST(DlgFinite01_SaveFailed, TheOverflowMessageIsTheWindowsTextForTheNumberTyped)
{
    CHECK_STR_EQ(condition_number_overflow_message(2),
                 "Condition number 2 cannot be used: the conditions numbered 2 and up would move up by one, and one of them is "
                 "already 2147483647, the highest condition number.");
    CHECK_STR_EQ(condition_number_overflow_message(3),
                 "Condition number 3 cannot be used: the conditions numbered 3 and up would move up by one, and one of them is "
                 "already 2147483647, the highest condition number.");
    CHECK_STR_EQ(condition_number_overflow_message(2147483647),
                 "Condition number 2147483647 cannot be used: the conditions numbered 2147483647 and up would move up by one, and "
                 "one of them is already 2147483647, the highest condition number.");
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-ALLOC-01 (job 6): the numbers of NEW conditions may not pass 2147483647. Windows 49a2d59,
// condition_operations.py _allocate_condition_identities :56-62. The expected strings are the output of that function run in memory with
// a stub cursor (the f-string of the refusal is condition_operations.py :57-62); this file pins the decision and the wording in Core, the store test
// (test_condition_duplicate.cpp, CondAlloc01_DuplicateLimit) pins that nothing is written.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
    constexpr const char *kAddOneText =
        "Cannot add 1 condition: the new condition numbers would reach 2147483648, past 2147483647, the highest condition number.";
    constexpr const char *kAddTwoText =
        "Cannot add 2 conditions: the new condition numbers would reach 2147483648, past 2147483647, the highest condition number.";

    // the refusal text of refuse_new_condition_numbers, or "<fits>" when every new number fits
    std::string new_numbers_refusal(long long highest, long long count)
    {
        const std::optional<std::string> refusal = refuse_new_condition_numbers(highest, count);
        return refusal ? *refusal : std::string("<fits>");
    }
}

TEST(CondAlloc01_NewNumbers, TheMessageIsTheWindowsTextWithSingularAndPlural)
{
    // "condition" for exactly one, "conditions" for every other count; X is printed as given
    CHECK_STR_EQ(condition_numbers_overflow_message(1, 2147483648ULL), kAddOneText);
    CHECK_STR_EQ(condition_numbers_overflow_message(2, 2147483648ULL), kAddTwoText);
    CHECK_STR_EQ(condition_numbers_overflow_message(2, 2147483649ULL),
                 "Cannot add 2 conditions: the new condition numbers would reach 2147483649, past 2147483647, the highest condition "
                 "number.");
    CHECK_STR_EQ(condition_numbers_overflow_message(5, 2147483652ULL),
                 "Cannot add 5 conditions: the new condition numbers would reach 2147483652, past 2147483647, the highest condition "
                 "number.");
    CHECK_STR_EQ(condition_numbers_overflow_message(1, 3000000001ULL),
                 "Cannot add 1 condition: the new condition numbers would reach 3000000001, past 2147483647, the highest condition "
                 "number.");
    CHECK_STR_EQ(condition_numbers_overflow_message(1, 9223372036854775808ULL),
                 "Cannot add 1 condition: the new condition numbers would reach 9223372036854775808, past 2147483647, the highest "
                 "condition number.");
    // Windows' f-string: `'' if count == 1 else 's'`, so even a count of zero is plural (never asked by a caller; pins the `== 1`)
    CHECK_STR_EQ(condition_numbers_overflow_message(0, 2147483648ULL),
                 "Cannot add 0 conditions: the new condition numbers would reach 2147483648, past 2147483647, the highest condition "
                 "number.");
}

// The owner's boundaries (one duplicate at 2147483646 stores 2147483647; at 2147483647 it gives X = 2147483648; two duplicates at
// 2147483646 fail) and the other rows computed with the same Windows function.
TEST(CondAlloc01_NewNumbers, TheOwnersThreeBoundariesAndTheEdgesOfTheOracle)
{
    // 2147483646, 2147483647, 2147483648 (one new condition)
    CHECK_STR_EQ(new_numbers_refusal(2147483646LL, 1), "<fits>"); // the last free number is stored: 2147483647
    CHECK_STR_EQ(new_numbers_refusal(2147483647LL, 1), kAddOneText); // X = 2147483648
    CHECK_STR_EQ(new_numbers_refusal(2147483648LL, 1), // a stored number already past the limit
                 "Cannot add 1 condition: the new condition numbers would reach 2147483649, past 2147483647, the highest condition "
                 "number.");

    // several new conditions: all or nothing, X = highest + count, plural
    CHECK_STR_EQ(new_numbers_refusal(2147483646LL, 2), kAddTwoText);
    CHECK_STR_EQ(new_numbers_refusal(2147483645LL, 2), "<fits>"); // 2147483646 and 2147483647
    CHECK_STR_EQ(new_numbers_refusal(2147483647LL, 2),
                 "Cannot add 2 conditions: the new condition numbers would reach 2147483649, past 2147483647, the highest condition "
                 "number.");
    CHECK_STR_EQ(new_numbers_refusal(2147483647LL, 5),
                 "Cannot add 5 conditions: the new condition numbers would reach 2147483652, past 2147483647, the highest condition "
                 "number.");

    // a bid without numbers (passed as 0), a negative highest number: nothing to refuse
    CHECK_STR_EQ(new_numbers_refusal(0, 1), "<fits>");
    CHECK_STR_EQ(new_numbers_refusal(0, 3), "<fits>");
    CHECK_STR_EQ(new_numbers_refusal(-5, 2), "<fits>");
    CHECK_STR_EQ(new_numbers_refusal(-2147483648LL, 1), "<fits>");

    // a stored number that an Access Long cannot hold but SQLite can
    CHECK_STR_EQ(new_numbers_refusal(3000000000LL, 1),
                 "Cannot add 1 condition: the new condition numbers would reach 3000000001, past 2147483647, the highest condition "
                 "number.");
}

TEST(CondAlloc01_NewNumbers, ANumberNearTheInt64EdgeNeverOverflowsTheArithmetic)
{
    // Python's ints do not overflow and print 9223372036854775808; a signed long long sum would wrap to a negative number
    CHECK_STR_EQ(new_numbers_refusal(std::numeric_limits<long long>::max(), 1),
                 "Cannot add 1 condition: the new condition numbers would reach 9223372036854775808, past 2147483647, the highest "
                 "condition number.");
    CHECK_STR_EQ(new_numbers_refusal(std::numeric_limits<long long>::min(), 1), "<fits>");
}

// Duplicate shows its existing alert ("Duplicate Error", the iOS wording of DBP-11) with the store's text when there is one and the
// generic text for every other failure.
TEST(CondAlloc01_DuplicateFailed, ADetailReplacesTheGenericTextUnderDuplicateError)
{
    const FormIssue detailed = condition_duplicate_failed_issue(kAddOneText);
    CHECK_STR_EQ(detailed.title, "Duplicate Error");
    CHECK_STR_EQ(detailed.message, kAddOneText);

    const FormIssue defaulted = condition_duplicate_failed_issue();
    CHECK_STR_EQ(defaulted.title, "Duplicate Error");
    CHECK_STR_EQ(defaulted.message, "Failed to duplicate the condition.");

    const FormIssue empty = condition_duplicate_failed_issue(std::string());
    CHECK_STR_EQ(empty.title, "Duplicate Error");
    CHECK_STR_EQ(empty.message, "Failed to duplicate the condition.");
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-TRIM-01
// ---------------------------------------------------------------------------------------------------------------------

// Port of ...::test_trim_disables_curved_segment_control
TEST(CondTrim01_TrimCurved, TrimAndCurvedSegmentExcludeEachOther)
{
    Condition c;
    c.condition_type = ConditionType::Linear;
    set_curved_segment(c, true);
    CHECK(c.is_curved_segment);
    CHECK(curved_segment_enabled(c));
    set_trim(c, true);
    CHECK_FALSE(c.is_curved_segment);
    CHECK(c.trim);
    CHECK_FALSE(curved_segment_enabled(c));
    set_trim(c, false);
    CHECK(curved_segment_enabled(c));
    CHECK_FALSE(c.is_curved_segment); // un-checking Trim does not bring Curved Segment back
    set_curved_segment(c, true);
    CHECK_FALSE(c.trim);
    CHECK(c.is_curved_segment);
}

// "a loaded condition with both set shows Curved off and persists it as false"
TEST(CondTrim01_TrimCurved, ALoadedConditionWithBothShowsCurvedOff)
{
    Condition c;
    c.condition_type = ConditionType::Linear;
    c.trim = true;
    c.is_curved_segment = true;
    normalize_trim_curved(c);
    CHECK(c.trim);
    CHECK_FALSE(c.is_curved_segment);
    CHECK_FALSE(curved_segment_enabled(c));

    Condition only_curved;
    only_curved.is_curved_segment = true;
    normalize_trim_curved(only_curved);
    CHECK(only_curved.is_curved_segment);
    CHECK_FALSE(only_curved.trim);
}

// ---------------------------------------------------------------------------------------------------------------------
// COL-PICK-01
// ---------------------------------------------------------------------------------------------------------------------

TEST(ColPick01_Colour, PackBgrIsRedGreenBlueFromTheLowByte)
{
    CHECK_EQ(pack_bgr(0x99, 0x66, 0x33), 0x336699);
    CHECK_EQ(pack_bgr(255, 128, 0), 0x0080FF);
    CHECK_EQ(pack_bgr(0, 0, 0), 0);
    CHECK_EQ(pack_bgr(255, 255, 255), 0xFFFFFF);
    // Windows: color.red() | color.green() << 8 | color.blue() << 16, each 0..255; only the low byte of a channel counts
    CHECK_EQ(pack_bgr(0x1FF, -1, 256), 0x00FFFF);
}

// iOS truncated (int)(c * 255.0): 0.6 * 255.0 is 152.99999999999997, so a picked #99... turned into 152 (a change of the colour
// the user never made). The components are rounded to the nearest 8-bit value.
TEST(ColPick01_Colour, FloatChannelsAreRoundedNotTruncated)
{
    CHECK_EQ(color_fill_from_rgb(0.6, 0.4, 0.2), pack_bgr(153, 102, 51)); // #996633
    CHECK_EQ(color_fill_from_rgb(1.0, 0.5, 0.0), pack_bgr(255, 128, 0));  // 127.5 rounds up
    CHECK_EQ(color_fill_from_rgb(0.0, 0.0, 0.0), 0);
    CHECK_EQ(color_fill_from_rgb(1.0, 1.0, 1.0), 0xFFFFFF);
    // every exact k/255 round-trips
    for (int k = 0; k <= 255; ++k)
    {
        SCOPED_CONTEXT("k " + std::to_string(k));
        const double channel = static_cast<double>(k) / 255.0;
        CHECK_EQ(color_fill_from_rgb(channel, channel, channel), pack_bgr(k, k, k));
    }
    // out of range channels (extended colour spaces) are clamped
    CHECK_EQ(color_fill_from_rgb(-0.2, 1.4, 0.5), pack_bgr(0, 255, 128));
}

// Changing the colour writes ColorFill and nothing else: ColorLine is not part of the change set, whatever the form holds
TEST(ColPick01_Colour, ThePickedColourChangesColorFillOnly)
{
    Condition stored;
    stored.uid = "1";
    stored.name = "Wall";
    stored.ref_no = 1;
    stored.uom1 = stored.uom2 = stored.uom3 = -1; // the Windows fixtures: a slot without a quantity has the unit -1
    stored.color_fill = 0x336699;
    stored.color_line = 0x111111;
    Condition form = stored;
    form.color_fill = color_fill_from_rgb(1.0, 0.5, 0.0);
    form.color_line = form.color_fill; // what the old picker callback did
    const Condition to_save = condition_to_save(stored, form, false);
    CHECK_EQ(to_save.color_fill, 0x0080FF);
    CHECK_EQ(to_save.color_line, 0x111111);
    const ConditionChanges changes = collect_condition_changes(stored, to_save);
    CHECK_EQ(changes.fields.count(), 1u);
    CHECK(changes.has(ConditionField::ColorFill));
}
