// BUG (device): "the meshes are not visible in the 3D view". A realistic bid goes through Core the way the app takes it: the
// takeoffs of the selected pages -> generate_view_meshes (page rotation / flip, holes, the boolean pass, colour and opacity of the
// visualization mode) -> the bounds of the scene as Renderer.mm computes them (float) -> the camera fit (frame_camera) -> the clip
// planes and the projection of Renderer.mm renderFrameWithTime. The tests assert what the owner expects to see: meshes exist, the
// bounds are finite and not empty, the whole scene lies inside the frustum at the fit pose, every mesh with an opacity below 1
// belongs to a pass that draws it, and the far plane reaches the scene wherever the camera is (the unit change MSH-01 made the
// meshes thousands of units wide while the renderer still started with a camera at distance 12 and a scene radius of 15).
//
// Inputs: no .ost / .osp bid exists in the repository (only the synthetic oracles and two CAB files), so the bid is a small house
// built by hand with the page scale of the oracle data (WindowsOracles.hpp linear_ratio96_cases: sf1 0.125, sf2 12; the other pages
// use the same kind of scale) and every takeoff kind: walls (straight and curved), a slab with a hole, columns, count shapes,
// attachments and a negative door.
// The patterns cover Solid, None and the transparent ones, so the default visualization mode (Original, opacity 0.5 for them)
// sends meshes through the transparent passes.
//
// Findings of the analysis (2026-11-19): Core produces a visible scene and the fit pose is inside the frustum (the first four tests
// pass on the code before the fix: the unit change MSH-01 is not what hides a fitted scene). What can hide a scene is the far plane
// (60 at the start of the renderer, against vertices 288 to 10123 units from the initial camera) when it appears without a refit,
// and, for the meshes with an opacity below 1, the transparent passes of the renderer: the shared depth texture and the composite
// blend are GPU state in Renderer.mm that no host test can run (see test_scene_rendering.cpp); they need a device check.
#include "MeshingTestUtil.hpp"

#include "Domain/CameraFraming.hpp"
#include "Domain/DisplayColor.hpp"
#include "Domain/RenderConfig.hpp"
#include "Domain/SceneRendering.hpp"
#include "Meshing/BooleanPass.hpp"
#include "Meshing/ExportMeshSet.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace mesh_test;
using ost_domain::CameraPose;
using ost_domain::Pattern;
using ost_domain::SceneBounds;

namespace
{
    // The boolean stand-in of the host tests that need no Manifold: the cut changes nothing here, the pass itself (which meshes are
    // cut, which are dropped) is what the scene goes through.
    ost_meshing::BooleanDifferenceFn keep_positive()
    {
        return [](const Mesh &positive, const std::vector<Mesh> &) -> std::optional<Mesh> { return positive; };
    }

    Condition styled(Condition c, Pattern pattern)
    {
        c.pattern = pattern;
        return c;
    }

    // The conditions of the house.
    void add_conditions(ost_data::ProjectData &p)
    {
        Condition wall = make_condition("wall", ConditionType::Linear);
        wall.height = 108.0;
        wall.thickness = 6.0;
        wall.color_fill = 0x00A0C0; // a non-grey colour
        add(p, styled(wall, Pattern::Horizontal));

        Condition door = make_condition("door", ConditionType::Linear); // the negative door cuts the walls (same type)
        door.height = 84.0;
        door.thickness = 8.0;
        add(p, styled(door, Pattern::Solid));

        Condition slab = make_condition("slab", ConditionType::Area);
        slab.thickness = 4.0;
        slab.z_value = 0.0;
        slab.color_fill = 0x808080;
        add(p, styled(slab, Pattern::Transparent));

        Condition opening = make_condition("opening", ConditionType::Area);
        opening.thickness = 4.0;
        opening.backout = true;
        add(p, styled(opening, Pattern::None));

        Condition column = make_condition("column", ConditionType::Count);
        column.shape = ShapeId::Circle;
        column.width = 12.0;
        column.depth = 12.0;
        column.height = 108.0;
        column.color_fill = 0x3030C0;
        add(p, styled(column, Pattern::Solid));

        Condition outlet = make_condition("outlet", ConditionType::Count);
        outlet.shape = ShapeId::Rhombus;
        outlet.width = 8.0;
        outlet.depth = 8.0;
        outlet.height = 12.0;
        outlet.z_value = 48.0;
        add(p, styled(outlet, Pattern::Vertical));

        Condition window = make_condition("window", ConditionType::Attachment);
        window.shape = ShapeId::Rectangle;
        window.width = 36.0;
        window.depth = 6.0;
        window.height = 48.0;
        window.z_value = 36.0;
        add(p, styled(window, Pattern::None));
    }

