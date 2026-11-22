#pragma once
#include "../Domain/Takeoff.hpp"
#include "../Domain/Condition.hpp"
#include "../Geometry/mesh_data.hpp"
#include <functional>
#include <optional>
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

    // The union of a wall and its drop stub (Windows boolean_union(mesh, stub, ...) =
    // ost_geometry::boolean_union + remove_degenerate_faces, module_geometry.cpp boolean_union_py). A function with
    // this signature can replace it (the host tests do).
    using DropStubUnionFn = std::function<std::optional<ost_geometry::CppMeshData>(
        const ost_geometry::CppMeshData &wall, const ost_geometry::CppMeshData &stub)>;

    // Windows MeshFactory._union_with_drop_stub(takeoff, mesh, stub): the wall united with its drop stub. A union that
    // throws, has no value or has no faces (also after remove_degenerate_faces removed every face) keeps the plain wall
    // mesh and logs exactly one warning naming the takeoff uid through ost_log (CoreLog.hpp): "Drop stub union raised
    // for takeoff <uid>; keeping the wall mesh" (with the exception text) or "Drop stub union failed for takeoff <uid>;
    // keeping the wall mesh". A good union is returned as it is, without a log. An empty union_fn is the real union.
    // create_mesh_for_takeoff calls it for straight and curved walls.
    ost_geometry::CppMeshData union_with_drop_stub(
        const ost_domain::Takeoff &takeoff,
        ost_geometry::CppMeshData wall,
        const ost_geometry::CppMeshData &stub,
        const DropStubUnionFn &union_fn = {});

}
