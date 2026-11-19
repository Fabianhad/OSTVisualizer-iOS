// Layer: Domain. See ConditionStyleChange.hpp.
#include "ConditionStyleChange.hpp"

#include "ConditionFormRules.hpp"
#include "PositionParser.hpp"

namespace ost_domain
{

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
        // TYPE_DEFAULTS: thickness, spacing, backout. Imperial bid: the imperial table columns, exactly what Windows sends
        // (edit_condition_dialog.py:1445-1448). Metric bid (COND-STYLE-02, decision of 2026-11-19, a deliberate deviation: Windows
        // sends the imperial columns here too): the metric columns, i.e. what _populate_defaults_for_type (:1241-1252) shows after
        // the style change, the same values apply_style_change puts into the form. The backout does not depend on the unit.
        // Unknown styles fall back to 4.0 / 4.0 (100 mm) / False.
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
        return TypeChangeDefaults{dimensions.thickness, dimensions.spacing, backout};
    }

    Condition condition_to_save(const Condition &stored, const Condition &form, bool metric)
    {
        Condition out = stored;
        const ConditionType type = form.condition_type;

        // Always compared: name (with the elevation), Cond. No., type, layer, colour, pattern, spacing, the three results, notes.
        // ColorLine is never taken. is_top and z_value are not stored columns: a reload derives them from the name, so the in-memory
        // condition gets them from the saved name with the SAME derivation (derive_elevation_from_name, the loader's), not from what
        // the dialog held: the Top segment (default) with an empty elevation is false / 0.0 after a reload, and the loader grammar
        // reads no metric text such as "3200". COND-ELEV-01, decided 2026-11-19: memory equals a reload (Windows takes the dialog's
        // values into the DTO, edit_condition_dialog.py:1427-1441, but shows the reloaded condition).
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
        // a slot without a quantity has no unit: `uom = -1 if calc == 0 else uom_combo.currentData()` (a stored slot of calc 0
        // with another unit is read back as -1, so a Save writes -1)
        out.calc_type1 = form.calc_type1;
        out.uom1 = form.calc_type1 == 0 ? -1 : form.uom1;
        out.calc_type2 = form.calc_type2;
        out.uom2 = form.calc_type2 == 0 ? -1 : form.uom2;
        out.calc_type3 = form.calc_type3;
        out.uom3 = form.calc_type3 == 0 ? -1 : form.uom3;
        out.notes = form.notes;

        // The controls of the style
        if (is_count_like(type))
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
        else if (type == ConditionType::Linear)
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
            out.rise = form.rise;
            out.run = form.run;
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

        // A style that differs from the stored one: dto.set("thickness", td), dto.set("spacing", td), dto.set("backout", td); a
        // field whose value differs from the stored one is set afterwards and wins. A Count / Attachment has no thickness field.
        // `td` is the imperial table in an imperial bid (= Windows) and the metric table in a metric bid (COND-STYLE-02, the
        // decision of 2026-11-19: Windows writes the imperial table there too), see type_change_defaults.
        if (type != stored.condition_type)
        {
            const TypeChangeDefaults defaults = type_change_defaults(type, metric);
            if (is_count_like(type) || form.thickness == stored.thickness)
                out.thickness = defaults.thickness;
            if (form.spacing == stored.spacing)
                out.spacing = defaults.spacing;
            out.backout = defaults.backout;
        }
        return out;
    }

}
