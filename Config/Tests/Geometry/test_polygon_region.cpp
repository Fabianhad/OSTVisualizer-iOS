// GEO-M02: the polygon region predicates behind the hole / attachment containment validation of a parent area edit.
// Windows oracle: plan_view/components/geometry_utils.py (closed_polygon_path, position_polygon_path, path_is_inside,
// path_intersects_any, path_fits_inside_excluding), which asks Qt's QPainterPath clipper. Values marked "Windows
// (PySide6)" were produced by running those functions of the Windows app (PySide6 6.10.1, read-only).
#include "HierarchyTestSupport.hpp"
#include "PolygonRegionOracleData.hpp"

#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace geo_test;
using ost_geom::closed_polygon;
using ost_geom::path_fits_inside_excluding;
using ost_geom::path_intersects_any;
using ost_geom::path_is_inside;

namespace
{
    Polygon rect_path(double left, double top, double right, double bottom)
    {
        return closed_polygon(square(left, top, right, bottom));
    }

    const double kNaN = std::numeric_limits<double>::quiet_NaN();
    const double kInf = std::numeric_limits<double>::infinity();

    std::string fmt_g(double v)
    {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%g", v);
        return buf;
    }
} // namespace

// ---- closed_polygon / position_polygon_path -----------------------------------------------------------------------

// Port of tests/presentation/components/plan_view/components/test_geometry_utils.py
//   ::PolygonPathHelperTests::test_closed_polygon_path_rejects_short_or_odd_coordinate_lists
//   ::PolygonPathHelperTests::test_position_polygon_path_rejects_invalid_positions_without_transforming
TEST(PolygonRegion, ClosedPolygonRejectsShortOrOddCoordinateLists)
{
    const std::vector<std::vector<double>> bad = {
        {},
        {0.0, 0.0},
        {0.0, 0.0, 1.0, 1.0},
        {0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 5.0},
    };
    for (size_t i = 0; i < bad.size(); ++i)
    {
        SCOPED_CONTEXT("case " + std::to_string(i));
        CHECK(closed_polygon(bad[i]).empty());
    }
    CHECK_FALSE(closed_polygon({0.0, 0.0, 1.0, 0.0, 1.0, 1.0}).empty());
}

// Port of ...::PolygonPathHelperTests::test_closed_polygon_path_keeps_point_order_and_closes_the_subpath
// (the vertices keep their order; the closing edge is implicit).
TEST(PolygonRegion, ClosedPolygonKeepsPointOrder)
{
    const Polygon p = closed_polygon({0.0, 0.0, 10.0, 0.0, 10.0, 4.0});
    REQUIRE_EQ(p.size(), size_t(3));
    CHECK_EQ(p[0][0], 0.0);
    CHECK_EQ(p[0][1], 0.0);
    CHECK_EQ(p[1][0], 10.0);
    CHECK_EQ(p[1][1], 0.0);
    CHECK_EQ(p[2][0], 10.0);
    CHECK_EQ(p[2][1], 4.0);
    const BBox box = bounding_rect(p);
    CHECK_EQ(box.x, 0.0);
    CHECK_EQ(box.y, 0.0);
    CHECK_EQ(box.w, 10.0);
    CHECK_EQ(box.h, 4.0);
}

// Windows crashes (Qt segfault) on non-finite coordinates, which cannot be reproduced and is unsafe: iOS treats such a
// polygon as the empty path (never inside, never intersecting) instead.
TEST(PolygonRegion, NonFiniteCoordinatesGiveTheEmptyPath)
{
    CHECK(closed_polygon({0.0, 0.0, kNaN, 0.0, 1.0, 1.0}).empty());
    CHECK(closed_polygon({0.0, 0.0, kInf, 0.0, 1.0, 1.0}).empty());
    CHECK(closed_polygon({0.0, 0.0, 1.0, 0.0, 1.0, -kInf}).empty());
    const Polygon parent = rect_path(0, 0, 100, 100);
    CHECK_FALSE(path_is_inside(closed_polygon({1.0, 1.0, kNaN, 2.0, 3.0, 3.0}), parent));
    CHECK_FALSE(path_is_inside(rect_path(1, 1, 2, 2), closed_polygon({0.0, 0.0, 100.0, 0.0, kInf, 5.0})));
    CHECK_FALSE(path_intersects_any(rect_path(1, 1, 2, 2), {closed_polygon({0.0, 0.0, kNaN, 0.0, 1.0, 1.0})}));
}

