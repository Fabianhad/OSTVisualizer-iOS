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

    // Windows ProjectDataService.is_image_layer_uid (project_data_service.py:639-641): the layer with that uid is the Image layer,
    // normalize_layer_name(its name) == "image". Windows reads the name from a map built from the layer list it was given (the
    // merged list of the layer sheet, :509-519; an empty name is not stored, so such a uid is no Image layer); here the merged
    // list is searched first and decides when it holds the uid, then the bid's own layers; an unknown uid is not the Image
    // layer. The single layer toggle uses it so that hiding or showing the Image layer leaves the placement tool alone
    // (COND-PLACE-01, ConditionChoices.hpp).
    bool is_image_layer_uid(const std::vector<BidLayer> &merged_layers, const std::unordered_map<std::string, Layer> &own_layers,
                            const std::string &uid);

    // Windows is_comments_layer_name (layer.py:13-14): the layer named "comments", ignoring case and surrounding whitespace.
    // The Windows layer list (project_read_service.get_merged_bid_layers, project_data_service.get_bid_layer_snapshot) never
    // shows it and Show All / Hide All never touch it.
    bool is_comments_layer_name(const std::string &name);

    // Windows is_shared_template_layer (layer.py:89-94): a row of the layer list that belongs to no bid or to another bid
    // (IsTemplate and IsLocked and the bid uid differs). The uids are compared as text, str(layer.bid_uid or "") !=
    // str(bid_uid or ""), so "" equals "" and "1" is not "01"; the bid's own locked copy of a template layer is NOT shared.
    bool is_shared_template_layer(const BidLayer &layer, const std::string &bid_uid);

    // What a Show All / Hide All does (Windows update_all_layers_visibility_deferred):
    //  - NoOwnLayers: the list has no layer of the bid (only shared rows, or nothing): Windows returns False before queuing
    //    anything, nothing is written and nothing changes;
    //  - AlreadyThere: the bid has layers but all already have the requested visibility: a success, nothing to write;
    //  - Write: the uids to write (and to change in memory), in list order.
    enum class BulkLayerOutcome
    {
        NoOwnLayers,
        AlreadyThere,
        Write
    };

    struct BulkLayerPlan
    {
        BulkLayerOutcome outcome = BulkLayerOutcome::NoOwnLayers;
        std::vector<std::string> uids; // empty unless outcome is Write
    };

    // The plan of a Show All (show = true) or Hide All (show = false) for the bid `bid_uid`. `listed` is the layer list
    // (merge_layers_for_bid without the Comments layer), `bid_layers` the bid's layers by uid with their current visibility.
    // A listed layer is one of the bid's own when it is not a shared row (is_shared_template_layer, so the bid's own locked
    // copy counts and another bid's or a bid-less locked row does not), is not the Comments layer and is among `bid_layers`
    // (a layer that is only listed has no live visibility here). Shared rows are never changed or counted: Windows captures
    // only the non-shared layers of the list, and update_all_layers_show writes only the rows with BidUID = bid.
    BulkLayerPlan plan_bulk_layer_visibility(const std::vector<BidLayer> &listed, const std::string &bid_uid,
                                             const std::unordered_map<std::string, Layer> &bid_layers, bool show);

}
