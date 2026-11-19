#import "ExportService.h"
#include "../Core/Export/dxf_writer.hpp"
#include "../Core/Export/export_filename.hpp"
#include "../Core/Export/obj_writer.hpp"
#include "../Core/Export/threejs_writer.hpp"
#include "../Core/Geometry/feature_edges.hpp"
#include "../Core/Meshing/ExportMeshSet.hpp"
#include <exception>

// All geometry, boolean, colour and naming rules are in Core
// (ExportMeshSet.hpp, export_filename.hpp): this file only writes the result
// into the temporary directory.

static void setOutcome(ExportOutcome *outcome, ExportOutcome value) {
  if (outcome)
    *outcome = value;
}

// The reason an export has nothing to write; YES when it has something.
static BOOL checkStatus(ost_meshing::ExportStatus status,
                        ExportOutcome *outcome) {
  switch (status) {
  case ost_meshing::ExportStatus::Ok:
    setOutcome(outcome, ExportOutcomeOK);
    return YES;
  case ost_meshing::ExportStatus::NoTakeoffs:
    setOutcome(outcome, ExportOutcomeNoTakeoffs);
    return NO;
  case ost_meshing::ExportStatus::NothingToExport:
    setOutcome(outcome, ExportOutcomeNothingToExport);
    return NO;
  }
  return NO;
}

// Runs a Core export generator; NO when the export has nothing to write (the
// reason is in *outcome). A Core exception is an error of the export like the
// Windows UNEXPECTED result, it must not leave the app.
template <typename Set, typename Generate>
static BOOL generateSet(Set &set, Generate generate, const char *format,
                        ExportOutcome *outcome) {
  try {
    set = generate();
  } catch (const std::exception &e) {
    NSLog(@"%s export failed: %s", format, e.what());
    setOutcome(outcome, ExportOutcomeFailed);
    return NO;
  }
  return checkStatus(set.status, outcome);
}

// The file name of the export, "<bid> - <pages>.<extension>" like Windows
// (EXP-09): the pages named are the selected pages that have something to
// export. nil when there is no such page (ExportOutcomeNoTakeoffs) or when the
// name is not valid UTF-8 (ExportOutcomeFailed, a nil name must never reach
// NSString path methods).
static NSString *exportFileName(const ost_data::ProjectData &project,
                                const std::set<std::string> &selectedPages,
                                const std::string &extension,
                                ExportOutcome *outcome) {
  std::vector<std::string> names =
      ost_meshing::export_page_names(project, selectedPages);
  if (names.empty()) {
    setOutcome(outcome, ExportOutcomeNoTakeoffs);
    return nil;
  }
  std::string fileName = ost_geometry::export_text::export_filename(
      extension, project.bid_name, names);
  NSString *name = [NSString stringWithUTF8String:fileName.c_str()];
  if (!name)
    setOutcome(outcome, ExportOutcomeFailed);
  return name;
}

@implementation ExportService

+ (NSArray<NSURL *> *)exportOBJ:(const ost_data::ProjectData &)project
                  selectedPages:(const std::set<std::string> &)selectedPages
                   renderConfig:(const ost_domain::RenderConfig &)renderConfig
                        outcome:(ExportOutcome *)outcome {
  ost_meshing::GroupedExport set;
  if (!generateSet(
          set,
          [&] {
            return ost_meshing::generate_condition_export_for_selection(
                project, selectedPages, renderConfig);
          },
          "OBJ", outcome))
    return nil;
  NSString *fileName = exportFileName(project, selectedPages, "obj", outcome);
  if (!fileName)
    return nil;

  // One group, material and mesh list per condition, also for a condition
  // whose meshes were all consumed (Windows OBJExporter).
  ost_geometry::OBJWriter writer;
  writer.set_display_mode(renderConfig.color_mode);
  for (const auto &group : set.groups) {
    writer.add_group(group.material_name, group.name, group.color.r,
                     group.color.g, group.color.b, group.mesh_pointers());
  }

  NSString *tmpDir = NSTemporaryDirectory();
  NSString *objPath = [tmpDir stringByAppendingPathComponent:fileName];
  // The writer puts the material file next to it: the same name, ".mtl".
  NSString *mtlName = [[fileName substringToIndex:fileName.length - 4]
      stringByAppendingString:@".mtl"];
  NSString *mtlPath = [tmpDir stringByAppendingPathComponent:mtlName];

  if (!writer.save([objPath UTF8String])) {
    setOutcome(outcome, ExportOutcomeWriteFailed);
    return nil;
  }

  NSLog(@"Exported OBJ: %zu groups, %zu verts, %zu faces", set.groups.size(),
        writer.get_total_vertices(), writer.get_total_faces());

  NSMutableArray<NSURL *> *urls = [NSMutableArray array];
  [urls addObject:[NSURL fileURLWithPath:objPath]];
  if ([[NSFileManager defaultManager] fileExistsAtPath:mtlPath]) {
    [urls addObject:[NSURL fileURLWithPath:mtlPath]];
  }
  return urls;
}

