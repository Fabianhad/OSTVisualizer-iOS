// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The choices and orders of the condition screens, extracted from the Edit Condition dialog and the Conditions list
// (EditConditionViewController.mm, EditConditionHelpers.mm, ViewController+Conditions.mm).
//
// Windows source of truth:
//   ost_visualizer/presentation/dialogs/condition_types_dialog.py:152          types sorted by name.lower() (COND-CDNPICK-01)
//   ost_visualizer/presentation/dialogs/edit_condition_dialog.py _layers       the layer list (COND-LAYER-01)
//   ost_visualizer/domain/entities/layer.py merge_layers_for_bid                (Layer.hpp)
//   ost_visualizer/presentation/dialogs/edit_condition_dialog.py _SHAPE_ITEMS   (COND-SHAPE-01)
//   ost_visualizer/presentation/components/conditions_sidebar.py
//                                collect_ordered_condition_uids                 (COND-NAV-01)
//   ost_visualizer/presentation/dialogs/edit_condition_dialog.py :841,1204-1207 (COND-M08)
//   ost_visualizer/presentation/coordinators/placement_coordinator.py enter, handlers/condition_action_handler.py
//                                _finish_condition_duplicate                    (COND-PLACE-01, COND-M07)
#pragma once
#include "CdnType.hpp"
#include "Condition.hpp"
#include "ConditionFolder.hpp"
#include "Layer.hpp"
#include "Types.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_domain
{

    // COND-CDNPICK-01: the uids of the condition types in the order of the type picker: sorted by name.lower() (Python
    // str.lower(), compared by code point), equal lower-cased names in database read order (ascending numeric uid), because
    // Windows sorts the rows as read with a stable sort. ("Apple", "apple", "Banana", "zebra" - not "Banana", "zebra", "apple".)
    std::vector<std::string> sorted_cdn_type_uids(const std::unordered_map<std::string, CdnType> &types);

    // COND-LAYER-01: the uids of the layers the layer picker offers, in the order of the Windows layer list: the layers of
    // `merged_layers` (merge_layers_for_bid: by case-insensitive name, sorted by sequence) except the Comments layer, so Image,
    // Annotation, Default and the layers of the user are all offered. Only layers of the bid (`bid_layers`) are offered: the
    // list also holds locked template layers of no bid, which no condition can be assigned to.
    std::vector<std::string> condition_layer_choices(const std::vector<BidLayer> &merged_layers,
                                                     const std::unordered_map<std::string, Layer> &bid_layers);

    // COND-SHAPE-01: one entry of the shape picker.
    struct ShapeChoice
    {
        ShapeId shape;
        const char *label;
    };

    // The shape picker in the Windows order: Square, Circle, Triangle, Rectangle, Ellipse (13), Diamond (12), Hexagon,
    // Octagon.
    const std::vector<ShapeChoice> &condition_shape_choices();

    // COND-NAV-01: the conditions in the order the Conditions list shows them (the pre-order of build_condition_tree: folders and
    // type groups by name, conditions by reference number): the order Edit Previous / Next walks.
    std::vector<std::string> condition_uids_in_list_order(const std::unordered_map<std::string, Condition> &conditions,
                                                          const std::unordered_map<std::string, ConditionFolder> &folders,
                                                          const std::unordered_map<std::string, CdnType> &cdn_types);

    // COND-M08: the style the editor shows. The database can hold a type outside Linear / Area / Count / Attachment; the Windows
    // style combo then falls back to its first entry, Linear (a save then writes that type). Every supported type is shown as it is.
    ConditionType editor_condition_type(ConditionType stored);

    // COND-PLACE-01 / COND-M07: what happens to the placement tool.
    enum class PlacementAction
    {
        Enter, // placement is entered for the condition
        Keep,  // the current placement stays as it is
        Exit   // placement is left (PlacementCoordinator.force_exit)
    };

    // PlacementCoordinator.enter: a condition whose layer is hidden is not placeable; entering for it leaves the placement
    // tool instead (force_exit).
    inline PlacementAction placement_for_selected_condition(bool condition_placeable)
    {
        return condition_placeable ? PlacementAction::Enter : PlacementAction::Exit;
    }

    // condition_action_handler._finish_condition_duplicate: in the 2D takeoff view placement.enter(new condition) (which
    // itself exits for an unplaceable condition); outside it the current placement is kept and only the list selection moves.
    inline PlacementAction placement_after_duplicate(bool takeoff_2d_view_active, bool new_condition_placeable)
    {
        if (!takeoff_2d_view_active)
            return PlacementAction::Keep;
        return placement_for_selected_condition(new_condition_placeable);
    }

    // COND-PLACE-01 note 1 (a press for a condition that is gone or hidden places nothing) is ost_data::placement_tap_accepted(
    // project, condition uid) in Data/ProjectData.hpp: it needs the project's layers, which Domain does not see.

    // COND-PLACE-01 / LP-17: a placement whose layer is hidden is suspended and put back when the layer is shown again.
    // ui_event_coordinator.py _active_layer_tool_snapshot (the place branch, :1453-1490), _suspend_active_layer_tool (:1493-1501),
    // _restore_suspended_layer_tool (:1503-1546), called from the single layer toggle _project_layer_visibility_if_current (:6476-
    // 6477, :6504-6505) and the Show All / Hide All paths (:6728-6729, :6752-6753, and the map projection :6790, :6823). Only the
    // placement tool exists on iOS (no annotation tool), and one condition is placed at a time.
    struct SuspendedPlacement
    {
        std::string condition_uid;
        // The layer of the condition: the layer whose Show brings the placement back (_SuspendedLayerTool.layer_uid)
        std::string layer_uid;
        std::string bid_uid;
        // The style of the condition when it was suspended (the Windows identity check "the same, unchanged condition")
        ConditionType condition_type = ConditionType::Linear;
        // The tool revision after the placement tool was left (PlanView.tool_revision at the end of _suspend_active_layer_tool)
        unsigned long long tool_revision = 0;
    };

    // _active_layer_tool_snapshot: hiding the layer `hidden_layer_uid` (nullopt = every layer, Hide All) suspends the placement
    // of `placing` when that is a condition of the hidden layer; a condition on no layer is never suspended and a hide of another
    // layer leaves the placement alone. The caller sets `tool_revision` once the placement tool has been left.
    // `hidden_layer_is_image` (ost_domain::is_image_layer_uid of `hidden_layer_uid`): hiding the Image layer with the single toggle
    // never suspends anything, not even the placement of a condition on that layer (Windows "if not show and not image_layer", so
    // the tool stays armed on a hidden condition, whose taps ost_data::placement_tap_accepted refuses). Hide All (nullopt) has no such guard
    // in Windows, so the flag is only read together with a layer uid.
    inline std::optional<SuspendedPlacement> suspended_placement_for_layer_hide(const std::string &bid_uid, const Condition &placing,
                                                                                 const std::optional<std::string> &hidden_layer_uid,
                                                                                 bool hidden_layer_is_image = false)
    {
        if (hidden_layer_uid.has_value() && hidden_layer_is_image)
            return std::nullopt;
        if (placing.layer_uid.empty())
            return std::nullopt;
        if (hidden_layer_uid.has_value() && *hidden_layer_uid != placing.layer_uid)
            return std::nullopt;
        SuspendedPlacement suspended;
        suspended.condition_uid = placing.uid;
        suspended.layer_uid = placing.layer_uid;
        suspended.bid_uid = bid_uid;
        suspended.condition_type = placing.condition_type;
        return suspended;
    }

    // What the app looks like when a layer is shown (the inputs of _restore_suspended_layer_tool).
    struct PlacementRestoreState
    {
        std::string bid_uid;           // the bid that is open
        bool tool_in_view_mode = false; // plan_view.cursor_mode == select: no other tool is active
        unsigned long long tool_revision = 0;
        const Condition *condition = nullptr; // the suspended condition as it is now, nullptr when it is gone
        bool condition_visible = false;       // its layer is shown (ProjectData::is_condition_visible)
        bool takeoff_2d_view_active = false;
    };

    enum class SuspendedPlacementOutcome
    {
        Keep,   // the suspension stays (another layer was shown)
        Drop,   // the suspension is forgotten, nothing is restored
        Restore // the suspension is forgotten and the placement is entered again
    };

    // _restore_suspended_layer_tool(layer_uid): `shown_layer_uid` is the layer that was shown (nullopt = Show All). Another layer
    // keeps the suspension; a tool chosen since (another mode or a changed tool revision), another bid, a condition that is gone,
    // of another style or layer, or still hidden, or a view other than the 2D takeoff view drops it.
    // `shown_layer_is_image` (ost_domain::is_image_layer_uid of `shown_layer_uid`): showing the Image layer with the single toggle
    // does not touch the suspension at all (Windows "if show and not image_layer", checked before everything else here), so it
    // stays for the next show that names its layer or for Show All. Show All (nullopt) is not guarded.
    inline SuspendedPlacementOutcome restore_of_suspended_placement(const SuspendedPlacement &suspended,
                                                                    const std::optional<std::string> &shown_layer_uid,
                                                                    const PlacementRestoreState &now, bool shown_layer_is_image = false)
    {
        if (shown_layer_uid.has_value() && shown_layer_is_image)
            return SuspendedPlacementOutcome::Keep;
        if (shown_layer_uid.has_value() && *shown_layer_uid != suspended.layer_uid)
            return SuspendedPlacementOutcome::Keep;
        if (!now.tool_in_view_mode || now.tool_revision != suspended.tool_revision)
            return SuspendedPlacementOutcome::Drop;
        if (now.bid_uid != suspended.bid_uid)
            return SuspendedPlacementOutcome::Drop;
        if (now.condition != nullptr && now.condition->condition_type == suspended.condition_type &&
            now.condition->layer_uid == suspended.layer_uid && now.condition_visible && now.takeoff_2d_view_active)
            return SuspendedPlacementOutcome::Restore;
        return SuspendedPlacementOutcome::Drop;
    }

}
