// Curved walls: segment count (MSH-05), arc side / sweep (MSH-06), control point at an end (MSH-07).
// Windows oracle: cpp_extensions/src/linear_geom/linear_geom.cpp (calc_adaptive_segs, arc_sweep,
// control_is_at_an_end, curve_pts_impl / gen_adv_curve_pts, proc_curved_pos) through the ost_linear_geom pyd.
// The iOS curve helpers work in the iOS (x, z) frame, z = -y: every test feeds (x, -y) and maps the result back, so
// the checks read exactly like the Windows tests.
#include "MeshingTestUtil.hpp"
#include "WindowsOracles.hpp"

#include "Domain/UomService.hpp"
#include "Geometry/strict_fp.hpp"

#include <cmath>
#include <limits>
#include <string>

using namespace mesh_test;

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    using Pt = std::pair<double, double>;

    int segs_w(double x1, double y1, double x2, double y2, double cx, double cy)
    {
        return ost_meshing::calc_curve_segments(x1, -y1, x2, -y2, cx, -cy);
    }

    // Windows-frame points of the iOS curve generator.
    std::vector<Pt> pts_w(double x1, double y1, double x2, double y2, double cx, double cy, int segs)
    {
        auto pts = ost_meshing::generate_curve_points(x1, -y1, x2, -y2, cx, -cy, segs);
        for (auto &p : pts)
            p.second = -p.second;
        return pts;
    }

    double polyline_length(const std::vector<Pt> &pts)
    {
        double total = 0.0;
        for (size_t i = 0; i + 1 < pts.size(); ++i)
            total += std::hypot(pts[i + 1].first - pts[i].first, pts[i + 1].second - pts[i].second);
        return total;
    }

    // ---- Port of tests/integration/native/test_geometry_boundaries.py::NativeLinearArcSideAndStraightChordTests ----
    constexpr int kSegments = 256;
    constexpr double kRadius = 100.0;
    constexpr double kCx = 500.0, kCy = 500.0;

    Pt at(double degrees)
    {
        double a = degrees * kPi / 180.0;
        return {kCx + kRadius * std::cos(a), kCy + kRadius * std::sin(a)};
    }

    double py_mod(double a, double m)
    {
        double r = std::fmod(a, m);
        if (r != 0.0 && ((r < 0.0) != (m < 0.0)))
            r += m;
        return r;
    }

    // Independent oracle (the Windows test's _arc_sweep_degrees).
    double arc_sweep_degrees(double start, double through, double end)
    {
        double ccw_to_end = py_mod(end - start, 360.0);
        double ccw_to_through = py_mod(through - start, 360.0);
        return ccw_to_through <= ccw_to_end ? ccw_to_end : 360.0 - ccw_to_end;
    }

    void assert_arc(double start, double through, double end)
    {
        SCOPED_CONTEXT("arc " + std::to_string(start) + " " + std::to_string(through) + " " + std::to_string(end));
        Pt p1 = at(start), pc = at(through), p2 = at(end);
        double expected = kRadius * (arc_sweep_degrees(start, through, end) * kPi / 180.0);
        double step = expected / kSegments;
        auto pts = pts_w(p1.first, p1.second, p2.first, p2.second, pc.first, pc.second, kSegments);
        REQUIRE_EQ(pts.size(), static_cast<size_t>(kSegments + 1));
        CHECK_NEAR(pts.front().first, p1.first, 1e-6);
        CHECK_NEAR(pts.front().second, p1.second, 1e-6);
        CHECK_NEAR(pts.back().first, p2.first, 1e-6);
        CHECK_NEAR(pts.back().second, p2.second, 1e-6);
        double nearest = 1e300;
        for (const auto &p : pts)
        {
            CHECK_NEAR(std::hypot(p.first - kCx, p.second - kCy), kRadius, 1e-6);
            nearest = std::min(nearest, std::hypot(p.first - pc.first, p.second - pc.second));
        }
        CHECK_NEAR(polyline_length(pts), expected, expected * 0.001);
        CHECK(nearest <= step);
    }
} // namespace

