// Layer: Domain
// Pure C++. See LayerSheet.hpp.
#include "LayerSheet.hpp"
#include "LayerEdit.hpp"
#include <string>

namespace ost_domain
{

    LayerRowActions layer_row_actions(const std::vector<BidLayer> &listed, const std::string &bid_uid, const std::string &uid)
    {
        LayerRowActions actions;
        if (uid.empty())
            return actions;
        const BidLayer *row = nullptr;
        for (const BidLayer &entry : listed)
        {
            if (entry.uid == uid)
            {
                row = &entry;
                break;
            }
        }
        if (row == nullptr)
            return actions;
        actions.listed = true;
        // layers_dialog.py:315-322: New runs with any row selected, its sequence is the position.
        actions.can_add_below = true;
        // layers_dialog.py:285-291: editable = neither a template nor locked. The sheet lists only the bid's rows and the shared ones, so a row of
        // another bid cannot occur; it is refused here as plan_layer_move refuses it.
        const bool own = row->bid_uid == bid_uid;
        actions.can_rename = own && can_modify_bid_layer(*row);
        actions.can_delete = own && can_delete_bid_layer(*row);
        actions.can_move_up = plan_layer_move(listed, bid_uid, uid, -1).allowed;
        actions.can_move_down = plan_layer_move(listed, bid_uid, uid, 1).allowed;
        return actions;
    }

    DialogText layer_write_failure_text(LayerAction action, const std::string &store_error)
    {
        DialogText text;
        const char *generic = "";
        switch (action)
        {
        case LayerAction::Create:
            text.title = kNewLayerTitle;
            generic = kNewLayerFailed;
            break;
        case LayerAction::Rename:
            text.title = kRenameLayerTitle;
            generic = kRenameLayerFailed;
            break;
        case LayerAction::Move:
            text.title = kMoveLayerTitle;
            generic = kMoveLayerFailed;
            break;
        case LayerAction::Delete:
            text.title = kDeleteLayerTitle;
            generic = kDeleteLayerFailed;
            break;
        }
        text.message = store_error.empty() ? std::string(generic) + " " + DB_LOCKED_HINT : store_error;
        return text;
    }

    DialogText bulk_layer_visibility_notice(const BulkLayerPlan &plan)
    {
        DialogText text;
        if (plan.outcome == BulkLayerOutcome::NoOwnLayers)
        {
            text.title = kLayerVisibilityTitle;
            text.message = kBidHasNoOwnLayersText;
        }
        return text;
    }

    DialogText layer_refresh_failure_text(LayerAction action)
    {
        DialogText text;
        switch (action)
        {
        case LayerAction::Create:
            text.title = kRefreshErrorTitle;
            text.message = kLayerCreatedRefreshFailed;
            break;
        case LayerAction::Delete:
            text.title = kDeleteLayerTitle;
            text.message = kLayersDeletedRefreshFailed;
            break;
        case LayerAction::Rename:
            text.title = kRefreshErrorTitle;
            text.message = kLayerRenamedRefreshFailed;
            break;
        case LayerAction::Move:
            text.title = kRefreshErrorTitle;
            text.message = kLayerMovedRefreshFailed;
            break;
        }
        return text;
    }

}
