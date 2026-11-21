// COND-ELEV-01 (Windows b9902e6): the Elevation field of the Edit Condition dialog is validated with the loader's own parser
// (ost_domain::elevation_text_inches, the port of elevation_text.py::elevation_text_inches), not with display_to_inches. A text the
// loader cannot read is refused with the forms that are accepted; a text it can read is saved verbatim as the name suffix and gives,
// in the dialog's z, the number a reload gives for that name: memory equals a reload after a Save.
//
// Windows: presentation/dialogs/edit_condition_dialog.py:1423-1447 (_validate_and_build_dto: strip, elevation_text_inches, a caught
// ValueError and a non-finite number are refused, 'Invalid Value' / '"<text>" is not a valid elevation. <hint>', the hint depends on
// the measurement system and nothing else does) and :386-389 (the placeholder). Windows oracle: tests/presentation/dialogs/
// test_edit_condition_dialog.py :1319-1380 (test_an_elevation_the_loader_cannot_read_is_rejected_with_the_accepted_forms,
// test_an_accepted_elevation_reloads_to_the_value_the_dialog_saved, test_the_elevation_placeholder_shows_a_form_the_loader_reads)
// and :641-651 ('nan' with the imperial hint). The extra rows ("5'", "5'6", decimal inches, a letter m next to a mark, a feet run
// beyond the float range) were computed by running the Windows elevation_text.py in memory (Python 3.11.4).
#include "TestHarness.hpp"

#include "Domain/ConditionFormRules.hpp"
#include "Domain/ConditionStyleChange.hpp"
#include "Domain/PositionParser.hpp"
#include "Persistence/ProjectStore/ProjectStore_Internal.hpp"

#include <cmath>
#include <string>

using ost_domain::Condition;
using ost_domain::ConditionType;
using ost_domain::kElevationHintImperial;
using ost_domain::kElevationHintMetric;
using ost_domain::parse_elevation_z;

namespace
{
    // The Windows wording, spelled out (not taken from the constants under test)
    const char *const kWindowsMetricHint = "Use metres or centimetres, for example \"3.2 m\" or \"320 cm\".";
    const char *const kWindowsImperialHint = "Use feet and inches with unit marks, for example 10' 6\".";

    std::string hint_of(bool metric) { return metric ? kWindowsMetricHint : kWindowsImperialHint; }

    // _validate_and_build_dto: the alert of a refused elevation text. `shown` is the text the alert quotes (the field text stripped).
    void check_refused(const std::string &text, bool metric, const std::string &shown)
    {
        SCOPED_CONTEXT("text=[" + text + "] metric=" + (metric ? "1" : "0"));
        double z = -1.0;
        const auto issue = parse_elevation_z(text, metric, z);
        if (!issue)
        {
            CHECK(issue.has_value()); // the text was accepted
            return;
        }
        CHECK_STR_EQ(issue->title, "Invalid Value");
        CHECK_STR_EQ(issue->message, "\"" + shown + "\" is not a valid elevation. " + hint_of(metric));
        CHECK_EQ(z, -1.0); // a refusal leaves the output untouched
    }

    void check_refused(const std::string &text, bool metric) { check_refused(text, metric, text); }

    Condition stored_condition(const std::string &name)
    {
        Condition c;
        c.uid = "10";
        c.name = name;
        c.condition_type = ConditionType::Linear;
        c.ref_no = 1;
        c.uom1 = c.uom2 = c.uom3 = -1;
        return c;
    }

