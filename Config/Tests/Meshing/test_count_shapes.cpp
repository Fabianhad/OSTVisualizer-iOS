// Count / attachment shapes: ellipse (MSH-08), triangle (MSH-09), rhombus (MSH-10), hexagon + octagon (MSH-11) and the
// depth rule (MSH-13). Windows oracle: meshing/mesh_factory.py _create_count_mesh / _generate_count_mesh_from_spec,
// core/mesh_generator.py generate_elliptical_cylinder_mesh / generate_polygon_prism_mesh /
// generate_isosceles_triangle_prism_mesh / generate_rhombus_prism_mesh, core/geometry/radial.py,
// core/geometry/takeoff_geometry.py (resolve_point_takeoff_shape, point_takeoff_vertical_extent) and
// utils/mesh.py (face tables).
//
// Vertices: iOS vertex i is the Windows vertex i mapped by (X, Y, Z) -> (X, Z, -Y), so the shapes are compared index by
// index. Faces: the triangle and rhombus tables are the Windows tables; the Windows radial table winds inward and Windows
// d1c430a turns it outward with outward_faces (domain/mesh.py:62); iOS stores the outward faces directly, so each radial face
// (a, b, c) is stored as (a, c, b). Every new prism therefore has a positive signed volume.
#include "MeshingTestUtil.hpp"
#include "WindowsOracles.hpp"

#include <cmath>
#include <cstring>
#include <string>

using namespace mesh_test;

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    using Faces = std::vector<std::array<uint32_t, 3>>;

    const Faces &windows_faces(const std::string &table)
    {
        static const Faces none;
        if (table == "triangle")
            return ost_oracle::faces_triangle();
        if (table == "rhombus")
            return ost_oracle::faces_rhombus();
        if (table == "radial6")
            return ost_oracle::faces_radial6();
        if (table == "radial8")
            return ost_oracle::faces_radial8();
        if (table == "radial24")
            return ost_oracle::faces_radial24();
        return none;
    }

    Condition count_condition(ShapeId shape, double width, double depth, double height, double display_size,
                              ConditionType type = ConditionType::Count)
    {
        Condition c = make_condition("c", type);
        c.shape = shape;
        c.width = width;
        c.depth = depth;
        c.height = height;
        c.display_size = display_size;
        return c;
    }

    ost_meshing::MeshResult mesh_of(const Condition &c, double x, double y, double rotation = 0.0)
    {
        Takeoff t = make_takeoff("t", "c", "p", {x, y}, "0", rotation);
        return ost_meshing::create_mesh_for_takeoff(t, c, 1.0);
    }

    double extent(const Mesh &m, int axis)
    {
        Bounds b = bounds_of(m);
        return b.hi[axis] - b.lo[axis];
    }
} // namespace

// ---------------------------------------------------------------- all shapes against the Windows factory
TEST(CountShapes, MatchWindowsMeshFactoryOnTheOracleCases)
{
    // Each case: the same condition / takeoff through Windows MeshFactory (vertices, face count, signed volume) and
    // through create_mesh_for_takeoff. New prisms (ellipse, triangle, rhombus, hexagon, octagon) are compared index by
    // index; the box shapes and the circle (existing iOS vertex order) as vertex sets.
    for (const auto &c : ost_oracle::count_cases())
    {
        SCOPED_CONTEXT(c.name);
        Condition cond = count_condition(static_cast<ShapeId>(c.shape), c.width, c.depth, c.height, c.display_size,
                                         c.condition_type == 3 ? ConditionType::Attachment : ConditionType::Count);
        cond.z_value = c.z_value;
        cond.is_top = c.is_top;
        auto r = mesh_of(cond, c.x, c.y, c.rotation);
        REQUIRE(r.valid);
        CHECK_EQ(r.mesh.vertices.size(), static_cast<size_t>(c.vertex_count));
        CHECK_EQ(r.mesh.faces.size(), static_cast<size_t>(c.face_count));
        CHECK_NEAR(std::fabs(signed_volume(r.mesh)), std::fabs(c.signed_volume),
                   1e-9 * std::max(1.0, std::fabs(c.signed_volume)));
        const std::string table = c.table;
        if (table == "box")
        {
            CHECK(vertex_sets_equal(r.mesh.vertices, to_ios_all(c.vertices)));
        }
        else if (table == "radial24" && c.shape == 1)
        {
            CHECK(vertex_sets_equal(r.mesh.vertices, to_ios_all(c.vertices)));
        }
        else
        {
            check_vertices_index_wise(r.mesh, c.vertices);
            const Faces &wf = windows_faces(table);
            REQUIRE_EQ(r.mesh.faces.size(), wf.size());
            for (size_t i = 0; i < wf.size(); ++i)
            {
                SCOPED_CONTEXT("face " + std::to_string(i));
                std::array<uint32_t, 3> expect = wf[i];
                if (table.rfind("radial", 0) == 0) // Windows radial tables wind inward (outward since d1c430a via outward_faces); iOS stores them outward
                    std::swap(expect[1], expect[2]);
                CHECK(r.mesh.faces[i] == expect);
            }
            CHECK(closed_and_consistent(r.mesh));
            CHECK(signed_volume(r.mesh) > 0.0);
        }
    }
}

