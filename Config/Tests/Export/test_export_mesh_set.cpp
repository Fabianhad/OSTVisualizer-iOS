// The meshes of the 3D view and of the OBJ / DXF / HTML exports (Core/Meshing/ExportMeshSet.*): one rule for all of them
// (MSH-16, MSH-15, EXP-05), the nothing-to-export result in every format (EXP-22, RB-02), one group / material / layer per
// condition including the empty ones (EXP-11, RB-03) and the Windows mesh order (MSH-20).
// Windows oracle: tests/presentation/visualization/exporters/boolean_rule_support.py through OBJExporter, DXFExporter and
// visualize_with_threejs, and the grouping / order scenarios below, all run read-only (ExportRuleOracleData.hpp).
// The scenarios of the rule run with a stand-in for the boolean difference (axis-aligned slab cuts only) and, under
// '#if OST_HAVE_BOOLEAN', with Manifold. iOS mesh frame: X = -x_ost (the Windows identity frame of the tests has X = x),
// Y up, Z = -y_ost.
#include "ExportRuleTestSupport.hpp"
#include "ExportRuleOracleData.hpp"

#include "Export/dxf_writer.hpp"
#include "Export/export_text.hpp"
#include "Export/obj_writer.hpp"
#include "Geometry/feature_edges.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace rule_test;
using ost_meshing::BooleanDifferenceFn;
using ost_meshing::ExportStatus;

namespace
{
    ost_domain::RenderConfig solid_config()
    {
        ost_domain::RenderConfig c;
        c.color_mode = ost_domain::RenderConfig::MODE_SOLID;
        c.grayscale_enabled = false;
        return c;
    }

    const std::set<std::string> kAllPages;

    // The Windows x span (identity frame) of the surviving meshes against the iOS X span (X = -x).
    void check_x_span(const std::vector<const ost_meshing::BooleanMesh *> &meshes, double x_lo, double x_hi)
    {
        Span s;
        for (const auto *m : meshes)
        {
            const Span one = span_of(m->mesh, 0);
            s.lo = std::min(s.lo, one.lo);
            s.hi = std::max(s.hi, one.hi);
        }
        CHECK_EQ(s.lo, -x_hi);
        CHECK_EQ(s.hi, -x_lo);
    }

    size_t feature_lines(const Mesh &mesh) { return ost_geometry::extract_feature_edges(mesh, 0.1).size(); }

    // The four scenarios of boolean_rule_support through the 3D view, the HTML export and the OBJ / DXF export.
    void check_rule_scenarios(const BooleanDifferenceFn &difference)
    {
        const auto config = solid_config();
        for (const auto &expected : export_rule_oracle::rule_scenarios())
        {
            SCOPED_CONTEXT(expected.name);
            std::vector<Takeoff> takeoffs;
            const std::string name = expected.name;
            if (name == "other_type")
                takeoffs = other_type_negative_takeoffs();
            else if (name == "same_type")
                takeoffs = same_type_negative_takeoffs();
            else if (name == "consumed")
                takeoffs = consumed_positive_takeoffs();
            else
                takeoffs = negative_only_takeoffs();
            const auto project = rule_project(takeoffs);

            const auto grouped = ost_meshing::generate_condition_export(project, kAllPages, config, difference);
            const auto html = ost_meshing::generate_html_export(project, kAllPages, config, difference);
            const auto view = ost_meshing::generate_view_meshes(project, kAllPages, config, difference);

            if (!expected.exported)
            {
                // RB-02 / EXP-22: negative-only -> OBJ False, DXF False, HTML None, nothing written.
                CHECK(grouped.status == ExportStatus::NothingToExport);
                CHECK(grouped.groups.empty());
                CHECK(html.status == ExportStatus::NothingToExport);
                CHECK(html.meshes.empty());
                CHECK(view.empty());
                continue;
            }
            CHECK(grouped.status == ExportStatus::Ok);
            CHECK(html.status == ExportStatus::Ok);

            // HTML scene geometries / 3D view: the takeoffs of the Windows scene, nothing else.
            CHECK_EQ(uids_of(html.meshes), expected.html_uids);
            CHECK_EQ(uids_of(view), expected.html_uids);

            // OBJ / MTL: the groups of the Windows export in its order, empty ones included (RB-03).
            REQUIRE_EQ(grouped.groups.size(), expected.obj_groups.size());
            size_t dxf_lines = 0;
            for (size_t g = 0; g < grouped.groups.size(); ++g)
            {
                SCOPED_CONTEXT("group " + expected.obj_groups[g]);
                const auto &group = grouped.groups[g];
                CHECK_STR_EQ(group.name, expected.obj_groups[g]);
                CHECK_STR_EQ(group.material_name, expected.materials[g]);
                size_t vertices = 0, faces = 0;
                for (const auto &m : group.meshes)
                {
                    vertices += m.mesh.vertices.size();
                    faces += m.mesh.faces.size();
                    dxf_lines += feature_lines(m.mesh);
                }
                CHECK_EQ(vertices, static_cast<size_t>(expected.obj_group_vertices[g]));
                CHECK_EQ(faces, static_cast<size_t>(expected.obj_group_faces[g]));
            }
            CHECK_EQ(dxf_lines, static_cast<size_t>(expected.dxf_lines));
            check_x_span(all_meshes(grouped), expected.x_lo, expected.x_hi);
            std::vector<const ost_meshing::BooleanMesh *> html_meshes;
            for (const auto &m : html.meshes)
                html_meshes.push_back(&m);
            check_x_span(html_meshes, expected.x_lo, expected.x_hi);
        }
    }
} // namespace

// ---------------------------------------------------------------- the four scenarios of the Windows tests
TEST(ExportRule, ScenariosOfTheWindowsExporterTests)
{
    // Port of tests/presentation/visualization/exporters/test_obj_exporter.py::OBJExporterBooleanRuleTests (4 tests),
    // test_dxf_exporter.py::DXFExporterBooleanRuleTests (4) and test_threejs_renderer.py::HtmlExportBooleanRuleTests (4):
    // other_type x -10..10 (8 vertices, 12 faces, 12 LINEs, HTML {'positive'}), same_type x -10..0, consumed -> only the
    // survivor x 190..210 with an empty '# count_block' group, negative_only -> nothing. The cut is a box difference.
    check_rule_scenarios(box_difference_fn());
}

#if OST_HAVE_BOOLEAN
TEST(ExportRuleManifold, ScenariosOfTheWindowsExporterTestsWithManifold)
{
    // The same four scenarios with the real boolean difference (Manifold).
    check_rule_scenarios({});
}

TEST(ExportRuleManifold, TheCutVolumeIsTheWindowsOne)
{
    // MSH-16 plan: |volume| 4000 uncut (other type) vs 2000 cut (same type); the survivor of the consumed case is whole.
    const auto config = solid_config();
    const auto uncut = ost_meshing::generate_html_export(rule_project(other_type_negative_takeoffs()), kAllPages, config);
    REQUIRE_EQ(uncut.meshes.size(), static_cast<size_t>(1));
    CHECK_NEAR(std::fabs(export_test::signed_volume(uncut.meshes[0].mesh)), 4000.0, 1e-6);
    const auto cut = ost_meshing::generate_html_export(rule_project(same_type_negative_takeoffs()), kAllPages, config);
    REQUIRE_EQ(cut.meshes.size(), static_cast<size_t>(1));
    CHECK_NEAR(std::fabs(export_test::signed_volume(cut.meshes[0].mesh)), 2000.0, 1e-6);
    const auto consumed = ost_meshing::generate_html_export(rule_project(consumed_positive_takeoffs()), kAllPages, config);
    REQUIRE_EQ(consumed.meshes.size(), static_cast<size_t>(1));
    CHECK_NEAR(std::fabs(export_test::signed_volume(consumed.meshes[0].mesh)), 4000.0, 1e-6);
}
#endif

