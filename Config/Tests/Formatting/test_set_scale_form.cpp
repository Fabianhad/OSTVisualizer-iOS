// Set Scale, the form rules (plan 5.4, tests-first): the validation and its Windows alert texts, the initial state, the style change, the dirty / baseline state
// machine and the apply flow, the page set of "Apply to all pages" and the "is the dialog's page still the same" check.
// Windows source of truth: set_scale_dialog.py:10-357 (the dialog), ui_event_coordinator.py:5381-5409 and :5493-5643 (the page set, the context, the save closure).
// Ported Windows tests: tests/integration/pages/test_set_scale_apply.py (group I, driven through ScaleFormSession with a fake store instead of Qt and the write
// service), tests/presentation/coordinators/test_ui_event_coordinator.py (group L, driven through page_dialog_context). No Windows test pins the validation texts,
// the presets or the {:g} fill (register: "source only"): those tests are characterisations of the source lines they cite.
// Differences from Windows, each in the name or the comment of the test: nan / inf refused in the form (SS-3), the custom fields filled with full precision (SS-5),
// Cancel + Save only (no Apply button), "same active page" dropped from the context (the menu acts on the PRESSED page).
#include "FormattingTestSupport.hpp"

#include "Data/ProjectData.hpp"
#include "Domain/PageScaleRules.hpp"

#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ost_domain;
using std::optional;
using std::string;
using std::vector;

namespace
{
    constexpr double kNotANumber = std::numeric_limits<double>::quiet_NaN();

    ScaleFormState custom_state(const string &first, const string &second, bool apply_all = false)
    {
        ScaleFormState state;
        state.style = kScaleStyleArchitectural;
        state.mode = ScaleInputMode::Custom;
        state.has_preset = true;
        state.preset_sf1 = 0.03125;
        state.preset_sf2 = 12.0;
        state.custom1 = first;
        state.custom2 = second;
        state.apply_to_all = apply_all;
        return state;
    }

    ScaleFormState preset_state(int style, double sf1, double sf2, bool apply_all = false)
    {
        ScaleFormState state;
        state.style = style;
        state.mode = ScaleInputMode::Predefined;
        state.has_preset = true;
        state.preset_sf1 = sf1;
        state.preset_sf2 = sf2;
        state.custom1 = "1";
        state.custom2 = "1";
        state.apply_to_all = apply_all;
        return state;
    }

    // One refused field: the alert and the focus.
    struct Refusal
    {
        bool refused = false;
        string title;
        string message;
        int focus = -1;
    };

    Refusal validate(const ScaleFormState &state, ScaleSettings *settings_out = nullptr)
    {
        ScaleSettings sentinel;
        sentinel.scale_factor1 = 777.0;
        sentinel.scale_factor2 = 888.0;
        int focus = -1;
        const optional<FormIssue> issue = validate_scale_form(state, sentinel, &focus);
        Refusal result;
        result.refused = issue.has_value();
        if (issue)
        {
            result.title = issue->title;
            result.message = issue->message;
            result.focus = focus;
            // `out` is untouched on a refusal
            CHECK_EQ(sentinel.scale_factor1, 777.0);
            CHECK_EQ(sentinel.scale_factor2, 888.0);
        }
        if (settings_out)
            *settings_out = sentinel;
        return result;
    }

    // The fake of the write service of test_set_scale_apply.py (_write): records every call and moves the stored pair of the page(s).
    struct FakeStore
    {
        std::map<string, std::pair<double, double>> pages;
        vector<string> write_log;      // "42" / "42,43": the pages one save wrote
        vector<ScaleSettings> settings; // what each save wrote
        bool fail = false;
        bool throw_instead = false;

        bool save(const ScaleSettings &s, const vector<string> &page_uids)
        {
            if (throw_instead)
                throw std::runtime_error("write failed");
            if (fail)
                return false;
            string joined;
            for (const string &uid : page_uids)
            {
                pages[uid] = {s.scale_factor1, s.scale_factor2};
                joined += (joined.empty() ? "" : ",") + uid;
            }
            write_log.push_back(joined);
            settings.push_back(s);
            return true;
        }
    };

    ost_data::ProjectData project_with_pages(const std::map<string, std::pair<double, double>> &pages, const vector<string> &order)
    {
        ost_data::ProjectData project;
        project.bid.uid = "7";
        for (const auto &entry : pages)
        {
            ost_domain::Page page;
            page.uid = entry.first;
            page.name = "Page " + entry.first;
            page.scale_factor1 = entry.second.first;
            page.scale_factor2 = entry.second.second;
            project.pages[page.uid] = page;
        }
        project.page_order = order;
        return project;
    }
}

// ---- validation: the texts and the order of the checks (set_scale_dialog.py:216-262) --------------------------------------------------------

// set_scale_dialog.py:216-234: a Pre-defined scale gives the pair of the preset; "Apply to all pages" is copied.
TEST(SetScale_FormValidation, APredefinedScaleGivesTheSelectedPair)
{
    ScaleSettings settings;
    CHECK_FALSE(validate(preset_state(kScaleStyleArchitectural, 0.125, 12.0), &settings).refused);
    CHECK_EQ(settings.scale_factor1, 0.125);
    CHECK_EQ(settings.scale_factor2, 12.0);
    CHECK_FALSE(settings.apply_to_all_pages);
    CHECK_FALSE(validate(preset_state(kScaleStyleMetric, 1.0, 100.0, true), &settings).refused);
    CHECK_EQ(settings.scale_factor1, 1.0);
    CHECK_EQ(settings.scale_factor2, 100.0);
    CHECK(settings.apply_to_all_pages);
}