// ---------------------------------------------------------------- MSH-08 ellipse
TEST(Ellipse, UsesDepthAsTheSecondRadius)
{
    // MSH-08 plan: shape 13, width 20, depth 12, height 10, display 100, at [10,20]: 50 vertices, 96 faces,
    // |signed volume| 1863.4971 (circular cylinder of diameter 20 gave 3105.8285), X extent 20, Z extent 12.
    auto r = mesh_of(count_condition(ShapeId::Ellipse, 20.0, 12.0, 10.0, 100.0), 10.0, 20.0);
    REQUIRE(r.valid);
    CHECK_EQ(r.mesh.vertices.size(), static_cast<size_t>(50));
    CHECK_EQ(r.mesh.faces.size(), static_cast<size_t>(96));
    CHECK_NEAR(std::fabs(signed_volume(r.mesh)), 1863.4971, 5e-5);
    CHECK_NEAR(extent(r.mesh, 0), 20.0, 1e-9);
    CHECK_NEAR(extent(r.mesh, 2), 12.0, 1e-9);
    CHECK_NEAR(extent(r.mesh, 1), 10.0, 1e-12);
}

TEST(Ellipse, RotatedByHalfPiSwapsTheFootprint)
{
    // Port of tests/integration/rendering/test_threejs_mesh_footprint.py::
    //   test_rotated_ellipse_uses_render_minimum_only_for_two_d_threejs (3D mesh part): width 20, depth 2, height 10,
    // takeoff [10,20], rotation pi/2: footprint 2.0 along X and 20.0 along the other axis (Windows y, iOS z).
    auto r = mesh_of(count_condition(ShapeId::Ellipse, 20.0, 2.0, 10.0, 100.0), 10.0, 20.0, kPi / 2.0);
    REQUIRE(r.valid);
    CHECK_NEAR(extent(r.mesh, 0), 2.0, 1e-9);
    CHECK_NEAR(extent(r.mesh, 2), 20.0, 1e-9);
}

TEST(Ellipse, RotatedAndDisplayScaled)
{
    // MSH-08 plan: rotation 0.35, display_size 175: |volume| 9987.1799.
    auto r = mesh_of(count_condition(ShapeId::Ellipse, 20.0, 12.0, 10.0, 175.0), 10.0, 20.0, 0.35);
    REQUIRE(r.valid);
    CHECK_NEAR(std::fabs(signed_volume(r.mesh)), 9987.1799, 5e-4);
}

TEST(Ellipse, GeneratorMatchesTheWindowsRadialTable)
{
    // Port of tests/presentation/visualization/utils/test_mesh.py::PrismTopologyTableTests::
    //   test_radial_tables_close_cylinders_and_polygon_prisms (ellipse: rx = width / 2, rz = depth / 2).
    Mesh m = ost_meshing::generate_elliptical_cylinder_mesh(0, 0, 4, 6, 3, 0, 0, 8);
    REQUIRE_EQ(m.vertices.size(), static_cast<size_t>(2 * 8 + 2));
    REQUIRE_EQ(m.faces.size(), static_cast<size_t>(4 * 8));
    CHECK(closed_and_consistent(m));
    // ellipse area = n/2 * rx * rz * sin(2 pi / n)
    CHECK_NEAR(std::fabs(signed_volume(m)), 8.0 / 2.0 * 2.0 * 3.0 * std::sin(2.0 * kPi / 8.0) * 3.0, 1e-9);
}