// ---------------------------------------------------------------- MSH-05
TEST(CurvedSegments, PlanCasesTruncateTheTwoPartsSeparately)
{
    // MSH-05 plan: p1 = (0,0), p2 = (137,0), control (60,39): Windows 34 segments (not 35); second case 56 (not 57).
    CHECK_EQ(segs_w(0, 0, 137, 0, 60, 39), 34);
    CHECK_EQ(segs_w(0, 0, 203.21968396585697, -10.101787042252369, 195.47789181682887, 57.74467022710263), 56);
}

TEST(CurvedSegments, MatchesWindowsCalcCurveSegsOnTheOracleTable)
{
    // Windows ost_linear_geom.calc_curve_segs on 48 inputs (WindowsOracles.hpp seg_cases).
    int i = 0;
    for (const auto &c : ost_oracle::seg_cases())
    {
        SCOPED_CONTEXT("case " + std::to_string(i++));
        CHECK_EQ(segs_w(c.x1, c.y1, c.x2, c.y2, c.cx, c.cy), c.segs);
    }
}

TEST(CurvedSegments, OutOfRangeAndNonFiniteDistancesGiveTheWindowsCount)
{
    // Windows calc_adaptive_segs does static_cast<int>(distance / 5): undefined in C++ for a value outside the int range,
    // +-inf and NaN. MSVC x64 gives INT_MIN there (Windows clamps it to 6; the sum base + INT_MIN or a sum past INT_MAX
    // wraps and clamps to 6 as well), AppleClang arm64 saturates (+huge -> INT_MAX -> 64, NaN -> 0). Counts computed by
    // running ost_linear_geom.calc_curve_segs (x1, y1, x2, y2, cx, cy) read-only; the same on every compiler here.
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    struct Case
    {
        double p[6];
        int segs;
    };
    const Case cases[] = {
        {{0, 0, 1e12, 0, 5e11, 1e11}, 6},        // chord / 5 far past INT_MAX
        {{0, 0, 1e10, 0, 5e9, 0}, 64},           // 2e9: still in range
        {{0, 0, 1.1e10, 0, 5e9, 0}, 6},          // 2.2e9: out of range
        {{0, 0, 100, 0, 50, 1e11}, 6},           // offset / 5 out of range
        {{0, 0, 100, 0, 50, 2e10}, 6},           //
        {{0, 0, 100, 0, 50, 1e10}, 64},          // offset / 5 = 2e9 in range: 6 + 2e9 clamps to 64
        {{0, 0, 1e308, 0, 5e307, 1e308}, 6},     //
        {{0, 0, inf, 0, 5, 5}, 6},               //
        {{0, 0, 100, 0, nan, 5}, 6},             //
        {{0, 0, 100, 0, 50, inf}, 6},            //
        {{0, 0, 1e10, 0, 5e9, 1e10}, 64},        //
        {{0, 0, 100, 0, 50, 10737417000.0}, 64}, // offset / 5 = 2147483400: the sum 6 + 2147483400 still fits
        {{0, 0, 100, 0, 50, 10737418000.0}, 64}, //
        {{0, 0, 100, 0, 50, 10737418210.0}, 6},  // offset / 5 = 2147483642: 6 + that passes INT_MAX and wraps negative
        {{0, 0, 100, 0, 50, 10737418220.0}, 6},  //
        {{0, 0, 100, 0, 50, 10737418235.0}, 6},  //
        {{0, 0, 100, 0, 50, 10737418240.0}, 6},  // offset / 5 = 2^31: out of range
    };
    int i = 0;
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT("case " + std::to_string(i++));
        CHECK_EQ(segs_w(c.p[0], c.p[1], c.p[2], c.p[3], c.p[4], c.p[5]), c.segs);
    }
}

TEST(CurvedSegments, CurvedMeshHasFourTimesSegmentsPlusOneVertices)
{
    // MSH-05 plan: the curved mesh has 4 * (segs + 1) vertices.
    Condition c = make_condition("c", ConditionType::Linear);
    c.height = 96.0;
    c.thickness = 6.0;
    Takeoff t = make_takeoff("t", "c", "p", {0, 0, 137, 0, 60, 39});
    t.curve = 0;
    auto r = ost_meshing::create_mesh_for_takeoff(t, c, 1.0);
    REQUIRE(r.valid);
    CHECK_EQ(r.mesh.vertices.size(), static_cast<size_t>(4 * (34 + 1)));
    Takeoff t2 = make_takeoff("t", "c", "p",
                              {0, 0, 203.21968396585697, -10.101787042252369, 195.47789181682887, 57.74467022710263});
    t2.curve = 0;
    auto r2 = ost_meshing::create_mesh_for_takeoff(t2, c, 1.0);
    REQUIRE(r2.valid);
    CHECK_EQ(r2.mesh.vertices.size(), static_cast<size_t>(4 * (56 + 1)));
}

