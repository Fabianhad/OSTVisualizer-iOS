// Layer: Domain. See PageScaleRules.hpp.
// Windows source of truth: ost_visualizer/presentation/dialogs/set_scale_dialog.py:10-357 (the dialog), presentation/coordinators/ui_event_coordinator.py:5381-5409.
#include "PageScaleRules.hpp"

#include "../Data/ProjectData.hpp"
#include "../Import/TextRules.hpp"
#include "NumberText.hpp"
#include "PositionParser.hpp"

#include <cmath>
#include <unordered_set>

namespace ost_domain
{
    namespace
    {
        // set_scale_dialog.py:38-39 float(sf or 1.0): a 0 (and a -0) is falsy and becomes 1.0, a NaN is truthy and stays NaN.
        double page_value_or_one(double value)
        {
            return value == 0.0 ? 1.0 : value;
        }

        // Python tuple equality of the form state (set_scale_dialog.py:197-205): == on the floats, the custom texts after strip().
        bool same_state(const ScaleFormState &a, const ScaleFormState &b)
        {
            if (a.style != b.style || a.mode != b.mode || a.has_preset != b.has_preset || a.apply_to_all != b.apply_to_all)
                return false;
            if (a.has_preset && (!(a.preset_sf1 == b.preset_sf1) || !(a.preset_sf2 == b.preset_sf2)))
                return false;
            return ost_import::text_rules::py_strip(a.custom1) == ost_import::text_rules::py_strip(b.custom1) &&
                   ost_import::text_rules::py_strip(a.custom2) == ost_import::text_rules::py_strip(b.custom2);
        }

        FormIssue invalid_scale(const std::string &message)
        {
            return FormIssue{kInvalidScaleTitle, message};
        }
    }

    std::optional<FormIssue> parse_custom_scale_value(const std::string &text, const char *which, double &out)
    {
        const std::string stripped = ost_import::text_rules::py_strip(text);
        if (stripped.empty())
            return invalid_scale(kScaleCustomEmptyMessage);
        double value = 0.0;
        // float(text) raising ValueError, and the SS-3 hardening: nan and the infinities (also "1e400", which float() reads as inf) are not valid numbers
        // here, and are refused BEFORE the sign is looked at ("-inf" is "not a valid number", not "must be positive").
        if (!parse_python_float(stripped, value) || !std::isfinite(value))
            return invalid_scale(std::string("The ") + which + " custom scale value \"" + stripped + "\" is not a valid number.");
        if (value <= 0.0)
            return invalid_scale(kScaleCustomPositiveMessage);
        out = value;
        return std::nullopt;
    }

    std::optional<FormIssue> validate_scale_form(const ScaleFormState &state, ScaleSettings &out, int *focus_field)
    {
        if (focus_field)
            *focus_field = 0;
        double sf1 = 0.0;
        double sf2 = 0.0;
        if (state.mode == ScaleInputMode::Predefined)
        {
            if (!state.has_preset)
                return invalid_scale(kScaleSelectPredefinedMessage);
            sf1 = state.preset_sf1;
            sf2 = state.preset_sf2;
        }
        else
        {
            if (std::optional<FormIssue> issue = parse_custom_scale_value(state.custom1, "first", sf1))
            {
                if (focus_field)
                    *focus_field = 1;
                return issue;
            }
            if (std::optional<FormIssue> issue = parse_custom_scale_value(state.custom2, "second", sf2))
            {
                if (focus_field)
                    *focus_field = 2;
                return issue;
            }
        }
        out.scale_factor1 = sf1;
        out.scale_factor2 = sf2;
        out.apply_to_all_pages = state.apply_to_all;
        return std::nullopt;
    }

    ScaleFormState scale_form_after_style_change(const ScaleFormState &state, int new_style)
    {
        ScaleFormState next = state;
        next.style = new_style;
        next.mode = ScaleInputMode::Predefined;
        const std::vector<ScalePreset> &list = scale_presets(new_style);
        if (list.empty())
        {
            next.has_preset = false;
            return next;
        }
        // _load_predefined_scales (:167-178): the previously selected pair stays selected when the new list has it (the first equal item), else the first item.
        size_t chosen = 0;
        if (state.has_preset)
        {
            for (size_t index = 0; index < list.size(); ++index)
            {
                if (list[index].sf1 == state.preset_sf1 && list[index].sf2 == state.preset_sf2)
                {
                    chosen = index;
                    break;
                }
            }
        }
        next.has_preset = true;
        next.preset_sf1 = list[chosen].sf1;
        next.preset_sf2 = list[chosen].sf2;
        return next;
    }

