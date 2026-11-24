// The Conditions list as a tree, the rules of the Windows sidebar (conditions_sidebar.py _rebuild_tree /
// _build_folder_tree / _build_cdn_type_groups, default group-by-type, default sort "No." ascending):
//   - folders nest through ParentUID; a parent that is not one of the folders makes a root folder;
//   - a condition without a folder (or with an unknown folder uid) sits at the root, next to the root folders;
//   - empty folders are part of the tree;
//   - inside a folder, and at the root, the conditions form one group per condition type: titled "(unassigned)" without
//     a type, the CdnTypes name with one, "Unknown" when the type has no CdnTypes row;
//   - the children of a node (sub-folders and type groups together) sort by name with the Qt tree's stable sort on the
//     title (code point order, i.e. UTF-8 byte order); on equal titles the earlier child stays first: folders were added
//     before the groups, and equal folders (or groups) keep database read order;
//   - the conditions of a group sort by reference number; equal numbers keep database read order.
// iOS keeps the conditions in unordered containers, so "database read order" is the order of the ascending (numeric)
// UID, which is the rowid order SELECT ... FROM BidConditions / BidConditionFolders returns.
#include "ConditionTree.hpp"
#include <algorithm>
#include <deque>

namespace ost_domain
{

    // Database read order of two uids: decimal uids numerically (without overflow), then by text; uids that are not
    // decimal come after the decimal ones, by text.
    bool uid_load_order_less(const std::string &a, const std::string &b)
    {
        auto is_decimal = [](const std::string &uid)
        {
            if (uid.empty())
                return false;
            for (const char c : uid)
                if (c < '0' || c > '9')
                    return false;
            return true;
        };
        const bool a_decimal = is_decimal(a);
        const bool b_decimal = is_decimal(b);
        if (a_decimal != b_decimal)
            return a_decimal;
        if (a_decimal)
        {
            const std::size_t a_start = std::min(a.find_first_not_of('0'), a.size());
            const std::size_t b_start = std::min(b.find_first_not_of('0'), b.size());
            const std::size_t a_len = a.size() - a_start;
            const std::size_t b_len = b.size() - b_start;
            if (a_len != b_len)
                return a_len < b_len;
            const int digits = a.compare(a_start, a_len, b, b_start, b_len);
            if (digits != 0)
                return digits < 0;
        }
        return a < b;
    }

    namespace
    {
        struct Group
        {
            std::string uid;
            std::string name;
            std::vector<const Condition *> conditions;
        };

        struct Child
        {
            bool is_folder;
            const ConditionFolder *folder; // is_folder
            const Group *group;            // !is_folder
            const std::string *name;
        };

        class TreeBuilder
        {
        public:
            TreeBuilder(const std::unordered_map<std::string, Condition> &conditions,
                        const std::unordered_map<std::string, ConditionFolder> &folders,
                        const std::unordered_map<std::string, CdnType> &cdn_types)
                : _cdn_types(cdn_types)
            {
                std::vector<const Condition *> ordered;
                ordered.reserve(conditions.size());
                for (const auto &pair : conditions)
                    ordered.push_back(&pair.second);
                std::sort(ordered.begin(), ordered.end(), [](const Condition *a, const Condition *b)
                          { return uid_load_order_less(a->uid, b->uid); });
                for (const Condition *condition : ordered)
                {
                    const std::string &folder_uid = condition->condition_folder_uid;
                    const bool known = !folder_uid.empty() && folders.count(folder_uid) > 0;
                    _conditions_by_folder[known ? folder_uid : std::string()].push_back(condition);
                }

                std::vector<const ConditionFolder *> ordered_folders;
                ordered_folders.reserve(folders.size());
                for (const auto &pair : folders)
                    ordered_folders.push_back(&pair.second);
                std::sort(ordered_folders.begin(), ordered_folders.end(),
                          [](const ConditionFolder *a, const ConditionFolder *b)
                          { return uid_load_order_less(a->uid, b->uid); });
                for (const ConditionFolder *folder : ordered_folders)
                {
                    const bool known_parent = !folder->parent_uid.empty() && folders.count(folder->parent_uid) > 0;
                    _children_by_parent[known_parent ? folder->parent_uid : std::string()].push_back(folder);
                }
            }