TEST(CurvedSegments, AgreesWithTheQuantityCodeOnTheWindowsGrid)
{
    // Port of tests/integration/native/test_geometry_boundaries.py::test_the_native_arc_matches_the_python_measure_on_a_grid
    // (native mesh geometry vs the measured-quantity geometry, ost_domain::calc_curve_segments / advanced_curve_points).
    int checked = 0;
    const double centers[3][2] = {{0.0, 0.0}, {500.0, 500.0}, {-1234.5, 777.25}};
    for (const auto &center : centers)
        for (double radius : {5.0, 100.0})
            for (double sweep : {40.0, 179.0, 181.0, 250.0, 330.0})
                for (double fraction : {0.02, 0.2, 0.5, 0.8, 0.98})
                    for (double direction : {1.0, -1.0})
                        for (double start : {0.0, 135.0, 179.0, 270.0})
                        {
                            double a[3] = {start, start + direction * sweep * fraction, start + direction * sweep};
                            double px[3], py[3];
                            for (int k = 0; k < 3; ++k)
                            {
                                px[k] = center[0] + radius * std::cos(a[k] * kPi / 180.0);
                                py[k] = center[1] + radius * std::sin(a[k] * kPi / 180.0);
                            }
                            SCOPED_CONTEXT("center " + std::to_string(center[0]) + " r " + std::to_string(radius) +
                                           " start " + std::to_string(start) + " sweep " + std::to_string(sweep));
                            int s_mesh = segs_w(px[0], py[0], px[2], py[2], px[1], py[1]);
                            int s_qty = ost_domain::calc_curve_segments(px[0], py[0], px[2], py[2], px[1], py[1]);
                            CHECK_EQ(s_mesh, s_qty);
                            auto native = pts_w(px[0], py[0], px[2], py[2], px[1], py[1], s_mesh);
                            auto python = ost_domain::advanced_curve_points(px[0], py[0], px[2], py[2], px[1], py[1],
                                                                            s_qty);
                            std::vector<Pt> py_pts(python.begin(), python.end());
                            CHECK_NEAR(polyline_length(native), polyline_length(py_pts), 1e-6);
                            ++checked;
                        }
    CHECK(checked > 1000);
}

// ---------------------------------------------------------------- MSH-06 / MSH-07
TEST(ArcSide, OstConfirmed251DegreeRoute)
{
    // Port of ...::test_the_ost_confirmed_arc_follows_the_251_degree_route (start 0, control 250, end 251, R = 100:
    // 438.1, not the short arc 190.2).
    Pt p1 = at(0), pc = at(250), p2 = at(251);
    auto pts = pts_w(p1.first, p1.second, p2.first, p2.second, pc.first, pc.second, kSegments);
    CHECK_NEAR(polyline_length(pts), kRadius * (251.0 * kPi / 180.0), 0.5);
}

TEST(ArcSide, ReversedDirectionAndSwappedEndPoints)
{
    // Port of ...::test_reversed_direction_and_swapped_end_points
    const double arcs[][3] = {{251, 250, 0}, {0, -110, -251}, {0, 110, 100}, {100, 110, 0}, {0, -250, -251}};
    for (const auto &a : arcs)
        assert_arc(a[0], a[1], a[2]);
}

TEST(ArcSide, MajorArcsWithTheControlPointNearEitherEnd)
{
    // Port of ...::test_major_arcs_with_the_control_point_near_either_end
    for (double sweep : {200.0, 250.0, 300.0})
        for (double fraction : {0.01, 0.03, 0.05, 0.5, 0.95, 0.97, 0.99})
            for (double direction : {1.0, -1.0})
                for (double start : {0.0, 100.0, 200.0, 310.0})
                    assert_arc(start, start + direction * sweep * fraction, start + direction * sweep);
}

