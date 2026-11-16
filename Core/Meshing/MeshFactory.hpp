#pragma once
#include "../Domain/Takeoff.hpp"
#include "../Domain/Condition.hpp"
#include "../Geometry/mesh_data.hpp"
#include <vector>
#include <utility>

namespace ost_meshing
{

    struct MeshResult
    {
        ost_geometry::CppMeshData mesh;
        float color_r = 0, color_g = 0, color_b = 0;
        bool valid = false;
    };

    // Windows MeshFactory.create_mesh_for_takeoff. scale_ratio multiplies every coordinate and dimension: the Windows
    // pipeline meshes in raw OST units (the page scale ratio never scales a mesh), so callers pass 1.0
    // (TakeoffMeshBuilder.hpp does). `holes` are area hole rings already in the mesh frame (-x, -y), see
    // TakeoffMeshBuilder.hpp for the shared hole grouping.
    MeshResult create_mesh_for_takeoff(
        const ost_domain::Takeoff &takeoff,
        const ost_domain::Condition &condition,
        double scale_ratio = 1.0,
        const std::vector<std::vector<std::pair<double, double>>> &holes = {});

}