            // Pre-order, depth first. The walk keeps its own stack of frames on the heap instead of recursing (plan 5.2, register
            // CF-10): Windows reads a chain of 8000 nested folders, and one native frame per level would overflow a 1 MB main-thread stack.
            std::vector<ConditionTreeNode> build()
            {
                std::vector<ConditionTreeNode> nodes;
                std::deque<Frame> stack; // a deque: a frame never moves, so the pointers of its children into its own groups stay valid
                open_frame(stack, std::string(), 0);
                while (!stack.empty())
                {
                    Frame &frame = stack.back();
                    if (frame.next == frame.children.size())
                    {
                        stack.pop_back();
                        continue;
                    }
                    const Child child = frame.children[frame.next++];
                    const int depth = frame.depth;

                    ConditionTreeNode node;
                    node.depth = depth;
                    if (child.is_folder)
                    {
                        node.kind = ConditionTreeNode::Kind::Folder;
                        node.uid = child.folder->uid;
                        node.name = child.folder->name;
                        nodes.push_back(node);
                        open_frame(stack, child.folder->uid, depth + 1); // its children come before the next sibling
                        continue;
                    }
                    node.kind = ConditionTreeNode::Kind::TypeGroup;
                    node.uid = child.group->uid;
                    node.name = child.group->name;
                    nodes.push_back(node);

                    std::vector<const Condition *> rows = child.group->conditions;
                    std::stable_sort(rows.begin(), rows.end(),
                                     [](const Condition *a, const Condition *b) { return a->ref_no < b->ref_no; });
                    for (const Condition *row : rows)
                    {
                        ConditionTreeNode leaf;
                        leaf.kind = ConditionTreeNode::Kind::Condition;
                        leaf.depth = depth + 1;
                        leaf.uid = row->uid;
                        leaf.name = row->name;
                        nodes.push_back(leaf);
                    }
                }
                return nodes;
            }

        private:
            // The children of one parent still to be emitted: `children` points into `groups` (and into the folders), `next` is the
            // index of the child to emit next.
            struct Frame
            {
                int depth = 0;
                std::vector<Group> groups;
                std::vector<Child> children;
                std::size_t next = 0;
            };

            // _build_cdn_type_groups: one group per CdnTypeUID in order of first appearance; the title of a group is
            // "(unassigned)" without a type, the type's name, or "Unknown" for a type uid with no CdnTypes row.
            std::vector<Group> make_groups(const std::vector<const Condition *> &conditions) const
            {
                std::vector<Group> groups;
                std::unordered_map<std::string, std::size_t> index_by_uid;
                for (const Condition *condition : conditions)
                {
                    auto found = index_by_uid.find(condition->cdn_type_uid);
                    if (found == index_by_uid.end())
                    {
                        Group group;
                        group.uid = condition->cdn_type_uid;
                        if (group.uid.empty())
                        {
                            group.name = condition_group_unassigned_title();
                        }
                        else
                        {
                            auto type = _cdn_types.find(group.uid);
                            group.name = type != _cdn_types.end() ? type->second.name : condition_group_unknown_title();
                        }
                        index_by_uid[group.uid] = groups.size();
                        groups.push_back(std::move(group));
                        found = index_by_uid.find(condition->cdn_type_uid);
                    }
                    groups[found->second].conditions.push_back(condition);
                }
                return groups;
            }

            // The children of one parent (the root when parent_uid is empty): sub-folders first, then the type groups of
            // the conditions placed directly here, stable-sorted by title.
            void open_frame(std::deque<Frame> &stack, const std::string &parent_uid, int depth)
            {
                stack.emplace_back();
                Frame &frame = stack.back();
                frame.depth = depth;
                auto placed = _conditions_by_folder.find(parent_uid);
                if (placed != _conditions_by_folder.end())
                    frame.groups = make_groups(placed->second);

                auto sub_folders = _children_by_parent.find(parent_uid);
                if (sub_folders != _children_by_parent.end())
                    for (const ConditionFolder *folder : sub_folders->second)
                        frame.children.push_back(Child{true, folder, nullptr, &folder->name});
                for (const Group &group : frame.groups)
                    frame.children.push_back(Child{false, nullptr, &group, &group.name});
                std::stable_sort(frame.children.begin(), frame.children.end(),
                                 [](const Child &a, const Child &b) { return *a.name < *b.name; });
            }

            const std::unordered_map<std::string, CdnType> &_cdn_types;
            // "" = the root
            std::unordered_map<std::string, std::vector<const Condition *>> _conditions_by_folder;
            std::unordered_map<std::string, std::vector<const ConditionFolder *>> _children_by_parent;
        };
    }

    std::vector<ConditionTreeNode> build_condition_tree(
        const std::unordered_map<std::string, Condition> &conditions,
        const std::unordered_map<std::string, ConditionFolder> &folders,
        const std::unordered_map<std::string, CdnType> &cdn_types)
    {
        return TreeBuilder(conditions, folders, cdn_types).build();
    }

}
