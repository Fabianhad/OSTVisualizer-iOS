// M1 layers UI (plan 5.3 touch adaptation, N-M1-17, N-M1-19): ost_domain::LayerSheet (Core/Domain/LayerSheet.hpp), the decisions of the Layers sheet
// that the Objective-C only converts and shows: which actions a row offers (the swipe, the long-press menu) and the texts of a write that
// failed or that committed but could not be shown.
//
// Windows oracle (49a2d59):
//   ost_visualizer/presentation/dialogs/layers_dialog.py:285-291   _can_modify / _can_delete (a row is editable when it is neither a template nor
//       locked); :315-322 New runs with ANY row selected (the new layer goes after that row's sequence); :628-654 _move_selected (the selected row
//       must be modifiable, target = row +- 1 inside the listed rows); :656-684 button states (Move Up needs row > 0, Move Down row < len - 1)
//   layers_dialog.py:417 / :473 / :646 / :544  "Failed to rename layer." / "Failed to create layer." / "Failed to move layer." / "Failed to delete layer."
//       with the titles "Rename Layer" / "New Layer" / "Move Layer" / "Delete Layer"
//   ui_event_coordinator.py:6648-6652  the sidebar's delete failure: "Delete Layer" / "Failed to delete layer. " + DB_LOCKED_HINT
//   ui_event_coordinator.py:6584-6587  "Refresh Error" / "The layer was created, but the layer list could not be refreshed. Reopen the database to
//       see the new layer."; layers_dialog.py:540 "Delete Layer" / "Layers were deleted, but the refresh failed."
//   messagebox.py:8 DB_LOCKED_HINT = "The database may be locked by another application."
// Owner decision 2 (parity pass 3): the dialog titles and texts are taken literally; the DB-locked hint of N-M1-17 is dropped (the dialog has none).
// Windows d26893c (parity-sync-2): the rename / move refresh texts (texts.py:1306-1313, 0a24294) and the Show All / Hide All message of a bid without
// layers of its own (texts.py:1321-1324).
#include "LayerEditTestSupport.hpp"

#include "Domain/BrowserRules.hpp"
#include "Domain/Layer.hpp"
#include "Domain/LayerEdit.hpp"
#include "Domain/LayerSheet.hpp"

#include <string>
#include <unordered_map>
#include <vector>

using ost_domain::BidLayer;
using ost_domain::LayerAction;
using ost_domain::LayerRowActions;
using ost_domain::layer_row_actions;

namespace
{
    BidLayer row(const std::string &uid, const std::string &name, int sequence, const std::string &bid = "10", bool is_template = false,
                 bool is_locked = false)
    {
        BidLayer layer;
        layer.uid = uid;
        layer.bid_uid = bid;
        layer.name = name;
        layer.show = true;
        layer.sequence = sequence;
        layer.is_template = is_template;
        layer.is_locked = is_locked;
        return layer;
    }

    BidLayer shared(const std::string &uid, const std::string &name, int sequence)
    {
        return row(uid, name, sequence, "", true, true);
    }

    // The M1 device-pack list (plan 6.3) as the sheet shows it, in list order: three locked own copies, three own layers, one shared row.
    std::vector<BidLayer> sheet()
    {
        return {row("101", "Image", 0, "10", false, true), row("102", "Annotation", 1, "10", false, true),
                row("103", "Default", 2, "10", false, true), row("105", "Walls", 4),
                row("106", "Floors", 5),                     row("107", "Unused", 6),
                shared("8", "Shared Template", 7)};
    }

    // The six flags of an answer in one text, so that a failure names the row.
    std::string flags(const LayerRowActions &a)
    {
        return std::string(a.listed ? "L" : "-") + (a.can_add_below ? "A" : "-") + (a.can_rename ? "R" : "-") + (a.can_delete ? "D" : "-") +
               (a.can_move_up ? "U" : "-") + (a.can_move_down ? "N" : "-");
    }

}

