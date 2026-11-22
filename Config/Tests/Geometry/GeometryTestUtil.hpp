// Shared helpers of the Geometry area tests (not a test_*.cpp file, so CMake does not compile it alone).
#pragma once

#include "TestHarness.hpp"

#include "Geometry/coord_transform.hpp"
#include "Geometry/geom_utils.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace geo_test
{
    using ost_types::Vec2;

    // Relative tolerance 1e-9 of the larger magnitude (at least 1.0): used where Windows is not bit-exact
    // (QTransform arithmetic, non-representable scale factors).
    inline double rel_tol(double a, double b)
    {
        return 1e-9 * std::max({1.0, std::fabs(a), std::fabs(b)});
    }

    // Windows ost_to_pdf_coordinates (cpp_extensions/src/coord_transform/coord_transform.cpp): 8 arguments.
    inline std::vector<Vec2> pdf(const std::vector<double> &position, double width_pts, double height_pts,
                                 double scale_factor1, double scale_factor2, int rotation, bool flip_x, bool flip_y)
    {
        return ost_coord::ost_to_pdf_coordinates(position, width_pts, height_pts, scale_factor1, scale_factor2,
                                                 rotation, flip_x, flip_y);
    }

    // True when fn() throws std::invalid_argument (the C++ counterpart of the ValueError Windows raises);
    // its message goes to *what. Any other outcome (no throw, other exception type) returns false.
    template <class F>
    bool throws_invalid_argument(F &&fn, std::string *what = nullptr)
    {
        try
        {
            fn();
        }
        catch (const std::invalid_argument &e)
        {
            if (what)
                *what = e.what();
            return true;
        }
        catch (...)
        {
            return false;
        }
        return false;
    }

    inline void check_vec2(const Vec2 &got, double ex, double ey, bool exact)
    {
        if (exact)
        {
            CHECK_EQ(got[0], ex);
            CHECK_EQ(got[1], ey);
        }
        else
        {
            CHECK_NEAR(got[0], ex, rel_tol(got[0], ex));
            CHECK_NEAR(got[1], ey, rel_tol(got[1], ey));
        }
    }

    struct Flips
    {
        bool x, y;
    };
    // Order of the Windows tables: none, flip_y only, flip_x only, both.
    inline const Flips (&all_flips())[4]
    {
        static const Flips f[4] = {{false, false}, {false, true}, {true, false}, {true, true}};
        return f;
    }

    inline std::string case_name(int rotation, bool flip_x, bool flip_y)
    {
        return "rot=" + std::to_string(rotation) + " flip_x=" + (flip_x ? "1" : "0") + " flip_y=" + (flip_y ? "1" : "0");
    }
} // namespace geo_test
