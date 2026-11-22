// COND-PLACE-01 notes 1 and 2: the Image layer and the placement tool.
//
// Windows oracle (HEAD b9902e6):
//  - presentation/coordinators/ui_event_coordinator.py _project_layer_visibility_if_current (:6474-6477, :6504-6505): the SINGLE
//    layer toggle computes image_layer = project_data.is_image_layer_uid(layer_uid) and calls _suspend_active_layer_tool(uid) only
//    "if not show and not image_layer" and _restore_suspended_layer_tool(uid) only "if show and not image_layer", so hiding the
//    Image layer neither leaves an armed placement nor touches an existing suspension;
//  - the same file, Hide All (:6728-6729) and Show All (:6752-6753): _suspend_active_layer_tool() / _restore_suspended_layer_tool()
//    with no uid and NO image check, so a placement of a condition on the Image layer is suspended and restored there like any
//    other;
//  - domain/services/project_data_service.py is_image_layer_uid (:639-641): normalize_layer_name(name of that uid) == "image",
//    the name map being built from the layer list (bid_layer_names_by_uid, :509-519; an empty name is not stored);
//  - presentation/components/plan_view/components/placement_mode.py handle_place_press (:1648): "if not condition or not
//    condition.layer_visible: event.accept(); return", so with the guard above (the armed condition of the Image layer is then
//    hidden) a press places nothing; enter_place_mode_for_condition (:1869) and PlacementCoordinator._is_condition_placeable
//    (placement_coordinator.py:127-129) refuse a hidden condition;
//  - tests/presentation/coordinators/test_ui_event_coordinator.py test_hiding_an_image_layer_does_not_suspend_the_active_tool
//    (:33485), test_showing_an_image_layer_updates_textures_and_skips_tool_handling (:33454), the Hide All / Show All calls with no
//    argument (:34527, :34564).
// Note 2 (not a defect): a condition on no layer is never suspended (_active_layer_tool_snapshot: no placed condition has a layer
// -> None, :1474-1491), is never hidden by Hide All (LP-22), so the tool stays armed on a visible, placeable condition.
//
// iOS: ost_domain::is_image_layer_uid (Layer.hpp), the defaulted trailing bool of suspended_placement_for_layer_hide and
// restore_of_suspended_placement (ConditionChoices.hpp) and ost_data::placement_tap_accepted(project, condition uid)
// (ProjectData.hpp, the one decision "the condition exists and is visible"); ViewController+Layers.mm computes the flag once for a
// single toggle and passes NO for Hide All / Show All. ViewController.mm asks placement_tap_accepted at the start of every
// placement gesture (the PlanViewEditDelegate query planView:canPlaceWithCondition:, asked by PlanView at the press), in the
// floating mode button before it enters Place and, as the last line of defence, in planView:didPlaceTakeoff...
#include "TestHarness.hpp"

#include "Data/ProjectData.hpp"
#include "Domain/ConditionChoices.hpp"
#include "Domain/Layer.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ost_domain;

namespace
{
    Condition placed_condition(const std::string &uid, const std::string &layer_uid, ConditionType type = ConditionType::Count)
    {
        Condition condition;
        condition.uid = uid;
        condition.layer_uid = layer_uid;
        condition.condition_type = type;
        return condition;
    }

    BidLayer listed(const std::string &uid, const std::string &name, int sequence)
    {
        BidLayer layer;
        layer.uid = uid;
        layer.bid_uid = "bid-1";
        layer.name = name;
        layer.show = true;
        layer.sequence = sequence;
        return layer;
    }

    Layer own(const std::string &uid, const std::string &name, int sequence, bool visible = true)
    {
        Layer layer;
        layer.uid = uid;
        layer.name = name;
        layer.sequence = sequence;
        layer.visible = visible;
        return layer;
    }

    // Whether a layer of that name is the Image layer, found through the merged list (the layer sheet) alone ...
    bool image_by_merged_name(const std::string &name)
    {
        return is_image_layer_uid({listed("u", name, 0)}, {}, "u");
    }

    // ... and through the bid's own layers alone.
    bool image_by_own_name(const std::string &name)
    {
        return is_image_layer_uid({}, {{"u", own("u", name, 0)}}, "u");
    }

