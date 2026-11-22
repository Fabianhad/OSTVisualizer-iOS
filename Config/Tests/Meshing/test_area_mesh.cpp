// Area meshes: slope direction (GEO-05) and tessellation frame (GEO-14).
// Windows oracle: presentation/visualization/core/geometry/area.py (calc_area_mesh_verts, calc_area_faces) and the
// native earcut. Windows builds area meshes in the 3D frame (X = -x_ost, Y = y_ost, Z up); the iOS generator takes the
// iOS frame (x, z) = (X, -Y). Every test maps the Windows inputs with z = -Y, so the expectations read as Windows.
#include "MeshingTestUtil.hpp"
#include "WindowsOracles.hpp"

#include <cmath>
#include <string>

using namespace mesh_test;

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kHalfPi = kPi / 2.0;
    using Ring = std::vector<std::pair<double, double>>;

    Ring ios_ring(const std::vector<ost_oracle::V2> &w)
    {
        Ring r;
        for (const auto &p : w)
            r.emplace_back(p[0], -p[1]);
        return r;
    }

    std::vector<Ring> ios_holes(const std::vector<std::vector<ost_oracle::V2>> &w)
    {
        std::vector<Ring> h;
        for (const auto &r : w)
            h.push_back(ios_ring(r));
        return h;
    }

    // Top-ring heights of the outer outline (vertices n .. 2n-1).
    std::vector<double> top_heights(const Mesh &m, size_t n)
    {
        std::vector<double> t;
        for (size_t i = 0; i < n; ++i)
            t.push_back(m.vertices[n + i][1]);
        return t;
    }
} // namespace

// ---------------------------------------------------------------- GEO-05
TEST(AreaSlope, PortOfWindowsSlopeDirectionUsesRotationNotRiseRunSigns)
{
    // Port of tests/presentation/visualization/core/geometry/test_area.py::
    //   AreaSlopeBehaviorTests::test_area_mesh_slope_direction_uses_rotation_not_rise_run_signs
    // Windows vertices (0,0),(10,0),(10,10),(0,10) -> iOS (0,0),(10,0),(10,-10),(0,-10).
    const Ring sq{{0, 0}, {10, 0}, {10, -10}, {0, -10}};
    auto top = [&](double rise, double run, double rotation)
    {
        Mesh m = ost_meshing::generate_area_mesh(sq, 1.0, 0.0, {}, rise, run, rotation);
        REQUIRE_EQ(m.vertices.size(), static_cast<size_t>(8));
        return top_heights(m, 4);
    };
    auto rounded = [](std::vector<double> v)
    {
        for (auto &x : v)
            x = std::round(x * 1e9) / 1e9;
        return v;
    };
    const std::vector<double> expected{1.0, 11.0, 11.0, 1.0};
    CHECK_EQ(top(1, 1, 0), expected);
    // rise = -1 gives the same tops: only the rotation picks the direction.
    CHECK_EQ(top(-1, 1, 0), expected);
    CHECK_EQ(top(1, -1, 0), expected);
    CHECK_EQ(top(-1, -1, 0), expected);
    CHECK_EQ(rounded(top(1, 1, kHalfPi)), (std::vector<double>{11.0, 11.0, 1.0, 1.0}));
    CHECK_EQ(rounded(top(1, 1, kPi)), (std::vector<double>{11.0, 1.0, 1.0, 11.0}));
    // the top ring keeps the outline's (x, z)
    Mesh m = ost_meshing::generate_area_mesh(sq, 1.0, 0.0, {}, 1, 1, kPi);
    for (size_t i = 0; i < 4; ++i)
    {
        CHECK_EQ(m.vertices[4 + i][0], sq[i].first);
        CHECK_EQ(m.vertices[4 + i][2], sq[i].second);
    }
}

TEST(AreaSlope, MatchesWindowsTopHeightsOnTheOracleTable)
{
    // Windows calc_area_mesh_verts tops: both rise/run signs, rotations in all quadrants, z offset, a pentagon with a
    // hole (the hole ring gets the same plane), rise = 0 (flat).
    for (const auto &c : ost_oracle::slope_cases())
    {
        SCOPED_CONTEXT(c.name);
        Ring outer = ios_ring(c.verts);
        auto holes = ios_holes(c.holes);
        Mesh m = ost_meshing::generate_area_mesh(outer, c.thickness, c.z_offset, holes, c.rise, c.run, c.rotation);
        const size_t n = outer.size();
        REQUIRE(m.vertices.size() >= 2 * n);
        REQUIRE_EQ(c.top.size(), n);
        for (size_t i = 0; i < n; ++i)
        {
            SCOPED_CONTEXT("top " + std::to_string(i));
            CHECK_NEAR(m.vertices[n + i][1], c.top[i], rel_tol(m.vertices[n + i][1], c.top[i]));
            CHECK_EQ(m.vertices[i][1], c.z_offset); // bottom stays flat
        }
        size_t base = 2 * n;
        for (size_t h = 0; h < holes.size(); ++h)
        {
            const size_t hn = holes[h].size();
            REQUIRE_EQ(c.top_holes[h].size(), hn);
            for (size_t i = 0; i < hn; ++i)
            {
                SCOPED_CONTEXT("hole " + std::to_string(h) + " top " + std::to_string(i));
                double got = m.vertices[base + hn + i][1];
                CHECK_NEAR(got, c.top_holes[h][i], rel_tol(got, c.top_holes[h][i]));
            }
            base += 2 * hn;
        }
    }
}

