// Set Scale, REVIEWER differential oracle: seeded random rows that were run through the REAL Windows functions (python -B -I, read-only) and compared with the iOS Core.
// The curated oracle tables of the other Set Scale tests pick their values by hand; these rows are drawn at random (decimal ties at the 4th decimal, binary fractions, tiny and huge
// magnitudes, the spellings a stored token can have) so that a rounding, a tie or a spelling rule that the hand-picked rows miss is found.
// Windows source of truth: page_operations.py:154-217 (_rescale_page_positions, one table at a time through a fake cursor), serialization.py:18-51, legend_position.py:6-26,
// overlay_rect.py:17-48. Rows whose Windows text is "inf" / "nan" are not here on purpose (the iOS store refuses them: register SS-2, pinned by test_page_scale_text.cpp).
// The legend rows compare the dX value only (the xml.etree byte form of the whole element is the adapted part: register SS-10).
#include "TestHarness.hpp"

#include "Domain/NumberText.hpp"
#include "Domain/OverlayRect.hpp"
#include "Domain/PageScaleTransform.hpp"
#include "Domain/PositionParser.hpp"

#include "PageScaleFuzzOracleData.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

using namespace ost_domain;
using std::string;

namespace
{
    // The rows were recorded from Windows BEFORE 0a24294: in the three-decimal tables a recorded text that holds a "0" / "-0" for a coordinate that was not 0 is now refused by
    // Windows (require_storable_scaled_values: before != 0 and round(after, 3) == 0), see test_page_scale_text.cpp.
    bool windows_now_refuses(const string &table, const string &stored, const string &expected)
    {
        if (table != "BidTakeoffs" && table != "BidComments" && table != "BidTypGroupViews")
            return false;
        const std::vector<double> before = parse_position(stored);
        const std::vector<double> after = parse_position(expected);
        for (size_t i = 0; i < before.size() && i < after.size(); ++i)
            if (before[i] != 0.0 && after[i] == 0.0)
                return true;
        return false;
    }

    string describe(const PositionRescale &result)
    {
        static const char *const names[] = {"Empty", "NotNumeric", "Rescaled", "NotFinite", "Underflow", "BadFactor"};
        return string(names[static_cast<int>(result.kind)]) + " '" + result.text + "'";
    }
}

TEST(SetScale_Fuzz, EveryRandomPositionRowGivesTheWindowsResult)
{
    size_t index = 0;
    for (const scale_fuzz::PositionRow &row : scale_fuzz::kPositions)
    {
        SCOPED_CONTEXT("row " + std::to_string(index++) + " " + row.table + " '" + row.stored + "' x " + format_general(row.factor, 17));
        const PositionRescale result = rescale_position_text(row.table, row.stored, row.factor);
        if (row.kind == 0)
        {
            CHECK(result.kind == PositionRescaleKind::Empty);
        }
        else if (row.kind == 1)
        {
            CHECK(result.kind == PositionRescaleKind::NotNumeric);
        }
        else if (windows_now_refuses(row.table, row.stored, row.expected))
        {
            CHECK(result.kind == PositionRescaleKind::Underflow);
            CHECK_EQ(result.text, string(""));
        }
        else
        {
            CHECK(result.kind == PositionRescaleKind::Rescaled);
            CHECK_EQ(result.text, string(row.expected));
            if (result.kind != PositionRescaleKind::Rescaled)
                CHECK_EQ(describe(result), string("Rescaled"));
        }
    }
    CHECK(index > 900);
}

TEST(SetScale_Fuzz, EveryRandomNumberIsFormattedLikePython)
{
    size_t index = 0;
    for (const scale_fuzz::NumberRow &row : scale_fuzz::kNumbers)
    {
        SCOPED_CONTEXT("number " + std::to_string(index++) + " " + format_general(row.value, 17));
        CHECK_EQ(format_g(row.value), string(row.g6));
        CHECK_EQ(format_general(row.value, 6), string(row.g6));
        CHECK_EQ(format_general(row.value, 15), string(row.g15));
        CHECK_EQ(format_fixed_trimmed(row.value, 3), string(row.f3trim));
        CHECK_EQ(format_fixed(row.value, 6), string(row.f6));
    }
    CHECK(index > 600);
}

TEST(SetScale_Fuzz, EveryRandomLegendValueScalesLikeWindows)
{
    size_t index = 0;
    for (const scale_fuzz::LegendRow &row : scale_fuzz::kLegends)
    {
        SCOPED_CONTEXT("legend " + std::to_string(index++) + " dX=" + row.text + " x " + format_general(row.factor, 17));
        const string xml = string("<Legends dX=\"") + row.text + "\" dY=\"2\"/>";
        string out;
        string error;
        REQUIRE(rescale_legend_xml(xml, row.factor, out, &error));
        const size_t start = out.find("dX=\"");
        REQUIRE(start != string::npos);
        const size_t end = out.find('"', start + 4);
        REQUIRE(end != string::npos);
        CHECK_EQ(out.substr(start + 4, end - start - 4), string(row.expected));
        // everything outside the two values is the input, byte for byte
        CHECK(out.rfind("<Legends dX=\"", 0) == 0);
        CHECK(out.size() >= 20 && out.compare(out.size() - 2, 2, "/>") == 0);
    }
    CHECK(index > 150);
}

