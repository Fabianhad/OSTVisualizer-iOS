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
        bool is_condition_visible(const ost_domain::Condition &cdn) const
        {
            if (cdn.layer_uid.empty())
                return true;
            auto it = layers.find(cdn.layer_uid);
            if (it == layers.end())
                return true;
            return it->second.visible;
        }
    };

}
