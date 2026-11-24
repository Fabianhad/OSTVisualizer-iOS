// Layer: Domain. The texts of the page screens: see PageDialogText.hpp (plan 5.5, 5.6).
#include "PageDialogText.hpp"

namespace ost_domain
{
    DialogText page_rename_prompt_text(const std::string &stored_name)
    {
        // rename_page_dialog.py:55-100: the dialog is titled "Rename Page" and its read-only row "Old Name:" shows the stored name.
        return {"Rename Page", "Old Name: " + stored_name};
    }

    DialogText page_delete_reload_failed_text(const std::string &reason)
    {
        std::string message = "The page was deleted, but the bid could not be read again";
        message += reason.empty() ? std::string(".") : ": " + reason;
        message += "\n\nThe bid will close. Open it again to continue.";
        return {"Delete Page", message};
    }
}
