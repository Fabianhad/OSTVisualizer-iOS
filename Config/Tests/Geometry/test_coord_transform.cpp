// GEO-06 (aliases RPV-01, LP-03, GEO-16): ost_coord::ost_to_pdf_coordinates must be the Windows function
// (cpp_extensions/src/coord_transform/coord_transform.cpp + common/page_transform.hpp):
//   points_per_unit = 72 * sf1 / sf2; the point (x, y) * points_per_unit in the UNROTATED top-left page is flipped
//   first (flip_x: W - x, flip_y: H - y), then rotated counter-clockwise; the result is returned Y-up in the
//   OUTPUT page (W and H swap for 90/270): y' = output_height - y.
// Tables marked "Windows pyd" were produced by running ost_coord_transform.ost_to_pdf_coordinates of the Windows
// app (read-only) with the inputs in the comment; rows are {rotation, flip_x, flip_y, point count, {x0, y0, ...}}.
#include "GeometryTestUtil.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

using geo_test::case_name;
using geo_test::check_vec2;
using geo_test::pdf;
using geo_test::throws_invalid_argument;
using ost_types::Vec2;

namespace
{
    struct PdfRow
    {
        int rot;
        bool fx, fy;
        int n;
        double e[8];
    };

    template <size_t N>
    void check_rows(const std::vector<double> &pos, double w, double h, double sf1, double sf2, const PdfRow (&rows)[N],
                    bool exact)
    {
        for (const PdfRow &r : rows)
        {
            SCOPED_CONTEXT(case_name(r.rot, r.fx, r.fy));
            std::vector<Vec2> got = pdf(pos, w, h, sf1, sf2, r.rot, r.fx, r.fy);
            REQUIRE_EQ(got.size(), static_cast<size_t>(r.n));
            for (int k = 0; k < r.n; ++k)
                check_vec2(got[static_cast<size_t>(k)], r.e[2 * k], r.e[2 * k + 1], exact);
        }
    }

    // Windows pyd: ost_to_pdf_coordinates([10.0, 20.0], 600.0, 800.0, 1.0, 2.0, rot, fx, fy)
    const PdfRow kPdf600x800[] = {
        {0, false, false, 1, {360.0, 80.0}},
        {0, false, true, 1, {360.0, 720.0}},
        {0, true, false, 1, {240.0, 80.0}},
        {0, true, true, 1, {240.0, 720.0}},
        {90, false, false, 1, {720.0, 360.0}},
        {90, false, true, 1, {80.0, 360.0}},
        {90, true, false, 1, {720.0, 240.0}},
        {90, true, true, 1, {80.0, 240.0}},
        {180, false, false, 1, {240.0, 720.0}},
        {180, false, true, 1, {240.0, 80.0}},
        {180, true, false, 1, {360.0, 720.0}},
        {180, true, true, 1, {360.0, 80.0}},
        {270, false, false, 1, {80.0, 240.0}},
        {270, false, true, 1, {720.0, 240.0}},
        {270, true, false, 1, {80.0, 360.0}},
        {270, true, true, 1, {720.0, 360.0}},
    };

    // Windows pyd: ost_to_pdf_coordinates([1.0, 2.0], 612.0, 792.0, 1.0, 1.0, rot, fx, fy)
    const PdfRow kPdfLetter[] = {
        {0, false, false, 1, {72.0, 648.0}},
        {0, false, true, 1, {72.0, 144.0}},
        {0, true, false, 1, {540.0, 648.0}},
        {0, true, true, 1, {540.0, 144.0}},
        {90, false, false, 1, {144.0, 72.0}},
        {90, false, true, 1, {648.0, 72.0}},
        {90, true, false, 1, {144.0, 540.0}},
        {90, true, true, 1, {648.0, 540.0}},
        {180, false, false, 1, {540.0, 144.0}},
        {180, false, true, 1, {540.0, 648.0}},
        {180, true, false, 1, {72.0, 144.0}},
        {180, true, true, 1, {72.0, 648.0}},
        {270, false, false, 1, {648.0, 540.0}},
        {270, false, true, 1, {144.0, 540.0}},
        {270, true, false, 1, {648.0, 72.0}},
        {270, true, true, 1, {144.0, 72.0}},
    };

