#include "MeshGenerator.hpp"
#include "mesh_helpers.hpp"
#include <vector>
#include <utility>
#include <algorithm>

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

    static std::vector<std::pair<double, double>> generate_curve_points(
        double x1, double z1, double x2, double z2,
        double cx, double cz, int segments)
    {
        std::vector<std::pair<double, double>> pts;
        double m1x = (x1 + cx) * 0.5, m1z = (z1 + cz) * 0.5;
        double m2x = (cx + x2) * 0.5, m2z = (cz + z2) * 0.5;
        double d1x = cx - x1, d1z = cz - z1;
        double d2x = x2 - cx, d2z = z2 - cz;

        bool arc_valid = false;
        double ctr_x = 0, ctr_z = 0, radius = 0;
        bool v1 = std::abs(d1z) < 1e-10;
        bool v2 = std::abs(d2z) < 1e-10;
        double ps1 = v1 ? 0 : -d1x / d1z;
        double ps2 = v2 ? 0 : -d2x / d2z;

        if (v1 && !v2)
        {
            ctr_x = m1x;
            ctr_z = m2z + ps2 * (m1x - m2x);
            arc_valid = true;
        }
        else if (v2 && !v1)
        {
            ctr_x = m2x;
            ctr_z = m1z + ps1 * (m2x - m1x);
            arc_valid = true;
        }
        else if (!v1 && !v2 && std::abs(ps1 - ps2) > 1e-10)
        {
            ctr_x = (m2z - m1z + ps1 * m1x - ps2 * m2x) / (ps1 - ps2);
            ctr_z = m1z + ps1 * (ctr_x - m1x);
            arc_valid = true;
        }

        if (arc_valid)
        {
            double r1 = std::hypot(ctr_x - x1, ctr_z - z1);
            double r2 = std::hypot(ctr_x - x2, ctr_z - z2);
            double rc = std::hypot(ctr_x - cx, ctr_z - cz);

            if (std::abs(r1 - r2) < 0.1 && std::abs(r1 - rc) < 0.1)
            {
                radius = r1;
                double a_s = std::atan2(z1 - ctr_z, x1 - ctr_x);
                double a_e = std::atan2(z2 - ctr_z, x2 - ctr_x);
                double cross1 = (x1 - ctr_x) * (cz - ctr_z) - (z1 - ctr_z) * (cx - ctr_x);
                double cross2 = (cx - ctr_x) * (z2 - ctr_z) - (cz - ctr_z) * (x2 - ctr_x);
                bool clockwise = (cross1 + cross2) < 0;

                if (clockwise)
                {
                    if (a_e > a_s)
                        a_e -= 2.0 * M_PI;
                }
                else
                {
                    if (a_e < a_s)
                        a_e += 2.0 * M_PI;
                }

                double a_diff = a_e - a_s;
                pts.resize(segments + 1);
                for (int i = 0; i <= segments; i++)
                {
                    double t = static_cast<double>(i) / segments;
                    double a = a_s + t * a_diff;
                    pts[i] = {ctr_x + radius * std::cos(a), ctr_z + radius * std::sin(a)};
                }
                return pts;
            }
        }
        pts.resize(segments + 1);
        for (int i = 0; i <= segments; i++)
        {
            double t = static_cast<double>(i) / segments;
            double u = 1.0 - t;
            pts[i] = {u * u * x1 + 2 * u * t * cx + t * t * x2,
                      u * u * z1 + 2 * u * t * cz + t * t * z2};
        }
        return pts;
    }

    static int calc_adaptive_segments(double x1, double z1, double x2, double z2,
                                      double cx, double cz)
    {
        double chord = std::hypot(x2 - x1, z2 - z1);
        double curve_d = (std::hypot(cx - x1, cz - z1) + std::hypot(x2 - cx, z2 - cz)) * 0.8;
        double base = std::max(chord, curve_d) / 5.0;
        base = clamp_val(base, 6.0, 64.0);

        if (chord > 0)
        {
            double dx = x2 - x1, dz = z2 - z1;
            double t = clamp_val(((cx - x1) * dx + (cz - z1) * dz) / (dx * dx + dz * dz), 0.0, 1.0);
            double offset = std::hypot(cx - (x1 + t * dx), cz - (z1 + t * dz));
            base += offset / 5.0;
        }
        return static_cast<int>(clamp_val(base, 6.0, 64.0));
    }

    ost_geometry::CppMeshData generate_curved_linear_mesh(
        double x1, double z1, double x2, double z2,
        double cx, double cz,
        double height, double thickness, double y_offset,
        double rise, double run)
    {
        ost_geometry::CppMeshData mesh;

        int segs = calc_adaptive_segments(x1, z1, x2, z2, cx, cz);
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
            if (tlen > 1e-10)
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
