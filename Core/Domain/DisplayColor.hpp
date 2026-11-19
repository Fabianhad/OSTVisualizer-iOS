// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
// The colour and opacity an element has in the 3D view and in every export (OBJ / MTL, DXF layers, HTML). Port of the
// Windows presentation/visualization/services/color_service.py (get_color_mapping + get_color_for_takeoff +
// _as_grayscale) and domain/entities/pattern.py (get_3d_opacity): one function for every consumer, so the views and the
// exports cannot disagree (EXP-06).
#include "Condition.hpp"
#include "RenderConfig.hpp"
#include <cstdint>
#include <string>

namespace ost_domain
{

    // Windows color_service._as_grayscale: the byte of gray_int = int(gray * 255) with
    // gray = 0.299 * r + 0.587 * g + 0.114 * b and r, g, b = byte / 255.0 (double, no fused multiply-add), truncated
    // towards zero; the result is written three times. White (255, 255, 255) gives 254 and (255, 0, 0) gives 76.
    uint8_t gray_byte(uint8_t r, uint8_t g, uint8_t b);

    // The opacity of an element of a condition with the pattern code `pattern` (Windows get_color_for_takeoff):
    //   Original    pattern_type = pattern if pattern else SOLID; SOLID (1) gives 1.0, every other code (2..8, unknown
    //               codes, negative codes) gives 0.5, so the pattern 0 (None) gives 1.0
    //   Transparent 0.5 (the colour map opacity 1.0 * 0.5)
    //   anything else (Solid, a mode outside the vocabulary) 1.0
    // The mode is compared exactly (case-sensitive), like Windows.
    double display_opacity(const std::string &display_mode, int pattern);

    // The fill opacity of a takeoff in the 2D plan view (COL-04; Windows color_service.get_color_mapping with
    // pattern.get_2d_opacity, then get_2d_color_for_takeoff):
    //   Original     the pattern 0 (None) gives 0.0 (no fill, the outline is still drawn: Windows paints its 2D fill clear),
    //                SOLID (1) gives 1.0, TRANSPARENT (8) and every unknown code 0.5. The line patterns 2..7 give 0.5: Windows
    //                gives them 0.0 and draws hatch lines instead, which iOS cannot draw, so a clear fill would make the
    //                condition invisible (user decision 2026-11-16, option c: keep the 0.5 approximation)
    //   Transparent  0.5
    //   anything else (Solid, a mode outside the vocabulary) 1.0
    // The mode is compared exactly (case-sensitive), like Windows.
    double display_opacity_2d(const std::string &display_mode, int pattern);

    struct DisplayColor
    {
        uint8_t red = 0, green = 0, blue = 0; // the colour byte (grayscale already applied)
        // The channel as the app hands it to the writers: float(byte) / 255.0f (Condition::rgb_from_color_fill), so a
        // writer recovers the byte exactly.
        float r = 0.0f, g = 0.0f, b = 0.0f;
        float opacity = 1.0f; // 1.0, 0.5

        // "#rrggbb" in lower case (Windows int_to_hex / _as_grayscale), the text the DXF layer colour takes.
        std::string hex() const;
    };

    // Windows int_to_hex(condition.color_fill) (r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF on the two's
    // complement of a negative value), then _as_grayscale when `grayscale_enabled`, with the opacity of
    // display_opacity(display_mode, pattern). The inactive-area colour of the Windows page_area_selections has no iOS
    // counterpart.
    DisplayColor display_color(const Condition &condition, const std::string &display_mode, bool grayscale_enabled);

    inline DisplayColor display_color(const Condition &condition, const RenderConfig &config)
    {
        return display_color(condition, config.color_mode, config.grayscale_enabled);
    }

    // The grayscale of the 2D overlays (COL-02): the three channels of a colour as the app holds them (float(byte) / 255.0f,
    // Condition::rgb_from_color_fill) become the channels of gray_byte(r, g, b) written three times, so a 2D overlay has
    // exactly the gray the 3D view and the exports have (Windows color_service._as_grayscale: the colour is quantised to a
    // byte). A channel is read back as the nearest byte (lround(channel * 255), clamped to 0..255).
    void grayscale_channels(float &r, float &g, float &b);

}