TEST(ArcSide, ArcsWhoseAnglesWrapPastPlusOrMinus180Degrees)
{
    // Port of ...::test_arcs_whose_angles_wrap_past_plus_or_minus_180_degrees
    for (double start : {-175.0, 170.0, 175.0, 179.5, 180.0, 185.0, 359.5})
        for (double sweep : {90.0, 200.0, 260.0})
            for (double fraction : {0.04, 0.5, 0.96})
                for (double direction : {1.0, -1.0})
                    assert_arc(start, start + direction * sweep * fraction, start + direction * sweep);
}

TEST(ArcSide, SweepsUpTo180DegreesAndBisectorControlsAreUnchanged)
{
    // Port of ...::test_sweeps_up_to_180_degrees_and_bisector_controls_are_unchanged
    for (double sweep : {20.0, 60.0, 120.0, 170.0})
        for (double fraction : {0.05, 0.3, 0.5, 0.7, 0.95})
            for (double direction : {1.0, -1.0})
                assert_arc(30.0, 30.0 + direction * sweep * fraction, 30.0 + direction * sweep);
    for (double sweep : {30.0, 90.0, 179.0, 181.0, 250.0, 300.0, 350.0})
        for (double direction : {1.0, -1.0})
            assert_arc(40.0, 40.0 + direction * sweep / 2.0, 40.0 + direction * sweep);
}

TEST(ArcSide, ControlPointNearButNotEqualToAnEndStillFollowsTheArc)
{
    // Port of ...::test_a_control_point_that_is_near_but_not_equal_to_an_end_still_follows_the_arc
    const double arcs[][3] = {{0, 0.2, 250},   {0, 249.8, 250}, {90, 90.2, 340}, {0, -0.2, -250},
                              {0, 0.01, 250},  {0, 1e-5, 250},  {0, 249.99999, 250}, {0, -1e-5, -250}};
    for (const auto &a : arcs)
        assert_arc(a[0], a[1], a[2]);
}

TEST(ArcSide, MatchesWindowsPointsOnTheOracleTable)
{
    // Windows ost_linear_geom.gen_adv_curve_pts on 26 arcs (major / minor, wrapped, near-end, straight chord,
    // axis-aligned pieces, collinear and Bezier fallbacks, both end points equal).
    for (const auto &c : ost_oracle::arc_cases())
    {
        SCOPED_CONTEXT(c.name);
        auto pts = pts_w(c.x1, c.y1, c.x2, c.y2, c.cx, c.cy, c.segs);
        REQUIRE_EQ(pts.size(), c.pts.size());
        for (size_t i = 0; i < pts.size(); ++i)
        {
            SCOPED_CONTEXT("point " + std::to_string(i));
            CHECK_NEAR(pts[i].first, c.pts[i][0], rel_tol(pts[i].first, c.pts[i][0]));
            CHECK_NEAR(pts[i].second, c.pts[i][1], rel_tol(pts[i].second, c.pts[i][1]));
        }
    }
}

// ---------------------------------------------------------------- MSH-07
TEST(StraightChord, ControlEqualToAnEndPointIsEvenlySpaced)
{
    // MSH-07 plan: p1 = (0,0), p2 = (100,0), control = (100,0): 20 segments, point i is x = 5 * i.
    int segs = segs_w(0, 0, 100, 0, 100, 0);
    CHECK_EQ(segs, 20);
    auto pts = pts_w(0, 0, 100, 0, 100, 0, segs);
    REQUIRE_EQ(pts.size(), static_cast<size_t>(21));
    CHECK_NEAR(pts[10].first, 50.0, 1e-9);
    CHECK_NEAR(pts[10].second, 0.0, 1e-9);
    CHECK_NEAR(pts[1].first, 5.0, 1e-9);
    for (size_t i = 0; i < pts.size(); ++i)
    {
        CHECK_NEAR(pts[i].first, 5.0 * static_cast<double>(i), 1e-9);
        CHECK_NEAR(pts[i].second, 0.0, 1e-9);
    }
}

