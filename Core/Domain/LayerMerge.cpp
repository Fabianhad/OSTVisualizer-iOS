// Layer: Domain. merge_layers_for_bid (see Layer.hpp).
// Windows source of truth: ost_visualizer/domain/entities/layer.py merge_layers_for_bid
//     merged = {}
//     for layer in sorted(layers, key=lambda l: (not l.is_template, l.sequence)):
//         merged[layer.name.lower()] = layer
//     return sorted(merged.values(), key=lambda l: l.sequence)
// Python semantics that matter here: sorted() is stable; assigning to an existing key of a dict replaces the value but keeps the
// key's place (the order of first insertion); str.lower() is the full Unicode lower-casing (ost_import::text_rules::py_lower).
#include "Layer.hpp"

#include "../Import/TextRules.hpp"

#include <algorithm>
#include <unordered_map>

namespace ost_domain
{

    std::vector<BidLayer> merge_layers_for_bid(const std::vector<BidLayer> &layers)
    {
        // sorted(layers, key=(not is_template, sequence)): template layers first, then by sequence, ties in input order
        std::vector<const BidLayer *> ordered;
        ordered.reserve(layers.size());
        for (const BidLayer &layer : layers)
            ordered.push_back(&layer);
        std::stable_sort(ordered.begin(), ordered.end(),
                         [](const BidLayer *a, const BidLayer *b)
                         {
                             if (a->is_template != b->is_template)
                                 return a->is_template; // key False (a template layer) sorts before True
                             return a->sequence < b->sequence;
                         });

        // merged[name.lower()] = layer: the last layer of a name is the value, the first one decides the place
        std::vector<const BidLayer *> merged;
        std::unordered_map<std::string, std::size_t> place_of;
        for (const BidLayer *layer : ordered)
        {
            const std::string key = ost_import::text_rules::py_lower(layer->name);
            auto found = place_of.find(key);
            if (found == place_of.end())
            {
                place_of.emplace(key, merged.size());
                merged.push_back(layer);
            }
            else
                merged[found->second] = layer;
        }

        // sorted(merged.values(), key=sequence)
        std::stable_sort(merged.begin(), merged.end(),
                         [](const BidLayer *a, const BidLayer *b) { return a->sequence < b->sequence; });

        std::vector<BidLayer> result;
        result.reserve(merged.size());
        for (const BidLayer *layer : merged)
            result.push_back(*layer);
        return result;
    }

    bool is_image_layer_visible(const std::unordered_map<std::string, Layer> &layers)
    {
        const std::pair<const std::string, Layer> *image = nullptr;
        for (const auto &entry : layers)
        {
            if (normalize_layer_name(entry.second.name) != "image")
                continue;
            if (image == nullptr || entry.second.sequence < image->second.sequence ||
                (entry.second.sequence == image->second.sequence && entry.first < image->first))
                image = &entry;
        }
        return image == nullptr ? true : image->second.visible;
    }

    std::string normalize_layer_name(const std::string &name)
    {
        return ost_import::text_rules::py_lower(ost_import::text_rules::py_strip(name));
    }

    bool is_image_layer_uid(const std::vector<BidLayer> &merged_layers, const std::unordered_map<std::string, Layer> &own_layers,
                            const std::string &uid)
    {
        // the merged list first (Windows' name map is built from it), and the name it holds for the uid is the one that counts
        for (const BidLayer &entry : merged_layers)
        {
            if (entry.uid == uid)
                return normalize_layer_name(entry.name) == "image";
        }
        auto found = own_layers.find(uid);
        if (found != own_layers.end())
            return normalize_layer_name(found->second.name) == "image";
        return false;
    }

    bool is_comments_layer_name(const std::string &name)
    {
        return normalize_layer_name(name) == "comments";
    }

    bool is_shared_template_layer(const BidLayer &layer, const std::string &bid_uid)
    {
        // bool(layer.is_template and layer.is_locked and str(layer.bid_uid or "") != str(bid_uid or "")); the loader already
        // reads a NULL or 0 BidUID as "", so the comparison is a plain text comparison
        return layer.is_template && layer.is_locked && layer.bid_uid != bid_uid;
    }

    BulkLayerPlan plan_bulk_layer_visibility(const std::vector<BidLayer> &listed, const std::string &bid_uid,
                                             const std::unordered_map<std::string, Layer> &bid_layers, bool show)
    {
        // Windows update_all_layers_visibility_deferred: previous_visibility = {uid: show for the layers of the list that are not
        // shared}; empty -> return False (nothing queued); changed_layer_uids = [uid for uid, visible in previous_visibility.items()
        // if visible != bool(show)]; none -> return True without a write. Only a layer of the bid has a live visibility here (a
        // template-only entry has none), and the Comments layer is never part of the list (it is dropped from it, LP-18).
        BulkLayerPlan plan;
        bool any_own_layer = false;
        for (const BidLayer &entry : listed)
        {
            if (is_shared_template_layer(entry, bid_uid) || is_comments_layer_name(entry.name))
                continue;
            auto own = bid_layers.find(entry.uid);
            if (own == bid_layers.end() || is_comments_layer_name(own->second.name))
                continue;
            any_own_layer = true;
            if (own->second.visible != show)
                plan.uids.push_back(entry.uid);
        }
        if (!plan.uids.empty())
            plan.outcome = BulkLayerOutcome::Write;
        else
            plan.outcome = any_own_layer ? BulkLayerOutcome::AlreadyThere : BulkLayerOutcome::NoOwnLayers;
        return plan;
    }

}