// set_scale_dialog.py:217-221: the Pre-defined radio with no preset data (it cannot happen with the fixed lists) is "Invalid Scale" / "Select a predefined scale.".
TEST(SetScale_FormValidation, APredefinedScaleWithoutAPresetIsRefused)
{
    ScaleFormState state = preset_state(kScaleStyleArchitectural, 0.0, 0.0);
    state.has_preset = false;
    const Refusal got = validate(state);
    CHECK(got.refused);
    CHECK_STR_EQ(got.title, "Invalid Scale");
    CHECK_STR_EQ(got.message, "Select a predefined scale.");
    CHECK_EQ(got.focus, 0);
}

// set_scale_dialog.py:239-245: an empty field (after strip) is "Custom scale values cannot be empty."; the first field is checked first, the focus goes to that field.
TEST(SetScale_FormValidation, AnEmptyCustomFieldIsRefusedFirstFieldFirst)
{
    const std::pair<string, string> rows[] = {{"", "1"}, {"   ", "1"}, {"", ""}, {"\t\n", "x"}};
    for (const auto &texts : rows)
    {
        SCOPED_CONTEXT("first '" + texts.first + "'");
        const Refusal got = validate(custom_state(texts.first, texts.second));
        CHECK(got.refused);
        CHECK_STR_EQ(got.title, "Invalid Scale");
        CHECK_STR_EQ(got.message, "Custom scale values cannot be empty.");
        CHECK_EQ(got.focus, 1);
    }
    const Refusal second = validate(custom_state("1", "  "));
    CHECK_STR_EQ(second.message, "Custom scale values cannot be empty.");
    CHECK_EQ(second.focus, 2);
}

// set_scale_dialog.py:246-255: a text that Python float() refuses names itself, stripped, and which field it is: `The first|second custom scale value "<text>" is not a valid number.`
// The accepted grammar is Python's: ',' / '0x10' / '1 2' / '1/8' / 'abc' are refused, surrounding whitespace, '+5', '.5', '5.', '1e3' and '1_0' are accepted.
TEST(SetScale_FormValidation, ATextThatPythonFloatRefusesIsNotAValidNumber)
{
    const vector<string> refused = {"abc", "1,5", "0x10", "1 2", "1/8", "1__0", "_1", "1_", "--1", "1e", "e5", ".", "1.2.3", "12abc", "\xE2\x82\xAC""5"};
    for (const string &text : refused)
    {
        SCOPED_CONTEXT("text '" + text + "'");
        const Refusal first = validate(custom_state(text, "1"));
        CHECK(first.refused);
        CHECK_STR_EQ(first.title, "Invalid Scale");
        CHECK_STR_EQ(first.message, "The first custom scale value \"" + text + "\" is not a valid number.");
        CHECK_EQ(first.focus, 1);
        const Refusal second = validate(custom_state("1", text));
        CHECK_STR_EQ(second.message, "The second custom scale value \"" + text + "\" is not a valid number.");
        CHECK_EQ(second.focus, 2);
    }
    // the shown text is the STRIPPED text
    CHECK_STR_EQ(validate(custom_state("  x  ", "1")).message, "The first custom scale value \"x\" is not a valid number.");
}

// The same rows parse: Python float() accepts them (S19 of the research: "1_0", " 12 ", "1e3", "+5", ".5", "5.", Unicode digits).
TEST(SetScale_FormValidation, TextsThatPythonFloatAcceptsAreAcceptedAndStripped)
{
    const struct
    {
        const char *text;
        double value;
    } rows[] = {
        {"1_0", 10.0}, {" 12 ", 12.0}, {"1e3", 1000.0}, {"+5", 5.0}, {".5", 0.5}, {"5.", 5.0}, {"0.125", 0.125}, {"1E2", 100.0}, {"\t7\n", 7.0},
        {"\xD9\xA1", 1.0}, // U+0661 ARABIC-INDIC DIGIT ONE: Python float() of that digit is 1.0
        {"1e-320", 1e-320}, {"1e308", 1e308}, {"0.0000001", 0.0000001},
    };
    for (const auto &row : rows)
    {
        SCOPED_CONTEXT(string("text '") + row.text + "'");
        double value = 0.0;
        CHECK(!parse_custom_scale_value(row.text, "first", value).has_value());
        CHECK_EQ(value, row.value);
        ScaleSettings settings;
        CHECK_FALSE(validate(custom_state(row.text, "2"), &settings).refused);
        CHECK_EQ(settings.scale_factor1, row.value);
        CHECK_EQ(settings.scale_factor2, 2.0);
    }
}

// set_scale_dialog.py:256-261: a value <= 0 is "Custom scale values must be positive." (0, -0, -1, -1e-300).
TEST(SetScale_FormValidation, ANonPositiveValueIsRefused)
{
    for (const char *text : {"0", "-0", "0.0", "-1", "-0.5", "-1e-300"})
    {
        SCOPED_CONTEXT(string("text '") + text + "'");
        const Refusal first = validate(custom_state(text, "1"));
        CHECK(first.refused);
        CHECK_STR_EQ(first.message, "Custom scale values must be positive.");
        CHECK_EQ(first.focus, 1);
        const Refusal second = validate(custom_state("1", text));
        CHECK_STR_EQ(second.message, "Custom scale values must be positive.");
        CHECK_EQ(second.focus, 2);
    }
}