TEST(SetScale_Fuzz, EveryRandomOverlayRectangleScalesLikeWindows)
{
    size_t index = 0;
    for (const scale_fuzz::RectRow &row : scale_fuzz::kRects)
    {
        SCOPED_CONTEXT("rect " + std::to_string(index++) + " '" + row.stored + "' x " + format_general(row.factor, 17));
        const OverlayRescale result = rescale_overlay_rect(row.stored, row.factor);
        if (row.touched == 0)
        {
            CHECK(result.kind == OverlayRescaleKind::Untouched);
        }
        else
        {
            REQUIRE(result.kind == OverlayRescaleKind::Rescaled);
            CHECK_EQ(result.rect_text, string(row.expected));
        }
    }
    CHECK(index > 120);
}

// parse_overlay_rect_storage (overlay_rect.py:33-41) on mutated spellings of a stored rectangle: the iOS parser accepts exactly what the Windows one accepts and reads the same four
// numbers (a part is strip()ped, float() grammar, exactly four parts, finite, a width / height that is not negative; "", blanks and "*" are the empty rectangle).
TEST(SetScale_Fuzz, EveryMutatedRectangleSpellingIsParsedLikeWindows)
{
    size_t index = 0;
    size_t accepted = 0;
    for (const scale_fuzz::RectParseRow &row : scale_fuzz::kRectParse)
    {
        SCOPED_CONTEXT("rect spelling " + std::to_string(index++) + " '" + row.stored + "'");
        OverlayRect rect;
        string error;
        const bool ok = parse_rect_storage(row.stored, rect, &error);
        CHECK_EQ(ok, row.ok != 0);
        if (ok && row.ok)
        {
            ++accepted;
            CHECK_EQ(rect.x, row.x);
            CHECK_EQ(rect.y, row.y);
            CHECK_EQ(rect.width, row.w);
            CHECK_EQ(rect.height, row.h);
        }
        else if (!ok)
            CHECK(!error.empty());
    }
    CHECK(index > 250);
    CHECK(accepted >= 40);
}

// overlay_units_per_sheet_inch (overlay.py:7-19, the strict rule), page_scale_ratio (page_scale_transform.py:9-20) and position_rescale_factor_between_page_scales (:23-30, the
// lenient rule) on 500 random pairs, a third of them awkward (0, -0, negative, 1e-320, 5e-324, 1e300, DBL_MAX, infinities, NaN): the same value as the real Windows functions,
// NaN for NaN. This pins the two ratio rules of plan 4.10 against each other on inputs the curated tables do not reach.
namespace
{
    bool same_double(double a, double b)
    {
        return (std::isnan(a) && std::isnan(b)) || a == b;
    }
}

TEST(SetScale_Fuzz, TheTwoRatioRulesAndTheLenientFactorMatchWindowsOnRandomAndAwkwardPairs)
{
    size_t index = 0;
    size_t usable = 0;
    for (const scale_fuzz::UnitsRow &row : scale_fuzz::kUnits)
    {
        SCOPED_CONTEXT("units row " + std::to_string(index++) + " (" + format_general(row.sf1, 17) + ", " + format_general(row.sf2, 17) + ")");
        const std::optional<double> got = units_per_sheet_inch(row.sf1, row.sf2);
        CHECK_EQ(got.has_value(), row.has != 0);
        if (got && row.has)
        {
            CHECK_EQ(*got, row.value);
            ++usable;
        }
    }
    CHECK(usable > 100);
    index = 0;
    for (const scale_fuzz::RatioRow &row : scale_fuzz::kRatios)
    {
        SCOPED_CONTEXT("ratio row " + std::to_string(index++) + " (" + format_general(row.sf1, 17) + ", " + format_general(row.sf2, 17) + ")");
        CHECK(same_double(page_scale_ratio(row.sf1, row.sf2), row.value));
    }
    index = 0;
    for (const scale_fuzz::FactorRow &row : scale_fuzz::kFactors)
    {
        SCOPED_CONTEXT("factor row " + std::to_string(index++) + " (" + format_general(row.a1, 17) + ", " + format_general(row.a2, 17) + ") -> (" + format_general(row.b1, 17) + ", " +
                       format_general(row.b2, 17) + ")");
        CHECK(same_double(position_rescale_factor_between_page_scales(PageScale{row.a1, row.a2}, PageScale{row.b1, row.b2}), row.value));
    }
}
