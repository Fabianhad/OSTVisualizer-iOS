// Layer: Data
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
// The page tree, the first page and the page selection rules of the page selector (LP-08, LP-10, LP-11, LP-12; EXP-09
// for the order of the page names). Port of the Windows presentation/components/page_combo.py (PageComboBox /
// SinglePageComboBox) and of the 'which page opens first' rule of presentation/coordinators/ui_event_coordinator.py
// (_resolve_takeoff_page_choice). Services/ProjectService.mm only keeps the selection state and calls these functions.
#include "ProjectData.hpp"
#include <set>
#include <string>
#include <vector>

namespace ost_data
{

    // The page tree walk of Windows (PageComboBox.get_page_order = _walk_page_uids over the model built by load_bid): the
    // folders at the top level in order (Name, UID), each with its subfolders first (in order, recursively) and then its own
    // pages, and after all folders the pages without a folder. Pages are in page order (project.page_order: BidPages
    // ORDER BY Sequence, Name, UID), not in Index1 order. A page that project.page_order does not list (a hand-built
    // project; load_project lists every page) follows at the end in (Sequence, Name, UID text) order, so no page can
    // vanish from the selector.
    std::vector<std::string> page_tree_order(const ProjectData &project);

    // The pages of `selected` in the order of page_tree_order (what the page selector shows first; the selection itself
    // keeps no click order on iOS), without a page the project does not have.
    std::vector<std::string> selected_pages_in_tree_order(const ProjectData &project, const std::set<std::string> &selected);

    // The page both the 3D selection and the 2D view start with (Windows: the restored selection, else
    // BidSettings.BidPageSelectedUID when that page still exists, else get_first_page_uid = the first page of the tree
    // walk); "" for a project without pages. A saved page that no longer exists falls back to the first page.
    std::string initial_page_uid(const ProjectData &project);

    // The folder check box of the page selector (Windows PageComboBox._apply_folder_check_state): a folder whose pages are
    // ALL selected is unchecked, which removes exactly those pages from `selected`; any other folder (none or only some of
    // its pages selected) is checked, which adds every page below it. Pages selected elsewhere are kept. `folder_pages` are
    // the pages below the folder, nested folders included (Folder::all_page_uids()). A folder without pages has no check
    // box: nothing happens. Returns whether `selected` changed (Windows emits an update only then).
    bool toggle_folder_pages(std::set<std::string> &selected, const std::vector<std::string> &folder_pages);

    // Activating a page row (Windows PageComboBox._set_active / SinglePageComboBox: a row only activates when its uid is
    // not the active one; the active page is never cleared by clicking it again). Returns whether the active page changed.
    bool activate_page(std::string &active_uid, const std::string &uid);

    // The text of a page button (Windows page_combo._format_page_label): [sequence][sheet number][name] joined by " - ", where
    // the sequence (only when it is above 0) and the sheet number (only when it is not empty) are shown only when the matching
    // preference is on (both are off by default and iOS has no such preference); a page with none of the parts shows its uid.
    std::string page_label(const ost_domain::Page &page, bool show_page_index = false, bool show_sheet_number = false);

}