// ---- path_is_inside / path_intersects_any / path_fits_inside_excluding ---------------------------------------------

// Port of ...::PolygonPathHelperTests::test_path_is_inside_requires_two_non_empty_paths_and_full_containment
TEST(PolygonRegion, PathIsInsideRequiresTwoNonEmptyPathsAndFullContainment)
{
    const Polygon parent = rect_path(0, 0, 100, 100);
    CHECK(path_is_inside(rect_path(10, 10, 20, 20), parent));
    CHECK(path_is_inside(parent, parent));
    CHECK_FALSE(path_is_inside(rect_path(90, 90, 110, 110), parent));
    CHECK_FALSE(path_is_inside(rect_path(200, 200, 210, 210), parent));
    CHECK_FALSE(path_is_inside(Polygon(), parent));
    CHECK_FALSE(path_is_inside(rect_path(10, 10, 20, 20), Polygon()));
    CHECK_FALSE(path_is_inside(Polygon(), Polygon()));
}

// Port of ...::PolygonPathHelperTests::test_path_intersects_any_ignores_empty_paths_and_accepts_generators
TEST(PolygonRegion, PathIntersectsAnyIgnoresEmptyPaths)
{
    const Polygon candidate = rect_path(0, 0, 10, 10);
    const Polygon near_ = rect_path(5, 5, 15, 15);
    const Polygon far_ = rect_path(50, 50, 60, 60);
    CHECK(path_intersects_any(candidate, {far_, near_}));
    CHECK_EQ(path_intersects_any(candidate, {far_}), false);
    CHECK_EQ(path_intersects_any(candidate, {}), false);
    CHECK_EQ(path_intersects_any(candidate, {Polygon()}), false);
    CHECK_EQ(path_intersects_any(Polygon(), {near_}), false);
}

// Port of ...::PolygonPathHelperTests::test_path_fits_inside_excluding_needs_containment_and_no_exclusion_hit
TEST(PolygonRegion, PathFitsInsideExcludingNeedsContainmentAndNoExclusionHit)
{
    const Polygon parent = rect_path(0, 0, 100, 100);
    const Polygon candidate = rect_path(10, 10, 20, 20);
    const Polygon hole = rect_path(15, 15, 30, 30);
    const Polygon elsewhere = rect_path(60, 60, 70, 70);
    CHECK(path_fits_inside_excluding(candidate, parent, {}));
    CHECK(path_fits_inside_excluding(candidate, parent, {elsewhere}));
    CHECK_FALSE(path_fits_inside_excluding(candidate, parent, {elsewhere, hole}));
    CHECK_FALSE(path_fits_inside_excluding(rect_path(90, 90, 110, 110), parent, {}));
}

namespace
{
    struct NamedCase
    {
        const char *name;
        std::vector<double> candidate;
        std::vector<double> parent;
        bool inside;
        bool intersects;
    };