    // A bid with the layers Image ("img") and Default ("def"), both shown, and three conditions: one on the Image layer, one on
    // Default and one on no layer. The merged list is the layer sheet in Sequence order.
    ost_data::ProjectData bid_with_image_layer()
    {
        ost_data::ProjectData project;
        project.bid.uid = "bid-1";
        project.merged_layers = {listed("img", "Image", 0), listed("def", "Default", 1)};
        project.layers["img"] = own("img", "Image", 0);
        project.layers["def"] = own("def", "Default", 1);
        project.conditions["c-img"] = placed_condition("c-img", "img");
        project.conditions["c-def"] = placed_condition("c-def", "def");
        project.conditions["c-none"] = placed_condition("c-none", "");
        return project;
    }

    // What the app looks like once the placement tool was left by a suspension and `condition` is looked up again: the same bid,
    // the view mode, the tool revision of the suspension, the 2D takeoff view, the visibility the project has now.
    PlacementRestoreState state_after_suspension(const SuspendedPlacement &suspended, const ost_data::ProjectData &project,
                                                 const std::string &condition_uid)
    {
        PlacementRestoreState now;
        now.bid_uid = suspended.bid_uid;
        now.tool_in_view_mode = true;
        now.tool_revision = suspended.tool_revision;
        auto found = project.conditions.find(condition_uid);
        if (found != project.conditions.end())
        {
            now.condition = &found->second;
            now.condition_visible = project.is_condition_visible(found->second);
        }
        now.takeoff_2d_view_active = true;
        return now;
    }
}

// Windows test_hiding_an_image_layer_does_not_suspend_the_active_tool (:33485): the Image layer is hidden with a condition of that
// layer being placed: neither the suspension nor anything else happens; the same hide of any other layer suspends (:33415)
TEST(CondPlace01_ImageLayer, HidingTheImageLayerDoesNotSuspendEvenForAConditionOnIt)
{
    const Condition on_image = placed_condition("c-1", "img");

    // the baseline the guard departs from: the layer of the placed condition is hidden and is not the Image layer
    CHECK(suspended_placement_for_layer_hide("bid-1", on_image, std::string("img"), false).has_value());
    // the existing three-argument call is the same thing (no Image flag)
    CHECK(suspended_placement_for_layer_hide("bid-1", on_image, std::string("img")).has_value());

    // the Image layer is hidden: the placement of its condition is left running
    CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", on_image, std::string("img"), true).has_value());
    // any style of condition: the guard does not look at the condition at all
    for (ConditionType type : {ConditionType::Linear, ConditionType::Area, ConditionType::Count, ConditionType::Attachment})
        CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", placed_condition("c-t", "img", type), std::string("img"), true)
                        .has_value());
    // a condition of another layer is not suspended by hiding the Image layer, with or without the guard
    CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", placed_condition("c-2", "def"), std::string("img"), true).has_value());
    CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", placed_condition("c-2", "def"), std::string("img"), false).has_value());
}

// Windows test_showing_an_image_layer_updates_textures_and_skips_tool_handling (:33454): showing the Image layer does not call
// _restore_suspended_layer_tool, so a suspension that exists (made by Hide All, say) is neither restored nor dropped by it
TEST(CondPlace01_ImageLayer, ShowingTheImageLayerKeepsAnExistingSuspension)
{
    const ost_data::ProjectData project = bid_with_image_layer();
    // the placement of a condition on the Default layer was suspended (Hide All, or hiding Default)
    std::optional<SuspendedPlacement> suspended =
        suspended_placement_for_layer_hide("bid-1", project.conditions.at("c-def"), std::string("def"));
    REQUIRE(suspended.has_value());
    suspended->tool_revision = 4;
    const PlacementRestoreState now = state_after_suspension(*suspended, project, "c-def");

    // the baseline: showing the layer of the suspension restores it
    CHECK(restore_of_suspended_placement(*suspended, std::string("def"), now, false) == SuspendedPlacementOutcome::Restore);
    // the existing three-argument call is the same thing
    CHECK(restore_of_suspended_placement(*suspended, std::string("def"), now) == SuspendedPlacementOutcome::Restore);

    // the Image layer is shown: the suspension stays as it is, whatever else happened
    CHECK(restore_of_suspended_placement(*suspended, std::string("img"), now, true) == SuspendedPlacementOutcome::Keep);
    PlacementRestoreState changed = now;
    changed.bid_uid = "bid-2";
    changed.tool_in_view_mode = false;
    CHECK(restore_of_suspended_placement(*suspended, std::string("img"), changed, true) == SuspendedPlacementOutcome::Keep);

    // even when the suspension is the one of a condition on the Image layer itself (made by Hide All, which has no guard)
    std::optional<SuspendedPlacement> on_image =
        suspended_placement_for_layer_hide("bid-1", project.conditions.at("c-img"), std::nullopt, true);
    REQUIRE(on_image.has_value());
    on_image->tool_revision = 7;
    const PlacementRestoreState image_now = state_after_suspension(*on_image, project, "c-img");
    CHECK(restore_of_suspended_placement(*on_image, std::string("img"), image_now, true) == SuspendedPlacementOutcome::Keep);
    CHECK(restore_of_suspended_placement(*on_image, std::string("img"), image_now, false) == SuspendedPlacementOutcome::Restore);
}

