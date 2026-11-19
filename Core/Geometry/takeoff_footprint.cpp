// Plain IEEE double arithmetic, like Windows (no fused multiply-add): the corner and curve arithmetic below is compared
// with the Python / MSVC values.
#include "strict_fp.hpp"
#include "takeoff_footprint.hpp"
#include "page_transform.hpp"
#include "../Meshing/MeshGenerator.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ost_footprint
{
    using ost_domain::Condition;
    using ost_domain::ShapeId;
    using ost_domain::Takeoff;

    namespace
    {
        const double kPi = 3.14159265358979323846;
        constexpr double kPointsPerInch = 72.0;

        // Windows domain/entities/shape.py create_polygon_points: `sides` points on an ellipse (half_width x half_depth)
        // starting at angle_offset, rotated by `rotation` about the centre.
        void polygon_points(double center_x, double center_y, double half_width, int sides, double angle_offset,
                            double rotation, double half_depth, std::vector<Vec2> &out)
        {
            const double cos_r = std::cos(rotation), sin_r = std::sin(rotation);
            for (int i = 0; i < sides; ++i)
            {
                const double angle = angle_offset + static_cast<double>(i * 2) * kPi / sides;
                const double px = half_width * std::cos(angle);
                const double py = half_depth * std::sin(angle);
                out.push_back({center_x + px * cos_r - py * sin_r, center_y + px * sin_r + py * cos_r});
            }
        }

        // The corner list of create_isosceles_triangle_points / create_rhombus_points / create_rectangle_points: the
        // corners are rotated when the rotation is not zero, otherwise only moved to the centre.
        std::vector<Vec2> corner_points(double center_x, double center_y, const std::vector<Vec2> &corners,
                                        double rotation)
        {
            std::vector<Vec2> out;
            if (rotation != 0.0)
            {
                const double cos_r = std::cos(rotation), sin_r = std::sin(rotation);
                for (const auto &c : corners)
                {
                    const double rx = c[0] * cos_r - c[1] * sin_r;
                    const double ry = c[0] * sin_r + c[1] * cos_r;
                    out.push_back({center_x + rx, center_y + ry});
                }
            }
            else
            {
                for (const auto &c : corners)
                    out.push_back({center_x + c[0], center_y + c[1]});
            }
            return out;
        }

        // Windows OSTCoordinateSystem.ost_to_pdf_points: ost_to_real_units (no scaling for a ratio of 0) * 72.
        double ost_to_pdf_points(double ost, double scale_ratio)
        {
            return (scale_ratio == 0.0 ? ost : ost / scale_ratio) * kPointsPerInch;
        }

        // The checks of ost_coord::ost_to_pdf_coordinates that the page frame needs besides the page transform's own.
        void validate_scale_factors(double scale_factor1, double scale_factor2)
        {
            if (!std::isfinite(scale_factor1) || !std::isfinite(scale_factor2) || scale_factor1 <= 0.0 ||
                scale_factor2 <= 0.0)
                throw std::invalid_argument("Page scale factors must be finite and positive");
        }
    } // namespace

    std::vector<Vec2> count_vertices(double cx, double cy, int shape_id, double width, double depth, double rotation)
    {
        const double half_w = width / 2.0;
        const double half_d = depth / 2.0;
        const int circle = static_cast<int>(ShapeId::Circle);
        const int ellipse = static_cast<int>(ShapeId::Ellipse);
        if (shape_id == circle || shape_id == ellipse)
        {
            std::vector<Vec2> out;
            polygon_points(cx, cy, half_w, kEllipseSegments, 0.0, rotation, half_d, out);
            return out;
        }
        if (shape_id == static_cast<int>(ShapeId::Triangle))
            return corner_points(cx, cy, {{0.0, -half_d}, {half_w, half_d}, {-half_w, half_d}}, rotation);
        if (shape_id == static_cast<int>(ShapeId::Rhombus))
        {
            const double horizontal_scale = 0.575;
            return corner_points(cx, cy,
                                 {{0.0, -half_w},
                                  {half_w * horizontal_scale, 0.0},
                                  {0.0, half_w},
                                  {-half_w * horizontal_scale, 0.0}},
                                 rotation);
        }
        const bool hexagon = shape_id == static_cast<int>(ShapeId::Hexagon);
        const bool octagon = shape_id == static_cast<int>(ShapeId::Octagon);
        if (hexagon || octagon)
        {
            // POLYGON_SHAPES: HEXAGON (6, 0), OCTAGON (8, pi / 8), built about the origin and then moved.
            std::vector<Vec2> points;
            polygon_points(0.0, 0.0, half_w, hexagon ? 6 : 8, hexagon ? 0.0 : kPi / 8, rotation, half_w, points);
            std::vector<Vec2> out;
            for (const auto &p : points)
                out.push_back({cx + p[0], cy + p[1]});
            return out;
        }
        return corner_points(cx, cy, {{-half_w, -half_d}, {half_w, -half_d}, {half_w, half_d}, {-half_w, half_d}},
                             rotation);
    }

    PointShape resolve_point_takeoff_shape(const Condition &condition)
    {
        const int shape_id = static_cast<int>(condition.shape);
        double width = std::max(condition.width != 0.0 ? condition.width : 1.0, 1.0);
        double depth;
        if (shape_id == static_cast<int>(ShapeId::Square) || shape_id == static_cast<int>(ShapeId::Circle))
            depth = width;
        else
            depth = std::max(condition.depth != 0.0 ? condition.depth : width, 1.0);
        if (condition.is_count())
        {
            const double display_scale = std::max(condition.display_size, 0.1) / 100.0;
            width *= display_scale;
            depth *= display_scale;
        }
        return {shape_id, width, depth};
    }

    std::array<double, 2> apply_minimum_point_takeoff_size(double width, double depth, double minimum_dimension)
    {
        const double smaller_dimension = std::min(width, depth);
        if (smaller_dimension < minimum_dimension)
        {
            // Windows divides by zero (ZeroDivisionError) for a smaller side of 0 and scales by a negative factor for a
            // negative one: defined here as the minimum on both sides.
            if (!(smaller_dimension > 0.0))
                return {minimum_dimension, minimum_dimension};
            const double scale = minimum_dimension / smaller_dimension;
            return {width * scale, depth * scale};
        }
        return {width, depth};
    }

    std::vector<Vec2> straight_linear_vertices(double x1, double y1, double x2, double y2, double thickness)
    {
        const double dx = x2 - x1, dy = y2 - y1;
        const double length = std::sqrt(dx * dx + dy * dy);
        if (length < 0.001)
            return {};
        const double dx_n = dx / length, dy_n = dy / length;
        const double half_t = thickness / 2.0;
        const double px = -dy_n * half_t, py = dx_n * half_t;
        return {{x1 + px, y1 + py}, {x1 - px, y1 - py}, {x2 - px, y2 - py}, {x2 + px, y2 + py}};
    }

    std::array<double, 6> proc_curved_pos(const std::vector<double> &position)
    {
        if (position.size() < 6)
            throw std::invalid_argument("A curved position needs at least 6 values");
        double x1 = position[0], y1 = position[1], x2 = position[2], y2 = position[3];
        double cx = position[4], cy = position[5];
        if (position.size() >= 7)
        {
            const double offset = -position[6];
            const double mx = (x1 + x2) * 0.5, my = (y1 + y2) * 0.5;
            double dx = x2 - x1, dy = y2 - y1;
            const double length = ost_fp::hypot(dx, dy);
            if (length > 0)
            {
                dx /= length;
                dy /= length;
                cx = mx - dy * offset;
                cy = my + dx * offset;
            }
        }
        return {x1, y1, x2, y2, cx, cy};
    }

    void thick_curve_offsets(const std::vector<Vec2> &curve_pts, double thickness, std::vector<Vec2> &inner,
                             std::vector<Vec2> &outer)
    {
        const size_t n = curve_pts.size();
        inner.clear();
        outer.clear();
        inner.reserve(n);
        outer.reserve(n);
        const double half_t = thickness * 0.5;
        for (size_t i = 0; i < n; ++i)
        {
            const double cx = curve_pts[i][0], cy = curve_pts[i][1];
            double dx, dy;
            if (i == 0 && n > 1)
            {
                dx = curve_pts[1][0] - cx;
                dy = curve_pts[1][1] - cy;
            }
            else if (i == n - 1)
            {
                dx = cx - curve_pts[i - 1][0];
                dy = cy - curve_pts[i - 1][1];
            }
            else
            {
                dx = (curve_pts[i + 1][0] - curve_pts[i - 1][0]) * 0.5;
                dy = (curve_pts[i + 1][1] - curve_pts[i - 1][1]) * 0.5;
            }
            const double length = ost_fp::hypot(dx, dy);
            if (length > 0)
            {
                dx /= length;
                dy /= length;
            }
            const double nx = -dy, ny = dx;
            inner.push_back({cx + nx * half_t, cy + ny * half_t});
            outer.push_back({cx - nx * half_t, cy - ny * half_t});
        }
    }

    std::vector<Vec2> curved_linear_vertices(double x1, double y1, double x2, double y2, double cx, double cy,
                                             double thickness, int num_segments)
    {
        // gen_curve_pts is the curve generator of the 3D walls (ost_meshing::generate_curve_points, the same code as
        // linear_geom.cpp) in the mesh frame (x, z) = (X, -Y): handing it -y and negating its y is exact, so the points are
        // the Windows ones.
        const auto mesh_pts = ost_meshing::generate_curve_points(x1, -y1, x2, -y2, cx, -cy, num_segments);
        std::vector<Vec2> curve_pts;
        curve_pts.reserve(mesh_pts.size());
        for (const auto &p : mesh_pts)
            curve_pts.push_back({p.first, -p.second});
        if (curve_pts.size() < 2)
            return {};
        std::vector<Vec2> inner, outer;
        thick_curve_offsets(curve_pts, thickness, inner, outer);
        std::vector<Vec2> vertices = inner;
        vertices.insert(vertices.end(), outer.rbegin(), outer.rend());
        return vertices;
    }

    double rendered_linear_thickness(double thickness_ost, double scale_ratio, double view_scale, double minimum)
    {
        const double thickness = thickness_ost != 0.0 ? thickness_ost : 1.0;
        const double thickness_px = ost_to_pdf_points(thickness, scale_ratio) * view_scale;
        return std::max(thickness_px, minimum);
    }

    Vec2 scene_to_canvas(const PageFrame &frame, const Vec2 &scene_px)
    {
        const auto p = ost_page_transform::transform_top_left_point(
            frame.width_pts, frame.height_pts, frame.rotation, frame.flip_x, frame.flip_y,
            scene_px[0] / frame.view_scale, scene_px[1] / frame.view_scale);
        return {p[0], p[1]};
    }

    std::vector<Vec2> scene_footprint(const Takeoff &takeoff, const Condition &condition, double scale_ratio,
                                      double view_scale, double minimum_linear_thickness)
    {
        // transform_vertices_to_2d: position * (72 * view_scale / scale_ratio)
        const double factor = kPointsPerInch * view_scale / scale_ratio;
        const std::vector<double> &position = takeoff.position;
        if (condition.is_area())
        {
            if (position.size() < 6 || position.size() % 2)
                return {};
            std::vector<Vec2> out;
            out.reserve(position.size() / 2);
            for (size_t i = 0; i + 1 < position.size(); i += 2)
                out.push_back({position[i] * factor, position[i + 1] * factor});
            return out;
        }
        if (condition.is_linear())
        {
            if (position.size() < 4)
                return {};
            const double thickness =
                rendered_linear_thickness(condition.thickness, scale_ratio, view_scale, minimum_linear_thickness);
            if (takeoff.curve >= 0 && position.size() >= 6)
            {
                const auto c = proc_curved_pos(position);
                return curved_linear_vertices(c[0] * factor, c[1] * factor, c[2] * factor, c[3] * factor, c[4] * factor,
                                              c[5] * factor, thickness);
            }
            return straight_linear_vertices(position[0] * factor, position[1] * factor, position[2] * factor,
                                            position[3] * factor, thickness);
        }
        if (condition.is_count() || condition.is_attachment())
        {
            if (position.size() < 2)
                return {};
            const PointShape shape = resolve_point_takeoff_shape(condition);
            const double width_px = ost_to_pdf_points(shape.width, scale_ratio) * view_scale;
            const double depth_px = ost_to_pdf_points(shape.depth, scale_ratio) * view_scale;
            const auto size = apply_minimum_point_takeoff_size(width_px, depth_px, kMinimumRenderedPointTakeoffSize);
            return count_vertices(position[0] * factor, position[1] * factor, shape.shape_id, size[0], size[1],
                                  takeoff.rotation);
        }
        return {};
    }

    std::vector<Vec2> canvas_footprint(const Takeoff &takeoff, const Condition &condition, const PageFrame &frame,
                                       double minimum_linear_thickness)
    {
        validate_scale_factors(frame.scale_factor1, frame.scale_factor2);
        ost_page_transform::output_dimensions(frame.width_pts, frame.height_pts, frame.rotation);
        const double scale_ratio = frame.scale_factor2 / frame.scale_factor1;
        std::vector<Vec2> out;
        for (const Vec2 &v :
             scene_footprint(takeoff, condition, scale_ratio, frame.view_scale, minimum_linear_thickness))
            out.push_back(scene_to_canvas(frame, v));
        return out;
    }

    Vec2 polygon_area_centroid(const std::vector<Vec2> &points)
    {
        const size_t n = points.size();
        double area = 0.0, cx = 0.0, cy = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double x0 = points[i][0], y0 = points[i][1];
            const double x1 = points[(i + 1) % n][0], y1 = points[(i + 1) % n][1];
            const double cross = x0 * y1 - x1 * y0;
            area += cross;
            cx += (x0 + x1) * cross;
            cy += (y0 + y1) * cross;
        }
        area *= 0.5;
        if (std::fabs(area) < 1e-10)
        {
            double sx = 0.0, sy = 0.0;
            for (const Vec2 &p : points)
            {
                sx += p[0];
                sy += p[1];
            }
            return {sx / static_cast<double>(n), sy / static_cast<double>(n)};
        }
        const double factor = 1.0 / (6.0 * area);
        return {cx * factor, cy * factor};
    }

    namespace
    {
        // Qt's qFuzzyCompare(double, double).
        bool fuzzy_equal(double a, double b)
        {
            return std::fabs(a - b) * 1000000000000.0 <= std::min(std::fabs(a), std::fabs(b));
        }

        // The elements of a QPainterPath that holds one closed polygon (moveTo, lineTo ..., closeSubpath): the points, then
        // the start point once more. QPainterPathPrivate::close() adds that line unless the last point already is the start
        // point, and snaps a last point that is fuzzy-equal to it.
        std::vector<Vec2> path_points_of(const std::vector<Vec2> &polygon)
        {
            std::vector<Vec2> pts = polygon;
            if (!pts.empty() && (pts.front()[0] != pts.back()[0] || pts.front()[1] != pts.back()[1]))
            {
                if (fuzzy_equal(pts.front()[0], pts.back()[0]) && fuzzy_equal(pts.front()[1], pts.back()[1]))
                    pts.back() = pts.front();
                else
                    pts.push_back(pts.front());
            }
            return pts;
        }

        // Qt's qt_painterpath_isect_line: the winding contribution of one edge for a ray to the left of `p`; horizontal
        // edges count nothing and the edge covers y in [y1, y2).
        void isect_line(const Vec2 &p1, const Vec2 &p2, const Vec2 &p, int &winding)
        {
            double x1 = p1[0], y1 = p1[1], x2 = p2[0], y2 = p2[1];
            const double y = p[1];
            int dir = 1;
            if (fuzzy_equal(y1, y2))
                return;
            if (y2 < y1)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
                dir = -1;
            }
            if (y >= y1 && y < y2)
            {
                const double x = x1 + ((x2 - x1) / (y2 - y1)) * (y - y1);
                if (x <= p[0])
                    winding += dir;
            }
        }

        // QPainterPath::contains(QPointF) of a path of closed polygons with the odd-even fill rule: a point on the border is
        // decided by the half-open scan conversion rule above (the centroid of an L-shape that lies on one of its edges is
        // "outside" for Qt).
        bool path_contains(const std::vector<std::vector<Vec2>> &contours, const Vec2 &p)
        {
            double left = std::numeric_limits<double>::infinity(), right = -left, top = left, bottom = -left;
            bool any = false;
            for (const auto &contour : contours)
                for (const Vec2 &v : contour)
                {
                    left = std::min(left, v[0]);
                    right = std::max(right, v[0]);
                    top = std::min(top, v[1]);
                    bottom = std::max(bottom, v[1]);
                    any = true;
                }
            // controlPointRect().contains(pt): a null rectangle contains nothing
            if (!any || left == right || top == bottom || p[0] < left || p[0] > right || p[1] < top || p[1] > bottom)
                return false;
            int winding = 0;
            for (const auto &contour : contours)
            {
                for (size_t i = 0; i + 1 < contour.size(); ++i)
                    isect_line(contour[i], contour[i + 1], p, winding);
                // implicitly close the subpath
                if (contour.size() > 1 && (contour.back()[0] != contour.front()[0] || contour.back()[1] != contour.front()[1]))
                    isect_line(contour.back(), contour.front(), p, winding);
            }
            return (winding % 2) != 0;
        }

        // _nearest_interior_anchor: the candidates in the order of Windows (the points a fraction of the way from each vertex
        // to the preferred point, then the centres of a 32 x 32 grid), the first one nearest to the preferred point.
        Vec2 nearest_interior_anchor(const std::vector<std::vector<Vec2>> &contours, const Vec2 &preferred,
                                     const std::vector<Vec2> &vertices)
        {
            std::vector<Vec2> candidates;
            for (const Vec2 &v : vertices)
                for (double fraction : {0.01, 0.05, 0.1, 0.25, 0.5})
                {
                    const Vec2 candidate = {v[0] + (preferred[0] - v[0]) * fraction,
                                            v[1] + (preferred[1] - v[1]) * fraction};
                    if (path_contains(contours, candidate))
                        candidates.push_back(candidate);
                }
            double left = std::numeric_limits<double>::infinity(), right = -left, top = left, bottom = -left;
            for (const auto &contour : contours)
                for (const Vec2 &v : contour)
                {
                    left = std::min(left, v[0]);
                    right = std::max(right, v[0]);
                    top = std::min(top, v[1]);
                    bottom = std::max(bottom, v[1]);
                }
            const double width = right - left, height = bottom - top;
            const int divisions = 32;
            if (width > 0 && height > 0)
                for (int row = 0; row < divisions; ++row)
                {
                    const double y = top + height * (row + 0.5) / divisions;
                    for (int column = 0; column < divisions; ++column)
                    {
                        const double x = left + width * (column + 0.5) / divisions;
                        if (path_contains(contours, {x, y}))
                            candidates.push_back({x, y});
                    }
                }
            if (candidates.empty())
                return preferred;
            size_t best = 0;
            double best_distance = std::numeric_limits<double>::infinity();
            for (size_t i = 0; i < candidates.size(); ++i)
            {
                const double dx = candidates[i][0] - preferred[0], dy = candidates[i][1] - preferred[1];
                const double distance = dx * dx + dy * dy;
                if (distance < best_distance)
                {
                    best_distance = distance;
                    best = i;
                }
            }
            return candidates[best];
        }

        // The area and the centroid of a closed polygon by the shoelace formula, signed area.
        double signed_area(const std::vector<Vec2> &polygon)
        {
            double area = 0.0;
            for (size_t i = 0; i < polygon.size(); ++i)
            {
                const Vec2 &a = polygon[i];
                const Vec2 &b = polygon[(i + 1) % polygon.size()];
                area += a[0] * b[1] - b[0] * a[1];
            }
            return area * 0.5;
        }
    } // namespace

    std::optional<Vec2> negative_marker_anchor(const std::vector<Vec2> &outline,
                                               const std::vector<std::vector<Vec2>> &holes)
    {
        std::vector<std::vector<Vec2>> contours;
        contours.push_back(path_points_of(outline));
        for (const auto &hole : holes)
            if (hole.size() >= 3)
                contours.push_back(path_points_of(hole));
        if (contours.front().size() < 3)
            return std::nullopt;

        std::vector<Vec2> vertices;
        for (const auto &contour : contours)
            vertices.insert(vertices.end(), contour.begin(), contour.end());

        Vec2 centroid;
        if (contours.size() == 1)
            centroid = polygon_area_centroid(contours.front());
        else
        {
            // the centroid of the region: the outline's area-weighted centroid less the holes' (a hole is subtracted)
            const double outer_area = std::fabs(signed_area(contours.front()));
            const Vec2 outer_centroid = polygon_area_centroid(contours.front());
            double area = outer_area;
            double sx = outer_area * outer_centroid[0], sy = outer_area * outer_centroid[1];
            for (size_t i = 1; i < contours.size(); ++i)
            {
                const double hole_area = std::fabs(signed_area(contours[i]));
                const Vec2 hole_centroid = polygon_area_centroid(contours[i]);
                area -= hole_area;
                sx -= hole_area * hole_centroid[0];
                sy -= hole_area * hole_centroid[1];
            }
            if (std::fabs(area) < 1e-10)
                centroid = polygon_area_centroid(vertices);
            else
                centroid = {sx / area, sy / area};
        }
        if (path_contains(contours, centroid))
            return centroid;
        return nearest_interior_anchor(contours, centroid, vertices);
    }

    std::optional<Vec2> canvas_marker_anchor(const Takeoff &takeoff, const Condition &condition,
                                             const std::vector<std::vector<double>> &hole_positions,
                                             const PageFrame &frame)
    {
        validate_scale_factors(frame.scale_factor1, frame.scale_factor2);
        ost_page_transform::output_dimensions(frame.width_pts, frame.height_pts, frame.rotation);
        const double scale_ratio = frame.scale_factor2 / frame.scale_factor1;
        const std::vector<Vec2> outline = scene_footprint(takeoff, condition, scale_ratio, frame.view_scale);
        if (outline.empty())
            return std::nullopt;
        // the holes are area takeoffs: their scene path is the vertices of the position (_create_area_path)
        std::vector<std::vector<Vec2>> holes;
        const double factor = kPointsPerInch * frame.view_scale / scale_ratio;
        for (const std::vector<double> &position : hole_positions)
        {
            if (position.size() < 6 || position.size() % 2)
                continue;
            std::vector<Vec2> hole;
            for (size_t i = 0; i + 1 < position.size(); i += 2)
                hole.push_back({position[i] * factor, position[i + 1] * factor});
            holes.push_back(std::move(hole));
        }
        const std::optional<Vec2> anchor = negative_marker_anchor(outline, holes);
        if (!anchor)
            return std::nullopt;
        return scene_to_canvas(frame, *anchor);
    }
}
