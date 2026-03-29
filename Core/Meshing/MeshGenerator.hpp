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

    ost_geometry::CppMeshData generate_area_mesh(
        const std::vector<std::pair<double, double>> &vertices,
        double thickness, double y_offset,
        const std::vector<std::vector<std::pair<double, double>>> &holes = {},
        double rise = 0.0, double run = 0.0, double rotation = 0.0);

}
