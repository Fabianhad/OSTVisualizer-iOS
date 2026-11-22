#include "page_view.hpp"
#include "page_transform.hpp"
#include <algorithm>
#include <cmath>

namespace ost_page_view
{
    int normalized_pdf_page_index(int requested_index, int page_count)
    {
        const int requested = std::max(0, requested_index);
        if (requested == 0)
            return 0;
        if (page_count <= 0)
            return 0;
        return std::min(requested, page_count - 1);
    }

    std::array<double, 2> effective_pdf_page_size(double box_width_pts, double box_height_pts, int intrinsic_rotation)
    {
        // FPDFPage_GetRotation is the /Rotate entry in quarter turns modulo 4: an odd number of them swaps the sides.
        const int quarter_turns = ((intrinsic_rotation / 90) % 4 + 4) % 4;
        if (quarter_turns % 2 == 1)
            return {box_height_pts, box_width_pts};
        return {box_width_pts, box_height_pts};
    }

    std::array<double, 2> resolve_pdf_page_size(double stored_width_inches, double stored_height_inches,
                                                double actual_width_pts, double actual_height_pts)
    {
        const auto positive = [](double v) { return std::isfinite(v) && v > 0.0; };
        if (positive(actual_width_pts) && positive(actual_height_pts))
            return {actual_width_pts, actual_height_pts};
        constexpr double kPointsPerInch = 72.0;
        return {positive(stored_width_inches) ? stored_width_inches * kPointsPerInch : kDefaultPageWidthPts,
                positive(stored_height_inches) ? stored_height_inches * kPointsPerInch : kDefaultPageHeightPts};
    }

    PageAffine page_affine(double width_pts, double height_pts, int rotation, bool flip_x, bool flip_y)
    {
        using ost_page_transform::transform_top_left_point;
        const auto origin = transform_top_left_point(width_pts, height_pts, rotation, flip_x, flip_y, 0.0, 0.0);
        const auto along_x = transform_top_left_point(width_pts, height_pts, rotation, flip_x, flip_y, 1.0, 0.0);
        const auto along_y = transform_top_left_point(width_pts, height_pts, rotation, flip_x, flip_y, 0.0, 1.0);
        // The linear part of a flip and a quarter turn is made of -1, 0 and 1 only; read off the unit vectors and snap them
        // so that the page size does not leave rounding noise in the matrix.
        return {std::round(along_x[0] - origin[0]), std::round(along_x[1] - origin[1]),
                std::round(along_y[0] - origin[0]), std::round(along_y[1] - origin[1]), origin[0], origin[1]};
    }
}
