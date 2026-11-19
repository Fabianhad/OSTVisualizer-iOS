#pragma once
// Snapping and acceptance rules of the Windows plan view for PLACING a takeoff
// (presentation/components/plan_view/components/placement_mode.py, drag_handler.py snap_ost).
//
// The platform layer (PlanView+Placement.mm) maps a tap / pan location to a raw OST point
// (ost_coord::canvas_to_ost_point) and then asks this header what Windows does with it:
//   1. grid snap   (_grid_snap_from_cursor): both axes are rounded to the snap increment, but the rounded point is only
//                  taken when it lies within a pixel threshold of the cursor; otherwise the raw point is used;
//   2. angle snap  (_snap_angle) of the second and later points of a linear / area against the previous point, and,
//                  only for a grid-snapped point, the length from that point rounded to the increment
//                  (_snap_angle_for_placement / _snap_placement_distance);
//   3. minimum size and polygon closing (_meets_placement_minimum, the 12 px closing distance).
// Platform independent: no UIKit, no Foundation. Pixel distances are measured by the caller in screen points.
#include "vec_types.hpp"
#include <cstddef>

namespace ost_placement
{
    using ost_types::Vec2;

    // Config.DEFAULT_MOUSE_UNPRESSED_SNAP_ANGLE: the angle step, in degrees, when the Shift key is not held. Windows
    // offers {0, 1, 2, 3, 4, 5, 10, 15, 30, 45, 90}; 0 disables the angle snap. (22.5 is not an option.)
    constexpr double kDefaultSnapAngleDegrees = 15.0;
    // Config.DEFAULT_SNAP_THRESHOLD_PX: a grid node further than this from the cursor is not snapped to (RPV-05, decision
    // a: 8 screen points on iOS).
    constexpr double kGridSnapThresholdPx = 8.0;
    // placement_mode.handle_place_press: a click within this many pixels (inclusive) of the first point of an area with at
    // least three points closes the polygon.
    constexpr double kPolygonCloseThresholdPx = 12.0;
    // The minimum placement size when the bid has no snap increment (placement_mode.py: `else 1e-6`).
    constexpr double kFallbackMinimumPlacementLength = 1e-6;

    // Windows snap_ost: round(v / increment) * increment with Python's round (ties to even: 0.5 -> 0, 1.5 -> 2, 2.5 -> 2).
    // An increment <= 0 returns the value unchanged.
    double snap_ost(double value, double increment);

    // Windows _snap_angle: the target is rotated about the origin onto the nearest multiple of `angle_degrees`, its
    // distance from the origin preserved (ties of the angle quotient to even like Python's round). A step <= 0 and a
    // distance below 1e-9 return the target unchanged.
    Vec2 snap_angle(double origin_x, double origin_y, double target_x, double target_y, double angle_degrees);

    // The kinds of a placement point that decide the length snap. Windows has line / endpoint / intersection snaps too;
    // iOS has none of them, so every point that is not a grid point is None.
    enum class SnapKind
    {
        None,
        Grid
    };

    // Windows _snap_placement_distance: the length from the origin rounded to the increment along the same direction; a
    // length that rounds to zero collapses onto the origin. An increment <= 0 and a length below 1e-9 return the target.
    Vec2 snap_placement_distance(double origin_x, double origin_y, double target_x, double target_y, double increment);

    // Windows _snap_angle_for_placement: angle snap first, then (only for a Grid point) snap_placement_distance. The angle
    // of a point that is not a grid point is preserved exactly (no rounding).
    Vec2 snap_angle_for_placement(double origin_x, double origin_y, double target_x, double target_y, SnapKind kind,
                                  double increment, double angle_degrees);

    struct PlacementPoint
    {
        Vec2 point;
        SnapKind kind = SnapKind::None;
    };

    // Windows _grid_snap_from_cursor + the fall-through of _placement_snap_from_scene. The cursor (raw OST) is snapped on
    // both axes to the increment; the snapped point is returned with kind Grid only when snapping is enabled, the
    // threshold and the increment are positive and the snapped point is at most `threshold_px` screen pixels from the
    // cursor (`screen_px_per_ost` converts an OST distance into those pixels); otherwise the raw cursor with kind None.
    PlacementPoint grid_snap(double cursor_x, double cursor_y, double increment, double screen_px_per_ost,
                             double threshold_px, bool enabled);

    // Windows _meets_placement_minimum: length >= minimum or math.isclose(length, minimum) (relative tolerance 1e-9).
    bool meets_placement_minimum(double length, double minimum);

    // The minimum placement size: the snap increment, 1e-6 without one.
    double placement_minimum_length(double snap_increment);

    // Windows handle_place_press: the click closes the polygon when the area already has at least three points and the
    // raw cursor is at most kPolygonCloseThresholdPx pixels (inclusive) from the first point.
    bool polygon_close_hit(std::size_t point_count, double distance_px);
}