TEST(ExportRule, TheScenesHaveNothingToWriteWhenNothingSurvivesThePass)
{
    // Port of ...::test_negative_only_export_fails_without_writing (OBJ False, DXF False, HTML None) and
    // tests/presentation/visualization/exporters/test_base_exporter.py::BaseExporterFailureTests::
    //   test_base_exporter_returns_false_without_writing_when_no_boolean_result_survives: the emptiness check comes after
    //   the boolean pass. Cases of the RB-02 plan: (a) negative-only; (b) a positive swallowed by a same-type negative;
    //   (c) a positive without a position.
    const auto config = solid_config();
    struct Case
    {
        const char *name;
        std::vector<Takeoff> takeoffs;
    };
    const std::vector<Case> cases = {
        {"negative_only", negative_only_takeoffs()},
        {"consumed", {box_takeoff("p", "count", 0.0), box_takeoff("n", "count_block", 0.0, true)}},
        {"no_position", {makeoff("p", "count", "p", {})}}};
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT(c.name);
        const auto project = rule_project(c.takeoffs);
        const auto grouped = ost_meshing::generate_condition_export(project, kAllPages, config, box_difference_fn());
        const auto html = ost_meshing::generate_html_export(project, kAllPages, config, box_difference_fn());
        CHECK(grouped.status == ExportStatus::NothingToExport);
        CHECK(grouped.groups.empty());
        CHECK(html.status == ExportStatus::NothingToExport);
        CHECK(html.meshes.empty());
        CHECK(ost_meshing::generate_view_meshes(project, kAllPages, config, box_difference_fn()).empty());
    }
}

TEST(ExportRule, NoTakeoffsIsAnotherResultThanNothingSurvives)
{
    // EXP-21: Windows collect_takeoffs_for_pages empty -> NO_DATA ('No takeoffs found for any of the selected pages.');
    // takeoffs that exist but give no mesh -> the exporter returns False (WORKER_FAILED). Hidden layers are not collected
    // (visible_only; EXP-15: iOS also filters them in the HTML export).
    const auto config = solid_config();
    // no takeoffs at all, and takeoffs of other pages only
    for (const auto &takeoffs : {std::vector<Takeoff>{}, std::vector<Takeoff>{makeoff("t", "count", "other", {0.0, 0.0})}})
    {
        const auto project = rule_project(takeoffs);
        const std::set<std::string> pages = {"p"};
        CHECK(ost_meshing::generate_condition_export(project, pages, config).status == ExportStatus::NoTakeoffs);
        CHECK(ost_meshing::generate_html_export(project, pages, config).status == ExportStatus::NoTakeoffs);
    }
    // the only takeoff is on a hidden layer
    auto hidden = rule_project({box_takeoff("t", "count", 0.0)});
    hidden.conditions["count"].layer_uid = "L";
    ost_domain::Layer layer;
    layer.uid = "L";
    layer.visible = false;
    hidden.layers["L"] = layer;
    CHECK(ost_meshing::generate_condition_export(hidden, kAllPages, config).status == ExportStatus::NoTakeoffs);
    CHECK(ost_meshing::generate_html_export(hidden, kAllPages, config).status == ExportStatus::NoTakeoffs);
    // takeoffs of a condition that does not exist: collected, but nothing is exportable
    auto orphan = project_of({}, {makeoff("t", "gone", "p", {0.0, 0.0})});
    CHECK(ost_meshing::generate_condition_export(orphan, kAllPages, config).status == ExportStatus::NothingToExport);
    CHECK(ost_meshing::generate_html_export(orphan, kAllPages, config).status == ExportStatus::NothingToExport);
}

TEST(ExportRule, ACutThatLeavesOnlyDegenerateFacesIsNotASurvivingMesh)
{
    // Deliberate deviation from Windows in an unreachable corner (see ExportMeshSet.cpp): the difference returns a valid
    // mesh whose faces are all degenerate; remove_degenerate_faces empties it. Windows keeps the empty entry, so its export
    // would "succeed" with an empty file (the guard only looks at the cache); iOS keeps the nothing-to-export result and
    // draws / writes nothing, like the iOS code before this change.
    const auto config = solid_config();
    const auto project = rule_project({box_takeoff("p", "count", 0.0), box_takeoff("n", "count", 10.0, true)});
    const BooleanDifferenceFn degenerate = [](const Mesh &p, const std::vector<Mesh> &) -> std::optional<Mesh>
    {
        Mesh m;
        m.vertices = p.vertices;
        m.faces = {{0, 0, 1}, {1, 1, 2}};
        return m;
    };
    const auto grouped = ost_meshing::generate_condition_export(project, kAllPages, config, degenerate);
    CHECK(grouped.status == ExportStatus::NothingToExport);
    CHECK(grouped.groups.empty());
    const auto html = ost_meshing::generate_html_export(project, kAllPages, config, degenerate);
    CHECK(html.status == ExportStatus::NothingToExport);
    CHECK(ost_meshing::generate_view_meshes(project, kAllPages, config, degenerate).empty());
}

TEST(ExportRule, TheConsumedTakeoffIsNeverRebuilt)
{
    // RB-03 plan / test_fully_consumed_positive_is_dropped: the consumed takeoff has no mesh in any group, in the HTML
    // list or in the 3D view; the group of its condition holds the survivor only and 'count_block' stays as an empty group.
    const auto config = solid_config();
    const auto project = rule_project(consumed_positive_takeoffs());
    const auto grouped = ost_meshing::generate_condition_export(project, kAllPages, config, box_difference_fn());
    REQUIRE(grouped.status == ExportStatus::Ok);
    REQUIRE_EQ(grouped.groups.size(), static_cast<size_t>(2));
    REQUIRE_EQ(grouped.groups[0].meshes.size(), static_cast<size_t>(1));
    CHECK_STR_EQ(grouped.groups[0].meshes[0].takeoff->uid, "survivor");
    CHECK(grouped.groups[1].meshes.empty());
    CHECK_STR_EQ(grouped.groups[1].condition_uid, "count_block");
    for (const auto *m : all_meshes(grouped))
        CHECK_NE(m->takeoff->uid, std::string("consumed"));
    // the survivor is the whole box at x = 200: X -210..-190, Y 0..10, Z -10..10, 8 vertices, 12 faces
    const Mesh &m = grouped.groups[0].meshes[0].mesh;
    CHECK_EQ(m.vertices.size(), static_cast<size_t>(8));
    CHECK_EQ(m.faces.size(), static_cast<size_t>(12));
    CHECK_EQ(span_of(m, 0).lo, -210.0);
    CHECK_EQ(span_of(m, 0).hi, -190.0);
    CHECK_EQ(span_of(m, 1).lo, 0.0);
    CHECK_EQ(span_of(m, 1).hi, 10.0);
    CHECK_EQ(span_of(m, 2).lo, -10.0);
    CHECK_EQ(span_of(m, 2).hi, 10.0);
}

TEST(ExportRule, ANegativeOfAnotherTypeLeavesAnEmptyGroupOfItsCondition)
{
    // RB-03 / EXP-11 plan other_type: groups 'count' (8 v, 12 f) then an empty 'attachment' group; MTL 'Materials: 2'.
    const auto grouped = ost_meshing::generate_condition_export(rule_project(other_type_negative_takeoffs()), kAllPages,
                                                                solid_config(), box_difference_fn());
    REQUIRE(grouped.status == ExportStatus::Ok);
    REQUIRE_EQ(grouped.groups.size(), static_cast<size_t>(2));
    CHECK_STR_EQ(grouped.groups[0].name, "count");
    CHECK_EQ(grouped.groups[0].meshes.size(), static_cast<size_t>(1));
    CHECK_STR_EQ(grouped.groups[1].name, "attachment");
    CHECK(grouped.groups[1].meshes.empty());
}

namespace
{
    // EXP-05 plan: slab = Area condition (thickness 6, z_value 0) [0,0,100,0,100,100,0,100]; wall = Linear (height 96,
    // thickness 6) [0,0,100,0]; the negative takeoff [50,-20,50,120] belongs to a Linear condition of the same shape.
    ost_data::ProjectData slab_and_wall_project(bool wall_is_positive)
    {
        Condition slab = box_condition("slab", ConditionType::Area);
        slab.thickness = 6.0;
        slab.z_value = 0.0;
        Condition wall = box_condition("wall", ConditionType::Linear);
        wall.height = 96.0;
        wall.thickness = 6.0;
        Condition negwall = wall;
        negwall.uid = negwall.name = "negwall";
        std::vector<Takeoff> takeoffs;
        if (wall_is_positive)
            takeoffs.push_back(makeoff("wall", "wall", "p", {0.0, 0.0, 100.0, 0.0}));
        else
            takeoffs.push_back(makeoff("slab", "slab", "p", {0.0, 0.0, 100.0, 0.0, 100.0, 100.0, 0.0, 100.0}));
        takeoffs.push_back(makeoff("neg", "negwall", "p", {50.0, -20.0, 50.0, 120.0}, true));
        return project_of({slab, wall, negwall}, takeoffs);
    }
} // namespace