// ---------------------------------------------------------------- MSH-09 triangle
TEST(TrianglePrism, VertexSetFromThePlan)
{
    // MSH-09 plan: shape 2, width 20, depth 12, height 10, rotation 0, takeoff [10,20], sr = 1:
    // bottom (X,Y,Z) (-20,0,-26), (-10,0,-14), (0,0,-26) and the same three at Y = 10; 6 vertices, 8 faces, |V| 1200.
    auto r = mesh_of(count_condition(ShapeId::Triangle, 20.0, 12.0, 10.0, 100.0), 10.0, 20.0);
    REQUIRE(r.valid);
    CHECK_EQ(r.mesh.vertices.size(), static_cast<size_t>(6));
    CHECK_EQ(r.mesh.faces.size(), static_cast<size_t>(8));
    CHECK_NEAR(std::fabs(signed_volume(r.mesh)), 1200.0, 1e-9);
    const std::vector<V3> expected{{-20, 0, -26}, {-10, 0, -14}, {0, 0, -26}, {-20, 10, -26}, {-10, 10, -14}, {0, 10, -26}};
    CHECK(vertex_sets_equal(r.mesh.vertices, expected, 1e-9));
}

TEST(TrianglePrism, GeneratorTableFromWindowsTest)
{
    // Port of ...::PrismTopologyTableTests::test_triangle_and_rhombus_tables_close_their_prisms (triangle part):
    // width 4, depth 6, height 3: 6 vertices, 8 faces, |volume| 36, every directed edge paired exactly once.
    Mesh m = ost_meshing::generate_isosceles_triangle_prism_mesh(0, 0, 4, 6, 3, 0);
    CHECK_EQ(m.vertices.size(), static_cast<size_t>(6));
    CHECK_EQ(m.faces.size(), static_cast<size_t>(8));
    CHECK(closed_and_consistent(m));
    CHECK_NEAR(std::fabs(signed_volume(m)), 36.0, 1e-9);
}

TEST(TrianglePrism, FlipTransformsEveryVertexNotOnlyTheAnchor)
{
    // Geometry half of tests/integration/rendering/test_page_flip_mesh.py::
    //   test_flip_transforms_every_point_mesh_vertex_not_only_its_anchor (triangle width 2, depth 3, height 1,
    //   rotation 0.35 at [2,1]); the page transform itself is tested in test_takeoff_meshes.cpp.
    auto r = mesh_of(count_condition(ShapeId::Triangle, 2.0, 3.0, 1.0, 100.0), 2.0, 1.0, 0.35);
    REQUIRE(r.valid);
    CHECK_EQ(r.mesh.vertices.size(), static_cast<size_t>(6));
    CHECK_NEAR(std::fabs(signed_volume(r.mesh)), 3.0, 1e-9); // 1/2 * 2 * 3 * 1
}

// ---------------------------------------------------------------- MSH-10 rhombus
TEST(RhombusPrism, VertexSetFromThePlan)
{
    // MSH-10 plan: shape 12, width 20, depth 12 (ignored), height 10, takeoff [10,20]: bottom (X,Y,Z)
    // (-15.75,0,-20), (-10,0,-30), (-10,0,-10), (-4.25,0,-20) plus the same four at Y = 10; 8 vertices, 12 faces,
    // |volume| 1150.
    auto r = mesh_of(count_condition(ShapeId::Rhombus, 20.0, 12.0, 10.0, 100.0), 10.0, 20.0);
    REQUIRE(r.valid);
    CHECK_EQ(r.mesh.vertices.size(), static_cast<size_t>(8));
    CHECK_EQ(r.mesh.faces.size(), static_cast<size_t>(12));
    CHECK_NEAR(std::fabs(signed_volume(r.mesh)), 1150.0, 1e-9);
    const std::vector<V3> expected{{-15.75, 0, -20}, {-10, 0, -30}, {-10, 0, -10}, {-4.25, 0, -20},
                                   {-15.75, 10, -20}, {-10, 10, -30}, {-10, 10, -10}, {-4.25, 10, -20}};
    CHECK(vertex_sets_equal(r.mesh.vertices, expected, 1e-9));
}

