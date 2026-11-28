// Layer: Domain (pure C++). What the app decides around the Set Scale form: see PageScaleApply.hpp.
#include "PageScaleApply.hpp"

#include <string>

namespace ost_domain
{
    namespace
    {
        // The explanation of the two numbers (an iOS-only text, plan 4.14): Windows has none. The example pairs are the first Architectural preset family and a metric one.
        constexpr const char *kPairExplanation = "Drawing length : real length, in the same unit, for example 0.125 : 12 for 1/8\" = 1' 0\", or 1 : 100 for metric.";
    }

    std::optional<ScaleConfirmation> scale_apply_all_confirmation(std::size_t page_count)
    {
        if (page_count <= 1)
            return std::nullopt;
        ScaleConfirmation confirmation;
        confirmation.title = "Apply to All Pages";
        confirmation.message = "Set the scale of " + std::to_string(page_count) + " pages? Takeoffs on those pages are rescaled and quantities change.";
        confirmation.confirm_title = "Set Scale";
        confirmation.cancel_title = "Cancel";
        return confirmation;
    }

    std::string scale_form_footer_text(const std::string &page_name, double sf1, double sf2)
    {
        const std::string label = scale_display_label(sf1, sf2);
        std::string first = "Current scale of ";
        first += page_name.empty() ? std::string("this page") : "\"" + page_name + "\"";
        first += ": ";
        first += label.empty() ? std::string("not set") : label;
        return first + "\n" + kPairExplanation;
    }

    int scale_form_preset_index(const ScaleFormState &state)
    {
        if (!state.has_preset)
            return -1;
        const std::vector<ScalePreset> &presets = scale_presets(state.style);
        for (std::size_t i = 0; i < presets.size(); ++i)
        {
            if (presets[i].sf1 == state.preset_sf1 && presets[i].sf2 == state.preset_sf2)
                return static_cast<int>(i);
        }
        return -1;
    }

    ScaleFormState scale_form_after_preset_pick(const ScaleFormState &state, int index)
    {
        const std::vector<ScalePreset> &presets = scale_presets(state.style);
        if (index < 0 || static_cast<std::size_t>(index) >= presets.size())
            return state;
        ScaleFormState picked = state;
        picked.mode = ScaleInputMode::Predefined;
        picked.has_preset = true;
        picked.preset_sf1 = presets[static_cast<std::size_t>(index)].sf1;
        picked.preset_sf2 = presets[static_cast<std::size_t>(index)].sf2;
        return picked;
    }

    PageScaleRefresh plan_page_scale_refresh(const std::vector<std::string> &changed_page_uids, const std::string &active_2d_page_uid,
                                             const std::set<std::string> &selected_3d_page_uids, bool is_2d_mode)
    {
        bool active_changed = false;
        bool selected_changed = false;
        for (const std::string &uid : changed_page_uids)
        {
            if (uid.empty())
                continue;
            if (uid == active_2d_page_uid)
                active_changed = true;
            if (selected_3d_page_uids.count(uid) != 0)
                selected_changed = true;
        }
        PageScaleRefresh refresh;
        refresh.reload_plan_view = is_2d_mode && active_changed;
        refresh.regenerate_meshes = selected_changed;
        refresh.refresh_quantities = active_changed || selected_changed;
        return refresh;
    }
}
