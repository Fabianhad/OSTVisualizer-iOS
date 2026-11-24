// Set Scale, what the app decides around the form (plan 5.4 touch adaptation, 4.11, N-M1-21; tests first): the confirmation before an "Apply to all pages" save, the footer of
// the form, the row of the Pre-defined picker and what a save refreshes. The Objective-C part (PageScaleViewController, ViewController+PageScale.mm) only shows what these answer
// and is device-only (checks M1-16, M1-17).
// Windows source of truth: the labels come from scales.py:58-74 and page_settings_bar.py:159-206 (pinned elsewhere, test_page_scale_presets.cpp); the picker rule is
// set_scale_dialog.py:167-189 (_on_style_changed / _on_predefined_scale_changed); the refresh is ui_event_coordinator.py:3318-3359 _on_page_metadata_changed. Windows has no confirmation
// and no footer, so those wordings are the iOS-only texts of plan 4.14 (N-M1-03: the owner may reword them) and the tests pin the wording of this build.
#include "FormattingTestSupport.hpp"

#include "Domain/PageScale.hpp"
#include "Domain/PageScaleApply.hpp"
#include "Domain/PageScaleRules.hpp"

#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <vector>

using namespace ost_domain;
using std::set;
using std::string;
using std::vector;

namespace
{
    constexpr double kNotANumber = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInfinity = std::numeric_limits<double>::infinity();

    ScaleFormState state_with_preset(int style, double sf1, double sf2)
    {
        ScaleFormState state;
        state.style = style;
        state.mode = ScaleInputMode::Predefined;
        state.has_preset = true;
        state.preset_sf1 = sf1;
        state.preset_sf2 = sf2;
        state.custom1 = "0.125";
        state.custom2 = "12";
        state.apply_to_all = false;
        return state;
    }
}

// ---- the confirmation (N-M1-21: one confirmation only when "apply to all" touches more than one page) -------------------------------------------------------------------------

// Zero or one page: no confirmation, the single page saves at once like Windows (which asks nothing).
TEST(SetScale_ApplyConfirmation, NothingToConfirmForZeroOrOnePage)
{
    CHECK_FALSE(scale_apply_all_confirmation(0).has_value());
    CHECK_FALSE(scale_apply_all_confirmation(1).has_value());
}

// Two pages and more: title, message with the page count, the two buttons (the wording of this build, N-M1-03).
TEST(SetScale_ApplyConfirmation, TwoPagesAskOnceWithTheCountInTheMessage)
{
    const auto two = scale_apply_all_confirmation(2);
    REQUIRE(two.has_value());
    CHECK_STR_EQ(two->title, "Apply to All Pages");
    CHECK_STR_EQ(two->message, "Set the scale of 2 pages? Takeoffs on those pages are rescaled and quantities change.");
    CHECK_STR_EQ(two->confirm_title, "Set Scale");
    CHECK_STR_EQ(two->cancel_title, "Cancel");

    const auto many = scale_apply_all_confirmation(137);
    REQUIRE(many.has_value());
    CHECK_STR_EQ(many->message, "Set the scale of 137 pages? Takeoffs on those pages are rescaled and quantities change.");
    CHECK_STR_EQ(many->title, two->title);
}

// ---- the footer of the form -------------------------------------------------------------------------------------------------------------------------------------------------

// A page on a preset shows the label of the first matching preset (the Windows bar rule), then the explanation of the pair.
TEST(SetScale_FormFooter, ThePresetLabelIsTheFirstLineAndTheExplanationTheSecond)
{
    CHECK_STR_EQ(scale_form_footer_text("Sheet A1", 0.125, 12.0),
                 "Current scale of \"Sheet A1\": 1/8\" = 1' 0\"\n"
                 "Drawing length : real length, in the same unit, for example 0.125 : 12 for 1/8\" = 1' 0\", or 1 : 100 for metric.");
    CHECK_STR_EQ(scale_form_footer_text("Plan", 1.0, 100.0),
                 "Current scale of \"Plan\": 1 : 100\n"
                 "Drawing length : real length, in the same unit, for example 0.125 : 12 for 1/8\" = 1' 0\", or 1 : 100 for metric.");
    // (1, 1) is in two presets and the first one wins (SS-7): Civil `1" = 1"`
    CHECK_STR_EQ(scale_form_footer_text("P", 1.0, 1.0).substr(0, string("Current scale of \"P\": 1\" = 1\"").size()), "Current scale of \"P\": 1\" = 1\"");
}