TEST(RhombusPrism, RotatedAndDisplayScaled)
{
    // MSH-10 plan: rotation 0.35, display_size 175: |volume| 6163.2812.
    auto r = mesh_of(count_condition(ShapeId::Rhombus, 20.0, 12.0, 10.0, 175.0), 10.0, 20.0, 0.35);
    REQUIRE(r.valid);
    CHECK_NEAR(std::fabs(signed_volume(r.mesh)), 6163.2812, 5e-4);
}

TEST(RhombusPrism, GeneratorTableFromWindowsTest)
{
    // Port of ...::test_triangle_and_rhombus_tables_close_their_prisms (rhombus part): width 4, height 3:
    // 8 vertices, 12 faces, closed; area = 2 * (hw * 0.575 * hw) = hw^2 * 1.15 with hw = 2.
    Mesh m = ost_meshing::generate_rhombus_prism_mesh(0, 0, 4, 3, 0);
    CHECK_EQ(m.vertices.size(), static_cast<size_t>(8));
    CHECK_EQ(m.faces.size(), static_cast<size_t>(12));
    CHECK(closed_and_consistent(m));
    CHECK_NEAR(std::fabs(signed_volume(m)), 2.0 * 2.0 * 0.575 * 2.0 * 3.0 / 1.0, 1e-9);
}

// ---------------------------------------------------------------- MSH-11 hexagon / octagon
TEST(PolygonPrism, HexagonFromThePlan)
{
    // MSH-11 plan: shape 14, width 20, height 10, rotation 0, takeoff [10,20]: 14 vertices, 24 faces, |volume|
    // 2598.0762, bottom ring (X,Z): (-20,-20), (-15,-28.660254), (-15,-11.339746), (-10,-20), (-5,-28.660254),
    // (-5,-11.339746), (0,-20) plus the two cap centres.
    auto r = mesh_of(count_condition(ShapeId::Hexagon, 20.0, 12.0, 10.0, 100.0), 10.0, 20.0);
    REQUIRE(r.valid);
    REQUIRE_EQ(r.mesh.vertices.size(), static_cast<size_t>(14));
    CHECK_EQ(r.mesh.faces.size(), static_cast<size_t>(24));
    CHECK_NEAR(std::fabs(signed_volume(r.mesh)), 2598.0762, 5e-5);
    // (the plan's list includes the cap centre (-10,-20), which is vertex 12, not part of the 6-point ring)
    const double ring[6][2] = {{-20, -20}, {-15, -28.660254}, {-15, -11.339746},
                               {-5, -28.660254}, {-5, -11.339746}, {0, -20}};
    for (const auto &p : ring)
    {
        bool found = false;
        for (size_t i = 0; i < 6; ++i) // bottom ring
            if (std::fabs(r.mesh.vertices[i][0] - p[0]) < 1e-6 && std::fabs(r.mesh.vertices[i][2] - p[1]) < 1e-6)
                found = true;
        CHECK(found);
    }
    CHECK_EQ(r.mesh.vertices[12][1], 0.0);  // bottom centre
    CHECK_NEAR(r.mesh.vertices[12][0], -10.0, 1e-12);
    CHECK_NEAR(r.mesh.vertices[12][2], -20.0, 1e-12);
    CHECK_EQ(r.mesh.vertices[13][1], 10.0); // top centre
}

TEST(PolygonPrism, OctagonFromThePlan)
{
    // MSH-11 plan: shape 15, same inputs: 18 vertices, 32 faces, |volume| 2828.4271.
    auto r = mesh_of(count_condition(ShapeId::Octagon, 20.0, 12.0, 10.0, 100.0), 10.0, 20.0);
    REQUIRE(r.valid);
    CHECK_EQ(r.mesh.vertices.size(), static_cast<size_t>(18));
    CHECK_EQ(r.mesh.faces.size(), static_cast<size_t>(32));
    CHECK_NEAR(std::fabs(signed_volume(r.mesh)), 2828.4271, 5e-5);
}

