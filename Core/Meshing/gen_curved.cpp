#include "MeshGenerator.hpp"
#include "mesh_helpers.hpp"
#include <vector>
#include <utility>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ost_meshing
{

    static void add_curved_faces(ost_geometry::CppMeshData &mesh, int n)
    {
        auto u = [](int v) -> uint32_t
        { return static_cast<uint32_t>(v); };
        int ib = 0, ob = n, it = 2 * n, ot = 3 * n;

        for (int i = 0; i < n - 1; i++)
        {
            mesh.faces.push_back({u(ib + i), u(it + i + 1), u(ib + i + 1)});
            mesh.faces.push_back({u(ib + i), u(it + i), u(it + i + 1)});
            mesh.faces.push_back({u(ob + i), u(ob + i + 1), u(ot + i + 1)});
            mesh.faces.push_back({u(ob + i), u(ot + i + 1), u(ot + i)});
            mesh.faces.push_back({u(ib + i), u(ib + i + 1), u(ob + i + 1)});
            mesh.faces.push_back({u(ib + i), u(ob + i + 1), u(ob + i)});
            mesh.faces.push_back({u(it + i), u(ot + i), u(ot + i + 1)});
            mesh.faces.push_back({u(it + i), u(ot + i + 1), u(it + i + 1)});
        }

        mesh.faces.push_back({u(ib), u(ob), u(ot)});
        mesh.faces.push_back({u(ib), u(ot), u(it)});
        mesh.faces.push_back({u(ib + n - 1), u(it + n - 1), u(ot + n - 1)});
        mesh.faces.push_back({u(ib + n - 1), u(ot + n - 1), u(ob + n - 1)});
    }

    namespace
    {
        // ---- Windows cpp_extensions/src/linear_geom/linear_geom.cpp, verbatim, in the Windows XY frame ----
        constexpr double kInf = std::numeric_limits<double>::infinity();

        double perp_slope(double dx, double dy)
        {
            if (dy == 0.0)
                return kInf;
            if (dx == 0.0)
                return 0.0;
            return -dx / dy;
        }

        // The arc always runs from the start through the control point to the end: counter-clockwise to the end when
        // the control angle lies inside the counter-clockwise arc start -> end, otherwise clockwise.
        double arc_sweep(double a_s, double a_e, double a_c)
        {
            constexpr double TWO_PI = 2.0 * M_PI;
            double ccw_to_end = std::fmod(a_e - a_s, TWO_PI);
            if (ccw_to_end < 0.0)
                ccw_to_end += TWO_PI;
            double ccw_to_ctl = std::fmod(a_c - a_s, TWO_PI);
            if (ccw_to_ctl < 0.0)
                ccw_to_ctl += TWO_PI;
            return ccw_to_ctl <= ccw_to_end ? ccw_to_end : ccw_to_end - TWO_PI;
        }

        // A control point within 1e-9 x chord length of an end point means "no curve": the straight chord.
        bool control_is_at_an_end(double x1, double y1, double x2, double y2, double cx, double cy)
        {
            constexpr double STRAIGHT_CHORD_EPSILON = 1e-9;
            double nearest = std::min(std::hypot(cx - x1, cy - y1), std::hypot(cx - x2, cy - y2));
            return nearest <= STRAIGHT_CHORD_EPSILON * std::hypot(x2 - x1, y2 - y1);
        }

        std::vector<std::pair<double, double>> straight_chord_pts(
            double x1, double y1, double x2, double y2, int segs)
        {
            std::vector<std::pair<double, double>> pts;
            pts.reserve(static_cast<size_t>(segs) + 1);
            for (int i = 0; i <= segs; ++i)
            {
                double t = static_cast<double>(i) / segs;
                double u = 1.0 - t;
                pts.push_back({u * x1 + t * x2, u * y1 + t * y2});
            }
            return pts;
        }

        // curve_pts_impl; gen_adv_curve_pts (the one the Windows wall uses) returns exactly the same points.
        std::vector<std::pair<double, double>> curve_pts_impl(
            double x1, double y1, double x2, double y2,
            double cx, double cy, int segs)
        {
            if (segs < 1)
                throw std::invalid_argument("segments must be at least 1");
            if (control_is_at_an_end(x1, y1, x2, y2, cx, cy))
                return straight_chord_pts(x1, y1, x2, y2, segs);
            double m1x = (x1 + cx) * 0.5, m1y = (y1 + cy) * 0.5;
            double m2x = (cx + x2) * 0.5, m2y = (cy + y2) * 0.5;
            double ps1 = perp_slope(cx - x1, cy - y1);
            double ps2 = perp_slope(x2 - cx, y2 - cy);
            double ctr_x = kInf, ctr_y = kInf;
            bool found = false;
            if (ps1 == kInf && ps2 != kInf)
            {
                ctr_x = m1x;
                ctr_y = m2y + ps2 * (m1x - m2x);
                found = true;
            }
            else if (ps2 == kInf && ps1 != kInf)
            {
                ctr_x = m2x;
                ctr_y = m1y + ps1 * (m2x - m1x);
                found = true;
            }
            else if (std::abs(ps1 - ps2) > 1e-10)
            {
                ctr_x = (m2y - m1y + ps1 * m1x - ps2 * m2x) / (ps1 - ps2);
                ctr_y = m1y + ps1 * (ctr_x - m1x);
                found = true;
            }
            if (found)
            {
                double r1 = std::hypot(ctr_x - x1, ctr_y - y1);
                double r2 = std::hypot(ctr_x - x2, ctr_y - y2);
                double rc = std::hypot(ctr_x - cx, ctr_y - cy);
                if (std::abs(r1 - r2) < 0.1 && std::abs(r1 - rc) < 0.1)
                {
                    double r = r1;
                    double a_s = std::atan2(y1 - ctr_y, x1 - ctr_x);
                    double a_e = std::atan2(y2 - ctr_y, x2 - ctr_x);
                    double a_c = std::atan2(cy - ctr_y, cx - ctr_x);
                    double a_diff = arc_sweep(a_s, a_e, a_c);
                    std::vector<std::pair<double, double>> pts;
                    pts.reserve(static_cast<size_t>(segs) + 1);
                    for (int i = 0; i <= segs; ++i)
                    {
                        double t = static_cast<double>(i) / segs;
                        double a = a_s + t * a_diff;
                        pts.push_back({ctr_x + r * std::cos(a), ctr_y + r * std::sin(a)});
                    }
                    return pts;
                }
            }
            std::vector<std::pair<double, double>> pts;
            pts.reserve(static_cast<size_t>(segs) + 1);
            for (int i = 0; i <= segs; ++i)
            {
                double t = static_cast<double>(i) / segs;
                double u = 1.0 - t;
                pts.push_back({u * u * x1 + 2.0 * u * t * cx + t * t * x2,
                               u * u * y1 + 2.0 * u * t * cy + t * t * y2});
            }
            return pts;
        }
    } // namespace

    // The iOS mesh frame is (x, z) = (X_windows, -Y_windows). The curve is computed in the Windows frame (negating is
    // exact) so the points are bit-identical to the Windows ones; the sweep direction flips with the mirror and
    // fmod / the +2 pi wrap are not mirror symmetric.
    std::vector<std::pair<double, double>> generate_curve_points(
        double x1, double z1, double x2, double z2,
        double cx, double cz, int segments)
    {
        auto pts = curve_pts_impl(x1, -z1, x2, -z2, cx, -cz, segments);
        for (auto &p : pts)
            p.second = -p.second;
        return pts;
    }

    // Windows calc_adaptive_segs: both parts truncate to int separately. The expressions are symmetric under z -> -z.
    int calc_curve_segments(double x1, double z1, double x2, double z2,
                            double cx, double cz)
    {
        double chord_d = std::hypot(x2 - x1, z2 - z1);
        double curve_d = (std::hypot(cx - x1, cz - z1) + std::hypot(x2 - cx, z2 - cz)) * 0.8;
        int base_s = std::clamp(static_cast<int>(std::max(chord_d, curve_d) / 5.0), 6, 64);
        if (chord_d > 0)
        {
            double dx = x2 - x1, dz = z2 - z1;
            double t = std::clamp(((cx - x1) * dx + (cz - z1) * dz) / (dx * dx + dz * dz), 0.0, 1.0);
            double offset = std::hypot(cx - (x1 + t * dx), cz - (z1 + t * dz));
            base_s += static_cast<int>(offset / 5.0);
        }
        return std::clamp(base_s, 6, 64);
    }

    ost_geometry::CppMeshData generate_curved_linear_mesh(
        double x1, double z1, double x2, double z2,
        double cx, double cz,
        double height, double thickness, double y_offset,
        double rise, double run)
    {
        ost_geometry::CppMeshData mesh;

        int segs = calc_curve_segments(x1, z1, x2, z2, cx, cz);
        auto curve_pts = generate_curve_points(x1, z1, x2, z2, cx, cz, segs);
        int n = static_cast<int>(curve_pts.size());
        if (n < 2)
            return mesh;
        double chord_len = std::hypot(x2 - x1, z2 - z1);
        auto slope = compute_slope(rise, run, chord_len);

        double half_t = thickness * 0.5;

        mesh.vertices.resize(n * 4);

        for (int i = 0; i < n; i++)
        {
            double px = curve_pts[i].first, pz = curve_pts[i].second;
            double tx, tz;
            if (i == 0 && n > 1)
            {
                tx = curve_pts[1].first - px;
                tz = curve_pts[1].second - pz;
            }
            else if (i == n - 1)
            {
                tx = px - curve_pts[i - 1].first;
                tz = pz - curve_pts[i - 1].second;
            }
            else
            {
                tx = ((px - curve_pts[i - 1].first) + (curve_pts[i + 1].first - px)) * 0.5;
                tz = ((pz - curve_pts[i - 1].second) + (curve_pts[i + 1].second - pz)) * 0.5;
            }
            double tlen = std::hypot(tx, tz);
            if (tlen > 0) // Windows gen_curved_wall_pts: any non-zero tangent is normalised (also a numerically tiny one)
            {
                tx /= tlen;
                tz /= tlen;
            }
            double nx = -tz, nz = tx;

            double ix = px + nx * half_t, iz = pz + nz * half_t;
            double ox = px - nx * half_t, oz = pz - nz * half_t;
            double y_slope = 0.0;
            if (slope.has_slope && n > 1)
            {
                double t_param = static_cast<double>(i) / (n - 1);
                y_slope = slope.slope_up ? t_param * slope.v_chg : (1.0 - t_param) * slope.v_chg;
            }

            double yb = y_offset + y_slope;
            double yt = y_offset + height + y_slope;

            mesh.vertices[i] = {ix, yb, iz};
            mesh.vertices[n + i] = {ox, yb, oz};
            mesh.vertices[2 * n + i] = {ix, yt, iz};
            mesh.vertices[3 * n + i] = {ox, yt, oz};
        }
        add_curved_faces(mesh, n);

        return mesh;
    }

    ost_geometry::CppMeshData generate_curved_drop_stub_mesh(
        double x1, double z1, double x2, double z2,
        double cx, double cz,
        double thickness, double drop_value,
        double y_offset, double height,
        double rise, double run)
    {
        ost_geometry::CppMeshData mesh;
        if (drop_value == 0.0)
            return mesh;

        auto full_mesh = generate_curved_linear_mesh(x1, z1, x2, z2, cx, cz,
                                                     height, thickness, y_offset,
                                                     rise, run);
        if (!full_mesh.is_valid() || full_mesh.vertices.size() < 8 ||
            (full_mesh.vertices.size() % 4) != 0)
            return mesh;

        int n_full = static_cast<int>(full_mesh.vertices.size() / 4);
        double arc_len = 0.0;
        int k = 0;
        for (int i = n_full - 2; i >= 0; --i)
        {
            const auto &ib0 = full_mesh.vertices[i];
            const auto &ob0 = full_mesh.vertices[n_full + i];
            const auto &ib1 = full_mesh.vertices[i + 1];
            const auto &ob1 = full_mesh.vertices[n_full + i + 1];

            double px0 = (ib0[0] + ob0[0]) * 0.5;
            double pz0 = (ib0[2] + ob0[2]) * 0.5;
            double px1 = (ib1[0] + ob1[0]) * 0.5;
            double pz1 = (ib1[2] + ob1[2]) * 0.5;
            arc_len += std::hypot(px1 - px0, pz1 - pz0);
            if (arc_len >= thickness)
            {
                k = i;
                break;
            }
        }

        int n = n_full - k;
        if (n < 2)
            return mesh;

        double real_drop = std::abs(drop_value);
        mesh.vertices.reserve(static_cast<size_t>(n) * 4);
        for (int j = 0; j < n; ++j)
        {
            int idx = k + j;
            const auto &ib = full_mesh.vertices[idx];
            const auto &ob = full_mesh.vertices[n_full + idx];
            const auto &it = full_mesh.vertices[2 * n_full + idx];
            const auto &ot = full_mesh.vertices[3 * n_full + idx];

            if (drop_value > 0.0)
            {
                mesh.vertices.push_back(it);
                mesh.vertices.push_back(ot);
                mesh.vertices.push_back({it[0], it[1] + real_drop, it[2]});
                mesh.vertices.push_back({ot[0], ot[1] + real_drop, ot[2]});
            }
            else
            {
                mesh.vertices.push_back({ib[0], ib[1] - real_drop, ib[2]});
                mesh.vertices.push_back({ob[0], ob[1] - real_drop, ob[2]});
                mesh.vertices.push_back(ib);
                mesh.vertices.push_back(ob);
            }
        }

        // Reorder from per-sample pushes to the same strip layout used by the
        // curved wall generator: inner bottom, outer bottom, inner top, outer top.
        std::vector<std::array<double, 3>> strips;
        strips.reserve(mesh.vertices.size());
        for (int strip = 0; strip < 4; ++strip)
        {
            for (int j = 0; j < n; ++j)
                strips.push_back(mesh.vertices[static_cast<size_t>(j) * 4 + strip]);
        }
        mesh.vertices = std::move(strips);
        add_curved_faces(mesh, n);
        return mesh;
    }

}
