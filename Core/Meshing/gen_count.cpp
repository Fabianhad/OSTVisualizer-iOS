#include "../Geometry/strict_fp.hpp"
#include "MeshGenerator.hpp"
#include "mesh_helpers.hpp"
#include <array>
#include <cmath>
#include <vector>

namespace ost_meshing
{

    ost_geometry::CppMeshData generate_count_mesh(
        double x, double z,
        double width, double depth, double height,
        double y_offset, double rotation)
    {
        ost_geometry::CppMeshData mesh;

        double hw = width / 2.0, hd = depth / 2.0;
        double corners[4][2] = {
            {-hw, -hd}, {hw, -hd}, {hw, hd}, {-hw, hd}};
        if (rotation != 0.0) // Windows calc_count_mesh_verts: `rotation != 0`, no tolerance (a 1e-10 rad turn is applied)
        {
            double cos_r = std::cos(rotation), sin_r = std::sin(rotation);
            for (int ci = 0; ci < 4; ci++)
            {
                double cx = corners[ci][0], cz = corners[ci][1];
                corners[ci][0] = cx * cos_r - cz * sin_r;
                corners[ci][1] = cx * sin_r + cz * cos_r;
            }
        }

        double y_b = y_offset, y_t = y_offset + height;
        mesh.vertices.resize(8);
        for (int i = 0; i < 4; i++)
        {
            double tx = x + corners[i][0], tz = z + corners[i][1];
            mesh.vertices[i] = {tx, y_b, tz};
            mesh.vertices[i + 4] = {tx, y_t, tz};
        }

        add_box_faces(mesh, count_box_windows_to_ios());
        return mesh;
    }

    ost_geometry::CppMeshData generate_cylinder_mesh(
        double x, double z,
        double diameter, double height,
        double y_offset, double rotation,
        int segments)
    {
        ost_geometry::CppMeshData mesh;
        if (segments < 3)
            return mesh;

        double rx = diameter / 2.0;
        double y_b = y_offset, y_t = y_offset + height;

        mesh.vertices.resize(static_cast<size_t>(segments * 2 + 2));

        for (int i = 0; i < segments; i++)
        {
            double a = 2.0 * M_PI * i / segments;
            double px = rx * std::cos(a);
            double pz = rx * std::sin(a);

            if (rotation != 0.0) // Windows calc_circ_pts: `rot != 0`, no tolerance
            {
                double cos_r = std::cos(rotation), sin_r = std::sin(rotation);
                double rpx = px * cos_r - pz * sin_r;
                double rpz = px * sin_r + pz * cos_r;
                px = rpx;
                pz = rpz;
            }

            double tx = x + px, tz = z + pz;
            mesh.vertices[i] = {tx, y_b, tz};
            mesh.vertices[i + segments] = {tx, y_t, tz};
        }

        mesh.vertices[segments * 2] = {x, y_b, z};
        mesh.vertices[segments * 2 + 1] = {x, y_t, z};
        int n = segments;
        int bc = n * 2;
        int tc = n * 2 + 1;

        mesh.faces.reserve(static_cast<size_t>(n * 4));

        for (int i = 0; i < n; i++)
        {
            int j = (i + 1) % n;
            uint32_t ui = static_cast<uint32_t>(i);
            uint32_t uj = static_cast<uint32_t>(j);
            uint32_t uin = static_cast<uint32_t>(i + n);
            uint32_t ujn = static_cast<uint32_t>(j + n);
            uint32_t ubc = static_cast<uint32_t>(bc);
            uint32_t utc = static_cast<uint32_t>(tc);
            // Outward-facing winding (positive signed volume), what the Windows OBJ has for a circle (EXP-13): the points
            // run towards +z with the angle, so the bottom cap (centre, i, j) and the top cap (centre, j, i) are seen
            // from outside counter-clockwise.
            mesh.faces.push_back({ubc, ui, uj});
            mesh.faces.push_back({utc, ujn, uin});
            // The side quad is split along the Windows diagonal (get_radial_mesh_faces: bottom point k to top point k + 1).
            // iOS point i is the Windows point (n - i) % n (the circle runs the other way round in the iOS (x, z) frame),
            // so the Windows diagonal is iOS bottom point j to top point i.
            mesh.faces.push_back({ui, uin, uj});
            mesh.faces.push_back({uj, uin, ujn});
        }

        return mesh;
    }

    // ---- Windows count shapes (core/mesh_generator.py + core/geometry/radial.py + utils/mesh.py) ----
    // The Windows shapes are built in the Windows 3D frame (X, Y) with (x, z) = (X, -Y) on iOS. The footprint offsets
    // are computed exactly as Windows does (Windows corner order, rotated by -rotation) and placed as
    // (x + px, y, z - py): vertex i of every shape is the Windows vertex i mapped by (X, Y, Z) -> (X, Z, -Y), bit for
    // bit, so the Windows vertex order and face tables apply unchanged.
    namespace
    {
        using Offsets = std::vector<std::array<double, 2>>;

        void rotate_offsets(Offsets &offsets, double rot)
        {
            if (rot != 0)
            {
                double cos_r = std::cos(rot), sin_r = std::sin(rot);
                for (auto &p : offsets)
                {
                    double px = p[0], py = p[1];
                    p[0] = px * cos_r - py * sin_r;
                    p[1] = px * sin_r + py * cos_r;
                }
            }
        }