TEST(ExportRule, ALinearNegativeDoesNotCutTheAreaSlab)
{
    // Port of the EXP-05 real-pipeline case (Windows OBJ / DXF / HTML, run read-only): the slab stays untouched, ost x 0..100
    // = iOS X -100..0, Y 0..6, Z -100..0, 8 vertices, 12 faces, |volume| 60000, because the negative belongs to a Linear
    // condition (type 0) and the slab is an Area (type 1). The difference must not even be asked.
    int calls = 0;
    const BooleanDifferenceFn recording = [&calls](const Mesh &p, const std::vector<Mesh> &) -> std::optional<Mesh>
    {
        ++calls;
        return p;
    };
    const auto project = slab_and_wall_project(false);
    const auto html = ost_meshing::generate_html_export(project, kAllPages, solid_config(), recording);
    REQUIRE(html.status == ExportStatus::Ok);
    REQUIRE_EQ(html.meshes.size(), static_cast<size_t>(1));
    const Mesh &m = html.meshes[0].mesh;
    CHECK_EQ(m.vertices.size(), static_cast<size_t>(8));
    CHECK_EQ(m.faces.size(), static_cast<size_t>(12));
    CHECK_EQ(span_of(m, 0).lo, -100.0);
    CHECK_EQ(span_of(m, 0).hi, 0.0);
    CHECK_EQ(span_of(m, 1).lo, 0.0);
    CHECK_EQ(span_of(m, 1).hi, 6.0);
    CHECK_EQ(span_of(m, 2).lo, -100.0);
    CHECK_EQ(span_of(m, 2).hi, 0.0);
    CHECK_NEAR(std::fabs(export_test::signed_volume(m)), 60000.0, 1e-6);
    const auto grouped = ost_meshing::generate_condition_export(project, kAllPages, solid_config(), recording);
    REQUIRE(grouped.status == ExportStatus::Ok);
    CHECK_EQ(all_meshes(grouped).size(), static_cast<size_t>(1));
    // the negative's own condition keeps an empty group (RB-03), the slab group holds the slab
    REQUIRE_EQ(grouped.groups.size(), static_cast<size_t>(2));
    CHECK_STR_EQ(grouped.groups[0].condition_uid, "negwall");
    CHECK(grouped.groups[0].meshes.empty());
    CHECK_EQ(grouped.groups[1].meshes.size(), static_cast<size_t>(1));
    CHECK_EQ(calls, 0);
}

#if OST_HAVE_BOOLEAN
TEST(ExportRuleManifold, AWallIsCutByACrossingNegativeOfTheSameType)
{
    // EXP-05 same-type control (Windows OBJ, run read-only): wall [0,0,100,0] (Linear, height 96, thickness 6) with the
    // crossing negative [50,-20,50,120] of a second Linear condition: 16 vertices, 24 faces, |volume| 54144 = 57600 - 3456,
    // bbox x -100..0, y 0..96, z -3..3 (the same ranges in the iOS frame).
    const auto html = ost_meshing::generate_html_export(slab_and_wall_project(true), kAllPages, solid_config());
    REQUIRE(html.status == ExportStatus::Ok);
    REQUIRE_EQ(html.meshes.size(), static_cast<size_t>(1));
    const Mesh &m = html.meshes[0].mesh;
    CHECK_EQ(m.vertices.size(), static_cast<size_t>(16));
    CHECK_EQ(m.faces.size(), static_cast<size_t>(24));
    CHECK_NEAR(std::fabs(export_test::signed_volume(m)), 54144.0, 1e-3);
    CHECK_NEAR(span_of(m, 0).lo, -100.0, 1e-6);
    CHECK_NEAR(span_of(m, 0).hi, 0.0, 1e-6);
    CHECK_NEAR(span_of(m, 1).lo, 0.0, 1e-6);
    CHECK_NEAR(span_of(m, 1).hi, 96.0, 1e-6);
    CHECK_NEAR(span_of(m, 2).lo, -3.0, 1e-6);
    CHECK_NEAR(span_of(m, 2).hi, 3.0, 1e-6);
}
#endif

TEST(ExportRule, EveryMeshCarriesTheDisplayColourOfItsCondition)
{
    // EXP-06: the HTML meshes, the OBJ / DXF groups and the 3D view meshes take the grayscale flag and the display-mode
    // opacity (Windows color_service via get_color_for_takeoff); the Original mode takes it from the pattern (5 -> 0.5).
    auto project = rule_project({box_takeoff("p", "count", 0.0)});
    project.conditions["count"].color_fill = 0x336699; // BGR: r 0x99, g 0x66, b 0x33
    project.conditions["count"].pattern = ost_domain::Pattern::ForwardDiag;
    struct Case
    {
        const char *mode;
        bool gray;
        const char *hex;
        float opacity;
    };
    for (const Case &c : {Case{"Solid", false, "#996633", 1.0f}, Case{"Solid", true, "#6f6f6f", 1.0f},
                          Case{"Original", false, "#996633", 0.5f}, Case{"Transparent", true, "#6f6f6f", 0.5f}})
    {
        SCOPED_CONTEXT(std::string(c.mode) + (c.gray ? " gray" : ""));
        ost_domain::RenderConfig config;
        config.color_mode = c.mode;
        config.grayscale_enabled = c.gray;
        const auto html = ost_meshing::generate_html_export(project, kAllPages, config);
        REQUIRE_EQ(html.meshes.size(), static_cast<size_t>(1));
        CHECK_STR_EQ(html.meshes[0].color.hex(), c.hex);
        CHECK_EQ(html.meshes[0].color.opacity, c.opacity);
        const auto grouped = ost_meshing::generate_condition_export(project, kAllPages, config);
        REQUIRE_EQ(grouped.groups.size(), static_cast<size_t>(1));
        CHECK_STR_EQ(grouped.groups[0].color.hex(), c.hex);
        const auto view = ost_meshing::generate_view_meshes(project, kAllPages, config);
        REQUIRE_EQ(view.size(), static_cast<size_t>(1));
        CHECK_STR_EQ(view[0].color.hex(), c.hex);
        CHECK_EQ(view[0].color.opacity, c.opacity);
    }
}

// ---------------------------------------------------------------- grouping (EXP-11)
namespace
{
    // The conditions and takeoffs of gen_oracle2.py (the old mapper scenario plus a nameless condition and a linear takeoff
    // with a single point).
    ost_data::ProjectData grouping_project()
    {
        auto cond = [](const std::string &uid, const std::string &name, ConditionType type, const std::string &cdn, int fill)
        {
            Condition c;
            c.uid = uid;
            c.name = name;
            c.condition_type = type;
            c.cdn_type_name = cdn;
            c.color_fill = fill;
            return c;
        };
        const std::string W = "11111111-aaaa-bbbb-cccc-000000000001", C = "22222222-aaaa-bbbb-cccc-000000000002",
                          P = "33333333-aaaa-bbbb-cccc-000000000003", SL = "44444444-aaaa-bbbb-cccc-000000000004",
                          E = "55555555-eeee-ffff-0000-123456789abc", SH = "66666666-aaaa-bbbb-cccc-000000000006";
        Condition wall = cond(W, "Wall A", ConditionType::Linear, "Walls", 0x3366CC);
        wall.height = 96.0;
        wall.thickness = 6.0;
        Condition column = cond(C, "Column", ConditionType::Count, "Columns", 0x00FF00);
        column.shape = ost_domain::ShapeId::Square;
        column.width = column.depth = 24.0;
        column.height = 96.0;
        Condition pier = cond(P, "Pier 1/2", ConditionType::Count, "Piers", 0x0000FF);
        pier.shape = ost_domain::ShapeId::Circle;
        pier.width = pier.depth = 36.0;
        pier.height = 60.0;
        Condition slab = cond(SL, "Slab", ConditionType::Area, "Slabs", 0x999999);
        slab.height = 6.0;
        slab.thickness = 6.0;
        Condition nameless = cond(E, "", ConditionType::Count, "", 0x112233);
        nameless.shape = ost_domain::ShapeId::Square;
        nameless.width = nameless.depth = 10.0;
        nameless.height = 10.0;
        Condition shortwall = cond(SH, "Short", ConditionType::Linear, "Walls", 0x445566);
        shortwall.height = 96.0;
        shortwall.thickness = 6.0;
        return project_of(
            {wall, column, pier, slab, nameless, shortwall},
            {makeoff("t1", W, "p", {0.0, 0.0, 100.0, 0.0}), makeoff("t2", C, "p", {200.0, 200.0}),
             makeoff("t3", P, "p", {300.0, 300.0}),
             makeoff("t4", SL, "p", {0.0, 0.0, 100.0, 0.0, 100.0, 100.0, 0.0, 100.0}),
             makeoff("t5", SL, "p", {20.0, 20.0, 40.0, 20.0, 40.0, 40.0, 20.0, 40.0}, false, "t4"),
             makeoff("t6", W, "p", {0.0, 50.0, 100.0, 50.0}), makeoff("t7", E, "p", {500.0, 500.0}),
             makeoff("t8", SH, "p", {0.0, 0.0})});
    }

