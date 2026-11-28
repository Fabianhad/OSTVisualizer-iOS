// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// "Count as Negative Quantity" (UND-09), extracted from ViewController._setNegative:forPressedTakeoff: and
// UndoRedoController (negativeValues).
//
// Windows (d1c430a): ost_visualizer/presentation/handlers/plan_view_action_handler.py on_set_negative (2134-2181): the flag is set
// for the takeoffs of the plan-view selection and ONE history entry records, for each of them, the old and the new value
// (old_updates / new_updates, unchanged takeoffs included); one undo writes the old values back, one redo the new ones.
// The menu item (presentation/utils/view_context_menu.py:162-196 build_selected_takeoff_context_state, input_handler.py:3247-3251):
// offered only when the selection has a root takeoff and NO child takeoff (hole or attachment: Takeoff.is_hole, takeoff.py:136); checked only when
// EVERY selected takeoff is negative; a press sets every selected takeoff to (not checked). Owner decision 3: a pasted GROUP (the selection that
// paste leaves behind, N-M2-04) follows that rule as ONE undoable entry; a single takeoff stays the one-takeoff rule below (negative_toggle_for_press).
#pragma once
#include "Takeoff.hpp"
#include <string>
#include <utility>
#include <vector>

namespace ost_domain
{

    // One takeoff whose "Count as Negative Quantity" flag changed, with the value before and after.
    struct NegativeChange
    {
        std::string uid;
        bool old_value;
        bool new_value;
    };

    // The changes of one press on "Count as Negative Quantity" for the takeoff `pressed_uid` (Windows on_set_negative with the
    // plan-view selection of iOS, which is the one pressed takeoff): exactly that takeoff, with the flag it has now and the
    // flag it gets. A condition that is merely selected in the sidebar widens nothing, and the other takeoffs of the
    // condition keep their flags. A uid that is not in the project gives no change (and so no history entry).
    inline std::vector<NegativeChange> negative_changes_for_press(const std::vector<Takeoff> &takeoffs,
                                                                  const std::string &pressed_uid,
                                                                  bool negative)
    {
        for (const Takeoff &takeoff : takeoffs)
        {
            if (takeoff.uid == pressed_uid)
                return {{takeoff.uid, takeoff.is_negative, negative}};
        }
        return {};
    }

    // The menu item of "Count as Negative Quantity" for one long press: whether it is shown, its check mark and the ONE history entry a press makes.
    struct NegativeToggle
    {
        bool offered = false; // the item is shown (Windows show_negative)
        bool checked = false; // the check mark (Windows all_negative)
        std::vector<NegativeChange> changes; // one entry; empty when not offered or when the pressed uid is unknown
    };

    // `group_uids` is the pasted group in order (the selection paste leaves behind; EMPTY until M2 has paste and multi-selection), `pressed_uid` the
    // long-pressed takeoff.
    //   GROUP (at least two distinct uids of the group resolve to takeoffs, and the pressed takeoff is one of them; annotation and unknown uids are
    //   skipped, a repeated uid counts once): offered = no member is a hole or attachment; checked = every member is negative; the new value is
    //   (not checked) for EVERY member; `changes` has one (uid, old, new) per member, members already at the target included (Windows old_updates).
    //   Not offered gives no change.
    //   SINGLE (empty group, a group of one, or a pressed takeoff outside the group): exactly the pressed takeoff, as negative_changes_for_press:
    //   offered when it exists (iOS keeps offering the item on a single hole or attachment, Windows hides it), checked = its flag, one change
    //   (uid, flag, not flag); an unknown uid is not offered and gives no change.
    inline NegativeToggle negative_toggle_for_press(const std::vector<Takeoff> &takeoffs, const std::vector<std::string> &group_uids,
                                                    const std::string &pressed_uid)
    {
        std::vector<const Takeoff *> members;
        bool pressed_is_member = false;
        for (const std::string &uid : group_uids)
        {
            const Takeoff *found = nullptr;
            for (const Takeoff &takeoff : takeoffs)
            {
                if (takeoff.uid == uid)
                {
                    found = &takeoff;
                    break;
                }
            }
            if (found == nullptr)
                continue;
            bool repeated = false;
            for (const Takeoff *member : members)
                repeated = repeated || member == found;
            if (repeated)
                continue;
            members.push_back(found);
            pressed_is_member = pressed_is_member || found->uid == pressed_uid;
        }

        NegativeToggle toggle;
        if (members.size() >= 2 && pressed_is_member)
        {
            bool every_member_negative = true;
            bool any_child = false;
            for (const Takeoff *member : members)
            {
                every_member_negative = every_member_negative && member->is_negative;
                any_child = any_child || member->is_hole();
            }
            toggle.offered = !any_child;
            toggle.checked = every_member_negative;
            if (toggle.offered)
            {
                for (const Takeoff *member : members)
                    toggle.changes.push_back({member->uid, member->is_negative, !toggle.checked});
            }
            return toggle;
        }

        for (const Takeoff &takeoff : takeoffs)
        {
            if (takeoff.uid != pressed_uid)
                continue;
            toggle.offered = true;
            toggle.checked = takeoff.is_negative;
            toggle.changes = {{takeoff.uid, takeoff.is_negative, !takeoff.is_negative}};
            break;
        }
        return toggle;
    }

    // The (uid, flag) pairs an entry writes: the old values on undo, the new ones on redo.
    inline std::vector<std::pair<std::string, bool>> negative_values(const std::vector<NegativeChange> &changes,
                                                                     bool use_old)
    {
        std::vector<std::pair<std::string, bool>> values;
        values.reserve(changes.size());
        for (const NegativeChange &change : changes)
            values.push_back({change.uid, use_old ? change.old_value : change.new_value});
        return values;
    }

}
