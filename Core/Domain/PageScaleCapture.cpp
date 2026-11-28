// Layer: Domain (pure C++). The page-scale capture of the undo history: see PageScaleCapture.hpp.
#include "PageScaleCapture.hpp"

#include "../Data/ProjectData.hpp"

namespace ost_domain
{
    std::optional<PageScale> page_scale_of_page(const ost_data::ProjectData &project, const std::string &page_uid)
    {
        if (page_uid.empty())
            return std::nullopt;
        const auto found = project.pages.find(page_uid);
        if (found == project.pages.end())
            return std::nullopt;
        // plan_view_action_handler.py:1667-1673: float(sf1 or 1.0), float(sf2 or 1.0): only a zero (and -0) is coalesced, a NaN is truthy in Python and stays
        PageScale scale;
        scale.sf1 = found->second.scale_factor1 != 0.0 ? found->second.scale_factor1 : 1.0;
        scale.sf2 = found->second.scale_factor2 != 0.0 ? found->second.scale_factor2 : 1.0;
        return scale;
    }

    std::optional<TakeoffPageScale> page_scale_of_takeoff(const ost_data::ProjectData &project, const std::string &takeoff_uid)
    {
        if (takeoff_uid.empty())
            return std::nullopt;
        for (const Takeoff &takeoff : project.takeoffs)
        {
            if (takeoff.uid != takeoff_uid)
                continue;
            const std::optional<PageScale> scale = page_scale_of_page(project, takeoff.page_uid);
            if (!scale)
                return std::nullopt;
            return TakeoffPageScale{takeoff.page_uid, *scale};
        }
        return std::nullopt;
    }

    std::optional<std::vector<double>> replay_entry_positions(const std::vector<double> &positions, const std::optional<PageScale> &captured,
                                                              const std::optional<PageScale> &current)
    {
        // nothing captured: factor 1 (not the pair (1, 1) of Windows, whose ratio would be the current one)
        if (!captured)
            return positions;
        // captured, but the page is gone: the undo / redo does not run (UND-02), the made-up pair of Windows is never used
        if (!current)
            return std::nullopt;
        return rescale_entry_positions(positions, captured, *current);
    }
}
