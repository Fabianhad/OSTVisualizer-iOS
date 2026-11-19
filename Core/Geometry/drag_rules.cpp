// Port of the Windows edit-drag rules (drag_handler.py compute_new_position / _constrained_edge_resize /
// _polygon_edit_geometry_valid / _positions_meaningfully_different / update_drag_handle_positions; input_handler.py
// _compute_group_translation_positions / _area_child_parent_map / _expanded_takeoff_transform_uids and the release of an
// area body drag). See drag_rules.hpp.
#include "strict_fp.hpp" // first: no fused multiply-add, like the Windows build (see strict_fp.hpp)
#include "drag_rules.hpp"
#include "geom_utils.hpp"
#include "placement_snap.hpp"
#include "../Domain/TakeoffDeletion.hpp"
#include <cmath>
#include <cstddef>

namespace ost_drag
{
    namespace
    {
        // The "same point / zero length" tolerance of every Windows drag helper.
        constexpr double kNanoUnit = 1e-9;

        // Windows _line_line_intersect: the intersection of p1 + t * d1 with the line through p2 along d2; none when the
        // lines are parallel (|det| < 1e-12).
        bool line_line_intersect(double p1x, double p1y, double d1x, double d1y, double p2x, double p2y, double d2x,
                                 double d2y, double &x, double &y)
        {
            const double det = d1x * d2y - d1y * d2x;
            if (std::fabs(det) < 1e-12)
                return false;
            const double t = ((p2x - p1x) * d2y - (p2y - p1y) * d2x) / det;
            x = p1x + d1x * t;
            y = p1y + d1y * t;
            return true;
        }

        // Windows _constrained_edge_resize: the edge ci0 -> ci1 slides along its normal by the projection of the
        // displacement; both corners are re-intersected with the lines of the neighbouring edges, then grid-snapped. A
        // corner whose neighbouring edge is parallel keeps its place; an edge shorter than 1e-9 changes nothing.
        std::vector<double> constrained_edge_resize(std::vector<double> pos, double ost_dx, double ost_dy, int ci0,
                                                    int ci1, int corner_count, double snap_increment)
        {
            const size_t c0 = static_cast<size_t>(ci0) * 2;
            const size_t c1 = static_cast<size_t>(ci1) * 2;
            const double ex = pos[c1] - pos[c0];
            const double ey = pos[c1 + 1] - pos[c0 + 1];
            const double edge_len = std::hypot(ex, ey);
            if (edge_len < kNanoUnit)
                return pos;
            const double nx = -ey / edge_len;
            const double ny = ex / edge_len;
            const double proj = ost_dx * nx + ost_dy * ny;
            const double moved_c0x = pos[c0] + nx * proj;
            const double moved_c0y = pos[c0 + 1] + ny * proj;
            const double moved_c1x = pos[c1] + nx * proj;
            const double moved_c1y = pos[c1 + 1] + ny * proj;
            const size_t prev = static_cast<size_t>((ci0 - 1 + corner_count) % corner_count) * 2;
            const size_t next = static_cast<size_t>((ci1 + 1) % corner_count) * 2;
            double c0x = 0.0, c0y = 0.0, c1x = 0.0, c1y = 0.0;
            const bool has_c0 = line_line_intersect(pos[prev], pos[prev + 1], pos[c0] - pos[prev],
                                                    pos[c0 + 1] - pos[prev + 1], moved_c0x, moved_c0y, ex, ey, c0x, c0y);
            const bool has_c1 = line_line_intersect(pos[next], pos[next + 1], pos[c1] - pos[next],
                                                    pos[c1 + 1] - pos[next + 1], moved_c1x, moved_c1y, ex, ey, c1x, c1y);
            if (has_c0)
            {
                pos[c0] = ost_placement::snap_ost(c0x, snap_increment);
                pos[c0 + 1] = ost_placement::snap_ost(c0y, snap_increment);
            }
            if (has_c1)
            {
                pos[c1] = ost_placement::snap_ost(c1x, snap_increment);
                pos[c1 + 1] = ost_placement::snap_ost(c1y, snap_increment);
            }
            return pos;
        }
    }

    bool positions_meaningfully_different(const std::vector<double> &original, const std::vector<double> &candidate)
    {
        if (original.size() != candidate.size())
            return true;
        for (size_t i = 0; i < original.size(); ++i)
        {
            // math.isclose(a, b, rel_tol=0.0, abs_tol=1e-9)
            if (original[i] == candidate[i])
                continue;
            if (std::isinf(original[i]) || std::isinf(candidate[i]))
                return true;
            if (!(std::fabs(candidate[i] - original[i]) <= kNanoUnit))
                return true;
        }
        return false;
    }

