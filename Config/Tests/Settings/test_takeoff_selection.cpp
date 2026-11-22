// RPV-16 / UND-08 / UND-10: the selection of the 2D view and what the Delete button does with it (Core/Domain/TakeoffSelection.hpp).
//
// Device report: deleting a selected takeoff in the 2D view selected the other takeoffs of the same condition (the overlay draws
// every takeoff of a condition that is highlighted while no takeoff is selected as a group selection), and the result depended on
// the mode circle (View / Select / Place) next to the big one.
//
// Windows (reference 6d4786f): plan_view/view.py delete_selected (3913-3921) clears the selection FIRST and emits the deleted uids;
// plan_view_action_handler.on_elements_deleted (3747-3779) deletes exactly those takeoffs and their parent_uid descendants and never
// reads a condition; _undo_delete (3854-3872) selects the restored takeoffs and _redo_delete (3874-3885) clears the selection, both
// only while the page that owned the delete is still current (tests/presentation/handlers/test_plan_view_action_handler.py
// 3299-3320, 3776-3847). iOS: the undo selects only while the page is on screen (select_after_history); the redo of a Delete and the
// undo of a Place clear without that check, which is the safe direction. Delete takes no cursor mode (input_handler.py 2562-2569)
// and a mode change never touches the selection (set_cursor_mode, view.py 6206-6231). The same clear-before-delete order is pinned
// for an annotation delete by tests/presentation/components/plan_view/test_view.py 5778-5789.
//
// The first version of the Core decision transcribed the rule ViewController.mm applied up to now; against it the tests marked
// "(red before the fix)" failed. That red run is against the old rule placed in Core, not against the .mm: the host harness cannot
// run Objective-C++.
#include "TestHarness.hpp"

#include "Domain/EditMode.hpp"
#include "Domain/Takeoff.hpp"
#include "Domain/TakeoffDeletion.hpp"
#include "Domain/TakeoffSelection.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using ost_domain::EditMode;
using ost_domain::SelectionState;
using ost_domain::Takeoff;

namespace
{
    Takeoff make_takeoff(const std::string &uid, const std::string &condition_uid, const std::string &page_uid,
                         const std::string &parent_uid = "")
    {
        Takeoff t;
        t.uid = uid;
        t.condition_uid = condition_uid;
        t.page_uid = page_uid;
        t.parent_uid = parent_uid;
        t.position = {1.0, 2.0, 3.0, 4.0};
        return t;
    }

    // Condition 30 has 500, 501, 502 on page 100 and 504 on page 101; condition 31 has 503 on page 100; area 600 (condition 32) has
    // the hole 601 (same condition), which has 602 under it, and the attachment 603 (condition 31, another condition than its parent).
    std::vector<Takeoff> world()
    {
        return {make_takeoff("500", "30", "100"),        make_takeoff("501", "30", "100"),
                make_takeoff("502", "30", "100"),        make_takeoff("503", "31", "100"),
                make_takeoff("504", "30", "101"),        make_takeoff("600", "32", "100"),
                make_takeoff("601", "32", "100", "600"), make_takeoff("602", "32", "100", "601"),
                make_takeoff("603", "31", "100", "600")};
    }

    const EditMode kModes[] = {EditMode::View, EditMode::Select, EditMode::PlaceTakeoff};

    const char *mode_name(EditMode mode)
    {
        switch (mode)
        {
        case EditMode::View:
            return "View";
        case EditMode::Select:
            return "Select";
        default:
            return "Place";
        }
    }

    // The state of the app with the ring of the floating button on `mode`; in Place mode the placement tool of condition 30 runs.
    SelectionState app_in(EditMode mode)
    {
        SelectionState state;
        state.mode = mode;
        if (mode == EditMode::PlaceTakeoff)
            state.place_condition_uid = "30";
        return state;
    }

    std::vector<Takeoff> without(std::vector<Takeoff> takeoffs, const std::vector<std::string> &uids)
    {
        takeoffs.erase(std::remove_if(takeoffs.begin(), takeoffs.end(),
                                      [&uids](const Takeoff &t) { return std::find(uids.begin(), uids.end(), t.uid) != uids.end(); }),
                       takeoffs.end());
        return takeoffs;
    }

    using Uids = std::vector<std::string>;
}

// ---------------------------------------------------------------------------------------------------------------
// what Delete removes
// ---------------------------------------------------------------------------------------------------------------

