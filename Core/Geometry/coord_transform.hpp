#pragma once
#include "vec_types.hpp"
#include <vector>

namespace ost_coord
{
    using ost_types::Vec2;

    // Page display model (Windows page_transform.hpp, see page_transform.hpp): OST positions live in the UNROTATED
    // page (pdf_width_pts x pdf_height_pts, top-left origin, points = ost * 72 * sf1 / sf2); the displayed page is
    // that page flipped (flip_x / flip_y) and then rotated counter-clockwise by `rotation`. The CANVAS of the point
    // maps and of the drag delta below is that displayed (output) page: width x height, swapped for 90/270, top-left
    // origin, Y down, in points (ost_page_transform::output_dimensions gives its size).

    // Drag delta: a canvas displacement (points of the displayed page) as an OST displacement. Windows
    // drag_handler.scene_to_ost_delta: the linear part of the inverse page transform, then * (sf2 / sf1) / 72.
    // Does not validate (as Windows); a rotation that is not a quarter turn rotates by that angle (QTransform).
    Vec2 canvas_delta_to_ost(
        double dx, double dy,
        double scale_factor1, double scale_factor2,
        int rotation, bool flip_x, bool flip_y);

    // OST point -> canvas point of the displayed page (top-left, Y down); the Windows page transform of
    // ost * points_per_unit. Throws std::invalid_argument for an invalid page contract (see ost_to_pdf_coordinates).
    Vec2 ost_to_canvas_point(
        double ost_x, double ost_y,
        double pdf_width_pts, double pdf_height_pts,
        double scale_factor1, double scale_factor2,
        int rotation, bool flip_x, bool flip_y);

    // Inverse of ost_to_canvas_point (Windows selection_manager._scene_pos_to_ost); no grid snapping here.
    // Throws std::invalid_argument for an invalid page contract.
    Vec2 canvas_to_ost_point(
        double canvas_x, double canvas_y,
        double pdf_width_pts, double pdf_height_pts,
        double scale_factor1, double scale_factor2,
        int rotation, bool flip_x, bool flip_y);

    // Windows cpp_extensions ost_to_pdf_coordinates: one (x, y) pair per two input values (an incomplete trailing
    // value is ignored), returned Y-up in the output page: y' = output_height - y_top_left. Fewer than two input
    // values return {} without validation. Throws std::invalid_argument ("Page scale factors must be finite and
    // positive", "Page dimensions must be finite and positive", "Page rotation must be a multiple of 90 degrees",
    // "Page coordinates must be finite"), checked in that order.
    std::vector<Vec2> ost_to_pdf_coordinates(
        const std::vector<double> &ost_position,
        double pdf_width_pts, double pdf_height_pts,
        double scale_factor1, double scale_factor2,
        int rotation,
        bool flip_x, bool flip_y);

}