// layers_dialog.py:285-291, :315-322, :628-654, :656-684 on the device-pack list. Own locked copies and the shared row are not editable but a
// new layer can still go after them; the neighbour of a move is the next LISTED row, a locked own copy is a valid neighbour (N-M1-19) and the shared
// row is not (LY-6).
TEST(LayerSheet, RowActionsOfTheDevicePackList)
{
    const std::vector<BidLayer> list = sheet();
    // listed, add-below, rename, delete, move up, move down
    CHECK_STR_EQ(flags(layer_row_actions(list, "10", "101")), "LA----"); // Image: first row, locked copy
    CHECK_STR_EQ(flags(layer_row_actions(list, "10", "102")), "LA----"); // Annotation: locked copy
    CHECK_STR_EQ(flags(layer_row_actions(list, "10", "103")), "LA----"); // Default: locked copy
    CHECK_STR_EQ(flags(layer_row_actions(list, "10", "105")), "LARDUN"); // Walls: Default (a locked own copy) above, Floors below
    CHECK_STR_EQ(flags(layer_row_actions(list, "10", "106")), "LARDUN");
    CHECK_STR_EQ(flags(layer_row_actions(list, "10", "107")), "LARDU-"); // Unused: the shared row below disables Move Down
    CHECK_STR_EQ(flags(layer_row_actions(list, "10", "8")), "LA----");   // the shared template row
}

// A uid the list does not have offers nothing at all (a stale row), also not "New Layer Below".
TEST(LayerSheet, AnUnlistedRowOffersNothing)
{
    CHECK_STR_EQ(flags(layer_row_actions(sheet(), "10", "999")), "------");
    CHECK_STR_EQ(flags(layer_row_actions(sheet(), "10", "")), "------");
    CHECK_STR_EQ(flags(layer_row_actions({}, "10", "105")), "------");
}

// layers_dialog.py:656-684: the ends of the list have no neighbour; one row alone cannot move; Windows allows New and Delete on it.
TEST(LayerSheet, OneOwnLayerAloneAndTheEndsOfTheList)
{
    CHECK_STR_EQ(flags(layer_row_actions({row("5", "Only", 1)}, "10", "5")), "LARD--");
    const std::vector<BidLayer> two = {row("5", "First", 1), row("6", "Second", 2)};
    CHECK_STR_EQ(flags(layer_row_actions(two, "10", "5")), "LARD-N");
    CHECK_STR_EQ(flags(layer_row_actions(two, "10", "6")), "LARDU-");
}

// The neighbour is the next row of the LIST, never "the next sequence value": a tie or a gap does not change what is offered
// (layers_dialog.py:636, LY-12 / O13 keep a move on equal sequences a success that changes nothing visible).
TEST(LayerSheet, TiesAndGapsDoNotChangeWhatIsOffered)
{
    const std::vector<BidLayer> list = {row("5", "A", 3), row("6", "B", 3), row("7", "C", 90)};
    CHECK_STR_EQ(flags(layer_row_actions(list, "10", "5")), "LARD-N");
    CHECK_STR_EQ(flags(layer_row_actions(list, "10", "6")), "LARDUN");
    CHECK_STR_EQ(flags(layer_row_actions(list, "10", "7")), "LARDU-");
}

// A row that is no row of the open bid is never renamed, deleted or moved (plan_layer_move refuses a row that is not the bid's own, the store
// refuses a layer of another bid, LY-9): the sheet lists only the bid's rows and the shared ones, so this is the defensive half.
TEST(LayerSheet, ARowOfAnotherBidIsNotMovable)
{
    const std::vector<BidLayer> list = {row("5", "A", 1), row("6", "B", 2, "99"), row("7", "C", 3)};
    const LayerRowActions other = layer_row_actions(list, "10", "6");
    CHECK(other.listed);
    CHECK(other.can_add_below);
    CHECK_FALSE(other.can_rename);
    CHECK_FALSE(other.can_delete);
    CHECK_FALSE(other.can_move_up);
    CHECK_FALSE(other.can_move_down);
    // the same row seen from the bid it belongs to is its own editable row
    const LayerRowActions mine = layer_row_actions(list, "99", "6");
    CHECK(mine.can_rename);
    CHECK(mine.can_delete);
}

