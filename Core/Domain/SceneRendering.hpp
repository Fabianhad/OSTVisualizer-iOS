// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// Two decisions of the 3D renderer that decide whether a mesh can be seen at all, extracted from Rendering/Renderer.mm
// (renderFrameWithTime) so that they can be tested on the host:
//
//  1. WHICH PASS draws a mesh. A mesh of alpha 1 goes through the opaque pass. A mesh with an alpha below kTransparentAlpha (the
//     Windows renderer.cpp mesh.transparent = color.a < 0.99) goes through the weighted-blended order independent transparency
//     passes (accum, revealage, composite); when those cannot be used it is drawn with plain alpha blending instead of being
//     dropped: a mesh is never left out of every pass.
//  2. THE CLIP PLANES. The near plane scales with the camera distance; the far plane must reach the whole scene wherever the camera
//     is, whatever the units. The meshes are in raw OST units like Windows (MSH-01, scenes thousands of units wide), and the
//     renderer starts with a camera at distance 12 and a scene radius of 15: a far plane that only knew the radius of the LAST fit
//     (60 at the start) cut a scene that appears without a refit completely off. Windows configure_clip_planes follows the live
//     scene bounds (it is called from Camera::show_object and restore_state on every scene update); here the far plane reaches at
//     least the farthest corner of the live bounds. Whether the camera LOOKS at such a scene is the camera rule (CAM-02, CAM-03:
//     when the camera is refit), which this does not change.
#pragma once
#include "CameraFraming.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace ost_domain
{

    // Windows renderer.cpp:464 `mesh.transparent = data.color.a < 0.99f`.
    constexpr float kTransparentAlpha = 0.99f;

    inline bool is_transparent_alpha(float alpha) { return alpha < kTransparentAlpha; }

    enum class MeshPass
    {
        Opaque,    // depth test and depth write, colour alpha 1
        Oit,       // weighted blended OIT (accum + revealage + composite), tests against the opaque depth
        AlphaBlend // plain alpha blending after the opaque meshes: the fallback when the OIT passes cannot be used
    };

    // The pass of one mesh. `oit_available`: the OIT framebuffers exist (a failed shader or pipeline is fatal in bgfx, so that case
    // cannot silently drop meshes; only the framebuffers can be invalid).
    inline MeshPass mesh_pass_for_alpha(float alpha, bool oit_available)
    {
        if (!is_transparent_alpha(alpha))
            return MeshPass::Opaque;
        return oit_available ? MeshPass::Oit : MeshPass::AlphaBlend;
    }

    // The position of the eye of an orbit camera (renderer space, Y up): target + distance * (cos(pitch) sin(yaw), sin(pitch),
    // cos(pitch) cos(yaw)), as Renderer.mm renderFrameWithTime has always computed it.
    struct EyePosition
    {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };

    inline EyePosition camera_eye(const CameraPose &pose)
    {
        EyePosition eye;
        eye.x = pose.target_x + std::cos(pose.pitch) * std::sin(pose.yaw) * pose.distance;
        eye.y = pose.target_y + std::sin(pose.pitch) * pose.distance;
        eye.z = pose.target_z + std::cos(pose.pitch) * std::cos(pose.yaw) * pose.distance;
        return eye;
    }

    struct RenderClipPlanes
    {
        float near_plane = 0.1f;
        float far_plane = 100.0f;
    };

    // The distance from the eye to the farthest corner of the bounds box.
    inline float farthest_corner_distance(const EyePosition &eye, const SceneBounds &bounds)
    {
        float farthest = 0.0f;
        for (int corner = 0; corner < 8; ++corner)
        {
            const float dx = ((corner & 1) ? bounds.max_x : bounds.min_x) - eye.x;
            const float dy = ((corner & 2) ? bounds.max_y : bounds.min_y) - eye.y;
            const float dz = ((corner & 4) ? bounds.max_z : bounds.min_z) - eye.z;
            farthest = std::max(farthest, std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        return farthest;
    }

    // The clip planes of one frame. `scene_radius` is the radius of the last camera fit (the renderer keeps it), `bounds` the live
    // bounds of the meshes in the scene (none when the scene is empty).
    //  * near = max(distance * 0.005, 0.01); 0.1 for an empty scene. Unchanged.
    //  * far = max(distance + 2 r, 4 r) from the radius of the last fit, as before, and at least the farthest corner of the live
    //    bounds (plus 0.1 %): at a fit pose that corner is always inside the old far plane, so nothing changes there; a scene that
    //    appears without a refit, or a camera that has been moved away, no longer loses everything behind the far plane. An empty
    //    scene has at least 100. Bounds that are not finite are ignored.
    inline RenderClipPlanes render_clip_planes(const CameraPose &pose, float scene_radius, const std::optional<SceneBounds> &bounds)
    {
        const bool has_meshes = bounds.has_value();
        RenderClipPlanes planes;
        planes.near_plane = has_meshes ? std::max(pose.distance * 0.005f, 0.01f) : 0.1f;
        planes.far_plane = std::max(pose.distance + scene_radius * 2.0f, scene_radius * 4.0f);
        if (planes.far_plane < 100.0f && !has_meshes)
            planes.far_plane = 100.0f;
        if (has_meshes && bounds->is_finite())
            planes.far_plane = std::max(planes.far_plane, farthest_corner_distance(camera_eye(pose), *bounds) * 1.001f);
        return planes;
    }

}
