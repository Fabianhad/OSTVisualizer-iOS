// LP-24 (a), owner note d (3), analyst row A8-19: a bid without any Image layer.
//
// Windows oracle (HEAD d26893c) domain/services/project_data_service.py:698-705 update_all_layer_visibility:
// `page_visible = visible if an Image layer exists else True`, so with no Image layer Hide All leaves the pages visible (before, the memory
// hid them while a reload showed them); tests/domain/services/test_project_data_service.py::ProjectDataServiceBulkVisibilityMatchesReloadTests.
// test_without_any_image_layer_hide_all_leaves_the_pages_visible. iOS derives the page background from the bid's own layers on every use
// (PlanView.mm: is_image_layer_visible(project.layers), no Image layer = visible), which is the new Windows memory behaviour; this pins it.
#include "StoreTestSupport.hpp"

#include "Domain/Layer.hpp"

#include <string>
#include <vector>

using ost_domain::BidLayer;
using ost_domain::BulkLayerOutcome;
using ost_domain::Layer;

namespace
{
    BidLayer own_layer(const std::string &uid, const std::string &name, bool show, int sequence)
    {
        BidLayer l;
        l.uid = uid;
        l.bid_uid = "b1";
        l.name = name;
        l.show = show;
        l.sequence = sequence;
        return l;
    }

    ost_data::ProjectData project_of(const std::vector<BidLayer> &list)
    {
        ost_data::ProjectData project;
        project.bid.uid = "b1";
        project.merged_layers = list;
        for (const BidLayer &l : list)
        {
            Layer live;
            live.uid = l.uid;
            live.name = l.name;
            live.visible = l.show;
            live.sequence = l.sequence;
            project.layers[l.uid] = live;
        }
        return project;
    }

    // What the Hide All / Show All handler does without a database: plan, then the change in memory.
    void bulk(ost_data::ProjectData &project, bool show)
    {
        const ost_domain::BulkLayerPlan plan =
            ost_domain::plan_bulk_layer_visibility(project.merged_layers, project.bid.uid, project.layers, show);
        REQUIRE(plan.outcome == BulkLayerOutcome::Write);
        project.apply_layer_visibility(plan.uids, show);
    }
}

TEST(Lp24_HideAllWithoutImageLayer, HideAllLeavesThePageBackgroundVisible)
{
    ost_data::ProjectData project = project_of({own_layer("1", "Default", true, 0), own_layer("2", "Walls", true, 1)});
    CHECK(ost_domain::is_image_layer_visible(project.layers));
    bulk(project, false);
    CHECK_FALSE(project.layers.at("1").visible); // the layers themselves are hidden ...
    CHECK_FALSE(project.layers.at("2").visible);
    CHECK(ost_domain::is_image_layer_visible(project.layers)); // ... the pages are not: there is no Image layer to hide
    bulk(project, true);
    CHECK(ost_domain::is_image_layer_visible(project.layers));
}

// the names that are not "image" after the strip / lower of the Windows lookup never count as the Image layer
TEST(Lp24_HideAllWithoutImageLayer, AnImageLikeNameThatIsNotTheImageLayerDoesNotHideThePages)
{
    ost_data::ProjectData project = project_of({own_layer("1", "Images", true, 0), own_layer("2", "My Image", true, 1)});
    bulk(project, false);
    CHECK(ost_domain::is_image_layer_visible(project.layers));
    // and the control: a bid's own Image layer (any case, blanks around) does decide the page background
    ost_data::ProjectData with_image = project_of({own_layer("1", " IMAGE ", true, 0), own_layer("2", "Walls", true, 1)});
    bulk(with_image, false);
    CHECK_FALSE(ost_domain::is_image_layer_visible(with_image.layers));
    bulk(with_image, true);
    CHECK(ost_domain::is_image_layer_visible(with_image.layers));
}
