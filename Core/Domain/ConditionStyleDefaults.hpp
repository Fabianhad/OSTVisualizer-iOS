// Layer: Domain (pure C++, no UIKit/Foundation/SQLite)
//
// The measurement-system dependent defaults of a new condition style (spacing, thickness, width of a count symbol, first
// unit of measure), extracted from _applyStyleDefaults: in EditConditionViewController.mm (imperial only there).
//
// Windows source of truth: ost_visualizer/presentation/dialogs/edit_condition_dialog.py
//   TYPE_DEFAULTS (:90-143)               "spacing" / "spacing_metric", "thickness" / "thickness_metric", "uom1" / "uom1_metric"
//   _DEFAULT_COUNT_WIDTH_IN / _MM (:144-145)
//   _populate_defaults_for_type (:1241-1252)
//     metric:   spacing = mm_to_inches(spacing_metric), thickness = mm_to_inches(thickness_metric),
//               width = mm_to_inches(300.0), uom1 = uom1_metric
//     imperial: spacing, thickness, width = 12.0, uom1 straight from the table
// A style that is not in TYPE_DEFAULTS (the Windows table miss, td = {}) gets spacing 4.0 / 100 mm, thickness 4.0 / 100 mm,
// uom1 0. The metric flag of a dialog is (bid.measure_base == 1).
#pragma once

#include "DimensionFormat.hpp"
#include "Types.hpp"

namespace ost_domain
{
    // Everything in inches, whatever the measurement system (the dialog shows it with inches_to_display(value, metric)).
    struct StyleDimensionDefaults
    {
        double spacing = 4.0;
        double thickness = 4.0;
        double width = 12.0; // default width of a Count / Attachment symbol
        int uom1 = 0;        // unit code of the first quantity slot
    };

    inline StyleDimensionDefaults style_dimension_defaults(ConditionType type, bool metric)
    {
        // The Windows TYPE_DEFAULTS rows: imperial inches, metric millimetres
        double spacing = 4.0, thickness = 4.0;
        double spacing_metric = 100.0, thickness_metric = 100.0;
        int uom1 = 0, uom1_metric = 0;
        switch (type)
        {
        case ConditionType::Linear:
            uom1 = 2;
            uom1_metric = 13;
            break;
        case ConditionType::Area:
            spacing = 6.0;
            spacing_metric = 150.0;
            uom1 = 5;
            uom1_metric = 14;
            break;
        case ConditionType::Count:
            break;
        case ConditionType::Attachment:
            thickness = 0.0;
            thickness_metric = 0.0;
            break;
        default:
            break;
        }

        StyleDimensionDefaults defaults;
        if (metric)
        {
            defaults.spacing = mm_to_inches(spacing_metric);
            defaults.thickness = mm_to_inches(thickness_metric);
            defaults.width = mm_to_inches(300.0);
            defaults.uom1 = uom1_metric;
        }
        else
        {
            defaults.spacing = spacing;
            defaults.thickness = thickness;
            defaults.width = 12.0;
            defaults.uom1 = uom1;
        }
        return defaults;
    }
}
