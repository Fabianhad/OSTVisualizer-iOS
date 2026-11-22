// Port of the Windows placement snapping (placement_mode.py _snap_angle / _snap_placement_distance /
// _snap_angle_for_placement / _grid_snap_from_cursor / _meets_placement_minimum, drag_handler.py snap_ost).
#include "strict_fp.hpp" // first: no fused multiply-add, like the Windows build (see strict_fp.hpp); std::hypot below is CPython's math.hypot
#include "placement_snap.hpp"
#include <cmath>

namespace ost_placement
{
    namespace
    {
        // math.radians(x) = x * (pi / 180) in CPython.
        constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
        // The "same point" tolerance of _snap_angle / _snap_placement_distance (`length < 1e-9`).
        constexpr double kMinimumLength = 1e-9;

        // Python's math.isclose(a, b) with the default rel_tol = 1e-9 and abs_tol = 0.
        bool is_close(double a, double b)
        {
            if (a == b)
                return true;
            if (std::isinf(a) || std::isinf(b))
                return false;
            const double diff = std::fabs(b - a);
            return diff <= std::fabs(1e-9 * b) || diff <= std::fabs(1e-9 * a);
        }
    }

    double snap_ost(double value, double increment)
    {
        if (increment <= 0.0)
            return value;
        // Python round(): ties to even (std::nearbyint under the default rounding mode), not C round().
        return std::nearbyint(value / increment) * increment;
    }

    Vec2 snap_angle(double origin_x, double origin_y, double target_x, double target_y, double angle_degrees)
    {
        if (angle_degrees <= 0.0)
            return {target_x, target_y};
        const double dx = target_x - origin_x;
        const double dy = target_y - origin_y;
        const double length = std::hypot(dx, dy);
        if (length < kMinimumLength)
            return {target_x, target_y};
        const double angle = std::atan2(dy, dx);
        const double step = angle_degrees * kDegreesToRadians;
        const double snapped = std::nearbyint(angle / step) * step;
        return {origin_x + length * std::cos(snapped), origin_y + length * std::sin(snapped)};
    }

    Vec2 snap_placement_distance(double origin_x, double origin_y, double target_x, double target_y, double increment)
    {
        if (!(increment > 0.0))
            return {target_x, target_y};
        const double dx = target_x - origin_x;
        const double dy = target_y - origin_y;
        const double length = std::hypot(dx, dy);
        if (length < kMinimumLength)
            return {target_x, target_y};
        const double snapped_length = std::nearbyint(length / increment) * increment;
        if (snapped_length <= 0.0)
            return {origin_x, origin_y};
        const double scale = snapped_length / length;
        return {origin_x + dx * scale, origin_y + dy * scale};
    }

    Vec2 snap_angle_for_placement(double origin_x, double origin_y, double target_x, double target_y, SnapKind kind,
                                  double increment, double angle_degrees)
    {
        const Vec2 snapped = snap_angle(origin_x, origin_y, target_x, target_y, angle_degrees);
        if (kind != SnapKind::Grid)
            return snapped;
        return snap_placement_distance(origin_x, origin_y, snapped[0], snapped[1], increment);
    }

    PlacementPoint grid_snap(double cursor_x, double cursor_y, double increment, double screen_px_per_ost,
                             double threshold_px, bool enabled)
    {
        PlacementPoint raw;
        raw.point = {cursor_x, cursor_y};
        raw.kind = SnapKind::None;
        if (!enabled || !(threshold_px > 0.0) || !(increment > 0.0))
            return raw;
        const double snapped_x = snap_ost(cursor_x, increment);
        const double snapped_y = snap_ost(cursor_y, increment);
        const double distance_px = std::hypot(snapped_x - cursor_x, snapped_y - cursor_y) * screen_px_per_ost;
        if (distance_px > threshold_px)
            return raw;
        PlacementPoint snapped;
        snapped.point = {snapped_x, snapped_y};
        snapped.kind = SnapKind::Grid;
        return snapped;
    }

    bool meets_placement_minimum(double length, double minimum)
    {
        return length >= minimum || is_close(length, minimum);
    }

    double placement_minimum_length(double snap_increment)
    {
        return snap_increment > 0.0 ? snap_increment : kFallbackMinimumPlacementLength;
    }

    bool polygon_close_hit(std::size_t point_count, double distance_px)
    {
        return point_count >= 3 && distance_px <= kPolygonCloseThresholdPx;
    }
}
