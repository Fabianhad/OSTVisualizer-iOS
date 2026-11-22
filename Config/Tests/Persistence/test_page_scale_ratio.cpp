// DBP-37: ProjectData::page_scale_ratio follows the Windows page metadata rule (user decision: any non-positive sf1 gives 1.0).
// Windows oracle: ost_visualizer/application/services/page_visualization_metadata_service.py::build_pages
//     sf1 = page.scale_factor1 or 1.0
//     sf2 = page.scale_factor2 or 1.0
//     ratio = sf2 / sf1 if sf1 > 0 else 1.0
// (the cited tests/application/services/test_page_visualization_metadata_service.py does not exist; every literal below
// is the value of that expression, computed by running it read-only with python -B). An unknown page has ratio 1.0.
#include "TestHarness.hpp"

#include "Data/ProjectData.hpp"

#include <cmath>
#include <limits>
#include <string>

namespace
{
    ost_data::ProjectData project_with_page(double sf1, double sf2)
    {
        ost_data::ProjectData project;
        ost_domain::Page page;
        page.uid = "7";
        page.scale_factor1 = sf1;
        page.scale_factor2 = sf2;
        project.pages["7"] = page;
        return project;
    }
} // namespace

// The four rows of the item's test plan.
TEST(Dbp37_PageScaleRatio, TheDecidedPlanRows)
{
    CHECK_EQ(project_with_page(-2.0, 5.0).page_scale_ratio("7"), 1.0);    // non-positive sf1
    CHECK_EQ(project_with_page(0.0, 5.0).page_scale_ratio("7"), 5.0);     // sf1 coalesced to 1.0
    CHECK_EQ(project_with_page(0.125, 12.0).page_scale_ratio("7"), 96.0); // the 1/8" = 1' scale
    CHECK_EQ(project_with_page(0.125, 0.0).page_scale_ratio("7"), 8.0);   // sf2 coalesced to 1.0
}

TEST(Dbp37_PageScaleRatio, AnUnknownPageHasRatioOne)
{
    ost_data::ProjectData project = project_with_page(2.0, 3.0);
    CHECK_EQ(project.page_scale_ratio("99"), 1.0);
    CHECK_EQ(project.page_scale_ratio(""), 1.0);
    CHECK_EQ(ost_data::ProjectData().page_scale_ratio("7"), 1.0);
}

// Every pair is the value of the Windows expression above (python float division, so exact equality).
TEST(Dbp37_PageScaleRatio, MatchesTheWindowsExpressionForEveryPair)
{
    struct Row
    {
        double sf1, sf2, expected;
    };
    const double inf = std::numeric_limits<double>::infinity();
    const Row rows[] = {
        {-2.0, 5.0, 1.0},
        {0.0, 5.0, 5.0},
        {0.125, 12.0, 96.0},
        {0.125, 0.0, 8.0},
        {4.0, 2.0, 0.5},
        {-0.0, 3.0, 3.0}, // negative zero is falsy in Python: coalesced
        {1.0, 1.0, 1.0},
        {2.0, -3.0, -1.5}, // only sf1 is guarded: a negative sf2 stays
        {-1.0, -1.0, 1.0},
        {0.0, 0.0, 1.0},
        {3.0, 0.5, 0.16666666666666666},
        {-inf, 2.0, 1.0},
        {inf, 2.0, 0.0},
        {2.0, inf, inf},
        {1e-300, 1e-300, 1.0},
        {96.0, 1.0, 0.010416666666666666},
        {0.05, 1.0, 20.0},
        {0.0625, 12.0, 192.0},
    };
    int index = 0;
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("row " + std::to_string(index++));
        CHECK_EQ(project_with_page(row.sf1, row.sf2).page_scale_ratio("7"), row.expected);
    }
}

// NaN is truthy in Python, so it stays and fails `sf1 > 0` (ratio 1.0) / divides through (NaN).
TEST(Dbp37_PageScaleRatio, NaNFollowsPythonTruthiness)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK_EQ(project_with_page(nan, 2.0).page_scale_ratio("7"), 1.0);
    CHECK(std::isnan(project_with_page(0.5, nan).page_scale_ratio("7")));
}
