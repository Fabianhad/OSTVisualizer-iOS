#include "../Geometry/strict_fp.hpp"
#include "MeshGenerator.hpp"
#include "mesh_helpers.hpp"

namespace ost_meshing
{

    ost_geometry::CppMeshData generate_linear_mesh(
        double x1, double z1, double x2, double z2,
        double height, double thickness, double y_offset,
        double rise, double run)
    {
        ost_geometry::CppMeshData mesh;

        double dx = x2 - x1, dz = z2 - z1;
        double h_len = ost_fp::hypot(dx, dz);
        if (h_len < 0.001)
            return mesh;

        double dx_n = dx / h_len, dz_n = dz / h_len;
        double px = -dz_n * thickness * 0.5;
        double pz = dx_n * thickness * 0.5;

        auto slope = compute_slope(rise, run, h_len);
        double yb_s = y_offset, yb_e = y_offset;
        double yt_s = y_offset + height, yt_e = y_offset + height;

        if (slope.has_slope)
        {
            if (slope.slope_up)
            {
                yb_e += slope.v_chg;
                yt_e += slope.v_chg;
            }
            else
            {
                yb_s += slope.v_chg;
                yt_s += slope.v_chg;
            }
        }

        mesh.vertices = {
            {x1 + px, yb_s, z1 + pz},
            {x1 - px, yb_s, z1 - pz},
            {x2 - px, yb_e, z2 - pz},
            {x2 + px, yb_e, z2 + pz},
            {x1 + px, yt_s, z1 + pz},
            {x1 - px, yt_s, z1 - pz},
            {x2 - px, yt_e, z2 - pz},
            {x2 + px, yt_e, z2 + pz},
        };

        add_box_faces(mesh, wall_windows_to_ios());
        return mesh;
    }

    ost_geometry::CppMeshData generate_drop_stub_mesh(
        double x1, double z1, double x2, double z2,
        double thickness, double drop_value,
        double y_offset, double height,
        double rise, double run)
    {
        ost_geometry::CppMeshData mesh;
        if (drop_value == 0.0)
            return mesh;

        double dx = x2 - x1, dz = z2 - z1;
        double seg_len = ost_fp::hypot(dx, dz);
        if (seg_len < 0.001)
            return mesh;

        double dx_n = dx / seg_len, dz_n = dz / seg_len;
        double half_t = thickness * 0.5;

        double endpoint_v_chg = 0.0;
        double slope_over_stub = 0.0;
        bool slope_up = true;
        if (rise != 0.0 && run != 0.0)
        {
            double v_chg = seg_len * std::abs(rise) / std::abs(run);
            slope_up = (rise > 0 && run > 0) || (rise < 0 && run < 0);
            endpoint_v_chg = slope_up ? v_chg : 0.0;
            slope_over_stub = thickness * std::abs(rise) / std::abs(run);
        }

        double px = -dz_n * half_t;
        double pz = dx_n * half_t;
        double sx = x2 - dx_n * thickness;
        double sz = z2 - dz_n * thickness;
        double ex = x2;
        double ez = z2;

        double y_extra_start = slope_up ? endpoint_v_chg - slope_over_stub
                                        : slope_over_stub;
        double y_extra_end = endpoint_v_chg;
        double real_drop = std::abs(drop_value);

        double yb_s = 0.0, yb_e = 0.0;
        double yt_s = 0.0, yt_e = 0.0;
        if (drop_value > 0.0)
        {
            yb_s = y_offset + y_extra_start + height;
            yt_s = yb_s + real_drop;
            yb_e = y_offset + y_extra_end + height;
            yt_e = yb_e + real_drop;
        }
        else
        {
            yt_s = y_offset + y_extra_start;
            yb_s = yt_s - real_drop;
            yt_e = y_offset + y_extra_end;
            yb_e = yt_e - real_drop;
        }

        mesh.vertices = {
            {sx + px, yb_s, sz + pz},
            {sx - px, yb_s, sz - pz},
            {ex - px, yb_e, ez - pz},
            {ex + px, yb_e, ez + pz},
            {sx + px, yt_s, sz + pz},
            {sx - px, yt_s, sz - pz},
            {ex - px, yt_e, ez - pz},
            {ex + px, yt_e, ez + pz},
        };

        add_box_faces(mesh, wall_windows_to_ios());
        return mesh;
    }

}
