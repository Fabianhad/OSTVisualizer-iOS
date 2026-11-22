// COL-04 (aliases RPV-23, SET-05, RPV-M05): the fill opacity of a takeoff in the 2D plan view (PlanView.mm /
// PlanView+Placement.mm: alpha = Solid ? 1.0 : 0.5 for every pattern, extracted to ost_domain::display_opacity_2d).
//
// Windows oracle: color_service.get_color_mapping (apply_pattern_alpha in the Original mode, _apply_transparent) with
// domain/entities/pattern.py get_2d_opacity: tests/domain/entities/test_pattern.py::PatternOpacityTests,
// tests/presentation/visualization/services/test_color_service.py::test_original_mode_takes_opacity_from_the_2d_pattern and
// ::test_transparent_mode_halves_opacity.
//
// Decision 2026-11-16 (option c): Windows gives the line patterns 2..7 a clear fill (0.0) and draws hatch lines; iOS has no hatch
// renderer, so those patterns keep the 0.5 approximation. The pattern 0 mirrors Windows (alpha 0.0, the outline is still drawn).
#include "TestHarness.hpp"

#include "Domain/DisplayColor.hpp"
#include "Domain/RenderConfig.hpp"
#include "Domain/Types.hpp"

#include <string>

using ost_domain::display_opacity_2d;
using ost_domain::RenderConfig;

// Windows SET-05 table for the Original mode, with the line patterns 2..7 at the decided 0.5 instead of Windows' 0.0:
// {0: 0.0, 1: 1.0, 2..7: 0.5, 8: 0.5, -1: 0.5, 99: 0.5}.
TEST(DisplayOpacity2D, OriginalModeFollowsThePattern)
{
    CHECK_EQ(display_opacity_2d(RenderConfig::MODE_ORIGINAL, 0), 0.0);
    CHECK_EQ(display_opacity_2d(RenderConfig::MODE_ORIGINAL, 1), 1.0);
    for (int pattern = 2; pattern <= 7; ++pattern)
    {
        SCOPED_CONTEXT("pattern " + std::to_string(pattern));
        CHECK_EQ(display_opacity_2d(RenderConfig::MODE_ORIGINAL, pattern), 0.5);
    }
    CHECK_EQ(display_opacity_2d(RenderConfig::MODE_ORIGINAL, 8), 0.5);
    CHECK_EQ(display_opacity_2d(RenderConfig::MODE_ORIGINAL, -1), 0.5);
    CHECK_EQ(display_opacity_2d(RenderConfig::MODE_ORIGINAL, 99), 0.5);
}

// Port of test_transparent_mode_halves_opacity (0.5 for every pattern) and the Solid mode (1.0 for every pattern, also the
// pattern 0: only the Original mode lets the pattern decide).
TEST(DisplayOpacity2D, TransparentAndSolidModesIgnoreThePattern)
{
    for (int pattern : {0, 1, 2, 7, 8, 99, -1})
    {
        SCOPED_CONTEXT("pattern " + std::to_string(pattern));
        CHECK_EQ(display_opacity_2d(RenderConfig::MODE_TRANSPARENT, pattern), 0.5);
        CHECK_EQ(display_opacity_2d(RenderConfig::MODE_SOLID, pattern), 1.0);
    }
}

// A mode outside the vocabulary (compared exactly, case-sensitive) is the Solid branch, like Windows' fall-through.
TEST(DisplayOpacity2D, UnknownModeIsOpaque)
{
    CHECK_EQ(display_opacity_2d("original", 0), 1.0);
    CHECK_EQ(display_opacity_2d("OST", 2), 1.0);
    CHECK_EQ(display_opacity_2d("", 0), 1.0);
}

// Port of test_original_mode_takes_opacity_from_the_2d_pattern: {c1 pattern 1: 1.0, c3 pattern 8: 0.5} (the c2 line pattern 2
// is 0.0 on Windows and 0.5 here by the decision above).
TEST(DisplayOpacity2D, PortOfOriginalModeTakesOpacityFromThe2DPattern)
{
    CHECK_EQ(display_opacity_2d(RenderConfig::MODE_ORIGINAL, static_cast<int>(ost_domain::Pattern::Solid)), 1.0);
    CHECK_EQ(display_opacity_2d(RenderConfig::MODE_ORIGINAL, static_cast<int>(ost_domain::Pattern::Horizontal)), 0.5);
    CHECK_EQ(display_opacity_2d(RenderConfig::MODE_ORIGINAL, static_cast<int>(ost_domain::Pattern::Transparent)), 0.5);
}