// PIN (matches Windows before and after this change): Hide All / Show All are not guarded. A placement of a condition on the Image
// layer is suspended by Hide All and put back by Show All, because both name no layer (_suspend_active_layer_tool() and
// _restore_suspended_layer_tool() with no uid, :6728-6729, :6752-6753); the Image flag is only read together with a layer uid.
TEST(CondPlace01_ImageLayer, HideAllStillSuspendsAConditionOnTheImageLayerAndShowAllRestoresIt)
{
    ost_data::ProjectData project = bid_with_image_layer();
    const Condition &on_image = project.conditions.at("c-img");

    // Hide All: the plan writes every layer, the placement of the condition on the Image layer is suspended (no layer uid, so the
    // flag does not apply, whichever value a caller passes)
    const BulkLayerPlan hide_all = plan_bulk_layer_visibility(project.merged_layers, "bid-1", project.layers, false);
    REQUIRE(hide_all.outcome == BulkLayerOutcome::Write);
    for (bool flag : {false, true})
    {
        const std::optional<SuspendedPlacement> suspended =
            suspended_placement_for_layer_hide("bid-1", on_image, std::nullopt, flag);
        CHECK(suspended.has_value());
    }
    std::optional<SuspendedPlacement> suspended = suspended_placement_for_layer_hide("bid-1", on_image, std::nullopt, true);
    REQUIRE(suspended.has_value());
    CHECK_EQ(suspended->condition_uid, std::string("c-img"));
    CHECK_EQ(suspended->layer_uid, std::string("img"));
    suspended->tool_revision = 3;
    project.apply_layer_visibility(hide_all.uids, false);
    CHECK_FALSE(project.is_condition_visible(project.conditions.at("c-img")));

    // Show All: nothing names a layer, the suspension is restored once the condition is visible again
    const BulkLayerPlan show_all = plan_bulk_layer_visibility(project.merged_layers, "bid-1", project.layers, true);
    REQUIRE(show_all.outcome == BulkLayerOutcome::Write);
    project.apply_layer_visibility(show_all.uids, true);
    CHECK(project.is_condition_visible(project.conditions.at("c-img")));
    const PlacementRestoreState now = state_after_suspension(*suspended, project, "c-img");
    for (bool flag : {false, true})
        CHECK(restore_of_suspended_placement(*suspended, std::nullopt, now, flag) == SuspendedPlacementOutcome::Restore);
}

// Windows ProjectDataService.is_image_layer_uid: the layer of that uid is named "image" after strip().lower(). The name is looked
// up in the merged list (the layer sheet, which Windows builds its name map from) first and then in the bid's own layers.
TEST(CondPlace01_ImageLayerUid, IsImageLayerUid)
{
    for (const char *name : {" IMAGE ", "image", "Image", "\tImage\n", "\r\n IMAGE\t"})
    {
        CHECK(image_by_merged_name(name));
        CHECK(image_by_own_name(name));
    }
    for (const char *name : {"Images", "Im age", "", "   ", "Imag", "Default", "Image 2", "Comments"})
    {
        CHECK_FALSE(image_by_merged_name(name));
        CHECK_FALSE(image_by_own_name(name));
    }

    // an unknown uid is never the Image layer, also when another layer is
    const std::vector<BidLayer> merged = {listed("img", "Image", 0), listed("def", "Default", 1)};
    const std::unordered_map<std::string, Layer> layers = {{"img", own("img", "Image", 0)}, {"def", own("def", "Default", 1)}};
    CHECK(is_image_layer_uid(merged, layers, "img"));
    CHECK_FALSE(is_image_layer_uid(merged, layers, "def"));
    CHECK_FALSE(is_image_layer_uid(merged, layers, "nope"));
    CHECK_FALSE(is_image_layer_uid(merged, layers, ""));
    CHECK_FALSE(is_image_layer_uid({}, {}, "img"));

    // a uid the sheet does not list (a layer of the bid only) is found through the bid's own layers, and the other way round (a
    // shared template row of the sheet that is no layer of the bid)
    CHECK(is_image_layer_uid({listed("def", "Default", 1)}, layers, "img"));
    CHECK(is_image_layer_uid(merged, {{"def", own("def", "Default", 1)}}, "img"));

    // the merged list decides first: its name for the uid is the one that counts
    CHECK_FALSE(is_image_layer_uid({listed("x", "Walls", 0)}, {{"x", own("x", "Image", 0)}}, "x"));
    CHECK(is_image_layer_uid({listed("x", "Image", 0)}, {{"x", own("x", "Walls", 0)}}, "x"));
    // uids are text: "1" is not "01"
    CHECK_FALSE(is_image_layer_uid({listed("01", "Image", 0)}, {}, "1"));
}

