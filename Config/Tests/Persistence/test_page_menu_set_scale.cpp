// The "Set Scale..." item of the page menu (plan 4.8, 5.4, decision N-M1-05): the first item of the 2D bar, absent from the 3D bar, enabled
// for a page of the open project while no area placement is in progress (Core/Data/PageMenuRules.*). The Objective-C++ layer
// (ViewController+PageMenu.mm) only builds the widget; ViewController+PageScale.mm's presenter repeats the same three conditions as its second
// line of defence (a locked bid is checked there by the bid lock guard).
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5496-5510   the Set Scale entry does nothing without a page or without
//                                                                                 permission (Feature.EDIT_PAGE_SETTINGS)
//   ost_visualizer/presentation/managers/ui_access_manager.py:166-191            _PLACEMENT_BLOCKED (an area placement blocks the page settings)
// Windows has no menu: the 2D page settings bar owns the scale control. A page count of one does NOT disable Set Scale (unlike Delete Page):
// the scale of the only page is a valid thing to set.
#include "TestHarness.hpp"

#include "Data/PageMenuRules.hpp"
#include "Domain/Page.hpp"

#include <string>
#include <vector>

using ost_data::PageMenuBar;
using ost_data::PageMenuState;

namespace
{
    ost_data::ProjectData project_with_pages(const std::vector<std::string> &order)
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

TEST(PageMenuSetScale, Bar2D_ShowsSetScale_Enabled)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2", "3"});
    const PageMenuState state = ost_data::page_menu_state(project, "2", PageMenuBar::Bar2D, false);
    CHECK(state.set_scale.shown);
    CHECK(state.set_scale.enabled);
}

TEST(PageMenuSetScale, Bar3D_HasNoSetScale_AndAHiddenItemIsNeverEnabled)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2", "3"});
    const PageMenuState state = ost_data::page_menu_state(project, "2", PageMenuBar::Bar3D, false);
    CHECK_FALSE(state.set_scale.shown);
    CHECK_FALSE(state.set_scale.enabled);
    // The 3D bar keeps its Rename item.
    CHECK(state.rename_page.shown);
}

TEST(PageMenuSetScale, OnePage_SetScaleStaysEnabled_WhileDeleteIsDisabled)
{
    const ost_data::ProjectData project = project_with_pages({"7"});
    const PageMenuState state = ost_data::page_menu_state(project, "7", PageMenuBar::Bar2D, false);
    CHECK(state.set_scale.shown);
    CHECK(state.set_scale.enabled);
    CHECK(state.delete_page.shown);
    CHECK_FALSE(state.delete_page.enabled);
}

TEST(PageMenuSetScale, AreaPlacementInProgress_DisablesSetScale_ThenEnablesItAgain)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2"});

    const PageMenuState during = ost_data::page_menu_state(project, "1", PageMenuBar::Bar2D, true);
    CHECK(during.set_scale.shown);
    CHECK_FALSE(during.set_scale.enabled);

    // Nothing is remembered between two calls.
    const PageMenuState after = ost_data::page_menu_state(project, "1", PageMenuBar::Bar2D, false);
    CHECK(after.set_scale.shown);
    CHECK(after.set_scale.enabled);
}

TEST(PageMenuSetScale, UnknownOrBlankPage_SetScaleIsDisabled)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2"});
    // Page uids are exact text: "01" and " 1" are not page "1".
    for (const char *uid : {"", "99", "01", " 1"})
    {
        SCOPED_CONTEXT(std::string("page uid [") + uid + "]");
        const PageMenuState state = ost_data::page_menu_state(project, uid, PageMenuBar::Bar2D, false);
        CHECK(state.set_scale.shown);
        CHECK_FALSE(state.set_scale.enabled);
    }
}

TEST(PageMenuSetScale, NoBidOpen_SetScaleIsDisabled)
{
    ost_data::ProjectData project = project_with_pages({"1", "2"});
    project.bid.uid.clear(); // ProjectData::empty()
    const PageMenuState state = ost_data::page_menu_state(project, "1", PageMenuBar::Bar2D, false);
    CHECK_FALSE(state.set_scale.enabled);
}

TEST(PageMenuSetScale, TheOtherItemsDoNotDependOnSetScale)
{
    // On the same call the three items answer independently: during a placement all three are disabled, afterwards all three are enabled.
    const ost_data::ProjectData project = project_with_pages({"1", "2"});
    const PageMenuState during = ost_data::page_menu_state(project, "2", PageMenuBar::Bar2D, true);
    CHECK_FALSE(during.set_scale.enabled);
    CHECK_FALSE(during.rename_page.enabled);
    CHECK_FALSE(during.delete_page.enabled);
    const PageMenuState after = ost_data::page_menu_state(project, "2", PageMenuBar::Bar2D, false);
    CHECK(after.set_scale.enabled);
    CHECK(after.rename_page.enabled);
    CHECK(after.delete_page.enabled);
}