    // Windows pyd: ost_to_pdf_coordinates([1.0, 2.0, 0.5, 1.0], 612.0, 792.0, 1.0, 1.0, rot, fx, fy)
    const PdfRow kPdfLetterTwo[] = {
        {0, false, false, 2, {72.0, 648.0, 36.0, 720.0}},
        {0, false, true, 2, {72.0, 144.0, 36.0, 72.0}},
        {0, true, false, 2, {540.0, 648.0, 576.0, 720.0}},
        {0, true, true, 2, {540.0, 144.0, 576.0, 72.0}},
        {90, false, false, 2, {144.0, 72.0, 72.0, 36.0}},
        {90, false, true, 2, {648.0, 72.0, 720.0, 36.0}},
        {90, true, false, 2, {144.0, 540.0, 72.0, 576.0}},
        {90, true, true, 2, {648.0, 540.0, 720.0, 576.0}},
        {180, false, false, 2, {540.0, 144.0, 576.0, 72.0}},
        {180, false, true, 2, {540.0, 648.0, 576.0, 720.0}},
        {180, true, false, 2, {72.0, 144.0, 36.0, 72.0}},
        {180, true, true, 2, {72.0, 648.0, 36.0, 720.0}},
        {270, false, false, 2, {648.0, 540.0, 720.0, 576.0}},
        {270, false, true, 2, {144.0, 540.0, 72.0, 576.0}},
        {270, true, false, 2, {648.0, 72.0, 720.0, 36.0}},
        {270, true, true, 2, {144.0, 72.0, 72.0, 36.0}},
    };

    // Windows pyd: ost_to_pdf_coordinates([10.5, -3.25, 100.0, 200.0, 0.0, 0.0, 1234.5678, 91.125],
    //                                     612.0, 792.0, 3.0, 7.0, rot, fx, fy)   (points_per_unit = 216/7, inexact)
    const PdfRow kPdf3over7[] = {
        {0, false, false, 4, {324.0, 892.2857142857143, 3085.714285714286, -5379.428571428572, 0.0, 792.0, 38095.234971428574, -2019.8571428571431}},
        {0, false, true, 4, {324.0, -100.28571428571433, 3085.714285714286, 6171.428571428572, 0.0, 0.0, 38095.234971428574, 2811.857142857143}},
        {0, true, false, 4, {288.0, 892.2857142857143, -2473.714285714286, -5379.428571428572, 612.0, 792.0, -37483.234971428574, -2019.8571428571431}},
        {0, true, true, 4, {288.0, -100.28571428571433, -2473.714285714286, 6171.428571428572, 612.0, 0.0, -37483.234971428574, 2811.857142857143}},
        {90, false, false, 4, {-100.28571428571429, 324.0, 6171.428571428572, 3085.714285714286, 0.0, 0.0, 2811.857142857143, 38095.234971428574}},
        {90, false, true, 4, {892.2857142857143, 324.0, -5379.428571428572, 3085.714285714286, 792.0, 0.0, -2019.8571428571431, 38095.234971428574}},
        {90, true, false, 4, {-100.28571428571429, 288.0, 6171.428571428572, -2473.714285714286, 0.0, 612.0, 2811.857142857143, -37483.234971428574}},
        {90, true, true, 4, {892.2857142857143, 288.0, -5379.428571428572, -2473.714285714286, 792.0, 612.0, -2019.8571428571431, -37483.234971428574}},
        {180, false, false, 4, {288.0, -100.28571428571433, -2473.714285714286, 6171.428571428572, 612.0, 0.0, -37483.234971428574, 2811.857142857143}},
        {180, false, true, 4, {288.0, 892.2857142857143, -2473.714285714286, -5379.428571428572, 612.0, 792.0, -37483.234971428574, -2019.8571428571431}},
        {180, true, false, 4, {324.0, -100.28571428571433, 3085.714285714286, 6171.428571428572, 0.0, 0.0, 38095.234971428574, 2811.857142857143}},
        {180, true, true, 4, {324.0, 892.2857142857143, 3085.714285714286, -5379.428571428572, 0.0, 792.0, 38095.234971428574, -2019.8571428571431}},
        {270, false, false, 4, {892.2857142857143, 288.0, -5379.428571428572, -2473.714285714286, 792.0, 612.0, -2019.8571428571431, -37483.234971428574}},
        {270, false, true, 4, {-100.28571428571433, 288.0, 6171.428571428572, -2473.714285714286, 0.0, 612.0, 2811.857142857143, -37483.234971428574}},
        {270, true, false, 4, {892.2857142857143, 324.0, -5379.428571428572, 3085.714285714286, 792.0, 0.0, -2019.8571428571431, 38095.234971428574}},
        {270, true, true, 4, {-100.28571428571433, 324.0, 6171.428571428572, 3085.714285714286, 0.0, 0.0, 2811.857142857143, 38095.234971428574}},
    };

