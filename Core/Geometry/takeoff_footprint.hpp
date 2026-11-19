#pragma once
// The 2D footprint of a takeoff, as the Windows plan view draws it (presentation/visualization/core/geometry/
// takeoff_geometry.py, domain/entities/shape.py and TakeoffRenderer._create_path in
// presentation/visualization/pdf/renderers/takeoff_renderer.py), platform independent.
//
// The Windows scene is measured in SCENE PIXELS: scene = ost / scale_ratio * 72 * view_scale, Y down, in the UNROTATED page.
// Sizes, minimums and curve offsets are computed in those pixels (a count item is at least 8 px, a linear band at least
// 2 px); the page transform (flips, then a counter-clockwise rotation about the page centre, ost_page_transform) is then
// applied to the whole item. The iOS canvas is measured in points (scene px / view_scale) and is the DISPLAYED page.
// view_scale is kInteractivePdfRenderScale (render_quality.INTERACTIVE_PDF_RENDER_SCALE) for a PDF page and for a blank
// page; the iOS app shows nothing else.
#include "../Domain/Condition.hpp"
#include "../Domain/Takeoff.hpp"
#include "vec_types.hpp"
#include <array>
#include <optional>
#include <vector>

namespace ost_footprint
{
    using ost_types::Vec2;

    // render_quality.INTERACTIVE_PDF_RENDER_SCALE: scene px per PDF point.
    constexpr double kInteractivePdfRenderScale = 3.0;
    // takeoff_geometry.MINIMUM_RENDERED_POINT_TAKEOFF_SIZE / MINIMUM_RENDERED_LINEAR_THICKNESS (scene px).
    constexpr double kMinimumRenderedPointTakeoffSize = 8.0;
    constexpr double kMinimumRenderedLinearThickness = 2.0;
    // compute_curved_linear_vertices(num_segments=24) and the 32-gon of an ellipse / circle.
    constexpr int kCurveSegments = 24;
    constexpr int kEllipseSegments = 32;

    // ---- shapes (shape.py, takeoff_geometry.compute_count_vertices) ----------------------------------------------------

    // The footprint of a count shape of the given size centred on (cx, cy), turned by `rotation` (radians, the Windows
    // matrix x' = x cos - y sin, y' = x sin + y cos in a Y-down frame) about its centre. Ellipse / circle: a 32-gon with the
    // radii width / 2 and depth / 2 starting at angle 0. Triangle: apex (0, -d/2), base (+-w/2, d/2). Rhombus: (0, -w/2),
    // (0.575 w/2, 0), (0, w/2), (-0.575 w/2, 0) from the width alone. Hexagon: 6 points from angle 0 and octagon: 8 points
    // from pi/8, both with the radius w/2 on both axes (the depth is ignored). Anything else is the rectangle
    // (-w/2, -d/2), (w/2, -d/2), (w/2, d/2), (-w/2, d/2).
    std::vector<Vec2> count_vertices(double cx, double cy, int shape_id, double width, double depth, double rotation);

    struct PointShape
    {
        int shape_id;
        double width; // OST units, display scale applied (count conditions only)
        double depth;
    };

    // resolve_point_takeoff_shape: width = max(width or 1.0, 1.0); depth = width for a square or a circle, otherwise
    // max(depth or width, 1.0); a COUNT condition scales both by max(display_size, 0.1) / 100 (an attachment is not scaled).
    // "x or y" is the Python truthiness: an exact 0 is replaced.
    PointShape resolve_point_takeoff_shape(const ost_domain::Condition &condition);

    // apply_minimum_point_takeoff_size: when the smaller side is below `minimum_dimension` both sides are scaled by the same
    // factor so that the smaller one is exactly the minimum. Defined for a smaller side that is not positive (Windows
    // divides by zero there): the minimum on both sides.
    std::array<double, 2> apply_minimum_point_takeoff_size(double width, double depth, double minimum_dimension);

    // ---- linear bands (takeoff_geometry.compute_*_linear_vertices, linear_geom.cpp) --------------------------------------

    // The band of a straight segment: (x1 + px, y1 + py), (x1 - px, y1 - py), (x2 - px, y2 - py), (x2 + px, y2 + py) with
    // (px, py) the half-thickness normal. Empty for a segment shorter than 0.001 (Windows None).
    std::vector<Vec2> straight_linear_vertices(double x1, double y1, double x2, double y2, double thickness);

    // linear_geom.proc_curved_pos for a position with 6 or more values: x1, y1, x2, y2 and the control point; with 7 or
    // more values the control is rebuilt from the chord midpoint and the perpendicular offset -position[6] (only when the
    // chord has a length; otherwise the stored control stays).
    std::array<double, 6> proc_curved_pos(const std::vector<double> &position);

    // linear_geom.gen_thick_curve_offsets: the two offset curves (inner, outer) at +-thickness/2 along the normal of the
    // curve points (the tangent of an inner point is half the vector from its previous to its next point).
    void thick_curve_offsets(const std::vector<Vec2> &curve_pts, double thickness, std::vector<Vec2> &inner,
                             std::vector<Vec2> &outer);