// layers_dialog.py:417, :473, :646, :544 with the titles of :427, :469, :654, :544. The dialog never adds DB_LOCKED_HINT (it exists only in the sidebar,
// ui_event_coordinator.py:6819-6825); owner decision 2 takes the dialog text literally, so there is no hint (this drops N-M1-17). The pinned strings
// are literals on purpose: a reworded constant must fail here.
TEST(LayerSheet, WriteFailureTextsAreTheWindowsDialogTexts)
{
    const ost_domain::DialogText create = ost_domain::layer_write_failure_text(LayerAction::Create, "");
    CHECK_STR_EQ(create.title, "New Layer");
    CHECK_STR_EQ(create.message, "Failed to create layer.");
    const ost_domain::DialogText rename = ost_domain::layer_write_failure_text(LayerAction::Rename, "");
    CHECK_STR_EQ(rename.title, "Rename Layer");
    CHECK_STR_EQ(rename.message, "Failed to rename layer.");
    const ost_domain::DialogText move = ost_domain::layer_write_failure_text(LayerAction::Move, "");
    CHECK_STR_EQ(move.title, "Move Layer");
    CHECK_STR_EQ(move.message, "Failed to move layer.");
    const ost_domain::DialogText del = ost_domain::layer_write_failure_text(LayerAction::Delete, "");
    CHECK_STR_EQ(del.title, "Delete Layer");
    CHECK_STR_EQ(del.message, "Failed to delete layer.");
    // no generic text carries the hint
    for (const ost_domain::DialogText *text : {&create, &rename, &move, &del})
        CHECK(text->message.find(ost_domain::DB_LOCKED_HINT) == std::string::npos);
}

// A text the store gave for the user (the duplicate text, the sequence limit, the identity limit) replaces the generic text: no hint after it,
// because the cause is not a locked database.
TEST(LayerSheet, AStoreTextReplacesTheGenericText)
{
    const ost_domain::DialogText limit = ost_domain::layer_write_failure_text(LayerAction::Create, ost_domain::kLayerSequenceLimitText);
    CHECK_STR_EQ(limit.title, "New Layer");
    CHECK_STR_EQ(limit.message, "Cannot add a layer: no sequence number is left below 2147483647.");
    const ost_domain::DialogText duplicate = ost_domain::layer_write_failure_text(LayerAction::Rename, "Layer Walls already exists.");
    CHECK_STR_EQ(duplicate.title, "Rename Layer");
    CHECK_STR_EQ(duplicate.message, "Layer Walls already exists.");
    const ost_domain::DialogText move = ost_domain::layer_write_failure_text(LayerAction::Move, "x");
    CHECK_STR_EQ(move.message, "x");
    const ost_domain::DialogText del = ost_domain::layer_write_failure_text(LayerAction::Delete, "y");
    CHECK_STR_EQ(del.message, "y");
}

// ui_event_coordinator.py:6584-6587 (create), layers_dialog.py:540 (delete) and, since d26893c (0a24294), the rename and the move
// (texts.py:1306-1313 UI_EVENT_COORDINATOR_MESSAGE_THE_LAYER_WAS_RENAMED_BUT_THE / _MOVED_BUT_THE, the twins at texts.py:1645-1652, tests
// test_ui_event_coordinator.py:14173, test_layers_dialog.py:1072) are Windows texts. Owner decision 2: the dialog's titles, layers_dialog.py:489 and :784
// (LAYERS_DIALOG_TITLE_RENAME_LAYER / _MOVE_LAYER); only the create refresh keeps "Refresh Error" (condition_action_handler.py:416-430 path).
TEST(LayerSheet, RefreshFailureTextsSayWhatWasCommitted)
{
    const ost_domain::DialogText create = ost_domain::layer_refresh_failure_text(LayerAction::Create);
    CHECK_STR_EQ(create.title, "Refresh Error");
    CHECK_STR_EQ(create.message,
                 "The layer was created, but the layer list could not be refreshed. Reopen the database to see the new layer.");
    const ost_domain::DialogText del = ost_domain::layer_refresh_failure_text(LayerAction::Delete);
    CHECK_STR_EQ(del.title, "Delete Layer");
    CHECK_STR_EQ(del.message, "Layers were deleted, but the refresh failed.");
    const ost_domain::DialogText rename = ost_domain::layer_refresh_failure_text(LayerAction::Rename);
    CHECK_STR_EQ(rename.title, "Rename Layer");
    CHECK_STR_EQ(rename.message,
                 "The layer was renamed, but the layer list could not be refreshed. Reopen the database to see the change.");
    const ost_domain::DialogText move = ost_domain::layer_refresh_failure_text(LayerAction::Move);
    CHECK_STR_EQ(move.title, "Move Layer");
    CHECK_STR_EQ(move.message,
                 "The layer was moved, but the layer list could not be refreshed. Reopen the database to see the change.");
    CHECK_NE(move.message, rename.message);
    CHECK_NE(rename.title, move.title);
}