    std::vector<double> compute_new_position(const std::vector<double> &orig_pos, double ost_dx, double ost_dy,
                                             int handle_idx, int corner_count, double snap_increment,
                                             double snap_angle_degrees, bool move_only_first_pair, bool free_mode)
    {
        using ost_placement::snap_angle;
        using ost_placement::snap_ost;

        std::vector<double> pos = orig_pos;
        const size_t n_pairs = pos.size() / 2;
        if (corner_count < 0 || static_cast<size_t>(corner_count) > n_pairs)
            return pos; // not a shape this position can belong to (Windows would raise IndexError)

        if (handle_idx == -1)
        {
            if (pos.size() < 2)
                return pos;
            // The first point is snapped to the grid and the SAME displacement is applied to every point: the shape keeps its size.
            const double snapped_dx = snap_ost(pos[0] + ost_dx, snap_increment) - pos[0];
            const double snapped_dy = snap_ost(pos[1] + ost_dy, snap_increment) - pos[1];
            const size_t limit = move_only_first_pair ? 2 : pos.size() - 1;
            for (size_t i = 0; i < limit; i += 2)
            {
                pos[i] = pos[i] + snapped_dx;
                pos[i + 1] = pos[i + 1] + snapped_dy;
            }
        }
        else if (corner_count > 0 && handle_idx >= corner_count)
        {
            const int edge_i = handle_idx - corner_count;
            if (edge_i >= corner_count)
                return pos; // no such edge handle
            const size_t ci0 = static_cast<size_t>(edge_i);
            const size_t ci1 = static_cast<size_t>((edge_i + 1) % corner_count);
            if (free_mode || corner_count < 3)
            {
                const auto snapped = snap_angle(0.0, 0.0, ost_dx, ost_dy, snap_angle_degrees);
                pos[ci0 * 2] = snap_ost(pos[ci0 * 2] + snapped[0], snap_increment);
                pos[ci0 * 2 + 1] = snap_ost(pos[ci0 * 2 + 1] + snapped[1], snap_increment);
                pos[ci1 * 2] = snap_ost(pos[ci1 * 2] + snapped[0], snap_increment);
                pos[ci1 * 2 + 1] = snap_ost(pos[ci1 * 2 + 1] + snapped[1], snap_increment);
            }
            else
            {
                pos = constrained_edge_resize(pos, ost_dx, ost_dy, edge_i, (edge_i + 1) % corner_count, corner_count,
                                              snap_increment);
            }
        }
        else if (handle_idx == 2 && corner_count == 0 && pos.size() >= 6)
        {
            // The centre (control) handle of a curved linear: not ported (RPV-22 decision b). Leave the shape alone rather
            // than overwrite the control point as if it were a plain vertex.
            return pos;
        }
        else if (handle_idx >= 0 && static_cast<size_t>(handle_idx) < n_pairs)
        {
            const size_t h = static_cast<size_t>(handle_idx);
            double new_x = snap_ost(pos[h * 2] + ost_dx, snap_increment);
            double new_y = snap_ost(pos[h * 2 + 1] + ost_dy, snap_increment);
            if (n_pairs >= 2 && corner_count == 0 && h < 2)
            {
                // the end of a linear snaps its angle against the OTHER end
                const size_t other = 1 - h;
                const auto snapped = snap_angle(pos[other * 2], pos[other * 2 + 1], new_x, new_y, snap_angle_degrees);
                new_x = snapped[0];
                new_y = snapped[1];
            }
            else if (corner_count > 0)
            {
                // an area vertex snaps its angle against the previous vertex
                const size_t prev = static_cast<size_t>((handle_idx - 1 + corner_count) % corner_count);
                const auto snapped = snap_angle(pos[prev * 2], pos[prev * 2 + 1], new_x, new_y, snap_angle_degrees);
                new_x = snapped[0];
                new_y = snapped[1];
            }
            if (corner_count == 0 && pos.size() >= 6 && h < 2)
            {
                // an end of a curved linear (6 or 7 values): the control point keeps its signed perpendicular offset from the chord
                const double old_lx = pos[2] - pos[0];
                const double old_ly = pos[3] - pos[1];
                const double old_len = std::hypot(old_lx, old_ly);
                double perp_dist = 0.0;
                if (old_len > kNanoUnit)
                {
                    const double old_perp_x = -old_ly / old_len;
                    const double old_perp_y = old_lx / old_len;
                    const double old_mx = (pos[0] + pos[2]) / 2.0;
                    const double old_my = (pos[1] + pos[3]) / 2.0;
                    perp_dist = (pos[4] - old_mx) * old_perp_x + (pos[5] - old_my) * old_perp_y;
                }
                pos[h * 2] = new_x;
                pos[h * 2 + 1] = new_y;
                const double new_lx = pos[2] - pos[0];
                const double new_ly = pos[3] - pos[1];
                const double new_len = std::hypot(new_lx, new_ly);
                const double new_mx = (pos[0] + pos[2]) / 2.0;
                const double new_my = (pos[1] + pos[3]) / 2.0;
                if (new_len > kNanoUnit)
                {
                    const double new_perp_x = -new_ly / new_len;
                    const double new_perp_y = new_lx / new_len;
                    pos[4] = new_mx + new_perp_x * perp_dist;
                    pos[5] = new_my + new_perp_y * perp_dist;
                }
                else
                {
                    pos[4] = new_mx;
                    pos[5] = new_my;
                }
            }
            else
            {
                pos[h * 2] = new_x;
                pos[h * 2 + 1] = new_y;
            }
        }
        return pos;
    }

