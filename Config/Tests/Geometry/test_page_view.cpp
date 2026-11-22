// LP-16 (alias RPV-18 / RPV-19) and the display half of GEO-06: which PDF page a plan page shows, how big the canvas is and how
// the page background is turned onto it (PDFCanvasView.mm / PlanView.mm extracted to Core/Geometry/page_view.hpp).
//
// Windows oracle: tests/presentation/visualization/pdf/test_page_cache.py::test_invalid_pdf_page_index_is_normalized_before_cache_key
// and test_page_index_normalization_handles_non_pdf_empty_and_unreadable_sources (page_cache._normalize_page_index),
// tests/application/services/test_page_load_strategy_service.py (_resolve_pdf_page_size) and zoom_handler._get_page_transform
// (page_transform.hpp, the model the canvas shares with ost_coord / ost_footprint).
#include "GeometryTestUtil.hpp"

#include "Geometry/page_transform.hpp"
#include "Geometry/page_view.hpp"

#include <array>
#include <string>

using geo_test::throws_invalid_argument;
using ost_types::Vec2;

// Port of test_invalid_pdf_page_index_is_normalized_before_cache_key (3 pages: 99 -> page 2, 2 -> 2, -5 -> 0, 0 -> 0) and of
// the empty-document case (page_count 0: index 5 -> 0). iOS always showed the first page.
TEST(PageView, NormalizedPdfPageIndex)
{
    using ost_page_view::normalized_pdf_page_index;
    CHECK_EQ(normalized_pdf_page_index(99, 3), 2);
    CHECK_EQ(normalized_pdf_page_index(2, 3), 2);
    CHECK_EQ(normalized_pdf_page_index(-5, 3), 0);
    CHECK_EQ(normalized_pdf_page_index(0, 3), 0);
    CHECK_EQ(normalized_pdf_page_index(1, 3), 1);
    CHECK_EQ(normalized_pdf_page_index(3, 3), 2);
    CHECK_EQ(normalized_pdf_page_index(5, 0), 0);
    CHECK_EQ(normalized_pdf_page_index(0, 0), 0);
    CHECK_EQ(normalized_pdf_page_index(4, 1), 0);
}

// The size PDFium reports for a page (FPDF_GetPageWidth / Height): the crop box turned by the page's own /Rotate.
TEST(PageView, EffectivePdfPageSizeAppliesTheIntrinsicRotation)
{
    using ost_page_view::effective_pdf_page_size;
    auto check = [](double w, double h, int rotation, double ew, double eh)
    {
        SCOPED_CONTEXT("box " + std::to_string(w) + " x " + std::to_string(h) + " rotate " + std::to_string(rotation));
        const auto size = effective_pdf_page_size(w, h, rotation);
        CHECK_EQ(size[0], ew);
        CHECK_EQ(size[1], eh);
    };
    check(612, 792, 0, 612, 792);
    check(612, 792, 90, 792, 612);
    check(612, 792, 180, 612, 792);
    check(612, 792, 270, 792, 612);
    check(612, 792, -90, 792, 612);
    check(612, 792, 450, 792, 612);
    check(612, 792, 360, 612, 792);
}

// Port of test_pdf_load_strategy_uses_native_geometry_when_stored_dimensions_differ (stored 3024 x 2160 pt, PDF 2592 x 1728:
// the PDF wins) and test_pdf_load_strategy_reads_page_size_when_stored_dimensions_missing; the stored size is the fallback
// when the PDF cannot be read, and 612 x 792 when there is none. The stored BidPages size is in inches.
TEST(PageView, ResolvePdfPageSize)
{
    using ost_page_view::resolve_pdf_page_size;
    auto check = [](double w_in, double h_in, double aw, double ah, double ew, double eh)
    {
        SCOPED_CONTEXT("stored " + std::to_string(w_in) + " x " + std::to_string(h_in) + " actual " +
                       std::to_string(aw) + " x " + std::to_string(ah));
        const auto size = resolve_pdf_page_size(w_in, h_in, aw, ah);
        CHECK_NEAR(size[0], ew, 1e-9);
        CHECK_NEAR(size[1], eh, 1e-9);
    };
    check(42.0, 30.0, 2592.0, 1728.0, 2592.0, 1728.0); // stored 3024 x 2160 pt, the PDF's own size is used
    check(0.0, 0.0, 3024.0, 2160.0, 3024.0, 2160.0);   // no stored size
    check(8.5, 14.0, 612.0, 792.0, 612.0, 792.0);      // RPV-19 (1): stored 8.5 x 14 in disagrees, the PDF's 612 x 792 wins
    check(8.5, 11.0, 0.0, 0.0, 612.0, 792.0);          // PDF not readable: the stored size
    check(8.5, 11.0, 612.0, 0.0, 612.0, 792.0);        // one side not readable counts as not readable
    check(0.0, 0.0, 0.0, 0.0, 612.0, 792.0);           // nothing known: 612 x 792
    check(2.0, 0.0, 0.0, 0.0, 144.0, 792.0);           // a side missing in the stored size falls back on its own
    check(-1.0, 5.0, 0.0, 0.0, 612.0, 360.0);          // a negative side is missing (Windows would fail later)
}

