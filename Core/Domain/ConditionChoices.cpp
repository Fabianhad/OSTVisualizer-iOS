// Layer: Domain. See ConditionChoices.hpp.
#include "ConditionChoices.hpp"

#include "../Import/TextRules.hpp"
#include "ConditionTree.hpp"

#include <algorithm>

namespace ost_domain
{

    std::vector<std::string> sorted_cdn_type_uids(const std::unordered_map<std::string, CdnType> &types)
    {
        // condition_types_dialog.py: sorted(types, key=lambda t: t.name.lower()). sorted() is stable, so types whose lower-cased
        // names are equal stay in the order they were read (ascending uid); the key is compared by code point, which is the
        // byte order of the UTF-8 text.
        struct Entry
        {
            std::string uid;
            std::string key;
        };
        std::vector<Entry> entries;
        entries.reserve(types.size());
        for (const auto &pair : types)
            entries.push_back(Entry{pair.first, ost_import::text_rules::py_lower(pair.second.name)});
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return uid_load_order_less(a.uid, b.uid); });
        std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.key < b.key; });

        std::vector<std::string> uids;
        uids.reserve(entries.size());
        for (const Entry &entry : entries)
            uids.push_back(entry.uid);
        return uids;
    }

    std::vector<std::string> condition_layer_choices(const std::vector<BidLayer> &merged_layers,
                                                     const std::unordered_map<std::string, Layer> &bid_layers)
    {
        // The Windows layer list is project_read_service.get_merged_bid_layers (merge_layers_for_bid) without the Comments layer.
        std::vector<std::string> uids;
        for (const BidLayer &entry : merged_layers)
        {
            if (is_comments_layer_name(entry.name))
                continue;
            if (bid_layers.find(entry.uid) == bid_layers.end())
                continue;
            uids.push_back(entry.uid);
        }
        return uids;
    }

    const std::vector<ShapeChoice> &condition_shape_choices()
    {
        // edit_condition_dialog.py _SHAPE_ITEMS
        static const std::vector<ShapeChoice> choices = {
            {ShapeId::Square, "Square"},   {ShapeId::Circle, "Circle"},   {ShapeId::Triangle, "Triangle"},
            {ShapeId::Rectangle, "Rectangle"}, {ShapeId::Ellipse, "Ellipse"}, {ShapeId::Rhombus, "Diamond"},
            {ShapeId::Hexagon, "Hexagon"}, {ShapeId::Octagon, "Octagon"}};
        return choices;
    }

    std::vector<std::string> condition_uids_in_list_order(const std::unordered_map<std::string, Condition> &conditions,
                                                          const std::unordered_map<std::string, ConditionFolder> &folders,
                                                          const std::unordered_map<std::string, CdnType> &cdn_types)
    {
        std::vector<std::string> uids;
        uids.reserve(conditions.size());
        for (const ConditionTreeNode &node : build_condition_tree(conditions, folders, cdn_types))
            if (node.kind == ConditionTreeNode::Kind::Condition)
                uids.push_back(node.uid);
        return uids;
    }

    ConditionType editor_condition_type(ConditionType stored)
    {
        switch (stored)
        {
        case ConditionType::Linear:
        case ConditionType::Area:
        case ConditionType::Count:
        case ConditionType::Attachment:
            return stored;
        }
        return ConditionType::Linear;
    }

}