TEST(StraightChord, ControlEqualToAnEndPointOnEveryChordDirectionAndLength)
{
    // Port of ...::test_a_control_point_equal_to_an_end_point_is_a_straight_chord
    for (int rotation = 0; rotation < 360; rotation += 15)
        for (double chord : {0.5, 100.0, 5000.0})
        {
            double angle = rotation * kPi / 180.0;
            Pt p1{500.0, -300.0};
            Pt p2{p1.first + chord * std::cos(angle), p1.second + chord * std::sin(angle)};
            for (int which = 0; which < 2; ++which)
            {
                SCOPED_CONTEXT("rotation " + std::to_string(rotation) + " chord " + std::to_string(chord) +
                               (which ? " control=end" : " control=start"));
                Pt c = which ? p2 : p1;
                auto pts = pts_w(p1.first, p1.second, p2.first, p2.second, c.first, c.second, kSegments);
                REQUIRE_EQ(pts.size(), static_cast<size_t>(kSegments + 1));
                CHECK_EQ(pts.front().first, p1.first);
                CHECK_EQ(pts.front().second, p1.second);
                CHECK_EQ(pts.back().first, p2.first);
                CHECK_EQ(pts.back().second, p2.second);
                CHECK_NEAR(polyline_length(pts), chord, chord * 1e-9);
                for (size_t i = 0; i < pts.size(); ++i)
                {
                    double t = static_cast<double>(i) / kSegments;
                    CHECK_NEAR(pts[i].first, p1.first + t * (p2.first - p1.first), chord * 1e-9);
                    CHECK_NEAR(pts[i].second, p1.second + t * (p2.second - p1.second), chord * 1e-9);
                }
            }
        }
}

TEST(StraightChord, ControlWithinATinyRelativeEpsilonOfAnEndIsStillThatEnd)
{
    // Port of ...::test_a_control_point_within_a_tiny_relative_epsilon_of_an_end_is_still_that_end
    for (double offset : {0.0, 1e-12, 1e-10})
    {
        const Pt controls[2] = {{offset, -offset}, {60 + offset, 80 - offset}};
        for (const auto &c : controls)
        {
            SCOPED_CONTEXT("offset " + std::to_string(offset));
            auto pts = pts_w(0, 0, 60, 80, c.first, c.second, kSegments);
            CHECK_NEAR(polyline_length(pts), 100.0, 1e-6);
        }
    }
}

TEST(StraightChord, EndPointEpsilonIsRelativeToTheChordLength)
{
    // Port of ...::test_the_end_point_epsilon_is_relative_to_the_chord_length: 1e-4 from an end of a 1e6 chord is 1e-10
    // of it, inside the 1e-9 epsilon: evenly spaced straight chord, not the quadratic fallback.
    const Pt controls[2] = {{1e-4, 0.0}, {6e5 - 1e-4, 8e5}};
    for (const auto &c : controls)
    {
        SCOPED_CONTEXT("control " + std::to_string(c.first));
        auto pts = pts_w(0.0, 0.0, 6e5, 8e5, c.first, c.second, kSegments);
        for (size_t i = 0; i < pts.size(); ++i)
        {
            CHECK_NEAR(pts[i].first, 6e5 * static_cast<double>(i) / kSegments, 1e-3);
            CHECK_NEAR(pts[i].second, 8e5 * static_cast<double>(i) / kSegments, 1e-3);
        }
    }
}

TEST(StraightChord, CollinearControlsKeepTheQuadraticFallback)
{
    // Port of ...::test_collinear_controls_keep_the_quadratic_fallback
    struct C
    {
        double p[6];
        double len;
    };
    const C cases[] = {{{0, 0, 100, 0, 50, 0}, 100.0}, {{0, 0, 100, 0, 150, 0}, 125.0}, {{0, 0, 100, 0, -50, 0}, 125.0}};
    for (const auto &c : cases)
    {
        auto pts = pts_w(c.p[0], c.p[1], c.p[2], c.p[3], c.p[4], c.p[5], 512);
        CHECK_NEAR(polyline_length(pts), c.len, 0.01);
    }
}

