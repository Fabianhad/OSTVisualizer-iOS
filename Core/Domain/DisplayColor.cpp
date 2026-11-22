// Plain IEEE double arithmetic (strict_fp.hpp): the truncated gray byte must not depend on a fused multiply-add.
#include "../Geometry/strict_fp.hpp"
#include "DisplayColor.hpp"
#include <cmath>
#include <cstdio>

namespace ost_domain
{

    uint8_t gray_byte(uint8_t r, uint8_t g, uint8_t b)
    {
        const double rr = static_cast<double>(r) / 255.0;
        const double gg = static_cast<double>(g) / 255.0;
        const double bb = static_cast<double>(b) / 255.0;
        // Python evaluates ((0.299 * r) + (0.587 * g)) + (0.114 * b) with every product and sum rounded to double. The
        // products go through volatile objects so that no compiler setting (not even -ffp-contract=fast, which ignores the
        // pragma above) can fuse a product into the following sum: a fused sum differs in the last bit and the truncation
        // below turns that into a different byte for some colours.
        volatile double product_r = 0.299 * rr;
        volatile double product_g = 0.587 * gg;
        volatile double product_b = 0.114 * bb;
        const double partial = product_r + product_g;
        const double gray = partial + product_b;
        const int gray_int = static_cast<int>(gray * 255.0);
        return static_cast<uint8_t>(gray_int);
    }

    double display_opacity(const std::string &display_mode, int pattern)
    {
        if (display_mode == RenderConfig::MODE_ORIGINAL)
        {
            const int pattern_type = pattern != 0 ? pattern : static_cast<int>(Pattern::Solid);
            return pattern_type == static_cast<int>(Pattern::Solid) ? 1.0 : 0.5;
        }
        if (display_mode == RenderConfig::MODE_TRANSPARENT)
            return 0.5;
        return 1.0;
    }

    double display_opacity_2d(const std::string &display_mode, int pattern)
    {
        if (display_mode == RenderConfig::MODE_ORIGINAL)
        {
            if (pattern == static_cast<int>(Pattern::None))
                return 0.0;
            return pattern == static_cast<int>(Pattern::Solid) ? 1.0 : 0.5;
        }
        if (display_mode == RenderConfig::MODE_TRANSPARENT)
            return 0.5;
        return 1.0;
    }

    std::string DisplayColor::hex() const
    {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", static_cast<unsigned>(red), static_cast<unsigned>(green),
                      static_cast<unsigned>(blue));
        return buf;
    }

    DisplayColor display_color(const Condition &condition, const std::string &display_mode, bool grayscale_enabled)
    {
        const unsigned int c = static_cast<unsigned int>(condition.color_fill);
        uint8_t red = static_cast<uint8_t>(c & 0xFFu);
        uint8_t green = static_cast<uint8_t>((c >> 8) & 0xFFu);
        uint8_t blue = static_cast<uint8_t>((c >> 16) & 0xFFu);
        if (grayscale_enabled)
        {
            const uint8_t gray = gray_byte(red, green, blue);
            red = green = blue = gray;
        }
        DisplayColor out;
        out.red = red;
        out.green = green;
        out.blue = blue;
        out.r = static_cast<float>(red) / 255.0f;
        out.g = static_cast<float>(green) / 255.0f;
        out.b = static_cast<float>(blue) / 255.0f;
        out.opacity = static_cast<float>(display_opacity(display_mode, static_cast<int>(condition.pattern)));
        return out;
    }

    namespace
    {
        // The colour byte of a channel that was made as float(byte) / 255.0f: the nearest byte, clamped to 0..255 (a
        // channel outside the range, NaN included, never reads as anything but a valid byte).
        uint8_t byte_of_channel(float channel)
        {
            if (!(channel > 0.0f))
                return 0;
            if (channel >= 1.0f)
                return 255;
            return static_cast<uint8_t>(std::lround(channel * 255.0f));
        }
    } // namespace

    void grayscale_channels(float &r, float &g, float &b)
    {
        const uint8_t gray = gray_byte(byte_of_channel(r), byte_of_channel(g), byte_of_channel(b));
        r = g = b = static_cast<float>(gray) / 255.0f;
    }

}
