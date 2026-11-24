// Set Scale keeps the sheet position of every takeoff (plan 5.4, "one more, not counted"): a change of scale multiplies the stored real-world coordinates by
// new ratio / old ratio, so the point of the sheet a coordinate lands on does not move; only the quantities change.
// Windows: tests/integration/geometry/test_page_scale_projection.py:19-41 test_authoritative_rescale_preserves_display_geometry_without_hybrid_jump
// ((0.125, 12) -> (0.25, 12), [960, 480, 1920, 960] -> [480, 240, 960, 480], identical display; the stale hybrid, the OLD positions on the NEW scale, differs).
// iOS twin of the Windows OSTCoordinateSystem: ost_coord::ost_to_canvas_point (Core/Geometry/coord_transform.hpp).
#include "TestHarness.hpp"

#include "Domain/PageScaleTransform.hpp"
#include "Geometry/coord_transform.hpp"

#include <optional>
#include <string>
#include <vector>

using namespace ost_domain;
using std::optional;
using ost_types::Vec2;
using std::vector;

namespace
{
    // The displayed points of a position (pairs of values) on a 42 x 30 inch sheet (3024 x 2160 points).
    vector<Vec2> display(const vector<double> &position, double sf1, double sf2, int rotation, bool flip_x, bool flip_y)
    {
        vector<Vec2> points;
        for (size_t i = 0; i + 1 < position.size(); i += 2)
            points.push_back(ost_coord::ost_to_canvas_point(position[i], position[i + 1], 3024.0, 2160.0, sf1, sf2, rotation, flip_x, flip_y));
        return points;
    }
}

// tests/integration/geometry/test_page_scale_projection.py:19: the rescaled position is exactly [480, 240, 960, 480], its display equals the display before the
// change, and the stale hybrid (the old position on the new scale) displays somewhere else.
TEST(SetScale_Projection, AuthoritativeRescalePreservesDisplayGeometryWithoutHybridJump)
{
    const PageScale source{0.125, 12.0};
    const PageScale target{0.25, 12.0};
    const vector<double> source_position = {960.0, 480.0, 1920.0, 960.0};
    const vector<double> target_position = rescale_position_between_page_scales(source_position, source, target);
    CHECK_EQ(target_position, vector<double>({480.0, 240.0, 960.0, 480.0}));

    const vector<Vec2> source_display = display(source_position, source.sf1, source.sf2, 0, false, false);
    const vector<Vec2> target_display = display(target_position, target.sf1, target.sf2, 0, false, false);
    const vector<Vec2> stale_hybrid = display(source_position, target.sf1, target.sf2, 0, false, false);
    REQUIRE_EQ(source_display.size(), static_cast<size_t>(2));
    for (size_t i = 0; i < source_display.size(); ++i)
    {
        SCOPED_CONTEXT("point " + std::to_string(i));
        CHECK_EQ(target_display[i][0], source_display[i][0]);
        CHECK_EQ(target_display[i][1], source_display[i][1]);
        CHECK(stale_hybrid[i][0] != source_display[i][0] || stale_hybrid[i][1] != source_display[i][1]);
    }
}

// The same holds on every rotation and flip of the page (the sheet position is what the user sees): 8 combinations, several scale changes, within 1e-9 points.
TEST(SetScale_Projection, TheSheetPositionIsKeptOnEveryRotationAndFlip)
{
    const struct
    {
        double from_sf1, from_sf2, to_sf1, to_sf2;
    } changes[] = {{0.125, 12.0, 0.25, 12.0}, {0.1875, 12.0, 0.125, 12.0}, {1.0, 48.0, 1.0, 96.0}, {0.25, 12.0, 0.5, 24.0 * 1.5}};
    const vector<double> position = {123.5, 456.25, 1000.0, 2000.5, 3.0, 4.0};
    for (const auto &change : changes)
        for (const int rotation : {0, 90, 180, 270})
            for (const bool flip : {false, true})
            {
                SCOPED_CONTEXT("rotation " + std::to_string(rotation) + " flip " + std::to_string(flip));
                const optional<PageScale> from = PageScale{change.from_sf1, change.from_sf2};
                const optional<PageScale> to = PageScale{change.to_sf1, change.to_sf2};
                const vector<double> rescaled = rescale_position_between_page_scales(position, from, to);
                const vector<Vec2> before = display(position, change.from_sf1, change.from_sf2, rotation, flip, !flip);
                const vector<Vec2> after = display(rescaled, change.to_sf1, change.to_sf2, rotation, flip, !flip);
                REQUIRE_EQ(before.size(), after.size());
                for (size_t i = 0; i < before.size(); ++i)
                {
                    CHECK_NEAR(after[i][0], before[i][0], 1e-9);
                    CHECK_NEAR(after[i][1], before[i][1], 1e-9);
                }
            }
}
