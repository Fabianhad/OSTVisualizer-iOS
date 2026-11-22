// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The selection of the 2D view and what Delete does with it, extracted from ViewController.mm (planView:didTapTakeoff:,
// planView:didTapEmptySpaceAtCanvasPoint:, _handleDeleteTakeoff, undoRedoSelectTakeoff:, undoRedoClearSelection,
// _clearSelectionForPageSwitch, _updateDeleteButton) and UndoRedoController.deleteSelection. The app calls select_takeoff,
// clear_selection, clear_selection_for_page_switch, keep_selection_on_page, select_after_history, delete_enabled and
// takeoffs_to_delete; group_highlighted_uids is the specification of the rule PlanView.mm appendSelection applies (the tests use
// it to say what the overlay draws).
//
// Windows (reference 6d4786f):
//  * plan_view/view.py delete_selected (3913-3921): the uids of the selection are taken, clear_selection() runs FIRST, then
//    elements_deleted is emitted; plan_view_action_handler.on_elements_deleted (3747-3779) deletes exactly those takeoffs and their
//    descendants (takeoff_domain_service.expand_takeoff_uids_with_descendants, parent_uid only; _delete_takeoffs_fast 1830) and
//    never looks at a condition (the condition uids are refresh metadata, 4023-4025).
//  * _undo_delete (3854-3872) selects the restored takeoffs while the history owner (the page that was on screen) is still
//    current; _redo_delete (3874-3885) clears the selection under the same condition.
//  * Delete takes no cursor mode (input_handler.py 2562-2569; main_window._delete_selected 1423-1426 for the Takeoff tab: on the
//    Windows Summary tab the same key deletes a condition row after a confirmation, a tab iOS does not have) and a mode change
//    never touches the selection (set_cursor_mode, view.py 6206-6231).
//  * The condition highlight of the sidebar is a projection of the takeoff selection (ui_event_coordinator.
//    _project_takeoff_selection_conditions 1560-1609) that follows the selection and is kept only while a placement session or the
//    Conditions list owns it. Selecting a condition in the sidebar never selects its takeoffs; the only ways to select the
//    takeoffs of a condition are the explicit "Select Objects" commands.
// iOS has ONE field for the highlighted condition (PlanView draws a group border on every takeoff of it while no takeoff is
// selected), and it cannot tell who owns the highlight. So after a Delete or any clear the highlight is dropped, also while the
// placement tool runs (that tool keeps its own condition): leaving it would draw the other takeoffs of the condition as selected,
// which is the symptom this model fixes.
#pragma once
#include "EditMode.hpp"
#include "Takeoff.hpp"
#include "TakeoffDeletion.hpp"

#include <string>
#include <utility>
#include <vector>

namespace ost_domain
{

    // What ViewController keeps in _selectedTakeoffUID / _selectedConditionUID / _placeConditionUID / _activeHandleIndex /
    // _editMode, in one value.
    struct SelectionState
    {
        EditMode mode = EditMode::View;
        // The selected takeoff, "" = none.
        std::string takeoff_uid;
        // The condition that is highlighted: the one of the tapped takeoff, or the one picked in the Conditions list.
        std::string condition_uid;
        // The condition the placement tool places; "" = the Place mode is not available.
        std::string place_condition_uid;
        int active_handle = -1;
    };

    inline const Takeoff *find_takeoff(const std::vector<Takeoff> &takeoffs, const std::string &uid)
    {
        for (const Takeoff &takeoff : takeoffs)
            if (takeoff.uid == uid)
                return &takeoff;
        return nullptr;
    }

    // planView:didTapTakeoff: the takeoff is selected (and no other: a takeoff of the same condition is not), its condition is
    // highlighted and offered to the placement tool. A placement that is running (Place mode) keeps its own condition.
    inline SelectionState select_takeoff(const std::vector<Takeoff> &takeoffs, SelectionState state, const std::string &uid)
    {
        const Takeoff *takeoff = find_takeoff(takeoffs, uid);
        const std::string condition = takeoff ? takeoff->condition_uid : std::string();
        state.takeoff_uid = uid;
        state.active_handle = -1;
        state.condition_uid = condition;
        if (state.mode != EditMode::PlaceTakeoff)
            state.place_condition_uid = condition;
        return state;
    }

    // A tap on empty space, a Delete (Windows delete_selected: clear_selection() before the delete), the redo of a Delete (_redo_delete)
    // and the undo of a Place: nothing is selected and no condition is highlighted, so the overlay draws no group selection on the
    // other takeoffs of the condition. The offer to place with the condition of the selection ends with it, except while the
    // placement tool runs (Place mode): that tool keeps its condition, so the floating button and the live tool stay in step. The
    // mode itself is never touched.
    inline SelectionState clear_selection(SelectionState state)
    {
        state.takeoff_uid.clear();
        state.condition_uid.clear();
        state.active_handle = -1;
        if (state.mode != EditMode::PlaceTakeoff)
            state.place_condition_uid.clear();
        return state;
    }

