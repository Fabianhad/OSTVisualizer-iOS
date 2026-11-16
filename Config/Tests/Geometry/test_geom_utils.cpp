// GEO-07 (alias RPV-11): ost_geom::signed_area is translation-invariant (relative to the first vertex).
// GEO-09 (alias RPV-14): ost_geom::polygon_is_valid rejects degenerate and self-touching polygons.
// Windows oracle: cpp_extensions/src/geom_utils/geom_utils.cpp. Values marked "Windows pyd" were produced by running
// ost_geom_utils.signed_area / polygon_is_valid of the Windows app (read-only).
#include "GeometryTestUtil.hpp"

#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>
#include <vector>

using ost_types::Vec2;

namespace
{
    std::vector<Vec2> poly(std::initializer_list<std::pair<double, double>> pts)
    {
        std::vector<Vec2> v;
        for (const auto &p : pts)
            v.push_back({p.first, p.second});
        return v;
    }

    std::vector<Vec2> reversed(const std::vector<Vec2> &pts)
    {
        return std::vector<Vec2>(pts.rbegin(), pts.rend());
    }

    std::vector<double> flat(const std::vector<Vec2> &pts, double offset = 0.0)
    {
        std::vector<double> out;
        for (const Vec2 &p : pts)
        {
            out.push_back(p[0] + offset);
            out.push_back(p[1] + offset);
        }
        return out;
    }

    const double kNaN = std::numeric_limits<double>::quiet_NaN();
    const double kInf = std::numeric_limits<double>::infinity();
} // namespace

// ---- signed_area (GEO-07) --------------------------------------------------------------------------------------

// Port of tests/presentation/components/plan_view/components/test_geometry_utils.py
//   ::TakeoffLifecyclePrecisionTests::test_native_winding_and_concave_rotation_pivot_preserve_translation
// (winding / reversed winding parts): the concave polygon translated by 1e8 returns exactly 12 and -12.
TEST(SignedArea, ConcavePolygonAtLargeOffsetPreservesTranslation)
{
    const std::vector<Vec2> pts = poly({{0, 0}, {4, 0}, {4, 1}, {1, 1}, {1, 3}, {0, 3}});
    const double offset = 1e8;
    CHECK_EQ(ost_geom::signed_area(flat(pts, offset)), 12.0);
    CHECK_EQ(ost_geom::signed_area(flat(reversed(pts), offset)), -12.0);
    // untranslated: same value
    CHECK_EQ(ost_geom::signed_area(flat(pts)), 12.0);
    CHECK_EQ(ost_geom::signed_area(flat(reversed(pts))), -12.0);
}

// GEO-07 test plan (Windows pyd): the unit square at 1e8 returns 2.0 (the un-centred sum returned 0.0).
TEST(SignedArea, UnitSquareAtLargeOffset)
{
    CHECK_EQ(ost_geom::signed_area({1e8, 1e8, 1e8 + 1, 1e8, 1e8 + 1, 1e8 + 1, 1e8, 1e8 + 1}), 2.0);
    // Windows pyd: a 4 x 4 triangle at 1e15
    CHECK_EQ(ost_geom::signed_area({1e15, 1e15, 1e15 + 4, 1e15, 1e15, 1e15 + 4}), 16.0);
}

// Windows pyd: the value is TWICE the area (shoelace sum), counter-clockwise positive.
TEST(SignedArea, IsTwiceTheAreaAndSignedByWinding)
{
    CHECK_EQ(ost_geom::signed_area({0, 0, 4, 0, 4, 4, 0, 4}), 32.0);
    CHECK_EQ(ost_geom::signed_area({0, 0, 0, 4, 4, 4, 4, 0}), -32.0);
}

// Windows pyd: n = size / 2; fewer than two points -> 0.0; two points -> 0.0; an incomplete trailing coordinate is
// ignored ([0,0,4,0,4,4,0] is the triangle (0,0),(4,0),(4,4): 16).
TEST(SignedArea, ShortAndOddLengthInputs)
{
    CHECK_EQ(ost_geom::signed_area({}), 0.0);
    CHECK_EQ(ost_geom::signed_area({1.0, 2.0}), 0.0);
    CHECK_EQ(ost_geom::signed_area({0.0, 0.0, 1.0, 1.0}), 0.0);
    CHECK_EQ(ost_geom::signed_area({0.0, 0.0, 4.0, 0.0, 4.0, 4.0, 0.0}), 16.0);
    CHECK_EQ(ost_geom::signed_area({5.0}), 0.0);
}

// Windows pyd: signed_area([0.1, 0.2, 3.3, 0.4, 2.2, 5.5, -1.1, 4.4]) = 31.719999999999995 (relative to vertex 0);
// non-finite coordinates propagate (Windows returns nan for [0,0,nan,1,1,1]).
TEST(SignedArea, NonIntegralAndNonFiniteInputs)
{
    const double v = ost_geom::signed_area({0.1, 0.2, 3.3, 0.4, 2.2, 5.5, -1.1, 4.4});
    CHECK_NEAR(v, 31.719999999999995, 1e-9);
    CHECK(std::isnan(ost_geom::signed_area({0.0, 0.0, kNaN, 1.0, 1.0, 1.0})));
}

// ---- polygon_is_valid (GEO-09) ---------------------------------------------------------------------------------

