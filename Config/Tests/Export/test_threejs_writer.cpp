// Three.js HTML writer (Core/Export/threejs_writer.*): scene frame and camera (EXP-01), escaping (EXP-08), the colour
// management and lighting of the viewer (EXP-19), number format and opacity of the scene JSON (EXP-20). Oracle: the
// Windows adapter / renderer run read-only (ExportFileOracleData.hpp) and the strings the Windows tests assert.
#include "ExportTestSupport.hpp"
#include "ExportFileOracleData.hpp"
#include "Export/threejs_writer.hpp"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace export_test;

namespace
{
    // Writes the page of a writer and returns its text (CRLF -> LF).
    std::string save_page(const ScratchDir &dir, ost_geometry::ThreeJSWriter &w)
    {
        const std::string path = dir.file("page.html");
        if (!w.save(path))
            return "<save failed>";
        return read_text_lf(path);
    }

    void add_scenario_meshes(ost_geometry::ThreeJSWriter &w, const export_oracle::SceneScenario &sc)
    {
        for (const auto &m : sc.meshes)
        {
            float r = 0, g = 0, b = 0;
            colour_floats(m.r, m.g, m.b, r, g, b);
            w.add_mesh(make_mesh(m.vertices, m.faces), m.name, r, g, b, static_cast<float>(m.opacity));
        }
    }

    Mesh triangle_ios(double x0, double y0, double z0, double x1, double y1, double z1, double x2, double y2, double z2)
    {
        return make_mesh({{x0, y0, z0}, {x1, y1, z1}, {x2, y2, z2}}, {{0, 1, 2}});
    }

    bool parse_scene(const std::string &html, Json &scene)
    {
        std::string error;
        if (!parse_json(scene_data_block(html), scene, error))
        {
            ost_test::report_failure(__FILE__, __LINE__, "scene-data block is not JSON: " + error);
            return false;
        }
        return true;
    }

    std::vector<double> camera_numbers(const Json &scene, const char *key)
    {
        const Json *cam = scene.get("camera");
        const Json *v = cam ? cam->get(key) : nullptr;
        return v ? v->numbers() : std::vector<double>();
    }
} // namespace

// ---------------------------------------------------------------- EXP-01 frame and camera
TEST(ThreeJsFrame, VerticesAndNormalsAreEmittedInTheMeshSpaceWithoutAnotherSwap)
{
    // Port of tests/presentation/visualization/renderers/threejs/adapters/test_threejs_mesh_adapter.py::
    // test_threejs_adapter_exports_layer_condition_and_area_metadata: the Windows Z-up triangle (0,0,0),(1,0,0),(0,1,0)
    // reaches the viewer as vertices [0,0,0, 1,0,0, 0,0,-1], normals [0,1,0] x 3, indices [0,1,2], colour [1,0,0]. That
    // is the iOS mesh space (iOS (X, Y, Z) = Windows (x, z, -y)), so the writer must not swap again: it used to emit the
    // third vertex as (0, -1, -0) and the normals sideways.
    ScratchDir dir("three_frame");
    ost_geometry::ThreeJSWriter w;
    float r = 0, g = 0, b = 0;
    colour_floats(255, 0, 0, r, g, b);
    w.add_mesh(triangle_ios(0, 0, 0, 1, 0, 0, 0, 0, -1), "Condition", r, g, b);
    const std::string html = save_page(dir, w);
    Json scene;
    REQUIRE(parse_scene(html, scene));
    const Json *geoms = scene.get("geometries");
    REQUIRE(geoms != nullptr);
    REQUIRE_EQ(geoms->items.size(), static_cast<size_t>(1));
    const Json &geo = geoms->items[0];
    const std::vector<double> expected_vertices = {0, 0, 0, 1, 0, 0, 0, 0, -1};
    REQUIRE(geo.get("vertices") != nullptr);
    const std::vector<double> vertices = geo.get("vertices")->numbers();
    REQUIRE_EQ(vertices.size(), expected_vertices.size());
    for (size_t i = 0; i < expected_vertices.size(); ++i)
        CHECK_EQ(vertices[i], expected_vertices[i]);
    const std::vector<double> expected_normals = {0, 1, 0, 0, 1, 0, 0, 1, 0};
    REQUIRE(geo.get("normals") != nullptr);
    const std::vector<double> normals = geo.get("normals")->numbers();
    REQUIRE_EQ(normals.size(), expected_normals.size());
    for (size_t i = 0; i < expected_normals.size(); ++i)
        CHECK_NEAR(normals[i], expected_normals[i], 1e-15);
    REQUIRE(geo.get("indices") != nullptr);
    const std::vector<double> indices = geo.get("indices")->numbers();
    REQUIRE_EQ(indices.size(), static_cast<size_t>(3));
    CHECK_EQ(indices[0], 0.0);
    CHECK_EQ(indices[1], 1.0);
    CHECK_EQ(indices[2], 2.0);
    REQUIRE(geo.get("color") != nullptr);
    const std::vector<double> colour = geo.get("color")->numbers();
    REQUIRE_EQ(colour.size(), static_cast<size_t>(3));
    CHECK_EQ(colour[0], 1.0);
    CHECK_EQ(colour[1], 0.0);
    CHECK_EQ(colour[2], 0.0);
    REQUIRE(geo.get("name") != nullptr);
    CHECK_STR_EQ(geo.get("name")->text, "Condition");
}