    // Windows pyd: ost_to_pdf_coordinates([1.0, 2.0, 50.5, 3.75], 300.0, 144.5, 0.125, 12.0, rot, fx, fy)
    const PdfRow kPdfNonSquare[] = {
        {0, false, false, 2, {0.75, 143.0, 37.875, 141.6875}},
        {0, false, true, 2, {0.75, 1.5, 37.875, 2.8125}},
        {0, true, false, 2, {299.25, 143.0, 262.125, 141.6875}},
        {0, true, true, 2, {299.25, 1.5, 262.125, 2.8125}},
        {90, false, false, 2, {1.5, 0.75, 2.8125, 37.875}},
        {90, false, true, 2, {143.0, 0.75, 141.6875, 37.875}},
        {90, true, false, 2, {1.5, 299.25, 2.8125, 262.125}},
        {90, true, true, 2, {143.0, 299.25, 141.6875, 262.125}},
        {180, false, false, 2, {299.25, 1.5, 262.125, 2.8125}},
        {180, false, true, 2, {299.25, 143.0, 262.125, 141.6875}},
        {180, true, false, 2, {0.75, 1.5, 37.875, 2.8125}},
        {180, true, true, 2, {0.75, 143.0, 37.875, 141.6875}},
        {270, false, false, 2, {143.0, 299.25, 141.6875, 262.125}},
        {270, false, true, 2, {1.5, 299.25, 2.8125, 262.125}},
        {270, true, false, 2, {143.0, 0.75, 141.6875, 37.875}},
        {270, true, true, 2, {1.5, 0.75, 2.8125, 37.875}},
    };

    // Windows pyd: ost_to_pdf_coordinates([100.0, 200.0], 600.0, 800.0, 1.0, 2.0, rot, fx, fy) with
    // rot in (-90, 450, 720, 360, -450, -180, 540, -270) and the four flip combinations each; the rotation is
    // reduced modulo 360 before the quarter-turn check.
    const PdfRow kPdfNormalise[] = {
        {-90, false, false, 1, {-6400.0, -3000.0}},
        {-90, false, true, 1, {7200.0, -3000.0}},
        {-90, true, false, 1, {-6400.0, 3600.0}},
        {-90, true, true, 1, {7200.0, 3600.0}},
        {450, false, false, 1, {7200.0, 3600.0}},
        {450, false, true, 1, {-6400.0, 3600.0}},
        {450, true, false, 1, {7200.0, -3000.0}},
        {450, true, true, 1, {-6400.0, -3000.0}},
        {720, false, false, 1, {3600.0, -6400.0}},
        {720, false, true, 1, {3600.0, 7200.0}},
        {720, true, false, 1, {-3000.0, -6400.0}},
        {720, true, true, 1, {-3000.0, 7200.0}},
        {360, false, false, 1, {3600.0, -6400.0}},
        {360, false, true, 1, {3600.0, 7200.0}},
        {360, true, false, 1, {-3000.0, -6400.0}},
        {360, true, true, 1, {-3000.0, 7200.0}},
        {-450, false, false, 1, {-6400.0, -3000.0}},
        {-450, false, true, 1, {7200.0, -3000.0}},
        {-450, true, false, 1, {-6400.0, 3600.0}},
        {-450, true, true, 1, {7200.0, 3600.0}},
        {-180, false, false, 1, {-3000.0, 7200.0}},
        {-180, false, true, 1, {-3000.0, -6400.0}},
        {-180, true, false, 1, {3600.0, 7200.0}},
        {-180, true, true, 1, {3600.0, -6400.0}},
        {540, false, false, 1, {-3000.0, 7200.0}},
        {540, false, true, 1, {-3000.0, -6400.0}},
        {540, true, false, 1, {3600.0, 7200.0}},
        {540, true, true, 1, {3600.0, -6400.0}},
        {-270, false, false, 1, {7200.0, 3600.0}},
        {-270, false, true, 1, {-6400.0, 3600.0}},
        {-270, true, false, 1, {7200.0, -3000.0}},
        {-270, true, true, 1, {-6400.0, -3000.0}},
    };
} // namespace

