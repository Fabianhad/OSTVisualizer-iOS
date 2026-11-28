// Set Scale, the form rules: a case added by the independent review of m1-set-scale for a mutant that the earlier tests did not kill (the removal of the repeat filter of
// page_setting_uids). Windows source of truth: ui_event_coordinator.py:5390-5399 _page_setting_uids (no filter of repeats there: the write service drops them,
// project_write_service.py:6885-6892, as ost_store::save_page_scales does; the header of PageScaleRules.hpp promises "once each").
#include "FormattingTestSupport.hpp"

#include "Data/ProjectData.hpp"
#include "Domain/PageScaleRules.hpp"

#include <string>
#include <vector>

using namespace ost_domain;
using std::string;
using std::vector;

// "Apply to all pages" names every page of the page order once, in that order, even when the order lists a page twice (the pages that the project does not know and the empty
// uids are dropped too). Kills: the removal of the `seen` filter in page_setting_uids.
TEST(SetScale_FormApplyExtra, ApplyToAllNamesARepeatedPageOnceInThePageBarOrder)
{
    ost_data::ProjectData project;
    project.bid.uid = "7";
    for (const char *uid : {"42", "43", "44"})
    {
        ost_domain::Page page;
        page.uid = uid;
        page.name = string("Page ") + uid;
        page.scale_factor1 = 1.0;
        page.scale_factor2 = 48.0;
        project.pages[page.uid] = page;
    }
    project.page_order = {"43", "42", "43", "99", "", "42", "44", "44"};
    CHECK_EQ(page_setting_uids(project, "42", true), vector<string>({"43", "42", "44"}));
    CHECK_EQ(page_setting_uids(project, "42", false), vector<string>({"42"}));
}
