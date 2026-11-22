// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
#include "../Domain/Page.hpp"
#include "../Domain/Condition.hpp"
#include "../Domain/ConditionChanges.hpp"
#include "../Domain/Takeoff.hpp"
#include "../Domain/Folder.hpp"
#include "../Domain/Layer.hpp"
#include "../Domain/CdnType.hpp"
#include "../Domain/ConditionFolder.hpp"
#include "../Domain/Bid.hpp"
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace ost_data
{

    struct ProjectData
    {
        ost_domain::Bid bid;
        std::string project_name;
        std::string bid_name;
        std::unordered_map<std::string, ost_domain::Page> pages;
        // The UIDs of `pages` in the Windows page order (BidPages ORDER BY Sequence, Name, UID; ost_store::load_project): the
        // order of the loose pages of the page tree and of the pages inside a folder (PageNavigation.hpp). Index1 is only the
        // PDF page index.
        std::vector<std::string> page_order;
        // BidSettings.BidPageSelectedUID: the page the bid was last left on ("" when none is saved), restored as the first
        // page when it still exists (ost_data::initial_page_uid).
        std::string selected_page_uid;
        std::unordered_map<std::string, ost_domain::Condition> conditions;
        // The bid's own layers by UID: the layers its conditions and pages refer to (Windows bid_layers).
        std::unordered_map<std::string, ost_domain::Layer> layers;
        // The layer list of the layer sheet, in display order: ost_domain::merge_layers_for_bid of the bid's own layers and the
        // locked template layers, without the Comments layer (ost_store::load_project; Windows get_merged_bid_layers). A
        // bid-owned entry's `show` is the stored value at load time; the current visibility of a bid layer is
        // layers[uid].visible. Empty when the list could not be read (Windows get_merged_bid_layers logs and returns no layers).
        std::vector<ost_domain::BidLayer> merged_layers;
        std::unordered_map<std::string, ost_domain::CdnType> cdn_types;
        std::unordered_map<std::string, ost_domain::ConditionFolder> condition_folders;
        // The page folders at the top level, in Windows order (Name, UID); the folders below them are in Folder::subfolders.
        ost_domain::OrderedUidMap<ost_domain::Folder> folders;
        std::vector<ost_domain::Takeoff> takeoffs;

        bool empty() const { return bid.uid.empty(); }

        // In-memory counterpart of ost_store::delete_condition: the condition and its takeoffs go, and every surviving
        // takeoff that named one of the removed takeoffs as its parent becomes a root takeoff (parent "0"), exactly like
        // the database after the delete (Windows nulls ParentUID of the survivors, condition_operations.py
        // delete_conditions) and like load_project reads it back.
        void remove_condition(const std::string &condition_uid)
        {
            conditions.erase(condition_uid);
            std::unordered_set<std::string> removed_takeoffs;
            for (const ost_domain::Takeoff &t : takeoffs)
                if (t.condition_uid == condition_uid)
                    removed_takeoffs.insert(t.uid);
            takeoffs.erase(std::remove_if(takeoffs.begin(), takeoffs.end(),
                                          [&condition_uid](const ost_domain::Takeoff &t)
                                          { return t.condition_uid == condition_uid; }),
                           takeoffs.end());
            for (ost_domain::Takeoff &t : takeoffs)
                if (removed_takeoffs.count(t.parent_uid))
                    t.parent_uid = "0";
        }

        // ost_domain::shift_conflicting_ref_nos on this project's conditions (see there).
        void shift_conflicting_ref_nos(const std::string &condition_uid, int new_ref_no)
        {
            ost_domain::shift_conflicting_ref_nos(conditions, condition_uid, new_ref_no);
        }

        // Windows page_visualization_metadata_service.py build_pages: both factors read `x or 1.0` (a zero becomes 1.0), then
        // ratio = sf2 / sf1 if sf1 > 0 else 1.0, so any non-positive sf1 (and NaN) gives 1.0. An unknown page has ratio 1.0.
        double page_scale_ratio(const std::string &page_uid) const
        {
            auto it = pages.find(page_uid);
            if (it == pages.end())
                return 1.0;
            const double sf1 = it->second.scale_factor1 != 0.0 ? it->second.scale_factor1 : 1.0;
            const double sf2 = it->second.scale_factor2 != 0.0 ? it->second.scale_factor2 : 1.0;
            return sf1 > 0.0 ? sf2 / sf1 : 1.0;
        }
        // A condition is visible unless its layer is one of the bid's own layers and that layer is hidden. There is no per-
        // condition flag: this is Windows is_layer_visible (layer.py:112-118, a falsy layer uid and a layer uid that is not
        // among the bid's own layers are visible) applied to the live layers on every use. So a condition without a layer, with
        // a blank or unknown layer or on a shared template row is visible, after Hide All / Show All exactly as after a reload
        // (LP-22; Windows update_all_layer_visibility sets conditions the same way). Layer uids are text.
        bool is_condition_visible(const ost_domain::Condition &cdn) const
        {
            if (cdn.layer_uid.empty())
                return true;
            auto it = layers.find(cdn.layer_uid);
            if (it == layers.end())
                return true;
            return it->second.visible;
        }

        // The in-memory side of Show All / Hide All after the write worked: the named layers of the bid get the visibility
        // `show` (the uids of ost_domain::plan_bulk_layer_visibility). A uid that is not one of the bid's layers is ignored
        // (no layer is created), so a shared row can never change here. Conditions and the page background follow because they
        // are derived from `layers` (is_condition_visible, ost_domain::is_image_layer_visible).
        void apply_layer_visibility(const std::vector<std::string> &layer_uids, bool show)
        {
            for (const std::string &uid : layer_uids)
            {
                auto it = layers.find(uid);
                if (it != layers.end())
                    it->second.visible = show;
            }
        }

        // What the layer sheet shows for a row of the layer list: the live visibility of a layer of the bid (the toggles made
        // since the load), else the stored Show of the listed row (a shared template row cannot be changed from the sheet),
        // else visible for a uid the list does not have.
        bool is_listed_layer_visible(const std::string &layer_uid) const
        {
            auto own = layers.find(layer_uid);
            if (own != layers.end())
                return own->second.visible;
            for (const ost_domain::BidLayer &entry : merged_layers)
            {
                if (entry.uid == layer_uid)
                    return entry.show;
            }
            return true;
        }
    };

    // COND-PLACE-01 note 1: placement_mode.py handle_place_press (:1648) "condition = self._current_conditions.get(active_uid); if
    // not condition or not condition.layer_visible: event.accept(); return", and the same check at the release of an Area / Linear
    // drag (:1754, :1819) and in enter_place_mode_for_condition (:1869): a press for a condition that is gone or not visible does
    // nothing at all - no preview, no point of a polygon, no takeoff. `condition_uid` is the condition the placement tool is
    // armed on. The Image layer toggle does not suspend the tool, so this is where an armed condition of that layer is refused
    // while the layer is hidden; its presses place again when the layer is shown. A condition without a layer, with a layer uid
    // the bid does not have or on a shared template row is visible (is_condition_visible), hence accepted. The app asks this at
    // the start of every placement gesture (PlanViewEditDelegate planView:canPlaceWithCondition:), when the floating mode button
    // would enter the Place mode, and where the finished placement arrives (planView:didPlaceTakeoffWithCondition:position:).
    inline bool placement_tap_accepted(const ProjectData &project, const std::string &condition_uid)
    {
        auto placing = project.conditions.find(condition_uid);
        return placing != project.conditions.end() && project.is_condition_visible(placing->second);
    }

}
