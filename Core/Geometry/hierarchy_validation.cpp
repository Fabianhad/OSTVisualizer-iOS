#include "hierarchy_validation.hpp"
#include "takeoff_footprint.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace ost_hierarchy
{
    using ost_domain::Condition;
    using ost_domain::Takeoff;
    using ost_geom::closed_polygon;
    using ost_geom::Polygon;

    namespace
    {
        // Windows polygon path of a stored flat position (position_polygon_path with the identity coordinate system).
        Polygon polygon_of(const std::vector<double> &position) { return closed_polygon(position); }

        const std::vector<double> &position_or_stored(const PositionOverrides *overrides, const Takeoff &takeoff)
        {
            if (overrides)
            {
                auto it = overrides->find(takeoff.uid);
                if (it != overrides->end())
                    return it->second;
            }
            return takeoff.position;
        }
    } // namespace

    HierarchyModel::HierarchyModel(const std::vector<Takeoff> &takeoffs,
                                   const std::unordered_map<std::string, Condition> &conditions)
        : takeoffs_(takeoffs), conditions_(conditions)
    {
        by_uid_.reserve(takeoffs_.size());
        for (const Takeoff &takeoff : takeoffs_)
            by_uid_[takeoff.uid] = &takeoff;
    }

    const Takeoff *HierarchyModel::find_takeoff(const std::string &uid) const
    {
        auto it = by_uid_.find(uid);
        return it == by_uid_.end() ? nullptr : it->second;
    }

    const Condition *HierarchyModel::find_condition(const std::string &uid) const
    {
        auto it = conditions_.find(uid);
        return it == conditions_.end() ? nullptr : &it->second;
    }

    Polygon attachment_footprint(const Condition &condition, const std::vector<double> &position, double rotation)
    {
        if (!condition.is_attachment() || position.size() != 2)
            return {};
        // takeoff_geometry.resolve_point_takeoff_shape (an attachment is not scaled by the display size) and
        // compute_count_vertices (Core/Geometry/takeoff_footprint.hpp).
        const ost_footprint::PointShape shape = ost_footprint::resolve_point_takeoff_shape(condition);
        return ost_footprint::count_vertices(position[0], position[1], shape.shape_id, shape.width, shape.depth, rotation);
    }

    bool attachment_fits_area(const Condition &condition, const std::vector<double> &position, double rotation,
                              const std::vector<double> &parent_position,
                              const std::vector<std::vector<double>> &backout_positions)
    {
        const Polygon footprint = attachment_footprint(condition, position, rotation);
        const Polygon parent = polygon_of(parent_position);
        std::vector<Polygon> exclusions;
        exclusions.reserve(backout_positions.size());
        for (const auto &backout : backout_positions)
            exclusions.push_back(polygon_of(backout));
        return ost_geom::path_fits_inside_excluding(footprint, parent, exclusions);
    }

    bool attachment_position_valid(const HierarchyModel &model, const Takeoff &takeoff,
                                   const std::vector<double> &position, const std::vector<double> *parent_position,
                                   const PositionOverrides *takeoff_positions, const std::optional<double> &rotation)
    {
        const Takeoff *parent = model.find_takeoff(takeoff.parent_uid);
        if (parent == nullptr || parent->page_uid != takeoff.page_uid)
            return false;
        const Condition *parent_condition = model.find_condition(parent->condition_uid);
        const Condition *condition = model.find_condition(takeoff.condition_uid);
        if (parent_condition == nullptr || !parent_condition->is_area() || parent->is_hole() ||
            condition == nullptr || !condition->is_attachment())
            return false;
        const std::vector<double> &parent_pos = parent_position ? *parent_position : parent->position;
        std::vector<std::vector<double>> backout_positions;
        for (const Takeoff &child : model.takeoffs())
        {
            if (child.parent_uid != parent->uid)
                continue;
            const Condition *child_condition = model.find_condition(child.condition_uid);
            if (child_condition == nullptr || !child_condition->is_area())
                continue;
            backout_positions.push_back(position_or_stored(takeoff_positions, child));
        }
        return attachment_fits_area(*condition, position, rotation ? *rotation : takeoff.rotation, parent_pos,
                                    backout_positions);
    }

    bool takeoff_children_valid_for_geometry_changes(const HierarchyModel &model,
                                                     const PositionOverrides &position_overrides,
                                                     const RotationOverrides &rotation_overrides)
    {
        std::unordered_set<std::string> affected_parent_uids;
        auto collect = [&](const std::string &uid)
        {
            const Takeoff *takeoff = model.find_takeoff(uid);
            if (takeoff == nullptr)
                return;
            const Condition *condition = model.find_condition(takeoff->condition_uid);
            if (condition == nullptr)
                return;
            if (condition->is_attachment() || (condition->is_area() && takeoff->is_hole()))
            {
                if (takeoff->is_hole())
                    affected_parent_uids.insert(takeoff->parent_uid);
            }
            if (condition->is_area())
                affected_parent_uids.insert(takeoff->uid);
        };
        for (const auto &entry : position_overrides)
            collect(entry.first);
        for (const auto &entry : rotation_overrides)
            collect(entry.first);
        if (affected_parent_uids.empty())
            return true;

        std::unordered_map<std::string, std::vector<Polygon>> sibling_paths;
        for (const Takeoff &child : model.takeoffs())
        {
            if (affected_parent_uids.count(child.parent_uid) == 0)
                continue;
            const Condition *condition = model.find_condition(child.condition_uid);
            if (condition == nullptr)
                return false;
            const Takeoff *parent = model.find_takeoff(child.parent_uid);
            const Condition *parent_condition = parent ? model.find_condition(parent->condition_uid) : nullptr;
            if (parent == nullptr || parent->page_uid != child.page_uid || parent_condition == nullptr ||
                !parent_condition->is_area())
                return false;
            if (condition->is_area())
            {
                const Polygon child_path = polygon_of(position_or_stored(&position_overrides, child));
                const Polygon parent_path = polygon_of(position_or_stored(&position_overrides, *parent));
                std::vector<Polygon> &siblings = sibling_paths[parent->uid];
                if (!ost_geom::path_is_inside(child_path, parent_path) ||
                    ost_geom::path_intersects_any(child_path, siblings))
                    return false;
                siblings.push_back(child_path);
                continue;
            }
            if (condition->is_attachment())
            {
                auto parent_override = position_overrides.find(child.parent_uid);
                const std::vector<double> *parent_position =
                    parent_override == position_overrides.end() ? nullptr : &parent_override->second;
                auto rotation_override = rotation_overrides.find(child.uid);
                const double rotation =
                    rotation_override == rotation_overrides.end() ? child.rotation : rotation_override->second;
                if (!attachment_position_valid(model, child, position_or_stored(&position_overrides, child),
                                               parent_position, &position_overrides, rotation))
                    return false;
            }
        }
        return true;
    }

    bool validate_parent_contains_holes(const HierarchyModel &model, const std::string &parent_uid,
                                        const std::vector<double> &new_parent_pos)
    {
        const Polygon parent_path = polygon_of(new_parent_pos);
        for (const Takeoff &child : model.takeoffs())
        {
            if (child.parent_uid != parent_uid)
                continue;
            const Condition *condition = model.find_condition(child.condition_uid);
            if (condition != nullptr && condition->is_attachment())
            {
                if (!attachment_position_valid(model, child, child.position, &new_parent_pos))
                    return false;
                continue;
            }
            if (condition == nullptr || !condition->is_area())
                continue;
            if (child.position.size() < 6)
                continue;
            if (!ost_geom::path_is_inside(polygon_of(child.position), parent_path))
                return false;
        }
        return true;
    }

    bool validate_hole_position(const HierarchyModel &model, const Takeoff &takeoff,
                                const std::vector<double> &new_pos)
    {
        const Takeoff *parent = model.find_takeoff(takeoff.parent_uid);
        if (parent == nullptr)
            return false;
        if (parent->position.size() < 6)
            return false;
        const Polygon parent_path = polygon_of(parent->position);
        const Polygon new_hole_path = polygon_of(new_pos);
        if (!ost_geom::path_is_inside(new_hole_path, parent_path))
            return false;
        const PositionOverrides position_overrides = {{takeoff.uid, new_pos}};
        for (const Takeoff &child : model.takeoffs())
        {
            const Condition *child_condition = model.find_condition(child.condition_uid);
            if (child.parent_uid == takeoff.parent_uid && child_condition != nullptr &&
                child_condition->is_attachment() &&
                !attachment_position_valid(model, child, child.position, nullptr, &position_overrides))
                return false;
        }
        std::vector<Polygon> sibling_paths;
        for (const Takeoff &sibling : model.takeoffs())
        {
            if (sibling.parent_uid != takeoff.parent_uid || sibling.uid == takeoff.uid)
                continue;
            const Condition *sibling_condition = model.find_condition(sibling.condition_uid);
            if (sibling_condition == nullptr || !sibling_condition->is_area())
                continue;
            if (sibling.position.size() < 6)
                continue;
            sibling_paths.push_back(polygon_of(sibling.position));
        }
        return !ost_geom::path_intersects_any(new_hole_path, sibling_paths);
    }

    bool has_child_holes(const HierarchyModel &model, const std::string &parent_uid)
    {
        for (const Takeoff &takeoff : model.takeoffs())
            if (takeoff.parent_uid == parent_uid)
                return true;
        return false;
    }
}