    // A page switch (Windows PlanView.load_page drops the selection, RPV-16): the takeoff and the handle are dropped. A placement in
    // progress survives the switch (Windows preserve_place, view.py 5052-5058) together with its highlighted condition; without
    // one the condition highlight and the offer to place go too. (This is the rule _clearSelectionForPageSwitch had before the Core
    // decision.)
    inline SelectionState clear_selection_for_page_switch(SelectionState state)
    {
        state.takeoff_uid.clear();
        state.active_handle = -1;
        if (state.mode != EditMode::PlaceTakeoff)
        {
            state.condition_uid.clear();
            state.place_condition_uid.clear();
        }
        return state;
    }

    // Entering the 2D view: the 3D view picks takeoffs of any selected page into the same state. A selected takeoff that is not on
    // the page on screen, or no longer exists, is dropped like at a page switch, so that Delete can never act on a takeoff the user
    // does not see (the RPV-16 rule for the 3D -> 2D switch).
    inline SelectionState keep_selection_on_page(const std::vector<Takeoff> &takeoffs, SelectionState state,
                                                 const std::string &page_uid)
    {
        if (state.takeoff_uid.empty())
            return state;
        const Takeoff *takeoff = find_takeoff(takeoffs, state.takeoff_uid);
        if (takeoff && takeoff->page_uid == page_uid)
            return state;
        return clear_selection_for_page_switch(std::move(state));
    }

    // The selection after a history step that brings a takeoff back or moves it (the undo of a Delete, Windows _undo_delete; RPV-16,
    // UND-10): the takeoff is selected and its condition highlighted (as the Windows sidebar projection does) while its page is the
    // one on screen; otherwise the selection stays as it is, so Delete can never act on a takeoff the user does not see. The offer to
    // place is left exactly as it was: a restored takeoff must not arm the placement tool for a condition whose layer may be hidden
    // (COND-PLACE-01), only a tap or the Conditions list offers it.
    inline SelectionState select_after_history(const std::vector<Takeoff> &takeoffs, SelectionState state,
                                               const std::string &restored_uid, const std::string &current_page_uid)
    {
        const Takeoff *takeoff = find_takeoff(takeoffs, restored_uid);
        if (!takeoff || takeoff->page_uid != current_page_uid)
            return state;
        const std::string place_condition = state.place_condition_uid;
        SelectionState selected = select_takeoff(takeoffs, std::move(state), restored_uid);
        selected.place_condition_uid = place_condition;
        return selected;
    }

    // The takeoffs the overlay draws as the group selection (PlanView appendSelection: no takeoff selected, a condition
    // highlighted, takeoff.condition_uid == that condition, on the page). This is the specification of that rule, for the tests.
    // Windows has no such state: a takeoff selection is always an explicit set.
    inline std::vector<std::string> group_highlighted_uids(const std::vector<Takeoff> &takeoffs, const SelectionState &state,
                                                           const std::string &page_uid)
    {
        std::vector<std::string> uids;
        if (!state.takeoff_uid.empty() || state.condition_uid.empty())
            return uids;
        for (const Takeoff &takeoff : takeoffs)
            if (takeoff.page_uid == page_uid && takeoff.condition_uid == state.condition_uid)
                uids.push_back(takeoff.uid);
        return uids;
    }

    // The Delete button is enabled while a takeoff that exists is selected. A highlighted condition is not a selection of takeoffs
    // (Windows: plan_view.has_selection).
    inline bool delete_enabled(const std::vector<Takeoff> &takeoffs, const SelectionState &state)
    {
        return !state.takeoff_uid.empty() && find_takeoff(takeoffs, state.takeoff_uid) != nullptr;
    }

    // What one press on Delete removes, as complete copies for the history entry: the selected takeoff and its descendants (UND-08,
    // parents first), and nothing else; nothing for a uid that is not a takeoff. The mode is not an input and no condition is an
    // input: the ring of the floating button cannot change the set, and no condition is ever part of it.
    // UndoRedoController deleteSelection:fromTakeoffs: calls exactly this.
    inline std::vector<Takeoff> takeoffs_to_delete(const std::vector<Takeoff> &takeoffs, const std::string &selected_uid)
    {
        if (selected_uid.empty() || find_takeoff(takeoffs, selected_uid) == nullptr)
            return {};
        return collect_takeoffs_for_deletion(takeoffs, {selected_uid});
    }

    // The uids of takeoffs_to_delete for the selection of the app.
    inline std::vector<std::string> takeoff_uids_to_delete(const std::vector<Takeoff> &takeoffs, const SelectionState &state)
    {
        std::vector<std::string> uids;
        for (const Takeoff &takeoff : takeoffs_to_delete(takeoffs, state.takeoff_uid))
            uids.push_back(takeoff.uid);
        return uids;
    }

}
