// Layer: Domain (pure C++)
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// What the app decides around the Set Scale form (plan 5.4, touch adaptation; the Objective-C layer only shows what these functions answer):
//   - the confirmation before an "Apply to all pages" save (N-M1-21: one confirmation only when more than one page is affected; Windows asks nothing,
//     ui_event_coordinator.py:5597-5643 _save_scale_settings, so the wording is an iOS-only text, plan 4.14, N-M1-03);
//   - the footer of the form (the current scale of the page with the label rule of the Windows page bar, page_settings_bar.py:159-206, plus the
//     explanation of the pair, which Windows does not have);
//   - the row of the Pre-defined preset menu that a form state shows, and the form state after a row is picked (set_scale_dialog.py:167-189);
//   - what refreshes after a save (plan 4.11, Windows ui_event_coordinator.py:3318-3359 _on_page_metadata_changed): the plan view of the active 2D page
//     (zoom kept), the 3D scene when a selected page changed, the quantities when the active or a selected page changed.
// Deliberate differences from Windows, each pinned by a test that says so:
//   - the plan view is reloaded only while the 2D view is open: entering the 2D view reloads the page anyway, so a reload into a hidden plan view would
//     do nothing the next entry does not do (Windows has one viewer and no mode).
//
// STATE: implemented (PageScaleApply.cpp); the host tests of Config/Tests/Formatting (test_page_scale_apply.cpp) pin it.
#pragma once

#include "PageScaleRules.hpp"

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ost_domain
{
    // The alert that stands between a Save with "Apply to all pages" and the write when the write reaches more than one page (N-M1-21). The Windows
    // dialog asks nothing; the change rewrites every coordinate of every page and has no undo, so the iOS form asks once. Cancel keeps the form open.
    struct ScaleConfirmation
    {
        std::string title;
        std::string message;
        std::string confirm_title;
        std::string cancel_title;
    };

    // nullopt for 0 or 1 page (nothing to confirm: a single page saves at once like Windows). Else title "Apply to All Pages", message
    // `Set the scale of <N> pages? Takeoffs on those pages are rescaled and quantities change.`, buttons "Set Scale" and "Cancel".
    std::optional<ScaleConfirmation> scale_apply_all_confirmation(std::size_t page_count);

    // The footer under the scale section of the form: line 1 the current scale of the page, line 2 what the two numbers mean, joined by a line feed.
    //   line 1  `Current scale of "<page name>": <label>`   label = scale_display_label(sf1, sf2), the preset label or the custom label;
    //           `Current scale of "<page name>": not set`   when the pair has no label (not finite, or <= 0: nothing is selected on the Windows bar);
    //           an empty page name reads `this page` without quotes.
    //   line 2  `Drawing length : real length, in the same unit, for example 0.125 : 12 for 1/8" = 1' 0", or 1 : 100 for metric.`
    // `sf1` / `sf2` are the pair the page stores (the form opens on it).
    std::string scale_form_footer_text(const std::string &page_name, double sf1, double sf2);

    // The row of scale_presets(state.style) that the Pre-defined preset menu marks for this state: the first preset whose pair equals the state's preset pair
    // exactly (the state takes the pair from the list, so an exact comparison finds it); -1 when the state has no preset (has_preset false), the style has
    // no presets, or the pair is not in the list of that style.
    int scale_form_preset_index(const ScaleFormState &state);

    // The user picked row `index` of the preset menu (set_scale_dialog.py _on_predefined_scale_changed): the state's preset pair becomes the pair of that row,
    // has_preset is true and the Pre-defined radio is on (the Windows combo is only enabled with the radio on, so a pick never happens in Custom mode there; the
    // touch form offers the menu in both modes and choosing a preset is choosing the radio, like the style change of :167-189); the style, the custom texts and
    // the switch stay. An index outside the list of the state's style (negative, too large, or a style without presets) returns the state unchanged (the radio too).
    ScaleFormState scale_form_after_preset_pick(const ScaleFormState &state, int index);

    // What a successful save refreshes (plan 4.11).
    struct PageScaleRefresh
    {
        bool reload_plan_view = false;   // PlanView loadPage again, the zoom kept: the 2D view is open and the active 2D page is among the changed pages
        bool regenerate_meshes = false;  // the 3D scene: a page selected in the 3D view is among the changed pages
        bool refresh_quantities = false; // the quantities of the conditions: the active 2D page or a selected 3D page is among the changed pages
    };

    // `changed_page_uids` are the pages the save wrote (ost_domain::apply_page_scales answers them); empty uids never match. Nothing changed: all false.
    PageScaleRefresh plan_page_scale_refresh(const std::vector<std::string> &changed_page_uids, const std::string &active_2d_page_uid,
                                             const std::set<std::string> &selected_3d_page_uids, bool is_2d_mode);
}
