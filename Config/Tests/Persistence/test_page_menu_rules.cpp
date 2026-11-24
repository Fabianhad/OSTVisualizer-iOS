// The page menu (plan 4.8, 5.5, 5.6): which items the long-press menu of a page button shows, which of them are enabled, which wording the
// Delete confirmation takes, and the two texts the screens add (Core/Data/PageMenuRules.*, Core/Domain/PageDialogText.*). The Objective-C++
// layer (ViewController+PageMenu.mm, +PageRename.mm, +PageDelete.mm) only converts types and calls these functions.
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5714-5731   open_rename_page_dialog (the open guard)
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5881-5893   can_delete_current_page (page exists, more than one page,
//                                                                                 Feature.EDIT_PAGE_SETTINGS allowed)
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5922-5926   the page asks for the content confirmation when its uid is
//                                                                                 in the stored scan OR the loaded model has takeoffs for it
//   ost_visualizer/presentation/utils/view_context_menu.py:446-459               "Rename Page..." then "Delete Page"
//   ost_visualizer/presentation/dialogs/rename_page_dialog.py:55-100             title "Rename Page", the read-only "Old Name" row
//   ost_visualizer/presentation/managers/ui_access_manager.py:166-191            _PLACEMENT_BLOCKED (an area placement blocks both items)
// iOS decisions: N-M1-05 (2D bar: Rename and Delete, 3D bar: Rename only), N-M1-23 (Delete always asks), plan 4.8 (disabled while an area
// placement is in progress), DP-2 (a committed delete is reported as deleted when the reload fails).
// Windows has no test of the menu as such: the table rows below are pinned from the source lines above (the 3D bar has no Delete because
// Windows disables the item outside the takeoff tab, can_delete_current_page :5882).
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
    // A project (not loaded from a database) with these pages as loose pages in this order, page names "Page <uid>".
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

    void add_takeoff(ost_data::ProjectData &project, const std::string &takeoff_uid, const std::string &page_uid)
    {
        ost_domain::Takeoff takeoff;
        takeoff.uid = takeoff_uid;
        takeoff.page_uid = page_uid;
        project.takeoffs.push_back(takeoff);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// page_menu_state: the 2D bar offers Rename Page and Delete Page, the 3D bar Rename Page only (N-M1-05, plan 4.8)
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(PageMenuRules, Bar2D_ShowsRenameAndDelete_BothEnabled)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2", "3"});
    const PageMenuState state = ost_data::page_menu_state(project, "2", PageMenuBar::Bar2D, false);
    CHECK(state.rename_page.shown);
    CHECK(state.rename_page.enabled);
    CHECK(state.delete_page.shown);
    CHECK(state.delete_page.enabled);
}

TEST(PageMenuRules, Bar3D_ShowsRenameOnly_DeleteIsNotInTheMenu)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2", "3"});
    const PageMenuState state = ost_data::page_menu_state(project, "2", PageMenuBar::Bar3D, false);
    CHECK(state.rename_page.shown);
    CHECK(state.rename_page.enabled);
    CHECK_FALSE(state.delete_page.shown);
    // A hidden item can never be acted on.
    CHECK_FALSE(state.delete_page.enabled);
}

TEST(PageMenuRules, Bar2D_OnePage_DeleteIsShownButDisabled_RenameStaysEnabled)
{
    // ui_event_coordinator.py:5893 `len(page_order) > 1`; the item is disabled, not hidden (plan 4.8).
    const ost_data::ProjectData project = project_with_pages({"7"});
    const PageMenuState state = ost_data::page_menu_state(project, "7", PageMenuBar::Bar2D, false);
    CHECK(state.rename_page.shown);
    CHECK(state.rename_page.enabled);
    CHECK(state.delete_page.shown);
    CHECK_FALSE(state.delete_page.enabled);
}

TEST(PageMenuRules, Bar2D_TwoPages_BothPagesCanBeDeleted)
{
    const ost_data::ProjectData project = project_with_pages({"4", "9"});
    CHECK(ost_data::page_menu_state(project, "4", PageMenuBar::Bar2D, false).delete_page.enabled);
    CHECK(ost_data::page_menu_state(project, "9", PageMenuBar::Bar2D, false).delete_page.enabled);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// an area placement in progress blocks both items, and they are enabled again afterwards (Windows _PLACEMENT_BLOCKED)
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(PageMenuRules, AreaPlacementInProgress_DisablesRenameAndDelete_ThenEnablesThemAgain)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2"});

    const PageMenuState during = ost_data::page_menu_state(project, "1", PageMenuBar::Bar2D, true);
    CHECK(during.rename_page.shown);
    CHECK_FALSE(during.rename_page.enabled);
    CHECK(during.delete_page.shown);
    CHECK_FALSE(during.delete_page.enabled);

    // The same project and page after the placement ended: nothing is remembered between two calls.
    const PageMenuState after = ost_data::page_menu_state(project, "1", PageMenuBar::Bar2D, false);
    CHECK(after.rename_page.enabled);
    CHECK(after.delete_page.enabled);
}