    std::string kd_text(const ost_domain::DisplayColor &c)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.6f %.6f %.6f", ost_geometry::export_text::colour_channel(c.r),
                      ost_geometry::export_text::colour_channel(c.g), ost_geometry::export_text::colour_channel(c.b));
        return buf;
    }

    void check_grouping(bool grayscale, const export_rule_oracle::GroupingScenario &expected)
    {
        ost_domain::RenderConfig config;
        config.color_mode = ost_domain::RenderConfig::MODE_ORIGINAL;
        config.grayscale_enabled = grayscale;
        const auto grouped = ost_meshing::generate_condition_export(grouping_project(), kAllPages, config);
        REQUIRE(grouped.status == ExportStatus::Ok);
        REQUIRE_EQ(grouped.groups.size(), expected.groups.size());
        for (size_t g = 0; g < expected.groups.size(); ++g)
        {
            SCOPED_CONTEXT("group " + std::string(expected.groups[g].name));
            const auto &group = grouped.groups[g];
            CHECK_STR_EQ(group.name, expected.groups[g].name);
            CHECK_STR_EQ(group.material_name, expected.groups[g].material);
            CHECK_STR_EQ(kd_text(group.color), expected.groups[g].kd);
            size_t vertices = 0, faces = 0;
            for (const auto &m : group.meshes)
            {
                vertices += m.mesh.vertices.size();
                faces += m.mesh.faces.size();
            }
            CHECK_EQ(vertices, static_cast<size_t>(expected.groups[g].vertices));
            CHECK_EQ(faces, static_cast<size_t>(expected.groups[g].faces));
        }
        // the DXF: one layer per condition (the layer of the single-point wall has no LINE)
        for (size_t g = 0; g < expected.groups.size(); ++g)
        {
            size_t lines = 0;
            for (const auto &m : grouped.groups[g].meshes)
                lines += feature_lines(m.mesh);
            int expected_lines = 0;
            for (const auto &kv : expected.dxf_line_counts)
                if (kv.first == expected.groups[g].material)
                    expected_lines = kv.second;
            SCOPED_CONTEXT("dxf lines of " + std::string(expected.groups[g].material));
            CHECK_EQ(lines, static_cast<size_t>(expected_lines));
        }
    }
} // namespace

TEST(ExportGroups, OneGroupMaterialAndLayerPerConditionInTheWindowsOrder)
{
    // Port of the EXP-11 plan (Windows OBJ of conditions Wall A, Column, Pier 1/2, Slab, takeoffs t1..t6 with t5 the hole of
    // t4): groups keyed by condition in first-seen order over [non-area takeoffs, area parents]; material and layer name
    // sanitize('<cdn type or Unknown>_<name or Condition_<uid>>_<uid[:8]>'); the first condition of a name wall A holds t1
    // and t6 together (16 vertices), the slab is last although t4 came before t6, a condition without a surviving mesh
    // ('Short', one point) keeps its empty group. Group names, emptiness and counts only (EXP-17: no vertex order).
    check_grouping(false, export_rule_oracle::grouping_color());
}

TEST(ExportGroups, GrayscaleExportColoursAreTheWindowsGrayBytes)
{
    // Same scenario with grayscale on: Windows Kd of the MTL is the truncated gray byte over 255 (EXP-06).
    check_grouping(true, export_rule_oracle::grouping_gray());
}

TEST(ExportGroups, TheMaterialNameTakesTheFirstEightCharactersOfTheConditionUid)
{
    // base_exporter.py: f"{cdn_type}_{condition_name}_{condition_uid[:8]}" slices characters (code points), not bytes;
    // verified on Windows: 'A\xc3\x84\xc3\x96\xc3\x9c12345-xyz' is 'AÄÖÜ1234'.
    Condition c;
    c.uid = "A\xC3\x84\xC3\x96\xC3\x9C" "12345-xyz";
    c.name = "n";
    c.cdn_type_name = "T";
    CHECK_STR_EQ(ost_meshing::export_material_name(c.uid, c),
                 "T_n_A\xC3\x84\xC3\x96\xC3\x9C" "1234");
    // a uid shorter than eight characters is used whole
    c.uid = "ab";
    CHECK_STR_EQ(ost_meshing::export_material_name(c.uid, c), "T_n_ab");
    // an empty name becomes Condition_<uid>, an empty cdn type 'Unknown'
    c.name = "";
    c.cdn_type_name = "";
    CHECK_STR_EQ(ost_meshing::export_condition_name(c.uid, c), "Condition_ab");
    CHECK_STR_EQ(ost_meshing::export_material_name(c.uid, c), "Unknown_Condition_ab_ab");
}

TEST(ExportGroups, TheBooleanPassSeesTheMeshesGroupByGroup)
{
    // _apply_boolean_operations iterates takeoffs_by_group: the meshes reach the pass group by group, not in takeoff order.
    // Conditions A and B of one type, takeoffs [a1, b1, a2] and a negative of A (so every positive goes through the
    // difference): the groups are A (a1, a2, n) then B (b1), so the positives are seen as a1, a2, b1.
    Condition a = box_condition("A", ConditionType::Count), b = box_condition("B", ConditionType::Count);
    auto project = project_of({a, b}, {box_takeoff("a1", "A", 0.0), box_takeoff("b1", "B", 100.0),
                                       box_takeoff("a2", "A", 200.0), box_takeoff("n", "A", 1000.0, true)});
    std::vector<double> seen; // iOS X of the lowest vertex: -x_ost - 10
    const auto grouped = ost_meshing::generate_condition_export(
        project, kAllPages, solid_config(),
        [&](const Mesh &p, const std::vector<Mesh> &) -> std::optional<Mesh>
        {
            seen.push_back(span_of(p, 0).lo);
            return p;
        });
    REQUIRE(grouped.status == ExportStatus::Ok);
    CHECK_EQ(seen, (std::vector<double>{-10.0, -210.0, -110.0}));
    REQUIRE_EQ(grouped.groups.size(), static_cast<size_t>(2));
    CHECK_STR_EQ(grouped.groups[0].condition_uid, "A");
    REQUIRE_EQ(grouped.groups[0].meshes.size(), static_cast<size_t>(2));
    CHECK_STR_EQ(grouped.groups[0].meshes[0].takeoff->uid, "a1");
    CHECK_STR_EQ(grouped.groups[0].meshes[1].takeoff->uid, "a2");
    REQUIRE_EQ(grouped.groups[1].meshes.size(), static_cast<size_t>(1));
    CHECK_STR_EQ(grouped.groups[1].meshes[0].takeoff->uid, "b1");
}