// The page transform as an affine map equals ost_page_transform::transform_top_left_point at every point, and maps the page
// onto the canvas of output_dimensions: 16 combinations of rotation and flips (zoom_handler._get_page_transform).
TEST(PageView, PageAffineIsThePageTransform)
{
    const double w = 600.0, h = 800.0;
    const std::array<std::array<double, 2>, 5> points = {{{0.0, 0.0}, {w, 0.0}, {0.0, h}, {w, h}, {123.5, 456.25}}};
    for (int rotation : {0, 90, 180, 270, -90, 450})
        for (bool fx : {false, true})
            for (bool fy : {false, true})
            {
                SCOPED_CONTEXT("rotation " + std::to_string(rotation) + (fx ? " fx" : "") + (fy ? " fy" : ""));
                const auto a = ost_page_view::page_affine(w, h, rotation, fx, fy);
                const auto out = ost_page_transform::output_dimensions(w, h, rotation);
                for (const auto &p : points)
                {
                    const auto want = ost_page_transform::transform_top_left_point(w, h, rotation, fx, fy, p[0], p[1]);
                    CHECK_NEAR(a.a * p[0] + a.c * p[1] + a.tx, want[0], 1e-9);
                    CHECK_NEAR(a.b * p[0] + a.d * p[1] + a.ty, want[1], 1e-9);
                    // the page corners land on the canvas
                    CHECK(want[0] >= -1e-9 && want[0] <= out[0] + 1e-9);
                    CHECK(want[1] >= -1e-9 && want[1] <= out[1] + 1e-9);
                }
            }
}

// GEO-06 item table (600 x 800 page): the canvas of a quarter turn is 800 x 600, and the corners of the page go where the
// Windows transform puts them (rotation 90: (x, y) -> (y, W - x); flip first).
TEST(PageView, PageAffineKnownCorners)
{
    const auto r90 = ost_page_view::page_affine(600.0, 800.0, 90, false, false);
    CHECK_NEAR(r90.a * 0 + r90.c * 0 + r90.tx, 0.0, 1e-9);   // (0, 0) -> (0, 600)
    CHECK_NEAR(r90.b * 0 + r90.d * 0 + r90.ty, 600.0, 1e-9);
    CHECK_NEAR(r90.a * 600 + r90.c * 0 + r90.tx, 0.0, 1e-9); // (600, 0) -> (0, 0)
    CHECK_NEAR(r90.b * 600 + r90.d * 0 + r90.ty, 0.0, 1e-9);
    const auto fx = ost_page_view::page_affine(600.0, 800.0, 0, true, false);
    CHECK_NEAR(fx.a, -1.0, 1e-12);
    CHECK_NEAR(fx.d, 1.0, 1e-12);
    CHECK_NEAR(fx.tx, 600.0, 1e-9);
    CHECK_NEAR(fx.ty, 0.0, 1e-9);
    const auto id = ost_page_view::page_affine(600.0, 800.0, 0, false, false);
    CHECK_EQ(id.a, 1.0);
    CHECK_EQ(id.b, 0.0);
    CHECK_EQ(id.c, 0.0);
    CHECK_EQ(id.d, 1.0);
    CHECK_EQ(id.tx, 0.0);
    CHECK_EQ(id.ty, 0.0);
    CHECK(throws_invalid_argument([] { ost_page_view::page_affine(600.0, 800.0, 45, false, false); }));
    CHECK(throws_invalid_argument([] { ost_page_view::page_affine(0.0, 800.0, 0, false, false); }));
}