// Windows update_all_layers_visibility_deferred (ui_event_coordinator.py:6812-6855, d26893c): a Show All / Hide All in a bid whose layer list has no
// layer of its own (only shared rows, or nothing) returns WriteReloadResult(user_message=UI_EVENT_COORDINATOR_MESSAGE_THIS_BID_HAS_NO_LAYERS_OF_ITS)
// and _on_layers_show_all shows it as a warning titled UI_EVENT_COORDINATOR_TITLE_LAYER_VISIBILITY; the Layers dialog shows the same text
// (layers_dialog.py:700-716). Tests: test_ui_event_coordinator.py:14256-14274 (test_hide_all_with_only_shared_layers_tells_the_user_in_the_sidebar_too),
// test_layers_dialog.py (test_hide_all_refused_with_a_reason_shows_that_reason). A bid with its own layers that already have the requested
// visibility is a success without a write (return True): no message.
TEST(LayerSheet, BulkVisibilityTextsAreTheWindowsTexts)
{
    CHECK_STR_EQ(ost_domain::kLayerVisibilityTitle, "Layer Visibility");
    CHECK_STR_EQ(ost_domain::kBidHasNoOwnLayersText, "This bid has no layers of its own to show or hide.");
}

TEST(LayerSheet, ShowAllAndHideAllTellWhenTheBidHasNoLayersOfItsOwn)
{
    using ost_domain::BulkLayerOutcome;
    using ost_domain::BulkLayerPlan;
    using ost_domain::Layer;
    const std::string bid = "10";
    const std::vector<BidLayer> only_shared = {shared("8", "Shared Template", 7)};
    const std::vector<BidLayer> own_walls = {row("105", "Walls", 4)};
    Layer walls;
    walls.uid = "105";
    walls.name = "Walls";
    walls.visible = true;
    const std::unordered_map<std::string, Layer> live = {{"105", walls}};
    const std::unordered_map<std::string, Layer> none;
    for (const bool show : {false, true})
    {
        SCOPED_CONTEXT(show ? "Show All" : "Hide All");
        // [shared template row] and [] -> NoOwnLayers -> the alert
        for (const std::vector<BidLayer> &listed : {only_shared, std::vector<BidLayer>{}})
        {
            const BulkLayerPlan plan = ost_domain::plan_bulk_layer_visibility(listed, bid, none, show);
            CHECK(plan.outcome == BulkLayerOutcome::NoOwnLayers);
            const ost_domain::DialogText notice = ost_domain::bulk_layer_visibility_notice(plan);
            CHECK_STR_EQ(notice.title, "Layer Visibility");
            CHECK_STR_EQ(notice.message, "This bid has no layers of its own to show or hide.");
        }
    }
    // an own layer that already has the requested visibility is a success: no alert; one that differs is written: no alert
    const BulkLayerPlan there = ost_domain::plan_bulk_layer_visibility(own_walls, bid, live, true);
    CHECK(there.outcome == BulkLayerOutcome::AlreadyThere);
    CHECK(ost_domain::bulk_layer_visibility_notice(there).title.empty());
    CHECK(ost_domain::bulk_layer_visibility_notice(there).message.empty());
    const BulkLayerPlan write = ost_domain::plan_bulk_layer_visibility(own_walls, bid, live, false);
    CHECK(write.outcome == BulkLayerOutcome::Write);
    CHECK(ost_domain::bulk_layer_visibility_notice(write).title.empty());
    CHECK(ost_domain::bulk_layer_visibility_notice(write).message.empty());
}