    bool polygon_edit_geometry_valid(const std::vector<double> &new_pos, const std::vector<double> &last_valid_pos)
    {
        std::vector<ost_types::Vec2> points;
        points.reserve(new_pos.size() / 2);
        for (size_t i = 0; i + 1 < new_pos.size(); i += 2)
            points.push_back({new_pos[i], new_pos[i + 1]});
        if (points.size() < 3 || !ost_geom::polygon_is_valid(points))
            return false;
        if (!last_valid_pos.empty())
        {
            const double orig_sign = ost_geom::signed_area(last_valid_pos);
            const double new_sign = ost_geom::signed_area(new_pos);
            if (orig_sign != 0.0 && new_sign != 0.0 && (orig_sign > 0.0) != (new_sign > 0.0))
                return false;
        }
        return true;
    }

    bool drag_candidate_valid(const ost_hierarchy::HierarchyModel &model, const ost_domain::Takeoff &takeoff,
                              const ost_domain::Condition &condition, const std::vector<double> &candidate,
                              int handle_idx, const std::vector<double> &last_valid_pos)
    {
        if (condition.is_attachment())
            return ost_hierarchy::attachment_position_valid(model, takeoff, candidate);
        if (!condition.is_area())
            return true;
        const bool vertex_drag = handle_idx >= 0 && candidate.size() >= 6;
        if (vertex_drag && !polygon_edit_geometry_valid(candidate, last_valid_pos))
            return false;
        if (takeoff.is_hole() && !ost_hierarchy::validate_hole_position(model, takeoff, candidate))
            return false;
        if (vertex_drag && !ost_hierarchy::validate_parent_contains_holes(model, takeoff.uid, candidate))
            return false;
        return true;
    }

    std::vector<double> translate_position(const std::vector<double> &position, double dx, double dy)
    {
        std::vector<double> translated = position;
        for (size_t i = 0; i + 1 < translated.size(); i += 2)
        {
            translated[i] += dx;
            translated[i + 1] += dy;
        }
        return translated;
    }

    std::pair<double, double> snapped_group_delta(double ost_dx, double ost_dy, double snap_increment)
    {
        return {ost_placement::snap_ost(ost_dx, snap_increment), ost_placement::snap_ost(ost_dy, snap_increment)};
    }

    std::set<std::string> expanded_transform_uids(const ost_hierarchy::HierarchyModel &model,
                                                  const std::set<std::string> &selected_uids)
    {
        std::vector<std::string> area_parents;
        for (const std::string &uid : selected_uids)
        {
            const ost_domain::Takeoff *takeoff = model.find_takeoff(uid);
            if (takeoff == nullptr)
                continue;
            const ost_domain::Condition *condition = model.find_condition(takeoff->condition_uid);
            if (condition != nullptr && condition->is_area())
                area_parents.push_back(uid);
        }
        std::set<std::string> expanded = selected_uids;
        const std::set<std::string> descendants =
            ost_domain::expand_takeoff_uids_with_descendants(model.takeoffs(), area_parents);
        expanded.insert(descendants.begin(), descendants.end());
        return expanded;
    }