TEST(ThreeJsFrame, CameraFramesTheMeshSpaceBounds)
{
    // Port of test_threejs_adapter_frames_camera_and_bounds_in_y_up_space in the iOS mesh space: bounds x 10..30,
    // y(height) 1..5, z -6..4 give position [20, 33, -21] and target [20, 3, -1] (centre 20, 3, -1; size 20;
    // position = [cx, cy + 1.5 size, cz - size]). The camera used to be framed from the swapped bounds (target y = -30).
    ScratchDir dir("three_camera");
    ost_geometry::ThreeJSWriter w;
    w.add_mesh(triangle_ios(10, 1, -6, 30, 5, 4, 20, 3, -1), "m", 0.5f, 0.5f, 0.5f);
    Json scene;
    REQUIRE(parse_scene(save_page(dir, w), scene));
    const auto pos = camera_numbers(scene, "position");
    const auto target = camera_numbers(scene, "target");
    REQUIRE_EQ(pos.size(), static_cast<size_t>(3));
    REQUIRE_EQ(target.size(), static_cast<size_t>(3));
    CHECK_EQ(pos[0], 20.0);
    CHECK_EQ(pos[1], 33.0);
    CHECK_EQ(pos[2], -21.0);
    CHECK_EQ(target[0], 20.0);
    CHECK_EQ(target[1], 3.0);
    CHECK_EQ(target[2], -1.0);
}

TEST(ThreeJsFrame, CameraSizeIsAtLeastOne)
{
    // size = max(extents, 1.0): a model of half a unit is framed as 1.0, position [0.25, 1.75, -1.25].
    ScratchDir dir("three_camera_small");
    ost_geometry::ThreeJSWriter w;
    w.add_mesh(triangle_ios(0, 0, 0, 0.5, 0.5, -0.5, 0.25, 0.25, -0.25), "m", 0.5f, 0.5f, 0.5f);
    Json scene;
    REQUIRE(parse_scene(save_page(dir, w), scene));
    const auto pos = camera_numbers(scene, "position");
    const auto target = camera_numbers(scene, "target");
    REQUIRE_EQ(pos.size(), static_cast<size_t>(3));
    REQUIRE_EQ(target.size(), static_cast<size_t>(3));
    CHECK_EQ(pos[0], 0.25);
    CHECK_EQ(pos[1], 1.75);
    CHECK_EQ(pos[2], -1.25);
    CHECK_EQ(target[0], 0.25);
    CHECK_EQ(target[1], 0.25);
    CHECK_EQ(target[2], -0.25);
}

