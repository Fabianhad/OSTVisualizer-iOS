// CAM-01 / CAM-03: how the 3D camera frames the scene on Fit / Reset.
//
// Windows: cpp_extensions/src/renderer/renderer.cpp Camera::show_object (lines 317-334) and Camera::configure_clip_planes:
//   size     = largest side of the bounds box (100 when it is below 1)
//   target   = centre of the box
//   position = centre + (0, size, 1.5 * size)        (Z up: the camera is on the +y side of the plan, above it)
//   fov 45 degrees. A scene without bounds is never framed: ost_visualizer/presentation/components/mesh_view.py reset_view
//   (817-826) returns without touching the camera (tests/presentation/components/test_mesh_view.py:1648).
// The expected positions below are the results of running the shipped Windows ost_renderer.Camera.show_object read-only
// (scratch script gen_camera_frame.py, Windows tree HEAD 6d4786f), float32 values printed as Python repr.
// Unit / axis mapping: the iOS meshes are the Windows meshes with the axes permuted (X, Y, Z) -> (X, Z, -Y)
// (TakeoffMeshBuilder.hpp), so a Windows point (x, y, z) is the renderer point (x, z, -y) and distances are unchanged.
#include "TestHarness.hpp"

#include "Domain/CameraFraming.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <string>

using ost_domain::CameraPose;
using ost_domain::SceneBounds;

namespace
{
    struct WindowsFrame
    {
        const char *label;
        double box_min[3];
        double box_max[3];
        double position[3]; // Camera.position after show_object (Z up)
        double target[3];   // Camera.target after show_object (Z up)
    };

    const WindowsFrame kFrames[] = {
        // the bounds of the item (a real plan) and its pinned elevation of 56.31 degrees (test_renderer_boundaries.py)
        {"plan", {-3306.0, 77.0, 0.0}, {-528.0, 2418.0, 332.0}, {-1917.0, 4025.5, 4333.0}, {-1917.0, 1247.5, 166.0}},
        {"box 100x50x20", {0.0, 0.0, 0.0}, {100.0, 50.0, 20.0}, {50.0, 125.0, 160.0}, {50.0, 25.0, 10.0}},
        // a size below 1 counts as 100
        {"size 0.5", {0.0, 0.0, 0.0}, {0.5, 0.4, 0.3},
         {0.25, 100.19999694824219, 150.14999389648438}, {0.25, 0.20000000298023224, 0.15000000596046448}},
        {"size 0.999", {0.0, 0.0, 0.0}, {0.999, 0.2, 0.1},
         {0.49950000643730164, 100.0999984741211, 150.0500030517578}, {0.49950000643730164, 0.10000000149011612, 0.05000000074505806}},
        // a size of exactly 1 stays 1
        {"size 1", {0.0, 0.0, 0.0}, {1.0, 0.5, 0.5}, {0.5, 1.25, 1.75}, {0.5, 0.25, 0.25}},
        {"offset box", {10.0, -20.0, 5.0}, {410.0, 80.0, 25.0}, {210.0, 430.0, 615.0}, {210.0, 30.0, 15.0}},
        {"huge", {-1.0e6, 0.0, 0.0}, {1.0e6, 10.0, 10.0}, {0.0, 2000005.0, 3000005.0}, {0.0, 5.0, 5.0}},
        // a box that is a single point is not empty: size 0 counts as 100
        {"point", {5.0, 5.0, 5.0}, {5.0, 5.0, 5.0}, {5.0, 105.0, 155.0}, {5.0, 5.0, 5.0}},
    };

    // the Windows box (Z up) as the renderer sees it (Y up): (x, y, z) -> (x, z, -y)
    SceneBounds renderer_bounds(const WindowsFrame &f)
    {
        SceneBounds b;
        b.min_x = static_cast<float>(f.box_min[0]);
        b.max_x = static_cast<float>(f.box_max[0]);
        b.min_y = static_cast<float>(f.box_min[2]);
        b.max_y = static_cast<float>(f.box_max[2]);
        b.min_z = static_cast<float>(-f.box_max[1]);
        b.max_z = static_cast<float>(-f.box_min[1]);
        return b;
    }

    // the eye of the renderer (Renderer.mm renderFrameWithTime): target + distance * (cos p sin yaw, sin p, cos p cos yaw)
    void eye_of(const CameraPose &pose, double eye[3])
    {
        const double cos_p = std::cos(static_cast<double>(pose.pitch));
        eye[0] = pose.target_x + pose.distance * cos_p * std::sin(static_cast<double>(pose.yaw));
        eye[1] = pose.target_y + pose.distance * std::sin(static_cast<double>(pose.pitch));
        eye[2] = pose.target_z + pose.distance * cos_p * std::cos(static_cast<double>(pose.yaw));
    }

    void check_near(double actual, double expected, double scale)
    {
        // float32 maths: a relative tolerance of the size of the scene
        CHECK_NEAR(actual, expected, 2e-5 * scale + 1e-4);
    }
}