// set_scale_dialog.py:224-229: the FIRST field is parsed completely (empty, number, sign) before the SECOND is looked at.
TEST(SetScale_FormValidation, TheFirstFieldIsParsedCompletelyBeforeTheSecond)
{
    CHECK_STR_EQ(validate(custom_state("0", "abc")).message, "Custom scale values must be positive.");
    CHECK_EQ(validate(custom_state("0", "abc")).focus, 1);
    CHECK_STR_EQ(validate(custom_state("abc", "")).message, "The first custom scale value \"abc\" is not a valid number.");
    CHECK_STR_EQ(validate(custom_state("", "abc")).message, "Custom scale values cannot be empty.");
    CHECK_EQ(validate(custom_state("", "abc")).focus, 1);
    CHECK_STR_EQ(validate(custom_state("1", "")).message, "Custom scale values cannot be empty.");
    CHECK_EQ(validate(custom_state("1", "")).focus, 2);
    CHECK_STR_EQ(validate(custom_state("-3", "-4")).message, "Custom scale values must be positive.");
    CHECK_EQ(validate(custom_state("-3", "-4")).focus, 1);
}

// SS-3 (hardening, register): nan and the infinities pass the Windows dialog (nan <= 0 is False) and are refused later with the generic "Save Failed". iOS refuses
// them in the form, as "not a valid number", in any case and with any sign: "-inf" is refused as not valid BEFORE the sign check.
TEST(SetScale_FormValidation, NanAndInfinityAreRefusedInTheForm_Hardening)
{
    for (const char *text : {"nan", "NaN", "NAN", "-nan", "+nan", "inf", "Inf", "INF", "+inf", "-inf", "infinity", "Infinity", "-Infinity", "1e400", "-1e400"})
    {
        SCOPED_CONTEXT(string("text '") + text + "'");
        const Refusal first = validate(custom_state(text, "1"));
        CHECK(first.refused);
        CHECK_STR_EQ(first.title, "Invalid Scale");
        CHECK_STR_EQ(first.message, string("The first custom scale value \"") + text + "\" is not a valid number.");
        const Refusal second = validate(custom_state("1", text));
        CHECK_STR_EQ(second.message, string("The second custom scale value \"") + text + "\" is not a valid number.");
    }
}

// The parser of one field returns the alert and leaves the value alone on a refusal.
TEST(SetScale_FormValidation, ParseCustomScaleValueLeavesTheOutputOnARefusal)
{
    double value = 123.0;
    const optional<FormIssue> issue = parse_custom_scale_value("abc", "second", value);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->title, "Invalid Scale");
    CHECK_STR_EQ(issue->message, "The second custom scale value \"abc\" is not a valid number.");
    CHECK_EQ(value, 123.0);
    CHECK(!parse_custom_scale_value("96", "first", value).has_value());
    CHECK_EQ(value, 96.0);
}

// ---- the initial state (set_scale_dialog.py:134-165) ----------------------------------------------------------------------------------------

// A page that is a preset opens on that preset (the first match over styles 1, 2, 3), the Pre-defined radio, and the custom fields filled with the pair.
TEST(SetScale_FormSession, APageThatIsAPresetOpensOnThePreset)
{
    const struct
    {
        double sf1, sf2;
        int style;
        const char *custom1;
        const char *custom2;
    } rows[] = {
        {0.125, 12.0, kScaleStyleArchitectural, "0.125", "12"},
        {1.0, 240.0, kScaleStyleCivil, "1", "240"},
        {1.0, 100.0, kScaleStyleMetric, "1", "100"},
        {1.0, 1.0, kScaleStyleCivil, "1", "1"}, // (1, 1) is in Civil and in Metric: the first style wins (SS-7)
        {50.0, 1.0, kScaleStyleMetric, "50", "1"},
    };
    for (const auto &row : rows)
    {
        SCOPED_CONTEXT("(" + ost_test::detail::exact_float(row.sf1) + ", " + ost_test::detail::exact_float(row.sf2) + ")");
        const ScaleFormSession session(row.sf1, row.sf2);
        const ScaleFormState state = session.initial();
        CHECK_EQ(state.style, row.style);
        CHECK(state.mode == ScaleInputMode::Predefined);
        CHECK(state.has_preset);
        CHECK_EQ(state.preset_sf1, row.sf1);
        CHECK_EQ(state.preset_sf2, row.sf2);
        CHECK_STR_EQ(state.custom1, row.custom1);
        CHECK_STR_EQ(state.custom2, row.custom2);
        CHECK_FALSE(state.apply_to_all);
        CHECK_FALSE(session.dirty(state)); // the initial state is the baseline
    }
}

// A page that matches no preset opens on style 1 with the Custom radio and the first preset of style 1 as the picker's value; the fields hold the pair.
TEST(SetScale_FormSession, APageThatMatchesNoPresetOpensOnCustom)
{
    const ScaleFormSession session(0.26, 12.0);
    const ScaleFormState state = session.initial();
    CHECK_EQ(state.style, kScaleStyleArchitectural);
    CHECK(state.mode == ScaleInputMode::Custom);
    CHECK(state.has_preset);
    CHECK_EQ(state.preset_sf1, 0.03125);
    CHECK_EQ(state.preset_sf2, 12.0);
    CHECK_STR_EQ(state.custom1, "0.26");
    CHECK_STR_EQ(state.custom2, "12");
    CHECK_FALSE(session.dirty(state));
}

