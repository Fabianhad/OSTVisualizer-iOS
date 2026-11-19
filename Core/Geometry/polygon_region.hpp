#pragma once
// Polygon region predicates of the Windows plan view (presentation/components/plan_view/components/geometry_utils.py:
// closed_polygon_path / position_polygon_path / path_is_inside / path_intersects_any / path_fits_inside_excluding).
//
// Windows builds one closed QPainterPath (odd-even fill) per polygon and asks Qt's path clipper two questions:
//   path_is_inside(candidate, parent)  = both paths non-empty and candidate.subtracted(parent).isEmpty()
//   path_intersects_any(candidate, xs) = candidate non-empty and, for some non-empty x, candidate.intersects(x)
// Qt's answers on polygons are those of the closed regions (verified against PySide6 on 60000 random polygon pairs
// on small integer lattices, with many touching cases, 0 mismatches):
//   - "inside" means the candidate region is a subset of the parent region; sharing an edge or a vertex with the parent
//     boundary is still inside, and a polygon is inside itself;
//   - "intersects" is true as soon as the closed regions share a point: touching edges, a shared vertex and
//     containment all count (like Qt, containment without boundary contact is decided from the first vertex of each
//     polygon only); a gap of more than rounding noise does not.
// Qt's clipper snaps near-coincident geometry at about 1e-12 of the polygon size, so a candidate that leaves the
// parent by less than that is still inside there; path_is_inside has the same relative tolerance (see the .cpp).
// Known residue (not reproduced, measured against PySide6): for "intersects" Qt also reports contact for parallel,
// overlapping, NON axis-aligned edges up to about 1e-12 apart (scene units), and for vertices that touch a slanted edge
// only up to rounding noise it answers differently from exact predicates in about 1 to 2 percent of such cases. Here
// "intersects" uses exact predicates, so contact that exists only within rounding noise can differ; exactly shared
// boundaries (the same coordinates, axis-aligned edges, lattice values) are identical.
//
// Known residue 2 (not reproduced, measured against PySide6 by the reviewer on about 40000 polygon pairs and 90000
// validator calls: every difference was one of these two classes, and the class below is unreachable from the app):
//   - degenerate candidates: zero-area polygons (collinear or repeated vertices) and weakly simple polygons with a
//     zero-width part (a vertex lying on one of its own edges, a backtracking spur). Qt drops the zero-width part; here
//     its boundary pieces are tested like any other piece, so such a candidate that Qt accepts can be rejected here.
//     polygon_is_valid (used by every drag) rejects all of them before the check is reached;
//   - a self-intersecting PARENT (bow-tie, pentagram): a candidate equal to the even-covered pocket of the parent is
//     "inside" here (its boundary lies on the parent's boundary) and not for Qt.
// Pointed overshoots (a vertex poking out of a parent corner or slanted edge) of up to about 1e-8 of the shape size are
// often (shape dependent) still "inside" for Qt (the sliver has no area worth keeping) and "outside" here beyond 1e-12
// of the candidate size.
// Edits come from snapped screen drags (increments of 1e-2 and up) or rounding noise (1e-16), never from this band.
//
// Platform-independent (no Qt, no UIKit). Coordinates are raw OST units (the Windows paths live in scene units, a
// uniform scaling of these: every decision above is scale invariant).
//
// Not ported: Windows crashes (Qt segfault) on non-finite coordinates; here a polygon with a non-finite coordinate is an
// empty polygon (never inside anything, never reported as intersecting).
#include "vec_types.hpp"
#include <vector>

namespace ost_geom
{
    // One closed polygon: the vertices in order, the closing edge from the last to the first vertex is implicit. An
    // empty vector is the empty path.
    using Polygon = std::vector<ost_types::Vec2>;

    // Windows closed_polygon_path / position_polygon_path (identity coordinate system): flat [x0, y0, x1, y1, ...].
    // Empty for fewer than 6 values or an odd number of values (and, see above, for a non-finite value).
    Polygon closed_polygon(const std::vector<double> &points);

    // Windows path_is_inside(candidate, parent).
    bool path_is_inside(const Polygon &candidate, const Polygon &parent);

    // Windows path_intersects_any(candidate, exclusions): empty exclusions are skipped, an empty candidate gives false.
    bool path_intersects_any(const Polygon &candidate, const std::vector<Polygon> &exclusions);

    // Windows path_fits_inside_excluding(candidate, parent, exclusions).
    bool path_fits_inside_excluding(const Polygon &candidate, const Polygon &parent,
                                    const std::vector<Polygon> &exclusions);
}