// Windows handle_place_press (:1648): "if not condition or not condition.layer_visible: event.accept(); return": a press for a
// condition that is gone or not visible places nothing. ost_data::placement_tap_accepted(project, uid) is that one decision; the
// app asks it at the start of every placement gesture (tap, press of a drag), when the floating mode button enters Place, and once
// more where the placements end (planView:didPlaceTakeoffWithCondition:position:).
TEST(CondPlace01_TapGuard, NoTapIsAcceptedForAHiddenPlacedCondition)
{
    // the situation the Image guard creates: the condition of the Image layer is armed, the Image layer is hidden (the single
    // toggle does not suspend), so the armed condition is hidden and its taps are ignored; shown again, they place again
    ost_data::ProjectData project = bid_with_image_layer();
    const Condition &armed = project.conditions.at("c-img");
    for (const char *uid : {"c-img", "c-def", "c-none"})
        CHECK(ost_data::placement_tap_accepted(project, uid));

    const bool image_layer = is_image_layer_uid(project.merged_layers, project.layers, "img");
    REQUIRE(image_layer);
    CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", armed, std::string("img"), image_layer).has_value());
    project.layers.at("img").visible = false;
    CHECK_FALSE(project.is_condition_visible(armed));
    CHECK_FALSE(ost_data::placement_tap_accepted(project, "c-img"));
    // a condition of another layer that is still shown, and a condition on no layer, are not affected by the Image layer
    CHECK(ost_data::placement_tap_accepted(project, "c-def"));
    CHECK(ost_data::placement_tap_accepted(project, "c-none"));

    project.layers.at("img").visible = true;
    CHECK(ost_data::placement_tap_accepted(project, "c-img"));

    // any other layer: hiding it suspends the placement, so no armed condition is ever hidden and tapped
    project.layers.at("def").visible = false;
    CHECK(suspended_placement_for_layer_hide("bid-1", project.conditions.at("c-def"), std::string("def"), false).has_value());
    CHECK_FALSE(ost_data::placement_tap_accepted(project, "c-def"));
    CHECK(ost_data::placement_tap_accepted(project, "c-img"));
}

// "not condition": a condition that was deleted (or never existed) since the tool was armed is refused, whatever the layers are
// (the app keeps the uid of the armed condition, so this is the stale-uid case), and the refusal does not depend on its style.
TEST(CondPlace01_TapGuard, AConditionThatIsGoneIsRefused)
{
    ost_data::ProjectData project = bid_with_image_layer();
    CHECK(ost_data::placement_tap_accepted(project, "c-def"));
    project.remove_condition("c-def");
    CHECK_FALSE(ost_data::placement_tap_accepted(project, "c-def"));
    CHECK_FALSE(ost_data::placement_tap_accepted(project, "never-existed"));
    CHECK_FALSE(ost_data::placement_tap_accepted(project, ""));
    // uids are text: the lookup is exact
    CHECK_FALSE(ost_data::placement_tap_accepted(project, "C-IMG"));
    CHECK_FALSE(ost_data::placement_tap_accepted(project, "c-img "));
    CHECK(ost_data::placement_tap_accepted(project, "c-img"));

    // nothing at all in the project: nothing is accepted
    const ost_data::ProjectData empty{};
    CHECK_FALSE(ost_data::placement_tap_accepted(empty, "c-img"));
    CHECK_FALSE(ost_data::placement_tap_accepted(empty, ""));
}

