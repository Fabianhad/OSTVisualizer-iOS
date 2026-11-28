// Layer: Domain. The texts of the page screens: see PageDialogText.hpp (plan 5.5, 5.6).
#include "PageDialogText.hpp"
#include "PageDeletion.hpp"

namespace ost_domain
{
    DialogText page_rename_prompt_text(const std::string &stored_name)
    {
        // rename_page_dialog.py:55-100: the dialog is titled "Rename Page" and its read-only row "Old name:" shows the stored name.
        return {"Rename Page", "Old name: " + stored_name};
    }

    DialogText page_delete_reload_failed_text(const std::string & /* reason: for the log only */)
    {
        return {"Delete Page", "The page was deleted, but the bid could not be read again.\n\nThe bid will close. Open it again to continue."};
    }

    DialogText page_delete_store_failure_text(const std::string &reason)
    {
        // ui_event_coordinator.py:5962-5968: every refusal of the delete is "Failed to delete page. <hint>"; the last page is the one the
        // store names exactly (ProjectStore delete_pages sets LAST_PAGE_MESSAGE) and Windows warns about it under its own text (:5995-6001).
        if (reason == LAST_PAGE_MESSAGE)
            return {LAST_PAGE_TITLE, LAST_PAGE_MESSAGE};
        return page_delete_failure_text(PageDeleteFailure::Delete);
    }
}
