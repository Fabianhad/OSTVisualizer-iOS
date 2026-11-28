// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// A stored quantity that the condition style does not offer, in the Results rows and the Quantity picker of the Edit Condition editor.
//
// Windows source of truth (Windows d1c430a): ost_visualizer/presentation/dialogs/edit_condition_dialog.py _show_stored_quantity
// (a stored calc_type that is not among the quantity combo's items is added as one more item, "Unknown (N)" with N the stored calc
// type (texts.py EDIT_CONDITION_DIALOG_CHOICE_UNKNOWN_QUANTITY), disabled so it can never be chosen, and shown as the current
// quantity) and _after_successful_save (the entry is dropped once the saved quantity is a real one). Choosing "(none)" clears the
// slot and a save that leaves the slot alone keeps it: those save rules are in ConditionStyleChange (test_style_change.cpp
// CondM08_UnknownStyle), not here.
// The options of a style are those of get_quantity_options_for_type (UomService.hpp): Attachment, Count and every unsupported type are
// offered the count options (Windows effective_type), Linear and Area their own.
// Tests: Config/Tests/Conditions/test_condition_quantity_choices.cpp.
#pragma once
#include "Types.hpp"
#include <optional>
#include <string>

namespace ost_domain
{

    // The stored calc type when it is not 0 (none) and not among the quantity options of `style`, else nullopt. This is the value of the
    // disabled "Unknown (N)" entry of the Quantity picker.
    std::optional<int> unknown_quantity_calc_type(ConditionType style, int calc_type);

    // "Unknown (N)" (EDIT_CONDITION_DIALOG_CHOICE_UNKNOWN_QUANTITY), N the raw stored calc type.
    std::string unknown_quantity_label(int calc_type);

    // The text of a Results row for the stored calc type: "(none)" for 0, the label of the option of `style`, else the unknown entry.
    std::string condition_quantity_label(ConditionType style, int calc_type);

}
