#include "polygon_region.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>

#if defined(__clang__)
// The Windows build (MSVC, no FMA) never fuses a*b - c*d; the exact-zero collinearity / on-segment tests below must see
// correctly rounded products on arm64 clang too.
#pragma STDC FP_CONTRACT OFF
#endif

namespace ost_geom
{
    namespace
    {
        using ost_types::Vec2;

        // A candidate that leaves its parent by less than this fraction of its own size is still "inside"
        // (Qt's clipper snaps near-coincident geometry at about 1e-12 of the polygon size; measured on QPainterPath
        // slivers and slanted edges, see polygon_region.hpp) ...
        const double kRelativeSnap = 1e-12;
        // ... plus the rounding noise of the sub-segment mid points computed below (a few ulp of the coordinates).
        const double kNoiseUlps = 8.0 * DBL_EPSILON;
        // Crossings are searched with this slack on the other segment's parameter, vertices closer than this to a
        // segment split it: extra split points never change the answer, missing ones could.
        const double kSplitSlack = 1e-9;

        double cross_2d(const Vec2 &o, const Vec2 &a, const Vec2 &b)
        {
            return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
        }

        // Closed segment [a, b] contains p (exactly collinear and inside the bounding box).
        bool on_segment(const Vec2 &p, const Vec2 &a, const Vec2 &b)
        {
            return cross_2d(a, b, p) == 0.0 &&
                   p[0] >= std::min(a[0], b[0]) && p[0] <= std::max(a[0], b[0]) &&
                   p[1] >= std::min(a[1], b[1]) && p[1] <= std::max(a[1], b[1]);
        }

        // The closed segments [a, b] and [c, d] share a point (crossing, touching, collinear overlap).
        bool segments_touch(const Vec2 &a, const Vec2 &b, const Vec2 &c, const Vec2 &d)
        {
            const double d1 = cross_2d(c, d, a);
            const double d2 = cross_2d(c, d, b);
            const double d3 = cross_2d(a, b, c);
            const double d4 = cross_2d(a, b, d);
            if (((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) &&
                ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0)))
                return true;
            return on_segment(a, c, d) || on_segment(b, c, d) || on_segment(c, a, b) || on_segment(d, a, b);
        }

        double squared_distance_to_segment(const Vec2 &p, const Vec2 &a, const Vec2 &b)
        {
            const double dx = b[0] - a[0];
            const double dy = b[1] - a[1];
            const double len2 = dx * dx + dy * dy;
            double t = 0.0;
            if (len2 > 0.0)
            {
                t = ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / len2;
                t = std::min(1.0, std::max(0.0, t));
            }
            const double ex = p[0] - (a[0] + t * dx);
            const double ey = p[1] - (a[1] + t * dy);
            return ex * ex + ey * ey;
        }

        // Odd-even (Qt's default fill rule) containment of p; the boundary itself is not decided here.
        bool inside_odd_even(const Vec2 &p, const Polygon &polygon)
        {
            bool inside = false;
            const size_t n = polygon.size();
            for (size_t i = 0, j = n - 1; i < n; j = i++)
            {
                const Vec2 &a = polygon[j];
                const Vec2 &b = polygon[i];
                if ((a[1] > p[1]) != (b[1] > p[1]))
                {
                    const double x = a[0] + (p[1] - a[1]) * (b[0] - a[0]) / (b[1] - a[1]);
                    if (p[0] < x)
                        inside = !inside;
                }
            }
            return inside;
        }

        // p is in the closed parent region, the boundary widened by tolerance (squared) counting as inside.
        bool in_closed_region(const Vec2 &p, const Polygon &parent, double tolerance_squared)
        {
            const size_t n = parent.size();
            for (size_t i = 0; i < n; ++i)
                if (squared_distance_to_segment(p, parent[i], parent[(i + 1) % n]) <= tolerance_squared)
                    return true;
            return inside_odd_even(p, parent);
        }

        struct Bounds
        {
            double min_x, min_y, max_x, max_y;
        };

        Bounds bounds_of(const Polygon &polygon)
        {
            Bounds b{polygon[0][0], polygon[0][1], polygon[0][0], polygon[0][1]};
            for (const Vec2 &p : polygon)
            {
                b.min_x = std::min(b.min_x, p[0]);
                b.max_x = std::max(b.max_x, p[0]);
                b.min_y = std::min(b.min_y, p[1]);
                b.max_y = std::max(b.max_y, p[1]);
            }
            return b;
        }

        double max_abs_coordinate(const Polygon &polygon)
        {
            double m = 0.0;
            for (const Vec2 &p : polygon)
                m = std::max(m, std::max(std::fabs(p[0]), std::fabs(p[1])));
            return m;
        }