// ---------------------------------------------------------------- winding of what leaves the pipeline (EXP-13)
namespace
{
    // The meshes that leave the three entry points of Core/Meshing/ExportMeshSet.cpp (the 3D view, the HTML scene and every
    // group of the OBJ / DXF export) of a project with Count A (a1 at x 0, a2 at x 500), Attachment B (b1 at x 200) and
    // three scenes: no negative; a negative of another condition type far away; a negative of A at x 10 that cuts a1 (the
    // box 20 x 20 x 10 at x -10..10 loses x 0..10, volume 4000 -> 2000). Decided: iOS keeps every mesh outward-facing, cut or
    // untouched (EXP-13 / EXP-M02), so every mesh that leaves any of the three has a positive signed volume. This is the test
    // that sees a face reversal added AFTER the pass (BooleanRule.Winding_ThePassNeverReversesTheFacesOfAPositive only sees
    // the pass itself, ObjWinding.* only the generators and the difference).
    void check_winding_of_entry_point(const char *entry_point,
                                      const std::vector<const ost_meshing::BooleanMesh *> &meshes, double a1_volume,
                                      bool a1_is_cut)
    {
        SCOPED_CONTEXT(std::string("entry point: ") + entry_point);
        // the three positives and nothing else (a negative is never a mesh of an entry point), so the loop is not vacuous
        std::vector<std::string> uids;
        for (const auto *m : meshes)
            uids.push_back(m->takeoff->uid);
        std::sort(uids.begin(), uids.end());
        CHECK_EQ(uids, (std::vector<std::string>{"a1", "a2", "b1"}));
        for (const auto *m : meshes)
        {
            SCOPED_CONTEXT("takeoff " + m->takeoff->uid);
            const double volume = export_test::signed_volume(m->mesh);
            CHECK(volume > 0.0); // outward-facing
            CHECK_NEAR(volume, m->takeoff->uid == "a1" ? a1_volume : 4000.0, 1e-6);
            if (m->takeoff->uid == "a1" && a1_is_cut)
            {
                // the cut piece (the Replaced path of the pass) is what this scene is for: iOS X = -x, Windows x -10..0
                CHECK_NEAR(span_of(m->mesh, 0).lo, 0.0, 1e-6);
                CHECK_NEAR(span_of(m->mesh, 0).hi, 10.0, 1e-6);
            }
        }
    }

    void check_winding_of_the_pipeline(const BooleanDifferenceFn &difference)
    {
        const auto config = solid_config();
        const Condition a = box_condition("A", ConditionType::Count), b = box_condition("B", ConditionType::Attachment);
        const std::vector<Takeoff> base = {box_takeoff("a1", "A", 0.0), box_takeoff("a2", "A", 500.0),
                                           box_takeoff("b1", "B", 200.0)};
        struct Scene
        {
            const char *name;
            std::vector<Takeoff> takeoffs;
            double a1_volume;
            bool a1_is_cut;
        };
        std::vector<Scene> scenes;
        scenes.push_back({"no negative", base, 4000.0, false});
        auto other_type = base;
        other_type.push_back(box_takeoff("n", "B", 5000.0, true));
        scenes.push_back({"negative of another type far away", other_type, 4000.0, false});
        auto same_type = base;
        same_type.push_back(box_takeoff("n", "A", 10.0, true));
        scenes.push_back({"same-type negative that cuts a1", same_type, 2000.0, true});
        for (const auto &scene : scenes)
        {
            SCOPED_CONTEXT(scene.name);
            const auto project = project_of({a, b}, scene.takeoffs);

            const auto grouped = ost_meshing::generate_condition_export(project, kAllPages, config, difference);
            REQUIRE(grouped.status == ExportStatus::Ok);
            check_winding_of_entry_point("condition export (OBJ / DXF groups)", all_meshes(grouped), scene.a1_volume,
                                         scene.a1_is_cut);

            const auto html = ost_meshing::generate_html_export(project, kAllPages, config, difference);
            REQUIRE(html.status == ExportStatus::Ok);
            std::vector<const ost_meshing::BooleanMesh *> html_meshes;
            for (const auto &m : html.meshes)
                html_meshes.push_back(&m);
            check_winding_of_entry_point("HTML export", html_meshes, scene.a1_volume, scene.a1_is_cut);

            const auto view = ost_meshing::generate_view_meshes(project, kAllPages, config, difference);
            std::vector<const ost_meshing::BooleanMesh *> view_meshes;
            for (const auto &m : view)
                view_meshes.push_back(&m);
            check_winding_of_entry_point("3D view", view_meshes, scene.a1_volume, scene.a1_is_cut);
        }
    }
} // namespace

TEST(ExportRule, Winding_EveryMeshThatLeavesTheViewAndBothExportsIsOutwardFacing)
{
    // The decided EXP-13 rule at the level of the entry points (the stand-in difference cuts the box exactly): a mesh that is
    // never cut, one that goes through the difference untouched (a2, b1) and the cut piece (a1) all have a positive volume.
    check_winding_of_the_pipeline(box_difference_fn());
}

#if OST_HAVE_BOOLEAN
TEST(ExportRuleManifold, Winding_EveryMeshThatLeavesTheViewAndBothExportsIsOutwardFacingWithManifold)
{
    // The same three scenes with the real boolean difference (Manifold).
    check_winding_of_the_pipeline({});
}
#endif

// ---------------------------------------------------------------- mesh order (MSH-20)
namespace
{
    ost_data::ProjectData order_project(const std::vector<Takeoff> &takeoffs)
    {
        auto cond = [](const std::string &uid, ConditionType type)
        {
            Condition c = box_condition(uid, type, 10.0, 10.0);
            if (type == ConditionType::Linear)
            {
                c.height = 96.0;
                c.thickness = 6.0;
            }
            if (type == ConditionType::Area)
            {
                c.height = 6.0;
                c.thickness = 6.0;
            }
            return c;
        };
        return project_of({cond("ar", ConditionType::Area), cond("li", ConditionType::Linear),
                           cond("co", ConditionType::Count), cond("at", ConditionType::Attachment)},
                          takeoffs, {make_page("a"), make_page("b")});
    }

    const std::vector<double> kArea{0.0, 0.0, 50.0, 0.0, 50.0, 50.0, 0.0, 50.0};
    const std::vector<double> kLin{0.0, 0.0, 60.0, 0.0};
    const std::vector<double> kCnt{10.0, 10.0};
} // namespace

TEST(MeshOrder, AreaLinearCountAttachmentByPageInSortedPageOrder)
{
    // Port of the MSH-20 plan: pages [a, b], takeoffs [l2(linear, a), a1(area, b), l1(linear, b), a2(area, b),
    // x1(attachment, b), n1(count, b)]. Windows process_takeoffs_to_meshes (run read-only) gives [a1, a2, l2, l1, n1, x1].
    const auto project = order_project(
        {makeoff("l2", "li", "a", kLin), makeoff("a1", "ar", "b", kArea), makeoff("l1", "li", "b", kLin),
         makeoff("a2", "ar", "b", kArea), makeoff("x1", "at", "b", kCnt), makeoff("n1", "co", "b", kCnt)});
    const auto expected = export_rule_oracle::order_plan_view();
    CHECK_EQ(uids_of(ost_meshing::generate_takeoff_meshes(project, {"a", "b"}, true)), expected);
    CHECK_EQ(uids_of(ost_meshing::generate_view_meshes(project, {"a", "b"}, solid_config())), expected);
}

TEST(MeshOrder, PagesAreCollectedInSortedOrderNotInRowOrder)
{
    // Windows: collect_takeoffs_for_pages(sorted page uids), then group_takeoffs_by_type. The rows are NOT in page order
    // here (b first): Windows collects [l2, a0, n0, x0 | l1, a1, x1, n1] and meshes [a0, a1, l2, l1, n0, n1, x0, x1].
    CHECK_EQ(export_rule_oracle::order_project(),
             (std::vector<std::string>{"l1", "l2", "a1", "a0", "x1", "n1", "n0", "x0"}));
    const auto project = order_project(
        {makeoff("l1", "li", "b", kLin), makeoff("l2", "li", "a", kLin), makeoff("a1", "ar", "b", kArea),
         makeoff("a0", "ar", "a", kArea), makeoff("x1", "at", "b", kCnt), makeoff("n1", "co", "b", kCnt),
         makeoff("n0", "co", "a", kCnt), makeoff("x0", "at", "a", kCnt)});
    CHECK_EQ(uids_of(ost_meshing::generate_takeoff_meshes(project, {"a", "b"}, true)), export_rule_oracle::order_view());
    CHECK_EQ(uids_of(ost_meshing::generate_view_meshes(project, {"a", "b"}, solid_config())), export_rule_oracle::order_view());
    // an empty selection is every page (the export): the same sorted order
    CHECK_EQ(uids_of(ost_meshing::generate_takeoff_meshes(project, {}, false)), export_rule_oracle::order_view());
    // the HTML export keeps the order of the Windows scene geometries (process_meshes_for_threejs: types [1, 0, 2, 3])
    const auto html = ost_meshing::generate_html_export(project, kAllPages, solid_config());
    CHECK_EQ(uids_of(html.meshes), export_rule_oracle::order_html());
}

