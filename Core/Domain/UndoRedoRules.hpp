// Layer: Domain (pure C++)
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// A user-triggered undo / redo that does not run (parity pass 3, owner decision "PORT"): when it is refused, the user is told, and the entry STAYS in the
// history. This file holds the DECISION (is the entry stale, which of the four texts, does the user see it) and the one step that applies it to an
// UndoHistory; the Objective-C UndoRedoController only builds the references of an entry and shows the alert.
//
// Windows source of truth (HEAD d1c430a):
//   ost_visualizer/presentation/services/undo_redo_service.py:25-26      HistoryTargetUnavailableError: "the page or item of a history entry no longer exists"
//   undo_redo_service.py:449-479  undo() / redo(user_initiated=True) return bool; an empty stack, a pending transition, a locked bid give False without a dialog
//   undo_redo_service.py:534-567  _submit_history_transition: an exception while the entry is submitted shows show_error("Undo" / "Redo", text) when the call is
//                                 user_initiated and only logs when it is not (an automatic / AI undo); the entry returns to READY on its stack
//   undo_redo_service.py:569-582  _history_failure_message: HistoryTargetUnavailableError -> the "because the page or item it belongs to no longer exists" text,
//                                 every other exception -> the generic text
//   texts.py:1752-1767            the two titles and the four messages (copied below, character by character)
//   plan_view_action_handler.py:1716-1735  _replay_page_scale / _replay_takeoff_scale / _replay_annotation_scale: the replay of a position rescales it with the
//                                 CURRENT scale of the page of the item and raises HistoryTargetUnavailableError when the page, or the item, is gone (before d1c430a
//                                 a missing page answered the pair (1.0, 1.0): the items came back on a deleted page at the wrong scale);
//                                 :1593-1597 _property_history_updates: a takeoff whose entry replays a property (the negative flag) that is gone, or that no longer
//                                 owns its page, is the same error. Applied to: positions of takeoffs (:1798), takeoff specs of a delete / place (:1847, :1865),
//                                 saved annotations (:1883).
// What is the same on iOS and what is not:
//   - The entry kinds iOS has are Move, MoveGroup (positions of takeoffs that exist), Place, Delete (takeoffs that are re-created), SetNegative (a property of a
//     takeoff that exists). The undo of a Place and the redo of a Delete only remove takeoffs: Windows has no replay position there and no check.
//   - Windows shows the generic text only when the submit raises (an SQL queue failure); a local action that answers False or fails inside the action is logged and
//     answers REJECTED / FAILED_BEFORE_COMMIT without a dialog. The owner decided that iOS says so for EVERY user-triggered undo / redo that does not advance
//     (an empty history is not a failure: no alert). That is deliberately wider than Windows; the stale text is for the cases above.
//   - iOS never changes the page of a takeoff, so "no longer owns its page" is covered by "the takeoff is gone or its page is gone".
//   - A programmatic caller (user_initiated false) gets no alert; the caller logs.
//
// STATE: implemented (UndoRedoRules.cpp); the host tests of Config/Tests/Settings (test_undo_refusal.cpp) pin it.
#pragma once

#include "BrowserRules.hpp"
#include "UndoHistory.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ost_data
{
    struct ProjectData;
}

namespace ost_domain
{
    enum class HistoryDirection
    {
        Undo,
        Redo
    };

    // Why an undo / redo did not advance (HistoryTargetUnavailableError or any other failure).
    enum class HistoryRefusal
    {
        Failed,           // the write or the replay failed: "The change could not be undone. It is still in the undo history."
        TargetUnavailable // the page or the item of the entry no longer exists
    };

    // The entry kinds of the iOS history.
    enum class HistoryEntryKind
    {
        Move,
        MoveGroup,
        Place,
        Delete,
        SetNegative
    };

    // What a replay needs to exist. `takeoff_uids`: takeoffs that are changed in place (each must be in the project AND on a page that is in the project);
    // `page_uids`: pages that takeoffs are re-created on (each must be in the project).
    struct HistoryReplayTargets
    {
        std::vector<std::string> takeoff_uids;
        std::vector<std::string> page_uids;
    };

    // The targets of one replay. `takeoff_uids` = the takeoffs the entry names, `stored_page_uids` = the pages of the takeoff copies the entry stores (Place: the
    // placed takeoff, Delete: every deleted takeoff).
    //   Move, MoveGroup, SetNegative: takeoff_uids (undo and redo).
    //   Delete: stored_page_uids on UNDO (the takeoffs come back); redo only removes them: nothing.
    //   Place:  stored_page_uids on REDO (the takeoff comes back); undo only removes it: nothing.
    HistoryReplayTargets history_replay_targets(HistoryEntryKind kind, HistoryDirection direction, const std::vector<std::string> &takeoff_uids,
                                                const std::vector<std::string> &stored_page_uids);

    // true = the page or an item of the replay no longer exists in `project` (the replay is refused with the stale text, the entry stays). An empty uid does not
    // exist (Windows get_page("") is None). Nothing to check = false.
    bool history_targets_unavailable(const ost_data::ProjectData &project, const HistoryReplayTargets &targets);

    // The alert of a refused undo / redo: title "Undo" / "Redo" and the message of the four cases. nullopt when the call is not user-initiated (the caller logs).
    std::optional<DialogText> history_refusal_dialog(HistoryDirection direction, HistoryRefusal refusal, bool user_initiated);

    struct HistoryStepResult
    {
        bool advanced = false;                 // the entry moved to the other stack
        std::optional<HistoryRefusal> refusal; // set when there WAS an entry and it did not advance
        std::optional<DialogText> alert;       // set when the refusal is shown to the user
    };

    // One user or programmatic undo / redo of an UndoHistory. `is_stale(entry)` answers whether the page or item of the entry is gone (checked BEFORE anything is
    // written); `apply(entry)` runs the change and answers whether it was applied AND stored (an exception counts as false). An empty stack is no refusal and no
    // alert. A refused entry stays where it is, the stacks and the entry are unchanged, and the next call tries the same entry again.
    template <typename Entry, typename IsStale, typename Apply>
    HistoryStepResult run_history_step(UndoHistory<Entry> &history, HistoryDirection direction, bool user_initiated, IsStale &&is_stale, Apply &&apply)
    {
        HistoryStepResult result;
        const bool has_entry = direction == HistoryDirection::Undo ? history.can_undo() : history.can_redo();
        if (!has_entry)
            return result;
        HistoryRefusal why = HistoryRefusal::Failed;
        auto action = [&](Entry &entry) -> bool
        {
            if (is_stale(static_cast<const Entry &>(entry)))
            {
                why = HistoryRefusal::TargetUnavailable;
                return false;
            }
            return static_cast<bool>(apply(entry));
        };
        result.advanced = direction == HistoryDirection::Undo ? history.undo(action) : history.redo(action);
        if (!result.advanced)
        {
            result.refusal = why;
            result.alert = history_refusal_dialog(direction, why, user_initiated);
        }
        return result;
    }
}
