// Layer: Persistence. The contract is in ProjectStore_ConditionFolderAlerts.hpp; this file is only the body.
#include "../ProjectStore_ConditionFolderAlerts.hpp"
#include "../../../Domain/ConditionFolderUi.hpp"

#include <unordered_set>

namespace ost_store
{
    namespace
    {
        const char *action_title(ConditionFolderAction action)
        {
            switch (action)
            {
            case ConditionFolderAction::Create: return "New Folder";
            case ConditionFolderAction::Rename: return "Rename Folder";
            case ConditionFolderAction::Delete: return ost_domain::kConditionFolderDeleteTitle;
            case ConditionFolderAction::Move: return ost_domain::kMoveToFolderTitle;
            }
            return "Folder";
        }

        // iOS wording, the pattern of BrowserFailure ("Failed to rename project. <hint>"): Windows only logs these failures (CF-7).
        std::string generic_message(ConditionFolderAction action, std::size_t item_count)
        {
            std::string text;
            switch (action)
            {
            case ConditionFolderAction::Create: text = "Failed to create folder."; break;
            case ConditionFolderAction::Rename: text = "Failed to rename folder."; break;
            case ConditionFolderAction::Delete: text = item_count > 1 ? "Failed to delete folders." : "Failed to delete folder."; break;
            case ConditionFolderAction::Move: text = "Failed to move the condition."; break;
            }
            return text + " " + ost_domain::DB_LOCKED_HINT;
        }
    }

    std::optional<ost_domain::DialogText> condition_folder_failure_dialog(ConditionFolderAction action,
                                                                         const ConditionFolderResult &result,
                                                                         const std::vector<ost_domain::FolderDeleteItem> &items)
    {
        if (result.ok || result.refusal == ConditionFolderRefusal::None)
            return std::nullopt;

        switch (result.refusal)
        {
        case ConditionFolderRefusal::None:
        case ConditionFolderRefusal::BlankName:
            return std::nullopt; // Windows reverts silently (conditions_sidebar.py:1466-1469)
        case ConditionFolderRefusal::BidLocked:
            return ost_domain::DialogText{"Bid Locked", "This bid is locked."};
        case ConditionFolderRefusal::DuplicateName:
            return ost_domain::DialogText{"Duplicate Folder", result.message};
        case ConditionFolderRefusal::NameTooLong:
            return ost_domain::DialogText{ost_domain::kInvalidFolderNameTitle, result.message};
        case ConditionFolderRefusal::InvalidName:
            return ost_domain::DialogText{ost_domain::kInvalidFolderNameTitle, ost_domain::kInvalidFolderNameText};
        case ConditionFolderRefusal::UidExhausted:
        case ConditionFolderRefusal::HierarchyNotSupported:
            if (!result.message.empty())
                return ost_domain::DialogText{action_title(action), result.message};
            break;
        case ConditionFolderRefusal::InUse:
        {
            // messagebox.py:84-105: the folders that are in use are named, one line each, in the order of the request.
            const std::unordered_set<std::string> blocked(result.blocked.begin(), result.blocked.end());
            std::vector<ost_domain::FolderDeleteItem> named;
            for (const ost_domain::FolderDeleteItem &item : items)
                if (blocked.count(item.uid) > 0)
                    named.push_back(item);
            if (!named.empty())
            {
                // every item of `named` is blocked, so the confirmation is the warning with the lines only
                return ost_domain::condition_folder_delete_confirmation(named, result.blocked, false).dialog;
            }
            break;
        }
        default:
            break;
        }
        return ost_domain::DialogText{action_title(action), generic_message(action, items.size())};
    }
}