TEST(MeshOrder, HoleChildrenAndHiddenTakeoffsAreNotMeshedAndKeepTheOrder)
{
    // The hole child is not an element of its own; an area takeoff after a linear one still comes first.
    auto project = order_project({makeoff("l", "li", "a", kLin), makeoff("a", "ar", "a", kArea),
                                  makeoff("h", "ar", "a", {10.0, 10.0, 20.0, 10.0, 20.0, 20.0, 10.0, 20.0}, false, "a")});
    CHECK_EQ(uids_of(ost_meshing::generate_takeoff_meshes(project, {"a"}, true)), (std::vector<std::string>{"a", "l"}));
    // a page that is not selected contributes nothing
    CHECK_EQ(uids_of(ost_meshing::generate_takeoff_meshes(project, {"b"}, true)), std::vector<std::string>{});
}

// ---------------------------------------------------------------- the page transform (3D view only)
TEST(ExportRule, TheViewAppliesThePageTransformAndTheExportsDoNot)
{
    // Windows: the 3D view meshes come from _MeshGeneratorAdapter.generate_meshes (one coordinate system per page with its
    // rotation / flip applied by _apply_page_transform), while OBJExporter / DXFExporter and visualize_with_threejs use the
    // untransformed default coordinate system. Oracle: both run read-only on one page (612 x 792 pt, scale factors 1) with a
    // Count box (Square 20 x 20 x 10) at OST (100, 50) and a Linear wall (height 96, thickness 6) [10,20,110,70], for the
    // rotations / flips below; the table is the view's mesh bbox in the iOS frame (Windows (X, Y, Z-up) -> (X, Z, -Y)):
    // { rotation, flip_x, flip_y, wall {x lo, x hi, z lo, z hi, y lo, y hi}, box {same} }. The untransformed meshes (OBJ,
    // HTML) are the first row (rotation 0, no flip) for every page setting.
    struct Row
    {
        int rotation;
        bool flip_x, flip_y;
        double wall[6];
        double box[6];
    };
    const Row rows[] = {
        {0, false, false, {-111.34164078649988, -8.658359213500127, -72.68328157299975, -17.316718427000254, 0.0, 96.0}, {-110.0, -90.0, -60.0, -40.0, 0.0, 10.0}},
        {90, false, false, {-72.68328157299975, -17.316718427000254, 0.15835921350012605, 102.84164078649988, 0.0, 96.0}, {-60.0, -40.0, 81.5, 101.5, 0.0, 10.0}},
        {180, false, false, {0.15835921350012605, 102.84164078649988, 6.316718427000252, 61.68328157299974, 0.0, 96.0}, {81.5, 101.5, 29.0, 49.0, 0.0, 10.0}},
        {270, false, false, {6.316718427000252, 61.68328157299974, -111.34164078649988, -8.658359213500127, 0.0, 96.0}, {29.0, 49.0, -110.0, -90.0, 0.0, 10.0}},
        {0, true, false, {0.15835921350012605, 102.84164078649988, -72.68328157299975, -17.316718427000254, 0.0, 96.0}, {81.5, 101.5, -60.0, -40.0, 0.0, 10.0}},
        {0, false, true, {-111.34164078649988, -8.658359213500127, 6.316718427000252, 61.68328157299974, 0.0, 96.0}, {-110.0, -90.0, 29.0, 49.0, 0.0, 10.0}},
        {90, true, false, {-72.68328157299975, -17.316718427000254, -111.34164078649988, -8.658359213500127, 0.0, 96.0}, {-60.0, -40.0, -110.0, -90.0, 0.0, 10.0}},
        {270, true, true, {-72.68328157299975, -17.316718427000254, 0.15835921350012605, 102.84164078649988, 0.0, 96.0}, {-60.0, -40.0, 81.5, 101.5, 0.0, 10.0}},
    };
    auto check_box = [](const Mesh &m, const double (&e)[6])
    {
        CHECK_NEAR(span_of(m, 0).lo, e[0], 1e-9);
        CHECK_NEAR(span_of(m, 0).hi, e[1], 1e-9);
        CHECK_NEAR(span_of(m, 2).lo, e[2], 1e-9);
        CHECK_NEAR(span_of(m, 2).hi, e[3], 1e-9);
        CHECK_NEAR(span_of(m, 1).lo, e[4], 1e-9);
        CHECK_NEAR(span_of(m, 1).hi, e[5], 1e-9);
    };
    const auto config = solid_config();
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("rotation " + std::to_string(row.rotation) + (row.flip_x ? " flip_x" : "") + (row.flip_y ? " flip_y" : ""));
        Condition box = box_condition("box", ConditionType::Count);
        Condition wall = box_condition("wall", ConditionType::Linear);
        wall.height = 96.0;
        wall.thickness = 6.0;
        box.color_fill = 0x336699;  // BGR: #996633
        wall.color_fill = 0x00FF00; // #00ff00 (every mesh carries the colour of ITS condition)
        ost_domain::Page page = make_page("p");
        page.rotation = row.rotation;
        page.flip_x = row.flip_x;
        page.flip_y = row.flip_y;
        const auto project = project_of({box, wall},
                                        {makeoff("b", "box", "p", {100.0, 50.0}),
                                         makeoff("w", "wall", "p", {10.0, 20.0, 110.0, 70.0})},
                                        {page});
        // the 3D view: the page transform, linear before count
        const auto view = ost_meshing::generate_view_meshes(project, kAllPages, config);
        REQUIRE_EQ(view.size(), static_cast<size_t>(2));
        CHECK_STR_EQ(view[0].takeoff->uid, "w");
        CHECK_STR_EQ(view[1].takeoff->uid, "b");
        check_box(view[0].mesh, row.wall);
        check_box(view[1].mesh, row.box);
        CHECK_NEAR(std::fabs(export_test::signed_volume(view[1].mesh)), 4000.0, 1e-6);
        CHECK_STR_EQ(view[0].color.hex(), "#00ff00");
        CHECK_STR_EQ(view[1].color.hex(), "#996633");
        // the exports: never transformed (the first row)
        const auto html = ost_meshing::generate_html_export(project, kAllPages, config);
        REQUIRE_EQ(html.meshes.size(), static_cast<size_t>(2));
        check_box(html.meshes[0].mesh, rows[0].wall);
        check_box(html.meshes[1].mesh, rows[0].box);
        CHECK_STR_EQ(html.meshes[0].color.hex(), "#00ff00");
        CHECK_STR_EQ(html.meshes[1].color.hex(), "#996633");
        const auto grouped = ost_meshing::generate_condition_export(project, kAllPages, config);
        REQUIRE_EQ(grouped.groups.size(), static_cast<size_t>(2));
        REQUIRE_EQ(grouped.groups[0].meshes.size(), static_cast<size_t>(1));
        REQUIRE_EQ(grouped.groups[1].meshes.size(), static_cast<size_t>(1));
        check_box(grouped.groups[0].meshes[0].mesh, rows[0].box); // groups: non-area takeoffs in takeoff order (box first)
        check_box(grouped.groups[1].meshes[0].mesh, rows[0].wall);
        CHECK_STR_EQ(grouped.groups[0].color.hex(), "#996633");
        CHECK_STR_EQ(grouped.groups[1].color.hex(), "#00ff00");
        CHECK_STR_EQ(grouped.groups[0].meshes[0].color.hex(), "#996633");
        CHECK_STR_EQ(grouped.groups[1].meshes[0].color.hex(), "#00ff00");
    }
}

