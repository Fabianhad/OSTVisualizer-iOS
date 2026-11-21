// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// What changing the style (Linear / Area / Count / Attachment) of a condition in the Edit Condition dialog does, and which
// condition a Save then writes: extracted from _applyStyleDefaults: in EditConditionViewController.mm.
//
// Windows source of truth: ost_visualizer/presentation/dialogs/edit_condition_dialog.py
//   TYPE_DEFAULTS (:94-147), _on_style_changed / _shared_options / _restore_shared_options / _populate_defaults_for_type
//   (:1201-1311)   the form after a style change: name, Cond. No., elevation, layer, type, notes, colour, pattern and the
//                  spacing text are kept; display_name is carried among Count / Attachment / Area, round_quantity and
//                  round_to among Linear / Area; only the controls of the new style get their defaults; the shape is reset
//                  only when entering Count / Attachment from another style
//   _validate_and_build_dto (:1414-1722)   the changes a Save sends: only the fields of the style's controls are compared with
//                  the stored condition; a style that differs from the stored one also writes the thickness default (metric
//                  columns in a metric bid, :1236-1239) and the backout of TYPE_DEFAULTS (:1458-1463); the Linear / Area
//                  Thickness and the Spacing are the fields' values, written on a style change even when equal to the stored
//                  value (:1559, :1589, :1624), a Count / Attachment keeps the default thickness (no Thickness field). What
//                  the field shows is what is saved (COND-STYLE-02, Windows b9902e6)
//   :1423-1457     is_top and z_value go into the DTO from the dialog, but they are no columns and Windows shows the reloaded
//                  condition; here the condition a Save leaves in memory derives them from the saved name like the loader
//                  (COND-ELEV-01: parity since Windows b9902e6, where the dialog validates with the loader's parser; only a blank
//                  elevation with the Top segment differs in the form), see condition_to_save
// Tests: Config/Tests/Conditions/test_style_change.cpp (ports of tests/presentation/dialogs/test_edit_condition_dialog.py
// test_notes_survive_count_to_linear_and_back, test_shared_fields_carry_over_and_type_specific_fields_get_defaults,
// test_rounding_carries_over_between_linear_and_area_only, the six COND-STYLE tests of EditConditionTypeChangeAndElevationTests:
// CondStyle02_FieldWins), Config/Tests/Conditions/test_condition_elevation.cpp (COND-ELEV-01).
#pragma once
#include "Condition.hpp"
#include "ConditionChanges.hpp"
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

    // The values of TYPE_DEFAULTS a Save writes when the style differs from the stored one: the thickness (in inches) of a style
    // without a Thickness field, and the backout (_type_default_thickness and td["backout"], edit_condition_dialog.py:1236-1239,
    // :1458-1463).
    //   imperial bid: the imperial table thickness (4.0, an Attachment 0.0)
    //   metric bid:   style_dimension_defaults(type, true).thickness, i.e. the metric column (100 mm / 25.4, an Attachment 0.0), the
    //                 value the dialog shows after the style change (COND-STYLE-02, Windows b9902e6)
    // The backout is the table's whatever the measurement system. There is no spacing: a type change saves the Spacing field.
    struct TypeChangeDefaults
    {
        double thickness;
        bool backout;
    };
    TypeChangeDefaults type_change_defaults(ConditionType type, bool metric);

    // _validate_and_build_dto: the condition a Save writes. `stored` is the condition as it is in the database, `form` the
    // dialog's condition after every text field was parsed into it. Only the fields of the controls of form.condition_type are
    // taken from the form (the thickness of a Count, the height of an Area ... have no control and keep their stored value);
    // ColorLine is never taken; the unit of a quantity slot whose quantity is 0 (none) is -1. When the style differs from the
    // stored one, the thickness of a style without a thickness control (Count / Attachment) and the backout are the TYPE_DEFAULTS
    // values of type_change_defaults(form.condition_type, metric); the spacing and the thickness of a Linear / Area are the form's,
    // even when equal to the stored value: what the field shows is what is saved (COND-STYLE-02; that these columns are WRITTEN on
    // a style change although equal is condition_changes_for_save). Without a style change an unchanged field is no change
    // (collect_condition_changes compares with the stored condition). `metric` is the measurement
    // system of the bid (bid.measure_base == 1), it has no default so that no caller can forget it. is_top and z_value of the result come from
    // the saved NAME (derive_elevation_from_name, the loader's derivation), whatever form.is_top / form.z_value hold: the saved
    // condition equals the one a reload gives (COND-ELEV-01).
    Condition condition_to_save(const Condition &stored, const Condition &form, bool metric);

    // _validate_and_build_dto: the UpdateConditionDto of a Save, which is what the Edit Condition dialog hands to the store: the
    // changes whose `values` are condition_to_save(stored, form, metric) and whose `fields` are the columns to write.
    //   without a style change: exactly collect_condition_changes(stored, condition_to_save(...)): a field equal to the stored
    //   one is not written ("without a type change, unchanged fields are left alone")
    //   with a style change (form.condition_type != stored.condition_type): in addition Spacing, Thickness and Backout are always
    //   written, also when equal to the stored value (edit_condition_dialog.py:1458-1463 sets the table thickness and backout,
    //   :1559 / :1589 the Linear / Area thickness field and :1624 the spacing field under `type_changed or ...`). That is
    //   observable where the stored column is NULL: the loader reads it as 0.0, Windows writes 0.0 on a style change. The values are
    //   those of condition_to_save: the fields' for Spacing and the Linear / Area thickness, the table's for the thickness of a
    //   Count / Attachment and for the backout (COND-STYLE-02, Windows b9902e6).
    // The dialog (EditConditionViewController.mm _applyChanges) calls this once and takes the condition to store from `.values`.
    ConditionChanges condition_changes_for_save(const Condition &stored, const Condition &form, bool metric);

}