// set_scale_dialog.py:38-39: float(sf or 1.0): a 0 (or -0) becomes 1.0, a NaN stays NaN. So a page stored as (0, 0) opens as (1, 1) = Civil `1" = 1"`.
TEST(SetScale_FormSession, ZeroScalesOpenAsOneAndNaNStaysNaN)
{
    const ScaleFormState zero = ScaleFormSession(0.0, 0.0).initial();
    CHECK_EQ(zero.style, kScaleStyleCivil);
    CHECK(zero.mode == ScaleInputMode::Predefined);
    CHECK_STR_EQ(zero.custom1, "1");
    CHECK_STR_EQ(zero.custom2, "1");
    const ScaleFormState negative_zero = ScaleFormSession(-0.0, 12.0).initial();
    CHECK_EQ(negative_zero.preset_sf1, 1.0 / 1.0);
    CHECK_STR_EQ(negative_zero.custom1, "1");
    const ScaleFormState nan = ScaleFormSession(kNotANumber, 12.0).initial();
    CHECK(nan.mode == ScaleInputMode::Custom);
    CHECK_STR_EQ(nan.custom1, "nan");
    CHECK_STR_EQ(nan.custom2, "12");
}

// SS-5 (deliberate, register): the custom fields are filled with FULL precision (".15g"), not f"{v:g}". A page that is (1, 12.0000001) shows "12.0000001", so ticking
// only "Apply to all pages" and saving writes that value back and does not rescale the page by the truncation error (Windows: "12", a change of 8e-9).
TEST(SetScale_FormSession, TheCustomFieldsAreFilledWithFullPrecision_Hardening)
{
    const ScaleFormSession session(1.0, 12.0000001);
    ScaleFormState state = session.initial();
    CHECK(state.mode == ScaleInputMode::Custom);
    CHECK_STR_EQ(state.custom1, "1");
    CHECK_STR_EQ(state.custom2, "12.0000001");
    CHECK_STR_EQ(ScaleFormSession(0.1 + 0.2, 1.0).initial().custom1, "0.3");
    CHECK_STR_EQ(ScaleFormSession(0.000000123456789, 1.0).initial().custom1, "1.23456789e-07");
    state.apply_to_all = true; // the only change
    ScaleSettings settings;
    CHECK_FALSE(validate(state, &settings).refused);
    CHECK_EQ(settings.scale_factor1, 1.0);
    CHECK_NEAR(settings.scale_factor2, 12.0000001, 1e-13);
    CHECK(settings.scale_factor2 != 12.0);
    CHECK(settings.apply_to_all_pages);
}

// ---- the style change (set_scale_dialog.py:167-189) ------------------------------------------------------------------------------------------

// The picker lists the presets of the new style, keeps the selected pair when the new list has it, else the first item; the Pre-defined radio turns on; the custom texts
// and the switch are kept.
TEST(SetScale_FormSession, ChangingTheStyleKeepsASharedPresetElseTheFirstItem)
{
    const struct
    {
        int from_style;
        double sf1, sf2;
        int to_style;
        double expected_sf1, expected_sf2;
    } rows[] = {
        {kScaleStyleArchitectural, 0.25, 12.0, kScaleStyleCivil, 1.0, 1.0},          // not in Civil: the first Civil item
        {kScaleStyleCivil, 1.0, 1.0, kScaleStyleMetric, 1.0, 1.0},                   // (1, 1) is in Metric: kept (not Metric's first, 1 : 1000)
        {kScaleStyleMetric, 1.0, 100.0, kScaleStyleCivil, 1.0, 1.0},                 // 1 : 100 is not in Civil
        {kScaleStyleMetric, 1.0, 100.0, kScaleStyleArchitectural, 0.03125, 12.0},    // not in Architectural
        {kScaleStyleCivil, 1.0, 240.0, kScaleStyleMetric, 1.0, 1000.0},              // not in Metric: the first Metric item
        {kScaleStyleCivil, 1.0, 1.0, kScaleStyleArchitectural, 0.03125, 12.0},
        {kScaleStyleArchitectural, 0.125, 12.0, kScaleStyleArchitectural, 0.125, 12.0}, // the same style: unchanged
    };
    for (const auto &row : rows)
    {
        SCOPED_CONTEXT("from " + std::to_string(row.from_style) + " to " + std::to_string(row.to_style));
        ScaleFormState before = custom_state("0.26", "12", true);
        before.style = row.from_style;
        before.preset_sf1 = row.sf1;
        before.preset_sf2 = row.sf2;
        const ScaleFormState after = scale_form_after_style_change(before, row.to_style);
        CHECK_EQ(after.style, row.to_style);
        CHECK(after.mode == ScaleInputMode::Predefined);
        CHECK(after.has_preset);
        CHECK_EQ(after.preset_sf1, row.expected_sf1);
        CHECK_EQ(after.preset_sf2, row.expected_sf2);
        CHECK_STR_EQ(after.custom1, "0.26"); // untouched
        CHECK_STR_EQ(after.custom2, "12");
        CHECK(after.apply_to_all);
    }
}

// A style with no presets (never offered by the form) leaves the picker empty: no preset, and a Save of it is "Select a predefined scale.".
TEST(SetScale_FormSession, AStyleWithoutPresetsLeavesThePickerEmpty)
{
    const ScaleFormState after = scale_form_after_style_change(preset_state(kScaleStyleCivil, 1.0, 240.0), 9);
    CHECK_EQ(after.style, 9);
    CHECK_FALSE(after.has_preset);
    CHECK_STR_EQ(validate(after).message, "Select a predefined scale.");
}

