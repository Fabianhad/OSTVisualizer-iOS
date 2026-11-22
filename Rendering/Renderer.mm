#import "Renderer.h"
#include "../Core/Domain/CameraFraming.hpp"
#include "../Core/Domain/SceneRendering.hpp"
#include "../Core/Geometry/feature_edges.hpp"
#include "MathUtils.hpp"
#include "MeshLoader.hpp"
#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "Shaders/generated/fs_mesh.h"
#include "Shaders/generated/fs_meshid.h"
#include "Shaders/generated/fs_oit_accum.h"
#include "Shaders/generated/fs_oit_composite.h"
#include "Shaders/generated/fs_oit_revealage.h"
#include "Shaders/generated/fs_selection_post.h"
#include "Shaders/generated/vs_fullscreen.h"
#include "Shaders/generated/vs_mesh.h"

static const uint32_t kBgColor = 0x353535FF;

enum ViewId : bgfx::ViewId {
  kViewOpaque = 0,
  kViewOITAccum = 1,
  kViewOITRevealage = 2,
  kViewOITComposite = 3,
  kViewWireframe = 4,
  kViewSelMask = 5,
  kViewMeshId = 6,
  kViewFinal = 7,
};

struct GpuMesh {
  bgfx::VertexBufferHandle vbh;
  bgfx::IndexBufferHandle ibh;
  bgfx::IndexBufferHandle lineIbh;
  Color color;
  int meshIndex;
};

struct PosUV {
  float px, py, pz, u, v;
};
static const PosUV kFsQuadVerts[] = {
    {-1, -1, 0, 0, 1},
    {1, -1, 0, 1, 1},
    {1, 1, 0, 1, 0},
    {-1, 1, 0, 0, 0},
};
static const uint16_t kFsQuadIndices[] = {0, 1, 2, 0, 2, 3};

static bgfx::ShaderHandle loadShader(const uint8_t *data, uint32_t size) {
  const bgfx::Memory *mem = bgfx::copy(data, size);
  return bgfx::createShader(mem);
}

@implementation Renderer {
  int _width, _height;
  float _sceneRadius;
  __weak UIView *_hostView;

  bgfx::VertexLayout _meshLayout;
  bgfx::VertexLayout _fsLayout;
  std::vector<GpuMesh> _gpuMeshes;
  std::vector<LoadedMesh> _meshes;
  std::vector<std::string> _meshConditionUIDs;
  std::vector<std::string> _meshTakeoffUIDs;

  bgfx::VertexBufferHandle _fsQuadVbh;
  bgfx::IndexBufferHandle _fsQuadIbh;

  bgfx::ProgramHandle _progMesh;
  bgfx::ProgramHandle _progOITAccum;
  bgfx::ProgramHandle _progOITRevealage;
  bgfx::ProgramHandle _progOITComposite;
  bgfx::ProgramHandle _progMeshId;
  bgfx::ProgramHandle _progSelectionPost;

  bgfx::UniformHandle _u_meshColor;
  bgfx::UniformHandle _u_lightParams;
  bgfx::UniformHandle _u_eyePos;
  bgfx::UniformHandle _u_meshId;
  bgfx::UniformHandle _s_accumTex;
  bgfx::UniformHandle _s_revealTex;
  bgfx::UniformHandle _s_sceneTex;
  bgfx::UniformHandle _s_selMaskTex;

  bgfx::TextureHandle _sceneTex;
  bgfx::TextureHandle _sceneDepthTex;
  bgfx::FrameBufferHandle _sceneFB;

  bgfx::TextureHandle _selMaskTex;
  bgfx::FrameBufferHandle _selMaskFB;

  bgfx::TextureHandle _oitAccumTex;
  bgfx::TextureHandle _oitRevealTex;
  bgfx::FrameBufferHandle _oitAccumFB;
  bgfx::FrameBufferHandle _oitRevealFB;

  bgfx::TextureHandle _pickTex;
  bgfx::TextureHandle _pickDepthTex;
  bgfx::TextureHandle _pickReadbackTex;
  bgfx::FrameBufferHandle _pickFB;
  std::vector<uint8_t> _pickReadback;

  bool _bgfxReady;
  bool _gpuMeshesDirty;
}

- (instancetype)initWithView:(UIView *)hostView {
  self = [super init];
  if (!self)
    return nil;

  _hostView = hostView;
  _cameraYaw = 0.5f;
  _cameraPitch = 0.35f;
  _cameraDistance = 12.0f;
  _cameraTargetX = 0;
  _cameraTargetY = 1.0f;
  _cameraTargetZ = 0;
  _sceneRadius = 15.0f;

  return self;
}