    ScaleFormSession::ScaleFormSession(double page_sf1, double page_sf2)
    {
        const double sf1 = page_value_or_one(page_sf1);
        const double sf2 = page_value_or_one(page_sf2);
        const PresetMatch match = find_matching_preset(sf1, sf2);
        ScaleFormState state;
        state.style = match.style;
        if (match.index >= 0)
        {
            const ScalePreset &preset = scale_presets(match.style)[static_cast<size_t>(match.index)];
            state.mode = ScaleInputMode::Predefined;
            state.has_preset = true;
            state.preset_sf1 = preset.sf1;
            state.preset_sf2 = preset.sf2;
        }
        else
        {
            // No preset matches: style 1, the Custom radio, and the first preset of style 1 as the picker's value (the combo's first item).
            const ScalePreset &first = scale_presets(kScaleStyleArchitectural).front();
            state.style = kScaleStyleArchitectural;
            state.mode = ScaleInputMode::Custom;
            state.has_preset = true;
            state.preset_sf1 = first.sf1;
            state.preset_sf2 = first.sf2;
        }
        // SS-5: full precision (the same digits as format_custom_scale), not f"{value:g}".
        state.custom1 = format_general(sf1, 15);
        state.custom2 = format_general(sf2, 15);
        state.apply_to_all = false;
        _opened = state;
        _baseline = state;
    }

    ScaleFormState ScaleFormSession::initial() const
    {
        return _opened;
    }

    bool ScaleFormSession::dirty(const ScaleFormState &state) const
    {
        return !same_state(state, _baseline);
    }

    void ScaleFormSession::mark_saved(const ScaleFormState &state)
    {
        _baseline = state;
    }

    ScaleApplyResult ScaleFormSession::apply(const ScaleFormState &state, const std::function<bool(const ScaleSettings &)> &save,
                                             std::optional<FormIssue> &issue, int *focus_field)
    {
        if (focus_field)
            *focus_field = 0;
        if (!dirty(state))
            return ScaleApplyResult::NothingToSave; // :265-266
        ScaleSettings settings;
        if (std::optional<FormIssue> refused = validate_scale_form(state, settings, focus_field))
        {
            issue = refused;
            return ScaleApplyResult::Invalid;
        }
        bool saved = false;
        try
        {
            saved = save(settings);
        }
        catch (...)
        {
            saved = false; // the Windows save closure logs the exception and answers False (ui_event_coordinator.py:5526-5534)
        }
        if (!saved)
        {
            issue = FormIssue{kScaleSaveFailedTitle, kScaleSaveFailedMessage};
            return ScaleApplyResult::SaveFailed;
        }
        mark_saved(state);
        return ScaleApplyResult::Saved;
    }

    std::vector<std::string> page_setting_uids(const ost_data::ProjectData &project, const std::string &page_uid, bool apply_to_all)
    {
        if (!apply_to_all)
            return {page_uid};
        std::vector<std::string> uids;
        std::unordered_set<std::string> seen;
        for (const std::string &uid : project.page_order)
        {
            if (uid.empty() || project.pages.find(uid) == project.pages.end())
                continue;
            if (seen.insert(uid).second)
                uids.push_back(uid);
        }
        return uids;
    }

    PageDialogContext page_dialog_context(const ost_data::ProjectData &project, const std::string &selected_bid_uid, const std::string &dialog_bid_uid,
                                          const std::string &page_uid, double baseline_sf1, double baseline_sf2, bool edit_allowed)
    {
        PageDialogContext context;
        context.selection_current = !selected_bid_uid.empty() && selected_bid_uid == dialog_bid_uid;
        const auto found = page_uid.empty() ? project.pages.end() : project.pages.find(page_uid);
        context.page_present = found != project.pages.end();
        // Bit for bit: the page still holds EXACTLY the pair the form was opened on (a NaN pair never matches itself, which is the safe side).
        context.page_current = context.page_present && found->second.scale_factor1 == baseline_sf1 && found->second.scale_factor2 == baseline_sf2;
        context.edit_allowed = edit_allowed;
        return context;
    }
}
