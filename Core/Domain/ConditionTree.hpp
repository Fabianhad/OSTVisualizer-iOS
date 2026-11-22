// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The Conditions list as a tree: folders (nested through ParentUID), the condition-type groups inside a folder or at
// the root, and the conditions of each group. Windows counterpart: ConditionsSidebar._rebuild_tree /
// _build_folder_tree / _build_cdn_type_groups with the default "group by type" and the default sort (column "No."
// ascending), ost_visualizer/presentation/components/conditions_sidebar.py.
#pragma once
#include "CdnType.hpp"
#include "Condition.hpp"
#include "ConditionFolder.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_domain
{

    // One node of the tree in display (pre-order) order. A node's children are the following nodes with depth + 1.
    struct ConditionTreeNode
    {
        enum class Kind
        {
            Folder,    // uid = folder uid, name = folder name
            TypeGroup, // uid = CdnTypeUID ("" = the "(unassigned)" group), name = the group title
            Condition  // uid = condition uid, name = condition name
        };

        Kind kind = Kind::Condition;
        int depth = 0; // 0 = directly under the root
        std::string uid;
        std::string name;

        bool operator==(const ConditionTreeNode &other) const
        {
            return kind == other.kind && depth == other.depth && uid == other.uid && name == other.name;
        }
    };

    // Title of the group of conditions without a condition type.
    inline const char *condition_group_unassigned_title() { return "(unassigned)"; }
    // Title of the group of conditions whose CdnTypeUID has no CdnTypes row (Windows Condition.cdn_type_name default).
    inline const char *condition_group_unknown_title() { return "Unknown"; }

    // Database read order of two uids (SELECT ... FROM BidConditions / BidConditionFolders / CdnTypes, UID INTEGER PRIMARY KEY):
    // decimal uids numerically (without overflow), then by text; uids that are not decimal come after the decimal ones, by text.
    // The tie-break of every list that Windows sorts with a stable sort over the rows as read.
    bool uid_load_order_less(const std::string &a, const std::string &b);

    std::vector<ConditionTreeNode> build_condition_tree(
        const std::unordered_map<std::string, Condition> &conditions,
        const std::unordered_map<std::string, ConditionFolder> &folders,
        const std::unordered_map<std::string, CdnType> &cdn_types);

}
