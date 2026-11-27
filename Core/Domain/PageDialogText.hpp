// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// Texts of the page screens (plan 5.5, 5.6) that no other Core file holds: the prompt of the Rename Page alert, the text of a delete that
// was committed although the bid could not be read again, and the alert of a delete the store refused.
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/presentation/dialogs/rename_page_dialog.py:55-100   title "Rename Page", the read-only "Old name:" row
//   ost_visualizer/application/services/project_write_service.py:428-438, :6945   a committed delete is reported "Failed to delete page"
//                                                                                  when the reload fails (register DP-2): iOS says it happened
#pragma once
#include "BrowserRules.hpp"
#include <string>

namespace ost_domain
{
    // The alert that asks for the new name. Title "Rename Page" (Windows), message "Old name: <name>" (the read-only Old Name row of the
    // Windows dialog). `stored_name` is the raw stored name, put in as it is: not stripped, not shortened, not the page label (an empty
    // stored name gives "Old name: ", register RP-1, RP-5, RP-6).
    DialogText page_rename_prompt_text(const std::string &stored_name);

    // The delete was committed but the bid could not be read again (iOS wording, plan 5.6 DP-2; Windows would say "Failed to delete page"
    // although the page is gone). Title "Delete Page"; the message says the page was deleted, gives `reason` when it is not empty, and
    // says the bid will close.
    //   with a reason:    "The page was deleted, but the bid could not be read again: <reason>\n\nThe bid will close. Open it again to continue."
    //   without a reason: "The page was deleted, but the bid could not be read again.\n\nThe bid will close. Open it again to continue."
    DialogText page_delete_reload_failed_text(const std::string &reason);

    // The alert for a delete that the STORE refused (`reason` is the `reason` of ost_store::delete_pages, the errorText of
    // +[ImportService deletePages:inBid:error:], "" when the store gave none). The last page has its own text: a reason that is exactly
    // LAST_PAGE_MESSAGE gives {LAST_PAGE_TITLE, LAST_PAGE_MESSAGE}. Every other reason (a failed identity check, a failed statement, a failed
    // read, a locked bid: the reason is for the log) gives the generic page_delete_failure_text(Delete) with its database-locked hint
    // (ui_event_coordinator.py:5962-5968). The comparison is exact: a reason that only looks like the last-page text is a generic failure.
    DialogText page_delete_store_failure_text(const std::string &reason);
}
