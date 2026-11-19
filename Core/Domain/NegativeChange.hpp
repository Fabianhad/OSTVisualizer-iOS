// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// "Count as Negative Quantity" (UND-09), extracted from ViewController._setNegative:forPressedTakeoff: and
// UndoRedoController (negativeValues).
//
// Windows: ost_visualizer/presentation/handlers/plan_view_action_handler.py on_set_negative (2072-2116): the flag is set
// for the takeoffs of the plan-view selection and ONE history entry records, for each of them, the old and the new value
// (old_updates / new_updates); one undo writes the old values back, one redo the new ones.
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
