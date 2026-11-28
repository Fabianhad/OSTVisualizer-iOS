// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The rules of Delete Page that have no database in them (plan 5.6): the page tables of the cascade, the name a page has in the
// confirmation, the normalisation of a page list, and the texts of the dialogs. The store (ost_store::delete_pages,
// pages_with_delete_content) and the screens call these functions and add only the database and the platform widgets.
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/infrastructure/mdb/components/constants.py:83-121   PAGE_DELETE_CHILD_TABLES, PAGE_DELETE_CONFIRMATION_TABLES
//   ost_visualizer/infrastructure/database/bid_owned_identity.py:6      the chunk of 50 uids per statement
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5895-5968, :6009-6015   delete_current_page, display name
//   ost_visualizer/presentation/utils/messagebox.py:8, :52-61           DB_LOCKED_HINT, confirm_delete_page_with_contents
//   ost_visualizer/application/services/project_write_service.py:6884-6892   _unique_nonempty_uids
#pragma once
#include "BrowserRules.hpp"
#include "Page.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace ost_domain
{
    // constants.py:83-110, in the Windows order: the 13 PAGE_ANNOTATION_TABLES (BidDimensions ... BidLegends, BidComments) then the 10
    // PAGE_AUXILIARY_CHILD_TABLES (BidPageSettings ... DPCCalcFilter). The tables the cascade deletes by their BidPageUID column in
    // step 8; BidTakeoffs, BidNamedViews and BidHotLinks are handled by their own steps (9, 10-12). The bid cascade
    // (permanently_delete_project) reads the SAME list, so a page table is added in one place.
    extern const char *const PAGE_DELETE_CHILD_TABLES[];
    extern const std::size_t PAGE_DELETE_CHILD_TABLE_COUNT; // 23

    // constants.py:111-121, in the Windows order: PAGE_CONTENT_TABLES without BidLegends: BidTakeoffs, the annotation tables, BidComments,
    // BidNamedViews, BidHotLinks. A page with a row in any of them asks for the content confirmation (a page with only a legend does not).
    extern const char *const PAGE_DELETE_CONFIRMATION_TABLES[];
    extern const std::size_t PAGE_DELETE_CONFIRMATION_TABLE_COUNT; // 15

    // bid_owned_identity.py:6 / bulk_write_helpers.py:6: the uids of one statement. An Access parameter limit; for SQLite only a boundary.
    constexpr std::size_t kBidOwnedUidChunk = 50;

    // ui_event_coordinator._page_delete_display_name: the page name when it is not empty; else the last component of image_path split on
    // '\' and then on '/'; else "Page <uid>" (an empty image path falls through).
    std::string page_delete_display_name(const Page &page);

    // project_write_service._unique_nonempty_uids: the list without empty texts and without repeats, in the order of the first
    // occurrence. Nothing else is changed (no trimming, "007" and "7" are two entries): the store judges the uids.
    std::vector<std::string> normalize_page_uids(const std::vector<std::string> &uids);

    // The confirmation of a delete (title "Delete Verification"; iOS always asks, plan N-M1-23, texts N-M1-03). `display_name` is put into
    // the quotes as it is.
    //   has_content: "The page \"<name>\" contains takeoffs, annotations, comments, Named Views or hotlinks. Deleting this page will remove
    //                 the page and everything on it from this bid.\n\nDeleting a page cannot be undone. Are you sure you want to delete this page?"
    //   empty page:  "Delete the page \"<name>\"?\n\nDeleting a page cannot be undone."
    // (Windows messagebox.py:55-60 promises the removal "in the Base Bid, and any Alternates or Change Orders" and names only some of the
    // content: register DP-3.)
    DialogText page_delete_confirmation(const std::string &display_name, bool has_content);

    enum class PageDeleteFailure
    {
        VerifyContent, // "Delete Page" / "Failed to verify page contents. <DB_LOCKED_HINT>"  (ui_event_coordinator.py:5915-5921)
        Delete         // "Delete Page" / "Failed to delete page. <DB_LOCKED_HINT>"           (:5962-5968)
    };
    DialogText page_delete_failure_text(PageDeleteFailure failure);

    // ui_event_coordinator.py:5995-6001: title "Delete Page"; also the `error` of ost_store::delete_pages when it refuses the last page.
    constexpr const char *LAST_PAGE_TITLE = "Delete Page";
    constexpr const char *LAST_PAGE_MESSAGE = "Cannot delete the last page in the bid.";
}