TEST(ThreeJsFrame, WallOfThePlanIsUpright)
{
    // Wall [0,0,100,0] height 96 thickness 6 in the iOS mesh space (the harness result of the plan: exported y-range
    // -3..3, z-range -96..0 and camera target y -30 before the fix): x -100..0, y(height) 0..96, z -3..3.
    ScratchDir dir("three_wall");
    ost_geometry::ThreeJSWriter w;
    const Mesh box = make_mesh({{-0.0, 0, 3}, {0.0, 0, -3}, {-100, 0, -3}, {-100, 0, 3}, {-0.0, 96, 3}, {0.0, 96, -3}, {-100, 96, -3}, {-100, 96, 3}},
                               {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 7, 3}, {0, 4, 7}, {1, 4, 0}, {1, 5, 4}, {2, 5, 1}, {2, 6, 5}, {3, 6, 2}, {3, 7, 6}});
    w.add_mesh(box, "Wall_0", 0.5f, 0.5f, 0.5f);
    Json scene;
    REQUIRE(parse_scene(save_page(dir, w), scene));
    const auto target = camera_numbers(scene, "target");
    const auto pos = camera_numbers(scene, "position");
    REQUIRE_EQ(target.size(), static_cast<size_t>(3));
    CHECK_EQ(target[0], -50.0);
    CHECK_EQ(target[1], 48.0);
    CHECK_EQ(target[2], 0.0);
    REQUIRE_EQ(pos.size(), static_cast<size_t>(3));
    CHECK_EQ(pos[0], -50.0);
    CHECK_EQ(pos[1], 48.0 + 100.0 * 1.5);
    CHECK_EQ(pos[2], -100.0);
    const Json *verts = scene.get("geometries")->items[0].get("vertices");
    double ymin = 1e300, ymax = -1e300, zmin = 1e300, zmax = -1e300;
    const std::vector<double> v = verts->numbers();
    for (size_t i = 0; i + 2 < v.size(); i += 3)
    {
        ymin = std::min(ymin, v[i + 1]);
        ymax = std::max(ymax, v[i + 1]);
        zmin = std::min(zmin, v[i + 2]);
        zmax = std::max(zmax, v[i + 2]);
    }
    CHECK_EQ(ymin, 0.0);
    CHECK_EQ(ymax, 96.0);
    CHECK_EQ(zmin, -3.0);
    CHECK_EQ(zmax, 3.0);
}

// ---------------------------------------------------------------- EXP-20 scene JSON text
TEST(ThreeJsScene, SceneJsonEqualsTheWindowsJsonDumpsText)
{
    // The scene of a Windows run (adapter vertices / indices / colour / opacity / name, the camera of build_scene_data,
    // json.dumps(separators=(',', ':')), '<' escaped) for the same meshes: full-precision numbers (colour
    // 0.5019607843137255, opacity 0.5 / 0.35, 0.7000000000000001), key order geometries, camera / vertices, normals,
    // indices, color, opacity, name. The normals are the iOS ones (EXP-20: no crease-split normals); in the one scenario
    // whose normals are irrational they are compared to 1e-15 instead of byte for byte.
    size_t checked = 0;
    for (const auto &sc : export_oracle::kSceneScenarios)
    {
        SCOPED_CONTEXT(sc.name);
        ScratchDir dir(std::string("three_scene_") + sc.name);
        ost_geometry::ThreeJSWriter w;
        add_scenario_meshes(w, sc);
        const std::string html = save_page(dir, w);
        const std::string block = scene_data_block(html);
        if (std::string(sc.name) != "box_nine_digit_vertices")
        {
            CHECK_STR_EQ(block, sc.json);
        }
        else
        {
            Json got, want;
            std::string error;
            REQUIRE(parse_json(block, got, error));
            REQUIRE(parse_json(sc.json, want, error));
            const Json &g = got.get("geometries")->items[0];
            const Json &e = want.get("geometries")->items[0];
            CHECK(g.get("vertices")->numbers() == e.get("vertices")->numbers());
            CHECK(g.get("indices")->numbers() == e.get("indices")->numbers());
            CHECK(g.get("color")->numbers() == e.get("color")->numbers());
            CHECK_EQ(g.get("opacity")->number, e.get("opacity")->number);
            CHECK_STR_EQ(g.get("name")->text, e.get("name")->text);
            CHECK(camera_numbers(got, "position") == camera_numbers(want, "position"));
            CHECK(camera_numbers(got, "target") == camera_numbers(want, "target"));
            const auto gn = g.get("normals")->numbers(), en = e.get("normals")->numbers();
            REQUIRE_EQ(gn.size(), en.size());
            for (size_t i = 0; i < gn.size(); ++i)
                CHECK_NEAR(gn[i], en[i], 1e-15);
        }
        ++checked;
    }
    CHECK_EQ(checked, static_cast<size_t>(4));
}