    std::unordered_map<std::string, std::string> area_child_parent_map(const ost_hierarchy::HierarchyModel &model,
                                                                       const std::set<std::string> &parent_uids,
                                                                       const std::set<std::string> *child_uids)
    {
        const std::set<std::string> expanded = expanded_transform_uids(model, parent_uids);
        const std::set<std::string> &candidates = child_uids != nullptr ? *child_uids : expanded;
        std::unordered_map<std::string, std::string> result;
        for (const std::string &uid : candidates)
        {
            const ost_domain::Takeoff *child = model.find_takeoff(uid);
            if (child == nullptr)
                continue;
            std::string ancestor = child->parent_uid;
            std::set<std::string> seen = {uid};
            while (expanded.count(ancestor) != 0 && seen.count(ancestor) == 0)
            {
                seen.insert(ancestor);
                const ost_domain::Takeoff *parent = model.find_takeoff(ancestor);
                const ost_domain::Condition *condition =
                    parent != nullptr ? model.find_condition(parent->condition_uid) : nullptr;
                if (condition == nullptr || !condition->is_area())
                    break;
                if (parent_uids.count(ancestor) != 0)
                    result[uid] = ancestor;
                ancestor = parent->parent_uid;
            }
        }
        return result;
    }

    PositionMap compute_group_translation_positions(const ost_hierarchy::HierarchyModel &model,
                                                    const PositionMap &orig_positions, double ost_dx, double ost_dy,
                                                    double snap_increment)
    {
        const auto group = snapped_group_delta(ost_dx, ost_dy, snap_increment);
        std::set<std::string> uids;
        for (const auto &entry : orig_positions)
            uids.insert(entry.first);
        const auto child_parent_by_uid = area_child_parent_map(model, uids, &uids);

        PositionMap new_positions;
        for (const auto &entry : orig_positions)
        {
            if (child_parent_by_uid.count(entry.first) != 0)
                continue;
            new_positions[entry.first] = translate_position(entry.second, group.first, group.second);
        }
        for (const auto &link : child_parent_by_uid)
        {
            const auto child_orig = orig_positions.find(link.first);
            const auto parent_orig = orig_positions.find(link.second);
            const auto parent_new = new_positions.find(link.second);
            if (child_orig == orig_positions.end() || parent_orig == orig_positions.end() ||
                parent_new == new_positions.end() || parent_orig->second.size() < 2 || parent_new->second.size() < 2)
                continue;
            const double parent_dx = parent_new->second[0] - parent_orig->second[0];
            const double parent_dy = parent_new->second[1] - parent_orig->second[1];
            new_positions[link.first] = translate_position(child_orig->second, parent_dx, parent_dy);
        }
        if (!ost_hierarchy::takeoff_children_valid_for_geometry_changes(model, new_positions))
            return orig_positions;
        return new_positions;
    }

    std::vector<PositionChange> descendants_following_body_drag(const ost_hierarchy::HierarchyModel &model,
                                                                const std::string &takeoff_uid, int handle_idx,
                                                                const std::vector<double> &orig_position,
                                                                const std::vector<double> &new_position)
    {
        std::vector<PositionChange> changes;
        if (handle_idx != -1)
            return changes;
        const ost_domain::Takeoff *takeoff = model.find_takeoff(takeoff_uid);
        if (takeoff == nullptr)
            return changes;
        const ost_domain::Condition *condition = model.find_condition(takeoff->condition_uid);
        if (condition == nullptr || !condition->is_area())
            return changes;
        if (orig_position.size() < 2 || new_position.size() < 2)
            return changes;
        const double parent_dx = new_position[0] - orig_position[0];
        const double parent_dy = new_position[1] - orig_position[1];
        const std::set<std::string> descendants =
            ost_domain::expand_takeoff_uids_with_descendants(model.takeoffs(), {takeoff_uid});
        for (const ost_domain::Takeoff &child : model.takeoffs())
        {
            if (child.uid == takeoff_uid || descendants.count(child.uid) == 0 || child.position.empty())
                continue;
            PositionChange change;
            change.uid = child.uid;
            change.old_position = child.position;
            change.new_position = translate_position(child.position, parent_dx, parent_dy);
            changes.push_back(std::move(change));
        }
        return changes;
    }
}
