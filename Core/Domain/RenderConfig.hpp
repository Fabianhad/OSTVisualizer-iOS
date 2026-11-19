// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
#include <string>

namespace ost_domain
{

    struct RenderConfig
    {
        // The display-mode vocabulary of the Windows app (domain/entities/config.py Config.DISPLAY_MODE_SOLID,
        // DISPLAY_MODE_ORIGINAL, DISPLAY_MODE_TRANSPARENT). "Original" is the pattern-driven mode the iOS app used to call
        // "OST".
        static constexpr const char *MODE_SOLID = "Solid";
        static constexpr const char *MODE_ORIGINAL = "Original";
        static constexpr const char *MODE_TRANSPARENT = "Transparent";
        // Deprecated alias of MODE_ORIGINAL (the former iOS name of that mode); the stored text "OST" itself is not part
        // of the vocabulary any more and, like every unknown mode, is replaced by the default.
        static constexpr const char *MODE_OST = MODE_ORIGINAL;
        // Windows Config.DEFAULT_DISPLAY_MODE: the default mode, and what a mode outside the vocabulary becomes.
        static constexpr const char *DEFAULT_MODE = MODE_ORIGINAL;

        std::string color_mode = DEFAULT_MODE;
        bool grayscale_enabled = false;

        bool is_solid() const { return color_mode == MODE_SOLID; }
        bool is_original() const { return color_mode == MODE_ORIGINAL; }
        bool is_ost() const { return is_original(); } // deprecated alias of is_original()
        bool is_transparent() const { return color_mode == MODE_TRANSPARENT; }

        // Windows ConfigAggregate.VALID_DISPLAY_MODES: an exact, case-sensitive match.
        static bool is_valid_mode(const std::string &mode)
        {
            return mode == MODE_SOLID || mode == MODE_ORIGINAL || mode == MODE_TRANSPARENT;
        }

        // Windows ConfigAggregate._validated_display_mode: a valid mode as it is, anything else the default.
        static std::string validated_mode(const std::string &mode)
        {
            return is_valid_mode(mode) ? mode : std::string(DEFAULT_MODE);
        }
    };

}