// Port of tests/domain/services/test_coordinate_transformation_service.py
//   ::OSTCoordinateSystemParsingTests::test_pdf_coordinates_honour_rotation_and_flips_on_a_scaled_page
TEST(PdfCoordinates, HonourRotationAndFlipsOnAScaledPage)
{
    // The four rows Windows pins explicitly.
    {
        auto p = pdf({10.0, 20.0}, 600.0, 800.0, 1.0, 2.0, 0, false, false);
        REQUIRE_EQ(p.size(), 1u);
        CHECK_EQ(p[0][0], 360.0);
        CHECK_EQ(p[0][1], 80.0);
    }
    {
        auto p = pdf({10.0, 20.0}, 600.0, 800.0, 1.0, 2.0, 90, false, false);
        REQUIRE_EQ(p.size(), 1u);
        CHECK_EQ(p[0][0], 720.0);
        CHECK_EQ(p[0][1], 360.0);
    }
    {
        auto p = pdf({10.0, 20.0}, 600.0, 800.0, 1.0, 2.0, 180, true, false);
        REQUIRE_EQ(p.size(), 1u);
        CHECK_EQ(p[0][0], 360.0);
        CHECK_EQ(p[0][1], 720.0);
    }
    {
        auto p = pdf({10.0, 20.0}, 600.0, 800.0, 1.0, 2.0, 270, false, true);
        REQUIRE_EQ(p.size(), 1u);
        CHECK_EQ(p[0][0], 720.0);
        CHECK_EQ(p[0][1], 240.0);
    }
}

// Full 4 x 4 table of the same page (GEO-06 / RPV-01 test plan), values from the Windows pyd.
TEST(PdfCoordinates, RotationFlipTableOn600x800Page)
{
    check_rows({10.0, 20.0}, 600.0, 800.0, 1.0, 2.0, kPdf600x800, true);
}

// Port of ...::OSTCoordinateSystemEdgeTests::test_pdf_coordinates_default_to_a_letter_width_when_flipping
// and ...::OSTCoordinateSystemParsingTests::test_pdf_coordinates_require_a_point_and_use_page_defaults
// (Windows fills the missing page keys with a 612x792 letter page, ratio 1, rotation 0, no flips).
TEST(PdfCoordinates, LetterPageDefaults)
{
    {
        auto p = pdf({1.0, 2.0}, 612.0, 792.0, 1.0, 1.0, 0, false, false);
        REQUIRE_EQ(p.size(), 1u);
        CHECK_EQ(p[0][0], 72.0);
        CHECK_EQ(p[0][1], 792.0 - 144.0);
    }
    {
        auto p = pdf({1.0, 2.0, 0.5, 1.0}, 612.0, 792.0, 1.0, 1.0, 0, false, false);
        REQUIRE_EQ(p.size(), 2u);
        CHECK_EQ(p[0][0], 72.0);
        CHECK_EQ(p[0][1], 648.0);
        CHECK_EQ(p[1][0], 36.0);
        CHECK_EQ(p[1][1], 720.0);
    }
    {
        auto p = pdf({1.0, 2.0}, 612.0, 792.0, 1.0, 1.0, 0, true, false); // flip_x
        REQUIRE_EQ(p.size(), 1u);
        CHECK_EQ(p[0][0], 612.0 - 72.0);
        CHECK_EQ(p[0][1], 648.0);
    }
    {
        auto p = pdf({1.0, 2.0}, 612.0, 792.0, 1.0, 1.0, 0, false, true); // flip_y
        REQUIRE_EQ(p.size(), 1u);
        CHECK_EQ(p[0][0], 72.0);
        CHECK_EQ(p[0][1], 144.0);
    }
    {
        auto p = pdf({1.0, 2.0}, 612.0, 792.0, 1.0, 1.0, 90, false, false);
        REQUIRE_EQ(p.size(), 1u);
        CHECK_EQ(p[0][0], 792.0 - 648.0);
        CHECK_EQ(p[0][1], 72.0);
    }
}

