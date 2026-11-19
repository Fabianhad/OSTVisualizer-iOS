// RPV-25 / RPV-26 / RPV-27 / RPV-M04 / GEO-18 (PlanView.mm _buildCountPath, _buildLinearPaths, the negative marker anchor and
// the hit areas, extracted to Core/Geometry/takeoff_footprint.hpp): the 2D footprint of a takeoff on the canvas.
//
// Windows oracle: tests/domain/entities/test_shape.py, tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py
// (CountGeometryTests, test_count_takeoff_is_centered_and_has_a_minimum_rendered_size,
// test_linear_thickness_scales_with_view_scale_and_has_a_minimum, test_area_with_hole_uses_anchor_inside_visible_fill) and, where
// no Windows test pins a value, the Windows code itself run read-only (TakeoffRenderer with the real OSTCoordinateSystem and the
// plan view's page QTransform): TakeoffFootprintOracleData.hpp, and the literals commented below.
#include "GeometryTestUtil.hpp"
#include "TakeoffFootprintOracleData.hpp"

#include "Geometry/page_transform.hpp"
#include "Geometry/takeoff_footprint.hpp"

#include <array>
#include <cmath>
#include <string>
#include <vector>

using geo_test::case_name;
using geo_test::rel_tol;
using geo_test::throws_invalid_argument;
using ost_domain::Condition;
using ost_domain::ConditionType;
using ost_domain::ShapeId;
using ost_domain::Takeoff;
using ost_types::Vec2;

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    struct Box
    {
        double left, top, right, bottom;
        double width() const { return right - left; }
        double height() const { return bottom - top; }
    };

    Box bounds_of(const std::vector<Vec2> &pts)
    {
        CHECK_FALSE(pts.empty());
        if (pts.empty())
            return {0.0, 0.0, 0.0, 0.0};
        Box b{pts[0][0], pts[0][1], pts[0][0], pts[0][1]};
        for (const Vec2 &p : pts)
        {
            b.left = std::min(b.left, p[0]);
            b.right = std::max(b.right, p[0]);
            b.top = std::min(b.top, p[1]);
            b.bottom = std::max(b.bottom, p[1]);
        }
        return b;
    }

    Condition count_condition(ShapeId shape, double width, double depth, double display_size = 100.0,
                              ConditionType type = ConditionType::Count)
    {
        Condition c;
        c.uid = "c1";
        c.condition_type = type;
        c.shape = shape;
        c.width = width;
        c.depth = depth;
        c.display_size = display_size;
        return c;
    }

    Condition linear_condition(double thickness)
    {
        Condition c;
        c.uid = "c1";
        c.condition_type = ConditionType::Linear;
        c.thickness = thickness;
        return c;
    }

    Condition area_condition()
    {
        Condition c;
        c.uid = "c1";
        c.condition_type = ConditionType::Area;
        return c;
    }

    Takeoff takeoff_at(std::vector<double> position, double rotation = 0.0, int curve = -1)
    {
        Takeoff t;
        t.uid = "t1";
        t.condition_uid = "c1";
        t.position = std::move(position);
        t.rotation = rotation;
        t.curve = curve;
        return t;
    }

    void check_points(const std::vector<Vec2> &actual, const std::vector<Vec2> &expected, double tol)
    {
        REQUIRE_EQ(actual.size(), expected.size());
        for (size_t i = 0; i < actual.size(); ++i)
        {
            SCOPED_CONTEXT("vertex " + std::to_string(i));
            CHECK_NEAR(actual[i][0], expected[i][0], tol);
            CHECK_NEAR(actual[i][1], expected[i][1], tol);
        }
    }

    // The Windows scene of a fake coordinate system with ost_to_pdf_points(v) = v and no page transform: scale_ratio 72
    // and view_scale 1 turn an OST value into the same number of scene px.
    std::vector<Vec2> unit_scene(const Takeoff &t, const Condition &c, double view_scale = 1.0)
    {
        return ost_footprint::scene_footprint(t, c, 72.0, view_scale);
    }

    // The inputs behind every row of TakeoffFootprintOracleData.hpp, by name.
    void oracle_inputs(const std::string &name, size_t occurrence, Takeoff &t, Condition &c)
    {
        const std::vector<double> p = {100.0, 200.0};
        if (name == "rectangle")
        {
            c = count_condition(ShapeId::Rectangle, 20.0, 40.0);
            t = takeoff_at(p);
        }
        else if (name == "triangle_rot0.3")
        {
            c = count_condition(ShapeId::Triangle, 20.0, 40.0);
            t = takeoff_at(p, 0.3);
        }
        else if (name == "hexagon")
        {
            c = count_condition(ShapeId::Hexagon, 20.0, 40.0);
            t = takeoff_at({50.0, 60.0});
        }
        else if (name == "octagon")
        {
            c = count_condition(ShapeId::Octagon, 20.0, 40.0);
            t = takeoff_at({50.0, 60.0}, 0.5);
        }
        else if (name == "rhombus")
        {
            c = count_condition(ShapeId::Rhombus, 20.0, 40.0);
            t = takeoff_at({50.0, 60.0});
        }
        else if (name == "ellipse_rot90")
        {
            c = count_condition(ShapeId::Ellipse, 20.0, 40.0);
            t = takeoff_at(p, kPi / 2);
        }
        else if (name == "circle")
        {
            c = count_condition(ShapeId::Circle, 20.0, 40.0);
            t = takeoff_at(p);
        }
        else if (name == "square_min")
        {
            c = count_condition(ShapeId::Square, 2.0, 2.0);
            t = takeoff_at(p);
        }
        else if (name == "rectangle_min")
        {
            c = count_condition(ShapeId::Rectangle, 2.0, 4.0);
            t = takeoff_at(p);
        }
        else if (name == "rectangle_display50")
        {
            c = count_condition(ShapeId::Rectangle, 20.0, 40.0, 50.0);
            t = takeoff_at(p);
        }
        else if (name == "attachment_display50")
        {
            c = count_condition(ShapeId::Rectangle, 20.0, 40.0, 50.0, ConditionType::Attachment);
            t = takeoff_at(p);
        }
        else if (name == "rectangle_width05")
        {
            c = count_condition(ShapeId::Rectangle, 0.5, 0.0);
            t = takeoff_at(p);
        }
        else if (name == "linear_straight")
        {
            c = linear_condition(8.0);
            t = takeoff_at({0.0, 0.0, 100.0, 0.0});
        }
        else if (name == "linear_straight_diag")
        {
            c = linear_condition(8.0);
            t = takeoff_at({10.0, 20.0, 110.0, 70.0});
        }
        else if (name == "linear_thin")
        {
            c = linear_condition(0.1);
            t = takeoff_at({0.0, 0.0, 100.0, 0.0});
        }
        else if (name == "curved6" || name == "curved6_rot270_flipy")
        {
            c = linear_condition(4.0);
            t = takeoff_at({0.0, 0.0, 100.0, 0.0, 50.0, 30.0}, 0.0, 0);
        }
        else if (name == "curved7")
        {
            c = linear_condition(4.0);
            t = takeoff_at({0.0, 0.0, 100.0, 0.0, 50.0, 30.0, 12.0}, 0.0, 0);
        }
        else if (name == "curved_quarter")
        {
            c = linear_condition(4.0);
            t = takeoff_at({0.0, 0.0, 100.0, 100.0, 100.0, 0.0}, 0.0, 0);
        }
        else if (name == "area")
        {
            c = area_condition();
            t = takeoff_at({10.0, 20.0, 110.0, 20.0, 110.0, 120.0, 10.0, 120.0, 60.0, 170.0});
        }
        else
            FAIL_TEST("unknown oracle row " + name + " #" + std::to_string(occurrence));
    }

    // The Qt-free odd-even point test the marker checks use (a strict interior point only).
    bool inside_even_odd(const std::vector<std::vector<Vec2>> &contours, double x, double y)
    {
        bool inside = false;
        for (const auto &poly : contours)
            for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
                if (((poly[i][1] > y) != (poly[j][1] > y)) &&
                    (x < (poly[j][0] - poly[i][0]) * (y - poly[i][1]) / (poly[j][1] - poly[i][1]) + poly[i][0]))
                    inside = !inside;
        return inside;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// RPV-25: shapes
// ---------------------------------------------------------------------------------------------------------------------

// Port of tests/domain/entities/test_shape.py (ShapePointTests, ShapeUnrotatedAndRotatedPointTests; assertAlmostEqual, 7 places)
// through compute_count_vertices (width 8 / depth 4 about (10, 20); the rhombus uses the width alone).
TEST(FootprintShapes, PortOfTestShapePoints)
{
    const double tol = 1e-7;
    using ost_footprint::count_vertices;
    const int rect = static_cast<int>(ShapeId::Rectangle);
    const int tri = static_cast<int>(ShapeId::Triangle);
    const int rhomb = static_cast<int>(ShapeId::Rhombus);
    // test_rectangle_and_triangle_rotation_keep_unequal_dimensions
    check_points(count_vertices(10, 20, rect, 8, 4, kPi / 2), {{12, 16}, {12, 24}, {8, 24}, {8, 16}}, tol);
    check_points(count_vertices(10, 20, tri, 8, 4, kPi / 2), {{12, 20}, {8, 24}, {8, 16}}, tol);
    // test_rhombus_keeps_legacy_width_derived_aspect_ratio (the depth is not used)
    check_points(count_vertices(10, 20, rhomb, 8, 99, 0.0), {{10, 16}, {12.3, 20}, {10, 24}, {7.7, 20}}, tol);
    // test_unrotated_rectangle_triangle_and_rhombus_are_axis_aligned_around_the_center
    check_points(count_vertices(10, 20, rect, 8, 4, 0.0), {{6, 18}, {14, 18}, {14, 22}, {6, 22}}, tol);
    check_points(count_vertices(10, 20, tri, 8, 4, 0.0), {{10, 18}, {14, 22}, {6, 22}}, tol);
    check_points(count_vertices(10, 20, rhomb, 8, 4, 0.0), {{10, 16}, {12.3, 20}, {10, 24}, {7.7, 20}}, tol);
    // test_half_turn_rotation_mirrors_every_corner_through_the_center
    check_points(count_vertices(10, 20, rect, 8, 4, kPi), {{14, 22}, {6, 22}, {6, 18}, {14, 18}}, tol);
    check_points(count_vertices(10, 20, tri, 8, 4, kPi), {{10, 22}, {6, 18}, {14, 18}}, tol);
    check_points(count_vertices(10, 20, rhomb, 8, 4, kPi), {{10, 24}, {7.7, 20}, {10, 16}, {12.3, 20}}, tol);
    // test_rhombus_quarter_turn_and_arbitrary_angle_use_the_rotation_matrix
    check_points(count_vertices(10, 20, rhomb, 8, 4, kPi / 2), {{14, 20}, {10, 22.3}, {6, 20}, {10, 17.7}}, tol);
    // a shape id that is not one of the codes is the rectangle (ShapeSpec 99: neither ellipse nor polygon)
    check_points(count_vertices(10, 20, 99, 8, 4, 0.0), {{6, 18}, {14, 18}, {14, 22}, {6, 22}}, tol);
}

// Windows values (compute_count_vertices run read-only), centre (0, 0), width 20, depth 40: the shapes the iOS overlay got wrong.
TEST(FootprintShapes, HexagonOctagonRhombusTriangleEllipseOfTheWindowsRenderer)
{
    const double tol = 1e-12;
    using ost_footprint::count_vertices;
    // hexagon: 6 points from angle 0 at radius w/2 on both axes (the depth is ignored); iOS started at (0, -20)
    check_points(count_vertices(0, 0, static_cast<int>(ShapeId::Hexagon), 20, 40, 0.0),
                 {{10.0, 0.0},
                  {5.000000000000001, 8.660254037844386},
                  {-4.999999999999998, 8.660254037844387},
                  {-10.0, 1.2246467991473533e-15},
                  {-5.000000000000004, -8.660254037844386},
                  {5.000000000000001, -8.660254037844386}},
                 tol);
    // octagon: 8 points from pi/8 at radius w/2
    const auto oct = count_vertices(0, 0, static_cast<int>(ShapeId::Octagon), 20, 40, 0.0);
    REQUIRE_EQ(oct.size(), static_cast<size_t>(8));
    check_points({oct[0], oct[1], oct[2]},
                 {{9.238795325112868, 3.826834323650898},
                  {3.8268343236508984, 9.238795325112868},
                  {-3.826834323650897, 9.238795325112868}},
                 tol);
    // rhombus from the width alone: (0, -10), (5.75, 0), (0, 10), (-5.75, 0); iOS used the depth vertically
    check_points(count_vertices(0, 0, static_cast<int>(ShapeId::Rhombus), 20, 40, 0.0),
                 {{0, -10.0}, {5.75, 0}, {0, 10.0}, {-5.75, 0}}, tol);
    // triangle: apex (0, -d/2), base (+-w/2, d/2)
    check_points(count_vertices(0, 0, static_cast<int>(ShapeId::Triangle), 20, 40, 0.0),
                 {{0, -20.0}, {10.0, 20.0}, {-10.0, 20.0}}, tol);
    // ellipse turned 90 degrees: a 32-gon with the radii (10, 20) that starts at (0, 10)
    const auto ell = count_vertices(0, 0, static_cast<int>(ShapeId::Ellipse), 20, 40, kPi / 2);
    REQUIRE_EQ(ell.size(), static_cast<size_t>(32));
    check_points({ell[0], ell[1], ell[2], ell[8], ell[31]},
                 {{6.123233995736766e-16, 10.0},
                  {-3.9018064403225643, 9.807852804032304},
                  {-7.653668647301795, 9.238795325112868},
                  {-20.0, 1.83697019872103e-15},
                  {3.901806440322575, 9.807852804032303}},
                 tol);
}

// ---------------------------------------------------------------------------------------------------------------------
// RPV-26: size rules
// ---------------------------------------------------------------------------------------------------------------------

// Windows resolve_point_takeoff_shape (run read-only): width floor 1.0, depth falls back to the width, square and circle as
// deep as wide, a COUNT scaled by max(display_size, 0.1) / 100 (no 100% fallback for a display size of 0), an attachment not.
TEST(FootprintSizeRules, ResolvePointTakeoffShape)
{
    struct Row
    {
        const char *name;
        Condition condition;
        int shape;
        double width, depth;
    };
    const int rect = static_cast<int>(ShapeId::Rectangle);
    const std::vector<Row> rows = {
        {"width 0.5, no depth: the 1.0 floor", count_condition(ShapeId::Rectangle, 0.5, 0.0), rect, 1.0, 1.0},
        {"depth falls back to the width", count_condition(ShapeId::Rectangle, 20.0, 0.0), rect, 20.0, 20.0},
        {"negative sizes", count_condition(ShapeId::Rectangle, -3.0, -2.0), rect, 1.0, 1.0},
        {"square: as deep as wide", count_condition(ShapeId::Square, 20.0, 99.0), 0, 20.0, 20.0},
        {"circle: as deep as wide", count_condition(ShapeId::Circle, 5.0, 99.0), 1, 5.0, 5.0},
        {"display size 50", count_condition(ShapeId::Ellipse, 20.0, 40.0, 50.0), 13, 10.0, 20.0},
        {"display size 0 is 0.1 percent", count_condition(ShapeId::Ellipse, 20.0, 40.0, 0.0), 13, 0.02, 0.04},
        {"negative display size", count_condition(ShapeId::Ellipse, 20.0, 40.0, -5.0), 13, 0.02, 0.04},
        {"display size below 0.1", count_condition(ShapeId::Ellipse, 20.0, 40.0, 0.05), 13, 0.02, 0.04},
        {"attachments are not scaled",
         count_condition(ShapeId::Rectangle, 20.0, 40.0, 50.0, ConditionType::Attachment), rect, 20.0, 40.0},
        {"no sizes, display size 200", count_condition(ShapeId::Rectangle, 0.0, 0.0, 200.0), rect, 2.0, 2.0},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(row.name);
        const auto got = ost_footprint::resolve_point_takeoff_shape(row.condition);
        CHECK_EQ(got.shape_id, row.shape);
        CHECK_NEAR(got.width, row.width, 1e-12);
        CHECK_NEAR(got.depth, row.depth, 1e-12);
    }
}

// Port of the item plan: apply_minimum_point_takeoff_size(2, 6, 8) = (8, 24), (10, 12, 8) = (10, 12): both sides grow by the
// same factor so that the smaller one is the minimum (iOS raised each side alone to 4 pt).
TEST(FootprintSizeRules, MinimumPointTakeoffSizeScalesBothSides)
{
    using ost_footprint::apply_minimum_point_takeoff_size;
    auto check = [](double w, double d, double ew, double ed)
    {
        const auto got = apply_minimum_point_takeoff_size(w, d, 8.0);
        CHECK_NEAR(got[0], ew, 1e-12);
        CHECK_NEAR(got[1], ed, 1e-12);
    };
    check(2.0, 6.0, 8.0, 24.0);
    check(10.0, 12.0, 10.0, 12.0);
    check(8.0, 8.0, 8.0, 8.0);
    check(30.0, 4.0, 60.0, 8.0);
    // Windows divides by zero for a side of 0: defined here as the minimum on both sides
    check(0.0, 6.0, 8.0, 8.0);
    check(-1.0, 5.0, 8.0, 8.0);
}

// Port of tests/.../test_takeoff_renderer.py::test_linear_thickness_scales_with_view_scale_and_has_a_minimum and the thickness
// rule of _create_linear_path: ost_to_pdf_points(thickness or 1.0) * view_scale, at least 2 px.
TEST(FootprintSizeRules, LinearThickness)
{
    using ost_footprint::rendered_linear_thickness;
    // fake coordinate system: ost_to_pdf_points(v) = v, i.e. scale_ratio 72
    CHECK_NEAR(rendered_linear_thickness(8.0, 72.0, 1.0), 8.0, 1e-12);
    CHECK_NEAR(rendered_linear_thickness(8.0, 72.0, 2.0), 16.0, 1e-12);
    CHECK_NEAR(rendered_linear_thickness(0.5, 72.0, 1.0), 2.0, 1e-12); // sub-pixel: raised to the minimum
    CHECK_NEAR(rendered_linear_thickness(0.0, 72.0, 3.0), 3.0, 1e-12); // no thickness: 1.0 OST
    CHECK_NEAR(rendered_linear_thickness(-3.0, 72.0, 1.0), 2.0, 1e-12); // Windows keeps the negative value, then the minimum
    // real scale 1:48 at view scale 3: 8 / 48 * 72 * 3 = 36 px
    CHECK_NEAR(rendered_linear_thickness(8.0, 48.0, 3.0), 36.0, 1e-12);
    // a raised minimum (the touch hit area)
    CHECK_NEAR(rendered_linear_thickness(0.1, 48.0, 3.0, 30.0), 30.0, 1e-12);
}

// Port of test_count_takeoff_is_centered_and_has_a_minimum_rendered_size (fake coordinate system, view scale 1): a 20 square
// is centred on its position; a 2 square becomes the 8 x 8 minimum.
TEST(FootprintSizeRules, PortOfCountTakeoffIsCenteredAndHasAMinimumRenderedSize)
{
    auto bounds = [](double size)
    {
        return bounds_of(unit_scene(takeoff_at({50.0, 60.0}), count_condition(ShapeId::Square, size, size)));
    };
    const Box big = bounds(20.0);
    CHECK_NEAR(big.left, 40.0, 1e-9);
    CHECK_NEAR(big.top, 50.0, 1e-9);
    CHECK_NEAR(big.width(), 20.0, 1e-9);
    CHECK_NEAR(big.height(), 20.0, 1e-9);
    const Box small = bounds(2.0);
    CHECK_NEAR(small.left, 46.0, 1e-9);
    CHECK_NEAR(small.top, 56.0, 1e-9);
    CHECK_NEAR(small.width(), 8.0, 1e-9);
    CHECK_NEAR(small.height(), 8.0, 1e-9);
}

// Port of CountGeometryTests (scene footprint, view scale): the size of the width and the depth scales alike; the marker turns
// about its position. (The Windows fake coordinate system does not scale the position, the real one does: the size doubles
// about the doubled centre.)
TEST(FootprintSizeRules, PortOfCountGeometryTests)
{
    const Condition rect = count_condition(ShapeId::Rectangle, 30.0, 10.0);
    const Takeoff at = takeoff_at({100.0, 50.0});
    const Box plain = bounds_of(unit_scene(at, rect));
    CHECK_NEAR(plain.left, 85.0, 1e-9);
    CHECK_NEAR(plain.top, 45.0, 1e-9);
    CHECK_NEAR(plain.width(), 30.0, 1e-9);
    CHECK_NEAR(plain.height(), 10.0, 1e-9);
    const Box zoomed = bounds_of(unit_scene(at, rect, 2.0));
    CHECK_NEAR(zoomed.width(), 60.0, 1e-9);
    CHECK_NEAR(zoomed.height(), 20.0, 1e-9);
    CHECK_NEAR((zoomed.left + zoomed.right) / 2.0, 200.0, 1e-9);
    CHECK_NEAR((zoomed.top + zoomed.bottom) / 2.0, 100.0, 1e-9);

    // test_the_marker_turns_about_its_position
    const Box turned = bounds_of(
        unit_scene(takeoff_at({100.0, 50.0}, kPi / 4.0), count_condition(ShapeId::Square, 20.0, 20.0)));
    const double half = 10.0 * std::sqrt(2.0);
    CHECK_NEAR(turned.left, 100.0 - half, 1e-6);
    CHECK_NEAR(turned.right, 100.0 + half, 1e-6);
    CHECK_NEAR(turned.top, 50.0 - half, 1e-6);
    CHECK_NEAR(turned.bottom, 50.0 + half, 1e-6);
}

// Port of test_linear_thickness_scales_with_view_scale_and_has_a_minimum on the scene footprint of a horizontal 20 long segment
// (the Windows fake does not scale the position): thickness 8 -> 8 px, view scale 2 -> 16 px, 0.5 -> the 2 px minimum.
TEST(FootprintSizeRules, PortOfLinearThicknessScalesWithViewScaleAndHasAMinimum)
{
    const Takeoff line = takeoff_at({0.0, 0.0, 20.0, 0.0});
    const Box plain = bounds_of(unit_scene(line, linear_condition(8.0)));
    CHECK_NEAR(plain.width(), 20.0, 1e-9);
    CHECK_NEAR(plain.height(), 8.0, 1e-9);
    const Box zoomed = bounds_of(unit_scene(line, linear_condition(8.0), 2.0));
    CHECK_NEAR(zoomed.width(), 40.0, 1e-9);
    CHECK_NEAR(zoomed.height(), 16.0, 1e-9);
    const Box thin = bounds_of(unit_scene(line, linear_condition(0.5)));
    CHECK_NEAR(thin.width(), 20.0, 1e-9);
    CHECK_NEAR(thin.height(), 2.0, 1e-9);
    // a segment shorter than 0.001 px has no band
    CHECK(unit_scene(takeoff_at({0.0, 0.0, 0.0005, 0.0}), linear_condition(8.0)).empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// GEO-18: curved linear
// ---------------------------------------------------------------------------------------------------------------------

// Windows compute_curved_linear_vertices (run read-only) for [0, 0, 10, 0, 5, 3], thickness 1: 24 segments, inner offset then
// the outer offset reversed: 2 * (24 + 1) = 50 vertices; the centreline peaks at the control point's side (y = 3).
TEST(FootprintCurved, CurvedBandOfTheWindowsRenderer)
{
    const auto verts = ost_footprint::curved_linear_vertices(0.0, 0.0, 10.0, 0.0, 5.0, 3.0, 1.0);
    REQUIRE_EQ(verts.size(), static_cast<size_t>(50));
    const double tol = 1e-12;
    check_points({verts[0], verts[12], verts[24], verts[25], verts[37], verts[49]},
                 {{-0.4301362814359027, 0.2549172010526038},
                  {5.0, 3.5},
                  {10.430136281435903, 0.2549172010526042},
                  {9.569863718564097, -0.2549172010526033},
                  {5.0, 2.5},
                  {0.4301362814359045, -0.254917201052602}},
                 tol);
}

// A control point on an end point is no curve: the 24 segment polyline of the chord (Windows run read-only: the band keeps its
// 2 * 25 vertices, from (0, 2) to (4.1666..., 2) on the inner side).
TEST(FootprintCurved, ControlOnAnEndIsTheStraightChord)
{
    const auto verts = ost_footprint::curved_linear_vertices(0.0, 0.0, 100.0, 0.0, 100.0, 0.0, 4.0);
    REQUIRE_EQ(verts.size(), static_cast<size_t>(50));
    CHECK_NEAR(verts[0][0], 0.0, 1e-12);
    CHECK_NEAR(verts[0][1], 2.0, 1e-12);
    CHECK_NEAR(verts[1][0], 4.166666666666666, 1e-12);
    CHECK_NEAR(verts[1][1], 2.0, 1e-12);
    CHECK_NEAR(verts[48][0], 4.166666666666666, 1e-12);
    CHECK_NEAR(verts[48][1], -2.0, 1e-12);
    CHECK_NEAR(verts[49][1], -2.0, 1e-12);
}

// linear_geom.proc_curved_pos (run read-only): the 7th value rebuilds the control from the chord midpoint and the offset
// (-position[6] along the normal); a zero-length chord keeps the stored control.
TEST(FootprintCurved, ProcCurvedPos)
{
    auto a = ost_footprint::proc_curved_pos({0.0, 0.0, 100.0, 0.0, 50.0, 30.0, 12.0});
    CHECK_NEAR(a[0], 0.0, 1e-12);
    CHECK_NEAR(a[2], 100.0, 1e-12);
    CHECK_NEAR(a[4], 50.0, 1e-12);
    CHECK_NEAR(a[5], -12.0, 1e-12);
    auto b = ost_footprint::proc_curved_pos({5.0, 5.0, 5.0, 5.0, 9.0, 9.0, 12.0});
    CHECK_NEAR(b[4], 9.0, 1e-12);
    CHECK_NEAR(b[5], 9.0, 1e-12);
    auto c = ost_footprint::proc_curved_pos({0.0, 0.0, 100.0, 0.0, 50.0, 30.0});
    CHECK_NEAR(c[4], 50.0, 1e-12);
    CHECK_NEAR(c[5], 30.0, 1e-12);
}

// A curved linear (curve >= 0 and 6 values) is drawn as the curved band, not as the chord p1-p2; a straight one (curve < 0, or
// fewer than 6 values) as the 4 vertex band.
TEST(FootprintCurved, CurvedOnlyWithCurveAndSixValues)
{
    const Condition cond = linear_condition(4.0);
    const std::vector<double> six = {0.0, 0.0, 100.0, 0.0, 50.0, 30.0};
    CHECK_EQ(ost_footprint::scene_footprint(takeoff_at(six, 0.0, 0), cond, 48.0, 3.0).size(), static_cast<size_t>(50));
    CHECK_EQ(ost_footprint::scene_footprint(takeoff_at(six, 0.0, -1), cond, 48.0, 3.0).size(), static_cast<size_t>(4));
    CHECK_EQ(ost_footprint::scene_footprint(takeoff_at({0.0, 0.0, 100.0, 0.0}, 0.0, 0), cond, 48.0, 3.0).size(),
             static_cast<size_t>(4));
    CHECK(ost_footprint::scene_footprint(takeoff_at({0.0, 0.0, 100.0}, 0.0, 0), cond, 48.0, 3.0).empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// RPV-M04 (+ GEO-06 / LP-16 display model): the footprint on the displayed page
// ---------------------------------------------------------------------------------------------------------------------

// Every row of the Windows oracle: the takeoff built in the unrotated scene and then turned / mirrored with the page.
TEST(FootprintOnCanvas, MatchesTheWindowsRendererAndPageTransform)
{
    std::vector<std::string> seen;
    size_t row_index = 0;
    for (const auto &row : footprint_oracle::rows())
    {
        size_t occurrence = 0;
        for (const std::string &s : seen)
            if (s == row.name)
                ++occurrence;
        seen.push_back(row.name);
        SCOPED_CONTEXT(std::string("row ") + std::to_string(row_index++) + " " + row.name + " rot " +
                       std::to_string(row.rotation) + (row.flip_x ? " fx" : "") + (row.flip_y ? " fy" : ""));
        Takeoff t;
        Condition c;
        oracle_inputs(row.name, occurrence, t, c);
        ost_footprint::PageFrame frame;
        frame.width_pts = 612.0;
        frame.height_pts = 792.0;
        frame.scale_factor1 = 1.0;
        frame.scale_factor2 = row.scale_factor2;
        frame.rotation = row.rotation;
        frame.flip_x = row.flip_x;
        frame.flip_y = row.flip_y;
        const auto got = ost_footprint::canvas_footprint(t, c, frame);
        REQUIRE_EQ(got.size(), row.outline.size());
        for (size_t i = 0; i < got.size(); ++i)
        {
            SCOPED_CONTEXT("vertex " + std::to_string(i));
            CHECK_NEAR(got[i][0], row.outline[i][0], rel_tol(got[i][0], row.outline[i][0]));
            CHECK_NEAR(got[i][1], row.outline[i][1], rel_tol(got[i][1], row.outline[i][1]));
        }
    }
}

// RPV-M04 item plan: a 20 x 40 rectangle is 20 wide and 40 tall on an unrotated page, 40 x 20 on a 90 / 270 page, and a
// triangle's apex follows the page rotation and the flips (iOS: always 20 x 40, apex up).
TEST(FootprintOnCanvas, CountShapeFollowsPageRotationAndFlips)
{
    ost_footprint::PageFrame frame;
    frame.width_pts = 612.0;
    frame.height_pts = 792.0;
    frame.scale_factor2 = 48.0;
    const Condition rect = count_condition(ShapeId::Rectangle, 20.0, 40.0);
    const Takeoff at = takeoff_at({100.0, 200.0});
    // ost 20 x 40 at 1:48 = 30 x 60 pt
    Box b = bounds_of(ost_footprint::canvas_footprint(at, rect, frame));
    CHECK_NEAR(b.width(), 30.0, 1e-9);
    CHECK_NEAR(b.height(), 60.0, 1e-9);
    for (int rotation : {90, 270, -90})
    {
        SCOPED_CONTEXT("rotation " + std::to_string(rotation));
        frame.rotation = rotation;
        b = bounds_of(ost_footprint::canvas_footprint(at, rect, frame));
        CHECK_NEAR(b.width(), 60.0, 1e-9);
        CHECK_NEAR(b.height(), 30.0, 1e-9);
    }
    // the apex of a triangle (vertex 0) is above the base on an upright page, below it at 180 degrees and with flip_y
    const Condition tri = count_condition(ShapeId::Triangle, 20.0, 40.0);
    frame.rotation = 0;
    auto t0 = ost_footprint::canvas_footprint(at, tri, frame);
    CHECK(t0[0][1] < t0[1][1]);
    frame.rotation = 180;
    auto t180 = ost_footprint::canvas_footprint(at, tri, frame);
    CHECK(t180[0][1] > t180[1][1]);
    frame.rotation = 0;
    frame.flip_y = true;
    auto tflip = ost_footprint::canvas_footprint(at, tri, frame);
    CHECK(tflip[0][1] > tflip[1][1]);
}

// An invalid page contract is an error, not a wrong picture (the caller skips the takeoff).
TEST(FootprintOnCanvas, InvalidPageContractThrows)
{
    ost_footprint::PageFrame frame;
    frame.width_pts = 612.0;
    frame.height_pts = 792.0;
    const Condition rect = count_condition(ShapeId::Rectangle, 20.0, 40.0);
    const Takeoff at = takeoff_at({100.0, 200.0});
    CHECK_FALSE(throws_invalid_argument([&] { ost_footprint::canvas_footprint(at, rect, frame); }));
    ost_footprint::PageFrame bad = frame;
    bad.rotation = 45;
    CHECK(throws_invalid_argument([&] { ost_footprint::canvas_footprint(at, rect, bad); }));
    bad = frame;
    bad.width_pts = 0.0;
    CHECK(throws_invalid_argument([&] { ost_footprint::canvas_footprint(at, rect, bad); }));
    bad = frame;
    bad.scale_factor1 = 0.0;
    CHECK(throws_invalid_argument([&] { ost_footprint::canvas_footprint(at, rect, bad); }));
    bad = frame;
    bad.scale_factor2 = std::nan("");
    CHECK(throws_invalid_argument([&] { ost_footprint::canvas_footprint(at, rect, bad); }));
}

// The footprint of a takeoff that Windows does not draw is empty: areas with fewer than 3 corners or an odd value count, a
// linear with fewer than 4 values, a count with fewer than 2, a condition of another type.
TEST(FootprintOnCanvas, NothingIsDrawnForAnIncompletePosition)
{
    ost_footprint::PageFrame frame;
    frame.width_pts = 612.0;
    frame.height_pts = 792.0;
    for (const std::vector<double> &position : std::vector<std::vector<double>>{
             {0.0, 0.0, 10.0, 0.0}, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 5.0}, {0.0, 0.0, 10.0}})
        CHECK(ost_footprint::canvas_footprint(takeoff_at(position), area_condition(), frame).empty());
    CHECK(ost_footprint::canvas_footprint(takeoff_at({1.0, 2.0, 3.0}), linear_condition(4.0), frame).empty());
    CHECK(ost_footprint::canvas_footprint(takeoff_at({1.0}), count_condition(ShapeId::Square, 4.0, 4.0), frame).empty());
    Condition other = count_condition(ShapeId::Square, 4.0, 4.0);
    other.condition_type = static_cast<ConditionType>(7);
    CHECK(ost_footprint::canvas_footprint(takeoff_at({1.0, 2.0}), other, frame).empty());
}

// The touch hit area raises the minimum thickness of a linear band (scene px): 0.1 thickness is 0.667 pt, a 30 px minimum 10 pt.
TEST(FootprintOnCanvas, LinearMinimumThicknessCanBeRaised)
{
    ost_footprint::PageFrame frame;
    frame.width_pts = 612.0;
    frame.height_pts = 792.0;
    frame.scale_factor2 = 48.0;
    const Condition thin = linear_condition(0.1);
    const Takeoff line = takeoff_at({0.0, 0.0, 100.0, 0.0});
    CHECK_NEAR(bounds_of(ost_footprint::canvas_footprint(line, thin, frame)).height(), 2.0 / 3.0, 1e-9);
    CHECK_NEAR(bounds_of(ost_footprint::canvas_footprint(line, thin, frame, 30.0)).height(), 10.0, 1e-9);
}

// ---------------------------------------------------------------------------------------------------------------------
// RPV-27: negative marker anchor
// ---------------------------------------------------------------------------------------------------------------------

// Windows TakeoffRenderer._path_centroid (run read-only on the QPainterPath of the polygon). The area-weighted centroid when it
// is inside the path; else the interior point nearest to it. Qt's contains() decides a centroid that lies exactly on an edge:
// the centroid of the item plan's L-shape, (1.5, 1.0), is on its horizontal edge y = 1 and Qt reports it outside, so Windows
// answers the nearest interior grid point (1.4375, 0.984375), not the centroid itself.
TEST(MarkerAnchor, MatchesPathCentroidOfTheWindowsRenderer)
{
    struct Row
    {
        const char *name;
        std::vector<Vec2> polygon;
        Vec2 expected;
    };
    const std::vector<Row> rows = {
        {"square", {{10, 10}, {70, 10}, {70, 70}, {10, 70}}, {40.0, 40.0}},
        {"L-shape (centroid on an edge)", {{0, 0}, {4, 0}, {4, 1}, {1, 1}, {1, 3}, {0, 3}}, {1.4375, 0.984375}},
        {"triangle", {{0, 0}, {60, 0}, {0, 60}}, {20.0, 20.0}},
        {"collinear: the mean of the points with the closing one", {{0, 0}, {10, 0}, {30, 0}}, {10.0, 0.0}},
        {"C-shape", {{0, 0}, {10, 0}, {10, 2}, {2, 2}, {2, 8}, {10, 8}, {10, 10}, {0, 10}}, {1.71875, 4.84375}},
        {"U-shape", {{0, 0}, {3, 0}, {3, 7}, {7, 7}, {7, 0}, {10, 0}, {10, 10}, {0, 10}}, {4.84375, 7.03125}},
        {"thin L", {{0, 0}, {20, 0}, {20, 2}, {2, 2}, {2, 20}, {0, 20}}, {5.9375, 1.5625}},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(row.name);
        const auto anchor = ost_footprint::negative_marker_anchor(row.polygon);
        REQUIRE(anchor.has_value());
        CHECK_NEAR((*anchor)[0], row.expected[0], 1e-12);
        CHECK_NEAR((*anchor)[1], row.expected[1], 1e-12);
    }
    // The path of a single point has one element: no marker. Two points are three path elements (closeSubpath repeats the
    // start): Windows answers the mean of those three points, (1/3, 1/3).
    CHECK_FALSE(ost_footprint::negative_marker_anchor({{0, 0}}).has_value());
    CHECK_FALSE(ost_footprint::negative_marker_anchor({}).has_value());
    const auto two = ost_footprint::negative_marker_anchor({{0, 0}, {1, 1}});
    REQUIRE(two.has_value());
    CHECK_NEAR((*two)[0], 1.0 / 3.0, 1e-12);
    CHECK_NEAR((*two)[1], 1.0 / 3.0, 1e-12);
}

// The centroid of a polygon: the item plan's L-shape (1.5, 1.0) against the vertex mean (1.667, 1.333) iOS used, and a
// zero area (1e-10) falls back to the mean of the points.
TEST(MarkerAnchor, PolygonAreaCentroid)
{
    const auto c = ost_footprint::polygon_area_centroid({{0, 0}, {4, 0}, {4, 1}, {1, 1}, {1, 3}, {0, 3}});
    CHECK_NEAR(c[0], 1.5, 1e-12);
    CHECK_NEAR(c[1], 1.0, 1e-12);
    const auto flat = ost_footprint::polygon_area_centroid({{0, 0}, {4, 0}, {2, 0}});
    CHECK_NEAR(flat[0], 2.0, 1e-12);
    CHECK_NEAR(flat[1], 0.0, 1e-12);
    // the same centroid for the clockwise polygon
    const auto cw = ost_footprint::polygon_area_centroid({{0, 3}, {1, 3}, {1, 1}, {4, 1}, {4, 0}, {0, 0}});
    CHECK_NEAR(cw[0], 1.5, 1e-12);
    CHECK_NEAR(cw[1], 1.0, 1e-12);
}

// Windows (run read-only): the marker of a triangle count shape sits at the polygon centroid, depth / 6 below the position
// point (here 30 px = 10 pt); the marker of a curved linear band is the Windows path centroid of the curved band, computed on
// the scene path (px) like Windows and then moved to the canvas (a centroid outside the band falls back on a nearest-point
// search whose ties are decided by the arithmetic of the coordinates it runs on).
TEST(MarkerAnchor, CountTriangleAndCurvedBand)
{
    ost_footprint::PageFrame frame;
    frame.width_pts = 612.0;
    frame.height_pts = 792.0;
    frame.scale_factor2 = 48.0;
    const auto a = ost_footprint::canvas_marker_anchor(takeoff_at({100.0, 200.0}),
                                                       count_condition(ShapeId::Triangle, 20.0, 40.0), {}, frame);
    REQUIRE(a.has_value());
    CHECK_NEAR((*a)[0], 150.0, 1e-9);
    CHECK_NEAR((*a)[1], 310.0, 1e-9);

    const auto b = ost_footprint::canvas_marker_anchor(takeoff_at({0.0, 0.0, 100.0, 0.0, 50.0, 30.0}, 0.0, 0),
                                                       linear_condition(4.0), {}, frame);
    REQUIRE(b.has_value());
    CHECK_NEAR((*b)[0], 72.57559944723074, 1e-9);
    CHECK_NEAR((*b)[1], 42.582710586809235, 1e-9);

    // on a page turned by 90 degrees the marker is the same point turned with the page: (x, y) -> (y, W - x)
    frame.rotation = 90;
    const auto c = ost_footprint::canvas_marker_anchor(takeoff_at({100.0, 200.0}),
                                                       count_condition(ShapeId::Triangle, 20.0, 40.0), {}, frame);
    REQUIRE(c.has_value());
    CHECK_NEAR((*c)[0], 310.0, 1e-9);
    CHECK_NEAR((*c)[1], 612.0 - 150.0, 1e-9);
    // no outline, no marker
    frame.rotation = 0;
    CHECK_FALSE(ost_footprint::canvas_marker_anchor(takeoff_at({1.0}), count_condition(ShapeId::Square, 4.0, 4.0), {}, frame)
                    .has_value());
    // an area with a hole: the anchor is inside the visible fill (outline minus hole), on the canvas
    const auto d = ost_footprint::canvas_marker_anchor(
        takeoff_at({0.0, 0.0, 96.0, 0.0, 96.0, 96.0, 0.0, 96.0}), area_condition(),
        {{24.0, 24.0, 72.0, 24.0, 72.0, 72.0, 24.0, 72.0}}, frame);
    REQUIRE(d.has_value());
    // 1:48 -> the outline is 144 x 144 pt, the hole 72 x 72 pt in the middle
    const bool in_outline = (*d)[0] > 0 && (*d)[0] < 144 && (*d)[1] > 0 && (*d)[1] < 144;
    const bool in_hole = (*d)[0] > 36 && (*d)[0] < 108 && (*d)[1] > 36 && (*d)[1] < 108;
    CHECK(in_outline);
    CHECK_FALSE(in_hole);
}

// Port of test_area_with_hole_uses_anchor_inside_visible_fill: the anchor of an area with a backout hole lies inside the
// visible fill (the outline minus the hole), also when the hole sits in the corner or in the middle. (Windows takes the path
// points of Qt's subtracted path as one polygon, which depends on Qt's clipper; the exact point is not portable, the contract
// is.)
TEST(MarkerAnchor, AnchorOfAnAreaWithAHoleIsInsideTheVisibleFill)
{
    const std::vector<Vec2> outer = {{0, 0}, {20, 0}, {20, 20}, {0, 20}};
    const std::vector<std::vector<Vec2>> holes_cases[] = {
        {{{0, 0}, {10, 0}, {10, 10}, {0, 10}}},   // the item's corner hole
        {{{5, 5}, {15, 5}, {15, 15}, {5, 15}}},   // a centred hole (the centroid is inside the hole)
        {{{2, 2}, {8, 2}, {8, 8}, {2, 8}}, {{12, 12}, {18, 12}, {18, 18}, {12, 18}}},
    };
    for (size_t i = 0; i < 3; ++i)
    {
        SCOPED_CONTEXT("holes case " + std::to_string(i));
        const auto &holes = holes_cases[i];
        const auto anchor = ost_footprint::negative_marker_anchor(outer, holes);
        REQUIRE(anchor.has_value());
        std::vector<std::vector<Vec2>> contours = {outer};
        for (const auto &h : holes)
            contours.push_back(h);
        CHECK(inside_even_odd(contours, (*anchor)[0], (*anchor)[1]));
    }
}
