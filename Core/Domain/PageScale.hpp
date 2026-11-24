// Layer: Domain (pure C++)
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The page-scale presets and the labels of a scale (Set Scale, plan 5.4). Windows source of truth:
//   ost_visualizer/presentation/utils/scales.py:4-55      ARCH_SCALES (12), CIVIL_SCALES (11), METRIC_SCALES (17), SCALES_BY_STYLE, ALL_SCALES (40)
//   ost_visualizer/presentation/utils/scales.py:58-74     format_custom_scale
//   ost_visualizer/presentation/dialogs/set_scale_dialog.py:158-165   _find_matching_predefined_scale
//   ost_visualizer/presentation/components/page_settings_bar.py:159-206   load_page / _predefined_scale_index (the label of the bar)
// A page scale is the pair ScaleFactor1 (drawing side) and ScaleFactor2 (real side), the same unit: ratio = sf2 / sf1.
//
// STATE: implemented (PageScale.cpp). The declarations are the contract the host tests of Config/Tests/Formatting pin.
#pragma once

#include <string>
#include <vector>

namespace ost_domain
{
    // The scale styles of the Set Scale dialog (set_scale_dialog.py:10-15). Style 1 is the default when no preset matches.
    inline constexpr int kScaleStyleArchitectural = 1;
    inline constexpr int kScaleStyleCivil = 2;
    inline constexpr int kScaleStyleMetric = 3;

    // One preset: the pair and its label exactly as written in scales.py (e.g. `1/8" = 1' 0"`, `1" = 20' 0"`, `1 : 100`).
    struct ScalePreset
    {
        double sf1 = 0.0;
        double sf2 = 0.0;
        const char *label = "";
    };

    // The presets of one style in the Windows order: 12 (Architectural), 11 (Civil), 17 (Metric); any other style gives an empty list.
    const std::vector<ScalePreset> &scale_presets(int style);

    // ALL_SCALES: Architectural + Civil + Metric = 40 entries (the pair (1.0, 1.0) occurs twice: Civil `1" = 1"` and Metric `1 : 1`).
    const std::vector<ScalePreset> &all_scale_presets();

    // The dialog's match (set_scale_dialog.py:158-165): walks styles 1, 2, 3 in this order and returns the first preset whose sf1 AND sf2
    // both differ from the given values by LESS than 1e-9 (strict). No match, or a NaN value: {kScaleStyleArchitectural, -1}.
    struct PresetMatch
    {
        int style = kScaleStyleArchitectural;
        int index = -1;
    };
    PresetMatch find_matching_preset(double sf1, double sf2);

    // The bar's match (page_settings_bar.py:193-206): the first entry of all_scale_presets() within 1e-9 (strict) on both values, as its
    // index in that list; -1 when none (also for NaN).
    int bar_preset_index(double sf1, double sf2);

    // scales.py:58-74: "" when either value is not finite or is <= 0; otherwise f"{sf1:.15g}" followed by `" = 1' 0"` when sf2 is within an
    // absolute 1e-9 of 12.0 (math.isclose(sf2, 12.0, rel_tol=0, abs_tol=1e-9)), else " : " and f"{sf2:.15g}".
    // 0.125, 12 -> `0.125" = 1' 0"`   1, 120 -> `1 : 120`   0.000000123456789, 1 -> `1.23456789e-07 : 1`.
    std::string format_custom_scale(double sf1, double sf2);

    // The label the page bar shows for a page (page_settings_bar.py:159-181): the label of the first preset that matches
    // (bar_preset_index), else format_custom_scale; "" when the pair is invalid (nothing is selected on the bar).
    std::string scale_display_label(double sf1, double sf2);
}