// The exact case of the report: tap a takeoff of a condition that has more takeoffs, press Delete. Only the tapped takeoff goes, in
// every mode; its siblings of the condition stay.
TEST(TakeoffSelection, TapOneTakeoffThenDeleteRemovesOnlyThatTakeoffInEveryMode)
{
    for (EditMode mode : kModes)
    {
        SCOPED_CONTEXT(std::string("mode ") + mode_name(mode));
        const SelectionState state = ost_domain::select_takeoff(world(), app_in(mode), "501");
        CHECK(ost_domain::delete_enabled(world(), state));
        CHECK_EQ(ost_domain::takeoff_uids_to_delete(world(), state), (Uids{"501"}));
    }
}

// Windows: Delete takes no cursor mode, and a mode change never touches the selection. The mode is a plain field of the state: the
// ring of the floating button changes it and nothing else (ViewController _setEditMode leaves the takeoff, the condition and the
// place offer alone), and neither what Delete removes nor whether it is on reads it.
TEST(TakeoffSelection, ChangingTheModeBetweenTheTapAndTheDeleteDoesNotChangeWhatDeleteRemoves)
{
    SelectionState state = ost_domain::select_takeoff(world(), app_in(EditMode::Select), "600");
    const Uids expected = {"600", "601", "602", "603"};
    CHECK_EQ(ost_domain::takeoff_uids_to_delete(world(), state), expected);
    for (EditMode mode : {EditMode::View, EditMode::PlaceTakeoff, EditMode::Select, EditMode::View, EditMode::PlaceTakeoff})
    {
        SCOPED_CONTEXT(std::string("after switching to ") + mode_name(mode));
        state.mode = mode;
        CHECK(ost_domain::delete_enabled(world(), state));
        CHECK_EQ(ost_domain::takeoff_uids_to_delete(world(), state), expected);
        // the decision UndoRedoController uses gives the same takeoffs, complete, parents first
        Uids uids;
        for (const Takeoff &takeoff : ost_domain::takeoffs_to_delete(world(), state.takeoff_uid))
            uids.push_back(takeoff.uid);
        CHECK_EQ(uids, expected);
    }
}

// A condition that is highlighted without a selected takeoff (picked in the Conditions list, or left over) is not a selection of
// takeoffs: Delete is off and the set is empty, in every mode. Nothing can turn it into a condition delete.
TEST(TakeoffSelection, AHighlightedConditionWithoutASelectedTakeoffDeletesNothing)
{
    for (EditMode mode : kModes)
    {
        SCOPED_CONTEXT(std::string("mode ") + mode_name(mode));
        SelectionState state = app_in(mode);
        state.condition_uid = "30";
        CHECK_FALSE(ost_domain::delete_enabled(world(), state));
        CHECK(ost_domain::takeoff_uids_to_delete(world(), state).empty());
    }
}

// For every takeoff and every mode the set is the takeoff plus its parent_uid descendants (UND-08) and nothing else: the siblings of
// the same condition always survive unless they are descendants, and a descendant of ANOTHER condition goes with its parent.
TEST(TakeoffSelection, TheDeleteSetIsTheTakeoffAndItsDescendantsForEveryTakeoffAndMode)
{
    const std::vector<Takeoff> takeoffs = world();
    for (const Takeoff &takeoff : takeoffs)
        for (EditMode mode : kModes)
        {
            SCOPED_CONTEXT("takeoff " + takeoff.uid + ", mode " + mode_name(mode));
            const SelectionState state = ost_domain::select_takeoff(takeoffs, app_in(mode), takeoff.uid);
            const Uids uids = ost_domain::takeoff_uids_to_delete(takeoffs, state);
            const std::set<std::string> expected = ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {takeoff.uid});
            REQUIRE(!uids.empty());
            CHECK_EQ(uids.front(), takeoff.uid);
            CHECK(std::set<std::string>(uids.begin(), uids.end()) == expected);
            CHECK_EQ(uids.size(), expected.size());
            for (const Takeoff &other : takeoffs)
            {
                const bool in_set = std::find(uids.begin(), uids.end(), other.uid) != uids.end();
                if (other.condition_uid == takeoff.condition_uid && expected.count(other.uid) == 0)
                    CHECK_FALSE(in_set);
            }
        }
    // the family: the hole goes with its area, the area does not go with its hole
    SelectionState area = ost_domain::select_takeoff(takeoffs, app_in(EditMode::Select), "600");
    CHECK_EQ(ost_domain::takeoff_uids_to_delete(takeoffs, area), (Uids{"600", "601", "602", "603"}));
    SelectionState hole = ost_domain::select_takeoff(takeoffs, app_in(EditMode::Select), "601");
    CHECK_EQ(ost_domain::takeoff_uids_to_delete(takeoffs, hole), (Uids{"601", "602"}));
    SelectionState attachment = ost_domain::select_takeoff(takeoffs, app_in(EditMode::Select), "603");
    CHECK_EQ(ost_domain::takeoff_uids_to_delete(takeoffs, attachment), (Uids{"603"}));
}

