#pragma once
// Plain IEEE double arithmetic for the meshing code.
//
// The Windows geometry (cpp_extensions linear_geom / the Python mesh generators) is built without fused multiply-add:
// every a * b + c is two correctly rounded operations. A compiler that contracts them into one fma (AppleClang on arm64
// does by default, -ffp-contract=on) changes the last bit of the result. That is harmless almost everywhere, but a few
// Windows decisions sit on those last bits:
//   * the segment count of a curved wall truncates int(offset / 5): a 7-value curved wall whose perpendicular offset is a
//     multiple of 5 (the control point is rebuilt as chord midpoint + offset) lands exactly on the boundary, so one rounding
//     decides between n and n + 1 segments;
//   * the tangent of a degenerate loop (zero-length chord with a stored control point) is a rounding residue that
//     Windows normalises to a unit vector, while a fused evaluation gives exactly 0 and pinches the wall;
//   * `arc_len >= thickness` (drop stub length) on chords whose steps add up to exactly the thickness.
// Every translation unit of Core/Meshing and the earcut instantiation (Geometry/earcut_impl.cpp: an ear test on a hole
// that nearly touches an edge decides between different triangulations) include this first, so the results do not depend
// on the compiler. Include it BEFORE any header whose function templates must be compiled this way (earcut.hpp).
// (Not available for compilers that ignore the pragma; MSVC does not contract at its default settings.)
#if defined(__clang__)
#pragma clang fp contract(off)
#endif

#include <cmath>

namespace ost_fp
{
    // sqrt(x * x + y * y), the value the Windows build's std::hypot returns for ordinary coordinates: the MSVC CRT
    // computes exactly that (identical on 4 million random pairs), while other libms (Apple's among them) may round the
    // last bit differently. The Windows decisions that truncate a distance (the curved wall segment count int(offset / 5),
    // the straight-chord epsilon) then differ by one segment on exact multiples. Inputs where the plain form over- or
    // underflows, and non-finite ones, take std::hypot.
    inline double hypot(double x, double y)
    {
        const double r = std::sqrt(x * x + y * y);
        return (r > 0.0 && std::isfinite(r)) ? r : std::hypot(x, y);
    }
}