// ---------------------------------------------------------------- the mesh uses them
TEST(CurvedMesh, CentreLineMatchesTheWindowsMeshOnTheOracleCases)
{
    // Windows MeshFactory curved linear meshes: arcs over 180 degrees (MSH-06), a zero-length 7-value curve that keeps
    // its stored control (MSH-07), the 7-value offset form, a control at an end (straight chord, 20 segments), the
    // segment-count plan cases (MSH-05) and a slope.
    for (const auto &c : ost_oracle::curved_mesh_cases())
    {
        SCOPED_CONTEXT(c.name);
        Condition cond = make_condition("c", ConditionType::Linear);
        cond.height = 96.0;
        cond.thickness = 6.0;
        cond.rise = c.rise;
        cond.run = c.run;
        Takeoff t = make_takeoff("t", "c", "p", c.position);
        t.curve = 0;
        auto r = ost_meshing::create_mesh_for_takeoff(t, cond, 1.0);
        REQUIRE(r.valid);
        const size_t n = static_cast<size_t>(c.samples);
        REQUIRE_EQ(r.mesh.vertices.size(), 4 * n);
        for (size_t i = 0; i < n; ++i)
        {
            SCOPED_CONTEXT("sample " + std::to_string(i));
            const V3 &ib = r.mesh.vertices[i];
            const V3 &ob = r.mesh.vertices[n + i];
            const V3 &it = r.mesh.vertices[2 * n + i];
            // iOS frame: (X, Y up, Z) with Z = -Y_windows
            V3 w = to_ios(c.center[i]);
            double mx = (ib[0] + ob[0]) * 0.5, mz = (ib[2] + ob[2]) * 0.5;
            CHECK_NEAR(mx, w[0], rel_tol(mx, w[0]));
            CHECK_NEAR(mz, w[2], rel_tol(mz, w[2]));
            CHECK_NEAR(ib[1], w[1], rel_tol(ib[1], w[1]));
            CHECK_NEAR(it[1], c.top_z[i], rel_tol(it[1], c.top_z[i]));
            CHECK_NEAR(std::hypot(ib[0] - ob[0], ib[2] - ob[2]) * 0.5, 3.0, 1e-9);
        }
    }
}

TEST(CurvedMesh, SevenValueOffsetOnTheSegmentCountBoundaryMatchesWindows)
{
    // The 7-value form rebuilds the control point as chord midpoint + perpendicular offset, so an offset that is a
    // multiple of 5 puts int(offset / 5) of the segment count exactly on a boundary and the last bit of the control point
    // decides between n and n + 1 segments. Windows rounds every operation; a compiler that fuses a * b + c (AppleClang on
    // arm64) gave 4 vertices more on these inputs. Expected: the vertex count of the Windows MeshFactory curved wall
    // (4 * (segments + 1)) for integer chords with an offset of 5, 20, 25 and 30.
    struct Case
    {
        double p[7];
        size_t vertices;
    };
    const Case cases[] = {
        {{57.0, 17.0, 20.0, 43.0, 8.0, 60.0, 20.0}, 52},
        {{-11.0, 46.0, -12.0, 57.0, 18.0, -3.0, 5.0}, 28},
        {{26.0, 10.0, 7.0, 22.0, 53.0, -37.0, 25.0}, 52},
        {{42.0, -19.0, -19.0, 24.0, -11.0, -39.0, 20.0}, 72},
        {{-14.0, 14.0, -33.0, 21.0, 6.0, 32.0, 30.0}, 64},
        {{18.0, 36.0, -37.0, -11.0, 41.0, -18.0, 30.0}, 84},
    };
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT("offset " + std::to_string(c.p[6]) + " chord from " + std::to_string(c.p[0]) + "," +
                       std::to_string(c.p[1]));
        Condition cond = make_condition("c", ConditionType::Linear);
        cond.height = 96.0;
        cond.thickness = 6.0;
        Takeoff t = make_takeoff("t", "c", "p", {c.p[0], c.p[1], c.p[2], c.p[3], c.p[4], c.p[5], c.p[6]});
        t.curve = 0;
        auto r = ost_meshing::create_mesh_for_takeoff(t, cond, 1.0);
        REQUIRE(r.valid);
        CHECK_EQ(r.mesh.vertices.size(), c.vertices);
    }
}

