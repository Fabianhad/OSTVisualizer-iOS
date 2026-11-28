// Layer: Data
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The page menu (plan 4.8, 5.5, 5.6): the long-press menu of a page button of the 2D and the 3D page bar. The screen
// (Controllers/ViewController/Components/ViewController+PageMenu.mm) asks these functions which items the menu shows, which of them are
// enabled and which wording the Delete confirmation takes; it adds only the widgets and converts the types.
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5714-5731   open_rename_page_dialog (the open guard)
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5881-5893   can_delete_current_page (page exists, more than one page)
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5922-5926   the confirmation is asked for a page whose uid is in the
//                                                                                stored scan OR for which the loaded model has takeoffs
//   ost_visualizer/presentation/utils/view_context_menu.py:446-459               "Rename Page..." then "Delete Page"
//   ost_visualizer/presentation/managers/ui_access_manager.py:166-191            _PLACEMENT_BLOCKED (an area placement blocks both items)
// iOS decisions: N-M1-05 (2D bar: Rename and Delete, 3D bar: Rename only), plan 4.8 (an item is disabled, not hidden, while it cannot act).
#pragma once
#include "../Domain/BrowserRules.hpp"
#include "PageDeletePlan.hpp"
#include "ProjectData.hpp"
#include <optional>
#include <set>
#include <string>

namespace ost_data
{
    // The page bar the pressed button belongs to.
    enum class PageMenuBar
    {
        Bar2D,
        Bar3D
    };

    struct PageMenuItem
    {
        bool shown = false;   // the item is in the menu
        bool enabled = false; // the item can be tapped (never true for a hidden item)
    };

    // The items of the page menu, in the order the screen shows them: Set Scale..., Rename Page..., Delete Page.
    struct PageMenuState
    {
        PageMenuItem set_scale;   // shown on the 2D bar only (N-M1-05), the first item
        PageMenuItem rename_page; // shown on both bars
        PageMenuItem delete_page; // shown on the 2D bar only (N-M1-05)
    };

    // The state of the menu of the page `page_uid`, computed every time the menu is shown (nothing is remembered between two calls):
    //   set_scale:   shown on Bar2D only; enabled = a bid is open, the page is a page of the open project and no area placement is in progress
    //                (ui_event_coordinator.py:5496-5510, _PLACEMENT_BLOCKED); a single page does not disable it;
    //   rename_page: shown; enabled = ost_domain::can_rename_page (a bid is open, the page is a page of the open project, no area
    //                placement in progress);
    //   delete_page: shown on Bar2D only; enabled = ost_data::plan_page_delete allows the page (a bid is open, the page is a page of the tree,
    //                it is not the only page: Windows can_delete_current_page :5893) and no area placement is in progress.
    // `page_uid` is compared as exact text. The project is never changed.
    PageMenuState page_menu_state(const ProjectData &project, const std::string &page_uid, PageMenuBar bar, bool area_placement_in_progress);

    // Whether the Delete confirmation names the content of the page (ui_event_coordinator.py:5922-5926: the uid is in `pages_with_content`,
    // the stored scan of ost_store::pages_with_delete_content, OR the loaded model has a takeoff on the page). An empty uid is never
    // content. The uids are compared as exact text ("7" is not "007"). iOS asks in both cases (N-M1-23); only the wording differs.
    bool page_delete_has_content(const ProjectData &project, const std::string &page_uid, const std::set<std::string> &pages_with_content);

    // Delete Page, after the reload (ui_event_coordinator.py:5989-6007, :1100-1131, register DP-10): the page the reloaded project starts on.
    // `requested_uid` (the page that followed the deleted one, ost_data::plan_page_delete) when the project has that page, else the page a
    // load opens (ost_data::initial_page_uid: the saved page when it still exists, else the first page of the tree walk). "" for a project
    // without pages. The uid is compared as exact text. The project is never changed.
    std::string page_selection_after_reload(const ProjectData &project, const std::string &requested_uid);

    // The alert the screen shows when ost_data::plan_page_delete refuses a delete BEFORE any question (ui_event_coordinator.py:5995-6001):
    // the last page is announced with {LAST_PAGE_TITLE, LAST_PAGE_MESSAGE}. A page the project does not know and an empty project get no
    // alert (Windows stages nothing and says nothing, :5993-5994), and an allowed plan has nothing to say: std::nullopt for those.
    std::optional<ost_domain::DialogText> page_delete_refusal_text(PageDeleteRefusal refusal);
}
