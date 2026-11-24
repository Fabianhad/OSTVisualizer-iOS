// Layer: Domain. The rules of the condition folders (plan 5.2). The contract of every function, and the Windows sources, are in
// ConditionFolderRules.hpp; this file is only the bodies.
#include "ConditionFolderRules.hpp"
#include "../Import/TextRules.hpp"
#include "TextLimits.hpp"

#include <optional>
#include <unordered_set>

namespace ost_domain
{
    namespace
    {
        // conditions_sidebar.py:469-475: the parent a folder is listed under, "" = the root (empty, "0" or not a folder of `folders`).
        std::string listed_parent(const std::unordered_map<std::string, ConditionFolder> &folders, const std::string &parent_uid)
        {
            if (parent_uid.empty() || parent_uid == "0" || folders.count(parent_uid) == 0)
                return std::string();
            return parent_uid;
        }
    }

    std::string effective_condition_folder_parent(const std::unordered_map<std::string, ConditionFolder> &folders, const ConditionFolder &folder)
    {
        return listed_parent(folders, folder.parent_uid);
    }

    FolderNameCheck check_condition_folder_name(const std::unordered_map<std::string, ConditionFolder> &folders,
                                                const std::string &parent_uid,
                                                const std::string &exclude_uid,
                                                const std::string &typed)
    {
        FolderNameCheck check;
        check.name = ost_import::text_rules::py_strip(typed); // conditions_sidebar.py:1462: new_name = editor.text().strip()
        if (check.name.empty())
        {
            check.issue = FolderNameIssue::Blank;
            return check;
        }
        // A NUL cuts a C string (sqlite3_bind_text of c_str()), invalid UTF-8 has no length: both are refused, never repaired.
        const std::optional<std::size_t> units = utf16_length(check.name);
        if (!units || contains_nul(check.name))
        {
            check.issue = FolderNameIssue::InvalidText;
            return check;
        }
        if (*units > kMaxConditionFolderNameUnits)
        {
            check.issue = FolderNameIssue::TooLong;
            check.dialog = DialogText{"Invalid Folder Name", "Folder name must not be longer than 50 characters."};
            return check;
        }

        // conditions_sidebar.py:1483-1497: the siblings are the other folders listed under the same parent; the comparison is
        // sibling.text().strip().lower() == new_name.lower().
        const std::string parent = listed_parent(folders, parent_uid);
        const std::string wanted = ost_import::text_rules::py_lower(check.name);
        for (const auto &entry : folders)
        {
            const ConditionFolder &sibling = entry.second;
            if (entry.first == exclude_uid || sibling.uid == exclude_uid)
                continue;
            if (listed_parent(folders, sibling.parent_uid) != parent)
                continue;
            if (ost_import::text_rules::py_lower(ost_import::text_rules::py_strip(sibling.name)) == wanted)
            {
                check.issue = FolderNameIssue::DuplicateSibling;
                check.dialog = DialogText{"Duplicate Folder", "Folder " + check.name + " already exists."};
                return check;
            }
        }
        return check;
    }

    FolderRenameDecision decide_condition_folder_rename(const std::unordered_map<std::string, ConditionFolder> &folders,
                                                        const std::string &folder_uid,
                                                        const std::string &typed,
                                                        std::string &new_name,
                                                        FolderNameCheck *check)
    {
        new_name.clear();
        if (check)
            *check = FolderNameCheck{};
        const auto found = folders.find(folder_uid);
        if (found == folders.end())
            return FolderRenameDecision::Refused;
        const ConditionFolder &folder = found->second;

        const std::string stripped = ost_import::text_rules::py_strip(typed);
        new_name = stripped;
        FolderNameCheck details;
        details.name = stripped;
        if (stripped.empty()) // :1466-1469: blank = revert, no write
        {
            details.issue = FolderNameIssue::Blank;
            if (check)
                *check = details;
            return FolderRenameDecision::Refused;
        }
        if (stripped == folder.name) // :1470-1471: the name the editor started with = no write (also a stored name over the limit)
        {
            if (check)
                *check = details;
            return FolderRenameDecision::Unchanged;
        }
        details = check_condition_folder_name(folders, listed_parent(folders, folder.parent_uid), folder_uid, typed);
        if (check)
            *check = details;
        return details.issue == FolderNameIssue::None ? FolderRenameDecision::Write : FolderRenameDecision::Refused;
    }