        // calc_isosceles_triangle_mesh_verts / calc_rhombus_mesh_verts: the bottom corners, then the top corners.
        void add_prism_rings(ost_geometry::CppMeshData &mesh, double x, double z,
                             const Offsets &offsets, double height, double y_offset)
        {
            double y_b = y_offset, y_t = y_offset + height;
            for (double y : {y_b, y_t})
                for (const auto &p : offsets)
                    mesh.vertices.push_back({x + p[0], y, z - p[1]});
        }

        // calc_radial_mesh_verts (+ calc_circ_pts) and get_radial_mesh_faces: `segments` points on an ellipse with
        // radii rx / ry (ry == 0 means rx, Windows `r_y or r_x`) starting at angle_offset, the same ring on top, then the
        // two cap centres. The Windows face table winds inward and Windows d1c430a turns it outward with outward_faces
        // (domain/mesh.py:62); iOS stores outward-facing faces directly (positive signed volume: every face (a, b, c) of the
        // table is stored as (a, c, b)): parity.
        ost_geometry::CppMeshData build_radial_prism(double x, double z, double rx, double ry, double height,
                                                     int segments, double y_offset, double rotation,
                                                     double angle_offset)
        {
            ost_geometry::CppMeshData mesh;
            if (segments < 3)
                return mesh;
            if (ry == 0.0)
                ry = rx;
            double rot = -rotation;
            Offsets pts;
            pts.reserve(static_cast<size_t>(segments));
            for (int i = 0; i < segments; i++)
            {
                double a = 2.0 * M_PI * i / segments + angle_offset;
                pts.push_back({rx * std::cos(a), ry * std::sin(a)});
            }
            rotate_offsets(pts, rot);
            double y_b = y_offset, y_t = y_offset + height;
            mesh.vertices.reserve(static_cast<size_t>(segments) * 2 + 2);
            for (double y : {y_b, y_t})
                for (const auto &p : pts)
                    mesh.vertices.push_back({x + p[0], y, z - p[1]});
            mesh.vertices.push_back({x, y_b, z});
            mesh.vertices.push_back({x, y_t, z});

            auto u = [](int v) -> uint32_t
            { return static_cast<uint32_t>(v); };
            int c_b = 2 * segments, c_t = 2 * segments + 1;
            mesh.faces.reserve(static_cast<size_t>(segments) * 4);
            for (int i = 0; i < segments; i++)
            {
                int j = (i + 1) % segments;
                mesh.faces.push_back({u(c_b), u(j), u(i)});
                mesh.faces.push_back({u(c_t), u(segments + i), u(segments + j)});
            }
            for (int i = 0; i < segments; i++)
            {
                int j = (i + 1) % segments;
                mesh.faces.push_back({u(i), u(segments + j), u(segments + i)});
                mesh.faces.push_back({u(i), u(j), u(segments + j)});
            }
            return mesh;
        }
    } // namespace

    ost_geometry::CppMeshData generate_elliptical_cylinder_mesh(
        double x, double z,
        double width, double depth, double height,
        double y_offset, double rotation,
        int segments)
    {
        return build_radial_prism(x, z, width / 2.0, depth / 2.0, height, segments, y_offset, rotation, 0.0);
    }

    ost_geometry::CppMeshData generate_polygon_prism_mesh(
        double x, double z,
        double width, double height, int sides,
        double y_offset, double rotation,
        double angle_offset)
    {
        return build_radial_prism(x, z, width / 2.0, 0.0, height, sides, y_offset, rotation, angle_offset);
    }

    ost_geometry::CppMeshData generate_isosceles_triangle_prism_mesh(
        double x, double z,
        double width, double depth, double height,
        double y_offset, double rotation)
    {
        ost_geometry::CppMeshData mesh;
        double hw = width / 2.0, hd = depth / 2.0;
        Offsets corners = {{0.0, -hd}, {hw, hd}, {-hw, hd}};
        rotate_offsets(corners, -rotation);
        add_prism_rings(mesh, x, z, corners, height, y_offset);
        // get_triangle_mesh_faces
        mesh.faces = {
            {0, 2, 1},
            {3, 4, 5},
            {0, 1, 4},
            {0, 4, 3},
            {1, 2, 5},
            {1, 5, 4},
            {2, 0, 3},
            {2, 3, 5},
        };
        return mesh;
    }

    ost_geometry::CppMeshData generate_rhombus_prism_mesh(
        double x, double z,
        double width, double height,
        double y_offset, double rotation)
    {
        ost_geometry::CppMeshData mesh;
        double hw = width / 2.0;
        double horizontal_scale = 0.575;
        double h_half = hw * horizontal_scale;
        Offsets corners = {{0.0, -hw}, {h_half, 0.0}, {0.0, hw}, {-h_half, 0.0}};
        rotate_offsets(corners, -rotation);
        add_prism_rings(mesh, x, z, corners, height, y_offset);
        // get_rhombus_mesh_faces
        mesh.faces = {
            {0, 3, 2},
            {0, 2, 1},
            {4, 5, 6},
            {4, 6, 7},
            {0, 1, 5},
            {0, 5, 4},
            {1, 2, 6},
            {1, 6, 5},
            {2, 3, 7},
            {2, 7, 6},
            {3, 0, 4},
            {3, 4, 7},
        };
        return mesh;
    }

}
