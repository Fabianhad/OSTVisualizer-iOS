#pragma once
#include "../Geometry/mesh_data.hpp"
#include <vector>
#include <utility>

namespace ost_meshing
{

    ost_geometry::CppMeshData generate_linear_mesh(
        double x1, double z1, double x2, double z2,
        double height, double thickness, double y_offset,
        double rise = 0.0, double run = 0.0);

    // Curve centre line of a curved wall (Windows ost_linear_geom.calc_curve_segs / gen_adv_curve_pts, in the iOS
    // (x, z) frame): adaptive segment count, and segments + 1 points from the start through the control point to the end.
    int calc_curve_segments(
        double x1, double z1, double x2, double z2,
        double cx, double cz);

    std::vector<std::pair<double, double>> generate_curve_points(
        double x1, double z1, double x2, double z2,
        double cx, double cz, int segments);

    ost_geometry::CppMeshData generate_curved_linear_mesh(
        double x1, double z1, double x2, double z2,
        double cx, double cz,
        double height, double thickness, double y_offset,
        double rise = 0.0, double run = 0.0);

    ost_geometry::CppMeshData generate_drop_stub_mesh(
        double x1, double z1, double x2, double z2,
        double thickness, double drop_value,
        double y_offset, double height,
        double rise = 0.0, double run = 0.0);

    ost_geometry::CppMeshData generate_curved_drop_stub_mesh(
        double x1, double z1, double x2, double z2,
        double cx, double cz,
        double thickness, double drop_value,
        double y_offset, double height,
        double rise = 0.0, double run = 0.0);

    ost_geometry::CppMeshData generate_count_mesh(
        double x, double z,
        double width, double depth, double height,
        double y_offset, double rotation = 0.0);

    ost_geometry::CppMeshData generate_cylinder_mesh(
        double x, double z,
        double diameter, double height,
        double y_offset, double rotation = 0.0,
        int segments = 24);

    ost_geometry::CppMeshData generate_elliptical_cylinder_mesh(
        double x, double z,
        double width, double depth, double height,
        double y_offset, double rotation = 0.0,
        int segments = 24);

    ost_geometry::CppMeshData generate_polygon_prism_mesh(
        double x, double z,
        double width, double height, int sides,
        double y_offset, double rotation = 0.0,
        double angle_offset = 0.0);

    ost_geometry::CppMeshData generate_isosceles_triangle_prism_mesh(
        double x, double z,
        double width, double depth, double height,
        double y_offset, double rotation = 0.0);

    ost_geometry::CppMeshData generate_rhombus_prism_mesh(
        double x, double z,
        double width, double height,
        double y_offset, double rotation = 0.0);

    ost_geometry::CppMeshData generate_area_mesh(
        const std::vector<std::pair<double, double>> &vertices,
        double thickness, double y_offset,
        const std::vector<std::vector<std::pair<double, double>>> &holes = {},
        double rise = 0.0, double run = 0.0, double rotation = 0.0);

}
