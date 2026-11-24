// Review additions for the page menu rules (Core/Data/PageMenuRules.*, Core/Domain/PageDialogText.*): cases a lazy implementation of
// page_delete_has_content / page_menu_state would survive. Added next to test_page_menu_rules.cpp; nothing there is changed.
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5922-5926   the scan names the page OR the loaded model has takeoffs for it
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5714-5731   open_rename_page_dialog (a page with an empty name can be renamed)
//   ost_visualizer/presentation/dialogs/rename_page_dialog.py:55-100             the read-only "Old Name" row shows the raw stored name
#include "TestHarness.hpp"

#include "Data/PageMenuRules.hpp"
#include "Domain/Page.hpp"
#include "Domain/PageDialogText.hpp"
#include "Domain/Takeoff.hpp"

#include <set>
#include <string>
#include <vector>

using ost_data::PageMenuBar;
using ost_data::PageMenuState;

namespace
{
    ost_data::ProjectData review_project(const std::vector<std::string> &order)
    {
        ost_data::ProjectData project;
        project.bid.uid = "10";
        int sequence = 1;
        for (const std::string &uid : order)
        {
            ost_domain::Page page;
            page.uid = uid;
            page.name = "Page " + uid;
            page.sequence = sequence++;
            project.pages[uid] = page;
            project.page_order.push_back(uid);
        }
        return project;
    }
}

// The header says "An empty uid is never content". Without the guard, a takeoff that names no page (page_uid "") or a scan entry "" would
// make the question for the empty uid answer true.
TEST(PageMenuRulesReview, DeleteContent_AnEmptyUidIsNeverContent_EvenWhenAnUnpagedTakeoffOrTheScanNamesIt)
{
    ost_data::ProjectData project = review_project({"1", "2"});
    ost_domain::Takeoff takeoff;
    takeoff.uid = "500";
    takeoff.page_uid = "";
    project.takeoffs.push_back(takeoff);
    CHECK_FALSE(ost_data::page_delete_has_content(project, "", {}));
    CHECK_FALSE(ost_data::page_delete_has_content(project, "", {""}));
    CHECK_FALSE(ost_data::page_delete_has_content(project, "", {"1", "2"}));
    // The real pages are not affected by the unpaged takeoff.
    CHECK_FALSE(ost_data::page_delete_has_content(project, "1", {}));
    CHECK_FALSE(ost_data::page_delete_has_content(project, "2", {}));
}

// A page with no name (the Cover Sheet can store one, register RP-6) still has a menu and its rename prompt shows the raw empty name.
TEST(PageMenuRulesReview, APageWithAnEmptyName_CanBeRenamed_AndTheDeleteItemFollowsThePageCount)
{
    ost_data::ProjectData project = review_project({"1", "2"});
    project.pages["2"].name = "";
    const PageMenuState two_d = ost_data::page_menu_state(project, "2", PageMenuBar::Bar2D, false);
    CHECK(two_d.rename_page.enabled);
    CHECK(two_d.delete_page.enabled);
    CHECK(ost_data::page_menu_state(project, "2", PageMenuBar::Bar3D, false).rename_page.enabled);
    CHECK_STR_EQ(ost_domain::page_rename_prompt_text(project.pages.at("2").name).message, "Old Name: ");
}

// The state is a pure function of its four inputs: asking twice gives the same answer, the 2D answer for Rename equals the 3D answer
// (the menu action asks the 3D variant for the rename item of either bar, ViewController+PageRename.mm), and the flag only ever disables.
TEST(PageMenuRulesReview, RenameIsTheSameOnBothBars_AndTheFlagOnlyEverDisables)
{
    const ost_data::ProjectData one = review_project({"7"});
    const ost_data::ProjectData three = review_project({"1", "2", "3"});
    for (const ost_data::ProjectData *project : {&one, &three})
    {
        for (const char *uid : {"7", "1", "2", "3", "9", ""})
        {
            for (bool placing : {false, true})
            {
                SCOPED_CONTEXT(std::string("page [") + uid + "] placing " + (placing ? "yes" : "no"));
                const PageMenuState bar2d = ost_data::page_menu_state(*project, uid, PageMenuBar::Bar2D, placing);
                const PageMenuState bar3d = ost_data::page_menu_state(*project, uid, PageMenuBar::Bar3D, placing);
                CHECK_EQ(bar2d.rename_page.enabled, bar3d.rename_page.enabled);
                CHECK_EQ(bar2d.rename_page.shown, bar3d.rename_page.shown);
                CHECK_FALSE(bar3d.delete_page.shown);
                CHECK_FALSE(bar3d.delete_page.enabled);
                if (placing)
                {
                    CHECK_FALSE(bar2d.rename_page.enabled);
                    CHECK_FALSE(bar2d.delete_page.enabled);
                }
                // Enabled implies shown, always.
                if (bar2d.delete_page.enabled)
                    CHECK(bar2d.delete_page.shown);
            }
        }
    }
}
