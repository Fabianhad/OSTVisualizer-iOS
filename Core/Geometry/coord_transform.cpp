#include "coord_transform.hpp"
#include "page_transform.hpp"
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace ost_coord
{

    constexpr double PDF_POINTS_PER_INCH = 72.0;
    constexpr double PI = 3.14159265358979323846;

    // Windows ost_to_pdf_coordinates: the scale factors are checked before the page.
    static void validate_scale_factors(double scale_factor1, double scale_factor2)
    {
        if (!std::isfinite(scale_factor1) || !std::isfinite(scale_factor2) ||
            scale_factor1 <= 0.0 || scale_factor2 <= 0.0)
            throw std::invalid_argument(
                "Page scale factors must be finite and positive");
    }

    Vec2 canvas_delta_to_ost(
        double dx, double dy,
        double scale_factor1, double scale_factor2,
        int rotation, bool flip_x, bool flip_y)
    {
        double units_per_inch = scale_factor2 / scale_factor1;
        double units_per_point = units_per_inch / PDF_POINTS_PER_INCH;

        // Linear part of the inverse page transform (Windows: QTransform translate(c) * rotate(-rotation) *
        // scale(flip) * translate(-c), inverted): rotate the canvas delta by +rotation, then undo the flips.
        int rot = ((rotation % 360) + 360) % 360;
        double x, y;
        switch (rot)
        {
        case 0:
            x = dx;
            y = dy;
            break;
        case 90:
            x = -dy;
            y = dx;
            break;
        case 180:
            x = -dx;
            y = -dy;
            break;
        case 270:
            x = dy;
            y = -dx;
            break;
        default:
        {
            double radians = rot * (PI / 180.0);
            double cos_a = std::cos(radians);
            double sin_a = std::sin(radians);
            x = cos_a * dx - sin_a * dy;
            y = sin_a * dx + cos_a * dy;
            break;
        }
        }
        if (flip_x)
            x = -x;
        if (flip_y)
            y = -y;
        return {x * units_per_point, y * units_per_point};
    }

    std::vector<Vec2> ost_to_pdf_coordinates(
        const std::vector<double> &ost_position,
        double pdf_width_pts, double pdf_height_pts,
        double scale_factor1, double scale_factor2,
        int rotation,
        bool flip_x, bool flip_y)
    {
        size_t len = ost_position.size();
        if (len < 2)
            return {};

        validate_scale_factors(scale_factor1, scale_factor2);

        double points_per_unit =
            PDF_POINTS_PER_INCH * scale_factor1 / scale_factor2;

        auto output_size = ost_page_transform::output_dimensions(
            pdf_width_pts, pdf_height_pts, rotation);

        std::vector<Vec2> vertices;
        vertices.reserve(len / 2);

        for (size_t i = 0; i + 1 < len; i += 2)
        {
            auto point = ost_page_transform::transform_top_left_point(
                pdf_width_pts,
                pdf_height_pts,
                rotation,
                flip_x,
                flip_y,
                ost_position[i] * points_per_unit,
                ost_position[i + 1] * points_per_unit);
            vertices.push_back({point[0], output_size[1] - point[1]});
        }
        return vertices;
    }

    Vec2 ost_to_canvas_point(
        double ost_x, double ost_y,
        double pdf_width_pts, double pdf_height_pts,
        double scale_factor1, double scale_factor2,
        int rotation, bool flip_x, bool flip_y)
    {
        validate_scale_factors(scale_factor1, scale_factor2);

        double points_per_unit =
            PDF_POINTS_PER_INCH * scale_factor1 / scale_factor2;

        ost_page_transform::output_dimensions(
            pdf_width_pts, pdf_height_pts, rotation);

        auto point = ost_page_transform::transform_top_left_point(
            pdf_width_pts,
            pdf_height_pts,
            rotation,
            flip_x,
            flip_y,
            ost_x * points_per_unit,
            ost_y * points_per_unit);
        return {point[0], point[1]};
    }

    Vec2 canvas_to_ost_point(
        double canvas_x, double canvas_y,
        double pdf_width_pts, double pdf_height_pts,
        double scale_factor1, double scale_factor2,
        int rotation, bool flip_x, bool flip_y)
    {
        validate_scale_factors(scale_factor1, scale_factor2);

        ost_page_transform::output_dimensions(
            pdf_width_pts, pdf_height_pts, rotation);
        if (!std::isfinite(canvas_x) || !std::isfinite(canvas_y))
            throw std::invalid_argument("Page coordinates must be finite");
        int normalized = ost_page_transform::normalize_quarter_turn(rotation);

        // Inverse of ost_page_transform::transform_top_left_point: undo the rotation, then the flips.
        double transformed_x, transformed_y;
        switch (normalized)
        {
        case 90:
            transformed_x = pdf_width_pts - canvas_y;
            transformed_y = canvas_x;
            break;
        case 180:
            transformed_x = pdf_width_pts - canvas_x;
            transformed_y = pdf_height_pts - canvas_y;
            break;
        case 270:
            transformed_x = canvas_y;
            transformed_y = pdf_height_pts - canvas_x;
            break;
        default:
            transformed_x = canvas_x;
            transformed_y = canvas_y;
            break;
        }
        double page_x = flip_x ? pdf_width_pts - transformed_x : transformed_x;
        double page_y = flip_y ? pdf_height_pts - transformed_y : transformed_y;

        double units_per_point = (scale_factor2 / scale_factor1) / PDF_POINTS_PER_INCH;
        return {page_x * units_per_point, page_y * units_per_point};
    }

}
