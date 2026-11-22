// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// How far the 3D camera may zoom in and out, relative to the scene. Port of the Windows native camera
// (cpp_extensions/src/renderer/renderer.cpp Camera::zoom lines 291-301 and Camera::configure_clip_planes lines 303-315),
// extracted from CameraController._handlePinch (which clamped the distance to a fixed 1 .. 500).
//
// Units: the iOS meshes are the Windows 3D meshes in the same raw OST units, only the axes are permuted (TakeoffMeshBuilder.hpp:
// (X, Y, Z) -> (X, Z, -Y)), so the largest side of the iOS scene bounds is the Windows `size` and the limits below hold in iOS
// scene units without any conversion.
#pragma once
#include <algorithm>

namespace ost_domain
{

    // Windows Camera::near_plane / far_plane before a scene is configured (renderer.hpp:69-70); an empty scene keeps them.
    constexpr float kDefaultCameraNearPlane = 0.1f;
    constexpr float kDefaultCameraFarPlane = 10000.0f;
    // Windows Camera::zoom: the distance changes by delta * distance * 0.001 per wheel unit.
    constexpr float kWindowsWheelZoomRate = 0.001f;
    // iOS pinch: the distance changes by delta * distance * 0.5 per unit of change of the pinch scale. Windows has a wheel
    // and no pinch, so this rate is a platform adaptation; the limits are not.
    constexpr float kPinchZoomRate = 0.5f;

    struct CameraClipPlanes
    {
        float near_plane = kDefaultCameraNearPlane;
        float far_plane = kDefaultCameraFarPlane;

        // Windows Camera::zoom clamps the distance to [2 * near_plane, 0.5 * far_plane].
        float min_distance() const { return near_plane * 2.0f; }
        float max_distance() const { return far_plane * 0.5f; }
    };

    // The planes of a camera that has no scene bounds (an empty scene).
    inline CameraClipPlanes default_camera_clip_planes() { return CameraClipPlanes(); }

    // Windows Camera::configure_clip_planes(bounds) for a non-empty scene: `max_extent` is the largest side of the scene bounds
    // box (a size below 1 counts as 100); near = max(0.1, size * 0.001), far = max(10000, size * 20).
    inline CameraClipPlanes camera_clip_planes_for_extent(float max_extent)
    {
        float size = max_extent;
        if (size < 1.0f)
            size = 100.0f;
        CameraClipPlanes planes;
        planes.near_plane = std::max(0.1f, size * 0.001f);
        planes.far_plane = std::max(10000.0f, size * 20.0f);
        return planes;
    }

    // glm::clamp(distance, 2 * near_plane, 0.5 * far_plane) = min(max(distance, lo), hi), written out so that a NaN or an
    // inverted range behaves exactly like the Windows build instead of depending on the standard library.
    inline float clamp_camera_distance(float distance, const CameraClipPlanes &planes)
    {
        const float lo = planes.min_distance();
        const float hi = planes.max_distance();
        const float raised = (distance < lo) ? lo : distance;
        return (hi < raised) ? hi : raised;
    }

    // Windows Camera::zoom for a camera that looks at its target from `distance`: nothing happens below a distance of 0.001,
    // otherwise new_distance = clamp(distance - delta * distance * rate, 2 * near, 0.5 * far). The Windows rate is
    // kWindowsWheelZoomRate; the iOS pinch passes kPinchZoomRate with the change of the pinch scale as delta.
    inline float zoom_camera_distance(float distance, float delta, float rate, const CameraClipPlanes &planes)
    {
        if (distance < 0.001f)
            return distance;
        const float zoom_amount = delta * distance * rate;
        return clamp_camera_distance(distance - zoom_amount, planes);
    }

}