// ---- dirty and baseline (set_scale_dialog.py:197-214) ---------------------------------------------------------------------------------------

// dirty = state != baseline: changing a value and changing it back clears the flag; the custom texts compare after strip().
TEST(SetScale_FormSession, DirtyIsTheStateComparedWithTheBaseline)
{
    const ScaleFormSession session(0.125, 12.0);
    ScaleFormState state = session.initial();
    CHECK_FALSE(session.dirty(state));
    state.apply_to_all = true;
    CHECK(session.dirty(state));
    state.apply_to_all = false;
    CHECK_FALSE(session.dirty(state)); // changed back: clean again
    state.custom1 = "  0.125 ";
    CHECK_FALSE(session.dirty(state)); // strip(): the same text
    state.custom1 = "0.126";
    CHECK(session.dirty(state));
    state.custom1 = "0.125";
    state.mode = ScaleInputMode::Custom;
    CHECK(session.dirty(state));
    state.mode = ScaleInputMode::Predefined;
    state.style = kScaleStyleCivil;
    CHECK(session.dirty(state));
    state.style = kScaleStyleArchitectural;
    state.preset_sf1 = 0.1875;
    CHECK(session.dirty(state));
}

// mark_saved moves the baseline: the saved state is clean, the old one is dirty, and so is any edit after it.
TEST(SetScale_FormSession, MarkSavedMovesTheBaseline)
{
    ScaleFormSession session(0.125, 12.0);
    const ScaleFormState original = session.initial();
    ScaleFormState changed = custom_state("1", "96");
    CHECK(session.dirty(changed));
    session.mark_saved(changed);
    CHECK_FALSE(session.dirty(changed));
    CHECK(session.dirty(original));
    changed.custom2 = "192";
    CHECK(session.dirty(changed));
}

// ---- group I: tests/integration/pages/test_set_scale_apply.py (driven through ScaleFormSession with a fake store) ------------------------------

// test_set_scale_apply.py:150 test_two_applies_use_same_authoritative_page_without_reload: page 42 is (1 : 48); custom 1 : 96 and Save writes once, the page reads (1, 96),
// the form is not dirty and the context is still current; then 1 : 192 writes a second time and the field still shows "192". (The Python object identity and the
// open dialog are widget state; the "authoritative page" is the baseline pair of the context check.)
TEST(SetScale_FormApply, TwoAppliesUseTheSamePageAsUpdatedByTheFirst)
{
    FakeStore store;
    store.pages["42"] = {1.0, 48.0};
    ost_data::ProjectData project = project_with_pages(store.pages, {"42"});
    ScaleFormSession session(1.0, 48.0);
    double baseline_sf1 = 1.0;
    double baseline_sf2 = 48.0;
    ScaleFormState state = session.initial();
    optional<FormIssue> issue;
    auto save = [&](const ScaleSettings &settings) {
        if (!page_dialog_context(project, "bid-7", "bid-7", "42", baseline_sf1, baseline_sf2, true).current())
            return false;
        if (!store.save(settings, page_setting_uids(project, "42", settings.apply_to_all_pages)))
            return false;
        project.pages["42"].scale_factor1 = settings.scale_factor1; // the memory twin of the success path
        project.pages["42"].scale_factor2 = settings.scale_factor2;
        baseline_sf1 = settings.scale_factor1; // the dialog adopts the page as updated (`page = current_page`)
        baseline_sf2 = settings.scale_factor2;
        return true;
    };
    state = custom_state("1", "96");
    CHECK(session.dirty(state));
    CHECK(session.apply(state, save, issue) == ScaleApplyResult::Saved);
    CHECK_EQ(store.write_log.size(), static_cast<size_t>(1));
    CHECK_EQ(project.pages["42"].scale_factor2, 96.0);
    CHECK_FALSE(session.dirty(state));
    CHECK(page_dialog_context(project, "bid-7", "bid-7", "42", baseline_sf1, baseline_sf2, true).current());
    state = custom_state("1", "192");
    CHECK(session.dirty(state));
    CHECK(session.apply(state, save, issue) == ScaleApplyResult::Saved);
    CHECK_EQ(store.write_log.size(), static_cast<size_t>(2));
    CHECK_EQ(project.pages["42"].scale_factor2, 192.0);
    CHECK_STR_EQ(state.custom2, "192"); // the field keeps what was typed
    CHECK_FALSE(session.dirty(state));
}