// Deleting the only takeoff of a condition names that takeoff only: no condition, no other takeoff.
TEST(TakeoffSelection, DeletingTheOnlyTakeoffOfAConditionNamesOnlyThatTakeoff)
{
    for (EditMode mode : kModes)
    {
        SCOPED_CONTEXT(std::string("mode ") + mode_name(mode));
        const SelectionState state = ost_domain::select_takeoff(world(), app_in(mode), "503");
        CHECK_EQ(ost_domain::takeoff_uids_to_delete(world(), state), (Uids{"503"}));
    }
}

// A selection that names a takeoff the project no longer has (a stale uid) offers no Delete and removes nothing.
TEST(TakeoffSelection, AStaleSelectionOffersNoDelete)
{
    SelectionState state = ost_domain::select_takeoff(world(), app_in(EditMode::Select), "501");
    const std::vector<Takeoff> remaining = without(world(), {"501"});
    CHECK_FALSE(ost_domain::delete_enabled(remaining, state));
    CHECK(ost_domain::takeoff_uids_to_delete(remaining, state).empty());
    CHECK_FALSE(ost_domain::delete_enabled(world(), SelectionState{}));
}

// ---------------------------------------------------------------------------------------------------------------
// what is selected afterwards
// ---------------------------------------------------------------------------------------------------------------

// The failing case of the report (red before the fix): after the Delete nothing is selected, no condition is highlighted, and so
// the overlay draws no group selection on the other takeoffs of the condition. Windows: delete_selected clears the selection.
TEST(TakeoffSelection, AfterTheDeleteNothingIsSelectedAndNoOtherTakeoffOfTheConditionIsHighlighted)
{
    for (EditMode mode : kModes)
    {
        SCOPED_CONTEXT(std::string("mode ") + mode_name(mode));
        SelectionState state = ost_domain::select_takeoff(world(), app_in(mode), "501");
        const std::vector<Takeoff> remaining = without(world(), ost_domain::takeoff_uids_to_delete(world(), state));
        state = ost_domain::clear_selection(state);
        CHECK(state.takeoff_uid.empty());
        CHECK(state.condition_uid.empty());
        CHECK_EQ(state.active_handle, -1);
        // the overlay: no group selection of the siblings 500 and 502 on page 100
        CHECK(ost_domain::group_highlighted_uids(remaining, state, "100").empty());
        CHECK_FALSE(ost_domain::delete_enabled(remaining, state));
        CHECK(state.mode == mode);
    }
}

// The condition highlight is released by the delete, and the offer to place with it goes with it, except while the placement tool
// runs (Place mode): that tool keeps its condition, so the floating button and the live tool stay in step (red before the fix:
// the place condition was dropped in every mode).
TEST(TakeoffSelection, TheDeleteKeepsThePlacementThatIsRunningAndEndsTheOfferOtherwise)
{
    SelectionState place = app_in(EditMode::PlaceTakeoff);
    place = ost_domain::select_takeoff(world(), place, "501");
    CHECK_EQ(place.place_condition_uid, std::string("30")); // the running tool keeps its own condition
    // also for a takeoff of ANOTHER condition: the highlight is the takeoff's, the placement stays the tool's
    const SelectionState other = ost_domain::select_takeoff(world(), app_in(EditMode::PlaceTakeoff), "503");
    CHECK_EQ(other.place_condition_uid, std::string("30"));
    CHECK_EQ(other.condition_uid, std::string("31"));
    place = ost_domain::clear_selection(place);
    CHECK_EQ(place.place_condition_uid, std::string("30"));
    CHECK(place.mode == EditMode::PlaceTakeoff);

    for (EditMode mode : {EditMode::View, EditMode::Select})
    {
        SCOPED_CONTEXT(std::string("mode ") + mode_name(mode));
        SelectionState state = ost_domain::select_takeoff(world(), app_in(mode), "501");
        CHECK_EQ(state.place_condition_uid, std::string("30")); // offered by the tap
        state = ost_domain::clear_selection(state);
        CHECK(state.place_condition_uid.empty()); // released with the selection
    }
}