// ---------------------------------------------------------------- the writers fed by the export set
TEST(ExportGroups, ObjAndMtlOfTheConsumedScenarioHaveTheWindowsGroups)
{
    // Windows OBJ text of the consumed scenario (item RB-03, run read-only; the vertex and face lines are not compared,
    // EXP-17): '# Created by OST Visualizer\nmtllib model.mtl\n\n# count\ng Unknown_count_count\nusemtl Unknown_count_count\n'
    // + 8 'v' + 12 'f' + '\n# count_block\ng Unknown_count_block_count_bl\nusemtl Unknown_count_block_count_bl\n\n'; MTL
    // '# Display Mode: Solid', '# Materials: 2' with both newmtl.
    const auto grouped = ost_meshing::generate_condition_export(rule_project(consumed_positive_takeoffs()), kAllPages,
                                                                solid_config(), box_difference_fn());
    REQUIRE(grouped.status == ExportStatus::Ok);
    export_test::ScratchDir dir("rule_consumed");
    ost_geometry::OBJWriter writer;
    writer.set_display_mode("Solid");
    for (const auto &g : grouped.groups)
        writer.add_group(g.material_name, g.name, g.color.r, g.color.g, g.color.b, g.mesh_pointers());
    REQUIRE(writer.save(dir.file("model.obj")));
    const std::string obj = export_test::read_text_lf(dir.file("model.obj"));
    std::string without_geometry;
    int vertices = 0, faces = 0;
    size_t pos = 0;
    while (pos < obj.size())
    {
        size_t end = obj.find('\n', pos);
        const std::string line = obj.substr(pos, end - pos);
        pos = end + 1;
        if (line.rfind("v ", 0) == 0)
            ++vertices;
        else if (line.rfind("f ", 0) == 0)
            ++faces;
        else
            without_geometry += line + "\n";
    }
    CHECK_EQ(vertices, 8);
    CHECK_EQ(faces, 12);
    CHECK_STR_EQ(without_geometry,
                 "# Created by OST Visualizer\nmtllib model.mtl\n\n"
                 "# count\ng Unknown_count_count\nusemtl Unknown_count_count\n\n"
                 "# count_block\ng Unknown_count_block_count_bl\nusemtl Unknown_count_block_count_bl\n\n");
    const std::string mtl = export_test::read_text_lf(dir.file("model.mtl"));
    CHECK(mtl.rfind("# Created by OST Visualizer\n# Display Mode: Solid\n# Materials: 2\n\nnewmtl Unknown_count_count\n# count\n", 0) == 0);
    CHECK(mtl.find("newmtl Unknown_count_block_count_bl\n# count_block\n") != std::string::npos);
}

TEST(ExportGroups, AGroupOfSeveralMeshesKeepsTheVertexIndicesOfEveryMeshAndGroup)
{
    // Windows OBJExporter._write_obj_file with OBJMeshWriter.write_mesh: the 1-based vertex index runs on over every mesh of
    // every group (reset once per file), so the second mesh of a group and the groups after it point at their own
    // vertices; an empty group (RB-03) writes no vertex and does not move the index. The iOS writer puts all the vertices
    // of a group before its faces (EXP-17: no line-for-line match), so every face is resolved to its vertex coordinates.
    const Mesh a = make_box(0, 0, 0, 1, 1, 1), b = make_box(10, 0, 0, 12, 2, 2), c = make_box(20, 0, 0, 23, 3, 3);
    export_test::ScratchDir dir("rule_indices");
    ost_geometry::OBJWriter writer;
    writer.add_group("G1", "one", 1.0f, 0.0f, 0.0f, {&a, &b});
    writer.add_group("G2", "two", 0.0f, 1.0f, 0.0f, {});
    writer.add_group("G3", "three", 0.0f, 0.0f, 1.0f, {&c});
    REQUIRE(writer.save(dir.file("model.obj")));
    const std::string obj = export_test::read_text_lf(dir.file("model.obj"));

    auto key = [](const std::array<double, 3> &v)
    {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.6f %.6f %.6f", v[0], v[1], v[2]);
        return std::string(buf);
    };
    std::vector<std::string> vertices;
    std::vector<std::vector<std::string>> faces_of_group; // each face as the three resolved vertex keys
    std::vector<std::string> group_names;
    size_t pos = 0;
    while (pos < obj.size())
    {
        size_t end = obj.find('\n', pos);
        const std::string line = obj.substr(pos, end - pos);
        pos = end + 1;
        if (line.rfind("g ", 0) == 0)
        {
            group_names.push_back(line.substr(2));
            faces_of_group.emplace_back();
        }
        else if (line.rfind("v ", 0) == 0)
        {
            vertices.push_back(line.substr(2));
        }
        else if (line.rfind("f ", 0) == 0)
        {
            int i = 0, j = 0, k = 0;
            REQUIRE_EQ(std::sscanf(line.c_str(), "f %d %d %d", &i, &j, &k), 3);
            REQUIRE(i >= 1 && j >= 1 && k >= 1);
            REQUIRE(static_cast<size_t>(std::max({i, j, k})) <= vertices.size());
            faces_of_group.back().push_back(vertices[static_cast<size_t>(i - 1)] + "|" + vertices[static_cast<size_t>(j - 1)] + "|" +
                                            vertices[static_cast<size_t>(k - 1)]);
        }
    }
    CHECK_EQ(vertices.size(), static_cast<size_t>(24));
    REQUIRE_EQ(group_names.size(), static_cast<size_t>(3));
    CHECK_STR_EQ(group_names[0], "G1");
    CHECK_STR_EQ(group_names[1], "G2");
    CHECK_STR_EQ(group_names[2], "G3");
    auto expected = [&](const std::vector<const Mesh *> &meshes)
    {
        std::vector<std::string> out;
        for (const Mesh *m : meshes)
            for (const auto &f : m->faces)
                out.push_back(key(m->vertices[f[0]]) + "|" + key(m->vertices[f[1]]) + "|" + key(m->vertices[f[2]]));
        return out;
    };
    CHECK(faces_of_group[0] == expected({&a, &b}));
    CHECK(faces_of_group[1].empty());
    CHECK(faces_of_group[2] == expected({&c}));
    CHECK_EQ(faces_of_group[0].size(), static_cast<size_t>(24));
}

TEST(ExportGroups, TheDxfHasALayerForEveryConditionAndLinesOnlyWhereMeshesSurvive)
{
    // Windows DXFExporter: add_layer for every condition of materials_info, add_line only for the cached meshes: 12 LINEs, all
    // on the 'count' layer; the 'count_block' layer exists (RB-03, consumed scenario).
    const auto grouped = ost_meshing::generate_condition_export(rule_project(consumed_positive_takeoffs()), kAllPages,
                                                                solid_config(), box_difference_fn());
    REQUIRE(grouped.status == ExportStatus::Ok);
    ost_geometry::DXFWriter writer;
    for (const auto &g : grouped.groups)
    {
        writer.add_layer(g.material_name, g.color.hex());
        for (const auto &m : g.meshes)
        {
            Mesh clean = m.mesh;
            clean.edges = ost_geometry::extract_feature_edges(clean, 0.1);
            writer.add_mesh_as_lines(clean, g.material_name);
        }
    }
    CHECK_EQ(writer.get_line_count(), static_cast<size_t>(12));
    export_test::ScratchDir dir("rule_dxf");
    REQUIRE(writer.save(dir.file("model.dxf")));
    const std::string dxf = export_test::read_text_lf(dir.file("model.dxf"));
    CHECK(dxf.find("\nUnknown_count_count\n") != std::string::npos);
    CHECK(dxf.find("\nUnknown_count_block_count_bl\n") != std::string::npos);
}

// ---------------------------------------------------------------- EXP-12 control characters in the names (Windows b9902e6)
namespace
{
    // The (group code, value) pairs of a DXF text (read_text_lf): two lines each, the code right-aligned in three columns.
    // `well_formed` is false when the number of lines is odd or a code line is not a plain number, which is what a name
    // that injected a line would break.
    struct DxfPairs
    {
        bool well_formed = true;
        std::vector<std::pair<std::string, std::string>> pairs;
    };