// A page whose pair is no preset shows the custom label (page_settings_bar.py:170: 0.26" = 1' 0").
TEST(SetScale_FormFooter, ACustomPairShowsTheCustomLabel)
{
    const string footer = scale_form_footer_text("Roof", 0.26, 12.0);
    CHECK_STR_EQ(footer.substr(0, footer.find('\n')), "Current scale of \"Roof\": 0.26\" = 1' 0\"");
    const string metric = scale_form_footer_text("Roof", 2.5, 1000.0);
    CHECK_STR_EQ(metric.substr(0, metric.find('\n')), "Current scale of \"Roof\": 2.5 : 1000");
}

// A pair that has no label (nothing is selected on the Windows bar, test_page_settings_bar.py:295) reads "not set".
TEST(SetScale_FormFooter, APairWithoutALabelReadsNotSet)
{
    const struct
    {
        double sf1;
        double sf2;
    } bad[] = {{0.0, 12.0}, {-0.26, 12.0}, {0.26, 0.0}, {0.26, -12.0}, {kNotANumber, 12.0}, {0.26, kNotANumber}, {kInfinity, 12.0}, {0.26, kInfinity}};
    for (const auto &pair : bad)
    {
        SCOPED_CONTEXT("pair " + std::to_string(pair.sf1) + " : " + std::to_string(pair.sf2));
        const string footer = scale_form_footer_text("Roof", pair.sf1, pair.sf2);
        CHECK_STR_EQ(footer.substr(0, footer.find('\n')), "Current scale of \"Roof\": not set");
        CHECK(footer.find("\nDrawing length : real length") != string::npos);
    }
}

// An empty page name reads "this page" without quotes.
TEST(SetScale_FormFooter, AnEmptyPageNameReadsThisPage)
{
    const string footer = scale_form_footer_text("", 0.125, 12.0);
    CHECK_STR_EQ(footer.substr(0, footer.find('\n')), "Current scale of this page: 1/8\" = 1' 0\"");
}

// ---- the Pre-defined picker ---------------------------------------------------------------------------------------------------------------------------------------------------

// The row the picker shows is the row of the preset pair of the state, in the list of the state's style (set_scale_dialog.py:167-189).
TEST(SetScale_PickerRow, TheRowIsThePositionOfThePresetPairInTheListOfTheStyle)
{
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleArchitectural, 0.03125, 12.0)), 0);
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleArchitectural, 0.125, 12.0)), 3);
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleArchitectural, 3.0, 12.0)), 11);
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleCivil, 1.0, 1.0)), 0);
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleCivil, 1.0, 240.0)), 2);
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleCivil, 1.0, 1200.0)), 10);
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleMetric, 1.0, 1000.0)), 0);
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleMetric, 1.0, 100.0)), 4);
    // (1, 1) is Metric row 13 in the Metric list (the Civil row 0 in the Civil list): the style of the state decides
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleMetric, 1.0, 1.0)), 13);
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleMetric, 50.0, 1.0)), 16);
}

// Every row of every style is found at its own position (the whole table, so a wrong comparison cannot hide in one style).
TEST(SetScale_PickerRow, EveryPresetOfEveryStyleIsFoundAtItsOwnRow)
{
    for (int style : {kScaleStyleArchitectural, kScaleStyleCivil, kScaleStyleMetric})
    {
        const auto &presets = scale_presets(style);
        for (std::size_t i = 0; i < presets.size(); ++i)
        {
            SCOPED_CONTEXT("style " + std::to_string(style) + " row " + std::to_string(i));
            // the first row with the same pair wins, like the first-match rule of the Windows bar (SS-7)
            std::size_t first = 0;
            while (!(presets[first].sf1 == presets[i].sf1 && presets[first].sf2 == presets[i].sf2))
                ++first;
            CHECK_EQ(scale_form_preset_index(state_with_preset(style, presets[i].sf1, presets[i].sf2)), static_cast<int>(first));
        }
    }
}