    // reassemble_elevation(base, type, value) of the dialog for a value that is already stripped
    std::string dialog_name(const std::string &base, bool bottom, const std::string &value)
    {
        return base + (bottom ? " @B " : " @T ") + value;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Refused texts
// ---------------------------------------------------------------------------------------------------------------------

// Port of test_an_elevation_the_loader_cannot_read_is_rejected_with_the_accepted_forms (the six rows, each in the mode the Windows
// test uses) and of the 'nan' row of test_edit_condition_rejects_non_finite_numeric_values. The title is "Invalid Value", the message
// quotes the text and ends with the hint of the measurement system, the output z is not written.
TEST(CondElev01_Validation, AnElevationTheLoaderCannotReadIsRefusedWithTheAcceptedForms)
{
    struct Row
    {
        bool metric;
        const char *text;
    };
    const Row windows_rows[] = {
        {true, "3200"}, {true, "3200mm"}, {false, "126"}, {false, "10' 6"}, {false, "10' 6\" 2m"}, {false, "1/0\""}, {false, "nan"},
    };
    for (const Row &row : windows_rows)
        check_refused(row.text, row.metric);
}

// Windows accepts and refuses the same texts in both measurement systems; only the hint depends on the mode. So every refused text
// is refused with the metric hint in a metric bid and with the imperial hint in an imperial bid: a bare number, mm, units of the
// two systems in one text, "5'" alone, a zero denominator, and the shapes the shared gate does not know.
TEST(CondElev01_Validation, EveryRefusedTextIsRefusedInBothModesWithTheHintOfTheMode)
{
    const char *const texts[] = {
        "3200",          // a bare number, metric bid in the Windows test
        "126",           // a bare number, imperial bid in the Windows test
        "5",             //
        "3200mm",        // mm
        "3200 mm",       //
        "25.4 mm",       //
        "10' 6\"  3 m",  // mixed units
        "10' 6\" 2m",    //
        "3 m 6\"",       //
        "5'",            // feet alone: no word boundary after the foot mark
        "5' ",           // (a trailing blank is stripped, so this is "5'" too)
        "-4'",           //
        "10' 6",         // no inch mark
        "1/0\"",         // a zero denominator (Windows raises ValueError, the dialog catches it)
        "5' 6 1/0\"",    //
        "nan",           //
        "inf",           //
        "1e2",           //
        "later",         //
        "5' 1/2\"",      // an inch fraction after feet without a whole inch is outside the gate
        "6.5\"",         // decimal inches
        "10' 6.3\"",     //
        "5' foam",       // refused by the GATE: the feet-only pattern needs a word boundary right after the foot mark, a blank follows
        "5'foam",        // the foot mark glued to a word with an m: the gate accepts it, then it reads as mixed units (value None)
        "6\" mm",        // an inch mark and a letter m: mixed units
    };
    for (const char *text : texts)
    {
        const std::string stripped = text == std::string("5' ") ? "5'" : text;
        check_refused(text, true, stripped);
        check_refused(text, false, stripped);
    }
}

// The alert quotes the STRIPPED text (elev_value = text.strip(), shown as '"<elev_value>"'), Unicode white space included.
TEST(CondElev01_Validation, TheAlertQuotesTheStrippedText)
{
    check_refused("  3200  ", true, "3200");
    check_refused("\t126\n", false, "126");
    check_refused("\xC2\xA0" "10' 6\xC2\xA0", false, "10' 6");
    // inner white space stays as typed
    check_refused(" 10' 6\"  2m ", false, "10' 6\"  2m");
}

// isfinite: a metre figure beyond the float range reads as inf (no exception) and is refused like the other unreadable texts.
TEST(CondElev01_Validation, AnInfiniteElevationIsRefused)
{
    const std::string huge = "1" + std::string(400, '0') + " m";
    for (const bool metric : {true, false})
    {
        check_refused(huge, metric);
        check_refused("-" + huge, metric);
        check_refused("1" + std::string(400, '0') + " cm", metric); // 1e400 cm is inf as well
    }
}

// Not in the Windows tests, a deliberate deviation: a feet run beyond the float range raises OverflowError in the Windows dialog,
// which catches only ValueError (an uncaught exception in a Qt slot). iOS refuses it with the normal message. A feet run beyond the
// 4300-digit int() limit is a ValueError on Windows, caught: the same message.
TEST(CondElev01_Validation, AFeetRunBeyondTheFloatRangeIsRefusedWithTheNormalMessage)
{
    for (const bool metric : {true, false})
    {
        check_refused("1" + std::string(404, '0') + "'x", metric);
        check_refused("-1" + std::string(404, '0') + "' 6\"", metric);
        check_refused("1" + std::string(4399, '0') + "'x", metric);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Accepted texts: the saved condition equals a reload
// ---------------------------------------------------------------------------------------------------------------------

// Port of test_an_accepted_elevation_reloads_to_the_value_the_dialog_saved. Windows runs each row in one mode (metric text in a
// metric bid, feet and inches in an imperial bid, and "2 m" in an imperial bid to show that the mode does not matter); acceptance
// does not depend on the mode, so every row runs in both. For Top and Bottom: the name is base + " @T " / " @B " + the text
// verbatim, the dialog's z is the number of the text, and a reload of that name (the loader's derivation, the loader's helper, and
// the condition condition_to_save returns) gives exactly that z and is_top = Top.
TEST(CondElev01_Validation, AnAcceptedElevationReloadsToTheValueTheDialogSaved)
{
    struct Row
    {
        const char *text;
        double inches;
    };
    const Row rows[] = {
        // the seven Windows rows
        {"3.2 m", 320.0 / 2.54},
        {"320 cm", 320.0 / 2.54},
        {"3m 20cm", 320.0 / 2.54},
        {"10' 6\"", 126.0},
        {"-2' 3 1/2\"", -27.5},
        {"6\"", 6.0},
        {"2 m", 200.0 / 2.54},
        // run on Windows: more forms of the shared grammar
        {"3,5 m", 350.0 / 2.54},
        {"-1m+5cm", -105.0 / 2.54},
        {"5'x", 60.0},
        {"5'6", 60.0},
    };
    size_t checked = 0;
    for (const Row &row : rows)
    {
        for (const bool metric : {true, false})
        {
            for (const bool bottom : {false, true})
            {
                SCOPED_CONTEXT(std::string("text=[") + row.text + "] metric=" + (metric ? "1" : "0") + " bottom=" + (bottom ? "1" : "0"));
                double z = -1.0;
                CHECK_FALSE(parse_elevation_z(row.text, metric, z).has_value());
                CHECK_NEAR(z, row.inches, 1e-9);

                const std::string name = dialog_name("Footing", bottom, row.text);

                // a reload of the saved name
                double reloaded_z = 123.0;
                bool reloaded_top = bottom;
                ost_domain::derive_elevation_from_name(name, reloaded_z, reloaded_top);
                CHECK_EQ(reloaded_z, z);
                CHECK(reloaded_top == !bottom);
                const std::pair<double, bool> loader = ost_store::extract_z_value(name);
                CHECK_EQ(loader.first, z);
                CHECK(loader.second == !bottom);

                // the form as the dialog builds it (z from the text, is_top from the segment) and the condition a Save keeps in memory
                const Condition stored = stored_condition("Footing");
                Condition form = stored;
                form.name = name;
                form.z_value = z;
                form.is_top = !bottom;
                const Condition saved = ost_domain::condition_to_save(stored, form, metric);
                CHECK_STR_EQ(saved.name, name);
                CHECK_EQ(saved.z_value, z);
                CHECK(saved.is_top == !bottom);
                // memory equals a reload, and the form's own values were already those
                CHECK_EQ(saved.z_value, form.z_value);
                CHECK(saved.is_top == form.is_top);
                ++checked;
            }
        }
    }
    CHECK_EQ(checked, size_t(44));
}

// The text is stripped before it is read and stored: surrounding white space is neither refused nor part of the suffix; the number is
// the one of the stripped text.
TEST(CondElev01_Validation, SurroundingWhiteSpaceIsStrippedAndTheNumberIsTheSame)
{
    for (const bool metric : {true, false})
    {
        double z = -1.0;
        CHECK_FALSE(parse_elevation_z(" 10' 6\" ", metric, z).has_value());
        CHECK_EQ(z, 126.0);
        CHECK_FALSE(parse_elevation_z("\t3.2 m\xC2\xA0", metric, z).has_value());
        CHECK_NEAR(z, 320.0 / 2.54, 1e-9);
    }
}

// A blank elevation is never validated: z 0.0, whatever the mode, and the name gets no suffix.
TEST(CondElev01_Validation, ABlankElevationIsZeroAndNeverValidated)
{
    for (const bool metric : {true, false})
    {
        for (const char *text : {"", " ", "   ", "\t", "\xC2\xA0 \xE2\x80\x83"})
        {
            SCOPED_CONTEXT(std::string("text length ") + std::to_string(std::string(text).size()) + " metric=" + (metric ? "1" : "0"));
            double z = 5.0;
            CHECK_FALSE(parse_elevation_z(text, metric, z).has_value());
            CHECK_EQ(z, 0.0);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The hints and the placeholder
// ---------------------------------------------------------------------------------------------------------------------

// edit_condition_dialog.py:386-389 setPlaceholderText: 'e.g. 3.2 m' in a metric bid, 'e.g. 10' 6"' in an imperial one: a form the
// loader reads (the old iOS placeholder 'e.g. 3200' was a text the loader does not read). The two hint strings are the Windows ones,
// and the examples they and the placeholders show are accepted by the validation in both modes.
TEST(CondElev01_Placeholder, ThePlaceholderShowsAFormTheLoaderReads)
{
    CHECK_STR_EQ(ost_domain::elevation_placeholder(true), "e.g. 3.2 m");
    CHECK_STR_EQ(ost_domain::elevation_placeholder(false), "e.g. 10' 6\"");

    CHECK_STR_EQ(kElevationHintMetric, kWindowsMetricHint);
    CHECK_STR_EQ(kElevationHintImperial, kWindowsImperialHint);

    for (const bool metric : {true, false})
    {
        const std::string placeholder = ost_domain::elevation_placeholder(metric);
        SCOPED_CONTEXT("placeholder=" + placeholder);
        REQUIRE(placeholder.rfind("e.g. ", 0) == 0);
        const std::string example = placeholder.substr(5);
        double inches = 0.0;
        CHECK(ost_domain::elevation_text_inches(example, inches) == ost_domain::ElevationTextStatus::Value);
        double z = -1.0;
        CHECK_FALSE(parse_elevation_z(example, metric, z).has_value());
        CHECK_EQ(z, inches);
    }

    // the examples of the hints
    double z = -1.0;
    CHECK_FALSE(parse_elevation_z("3.2 m", true, z).has_value());
    CHECK_FALSE(parse_elevation_z("320 cm", true, z).has_value());
    CHECK_FALSE(parse_elevation_z("10' 6\"", false, z).has_value());
    CHECK_EQ(z, 126.0);
}
