#pragma once
#include "Types.hpp"
#include "Condition.hpp"
#include "Takeoff.hpp"
#include <string>
#include <vector>
#include <utility>
#include <set>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ost_domain
{

    // UOM codes
    enum UomCode
    {
        UOM_EACH = 0,
        UOM_INCHES = 1,
        UOM_LINEAR_FEET = 2,
        UOM_LINEAR_YARDS = 3,
        UOM_SQUARE_INCHES = 4,
        UOM_SQUARE_FEET = 5,
        UOM_SQUARE_YARDS = 6,
        UOM_SQUARE_ROOFING = 7,
        UOM_CUBIC_FEET = 8,
        UOM_CUBIC_YARDS = 9,
    };

    // Calc type constants
    enum CalcType
    {
        CALC_TOP_BOTTOM = 5,
        CALC_TOP_AND_BOTTOM = 6,
        CALC_VOLUME = 20,
        CALC_COUNT = 23,
        CALC_HEIGHT_OR_LENGTH = 24,
        CALC_WIDTH_SIDE = 25,
        CALC_BOTH_WIDTH_SIDES = 26,
        CALC_DEPTH_SIDE = 27,
        CALC_BOTH_DEPTH_SIDES = 28,
        CALC_ALL_SIDES = 29,
        CALC_ALL_SIDES_PLUS_TB = 30,
    };

    using QtyOption = std::pair<int, std::string>; // (calc_type, label)
    using UomOption = std::pair<int, std::string>; // (uom_code, label)

    inline std::string get_uom_label(int code)
    {
        switch (code)
        {
        case UOM_EACH:
            return "EA";
        case UOM_INCHES:
            return "IN";
        case UOM_LINEAR_FEET:
            return "LF";
        case UOM_LINEAR_YARDS:
            return "LY";
        case UOM_SQUARE_INCHES:
            return "SQ IN";
        case UOM_SQUARE_FEET:
            return "SF";
        case UOM_SQUARE_YARDS:
            return "SY";
        case UOM_SQUARE_ROOFING:
            return "ROOF";
        case UOM_CUBIC_FEET:
            return "CF";
        case UOM_CUBIC_YARDS:
            return "CY";
        default:
            return "";
        }
    }

    inline std::vector<QtyOption> get_quantity_options_for_type(ConditionType ct)
    {
        if (ct == ConditionType::Count || ct == ConditionType::Attachment)
        {
            return {
                {CALC_COUNT, "Count"},
                {CALC_HEIGHT_OR_LENGTH, "Total Height"},
                {13, "Perimeter"},
                {CALC_TOP_BOTTOM, "Surface area (top or bottom)"},
                {CALC_TOP_AND_BOTTOM, "Surface area (top and bottom)"},
                {CALC_WIDTH_SIDE, "Surface area (single width side)"},
                {CALC_BOTH_WIDTH_SIDES, "Surface area (both width sides)"},
                {CALC_ALL_SIDES, "Surface area (all sides)"},
                {CALC_ALL_SIDES_PLUS_TB, "Surface area (all sides + top and bottom)"},
                {CALC_VOLUME, "Volume"},
            };
        }
        if (ct == ConditionType::Linear)
        {
            return {
                {1, "Length"},
                {2, "Segment count"},
                {3, "Surface area (single side)"},
                {4, "Surface area (both sides)"},
                {CALC_TOP_BOTTOM, "Surface area (top or bottom)"},
                {CALC_TOP_AND_BOTTOM, "Surface area (top and bottom)"},
                {7, "Surface area (single end)"},
                {8, "Surface area (both ends)"},
                {9, "Surface area (all side/duct)"},
                {CALC_VOLUME, "Volume"},
            };
        }
        // Area
        return {
            {11, "Area"},
            {34, "Area (Ignore Backout Areas)"},
            {12, "Area (minus Attachments)"},
            {15, "Perimeter"},
            {14, "Perimeter (without Backout Perimeters)"},
            {33, "Perimeter (plus Attachments Perimeter)"},
            {16, "Grid length (visible)"},
            {17, "Tile count (average)"},
            {18, "Tile count (visible)"},
            {19, "Area counts"},
            {21, "Volume"},
            {35, "Volume (Ignore Backout Volume)"},
            {22, "Volume (minus Attachments)"},
        };
    }

    inline std::vector<UomOption> get_valid_uoms_for_calc_type(int calc_type)
    {
        // Count-like calc types
        static const std::set<int> countCalcs = {CALC_COUNT, 2, 17, 18, 19};
        // Length-like
        static const std::set<int> lengthCalcs = {CALC_HEIGHT_OR_LENGTH, 1, 13, 14, 15, 16, 33};
        // Area-like
        static const std::set<int> areaCalcs = {
            CALC_TOP_BOTTOM,
            CALC_TOP_AND_BOTTOM,
            CALC_WIDTH_SIDE,
            CALC_BOTH_WIDTH_SIDES,
            CALC_DEPTH_SIDE,
            CALC_BOTH_DEPTH_SIDES,
            CALC_ALL_SIDES,
            CALC_ALL_SIDES_PLUS_TB,
            3,
            4,
            7,
            8,
            9,
            11,
            12,
            34,
        };
        // Volume-like
        static const std::set<int> volumeCalcs = {CALC_VOLUME, 21, 22, 35};

        if (countCalcs.count(calc_type) || calc_type == 0)
        {
            return {{UOM_EACH, "EA"}};
        }
        if (lengthCalcs.count(calc_type))
        {
            return {
                {UOM_LINEAR_FEET, "LF"},
                {UOM_LINEAR_YARDS, "LY"},
                {UOM_INCHES, "IN"},
            };
        }
        if (volumeCalcs.count(calc_type))
        {
            return {
                {UOM_CUBIC_FEET, "CF"},
                {UOM_CUBIC_YARDS, "CY"},
            };
        }
        if (areaCalcs.count(calc_type))
        {
            return {
                {UOM_SQUARE_FEET, "SF"},
                {UOM_SQUARE_YARDS, "SY"},
                {UOM_SQUARE_INCHES, "SQ IN"},
            };
        }
        // Default to length
        return {
            {UOM_LINEAR_FEET, "LF"},
            {UOM_LINEAR_YARDS, "LY"},
            {UOM_INCHES, "IN"},
        };
    }

    // --- UOM conversion ---

    inline double convert_to_uom(double raw_value, int uom_code)
    {
        double divisor = 1.0;
        switch (uom_code)
        {
        case UOM_EACH:
            divisor = 1.0;
            break;
        case UOM_INCHES:
            divisor = 1.0;
            break;
        case UOM_LINEAR_FEET:
            divisor = 12.0;
            break;
        case UOM_LINEAR_YARDS:
            divisor = 36.0;
            break;
        case UOM_SQUARE_INCHES:
            divisor = 1.0;
            break;
        case UOM_SQUARE_FEET:
            divisor = 144.0;
            break;
        case UOM_SQUARE_YARDS:
            divisor = 1296.0;
            break;
        case UOM_SQUARE_ROOFING:
            divisor = 14400.0;
            break;
        case UOM_CUBIC_FEET:
            divisor = 1728.0;
            break;
        case UOM_CUBIC_YARDS:
            divisor = 46656.0;
            break;
        }
        return (divisor == 0.0) ? 0.0 : raw_value / divisor;
    }

    // --- Geometry helpers ---

    inline std::vector<std::pair<double, double>> vertices_from_position(
        const std::vector<double> &pos)
    {
        std::vector<std::pair<double, double>> verts;
        for (size_t i = 0; i + 1 < pos.size(); i += 2)
            verts.push_back({pos[i], pos[i + 1]});
        return verts;
    }

    inline double calc_polygon_area(const std::vector<std::pair<double, double>> &verts)
    {
        size_t n = verts.size();
        if (n < 3)
            return 0.0;
        double area = 0.0;
        for (size_t i = 0; i < n; i++)
        {
            size_t j = (i + 1) % n;
            area += verts[i].first * verts[j].second;
            area -= verts[j].first * verts[i].second;
        }
        return std::abs(area) / 2.0;
    }

    inline double calc_polygon_area_sq_inches(const std::vector<double> &pos)
    {
        auto verts = vertices_from_position(pos);
        return (verts.size() < 3) ? 0.0 : calc_polygon_area(verts);
    }

    inline double calc_polygon_perimeter(const std::vector<double> &pos)
    {
        auto verts = vertices_from_position(pos);
        size_t n = verts.size();
        if (n < 2)
            return 0.0;
        double perimeter = 0.0;
        for (size_t i = 0; i < n; i++)
        {
            size_t j = (i + 1) % n;
            double dx = verts[j].first - verts[i].first;
            double dy = verts[j].second - verts[i].second;
            perimeter += std::sqrt(dx * dx + dy * dy);
        }
        return perimeter;
    }

    inline std::pair<double, double> calc_bounding_box(const std::vector<double> &pos)
    {
        auto verts = vertices_from_position(pos);
        if (verts.empty())
            return {0.0, 0.0};
        double minX = verts[0].first, maxX = minX;
        double minY = verts[0].second, maxY = minY;
        for (size_t i = 1; i < verts.size(); i++)
        {
            minX = std::min(minX, verts[i].first);
            maxX = std::max(maxX, verts[i].first);
            minY = std::min(minY, verts[i].second);
            maxY = std::max(maxY, verts[i].second);
        }
        return {maxX - minX, maxY - minY};
    }

    // Linear takeoff: position is segments of stride 7 (x1,y1,x2,y2,bulge,cx,cy)
    inline double calc_linear_length(const std::vector<double> &pos)
    {
        if (pos.size() < 4)
            return 0.0;
        double total = 0.0;
        const size_t stride = 7;
        size_t i = 0;
        while (i + 3 < pos.size())
        {
            double dx = pos[i + 2] - pos[i];
            double dy = pos[i + 3] - pos[i + 1];
            total += std::sqrt(dx * dx + dy * dy);
            i += stride;
        }
        return total;
    }

    inline double perp_slope(double dx, double dy)
    {
        if (dx == 0.0)
            return std::numeric_limits<double>::infinity();
        if (dy == 0.0)
            return 0.0;
        return -dx / dy;
    }

    inline std::vector<std::pair<double, double>> quadratic_curve_points(
        double x1, double y1, double x2, double y2,
        double cx, double cy, int segments)
    {
        std::vector<std::pair<double, double>> points;
        if (segments < 1)
            return points;

        points.reserve(static_cast<size_t>(segments) + 1);
        for (int i = 0; i <= segments; ++i)
        {
            double t = static_cast<double>(i) / segments;
            double u = 1.0 - t;
            points.push_back({
                u * u * x1 + 2.0 * u * t * cx + t * t * x2,
                u * u * y1 + 2.0 * u * t * cy + t * t * y2,
            });
        }
        return points;
    }

    inline std::array<double, 6> process_curved_position(
        const std::vector<double> &position)
    {
        double x1 = position[0];
        double y1 = position[1];
        double x2 = position[2];
        double y2 = position[3];
        double cx = position[4];
        double cy = position[5];

        if (position.size() >= 7)
        {
            double offset = -position[6];
            double mid_x = (x1 + x2) * 0.5;
            double mid_y = (y1 + y2) * 0.5;
            double dx = x2 - x1;
            double dy = y2 - y1;
            double length = std::hypot(dx, dy);
            if (length > 0.0)
            {
                dx /= length;
                dy /= length;
                cx = mid_x - dy * offset;
                cy = mid_y + dx * offset;
            }
        }

        return {x1, y1, x2, y2, cx, cy};
    }

    inline int calc_curve_segments(double x1, double y1, double x2, double y2,
                                   double cx, double cy)
    {
        double chord_d = std::hypot(x2 - x1, y2 - y1);
        double curve_d = (std::hypot(cx - x1, cy - y1) +
                          std::hypot(x2 - cx, y2 - cy)) *
                         0.8;
        int base = static_cast<int>(std::max(chord_d, curve_d) / 5.0);
        base = std::max(6, std::min(64, base));

        if (chord_d > 0.0)
        {
            double dx = x2 - x1;
            double dy = y2 - y1;
            double t = ((cx - x1) * dx + (cy - y1) * dy) / (dx * dx + dy * dy);
            t = std::max(0.0, std::min(1.0, t));
            double offset = std::hypot(cx - (x1 + t * dx), cy - (y1 + t * dy));
            base += static_cast<int>(offset / 5.0);
        }

        return std::max(6, std::min(64, base));
    }

    inline std::vector<std::pair<double, double>> advanced_curve_points(
        double x1, double y1, double x2, double y2,
        double cx, double cy, int segments)
    {
        double mid1_x = (x1 + cx) * 0.5;
        double mid1_y = (y1 + cy) * 0.5;
        double mid2_x = (cx + x2) * 0.5;
        double mid2_y = (cy + y2) * 0.5;
        double slope1 = perp_slope(cx - x1, cy - y1);
        double slope2 = perp_slope(x2 - cx, y2 - cy);
        bool found = false;
        double center_x = 0.0;
        double center_y = 0.0;

        if (std::isinf(slope1))
        {
            if (!std::isinf(slope2))
            {
                center_x = mid1_x;
                center_y = mid2_y + slope2 * (mid1_x - mid2_x);
                found = true;
            }
        }
        else if (std::isinf(slope2))
        {
            center_x = mid2_x;
            center_y = mid1_y + slope1 * (mid2_x - mid1_x);
            found = true;
        }
        else if (std::abs(slope1 - slope2) > 1e-10)
        {
            center_x = (mid2_y - mid1_y + slope1 * mid1_x - slope2 * mid2_x) /
                       (slope1 - slope2);
            center_y = mid1_y + slope1 * (center_x - mid1_x);
            found = true;
        }

        if (found)
        {
            double radius1 = std::hypot(center_x - x1, center_y - y1);
            double radius2 = std::hypot(center_x - x2, center_y - y2);
            double radius_c = std::hypot(center_x - cx, center_y - cy);
            if (std::abs(radius1 - radius2) < 0.1 &&
                std::abs(radius1 - radius_c) < 0.1)
            {
                double start_angle = std::atan2(y1 - center_y, x1 - center_x);
                double end_angle = std::atan2(y2 - center_y, x2 - center_x);
                double cross1 = (x1 - center_x) * (cy - center_y) -
                                (y1 - center_y) * (cx - center_x);
                double cross2 = (cx - center_x) * (y2 - center_y) -
                                (cy - center_y) * (x2 - center_x);
                bool clockwise = (cross1 + cross2) < 0.0;
                if (clockwise && end_angle > start_angle)
                    end_angle -= 2.0 * M_PI;
                else if (!clockwise && end_angle < start_angle)
                    end_angle += 2.0 * M_PI;

                double angle_diff = end_angle - start_angle;
                std::vector<std::pair<double, double>> points;
                points.reserve(static_cast<size_t>(segments) + 1);
                for (int i = 0; i <= segments; ++i)
                {
                    double t = static_cast<double>(i) / segments;
                    double angle = start_angle + t * angle_diff;
                    points.push_back({
                        center_x + radius1 * std::cos(angle),
                        center_y + radius1 * std::sin(angle),
                    });
                }
                return points;
            }
        }

        return quadratic_curve_points(x1, y1, x2, y2, cx, cy, segments);
    }

    inline double calc_curved_linear_length(const std::vector<double> &position)
    {
        if (position.size() < 6)
            return 0.0;

        auto p = process_curved_position(position);
        for (double value : p)
        {
            if (!std::isfinite(value))
                return calc_linear_length(position);
        }

        int segments = calc_curve_segments(p[0], p[1], p[2], p[3], p[4], p[5]);
        auto points = advanced_curve_points(
            p[0], p[1], p[2], p[3], p[4], p[5], segments);
        if (points.size() < 2)
            return calc_linear_length(position);

        double total = 0.0;
        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            total += std::hypot(points[i + 1].first - points[i].first,
                                points[i + 1].second - points[i].second);
        }
        return total;
    }

    inline double calc_linear_length(const std::vector<double> &pos, int curve)
    {
        if (pos.size() < 4)
            return 0.0;
        if (curve >= 0 && pos.size() >= 6)
            return calc_curved_linear_length(pos);
        return calc_linear_length(pos);
    }

    inline double calc_slope_factor(double rise, double run)
    {
        if (run > 0.0 && rise > 0.0)
            return std::sqrt(rise * rise + run * run) / run;
        return 1.0;
    }

    // --- Per-type raw quantity calculators ---

    inline double calc_raw_for_count(int calc_type, double width, double height, double depth)
    {
        switch (calc_type)
        {
        case 13:
            return 2.0 * (width + depth); // Perimeter
        case CALC_COUNT:
            return 1.0;
        case CALC_HEIGHT_OR_LENGTH:
            return height;
        case CALC_TOP_BOTTOM:
            return width * depth;
        case CALC_TOP_AND_BOTTOM:
            return 2.0 * width * depth;
        case CALC_VOLUME:
            return width * depth * height;
        case CALC_WIDTH_SIDE:
            return width * height;
        case CALC_BOTH_WIDTH_SIDES:
            return 2.0 * width * height;
        case CALC_DEPTH_SIDE:
            return depth * height;
        case CALC_BOTH_DEPTH_SIDES:
            return 2.0 * depth * height;
        case CALC_ALL_SIDES:
            return 2.0 * (width * height + depth * height);
        case CALC_ALL_SIDES_PLUS_TB:
            return 2.0 * (width * height + depth * height + width * depth);
        default:
            return 0.0;
        }
    }

    inline double calc_raw_for_linear(int calc_type, double height, double thickness,
                                      double length_inches)
    {
        switch (calc_type)
        {
        case 0:
            return 0.0;
        case 1:
            return length_inches; // Length
        case 2:
            return 1.0; // Segment count
        case 3:
            return length_inches * height; // Single side
        case 4:
            return 2.0 * length_inches * height; // Both sides
        case 5:
            return length_inches * thickness; // Top or bottom
        case 6:
            return 2.0 * length_inches * thickness; // Top and bottom
        case 7:
            return thickness * height; // Single end
        case 8:
            return 2.0 * thickness * height; // Both ends
        case 9:
            return 2.0 * length_inches * (height + thickness); // All side/duct
        case 20:
            return length_inches * height * thickness; // Volume
        default:
            return 0.0;
        }
    }

    inline double calc_raw_for_area(int calc_type, double thickness,
                                    double polygon_area, double hole_area,
                                    double polygon_perimeter, double hole_perimeter,
                                    double attachment_footprint, double attachment_perimeter,
                                    double grid_size1, double grid_size2, double gap,
                                    double bbox_w, double bbox_h)
    {
        double net_area = polygon_area - hole_area;
        switch (calc_type)
        {
        case 0:
            return 0.0;
        case 11:
            return net_area; // Area
        case 12:
            return net_area - attachment_footprint; // Area minus attachments
        case 14:
            return polygon_perimeter; // Perimeter (no backout)
        case 15:
            return polygon_perimeter + hole_perimeter; // Perimeter
        case 16:
        { // Grid length (visible)
            double u1 = grid_size1 + gap, u2 = grid_size2 + gap;
            if (u1 <= 0 || u2 <= 0)
                return 0.0;
            return std::ceil(bbox_w / u1) * bbox_h + std::ceil(bbox_h / u2) * bbox_w;
        }
        case 17:
        { // Tile count (average)
            double u1 = grid_size1 + gap, u2 = grid_size2 + gap;
            if (u1 <= 0 || u2 <= 0)
                return 0.0;
            return net_area / (u1 * u2);
        }
        case 18:
        { // Tile count (visible)
            double u1 = grid_size1 + gap, u2 = grid_size2 + gap;
            if (u1 <= 0 || u2 <= 0)
                return 0.0;
            return std::floor(bbox_w / u1) * std::floor(bbox_h / u2);
        }
        case 19:
            return 1.0; // Area counts
        case 21:
            return net_area * thickness; // Volume
        case 22:
            return (net_area - attachment_footprint) * thickness; // Volume minus att.
        case 23:
            return 1.0; // Count
        case 33:
            return polygon_perimeter + hole_perimeter + attachment_perimeter;
        case 34:
            return polygon_area; // Area (ignore backout)
        case 35:
            return polygon_area * thickness; // Volume (ignore backout)
        default:
            return 0.0;
        }
    }

    // --- Main per-takeoff quantity calculation ---

    struct QuantityResult
    {
        double q1 = 0.0, q2 = 0.0, q3 = 0.0;
    };

    inline QuantityResult calculate_condition_quantities(
        ConditionType condition_type,
        int calc_type1, int calc_type2, int calc_type3,
        int uom1, int uom2, int uom3,
        double width, double height, double depth, double thickness,
        const std::vector<double> &position,
        const std::vector<std::vector<double>> &hole_positions = {},
        double attachment_footprint = 0.0,
        double attachment_perimeter = 0.0,
        double rise = 0.0, double run = 0.0,
        double grid_size1 = 0.0, double grid_size2 = 0.0, double gap = 0.0,
        int curve = -1)
    {

        double slope = calc_slope_factor(rise, run);

        // Pre-compute type-specific geometry
        double length_inches = 0.0;
        double polygon_area = 0.0, polygon_perim = 0.0;
        double hole_area = 0.0, hole_perim = 0.0;
        double bbox_w = 0.0, bbox_h = 0.0;

        if (condition_type == ConditionType::Linear)
        {
            length_inches = calc_linear_length(position, curve) * slope;
        }
        else if (condition_type == ConditionType::Area)
        {
            polygon_area = calc_polygon_area_sq_inches(position) * slope;
            polygon_perim = calc_polygon_perimeter(position) * slope;
            auto bb = calc_bounding_box(position);
            bbox_w = bb.first;
            bbox_h = bb.second;
            for (const auto &hp : hole_positions)
            {
                hole_area += calc_polygon_area_sq_inches(hp) * slope;
                hole_perim += calc_polygon_perimeter(hp) * slope;
            }
        }

        auto calc_one = [&](int calc_type, int uom) -> double
        {
            double raw = 0.0;
            if (condition_type == ConditionType::Count ||
                condition_type == ConditionType::Attachment)
            {
                raw = calc_raw_for_count(calc_type, width, height, depth);
            }
            else if (condition_type == ConditionType::Linear)
            {
                raw = calc_raw_for_linear(calc_type, height, thickness, length_inches);
            }
            else
            {
                raw = calc_raw_for_area(calc_type, thickness,
                                        polygon_area, hole_area,
                                        polygon_perim, hole_perim,
                                        attachment_footprint, attachment_perimeter,
                                        grid_size1, grid_size2, gap,
                                        bbox_w, bbox_h);
            }
            return convert_to_uom(raw, uom);
        };

        QuantityResult r;
        r.q1 = calc_one(calc_type1, uom1);
        r.q2 = calc_one(calc_type2, uom2);
        r.q3 = calc_one(calc_type3, uom3);
        return r;
    }

    // --- Page-level quantity aggregation ---
    // Groups takeoffs by condition, sums per-takeoff quantities for each condition.
    // Matches desktop compute_page_quantities().

    inline std::unordered_map<std::string, QuantityResult> compute_page_quantities(
        const std::unordered_map<std::string, Condition> &conditions,
        const std::vector<Takeoff> &takeoffs)
    {

        // Group takeoffs by condition_uid
        std::unordered_map<std::string, std::vector<const Takeoff *>> by_condition;
        for (const auto &t : takeoffs)
            by_condition[t.condition_uid].push_back(&t);

        // Index hole takeoffs by parent_uid
        std::unordered_map<std::string, std::vector<const Takeoff *>> holes_by_parent;
        for (const auto &t : takeoffs)
        {
            if (t.is_hole())
                holes_by_parent[t.parent_uid].push_back(&t);
        }

        std::unordered_map<std::string, QuantityResult> result;

        for (const auto &pair : by_condition)
        {
            const std::string &cond_uid = pair.first;
            auto cit = conditions.find(cond_uid);
            if (cit == conditions.end())
                continue;
            const Condition &cdn = cit->second;

            double total_q1 = 0.0, total_q2 = 0.0, total_q3 = 0.0;

            for (const Takeoff *t : pair.second)
            {
                // Skip holes — they are accounted for under their parent
                if (t->is_hole())
                    continue;

                // Gather hole positions for this takeoff
                std::vector<std::vector<double>> hole_pos;
                auto hit = holes_by_parent.find(t->uid);
                if (hit != holes_by_parent.end())
                {
                    for (const Takeoff *h : hit->second)
                    {
                        if (!h->position.empty())
                            hole_pos.push_back(h->position);
                    }
                }

                double sign = t->is_negative ? -1.0 : 1.0;

                QuantityResult qr = calculate_condition_quantities(
                    cdn.condition_type,
                    cdn.calc_type1, cdn.calc_type2, cdn.calc_type3,
                    cdn.uom1, cdn.uom2, cdn.uom3,
                    cdn.width, cdn.height, cdn.depth, cdn.thickness,
                    t->position, hole_pos,
                    0.0, 0.0, // attachment_footprint, attachment_perimeter (TODO)
                    cdn.rise, cdn.run,
                    cdn.grid_size1, cdn.grid_size2, cdn.gap,
                    t->curve);

                total_q1 += qr.q1 * sign;
                total_q2 += qr.q2 * sign;
                total_q3 += qr.q3 * sign;
            }

            result[cond_uid] = {total_q1, total_q2, total_q3};
        }

        return result;
    }

} // namespace ost_domain
