#pragma once
#include "../Geometry/mesh_data.hpp"
#include <array>
#include <cmath>
#include <cstdint>

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

    // The 12 triangles of a closed box whose bottom corners are vertices 0..3 and top corners 4..7, split along the same
    // diagonals as Windows (utils/mesh.py get_box_faces, used for the count box and for every straight wall) and facing
    // outward. `windows_to_ios[k]` is the iOS index of the vertex that sits where Windows vertex k sits: the Windows
    // table is applied through that relabelling, and every face is stored (a, c, b), which makes the box outward (Windows d1c430a
    // reaches the same outward faces through outward_faces, domain/mesh.py:62). Vertex order and face order are those of the iOS generators; only the
    // diagonal that splits each rectangular face is the Windows one (EXP-17: same triangle set as Windows).
    inline void add_box_faces(ost_geometry::CppMeshData &mesh, const std::array<uint32_t, 8> &windows_to_ios)
    {
        static const uint32_t windows_table[12][3] = {
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
        mesh.faces.clear();
        mesh.faces.reserve(12);
        for (const auto &f : windows_table)
            mesh.faces.push_back({windows_to_ios[f[0]], windows_to_ios[f[2]], windows_to_ios[f[1]]});
    }

    // Count box: iOS corner i is the footprint offset {-hw,-hd}, {hw,-hd}, {hw,hd}, {-hw,hd} in the iOS (x, z) frame, while
    // the Windows corner list is the same list in the (X, Y) frame with z = -Y: Windows corner k is iOS corner 3 - k.
    inline const std::array<uint32_t, 8> &count_box_windows_to_ios()
    {
        static const std::array<uint32_t, 8> labels = {3, 2, 1, 0, 7, 6, 5, 4};
        return labels;
    }

    // Straight wall and drop stub: iOS vertex 0 / 1 are start + perpendicular and start - perpendicular with the
    // perpendicular (-dz, dx) of the iOS (x, z) frame, the opposite side of the Windows (-dy, dx) in (X, Y) with z = -Y:
    // Windows vertex k is iOS vertex k with its lowest bit flipped (0 <-> 1, 2 <-> 3, 4 <-> 5, 6 <-> 7).
    inline const std::array<uint32_t, 8> &wall_windows_to_ios()
    {
        static const std::array<uint32_t, 8> labels = {1, 0, 3, 2, 5, 4, 7, 6};
        return labels;
    }

    template <typename T>
    inline T clamp_val(T v, T lo, T hi)
    {
        return (v < lo) ? lo : (v > hi) ? hi
                                        : v;
    }

}
