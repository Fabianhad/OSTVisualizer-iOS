// Layer: Domain (pure C++)
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The rules of the Set Scale form (plan 5.4): what a typed pair means, which alert a refused value shows, the dirty / baseline state
// of the form and the "is the page I was opened on still the same" check. Windows source of truth:
//   ost_visualizer/presentation/dialogs/set_scale_dialog.py:10-357      the dialog: states, texts, validation, apply / OK, dirty tracking
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5381-5409   _page_setting_uids, _page_dialog_context_is_current
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5493-5643   open_set_scale_dialog (the save closure), _save_scale_settings
// Deliberate differences from Windows, each pinned by a test that says so (register SS-3, SS-5, SS-14; N-M1-21, N-M1-22):
//   - a NaN or an infinity typed in a custom field is refused with "... is not a valid number." (Windows lets nan / inf through the
//     dialog and refuses them later with the generic "Save Failed"), the stripped text is shown;
//   - (retired by Windows 0a24294, now PARITY: set_scale_dialog.py:415-418 _format_number) the custom fields are filled with the shortest round-trip text, Python
//     repr(float(v)) without a trailing ".0" (1234567, 12.0000001, 0.3333333333333333), instead of f"{v:g}" (six digits), so that ticking only
//     "Apply to all pages" does not rescale the page by the truncation error;
//   - the form has no Apply button (Cancel + Save only), so apply() below is "Save";
//   - the page the form was opened on is the PRESSED page, not necessarily the active one: the Windows clause "same active page" of
//     _page_dialog_context_is_current is dropped (the plan's touch adaptation), and "the page object is the same" becomes "the page
//     still holds the scale pair the form was opened on".
//
// STATE: implemented (PageScaleRules.cpp); the host tests of Config/Tests/Formatting pin it.
#pragma once

#include "ConditionFormRules.hpp" // FormIssue
#include "PageScale.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ost_data
{
    struct ProjectData;
}

namespace ost_domain
{
    // The texts (set_scale_dialog.py:15, :220, :242, :252, :258, :284, :303). Every alert of the dialog is title + message.
    inline constexpr const char *kInvalidScaleTitle = "Invalid Scale";
    inline constexpr const char *kScaleSelectPredefinedMessage = "Select a predefined scale.";
    inline constexpr const char *kScaleCustomEmptyMessage = "Custom scale values cannot be empty.";
    inline constexpr const char *kScaleCustomPositiveMessage = "Custom scale values must be positive.";
    inline constexpr const char *kScaleSaveFailedTitle = "Save Failed";
    inline constexpr const char *kScaleSaveFailedMessage = "Failed to save page scale.";
    // shared/texts.py:1056-1060 PAGE_OPERATIONS_MESSAGE_THE_SCALE_WAS_NOT_CHANGED_BECAUSE, shown by set_scale_dialog.py:351-358 under kScaleSaveFailedTitle when the save is refused
    // because the content of the page cannot be stored at the new scale (a PageScaleRangeError; Windows 0a24294).
    inline constexpr const char *kScaleRangeRefusedMessage =
        "The scale was not changed because the takeoffs and annotations on the page cannot be stored at that scale.";

    // The pair a Save writes and whether it goes to every page of the bid (set_scale_dialog.py:18-22).
    struct ScaleSettings
    {
        double scale_factor1 = 0.0;
        double scale_factor2 = 0.0;
        bool apply_to_all_pages = false;
    };

    enum class ScaleInputMode
    {
        Predefined,
        Custom
    };

    // The tuple of _current_form_state (:197-205): the style (1, 2, 3), the "Pre-defined" radio, the pair of the preset the picker shows
    // (has_preset false = the picker has no item), the two custom texts as typed, the "Apply to all pages" switch. Windows compares the
    // custom texts after strip(): dirty() does the same.
    struct ScaleFormState
    {
        int style = kScaleStyleArchitectural;
        ScaleInputMode mode = ScaleInputMode::Predefined;
        bool has_preset = false;
        double preset_sf1 = 0.0;
        double preset_sf2 = 0.0;
        std::string custom1;
        std::string custom2;
        bool apply_to_all = false;
    };

    // :236-262 for ONE custom field. `which` is "first" or "second". text.strip() is read as Python float(): empty -> kScaleCustomEmptyMessage,
    // not a float (or NaN / +-infinity, the SS-3 hardening) -> `The <which> custom scale value "<stripped text>" is not a valid number.`,
    // a value <= 0 -> kScaleCustomPositiveMessage; all under kInvalidScaleTitle. nullopt and `out` set on success, `out` untouched otherwise.
    // The checks run in this order (a non-finite value is refused before the sign is looked at: "-inf" is "not a valid number").
    std::optional<FormIssue> parse_custom_scale_value(const std::string &text, const char *which, double &out);

    // :216-234. Predefined: the preset pair of the state (none: kScaleSelectPredefinedMessage). Custom: the FIRST field is parsed completely
    // (empty, number, sign) before the SECOND is looked at. `focus_field` (optional) receives the field the alert focuses: 0 none, 1 the first
    // custom field, 2 the second (the Windows edit.setFocus()). `out` is untouched on a refusal.
    std::optional<FormIssue> validate_scale_form(const ScaleFormState &state, ScaleSettings &out, int *focus_field = nullptr);