// Windows d1c430a (owner note b): update_layer_visibility_deferred (ui_event_coordinator.py:6564-6573) refuses the single show / hide of a shared
// template layer (is_shared_template_layer(layer, bid_ref.bid_uid): locked template row of no bid or of another bid): it returns
// WriteReloadResult(user_message=UI_EVENT_COORDINATOR_MESSAGE_THIS_LAYER_IS_A_LOCKED_TEMPLATE) before anything is scheduled, and the sidebar
// (_on_layer_visibility_toggled :6525-6535) and the Layers dialog (layers_dialog.py:677-683) show it under "Layer Visibility". Tests:
// test_ui_event_coordinator.py:11507 (test_a_single_toggle_of_a_shared_locked_template_layer_is_refused: the layers l1 (own), "1" Image (no bid) and
// "23" Callouts (bid-2), both locked templates, are refused with the text, nothing queued, nothing changed) and :11530
// (test_the_sidebar_shows_the_shared_layer_refusal_and_restores_the_tick), test_layers_dialog.py:996. The Image layer has no exemption.
TEST(LayerSheet, SingleToggleOfASharedTemplateLayerTextsAreTheWindowsTexts)
{
    CHECK_STR_EQ(ost_domain::kSharedTemplateLayerToggleText,
                 "This layer is a locked template layer that other bids share, so it cannot be shown or hidden from this bid.");
    CHECK_STR_EQ(ost_domain::kLayerVisibilityTitle, "Layer Visibility");
}

TEST(LayerSheet, SingleToggleOfASharedTemplateLayerIsRefusedWithTheTextAndTheTitle)
{
    // the list of test_a_single_toggle_of_a_shared_locked_template_layer_is_refused, for the open bid "bid-1"
    const std::vector<BidLayer> listed = {row("l1", "Layer 1", 1, "bid-1"), shared("1", "Image", 0),
                                          row("23", "Callouts", 4, "bid-2", true, true)};
    for (const char *uid : {"1", "23"})
    {
        SCOPED_CONTEXT(std::string("uid ") + uid);  // not SCOPED_CONTEXT(uid): std::string(uid) would parse as a declaration (-Wvexing-parse)
        const ost_domain::DialogText refusal = ost_domain::single_layer_visibility_refusal(listed, "bid-1", uid);
        CHECK_STR_EQ(refusal.title, "Layer Visibility");
        CHECK_STR_EQ(refusal.message,
                     "This layer is a locked template layer that other bids share, so it cannot be shown or hidden from this bid.");
    }
}

// Everything else is toggled as before (no title, no message): the bid's own layer, the bid's own locked copy of a template layer (not shared),
// a template row that is not locked, a plain layer of another bid (Windows writes it; it cannot be in the merged list), and a uid that is not
// listed (the store decides: Windows returns False there and the sheet shows its generic failure).
TEST(LayerSheet, SingleToggleOfAnyOtherRowIsNotRefused)
{
    const std::vector<BidLayer> listed = {row("l1", "Layer 1", 1, "bid-1"),
                                          row("13", "Annotation", 2, "bid-1", true, true),
                                          row("101", "Image", 0, "bid-1", false, true),
                                          row("2", "Default", 3, "", true, false),
                                          row("9", "Other", 4, "bid-2")};
    for (const char *uid : {"l1", "13", "101", "2", "9", "404", ""})
    {
        SCOPED_CONTEXT(std::string("uid ") + uid);  // not SCOPED_CONTEXT(uid): std::string(uid) would parse as a declaration (-Wvexing-parse)
        const ost_domain::DialogText refusal = ost_domain::single_layer_visibility_refusal(listed, "bid-1", uid);
        CHECK(refusal.title.empty());
        CHECK(refusal.message.empty());
    }
    // with no bid open the rows of no bid are not shared ('' == ''), as is_shared_template_layer says
    CHECK(ost_domain::single_layer_visibility_refusal({shared("1", "Image", 0)}, "", "1").message.empty());
}

// None of the committed-but-not-shown texts claims a failure, and none of the failure texts claims a success.
TEST(LayerSheet, RefreshTextsAreNeverTheFailureTexts)
{
    for (const LayerAction action : {LayerAction::Create, LayerAction::Rename, LayerAction::Move, LayerAction::Delete})
    {
        const ost_domain::DialogText failed = ost_domain::layer_write_failure_text(action, "");
        const ost_domain::DialogText refreshed = ost_domain::layer_refresh_failure_text(action);
        CHECK_NE(failed.message, refreshed.message);
        CHECK(failed.message.find("Failed to") == 0);
        CHECK(refreshed.message.find("Failed to") == std::string::npos);
        CHECK_FALSE(refreshed.message.empty());
    }
}