TEST(AreaSlope, PipelineExamplesFromThePlan)
{
    // GEO-05 plan, OST square [0,0,4,0,4,4,0,4], thickness 1, rise 1, run 4, sr = 1: rotation 0.5 -> Windows tops
    // [2.357, 1.4794, 1.0, 1.8776]; rotation pi/2 -> [2, 2, 1, 1]; rise = -1, run = -4 equals rise 1, run 4;
    // rise = -1, run = 4 must NOT descend.
    Condition c = make_condition("a", ConditionType::Area);
    c.thickness = 1.0;
    const std::vector<double> pos{0, 0, 4, 0, 4, 4, 0, 4};
    auto tops = [&](double rise, double run, double rotation)
    {
        c.rise = rise;
        c.run = run;
        Takeoff t = make_takeoff("t", "a", "p", pos, "0", rotation);
        auto r = ost_meshing::create_mesh_for_takeoff(t, c, 1.0);
        REQUIRE(r.valid);
        return top_heights(r.mesh, 4);
    };
    auto near_vec = [](const std::vector<double> &got, const std::vector<double> &want, double tol)
    {
        REQUIRE_EQ(got.size(), want.size());
        for (size_t i = 0; i < got.size(); ++i)
            CHECK_NEAR(got[i], want[i], tol);
    };
    near_vec(tops(1, 4, 0.5), {2.357, 1.4794, 1.0, 1.8776}, 5e-4);
    near_vec(tops(1, 4, kHalfPi), {2.0, 2.0, 1.0, 1.0}, 1e-9);
    near_vec(tops(-1, -4, 0.0), tops(1, 4, 0.0), 0.0);
    near_vec(tops(-1, 4, 0.0), tops(1, 4, 0.0), 0.0);
    near_vec(tops(1, -4, 0.0), tops(1, 4, 0.0), 0.0);
    // MSH-03 plan, OST [0,0,10,0,10,10,0,10] thickness 1, rise 1 run 1, rotation 0.5236: top Y [14.6603, 6.0, 1.0, 9.6602]
    const std::vector<double> big{0, 0, 10, 0, 10, 10, 0, 10};
    c.rise = 1;
    c.run = 1;
    Takeoff t = make_takeoff("t", "a", "p", big, "0", 0.5236);
    auto r = ost_meshing::create_mesh_for_takeoff(t, c, 1.0);
    REQUIRE(r.valid);
    near_vec(top_heights(r.mesh, 4), {14.6603, 6.0, 1.0, 9.6602}, 5e-4);
    Takeoff t2 = make_takeoff("t", "a", "p", big, "0", 3.0 * kHalfPi);
    auto r2 = ost_meshing::create_mesh_for_takeoff(t2, c, 1.0);
    near_vec(top_heights(r2.mesh, 4), {1.0, 1.0, 11.0, 11.0}, 1e-9);
}

// ---------------------------------------------------------------- GEO-14
TEST(AreaTessellation, TriangleSetEqualsWindowsOnTheOracleCases)
{
    // Windows calc_area_faces on 13 outlines (CW / CCW, L-shape, concave, holes of both windings, two holes, ...):
    // iOS must use exactly the same triangles for the bottom cap, the top cap and every side wall (the winding
    // of each face is the separate GEO-15 question, so triples are compared unordered).
    for (const auto &c : ost_oracle::face_cases())
    {
        SCOPED_CONTEXT(c.name);
        Ring outer = ios_ring(c.outer);
        auto holes = ios_holes(c.holes);
        Mesh m = ost_meshing::generate_area_mesh(outer, 1.0, 0.0, holes, 0.0, 0.0, 0.0);
        const size_t caps = static_cast<size_t>(c.cap_faces_each);
        REQUIRE_EQ(m.faces.size(), c.faces.size());
        // bottom cap and top cap separately
        std::vector<std::array<uint32_t, 3>> w_bottom(c.faces.begin(), c.faces.begin() + caps);
        std::vector<std::array<uint32_t, 3>> w_top(c.faces.begin() + caps, c.faces.begin() + 2 * caps);
        std::vector<std::array<uint32_t, 3>> i_bottom(m.faces.begin(), m.faces.begin() + caps);
        std::vector<std::array<uint32_t, 3>> i_top(m.faces.begin() + caps, m.faces.begin() + 2 * caps);
        CHECK(triangle_set(i_bottom) == triangle_set(w_bottom));
        CHECK(triangle_set(i_top) == triangle_set(w_top));
        CHECK(triangle_set(m.faces) == triangle_set(c.faces));
    }
}

