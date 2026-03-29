#import "ProjectService.h"
#include "../Core/Geometry/boolean_ops.hpp"
#include "../Core/Geometry/mesh_repair.hpp"
#include "../Core/Meshing/MeshFactory.hpp"
#include <algorithm>

@implementation ProjectService {
  ost_data::ProjectData _project;
  std::set<std::string> _selected3DPageUIDs;
  std::string _selected2DPageUID;
  std::vector<PageEntry> _pageList;
  std::vector<std::string> _folderOrder;
  std::vector<std::string> _sortedLayerUIDs;
}

- (const ost_data::ProjectData &)project {
  return _project;
}
- (ost_data::ProjectData &)mutableProject {
  return _project;
}
- (const std::set<std::string> &)selected3DPageUIDs {
  return _selected3DPageUIDs;
}
- (const std::string &)selected2DPageUID {
  return _selected2DPageUID;
}
- (const std::vector<PageEntry> &)pageList {
  return _pageList;
}
- (const std::vector<std::string> &)folderOrder {
  return _folderOrder;
}
- (std::vector<std::string> &)sortedLayerUIDs {
  return _sortedLayerUIDs;
}

#pragma mark - Project Loading

- (void)loadProject:(const ost_data::ProjectData &)project {
  _project = project;
  _selected3DPageUIDs.clear();
  _selected2DPageUID.clear();
  _pageList.clear();
  for (const auto &pair : _project.pages) {
    _pageList.push_back({pair.first, pair.second.name, pair.second.page_index,
                         pair.second.folder_uid});
  }
  std::sort(_pageList.begin(), _pageList.end(),
            [](const PageEntry &a, const PageEntry &b) {
              if (a.index != b.index)
                return a.index < b.index;
              return a.name < b.name;
            });
  _folderOrder.clear();
  for (const auto &pair : _project.folders) {
    _folderOrder.push_back(pair.first);
  }
  const auto &folders = _project.folders;
  std::sort(_folderOrder.begin(), _folderOrder.end(),
            [&folders](const std::string &a, const std::string &b) {
              return folders.at(a).name < folders.at(b).name;
            });
  if (!_pageList.empty()) {
    _selected3DPageUIDs.insert(_pageList[0].uid);
    _selected2DPageUID = _pageList[0].uid;
  }
}

#pragma mark - Page Selection

- (BOOL)toggle3DPage:(const std::string &)uid {
  if (_selected3DPageUIDs.count(uid)) {
    _selected3DPageUIDs.erase(uid);
    return NO;
  } else {
    _selected3DPageUIDs.insert(uid);
    return YES;
  }
}

- (void)select2DPage:(const std::string &)uid {
  if (_selected2DPageUID == uid) {
    _selected2DPageUID.clear();
  } else {
    _selected2DPageUID = uid;
  }
}

- (void)select3DFolder:(const std::string &)folderUID {
  auto it = _project.folders.find(folderUID);
  if (it == _project.folders.end())
    return;

  auto folderPages = it->second.all_page_uids();
  _selected3DPageUIDs.clear();
  for (const auto &uid : folderPages) {
    _selected3DPageUIDs.insert(uid);
  }
}

#pragma mark - Mesh Generation
static std::unordered_map<std::string,
                          std::vector<std::vector<std::pair<double, double>>>>
build_hole_map(const ost_data::ProjectData &project,
               const std::set<std::string> &selectedPages) {
  std::unordered_map<std::string,
                     std::vector<std::vector<std::pair<double, double>>>>
      holes;

  for (const auto &takeoff : project.takeoffs) {
    if (!takeoff.is_hole())
      continue;
    if (!selectedPages.empty() && !selectedPages.count(takeoff.page_uid))
      continue;
    if (takeoff.position.size() < 6)
      continue;

    double sr = 1.0 / project.page_scale_ratio(takeoff.page_uid);
    std::vector<std::pair<double, double>> hole_verts;
    for (size_t j = 0; j + 1 < takeoff.position.size(); j += 2) {
      hole_verts.emplace_back(-takeoff.position[j] * sr,
                              -takeoff.position[j + 1] * sr);
    }
    if (hole_verts.size() >= 3) {
      holes[takeoff.parent_uid].push_back(std::move(hole_verts));
    }
  }
  return holes;
}

