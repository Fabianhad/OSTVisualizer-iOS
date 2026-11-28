// Layer: Domain
// Pure C++. See LayerChangeImpact.hpp. Port of ost_visualizer/application/layer_change_impact.py:12-34 (Windows 49a2d59).
#include "LayerChangeImpact.hpp"
#include <string>
#include <unordered_map>

namespace ost_domain
{
    namespace
    {
        // layer.py:4-6 ANNOTATION_LAYER_NAME / IMAGE_LAYER_NAME / COMMENTS_LAYER_NAME, compared after normalize_layer_name (strip().lower()).
        bool is_role_name(const std::string &name)
        {
            const std::string normalized = normalize_layer_name(name);
            return normalized == "annotation" || normalized == "comments" || normalized == "image";
        }

        // dataclasses.replace(old, name=new.name) == new: every field but the name is equal (layer_change_impact.py:27).
        bool equal_but_name(const BidLayer &a, const BidLayer &b)
        {
            return a.uid == b.uid && a.bid_uid == b.bid_uid && a.show == b.show && a.sequence == b.sequence &&
                   a.is_template == b.is_template && a.is_locked == b.is_locked;
        }

        // {layer.uid: layer for layer in layers}: a repeated uid keeps the last row (the caller compares the sizes to catch it).
        std::unordered_map<std::string, const BidLayer *> by_uid(const std::vector<BidLayer> &layers)
        {
            std::unordered_map<std::string, const BidLayer *> out;
            for (const BidLayer &layer : layers)
                out[layer.uid] = &layer;
            return out;
        }
    }

    bool layer_rename_preserves_rendering(const std::vector<BidLayer> &before, const std::vector<BidLayer> &after)
    {
        const std::unordered_map<std::string, const BidLayer *> previous = by_uid(before);
        const std::unordered_map<std::string, const BidLayer *> current = by_uid(after);
        // :20-21 `if not previous or previous.keys() != current.keys()`
        if (previous.empty() || previous.size() != current.size())
            return false;
        for (const auto &entry : previous)
            if (current.find(entry.first) == current.end())
                return false;
        // :22-23 a uid that occurs twice in either list
        if (previous.size() != before.size() || current.size() != after.size())
            return false;
        bool renamed = false;
        for (const auto &entry : previous)
        {
            const BidLayer &old_layer = *entry.second;
            const BidLayer &new_layer = *current.at(entry.first);
            // :27-28 anything but the name changed
            if (!equal_but_name(old_layer, new_layer))
                return false;
            // :29-35 the raw names decide "renamed"; a role name on either side means the drawing may change
            if (old_layer.name != new_layer.name)
            {
                renamed = true;
                if (is_role_name(old_layer.name) || is_role_name(new_layer.name))
                    return false;
            }
        }
        return renamed;
    }

}