    // The house of one page: 16 positive takeoffs (5 straight walls, 1 curved wall, 1 slab with a hole, 4 columns, 3 outlets,
    // 2 windows) and a negative door; positions in raw OST units.
    void add_house(ost_data::ProjectData &p, const std::string &page)
    {
        auto id = [&page](const std::string &name) { return page + "_" + name; };
        add(p, make_takeoff(id("wN"), "wall", page, {200, 200, 1400, 200}));
        add(p, make_takeoff(id("wE"), "wall", page, {1400, 200, 1400, 920}));
        add(p, make_takeoff(id("wS"), "wall", page, {1400, 920, 200, 920}));
        add(p, make_takeoff(id("wW"), "wall", page, {200, 920, 200, 200}));
        add(p, make_takeoff(id("wI"), "wall", page, {800, 200, 800, 920}));
        Takeoff curved = make_takeoff(id("wC"), "wall", page, {300, 500, 600, 500, 450, 580});
        curved.curve = 0;
        add(p, curved);
        add(p, make_takeoff(id("slab"), "slab", page, {200, 200, 1400, 200, 1400, 920, 200, 920}));
        add(p, make_takeoff(id("hole"), "opening", page, {900, 300, 1100, 300, 1100, 500, 900, 500}, id("slab")));
        add(p, make_takeoff(id("c1"), "column", page, {200, 200}));
        add(p, make_takeoff(id("c2"), "column", page, {1400, 200}));
        add(p, make_takeoff(id("c3"), "column", page, {1400, 920}));
        add(p, make_takeoff(id("c4"), "column", page, {200, 920}));
        add(p, make_takeoff(id("o1"), "outlet", page, {400, 200}));
        add(p, make_takeoff(id("o2"), "outlet", page, {600, 200}));
        add(p, make_takeoff(id("o3"), "outlet", page, {1000, 920}));
        add(p, make_takeoff(id("a1"), "window", page, {500, 200}));
        add(p, make_takeoff(id("a2"), "window", page, {1000, 200}));
        Takeoff cut = make_takeoff(id("cut"), "door", page, {700, 200, 760, 200});
        cut.is_negative = true;
        add(p, cut);
    }

    constexpr size_t kPositivesPerHouse = 16;

    ost_data::ProjectData bid(const std::vector<std::string> &pages)
    {
        ost_data::ProjectData p;
        // 36 x 24 in sheets (2592 x 1728 pt): 1:96 as in the oracle data (sf1 0.125, sf2 12), 1:48 rotated, 1:96 flipped
        add(p, make_page("p96", 2592, 1728, 0.125, 12.0));
        add(p, make_page("p48r", 2592, 1728, 0.25, 12.0, 90));
        add(p, make_page("p96f", 2592, 1728, 0.125, 12.0, 0, true, true));
        add(p, make_page("p240", 2592, 1728, 0.05, 12.0, 180));
        add_conditions(p);
        for (const auto &page : pages)
            add_house(p, page);
        return p;
    }

    std::vector<ost_meshing::BooleanMesh> scene(const ost_data::ProjectData &p, const std::set<std::string> &pages,
                                                const char *mode = ost_domain::RenderConfig::MODE_ORIGINAL)
    {
        ost_domain::RenderConfig config;
        config.color_mode = mode;
        return ost_meshing::generate_view_meshes(p, pages, config, keep_positive());
    }

