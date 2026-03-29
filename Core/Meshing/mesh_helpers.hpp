#pragma once
#include "../Geometry/mesh_data.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ost_meshing
{

    struct SlopeParams
    {
        bool has_slope = false;
        bool slope_up = false;
        double v_chg = 0.0;
    };

    inline SlopeParams compute_slope(double rise, double run, double h_dist)
    {
        SlopeParams s;
        if (run == 0.0 || rise == 0.0)
            return s;
        s.has_slope = true;
        s.v_chg = h_dist * std::abs(rise) / std::abs(run);
        s.slope_up = (rise > 0 && run > 0) || (rise < 0 && run < 0);
        return s;
    }

    inline void add_box_faces(ost_geometry::CppMeshData &mesh)
    {
        mesh.faces = {
            {0, 1, 2},
            {0, 2, 3},
            {4, 6, 5},
            {4, 7, 6},
            {0, 3, 7},
            {0, 7, 4},
            {1, 0, 4},
            {1, 4, 5},
            {2, 1, 5},
            {2, 5, 6},
            {3, 2, 6},
            {3, 6, 7},
        };
    }

    template <typename T>
    inline T clamp_val(T v, T lo, T hi)
    {
        return (v < lo) ? lo : (v > hi) ? hi
                                        : v;
    }

}
