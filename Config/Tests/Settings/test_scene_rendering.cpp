// The two renderer decisions that decide whether a mesh can be seen (Core/Domain/SceneRendering.hpp, extracted from
// Rendering/Renderer.mm renderFrameWithTime): which pass draws a mesh, and the clip planes of a frame.
//
// Device report: the meshes are not visible when the visualization mode is Transparent, or Original with the transparent pattern.
// Those are the meshes with an opacity below 1; Renderer.mm skipped them in the opaque pass and drew them only through the
// weighted-blended OIT passes, which therefore decided alone whether they showed. What was wrong there is GPU state in
// Rendering/Renderer.mm: the OIT passes tested against a depth texture of their own that nothing cleared or wrote (they now test
// against the depth of the opaque scene like Windows renderer.cpp does), and the composite blended its premultiplied output with
// SRC_ALPHA (now ONE / INV_SRC_ALPHA). NEITHER of those two lines can run on the host: they are compiled by the CI and need a device
// check (Transparent mode; Original mode with a Transparent-pattern condition). What the tests below pin is the Core part: the
// pass of every mesh (none is left out of every pass) and the clip planes.
//
// Windows (renderer.cpp 6d4786f): `mesh.transparent = data.color.a < 0.99f` (464); accum_fbo and opaque_fbo share one depth texture
// (1803, 1815) and render_transparent tests against it with glDepthMask(GL_FALSE) (2266); configure_clip_planes follows the live
// scene bounds on every scene update (near = max(0.1, size * 0.001), far = max(10000, size * 20); the iOS near plane scales with the
// camera distance and is unchanged here; Windows calls it from show_object and restore_state, mesh_view.py 1425 / 1438 / 1446).
#include "TestHarness.hpp"

#include "Domain/CameraFraming.hpp"
#include "Domain/SceneRendering.hpp"
#include "MeshLoader.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

using ost_domain::CameraPose;
using ost_domain::MeshPass;
using ost_domain::SceneBounds;

namespace
{
    // The renderer's clip planes before the fix, written out as the oracle of "unchanged where nothing was cut off".
    float old_near(float distance, bool has_meshes) { return has_meshes ? std::max(distance * 0.005f, 0.01f) : 0.1f; }
    float old_far(float distance, float radius, bool has_meshes)
    {
        float far_plane = std::max(distance + radius * 2.0f, radius * 4.0f);
        if (far_plane < 100.0f && !has_meshes)
            far_plane = 100.0f;
        return far_plane;
    }

    // A scene box of the given largest side, placed where the pages of a rotated or flipped sheet put it (far from the origin).
    SceneBounds box(float size, float x, float y, float z) { return {x, y, z, x + size, y + size * 0.1f, z + size * 0.6f}; }
}

// ---------------------------------------------------------------------------------------------------------------
// which pass draws a mesh
// ---------------------------------------------------------------------------------------------------------------

TEST(SceneRendering, AMeshIsTransparentBelowAnAlphaOfPointNinetyNine)
{
    CHECK_FALSE(ost_domain::is_transparent_alpha(1.0f));
    CHECK_FALSE(ost_domain::is_transparent_alpha(0.99f));
    CHECK(ost_domain::is_transparent_alpha(std::nextafter(0.99f, 0.0f)));
    CHECK(ost_domain::is_transparent_alpha(0.5f));
    CHECK(ost_domain::is_transparent_alpha(0.0f));
}