    // The bounds of the scene as Renderer.mm _sceneBounds computes them: float min / max over the vertices of every mesh.
    std::optional<SceneBounds> bounds_of_scene(const std::vector<ost_meshing::BooleanMesh> &meshes)
    {
        bool any = false;
        SceneBounds b;
        for (const auto &m : meshes)
            for (const auto &v : m.mesh.vertices)
            {
                const float x = static_cast<float>(v[0]), y = static_cast<float>(v[1]), z = static_cast<float>(v[2]);
                if (!any)
                {
                    b = {x, y, z, x, y, z};
                    any = true;
                    continue;
                }
                b.min_x = std::fmin(b.min_x, x);
                b.min_y = std::fmin(b.min_y, y);
                b.min_z = std::fmin(b.min_z, z);
                b.max_x = std::fmax(b.max_x, x);
                b.max_y = std::fmax(b.max_y, y);
                b.max_z = std::fmax(b.max_z, z);
            }
        if (!any)
            return std::nullopt;
        return b;
    }

    // What the renderer's projection does with one vertex: its depth along the view direction and its normalised device
    // coordinates for a vertical field of view of pi / 4 and the given aspect (Renderer.mm Mat4::perspective(M_PI / 4, aspect, ...)).
    struct Seen
    {
        double depth, ndc_x, ndc_y, distance;
    };

    Seen see(const CameraPose &pose, const std::array<double, 3> &v, double aspect)
    {
        const auto eye = ost_domain::camera_eye(pose);
        double f[3] = {pose.target_x - eye.x, pose.target_y - eye.y, pose.target_z - eye.z};
        const double fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
        for (double &c : f)
            c /= fl;
        // right = forward x up (0, 1, 0), up' = right x forward
        double r[3] = {f[1] * 0.0 - f[2] * 1.0, f[2] * 0.0 - f[0] * 0.0, f[0] * 1.0 - f[1] * 0.0};
        const double rl = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
        for (double &c : r)
            c /= rl;
        double u[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};
        const double d[3] = {v[0] - eye.x, v[1] - eye.y, v[2] - eye.z};
        Seen s;
        s.depth = d[0] * f[0] + d[1] * f[1] + d[2] * f[2];
        const double tan_half = std::tan(3.14159265358979323846 / 8.0); // fov pi / 4
        s.ndc_x = (d[0] * r[0] + d[1] * r[1] + d[2] * r[2]) / (s.depth * tan_half * aspect);
        s.ndc_y = (d[0] * u[0] + d[1] * u[1] + d[2] * u[2]) / (s.depth * tan_half);
        s.distance = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        return s;
    }

    // The frame the renderer builds after the fit of a scene with these bounds.
    struct Frame
    {
        CameraPose pose;
        ost_domain::RenderClipPlanes planes;
        float scene_radius;
    };

    Frame fitted(const SceneBounds &bounds)
    {
        Frame frame;
        frame.pose = ost_domain::frame_camera(CameraPose{}, bounds);
        frame.scene_radius = bounds.max_extent() * 0.5f;
        frame.planes = ost_domain::render_clip_planes(frame.pose, frame.scene_radius, bounds);
        return frame;
    }

    const std::vector<std::set<std::string>> kPageSets = {{"p96"}, {"p48r"}, {"p96f"}, {"p240"}, {"p96", "p48r", "p96f", "p240"}};
}

// ---------------------------------------------------------------------------------------------------------------
// Core produces a scene
// ---------------------------------------------------------------------------------------------------------------

TEST(SceneInView, ARealisticBidGivesMeshesWithFiniteBounds)
{
    const auto p = bid({"p96", "p48r", "p96f", "p240"});
    for (const auto &pages : kPageSets)
    {
        SCOPED_CONTEXT("pages " + std::to_string(pages.size()) + " first " + *pages.begin());
        const auto meshes = scene(p, pages);
        // every positive takeoff has a mesh; the hole, the negative door and nothing else are not meshes of their own
        CHECK_EQ(meshes.size(), kPositivesPerHouse * pages.size());
        for (const auto &m : meshes)
        {
            CHECK(m.mesh.is_valid());
            CHECK(m.color.opacity > 0.0f && m.color.opacity <= 1.0f);
        }
        const auto bounds = bounds_of_scene(meshes);
        REQUIRE(bounds.has_value());
        CHECK(bounds->is_finite());
        CHECK(bounds->max_extent() > 100.0f); // raw OST units: a house is thousands of units wide, not a few
        CHECK(bounds->max_x > bounds->min_x);
        CHECK(bounds->max_y > bounds->min_y);
        CHECK(bounds->max_z > bounds->min_z);
    }
}