- (void)startBgfx {
  if (_bgfxReady)
    return;

  UIView *hostView = _hostView;
  if (!hostView)
    return;

  CGFloat scale = [UIScreen mainScreen].scale;
  _width = (int)(hostView.bounds.size.width * scale);
  _height = (int)(hostView.bounds.size.height * scale);
  if (_width < 1)
    _width = 1;
  if (_height < 1)
    _height = 1;

  bgfx::Init init;
  init.platformData.nwh = (__bridge void *)hostView.layer;
  init.type = bgfx::RendererType::Metal;
  init.resolution.width = (uint32_t)_width;
  init.resolution.height = (uint32_t)_height;
  init.resolution.reset = BGFX_RESET_NONE;

  if (!bgfx::init(init)) {
    return;
  }
  _bgfxReady = true;
  bgfx::setDebug(BGFX_DEBUG_NONE);

  _meshLayout.begin()
      .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
      .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
      .end();

  _fsLayout.begin()
      .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
      .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
      .end();

  bgfx::ShaderHandle vsMesh = loadShader(vs_mesh_bin, sizeof(vs_mesh_bin));
  bgfx::ShaderHandle fsMesh = loadShader(fs_mesh_bin, sizeof(fs_mesh_bin));
  _progMesh = bgfx::createProgram(vsMesh, fsMesh, true);

  bgfx::ShaderHandle vsMesh2 = loadShader(vs_mesh_bin, sizeof(vs_mesh_bin));
  bgfx::ShaderHandle fsOitA =
      loadShader(fs_oit_accum_bin, sizeof(fs_oit_accum_bin));
  _progOITAccum = bgfx::createProgram(vsMesh2, fsOitA, true);

  bgfx::ShaderHandle vsMesh3 = loadShader(vs_mesh_bin, sizeof(vs_mesh_bin));
  bgfx::ShaderHandle fsOitR =
      loadShader(fs_oit_revealage_bin, sizeof(fs_oit_revealage_bin));
  _progOITRevealage = bgfx::createProgram(vsMesh3, fsOitR, true);

  bgfx::ShaderHandle vsFs =
      loadShader(vs_fullscreen_bin, sizeof(vs_fullscreen_bin));
  bgfx::ShaderHandle fsComp =
      loadShader(fs_oit_composite_bin, sizeof(fs_oit_composite_bin));
  _progOITComposite = bgfx::createProgram(vsFs, fsComp, true);

  bgfx::ShaderHandle vsMesh4 = loadShader(vs_mesh_bin, sizeof(vs_mesh_bin));
  bgfx::ShaderHandle fsMid = loadShader(fs_meshid_bin, sizeof(fs_meshid_bin));
  _progMeshId = bgfx::createProgram(vsMesh4, fsMid, true);

  bgfx::ShaderHandle vsFs2 =
      loadShader(vs_fullscreen_bin, sizeof(vs_fullscreen_bin));
  bgfx::ShaderHandle fsSelPost =
      loadShader(fs_selection_post_bin, sizeof(fs_selection_post_bin));
  _progSelectionPost = bgfx::createProgram(vsFs2, fsSelPost, true);

  _u_meshColor = bgfx::createUniform("u_meshColor", bgfx::UniformType::Vec4);
  _u_lightParams =
      bgfx::createUniform("u_lightParams", bgfx::UniformType::Vec4);
  _u_eyePos = bgfx::createUniform("u_eyePos", bgfx::UniformType::Vec4);
  _u_meshId = bgfx::createUniform("u_meshId", bgfx::UniformType::Vec4);
  _s_accumTex = bgfx::createUniform("s_accumTex", bgfx::UniformType::Sampler);
  _s_revealTex = bgfx::createUniform("s_revealTex", bgfx::UniformType::Sampler);
  _s_sceneTex = bgfx::createUniform("s_sceneTex", bgfx::UniformType::Sampler);
  _s_selMaskTex =
      bgfx::createUniform("s_selMaskTex", bgfx::UniformType::Sampler);

  _fsQuadVbh = bgfx::createVertexBuffer(
      bgfx::makeRef(kFsQuadVerts, sizeof(kFsQuadVerts)), _fsLayout);
  _fsQuadIbh = bgfx::createIndexBuffer(
      bgfx::makeRef(kFsQuadIndices, sizeof(kFsQuadIndices)));

  [self _createOffscreenFBs];

  if (!_meshes.empty()) {
    [self _uploadMeshesToGpu];
  }
}

