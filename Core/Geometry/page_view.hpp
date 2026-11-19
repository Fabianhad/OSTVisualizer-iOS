#pragma once
// How the plan view shows a page, platform independent (Windows application/services/page_load_strategy_service.py
// PageLoadStrategyService._resolve_pdf_page_size, presentation/visualization/pdf/page_cache.py _normalize_page_index,
// presentation/components/plan_view/components/zoom_handler.py _get_page_transform and pdf_renderer.cpp page_size).
//
// The canvas is the DISPLAYED page: the PDF page (width x height points, upright) mirrored by flip_x / flip_y about its
// centre and then turned counter-clockwise by the page rotation; its size is ost_page_transform::output_dimensions
// (width and height swapped for 90 and 270). The takeoffs go through the same transform (ost_coord, ost_footprint), so the
// background and the overlays always agree.
#include <array>

namespace ost_page_view
{
    // The page size Windows falls back to (Page.width_pts / height_pts missing): 612 x 792 points (US Letter).
    constexpr double kDefaultPageWidthPts = 612.0;
    constexpr double kDefaultPageHeightPts = 792.0;

    // The PDF page that shows a plan page: BidPages.Index1 - 1 (ost_domain::Page::page_index), kept inside the document
    // (page_cache._normalize_page_index: a negative index is 0, an index beyond the last page is the last page, a document
    // without pages gives 0).
    int normalized_pdf_page_index(int requested_index, int page_count);

    // The size PDFium reports for a page (FPDF_GetPageWidth / Height, the Windows page_size): the crop box (the media box when
    // the page has none) with the page's own /Rotate applied, so a 90 or 270 page is as wide as its box is tall.
    std::array<double, 2> effective_pdf_page_size(double box_width_pts, double box_height_pts, int intrinsic_rotation);

    // _resolve_pdf_page_size: the size of the page in the PDF when it could be read (both sides above 0), otherwise the stored
    // BidPages Width / Height (inches, x 72), otherwise 612 x 792. A stored side that is not a finite positive number is
    // missing (Windows would carry it into the transform and fail there).
    std::array<double, 2> resolve_pdf_page_size(double stored_width_inches, double stored_height_inches,
                                                double actual_width_pts, double actual_height_pts);

    // The page transform as an affine map of the unrotated page (width x height points, top-left origin, Y down) onto the
    // canvas: x' = a x + c y + tx, y' = b x + d y + ty (the CGAffineTransform convention). Throws std::invalid_argument for
    // an invalid page contract (ost_page_transform).
    struct PageAffine
    {
        double a, b, c, d, tx, ty;
    };
    PageAffine page_affine(double width_pts, double height_pts, int rotation, bool flip_x, bool flip_y);
}
