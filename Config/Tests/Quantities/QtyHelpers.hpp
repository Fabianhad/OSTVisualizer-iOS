// Shared helpers for the Quantities host tests (ports of the Windows quantity tests).
//
// Windows oracle: tests/domain/services/test_uom_service.py (module helpers `_quantities`,
// SQUARE_12, HOLE_6, `_arc_length`, `_circumcircle_arc_length`) and
// tests/domain/services/test_condition_quantity_service.py.
#pragma once

#include "TestHarness.hpp"
#include "Domain/UomService.hpp"

#include <cmath>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// |a - b| <= |b| * rel   (b is the oracle value)
#define CHECK_REL(a, b, rel) CHECK_NEAR((a), (b), std::fabs(static_cast<double>(b)) * (rel))

namespace qty
{
    using namespace ost_domain;

    // unittest assertAlmostEqual(places=7): round(|a - b|, 7) == 0
    inline constexpr double TOL7 = 5e-8;

    inline const std::vector<double> SQUARE_12 = {0, 0, 12, 0, 12, 12, 0, 12};
    inline const std::vector<double> HOLE_6 = {0, 0, 6, 0, 6, 6, 0, 6};
    inline const std::vector<double> SQUARE_10 = {0, 0, 10, 0, 10, 10, 0, 10};

    // Mirrors tests/domain/services/test_uom_service.py::_quantities (every optional input has the
    // Windows default: no holes, no attachments, no slope, no grid, curve = -1, rounding off).
    struct QArgs
    {
        double width = 0.0, height = 0.0, depth = 0.0, thickness = 0.0;
        std::vector<double> position;
        std::vector<std::vector<double>> holes;
        double attachment_footprint = 0.0, attachment_perimeter = 0.0;
        double rise = 0.0, run = 0.0;
        double grid_size1 = 0.0, grid_size2 = 0.0, gap = 0.0;
        int curve = -1;
        bool round_quantity = false;
        double round_up = 0.0;
        int calc2 = 0, calc3 = 0, uom2 = 0, uom3 = 0;
    };

    inline QuantityResult quantities(ConditionType type, int calc1, int uom1, const QArgs &a = QArgs())
    {
        return calculate_condition_quantities(
            type, calc1, a.calc2, a.calc3, uom1, a.uom2, a.uom3,
            a.width, a.height, a.depth, a.thickness,
            a.position, a.holes,
            a.attachment_footprint, a.attachment_perimeter,
            a.rise, a.run,
            a.grid_size1, a.grid_size2, a.gap,
            a.curve, a.round_quantity, a.round_up);
    }

    inline Condition make_condition(const std::string &uid, ConditionType type, int calc1 = 0, int uom1 = 0)
    {
        Condition c;
        c.uid = uid;
        c.condition_type = type;
        c.calc_type1 = calc1;
        c.uom1 = uom1;
        return c;
    }

    inline Takeoff make_takeoff(const std::string &uid, const std::string &condition_uid,
                                std::vector<double> position, const std::string &page_uid = "",
                                const std::string &parent_uid = "0", bool is_negative = false,
                                int curve = -1)
    {
        Takeoff t;
        t.uid = uid;
        t.condition_uid = condition_uid;
        t.page_uid = page_uid;
        t.position = std::move(position);
        t.parent_uid = parent_uid;
        t.is_negative = is_negative;
        t.curve = curve;
        return t;
    }

    using CondMap = std::unordered_map<std::string, Condition>;

    inline CondMap cond_map(std::initializer_list<Condition> list)
    {
        CondMap m;
        for (const auto &c : list)
            m[c.uid] = c;
        return m;
    }

    // Python math.radians
    inline double radians(double degrees) { return degrees * (M_PI / 180.0); }

    // Python float '%' (result takes the sign of the divisor)
    inline double py_mod(double x, double m)
    {
        double r = std::fmod(x, m);
        if (r != 0.0)
        {
            if ((r < 0.0) != (m < 0.0))
                r += m;
        }
        else
        {
            r = std::copysign(0.0, m);
        }
        return r;
    }

    // Windows `_arc_length`: arc from start to end through the middle angle (independent oracle).
    inline double arc_length_oracle(double radius, double start_deg, double through_deg, double end_deg)
    {
        const double full = 360.0;
        double ccw_to_end = py_mod(end_deg - start_deg, full);
        double ccw_to_through = py_mod(through_deg - start_deg, full);
        double sweep = ccw_to_through <= ccw_to_end ? ccw_to_end : full - ccw_to_end;
        return radius * radians(sweep);
    }
} // namespace qty
