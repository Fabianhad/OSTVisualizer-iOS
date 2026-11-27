// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The decisions of the Layers sheet that the screen (ViewController+LayerEditing.mm) only converts and shows (plan 5.3 touch adaptation, N-M1-17,
// N-M1-19): which actions a row offers in the swipe and in the long-press menu, and the texts of a write that failed or that committed but whose
// list could not be read again. The name rules, the move plan, the delete plan and the Windows titles live in LayerEdit.hpp; this header only
// combines them for one row and builds the alert texts.
//
// Windows source of truth (49a2d59):
//   ost_visualizer/presentation/dialogs/layers_dialog.py:285-291    _can_modify / _can_delete
//   layers_dialog.py:315-322                                        New runs with ANY row selected (after that row's sequence)
//   layers_dialog.py:628-654, :656-684                              move: the selected row must be modifiable, the ends of the list have no neighbour
//   layers_dialog.py:417, :473, :646, :544                          "Failed to rename / create / move / delete layer."
//   presentation/coordinators/ui_event_coordinator.py:6648-6652     the sidebar's delete failure adds DB_LOCKED_HINT
//   ui_event_coordinator.py:6584-6587, layers_dialog.py:540          the committed-but-not-refreshed texts of create and delete
//   presentation/utils/messagebox.py:8                              DB_LOCKED_HINT
#pragma once
#include "BrowserRules.hpp"
#include "Layer.hpp"
#include <string>
#include <vector>

namespace ost_domain
{

    // A rename or a move that was written, whose layer list could not be read again. Windows 49a2d59 answered "Failed to rename layer." / "Failed to
    // move layer." there although the change committed; since d26893c (0a24294) it says what was kept (texts.py:1306-1313, the dialog twins
    // texts.py:1645-1652, tests test_ui_event_coordinator.py:14173 and test_layers_dialog.py:1072). The title stays "Refresh Error" (the sidebar's).
    inline constexpr const char *kLayerRenamedRefreshFailed =
        "The layer was renamed, but the layer list could not be refreshed. Reopen the database to see the change.";
    inline constexpr const char *kLayerMovedRefreshFailed =
        "The layer was moved, but the layer list could not be refreshed. Reopen the database to see the change.";

    // What one row of the sheet offers. A row that is not in the list offers nothing (every flag false, also can_add_below).
    struct LayerRowActions
    {
        bool listed = false;
        // "New Layer Below": Windows runs New with any row selected, a locked copy and a shared row included (the new layer goes after that row's
        // sequence). True for every listed row.
        bool can_add_below = false;
        // Rename and Delete: the bid's own row that is neither a template nor locked (can_modify_bid_layer, can_delete_bid_layer). A locked copy of
        // a template layer, a shared template row and a row of another bid are not.
        bool can_rename = false;
        bool can_delete = false;
        // plan_layer_move(.., -1 / +1).allowed: the row is editable, the list has a neighbour in that direction and the neighbour is not a shared
        // row. A locked copy of the bid's own is a valid neighbour.
        bool can_move_up = false;
        bool can_move_down = false;
    };

    // The actions of the row `uid` of the sheet's list `listed` for the open bid `bid_uid`.
    LayerRowActions layer_row_actions(const std::vector<BidLayer> &listed, const std::string &bid_uid, const std::string &uid);

    enum class LayerAction
    {
        Create,
        Rename,
        Move,
        Delete
    };

    // A write that was refused or failed (nothing changed). The title is the Windows title of the action ("New Layer", "Rename Layer", "Move
    // Layer", "Delete Layer"). The message is `store_error` when the store gave a text for the user (the duplicate text, a limit text), else the
    // Windows generic text of the action ("Failed to create layer.", ...) followed by one space and DB_LOCKED_HINT (plan 5.3, N-M1-17: Windows
    // adds the hint to the delete failure of the sidebar, ui_event_coordinator.py:6648-6652; the other three get it the same way on iOS).
    DialogText layer_write_failure_text(LayerAction action, const std::string &store_error);

    // A write that committed but whose layer list could not be read again; the new state is kept. Create: "Refresh Error" and kLayerCreatedRefreshFailed;
    // Delete: "Delete Layer" and kLayersDeletedRefreshFailed (both Windows); Rename and Move: "Refresh Error" and kLayerRenamedRefreshFailed /
    // kLayerMovedRefreshFailed (Windows d26893c).
    DialogText layer_refresh_failure_text(LayerAction action);

    // What a Show All / Hide All tells the user when it did not write (Windows d26893c, update_all_layers_visibility_deferred
    // ui_event_coordinator.py:6812-6855 and layers_dialog._set_all_show :700-716): NoOwnLayers (the layer list has only shared rows, or nothing)
    // gives {kLayerVisibilityTitle, kBidHasNoOwnLayersText}; AlreadyThere (a success, nothing to write) and Write give an empty title and message:
    // no alert. Nothing is written in any of these cases by this function; it only decides the text.
    DialogText bulk_layer_visibility_notice(const BulkLayerPlan &plan);

}
