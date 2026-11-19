// The grayscale of the 2D overlays (COL-02): Core/Domain/DisplayColor.* grayscale_channels.
// Windows oracle: presentation/visualization/services/color_service.py _as_grayscale (r, g, b = byte / 255.0; gray =
// 0.299 * r + 0.587 * g + 0.114 * b; gray_int = int(gray * 255), truncated; the byte is written three times), pinned by
// tests/presentation/visualization/services/test_color_service.py::ColorMappingTests::
// test_grayscale_desaturates_colors_and_legend_but_keeps_opacity. The 3D view and the exports get the same byte from
// display_color (test_display_color.cpp); the 2D overlay and the placement preview must not differ by a rounding step.
#include "ExportTestSupport.hpp"

#include "Domain/Condition.hpp"
#include "Domain/DisplayColor.hpp"

#include <cstdint>

namespace
{
    // The channels of a colour byte as the overlays hold them (Condition::rgb_from_color_fill: float(byte) / 255.0f).
    float channel_of(unsigned byte) { return static_cast<float>(byte) / 255.0f; }
} // namespace

TEST(GrayscaleChannels, EveryChannelBecomesTheWindowsGrayByteOver255)
{
    // The COL-02 plan values: (255,0,0) -> 76, (0,255,0) -> 149, (0,0,255) -> 29, (200,200,200) -> 199,
    // (255,255,255) -> 254 (0.299 + 0.587 + 0.114 is 0.9999999999999999), (127,127,127) -> 127, (18,171,52) -> 111,
    // #996633 -> 111 (the Windows test: 0.437 -> 111 -> #6f6f6f).
    struct Row
    {
        unsigned r, g, b;
        unsigned expected;
    };
    const Row rows[] = {{255, 0, 0, 76},     {0, 255, 0, 149},     {0, 0, 255, 29},   {200, 200, 200, 199}, {255, 255, 255, 254},
                        {127, 127, 127, 127}, {18, 171, 52, 111},   {0x99, 0x66, 0x33, 111}, {0, 0, 0, 0},      {128, 128, 128, 128}};
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("colour " + std::to_string(row.r) + "," + std::to_string(row.g) + "," + std::to_string(row.b));
        float r = channel_of(row.r), g = channel_of(row.g), b = channel_of(row.b);
        ost_domain::grayscale_channels(r, g, b);
        CHECK_EQ(r, channel_of(row.expected));
        CHECK_EQ(g, channel_of(row.expected));
        CHECK_EQ(b, channel_of(row.expected));
    }
}

TEST(GrayscaleChannels, WhiteAndRedAreNotTheUnquantisedFloatGray)
{
    // The old iOS formula (float gray = 0.299f * r + 0.587f * g + 0.114f * b, no truncation) gave 1.0 for white and
    // 0.299 for red: Windows has 254 / 255 and 76 / 255.
    float r = 1.0f, g = 1.0f, b = 1.0f;
    ost_domain::grayscale_channels(r, g, b);
    CHECK(r < 1.0f);
    CHECK_EQ(r, 254.0f / 255.0f);
    float r2 = 1.0f, g2 = 0.0f, b2 = 0.0f;
    ost_domain::grayscale_channels(r2, g2, b2);
    CHECK_EQ(r2, 76.0f / 255.0f);
}

TEST(GrayscaleChannels, AgreesWithTheGrayOfTheThreeDViewForEveryColourByteOfAGreyRamp)
{
    // The 3D view and the exports use display_color (gray_byte of the colour bytes); the overlay must give the same byte
    // for every gray level and a spread of colours.
    for (unsigned v = 0; v < 256; ++v)
    {
        SCOPED_CONTEXT("grey level " + std::to_string(v));
        ost_domain::Condition condition;
        condition.color_fill = static_cast<int>(v | (v << 8) | (v << 16));
        const ost_domain::DisplayColor view = ost_domain::display_color(condition, ost_domain::RenderConfig::MODE_SOLID, true);
        float r = channel_of(v), g = channel_of(v), b = channel_of(v);
        ost_domain::grayscale_channels(r, g, b);
        CHECK_EQ(r, view.r);
        CHECK_EQ(g, view.g);
        CHECK_EQ(b, view.b);
    }
    for (unsigned seed = 0; seed < 500; ++seed)
    {
        const unsigned red = (seed * 37u) & 0xFFu, green = (seed * 91u + 13u) & 0xFFu, blue = (seed * 53u + 200u) & 0xFFu;
        SCOPED_CONTEXT("colour " + std::to_string(red) + "," + std::to_string(green) + "," + std::to_string(blue));
        ost_domain::Condition condition;
        condition.color_fill = static_cast<int>(red | (green << 8) | (blue << 16));
        const ost_domain::DisplayColor view = ost_domain::display_color(condition, ost_domain::RenderConfig::MODE_SOLID, true);
        float r = channel_of(red), g = channel_of(green), b = channel_of(blue);
        ost_domain::grayscale_channels(r, g, b);
        CHECK_EQ(r, view.r);
        CHECK_EQ(g, view.g);
        CHECK_EQ(b, view.b);
    }
}
