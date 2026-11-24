// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The decisions and texts that the Conditions list and the folder manager screens need on top of the folder rules (ConditionFolderRules.hpp)
// and the New Condition rules (ConditionCreate.hpp) (plan 5.1 and 5.2, UI of stream S1): the Objective-C only converts types, shows these and
// calls the writers.
//
// Windows source of truth (HEAD 49a2d59):
//   presentation/components/conditions_sidebar.py:423-513, :606-638   the tree the folder rows come from (a folder whose parent is not a known
//                                                                    folder is a ROOT folder, siblings sort by name; ported in ConditionTree.hpp)
//   application/dtos/create_condition_spec_dto.py:5-43                a new condition has no ref_no: the writer gives MAX(RefNo of the bid) + 1
//                                                                    (infrastructure/mdb/components/condition_operations.py:42-66), which the
//                                                                    create form shows read-only (N-M1-07, register NC-1)
// iOS-only texts (plan 4.14, N-M1-03: the owner may reword them): the folder count line, the "Next number" line, the invalid-name alert and the
// titles of the new menu items.
#pragma once
#include "BrowserRules.hpp"
#include "CdnType.hpp"
#include "Condition.hpp"
#include "ConditionFolder.hpp"
#include "ConditionFolderRules.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_domain
{
    // ---- the folder manager rows ------------------------------------------------------------------------------------------------

    // One row of the folder manager (and of the "Move to Folder" picker): a folder of the bid in the order of the Conditions tree.
    struct ConditionFolderRow
    {
        std::string uid;
        std::string name;
        int depth = 0;           // 0 = a top-level folder, else the number of folders above it (the indentation of the row)
        int condition_count = 0; // conditions placed DIRECTLY in this folder (not those of a sub-folder)
    };

    // The folders of ONE bid as the Conditions tree lists them (build_condition_tree: nested through ParentUID, a folder whose parent is not
    // a folder of `folders` is a top-level folder, siblings sorted by name in code point order, equal names in database read order), pre-order,
    // so a folder's sub-folders follow it. `condition_count` counts the conditions whose folder uid is that folder; a condition whose folder uid
    // is empty, "0" or not a folder of `folders` sits at the top level and counts nowhere. `cdn_types` only matters for the tree the order is
    // taken from. The walk is iterative (a chain of 8000 folders is read by Windows, ConditionTree.cpp).
    std::vector<ConditionFolderRow> condition_folder_rows(const std::unordered_map<std::string, ConditionFolder> &folders,
                                                          const std::unordered_map<std::string, Condition> &conditions,
                                                          const std::unordered_map<std::string, CdnType> &cdn_types);

    // The second line of a folder row: "No conditions", "1 condition", "N conditions" (a negative count reads as none).
    std::string condition_folder_count_text(int count);

    // The folder a condition is listed under, as the Conditions tree seats it: its folder uid when that is a folder of `folders`, else "" (the top
    // level: an empty uid, "0" and a uid that names no folder of the bid all sit at the top, conditions_sidebar.py:461-467). The "Move to Folder"
    // picker puts its checkmark on this folder, or on "(No folder)" for "".
    std::string condition_listed_folder_uid(const std::unordered_map<std::string, ConditionFolder> &folders, const Condition &condition);

    // ---- New Condition ----------------------------------------------------------------------------------------------------------

    // The title of the "New Condition" item of a long-press sheet: "New Condition" for the top level (an empty `folder_uid`), else
    // "New Condition in <folder_name>" ("New Condition in Folder" when the folder has no name).
    std::string new_condition_action_title(const std::string &folder_uid, const std::string &folder_name);

    // The text of the read-only Cond. No. row of the create form (N-M1-07): "Next number: N" where N = the highest reference number of
    // `conditions` + 1, 1 when there is none. A condition without a number (0) and a negative number do not count. Exactly what
    // ost_store::insert_condition assigns (MAX(RefNo of the bid) + 1, condition_operations.py:42-66) for the conditions of ONE bid; the
    // arithmetic is 64 bit, so a highest number of 2147483647 reads "Next number: 2147483648" (the writer refuses that one).
    std::string next_condition_number_text(const std::unordered_map<std::string, Condition> &conditions);

    // ---- titles of the folder screens -------------------------------------------------------------------------------------------

    inline constexpr const char *kFoldersButtonTitle = "Folders";
    inline constexpr const char *kFoldersScreenTitle = "Folders";
    inline constexpr const char *kMoveToFolderAction = "Move to Folder...";   // the item of the condition long-press sheet
    inline constexpr const char *kMoveToFolderTitle = "Move to Folder";       // the picker screen
    inline constexpr const char *kNoFolderRowTitle = "(No folder)";           // the picker row for the top level

    // ---- the refusal of a typed folder name -------------------------------------------------------------------------------------

    inline constexpr const char *kInvalidFolderNameTitle = "Invalid Folder Name";
    // iOS text: a name that is not valid UTF-8 or holds a NUL cannot be stored (check_condition_folder_name: InvalidText has no dialog there).
    inline constexpr const char *kInvalidFolderNameText = "The folder name contains characters that cannot be saved.";

    // What to show for a refused name check (check_condition_folder_name / decide_condition_folder_rename): the Windows "Duplicate Folder"
    // warning, the length text, the invalid-text alert. nullopt for None and for Blank (Windows reverts silently, :1466-1469).
    std::optional<DialogText> condition_folder_name_refusal_dialog(const FolderNameCheck &check);
}
