// LP-15: the Image layer decides whether the page background (the PDF) is drawn (PlanView.mm drew it whenever image_path existed,
// the Image layer had no effect). Core ost_domain::is_image_layer_visible.
//
// Windows oracle: domain/entities/layer.py (normalize_layer_name, get_layer_uid_by_name, is_layer_visible; IMAGE_LAYER_NAME =
// "image"), infrastructure/mdb/components/bid_data_reader.py:678-679 (Page.layer_visible = the visibility of the layer named
// "image"), tests/domain/services/test_project_data_service.py::test_image_layer_visibility_updates_page_memory_immediately,
// ::test_show_all_layer_visibility_updates_conditions_and_pages_immediately and
// ::test_non_image_layer_visibility_does_not_update_page_memory_by_name_guess.
#include "TestHarness.hpp"

#include "Domain/Layer.hpp"

#include <string>
#include <unordered_map>

using ost_domain::Layer;

namespace
{
    std::unordered_map<std::string, Layer> layers_of(std::initializer_list<Layer> list)
    {
        std::unordered_map<std::string, Layer> map;
        for (const Layer &layer : list)
            map[layer.uid] = layer;
        return map;
    }
}

// Item plan: layers (uid 1 'Default' show, uid 3 'Image' show = 0): the background is hidden; with the Image layer shown it is
// drawn. Show All / Hide All change the visibility of every layer, the Image layer included.
TEST(ImageLayer, TheImageLayerDecides)
{
    auto layers = layers_of({{"1", "Default", true, 0}, {"3", "Image", false, 1}});
    CHECK_FALSE(ost_domain::is_image_layer_visible(layers));
    layers["3"].visible = true; // Show All
    CHECK(ost_domain::is_image_layer_visible(layers));
    for (auto &entry : layers) // Hide All
        entry.second.visible = false;
    CHECK_FALSE(ost_domain::is_image_layer_visible(layers));
}

// "No layer named 'image' (any case / padding): it is visible"; the name is str.strip().lower(); another layer's visibility does
// not matter ("non-image layer visibility does not update page memory by name guess").
TEST(ImageLayer, NameMatchIsTrimmedAndCaseInsensitive)
{
    CHECK(ost_domain::is_image_layer_visible({}));
    CHECK(ost_domain::is_image_layer_visible(layers_of({{"1", "Default", false, 0}, {"2", "Annotation", false, 1}})));
    CHECK_FALSE(ost_domain::is_image_layer_visible(layers_of({{"3", "  IMAGE ", false, 0}})));
    CHECK_FALSE(ost_domain::is_image_layer_visible(layers_of({{"3", "image", false, 0}})));
    CHECK_FALSE(ost_domain::is_image_layer_visible(layers_of({{"3", "\tImage\n", false, 0}})));
    CHECK(ost_domain::is_image_layer_visible(layers_of({{"3", "Images", false, 0}})));
    CHECK(ost_domain::is_image_layer_visible(layers_of({{"3", "Im age", false, 0}})));
    CHECK(ost_domain::is_image_layer_visible(layers_of({{"3", "", false, 0}})));
}

// Windows takes the first layer of that name in database order, which a map does not keep: of several layers named "image" the
// one with the smallest sequence decides (then the smallest uid), the same on every run.
TEST(ImageLayer, SeveralLayersOfThatNameAreDecidedDeterministically)
{
    CHECK_FALSE(ost_domain::is_image_layer_visible(layers_of({{"9", "Image", false, 0}, {"4", "image", true, 5}})));
    CHECK(ost_domain::is_image_layer_visible(layers_of({{"9", "Image", true, 0}, {"4", "image", false, 5}})));
    CHECK_FALSE(ost_domain::is_image_layer_visible(layers_of({{"2", "Image", false, 3}, {"7", "IMAGE", true, 3}})));
    CHECK_FALSE(ost_domain::is_image_layer_visible(layers_of({{"7", "Image", true, 3}, {"2", "IMAGE", false, 3}})));
}
