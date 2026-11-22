// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The sidebar quantities of the open view, extracted from ViewController._updateConditionQuantities (QTY-11, QTY-15).
//
// Windows: ost_visualizer/presentation/coordinators/sidebar_coordinator.py update_conditions_quantities (218-248): in the 3D
// view the totals use the selected 3D page uids, in the 2D view only the active 2D page, and with no page at all they are {}
// (every condition shows zero); project_data_service.compute_quantities_for_pages (1097-1104) feeds the takeoffs of exactly
// those pages, page after page, to compute_page_quantities.
#pragma once
#include "Condition.hpp"
#include "Takeoff.hpp"
#include "UomService.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_domain
{

    // The pages the sidebar totals are summed over: the selected 3D pages in the 3D view, the active 2D page (none when
    // there is none) in the 2D view.
    inline std::vector<std::string> quantity_page_uids(bool is_3d_view,
                                                       const std::string &active_2d_page_uid,
                                                       const std::vector<std::string> &selected_3d_page_uids)
    {
        if (is_3d_view)
            return selected_3d_page_uids;
        if (active_2d_page_uid.empty())
            return {};
        return {active_2d_page_uid};
    }

    // The quantities of every condition for the open view (the map has an entry for each condition that has a takeoff on
    // those pages; a missing condition shows zero). `selected_3d_page_uids` lists each page once, in page order.
    inline std::unordered_map<std::string, QuantityResult> compute_view_quantities(
        const std::unordered_map<std::string, Condition> &conditions,
        const std::vector<Takeoff> &takeoffs,
        bool is_3d_view,
        const std::string &active_2d_page_uid,
        const std::vector<std::string> &selected_3d_page_uids)
    {
        const std::vector<std::string> page_uids =
            quantity_page_uids(is_3d_view, active_2d_page_uid, selected_3d_page_uids);
        if (page_uids.empty())
            return {};
        std::vector<Takeoff> page_takeoffs;
        for (const std::string &page_uid : page_uids)
        {
            for (const Takeoff &takeoff : takeoffs)
            {
                if (takeoff.page_uid == page_uid)
                    page_takeoffs.push_back(takeoff);
            }
        }
        return compute_page_quantities(conditions, page_takeoffs);
    }

}
