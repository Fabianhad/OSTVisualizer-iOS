// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// What a layer rename means for what is drawn (plan 5.3 and 4.11): the Layers sheet asks it after a rename that worked, with the layer list before
// the write and the list the reload returned, and draws the page background and the meshes again only when the answer is false.
//
// Windows source of truth (49a2d59): ost_visualizer/application/layer_change_impact.py:12-34 layer_rename_preserves_rendering, used by
// application/services/project_write_service.py:6832-6854 update_layer_name (`unchanged = layer_rename_preserves_rendering(before, after)`, then
// notify_database_refreshed(image_sources_unchanged=unchanged, mesh_scene_unchanged=unchanged)); pinned by
// tests/application/test_layer_change_impact.py (7 tests).
#pragma once
#include "Layer.hpp"
#include <vector>

namespace ost_domain
{

    // True only when the two lists differ by renames of ordinary layers and nothing else, so that nothing already drawn can change (the page
    // background follows the layer named "image", the annotations the layer named "annotation", the comments the layer named "comments"; every
    // other name is a label). The rules, in the order of the Windows function:
    //   - an empty `before`, or lists that do not hold the same set of uids, answer false;
    //   - a uid that occurs twice in either list answers false (the lists are not a clean snapshot);
    //   - every uid keeps all its fields but the name (bid, show, sequence, template and locked flags): any other change answers false;
    //   - a layer whose name changed (compared exactly, so a change of letter case or of surrounding spaces is a rename) answers false when its old
    //     or its new name is, after normalize_layer_name (Python strip().lower()), "annotation", "comments" or "image";
    //   - at least one layer must have been renamed: two equal lists answer false.
    // The order of the lists does not matter. The merged list of the sheet merges layers whose names collide (merge_layers_for_bid), so a rename
    // that makes two names equal drops a uid from `after` and answers false.
    bool layer_rename_preserves_rendering(const std::vector<BidLayer> &before, const std::vector<BidLayer> &after);

}
