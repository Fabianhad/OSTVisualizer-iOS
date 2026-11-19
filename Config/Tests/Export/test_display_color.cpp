// The colour and opacity of exported elements (EXP-06; also the 3D view): Core/Domain/DisplayColor.*.
// Windows oracle: presentation/visualization/services/color_service.py (get_color_mapping, get_color_for_takeoff,
// _as_grayscale) and domain/entities/pattern.py get_3d_opacity, run read-only (ExportRuleOracleData.hpp).
#include "ExportTestSupport.hpp"
#include "ExportRuleOracleData.hpp"

#include "Domain/Condition.hpp"
#include "Domain/DisplayColor.hpp"
#include "Domain/RenderConfig.hpp"
#include "Export/export_text.hpp"

#include <cstdio>
#include <string>

using ost_domain::Condition;
using ost_domain::DisplayColor;
using ost_domain::RenderConfig;

namespace
{
    Condition condition(int fill, int pattern)
    {
        Condition c;
        c.uid = "c";
        c.name = "c";
        c.color_fill = fill;
        c.pattern = static_cast<ost_domain::Pattern>(pattern);
        return c;
    }
} // namespace

TEST(DisplayColor, GrayByteIsTheTruncatedWindowsByte)
{
    // Port of tests/presentation/visualization/services/test_color_service.py (grayscale: (0.6, 0.4, 0.2) -> 111 -> #6f6f6f)
    // and the EXP-06 plan: 0x0000FF -> 76 (the plan's BGR order: red 255), green 149, blue 29, 0x808080 -> 128, gray level
    // 5 -> 4; white 255 -> 254 (0.299 + 0.587 + 0.114 is 0.9999999999999999).
    CHECK_EQ(ost_domain::gray_byte(255, 0, 0), 76);
    CHECK_EQ(ost_domain::gray_byte(0, 255, 0), 149);
    CHECK_EQ(ost_domain::gray_byte(0, 0, 255), 29);
    CHECK_EQ(ost_domain::gray_byte(0x80, 0x80, 0x80), 128);
    CHECK_EQ(ost_domain::gray_byte(5, 5, 5), 4);
    CHECK_EQ(ost_domain::gray_byte(255, 255, 255), 254);
    CHECK_EQ(ost_domain::gray_byte(200, 200, 200), 199);
    CHECK_EQ(ost_domain::gray_byte(127, 127, 127), 127);
    CHECK_EQ(ost_domain::gray_byte(18, 171, 52), 111);
    CHECK_EQ(ost_domain::gray_byte(0x99, 0x66, 0x33), 111);
    CHECK_EQ(ost_domain::gray_byte(0, 0, 0), 0);
}

TEST(DisplayColor, GrayByteMatchesWindowsOnEveryGreyLevelAndRandomColours)
{
    const auto &levels = export_rule_oracle::gray_levels();
    REQUIRE_EQ(levels.size(), static_cast<size_t>(256));
    for (int v = 0; v < 256; ++v)
    {
        SCOPED_CONTEXT("grey level " + std::to_string(v));
        CHECK_EQ(ost_domain::gray_byte(static_cast<uint8_t>(v), static_cast<uint8_t>(v), static_cast<uint8_t>(v)), levels[static_cast<size_t>(v)]);
    }
    for (const auto &t : export_rule_oracle::gray_triples())
    {
        SCOPED_CONTEXT("colour " + std::to_string(t.r) + "," + std::to_string(t.g) + "," + std::to_string(t.b));
        CHECK_EQ(ost_domain::gray_byte(t.r, t.g, t.b), t.gray);
    }
}

TEST(DisplayColor, ExportKdOfAGrayscaleConditionIsTheTruncatedByte)
{
    // SET-07 alias plan: grayscale on, BGR fill 0x336699 (r 0x99, g 0x66, b 0x33): Kd 0.435294 (0x6f) for all three
    // channels, not 0.6 / 0.4 / 0.2; with grayscale off the colour is untouched.
    const DisplayColor gray = ost_domain::display_color(condition(0x336699, 1), "Solid", true);
    for (float channel : {gray.r, gray.g, gray.b})
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.6f", ost_geometry::export_text::colour_channel(channel));
        CHECK_STR_EQ(buf, "0.435294");
    }
    CHECK_EQ(gray.red, 0x6f);
    CHECK_STR_EQ(gray.hex(), "#6f6f6f");
    const DisplayColor plain = ost_domain::display_color(condition(0x336699, 1), "Solid", false);
    CHECK_EQ(plain.red, 0x99);
    CHECK_EQ(plain.green, 0x66);
    CHECK_EQ(plain.blue, 0x33);
    CHECK_STR_EQ(plain.hex(), "#996633");
    // the channel is the float of the byte, so a writer recovers the byte
    CHECK_EQ(plain.r, 153.0f / 255.0f);
}

TEST(DisplayColor, ColourAndOpacityEqualTheWindowsColourServiceForEveryModePatternAndFill)
{
    // get_color_mapping(display_mode, grayscale) + get_color_for_takeoff of a takeoff of one condition: hex and opacity of
    // 600 combinations (modes Solid / Original / Transparent, the old name 'OST', an unknown mode and '' ; patterns 0..9,
    // 99, -1; fills 0, 0x336699, white, -1 and a negative with alpha bits).
    const auto &rows = export_rule_oracle::color_rows();
    REQUIRE(rows.size() >= 600);
    for (const auto &row : rows)
    {
        SCOPED_CONTEXT(std::string(row.mode) + (row.grayscale ? " gray" : "") + " pattern " + std::to_string(row.pattern) +
                       " fill " + std::to_string(row.fill));
        const DisplayColor c = ost_domain::display_color(condition(row.fill, row.pattern), row.mode, row.grayscale);
        CHECK_STR_EQ(c.hex(), row.hex);
        CHECK_EQ(static_cast<double>(c.opacity), row.opacity);
    }
}

TEST(DisplayColor, OpacityOfTheDisplayModes)
{
    // EXP-06 plan: Original pattern 5 -> 0.5, pattern 0 or 1 -> 1.0; Transparent -> 0.5; Solid -> 1.0 (also: every code
    // 2..8, unknown codes and negative codes in Original -> 0.5; an unknown mode is like Solid).
    for (int pattern : {0, 1})
        CHECK_EQ(ost_domain::display_opacity("Original", pattern), 1.0);
    for (int pattern : {2, 3, 4, 5, 6, 7, 8, 9, 99, -1})
        CHECK_EQ(ost_domain::display_opacity("Original", pattern), 0.5);
    for (int pattern : {0, 1, 5, 99})
    {
        CHECK_EQ(ost_domain::display_opacity("Transparent", pattern), 0.5);
        CHECK_EQ(ost_domain::display_opacity("Solid", pattern), 1.0);
        CHECK_EQ(ost_domain::display_opacity("OST", pattern), 1.0);
        CHECK_EQ(ost_domain::display_opacity("original", pattern), 1.0); // the comparison is case-sensitive
    }
}

TEST(DisplayColor, RenderConfigOverloadUsesTheModeAndTheGrayscaleFlag)
{
    RenderConfig config;
    config.color_mode = RenderConfig::MODE_TRANSPARENT;
    config.grayscale_enabled = true;
    const DisplayColor c = ost_domain::display_color(condition(255, 1), config);
    CHECK_EQ(c.red, 76);
    CHECK_EQ(c.opacity, 0.5f);
}
