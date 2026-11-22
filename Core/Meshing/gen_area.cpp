#include "../Geometry/strict_fp.hpp"
#include "MeshGenerator.hpp"
#include "mesh_helpers.hpp"
#include "../Geometry/earcut_impl.hpp"
#include <array>
#include <algorithm>

namespace ost_meshing
{

    static double calc_signed_area(const std::vector<std::pair<double, double>> &verts)
    {
        double sum = 0;
        size_t n = verts.size();
        for (size_t i = 0; i < n; i++)
        {
            size_t j = (i + 1) % n;
            sum += verts[i].first * verts[j].second - verts[j].first * verts[i].second;
        }
        return sum * 0.5;
    }

    // Windows area.py calc_z: the slope always rises along +proj; the sign of rise / run is ignored (only the takeoff
    // rotation picks the direction). Windows works in the 3D frame (X, Y) = (x, -z): its Y term (y - cy) * sin is
    // -((z - cz) * sin) here, so the projection is a subtraction (exact, negation commutes with the arithmetic).
    static double slope_y_for_vertex(double x, double z,
                                     double cx, double cz, double cos_r, double sin_r,
                                     double min_proj, double span, double slope_ratio,
                                     double y_base, double thickness)
    {
        double proj = (x - cx) * cos_r - (z - cz) * sin_r;
        double progress = (span > 0) ? (proj - min_proj) / span : 0.0;
        return y_base + thickness + span * progress * slope_ratio;
    }

