// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The text of the Delete Condition confirmation (the Conditions sheet, ConditionsViewController.mm).
//
// Windows source of truth (Windows d1c430a):
//   ost_visualizer/presentation/texts.py   MESSAGEBOX_TITLE_DELETE, MESSAGEBOX_LABEL_DELETING_THIS_CONDITION_WILL_DELETE_ALL,
//                                          MESSAGEBOX_LABEL_YOU_ARE_DELETING_CONDITIONS
//   ost_visualizer/presentation/utils/messagebox.py confirm_delete_conditions   one condition: the singular sentence; several:
//                                          the plural sentence with the number of conditions being deleted
// iOS deletes one condition at a time (there is no multi-select), so the plural text is not reachable from the UI today; it is here
// so that a later multi-delete asks the Windows question.
// Tests: Config/Tests/Conditions/test_condition_delete_text.cpp.
#pragma once
#include <string>

namespace ost_domain
{

    // Delete "<name>"?  (MESSAGEBOX_TITLE_DELETE)
    std::string condition_delete_title(const std::string &name);

    // One condition: "Deleting this condition will delete all its takeoffs on all pages within this bid.\n\nThis cannot be
    // undone.\n\nAre you sure you want to delete this condition?"; any other count: "You are deleting N conditions. Deleting a
    // condition will delete all its takeoffs on all pages within this bid.\n\nThis cannot be undone.\n\nAre you sure you want to
    // delete this condition?"
    std::string condition_delete_message(int condition_count);

}