// No preset in the state, a pair that is not in the list of the style, or a style without presets: no row.
TEST(SetScale_PickerRow, NoRowWithoutAPresetOrForAPairThatTheStyleDoesNotList)
{
    ScaleFormState none = state_with_preset(kScaleStyleArchitectural, 0.125, 12.0);
    none.has_preset = false;
    CHECK_EQ(scale_form_preset_index(none), -1);
    // a Civil pair in the Architectural list
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleArchitectural, 1.0, 240.0)), -1);
    // an Architectural pair in the Metric list
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleMetric, 0.125, 12.0)), -1);
    // a pair that is close but not equal (the state takes its pair from the list: no tolerance here)
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleArchitectural, 0.125 + 1e-12, 12.0)), -1);
    CHECK_EQ(scale_form_preset_index(state_with_preset(kScaleStyleArchitectural, kNotANumber, 12.0)), -1);
    // a style that has no list
    CHECK_EQ(scale_form_preset_index(state_with_preset(0, 0.125, 12.0)), -1);
    CHECK_EQ(scale_form_preset_index(state_with_preset(4, 0.125, 12.0)), -1);
}

// The state that a session opens on points at its own row (the page shows its preset in the picker; the custom page shows the first row of style 1).
TEST(SetScale_PickerRow, TheOpeningStateOfASessionPointsAtItsRow)
{
    CHECK_EQ(scale_form_preset_index(ScaleFormSession(0.125, 12.0).initial()), 3);
    CHECK_EQ(scale_form_preset_index(ScaleFormSession(1.0, 240.0).initial()), 2);
    CHECK_EQ(scale_form_preset_index(ScaleFormSession(1.0, 100.0).initial()), 4);
    // no preset: style 1, the first row of its list as the picker's value (ScaleFormSession::initial)
    CHECK_EQ(scale_form_preset_index(ScaleFormSession(0.26, 12.0).initial()), 0);
}

// Picking a row sets the preset pair and turns the Pre-defined radio on; the style, the custom texts and the switch stay.
TEST(SetScale_PickerPick, APickSetsThePairAndKeepsTheRest)
{
    ScaleFormState state = state_with_preset(kScaleStyleArchitectural, 0.125, 12.0);
    state.custom1 = "7";
    state.custom2 = "9";
    state.apply_to_all = true;
    const ScaleFormState picked = scale_form_after_preset_pick(state, 5);
    CHECK(picked.has_preset);
    CHECK_EQ(picked.preset_sf1, 0.25);
    CHECK_EQ(picked.preset_sf2, 12.0);
    CHECK_EQ(picked.style, kScaleStyleArchitectural);
    CHECK(picked.mode == ScaleInputMode::Predefined);
    CHECK_STR_EQ(picked.custom1, "7");
    CHECK_STR_EQ(picked.custom2, "9");
    CHECK(picked.apply_to_all);
    CHECK_EQ(scale_form_preset_index(picked), 5);

    // the other styles pick from their own list
    const ScaleFormState civil = scale_form_after_preset_pick(state_with_preset(kScaleStyleCivil, 1.0, 1.0), 2);
    CHECK_EQ(civil.preset_sf1, 1.0);
    CHECK_EQ(civil.preset_sf2, 240.0);
    const ScaleFormState metric = scale_form_after_preset_pick(state_with_preset(kScaleStyleMetric, 1.0, 1000.0), 16);
    CHECK_EQ(metric.preset_sf1, 50.0);
    CHECK_EQ(metric.preset_sf2, 1.0);

    // a pick turns a state without a preset into one that has it
    ScaleFormState empty = state_with_preset(kScaleStyleArchitectural, 0.0, 0.0);
    empty.has_preset = false;
    const ScaleFormState filled = scale_form_after_preset_pick(empty, 0);
    CHECK(filled.has_preset);
    CHECK_EQ(filled.preset_sf1, 0.03125);
    CHECK_EQ(filled.preset_sf2, 12.0);

    // a pick on a Custom form turns the Pre-defined radio on: the touch form offers the preset menu in both modes, and choosing a preset is choosing the radio (the Windows combo
    // is only enabled with the radio on, so there a pick never happens in Custom mode; set_scale_dialog.py:167-189 also turns the radio on for a style change)
    ScaleFormState custom = state_with_preset(kScaleStyleArchitectural, 0.125, 12.0);
    custom.mode = ScaleInputMode::Custom;
    custom.custom1 = "3";
    custom.custom2 = "4";
    const ScaleFormState fromCustom = scale_form_after_preset_pick(custom, 1);
    CHECK(fromCustom.mode == ScaleInputMode::Predefined);
    CHECK_EQ(fromCustom.preset_sf1, 0.0625);
    CHECK_STR_EQ(fromCustom.custom1, "3");
    CHECK_STR_EQ(fromCustom.custom2, "4");
}