TEST(PolygonPrism, GeneratorTableFromWindowsTest)
{
    // Port of ...::test_radial_tables_close_cylinders_and_polygon_prisms (hexagon part):
    // generate_polygon_prism_mesh(0, 0, 4, 3, 6): 14 vertices, 24 faces, volume = 6/2 * 4 * sin(60 deg) * 3.
    Mesh m = ost_meshing::generate_polygon_prism_mesh(0, 0, 4, 3, 6, 0);
    CHECK_EQ(m.vertices.size(), static_cast<size_t>(14));
    CHECK_EQ(m.faces.size(), static_cast<size_t>(24));
    CHECK(closed_and_consistent(m));
    CHECK_NEAR(std::fabs(signed_volume(m)), 6.0 / 2.0 * 4.0 * std::sin(2.0 * kPi / 6.0) * 3.0, 1e-9);
}

// ---------------------------------------------------------------- MSH-13 depth
TEST(CountDepth, NegativeDepthBecomesOneAndZeroFallsBackToWidth)
{
    // MSH-13 plan: rectangle (shape 3), width 20, depth -3, height 10 at [10,20]: Windows depth 1.0 (Y range
    // 19.5..20.5 -> iOS Z -20.5..-19.5), volume 200. depth 0 uses the width (20).
    auto neg = mesh_of(count_condition(ShapeId::Rectangle, 20.0, -3.0, 10.0, 100.0), 10.0, 20.0);
    REQUIRE(neg.valid);
    Bounds b = bounds_of(neg.mesh);
    CHECK_NEAR(b.lo[2], -20.5, 1e-12);
    CHECK_NEAR(b.hi[2], -19.5, 1e-12);
    CHECK_NEAR(std::fabs(signed_volume(neg.mesh)), 200.0, 1e-9);
    auto zero = mesh_of(count_condition(ShapeId::Rectangle, 20.0, 0.0, 10.0, 100.0), 10.0, 20.0);
    REQUIRE(zero.valid);
    CHECK_NEAR(extent(zero.mesh, 2), 20.0, 1e-12);
    CHECK_NEAR(std::fabs(signed_volume(zero.mesh)), 4000.0, 1e-9);
}

TEST(CountDepth, SquareAndCircleIgnoreDepth)
{
    // Windows resolve_point_takeoff_shape: SQUARE and CIRCLE use depth = width.
    auto sq = mesh_of(count_condition(ShapeId::Square, 20.0, 12.0, 10.0, 100.0), 10.0, 20.0);
    REQUIRE(sq.valid);
    CHECK_NEAR(extent(sq.mesh, 0), 20.0, 1e-12);
    CHECK_NEAR(extent(sq.mesh, 2), 20.0, 1e-12);
    auto ci = mesh_of(count_condition(ShapeId::Circle, 20.0, 12.0, 10.0, 100.0), 10.0, 20.0);
    REQUIRE(ci.valid);
    CHECK_NEAR(extent(ci.mesh, 0), 20.0, 1e-9);
    CHECK_NEAR(extent(ci.mesh, 2), 20.0, 1e-9);
}