    // Windows (PySide6): path_is_inside(candidate, parent) and path_intersects_any(candidate, [parent]) (the same
    // answer both ways round) for shared edges and vertices, containment, notches and the repeated / closing vertex
    // forms that Qt accepts. Shared boundaries count as inside AND as intersecting; only a gap does not.
    const NamedCase kNamedCases[] = {
        {"identical polygon", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"rects sharing an edge from outside", {10.0, 0.0, 20.0, 0.0, 20.0, 10.0, 10.0, 10.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, false, true},
        {"rects touching at a corner", {10.0, 10.0, 20.0, 10.0, 20.0, 20.0, 10.0, 20.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, false, true},
        {"inner rect sharing one edge", {0.0, 0.0, 5.0, 0.0, 5.0, 5.0, 0.0, 5.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"inner rect flush with all edges", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"inner rect strictly inside", {2.0, 2.0, 5.0, 2.0, 5.0, 5.0, 2.0, 5.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"rect partially outside", {8.0, 8.0, 12.0, 8.0, 12.0, 12.0, 8.0, 12.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, false, true},
        {"rect far away", {20.0, 20.0, 30.0, 20.0, 30.0, 30.0, 20.0, 30.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, false, false},
        {"triangle in a square", {0.0, 0.0, 10.0, 0.0, 0.0, 10.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"square in a triangle", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, {0.0, 0.0, 10.0, 0.0, 0.0, 10.0}, false, true},
        {"triangle edge on the square edge from outside", {10.0, 0.0, 20.0, 0.0, 10.0, 10.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, false, true},
        {"triangle vertex on a square edge from outside", {10.0, 5.0, 20.0, 0.0, 20.0, 10.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, false, true},
        {"triangle vertex on a square corner from outside", {10.0, 10.0, 20.0, 10.0, 10.0, 20.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, false, true},
        {"triangle with a partial edge on a square edge", {2.0, 0.0, 8.0, 0.0, 5.0, 5.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"diamond touching all four edges from inside", {5.0, 0.0, 10.0, 5.0, 5.0, 10.0, 0.0, 5.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"diamond with its tips outside", {5.0, -1.0, 11.0, 5.0, 5.0, 11.0, -1.0, 5.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, false, true},
        {"triangle with a vertex on the corner", {5.0, 5.0, 10.0, 10.0, 5.0, 10.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"diamond and rect with overlapping bounding boxes but disjoint", {20.0, 0.0, 25.0, 5.0, 20.0, 10.0, 15.0, 5.0}, {10.0, 0.0, 16.0, 0.0, 16.0, 3.0, 10.0, 3.0}, false, false},
        {"candidate with a repeated vertex", {2.0, 2.0, 2.0, 2.0, 5.0, 2.0, 5.0, 5.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"candidate with a repeated vertex, outside", {20.0, 20.0, 20.0, 20.0, 25.0, 20.0, 25.0, 25.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, false, false},
        {"parent with a repeated vertex", {2.0, 2.0, 5.0, 2.0, 5.0, 5.0}, {0.0, 0.0, 0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"explicit closing vertex on the candidate", {2.0, 2.0, 5.0, 2.0, 5.0, 5.0, 2.0, 2.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"explicit closing vertex on the parent", {2.0, 2.0, 5.0, 2.0, 5.0, 5.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0, 0.0, 0.0}, true, true},
        {"collinear extra vertex on a parent edge", {2.0, 2.0, 5.0, 2.0, 5.0, 5.0}, {0.0, 0.0, 5.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"bow-tie candidate inside the square", {2.0, 2.0, 8.0, 8.0, 8.0, 2.0, 2.0, 8.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, true, true},
        {"bow-tie candidate partially outside", {2.0, 2.0, 12.0, 12.0, 12.0, 2.0, 2.0, 12.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, false, true},
        {"triangle inside the left lobe of a bow-tie parent", {1.0, 4.0, 2.0, 4.0, 2.0, 6.0}, {0.0, 0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 10.0}, true, true},
        {"triangle in the (even-odd) outside of a bow-tie parent", {4.0, 1.0, 6.0, 1.0, 5.0, 2.0}, {0.0, 0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 10.0}, false, false},
        {"rect in the notch of a U", {4.0, 0.0, 6.0, 0.0, 6.0, 8.0, 4.0, 8.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 6.0, 10.0, 6.0, 2.0, 4.0, 2.0, 4.0, 10.0, 0.0, 10.0}, false, true},
        {"rect bridging the notch of a U", {3.0, 5.0, 7.0, 5.0, 7.0, 8.0, 3.0, 8.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 6.0, 10.0, 6.0, 2.0, 4.0, 2.0, 4.0, 10.0, 0.0, 10.0}, false, true},
        {"triangle under the notch floor of a U", {4.0, 2.0, 6.0, 2.0, 5.0, 0.5}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 6.0, 10.0, 6.0, 2.0, 4.0, 2.0, 4.0, 10.0, 0.0, 10.0}, true, true},
        {"rect in the gap of a U", {4.5, 3.0, 5.5, 3.0, 5.5, 9.0, 4.5, 9.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 6.0, 10.0, 6.0, 2.0, 4.0, 2.0, 4.0, 10.0, 0.0, 10.0}, false, false},
        {"rect through a reflex vertex of an L", {1.0, 1.0, 9.0, 1.0, 9.0, 9.0, 1.0, 9.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 5.0, 5.0, 5.0, 5.0, 10.0, 0.0, 10.0}, false, true},
        {"rect inside the arm of an L, touching the reflex vertex", {1.0, 1.0, 5.0, 1.0, 5.0, 5.0, 1.0, 5.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 5.0, 5.0, 5.0, 5.0, 10.0, 0.0, 10.0}, true, true},
    };
} // namespace

TEST(PolygonRegion, SharedBoundariesAndConcaveParentsMatchTheWindowsClipper)
{
    for (const NamedCase &c : kNamedCases)
    {
        SCOPED_CONTEXT(std::string("case: ") + c.name);
        const Polygon candidate = closed_polygon(c.candidate);
        const Polygon parent = closed_polygon(c.parent);
        CHECK_EQ(path_is_inside(candidate, parent), c.inside);
        CHECK_EQ(path_intersects_any(candidate, {parent}), c.intersects);
        CHECK_EQ(path_intersects_any(parent, {candidate}), c.intersects);
    }
}

// Windows (PySide6): a sliver that sticks out of the parent by about 1e-9 of the shape size is rejected, one of about
// 1e-13 or less is below Qt's snapping and still "inside"; for "intersects" of axis-aligned shapes and of the triangles
// below there is no tolerance at all (a gap of 1e-14 is no intersection, a shared point is; Qt's extra ~1e-12 band for
// parallel NON axis-aligned edges is not reproduced, see polygon_region.hpp). Values from QPainterPath at several
// coordinate scales (identical results).
TEST(PolygonRegion, NearCoincidentGeometryFollowsTheWindowsTolerance)
{
    const Polygon parent = rect_path(0, 0, 100, 100);
    // Hole whose left edge lies `eps` outside the parent's left edge.
    for (double eps : {1e-1, 1e-3, 1e-6, 1e-9})
    {
        SCOPED_CONTEXT("eps=" + fmt_g(eps));
        CHECK_FALSE(path_is_inside(rect_path(-eps, 10, 20, 30), parent));
        CHECK_FALSE(path_is_inside(closed_polygon({10.0, 10.0, 30.0, 10.0, -eps, 40.0}), parent));
    }
    for (double eps : {1e-13, 1e-14, 1e-15, 0.0})
    {
        SCOPED_CONTEXT("eps=" + fmt_g(eps));
        CHECK(path_is_inside(rect_path(-eps, 10, 20, 30), parent));
        CHECK(path_is_inside(closed_polygon({10.0, 10.0, 30.0, 10.0, -eps, 40.0}), parent));
    }
    // A vertex on the slanted edge (100,0)-(60,100) of a trapezoid, displaced outward along its normal by `d`.
    const Polygon trapezoid = closed_polygon({0.0, 0.0, 100.0, 0.0, 60.0, 100.0, 0.0, 100.0});
    struct Slanted
    {
        std::vector<double> triangle;
        bool inside;
    };
    const Slanted slanted[] = {
        {{80.00092847669089, 50.00037139067636, 79.00092847669089, 49.800371390676354, 79.10092847669088, 50.800371390676354}, false},     // d = 1e-3
        {{80.0000009284767, 50.000000371390676, 79.0000009284767, 49.800000371390674, 79.10000092847669, 50.800000371390674}, false},     // d = 1e-6
        {{80.00000000092848, 50.000000000371394, 79.00000000092848, 49.80000000037139, 79.10000000092847, 50.80000000037139}, false},     // d = 1e-9
        {{80.0000000000001, 50.000000000000036, 79.0000000000001, 49.80000000000003, 79.1000000000001, 50.80000000000003}, true},         // d = 1e-13
        {{80.00000000000001, 50.00000000000001, 79.00000000000001, 49.800000000000004, 79.10000000000001, 50.800000000000004}, true},     // d = 1e-14
        {{80.0, 50.0, 79.0, 49.8, 79.1, 50.8}, true},                                                                                      // d = 0
    };
    for (size_t i = 0; i < sizeof(slanted) / sizeof(slanted[0]); ++i)
    {
        SCOPED_CONTEXT("slanted case " + std::to_string(i));
        CHECK_EQ(path_is_inside(closed_polygon(slanted[i].triangle), trapezoid), slanted[i].inside);
    }
    // No tolerance for intersections of these axis-aligned shapes and triangles.
    const Polygon a = rect_path(0, 0, 10, 10);
    CHECK(path_intersects_any(a, {rect_path(10, 0, 20, 10)}));
    CHECK_FALSE(path_intersects_any(a, {rect_path(10.000000000000002, 0, 20, 10)}));
    CHECK(path_intersects_any(a, {rect_path(9.999999999999998, 0, 20, 10)}));
    const Polygon t = closed_polygon({0.0, 0.0, 10.0, 0.0, 0.0, 10.0});
    CHECK(path_intersects_any(t, {closed_polygon({10.0, 0.0, 20.0, 0.0, 10.0, 10.0})}));
    CHECK_FALSE(path_intersects_any(t, {closed_polygon({10.000000000000002, 0.0, 20.0, 0.0, 10.000000000000002, 10.0})}));
    CHECK_FALSE(path_intersects_any(t, {closed_polygon({10.0 + 1e-9, 0.0, 20.0, 0.0, 10.0 + 1e-9, 10.0})}));
}

// Windows (PySide6, read-only run by the reviewer): Qt's snapping band is about 1e-12 of the CANDIDATE's size (its
// larger bounding-box side), not of the parent's or of the coordinates. Parent (0,0)-(100,100), candidate whose left edge
// lies `eps` outside the parent; (eps, inside) pairs were measured:
//   rect (-eps,10)-(20,30), size 20: 1e-12 yes, 5e-12 yes, 1e-11 yes, 2e-11 yes, 5e-11 no, 1e-10 no
//   rect (-eps,10)-(80,90), size 80: 2e-11 yes, 5e-11 yes, 1e-10 no, 1e-9 no
//   rect (-eps,10)-(11,11), size 11: 1e-12 yes, 5e-12 yes, 1e-11 yes, 2e-11 no, 5e-11 no
//   triangle (10,10),(30,10),(-eps,40), size 30: 1e-12 yes, 1e-11 yes, 2e-11 yes, 1e-10 no (between 2e-11 and 1e-10 the
//   exact edge of Qt's band for a pointed overshoot is not reproduced, so nothing is pinned there)
// Only values with a clear margin to the Core threshold (1e-12 x candidate size) are used: right at the threshold the
// result depends on the rounding noise of the compiler (fused multiply-add), which Qt's own band cannot be matched to.
TEST(PolygonRegion, SnappingBandScalesWithTheCandidateSize)
{
    const Polygon parent = rect_path(0, 0, 100, 100);
    struct Row
    {
        double eps;
        bool inside;
    };
    for (const Row &r : {Row{1e-12, true}, Row{5e-12, true}, Row{1e-11, true}, Row{5e-11, false}, Row{1e-10, false}})
    {
        SCOPED_CONTEXT("rect size 20, eps=" + fmt_g(r.eps));
        CHECK_EQ(path_is_inside(rect_path(-r.eps, 10, 20, 30), parent), r.inside);
    }
    for (const Row &r : {Row{1e-12, true}, Row{1e-11, true}, Row{2e-11, true}, Row{1e-10, false}})
    {
        SCOPED_CONTEXT("triangle size 30, eps=" + fmt_g(r.eps));
        CHECK_EQ(path_is_inside(closed_polygon({10.0, 10.0, 30.0, 10.0, -r.eps, 40.0}), parent), r.inside);
    }
    for (const Row &r : {Row{2e-11, true}, Row{5e-11, true}, Row{1e-9, false}})
    {
        SCOPED_CONTEXT("rect size 80, eps=" + fmt_g(r.eps));
        CHECK_EQ(path_is_inside(rect_path(-r.eps, 10, 80, 90), parent), r.inside);
    }
    for (const Row &r : {Row{1e-12, true}, Row{5e-12, true}, Row{5e-11, false}})
    {
        SCOPED_CONTEXT("rect size 11, eps=" + fmt_g(r.eps));
        CHECK_EQ(path_is_inside(rect_path(-r.eps, 10, 11, 11), parent), r.inside);
    }
}

// Windows (PySide6) answers for 880 random simple polygon pairs: integer lattices 0..6 (about half of the pairs touch
// somewhere), a quarter-unit lattice 0..40, generic 3-decimal coordinates, near-nested copies (one vertex moved, halved
// about a vertex, a vertex dropped) and translated neighbours. Line: candidate parent inside intersects.
TEST(PolygonRegion, RandomPolygonPairsMatchTheWindowsClipper)
{
    const size_t count = sizeof(kPolygonRegionOracle) / sizeof(kPolygonRegionOracle[0]);
    REQUIRE(count > 800);
    size_t inside_true = 0, inter_true = 0;
    for (size_t i = 0; i < count; ++i)
    {
        const std::vector<std::string> f = split_fields(kPolygonRegionOracle[i]);
        REQUIRE_EQ(f.size(), size_t(4));
        SCOPED_CONTEXT("oracle line " + std::to_string(i) + ": " + kPolygonRegionOracle[i]);
        const Polygon candidate = closed_polygon(parse_numbers(f[0]));
        const Polygon parent = closed_polygon(parse_numbers(f[1]));
        const bool inside = f[2] == "1";
        const bool intersects = f[3] == "1";
        inside_true += inside;
        inter_true += intersects;
        CHECK_EQ(path_is_inside(candidate, parent), inside);
        CHECK_EQ(path_intersects_any(candidate, {parent}), intersects);
    }
    CHECK(inside_true > 150);
    CHECK(inter_true > 500 && inter_true < count);
}