TEST(ThreeJsScene, ColourAndOpacityAreFullPrecisionNumbers)
{
    // A triangle at the 0x80 grey writes the colour 0.5019607843137255 (not 0.5020), opacity 0.5 round-trips, and the
    // opacity 0.35f is the number 0.35.
    ScratchDir dir("three_numbers");
    ost_geometry::ThreeJSWriter w;
    float r = 0, g = 0, b = 0;
    colour_floats(128, 128, 128, r, g, b);
    w.add_mesh(triangle_ios(0, 0, 0, 1, 0, 0, 0, 0, -1), "grey", r, g, b, 0.5f);
    w.add_mesh(triangle_ios(0, 0, 0, 1, 0, 0, 0, 0, -1), "decimal", r, g, b, 0.35f);
    const std::string html = save_page(dir, w);
    CHECK(html.find("\"color\":[0.5019607843137255,0.5019607843137255,0.5019607843137255],\"opacity\":0.5,") != std::string::npos);
    CHECK(html.find("\"color\":[0.5019607843137255,0.5019607843137255,0.5019607843137255],\"opacity\":0.35,") != std::string::npos);
    Json scene;
    REQUIRE(parse_scene(html, scene));
    const auto &geoms = scene.get("geometries")->items;
    REQUIRE_EQ(geoms.size(), static_cast<size_t>(2));
    CHECK_EQ(geoms[0].get("opacity")->number, 0.5);
    CHECK_EQ(geoms[1].get("opacity")->number, 0.35);
    const std::vector<double> colour = geoms[0].get("color")->numbers();
    REQUIRE_EQ(colour.size(), static_cast<size_t>(3));
    CHECK_EQ(colour[0], 128.0 / 255.0);
}

TEST(ThreeJsScene, JsonIsCompactLikeJsonDumpsSeparators)
{
    ScratchDir dir("three_compact");
    ost_geometry::ThreeJSWriter w;
    w.add_mesh(triangle_ios(0, 0, 0, 1, 0, 0, 0, 0, -1), "t", 1.0f, 0.0f, 0.0f);
    const std::string block = scene_data_block(save_page(dir, w));
    CHECK(block.find('\n') == std::string::npos);
    CHECK(block.find(", ") == std::string::npos);
    CHECK(block.find(": ") == std::string::npos);
    CHECK(block.rfind("{\"geometries\":[{\"vertices\":[", 0) == 0);
}

// ---------------------------------------------------------------- EXP-08 escaping
TEST(ThreeJsEscape, TitleAndScriptTerminatorsAreEscaped)
{
    // Port of test_threejs_renderer.py::test_html_generation_escapes_title_and_script_terminators.
    const std::string injected = "</script><script>window.injected=true</script>";
    ScratchDir dir("three_escape_title");
    ost_geometry::ThreeJSWriter w;
    w.set_title("Bid & " + injected);
    w.add_mesh(triangle_ios(0, 0, 0, 1, 0, 0, 0, 0, -1), "m", 1.0f, 0.0f, 0.0f);
    const std::string html = save_page(dir, w);
    CHECK(html.find("<title>Bid &amp; &lt;/script&gt;&lt;script&gt;window.injected=true&lt;/script&gt;</title>") != std::string::npos);
    CHECK(html.find(injected) == std::string::npos);
    Json scene;
    CHECK(parse_scene(html, scene)); // the scene-data block is valid JSON
}

