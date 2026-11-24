// Layer: Data. The page menu rules: see PageMenuRules.hpp (plan 4.8, 5.5, 5.6).
#include "PageMenuRules.hpp"
#include "PageDeletePlan.hpp"
#include "PageNavigation.hpp"
#include "../Domain/PageDeletion.hpp"
#include "../Domain/PageRules.hpp"

namespace ost_data
{
    PageMenuState page_menu_state(const ProjectData &project, const std::string &page_uid, PageMenuBar bar, bool area_placement_in_progress)
    {
        PageMenuState state;

        // Rename Page...: ui_event_coordinator.py:5714-5731 (a bid is open, the page resolves) plus the Feature.EDIT_PAGE_SETTINGS gate
        // (_PLACEMENT_BLOCKED: an area placement in progress). A blank or unknown uid names no page.
        ost_domain::RenameTargetState rename;
        rename.bid_open = !project.empty();
        rename.target_page_exists_in_project = !page_uid.empty() && project.pages.find(page_uid) != project.pages.end();
        rename.area_placement_in_progress = area_placement_in_progress;
        state.rename_page.shown = true;
        state.rename_page.enabled = ost_domain::can_rename_page(rename);

        // Delete Page: only the 2D bar offers it (N-M1-05). The item is disabled, not hidden, when it cannot act: can_delete_current_page
        // (:5881-5893) needs the page and more than one page, which is plan_page_delete's allowed; an area placement blocks it as well.
        if (bar == PageMenuBar::Bar2D)
        {
            state.delete_page.shown = true;
            state.delete_page.enabled = !area_placement_in_progress && plan_page_delete(project, page_uid).allowed;
        }
        return state;
    }

    std::string page_selection_after_reload(const ProjectData &project, const std::string &requested_uid)
    {
        // _stage_selection_after_page_delete (:5989-6007) stages a page of the project; a page the reloaded project does not have cannot be
        // selected, so the load opens the page it always opens.
        if (!requested_uid.empty() && project.pages.find(requested_uid) != project.pages.end())
            return requested_uid;
        return initial_page_uid(project);
    }

    std::optional<ost_domain::DialogText> page_delete_refusal_text(PageDeleteRefusal refusal)
    {
        // ui_event_coordinator.py:5993-6001: the last page is warned about; a page that is not in the page order stages nothing and says
        // nothing (:5993-5994); an empty project has no page to delete.
        if (refusal == PageDeleteRefusal::LastPage)
            return ost_domain::DialogText{ost_domain::LAST_PAGE_TITLE, ost_domain::LAST_PAGE_MESSAGE};
        return std::nullopt;
    }

    bool page_delete_has_content(const ProjectData &project, const std::string &page_uid, const std::set<std::string> &pages_with_content)
    {
        if (page_uid.empty())
            return false;
        // ui_event_coordinator.py:5922-5926: the stored scan names the page, or the loaded model has a takeoff on it.
        if (pages_with_content.count(page_uid) > 0)
            return true;
        for (const ost_domain::Takeoff &takeoff : project.takeoffs)
            if (takeoff.page_uid == page_uid)
                return true;
        return false;
    }
}