- (void)_createOffscreenFBs {
  uint16_t w = (uint16_t)_width;
  uint16_t h = (uint16_t)_height;
  uint64_t rtFlags = BGFX_TEXTURE_RT;

  _sceneTex = bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8,
                                    rtFlags);
  _sceneDepthTex = bgfx::createTexture2D(w, h, false, 1,
                                         bgfx::TextureFormat::D24S8, rtFlags);
  bgfx::Attachment sceneAtt[2];
  sceneAtt[0].init(_sceneTex);
  sceneAtt[1].init(_sceneDepthTex);
  _sceneFB = bgfx::createFrameBuffer(2, sceneAtt, false);

  _selMaskTex = bgfx::createTexture2D(w, h, false, 1,
                                      bgfx::TextureFormat::RGBA8, rtFlags);
  bgfx::Attachment selAtt[2];
  selAtt[0].init(_selMaskTex);
  selAtt[1].init(_sceneDepthTex);
  _selMaskFB = bgfx::createFrameBuffer(2, selAtt, false);

  _oitAccumTex = bgfx::createTexture2D(w, h, false, 1,
                                       bgfx::TextureFormat::RGBA16F, rtFlags);
  // The transparent passes test against the depth of the OPAQUE scene (kViewOpaque clears and fills _sceneDepthTex every frame, the
  // OIT views neither clear nor write it), like Windows, where accum_fbo and opaque_fbo share one depth texture. A depth texture of
  // their own was never cleared and never written: `DEPTH_TEST_LESS` ran against undefined contents and, with zeros, rejected
  // every fragment of every mesh with an alpha below 0.99.
  bgfx::Attachment oitAccumAtt[2];
  oitAccumAtt[0].init(_oitAccumTex);
  oitAccumAtt[1].init(_sceneDepthTex);
  _oitAccumFB = bgfx::createFrameBuffer(2, oitAccumAtt, false);

  _oitRevealTex =
      bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::R8, rtFlags);
  bgfx::Attachment oitRevAtt[2];
  oitRevAtt[0].init(_oitRevealTex);
  oitRevAtt[1].init(_sceneDepthTex);
  _oitRevealFB = bgfx::createFrameBuffer(2, oitRevAtt, false);

  _pickTex = bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8,
                                   rtFlags | BGFX_TEXTURE_BLIT_DST);
  _pickDepthTex = bgfx::createTexture2D(w, h, false, 1,
                                        bgfx::TextureFormat::D24S8, rtFlags);
  bgfx::Attachment pickAtt[2];
  pickAtt[0].init(_pickTex);
  pickAtt[1].init(_pickDepthTex);
  _pickFB = bgfx::createFrameBuffer(2, pickAtt, false);

  _pickReadbackTex =
      bgfx::createTexture2D(w, h, false, 1, bgfx::TextureFormat::RGBA8,
                            BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);

  _pickReadback.resize((size_t)_width * _height * 4, 0);
}

- (void)_destroyOffscreenFBs {
  if (bgfx::isValid(_sceneFB))
    bgfx::destroy(_sceneFB);
  if (bgfx::isValid(_sceneTex))
    bgfx::destroy(_sceneTex);
  if (bgfx::isValid(_sceneDepthTex))
    bgfx::destroy(_sceneDepthTex);
  if (bgfx::isValid(_selMaskFB))
    bgfx::destroy(_selMaskFB);
  if (bgfx::isValid(_selMaskTex))
    bgfx::destroy(_selMaskTex);
  if (bgfx::isValid(_oitAccumFB))
    bgfx::destroy(_oitAccumFB);
  if (bgfx::isValid(_oitRevealFB))
    bgfx::destroy(_oitRevealFB);
  if (bgfx::isValid(_oitAccumTex))
    bgfx::destroy(_oitAccumTex);
  if (bgfx::isValid(_oitRevealTex))
    bgfx::destroy(_oitRevealTex);
  if (bgfx::isValid(_pickFB))
    bgfx::destroy(_pickFB);
  if (bgfx::isValid(_pickTex))
    bgfx::destroy(_pickTex);
  if (bgfx::isValid(_pickDepthTex))
    bgfx::destroy(_pickDepthTex);
  if (bgfx::isValid(_pickReadbackTex))
    bgfx::destroy(_pickReadbackTex);
  _sceneFB = BGFX_INVALID_HANDLE;
  _selMaskFB = BGFX_INVALID_HANDLE;
  _oitAccumFB = BGFX_INVALID_HANDLE;
  _oitRevealFB = BGFX_INVALID_HANDLE;
  _pickFB = BGFX_INVALID_HANDLE;
}

- (void)resize:(int)width height:(int)height {
  if (!_bgfxReady)
    return;
  if (width == _width && height == _height)
    return;
  _width = width;
  _height = height;
  if (_width < 1)
    _width = 1;
  if (_height < 1)
    _height = 1;
  bgfx::reset((uint32_t)_width, (uint32_t)_height, BGFX_RESET_NONE);
  [self _destroyOffscreenFBs];
  [self _createOffscreenFBs];
}

#pragma mark - Mesh Management