// The renderer's Color::is_transparent() (Rendering/MathUtils.hpp; the passes of Renderer.mm now ask the Core rule directly) is the
// same rule.
TEST(SceneRendering, TheRendererColourUsesTheCoreThreshold)
{
    const double vertices[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const uint32_t faces[] = {0, 1, 2};
    for (float alpha : {1.0f, 0.99f, std::nextafter(0.99f, 0.0f), 0.5f, 0.0f})
    {
        SCOPED_CONTEXT("alpha " + std::to_string(alpha));
        const LoadedMesh mesh = loadMeshFromRaw(vertices, 3, faces, 1, 0.2f, 0.4f, 0.6f, alpha);
        CHECK(mesh.color.a == alpha);
        CHECK(mesh.color.is_transparent() == ost_domain::is_transparent_alpha(alpha));
    }
}

TEST(SceneRendering, EveryMeshIsDrawnByExactlyOnePassWhateverTheTransparentPassesCanDo)
{
    for (float alpha : {1.0f, 0.99f, 0.989f, 0.75f, 0.5f, 0.25f, 0.01f, 0.0f})
    {
        SCOPED_CONTEXT("alpha " + std::to_string(alpha));
        const MeshPass with_oit = ost_domain::mesh_pass_for_alpha(alpha, true);
        const MeshPass without_oit = ost_domain::mesh_pass_for_alpha(alpha, false);
        if (ost_domain::is_transparent_alpha(alpha))
        {
            CHECK(with_oit == MeshPass::Oit);
            // when the OIT framebuffers cannot be used the mesh is blended, never left out (red before the fix: no pass)
            CHECK(without_oit == MeshPass::AlphaBlend);
        }
        else
        {
            CHECK(with_oit == MeshPass::Opaque);
            CHECK(without_oit == MeshPass::Opaque);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the eye
// ---------------------------------------------------------------------------------------------------------------

TEST(SceneRendering, TheEyeIsAtTheOrbitDistanceFromTheTarget)
{
    for (const CameraPose &pose : {CameraPose{}, CameraPose{10, -4, 7, 300, 2.0f, 0.1f}, CameraPose{0, 0, 0, 50, 3.14159265f, 0.98f}})
    {
        const auto eye = ost_domain::camera_eye(pose);
        const float dx = eye.x - pose.target_x, dy = eye.y - pose.target_y, dz = eye.z - pose.target_z;
        CHECK_NEAR(std::sqrt(dx * dx + dy * dy + dz * dz), pose.distance, 1e-3f * pose.distance);
    }
    // the fit pose looks from the page-bottom side, above the plan: renderer offset (0, 1.5 size, -size), CameraFraming.hpp
    SceneBounds b = {0, 0, 0, 100, 10, 60};
    const CameraPose fit = ost_domain::frame_camera(CameraPose{}, b);
    const auto eye = ost_domain::camera_eye(fit);
    CHECK_NEAR(eye.x - fit.target_x, 0.0f, 1e-2f);
    CHECK_NEAR(eye.y - fit.target_y, 150.0f, 1e-2f);
    CHECK_NEAR(eye.z - fit.target_z, -100.0f, 1e-2f);
}

// The pose of Renderer.mm at start (target (0, 1, 0), distance 12, yaw 0.5, pitch 0.35) and the axis-aligned poses: they pin the
// DIRECTION of the orbit (a mirrored x or z would mirror the camera), which the fit pose (yaw pi) cannot.
TEST(SceneRendering, TheEyeIsOnTheRightSideForTheStartPoseAndTheAxisAlignedPoses)
{
    CameraPose initial;
    initial.target_y = 1.0f;
    const auto start = ost_domain::camera_eye(initial);
    CHECK_NEAR(start.x, 5.4043f, 1e-3f);
    CHECK_NEAR(start.y, 5.1148f, 1e-3f);
    CHECK_NEAR(start.z, 9.8925f, 1e-3f);

    const float half_pi = 1.57079633f;
    auto eye = ost_domain::camera_eye(CameraPose{0, 0, 0, 100, 0.0f, 0.0f}); // yaw 0, pitch 0: +z
    CHECK_NEAR(eye.x, 0.0f, 1e-3f);
    CHECK_NEAR(eye.y, 0.0f, 1e-3f);
    CHECK_NEAR(eye.z, 100.0f, 1e-3f);
    eye = ost_domain::camera_eye(CameraPose{0, 0, 0, 100, half_pi, 0.0f}); // yaw pi / 2: +x
    CHECK_NEAR(eye.x, 100.0f, 1e-3f);
    CHECK_NEAR(eye.y, 0.0f, 1e-3f);
    CHECK_NEAR(eye.z, 0.0f, 1e-3f);
    eye = ost_domain::camera_eye(CameraPose{0, 0, 0, 100, 0.0f, half_pi}); // pitch pi / 2: +y
    CHECK_NEAR(eye.x, 0.0f, 1e-3f);
    CHECK_NEAR(eye.y, 100.0f, 1e-3f);
    CHECK_NEAR(eye.z, 0.0f, 1e-3f);
    eye = ost_domain::camera_eye(CameraPose{5, -2, 7, 100, half_pi, 0.0f}); // the target is added
    CHECK_NEAR(eye.x, 105.0f, 1e-3f);
    CHECK_NEAR(eye.y, -2.0f, 1e-3f);
    CHECK_NEAR(eye.z, 7.0f, 1e-3f);
}

// ---------------------------------------------------------------------------------------------------------------
// the clip planes
// ---------------------------------------------------------------------------------------------------------------

// The farthest of the eight corners is found wherever the eye is (an independent loop as the oracle): the scene may lie on any side
// of the eye.
TEST(SceneRendering, TheFarthestCornerIsFoundWhicheverCornerItIs)
{
    const SceneBounds b = {0, 0, 0, 10, 20, 30};
    for (int corner = 0; corner < 8; ++corner)
    {
        SCOPED_CONTEXT("eye beyond corner " + std::to_string(corner));
        // the eye sits beyond the corner OPPOSITE to `corner`, so `corner` is the farthest one
        const ost_domain::EyePosition eye{(corner & 1) ? -5.0f : 15.0f, (corner & 2) ? -5.0f : 25.0f, (corner & 4) ? -5.0f : 35.0f};
        CHECK_NEAR(ost_domain::farthest_corner_distance(eye, b), std::sqrt(15.0f * 15.0f + 25.0f * 25.0f + 35.0f * 35.0f), 1e-3f);
    }
}

// The far plane reaches a scene in every octant around the initial camera.
TEST(SceneRendering, TheFarPlaneReachesASceneInEveryOctant)
{
    CameraPose initial;
    initial.target_y = 1.0f;
    const auto eye = ost_domain::camera_eye(initial);
    for (float sx : {-1.0f, 1.0f})
        for (float sy : {-1.0f, 1.0f})
            for (float sz : {-1.0f, 1.0f})
            {
                SCOPED_CONTEXT("octant " + std::to_string(sx) + " " + std::to_string(sy) + " " + std::to_string(sz));
                const float x0 = sx > 0 ? 2000.0f : -3200.0f, y0 = sy > 0 ? 300.0f : -400.0f, z0 = sz > 0 ? 1500.0f : -2700.0f;
                const SceneBounds scene = {x0, y0, z0, x0 + 1200.0f, y0 + 100.0f, z0 + 1200.0f};
                double farthest = 0.0;
                for (double x : {static_cast<double>(scene.min_x), static_cast<double>(scene.max_x)})
                    for (double y : {static_cast<double>(scene.min_y), static_cast<double>(scene.max_y)})
                        for (double z : {static_cast<double>(scene.min_z), static_cast<double>(scene.max_z)})
                            farthest = std::max(farthest, std::sqrt((x - eye.x) * (x - eye.x) + (y - eye.y) * (y - eye.y) +
                                                                    (z - eye.z) * (z - eye.z)));
                const auto planes = ost_domain::render_clip_planes(initial, 15.0f, scene);
                CHECK(planes.far_plane > farthest);
            }
}

// At a fit pose every corner of the scene is inside the old far plane, so the planes are exactly the ones the renderer had: the
// far plane only gets longer where a scene would have been cut off.
TEST(SceneRendering, TheClipPlanesAtAFitPoseAreTheOnesTheRendererAlwaysHad)
{
    for (float size : {3.0f, 50.0f, 100.0f, 800.0f, 1600.0f, 6573.0f, 30000.0f})
        for (float x : {0.0f, -515.0f, -6569.0f})
        {
            SCOPED_CONTEXT("size " + std::to_string(size) + " at x " + std::to_string(x));
            const SceneBounds bounds = box(size, x, 0.0f, -x * 0.5f);
            const CameraPose fit = ost_domain::frame_camera(CameraPose{}, bounds);
            const float radius = bounds.max_extent() * 0.5f;
            const auto planes = ost_domain::render_clip_planes(fit, radius, bounds);
            CHECK_EQ(planes.near_plane, old_near(fit.distance, true));
            CHECK_EQ(planes.far_plane, old_far(fit.distance, radius, true));
        }
}

// The renderer starts with target (0, 1, 0), distance 12 and a scene radius of 15: far 60. A raw-unit scene that appears without a
// refit (first selected page without takeoffs, a page added to the selection, a camera moved away) is far behind that plane
// (red before the fix). The far plane reaches the farthest corner of the live bounds, with a margin.
TEST(SceneRendering, TheFarPlaneReachesTheFarthestCornerOfTheLiveScene)
{
    CameraPose initial;
    initial.target_y = 1.0f;
    const SceneBounds scene = box(1200.0f, -3000.0f, 0.0f, -2000.0f);
    const auto planes = ost_domain::render_clip_planes(initial, 15.0f, scene);
    const float farthest = ost_domain::farthest_corner_distance(ost_domain::camera_eye(initial), scene);
    CHECK(farthest > 3000.0f);
    CHECK(planes.far_plane > farthest); // with the 0.1 % margin, not just equal
    CHECK(planes.far_plane <= farthest * 1.01f + old_far(initial.distance, 15.0f, true)); // reach, not an arbitrary huge plane
    CHECK_EQ(planes.near_plane, old_near(initial.distance, true));

    // zoomed out far beyond the last fit
    CameraPose far_away = initial;
    far_away.distance = 4000.0f;
    const auto zoomed = ost_domain::render_clip_planes(far_away, 15.0f, scene);
    CHECK(zoomed.far_plane >= ost_domain::farthest_corner_distance(ost_domain::camera_eye(far_away), scene));

    // a scene that is already inside the old far plane keeps it
    const SceneBounds near_scene = {-5, 0, -5, 5, 3, 5};
    CHECK_EQ(ost_domain::render_clip_planes(initial, 15.0f, near_scene).far_plane, old_far(initial.distance, 15.0f, true));
}

TEST(SceneRendering, AnEmptySceneAndNonFiniteBoundsKeepTheOldPlanes)
{
    CameraPose pose;
    pose.target_y = 1.0f;
    const auto empty = ost_domain::render_clip_planes(pose, 15.0f, std::nullopt);
    CHECK_EQ(empty.near_plane, 0.1f);
    CHECK_EQ(empty.far_plane, 100.0f); // old_far is 60, an empty scene has at least 100
    CHECK_EQ(ost_domain::render_clip_planes(pose, 40.0f, std::nullopt).far_plane, old_far(pose.distance, 40.0f, false));

    const float inf = std::numeric_limits<float>::infinity();
    const SceneBounds broken = {0, 0, 0, inf, 1, 1};
    const auto planes = ost_domain::render_clip_planes(pose, 15.0f, broken);
    CHECK(std::isfinite(planes.far_plane));
    CHECK(std::isfinite(planes.near_plane));
    CHECK_EQ(planes.far_plane, old_far(pose.distance, 15.0f, true));
    CHECK(planes.near_plane > 0.0f);
}

// The planes stay valid for every pose the pinch and orbit can reach: near > 0 and far > near.
TEST(SceneRendering, TheClipPlanesAreValidForEveryDistance)
{
    const SceneBounds scene = box(2000.0f, -2500.0f, 0.0f, -1500.0f);
    for (float distance : {0.0f, 0.2f, 1.0f, 12.0f, 500.0f, 5000.0f, 100000.0f})
    {
        SCOPED_CONTEXT("distance " + std::to_string(distance));
        CameraPose pose;
        pose.distance = distance;
        for (const std::optional<SceneBounds> &bounds : {std::optional<SceneBounds>(scene), std::optional<SceneBounds>()})
        {
            const auto planes = ost_domain::render_clip_planes(pose, 1000.0f, bounds);
            CHECK(planes.near_plane > 0.0f);
            CHECK(planes.far_plane > planes.near_plane);
            CHECK(std::isfinite(planes.far_plane));
        }
    }
}
