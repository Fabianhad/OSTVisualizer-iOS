// Layer: Domain. The contract of every function is in ConditionFolderUi.hpp; this file is only the bodies.
#include "ConditionFolderUi.hpp"
#include "ConditionTree.hpp"

#include <algorithm>

namespace ost_domain
{
    std::vector<ConditionFolderRow> condition_folder_rows(const std::unordered_map<std::string, ConditionFolder> &folders,
                                                          const std::unordered_map<std::string, Condition> &conditions,
                                                          const std::unordered_map<std::string, CdnType> &cdn_types)
    {
        // The direct counts: a condition counts in its folder when that is a folder of the bid (build_condition_tree seats a condition with any
        // other folder uid at the top level, so it counts nowhere).
        std::unordered_map<std::string, int> direct;
        for (const auto &entry : conditions)
        {
            const std::string &folder_uid = entry.second.condition_folder_uid;
            if (!folder_uid.empty() && folders.count(folder_uid) > 0)
                ++direct[folder_uid];
        }

        // The order and the depth are those of the Conditions tree: one source, so the list and the manager cannot disagree.
        std::vector<ConditionFolderRow> rows;
        rows.reserve(folders.size());
        for (const ConditionTreeNode &node : build_condition_tree(conditions, folders, cdn_types))
        {
            if (node.kind != ConditionTreeNode::Kind::Folder)
                continue;
            ConditionFolderRow row;
            row.uid = node.uid;
            row.name = node.name;
            row.depth = node.depth;
            const auto found = direct.find(node.uid);
            row.condition_count = found == direct.end() ? 0 : found->second;
            rows.push_back(std::move(row));
        }
        return rows;
    }

    std::string condition_listed_folder_uid(const std::unordered_map<std::string, ConditionFolder> &folders, const Condition &condition)
    {
        const std::string &folder_uid = condition.condition_folder_uid;
        return !folder_uid.empty() && folders.count(folder_uid) > 0 ? folder_uid : std::string();
    }

    std::string condition_folder_count_text(int count)
    {
        if (count <= 0)
            return "No conditions";
        if (count == 1)
            return "1 condition";
        return std::to_string(count) + " conditions";
    }

    std::string new_condition_action_title(const std::string &folder_uid, const std::string &folder_name)
    {
        if (folder_uid.empty())
            return "New Condition";
        return "New Condition in " + (folder_name.empty() ? std::string("Folder") : folder_name);
    }

    std::string next_condition_number_text(const std::unordered_map<std::string, Condition> &conditions)
    {
        long long highest = 0; // a bid without numbers starts at 1 (NULL and 0 do not count, like MAX over the numbers that exist)
        for (const auto &entry : conditions)
            highest = std::max(highest, static_cast<long long>(entry.second.ref_no));
        return "Next number: " + std::to_string(highest + 1);
    }

    std::optional<DialogText> condition_folder_name_refusal_dialog(const FolderNameCheck &check)
    {
        switch (check.issue)
        {
        case FolderNameIssue::None:
        case FolderNameIssue::Blank:
            return std::nullopt;
        case FolderNameIssue::InvalidText:
            return DialogText{kInvalidFolderNameTitle, kInvalidFolderNameText};
        case FolderNameIssue::TooLong:
        case FolderNameIssue::DuplicateSibling:
            return check.dialog;
        }
        return std::nullopt;
    }
}
