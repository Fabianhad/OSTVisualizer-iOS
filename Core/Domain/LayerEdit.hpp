// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The rules of the M1 layer editing (plan 5.3): create, rename, delete, reorder. Everything a screen has to decide BEFORE it calls the
// store lives here, and the store (ost_store::insert_layer, rename_layer, delete_layers, swap_layer_sequence,
// Core/Persistence/ProjectStore/ProjectStore_Layers.hpp) re-runs the name rules inside its own transaction, so no screen can bypass them.
//
// Windows source of truth (49a2d59):
//   ost_visualizer/domain/entities/layer.py:9-14                      normalize_layer_name, is_comments_layer_name (Layer.hpp)
//   ost_visualizer/presentation/dialogs/layers_dialog.py:285-306      _can_modify, _can_delete, _find_layer_by_name, _max_sequence
//   ost_visualizer/presentation/dialogs/layers_dialog.py:315-336      New: after_sequence = the selected row's sequence, else the highest
//   ost_visualizer/presentation/dialogs/layers_dialog.py:397-436      rename and create paths: strip, blank, equal, duplicate, texts
//   ost_visualizer/presentation/components/layers_sidebar.py:294-354, :416-439   the same rules in the sidebar
//   ost_visualizer/presentation/dialogs/layers_dialog.py:487-544      delete flow
//   ost_visualizer/presentation/utils/messagebox.py:78-119            confirm_multi_delete (the delete plan and its texts)
//   ost_visualizer/presentation/dialogs/layers_dialog.py:628-654      move: the neighbour is the adjacent LISTED row (:636)
// The oddities Windows has and this file does NOT copy (plan section 8, register LY-1 to LY-14): the Comments layer is always taken
// (LY-1), a name has at most 50 UTF-16 units (LY-8), no control character or line break (LY-14), Sequence and UID stay within the
// Access Long (LY-7), a move never goes past a shared template row (LY-6).
#pragma once
#include "Layer.hpp"
#include <cstddef>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_domain
{

    // The Access Long (32 bit) limits of BidLayers.UID and BidLayers.Sequence (database_creator.py:306-314: UID COUNTER, Sequence INTEGER).
    // Windows has no guard (identity_allocation.py:5-8, layer_operations.py:88-97, :211-216: the driver fails and the whole mutation rolls
    // back with a raw text); iOS refuses before it writes anything (N-M1-04, register LY-7). All arithmetic on them is 64 bit:
    // BidLayer::sequence is an int, and column_int truncates, so a stored value beyond 32 bit must be read as 64 bit.
    inline constexpr long long kMaxLayerSequence = 2147483647LL;
    inline constexpr long long kMinLayerSequence = -2147483648LL;
    inline constexpr long long kMaxLayerUid = 2147483647LL;
    // BidLayers.Name is VARCHAR(50) (database_creator.py:310): at most 50 UTF-16 code units (ost_domain::utf16_length), N-M1-13.
    inline constexpr std::size_t kMaxLayerNameUnits = 50;

    // The Windows titles and texts (plan 2.11), exact. "Layer {name} already exists." is built by duplicate_layer_message.
    inline constexpr const char *kDuplicateLayerTitle = "Duplicate Layer";       // layers_dialog.py:405, :435; layers_sidebar.py:338, :350
    inline constexpr const char *kNewLayerTitle = "New Layer";                    // layers_dialog.py:469, :473
    inline constexpr const char *kRenameLayerTitle = "Rename Layer";              // layers_dialog.py:427
    inline constexpr const char *kMoveLayerTitle = "Move Layer";                  // layers_dialog.py:654
    inline constexpr const char *kDeleteLayerTitle = "Delete Layer";              // layers_dialog.py:506, :508, :542, :544
    inline constexpr const char *kRefreshErrorTitle = "Refresh Error";            // ui_event_coordinator.py:6747; since owner decision 2 only the create refresh failure uses it
    inline constexpr const char *kNewLayerFailed = "Failed to create layer.";     // layers_dialog.py:473
    inline constexpr const char *kRenameLayerFailed = "Failed to rename layer.";  // layers_dialog.py:417
    inline constexpr const char *kMoveLayerFailed = "Failed to move layer.";      // layers_dialog.py:646
    inline constexpr const char *kDeleteLayerFailed = "Failed to delete layer.";  // layers_dialog.py:544
    inline constexpr const char *kLayerUsageUnreadable = "Failed to validate layer usage."; // layers_dialog.py:506
    // Show All / Hide All in a bid whose layer list has no layer of its own (d26893c, ui_event_coordinator.py:6812-6855, texts.py:1321-1324 and
    // layers_dialog.py:700-716): the warning that replaced the old silent return (Windows owner note c of PORTING_BLOCKERS.md).
    inline constexpr const char *kLayerVisibilityTitle = "Layer Visibility";
    inline constexpr const char *kBidHasNoOwnLayersText = "This bid has no layers of its own to show or hide.";
    // The single show / hide of a shared template layer (a locked template row of no bid or of another bid) is refused (Windows d1c430a, owner note b,
    // texts.py UI_EVENT_COORDINATOR_MESSAGE_THIS_LAYER_IS_A_LOCKED_TEMPLATE, ui_event_coordinator.py:6564-6573), under kLayerVisibilityTitle.
    inline constexpr const char *kSharedTemplateLayerToggleText =
        "This layer is a locked template layer that other bids share, so it cannot be shown or hidden from this bid.";
    inline constexpr const char *kLayersDeletedRefreshFailed = "Layers were deleted, but the refresh failed."; // layers_dialog.py:540
    inline constexpr const char *kLayerCreatedRefreshFailed =
        "The layer was created, but the layer list could not be refreshed. Reopen the database to see the new layer."; // ui_event_coordinator.py:6585-6587

    // The iOS-only texts (plan 4.14, N-M1-03: the owner may reword them).
    inline constexpr const char *kLayerNameTooLongHint = "A layer name can have at most 50 characters.";
    inline constexpr const char *kLayerNameControlHint = "A layer name cannot contain control characters or line breaks.";
    inline constexpr const char *kLayerSequenceLimitText = "Cannot add a layer: no sequence number is left below 2147483647.";

    // "Layer {name} already exists." (layers_dialog.py:405, :435).
    std::string duplicate_layer_message(const std::string &name);

    // Windows _can_modify in the bid dialog (SELECT mode, layers_dialog.py:285-288) and the sidebar's editable rows (layers_sidebar.py:
    // :230-231): an own row that is neither a template nor locked. A locked copy of a template layer and a shared template row are not.
    bool can_modify_bid_layer(const BidLayer &layer);
    // Windows _can_delete (layers_dialog.py:290-291): can_modify, and in the bid dialog the built-in default UIDs 1-4 do not matter.
    bool can_delete_bid_layer(const BidLayer &layer);

    enum class LayerNameVerdict
    {
        Accept,           // write it: `name` is the stripped text to store
        Blank,            // blank after strip: nothing happens, no message (layers_dialog.py:397-400, :430-432)
        Unchanged,        // rename only: the stripped text equals the stored name exactly: nothing happens, no message (:401-402)
        Duplicate,        // another layer of the bid already has this name by strip().lower(): "Duplicate Layer" (:403-407, :433-436)
        Reserved,         // "comments" in any spelling: the Comments layer is always taken (N-M1-13, LY-1): the duplicate text
        TooLong,          // more than 50 UTF-16 units (N-M1-13, LY-8): the generic failure text and a hint line
        ControlCharacter  // a control character, a line break, a NUL or invalid UTF-8 (N-M1-13, LY-14): the same, with its own hint
    };

    struct LayerNameCheck
    {
        LayerNameVerdict verdict = LayerNameVerdict::Blank;
        // The typed text after Python strip() (Unicode whitespace); stored exactly like that (case and inner spaces kept).
        std::string name;
        // What to show. Both empty for Accept, Blank and Unchanged (silent). Duplicate and Reserved: kDuplicateLayerTitle and
        // duplicate_layer_message(name). TooLong and ControlCharacter: kNewLayerTitle or kRenameLayerTitle, and the generic failure
        // text (kNewLayerFailed / kRenameLayerFailed), a line break and the hint line.
        std::string title, message;
    };

    // The name rules of create and rename, in this order (the first that applies decides):
    //   1. name = strip(typed); empty -> Blank
    //   2. rename only (exclude_uid not empty) and name == stored_name (exact, case sensitive) -> Unchanged
    //   3. has_control_character(name) || has_line_break(name) (TextLimits.hpp; invalid UTF-8 counts) -> ControlCharacter
    //   4. utf16_length(name) > kMaxLayerNameUnits -> TooLong
    //   5. is_comments_layer_name(name) -> Reserved
    //   6. normalize_layer_name(name) equals the normalized name of a row of `listed` or an entry of `own_layers` whose uid is not
    //      exclude_uid -> Duplicate (the layer's own row never counts, so a change of letter case alone is accepted)
    //   7. else Accept.
    // `listed` = the merged list of the sheet (shared template rows and locked copies included, no Comments row), `own_layers` = the bid's
    // layers by uid (ProjectData::layers; it includes the hidden Comments row and any row the merge shadowed, so the check is a superset
    // of Windows' listed rows: stricter, never looser). exclude_uid = the uid of the row being renamed, "" for create (which is also what
    // tells create from rename for the titles). `stored_name` is only used by step 2.
    LayerNameCheck check_layer_name(const std::string &typed, const std::string &stored_name, const std::vector<BidLayer> &listed,
                                    const std::unordered_map<std::string, Layer> &own_layers, const std::string &exclude_uid);

    // New Layer position (layers_dialog.py:299-306, :315-322; layers_sidebar.py get_selected_sequence / get_max_sequence): the sequence of the
    // listed row with uid `selected_uid`; when there is none (empty uid, uid not listed) the highest sequence of `listed` (0 for an empty
    // list). No +1 here: the store gives the new row after_sequence + 1. Pure, no overflow, whatever the sequences are.
    long long new_layer_after_sequence(const std::vector<BidLayer> &listed, const std::string &selected_uid);

    struct LayerMovePlan
    {
        bool allowed = false;
        // The adjacent LISTED row the sequence is swapped with (layers_dialog.py:628-654: neighbour = listed[row + direction]), "" unless allowed.
        std::string neighbor_uid;
        // Why not (a short reason for logs and tests), empty when allowed. The screens show kMoveLayerFailed or hide the item.
        std::string refusal;
    };

    // Move Up (direction -1) / Move Down (+1) of the row `uid` of `listed`. Allowed when: direction is -1 or +1, the row is listed, it is one of
    // the bid's own (bid_uid), neither template nor locked (can_modify_bid_layer), the neighbour exists (the ends of the list have none) and
    // the neighbour is not a shared template row (is_shared_template_layer: LY-6; Windows raises a raw scope error there). A locked copy of the
    // bid's own as the neighbour is allowed (N-M1-19: its sequence is rewritten). The neighbour is the next row of `listed` in list order,
    // never "the next sequence value" (ties and gaps stay as they are).
    LayerMovePlan plan_layer_move(const std::vector<BidLayer> &listed, const std::string &bid_uid, const std::string &uid, int direction);

    struct LayerRef
    {
        std::string uid, name;
    };

    enum class LayerDeleteKind
    {
        NothingToDelete, // no selected uid is a deletable listed row: nothing happens, no message (layers_dialog.py:490-496)
        OnlyInUse,       // every deletable row is in use: a warning, no question (messagebox.py:101-106)
        ConfirmMixed,    // some in use, some not: the in-use lines and "Delete the other {n} item?" / "items?" (messagebox.py:295-317)
        Confirm          // none in use: "Delete {n} item?" / "items?" (messagebox.py:318-330)
    };

    struct LayerDeletePlan
    {
        LayerDeleteKind kind = LayerDeleteKind::NothingToDelete;
        std::vector<LayerRef> deletable; // the rows to delete when the user confirms (not in use), in the order of `selected_uids`
        std::vector<LayerRef> in_use;    // the rows that are refused, in the same order
        // kDeleteLayerTitle and the Windows text for every kind but NothingToDelete (both empty then). OnlyInUse: one line
        // 'Cannot delete "{name}" because it is in use.' per row, joined by '\n'; ConfirmMixed: those lines, "\n\n" and
        // "Delete the other {n} item?" (n == 1) or "... {n} items?"; Confirm: "Delete {n} item?" (n == 1) or "Delete {n} items?" (d26893c: the
        // singular and the plural texts; n is never 0 here, a plan with nothing to delete is NothingToDelete).
        std::string title, message;
    };

    // The Windows delete flow as a pure decision: of `selected_uids` only the listed rows that can_delete_bid_layer count (a locked copy, a
    // shared row and an unknown uid are dropped silently: layers_dialog.py:490-496, test_layers_dialog.py:824), each uid once, in order; a
    // row whose uid text is in `used_uids` is in use (messagebox.py:84-85). `used_uids` is the usage read when the action starts
    // (ost_store::read_layer_uids_in_use); a caller whose read failed does not call this and refuses (LY-2).
    LayerDeletePlan plan_layer_delete(const std::vector<BidLayer> &listed, const std::vector<std::string> &selected_uids,
                                      const std::set<std::string> &used_uids);

    // 32-bit guard of a new layer (LY-7): true when the write must be refused BEFORE anything is shifted, i.e. when the new row's sequence
    // after_sequence + 1 is outside [kMinLayerSequence, kMaxLayerSequence], or when a later row (Sequence > after_sequence) with the sequence
    // `highest_later_sequence` would pass kMaxLayerSequence when it is shifted by one (highest_later_sequence >= kMaxLayerSequence). -1 stands
    // for "no later row". 64-bit safe for every input (LLONG_MAX, LLONG_MIN).
    bool layer_sequence_would_overflow(long long after_sequence, long long highest_later_sequence);

}