// test_set_scale_apply.py:191 test_unrelated_replacement_deletion_navigation_and_permission_loss_reject: after the page is replaced, deleted, another page is active, another
// bid is selected or the permission is lost, Save refuses (the store is not called) and the stored scale stays 96 (if a first Save succeeded) or 48. ADAPTED where iOS differs:
//   replace -> the page now holds ANOTHER pair than the form was opened on (iOS has no object identity), page_current false;
//   page    -> Windows refuses (selection_current false); the iOS menu acts on the PRESSED page, so the active page does not matter: the context stays CURRENT (adapted);
//   permission -> edit_allowed false (iOS: the bid is locked).
TEST(SetScale_FormApply, ContextChangesRejectTheSaveAndNothingIsWritten_Adapted)
{
    for (const bool saved_first : {false, true})
    {
        for (const char *change : {"replace", "delete", "page", "bid", "permission"})
        {
            SCOPED_CONTEXT(string(change) + (saved_first ? " after a first save" : ""));
            FakeStore store;
            store.pages["42"] = {1.0, 48.0};
            ost_data::ProjectData project = project_with_pages(store.pages, {"42"});
            double baseline_sf2 = 48.0;
            string selected_bid = "bid-7";
            bool allowed = true;
            ScaleFormSession session(1.0, 48.0);
            optional<FormIssue> issue;
            auto save = [&](const ScaleSettings &settings) {
                if (!page_dialog_context(project, selected_bid, "bid-7", "42", 1.0, baseline_sf2, allowed).current())
                    return false;
                if (!store.save(settings, page_setting_uids(project, "42", settings.apply_to_all_pages)))
                    return false;
                project.pages["42"].scale_factor2 = settings.scale_factor2;
                baseline_sf2 = settings.scale_factor2;
                return true;
            };
            if (saved_first)
                CHECK(session.apply(custom_state("1", "96"), save, issue) == ScaleApplyResult::Saved);
            if (string(change) == "replace")
                project.pages["42"].scale_factor2 = 24.0; // replaced under the dialog by a page with another scale
            else if (string(change) == "delete")
                project.pages.erase("42");
            else if (string(change) == "bid")
                selected_bid = "bid-9";
            else if (string(change) == "permission")
                allowed = false;
            const PageDialogContext context = page_dialog_context(project, selected_bid, "bid-7", "42", 1.0, baseline_sf2, allowed);
            const ScaleApplyResult result = session.apply(custom_state("1", "192"), save, issue);
            if (string(change) == "page")
            {
                // adapted: another ACTIVE page is not a reason to refuse, the pressed page is still the same page
                CHECK(context.current());
                CHECK(result == ScaleApplyResult::Saved);
                CHECK_EQ(store.pages["42"].second, 192.0);
                CHECK_EQ(store.write_log.size(), static_cast<size_t>(saved_first ? 2 : 1));
                continue;
            }
            CHECK_FALSE(context.current());
            CHECK(result == ScaleApplyResult::SaveFailed);
            REQUIRE(issue.has_value());
            CHECK_STR_EQ(issue->title, "Save Failed");
            CHECK_STR_EQ(issue->message, "Failed to save page scale.");
            CHECK_EQ(store.write_log.size(), static_cast<size_t>(saved_first ? 1 : 0));
            CHECK_EQ(store.pages["42"].second, saved_first ? 96.0 : 48.0);
            if (string(change) == "replace")
            {
                CHECK_FALSE(context.page_current);
                CHECK(context.page_present);
            }
            else if (string(change) == "delete")
                CHECK_FALSE(context.page_present);
            else if (string(change) == "bid")
                CHECK_FALSE(context.selection_current);
            else
                CHECK_FALSE(context.edit_allowed);
        }
    }
}

// test_set_scale_apply.py:239 test_failed_write_and_exception_keep_original_owner_for_retry (the pinned texts: :263 and tests/presentation/dialogs/test_set_scale_dialog.py:49-50): a write that answers false AND a write that throws both give the warning
// "Save Failed" / "Failed to save page scale.", leave the page and the baseline alone, keep the form dirty (Save stays possible); a retry then succeeds.
TEST(SetScale_FormApply, AFailedWriteOrAnExceptionKeepsTheFormDirtyForARetry)
{
    for (const bool throws : {false, true})
    {
        SCOPED_CONTEXT(throws ? "the write throws" : "the write answers false");
        FakeStore store;
        store.pages["42"] = {1.0, 48.0};
        ScaleFormSession session(1.0, 48.0);
        optional<FormIssue> issue;
        auto save = [&](const ScaleSettings &settings) { return store.save(settings, {"42"}); };
        store.fail = !throws;
        store.throw_instead = throws;
        const ScaleFormState state = custom_state("1", "96");
        CHECK(session.apply(state, save, issue) == ScaleApplyResult::SaveFailed);
        REQUIRE(issue.has_value());
        CHECK_STR_EQ(issue->title, "Save Failed");
        CHECK_STR_EQ(issue->message, "Failed to save page scale.");
        CHECK_EQ(store.pages["42"].second, 48.0);
        CHECK(session.dirty(state)); // still dirty: a retry is possible
        store.fail = false;
        store.throw_instead = false;
        CHECK(session.apply(state, save, issue) == ScaleApplyResult::Saved); // the retry writes
        CHECK_EQ(store.write_log.size(), static_cast<size_t>(1));
        CHECK(session.apply(custom_state("1", "192"), save, issue) == ScaleApplyResult::Saved);
        CHECK_EQ(store.write_log.size(), static_cast<size_t>(2));
        CHECK_EQ(store.pages["42"].second, 192.0);
    }
}

// test_set_scale_apply.py:293 test_apply_all_then_single_page_save_uses_replacement: pages 42 (1 : 48) and 43 (1 : 24); apply-all (1 : 96) writes ["42", "43"], then
// a single-page save (1 : 192) writes ["42"]; page 43 stays 96.
TEST(SetScale_FormApply, ApplyAllThenSinglePageSaveWritesTheRightPages)
{
    FakeStore store;
    store.pages["42"] = {1.0, 48.0};
    store.pages["43"] = {1.0, 24.0};
    ost_data::ProjectData project = project_with_pages(store.pages, {"42", "43"});
    ScaleFormSession session(1.0, 48.0);
    optional<FormIssue> issue;
    auto save = [&](const ScaleSettings &settings) { return store.save(settings, page_setting_uids(project, "42", settings.apply_to_all_pages)); };
    CHECK(session.apply(custom_state("1", "96", true), save, issue) == ScaleApplyResult::Saved);
    CHECK_EQ(store.write_log, vector<string>({"42,43"}));
    REQUIRE(!store.settings.empty());
    CHECK(store.settings.back().apply_to_all_pages);
    CHECK(session.apply(custom_state("1", "192", false), save, issue) == ScaleApplyResult::Saved);
    CHECK_EQ(store.write_log, vector<string>({"42,43", "42"}));
    CHECK_EQ(store.pages["42"].second, 192.0);
    CHECK_EQ(store.pages["43"].second, 96.0);
}