TEST(CurvedMesh, MajorArcRouteLengthThroughTheFactory)
{
    // MSH-06 plan: centre (500,500), R = 100, start 0, control 250, end 251: Windows centre line 437.7 (analytic
    // R * 251 deg = 438.1; the Windows test delta is 0.5), iOS used to give the short arc 190.2.
    Pt p1 = at(0), pc = at(250), p2 = at(251);
    Condition cond = make_condition("c", ConditionType::Linear);
    cond.height = 1.0;
    cond.thickness = 2.0;
    Takeoff t = make_takeoff("t", "c", "p", {p1.first, p1.second, p2.first, p2.second, pc.first, pc.second});
    t.curve = 0;
    auto r = ost_meshing::create_mesh_for_takeoff(t, cond, 1.0);
    REQUIRE(r.valid);
    const size_t n = r.mesh.vertices.size() / 4;
    double len = 0.0;
    for (size_t i = 0; i + 1 < n; ++i)
    {
        double ax = (r.mesh.vertices[i][0] + r.mesh.vertices[n + i][0]) * 0.5;
        double az = (r.mesh.vertices[i][2] + r.mesh.vertices[n + i][2]) * 0.5;
        double bx = (r.mesh.vertices[i + 1][0] + r.mesh.vertices[n + i + 1][0]) * 0.5;
        double bz = (r.mesh.vertices[i + 1][2] + r.mesh.vertices[n + i + 1][2]) * 0.5;
        len += std::hypot(bx - ax, bz - az);
    }
    CHECK_NEAR(len, 437.7, 0.5);
}

// ---------------------------------------------------------------- plain hypot (second review)
TEST(StrictFp, HypotIsTheSquareRootOfTheSumOfSquaresAndSurvivesTheExtremes)
{
    // ost_fp::hypot replaces std::hypot in the meshing code (the MSVC CRT of the Windows build returns exactly
    // sqrt(x * x + y * y); other libms round the last bit differently and moved the curved wall segment count).
    CHECK_EQ(ost_fp::hypot(3.0, 4.0), 5.0);
    CHECK_EQ(ost_fp::hypot(-6.0, 8.0), 10.0);
    CHECK_EQ(ost_fp::hypot(0.0, 0.0), 0.0);
    CHECK_EQ(ost_fp::hypot(0.0, -7.5), 7.5);
    const double xs[] = {0.1, 12.345, -3.7, 1e5 / 3.0, 1e-3};
    for (double x : xs)
        for (double y : xs)
            CHECK_EQ(ost_fp::hypot(x, y), std::sqrt(x * x + y * y));
    // where the plain form overflows / underflows / is not a number, std::hypot decides
    CHECK_EQ(ost_fp::hypot(1e200, 1e200), std::hypot(1e200, 1e200));
    CHECK(std::isfinite(ost_fp::hypot(1e200, 1e200)));
    CHECK_EQ(ost_fp::hypot(1e-200, 1e-200), std::hypot(1e-200, 1e-200));
    CHECK(ost_fp::hypot(1e-200, 1e-200) > 0.0);
    CHECK(std::isnan(ost_fp::hypot(std::nan(""), 1.0)));
    CHECK(std::isinf(ost_fp::hypot(std::numeric_limits<double>::infinity(), 1.0)));
}