// RPV-01 / LP-03 table on the letter page: the 16 rotation/flip rows, one and two points (Windows pyd).
TEST(PdfCoordinates, RotationFlipTableOnLetterPage)
{
    check_rows({1.0, 2.0}, 612.0, 792.0, 1.0, 1.0, kPdfLetter, true);
    check_rows({1.0, 2.0, 0.5, 1.0}, 612.0, 792.0, 1.0, 1.0, kPdfLetterTwo, true);
}

// Non-representable points_per_unit (216/7) and a non-square page with fractional size (Windows pyd).
TEST(PdfCoordinates, NonIntegralScaleAndNonSquarePage)
{
    check_rows({10.5, -3.25, 100.0, 200.0, 0.0, 0.0, 1234.5678, 91.125}, 612.0, 792.0, 3.0, 7.0, kPdf3over7, false);
    check_rows({1.0, 2.0, 50.5, 3.75}, 300.0, 144.5, 0.125, 12.0, kPdfNonSquare, true);
}

// Port of ...::CanonicalCoordinateValidationTests::test_canonical_coordinate_transform_rejects_invalid_page_contracts
// (the valid page: ratio 72 maps OST units to points 1:1; PDF Y starts at the lower edge).
TEST(PdfCoordinates, ValidPageMapsUnitsToPointsAndFlipsY)
{
    auto p = pdf({100.0, 200.0}, 600.0, 800.0, 1.0, 72.0, 0, false, false);
    REQUIRE_EQ(p.size(), 1u);
    CHECK_EQ(p[0][0], 100.0);
    CHECK_EQ(p[0][1], 600.0);
}

// Port of ...::CanonicalCoordinateValidationTests::test_canonical_coordinate_transform_rejects_invalid_page_contracts
// (Windows raises ValueError; the C++ function throws std::invalid_argument, the message is Windows' text).
TEST(PdfCoordinates, RejectsInvalidPageContracts)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const char *kDims = "Page dimensions must be finite and positive";
    const char *kScale = "Page scale factors must be finite and positive";
    const char *kRot = "Page rotation must be a multiple of 90 degrees";
    struct Case
    {
        const char *label;
        double w, h, sf1, sf2;
        int rot;
        const char *message;
    };
    const Case cases[] = {
        // the five Windows subTests
        {"width 0", 0.0, 800.0, 1.0, 72.0, 0, kDims},
        {"height NaN", 600.0, nan, 1.0, 72.0, 0, kDims},
        {"sf1 0", 600.0, 800.0, 0.0, 72.0, 0, kScale},
        {"sf2 inf", 600.0, 800.0, 1.0, inf, 0, kScale},
        {"rotation 45", 600.0, 800.0, 1.0, 72.0, 45, kRot},
        // further rejections of the same Windows predicates (run on the pyd: all raise ValueError)
        {"width negative", -1.0, 800.0, 1.0, 72.0, 0, kDims},
        {"width inf", inf, 800.0, 1.0, 72.0, 0, kDims},
        {"height 0", 600.0, 0.0, 1.0, 72.0, 0, kDims},
        {"sf1 negative", 600.0, 800.0, -1.0, 72.0, 0, kScale},
        {"sf2 negative", 600.0, 800.0, 1.0, -72.0, 0, kScale},
        {"sf2 0", 600.0, 800.0, 1.0, 0.0, 0, kScale},
        {"sf1 NaN", 600.0, 800.0, nan, 72.0, 0, kScale},
        {"rotation 1", 600.0, 800.0, 1.0, 72.0, 1, kRot},
        {"rotation -45", 600.0, 800.0, 1.0, 72.0, -45, kRot},
        // validation order: scale factors, then dimensions, then rotation
        {"sf1 0 beats rotation 45", 600.0, 800.0, 0.0, 72.0, 45, kScale},
        {"width 0 beats rotation 45", 0.0, 800.0, 1.0, 72.0, 45, kDims},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.label);
        std::string what;
        CHECK(throws_invalid_argument([&] { pdf({100.0, 200.0}, c.w, c.h, c.sf1, c.sf2, c.rot, false, false); }, &what));
        CHECK_STR_EQ(what, c.message);
    }
}

