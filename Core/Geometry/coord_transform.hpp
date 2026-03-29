#pragma once
#include "vec_types.hpp"
#include <vector>

namespace ost_coord
{
    using ost_types::Vec2;

    Vec2 canvas_delta_to_ost(
        double dx, double dy,
        double scale_factor1, double scale_factor2,
        int rotation);

    std::vector<Vec2> ost_to_pdf_coordinates(
        const std::vector<double> &ost_position,
        double pdf_width_pts, double pdf_height_pts,
        double scale_factor1, double scale_factor2,
        int rotation,
        bool flip_x, bool flip_y,
        double coord_scale_x, double coord_scale_y,
        bool is_page_rotated, bool auto_rotate_180,
        double coord_offset_x, double coord_offset_y);

}