// Port of tests/integration/native/test_geometry_boundaries.py
//   ::NativeGeometryBoundaryTests::test_polygon_rejects_degenerate_and_self_touching_boundaries
// (Windows tests the forward order; the Windows pyd also returns false reversed: RPV-14 plan).
TEST(PolygonIsValid, RejectsDegenerateAndSelfTouchingBoundaries)
{
    const std::vector<std::vector<Vec2>> invalid = {
        poly({{0, 0}, {1, 0}, {2, 0}}),                  // collinear three points (zero area)
        poly({{0, 0}, {2, 0}, {2, 0}, {0, 2}}),          // a vertex equal to the next one
        poly({{0, 0}, {4, 0}, {4, 4}, {2, 0}, {0, 4}}),  // a vertex lying on a non-adjacent edge
        poly({{0, 0}, {4, 0}, {2, 0}, {2, 4}, {0, 4}}),  // zero-turn spike / self-touch
        poly({{0, 0}, {kNaN, 0}, {0, 2}}),               // NaN vertex
        poly({{0, 0}, {kInf, 0}, {0, 2}}),               // Inf vertex
    };
    int i = 0;
    for (const auto &pts : invalid)
    {
        SCOPED_CONTEXT("case " + std::to_string(i++));
        CHECK_FALSE(ost_geom::polygon_is_valid(pts));
        CHECK_FALSE(ost_geom::polygon_is_valid(reversed(pts)));
    }
}

// Port of ...::NativeGeometryBoundaryTests::test_polygon_preserves_small_slivers_concavity_and_collinear_edge_vertices
// (both orientations, as the Windows test).
TEST(PolygonIsValid, PreservesSmallSliversConcavityAndCollinearEdgeVertices)
{
    const std::vector<std::vector<Vec2>> valid = {
        poly({{0, 0}, {0.001, 0}, {0, 0.001}}),
        poly({{10000, 10000}, {10001, 10000}, {10001, 10000.000001}}),
        poly({{0, 0}, {2, 0}, {4, 0}, {4, 4}, {0, 4}}),
        poly({{0, 0}, {4, 0}, {4, 1}, {1, 1}, {1, 4}, {0, 4}}),
    };
    int i = 0;
    for (const auto &pts : valid)
    {
        SCOPED_CONTEXT("case " + std::to_string(i++));
        CHECK(ost_geom::polygon_is_valid(pts));
        CHECK(ost_geom::polygon_is_valid(reversed(pts)));
    }
}

// Further inputs run on the Windows pyd (all return false forward and reversed): fewer than three points, a closing
// duplicate, a proper crossing (bow-tie), zero-turn 180 degree spikes, four collinear points, non-finite and
// overflowing coordinates, vertices touching non-adjacent edges or vertices.
TEST(PolygonIsValid, OtherDegenerateInputsAreRejected)
{
    const std::vector<std::vector<Vec2>> invalid = {
        poly({}),
        poly({{0, 0}}),
        poly({{0, 0}, {1, 1}}),
        poly({{0, 0}, {4, 0}, {4, 4}, {0, 0}}),                           // first vertex repeated at the end
        poly({{0, 0}, {4, 4}, {4, 0}, {0, 4}}),                           // bow-tie: proper crossing
        poly({{0, 0}, {4, 0}, {8, 0}, {4, 0}, {4, 4}}),                   // out-and-back spike
        poly({{0, 0}, {4, 0}, {2, 0}, {4, 4}}),                           // 180 degree turn
        poly({{0, 0}, {1, 0}, {2, 0}, {3, 0}}),                           // four collinear points
        poly({{0, 0}, {1, 0}, {-kInf, 2}}),                               // -Inf
        poly({{0, 0}, {1, kNaN}, {0, 2}}),                                // NaN y
        poly({{0, 0}, {1e200, 0}, {1e200, 1e200}, {0, 1e200}}),           // area overflows to Inf
        poly({{0, 0}, {6, 0}, {6, 6}, {3, 0}, {3, -3}, {0, 6}}),          // vertex on a non-adjacent edge
        poly({{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}, {2, 2}}),           // repeated vertex
        poly({{0, 0}, {4, 0}, {4, 4}, {0, 0}, {0, 4}}),                   // same vertex twice (non-adjacent)
    };
    int i = 0;
    for (const auto &pts : invalid)
    {
        SCOPED_CONTEXT("case " + std::to_string(i++));
        CHECK_FALSE(ost_geom::polygon_is_valid(pts));
        CHECK_FALSE(ost_geom::polygon_is_valid(reversed(pts)));
    }
}

// More valid shapes run on the Windows pyd (true forward and reversed): triangle, square with negative coordinates,
// concave arrow, huge square (1e15), a translated concave polygon, convex pentagon, U shape.
TEST(PolygonIsValid, OtherValidShapesAreAccepted)
{
    const std::vector<std::vector<Vec2>> valid = {
        poly({{0, 0}, {1, 0}, {0, 1}}),
        poly({{0, 0}, {4, 0}, {4, 4}, {0, 4}}),
        poly({{-5, -5}, {5, -5}, {5, 5}, {-5, 5}}),
        poly({{0, 0}, {4, 2}, {0, 4}, {1, 2}}),
        poly({{0, 0}, {1e15, 0}, {1e15, 1e15}, {0, 1e15}}),
        poly({{1e8, 1e8}, {1e8 + 4, 1e8}, {1e8 + 4, 1e8 + 1}, {1e8 + 1, 1e8 + 1}, {1e8 + 1, 1e8 + 4}, {1e8, 1e8 + 4}}),
        poly({{0, 0}, {2, -1}, {4, 0}, {3, 3}, {1, 3}}),
        poly({{0, 0}, {6, 0}, {6, 5}, {4, 5}, {4, 2}, {2, 2}, {2, 5}, {0, 5}}),
    };
    int i = 0;
    for (const auto &pts : valid)
    {
        SCOPED_CONTEXT("case " + std::to_string(i++));
        CHECK(ost_geom::polygon_is_valid(pts));
        CHECK(ost_geom::polygon_is_valid(reversed(pts)));
    }
}