        // Parameters t in [0, 1] along a + t (b - a) that cut the segment into pieces none of which crosses the
        // boundary of `parent`: both ends, every crossing, every parent vertex on or next to the segment.
        std::vector<double> split_parameters(const Vec2 &a, const Vec2 &b, const Polygon &parent)
        {
            std::vector<double> ts = {0.0, 1.0};
            const double dx = b[0] - a[0];
            const double dy = b[1] - a[1];
            const double len2 = dx * dx + dy * dy;
            const size_t n = parent.size();
            for (size_t i = 0; i < n; ++i)
            {
                const Vec2 &c = parent[i];
                const Vec2 &e = parent[(i + 1) % n];
                const double fx = e[0] - c[0];
                const double fy = e[1] - c[1];
                const double den = dx * fy - dy * fx;
                if (den != 0.0)
                {
                    const double t = ((c[0] - a[0]) * fy - (c[1] - a[1]) * fx) / den;
                    const double u = ((c[0] - a[0]) * dy - (c[1] - a[1]) * dx) / den;
                    if (t > 0.0 && t < 1.0 && u >= -kSplitSlack && u <= 1.0 + kSplitSlack)
                        ts.push_back(t);
                }
            }
            if (len2 > 0.0)
            {
                const double length = std::sqrt(len2);
                for (const Vec2 &q : parent)
                {
                    const double t = ((q[0] - a[0]) * dx + (q[1] - a[1]) * dy) / len2;
                    if (t > 0.0 && t < 1.0 && std::fabs(cross_2d(a, b, q)) / length <= kSplitSlack * std::max(1.0, length))
                        ts.push_back(t);
                }
            }
            std::sort(ts.begin(), ts.end());
            ts.erase(std::unique(ts.begin(), ts.end()), ts.end());
            return ts;
        }

        bool polygons_intersect(const Polygon &a, const Polygon &b)
        {
            const Bounds ba = bounds_of(a);
            const Bounds bb = bounds_of(b);
            if (std::max(ba.min_x, bb.min_x) > std::min(ba.max_x, bb.max_x) ||
                std::max(ba.min_y, bb.min_y) > std::min(ba.max_y, bb.max_y))
                return false;
            const size_t na = a.size();
            const size_t nb = b.size();
            for (size_t i = 0; i < na; ++i)
            {
                const Vec2 &p1 = a[i];
                const Vec2 &p2 = a[(i + 1) % na];
                for (size_t j = 0; j < nb; ++j)
                    if (segments_touch(p1, p2, b[j], b[(j + 1) % nb]))
                        return true;
            }
            // No boundary contact: one polygon lies inside the other or they are apart.
            return inside_odd_even(a[0], b) || inside_odd_even(b[0], a);
        }
    } // namespace

    Polygon closed_polygon(const std::vector<double> &points)
    {
        if (points.size() < 6 || points.size() % 2)
            return {};
        Polygon polygon;
        polygon.reserve(points.size() / 2);
        for (size_t i = 0; i + 1 < points.size(); i += 2)
        {
            if (!std::isfinite(points[i]) || !std::isfinite(points[i + 1]))
                return {};
            polygon.push_back({points[i], points[i + 1]});
        }
        return polygon;
    }

    // candidate.subtracted(parent).isEmpty(): the candidate region lies in the parent region. For polygons that is the
    // case when the candidate's boundary does, which is tested piece by piece: every edge is cut at the parent's
    // boundary and the middle of each piece must be in the closed parent region.
    bool path_is_inside(const Polygon &candidate, const Polygon &parent)
    {
        if (candidate.empty() || parent.empty())
            return false;
        const Bounds bc = bounds_of(candidate);
        const double extent = std::max(bc.max_x - bc.min_x, bc.max_y - bc.min_y);
        const double tolerance = std::max(kRelativeSnap * extent,
                                          kNoiseUlps * std::max(max_abs_coordinate(candidate), max_abs_coordinate(parent)));
        const double tolerance_squared = tolerance * tolerance;
        const size_t n = candidate.size();
        for (size_t i = 0; i < n; ++i)
        {
            const Vec2 &a = candidate[i];
            const Vec2 &b = candidate[(i + 1) % n];
            const std::vector<double> ts = split_parameters(a, b, parent);
            for (size_t k = 0; k + 1 < ts.size(); ++k)
            {
                const double tm = 0.5 * (ts[k] + ts[k + 1]);
                const Vec2 middle = {a[0] + (b[0] - a[0]) * tm, a[1] + (b[1] - a[1]) * tm};
                if (!in_closed_region(middle, parent, tolerance_squared))
                    return false;
            }
        }
        return true;
    }

    // candidate.intersects(exclusion) for some non-empty exclusion: the closed regions share a point.
    bool path_intersects_any(const Polygon &candidate, const std::vector<Polygon> &exclusions)
    {
        if (candidate.empty())
            return false;
        for (const Polygon &exclusion : exclusions)
            if (!exclusion.empty() && polygons_intersect(candidate, exclusion))
                return true;
        return false;
    }

    bool path_fits_inside_excluding(const Polygon &candidate, const Polygon &parent,
                                    const std::vector<Polygon> &exclusions)
    {
        return path_is_inside(candidate, parent) && !path_intersects_any(candidate, exclusions);
    }
}