- (void)addMeshWithVertices:(const double *)vertices
                vertexCount:(int)vertexCount
                      faces:(const uint32_t *)faces
                  faceCount:(int)faceCount
                     colorR:(float)r
                          g:(float)g
                          b:(float)b
                      alpha:(float)a
               conditionUID:(NSString *)conditionUID
                 takeoffUID:(NSString *)takeoffUID {
  LoadedMesh mesh = loadMeshFromRaw(vertices, (size_t)vertexCount, faces,
                                    (size_t)faceCount, r, g, b, a);
  if (mesh.empty())
    return;

  _meshes.push_back(std::move(mesh));
  _meshConditionUIDs.push_back(
      conditionUID ? std::string(conditionUID.UTF8String) : "");
  _meshTakeoffUIDs.push_back(takeoffUID ? std::string(takeoffUID.UTF8String)
                                        : "");
  _gpuMeshesDirty = true;
}

- (void)clearMeshes {
  if (_bgfxReady) {
    for (auto &gm : _gpuMeshes) {
      bgfx::destroy(gm.vbh);
      bgfx::destroy(gm.ibh);
      if (bgfx::isValid(gm.lineIbh))
        bgfx::destroy(gm.lineIbh);
    }
  }
  _gpuMeshes.clear();
  _meshes.clear();
  _meshConditionUIDs.clear();
  _meshTakeoffUIDs.clear();
  _gpuMeshesDirty = false;
}

- (void)_uploadMeshesToGpu {
  for (auto &gm : _gpuMeshes) {
    bgfx::destroy(gm.vbh);
    bgfx::destroy(gm.ibh);
    bgfx::destroy(gm.lineIbh);
  }
  _gpuMeshes.clear();

  for (size_t mi = 0; mi < _meshes.size(); mi++) {
    auto &mesh = _meshes[mi];
    int triCount = mesh.triangleCount();
    if (triCount == 0)
      continue;

    size_t vertCount = (size_t)triCount * 3;
    std::vector<float> vdata(vertCount * 6);
    std::vector<uint32_t> idata(vertCount);

    for (int t = 0; t < triCount; t++) {
      Vec3 fn = mesh.faceNormals[t];
      size_t base = (size_t)t * 3;

      for (int v = 0; v < 3; v++) {
        uint32_t idx = mesh.indices[t * 3 + v];
        size_t off = (base + v) * 6;
        vdata[off + 0] = mesh.positions[idx].x;
        vdata[off + 1] = mesh.positions[idx].y;
        vdata[off + 2] = mesh.positions[idx].z;
        vdata[off + 3] = fn.x;
        vdata[off + 4] = fn.y;
        vdata[off + 5] = fn.z;
        idata[base + v] = (uint32_t)(base + v);
      }
    }

    ost_geometry::CppMeshData cppMesh;
    cppMesh.vertices.resize(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); i++) {
      cppMesh.vertices[i] = {(double)mesh.positions[i].x,
                             (double)mesh.positions[i].y,
                             (double)mesh.positions[i].z};
    }
    cppMesh.faces.resize(triCount);
    for (int t = 0; t < triCount; t++) {
      cppMesh.faces[t] = {mesh.indices[t * 3], mesh.indices[t * 3 + 1],
                          mesh.indices[t * 3 + 2]};
    }
    auto featureEdges = ost_geometry::extract_feature_edges(cppMesh);

    std::unordered_multimap<uint64_t, uint32_t> vertFaceMap;
    for (int t = 0; t < triCount; t++) {
      size_t base = (size_t)t * 3;
      for (int v = 0; v < 3; v++) {
        uint32_t origIdx = mesh.indices[t * 3 + v];
        vertFaceMap.emplace(origIdx, (uint32_t)(base + v));
      }
    }

    std::vector<uint32_t> lineData;
    for (auto &edge : featureEdges) {
      auto it0 = vertFaceMap.find(edge[0]);
      auto it1 = vertFaceMap.find(edge[1]);
      if (it0 != vertFaceMap.end() && it1 != vertFaceMap.end()) {
        lineData.push_back(it0->second);
        lineData.push_back(it1->second);
      }
    }

    GpuMesh gm;
    gm.vbh = bgfx::createVertexBuffer(
        bgfx::copy(vdata.data(), (uint32_t)(vdata.size() * sizeof(float))),
        _meshLayout);
    gm.ibh = bgfx::createIndexBuffer(
        bgfx::copy(idata.data(), (uint32_t)(idata.size() * sizeof(uint32_t))),
        BGFX_BUFFER_INDEX32);
    if (!lineData.empty()) {
      gm.lineIbh = bgfx::createIndexBuffer(
          bgfx::copy(lineData.data(),
                     (uint32_t)(lineData.size() * sizeof(uint32_t))),
          BGFX_BUFFER_INDEX32);
    } else {
      gm.lineIbh = BGFX_INVALID_HANDLE;
    }
    gm.color = mesh.color;
    gm.meshIndex = (int)mi;
    _gpuMeshes.push_back(gm);
  }
  _gpuMeshesDirty = false;
}

