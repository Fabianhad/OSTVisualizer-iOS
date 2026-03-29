// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
#include <string>

namespace ost_domain
{

    struct RenderConfig
    {
        static constexpr const char *MODE_SOLID = "Solid";
        static constexpr const char *MODE_OST = "OST";
        static constexpr const char *MODE_TRANSPARENT = "Transparent";

        std::string color_mode = MODE_SOLID;
        bool grayscale_enabled = false;

        bool is_solid() const { return color_mode == MODE_SOLID; }
        bool is_ost() const { return color_mode == MODE_OST; }
        bool is_transparent() const { return color_mode == MODE_TRANSPARENT; }

        bool is_valid_mode(const std::string &mode) const
        {
            return mode == MODE_SOLID || mode == MODE_OST || mode == MODE_TRANSPARENT;
        }
    };

}