    ost_geometry::CppMeshData generate_area_mesh(
        const std::vector<std::pair<double, double>> &vertices,
        double thickness, double y_offset,
        const std::vector<std::vector<std::pair<double, double>>> &holes,
        double rise, double run, double rotation)
    {
        ost_geometry::CppMeshData mesh;
        size_t n = vertices.size();
        if (n < 3)
            return mesh;

        double y_b = y_offset;
        bool has_slope = (run != 0.0 && rise != 0.0);
        double cx = 0, cz = 0, cos_r = 1, sin_r = 0;
        double min_proj = 0, span = 0, slope_ratio = 0;

        if (has_slope)
        {
            double x_min = 1e30, x_max = -1e30, z_min = 1e30, z_max = -1e30;
            for (const auto &v : vertices)
            {
                x_min = std::min(x_min, v.first);
                x_max = std::max(x_max, v.first);
                z_min = std::min(z_min, v.second);
                z_max = std::max(z_max, v.second);
            }
            cx = (x_min + x_max) * 0.5;
            cz = (z_min + z_max) * 0.5;
            double rot = -rotation;
            cos_r = std::cos(rot);
            sin_r = std::sin(rot);

            min_proj = 1e30;
            double max_proj = -1e30;
            for (const auto &v : vertices)
            {
                double proj = (v.first - cx) * cos_r - (v.second - cz) * sin_r;
                min_proj = std::min(min_proj, proj);
                max_proj = std::max(max_proj, proj);
            }
            span = max_proj - min_proj;
            slope_ratio = std::abs(rise) / std::abs(run);
        }

        size_t total_outer = n;
        size_t total_holes_verts = 0;
        for (const auto &h : holes)
            total_holes_verts += h.size();
        size_t total_verts = (total_outer + total_holes_verts) * 2;

        mesh.vertices.resize(total_verts);
        for (size_t i = 0; i < n; i++)
        {
            double x = vertices[i].first, z = vertices[i].second;
            mesh.vertices[i] = {x, y_b, z};
            double y_t = has_slope
                             ? slope_y_for_vertex(x, z, cx, cz, cos_r, sin_r, min_proj, span, slope_ratio, y_offset, thickness)
                             : y_offset + thickness;
            mesh.vertices[i + n] = {x, y_t, z};
        }
        size_t h_offset = n * 2;
        std::vector<size_t> hole_bot_starts, hole_top_starts;
        std::vector<size_t> hole_sizes;
        for (const auto &h : holes)
        {
            size_t hn = h.size();
            hole_sizes.push_back(hn);
            hole_bot_starts.push_back(h_offset);
            hole_top_starts.push_back(h_offset + hn);
            for (size_t i = 0; i < hn; i++)
            {
                double x = h[i].first, z = h[i].second;
                mesh.vertices[h_offset + i] = {x, y_b, z};
                double y_t = has_slope
                                 ? slope_y_for_vertex(x, z, cx, cz, cos_r, sin_r, min_proj, span, slope_ratio, y_offset, thickness)
                                 : y_offset + thickness;
                mesh.vertices[h_offset + hn + i] = {x, y_t, z};
            }
            h_offset += hn * 2;
        }

        // Windows hands earcut the 3D footprint (X, Y) = (x, -z) (area.py triangulate); ear selection and the diagonals
        // depend on the orientation, so the mirrored (x, z) frame would pick other triangles. Earcut's output has a
        // fixed orientation in the frame it is given, i.e. the opposite of the (x, z) one: the caps below reverse it to
        // keep the outward-facing winding of the rest of the shell.
        std::vector<std::vector<std::array<double, 2>>> polygon;
        polygon.resize(1 + holes.size());
        polygon[0].resize(n);
        for (size_t i = 0; i < n; i++)
            polygon[0][i] = {vertices[i].first, -vertices[i].second};
        for (size_t hi = 0; hi < holes.size(); hi++)
        {
            polygon[1 + hi].resize(holes[hi].size());
            for (size_t i = 0; i < holes[hi].size(); i++)
                polygon[1 + hi][i] = {holes[hi][i].first, -holes[hi][i].second};
        }

        std::vector<uint32_t> indices = ost_geometry::triangulate_polygon(polygon);

        auto remap_earcut_idx = [&](uint32_t ei) -> uint32_t
        {
            if (ei < n)
                return static_cast<uint32_t>(ei);
            size_t rem = ei - n;
            for (size_t hi = 0; hi < holes.size(); hi++)
            {
                if (rem < hole_sizes[hi])
                    return static_cast<uint32_t>(hole_bot_starts[hi] + rem);
                rem -= hole_sizes[hi];
            }
            return ei;
        };

        auto remap_earcut_idx_top = [&](uint32_t ei) -> uint32_t
        {
            if (ei < n)
                return static_cast<uint32_t>(ei + n);
            size_t rem = ei - n;
            for (size_t hi = 0; hi < holes.size(); hi++)
            {
                if (rem < hole_sizes[hi])
                    return static_cast<uint32_t>(hole_top_starts[hi] + rem);
                rem -= hole_sizes[hi];
            }
            return ei;
        };

        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            mesh.faces.push_back({remap_earcut_idx(indices[i + 2]),
                                  remap_earcut_idx(indices[i + 1]),
                                  remap_earcut_idx(indices[i])});
        }
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            mesh.faces.push_back({remap_earcut_idx_top(indices[i]),
                                  remap_earcut_idx_top(indices[i + 1]),
                                  remap_earcut_idx_top(indices[i + 2])});
        }

        bool is_ccw = calc_signed_area(vertices) > 0;
        auto add_side_walls = [&](size_t b_start, size_t t_start, size_t count, bool ccw)
        {
            for (size_t i = 0; i < count; i++)
            {
                size_t j = (i + 1) % count;
                uint32_t bi = static_cast<uint32_t>(b_start + i);
                uint32_t bj = static_cast<uint32_t>(b_start + j);
                uint32_t ti = static_cast<uint32_t>(t_start + i);
                uint32_t tj = static_cast<uint32_t>(t_start + j);
                if (ccw)
                {
                    mesh.faces.push_back({bi, ti, tj});
                    mesh.faces.push_back({bi, tj, bj});
                }
                else
                {
                    mesh.faces.push_back({bi, bj, tj});
                    mesh.faces.push_back({bi, tj, ti});
                }
            }
        };

        add_side_walls(0, n, n, is_ccw);
        for (size_t hi = 0; hi < holes.size(); hi++)
        {
            bool h_ccw = calc_signed_area(holes[hi]) > 0;
            add_side_walls(hole_bot_starts[hi], hole_top_starts[hi], hole_sizes[hi], !h_ccw);
        }

        return mesh;
    }

}