TEST(ThreeJsEscape, TemplatePlaceholdersInUserTextAreNotExpanded)
{
    // Port of test_html_generation_does_not_expand_template_placeholders_in_user_text: a title with the placeholders is
    // shown literally and the scene appears once, in its data block. (The old sequential replace expanded the whole
    // scene JSON inside the <title>.)
    ScratchDir dir("three_placeholders");
    ost_geometry::ThreeJSWriter w;
    w.set_title("Bid {{SCENE_DATA}} {{TITLE}}");
    w.add_mesh(triangle_ios(0, 0, 0, 1, 0, 0, 0, 0, -1), "Scene {{TITLE}} {{SCENE_DATA}}", 1.0f, 0.0f, 0.0f);
    const std::string html = save_page(dir, w);
    CHECK(html.find("<title>Bid {{SCENE_DATA}} {{TITLE}}</title>") != std::string::npos);
    CHECK_EQ(count_of(html, "\"geometries\":["), static_cast<size_t>(1));
    Json scene;
    REQUIRE(parse_scene(html, scene));
    CHECK_STR_EQ(scene.get("geometries")->items[0].get("name")->text, "Scene {{TITLE}} {{SCENE_DATA}}");
}

TEST(ThreeJsEscape, TitleWithAngleBracketsIsEscapedLikeTheWindowsHeader)
{
    // Port of test_header_shows_escaped_title_and_subtitle (the <title> part).
    ScratchDir dir("three_title_angle");
    ost_geometry::ThreeJSWriter w;
    w.set_title("26-063 41 Maryland Ave <Apartments>, MD");
    w.add_mesh(triangle_ios(0, 0, 0, 1, 0, 0, 0, 0, -1), "m", 1.0f, 0.0f, 0.0f);
    CHECK(save_page(dir, w).find("<title>26-063 41 Maryland Ave &lt;Apartments&gt;, MD</title>") != std::string::npos);
}

TEST(ThreeJsEscape, GeometryNamesRoundTripThroughJsonParse)
{
    // The harness name of the plan 'Wall "A"\x </script><script>alert(1)</script>_0' produced invalid scene JSON. Every
    // name must read back unchanged (JSON.parse), the '<' written as a JSON escape so no script terminator is left in
    // the data block, and non-ASCII text escaped.
    const std::vector<std::string> names = {
        std::string("Wall \"A\"\\x </script><script>alert(1)</script>_0"),
        "plain_1",
        "tab\tand\nnewline\rcontrol\x01\x1f\x7f",
        "caf\xc3\xa9 \xe4\xb8\xad\xe6\x96\x87 \xf0\x9f\x98\x80",
        "back\\slash / slash",
    };
    ScratchDir dir("three_names");
    ost_geometry::ThreeJSWriter w;
    for (const auto &n : names)
        w.add_mesh(triangle_ios(0, 0, 0, 1, 0, 0, 0, 0, -1), n, 1.0f, 0.0f, 0.0f);
    const std::string html = save_page(dir, w);
    const std::string block = scene_data_block(html);
    CHECK(block.find('<') == std::string::npos);
    for (unsigned char c : block)
        CHECK(c >= 0x20 && c < 0x7f); // the whole block is printable ASCII
    Json scene;
    REQUIRE(parse_scene(html, scene));
    const auto &geoms = scene.get("geometries")->items;
    REQUIRE_EQ(geoms.size(), names.size());
    for (size_t i = 0; i < names.size(); ++i)
    {
        SCOPED_CONTEXT("name " + std::to_string(i));
        CHECK_STR_EQ(geoms[i].get("name")->text, names[i]);
    }
}