// ---------------------------------------------------------------- drop stub length (second review)
TEST(CurvedDropStub, StubLengthIsMeasuredOnTheCentreLineLikeWindows)
{
    // Windows MeshGenerator.generate_curved_drop_stub_mesh measures the stub length on the centre-line points of the
    // wall (sqrt(dx**2 + dy**2), `arc_len >= thickness`). Chords whose steps add up to exactly the thickness sit on that
    // threshold; the midpoints of the offset rings carry rounding noise and kept one sample more or less than Windows
    // (24 / 24 / 16 vertices instead of 28 / 20 / 12). Expected vertices: the Windows stub of each case (Windows frame),
    // compared as a set (the iOS offset rings are the mirror image of the Windows ones).
    struct Case
    {
        const char *name;
        double p[6];
        double thickness, drop, z_offset, height, rise, run;
        size_t faces;
        std::vector<V3> windows;
    };
    const Case cases[] = {
        {"7-24-25 chord x3, control at the start, thickness 25",
         {5.0, 13.0, 26.0, 85.0, 5.0, 13.0}, 25.0, 12.0, 96.0, 1.0, 0.0, 1.0, 52,
         {
            {-29.6, 52.7, 97.0},
            {-31.0, 57.5, 97.0},
            {-32.4, 62.3, 97.0},
            {-33.8, 67.1, 97.0},
            {-35.2, 71.9, 97.0},
            {-36.599999999999994, 76.69999999999999, 97.0},
            {-38.0, 81.5, 97.0},
            {-5.600000000000001, 59.7, 97.0},
            {-7.0, 64.5, 97.0},
            {-8.399999999999999, 69.3, 97.0},
            {-9.8, 74.1, 97.0},
            {-11.200000000000003, 78.9, 97.0},
            {-12.599999999999998, 83.69999999999999, 97.0},
            {-13.999999999999998, 88.5, 97.0},
            {-29.6, 52.7, 109.0},
            {-31.0, 57.5, 109.0},
            {-32.4, 62.3, 109.0},
            {-33.8, 67.1, 109.0},
            {-35.2, 71.9, 109.0},
            {-36.599999999999994, 76.69999999999999, 109.0},
            {-38.0, 81.5, 109.0},
            {-5.600000000000001, 59.7, 109.0},
            {-7.0, 64.5, 109.0},
            {-8.399999999999999, 69.3, 109.0},
            {-9.8, 74.1, 109.0},
            {-11.200000000000003, 78.9, 109.0},
            {-12.599999999999998, 83.69999999999999, 109.0},
            {-13.999999999999998, 88.5, 109.0}
         }},
        {"3-4-5 chord x3, control near the middle, thickness 10, negative drop with slope",
         {-18.0, 11.0, -9.0, 23.0, -13.5, 17.0}, 10.0, -1.5, 5.0, 10.0, 1.0, 1.0, 36,
         {
            {11.000000000000002, 12.0, 8.5},
            {9.500000000000002, 13.999999999999998, 11.0},
            {8.0, 16.0, 13.5},
            {6.5, 18.0, 16.0},
            {5.0, 20.0, 18.5},
            {19.0, 18.000000000000004, 8.5},
            {17.5, 20.0, 11.0},
            {16.0, 22.0, 13.5},
            {14.5, 24.0, 16.0},
            {13.0, 26.0, 18.5},
            {11.000000000000002, 12.0, 10.0},
            {9.500000000000002, 13.999999999999998, 12.5},
            {8.0, 16.0, 15.0},
            {6.5, 18.0, 17.5},
            {5.0, 20.0, 20.0},
            {19.0, 18.000000000000004, 10.0},
            {17.5, 20.0, 12.5},
            {16.0, 22.0, 15.0},
            {14.5, 24.0, 17.5},
            {13.0, 26.0, 20.0}
         }},
        {"12-5-13 chord x5, control off the chord, thickness 10",
         {13.0, -14.0, 73.0, 11.0, 43.0, -1.5}, 10.0, 12.0, 0.0, 1.0, 0.0, 0.0, 20,
         {
            {-65.6923076923077, 2.538461538461539, 1.0},
            {-70.3076923076923, 4.461538461538463, 1.0},
            {-74.92307692307692, 6.384615384615384, 1.0},
            {-61.84615384615385, 11.76923076923077, 1.0},
            {-66.46153846153847, 13.692307692307693, 1.0},
            {-71.07692307692308, 15.615384615384617, 1.0},
            {-65.6923076923077, 2.538461538461539, 13.0},
            {-70.3076923076923, 4.461538461538463, 13.0},
            {-74.92307692307692, 6.384615384615384, 13.0},
            {-61.84615384615385, 11.76923076923077, 13.0},
            {-66.46153846153847, 13.692307692307693, 13.0},
            {-71.07692307692308, 15.615384615384617, 13.0}
         }},
    };
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT(c.name);
        // MeshFactory frame: (x, z) = (-x_ost, -y_ost)
        Mesh m = ost_meshing::generate_curved_drop_stub_mesh(-c.p[0], -c.p[1], -c.p[2], -c.p[3], -c.p[4], -c.p[5],
                                                             c.thickness, c.drop, c.z_offset, c.height, c.rise, c.run);
        REQUIRE_EQ(m.vertices.size(), c.windows.size());
        CHECK_EQ(m.faces.size(), c.faces);
        CHECK(vertex_sets_equal(m.vertices, to_ios_all(c.windows), 1e-9));
    }
}
