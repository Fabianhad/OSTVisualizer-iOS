// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The rules of the condition folders (plan 5.2) that have no database in them: the name check (strip, blank, length, sibling duplicates),
// the rename decision, the delete plan (THE in-use rule) and the texts of the delete confirmation. The screens and the store both call
// them: the store re-checks every rule inside its own transaction from database rows (the store never trusts the screen).
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/presentation/components/conditions_sidebar.py    :1455-1481 _on_folder_editor_closed (the rename rule),
//                                                                    :1483-1497 _sibling_folder_name_exists, :1499-1513 _resolve_folder_context_uid,
//                                                                    :461-475 (a folder whose parent is not a known folder is a ROOT folder)
//   ost_visualizer/presentation/handlers/condition_action_handler.py :1392 and :1408 (the default name "New Folder"), :1514-1575
//                                                                    (delete: an unknown uid = nothing happens, then validate, confirm, delete)
//   ost_visualizer/application/services/project_write_service.py    :855-864 validate_condition_folder_delete, :936-953 _condition_folder_uids_in_use
//   ost_visualizer/presentation/utils/messagebox.py                 :78-119 confirm_multi_delete (the texts; HEAD d26893c: :288-340, texts.py:1204-1205 and :1452-1457)
//   condition_action_handler.py (HEAD d26893c)                      :1336-1349 _free_folder_name (the name of a new folder)
#pragma once
#include "BrowserRules.hpp"
#include "Condition.hpp"
#include "ConditionFolder.hpp"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_domain
{
    // condition_action_handler.py:1392 / :1408: the name of a new folder; the first candidate of free_condition_folder_name (the iOS alert is prefilled with that).
    constexpr const char *kDefaultConditionFolderName = "New Folder";

    // BidConditionFolders.Name is VARCHAR(50) (database_creator.py:142), counted in UTF-16 code units (TextLimits.hpp: an emoji takes two).
    // Windows checks no length (register CF-6): refusing more is the plan's safe behaviour (N-M1-13).
    constexpr std::size_t kMaxConditionFolderNameUnits = 50;

    // The title of the delete confirmation of Windows (condition_action_handler.py:1543: confirm_multi_delete(..., "Delete Folder", ...)).
    constexpr const char *kConditionFolderDeleteTitle = "Delete Folder";

    // iOS-only line of the delete confirmation (plan 4.14, N-M1-16; the owner may reword it, N-M1-03): the sub-folders of a deleted folder
    // move to the TOP level (condition_folder_operations.py:124-131), which the Windows dialog does not say (register CF-3).
    constexpr const char *kConditionFolderSubfoldersMoveText = "Folders inside it move to the top level.";

    // The Windows text of a folder insert under a parent when the file has no ParentUID column (condition_folder_operations.py:26-30).
    constexpr const char *kConditionFolderHierarchyNotSupportedText =
        "This OST database does not support Condition folder hierarchy persistence.";

    // ---- the folder a folder is listed under ----------------------------------------------------------------------------------

    // conditions_sidebar.py:469-475 (`parent = folder.parent_uid if folder.parent_uid in self._folders else None`): "" (the root) when the
    // folder's parent_uid is empty, "0" or not the uid of a folder of `folders`; else the parent's uid. `folders` = the folders of ONE bid.
    std::string effective_condition_folder_parent(const std::unordered_map<std::string, ConditionFolder> &folders,
                                                  const ConditionFolder &folder);

    // ---- the name of a new folder ---------------------------------------------------------------------------------------------

    // condition_action_handler._free_folder_name (Windows HEAD d26893c :1336-1349, texts.py:723 "New Folder ({number})"): the first name that no
    // SIBLING has. Siblings = the folders whose effective parent is the effective parent of `parent_uid` ("" = the root; "0" and a uid that is not
    // a folder of `folders` count as the root, like effective_condition_folder_parent). Candidates in order: "New Folder" (kDefaultConditionFolderName),
    // "New Folder (2)", "New Folder (3)", ...; a candidate is taken when py_lower(py_strip(candidate)) equals py_lower(py_strip(sibling.name)) of a
    // sibling (the same comparison as check_condition_folder_name). A name under ANOTHER parent does not count, and "New Folder (3)" alone does
    // not make "New Folder" taken. The result passes the DuplicateSibling check of check_condition_folder_name for the same parent.
    std::string free_condition_folder_name(const std::unordered_map<std::string, ConditionFolder> &folders, const std::string &parent_uid);

    // ---- the name rule --------------------------------------------------------------------------------------------------------

    enum class FolderNameIssue
    {
        None,
        Blank,           // nothing left after the strip: silent (Windows reverts, no dialog, no write)
        InvalidText,     // not valid UTF-8, or a NUL byte (a NUL cuts a C string): refused, never repaired (TextLimits.hpp)
        TooLong,         // more than kMaxConditionFolderNameUnits UTF-16 units after the strip
        DuplicateSibling // another folder with the same effective parent has the same name (strip + lower-case): the Windows warning
    };

    struct FolderNameCheck
    {
        FolderNameIssue issue = FolderNameIssue::None;
        std::string name; // py_strip(typed) for EVERY issue ("" for Blank): what would be written
        DialogText dialog; // DuplicateSibling: {"Duplicate Folder", "Folder " + name + " already exists."} (Windows, verbatim);
                           // TooLong: {"Invalid Folder Name", "Folder name must not be longer than 50 characters."} (iOS text);
                           // every other issue: empty (Blank is silent; InvalidText has no dialog of its own)
    };

    // The check of a folder name for a NEW folder (`exclude_uid` = "") or a renamed one (`exclude_uid` = the folder itself, which is not
    // its own sibling). `parent_uid` is the parent the folder will have ("" = the root; a uid that is not in `folders` counts as the root,
    // like the tree). Order: Blank, InvalidText, TooLong, DuplicateSibling. Siblings = the folders with the same effective parent;
    // equal when py_lower(py_strip(sibling.name)) == py_lower(name) (conditions_sidebar.py:1493-1497: `.strip().lower()`).
    FolderNameCheck check_condition_folder_name(const std::unordered_map<std::string, ConditionFolder> &folders,
                                                const std::string &parent_uid,
                                                const std::string &exclude_uid,
                                                const std::string &typed);

    // conditions_sidebar.py:1455-1481 for the folder `folder_uid`, whose stored name is the one the editor started with:
    //   blank -> Refused (check.issue Blank, silent); stripped text == the stored name EXACTLY -> Unchanged (no write, no dialog; also when
    //   the stored name is longer than the limit); otherwise the check above (TooLong / InvalidText / DuplicateSibling -> Refused with its
    //   dialog; a case-only change is allowed because the folder is excluded from its own siblings) and Write.
    // `new_name` and `check` (optional) are ALWAYS written, never left as they were: new_name = py_strip(typed) (so "" for Blank), check = the
    // details (for Unchanged and Write: issue None, name = py_strip(typed), empty dialog). A folder that is not in `folders` is Refused
    // silently: new_name "" and a default FolderNameCheck (issue None, empty name, empty dialog).
    FolderRenameDecision decide_condition_folder_rename(const std::unordered_map<std::string, ConditionFolder> &folders,
                                                        const std::string &folder_uid,
                                                        const std::string &typed,
                                                        std::string &new_name,
                                                        FolderNameCheck *check = nullptr);

    // conditions_sidebar.py:1499-1513 _resolve_folder_context_uid + :989-1001 _request_create_in_context (the parent of a new folder or of a
    // new condition is the nearest FOLDER ancestor-or-self of the CLICKED item) is ONE rule, ost_domain::folder_context_uid(nodes, index) of
    // ConditionCreate.hpp, over the pre-order nodes of build_condition_tree: the Conditions list asks it for every section of the tree it
    // shows. (A second overload over a folder map and a folder uid used to be here; it was the same rule for a caller that had already found
    // the folder, no screen called it, and it was removed in the UI step of S1. Its test now asks the rule of the tree: test_condition_folder_rules.cpp.)

    // ---- delete ---------------------------------------------------------------------------------------------------------------

    struct ConditionFolderDeletePlan
    {
        std::vector<std::string> requested; // as given (duplicates kept)
        std::vector<std::string> blocked;   // _condition_folder_uids_in_use: the requested uids that are blocked, request order, a duplicated
                                            // request uid appears as often as it was requested (Windows `[uid for uid in folder_uids if uid in blocked]`)
        std::vector<std::string> deletable; // requested minus blocked, request order (Windows `deletable_uids`)
        bool unknown_folder = false;        // some requested uid is not a folder of `folders`: the handler does nothing (:1526-1530, :167-177)
        bool subfolders_move = false;       // a folder that stays (not requested-and-deletable) has a deletable folder as its effective
                                            // parent: it moves to the top level (the iOS line of the confirmation)
    };

    // project_write_service._condition_folder_uids_in_use (:936-953) plus the handler's unknown-uid rule. `folders` and `conditions` are the
    // folders and conditions of ONE bid. A folder is blocked when it is the folder of a condition or an ANCESTOR of such a folder (the walk
    // follows ParentUID from the condition's folder to the root, every REQUESTED uid on the way is blocked); a condition with no folder or
    // with a folder uid that is not in `folders` blocks nothing; descendants of a condition's folder are not blocked. Like Windows, the walk
    // tests the request BEFORE it looks the parent up, so a requested uid that is no folder but the dangling ParentUID (or the text "0") of
    // a folder on the way is blocked too (`unknown_folder` is true then: the screens do nothing with such a request). The walk never loops
    // on a cycle (visited guard) and is iterative (a chain of 8000 folders is read by Windows).
    ConditionFolderDeletePlan plan_condition_folder_delete(const std::unordered_map<std::string, ConditionFolder> &folders,
                                                           const std::unordered_map<std::string, Condition> &conditions,
                                                           const std::vector<std::string> &requested);

    struct FolderDeleteItem
    {
        std::string name;
        std::string uid;
    };

    enum class FolderDeleteDialogKind
    {
        Ask, // a Yes / No question, default No (Windows QMessageBox.question): "Yes" deletes `deletable`
        Warn // an OK-only warning (Windows QMessageBox.warning): every folder is blocked, nothing is deleted (`deletable` is empty)
    };

    struct FolderDeleteConfirmation
    {
        FolderDeleteDialogKind kind = FolderDeleteDialogKind::Ask;
        DialogText dialog;
        std::vector<std::string> deletable; // the uids "Yes" deletes (items whose uid is not blocked, item order)
    };

    // messagebox.confirm_multi_delete(parent, "Delete Folder", items, used_uids), texts verbatim (title kConditionFolderDeleteTitle):
    //   none blocked:            Ask,  "Delete 1 item?" / "Delete {n} items?"  (n = number of items; singular only for 1, 0 is plural)
    //   some blocked, some not:  Ask,  one line `Cannot delete "{name}" because it is in use.` per blocked item, then a blank line, then
    //                                  "Delete the other 1 item?" / "Delete the other {n} items?"  (n = number deletable)
    //   all blocked:             Warn, the lines only
    // `blocked_uids` are the uids of items that are in use. `subfolders_move` (the plan's `subfolders_move`) appends, for an Ask only, an empty
    // line and kConditionFolderSubfoldersMoveText; it changes nothing else. (An empty `items` gives "Delete 0 items?" like Windows; the
    // screens never ask: an empty request returns before.)
    FolderDeleteConfirmation condition_folder_delete_confirmation(const std::vector<FolderDeleteItem> &items,
                                                                  const std::vector<std::string> &blocked_uids,
                                                                  bool subfolders_move);
}