// ---------------------------------------------------------------- rotation has no dead zone (second review)
TEST(CountRotation, ATinyRotationIsStillApplied)
{
    // Windows calc_count_mesh_verts / calc_circ_pts rotate whenever `rotation != 0` (generate_count_mesh and
    // generate_cylinder_mesh pass -rotation): a takeoff rotation of 1e-9 rad moves a 36 wide box corner by 1.8e-8.
    // iOS ignored |rotation| <= 1e-9. Expected: the Windows MeshFactory meshes (Windows frame) of a 36 x 2 x 10 rectangle
    // and a 20 x 10 circle at [10, 20], rotation 1e-9, as vertex sets (the iOS vertex order of the box and the circle
    // differs from Windows). The values differ from the unrotated mesh by up to 1.8e-8, the tolerance is 1e-11.
    auto box = mesh_of(count_condition(ShapeId::Rectangle, 36.0, 2.0, 10.0, 100.0), 10.0, 20.0, 1e-9);
    REQUIRE(box.valid);
    const std::vector<V3> windows_box = {
        {-28.000000001, 19.000000018, 0.0},
        {7.999999999, 18.999999982, 0.0},
        {8.000000001, 20.999999982, 0.0},
        {-27.999999999, 21.000000018, 0.0},
        {-28.000000001, 19.000000018, 10.0},
        {7.999999999, 18.999999982, 10.0},
        {8.000000001, 20.999999982, 10.0},
        {-27.999999999, 21.000000018, 10.0}};
    CHECK(vertex_sets_equal(box.mesh.vertices, to_ios_all(windows_box), 1e-11));

    auto circle = mesh_of(count_condition(ShapeId::Circle, 20.0, 0.0, 10.0, 100.0), 10.0, 20.0, 1e-9);
    REQUIRE(circle.valid);
    const std::vector<V3> windows_circle = {
        {0.0, 19.99999999, 0.0},
        {-0.3407417345211261, 22.58819044136595, 0.0},
        {-1.3397459571556123, 24.999999991339745, 0.0},
        {-2.9289321810634563, 27.071067804794406, 0.0},
        {-4.999999991339745, 28.660254032844385, 0.0},
        {-7.411809539315534, 29.659258260302494, 0.0},
        {-9.99999999, 30.0, 0.0},
        {-12.58819044136595, 29.659258265478876, 0.0},
        {-14.999999991339745, 28.660254042844386, 0.0},
        {-17.071067804794406, 27.071067818936545, 0.0},
        {-18.660254032844385, 25.00000000866025, 0.0},
        {-19.65925826030249, 22.58819046068447, 0.0},
        {-20.0, 20.00000001, 0.0},
        {-19.659258265478876, 17.41180955863405, 0.0},
        {-18.66025404284439, 15.000000008660257, 0.0},
        {-17.071067818936548, 12.928932195205597, 0.0},
        {-15.000000008660258, 11.339745967155615, 0.0},
        {-12.588190460684466, 10.340741739697508, 0.0},
        {-10.000000010000003, 10.0, 0.0},
        {-7.411809558634055, 10.340741734521124, 0.0},
        {-5.000000008660253, 11.339745957155614, 0.0},
        {-2.9289321952055944, 12.928932181063455, 0.0},
        {-1.3397459671556167, 14.999999991339742, 0.0},
        {-0.34074173969750987, 17.411809539315527, 0.0},
        {0.0, 19.99999999, 10.0},
        {-0.3407417345211261, 22.58819044136595, 10.0},
        {-1.3397459571556123, 24.999999991339745, 10.0},
        {-2.9289321810634563, 27.071067804794406, 10.0},
        {-4.999999991339745, 28.660254032844385, 10.0},
        {-7.411809539315534, 29.659258260302494, 10.0},
        {-9.99999999, 30.0, 10.0},
        {-12.58819044136595, 29.659258265478876, 10.0},
        {-14.999999991339745, 28.660254042844386, 10.0},
        {-17.071067804794406, 27.071067818936545, 10.0},
        {-18.660254032844385, 25.00000000866025, 10.0},
        {-19.65925826030249, 22.58819046068447, 10.0},
        {-20.0, 20.00000001, 10.0},
        {-19.659258265478876, 17.41180955863405, 10.0},
        {-18.66025404284439, 15.000000008660257, 10.0},
        {-17.071067818936548, 12.928932195205597, 10.0},
        {-15.000000008660258, 11.339745967155615, 10.0},
        {-12.588190460684466, 10.340741739697508, 10.0},
        {-10.000000010000003, 10.0, 10.0},
        {-7.411809558634055, 10.340741734521124, 10.0},
        {-5.000000008660253, 11.339745957155614, 10.0},
        {-2.9289321952055944, 12.928932181063455, 10.0},
        {-1.3397459671556167, 14.999999991339742, 10.0},
        {-0.34074173969750987, 17.411809539315527, 10.0},
        {-10.0, 20.0, 0.0},
        {-10.0, 20.0, 10.0}};
    CHECK(vertex_sets_equal(circle.mesh.vertices, to_ios_all(windows_circle), 1e-11));
}