// Non-finite coordinates (also after scaling) are rejected with Windows' message; the page contract is checked first.
TEST(PdfCoordinates, RejectsNonFinitePoints)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const char *kPoint = "Page coordinates must be finite";
    struct Case
    {
        const char *label;
        std::vector<double> pos;
        double w, sf2;
        int rot;
        const char *message;
    };
    const Case cases[] = {
        {"x NaN", {nan, 1.0}, 600.0, 72.0, 0, kPoint},
        {"y inf", {1.0, inf}, 600.0, 72.0, 0, kPoint},
        {"scaled overflow", {1e308, 1.0}, 600.0, 0.5, 0, kPoint},
        {"second point NaN", {1.0, 2.0, nan, 1.0}, 600.0, 72.0, 0, kPoint},
        {"rotation error before point error", {nan, 2.0}, 600.0, 72.0, 45, "Page rotation must be a multiple of 90 degrees"},
        {"dimension error before point error", {nan, 2.0}, 0.0, 72.0, 0, "Page dimensions must be finite and positive"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.label);
        std::string what;
        CHECK(throws_invalid_argument([&] { pdf(c.pos, c.w, 800.0, 1.0, c.sf2, c.rot, false, false); }, &what));
        CHECK_STR_EQ(what, c.message);
    }
}

// Port of ...::OSTCoordinateSystemParsingTests::test_pdf_coordinates_require_a_point_and_use_page_defaults
// ("Without a full point there is nothing to convert, so even an invalid page contract is not reported").
TEST(PdfCoordinates, ShortInputReturnsEmptyWithoutValidation)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(pdf({}, 612.0, 792.0, 1.0, 1.0, 0, false, false).empty());
    CHECK(pdf({5.0}, 612.0, 792.0, 1.0, 1.0, 0, false, false).empty());
    CHECK(pdf({}, 0.0, 792.0, 1.0, 1.0, 0, false, false).empty());
    CHECK(pdf({5.0}, 0.0, 792.0, 1.0, 1.0, 0, false, false).empty());
    CHECK(pdf({5.0}, 600.0, 800.0, 1.0, 72.0, 45, false, false).empty());
    CHECK(pdf({}, 600.0, nan, 0.0, -1.0, 45, false, false).empty());
    // ... but one full point does validate.
    CHECK(throws_invalid_argument([] { pdf({5.0, 6.0}, 0.0, 792.0, 1.0, 1.0, 0, false, false); }));
}

// Windows pyd: ost_to_pdf_coordinates([100.0, 200.0, 300.0], 600, 800, 1, 2, 0, F, F) -> [(3600.0, -6400.0)]:
// the incomplete trailing coordinate is ignored and out-of-page points are not clipped.
TEST(PdfCoordinates, OddLengthInputIgnoresTheTrailingValue)
{
    auto p = pdf({100.0, 200.0, 300.0}, 600.0, 800.0, 1.0, 2.0, 0, false, false);
    REQUIRE_EQ(p.size(), 1u);
    CHECK_EQ(p[0][0], 3600.0);
    CHECK_EQ(p[0][1], -6400.0);
}

// Windows normalises the rotation with ((r % 360) + 360) % 360 before the quarter-turn check
// (page_transform.hpp normalize_quarter_turn): -90 == 270, 450 == 90, 720 == 360 == 0 (Windows pyd rows).
TEST(PdfCoordinates, RotationIsReducedModuloAFullTurn)
{
    check_rows({100.0, 200.0}, 600.0, 800.0, 1.0, 2.0, kPdfNormalise, true);
    // Equivalences stated directly on the same page.
    const std::vector<double> pos{100.0, 200.0};
    for (const geo_test::Flips &f : geo_test::all_flips())
    {
        SCOPED_CONTEXT(case_name(0, f.x, f.y));
        const int equivalents[][2] = {{-90, 270}, {450, 90}, {-450, 270}, {720, 0}, {360, 0}, {-180, 180}, {540, 180}, {-270, 90}};
        for (const auto &eq : equivalents)
        {
            auto a = pdf(pos, 600.0, 800.0, 1.0, 2.0, eq[0], f.x, f.y);
            auto b = pdf(pos, 600.0, 800.0, 1.0, 2.0, eq[1], f.x, f.y);
            REQUIRE_EQ(a.size(), 1u);
            REQUIRE_EQ(b.size(), 1u);
            CHECK_EQ(a[0][0], b[0][0]);
            CHECK_EQ(a[0][1], b[0][1]);
        }
    }
}
