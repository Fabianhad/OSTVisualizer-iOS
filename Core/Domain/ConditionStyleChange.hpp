// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// What changing the style (Linear / Area / Count / Attachment) of a condition in the Edit Condition dialog does, and which
// condition a Save then writes: extracted from _applyStyleDefaults: in EditConditionViewController.mm.
//
// Windows source of truth: ost_visualizer/presentation/dialogs/edit_condition_dialog.py
//   TYPE_DEFAULTS (:90-143), _on_style_changed / _shared_options / _restore_shared_options / _populate_defaults_for_type
//   (:1197-1303)   the form after a style change: name, Cond. No., elevation, layer, type, notes, colour, pattern and the
//                  spacing text are kept; display_name is carried among Count / Attachment / Area, round_quantity and
//                  round_to among Linear / Area; only the controls of the new style get their defaults; the shape is reset
//                  only when entering Count / Attachment from another style
//   _validate_and_build_dto (:1405-1690)   the changes a Save sends: only the fields of the style's controls are compared with
//                  the stored condition; a style that differs from the stored one also writes thickness, spacing and backout
//                  of TYPE_DEFAULTS (the field value wins when it differs from the stored value). Windows sends the IMPERIAL
//                  columns (:1445-1448) in a metric bid too, although the dialog shows the metric ones; here a metric bid
//                  writes the metric columns (COND-STYLE-02, deliberate deviation decided on 2026-11-19), an imperial bid
//                  the imperial ones (= Windows)
//   :1427-1441     is_top and z_value go into the DTO from the dialog, but they are no columns and Windows shows the reloaded
//                  condition; here the condition a Save leaves in memory derives them from the saved name like the loader
//                  (COND-ELEV-01, deliberate deviation decided on 2026-11-19), see condition_to_save
// Tests: Config/Tests/Conditions/test_style_change.cpp (ports of tests/presentation/dialogs/test_edit_condition_dialog.py
// test_notes_survive_count_to_linear_and_back, test_shared_fields_carry_over_and_type_specific_fields_get_defaults,
// test_rounding_carries_over_between_linear_and_area_only), Config/Tests/Conditions/test_condition_elevation.cpp (COND-ELEV-01).
#pragma once
#include "Condition.hpp"
#include "ConditionStyleDefaults.hpp"
#include <vector>

namespace ost_domain
{

    // A text field of the dialog that a style change can reset (every text field not listed keeps what was typed).
    enum class StyleField
    {
        Height,
        Width,
        Depth,
        Thickness,
        DisplaySize,
        Rise,
        Run,
        Tolerance, // iOS-only control (Connect / Tolerance)
        RoundTo,
        AddLength,
        TileX,
        TileY,
        Gap
    };

    struct StyleChange
    {
        // The form's condition after the change: the state of the switches and pickers, and the number each reset text field
        // starts from. It is not what a Save writes (see condition_to_save).
        Condition condition;
        // The text fields whose text is regenerated from `condition`; the texts of all other fields (name, Cond. No., the
        // elevation, notes, spacing, whatever was typed into a field of the old style that the new style also has) stay.
        std::vector<StyleField> reset_fields;
    };

    // _on_style_changed: the form `form` (its condition_type is the style before the change) gets `new_type`. `metric` is the
    // measurement system of the bid (the defaults of a metric bid are millimetres).
    StyleChange apply_style_change(const Condition &form, ConditionType new_type, bool metric);

    // The values a Save writes when the style differs from the stored one (TYPE_DEFAULTS). Everything in inches.
    //   imperial bid: the imperial table values, as Windows sends td["thickness"], td["spacing"], td["backout"]
    //   metric bid:   style_dimension_defaults(type, true), i.e. the metric columns (mm / 25.4), the values the dialog shows after
    //                 the style change (COND-STYLE-02, decided 2026-11-19; Windows writes the imperial values even in a metric bid)
    // The backout is the table's whatever the measurement system.
    struct TypeChangeDefaults
    {
        double thickness;
        double spacing; // imperial: Area 6.0, the others 4.0 (COND-STYLE-02); metric: Area 150 mm, the others 100 mm
        bool backout;
    };
    TypeChangeDefaults type_change_defaults(ConditionType type, bool metric);

    // _validate_and_build_dto: the condition a Save writes. `stored` is the condition as it is in the database, `form` the
    // dialog's condition after every text field was parsed into it. Only the fields of the controls of form.condition_type are
    // taken from the form (the thickness of a Count, the height of an Area ... have no control and keep their stored value);
    // ColorLine is never taken; the unit of a quantity slot whose quantity is 0 (none) is -1. When the style differs from the
    // stored one, thickness (if the style has no thickness control, or its value equals the stored one), spacing (if it equals the
    // stored one) and backout are the TYPE_DEFAULTS values of type_change_defaults(form.condition_type, metric): a style change
    // persists the type default spacing unless the user edited the field (COND-STYLE-02). `metric` is the measurement system of
    // the bid (bid.measure_base == 1), it has no default so that no caller can forget it. is_top and z_value of the result come from
    // the saved NAME (derive_elevation_from_name, the loader's derivation), whatever form.is_top / form.z_value hold: the saved
    // condition equals the one a reload gives (COND-ELEV-01).
    Condition condition_to_save(const Condition &stored, const Condition &form, bool metric);

}