// The visualization mode only colours: the meshes, their order and their bounds are the same in every mode.
TEST(SceneInView, TheVisualizationModeNeverChangesWhichMeshesExist)
{
    const auto p = bid({"p96", "p48r"});
    const std::set<std::string> pages = {"p96", "p48r"};
    const auto solid = scene(p, pages, ost_domain::RenderConfig::MODE_SOLID);
    const auto original = scene(p, pages, ost_domain::RenderConfig::MODE_ORIGINAL);
    const auto transparent = scene(p, pages, ost_domain::RenderConfig::MODE_TRANSPARENT);
    REQUIRE_EQ(solid.size(), original.size());
    REQUIRE_EQ(solid.size(), transparent.size());
    for (size_t i = 0; i < solid.size(); ++i)
    {
        CHECK_EQ(solid[i].takeoff->uid, original[i].takeoff->uid);
        CHECK_EQ(solid[i].takeoff->uid, transparent[i].takeoff->uid);
        CHECK_EQ(solid[i].mesh.vertices.size(), original[i].mesh.vertices.size());
        CHECK_EQ(solid[i].mesh.faces.size(), transparent[i].mesh.faces.size());
        CHECK(solid[i].color.opacity == 1.0f);      // Solid: everything opaque
        CHECK(transparent[i].color.opacity == 0.5f); // Transparent: everything half transparent
    }
    size_t translucent = 0;
    for (const auto &m : original)
        if (ost_domain::is_transparent_alpha(m.color.opacity))
            ++translucent;
    // Original mode: the patterns that are not Solid or None (walls Horizontal, slab Transparent, outlets Vertical) are 0.5
    CHECK_EQ(translucent, static_cast<size_t>(2 * (5 + 1 + 1 + 3)));
}

// ---------------------------------------------------------------------------------------------------------------
// the scene is in front of the camera
// ---------------------------------------------------------------------------------------------------------------

// The fit pose frames the whole scene: every vertex is between the near and the far plane and inside the side planes at the aspect
// of a landscape view (a portrait phone crops the sides: the Windows show_object takes no aspect, see CameraFraming.hpp).
TEST(SceneInView, TheWholeSceneLiesInsideTheFrustumAtTheFitPose)
{
    const auto p = bid({"p96", "p48r", "p96f", "p240"});
    for (const auto &pages : kPageSets)
    {
        SCOPED_CONTEXT("pages " + std::to_string(pages.size()) + " first " + *pages.begin());
        const auto meshes = scene(p, pages);
        const auto bounds = bounds_of_scene(meshes);
        REQUIRE(bounds.has_value());
        const Frame frame = fitted(*bounds);
        REQUIRE(frame.planes.near_plane > 0.0f);
        REQUIRE(frame.planes.far_plane > frame.planes.near_plane);
        size_t outside = 0, total = 0;
        for (const auto &m : meshes)
            for (const auto &v : m.mesh.vertices)
            {
                ++total;
                const Seen s = see(frame.pose, v, 4.0 / 3.0);
                if (!(s.depth >= frame.planes.near_plane && s.depth <= frame.planes.far_plane && std::fabs(s.ndc_x) <= 1.0 &&
                      std::fabs(s.ndc_y) <= 1.0))
                    ++outside;
            }
        CHECK_EQ(outside, static_cast<size_t>(0));
        CHECK(total > 0);
    }
}

// The planes at the fit pose are the ones of the renderer before the fix: the fix may only add reach where a scene would be cut off.
TEST(SceneInView, TheClipPlanesAtTheFitPoseAreUnchanged)
{
    const auto p = bid({"p96", "p48r", "p96f", "p240"});
    for (const auto &pages : kPageSets)
    {
        const auto bounds = bounds_of_scene(scene(p, pages));
        REQUIRE(bounds.has_value());
        const Frame frame = fitted(*bounds);
        const float radius = bounds->max_extent() * 0.5f;
        const float old_near = std::fmax(frame.pose.distance * 0.005f, 0.01f);
        const float old_far = std::fmax(frame.pose.distance + radius * 2.0f, radius * 4.0f);
        CHECK_EQ(frame.planes.near_plane, old_near);
        CHECK_EQ(frame.planes.far_plane, old_far);
    }
}

