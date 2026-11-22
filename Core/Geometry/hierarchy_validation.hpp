#pragma once
// Hole / attachment containment validation of the Windows plan view
// (presentation/components/plan_view/components/drag_handler.py: _attachment_position_valid,
// _takeoff_children_valid_for_geometry_changes, _validate_parent_contains_holes, _validate_hole_position,
// _has_child_holes; geometry_utils.py: _attachment_footprint_path, attachment_fits_area).
//
// An area takeoff can own children: area "holes" (backouts) and attachments (count-like footprints). Windows rejects a
// geometry edit, keeping the previous valid geometry, when it would
//   - move a hole out of its parent, or onto a sibling hole or an attachment of the same parent;
//   - leave a hole or an attachment of a parent outside the edited parent;
//   - make an attachment footprint stick out of its parent area or overlap one of the parent's holes.
// The checks run on the vertex / edge drag of an area (validate_hole_position for a hole, validate_parent_contains_holes
// for any area with children), on the move, rotation and flip of selections (takeoff_children_valid_for_geometry_changes
// with the candidate positions and rotations) and on the move / rotation of one attachment (attachment_position_valid).
//
// Everything works on the stored takeoffs and conditions of the project (ProjectData::takeoffs / ::conditions) plus
// candidate positions that are not stored yet, in raw OST units. Platform independent; no UI, no persistence.
#include "../Domain/Condition.hpp"
#include "../Domain/Takeoff.hpp"
#include "polygon_region.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_hierarchy
{
    // takeoff uid -> candidate flat position (the Windows `position_overrides` / `takeoff_positions` dicts).
    using PositionOverrides = std::unordered_map<std::string, std::vector<double>>;
    // takeoff uid -> candidate rotation in radians (the Windows `rotation_overrides` dict).
    using RotationOverrides = std::unordered_map<std::string, double>;

    // Lookup view of one project: uid -> takeoff and uid -> condition, the Windows `_current_takeoffs` /
    // `_current_conditions` dicts. Does not copy or own the data; keep both containers alive and unchanged while the
    // model is in use. A takeoff vector with a repeated uid resolves the uid to its LAST entry (a Python dict).
    class HierarchyModel
    {
    public:
        HierarchyModel(const std::vector<ost_domain::Takeoff> &takeoffs,
                       const std::unordered_map<std::string, ost_domain::Condition> &conditions);

        const std::vector<ost_domain::Takeoff> &takeoffs() const { return takeoffs_; }
        const ost_domain::Takeoff *find_takeoff(const std::string &uid) const;
        const ost_domain::Condition *find_condition(const std::string &uid) const;

    private:
        const std::vector<ost_domain::Takeoff> &takeoffs_;
        const std::unordered_map<std::string, ost_domain::Condition> &conditions_;
        std::unordered_map<std::string, const ost_domain::Takeoff *> by_uid_;
    };

    // Windows _attachment_footprint_path / compute_count_vertices / resolve_point_takeoff_shape: the footprint polygon of
    // an attachment condition at `position` (exactly 2 values) turned by `rotation` radians, in OST units. Empty for a
    // condition that is not an attachment or a position that is not exactly 2 values.
    ost_geom::Polygon attachment_footprint(const ost_domain::Condition &condition,
                                           const std::vector<double> &position, double rotation);

    // Windows attachment_fits_area: the footprint is inside the parent polygon and touches none of the backouts.
    bool attachment_fits_area(const ost_domain::Condition &condition, const std::vector<double> &position,
                              double rotation, const std::vector<double> &parent_position,
                              const std::vector<std::vector<double>> &backout_positions);

    // Windows _attachment_position_valid(takeoff, position, parent_position, takeoff_positions, rotation).
    // False without a parent on the same page, a parent that is not a top level area, or a condition that is not an
    // attachment. `parent_position` replaces the parent's stored position when given; `takeoff_positions` replaces
    // the stored positions of the parent's area children (the backouts) when given; `rotation` replaces
    // takeoff.rotation when given.
    bool attachment_position_valid(const HierarchyModel &model, const ost_domain::Takeoff &takeoff,
                                   const std::vector<double> &position,
                                   const std::vector<double> *parent_position = nullptr,
                                   const PositionOverrides *takeoff_positions = nullptr,
                                   const std::optional<double> &rotation = std::nullopt);

    // Windows _takeoff_children_valid_for_geometry_changes(position_overrides, rotation_overrides): true when, with the
    // candidate positions / rotations applied, every hole of an affected parent lies inside it and clear of its sibling
    // holes and every attachment of an affected parent fits. Affected parents: the parent of each overridden hole or
    // attachment and each overridden area itself. Overrides that touch no hierarchy are always valid.
    bool takeoff_children_valid_for_geometry_changes(const HierarchyModel &model,
                                                     const PositionOverrides &position_overrides,
                                                     const RotationOverrides &rotation_overrides = {});

    // Windows _validate_parent_contains_holes(parent_uid, new_parent_pos): the vertex / edge drag of an area. Every
    // area child must stay inside the new parent shape and every attachment child must still fit.
    bool validate_parent_contains_holes(const HierarchyModel &model, const std::string &parent_uid,
                                        const std::vector<double> &new_parent_pos);

    // Windows _validate_hole_position(takeoff, new_pos): the hole (an area with a parent) at its candidate position is
    // inside its parent, no attachment of the parent is hit and no sibling hole is touched.
    bool validate_hole_position(const HierarchyModel &model, const ost_domain::Takeoff &takeoff,
                                const std::vector<double> &new_pos);

    // Windows _has_child_holes(parent_uid): any takeoff names parent_uid as its parent.
    bool has_child_holes(const HierarchyModel &model, const std::string &parent_uid);
}