    // compute_curved_linear_vertices: the 24-segment curve (gen_curve_pts) through the end points and the control point,
    // thickened: inner offset, then the outer offset reversed (2 * (segments + 1) vertices). Empty when the curve has
    // fewer than two points.
    std::vector<Vec2> curved_linear_vertices(double x1, double y1, double x2, double y2, double cx, double cy,
                                             double thickness, int num_segments = kCurveSegments);

    // The thickness of a linear band in scene px (_create_linear_path): ost_to_pdf_points(thickness or 1.0) * view_scale,
    // at least `minimum` px.
    double rendered_linear_thickness(double thickness_ost, double scale_ratio, double view_scale,
                                     double minimum = kMinimumRenderedLinearThickness);

    // ---- a takeoff on the canvas -------------------------------------------------------------------------------------------

    // The page a takeoff is drawn on: the UNROTATED page in points and the page transform (ost_page_transform).
    struct PageFrame
    {
        double width_pts = 0.0;
        double height_pts = 0.0;
        double scale_factor1 = 1.0;
        double scale_factor2 = 1.0;
        int rotation = 0;
        bool flip_x = false;
        bool flip_y = false;
        double view_scale = kInteractivePdfRenderScale;
    };

    // The scene pixels of the unrotated page (x, y) as a point of the canvas, the DISPLAYED page in points, top-left origin,
    // Y down: (x / view_scale, y / view_scale) through ost_page_transform::transform_top_left_point.
    // Throws std::invalid_argument for an invalid page contract (non-positive page size, rotation that is not a multiple of
    // 90 degrees, non-finite point).
    Vec2 scene_to_canvas(const PageFrame &frame, const Vec2 &scene_px);

    // TakeoffRenderer._create_path in scene px (unrotated): the outline of a takeoff, empty when Windows draws nothing.
    //   area:        the vertices of a position of at least 6 values and an even count;
    //   linear:      position of at least 4 values; with curve >= 0 and at least 6 values the curved band, otherwise the
    //                straight band; the thickness is at least `minimum_linear_thickness` px;
    //   count / attachment: the shape of the condition (size rules above, minimum 8 px), turned by takeoff.rotation.
    std::vector<Vec2> scene_footprint(const ost_domain::Takeoff &takeoff, const ost_domain::Condition &condition,
                                      double scale_ratio, double view_scale,
                                      double minimum_linear_thickness = kMinimumRenderedLinearThickness);

    // scene_footprint of the page frame, then the page transform: the outline of the takeoff on the canvas (points).
    // `minimum_linear_thickness` (scene px) is raised for a touch hit area. Throws std::invalid_argument for an invalid page
    // contract (scale factors that are not finite and positive, see scene_to_canvas).
    std::vector<Vec2> canvas_footprint(const ost_domain::Takeoff &takeoff, const ost_domain::Condition &condition,
                                       const PageFrame &frame,
                                       double minimum_linear_thickness = kMinimumRenderedLinearThickness);

    // ---- negative marker anchor (takeoff_renderer._path_centroid) -----------------------------------------------------------

    // _calculate_polygon_centroid: the area-weighted centroid of a closed polygon; the vertex mean for an area below 1e-10.
    // Needs at least one point.
    Vec2 polygon_area_centroid(const std::vector<Vec2> &points);

    // _path_centroid of the path of a takeoff: the area centroid of the outline, or, when that point is not inside the path,
    // the interior point nearest to it (the points a fraction 0.01 .. 0.5 of the way from each vertex to the centroid, then
    // the centres of a 32 x 32 grid of the bounding box); the centroid itself when there is none (zero area). Empty (nullopt)
    // for fewer than 3 path points. `holes` are the contours subtracted from the outline (an area with backouts); the
    // inside test is odd-even over all contours. A path point list is the outline closed by one more point, like the
    // elements of a QPainterPath after closeSubpath.
    // Holes: Windows takes the points of the subtracted path as one polygon, which depends on the order Qt's path clipper
    // emits them; here the centroid of the region (outline minus holes) stands in for it, and the contract that the anchor
    // lies inside the visible fill (test_area_with_hole_uses_anchor_inside_visible_fill) is kept.
    std::optional<Vec2> negative_marker_anchor(const std::vector<Vec2> &outline,
                                               const std::vector<std::vector<Vec2>> &holes = {});

    // Where the marker of a negative takeoff sits on the canvas (points): negative_marker_anchor of the scene outline of the
    // takeoff (scene px, as Windows computes it) and of its holes (`hole_positions`: the OST positions of the area takeoffs
    // that are cut out of it; one of fewer than 6 values or an odd count is skipped), then through the page transform.
    // nullopt when the takeoff has no outline or the path has fewer than 3 points. Throws std::invalid_argument for an
    // invalid page contract.
    std::optional<Vec2> canvas_marker_anchor(const ost_domain::Takeoff &takeoff, const ost_domain::Condition &condition,
                                             const std::vector<std::vector<double>> &hole_positions,
                                             const PageFrame &frame);
}
