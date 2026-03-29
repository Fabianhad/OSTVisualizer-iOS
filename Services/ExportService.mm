#import "ExportService.h"
#include "../Core/Export/dxf_writer.hpp"
#include "../Core/Export/obj_writer.hpp"
#include "../Core/Export/threejs_writer.hpp"
#include "../Core/Geometry/boolean_ops.hpp"
#include "../Core/Geometry/feature_edges.hpp"
#include "../Core/Geometry/mesh_repair.hpp"
#include "../Core/Meshing/MeshFactory.hpp"
#include <unordered_map>

struct ExportedMesh {
  ost_geometry::CppMeshData mesh;
  std::string condition_name;
  float r, g, b;
};

static std::vector<ExportedMesh>
generateVisibleMeshes(const ost_data::ProjectData &project,
                      const std::set<std::string> &selectedPages) {
  std::unordered_map<std::string,
                     std::vector<std::vector<std::pair<double, double>>>>
      holeMap;
  for (const auto &takeoff : project.takeoffs) {
    if (!takeoff.is_hole())
      continue;
    if (!selectedPages.empty() && !selectedPages.count(takeoff.page_uid))
      continue;
    if (takeoff.position.size() < 6)
      continue;
    double sr = 1.0 / project.page_scale_ratio(takeoff.page_uid);
    std::vector<std::pair<double, double>> hole_verts;
    for (size_t j = 0; j + 1 < takeoff.position.size(); j += 2)
      hole_verts.emplace_back(-takeoff.position[j] * sr,
                              takeoff.position[j + 1] * sr);
    if (hole_verts.size() >= 3)
      holeMap[takeoff.parent_uid].push_back(std::move(hole_verts));
  }

  // Phase 1: Generate all meshes, separating positives and negatives
  std::vector<ExportedMesh> positives;
  std::vector<ost_geometry::CppMeshData> negatives;

  for (const auto &takeoff : project.takeoffs) {
    if (!selectedPages.empty() && !selectedPages.count(takeoff.page_uid))
      continue;

    auto cdnIt = project.conditions.find(takeoff.condition_uid);
    if (cdnIt == project.conditions.end())
      continue;

    const auto &condition = cdnIt->second;
    if (!project.is_condition_visible(condition))
      continue;
    if (takeoff.is_hole())
      continue;

    double sr = 1.0 / project.page_scale_ratio(takeoff.page_uid);

    std::vector<std::vector<std::pair<double, double>>> holes;
    if (condition.is_area()) {
      auto it = holeMap.find(takeoff.uid);
      if (it != holeMap.end())
        holes = it->second;
    }

    auto meshResult =
        ost_meshing::create_mesh_for_takeoff(takeoff, condition, sr, holes);
    if (!meshResult.valid)
      continue;

    if (takeoff.is_negative) {
      negatives.push_back(std::move(meshResult.mesh));
    } else {
      positives.push_back({std::move(meshResult.mesh), condition.name,
                           meshResult.color_r, meshResult.color_g,
                           meshResult.color_b});
    }
  }

  // Phase 2: Apply boolean operations (subtract negatives from positives)
  if (!negatives.empty()) {
    for (auto &pos : positives) {
      auto diff = ost_geometry::boolean_difference_batch(pos.mesh, negatives);
      if (diff.has_value()) {
        ost_geometry::remove_degenerate_faces(*diff);
        pos.mesh = std::move(*diff);
      }
    }
  }

  return positives;
}

static NSString *tempBaseName(const ost_data::ProjectData &project) {
  return [NSString stringWithUTF8String:project.bid_name.empty()
                                            ? "export"
                                            : project.bid_name.c_str()];
}

@implementation ExportService

