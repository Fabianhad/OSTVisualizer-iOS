// CAM-04: the zoom limits of the 3D camera are relative to the scene (Windows) instead of a fixed 1 .. 500.
//
// Windows: cpp_extensions/src/renderer/renderer.cpp Camera::zoom (lines 291-301) and Camera::configure_clip_planes (303-315):
//   size       = largest side of the scene bounds (100 when it is below 1)
//   near_plane = max(0.1, size * 0.001)       far_plane = max(10000, size * 20)
//   zoom:        new_distance = clamp(distance - delta * distance * 0.001, 2 * near_plane, 0.5 * far_plane)
//                (nothing happens when the distance is below 0.001); an empty scene keeps near 0.1 / far 10000.
// Unit mapping to iOS: the iOS meshes ARE the Windows 3D meshes in the same raw OST units (TakeoffMeshBuilder: the Windows frame
// (X, Y, Z-up) mapped by (X, Y, Z) -> (X, Z, -Y), a permutation that keeps distances), so the largest side of the iOS scene
// bounds is the Windows `size` and the Windows limits apply in iOS scene units unchanged (factor 1). Only the pinch rate
// (delta * distance * 0.5) is an iOS adaptation.
// The expected values in CameraOracleData.hpp come from running the shipped Windows ost_renderer module.
#include "TestHarness.hpp"

#include "CameraOracleData.hpp"
#include "Domain/CameraLimits.hpp"

#include <cmath>
#include <string>

using ost_domain::CameraClipPlanes;