    std::vector<std::string> split_text_lines(const std::string &text)
    {
        std::vector<std::string> out;
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos)
                end = text.size();
            out.push_back(text.substr(pos, end - pos));
            pos = end + 1;
        }
        return out;
    }

    DxfPairs dxf_pairs(const std::string &text)
    {
        DxfPairs out;
        const std::vector<std::string> rows = split_text_lines(text);
        if (rows.size() % 2 != 0)
            out.well_formed = false;
        for (size_t i = 0; i + 1 < rows.size(); i += 2)
        {
            const size_t first = rows[i].find_first_not_of(' ');
            const std::string code = first == std::string::npos ? std::string() : rows[i].substr(first);
            if (code.empty() || code.find_first_not_of("0123456789") != std::string::npos)
                out.well_formed = false;
            out.pairs.emplace_back(code, rows[i + 1]);
        }
        return out;
    }

    // The rest of every line that starts with `prefix`.
    std::vector<std::string> values_after_prefix(const std::vector<std::string> &lines, const std::string &prefix)
    {
        std::vector<std::string> out;
        for (const auto &line : lines)
            if (line.compare(0, prefix.size(), prefix) == 0)
                out.push_back(line.substr(prefix.size()));
        return out;
    }

    // Every line is empty or starts with one of the statements the writer produces.
    bool only_known_statements(const std::vector<std::string> &lines, const std::vector<std::string> &prefixes)
    {
        for (const auto &line : lines)
        {
            bool known = line.empty();
            for (const auto &p : prefixes)
                known = known || line.compare(0, p.size(), p) == 0;
            if (!known)
                return false;
        }
        return true;
    }
} // namespace

TEST(ExportGroups, Exp12_ControlCharactersOfAConditionNameBecomeUnderscoresInTheMaterialName)
{
    // base_exporter.py sanitizes f"{cdn_type or 'Unknown'}_{condition_name}_{condition_uid[:8]}" as a whole, and since
    // b9902e6 that turns every U+0000..U+001F and U+007F into '_' (iOS dropped them before).
    Condition c;
    c.uid = "abcd1234";
    c.name = "Wall\nusemtl Evil";
    CHECK_STR_EQ(ost_meshing::export_material_name(c.uid, c), "Unknown_Wall_usemtl_Evil_abcd1234");
    // the '# <name>' comment of the OBJ / MTL keeps the raw name (the writer turns CR and LF into a space, EXP-16)
    CHECK_STR_EQ(ost_meshing::export_condition_name(c.uid, c), "Wall\nusemtl Evil");
    // a control character in the cdn type or in the first eight characters of the uid is replaced the same way
    c.cdn_type_name = "Ty\x01" "pe";
    c.uid = "ab\tcdefghij";
    c.name = "";
    CHECK_STR_EQ(ost_meshing::export_material_name(c.uid, c), "Ty_pe_Condition_ab_cdefghij_ab_cdefg");
}

TEST(ExportGroups, Exp12_ALineBreakOrNulInAConditionNameInjectsNoLineIntoTheObjMtlOrDxf)
{
    // A condition named 'Wall\r\nusemtl Evil\rnewmtl Bad\nv 9 9 9' + NUL + 'x' goes through the export set, the OBJ / MTL writer
    // and the DXF writer exactly as Services/ExportService.mm does it (OBJ: add_group(material_name, name, ...); DXF:
    // add_layer(material_name) and the lines of the meshes on that layer). The name is one material / layer name with an
    // underscore for every control character; no statement of the OBJ, MTL or DXF comes from the name.
    std::string name = "Wall\r\nusemtl Evil\rnewmtl Bad\nv 9 9 9";
    name.push_back('\0');
    name += "x";
    Condition c = box_condition("abcd1234", ConditionType::Count);
    c.name = name;
    const auto grouped = ost_meshing::generate_condition_export(project_of({c}, {box_takeoff("t1", "abcd1234", 0.0)}), kAllPages,
                                                                solid_config(), box_difference_fn());
    REQUIRE(grouped.status == ExportStatus::Ok);
    REQUIRE_EQ(grouped.groups.size(), static_cast<size_t>(1));
    const auto &group = grouped.groups[0];
    const std::string material = "Unknown_Wall__usemtl_Evil_newmtl_Bad_v_9_9_9_x_abcd1234";
    CHECK_STR_EQ(group.material_name, material);
    CHECK_STR_EQ(group.name, name); // the comment text is the raw name

    export_test::ScratchDir dir("exp12_injection");

    // OBJ / MTL: one 'g', one 'usemtl', one 'newmtl' and one 'Kd' line, every line a statement the writer produces
    ost_geometry::OBJWriter obj_writer;
    obj_writer.set_display_mode("solid");
    obj_writer.add_group(group.material_name, group.name, group.color.r, group.color.g, group.color.b, group.mesh_pointers());
    REQUIRE(obj_writer.save(dir.file("model.obj")));
    const std::string obj = export_test::read_text_lf(dir.file("model.obj"));
    const std::string mtl = export_test::read_text_lf(dir.file("model.mtl"));
    const auto obj_lines = split_text_lines(obj);
    const auto mtl_lines = split_text_lines(mtl);
    CHECK_EQ(values_after_prefix(obj_lines, "g "), (std::vector<std::string>{material}));
    CHECK_EQ(values_after_prefix(obj_lines, "usemtl "), (std::vector<std::string>{material}));
    CHECK_EQ(values_after_prefix(mtl_lines, "newmtl "), (std::vector<std::string>{material}));
    CHECK_EQ(values_after_prefix(mtl_lines, "Kd ").size(), static_cast<size_t>(1));
    CHECK_EQ(values_after_prefix(obj_lines, "v ").size(), static_cast<size_t>(8)); // the 8 corners of the box, not '9 9 9'
    CHECK_EQ(values_after_prefix(obj_lines, "f ").size(), static_cast<size_t>(12));
    CHECK(only_known_statements(obj_lines, {"# ", "mtllib ", "g ", "usemtl ", "v ", "f "}));
    CHECK(only_known_statements(mtl_lines, {"# ", "newmtl ", "Kd ", "Ka ", "Ks ", "Ns ", "Ni ", "d ", "illum "}));
    CHECK(obj.find('\r') == std::string::npos);
    CHECK(mtl.find('\r') == std::string::npos);
    // the comment line holds the name with CR and LF as spaces, on ONE line
    REQUIRE(obj_lines.size() > 3);
    CHECK(obj_lines[3].rfind("# Wall  usemtl Evil newmtl Bad v 9 9 9", 0) == 0);

    // DXF: one LAYER record named like the material (after the default layer 0), the LINEs on that layer, no injected group
    ost_geometry::DXFWriter dxf_writer;
    dxf_writer.add_layer(group.material_name, group.color.hex());
    for (const auto &m : group.meshes)
    {
        Mesh clean = m.mesh;
        clean.edges = ost_geometry::extract_feature_edges(clean, 0.1);
        dxf_writer.add_mesh_as_lines(clean, group.material_name);
    }
    REQUIRE(dxf_writer.get_line_count() > 0);
    REQUIRE(dxf_writer.save(dir.file("model.dxf")));
    const DxfPairs dxf = dxf_pairs(export_test::read_text_lf(dir.file("model.dxf")));
    CHECK(dxf.well_formed);
    std::vector<std::string> layer_names, line_layers;
    std::string entity;
    for (const auto &p : dxf.pairs)
    {
        if (p.first == "0")
            entity = p.second;
        else if (entity == "LAYER" && p.first == "2")
            layer_names.push_back(p.second);
        else if (entity == "LINE" && p.first == "8")
            line_layers.push_back(p.second);
        // no value of the file reads as a statement of the OBJ / MTL ('usemtl Evil'), and none carries a control character
        CHECK(p.second.rfind("usemtl ", 0) != 0);
        CHECK(p.second.rfind("newmtl ", 0) != 0);
        for (unsigned char ch : p.second)
            CHECK(ch >= 0x20 && ch != 0x7F);
    }
    CHECK_EQ(layer_names, (std::vector<std::string>{"0", material})); // the default layer 0 and the condition layer
    CHECK_EQ(line_layers.size(), dxf_writer.get_line_count());
    for (const auto &l : line_layers)
        CHECK_STR_EQ(l, material);
}
