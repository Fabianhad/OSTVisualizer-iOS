// Layer: Domain. See ConditionStyleChange.hpp.
#include "ConditionStyleChange.hpp"

#include "ConditionChoices.hpp"
#include "ConditionFormRules.hpp"
#include "PositionParser.hpp"

#include <cmath>

namespace ost_domain
{

    Condition condition_form_for_edit(const Condition &stored)
    {
        // _load_condition (:839-847; Windows 49a2d59) selects the stored type as it is, a type outside
        // 0..3 as the disabled "Unknown (N)" item with _style_type = N, so the form keeps it and a Save without a style choice
        // finds form type == stored type (type_changed False, :1474). Only Trim / Curved Segment are normalised (COND-TRIM-01).
        Condition form = stored;
        normalize_trim_curved(form);
        return form;
    }

    namespace
    {
        // edit_condition_dialog.py _COUNT_LIKE_TYPES, _DISPLAY_NAME_TYPES, _ROUNDING_TYPES
        bool is_count_like(ConditionType type)
        {
            return type == ConditionType::Count || type == ConditionType::Attachment;
        }

        bool has_display_name(ConditionType type)
        {
            return is_count_like(type) || type == ConditionType::Area;
        }

        bool has_rounding(ConditionType type)
        {
            return type == ConditionType::Linear || type == ConditionType::Area;
        }

        // TYPE_DEFAULTS calc_type1
        int default_calc_type1(ConditionType type)
        {
            switch (type)
            {
            case ConditionType::Linear:
                return 1;
            case ConditionType::Area:
                return 11;
            case ConditionType::Count:
            case ConditionType::Attachment:
                break;
            }
            return 23;
        }
    }

    StyleChange apply_style_change(const Condition &form, ConditionType new_type, bool metric)
    {
        const ConditionType previous = form.condition_type;
        const StyleDimensionDefaults defaults = style_dimension_defaults(new_type, metric);
        // _shared_options / _restore_shared_options: a value is carried only when the old and the new style both have its control
        const bool carries_display_name = has_display_name(previous) && has_display_name(new_type);
        const bool carries_rounding = has_rounding(previous) && has_rounding(new_type);

        StyleChange change;
        change.condition = form;
        Condition &c = change.condition;
        c.condition_type = new_type;

        // The results: the first slot restarts from the style's quantity and unit, the others are empty.
        c.calc_type1 = default_calc_type1(new_type);
        c.uom1 = defaults.uom1;
        c.calc_type2 = 0;
        c.uom2 = -1;
        c.calc_type3 = 0;
        c.uom3 = -1;

        // Pattern, spacing, notes, name, Cond. No., elevation, layer, type and colour are not touched (Windows sets the pattern
        // and the spacing text only when the dialog is first populated).
        switch (new_type)
        {
        case ConditionType::Linear:
            c.backout = false;
            c.height = 0.0;
            c.thickness = defaults.thickness;
            c.rise = 0.0;
            c.run = 0.0;
            if (!carries_rounding)
            {
                c.round_quantity = false;
                c.round_up = 0.0;
                change.reset_fields.push_back(StyleField::RoundTo);
            }
            c.drop_run = false;
            c.drop_value = 0.0;
            c.trim = false;
            c.is_curved_segment = false;
            c.connect = true;
            c.connect_tolerance = 6.0;
            for (StyleField field : {StyleField::Height, StyleField::Thickness, StyleField::Rise, StyleField::Run,
                                     StyleField::AddLength, StyleField::Tolerance})
                change.reset_fields.push_back(field);
            break;
        case ConditionType::Area:
            c.backout = false;
            c.thickness = defaults.thickness;
            c.rise = 0.0;
            c.run = 0.0;
            if (!carries_rounding)
            {
                c.round_quantity = false;
                c.round_up = 0.0;
                change.reset_fields.push_back(StyleField::RoundTo);
            }
            c.grid = false;
            c.grid_size1 = 0.0;
            c.grid_size2 = 0.0;
            c.gap = 0.0;
            c.display_grid_while_drawing = true; // TYPE_DEFAULTS Area
            c.display_dimension = false;
            if (!carries_display_name)
                c.display_name = false;
            c.snap_to_linear = -1;
            for (StyleField field : {StyleField::Thickness, StyleField::Rise, StyleField::Run, StyleField::TileX,
                                     StyleField::TileY, StyleField::Gap})
                change.reset_fields.push_back(field);
            break;
        case ConditionType::Count:
        case ConditionType::Attachment:
            c.backout = true;
            c.height = 0.0;
            c.width = defaults.width;
            c.depth = 0.0;
            c.display_size = kDisplaySizeDefault;
            c.thickness = defaults.thickness;
            if (!is_count_like(previous))
                c.shape = ShapeId::Square; // Count <-> Attachment keeps the shape
            if (!carries_display_name)
                c.display_name = false;
            c.connect = true;
            c.connect_tolerance = 6.0;
            for (StyleField field : {StyleField::Height, StyleField::Width, StyleField::Depth, StyleField::DisplaySize,
                                     StyleField::Tolerance})
                change.reset_fields.push_back(field);
            break;
        }
        return change;
    }

