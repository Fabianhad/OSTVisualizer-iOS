// The field rules of the Edit Condition dialog (ConditionFormRules.hpp): COND-REF-01 (condition number is validated, not
// coerced), COND-VAL-01 (Display Size and plain numbers), DLG-RISE-01 (Area rise / run are absolute), UI-MSG-01 (alert
// wording), COND-ELEV-01 (the elevation text is kept verbatim and validated; the saved z / is_top come from the name:
// test_condition_elevation.cpp), COND-TRIM-01 (Trim and Curved Segment exclude
// each other), COL-PICK-01 (the colour writes ColorFill only).
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
#include <string>

using namespace ost_domain;

namespace
{
    constexpr const char *kRefNoMessage = "Condition number must be a positive whole number.";

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

// A number beyond the int range of a Condition cannot be stored: refused with the same alert (Windows would accept it up to
// the 64 bit range of the database column).
TEST(CondRef01_ConditionNumber, ANumberBeyondTheStorableRangeIsRefused)
{
    int value = 0;
    CHECK_STR_EQ(ref_no_issue("2147483648", value), kRefNoMessage);
    CHECK_STR_EQ(ref_no_issue("99999999999999999999", value), kRefNoMessage);
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

// edit_condition_dialog.py :1539 / :1551 (Linear) and :1569 / :1581 (Area, abs()): float(self._rise_edit.text() or "0"). The field
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
// (edit_condition_dialog.py :1566-1593, abs(float(...)) for every style but Linear).
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

// Port of ...::test_edit_condition_rejects_non_finite_numeric_values ("elevation": '"nan" is not a valid elevation.') and the
// COND-ELEV-01 test plan: "10'6" -> z 126.0, "10' 6.3\"" -> z 126.3, empty -> 0.0
TEST(CondElev01_Elevation, TheTextGivesZAndNanIsRefused)
{
    double z = -1.0;
    CHECK_FALSE(parse_elevation_z("10'6", false, z).has_value());
    CHECK_EQ(z, 126.0);
    CHECK_FALSE(parse_elevation_z("10' 6.3\"", false, z).has_value());
    CHECK_EQ(z, 126.3);
    CHECK_FALSE(parse_elevation_z("", false, z).has_value());
    CHECK_EQ(z, 0.0);
    z = -1.0;
    const auto issue = parse_elevation_z("nan", false, z);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->title, "Invalid Value");
    CHECK_STR_EQ(issue->message, "\"nan\" is not a valid elevation.");
    CHECK_EQ(z, -1.0);
    // a metric bid reads millimetres
    CHECK_FALSE(parse_elevation_z("3200", true, z).has_value());
    CHECK_EQ(z, 125.98425196850394);
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