+ (NSArray<NSURL *> *)exportDXF:(const ost_data::ProjectData &)project
                  selectedPages:(const std::set<std::string> &)selectedPages
                   renderConfig:(const ost_domain::RenderConfig &)renderConfig
                        outcome:(ExportOutcome *)outcome {
  ost_meshing::GroupedExport set;
  if (!generateSet(
          set,
          [&] {
            return ost_meshing::generate_condition_export_for_selection(
                project, selectedPages, renderConfig);
          },
          "DXF", outcome))
    return nil;
  NSString *fileName = exportFileName(project, selectedPages, "dxf", outcome);
  if (!fileName)
    return nil;

  // One layer per condition, named like the OBJ material (Windows
  // DXFExporter), also for a condition without a surviving mesh.
  ost_geometry::DXFWriter writer;
  for (const auto &group : set.groups) {
    writer.add_layer(group.material_name, group.color.hex());
    for (const auto &m : group.meshes) {
      // Extract feature edges to avoid noisy triangulation artifacts
      ost_geometry::CppMeshData clean_mesh = m.mesh;
      clean_mesh.edges = ost_geometry::extract_feature_edges(clean_mesh, 0.1);
      writer.add_mesh_as_lines(clean_mesh, group.material_name);
    }
  }

  NSString *dxfPath =
      [NSTemporaryDirectory() stringByAppendingPathComponent:fileName];

  if (!writer.save([dxfPath UTF8String])) {
    setOutcome(outcome, ExportOutcomeWriteFailed);
    return nil;
  }

  NSLog(@"Exported DXF: %zu layers, %zu lines", set.groups.size(),
        writer.get_line_count());
  return @[ [NSURL fileURLWithPath:dxfPath] ];
}

+ (NSArray<NSURL *> *)exportThreeJS:(const ost_data::ProjectData &)project
                      selectedPages:
                          (const std::set<std::string> &)selectedPages
                       renderConfig:
                           (const ost_domain::RenderConfig &)renderConfig
                            outcome:(ExportOutcome *)outcome {
  ost_meshing::SceneExport set;
  if (!generateSet(
          set,
          [&] {
            return ost_meshing::generate_html_export_for_selection(
                project, selectedPages, renderConfig);
          },
          "HTML", outcome))
    return nil;
  NSString *fileName = exportFileName(project, selectedPages, "html", outcome);
  if (!fileName)
    return nil;

  ost_geometry::ThreeJSWriter writer;
  writer.set_title(project.bid_name.empty() ? "OST Export" : project.bid_name);

  for (size_t i = 0; i < set.meshes.size(); i++) {
    const auto &m = set.meshes[i];
    std::string name = m.condition->name + "_" + std::to_string(i);
    writer.add_mesh(m.mesh, name, m.color.r, m.color.g, m.color.b,
                    m.color.opacity);
  }

  NSString *htmlPath =
      [NSTemporaryDirectory() stringByAppendingPathComponent:fileName];

  if (!writer.save([htmlPath UTF8String])) {
    setOutcome(outcome, ExportOutcomeWriteFailed);
    return nil;
  }

  NSLog(@"Exported Three.js HTML: %zu meshes", set.meshes.size());
  return @[ [NSURL fileURLWithPath:htmlPath] ];
}

@end