// test_set_scale_apply.py:313 test_unchanged_and_cancelled_drafts_do_not_write: Save on an untouched form writes nothing; Save twice after one change writes once; a second change
// that is cancelled (no Save) leaves 96 and one write.
TEST(SetScale_FormApply, UnchangedAndCancelledDraftsDoNotWrite)
{
    FakeStore store;
    store.pages["42"] = {1.0, 48.0};
    ScaleFormSession session(1.0, 48.0);
    optional<FormIssue> issue;
    int calls = 0;
    auto save = [&](const ScaleSettings &settings) {
        ++calls;
        return store.save(settings, {"42"});
    };
    CHECK(session.apply(session.initial(), save, issue) == ScaleApplyResult::NothingToSave);
    CHECK_EQ(calls, 0);
    CHECK(store.write_log.empty());
    const ScaleFormState changed = custom_state("1", "96");
    CHECK(session.apply(changed, save, issue) == ScaleApplyResult::Saved);
    CHECK(session.apply(changed, save, issue) == ScaleApplyResult::NothingToSave); // the second Save of the same state: not dirty
    CHECK_EQ(store.write_log.size(), static_cast<size_t>(1));
    const ScaleFormState cancelled = custom_state("1", "192");
    CHECK(session.dirty(cancelled)); // an edit that is then dropped (Cancel): nothing calls save
    CHECK_EQ(store.pages["42"].second, 96.0);
    CHECK_EQ(store.write_log.size(), static_cast<size_t>(1));
    CHECK_EQ(calls, 1);
}

// set_scale_dialog.py:264-308: an invalid dirty form does not call the store and does not move the baseline; the alert is the validation alert.
TEST(SetScale_FormApply, AnInvalidFormIsRefusedBeforeTheStoreIsCalled)
{
    FakeStore store;
    store.pages["42"] = {1.0, 48.0};
    ScaleFormSession session(1.0, 48.0);
    optional<FormIssue> issue;
    int calls = 0;
    auto save = [&](const ScaleSettings &settings) {
        ++calls;
        return store.save(settings, {"42"});
    };
    int focus = -1;
    CHECK(session.apply(custom_state("1", "abc"), save, issue, &focus) == ScaleApplyResult::Invalid);
    REQUIRE(issue.has_value());
    CHECK_STR_EQ(issue->title, "Invalid Scale");
    CHECK_STR_EQ(issue->message, "The second custom scale value \"abc\" is not a valid number.");
    CHECK_EQ(focus, 2);
    CHECK_EQ(calls, 0);
    CHECK(session.dirty(custom_state("1", "abc"))); // the baseline did not move
    CHECK(session.apply(custom_state("nan", "1"), save, issue) == ScaleApplyResult::Invalid); // SS-3
    CHECK(session.apply(custom_state("1", "-4"), save, issue) == ScaleApplyResult::Invalid);
    CHECK_EQ(calls, 0);
}

// ---- the page set of "Apply to all pages" (ui_event_coordinator.py:5390-5399) -------------------------------------------------------------------

// _page_setting_uids: one page, or every page of the page bar order that exists; a uid that is no page and an empty uid are dropped; no order is {} (Windows: no sidebar).
TEST(SetScale_FormApply, ThePageSetOfApplyAllIsThePageBarOrder)
{
    const ost_data::ProjectData project = project_with_pages({{"42", {1, 48}}, {"43", {1, 24}}, {"44", {1, 12}}}, {"43", "99", "42", "", "44"});
    CHECK_EQ(page_setting_uids(project, "42", false), vector<string>({"42"}));
    CHECK_EQ(page_setting_uids(project, "42", true), vector<string>({"43", "42", "44"}));
    const ost_data::ProjectData without_order = project_with_pages({{"42", {1, 48}}}, {});
    CHECK_EQ(page_setting_uids(without_order, "42", true), vector<string>());
    CHECK_EQ(page_setting_uids(without_order, "42", false), vector<string>({"42"}));
}

// ---- group L: tests/presentation/coordinators/test_ui_event_coordinator.py (the context rules) -----------------------------------------------

// test_ui_event_coordinator.py:12565 test_mdb_set_scale_rejects_dialog_save_after_bid_context_changes: another selected bid is not the dialog's bid: not current.
TEST(SetScale_FormContext, ASaveAfterTheBidContextChangedIsRejected)
{
    const ost_data::ProjectData project = project_with_pages({{"page-1", {1.0, 1.0}}}, {"page-1"});
    const PageDialogContext context = page_dialog_context(project, "bid-2", "bid-1", "page-1", 1.0, 1.0, true);
    CHECK_FALSE(context.current());
    CHECK_FALSE(context.selection_current);
    CHECK(context.page_present);
    CHECK(context.page_current);
    CHECK(page_dialog_context(project, "bid-1", "bid-1", "page-1", 1.0, 1.0, true).current()); // the control
}

