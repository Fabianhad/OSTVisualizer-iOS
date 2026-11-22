// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// How the 3D camera frames the scene (Fit / Reset), extracted from Renderer.fitCameraToMeshes and CameraController.fitScene
// (CAM-01, CAM-03). Port of the Windows native camera (cpp_extensions/src/renderer/renderer.cpp Camera::show_object, lines
// 317-334, with Camera::configure_clip_planes for the size) and of mesh_view.reset_view (mesh_view.py 817-826).
//
// Windows (Z up): size = the largest side of the scene bounds (100 when it is below 1); target = the centre of the bounds;
// position = centre + (0, size, 1.5 * size), i.e. the camera is on the +y (page-bottom) side, above the plan: distance
// size * sqrt(1 + 1.5^2) = 1.80278 * size, elevation atan(1.5) = 56.31 degrees. A scene without bounds is not framed at all
// (reset_view returns before it touches the camera).
//
// Renderer space (Y up): the iOS meshes are the Windows meshes with the axes permuted (X, Y, Z) -> (X, Z, -Y)
// (TakeoffMeshBuilder.hpp), which keeps every distance. The Windows offset (0, size, 1.5 * size) is the renderer offset
// (0, 1.5 * size, -size); the renderer's eye is target + distance * (cos(pitch) sin(yaw), sin(pitch), cos(pitch) cos(yaw)),
// so that offset is yaw = pi and pitch = atan(1.5 / 1).
#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

namespace ost_domain
{

    // The orbit camera of the renderer: it looks at `target` from `distance`, `yaw` / `pitch` in radians.
    struct CameraPose
    {
        float target_x = 0.0f, target_y = 0.0f, target_z = 0.0f;
        float distance = 12.0f;
        float yaw = 0.5f;
        float pitch = 0.35f;
    };

    // The bounds box of everything the 3D view shows (renderer space, Y up).
    struct SceneBounds
    {
        float min_x = 0.0f, min_y = 0.0f, min_z = 0.0f;
        float max_x = 0.0f, max_y = 0.0f, max_z = 0.0f;

        // The largest side of the box.
        float max_extent() const { return std::max({max_x - min_x, max_y - min_y, max_z - min_z}); }

        bool is_finite() const
        {
            return std::isfinite(min_x) && std::isfinite(min_y) && std::isfinite(min_z) && std::isfinite(max_x) &&
                   std::isfinite(max_y) && std::isfinite(max_z);
        }
    };

    // Camera::show_object looks from this side and height: yaw pi (from the page-bottom side), pitch atan(1.5).
    constexpr float kShowObjectYaw = 3.14159265358979323846f;
    constexpr float kShowObjectPitchRatio = 1.5f; // height of the eye over the horizontal offset: 1.5 * size / size

    // The pose of Fit / Reset. With bounds the camera is framed like Camera::show_object whatever the previous pose was;
    // without bounds (nothing is shown) or with bounds that are not finite numbers the camera stays exactly as it is
    // (Windows reset_view returns; non-finite bounds cannot be framed).
    inline CameraPose frame_camera(const CameraPose &current, const std::optional<SceneBounds> &bounds)
    {
        if (!bounds || !bounds->is_finite())
            return current;
        float size = bounds->max_extent();
        if (size < 1.0f)
            size = 100.0f;
        CameraPose pose;
        pose.target_x = (bounds->min_x + bounds->max_x) * 0.5f;
        pose.target_y = (bounds->min_y + bounds->max_y) * 0.5f;
        pose.target_z = (bounds->min_z + bounds->max_z) * 0.5f;
        pose.distance = size * std::sqrt(1.0f + kShowObjectPitchRatio * kShowObjectPitchRatio);
        pose.yaw = kShowObjectYaw;
        pose.pitch = std::atan2(kShowObjectPitchRatio, 1.0f);
        return pose;
    }

}