// A row outside the list of the style changes nothing (the radio stays as it is too).
TEST(SetScale_PickerPick, ARowOutsideTheListChangesNothing)
{
    ScaleFormState custom = state_with_preset(kScaleStyleCivil, 1.0, 240.0);
    custom.mode = ScaleInputMode::Custom;
    CHECK(scale_form_after_preset_pick(custom, 99).mode == ScaleInputMode::Custom);
    CHECK(scale_form_after_preset_pick(custom, -1).mode == ScaleInputMode::Custom);
    const ScaleFormState state = state_with_preset(kScaleStyleCivil, 1.0, 240.0);
    for (int index : {-1, 11, 12, 100, -100})
    {
        SCOPED_CONTEXT("index " + std::to_string(index));
        const ScaleFormState same = scale_form_after_preset_pick(state, index);
        CHECK_EQ(same.preset_sf1, 1.0);
        CHECK_EQ(same.preset_sf2, 240.0);
        CHECK(same.has_preset);
        CHECK_EQ(same.style, kScaleStyleCivil);
    }
    // the last row of each list is a valid pick, the one after it is not
    CHECK(scale_form_after_preset_pick(state_with_preset(kScaleStyleArchitectural, 0.125, 12.0), 11).preset_sf1 == 3.0);
    CHECK(scale_form_after_preset_pick(state_with_preset(kScaleStyleArchitectural, 0.125, 12.0), 12).preset_sf1 == 0.125);
    CHECK(scale_form_after_preset_pick(state_with_preset(kScaleStyleCivil, 1.0, 240.0), 10).preset_sf2 == 1200.0);
    CHECK(scale_form_after_preset_pick(state_with_preset(kScaleStyleMetric, 1.0, 1000.0), 17).preset_sf2 == 1000.0);
    // a style without a list
    const ScaleFormState odd = scale_form_after_preset_pick(state_with_preset(7, 0.125, 12.0), 0);
    CHECK_EQ(odd.preset_sf1, 0.125);
    CHECK_EQ(odd.preset_sf2, 12.0);
}

// A pick followed by a style change and back keeps the rule of the Windows combo (the picker keeps a pair the new list has).
TEST(SetScale_PickerPick, AStyleChangeAfterAPickKeepsTheSharedPair)
{
    // (1, 1) is in Civil row 0 and Metric row 13: the Metric -> Civil change keeps the pair (set_scale_dialog.py:167-189)
    ScaleFormState metric = scale_form_after_preset_pick(state_with_preset(kScaleStyleMetric, 1.0, 1000.0), 13);
    const ScaleFormState civil = scale_form_after_style_change(metric, kScaleStyleCivil);
    CHECK_EQ(scale_form_preset_index(civil), 0);
    CHECK_EQ(civil.preset_sf1, 1.0);
    CHECK_EQ(civil.preset_sf2, 1.0);
    // a pair the new list does not have: the first row
    const ScaleFormState architectural = scale_form_after_style_change(civil, kScaleStyleArchitectural);
    CHECK_EQ(scale_form_preset_index(architectural), 0);
}

// ---- what a save refreshes ----------------------------------------------------------------------------------------------------------------------------------------------------