    std::string folder_context_uid(const std::unordered_map<std::string, ConditionFolder> &folders, const std::string &clicked_folder_uid)
    {
        return folders.count(clicked_folder_uid) > 0 ? clicked_folder_uid : std::string();
    }

    ConditionFolderDeletePlan plan_condition_folder_delete(const std::unordered_map<std::string, ConditionFolder> &folders,
                                                           const std::unordered_map<std::string, Condition> &conditions,
                                                           const std::vector<std::string> &requested)
    {
        ConditionFolderDeletePlan plan;
        plan.requested = requested;
        const std::unordered_set<std::string> selected(requested.begin(), requested.end());

        // project_write_service.py:936-953: every REQUESTED uid on the chain folder -> parent -> ... of a condition's folder is blocked. The
        // chain starts only at a folder of `folders` (:947), but it ends where the parent lookup finds nothing (:952): a dangling parent
        // uid, or the text "0", is tested against the request (:950) before the lookup fails, exactly like Windows.
        // A uid that an earlier walk already passed is not walked again: everything above it was marked then, so the work stays linear in
        // the number of folders (a chain of 8000 holding a condition in every folder would be quadratic otherwise) and a cycle ends where
        // it meets itself. The walk is a loop, not a recursion.
        std::unordered_set<std::string> blocked_set;
        std::unordered_set<std::string> passed;
        for (const auto &entry : conditions)
        {
            std::string current = entry.second.condition_folder_uid;
            if (current.empty() || folders.count(current) == 0)
                continue;
            while (!current.empty() && passed.insert(current).second)
            {
                if (selected.count(current) > 0)
                    blocked_set.insert(current);
                const auto folder = folders.find(current);
                if (folder == folders.end())
                    break;
                current = folder->second.parent_uid;
            }
        }

        std::unordered_set<std::string> deletable_set;
        for (const std::string &uid : requested)
        {
            if (blocked_set.count(uid) > 0)
                plan.blocked.push_back(uid);
            else
            {
                plan.deletable.push_back(uid);
                deletable_set.insert(uid);
            }
            if (folders.count(uid) == 0)
                plan.unknown_folder = true;
        }

        // A folder that stays, listed under a folder that goes, moves to the top level (condition_folder_operations.py:124-131).
        for (const auto &entry : folders)
        {
            const std::string parent = listed_parent(folders, entry.second.parent_uid);
            if (!parent.empty() && deletable_set.count(parent) > 0 && deletable_set.count(entry.first) == 0)
            {
                plan.subfolders_move = true;
                break;
            }
        }
        return plan;
    }

    FolderDeleteConfirmation condition_folder_delete_confirmation(const std::vector<FolderDeleteItem> &items,
                                                                  const std::vector<std::string> &blocked_uids,
                                                                  bool subfolders_move)
    {
        // messagebox.py:78-119 confirm_multi_delete.
        const std::unordered_set<std::string> used(blocked_uids.begin(), blocked_uids.end());
        FolderDeleteConfirmation confirmation;
        confirmation.dialog.title = kConditionFolderDeleteTitle;
        std::string in_use_lines;
        std::size_t in_use_count = 0;
        for (const FolderDeleteItem &item : items)
        {
            if (used.count(item.uid) > 0)
            {
                if (in_use_count++ > 0)
                    in_use_lines += "\n";
                in_use_lines += "Cannot delete \"" + item.name + "\" because it is in use.";
            }
            else
                confirmation.deletable.push_back(item.uid);
        }

        if (in_use_count > 0 && confirmation.deletable.empty())
        {
            confirmation.kind = FolderDeleteDialogKind::Warn; // every folder is in use: a warning, nothing to ask
            confirmation.dialog.message = in_use_lines;
            return confirmation;
        }
        if (in_use_count > 0)
            confirmation.dialog.message = in_use_lines + "\n\nDelete the other " + std::to_string(confirmation.deletable.size()) + " item(s)?";
        else
            confirmation.dialog.message = "Delete " + std::to_string(items.size()) + " item(s)?";
        if (subfolders_move)
            confirmation.dialog.message += std::string("\n\n") + kConditionFolderSubfoldersMoveText;
        return confirmation;
    }
}
