// Layer: Data
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The decision of Delete Page that needs the loaded project (plan 5.6): may this page be deleted and which page is shown afterwards.
// Windows: ui_event_coordinator.can_delete_current_page (:5881-5893: the page exists and the page tree has more than one page) and
// _stage_selection_after_page_delete (:5989-6007: the tree order of the page selector, remaining = the order without the page,
// next = remaining[min(index of the page, remaining.size() - 1)], so the page that followed the deleted one becomes current and the
// previous one when the last page goes). The tree order is ost_data::page_tree_order (PageNavigation.hpp).
#pragma once
#include "ProjectData.hpp"
#include <string>

namespace ost_data
{
    enum class PageDeleteRefusal
    {
        None,        // allowed
        NoProject,   // the project is empty (no bid loaded)
        UnknownPage, // the uid is empty or names no page of the project
        LastPage     // the page is the only page of the bid (Windows: "Cannot delete the last page in the bid.")
    };

    struct PageDeletePlan
    {
        bool allowed = false;
        PageDeleteRefusal refusal = PageDeleteRefusal::None;
        std::string next_page_uid; // the only page selected afterwards (2D and 3D); "" when refused
        std::string display_name;  // ost_domain::page_delete_display_name of the page; "" when the page is unknown
    };

    // Refusal order: an empty project (NoProject), a uid that is no page of the tree (UnknownPage: Windows stages nothing and says nothing),
    // a project whose tree has no other page (LastPage: Windows warns "Cannot delete the last page in the bid."). Reads only; the project is
    // never changed. The store re-checks the last-page rule (ost_store::delete_pages), so this plan only decides what the screen shows.
    PageDeletePlan plan_page_delete(const ProjectData &project, const std::string &page_uid);
}
