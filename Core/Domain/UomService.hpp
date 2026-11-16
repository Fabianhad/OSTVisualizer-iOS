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
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(__clang__)
// Windows evaluates every quantity formula as separate IEEE double operations (CPython floats, no FMA). Clang on
// arm64 contracts a*b + c*d into fused multiply-adds by default, which changes the last bits of areas, slopes,
// arc centres and the ceil() of rounded quantities (a 1-in-25000 page scenario then differs by a whole rounding
// increment). The default is restored at the end of this header.
#pragma STDC FP_CONTRACT OFF
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
        UOM_MM = 11,
        UOM_MM2 = 12,
        UOM_M = 13,
        UOM_M2 = 14,
        UOM_MM3 = 15,
        UOM_M3 = 16,
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
            return "IN\xC2\xB2"; // IN + superscript 2 (UTF-8), as on Windows
        case UOM_SQUARE_FEET:
            return "SF";
        case UOM_SQUARE_YARDS:
            return "SY";
        case UOM_SQUARE_ROOFING:
            return "SQ";
        case UOM_CUBIC_FEET:
            return "CF";
        case UOM_CUBIC_YARDS:
            return "CY";
        case UOM_MM:
            return "mm";
        case UOM_MM2:
            return "mm\xC2\xB2"; // mm + superscript 2
        case UOM_M:
            return "m";
        case UOM_M2:
            return "m\xC2\xB2"; // m + superscript 2
        case UOM_MM3:
            return "mm\xC2\xB3"; // mm + superscript 3
        case UOM_M3:
            return "m\xC2\xB3"; // m + superscript 3
        default:
            return "";
        }
    }

    inline bool is_metric_uom(int code)
    {
        return code == UOM_MM || code == UOM_MM2 || code == UOM_M ||
               code == UOM_M2 || code == UOM_MM3 || code == UOM_M3;
    }

    inline std::vector<QtyOption> get_quantity_options_for_type(ConditionType ct)
    {
        // Count, Attachment and every unsupported type get the count options.
        if (ct != ConditionType::Linear && ct != ConditionType::Area)
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

    inline std::vector<UomOption> get_valid_uoms_for_calc_type(int calc_type, bool metric = false)
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
        // Metric lists (bids with measure_base == 1): largest unit first.
        static const std::vector<UomOption> lengthMetric = {{UOM_M, "m"}, {UOM_MM, "mm"}};
        static const std::vector<UomOption> volumeMetric = {{UOM_M3, "m\xC2\xB3"}, {UOM_MM3, "mm\xC2\xB3"}};
        static const std::vector<UomOption> areaMetric = {{UOM_M2, "m\xC2\xB2"}, {UOM_MM2, "mm\xC2\xB2"}};
        if (lengthCalcs.count(calc_type))
        {
            if (metric)
                return lengthMetric;
            return {
                {UOM_LINEAR_FEET, "LF"},
                {UOM_LINEAR_YARDS, "LY"},
                {UOM_INCHES, "IN"},
            };
        }
        if (volumeCalcs.count(calc_type))
        {
            if (metric)
                return volumeMetric;
            return {
                {UOM_CUBIC_FEET, "CF"},
                {UOM_CUBIC_YARDS, "CY"},
            };
        }
        if (areaCalcs.count(calc_type))
        {
            if (metric)
                return areaMetric;
            return {
                {UOM_SQUARE_FEET, "SF"},
                {UOM_SQUARE_YARDS, "SY"},
                {UOM_SQUARE_ROOFING, "SQ"},
                {UOM_SQUARE_INCHES, "IN\xC2\xB2"},
            };
        }
        // Default to length
        if (metric)
            return lengthMetric;
        return {
            {UOM_LINEAR_FEET, "LF"},
            {UOM_LINEAR_YARDS, "LY"},
            {UOM_INCHES, "IN"},
        };
    }

    // --- UOM conversion ---

    // Metric divisors: raw inches are divided by the unit's size in inches.
    inline constexpr double UOM_MM_PER_INCH = 25.4;
    inline constexpr double UOM_MM2_PER_SQIN = UOM_MM_PER_INCH * UOM_MM_PER_INCH;
    inline constexpr double UOM_MM3_PER_CUIN = UOM_MM2_PER_SQIN * UOM_MM_PER_INCH;

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
        case UOM_MM:
            divisor = 1.0 / UOM_MM_PER_INCH;
            break;
        case UOM_M:
            divisor = 1000.0 / UOM_MM_PER_INCH;
            break;
        case UOM_MM2:
            divisor = 1.0 / UOM_MM2_PER_SQIN;
            break;
        case UOM_M2:
            divisor = 1000000.0 / UOM_MM2_PER_SQIN;
            break;
        case UOM_MM3:
            divisor = 1.0 / UOM_MM3_PER_CUIN;
            break;
        case UOM_M3:
            divisor = 1000000000.0 / UOM_MM3_PER_CUIN;
            break;
        }
        return (divisor == 0.0) ? 0.0 : raw_value / divisor;
    }

    // convert_to_uom, then (when round_quantity is on and round_up > 0) round UP to the next multiple
    // of round_up (given in inches) expressed in the same unit.
    inline double convert_and_round_quantity(double raw_value, int uom_code,
                                             bool round_quantity = false,
                                             double round_up = 0.0)
    {
        double value = convert_to_uom(raw_value, uom_code);
        if (round_quantity && round_up > 0.0)
        {
            double increment = convert_to_uom(round_up, uom_code);
            if (increment > 0.0)
                value = std::ceil(value / increment) * increment;
        }
        return value;
    }

    // --- UOM normalization (measurement system / calc-type compatibility) ---

    // Imperial -> metric: lengths -> m, areas -> m2, volumes -> m3 (EACH and unknown codes unchanged).
    // Metric -> imperial: mm, m -> LF; mm2, m2 -> SF; mm3, m3 -> CF.
    inline int normalize_uom_for_system(int uom_code, bool metric)
    {
        if (metric)
        {
            switch (uom_code)
            {
            case UOM_INCHES:
            case UOM_LINEAR_FEET:
            case UOM_LINEAR_YARDS:
                return UOM_M;
            case UOM_SQUARE_INCHES:
            case UOM_SQUARE_FEET:
            case UOM_SQUARE_YARDS:
            case UOM_SQUARE_ROOFING:
                return UOM_M2;
            case UOM_CUBIC_FEET:
            case UOM_CUBIC_YARDS:
                return UOM_M3;
            default:
                return uom_code;
            }
        }
        switch (uom_code)
        {
        case UOM_MM:
        case UOM_M:
            return UOM_LINEAR_FEET;
        case UOM_MM2:
        case UOM_M2:
            return UOM_SQUARE_FEET;
        case UOM_MM3:
        case UOM_M3:
            return UOM_CUBIC_FEET;
        default:
            return uom_code;
        }
    }

    inline std::vector<int> normalize_uoms_for_system(const std::vector<int> &uom_codes, bool metric)
    {
        std::vector<int> out;
        for (int code : uom_codes)
            out.push_back(normalize_uom_for_system(code, metric));
        return out;
    }

    // Keeps the (system-normalized) unit when it is valid for the calc type, otherwise the first valid
    // unit of that calc type. Calc type 0 (none) only converts between systems.
    inline int normalize_uom_for_calculation(int calc_type, int uom_code, bool metric)
    {
        int normalized = normalize_uom_for_system(uom_code, metric);
        if (calc_type == 0)
            return normalized;
        std::vector<UomOption> valid_uoms = get_valid_uoms_for_calc_type(calc_type, metric);
        for (const UomOption &opt : valid_uoms)
        {
            if (opt.first == normalized)
                return normalized;
        }
        return valid_uoms[0].first;
    }

    // zip semantics: stops at the shorter of the two lists.
    inline std::vector<int> normalize_uoms_for_calculations(const std::vector<int> &calc_types,
                                                            const std::vector<int> &uom_codes,
                                                            bool metric)
    {
        std::vector<int> out;
        for (size_t i = 0; i < calc_types.size() && i < uom_codes.size(); ++i)
            out.push_back(normalize_uom_for_calculation(calc_types[i], uom_codes[i], metric));
        return out;
    }

    // Rewrites uom1..3 of the condition in place (call with metric = (bid.measure_base == 1) whenever
    // conditions are loaded or replaced). Idempotent.
    inline Condition &normalize_condition_uoms_for_system(Condition &condition, bool metric)
    {
        std::vector<int> uoms = normalize_uoms_for_calculations(
            {condition.calc_type1, condition.calc_type2, condition.calc_type3},
            {condition.uom1, condition.uom2, condition.uom3},
            metric);
        condition.uom1 = uoms[0];
        condition.uom2 = uoms[1];
        condition.uom3 = uoms[2];
        return condition;
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
        // Shoelace sum relative to the first vertex: a translation costs no precision.
        double area = 0.0;
        const double origin_x = verts[0].first;
        const double origin_y = verts[0].second;
        for (size_t i = 0; i < n; i++)
        {
            size_t j = (i + 1) % n;
            area += (verts[i].first - origin_x) * (verts[j].second - origin_y) -
                    (verts[j].first - origin_x) * (verts[i].second - origin_y);
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
        if (dy == 0.0)
            return std::numeric_limits<double>::infinity();
        if (dx == 0.0)
            return 0.0;
        return -dx / dy;
    }

    // sqrt(x*x + y*y), correctly rounded in all but pathological ties (double-double sum of squares plus
    // one Newton step). Windows measures arcs with Python's math.hypot, which is accurate to the last
    // bit; std::hypot differs from it by one ulp in roughly one call in six, and the arc circle test
    // |radius1 - radius2| < 0.1 on a huge, nearly collinear circle flips on that last bit.
    inline double accurate_hypot(double x, double y)
    {
        x = std::fabs(x);
        y = std::fabs(y);
        if (std::isinf(x) || std::isinf(y))
            return std::numeric_limits<double>::infinity();
        if (std::isnan(x) || std::isnan(y))
            return std::numeric_limits<double>::quiet_NaN();
        if (x < y)
            std::swap(x, y);
        if (y == 0.0)
            return x;
        // scale by a power of two so that 0.5 <= xs < 1 (exact)
        int e = 0;
        std::frexp(x, &e);
        const double xs = std::ldexp(x, -e);
        const double ys = std::ldexp(y, -e);
        const double p1 = xs * xs;
        const double e1 = std::fma(xs, xs, -p1);
        const double p2 = ys * ys;
        const double e2 = std::fma(ys, ys, -p2);
        const double s = p1 + p2;
        const double lo = ((p2 - (s - p1)) + e1) + e2;
        double h = std::sqrt(s);
        h += (std::fma(-h, h, s) + lo) / (2.0 * h);
        return std::ldexp(h, e);
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
            double length = accurate_hypot(dx, dy);
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

    // Python int(value) of a float, saturated to the int range. Windows computes the segment count with
    // unbounded integers and clamps it to [6, 64], so only the saturated value matters; a plain static_cast of a
    // double beyond the int range is undefined behaviour (INT_MIN on x86-64, which gave 6 segments for chords over
    // 1e10 inches). NaN and infinity make Python raise; here they saturate instead.
    inline long long saturating_int(double value)
    {
        if (!(value < 2147483647.0))
            return 2147483647LL;
        if (value < -2147483648.0)
            return -2147483648LL;
        return static_cast<long long>(value);
    }

    inline int calc_curve_segments(double x1, double y1, double x2, double y2,
                                   double cx, double cy)
    {
        double chord_d = accurate_hypot(x2 - x1, y2 - y1);
        double curve_d = (accurate_hypot(cx - x1, cy - y1) +
                          accurate_hypot(x2 - cx, y2 - cy)) *
                         0.8;
        long long base = saturating_int(std::max(chord_d, curve_d) / 5.0);
        base = std::max(6LL, std::min(64LL, base));

        if (chord_d > 0.0)
        {
            double dx = x2 - x1;
            double dy = y2 - y1;
            double t = ((cx - x1) * dx + (cy - y1) * dy) / (dx * dx + dy * dy);
            t = std::max(0.0, std::min(1.0, t));
            double offset = accurate_hypot(cx - (x1 + t * dx), cy - (y1 + t * dy));
            base += saturating_int(offset / 5.0);
        }

        return static_cast<int>(std::max(6LL, std::min(64LL, base)));
    }

    // A control point within 1e-9 x chord length of an end point means "no curve": the straight chord.
    // The band is relative to the chord length, not to the coordinates.
    inline constexpr double STRAIGHT_CHORD_EPSILON = 1e-9;

    inline bool control_is_at_an_end(double x1, double y1, double x2, double y2,
                                     double cx, double cy)
    {
        double nearest = std::min(accurate_hypot(cx - x1, cy - y1),
                                  accurate_hypot(cx - x2, cy - y2));
        return nearest <= STRAIGHT_CHORD_EPSILON * accurate_hypot(x2 - x1, y2 - y1);
    }

    inline std::vector<std::pair<double, double>> straight_chord_points(
        double x1, double y1, double x2, double y2, int segments)
    {
        std::vector<std::pair<double, double>> points;
        points.reserve(static_cast<size_t>(segments) + 1);
        for (int i = 0; i <= segments; ++i)
        {
            double t = static_cast<double>(i) / segments;
            double u = 1.0 - t;
            points.push_back({u * x1 + t * x2, u * y1 + t * y2});
        }
        return points;
    }

    // Floored float modulo (Python's '%' on floats): the result takes the sign of the modulus.
    inline double floor_mod(double value, double modulus)
    {
        double mod = std::fmod(value, modulus);
        if (mod != 0.0)
        {
            if ((modulus < 0.0) != (mod < 0.0))
                mod += modulus;
        }
        else
        {
            mod = std::copysign(0.0, modulus);
        }
        return mod;
    }

    inline std::vector<std::pair<double, double>> advanced_curve_points(
        double x1, double y1, double x2, double y2,
        double cx, double cy, int segments)
    {
        if (control_is_at_an_end(x1, y1, x2, y2, cx, cy))
            return straight_chord_points(x1, y1, x2, y2, segments);

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
            double radius1 = accurate_hypot(center_x - x1, center_y - y1);
            double radius2 = accurate_hypot(center_x - x2, center_y - y2);
            double radius_c = accurate_hypot(center_x - cx, center_y - cy);
            if (std::abs(radius1 - radius2) < 0.1 &&
                std::abs(radius1 - radius_c) < 0.1)
            {
                double start_angle = std::atan2(y1 - center_y, x1 - center_x);
                double end_angle = std::atan2(y2 - center_y, x2 - center_x);
                double control_angle = std::atan2(cy - center_y, cx - center_x);
                // The arc always runs from the start through the control point to the end.
                double ccw_to_end = floor_mod(end_angle - start_angle, 2.0 * M_PI);
                double ccw_to_control = floor_mod(control_angle - start_angle, 2.0 * M_PI);
                double angle_diff;
                if (ccw_to_control <= ccw_to_end)
                    angle_diff = ccw_to_end;
                else
                    angle_diff = ccw_to_end - 2.0 * M_PI;
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
            total += accurate_hypot(points[i + 1].first - points[i].first,
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

    // use_absolute: area conditions use the magnitudes of rise and run; every other type needs strictly
    // positive values to form a slope.
    inline double calc_slope_factor(double rise, double run, bool use_absolute = false)
    {
        if (use_absolute)
        {
            rise = std::abs(rise);
            run = std::abs(run);
        }
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

    class UnsupportedConditionTypeError : public std::invalid_argument
    {
    public:
        explicit UnsupportedConditionTypeError(const std::string &message)
            : std::invalid_argument(message) {}
    };

    // Only Linear (0), Area (1), Count (2) and Attachment (3) are measured; any other stored Type
    // (corrupted data) is unsupported.
    inline bool is_supported_condition_type(ConditionType condition_type)
    {
        return condition_type == ConditionType::Linear ||
               condition_type == ConditionType::Area ||
               condition_type == ConditionType::Count ||
               condition_type == ConditionType::Attachment;
    }

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
        int curve = -1,
        bool round_quantity = false, double round_up = 0.0)
    {
        if (!is_supported_condition_type(condition_type))
        {
            throw UnsupportedConditionTypeError(
                "Unsupported condition type " +
                std::to_string(static_cast<int>(condition_type)));
        }

        double slope = calc_slope_factor(rise, run, condition_type == ConditionType::Area);

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
            return convert_and_round_quantity(raw, uom, round_quantity, round_up);
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

            // An unsupported condition type reports (0, 0, 0) and never disturbs the other conditions.
            if (!is_supported_condition_type(cdn.condition_type))
            {
                result[cond_uid] = {0.0, 0.0, 0.0};
                continue;
            }

            double total_q1 = 0.0, total_q2 = 0.0, total_q3 = 0.0;

            for (const Takeoff *t : pair.second)
            {
                // Only AREA takeoffs with a parent are holes (accounted for under their parent);
                // every other condition type counts every takeoff, e.g. an attachment placed on an area.
                if (cdn.is_area() && t->is_hole())
                    continue;

                // Gather the area holes and attachment dimensions of this takeoff's children.
                // A child counts only when its condition exists and it is on the same page.
                std::vector<std::vector<double>> hole_pos;
                double attachment_footprint = 0.0;
                double attachment_perimeter = 0.0;
                auto hit = holes_by_parent.find(t->uid);
                if (hit != holes_by_parent.end())
                {
                    for (const Takeoff *child : hit->second)
                    {
                        auto child_cit = conditions.find(child->condition_uid);
                        if (child_cit == conditions.end() || child->page_uid != t->page_uid)
                            continue;
                        const Condition &child_cdn = child_cit->second;
                        if (child_cdn.is_area() && !child->position.empty())
                        {
                            hole_pos.push_back(child->position);
                        }
                        else if (child_cdn.is_attachment())
                        {
                            attachment_footprint += child_cdn.width * child_cdn.depth;
                            attachment_perimeter += 2.0 * (child_cdn.width + child_cdn.depth);
                        }
                    }
                }

                double sign = t->is_negative ? -1.0 : 1.0;

                QuantityResult qr = calculate_condition_quantities(
                    cdn.condition_type,
                    cdn.calc_type1, cdn.calc_type2, cdn.calc_type3,
                    cdn.uom1, cdn.uom2, cdn.uom3,
                    cdn.width, cdn.height, cdn.depth, cdn.thickness,
                    t->position, hole_pos,
                    attachment_footprint, attachment_perimeter,
                    cdn.rise, cdn.run,
                    cdn.grid_size1, cdn.grid_size2, cdn.gap,
                    t->curve,
                    cdn.round_quantity, cdn.round_up);

                total_q1 += qr.q1 * sign;
                total_q2 += qr.q2 * sign;
                total_q3 += qr.q3 * sign;
            }

            result[cond_uid] = {total_q1, total_q2, total_q3};
        }

        return result;
    }

} // namespace ost_domain

#if defined(__clang__)
#pragma STDC FP_CONTRACT DEFAULT
#endif
