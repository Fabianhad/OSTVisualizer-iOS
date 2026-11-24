// Layer: Domain (pure C++)
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The page-scale capture of the undo history (plan 4.7, 5.4, N-M1-06): which page and which scale pair an undo / redo entry stores for a takeoff when the
// entry is created, and what a replay does with the stored positions. The arithmetic (the factor, the refusals of a factor or a result that is not
// usable) is ost_domain::rescale_entry_positions (PageScaleTransform.hpp); this file is the lookup the UndoRedoDelegate answers with and the rule of the
// two cases the arithmetic does not know (nothing captured, a captured page that is gone). Windows source of truth:
//   ost_visualizer/presentation/handlers/plan_view_action_handler.py:1667-1673   _page_scale_for_page_uid: (float(sf1 or 1.0), float(sf2 or 1.0))
//   ost_visualizer/presentation/handlers/plan_view_action_handler.py:1706-1725   _capture_item_page_scales (keyed by the takeoff's page uid)
//   ost_visualizer/presentation/handlers/plan_view_action_handler.py:1788        _page_scale_for_page_uid(spec.page_uid) at replay
// Deliberate differences from Windows, each pinned by a test that says so:
//   - a page the project does not have is UNKNOWN (nullopt), not the pair (1.0, 1.0) that the Windows lookup answers: at creation nothing is captured for
//     that takeoff (its replay factor is 1), at replay the undo / redo is REFUSED (the entry stays, UND-02) instead of writing positions scaled by a made-up
//     pair.
//
// STATE: implemented (PageScaleCapture.cpp); the host tests of Config/Tests/Settings (test_page_scale_capture.cpp) pin it.
#pragma once

#include "PageScaleTransform.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ost_data
{
    struct ProjectData;
}

namespace ost_domain
{
    // The page of a takeoff and the pair of that page, as an undo entry stores them.
    struct TakeoffPageScale
    {
        std::string page_uid;
        PageScale scale;
    };

    // _page_scale_for_page_uid: the pair of the page, each factor `x or 1.0` (a stored 0 or -0 reads 1.0, a NaN or a negative value stays: the replay
    // arithmetic refuses what it cannot use). nullopt for an empty uid and for a page the project does not have.
    std::optional<PageScale> page_scale_of_page(const ost_data::ProjectData &project, const std::string &page_uid);

    // The capture of one takeoff: its page uid and that page's pair. nullopt when the takeoff is not in the project, has no page uid, or its page is not in
    // the project (nothing is captured: the replay factor of that takeoff is 1).
    std::optional<TakeoffPageScale> page_scale_of_takeoff(const ost_data::ProjectData &project, const std::string &takeoff_uid);

    // The positions of one takeoff of an undo / redo entry at the scale that is current NOW. `captured` is what the entry stored (nullopt: nothing was
    // captured, the positions come back unchanged and `current` is not looked at). `current` is the pair of the captured page NOW (nullopt: the page is gone).
    // A captured entry with no current pair refuses (nullopt: the undo / redo does not run). Else rescale_entry_positions(positions, captured, *current),
    // which refuses a factor that is not finite and > 0 and a result that is not usable.
    std::optional<std::vector<double>> replay_entry_positions(const std::vector<double> &positions, const std::optional<PageScale> &captured,
                                                              const std::optional<PageScale> &current);
}
