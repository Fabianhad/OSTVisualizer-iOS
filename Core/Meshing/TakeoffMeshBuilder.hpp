#pragma once
// Takeoff -> mesh orchestration shared by the 3D view (Services/ProjectService.mm) and the OBJ / DXF / HTML exports
// (Services/ExportService.mm). Port of the Windows presentation/visualization/meshing/mesh_builder.py
// (process_takeoffs_to_meshes, get_holes_for_takeoff, _apply_page_transform), the hole grouping of
// domain/services/takeoff_domain_service.py (group_area_takeoffs_with_holes) and the page set-up of
// infrastructure/visualization_provider.py (_MeshGeneratorAdapter.generate_meshes).
//
// Frames: positions are raw OST units; meshes are Y-up with (X, Z) = (-x_ost, -y_ost) (the Windows 3D frame
// (X, Y, Z-up) = (-x_ost, y_ost, z) mapped by (X, Y, Z) -> (X, Z, -Y)).
#include "../Data/ProjectData.hpp"
#include "../Domain/Condition.hpp"
#include "../Domain/Takeoff.hpp"
#include "../Geometry/mesh_data.hpp"
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ost_meshing
{

    // Result of group_area_takeoffs_with_holes.
    struct AreaHoleGrouping
    {
        // The takeoffs that are meshed as elements of their own, in the Windows order: every takeoff whose condition is
        // not an area condition (unknown conditions included), then the area takeoffs that are not a hole.
        std::vector<const ost_domain::Takeoff *> takeoffs;
        // parent takeoff uid -> its hole takeoffs, in input order.
        std::unordered_map<std::string, std::vector<const ost_domain::Takeoff *>> holes;
    };

    // Windows takeoff_domain_service.group_area_takeoffs_with_holes(bid_takeoffs, bid_conditions): a takeoff is a hole
    // only if its condition is an AREA condition, it has a parent (parent_uid not "" / "0") and that parent is an AREA
    // takeoff of the SAME list. Everything else (orphans, children of non-area parents, linear / count / attachment
    // children) stays an ordinary takeoff. Without any area takeoff the input list is returned unchanged.
    // `takeoffs` must already be filtered to the selected pages and visible layers (see generate_takeoff_meshes).
    AreaHoleGrouping group_area_takeoffs_with_holes(
        const std::vector<const ost_domain::Takeoff *> &takeoffs,
        const std::unordered_map<std::string, ost_domain::Condition> &conditions);

    // Windows mesh_builder.get_holes_for_takeoff(takeoff_uid, area_holes_map): the raw OST (x, y) rings of the holes of
    // the takeoff. A hole needs at least 6 position values and 3 points (an odd trailing value is ignored); empty when
    // the takeoff has no usable hole.
    std::vector<std::vector<std::pair<double, double>>> holes_for_takeoff(
        const std::string &takeoff_uid,
        const AreaHoleGrouping &grouping);

    // One generated element mesh (before the negative-quantity boolean pass, which stays with the caller).
    struct TakeoffMesh
    {
        const ost_domain::Takeoff *takeoff = nullptr;
        const ost_domain::Condition *condition = nullptr;
        ost_geometry::CppMeshData mesh;
        float color_r = 0, color_g = 0, color_b = 0;
    };

    // Meshes of all takeoffs of `selected_pages` (an empty set means every page, as the export does) whose condition
    // layer is visible. Windows: raw OST units (the page scale factors never scale a mesh), area holes cut into their
    // area parent (same transform as the outline), hole children not meshed on their own.
    // apply_page_transform = true (3D view): each page's rotation / flip_x / flip_y is applied to its meshes
    // (_MeshGeneratorAdapter + _apply_page_transform). false (OBJ / DXF / HTML exports): meshes stay untransformed.
    // Throws std::invalid_argument exactly where Windows raises ValueError (a page with a rotation that is not a
    // multiple of 90 degrees, or a non-finite coordinate, when its first mesh is transformed): the 3D scene then fails.
    std::vector<TakeoffMesh> generate_takeoff_meshes(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        bool apply_page_transform);

}
