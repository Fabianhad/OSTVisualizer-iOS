// What an export does with the selected pages (EXP-14) and which pages name the export file (EXP-09): the
// generate_*_export_for_selection and export_page_names functions of Core/Meshing/ExportMeshSet.* (+ pages_with_visible_
// takeoffs of TakeoffMeshBuilder.*).
// Windows oracle: presentation/handlers/export_handler.py export_format (`if not page_uids: return`),
// application/services/export_service.py (collect_takeoffs_for_pages([]) is empty = NO_DATA; page_names =
// get_page_name of valid_page_uids), domain/services/project_data_service.py collect_takeoffs_for_pages(page_uids,
// visible_only) and tests/application/services/test_export_service.py::test_empty_or_hidden_only_collection_returns_no_data_
// without_export.
#include "ExportRuleTestSupport.hpp"

#include "Data/PageNavigation.hpp"
#include "Export/export_filename.hpp"
#include "Export/obj_writer.hpp"
#include "Meshing/TakeoffMeshBuilder.hpp"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

using namespace rule_test;
using ost_meshing::ExportStatus;
using Uids = std::vector<std::string>;

namespace
{
    ost_domain::RenderConfig solid_config()
    {
        ost_domain::RenderConfig config;
        config.color_mode = ost_domain::RenderConfig::MODE_SOLID;
        return config;
    }

    // Pages p1 'A101' and p2 'A102' with one count box each (t1 at x = 0 on p1, t2 at x = 200 on p2) and a third page p3
    // 'A103' without a takeoff.
    ost_data::ProjectData two_pages_with_takeoffs()
    {
        std::vector<ost_domain::Page> pages = {make_page("p1"), make_page("p2"), make_page("p3")};
        pages[0].name = "A101";
        pages[1].name = "A102";
        pages[2].name = "A103";
        ost_data::ProjectData project = project_of({box_condition("count", ConditionType::Count)},
                                                   {makeoff("t1", "count", "p1", {0.0, 0.0}), makeoff("t2", "count", "p2", {200.0, 0.0})}, pages);
        project.bid_name = "Bid";
        project.page_order = {"p1", "p2", "p3"};
        return project;
    }

    Uids mesh_takeoffs_of(const ost_meshing::GroupedExport &set)
    {
        Uids out;
        for (const auto &group : set.groups)
            for (const auto &mesh : group.meshes)
                out.push_back(mesh.takeoff->uid);
        return out;
    }