namespace
{
    // relative tolerance of a float32 result computed in a different order / with fused multiply-add
    void check_float_near(double actual, double expected)
    {
        CHECK_NEAR(actual, expected, 2e-6 * std::fabs(expected) + 1e-9);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// configure_clip_planes
// ---------------------------------------------------------------------------------------------------------------

// A scene that has no bounds keeps the camera's default planes (Camera::near_plane = 0.1, far_plane = 10000).
TEST(CameraLimits, DefaultPlanesAreTheWindowsDefaults)
{
    const CameraClipPlanes planes = ost_domain::default_camera_clip_planes();
    CHECK_EQ(planes.near_plane, 0.1f);
    CHECK_EQ(planes.far_plane, 10000.0f);
}

// near = max(0.1, size * 0.001), far = max(10000, size * 20), size = the largest side of the scene.
TEST(CameraLimits, ClipPlanesScaleWithTheScene)
{
    struct Case
    {
        float extent, near_plane, far_plane;
    };
    const Case cases[] = {
        {100.0f, 0.1f, 10000.0f},     // task plan: size 100 -> limits 0.2 .. 5000
        {400.0f, 0.4f, 10000.0f},     // size * 0.001 = 0.4 beats 0.1, size * 20 = 8000 does not beat 10000
        {500.0f, 0.5f, 10000.0f},     // size * 20 = 10000: the two terms meet
        {1500.0f, 1.5f, 30000.0f},
        {3000.0f, 3.0f, 60000.0f},
        {20000.0f, 20.0f, 400000.0f},
        {1.0e6f, 1000.0f, 2.0e7f},
        {50.0f, 0.1f, 10000.0f},
        {1.0f, 0.1f, 10000.0f},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT("extent " + std::to_string(c.extent));
        const CameraClipPlanes planes = ost_domain::camera_clip_planes_for_extent(c.extent);
        CHECK_NEAR(planes.near_plane, c.near_plane, 1e-6 * c.near_plane);
        CHECK_NEAR(planes.far_plane, c.far_plane, 1e-6 * c.far_plane);
    }
}

// size < 1 is treated as 100 (renderer.cpp:309-310): a point-sized scene gets the planes of a size 100 scene.
TEST(CameraLimits, ATinySceneUsesSizeOneHundred)
{
    for (float extent : {0.0f, 0.5f, 0.999f})
    {
        SCOPED_CONTEXT("extent " + std::to_string(extent));
        const CameraClipPlanes planes = ost_domain::camera_clip_planes_for_extent(extent);
        CHECK_EQ(planes.near_plane, 0.1f);
        CHECK_EQ(planes.far_plane, 10000.0f);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Camera::zoom
// ---------------------------------------------------------------------------------------------------------------

// Native results from distance 100 with the Windows camera (item CAM-04): zoom(+120) -> 88.0, zoom(-120) -> 112.0,
// zoom(+1e6) -> 0.2, zoom(-1e9) -> 5000.0 (size 100 scene: limits 0.2 .. 5000).
TEST(CameraLimits, WindowsZoomStepsFromDistance100)
{
    const CameraClipPlanes planes = ost_domain::camera_clip_planes_for_extent(100.0f);
    const float k = ost_domain::kWindowsWheelZoomRate;
    CHECK_NEAR(ost_domain::zoom_camera_distance(100.0f, 120.0f, k, planes), 88.0, 1e-3);
    CHECK_NEAR(ost_domain::zoom_camera_distance(100.0f, -120.0f, k, planes), 112.0, 1e-3);
    CHECK_NEAR(ost_domain::zoom_camera_distance(100.0f, 1.0e6f, k, planes), 0.2, 1e-3);
    CHECK_NEAR(ost_domain::zoom_camera_distance(100.0f, -1.0e9f, k, planes), 5000.0, 1e-3);
    // the limits themselves: 2 * near and 0.5 * far
    CHECK_NEAR(planes.near_plane * 2.0f, 0.2, 1e-6);
    CHECK_NEAR(planes.far_plane * 0.5f, 5000.0, 1e-3);
}

// The whole table of results of the Windows camera: 10 scenes (no bounds, tiny, 1 .. 1e6) x 6 distances x 9 deltas, among
// them the clamps at both ends, the unchanged distance of a zero delta and the "below 0.001: no zoom" case.
TEST(CameraLimits, MatchesTheWindowsCameraOnTheOracleTable)
{
    const size_t count = sizeof(ost_stest::kCameraZoomCases) / sizeof(ost_stest::kCameraZoomCases[0]);
    CHECK_EQ(count, static_cast<size_t>(540));
    for (size_t i = 0; i < count; ++i)
    {
        const ost_stest::CameraZoomCase &c = ost_stest::kCameraZoomCases[i];
        SCOPED_CONTEXT(std::string(c.scene) + " distance " + std::to_string(c.distance) + " delta " + std::to_string(c.delta));
        const CameraClipPlanes planes = std::string(c.scene) == "empty"
                                            ? ost_domain::default_camera_clip_planes()
                                            : ost_domain::camera_clip_planes_for_extent(static_cast<float>(c.extent));
        const float actual = ost_domain::zoom_camera_distance(static_cast<float>(c.distance), static_cast<float>(c.delta),
                                                              ost_domain::kWindowsWheelZoomRate, planes);
        check_float_near(actual, c.expected);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the iOS pinch (same limits, its own rate)
// ---------------------------------------------------------------------------------------------------------------

// Test plan of CAM-04: scene extent 400, the camera fitted at 2.5 * extent = 1000. A pinch change with delta 0 leaves the
// distance at 1000 (iOS used to snap it to 500, the fixed upper bound).
TEST(CameraLimits, AFitDistanceBeyondFiveHundredSurvivesAZeroPinch)
{
    const CameraClipPlanes planes = ost_domain::camera_clip_planes_for_extent(400.0f);
    CHECK_NEAR(ost_domain::zoom_camera_distance(1000.0f, 0.0f, ost_domain::kPinchZoomRate, planes), 1000.0, 1e-3);
}

// The fit distance of Renderer.fitCameraToMeshes (2.5 * the largest extent, at least 3) lies inside the limits of every
// scene, so the first pinch event never moves the camera by itself.
TEST(CameraLimits, TheFitDistanceIsInsideTheLimitsOfEveryScene)
{
    const float extents[] = {0.01f, 0.5f, 1.0f, 3.0f, 10.0f, 100.0f, 200.0f, 201.0f, 400.0f, 2000.0f, 4000.0f, 1.0e5f, 1.0e7f};
    for (float extent : extents)
    {
        SCOPED_CONTEXT("extent " + std::to_string(extent));
        float fit = extent * 2.5f;
        if (fit < 3.0f)
            fit = 3.0f;
        const CameraClipPlanes planes = ost_domain::camera_clip_planes_for_extent(extent);
        CHECK_NEAR(ost_domain::clamp_camera_distance(fit, planes), fit, 1e-6 * fit);
        CHECK_NEAR(ost_domain::zoom_camera_distance(fit, 0.0f, ost_domain::kPinchZoomRate, planes), fit, 1e-6 * fit);
    }
}

// The pinch moves the distance by delta * distance * 0.5 inside the limits and is clamped to them outside.
TEST(CameraLimits, ThePinchUsesTheSceneLimits)
{
    const CameraClipPlanes planes = ost_domain::camera_clip_planes_for_extent(100.0f); // 0.2 .. 5000
    const float k = ost_domain::kPinchZoomRate;
    CHECK_NEAR(ost_domain::zoom_camera_distance(100.0f, 0.1f, k, planes), 95.0, 1e-4);   // pinch out: closer
    CHECK_NEAR(ost_domain::zoom_camera_distance(100.0f, -0.2f, k, planes), 110.0, 1e-4); // pinch in: farther
    CHECK_NEAR(ost_domain::zoom_camera_distance(100.0f, 5.0f, k, planes), 0.2, 1e-6);    // clamped at 2 * near
    CHECK_NEAR(ost_domain::zoom_camera_distance(100.0f, -1000.0f, k, planes), 5000.0, 1e-3); // clamped at 0.5 * far
    // the old fixed range is gone: 600 is a legal distance for any scene, 0.5 is a legal distance for a small one
    CHECK_NEAR(ost_domain::zoom_camera_distance(600.0f, 0.0f, k, planes), 600.0, 1e-3);
    CHECK_NEAR(ost_domain::zoom_camera_distance(0.5f, 0.0f, k, planes), 0.5, 1e-6);
    // a bigger scene allows a bigger distance: 20000 -> limits 40 .. 200000
    const CameraClipPlanes big = ost_domain::camera_clip_planes_for_extent(20000.0f);
    CHECK_NEAR(ost_domain::zoom_camera_distance(150000.0f, 0.0f, k, big), 150000.0, 1e-1);
    CHECK_NEAR(ost_domain::zoom_camera_distance(10.0f, 0.0f, k, big), 40.0, 1e-4);
}