TEST(AreaTessellation, SquareWithAHoleMatchesTheWindowsFaceListFromThePlan)
{
    // GEO-14 plan: OST [0,0,4,0,4,4,0,4] with the 1x1 hole [1,1,2,1,2,2,1,2]: the first 8 bottom faces are the Windows
    // (1,9,10),(8,9,1),(2,1,10),(8,1,0),(3,2,10),(11,8,0),(3,10,11),(11,0,3), as index sets.
    Ring outer{{-0.0, -0.0}, {-4.0, -0.0}, {-4.0, -4.0}, {-0.0, -4.0}}; // iOS frame (x, z) = (-x_ost, -y_ost)
    Ring hole{{-1.0, -1.0}, {-2.0, -1.0}, {-2.0, -2.0}, {-1.0, -2.0}};
    Mesh m = ost_meshing::generate_area_mesh(outer, 1.0, 0.0, {hole}, 0.0, 0.0, 0.0);
    const std::vector<std::array<uint32_t, 3>> windows{{1, 9, 10}, {8, 9, 1}, {2, 1, 10}, {8, 1, 0},
                                                       {3, 2, 10}, {11, 8, 0}, {3, 10, 11}, {11, 0, 3}};
    REQUIRE(m.faces.size() >= 8);
    std::vector<std::array<uint32_t, 3>> got(m.faces.begin(), m.faces.begin() + 8);
    CHECK(triangle_set(got) == triangle_set(windows));
    // and the plain square: {0,1,3},{1,2,3}
    Mesh sq = ost_meshing::generate_area_mesh(Ring{{0, 0}, {-4, 0}, {-4, -4}, {0, -4}}, 1.0, 0.0, {}, 0.0, 0.0, 0.0);
    std::vector<std::array<uint32_t, 3>> sq_bottom(sq.faces.begin(), sq.faces.begin() + 2);
    CHECK(triangle_set(sq_bottom) == triangle_set(std::vector<std::array<uint32_t, 3>>{{1, 0, 3}, {3, 2, 1}}));
}

TEST(AreaTessellation, WindingSenseAndTopologyAreNotChangedByTheFrameFix)
{
    // GEO-14 must change the triangle set only: the mesh stays a closed, consistently oriented shell with the
    // outward (positive) signed volume iOS has today (GEO-15, the winding sense, is a separate decision).
    for (const auto &c : ost_oracle::face_cases())
    {
        SCOPED_CONTEXT(c.name);
        Ring outer = ios_ring(c.outer);
        auto holes = ios_holes(c.holes);
        Mesh m = ost_meshing::generate_area_mesh(outer, 1.0, 0.0, holes, 0.0, 0.0, 0.0);
        CHECK(closed_and_consistent(m));
        double area = shoelace_area(outer);
        for (const auto &h : holes)
            area -= shoelace_area(h);
        CHECK_NEAR(signed_volume(m), area * 1.0, 1e-9 * std::max(1.0, area));
    }
}

TEST(AreaTessellation, SelfIntersectingHoleTriangulatesLikeWindowsOnEveryCompiler)
{
    // Second review. Windows MeshFactory area (thickness 24, is_top) with the 4-point outline
    // [25.463,5.389, 11.48,2.136, 8.779,-1.244, 12.365,-6.719] and a hole [14.908,1.6, 15.055,1.173, 13.215,0.283,
    // 13.581,-0.226] whose edges 2 and 4 cross: earcut gives 2 outline + 1 hole triangles per cap (22 faces in all).
    // The ear tests of such input sit on the last bit; a compiler that fuses a * b + c (AppleClang on arm64) found a
    // complete 7-triangle cap (30 faces). The face list is the one the Windows pipeline returns.
    Condition c = make_condition("a", ConditionType::Area);
    c.thickness = 24.0;
    c.is_top = true;
    const std::vector<double> outer{25.463, 5.389, 11.48, 2.136, 8.779, -1.244, 12.365, -6.719};
    const Ring hole{{-14.908, -1.6}, {-15.055, -1.173}, {-13.215, -0.283}, {-13.581, 0.226}}; // iOS frame (-x, -y)
    Takeoff t = make_takeoff("t", "a", "p", outer);
    auto r = ost_meshing::create_mesh_for_takeoff(t, c, 1.0, {hole});
    REQUIRE(r.valid);
    const std::vector<std::array<uint32_t, 3>> windows{
        {3, 2, 1}, {9, 10, 8}, {3, 1, 0}, {5, 6, 7}, {12, 14, 13}, {4, 5, 7}, {0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5}, {2, 3, 7},
        {2, 7, 6}, {3, 0, 4}, {3, 4, 7}, {8, 12, 13}, {8, 13, 9}, {9, 13, 14}, {9, 14, 10}, {10, 14, 15}, {10, 15, 11}, {11, 15, 12},
        {11, 12, 8}};
    CHECK_EQ(r.mesh.vertices.size(), static_cast<size_t>(16));
    CHECK_EQ(r.mesh.faces.size(), windows.size());
    CHECK(triangle_set(r.mesh.faces) == triangle_set(windows));
}