+ (NSArray<NSURL *> *)exportOBJ:(const ost_data::ProjectData &)project
                  selectedPages:(const std::set<std::string> &)selectedPages {
  auto meshes = generateVisibleMeshes(project, selectedPages);
  if (meshes.empty())
    return nil;

  ost_geometry::OBJWriter writer;
  for (size_t i = 0; i < meshes.size(); i++) {
    std::string group = meshes[i].condition_name + "_" + std::to_string(i);
    writer.add_mesh(meshes[i].mesh, group, meshes[i].r, meshes[i].g,
                    meshes[i].b);
  }

  NSString *tmpDir = NSTemporaryDirectory();
  NSString *base = tempBaseName(project);
  NSString *objPath = [tmpDir
      stringByAppendingPathComponent:[base stringByAppendingString:@".obj"]];
  NSString *mtlPath = [tmpDir
      stringByAppendingPathComponent:[base stringByAppendingString:@".mtl"]];

  if (!writer.save([objPath UTF8String]))
    return nil;

  NSLog(@"Exported OBJ: %zu meshes, %zu verts, %zu faces", meshes.size(),
        writer.get_total_vertices(), writer.get_total_faces());

  NSMutableArray<NSURL *> *urls = [NSMutableArray array];
  [urls addObject:[NSURL fileURLWithPath:objPath]];
  if ([[NSFileManager defaultManager] fileExistsAtPath:mtlPath]) {
    [urls addObject:[NSURL fileURLWithPath:mtlPath]];
  }
  return urls;
}

+ (NSArray<NSURL *> *)exportDXF:(const ost_data::ProjectData &)project
                  selectedPages:(const std::set<std::string> &)selectedPages {
  auto meshes = generateVisibleMeshes(project, selectedPages);
  if (meshes.empty())
    return nil;

  ost_geometry::DXFWriter writer;
  for (size_t i = 0; i < meshes.size(); i++) {
    std::string layerName = meshes[i].condition_name;
    if (layerName.empty())
      layerName = "Layer_" + std::to_string(i);

    uint8_t cr = (uint8_t)(meshes[i].r * 255);
    uint8_t cg = (uint8_t)(meshes[i].g * 255);
    uint8_t cb = (uint8_t)(meshes[i].b * 255);
    writer.add_layer(layerName, ost_geometry::RGBColor(cr, cg, cb));
    // Extract feature edges to avoid noisy triangulation artifacts
    ost_geometry::CppMeshData clean_mesh = meshes[i].mesh;
    clean_mesh.edges = ost_geometry::extract_feature_edges(clean_mesh, 0.1);
    writer.add_mesh_as_lines(clean_mesh, layerName);
  }

  NSString *tmpDir = NSTemporaryDirectory();
  NSString *dxfPath = [tmpDir
      stringByAppendingPathComponent:[tempBaseName(project)
                                         stringByAppendingString:@".dxf"]];

  if (!writer.save([dxfPath UTF8String]))
    return nil;

  NSLog(@"Exported DXF: %zu meshes, %zu lines", meshes.size(),
        writer.get_line_count());
  return @[ [NSURL fileURLWithPath:dxfPath] ];
}

+ (NSArray<NSURL *> *)exportThreeJS:(const ost_data::ProjectData &)project
                      selectedPages:
                          (const std::set<std::string> &)selectedPages {
  auto meshes = generateVisibleMeshes(project, selectedPages);
  if (meshes.empty())
    return nil;

  ost_geometry::ThreeJSWriter writer;
  writer.set_title(project.bid_name.empty() ? "OST Export" : project.bid_name);

  for (size_t i = 0; i < meshes.size(); i++) {
    std::string name = meshes[i].condition_name + "_" + std::to_string(i);
    writer.add_mesh(meshes[i].mesh, name, meshes[i].r, meshes[i].g,
                    meshes[i].b);
  }

  NSString *tmpDir = NSTemporaryDirectory();
  NSString *htmlPath = [tmpDir
      stringByAppendingPathComponent:[tempBaseName(project)
                                         stringByAppendingString:@".html"]];

  if (!writer.save([htmlPath UTF8String]))
    return nil;

  NSLog(@"Exported Three.js HTML: %zu meshes", meshes.size());
  return @[ [NSURL fileURLWithPath:htmlPath] ];
}

@end