// handle_place_press asks layer_visible before it looks at the style, so the same decision holds for Count (tap), Linear (drag),
// Area (polygon or rectangle drag) and Attachment: a hidden condition is refused for every style, a visible one accepted.
TEST(CondPlace01_TapGuard, TheDecisionDoesNotDependOnTheConditionStyle)
{
    ost_data::ProjectData project = bid_with_image_layer();
    int index = 0;
    for (ConditionType type : {ConditionType::Count, ConditionType::Linear, ConditionType::Area, ConditionType::Attachment})
    {
        const std::string shown = "shown-" + std::to_string(index);
        const std::string hidden = "hidden-" + std::to_string(index);
        project.conditions[shown] = placed_condition(shown, "def", type);
        project.conditions[hidden] = placed_condition(hidden, "img", type);
        ++index;
    }
    project.layers.at("img").visible = false;
    for (int i = 0; i < index; ++i)
    {
        CHECK(ost_data::placement_tap_accepted(project, "shown-" + std::to_string(i)));
        CHECK_FALSE(ost_data::placement_tap_accepted(project, "hidden-" + std::to_string(i)));
    }
}

// Windows layer_visible is true for a condition that no layer of the bid hides (layer.py is_layer_visible: a falsy layer uid and a
// layer uid that is not among the bid's own layers are visible), so those are accepted even when every layer of the bid is hidden: a
// condition without a layer, with a layer uid the bid does not have, and on a locked template row that is listed in the layer sheet
// with Show off but is no layer of the bid (a shared row; the sheet cannot change it).
TEST(CondPlace01_TapGuard, ALayerlessUnknownLayerOrSharedLayerConditionIsAccepted)
{
    ost_data::ProjectData project = bid_with_image_layer();
    BidLayer shared = listed("shared", "Shared template", 2);
    shared.bid_uid = "bid-2";
    shared.show = false;
    project.merged_layers.push_back(shared);
    project.conditions["c-shared"] = placed_condition("c-shared", "shared");
    project.conditions["c-ghost"] = placed_condition("c-ghost", "layer-that-is-gone");
    project.conditions["c-blank"] = placed_condition("c-blank", "");
    REQUIRE(project.layers.count("shared") == 0);
    REQUIRE(project.layers.count("layer-that-is-gone") == 0);

    project.apply_layer_visibility({"img", "def"}, false); // Hide All of the bid's own layers
    CHECK_FALSE(project.is_listed_layer_visible("shared"));
    CHECK(ost_data::placement_tap_accepted(project, "c-none"));
    CHECK(ost_data::placement_tap_accepted(project, "c-blank"));
    CHECK(ost_data::placement_tap_accepted(project, "c-ghost"));
    CHECK(ost_data::placement_tap_accepted(project, "c-shared"));
    // while the conditions of the hidden own layers are refused
    CHECK_FALSE(ost_data::placement_tap_accepted(project, "c-img"));
    CHECK_FALSE(ost_data::placement_tap_accepted(project, "c-def"));

    // Show All brings the own layers back
    project.apply_layer_visibility({"img", "def"}, true);
    CHECK(ost_data::placement_tap_accepted(project, "c-img"));
    CHECK(ost_data::placement_tap_accepted(project, "c-def"));
}

// Note 2 (not a defect, nothing to change): a condition on no layer is never suspended and is never hidden by Hide All, so the tool
// stays armed on a visible, placeable condition and Show All has nothing to restore.
TEST(CondPlace01_LayerlessCondition, ALayerlessPlacedConditionIsNeverSuspendedAndStaysVisibleAfterHideAll)
{
    ost_data::ProjectData project = bid_with_image_layer();
    const Condition &layerless = project.conditions.at("c-none");

    const BulkLayerPlan hide_all = plan_bulk_layer_visibility(project.merged_layers, "bid-1", project.layers, false);
    REQUIRE(hide_all.outcome == BulkLayerOutcome::Write);
    // Hide All: no suspension, whichever way the Image flag is passed, for Hide All and for a single toggle of any layer
    for (bool flag : {false, true})
    {
        CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", layerless, std::nullopt, flag).has_value());
        CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", layerless, std::string("img"), flag).has_value());
        CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", layerless, std::string(""), flag).has_value());
    }
    project.apply_layer_visibility(hide_all.uids, false);

    // every layer is hidden, the condition on none is still visible, so its taps still place
    CHECK_FALSE(project.layers.at("img").visible);
    CHECK_FALSE(project.layers.at("def").visible);
    CHECK(project.is_condition_visible(layerless));
    CHECK(ost_data::placement_tap_accepted(project, "c-none"));
    // the conditions on the hidden layers are hidden, as Windows
    CHECK_FALSE(project.is_condition_visible(project.conditions.at("c-img")));
    CHECK_FALSE(project.is_condition_visible(project.conditions.at("c-def")));
}