// Test plan of CAM-01: target, eye and distance of Fit / Reset equal Camera.show_object for every bounds box.
TEST(CameraFraming, FitFramesTheSceneLikeShowObject)
{
    const CameraPose before; // the camera before the fit: anywhere
    for (const WindowsFrame &f : kFrames)
    {
        SCOPED_CONTEXT(std::string("scene ") + f.label);
        const CameraPose pose = ost_domain::frame_camera(before, renderer_bounds(f));

        const double scale = std::fabs(f.position[0]) + std::fabs(f.position[1]) + std::fabs(f.position[2]) + 1.0;
        // target: the centre of the box, (x, y, z) -> (x, z, -y)
        check_near(pose.target_x, f.target[0], scale);
        check_near(pose.target_y, f.target[2], scale);
        check_near(pose.target_z, -f.target[1], scale);
        // eye: the Windows position mapped the same way
        double eye[3];
        eye_of(pose, eye);
        check_near(eye[0], f.position[0], scale);
        check_near(eye[1], f.position[2], scale);
        check_near(eye[2], -f.position[1], scale);
        // distance = |position - target| of Windows
        const double dx = f.position[0] - f.target[0];
        const double dy = f.position[1] - f.target[1];
        const double dz = f.position[2] - f.target[2];
        check_near(pose.distance, std::sqrt(dx * dx + dy * dy + dz * dz), scale);
    }
}

// Test plan: the bounds of the item give a distance of 5008.111 and an elevation of 56.31 degrees (pinned by
// test_renderer_boundaries); the iOS pitch is the same angle above the plan, the yaw looks from the page-bottom side.
TEST(CameraFraming, TheElevationAndDistanceMatchTheWindowsView)
{
    const WindowsFrame &plan = kFrames[0];
    const CameraPose pose = ost_domain::frame_camera(CameraPose(), renderer_bounds(plan));
    CHECK_NEAR(pose.distance, 5008.111, 1e-3);
    CHECK_NEAR(static_cast<double>(pose.pitch) * 180.0 / 3.14159265358979323846, 56.31, 1e-2);
    // distance = 1.80278 * size for a box whose largest side is `size`
    const CameraPose unit = ost_domain::frame_camera(
        CameraPose(), SceneBounds{0.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f});
    CHECK_NEAR(unit.distance, 180.278, 1e-3);
}

// Test plan of CAM-03: Reset with no renderable content (no bounds) leaves yaw, pitch, distance and target exactly as they
// were (mesh_view.reset_view returns without touching the camera).
TEST(CameraFraming, ResetWithoutContentLeavesTheCameraUntouched)
{
    CameraPose current;
    current.target_x = 12.5f;
    current.target_y = -3.0f;
    current.target_z = 77.0f;
    current.distance = 321.0f;
    current.yaw = 1.25f;
    current.pitch = -0.4f;
    const CameraPose after = ost_domain::frame_camera(current, std::nullopt);
    CHECK_EQ(after.target_x, current.target_x);
    CHECK_EQ(after.target_y, current.target_y);
    CHECK_EQ(after.target_z, current.target_z);
    CHECK_EQ(after.distance, current.distance);
    CHECK_EQ(after.yaw, current.yaw);
    CHECK_EQ(after.pitch, current.pitch);
}

// Bounds that are not finite numbers cannot be framed: the camera stays as it is (defined here so that the result does not
// depend on how a platform orders NaN in std::max).
TEST(CameraFraming, NonFiniteBoundsLeaveTheCameraUntouched)
{
    CameraPose current;
    current.distance = 40.0f;
    current.yaw = 0.7f;
    SceneBounds nan_bounds{0.0f, 0.0f, 0.0f, std::nanf(""), 1.0f, 1.0f};
    SceneBounds inf_bounds{0.0f, 0.0f, 0.0f, 1.0f, 1.0f, std::numeric_limits<float>::infinity()};
    for (const SceneBounds &bounds : {nan_bounds, inf_bounds})
    {
        const CameraPose after = ost_domain::frame_camera(current, bounds);
        CHECK_EQ(after.distance, current.distance);
        CHECK_EQ(after.yaw, current.yaw);
        CHECK_EQ(after.pitch, current.pitch);
        CHECK_EQ(after.target_x, current.target_x);
    }
}

// With content the previous pose does not matter: the frame depends on the bounds only.
TEST(CameraFraming, TheFrameDoesNotDependOnThePreviousPose)
{
    CameraPose a;
    CameraPose b;
    b.target_x = 99.0f;
    b.distance = 1.0f;
    b.yaw = -2.0f;
    b.pitch = 1.4f;
    const SceneBounds bounds = renderer_bounds(kFrames[1]);
    const CameraPose fa = ost_domain::frame_camera(a, bounds);
    const CameraPose fb = ost_domain::frame_camera(b, bounds);
    CHECK_EQ(fa.target_x, fb.target_x);
    CHECK_EQ(fa.target_y, fb.target_y);
    CHECK_EQ(fa.target_z, fb.target_z);
    CHECK_EQ(fa.distance, fb.distance);
    CHECK_EQ(fa.yaw, fb.yaw);
    CHECK_EQ(fa.pitch, fb.pitch);
}
