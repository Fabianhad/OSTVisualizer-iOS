// Layer: Persistence
// Depends on: Domain (BrowserRules, ConditionFolderRules, ConditionFolderUi), ProjectStore_ConditionFolders
// Forbidden: UIKit, Foundation, Renderer, sqlite
// The alert a refused or failed condition folder write shows (plan 5.2, N-M1-17): the Windows texts where Windows has one, the iOS wording and the
// database-locked hint where it only logs (register CF-7). The folder screens call it with the result of the writers of
// ProjectStore_ConditionFolders.hpp, so no screen decides which refusal says what.
#pragma once
#include "ProjectStore_ConditionFolders.hpp"
#include "../../Domain/BrowserRules.hpp"
#include "../../Domain/ConditionFolderRules.hpp"
#include <optional>
#include <vector>

namespace ost_store
{
    enum class ConditionFolderAction
    {
        Create,
        Rename,
        Delete,
        Move // "Move to Folder" of one condition
    };

    // The dialog of a result that is not ok; nullopt when there is nothing to say (a blank name: Windows reverts silently, conditions_sidebar.py:1466-1469).
    // `items` are the folders (name and uid) of the request of a Delete, to name the blocked ones; it only matters for InUse and for "folder" or
    // "folders" in the generic text. A result with ok == true has no dialog (nullopt).
    //   BidLocked                              "Bid Locked" / "This bid is locked."
    //   DuplicateName                          Windows "Duplicate Folder" / result.message ("Folder X already exists.")
    //   NameTooLong                            "Invalid Folder Name" / result.message
    //   InvalidName                            "Invalid Folder Name" / ost_domain::kInvalidFolderNameText
    //   InUse                                  Windows "Delete Folder" / one line `Cannot delete "X" because it is in use.` per blocked folder
    //   UidExhausted, HierarchyNotSupported    the title of the action / result.message
    //   every other refusal                    the title of the action / "Failed to <create|rename|delete|move> ... ." + " " + DB_LOCKED_HINT
    std::optional<ost_domain::DialogText> condition_folder_failure_dialog(ConditionFolderAction action,
                                                                         const ConditionFolderResult &result,
                                                                         const std::vector<ost_domain::FolderDeleteItem> &items);
}