TEST(PageMenuRules, AreaPlacementInProgress_Bar3D_DisablesRename)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2"});
    const PageMenuState during = ost_data::page_menu_state(project, "2", PageMenuBar::Bar3D, true);
    CHECK(during.rename_page.shown);
    CHECK_FALSE(during.rename_page.enabled);
    CHECK_FALSE(during.delete_page.shown);
    CHECK(ost_data::page_menu_state(project, "2", PageMenuBar::Bar3D, false).rename_page.enabled);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// a page that is not a page of the open project, and a project that is not open (open guard :5714-5731, :5881-5893)
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(PageMenuRules, UnknownOrBlankPage_BothItemsDisabled)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2"});
    for (const char *uid : {"", "99", "01", " 1"})
    {
        SCOPED_CONTEXT(std::string("page uid [") + uid + "]");
        const PageMenuState two_d = ost_data::page_menu_state(project, uid, PageMenuBar::Bar2D, false);
        CHECK_FALSE(two_d.rename_page.enabled);
        CHECK_FALSE(two_d.delete_page.enabled);
        const PageMenuState three_d = ost_data::page_menu_state(project, uid, PageMenuBar::Bar3D, false);
        CHECK_FALSE(three_d.rename_page.enabled);
    }
}

TEST(PageMenuRules, NoBidOpen_BothItemsDisabled)
{
    ost_data::ProjectData project = project_with_pages({"1", "2"});
    project.bid.uid.clear(); // ProjectData::empty()
    const PageMenuState state = ost_data::page_menu_state(project, "1", PageMenuBar::Bar2D, false);
    CHECK_FALSE(state.rename_page.enabled);
    CHECK_FALSE(state.delete_page.enabled);
}

TEST(PageMenuRules, TheStateDoesNotChangeTheProject)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2"});
    ost_data::ProjectData copy = project;
    (void)ost_data::page_menu_state(copy, "1", PageMenuBar::Bar2D, false);
    CHECK_EQ(copy.pages.size(), project.pages.size());
    CHECK_EQ(copy.page_order.size(), project.page_order.size());
    CHECK_STR_EQ(copy.pages.at("1").name, project.pages.at("1").name);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// page_delete_has_content: the wording of the confirmation (ui_event_coordinator.py:5922-5926)
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(PageMenuRules, DeleteContent_UidInTheStoredScan_IsContent)
{
    const ost_data::ProjectData project = project_with_pages({"1", "2"});
    CHECK(ost_data::page_delete_has_content(project, "2", {"2"}));
}

TEST(PageMenuRules, DeleteContent_LoadedTakeoffOnThePage_IsContentEvenWhenTheScanIsEmpty)
{
    // `loaded_page_has_content` (:5922-5925): the loaded model has takeoffs for the page.
    ost_data::ProjectData project = project_with_pages({"1", "2"});
    add_takeoff(project, "500", "2");
    CHECK(ost_data::page_delete_has_content(project, "2", {}));
}

TEST(PageMenuRules, DeleteContent_NeitherScanNorTakeoff_IsEmpty)
{
    ost_data::ProjectData project = project_with_pages({"1", "2"});
    add_takeoff(project, "500", "1"); // a takeoff of ANOTHER page
    CHECK_FALSE(ost_data::page_delete_has_content(project, "2", {}));
    CHECK_FALSE(ost_data::page_delete_has_content(project, "2", {"1"})); // another page holds content
}

TEST(PageMenuRules, DeleteContent_UidsAreComparedAsExactText)
{
    // Windows `str(page_uid) in pages_with_content`: "7" is not "007".
    const ost_data::ProjectData project = project_with_pages({"7", "8"});
    CHECK_FALSE(ost_data::page_delete_has_content(project, "7", {"007"}));
    CHECK(ost_data::page_delete_has_content(project, "7", {"007", "7"}));
}