// The unit change made the scene thousands of units wide while the renderer starts with a camera at distance 12, target (0, 1, 0)
// and a scene radius of 15 (far plane 60). A scene that appears without a refit (the first selected page had no takeoffs, a page
// is added to the selection) must not be cut off by the far plane: it reaches the farthest vertex wherever the camera is.
// (Red before the fix: far = 60 against a scene 288 to 10123 units away.)
TEST(SceneInView, TheFarPlaneReachesTheWholeSceneWhereverTheCameraIs)
{
    const auto p = bid({"p96", "p48r", "p96f", "p240"});
    CameraPose initial; // Renderer.mm: target (0, 1, 0), distance 12, yaw 0.5, pitch 0.35
    initial.target_y = 1.0f;
    CameraPose zoomed_out = initial;
    zoomed_out.distance = 5000.0f;
    for (const CameraPose &pose : {initial, zoomed_out})
        for (const auto &pages : kPageSets)
        {
            SCOPED_CONTEXT("pages " + std::to_string(pages.size()) + " first " + *pages.begin() + ", distance " +
                           std::to_string(pose.distance));
            const auto meshes = scene(p, pages);
            const auto bounds = bounds_of_scene(meshes);
            REQUIRE(bounds.has_value());
            const auto planes = ost_domain::render_clip_planes(pose, 15.0f, bounds);
            double farthest = 0.0;
            for (const auto &m : meshes)
                for (const auto &v : m.mesh.vertices)
                    farthest = std::max(farthest, see(pose, v, 4.0 / 3.0).distance);
            CHECK(planes.far_plane > farthest); // with the 0.1 % margin
            CHECK(planes.near_plane > 0.0f);
            CHECK(planes.far_plane > planes.near_plane);
        }
}

// ---------------------------------------------------------------------------------------------------------------
// every mesh is drawn by some pass
// ---------------------------------------------------------------------------------------------------------------

// The meshes that the default visualization mode makes half transparent are drawn by the transparent passes, and when those cannot be
// used by plain alpha blending: no mesh is left out of every pass (red against the old rule, which skipped a transparent mesh from
// the opaque pass and drew it by nothing when the OIT framebuffers were unusable).
TEST(SceneInView, EveryMeshOfTheSceneIsDrawnByAPass)
{
    const auto p = bid({"p96", "p48r"});
    for (const char *mode : {ost_domain::RenderConfig::MODE_SOLID, ost_domain::RenderConfig::MODE_ORIGINAL,
                             ost_domain::RenderConfig::MODE_TRANSPARENT})
        for (bool oit : {true, false})
        {
            SCOPED_CONTEXT(std::string("mode ") + mode + (oit ? ", OIT available" : ", OIT unavailable"));
            size_t opaque = 0, oit_passes = 0, blended = 0;
            for (const auto &m : scene(p, {"p96", "p48r"}, mode))
            {
                const auto pass = ost_domain::mesh_pass_for_alpha(m.color.opacity, oit);
                // opaque meshes go to the opaque pass, the others to the OIT passes, or to plain alpha blending when those are
                // unusable: always exactly one pass that draws them
                const auto expected = !ost_domain::is_transparent_alpha(m.color.opacity) ? ost_domain::MeshPass::Opaque
                                      : oit                                              ? ost_domain::MeshPass::Oit
                                                                                         : ost_domain::MeshPass::AlphaBlend;
                CHECK(pass == expected);
                opaque += pass == ost_domain::MeshPass::Opaque;
                oit_passes += pass == ost_domain::MeshPass::Oit;
                blended += pass == ost_domain::MeshPass::AlphaBlend;
            }
            CHECK_EQ(opaque + oit_passes + blended, kPositivesPerHouse * 2);
            if (std::string(mode) == ost_domain::RenderConfig::MODE_SOLID)
                CHECK_EQ(opaque, kPositivesPerHouse * 2);
            else
                CHECK(opaque + (oit ? oit_passes : blended) == kPositivesPerHouse * 2);
        }
}
