#pragma once
// The edit-drag rules of the Windows plan view (presentation/components/plan_view/components/drag_handler.py:
// compute_new_position, _constrained_edge_resize, _polygon_edit_geometry_valid, _positions_meaningfully_different,
// update_drag_handle_positions; input_handler.py: _compute_group_translation_positions, _area_child_parent_map,
// _expanded_takeoff_transform_uids and the child translation of the area body drag).
//
// The platform layer turns a finger movement into an OST displacement (ost_coord::canvas_delta_to_ost) and calls these
// functions with the position the drag started from; everything that decides WHERE the shape ends up lives here:
//   - which vertices move, how grid and angle snapping apply (corner, edge, whole shape, end of a curved linear);
//   - when a candidate is rejected and the previous valid geometry kept (polygon validity, winding, hole / attachment
//     containment through ost_hierarchy);
//   - how a group of takeoffs is translated (snapped delta, children follow their selected area ancestor, the whole move
//     rejected when a child would become invalid) and which descendants follow an area dragged by its body;
//   - when a drag counts as a change at all (1e-9 absolute tolerance).
// Handle index convention (Windows): -1 = the whole shape; 0..n-1 = a vertex; n..2n-1 = the midpoint handle of edge i-n
// (areas only, n = corner_count = number of vertices of the area, 0 for every other shape).
// Platform independent: no UIKit, no Foundation.
#include "hierarchy_validation.hpp"
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ost_drag
{
    using PositionMap = std::unordered_map<std::string, std::vector<double>>;

    // One takeoff whose position a gesture changed: the entry of the history and of the batch write.
    struct PositionChange
    {
        std::string uid;
        std::vector<double> old_position;
        std::vector<double> new_position;
    };

    // Windows _positions_meaningfully_different: a length mismatch is different; otherwise any value that differs by more
    // than 1e-9 (absolute) is. Two empty lists are equal.
    bool positions_meaningfully_different(const std::vector<double> &original, const std::vector<double> &candidate);

    // Windows compute_new_position: the position after dragging handle `handle_idx` of the shape that STARTED at
    // `orig_pos` by the total OST displacement (ost_dx, ost_dy). The input is never modified.
    //   handle -1        every vertex moves by the displacement of the first vertex after grid snapping (the shape size is
    //                    kept); trailing values (odd length) stay; `move_only_first_pair` moves the first pair only;
    //   area edge        (corner_count >= 3 and not `free_mode`) the edge slides along its normal and its two corners are
    //                    re-intersected with the neighbouring edges (_constrained_edge_resize), then grid-snapped;
    //                    (`free_mode` or fewer than 3 corners) both corners move by the angle-snapped displacement, each
    //                    grid-snapped;
    //   linear end       the new end is grid-snapped, then angle-snapped about the opposite end; a curved linear (6 or 7
    //                    values) keeps its control point at the same perpendicular offset from the chord (RPV-M03);
    //   area vertex      grid-snapped, then angle-snapped about the previous vertex;
    //   other vertex     (count, later points of a multi-point shape) grid-snapped, no angle snap.
    // A handle that does not exist leaves the position unchanged. NOT PORTED: the centre handle of a curved linear
    // (Windows handle 2 of a 6/7 value linear; RPV-22 decision b), which leaves the position unchanged here.
    // `snap_angle_degrees` <= 0 disables the angle snap (Windows: Shift held with the default configuration).
    // A position with fewer than two values is returned unchanged for the whole-shape drag (Windows raises IndexError).
    std::vector<double> compute_new_position(const std::vector<double> &orig_pos, double ost_dx, double ost_dy,
                                             int handle_idx, int corner_count, double snap_increment,
                                             double snap_angle_degrees, bool move_only_first_pair = false,
                                             bool free_mode = false);

    // Windows _polygon_edit_geometry_valid(new_pos, area_pts): at least three vertices, a valid (simple, non-degenerate)
    // polygon and, when `last_valid_pos` is not empty, the same winding as the last valid position (signed areas of
    // opposite sign reject the candidate; a zero area on either side skips the winding check).
    bool polygon_edit_geometry_valid(const std::vector<double> &new_pos, const std::vector<double> &last_valid_pos);

    // The validity chain of Windows update_drag_handle_positions for ONE candidate position of the dragged takeoff:
    //   attachment condition   _attachment_position_valid(takeoff, candidate);
    //   area condition         a vertex or edge drag (handle >= 0, at least 6 values) must pass polygon_edit_geometry_valid
    //                          against `last_valid_pos`; a hole (area with a parent) must pass _validate_hole_position for
    //                          every kind of drag; a vertex or edge drag must also pass _validate_parent_contains_holes;
    //   any other condition    always valid (a linear is never rejected).
    // `takeoff` is the dragged takeoff as stored (its stored position is not used: the candidate replaces it).
    bool drag_candidate_valid(const ost_hierarchy::HierarchyModel &model, const ost_domain::Takeoff &takeoff,
                              const ost_domain::Condition &condition, const std::vector<double> &candidate,
                              int handle_idx, const std::vector<double> &last_valid_pos);

    // Windows _translate_position: every complete pair moves by (dx, dy); a trailing value stays.
    std::vector<double> translate_position(const std::vector<double> &position, double dx, double dy);

    // Windows _snapped_group_translation_delta: both axes of a group displacement are rounded to the snap increment
    // (increment <= 0: unchanged).
    std::pair<double, double> snapped_group_delta(double ost_dx, double ost_dy, double snap_increment);

    // Windows _expanded_takeoff_transform_uids: the selected uids plus every descendant (any depth) of the selected area
    // takeoffs.
    std::set<std::string> expanded_transform_uids(const ost_hierarchy::HierarchyModel &model,
                                                  const std::set<std::string> &selected_uids);

    // Windows _area_child_parent_map(parent_uids, child_uids): for every candidate takeoff (child_uids, or every
    // expanded uid when it is null) the selected area ancestor it follows (the highest one when several are selected);
    // takeoffs that follow nobody are absent.
    std::unordered_map<std::string, std::string> area_child_parent_map(const ost_hierarchy::HierarchyModel &model,
                                                                       const std::set<std::string> &parent_uids,
                                                                       const std::set<std::string> *child_uids = nullptr);

    // Windows _compute_group_translation_positions (takeoffs): every takeoff of `orig_positions` that does not follow a
    // selected area ancestor moves by the snapped group delta; the followers move by the actual displacement of their
    // ancestor's first point; when the moved geometry would leave a hole outside its parent, overlap sibling holes or push
    // an attachment out of its area (takeoff_children_valid_for_geometry_changes) the original positions are returned.
    PositionMap compute_group_translation_positions(const ost_hierarchy::HierarchyModel &model,
                                                    const PositionMap &orig_positions, double ost_dx, double ost_dy,
                                                    double snap_increment);

    // Windows release of a drag (input_handler.py, mouseReleaseEvent): when the dragged takeoff is an AREA dragged by its
    // body (handle -1), every descendant of the area is translated by the displacement of the area's first point
    // (new - original) and committed with it; a corner or edge drag, and a parent that is not an area, move no child. One
    // entry per descendant, in the order of the project's takeoff list; descendants without a position are skipped (there
    // is nothing to move; Windows would write an empty position). Empty when the area has fewer than two values before or
    // after the drag.
    std::vector<PositionChange> descendants_following_body_drag(const ost_hierarchy::HierarchyModel &model,
                                                                const std::string &takeoff_uid, int handle_idx,
                                                                const std::vector<double> &orig_position,
                                                                const std::vector<double> &new_position);
}