TEST(PageMenuRules, DeleteContent_UnknownPage_IsNotContent)
{
    ost_data::ProjectData project = project_with_pages({"1", "2"});
    add_takeoff(project, "500", "99");
    CHECK_FALSE(ost_data::page_delete_has_content(project, "", {}));
    CHECK(ost_data::page_delete_has_content(project, "99", {})); // the loaded takeoff says it: the decision is about the uid, not the page row
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the texts the screens add
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(PageDialogText, RenamePrompt_TitleAndTheRawOldName)
{
    // rename_page_dialog.py: title "Rename Page", the read-only "Old Name" row shows the raw stored name.
    const ost_domain::DialogText text = ost_domain::page_rename_prompt_text("A-101");
    CHECK_STR_EQ(text.title, "Rename Page");
    CHECK_STR_EQ(text.message, "Old Name: A-101");
}

TEST(PageDialogText, RenamePrompt_TheStoredNameIsShownAsItIs)
{
    // Not stripped, not shortened, not the label: a stored name with surrounding spaces and an empty name (register RP-1, RP-5, RP-6).
    CHECK_STR_EQ(ost_domain::page_rename_prompt_text("  A-101 ").message, "Old Name:   A-101 ");
    CHECK_STR_EQ(ost_domain::page_rename_prompt_text("").message, "Old Name: ");
    CHECK_STR_EQ(ost_domain::page_rename_prompt_text("Plan 'B' \"C\"\n2").message, "Old Name: Plan 'B' \"C\"\n2");
}

TEST(PageDialogText, DeleteReloadFailed_SaysTheDeleteHappened_AndGivesTheReason)
{
    // DP-2: Windows reports "Failed to delete page" when only the reload failed; iOS says the page is gone.
    const ost_domain::DialogText text = ost_domain::page_delete_reload_failed_text("database is locked");
    CHECK_STR_EQ(text.title, "Delete Page");
    CHECK_STR_EQ(text.message,
                 "The page was deleted, but the bid could not be read again: database is locked\n\nThe bid will close. Open it again to continue.");
}

TEST(PageDialogText, DeleteReloadFailed_WithoutAReason_HasNoDanglingColon)
{
    const ost_domain::DialogText text = ost_domain::page_delete_reload_failed_text("");
    CHECK_STR_EQ(text.title, "Delete Page");
    CHECK_STR_EQ(text.message, "The page was deleted, but the bid could not be read again.\n\nThe bid will close. Open it again to continue.");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// page_selection_after_reload: which page the bid shows after the reload that follows a delete
// ---------------------------------------------------------------------------------------------------------------------------------
// Windows ui_event_coordinator.py:5989-6007 stages the page that followed the deleted one (_stage_selection_after_page_delete) and :1100-1131
// (_stage_takeoff_restore, pinned :31905-31921) makes it the only selected page after the reload. A page that is not in the reloaded project
// cannot be staged: the load then opens the page it always opens (ost_data::initial_page_uid: the saved page when it still exists, else the
// first page of the tree walk).

TEST(PageSelectionAfterReload, TheRequestedPageIsSelected_EvenWhenAnotherPageIsSaved)
{
    ost_data::ProjectData project = project_with_pages({"1", "2", "3"});
    project.selected_page_uid = "3";
    CHECK_STR_EQ(ost_data::page_selection_after_reload(project, "2"), "2");
    CHECK_STR_EQ(ost_data::page_selection_after_reload(project, "1"), "1");
}

TEST(PageSelectionAfterReload, ARequestedPageThatIsGone_FallsBackToTheSavedPage)
{
    ost_data::ProjectData project = project_with_pages({"1", "2", "3"});
    project.selected_page_uid = "3";
    CHECK_STR_EQ(ost_data::page_selection_after_reload(project, "9"), "3");
}

TEST(PageSelectionAfterReload, NoRequest_OpensTheSavedPageThenTheFirstPage)
{
    ost_data::ProjectData project = project_with_pages({"4", "5", "6"});
    project.selected_page_uid = "6";
    CHECK_STR_EQ(ost_data::page_selection_after_reload(project, ""), "6");
    project.selected_page_uid = "";
    CHECK_STR_EQ(ost_data::page_selection_after_reload(project, ""), "4");
    project.selected_page_uid = "77"; // a saved page that does not exist (any more)
    CHECK_STR_EQ(ost_data::page_selection_after_reload(project, ""), "4");
}

TEST(PageSelectionAfterReload, UidsAreComparedAsExactText)
{
    ost_data::ProjectData project = project_with_pages({"7", "8"});
    CHECK_STR_EQ(ost_data::page_selection_after_reload(project, "07"), "7");
    CHECK_STR_EQ(ost_data::page_selection_after_reload(project, " 8"), "7");
}

TEST(PageSelectionAfterReload, AProjectWithoutPages_SelectsNothing)
{
    ost_data::ProjectData project = project_with_pages({});
    CHECK_STR_EQ(ost_data::page_selection_after_reload(project, "1"), "");
    CHECK_STR_EQ(ost_data::page_selection_after_reload(project, ""), "");
}