// test_ui_event_coordinator.py:12594 test_mdb_set_scale_saves_only_while_the_dialog_context_is_current: the five cases. ADAPTED: other_active_page (Windows: refused)
// stays CURRENT on iOS (the page menu acts on the pressed page, not the active one); replaced_page is "the page holds another scale pair than the form was opened on".
// Only the current case writes: once for the one-page save and once for the "apply to all" save of both pages.
TEST(SetScale_FormContext, TheSaveRunsOnlyWhileTheDialogContextIsCurrent_Adapted)
{
    const struct
    {
        const char *name;
        string selected_bid;
        bool replaced;
        bool allowed;
        bool expect_current;
    } cases[] = {
        {"current", "bid-1", false, true, true},
        {"other_bid", "bid-2", false, true, false},
        {"other_active_page (adapted: not a reason to refuse)", "bid-1", false, true, true},
        {"replaced_page", "bid-1", true, true, false},
        {"access_revoked", "bid-1", false, false, false},
    };
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT(c.name);
        ost_data::ProjectData project = project_with_pages({{"page-1", {1.0, 1.0}}, {"page-2", {1.0, 2.0}}}, {"page-1", "page-2"});
        if (c.replaced)
            project.pages["page-1"].scale_factor2 = 5.0;
        FakeStore store;
        store.pages = {{"page-1", {1.0, 1.0}}, {"page-2", {1.0, 2.0}}};
        auto save = [&](const ScaleSettings &settings) {
            if (!page_dialog_context(project, c.selected_bid, "bid-1", "page-1", 1.0, 1.0, c.allowed).current())
                return false;
            return store.save(settings, page_setting_uids(project, "page-1", settings.apply_to_all_pages));
        };
        const bool saved_one = save(ScaleSettings{1.0, 48.0, false});
        const bool saved_all = save(ScaleSettings{2.0, 24.0, true});
        CHECK_EQ(saved_one, c.expect_current);
        CHECK_EQ(saved_all, c.expect_current);
        if (c.expect_current)
            CHECK_EQ(store.write_log, vector<string>({"page-1", "page-1,page-2"}));
        else
            CHECK(store.write_log.empty());
    }
}

// test_ui_event_coordinator.py:30469 test_set_scale_dialog_rejects_inconsistent_file_context and :30445 test_dialog_is_not_opened_without_context_rights_or_page (by intent):
// the form opens only for a page that exists, in a selected bid, with the right to edit: no page uid, no bid, no right and a missing page are all "not current".
// (The Windows "file" of a database does not exist on iOS: there is one database; the explicit-file / explicit-page / active-page defaults of :30487 and :30503 are
// the wiring of the page menu and are not portable.)
TEST(SetScale_FormContext, TheFormOpensOnlyForAnExistingPageOfTheSelectedBidWithTheRightToEdit)
{
    const ost_data::ProjectData project = project_with_pages({{"p1", {0.25, 12.0}}}, {"p1"});
    CHECK(page_dialog_context(project, "bid-8", "bid-8", "p1", 0.25, 12.0, true).current()); // the control
    CHECK_FALSE(page_dialog_context(project, "bid-8", "bid-8", "", 0.25, 12.0, true).current());      // no page uid
    CHECK_FALSE(page_dialog_context(project, "", "", "p1", 0.25, 12.0, true).current());                // no bid (an empty selection is not a selection)
    CHECK_FALSE(page_dialog_context(project, "", "bid-8", "p1", 0.25, 12.0, true).current());
    CHECK_FALSE(page_dialog_context(project, "bid-8", "bid-8", "p1", 0.25, 12.0, false).current());     // no right to edit
    CHECK_FALSE(page_dialog_context(project, "bid-8", "bid-8", "p2", 0.25, 12.0, true).current());      // the page is missing
    CHECK_FALSE(page_dialog_context(ost_data::ProjectData(), "bid-8", "bid-8", "p1", 0.25, 12.0, true).current());
}

// The flags of a refused save (the Windows warning prints them): each cause turns exactly its own flag off.
TEST(SetScale_FormContext, EachCauseTurnsExactlyItsOwnFlagOff)
{
    const ost_data::ProjectData project = project_with_pages({{"p1", {0.25, 12.0}}}, {"p1"});
    PageDialogContext all = page_dialog_context(project, "b", "b", "p1", 0.25, 12.0, true);
    CHECK(all.selection_current && all.page_present && all.page_current && all.edit_allowed);
    all = page_dialog_context(project, "x", "b", "p1", 0.25, 12.0, true);
    CHECK(!all.selection_current && all.page_present && all.page_current && all.edit_allowed);
    all = page_dialog_context(project, "b", "b", "missing", 0.25, 12.0, true);
    CHECK(all.selection_current && !all.page_present && !all.page_current && all.edit_allowed);
    all = page_dialog_context(project, "b", "b", "p1", 0.5, 12.0, true);
    CHECK(all.selection_current && all.page_present && !all.page_current && all.edit_allowed);
    all = page_dialog_context(project, "b", "b", "p1", 0.25, 12.0 + 1e-12, true); // bit for bit: even a tiny difference is "not the same page"
    CHECK(!all.page_current);
    all = page_dialog_context(project, "b", "b", "p1", 0.25, 12.0, false);
    CHECK(all.selection_current && all.page_present && all.page_current && !all.edit_allowed);
}