// The bounds box of all meshes in the scene; NO when there are none.
- (BOOL)_sceneBounds:(ost_domain::SceneBounds *)out {
  if (_meshes.empty())
    return NO;
  Vec3 totalMin = _meshes[0].boundsMin;
  Vec3 totalMax = _meshes[0].boundsMax;
  for (size_t i = 1; i < _meshes.size(); i++) {
    totalMin.x = fminf(totalMin.x, _meshes[i].boundsMin.x);
    totalMin.y = fminf(totalMin.y, _meshes[i].boundsMin.y);
    totalMin.z = fminf(totalMin.z, _meshes[i].boundsMin.z);
    totalMax.x = fmaxf(totalMax.x, _meshes[i].boundsMax.x);
    totalMax.y = fmaxf(totalMax.y, _meshes[i].boundsMax.y);
    totalMax.z = fmaxf(totalMax.z, _meshes[i].boundsMax.z);
  }
  out->min_x = totalMin.x;
  out->min_y = totalMin.y;
  out->min_z = totalMin.z;
  out->max_x = totalMax.x;
  out->max_y = totalMax.y;
  out->max_z = totalMax.z;
  return YES;
}

- (float)sceneMaxExtent {
  ost_domain::SceneBounds bounds;
  if (![self _sceneBounds:&bounds])
    return 0.0f;
  return bounds.max_extent();
}

- (void)fitCameraToMeshes {
  // Windows Camera.show_object (CAM-01); without any mesh nothing is framed and the camera stays as it is (CAM-03).
  ost_domain::SceneBounds bounds;
  if (![self _sceneBounds:&bounds])
    return;
  ost_domain::CameraPose pose;
  pose.target_x = _cameraTargetX;
  pose.target_y = _cameraTargetY;
  pose.target_z = _cameraTargetZ;
  pose.distance = _cameraDistance;
  pose.yaw = _cameraYaw;
  pose.pitch = _cameraPitch;
  pose = ost_domain::frame_camera(pose, bounds);
  _cameraTargetX = pose.target_x;
  _cameraTargetY = pose.target_y;
  _cameraTargetZ = pose.target_z;
  _cameraDistance = pose.distance;
  _cameraYaw = pose.yaw;
  _cameraPitch = pose.pitch;
  if (bounds.is_finite())
    _sceneRadius = bounds.max_extent() * 0.5f;
}

- (NSInteger)meshTriangleCount {
  NSInteger total = 0;
  for (auto &m : _meshes)
    total += m.triangleCount();
  return total;
}

#pragma mark - Picking

- (NSString *)conditionUIDAtScreenX:(CGFloat)x y:(CGFloat)y {
  int idx = [self _pickIndexAtX:x y:y];
  if (idx < 0 || (size_t)idx >= _meshConditionUIDs.size())
    return nil;
  return [NSString stringWithUTF8String:_meshConditionUIDs[idx].c_str()];
}

- (NSString *)takeoffUIDAtScreenX:(CGFloat)x y:(CGFloat)y {
  int idx = [self _pickIndexAtX:x y:y];
  if (idx < 0 || (size_t)idx >= _meshTakeoffUIDs.size())
    return nil;
  return [NSString stringWithUTF8String:_meshTakeoffUIDs[idx].c_str()];
}

- (int)_pickIndexAtX:(CGFloat)x y:(CGFloat)y {
  int px = (int)x;
  int py = (int)y;
  if (px < 0 || px >= _width || py < 0 || py >= _height)
    return -1;

  size_t offset = ((size_t)py * _width + px) * 4;
  if (offset + 3 >= _pickReadback.size())
    return -1;

  uint8_t r = _pickReadback[offset];
  uint8_t g = _pickReadback[offset + 1];
  int32_t meshIdx = (int32_t)r | ((int32_t)g << 8);
  if (meshIdx == 0)
    return -1;
  return meshIdx - 1;
}

#pragma mark - Render

