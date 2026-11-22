// COL-01: Condition::get_rgb must read ColorFill like the Windows color_service.int_to_hex.
//   r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF on Python ints, so a negative ColorFill
//   (system colours / alpha bits) uses the low bytes of its two's complement: no "grey for negatives" rule exists.
#include "TestHarness.hpp"

#include "Domain/Condition.hpp"

namespace
{
    struct Rgb
    {
        float r, g, b;
    };

    Rgb rgb_of(int color_fill)
    {
        ost_domain::Condition c;
        c.color_fill = color_fill;
        Rgb out{-1.0f, -1.0f, -1.0f};
        c.get_rgb(out.r, out.g, out.b);
        return out;
    }

    void expect_bytes(int color_fill, int r, int g, int b)
    {
        SCOPED_CONTEXT("color_fill=" + std::to_string(color_fill));
        const Rgb got = rgb_of(color_fill);
        CHECK_EQ(got.r, r / 255.0f);
        CHECK_EQ(got.g, g / 255.0f);
        CHECK_EQ(got.b, b / 255.0f);
    }
}

TEST(ConditionColor, BgrIntegerLowByteIsRed)
{
    // Port of tests/presentation/visualization/services/test_color_service.py::test_hex_helpers_round_trip_bgr_integers
    // and ::test_condition_color_uses_bgr_fill_and_defaults_missing_fill_to_red
    expect_bytes(0x336699, 0x99, 0x66, 0x33);
    expect_bytes(0x0000FF, 255, 0, 0);
    expect_bytes(0xFF0000, 0, 0, 255);
    expect_bytes(255, 255, 0, 0);
    expect_bytes(0, 0, 0, 0);
    expect_bytes(16777215, 255, 255, 255);
    expect_bytes(0x123456, 0x56, 0x34, 0x12);
}

TEST(ConditionColor, NegativeColorFillMasksTheLowTwentyFourBits)
{
    // Windows int_to_hex(-1) = '#ffffff', int_to_hex(-16776961) = '#ff0000', int_to_hex(-2147483633) = '#0f0000'
    expect_bytes(-1, 255, 255, 255);
    expect_bytes(-16776961, 255, 0, 0);
    expect_bytes(-2147483633, 0x0f, 0, 0);
    expect_bytes(-16777216, 0, 0, 0);
    // alpha byte set (0xFF000000 as a signed int) keeps the rgb bytes
    expect_bytes(static_cast<int>(0xFF336699u), 0x99, 0x66, 0x33);
}

TEST(ConditionColor, NegativeColorFillIsNotGrey)
{
    const Rgb got = rgb_of(-1);
    CHECK_NE(got.r, 0.5f);
    CHECK_NE(got.g, 0.5f);
    CHECK_NE(got.b, 0.5f);
}

TEST(ConditionColor, TheStaticHelperUsedBySwatchesAgreesWithGetRgb)
{
    // The list and edit-dialog swatches (Obj-C) call Condition::rgb_from_color_fill instead of a private grey rule.
    for (int fill : {0, 255, 0x336699, 16777215, -1, -16776961, -2147483633, -16777216, static_cast<int>(0xFF336699u)})
    {
        SCOPED_CONTEXT("color_fill=" + std::to_string(fill));
        float r = -1.0f, g = -1.0f, b = -1.0f;
        ost_domain::Condition::rgb_from_color_fill(fill, r, g, b);
        const Rgb viaCondition = rgb_of(fill);
        CHECK_EQ(r, viaCondition.r);
        CHECK_EQ(g, viaCondition.g);
        CHECK_EQ(b, viaCondition.b);
    }
}
