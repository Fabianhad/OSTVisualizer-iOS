#include "BrowserRules.hpp"
#include "../Import/TextRules.hpp"

namespace ost_domain
{
    std::string normalize_folder_name(const std::string &entered)
    {
        return ost_import::text_rules::py_strip(entered);
    }

    FolderRenameDecision decide_folder_rename(const std::string &entered, const std::string &shown_name, std::string &new_name)
    {
        new_name = normalize_folder_name(entered);
        if (new_name.empty())
            return FolderRenameDecision::Refused;
        if (new_name == shown_name)
            return FolderRenameDecision::Unchanged;
        return FolderRenameDecision::Write;
    }

    std::string display_name_for_folder(const std::string &folder_uid, const std::string &name)
    {
        return name.empty() ? "Project " + folder_uid : name;
    }

    std::string display_name_for_bid(const std::string &bid_uid, const std::string &name)
    {
        return name.empty() ? "Bid " + bid_uid : name;
    }

    DialogText trash_bids_confirmation(size_t count)
    {
        if (count == 1)
            return {"Move to Deleted Bids", "Move this bid to Deleted Bids?\nYou can permanently delete it from there."};
        return {"Move to Deleted Bids",
                "Move " + std::to_string(count) + " bids to Deleted Bids?\nYou can permanently delete them from there."};
    }

    DialogText delete_bids_confirmation(size_t count)
    {
        if (count == 1)
            return {"Delete Bids", "Permanently delete this bid and all its data?\nThis cannot be undone."};
        return {"Delete Bids",
                "Permanently delete " + std::to_string(count) + " bids and all their data?\nThis cannot be undone."};
    }

    DialogText delete_folders_confirmation(size_t count)
    {
        if (count == 1)
            return {"Delete Projects", "Permanently delete this empty project?\nThis cannot be undone."};
        return {"Delete Projects", "Permanently delete " + std::to_string(count) + " empty projects?\nThis cannot be undone."};
    }

    DialogText browser_failure_text(BrowserFailure failure)
    {
        const std::string hint = std::string(" ") + DB_LOCKED_HINT;
        switch (failure)
        {
        case BrowserFailure::RenameFolder:
            return {"Rename Error", "Failed to rename project." + hint};
        case BrowserFailure::DeleteBids:
            return {"Delete Error", "Failed to delete bids." + hint};
        case BrowserFailure::TrashBids:
            return {"Move Error", "Failed to move bids to Deleted Bids." + hint};
        case BrowserFailure::RestoreBids:
            return {"Restore Error", "Failed to restore bids." + hint};
        case BrowserFailure::MoveBids:
            return {"Move Error", "Failed to move bids." + hint};
        case BrowserFailure::DeleteFolders:
            return {"Delete Error", "Failed to delete projects." + hint};
        case BrowserFailure::CreateFolder:
            return {"New Project", "Failed to create project." + hint};
        case BrowserFailure::SaveSettings:
            return {"Save Failed", "Failed to save the display settings."};
        }
        return {"Error", "The operation failed."};
    }

    DialogText load_failure_text(const std::string &display_name, const std::string &reason)
    {
        return {"Error Loading File", "Failed to load " + display_name + ":\n" + reason};
    }
}
