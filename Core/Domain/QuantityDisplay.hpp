// Layer: Domain (pure C++, no UIKit/Foundation/SQLite)
//
// Quantity text of the Conditions list (the subtitle of a condition row), extracted from formatQty() in
// ConditionsViewController.mm.
//
// Windows source of truth:
//   ost_visualizer/application/utils/quantity_display.py   format_quantity_number, format_quantity_with_uom
//   ost_visualizer/presentation/components/conditions_sidebar.py update_quantities (three slots per condition row,
//                                                          a missing quantity entry is (0, 0, 0))
//
// Python's format(value, ",.0f") / format(value, ",.2f"): "," grouping every three integer digits, "." decimal point,
// rounding of the exact binary value to nearest-even. Never locale dependent (no NSNumberFormatter semantics).
#pragma once

#include "UomService.hpp"

#include <array>
#include <functional>
#include <string>

namespace ost_domain
{
    // format_quantity_number(value, uom_code, zero_text=""): value == 0 (also -0.0) -> zero_text; a metric unit
    // (mm, mm2, m, m2, mm3, m3) with |value| < 100 -> two decimals ("12.35", "-99.99", "100.00" for 99.995); everything
    // else -> no decimals with "," grouping ("1,234", "-13", "1,235"). nan -> "nan", +-inf -> "inf" / "-inf".
    std::string format_quantity_number(double value, int uom_code, const std::string &zero_text = "");

    // Label of a unit code; an empty string means "no label" (Windows: '' or None).
    using UomLabelFn = std::function<std::string(int)>;

    // format_quantity_with_uom(value, uom_code, uom_label_fn): "<number or 0> <label>" when the label is non-empty
    // ("0 LF", "12.35 M"), else the bare number ("-13", and "" for a zero). uom_label_fn is called exactly once.
    std::string format_quantity_with_uom(double value, int uom_code, const UomLabelFn &uom_label_fn);

    // The same with the labels of get_uom_label (UomService.hpp).
    std::string format_quantity_with_uom(double value, int uom_code);

    // The three quantity texts of one Conditions list row (conditions_sidebar.update_quantities): slot i is
    // format_quantity_with_uom(q_i, uom_i). A condition without a computed quantity entry (quantities == nullptr)
    // counts as (0, 0, 0), so every slot whose unit has a label reads "0 <label>". An empty slot text means the unit
    // has no label and the quantity is 0.
    std::array<std::string, 3> format_condition_quantity_texts(const QuantityResult *quantities, int uom1, int uom2,
                                                               int uom3);
}
