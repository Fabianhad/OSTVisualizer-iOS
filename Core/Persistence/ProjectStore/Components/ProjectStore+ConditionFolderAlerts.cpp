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
            case ConditionFolderAction::Create: return ost_domain::kNewFolderTitle;
            case ConditionFolderAction::Rename: return ost_domain::kRenameFolderTitle;
            case ConditionFolderAction::Delete: return ost_domain::kConditionFolderDeleteTitle;
            case ConditionFolderAction::Move: return ost_domain::kMoveToFolderTitle;
            }
            return "Folder";
        }

        // The Windows sentences of a failed folder write (HEAD d26893c texts.py:711-722, shown by condition_action_handler._report_folder_write_failure),
        // each + the database-locked hint. The TITLES stay the iOS ones (the title of the name alert of the action, owner choice, CF-7). The move
        // sentence is iOS: Windows only logs a failed move.
        std::string generic_message(ConditionFolderAction action, std::size_t item_count)
        {
            std::string text;
            switch (action)
            {
            case ConditionFolderAction::Create: text = "Failed to create the condition folder."; break;
            case ConditionFolderAction::Rename: text = "Failed to rename the condition folder."; break;
            case ConditionFolderAction::Delete:
                text = item_count > 1 ? "Failed to delete " + std::to_string(item_count) + " condition folders." : "Failed to delete the condition folder.";
                break;
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
        // the text of the store is shown only when the store gave one: an empty text is the generic failure of the action, never an empty body
        case ConditionFolderRefusal::DuplicateName:
            if (!result.message.empty())
                return ost_domain::DialogText{"Duplicate Folder", result.message};
            break;
        case ConditionFolderRefusal::NameTooLong:
            if (!result.message.empty())
                return ost_domain::DialogText{ost_domain::kInvalidFolderNameTitle, result.message};
            break;
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