// Windows _redo_delete clears the selection (red before the fix: the condition stayed highlighted).
TEST(TakeoffSelection, RedoOfTheDeleteClearsTheSelection)
{
    for (EditMode mode : kModes)
    {
        SCOPED_CONTEXT(std::string("mode ") + mode_name(mode));
        SelectionState state = ost_domain::select_takeoff(world(), app_in(mode), "501");
        state = ost_domain::clear_selection(state);
        CHECK(state.takeoff_uid.empty());
        CHECK(state.condition_uid.empty());
        CHECK_EQ(state.active_handle, -1);
        CHECK(ost_domain::group_highlighted_uids(world(), state, "100").empty());
    }
}

// Windows _undo_delete selects the restored takeoff while its page is the one on screen (UND-10): the takeoff is selected and its
// condition highlighted (red before the fix: only the takeoff was set). The offer to place is left as it was: a restored takeoff
// must not arm the placement tool for a condition whose layer may be hidden (COND-PLACE-01).
TEST(TakeoffSelection, UndoSelectsTheRestoredTakeoffWhenItsPageIsOnScreen)
{
    SelectionState state = ost_domain::select_takeoff(world(), app_in(EditMode::Select), "501");
    state = ost_domain::clear_selection(state);
    CHECK(state.place_condition_uid.empty());
    state = ost_domain::select_after_history(world(), state, "501", "100");
    CHECK_EQ(state.takeoff_uid, std::string("501"));
    CHECK_EQ(state.condition_uid, std::string("30"));
    CHECK(state.place_condition_uid.empty()); // not offered by a history step
    CHECK_EQ(state.active_handle, -1);
    // an offer that exists (a tapped takeoff of another condition) and a running placement stay exactly as they were
    SelectionState offered = ost_domain::select_takeoff(world(), app_in(EditMode::Select), "503");
    offered = ost_domain::select_after_history(world(), offered, "501", "100");
    CHECK_EQ(offered.place_condition_uid, std::string("31"));
    CHECK_EQ(offered.condition_uid, std::string("30"));
    const SelectionState running = ost_domain::select_after_history(world(), app_in(EditMode::PlaceTakeoff), "503", "100");
    CHECK_EQ(running.place_condition_uid, std::string("30"));
    CHECK_EQ(running.takeoff_uid, std::string("503"));
    // a takeoff is selected: no group selection, and the Delete button works on exactly the restored takeoff
    CHECK(ost_domain::group_highlighted_uids(world(), state, "100").empty());
    CHECK_EQ(ost_domain::takeoff_uids_to_delete(world(), state), (Uids{"501"}));
}

// Windows: the history only selects while its page is current (_history_selection_owner); a restored takeoff of another page
// leaves the selection as it is (RPV-16), so Delete can never act on a takeoff the user does not see.
TEST(TakeoffSelection, UndoOfATakeoffOnAnotherPageSelectsNothing)
{
    SelectionState state = app_in(EditMode::Select);
    const SelectionState after = ost_domain::select_after_history(world(), state, "504", "100");
    CHECK(after.takeoff_uid.empty());
    CHECK(after.condition_uid.empty());

    SelectionState other = ost_domain::select_takeoff(world(), app_in(EditMode::Select), "502");
    const SelectionState kept = ost_domain::select_after_history(world(), other, "504", "100");
    CHECK_EQ(kept.takeoff_uid, std::string("502"));
    CHECK_EQ(kept.condition_uid, std::string("30"));
}

// ---------------------------------------------------------------------------------------------------------------
// the other ways the selection changes
// ---------------------------------------------------------------------------------------------------------------

