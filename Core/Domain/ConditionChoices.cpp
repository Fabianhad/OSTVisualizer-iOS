// Layer: Domain. See ConditionChoices.hpp.
#include "ConditionChoices.hpp"

#include "../Import/TextRules.hpp"
#include "ConditionTree.hpp"
#include "UomService.hpp"

#include <algorithm>
#include <string>

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

    namespace
    {
        // edit_condition_dialog.py _STYLE_ITEMS (:64-69): the four styles in the order of the style combo
        struct StyleName
        {
            ConditionType type;
            const char *label;
        };
        const StyleName kStyleNames[] = {{ConditionType::Linear, "Linear"},
                                         {ConditionType::Area, "Area"},
                                         {ConditionType::Count, "Count"},
                                         {ConditionType::Attachment, "Attachment"}};
    }

    std::optional<ConditionType> unknown_style_type(ConditionType stored)
    {
        // _KNOWN_STYLE_TYPES (:70) = the four types of _STYLE_ITEMS = the types the quantity calculation supports
        if (is_supported_condition_type(stored))
            return std::nullopt;
        return stored;
    }

    std::vector<StyleChoice> condition_style_choices(const std::optional<ConditionType> &unknown)
    {
        std::vector<StyleChoice> choices;
        choices.reserve(5);
        for (const StyleName &style : kStyleNames)
            choices.push_back(StyleChoice{style.type, style.label, true});
        // _sync_unknown_style_item (:970-978): the entry of the stored type is added after Attachment and disabled
        if (unknown.has_value())
            choices.push_back(StyleChoice{*unknown, condition_style_label(*unknown), false});
        return choices;
    }

    std::string condition_style_label(ConditionType type)
    {
        for (const StyleName &style : kStyleNames)
            if (style.type == type)
                return style.label;
        // f"Unknown ({condition_type})" (:977): the raw integer, so "Unknown (-1)"
        return "Unknown (" + std::to_string(static_cast<int>(type)) + ")";
    }

    EditorLayout editor_layout(ConditionType type)
    {
        switch (type)
        {
        case ConditionType::Linear:
            return EditorLayout::Linear;
        case ConditionType::Count:
        case ConditionType::Attachment:
            return EditorLayout::CountLike;
        case ConditionType::Area:
            break;
        }
        // Area and every other type: `is_area = not is_count and not is_linear` (:547), the Area branch of _build_advanced_tab (:740)
        return EditorLayout::Area;
    }

}
