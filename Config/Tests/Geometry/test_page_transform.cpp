// GEO-06: Core/Geometry/page_transform.hpp is the port of the Windows cpp_extensions/src/common/page_transform.hpp.
// The Windows tests reach it only through ost_coord_transform.ost_to_pdf_coordinates (test_coord_transform.cpp) and the
// PDF writer; the expectations below are the same page model (flip first, then rotate counter-clockwise, top-left
// origin) and agree with the Windows pyd rows of the 600x800 table (e.g. top-left (720, 240) <-> Y-up (720, 360)).
#include "GeometryTestUtil.hpp"

#include "Geometry/page_transform.hpp"

#include <limits>
#include <string>

using geo_test::throws_invalid_argument;
namespace pt = ost_page_transform;

TEST(PageTransform, NormalizeQuarterTurnReducesModuloAFullTurn)
{
    const int cases[][2] = {{0, 0}, {90, 90}, {180, 180}, {270, 270}, {360, 0}, {450, 90}, {720, 0},
                            {-90, 270}, {-180, 180}, {-270, 90}, {-360, 0}, {-450, 270}};
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT("rotation " + std::to_string(c[0]));
        CHECK_EQ(pt::normalize_quarter_turn(c[0]), c[1]);
    }
}

TEST(PageTransform, NormalizeQuarterTurnRejectsOtherAngles)
{
    for (int rotation : {45, 1, -45, 91, 135, 359, -1})
    {
        SCOPED_CONTEXT("rotation " + std::to_string(rotation));
        std::string what;
        CHECK(throws_invalid_argument([&] { pt::normalize_quarter_turn(rotation); }, &what));
        CHECK_STR_EQ(what, "Page rotation must be a multiple of 90 degrees");
    }
}

TEST(PageTransform, OutputDimensionsSwapForQuarterTurns)
{
    const int rotations[] = {0, 90, 180, 270, -90, 450};
    const bool swapped[] = {false, true, false, true, true, true};
    for (int i = 0; i < 6; ++i)
    {
        SCOPED_CONTEXT("rotation " + std::to_string(rotations[i]));
        auto d = pt::output_dimensions(600.0, 800.0, rotations[i]);
        CHECK_EQ(d[0], swapped[i] ? 800.0 : 600.0);
        CHECK_EQ(d[1], swapped[i] ? 600.0 : 800.0);
    }
}

TEST(PageTransform, ValidationOrderAndMessages)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    std::string what;
    for (double bad : {0.0, -1.0, nan, inf})
    {
        SCOPED_CONTEXT("dimension " + std::to_string(bad));
        CHECK(throws_invalid_argument([&] { pt::output_dimensions(bad, 800.0, 0); }, &what));
        CHECK_STR_EQ(what, "Page dimensions must be finite and positive");
        CHECK(throws_invalid_argument([&] { pt::output_dimensions(600.0, bad, 0); }, &what));
        CHECK_STR_EQ(what, "Page dimensions must be finite and positive");
    }
    // dimensions are checked before the rotation
    CHECK(throws_invalid_argument([&] { pt::output_dimensions(0.0, 800.0, 45); }, &what));
    CHECK_STR_EQ(what, "Page dimensions must be finite and positive");
    CHECK(throws_invalid_argument([&] { pt::output_dimensions(600.0, 800.0, 45); }, &what));
    CHECK_STR_EQ(what, "Page rotation must be a multiple of 90 degrees");
    // a non-finite point is reported before the page and the rotation
    CHECK(throws_invalid_argument([&] { pt::transform_top_left_point(0.0, 800.0, 45, false, false, nan, 1.0); }, &what));
    CHECK_STR_EQ(what, "Page coordinates must be finite");
    CHECK(throws_invalid_argument([&] { pt::transform_top_left_point(600.0, 800.0, 0, false, false, 1.0, inf); }, &what));
    CHECK_STR_EQ(what, "Page coordinates must be finite");
}

// The unrotated top-left point (360, 720) of a 600x800 page (OST (10, 20) at sf 1:2).
TEST(PageTransform, TopLeftPointFlipsFirstThenRotatesCounterClockwise)
{
    struct Case
    {
        int rot;
        bool fx, fy;
        double ex, ey;
    };
    const Case cases[] = {
        {0, false, false, 360.0, 720.0},   {0, false, true, 360.0, 80.0},    {0, true, false, 240.0, 720.0},
        {0, true, true, 240.0, 80.0},      {90, false, false, 720.0, 240.0}, {90, false, true, 80.0, 240.0},
        {90, true, false, 720.0, 360.0},   {90, true, true, 80.0, 360.0},    {180, false, false, 240.0, 80.0},
        {180, false, true, 240.0, 720.0},  {180, true, false, 360.0, 80.0},  {180, true, true, 360.0, 720.0},
        {270, false, false, 80.0, 360.0},  {270, false, true, 720.0, 360.0}, {270, true, false, 80.0, 240.0},
        {270, true, true, 720.0, 240.0},   {-90, false, false, 80.0, 360.0}, {450, false, false, 720.0, 240.0},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(geo_test::case_name(c.rot, c.fx, c.fy));
        auto p = pt::transform_top_left_point(600.0, 800.0, c.rot, c.fx, c.fy, 360.0, 720.0);
        CHECK_EQ(p[0], c.ex);
        CHECK_EQ(p[1], c.ey);
    }
}

TEST(PageTransform, DimensionsMatchUsesARelativeToleranceOfOneNanoUnit)
{
    CHECK(pt::dimensions_match(600.0, 800.0, 600.0, 800.0));
    CHECK(pt::dimensions_match(600.0, 800.0, 600.0000001, 800.0)); // 1e-7 <= 1e-9 * 600
    CHECK_FALSE(pt::dimensions_match(600.0, 800.0, 600.001, 800.0));
    CHECK_FALSE(pt::dimensions_match(600.0, 800.0, 600.0, 800.001));
    CHECK(pt::dimensions_match(0.0, 0.0, 5e-10, 0.0)); // the tolerance is at least 1e-9
    CHECK_FALSE(pt::dimensions_match(0.0, 0.0, 2e-9, 0.0));
}