// A takeoff picked in the 3D view can be on any selected page. Entering the 2D view drops a selection that is not on the page on
// screen, with its condition highlight, so Delete cannot act on a takeoff the user does not see (RPV-16 for the 3D -> 2D switch).
TEST(TakeoffSelection, ASelectionOfAnotherPageIsDroppedWhenTheTwoDViewIsEntered)
{
    SelectionState picked = ost_domain::select_takeoff(world(), app_in(EditMode::View), "504"); // page 101
    const SelectionState dropped = ost_domain::keep_selection_on_page(world(), picked, "100");
    CHECK(dropped.takeoff_uid.empty());
    CHECK(dropped.condition_uid.empty());
    CHECK_FALSE(ost_domain::delete_enabled(world(), dropped));

    SelectionState here = ost_domain::select_takeoff(world(), app_in(EditMode::View), "501"); // page 100
    const SelectionState kept = ost_domain::keep_selection_on_page(world(), here, "100");
    CHECK_EQ(kept.takeoff_uid, std::string("501"));
    CHECK_EQ(kept.condition_uid, std::string("30"));

    // nothing selected: the state is returned as it is (a condition picked in the list stays highlighted)
    SelectionState listed = app_in(EditMode::View);
    listed.condition_uid = "30";
    CHECK_EQ(ost_domain::keep_selection_on_page(world(), listed, "100").condition_uid, std::string("30"));

    // a selected takeoff that no longer exists is dropped too
    const std::vector<Takeoff> remaining = without(world(), {"501"});
    CHECK(ost_domain::keep_selection_on_page(remaining, here, "100").takeoff_uid.empty());
}

// A page switch (RPV-16) drops the takeoff and the handle; a placement in progress survives it together with its highlighted
// condition (Windows preserve_place); without a placement the highlight and the offer to place go too. This is the rule
// _clearSelectionForPageSwitch had before the Core decision, pinned so that the delete fix cannot change it.
TEST(TakeoffSelection, APageSwitchKeepsARunningPlacementAndDropsTheRest)
{
    SelectionState place = app_in(EditMode::PlaceTakeoff);
    place.condition_uid = "30"; // the condition of the placement is highlighted
    place.takeoff_uid = "501";
    place.active_handle = 2;
    const SelectionState kept = ost_domain::clear_selection_for_page_switch(place);
    CHECK(kept.takeoff_uid.empty());
    CHECK_EQ(kept.active_handle, -1);
    CHECK_EQ(kept.condition_uid, std::string("30"));
    CHECK_EQ(kept.place_condition_uid, std::string("30"));

    for (EditMode mode : {EditMode::View, EditMode::Select})
    {
        SCOPED_CONTEXT(std::string("mode ") + mode_name(mode));
        SelectionState state = ost_domain::select_takeoff(world(), app_in(mode), "501");
        state = ost_domain::clear_selection_for_page_switch(state);
        CHECK(state.takeoff_uid.empty());
        CHECK(state.condition_uid.empty());
        CHECK(state.place_condition_uid.empty());
        CHECK_EQ(state.active_handle, -1);
    }
}

// A tap on empty space clears the selection and the highlight (Place mode has no edit taps, so a tap never meets a running
// placement), a tap resets the handle, and a tap on a
// takeoff selects exactly that takeoff: the other takeoffs of its condition are not selected and not highlighted as a group.
TEST(TakeoffSelection, ATapSelectsOnlyTheTappedTakeoffAndATapOnEmptySpaceClears)
{
    SelectionState with_handle = app_in(EditMode::Select);
    with_handle.active_handle = 2; // a handle of the previously selected takeoff
    CHECK_EQ(ost_domain::select_takeoff(world(), with_handle, "501").active_handle, -1);

    SelectionState state = ost_domain::select_takeoff(world(), app_in(EditMode::Select), "501");
    CHECK_EQ(state.takeoff_uid, std::string("501"));
    CHECK(ost_domain::group_highlighted_uids(world(), state, "100").empty()); // a takeoff is selected: no group
    state = ost_domain::select_takeoff(world(), state, "502"); // a second tap replaces the selection
    CHECK_EQ(state.takeoff_uid, std::string("502"));
    CHECK_EQ(ost_domain::takeoff_uids_to_delete(world(), state), (Uids{"502"}));

    state = ost_domain::clear_selection(state);
    CHECK(state.takeoff_uid.empty());
    CHECK(state.condition_uid.empty());
    CHECK(state.place_condition_uid.empty());

    // the group state that remains for a condition picked in the Conditions list: the overlay draws the takeoffs of the condition on
    // the page, Delete stays off (it is not a selection of takeoffs)
    SelectionState listed = app_in(EditMode::Select);
    listed.condition_uid = "30";
    CHECK_EQ(ost_domain::group_highlighted_uids(world(), listed, "100"), (Uids{"500", "501", "502"}));
    CHECK_FALSE(ost_domain::delete_enabled(world(), listed));
}
