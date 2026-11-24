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
// iOS-only (plan 4.14, N-M1-03): the DB-locked hint after the generic failure texts of create / rename / move (N-M1-17, plan 5.3), and the refresh
// text of a rename and a move that committed (register CF-8 is the folder twin: Windows answers "Failed to rename layer." for a rename that DID
// commit when only the reload failed, project_write_service.py:6845-6847).
#include "LayerEditTestSupport.hpp"

#include "Domain/BrowserRules.hpp"
#include "Domain/Layer.hpp"
#include "Domain/LayerEdit.hpp"
#include "Domain/LayerSheet.hpp"

#include <string>
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

    constexpr const char *kHint = "The database may be locked by another application.";
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

// layers_dialog.py:417, :473, :646, :544 with the titles of :427, :469, :654, :544, and the hint of ui_event_coordinator.py:6648-6652. The pinned strings
// are literals on purpose: a reworded constant must fail here.
TEST(LayerSheet, WriteFailureTextsAreTheWindowsTextsPlusTheHint)
{
    const ost_domain::DialogText create = ost_domain::layer_write_failure_text(LayerAction::Create, "");
    CHECK_STR_EQ(create.title, "New Layer");
    CHECK_STR_EQ(create.message, std::string("Failed to create layer. ") + kHint);
    const ost_domain::DialogText rename = ost_domain::layer_write_failure_text(LayerAction::Rename, "");
    CHECK_STR_EQ(rename.title, "Rename Layer");
    CHECK_STR_EQ(rename.message, std::string("Failed to rename layer. ") + kHint);
    const ost_domain::DialogText move = ost_domain::layer_write_failure_text(LayerAction::Move, "");
    CHECK_STR_EQ(move.title, "Move Layer");
    CHECK_STR_EQ(move.message, std::string("Failed to move layer. ") + kHint);
    const ost_domain::DialogText del = ost_domain::layer_write_failure_text(LayerAction::Delete, "");
    CHECK_STR_EQ(del.title, "Delete Layer");
    CHECK_STR_EQ(del.message, std::string("Failed to delete layer. ") + kHint);
    CHECK_STR_EQ(ost_domain::DB_LOCKED_HINT, kHint);
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

// ui_event_coordinator.py:6584-6587 (create) and layers_dialog.py:540 (delete) are Windows texts; a rename and a move that committed get the
// iOS sentence (Windows says "Failed to ..." there although the change was written, register CF-8's twin).
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
    CHECK_STR_EQ(rename.title, "Refresh Error");
    CHECK_STR_EQ(rename.message, "The change was saved, but the layer list could not be refreshed. Reopen the database to see it.");
    const ost_domain::DialogText move = ost_domain::layer_refresh_failure_text(LayerAction::Move);
    CHECK_STR_EQ(move.title, "Refresh Error");
    CHECK_STR_EQ(move.message, rename.message);
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