    // The user changed the Scale Style (:167-189): the picker lists the presets of `new_style`, keeps the previously selected pair when the
    // new list has the same (sf1, sf2) (exact comparison of the pairs), else shows the first item of the list; the "Pre-defined" radio is
    // turned on; the custom texts and the switch are kept. A style without presets leaves has_preset false.
    ScaleFormState scale_form_after_style_change(const ScaleFormState &state, int new_style);

    // The alert of a refused save: kScaleSaveFailedTitle with kScaleRangeRefusedMessage when the content of the page cannot be stored at the new scale
    // (`unstorable_content`, ost_store::page_scale_refusal_is_unstorable_content), else kScaleSaveFailedMessage (set_scale_dialog.py:351-372).
    FormIssue scale_save_failed_issue(bool unstorable_content);

    // What apply() did (the Windows _apply_changes, :264-308, without the SQL queue branch).
    enum class ScaleApplyResult
    {
        NothingToSave, // not dirty: True without validating or saving (:265-266)
        Invalid,       // validation refused: `issue` is the alert, the form stays open
        SaveFailed,    // the save callback answered false: `issue` is kScaleSaveFailedTitle / kScaleSaveFailedMessage, the form stays dirty
        Saved          // saved: the baseline is now the state, the form is no longer dirty
    };

    // The state machine behind the Windows dialog: the baseline (_saved_form_state), the dirty flag and the apply flow.
    class ScaleFormSession
    {
    public:
        // The page's pair as the dialog reads it: float(sf or 1.0) for each (a 0 or -0 becomes 1.0, a NaN stays NaN) (:38-39).
        ScaleFormSession(double page_sf1, double page_sf2);

        // The state at open (:134-156). The first preset of the styles 1, 2, 3 (find_matching_preset) that matches the page: that style, the
        // "Pre-defined" radio, that preset; no match: style 1, the "Custom" radio and the first preset of style 1 as the picker's value. The
        // two custom texts are the shortest round-trip text of the value (repr without a trailing ".0") in BOTH cases. apply_to_all is false. This state is the baseline.
        ScaleFormState initial() const;

        // state != baseline, the custom texts compared after strip() (:207-211).
        bool dirty(const ScaleFormState &state) const;

        // baseline := state (after a successful save).
        void mark_saved(const ScaleFormState &state);

        // Save: not dirty -> NothingToSave without validating or calling `save`. Else validate (Invalid on a refusal: `save` is not called),
        // then `save(settings)`: false -> SaveFailed, true -> mark_saved(state) and Saved. An exception thrown by `save` counts as false (the Windows save
        // closure logs it and answers False, ui_event_coordinator.py:5526-5534): it never escapes apply(), the form stays dirty. `issue` is written
        // only for Invalid / SaveFailed.
        ScaleApplyResult apply(const ScaleFormState &state, const std::function<bool(const ScaleSettings &)> &save,
                               std::optional<FormIssue> &issue, int *focus_field = nullptr);

    private:
        ScaleFormState _opened;   // the state at open (initial() keeps answering it after a save)
        ScaleFormState _baseline; // the saved form state the dirty flag compares with
    };

    // ui_event_coordinator.py:5390-5399 _page_setting_uids: the pages a Save writes. Not apply_to_all: {page_uid}. Apply to all: every uid of
    // `project.page_order` (the pages of the bid in page-bar order) that is not empty and is a page of the project, in that order, once each; an empty
    // page_order (Windows: no sidebar) gives {}. The pressed page itself is not added when it is missing from the order.
    std::vector<std::string> page_setting_uids(const ost_data::ProjectData &project, const std::string &page_uid, bool apply_to_all);

    // The Windows flags of a refused save (ui_event_coordinator.py:5535-5551 logs them) and the predicate
    // _page_dialog_context_is_current (:5401-5409), adapted to the pressed page (see the file comment):
    //   selection_current  `selected_bid_uid` is not empty and equals `dialog_bid_uid` (the bid the form was opened for)
    //   page_present       `page_uid` is a page of the project
    //   page_current       that page still holds EXACTLY (bit for bit) the pair the form was opened on (baseline_sf1, baseline_sf2)
    //   edit_allowed       the caller's answer to "may this bid be edited" (false when the bid is locked)
    // `current()` is true only when all four are. An empty `page_uid` is no page.
    struct PageDialogContext
    {
        bool selection_current = false;
        bool page_present = false;
        bool page_current = false;
        bool edit_allowed = false;
        bool current() const
        {
            return selection_current && page_present && page_current && edit_allowed;
        }
    };
    PageDialogContext page_dialog_context(const ost_data::ProjectData &project, const std::string &selected_bid_uid,
                                          const std::string &dialog_bid_uid, const std::string &page_uid, double baseline_sf1,
                                          double baseline_sf2, bool edit_allowed);
}