// The active 2D page changed while the 2D view is open: plan view, quantities; no scene when no changed page is selected in 3D.
TEST(SetScale_Refresh, TheActivePageInThe2DViewReloadsThePlanViewAndTheQuantities)
{
    const PageScaleRefresh refresh = plan_page_scale_refresh({"7"}, "7", set<string>{"8"}, true);
    CHECK(refresh.reload_plan_view);
    CHECK_FALSE(refresh.regenerate_meshes);
    CHECK(refresh.refresh_quantities);
}

// The pressed page is not the active one and not selected in 3D: nothing is shown on it, nothing refreshes (Windows: the three clauses of _on_page_metadata_changed).
TEST(SetScale_Refresh, AChangedPageThatNothingShowsRefreshesNothing)
{
    const PageScaleRefresh refresh = plan_page_scale_refresh({"9"}, "7", set<string>{"8"}, true);
    CHECK_FALSE(refresh.reload_plan_view);
    CHECK_FALSE(refresh.regenerate_meshes);
    CHECK_FALSE(refresh.refresh_quantities);
}

// A page selected in the 3D view changed: the scene is rebuilt and the quantities refresh, the plan view does not reload (it is another page).
TEST(SetScale_Refresh, ASelected3DPageRegeneratesTheMeshesAndRefreshesTheQuantities)
{
    const PageScaleRefresh refresh = plan_page_scale_refresh({"8"}, "7", set<string>{"8", "9"}, true);
    CHECK_FALSE(refresh.reload_plan_view);
    CHECK(refresh.regenerate_meshes);
    CHECK(refresh.refresh_quantities);
}

// Apply to all: every page changed, the active page and the 3D selection are both among them: all three refreshes.
TEST(SetScale_Refresh, ApplyToAllRefreshesEverything)
{
    const PageScaleRefresh refresh = plan_page_scale_refresh({"7", "8", "9"}, "7", set<string>{"8"}, true);
    CHECK(refresh.reload_plan_view);
    CHECK(refresh.regenerate_meshes);
    CHECK(refresh.refresh_quantities);
}

// The 3D view is open (deliberate difference from Windows, which has one viewer): the plan view is not reloaded, because entering the 2D view reloads the page anyway;
// the scene and the quantities follow the same rule as in 2D.
TEST(SetScale_Refresh, TheHiddenPlanViewIsNotReloadedIn3DMode_DeliberateDifferenceFromWindows)
{
    const PageScaleRefresh refresh = plan_page_scale_refresh({"7", "8"}, "7", set<string>{"8"}, false);
    CHECK_FALSE(refresh.reload_plan_view);
    CHECK(refresh.regenerate_meshes);
    CHECK(refresh.refresh_quantities);
    // the active page alone changed in 3D mode: quantities refresh (Windows: active_page_uid in affected), no scene, no reload
    const PageScaleRefresh active = plan_page_scale_refresh({"7"}, "7", set<string>{"8"}, false);
    CHECK_FALSE(active.reload_plan_view);
    CHECK_FALSE(active.regenerate_meshes);
    CHECK(active.refresh_quantities);
}

// Nothing changed, no active page, empty uids never match an empty active page or selection.
TEST(SetScale_Refresh, NothingChangedAndEmptyUidsRefreshNothing)
{
    for (bool is2d : {true, false})
    {
        SCOPED_CONTEXT(string("is_2d_mode ") + (is2d ? "true" : "false"));
        const PageScaleRefresh none = plan_page_scale_refresh({}, "7", set<string>{"7"}, is2d);
        CHECK_FALSE(none.reload_plan_view);
        CHECK_FALSE(none.regenerate_meshes);
        CHECK_FALSE(none.refresh_quantities);
        // an empty uid in the changed list and an empty active page are not a match
        const PageScaleRefresh empty = plan_page_scale_refresh({""}, "", set<string>{""}, is2d);
        CHECK_FALSE(empty.reload_plan_view);
        CHECK_FALSE(empty.regenerate_meshes);
        CHECK_FALSE(empty.refresh_quantities);
    }
}