    Uids mesh_takeoffs_of(const ost_meshing::SceneExport &set)
    {
        Uids out;
        for (const auto &mesh : set.meshes)
            out.push_back(mesh.takeoff->uid);
        return out;
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// EXP-14
// ---------------------------------------------------------------------------------------------------------------

// The EXP-14 plan: a project with 2 pages that have takeoffs and an empty selection exports nothing (Windows: no export;
// NO_DATA). iOS used to write both pages.
TEST(ExportSelection, AnEmptySelectionExportsNothing)
{
    const auto project = two_pages_with_takeoffs();
    const std::set<std::string> none;
    const auto grouped = ost_meshing::generate_condition_export_for_selection(project, none, solid_config());
    CHECK(grouped.status == ExportStatus::NoTakeoffs);
    CHECK(grouped.groups.empty());
    const auto html = ost_meshing::generate_html_export_for_selection(project, none, solid_config());
    CHECK(html.status == ExportStatus::NoTakeoffs);
    CHECK(html.meshes.empty());
}

// "Selection {page1} exports only page1" (EXP-14 plan, LP-13 / MSH-18 aliases), {page2} only page2, both pages both.
TEST(ExportSelection, OnlyTheSelectedPagesAreExported)
{
    const auto project = two_pages_with_takeoffs();
    const auto config = solid_config();

    const auto first = ost_meshing::generate_condition_export_for_selection(project, {"p1"}, config);
    CHECK(first.status == ExportStatus::Ok);
    CHECK_EQ(mesh_takeoffs_of(first), (Uids{"t1"}));
    CHECK_EQ(mesh_takeoffs_of(ost_meshing::generate_html_export_for_selection(project, {"p1"}, config)), (Uids{"t1"}));

    const auto second = ost_meshing::generate_condition_export_for_selection(project, {"p2"}, config);
    CHECK_EQ(mesh_takeoffs_of(second), (Uids{"t2"}));
    CHECK_EQ(mesh_takeoffs_of(ost_meshing::generate_html_export_for_selection(project, {"p2"}, config)), (Uids{"t2"}));

    const auto both = ost_meshing::generate_condition_export_for_selection(project, {"p1", "p2"}, config);
    CHECK_EQ(mesh_takeoffs_of(both), (Uids{"t1", "t2"}));
    CHECK_EQ(mesh_takeoffs_of(ost_meshing::generate_html_export_for_selection(project, {"p1", "p2"}, config)), (Uids{"t1", "t2"}));

    // a selected page without takeoffs is NO_DATA like Windows
    CHECK(ost_meshing::generate_condition_export_for_selection(project, {"p3"}, config).status == ExportStatus::NoTakeoffs);
    CHECK(ost_meshing::generate_html_export_for_selection(project, {"p3"}, config).status == ExportStatus::NoTakeoffs);
}

// The generators keep their documented meaning of the empty set (every page): the 3D view and the other Core tests rely on
// it, and the exports of the app never reach them with an empty selection.
TEST(ExportSelection, TheGeneratorsStillReadAnEmptySetAsEveryPage)
{
    const auto project = two_pages_with_takeoffs();
    const std::set<std::string> none;
    CHECK_EQ(mesh_takeoffs_of(ost_meshing::generate_condition_export(project, none, solid_config())), (Uids{"t1", "t2"}));
    CHECK_EQ(mesh_takeoffs_of(ost_meshing::generate_html_export(project, none, solid_config())), (Uids{"t1", "t2"}));
}

// ---------------------------------------------------------------------------------------------------------------
// EXP-09: the pages that name the file
// ---------------------------------------------------------------------------------------------------------------

// Windows: page_names = get_page_name(uid) for valid_page_uids, the selected pages that have a visible takeoff: a selected
// page without a takeoff is not part of the name (test_export_service.py).
TEST(ExportSelection, OnlyPagesWithTakeoffsNameTheFile)
{
    const auto project = two_pages_with_takeoffs();
    CHECK_EQ(ost_meshing::export_page_names(project, {"p1"}), (Uids{"A101"}));
    CHECK_EQ(ost_meshing::export_page_names(project, {"p3", "p2", "p1"}), (Uids{"A101", "A102"}));
    CHECK_EQ(ost_meshing::export_page_names(project, {"p3"}), (Uids{}));
    CHECK_EQ(ost_meshing::export_page_names(project, {}), (Uids{}));
    CHECK_EQ(ost_meshing::export_page_names(project, {"nope"}), (Uids{}));
}

// A page whose only takeoffs are on a hidden layer is not exportable (visible_only), so it does not name the file.
TEST(ExportSelection, ATakeoffOnAHiddenLayerDoesNotCount)
{
    auto project = two_pages_with_takeoffs();
    project.conditions["count"].layer_uid = "L";
    ost_domain::Layer layer;
    layer.uid = "L";
    layer.visible = false;
    project.layers["L"] = layer;
    CHECK_EQ(ost_meshing::export_page_names(project, {"p1", "p2"}), (Uids{}));
    project.layers["L"].visible = true;
    CHECK_EQ(ost_meshing::export_page_names(project, {"p1", "p2"}), (Uids{"A101", "A102"}));
}

// The order is the page selector's: a folder's pages come in the tree walk, not in the page order.
TEST(ExportSelection, PageNamesFollowThePageSelectorOrder)
{
    auto project = two_pages_with_takeoffs();
    ost_domain::Folder folder;
    folder.uid = "f";
    folder.name = "Folder";
    folder.page_uids = {"p2"};
    project.folders["f"] = folder;
    project.pages["p2"].folder_uid = "f";
    // tree walk: the folder's page p2 first, then the loose pages p1, p3
    CHECK_EQ(ost_data::page_tree_order(project), (Uids{"p2", "p1", "p3"}));
    CHECK_EQ(ost_meshing::export_page_names(project, {"p1", "p2", "p3"}), (Uids{"A102", "A101"}));
}

// pages_with_visible_takeoffs itself: the order of the argument, a page twice counts twice (the Windows list), an unknown
// page has no takeoff.
TEST(ExportSelection, PagesWithVisibleTakeoffsKeepTheGivenOrder)
{
    const auto project = two_pages_with_takeoffs();
    CHECK_EQ(ost_meshing::pages_with_visible_takeoffs(project, {"p2", "p3", "p1", "p2", "nope"}), (Uids{"p2", "p1", "p2"}));
    CHECK_EQ(ost_meshing::pages_with_visible_takeoffs(project, {}), (Uids{}));
}

// End to end with the Windows rule: the page names feed the file name.
TEST(ExportSelection, TheFileNameFollowsTheSelectedPages)
{
    const auto project = two_pages_with_takeoffs();
    using ost_geometry::export_text::export_filename;
    CHECK_STR_EQ(export_filename("obj", project.bid_name, ost_meshing::export_page_names(project, {"p1"})), "Bid - A101.obj");
    CHECK_STR_EQ(export_filename("dxf", project.bid_name, ost_meshing::export_page_names(project, {"p1", "p2"})), "Bid - A101 + A102.dxf");
    // p3 has no takeoff: it is not "+ 1 more"
    CHECK_STR_EQ(export_filename("html", project.bid_name, ost_meshing::export_page_names(project, {"p1", "p2", "p3"})), "Bid - A101 + A102.html");
}

// The steps of the OBJ export command (Services/ExportService.mm) on the host: the file name rule, the material file next
// to the OBJ under the same stem (".mtl" for the ".obj" of the name), and a write that fails is a different result from
// "nothing to export" (EXP-21): the generation says Ok, the writer says false.
TEST(ExportSelection, TheObjAndItsMaterialFileShareTheStemOfTheName)
{
    auto project = two_pages_with_takeoffs();
    project.bid_name = "Phase 1.2";
    const auto config = solid_config();
    const std::set<std::string> pages = {"p1", "p2"};
    const auto grouped = ost_meshing::generate_condition_export_for_selection(project, pages, config);
    REQUIRE(grouped.status == ExportStatus::Ok);
    const std::string obj_name = ost_geometry::export_text::export_filename("obj", project.bid_name,
                                                                            ost_meshing::export_page_names(project, pages));
    CHECK_STR_EQ(obj_name, "Phase 1.2 - A101 + A102.obj");

    ost_geometry::OBJWriter writer;
    writer.set_display_mode(config.color_mode);
    for (const auto &group : grouped.groups)
        writer.add_group(group.material_name, group.name, group.color.r, group.color.g, group.color.b, group.mesh_pointers());
    export_test::ScratchDir dir("pipeline_names");
    CHECK(writer.save(dir.file(obj_name)));

    const std::string mtl_name = obj_name.substr(0, obj_name.size() - 4) + ".mtl";
    CHECK_STR_EQ(mtl_name, "Phase 1.2 - A101 + A102.mtl");
    CHECK(std::filesystem::exists(std::filesystem::path(dir.file(mtl_name))));
    CHECK(export_test::read_text_lf(dir.file(obj_name)).find("mtllib " + mtl_name + "\n") != std::string::npos);
}

TEST(ExportSelection, AFailedWriteIsNotTheNothingToExportResult)
{
    const auto project = two_pages_with_takeoffs();
    const auto config = solid_config();
    const auto grouped = ost_meshing::generate_condition_export_for_selection(project, {"p1"}, config);
    REQUIRE(grouped.status == ExportStatus::Ok); // there is something to write
    ost_geometry::OBJWriter writer;
    for (const auto &group : grouped.groups)
        writer.add_group(group.material_name, group.name, group.color.r, group.color.g, group.color.b, group.mesh_pointers());
    export_test::ScratchDir dir("pipeline_unwritable");
    CHECK(!writer.save(dir.file("no_such_directory/Bid - A101.obj")));
    // and the opposite: nothing survives, so there is no writer to fail
    const auto nothing = ost_meshing::generate_condition_export_for_selection(
        rule_project(negative_only_takeoffs()), {"p"}, config);
    CHECK(nothing.status == ExportStatus::NothingToExport);
    CHECK(nothing.groups.empty());
}