    TypeChangeDefaults type_change_defaults(ConditionType type, bool metric)
    {
        // TYPE_DEFAULTS: thickness and backout, what a type change sends (edit_condition_dialog.py:1474-1479). The thickness is
        // _type_default_thickness (:1250-1253): the metric column (mm_to_inches(thickness_metric)) in a metric bid, the imperial
        // one otherwise, the same values apply_style_change puts into the Thickness field. The backout does not depend on the
        // unit. The spacing has no default here: a type change saves the Spacing field (COND-STYLE-02). Only a real style is ever
        // the target of a type change (the "Unknown (N)" entry of the style choice is disabled, COND-M08); a type outside the four
        // would get 4.0 (100 mm) / False.
        const StyleDimensionDefaults dimensions = style_dimension_defaults(type, metric);
        bool backout = false;
        switch (type)
        {
        case ConditionType::Linear:
        case ConditionType::Area:
            break;
        case ConditionType::Count:
        case ConditionType::Attachment:
            backout = true;
            break;
        }
        return TypeChangeDefaults{dimensions.thickness, backout};
    }

    Condition condition_to_save(const Condition &stored, const Condition &form, bool metric)
    {
        Condition out = stored;
        const ConditionType type = form.condition_type;

        // Always compared: name (with the elevation), Cond. No., type, layer, colour, pattern, spacing, the three results, notes.
        // ColorLine is never taken. is_top and z_value are not stored columns: a reload derives them from the name, so the in-memory
        // condition gets them from the saved name with the SAME derivation (derive_elevation_from_name, the loader's), not from what
        // the dialog held: the Top segment (default) with an empty elevation is false / 0.0 after a reload (a text the dialog accepts
        // already has the number a reload gives: it is validated with the loader's own parser, elevation_text_inches, since Windows
        // b9902e6). COND-ELEV-01, decided 2026-11-19: memory equals a reload (Windows takes the dialog's values into the DTO,
        // edit_condition_dialog.py:1471-1473, but shows the reloaded condition).
        out.name = form.name;
        derive_elevation_from_name(out.name, out.z_value, out.is_top);
        out.condition_type = type;
        out.ref_no = form.ref_no;
        out.cdn_type_uid = form.cdn_type_uid;
        out.cdn_type_name = form.cdn_type_name;
        out.layer_uid = form.layer_uid;
        out.color_fill = form.color_fill;
        out.pattern = form.pattern;
        out.spacing = form.spacing;
        // a slot without a quantity has no unit: `uom = -1 if calc == 0 else uom_combo.currentData()`, but only for a slot the user
        // changed: `if self._qty_rows()[i] == self._saved_qty_rows[i]: continue` (Windows d26893c, edit_condition_dialog.py :1905-1925)
        // skips a slot whose quantity is as loaded, and the unit combo of a slot of calc 0 is empty, so a stored slot of calc 0 with
        // another unit (an import) keeps that unit through any Save.
        out.calc_type1 = form.calc_type1;
        out.uom1 = form.calc_type1 == 0 ? (stored.calc_type1 == 0 ? stored.uom1 : -1) : form.uom1;
        out.calc_type2 = form.calc_type2;
        out.uom2 = form.calc_type2 == 0 ? (stored.calc_type2 == 0 ? stored.uom2 : -1) : form.uom2;
        out.calc_type3 = form.calc_type3;
        out.uom3 = form.calc_type3 == 0 ? (stored.calc_type3 == 0 ? stored.uom3 : -1) : form.uom3;
        out.notes = form.notes;

        // The controls of the style: Linear, Count / Attachment, and everything else (Area and a stored type outside 0..3, COND-M08)
        // is the Area-like branch (edit_condition_dialog.py :1601-1630, :1712-1741)
        const EditorLayout layout = editor_layout(type);
        if (layout == EditorLayout::CountLike)
        {
            out.height = form.height;
            out.width = form.width;
            out.depth = form.depth;
            out.display_size = form.display_size;
            out.shape = form.shape;
            out.display_name = form.display_name;
            out.connect = form.connect;
            out.connect_tolerance = form.connect_tolerance;
        }
        else if (layout == EditorLayout::Linear)
        {
            out.height = form.height;
            out.thickness = form.thickness;
            out.rise = form.rise;
            out.run = form.run;
            out.round_quantity = form.round_quantity;
            out.round_up = form.round_up;
            out.drop_run = form.drop_run;
            out.drop_value = form.drop_value;
            out.trim = form.trim;
            out.is_curved_segment = form.is_curved_segment;
            out.connect = form.connect;
            out.connect_tolerance = form.connect_tolerance;
        }
        else
        {
            out.thickness = form.thickness;
            // The form shows abs(rise) / abs(run) and the save compares abs(typed) with abs(stored) (Windows d26893c,
            // edit_condition_dialog.py :1860, :1882): a stored negative value is not rewritten by a save that did not change it,
            // a changed value is written as its absolute value. The Linear branch above compares the signed values.
            out.rise = std::fabs(form.rise) == std::fabs(stored.rise) ? stored.rise : std::fabs(form.rise);
            out.run = std::fabs(form.run) == std::fabs(stored.run) ? stored.run : std::fabs(form.run);
            out.round_quantity = form.round_quantity;
            out.round_up = form.round_up;
            out.grid = form.grid;
            out.grid_size1 = form.grid_size1;
            out.grid_size2 = form.grid_size2;
            out.gap = form.gap;
            out.display_grid_while_drawing = form.display_grid_while_drawing;
            out.display_dimension = form.display_dimension;
            out.display_name = form.display_name;
            out.snap_to_linear = form.snap_to_linear;
        }

        // A style that differs from the stored one (edit_condition_dialog.py:1474-1479): dto.set("thickness",
        // _type_default_thickness(td)) and dto.set("backout", td["backout"]); the spacing is NOT taken from the table. What the
        // fields show is what is saved: the spacing (:1640) and the Thickness of a Linear / Area (:1575, :1605) are written from
        // the field on a style change even when equal to the stored value (`if type_changed or value != stored`), which is the
        // `out.spacing` / `out.thickness` copied from the form above (condition_changes_for_save marks those columns as written
        // on a style change). Only a Count / Attachment, which has no Thickness field,
        // keeps the table's thickness, and the backout is the table's. In a metric bid the thickness is the metric default
        // (100 mm, an Attachment 0), see type_change_defaults. COND-STYLE-02, Windows b9902e6.
        if (type != stored.condition_type)
        {
            const TypeChangeDefaults defaults = type_change_defaults(type, metric);
            if (is_count_like(type))
                out.thickness = defaults.thickness;
            out.backout = defaults.backout;
        }
        return out;
    }

    ConditionChanges condition_changes_for_save(const Condition &stored, const Condition &form, bool metric)
    {
        ConditionChanges changes = collect_condition_changes(stored, condition_to_save(stored, form, metric));
        // `type_changed = ct != cond.condition_type` (edit_condition_dialog.py:1474): on a style change the DTO always carries the
        // thickness and the backout of the table (:1475-1479), the Linear / Area thickness field (:1575, :1605:
        // `if type_changed or thickness != cond.thickness`) and the spacing field (:1640), so the UPDATE writes those columns even
        // when the value equals the stored one (observable where the stored column is NULL: Windows writes 0.0). The values are
        // already in `changes.values` (condition_to_save). Without a style change nothing is added: an unchanged field is left alone.
        if (form.condition_type != stored.condition_type)
        {
            changes.mark(ConditionField::Spacing);
            changes.mark(ConditionField::Thickness);
            changes.mark(ConditionField::Backout);
        }
        return changes;
    }

}