// ---------------------------------------------------------------- EXP-19 viewer constants
TEST(ThreeJsViewer, UsesTheWindowsColourManagementAndLightingConstants)
{
    // Port of test_threejs_renderer.py::test_viewer_shading_matches_the_native_renderer, narrowed by the decision of
    // 2026-11-16 to the colour-management and lighting constants: no tone mapping, linear colour (no sRGB conversion),
    // ambient 0.55, key [500,500,500] 0.45, fill [-500,500,-500] 0.35, rim [0,-500,-500] 0.2, the lighting multiplied
    // with the colour; no MeshPhongMaterial, DirectionalLight, AmbientLight, ACES tone mapping or exposure.
    ScratchDir dir("three_viewer");
    ost_geometry::ThreeJSWriter w;
    w.add_mesh(triangle_ios(0, 0, 0, 1, 0, 0, 0, 0, -1), "m", 1.0f, 0.0f, 0.0f);
    const std::string html = compact_code(save_page(dir, w));
    auto has = [&](const std::string &code) { return html.find(compact_code(code)) != std::string::npos; };
    CHECK(has("renderer.toneMapping = THREE.NoToneMapping;"));
    CHECK(has("renderer.outputEncoding = THREE.LinearEncoding;"));
    CHECK(has("if (THREE.ColorManagement) THREE.ColorManagement.enabled = false;"));
    CHECK(has("const NATIVE_AMBIENT_INTENSITY = 0.55;"));
    CHECK(has("const NATIVE_KEY_LIGHT = { dir: [500.0, 500.0, 500.0], intensity: 0.45 };"));
    CHECK(has("const NATIVE_FILL_LIGHT = { dir: [-500.0, 500.0, -500.0], intensity: 0.35 };"));
    CHECK(has("const NATIVE_RIM_LIGHT = { dir: [0.0, -500.0, -500.0], intensity: 0.2 };"));
    CHECK(has("return lighting * uColor;"));
    CHECK(html.find("MeshPhongMaterial") == std::string::npos);
    CHECK(html.find("DirectionalLight") == std::string::npos);
    CHECK(html.find("AmbientLight") == std::string::npos);
    CHECK(html.find("ACESFilmic") == std::string::npos);
    CHECK(html.find("legacyMode") == std::string::npos); // assertCodeNotIn("legacyMode", html) of the Windows test
    CHECK(html.find("toneMappingExposure") == std::string::npos);
    CHECK(html.find("sRGBEncoding") == std::string::npos);
    CHECK(html.find("SRGBColorSpace") == std::string::npos);
}

TEST(ThreeJsViewer, ViewerLoadsTheThreeVersionOfTheWindowsViewer)
{
    // The colour-management calls above are the API of three@0.150.0, the version of the Windows viewer (viewer.html).
    ScratchDir dir("three_version");
    ost_geometry::ThreeJSWriter w;
    w.add_mesh(triangle_ios(0, 0, 0, 1, 0, 0, 0, 0, -1), "m", 1.0f, 0.0f, 0.0f);
    const std::string html = save_page(dir, w);
    CHECK(html.find("https://unpkg.com/three@0.150.0/build/three.module.js") != std::string::npos);
    CHECK(html.find("https://unpkg.com/three@0.150.0/examples/jsm/") != std::string::npos);
    CHECK(html.find("three@0.152.0") == std::string::npos);
}

// ---------------------------------------------------------------- writer contract
TEST(ThreeJsWriter, NothingIsWrittenWithoutAMesh)
{
    ScratchDir dir("three_empty");
    ost_geometry::ThreeJSWriter w;
    CHECK_FALSE(w.save(dir.file("e.html")));
    w.add_mesh(Mesh(), "empty", 1.0f, 1.0f, 1.0f);
    CHECK_FALSE(w.save(dir.file("e.html")));
    std::string text;
    CHECK_FALSE(ost_test::read_file(dir.file("e.html"), text));
}