- (void)generateMeshesIntoRenderer:(Renderer *)renderer {
  [renderer clearMeshes];

  if (_project.empty())
    return;
  if (_selected3DPageUIDs.empty())
    return;

  auto holeMap = build_hole_map(_project, _selected3DPageUIDs);

  struct PositiveMesh {
    ost_geometry::CppMeshData mesh;
    int condition_type;
    std::string condition_uid;
    std::string takeoff_uid;
    float r, g, b, alpha;
  };

  std::vector<PositiveMesh> positives;
  std::unordered_map<int, std::vector<ost_geometry::CppMeshData>>
      negativesByType;

  // Phase 1: Generate all meshes, separating positives and negatives per
  // condition type
  for (const auto &takeoff : _project.takeoffs) {
    if (!_selected3DPageUIDs.count(takeoff.page_uid))
      continue;

    auto cdnIt = _project.conditions.find(takeoff.condition_uid);
    if (cdnIt == _project.conditions.end())
      continue;

    const auto &condition = cdnIt->second;
    if (!_project.is_condition_visible(condition))
      continue;
    if (takeoff.is_hole())
      continue;

    double sr = 1.0 / _project.page_scale_ratio(takeoff.page_uid);
    std::vector<std::vector<std::pair<double, double>>> holes;
    if (condition.is_area()) {
      auto it = holeMap.find(takeoff.uid);
      if (it != holeMap.end())
        holes = it->second;
    }

    auto result =
        ost_meshing::create_mesh_for_takeoff(takeoff, condition, sr, holes);
    if (!result.valid)
      continue;

    int cdnType = static_cast<int>(condition.condition_type);
    if (takeoff.is_negative) {
      negativesByType[cdnType].push_back(std::move(result.mesh));
    } else {
      float alpha = 1.0f;
      if (_renderConfig.is_transparent()) {
        alpha = 0.5f;
      } else if (_renderConfig.is_ost()) {
        alpha = (condition.pattern == ost_domain::Pattern::Solid) ? 1.0f : 0.5f;
      }

      float cr = result.color_r;
      float cg = result.color_g;
      float cb = result.color_b;
      if (_renderConfig.grayscale_enabled) {
        float gray = 0.299f * cr + 0.587f * cg + 0.114f * cb;
        cr = cg = cb = gray;
      }

      positives.push_back({std::move(result.mesh), cdnType,
                           takeoff.condition_uid, takeoff.uid, cr, cg, cb,
                           alpha});
    }
  }

  // Phase 2: Apply boolean operations per condition type and add to renderer
  for (auto &pos : positives) {
    ost_geometry::CppMeshData finalMesh = std::move(pos.mesh);

    auto negIt = negativesByType.find(pos.condition_type);
    if (negIt != negativesByType.end() && !negIt->second.empty()) {
      auto diff =
          ost_geometry::boolean_difference_batch(finalMesh, negIt->second);
      if (diff.has_value()) {
        ost_geometry::remove_degenerate_faces(*diff);
        finalMesh = std::move(*diff);
      }
    }

    NSString *condUID =
        [NSString stringWithUTF8String:pos.condition_uid.c_str()];
    NSString *tkUID = [NSString stringWithUTF8String:pos.takeoff_uid.c_str()];
    [renderer addMeshWithVertices:(const double *)finalMesh.vertices.data()
                      vertexCount:(int)finalMesh.vertices.size()
                            faces:(const uint32_t *)finalMesh.faces.data()
                        faceCount:(int)finalMesh.faces.size()
                           colorR:pos.r
                                g:pos.g
                                b:pos.b
                            alpha:pos.alpha
                     conditionUID:condUID
                       takeoffUID:tkUID];
  }
}

@end