- (void)renderFrameWithTime:(float)time {
  if (!_bgfxReady)
    return;

  if (_gpuMeshesDirty) {
    [self _uploadMeshesToGpu];
  }

  bool hasMeshes = !_gpuMeshes.empty();

  ost_domain::CameraPose pose;
  pose.target_x = _cameraTargetX;
  pose.target_y = _cameraTargetY;
  pose.target_z = _cameraTargetZ;
  pose.distance = _cameraDistance;
  pose.yaw = _cameraYaw;
  pose.pitch = _cameraPitch;
  const ost_domain::EyePosition eyePosition = ost_domain::camera_eye(pose);
  Vec3 target(_cameraTargetX, _cameraTargetY, _cameraTargetZ);
  Vec3 eye(eyePosition.x, eyePosition.y, eyePosition.z);

  // The clip planes follow the LIVE scene bounds (Windows configure_clip_planes): the far plane reaches the farthest corner of
  // the meshes now in the scene, so a scene that appears without a refit is not cut off (Core/Domain/SceneRendering.hpp).
  std::optional<ost_domain::SceneBounds> liveBounds;
  if (hasMeshes) {
    ost_domain::SceneBounds bounds;
    if ([self _sceneBounds:&bounds])
      liveBounds = bounds;
  }
  const ost_domain::RenderClipPlanes planes =
      ost_domain::render_clip_planes(pose, _sceneRadius, liveBounds);
  float near = planes.near_plane;
  float far = planes.far_plane;
  float aspect = (float)_width / fmaxf(1.0f, (float)_height);

  Mat4 view = Mat4::lookAt(eye, target, Vec3(0, 1, 0));
  Mat4 proj = Mat4::perspective(M_PI / 4.0f, aspect, near, far);

  // The Windows lighting constants (renderer.cpp opaque_fragment_shader / set_lighting_uniforms): ambient 0.55, key 0.45, fill
  // 0.35, rim 0.2 (RPV-29). The light directions of the shaders are the Windows ones in this Y-up space.
  float lightParams[4] = {0.55f, 0.45f, 0.35f, 0.20f};
  float eyePos[4] = {eye.x, eye.y, eye.z, 0.0f};
  float identityMtx[16];
  memcpy(identityMtx, Mat4::identity().m, sizeof(identityMtx));

  bgfx::setViewClear(kViewOpaque, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, kBgColor,
                     1.0f, 0);
  bgfx::setViewRect(kViewOpaque, 0, 0, (uint16_t)_width, (uint16_t)_height);
  bgfx::setViewFrameBuffer(kViewOpaque, _sceneFB);
  bgfx::setViewTransform(kViewOpaque, view.m, proj.m);

  // Which pass draws a mesh is one Core rule: opaque meshes here, transparent ones through the OIT passes, or with plain alpha
  // blending below when those cannot be used: a mesh is never left out of every pass.
  const bool oitAvailable =
      bgfx::isValid(_oitAccumFB) && bgfx::isValid(_oitRevealFB);
  bool hasTransparent = false;
  bool hasBlended = false;
  for (auto &gm : _gpuMeshes) {
    const ost_domain::MeshPass pass =
        ost_domain::mesh_pass_for_alpha(gm.color.a, oitAvailable);
    if (pass == ost_domain::MeshPass::Oit) {
      hasTransparent = true;
      continue;
    }
    if (pass == ost_domain::MeshPass::AlphaBlend) {
      hasBlended = true;
      continue;
    }
    bgfx::setTransform(identityMtx);
    bgfx::setVertexBuffer(0, gm.vbh);
    bgfx::setIndexBuffer(gm.ibh);
    float col[4] = {gm.color.r, gm.color.g, gm.color.b, 1.0f};
    bgfx::setUniform(_u_meshColor, col);
    bgfx::setUniform(_u_lightParams, lightParams);
    bgfx::setUniform(_u_eyePos, eyePos);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                   BGFX_STATE_MSAA);
    bgfx::submit(kViewOpaque, _progMesh);
  }

  if (hasBlended) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      NSLog(@"Renderer: the transparency passes are not available; "
            @"transparent meshes are drawn with plain alpha blending");
    }
    // After the opaque meshes, tested against their depth, without writing it.
    for (auto &gm : _gpuMeshes) {
      if (ost_domain::mesh_pass_for_alpha(gm.color.a, oitAvailable) !=
          ost_domain::MeshPass::AlphaBlend)
        continue;
      bgfx::setTransform(identityMtx);
      bgfx::setVertexBuffer(0, gm.vbh);
      bgfx::setIndexBuffer(gm.ibh);
      float col[4] = {gm.color.r, gm.color.g, gm.color.b, gm.color.a};
      bgfx::setUniform(_u_meshColor, col);
      bgfx::setUniform(_u_lightParams, lightParams);
      bgfx::setUniform(_u_eyePos, eyePos);
      bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                     BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_BLEND_ALPHA |
                     BGFX_STATE_MSAA);
      bgfx::submit(kViewOpaque, _progMesh);
    }
  }

  if (hasTransparent && oitAvailable) {
    bgfx::setViewClear(kViewOITAccum, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
    bgfx::setViewRect(kViewOITAccum, 0, 0, (uint16_t)_width, (uint16_t)_height);
    bgfx::setViewFrameBuffer(kViewOITAccum, _oitAccumFB);
    bgfx::setViewTransform(kViewOITAccum, view.m, proj.m);

    for (auto &gm : _gpuMeshes) {
      if (ost_domain::mesh_pass_for_alpha(gm.color.a, oitAvailable) !=
          ost_domain::MeshPass::Oit)
        continue;
      bgfx::setTransform(identityMtx);
      bgfx::setVertexBuffer(0, gm.vbh);
      bgfx::setIndexBuffer(gm.ibh);
      float col[4] = {gm.color.r, gm.color.g, gm.color.b, gm.color.a};
      bgfx::setUniform(_u_meshColor, col);
      bgfx::setUniform(_u_lightParams, lightParams);
      bgfx::setUniform(_u_eyePos, eyePos);
      bgfx::setState(
          BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
          BGFX_STATE_DEPTH_TEST_LESS |
          BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE));
      bgfx::submit(kViewOITAccum, _progOITAccum);
    }

    bgfx::setViewClear(kViewOITRevealage, BGFX_CLEAR_COLOR, 0xFF000000, 1.0f,
                       0);
    bgfx::setViewRect(kViewOITRevealage, 0, 0, (uint16_t)_width,
                      (uint16_t)_height);
    bgfx::setViewFrameBuffer(kViewOITRevealage, _oitRevealFB);
    bgfx::setViewTransform(kViewOITRevealage, view.m, proj.m);

    for (auto &gm : _gpuMeshes) {
      if (ost_domain::mesh_pass_for_alpha(gm.color.a, oitAvailable) !=
          ost_domain::MeshPass::Oit)
        continue;
      bgfx::setTransform(identityMtx);
      bgfx::setVertexBuffer(0, gm.vbh);
      bgfx::setIndexBuffer(gm.ibh);
      float col[4] = {gm.color.r, gm.color.g, gm.color.b, gm.color.a};
      bgfx::setUniform(_u_meshColor, col);
      bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS |
                     BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO,
                                           BGFX_STATE_BLEND_SRC_COLOR));
      bgfx::submit(kViewOITRevealage, _progOITRevealage);
    }

    bgfx::setViewRect(kViewOITComposite, 0, 0, (uint16_t)_width,
                      (uint16_t)_height);
    bgfx::setViewFrameBuffer(kViewOITComposite, _sceneFB);
    bgfx::setViewTransform(kViewOITComposite, nullptr, nullptr);
    bgfx::setVertexBuffer(0, _fsQuadVbh);
    bgfx::setIndexBuffer(_fsQuadIbh);
    bgfx::setTexture(0, _s_accumTex, _oitAccumTex);
    bgfx::setTexture(1, _s_revealTex, _oitRevealTex);
    // fs_oit_composite outputs a premultiplied colour (avgColor * coverage, coverage), so the matching blend is ONE / INV_SRC_ALPHA
    // (the usual weighted-blended OIT composite); SRC_ALPHA applied the coverage a second time and made mid and dark grey meshes
    // indistinguishable from the 0x353535 background. This does not reproduce the Windows look: its accum / revealage shaders
    // weight the alpha twice, so an opacity of 0.5 comes out almost opaque there (PORTING_BLOCKERS.md, owner decision).
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                         BGFX_STATE_BLEND_INV_SRC_ALPHA));
    bgfx::submit(kViewOITComposite, _progOITComposite);
  }

  std::string selCond = _selectedConditionUID
                            ? std::string(_selectedConditionUID.UTF8String)
                            : "";
  std::string selTk =
      _selectedTakeoffUID ? std::string(_selectedTakeoffUID.UTF8String) : "";
  bool hasSelection = !selCond.empty() && hasMeshes;

  if (hasSelection) {
    bgfx::setViewRect(kViewWireframe, 0, 0, (uint16_t)_width,
                      (uint16_t)_height);
    bgfx::setViewFrameBuffer(kViewWireframe, _sceneFB);
    bgfx::setViewTransform(kViewWireframe, view.m, proj.m);

    for (auto &gm : _gpuMeshes) {
      bool match = false;
      if (!selTk.empty()) {
        match = (gm.meshIndex < (int)_meshTakeoffUIDs.size() &&
                 _meshTakeoffUIDs[gm.meshIndex] == selTk);
      } else {
        match = (gm.meshIndex < (int)_meshConditionUIDs.size() &&
                 _meshConditionUIDs[gm.meshIndex] == selCond);
      }
      if (!match || !bgfx::isValid(gm.lineIbh))
        continue;

      bgfx::setTransform(identityMtx);
      bgfx::setVertexBuffer(0, gm.vbh);
      bgfx::setIndexBuffer(gm.lineIbh);
      float yellow[4] = {1.0f, 0.95f, 0.0f, 1.0f};
      bgfx::setUniform(_u_meshId, yellow);
      bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                     BGFX_STATE_PT_LINES | BGFX_STATE_MSAA);
      bgfx::submit(kViewWireframe, _progMeshId);
    }
  }

  if (bgfx::isValid(_selMaskFB)) {
    bgfx::setViewClear(kViewSelMask, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x00000000, 1.0f, 0);
    bgfx::setViewRect(kViewSelMask, 0, 0, (uint16_t)_width, (uint16_t)_height);
    bgfx::setViewFrameBuffer(kViewSelMask, _selMaskFB);
    bgfx::setViewTransform(kViewSelMask, view.m, proj.m);

    if (hasSelection) {
      bgfx::setViewTransform(kViewSelMask, view.m, proj.m);

      for (auto &gm : _gpuMeshes) {
        bool match = false;
        if (!selTk.empty()) {
          match = (gm.meshIndex < (int)_meshTakeoffUIDs.size() &&
                   _meshTakeoffUIDs[gm.meshIndex] == selTk);
        } else {
          match = (gm.meshIndex < (int)_meshConditionUIDs.size() &&
                   _meshConditionUIDs[gm.meshIndex] == selCond);
        }
        if (!match)
          continue;

        bgfx::setTransform(identityMtx);
        bgfx::setVertexBuffer(0, gm.vbh);
        bgfx::setIndexBuffer(gm.ibh);
        int32_t encoded = gm.meshIndex + 1;
        float mid[4] = {((encoded) & 0xFF) / 255.0f,
                        ((encoded >> 8) & 0xFF) / 255.0f, 0.0f, 1.0f};
        bgfx::setUniform(_u_meshId, mid);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                       BGFX_STATE_DEPTH_TEST_LEQUAL);
        bgfx::submit(kViewSelMask, _progMeshId);
      }
    } else {
      bgfx::touch(kViewSelMask);
    }
  }

  if (hasMeshes && bgfx::isValid(_pickFB)) {
    bgfx::setViewClear(kViewMeshId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x00000000, 1.0f, 0);
    bgfx::setViewRect(kViewMeshId, 0, 0, (uint16_t)_width, (uint16_t)_height);
    bgfx::setViewFrameBuffer(kViewMeshId, _pickFB);
    bgfx::setViewTransform(kViewMeshId, view.m, proj.m);

    for (auto &gm : _gpuMeshes) {
      bgfx::setTransform(identityMtx);
      bgfx::setVertexBuffer(0, gm.vbh);
      bgfx::setIndexBuffer(gm.ibh);
      int32_t encoded = gm.meshIndex + 1;
      float mid[4] = {((encoded) & 0xFF) / 255.0f,
                      ((encoded >> 8) & 0xFF) / 255.0f, 0.0f, 1.0f};
      bgfx::setUniform(_u_meshId, mid);
      bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                     BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
      bgfx::submit(kViewMeshId, _progMeshId);
    }

    bgfx::blit(kViewMeshId, _pickReadbackTex, 0, 0, _pickTex);
    bgfx::readTexture(_pickReadbackTex, _pickReadback.data());
  }

  bgfx::setViewRect(kViewFinal, 0, 0, (uint16_t)_width, (uint16_t)_height);
  bgfx::setViewTransform(kViewFinal, nullptr, nullptr);
  bgfx::setVertexBuffer(0, _fsQuadVbh);
  bgfx::setIndexBuffer(_fsQuadIbh);
  bgfx::setTexture(0, _s_sceneTex, _sceneTex);

  bgfx::setTexture(1, _s_selMaskTex, _selMaskTex);
  bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
  bgfx::submit(kViewFinal, _progSelectionPost);

  bgfx::touch(kViewOpaque);
  bgfx::frame();
}

#pragma mark - Shutdown

- (void)shutdown {
  if (!_bgfxReady)
    return;
  [self clearMeshes];

  bgfx::destroy(_fsQuadVbh);
  bgfx::destroy(_fsQuadIbh);

  bgfx::destroy(_progMesh);
  bgfx::destroy(_progOITAccum);
  bgfx::destroy(_progOITRevealage);
  bgfx::destroy(_progOITComposite);
  bgfx::destroy(_progMeshId);
  bgfx::destroy(_progSelectionPost);

  bgfx::destroy(_u_meshColor);
  bgfx::destroy(_u_lightParams);
  bgfx::destroy(_u_eyePos);
  bgfx::destroy(_u_meshId);
  bgfx::destroy(_s_accumTex);
  bgfx::destroy(_s_revealTex);
  bgfx::destroy(_s_sceneTex);
  bgfx::destroy(_s_selMaskTex);

  [self _destroyOffscreenFBs];

  bgfx::shutdown();
  _bgfxReady = false;
}

- (void)dealloc {
  if (_bgfxReady)
    [self shutdown];
}

@end
