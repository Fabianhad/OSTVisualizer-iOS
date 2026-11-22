#pragma once
// The meshes of the 3D view and of the OBJ / DXF / HTML exports after the boolean pass: ONE platform-independent
// orchestration for all of them, as in Windows (MSH-15 / MSH-16 / MSH-20 / EXP-05 / EXP-06 / EXP-11 / EXP-22 / RB-02 /
// RB-03). Services/ProjectService.mm (generateMeshesIntoRenderer) and Services/ExportService.mm only hand the result to
// the renderer / the writers.
//
//   takeoffs of the selected pages -> element takeoffs (TakeoffMeshBuilder.hpp) -> meshes -> boolean pass
//   (BooleanPass.hpp: negatives cut only the positives of their condition type, a consumed positive is dropped,
//   negatives are never output) -> the surviving positives with the colour of display_color (DisplayColor.hpp).
//
// The writers use ONLY these meshes: a takeoff without a surviving mesh (a negative, a consumed positive, a takeoff
// without a mesh) writes nothing and is never rebuilt from its unsubtracted mesh.
#include "BooleanPass.hpp"
#include "TakeoffMeshBuilder.hpp"
#include "../Domain/DisplayColor.hpp"
#include "../Domain/RenderConfig.hpp"
#include <set>
#include <string>
#include <vector>

namespace ost_meshing
{

    // Why an export has nothing to write (the caller shows the matching alert, EXP-21); no file may be written then.
    enum class ExportStatus
    {
        Ok,
        // The selected pages have no (visible) takeoff: Windows ExportErrorCode.NO_DATA, "No takeoffs found for any of the
        // selected pages."
        NoTakeoffs,
        // There are takeoffs, but no mesh survives: negative-only, every positive consumed by negatives of its type, no
        // takeoff with a valid mesh, no exportable condition (the Windows exporter returns False, HTML None;
        // ExportErrorCode.WORKER_FAILED, "Export function returned False"). Reported in every format (EXP-22).
        NothingToExport
    };

    // One element mesh after the boolean pass.
    struct BooleanMesh
    {
        const ost_domain::Takeoff *takeoff = nullptr;
        const ost_domain::Condition *condition = nullptr;
        ost_geometry::CppMeshData mesh;
        ost_domain::DisplayColor color;
    };

    // The meshes of the 3D view (Windows process_takeoffs_to_meshes through _MeshGeneratorAdapter): the page rotation and
    // flip are applied to every mesh before the boolean pass. Order: area, linear, count, attachment (MSH-20). Empty
    // when the scene has nothing (no takeoffs, nothing survives): the renderer shows nothing; there is no export status
    // in the view. Throws std::invalid_argument where generate_takeoff_meshes does (a page rotation that is not a
    // multiple of 90 degrees: the scene generation fails and the view stays empty).
    std::vector<BooleanMesh> generate_view_meshes(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        const ost_domain::RenderConfig &config,
        const BooleanDifferenceFn &difference = {});

    // The meshes of the HTML export (Windows process_meshes_for_threejs): no page transform, order area, linear, count,
    // attachment. status NoTakeoffs / NothingToExport and no meshes when there is nothing to export.
    struct SceneExport
    {
        ExportStatus status = ExportStatus::Ok;
        std::vector<BooleanMesh> meshes;
    };
    SceneExport generate_html_export(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        const ost_domain::RenderConfig &config,
        const BooleanDifferenceFn &difference = {});

    // One condition of an OBJ / DXF export: one group, one material and one layer per condition, also when none of its
    // takeoffs has a surviving mesh (RB-03: a negative-only condition or a condition whose positives were all consumed
    // keeps an empty group, material and layer).
    struct ExportGroup
    {
        std::string condition_uid;
        // The material name of the OBJ group (g / usemtl / newmtl) and the DXF layer: sanitize('<cdn type name or
        // Unknown>_<name>_<condition uid[:8]>') (base_exporter.py _prepare_hierarchical_export).
        std::string material_name;
        // The condition name of the '# <name>' comment lines: the condition name, 'Condition_<uid>' when it is empty.
        std::string name;
        ost_domain::DisplayColor color;
        // The surviving positive meshes of the condition in takeoff order; may be empty.
        std::vector<BooleanMesh> meshes;

        // The meshes as the pointers OBJWriter::add_group takes.
        std::vector<const ost_geometry::CppMeshData *> mesh_pointers() const
        {
            std::vector<const ost_geometry::CppMeshData *> out;
            out.reserve(meshes.size());
            for (const auto &m : meshes)
                out.push_back(&m.mesh);
            return out;
        }
    };

    // The OBJ / DXF export (Windows BaseExporter.export): the groups in the order the conditions first occur among the
    // non-area takeoffs followed by the area parent takeoffs (EXP-11). The boolean pass sees the meshes group by group.
    // status NoTakeoffs / NothingToExport and no groups when there is nothing to export; nothing may be written then.
    struct GroupedExport
    {
        ExportStatus status = ExportStatus::Ok;
        std::vector<ExportGroup> groups;
    };
    GroupedExport generate_condition_export(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        const ost_domain::RenderConfig &config,
        const BooleanDifferenceFn &difference = {});

    // base_exporter.py _prepare_hierarchical_export: condition.name, or 'Condition_<uid>' when the name is empty.
    std::string export_condition_name(const std::string &condition_uid, const ost_domain::Condition &condition);
    // The sanitised material / layer name of a condition (see ExportGroup::material_name).
    std::string export_material_name(const std::string &condition_uid, const ost_domain::Condition &condition);

    // ---- what the app's export commands call (EXP-14, EXP-09) -------------------------------------------------------------
    // An export acts on exactly the pages the user selected: Windows export_handler.export_format returns without doing
    // anything for an empty selection (the menu action is disabled), and collect_takeoffs_for_pages([]) is empty, which is
    // NO_DATA. The generators above read an EMPTY set as "every page" (the 3D view keeps its own early return and the host
    // tests use the empty set for "all pages"), so the OBJ / DXF / HTML exports of the app go through these two: an empty
    // selection is ExportStatus::NoTakeoffs, nothing is generated and nothing may be written. Otherwise they are
    // generate_condition_export / generate_html_export.
    GroupedExport generate_condition_export_for_selection(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        const ost_domain::RenderConfig &config,
        const BooleanDifferenceFn &difference = {});
    SceneExport generate_html_export_for_selection(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        const ost_domain::RenderConfig &config,
        const BooleanDifferenceFn &difference = {});

    // The names of the pages an export file is named after (Windows ExportService: get_page_name of
    // collect_takeoffs_for_pages(...).valid_page_uids): the selected pages that have a visible takeoff, in the order of the
    // page selector (ost_data::selected_pages_in_tree_order; the Windows list is in click order, which the iOS selection
    // does not keep). An unknown page cannot be selected here. Empty when there is nothing to export for the selection.
    std::vector<std::string> export_page_names(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages);

}
