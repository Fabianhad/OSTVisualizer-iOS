// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_domain
{

    struct Layer
    {
        std::string uid;
        std::string name;
        bool visible = true;
        int sequence = 0;
    };

    // Windows domain/entities/layer.py BidLayer: one BidLayers row as the layer list shows it, either a layer of the bid
    // or a locked template layer (IsTemplate and IsLocked). bid_uid is "" for a layer that belongs to no bid.
    struct BidLayer
    {
        std::string uid;
        std::string bid_uid;
        std::string name;
        bool show = true;
        int sequence = 0;
        bool is_template = false;
        bool is_locked = false;
    };

    // Windows merge_layers_for_bid: the layers of the layer list. Template layers come first, then by Sequence (stable); a
    // layer replaces an earlier one of the same lower-cased name (so the bid's own layer wins over a template layer, and of
    // two bid layers with one name the later Sequence wins) but keeps the earlier one's place; the result is sorted by
    // Sequence only, stable, so equal sequences keep the order of that first appearance (the order of `layers`).
    std::vector<BidLayer> merge_layers_for_bid(const std::vector<BidLayer> &layers);

    // Windows get_layer_uid_by_name(layers, IMAGE_LAYER_NAME) + is_layer_visible: the page background (the PDF) is drawn
    // when the layer named "image" (name trimmed and lower-cased) is shown; a bid without such a layer shows it. Windows
    // takes the first layer of that name in database order, which a map does not keep: of several layers with that name the
    // one with the smallest sequence (then the smallest uid) decides, the same on every run (a bid has exactly one Image
    // layer).
    bool is_image_layer_visible(const std::unordered_map<std::string, Layer> &layers);

    // Windows normalize_layer_name: str(name or "").strip().lower() (domain/entities/layer.py:9-10).
    std::string normalize_layer_name(const std::string &name);

    // Windows is_comments_layer_name (layer.py:13-14): the layer named "comments", ignoring case and surrounding whitespace.
    // The Windows layer list (project_read_service.get_merged_bid_layers, project_data_service.get_bid_layer_snapshot) never
    // shows it and Show All / Hide All never touch it.
    bool is_comments_layer_name(const std::string &name);

    // The layers a Show All (show = true) or Hide All (show = false) writes (Windows update_all_layers_visibility_deferred):
    // the layers of the layer list `listed` (merge_layers_for_bid without the Comments layer) that are layers of the bid
    // (`bid_layers`, by uid, with their current visibility) and do not have the requested visibility yet, in list order. A
    // layer that is only listed (a locked template layer of no bid) is not one of the bid's layers and is left alone.
    std::vector<std::string> layers_to_change_for_bulk_visibility(const std::vector<BidLayer> &listed,
                                                                  const std::unordered_map<std::string, Layer> &bid_layers,
                                                                  bool show);

}
