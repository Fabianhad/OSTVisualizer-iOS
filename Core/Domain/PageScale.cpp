// Layer: Domain. See PageScale.hpp.
// Windows source of truth: ost_visualizer/presentation/utils/scales.py:4-74, presentation/dialogs/set_scale_dialog.py:158-165,
// presentation/components/page_settings_bar.py:159-206. The label text is built with the Python formatters of NumberText (format_general = f"{v:.15g}").
#include "PageScale.hpp"

#include "NumberText.hpp"

#include <cmath>

namespace ost_domain
{
    namespace
    {
        // scales.py:4-55, in the Windows order, every label as written in the source.
        const std::vector<ScalePreset> &architectural()
        {
            static const std::vector<ScalePreset> list = {
                {0.03125, 12.0, "1/32\" = 1' 0\""}, {0.0625, 12.0, "1/16\" = 1' 0\""}, {0.09375, 12.0, "3/32\" = 1' 0\""},
                {0.125, 12.0, "1/8\" = 1' 0\""},    {0.1875, 12.0, "3/16\" = 1' 0\""}, {0.25, 12.0, "1/4\" = 1' 0\""},
                {0.375, 12.0, "3/8\" = 1' 0\""},    {0.5, 12.0, "1/2\" = 1' 0\""},     {0.75, 12.0, "3/4\" = 1' 0\""},
                {1.0, 12.0, "1\" = 1' 0\""},        {1.5, 12.0, "1-1/2\" = 1' 0\""},   {3.0, 12.0, "3\" = 1' 0\""},
            };
            return list;
        }

        const std::vector<ScalePreset> &civil()
        {
            static const std::vector<ScalePreset> list = {
                {1.0, 1.0, "1\" = 1\""},       {1.0, 120.0, "1\" = 10' 0\""}, {1.0, 240.0, "1\" = 20' 0\""},  {1.0, 360.0, "1\" = 30' 0\""},
                {1.0, 480.0, "1\" = 40' 0\""}, {1.0, 600.0, "1\" = 50' 0\""}, {1.0, 720.0, "1\" = 60' 0\""},  {1.0, 840.0, "1\" = 70' 0\""},
                {1.0, 960.0, "1\" = 80' 0\""}, {1.0, 1080.0, "1\" = 90' 0\""}, {1.0, 1200.0, "1\" = 100' 0\""},
            };
            return list;
        }

        const std::vector<ScalePreset> &metric()
        {
            static const std::vector<ScalePreset> list = {
                {1.0, 1000.0, "1 : 1000"}, {1.0, 500.0, "1 : 500"}, {1.0, 300.0, "1 : 300"}, {1.0, 200.0, "1 : 200"}, {1.0, 100.0, "1 : 100"}, {1.0, 60.0, "1 : 60"},
                {1.0, 50.0, "1 : 50"},     {1.0, 40.0, "1 : 40"},   {1.0, 30.0, "1 : 30"},   {1.0, 20.0, "1 : 20"},   {1.0, 10.0, "1 : 10"},   {1.0, 5.0, "1 : 5"},
                {1.0, 2.0, "1 : 2"},       {1.0, 1.0, "1 : 1"},     {10.0, 1.0, "10 : 1"},   {20.0, 1.0, "20 : 1"},   {50.0, 1.0, "50 : 1"},
            };
            return list;
        }

        // abs(a - b) < 1e-9 on BOTH values (set_scale_dialog.py:162-163, page_settings_bar.py:199-202): a NaN difference is not smaller than anything.
        bool near_pair(const ScalePreset &preset, double sf1, double sf2)
        {
            return std::fabs(preset.sf1 - sf1) < 1e-9 && std::fabs(preset.sf2 - sf2) < 1e-9;
        }
    }

    const std::vector<ScalePreset> &scale_presets(int style)
    {
        static const std::vector<ScalePreset> none;
        switch (style)
        {
        case kScaleStyleArchitectural:
            return architectural();
        case kScaleStyleCivil:
            return civil();
        case kScaleStyleMetric:
            return metric();
        default:
            return none;
        }
    }

    const std::vector<ScalePreset> &all_scale_presets()
    {
        static const std::vector<ScalePreset> all = []
        {
            std::vector<ScalePreset> list;
            for (int style : {kScaleStyleArchitectural, kScaleStyleCivil, kScaleStyleMetric})
            {
                const std::vector<ScalePreset> &part = scale_presets(style);
                list.insert(list.end(), part.begin(), part.end());
            }
            return list;
        }();
        return all;
    }

    PresetMatch find_matching_preset(double sf1, double sf2)
    {
        for (int style : {kScaleStyleArchitectural, kScaleStyleCivil, kScaleStyleMetric})
        {
            const std::vector<ScalePreset> &list = scale_presets(style);
            for (size_t index = 0; index < list.size(); ++index)
            {
                if (near_pair(list[index], sf1, sf2))
                {
                    PresetMatch match;
                    match.style = style;
                    match.index = static_cast<int>(index);
                    return match;
                }
            }
        }
        return PresetMatch{}; // style 1, index -1
    }

    int bar_preset_index(double sf1, double sf2)
    {
        const std::vector<ScalePreset> &all = all_scale_presets();
        for (size_t index = 0; index < all.size(); ++index)
        {
            if (near_pair(all[index], sf1, sf2))
                return static_cast<int>(index);
        }
        return -1;
    }

    std::string format_custom_scale(double sf1, double sf2)
    {
        if (!std::isfinite(sf1) || !std::isfinite(sf2) || sf1 <= 0.0 || sf2 <= 0.0)
            return std::string();
        const std::string drawing = format_general(sf1, 15);
        // math.isclose(real, 12.0, rel_tol=0.0, abs_tol=1e-9) is abs(real - 12.0) <= 1e-9.
        if (std::fabs(sf2 - 12.0) <= 1e-9)
            return drawing + "\" = 1' 0\"";
        return drawing + " : " + format_general(sf2, 15);
    }

    std::string scale_display_label(double sf1, double sf2)
    {
        const int index = bar_preset_index(sf1, sf2);
        if (index >= 0)
            return all_scale_presets()[static_cast<size_t>(index)].label;
        return format_custom_scale(sf1, sf2);
    }
}
