#import "ProjectService.h"
#include "../Core/Data/PageNavigation.hpp"
#include "../Core/Meshing/ExportMeshSet.hpp"
#include <exception>

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
  // LP-08: the pages in the Windows page order (Sequence, Name, UID; Index1
  // is only the PDF page index) of the Windows page tree walk.
  for (const std::string &uid : ost_data::page_tree_order(_project)) {
    const ost_domain::Page &page = _project.pages.at(uid);
    _pageList.push_back({uid, page.name, page.page_index, page.folder_uid});
  }
  // BRW-17: the page folders in the order Core loaded them (Name, UID).
  _folderOrder.clear();
  for (const auto &pair : _project.folders) {
    _folderOrder.push_back(pair.first);
  }
  // LP-12: the saved page of the bid (BidPageSelectedUID) when it still
  // exists, else the first page of the tree walk; 3D and 2D start with it.
  const std::string firstPage = ost_data::initial_page_uid(_project);
  if (!firstPage.empty()) {
    _selected3DPageUIDs.insert(firstPage);
    _selected2DPageUID = firstPage;
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

- (BOOL)select2DPage:(const std::string &)uid {
  // LP-11: selecting the active page again keeps it (Windows only activates a
  // page that is not the active one).
  return ost_data::activate_page(_selected2DPageUID, uid);
}

- (void)select3DFolder:(const std::string &)folderUID {
  auto it = _project.folders.find(folderUID);
  if (it == _project.folders.end())
    return;

  // LP-10: all pages of the folder are added to the selection, or, when they
  // are all selected already, removed; other selected pages are kept.
  ost_data::toggle_folder_pages(_selected3DPageUIDs,
                                it->second.all_page_uids());
}

#pragma mark - Mesh Generation

- (void)generateMeshesIntoRenderer:(Renderer *)renderer {
  [renderer clearMeshes];

  if (_project.empty())
    return;
  // Core reads an empty page set as "every page"; the view shows nothing.
  if (_selected3DPageUIDs.empty())
    return;

  // One Core call builds the scene like Windows: raw OST units, page rotation
  // and flip, holes, the boolean pass (a negative cuts the positives of its
  // condition type), the mesh order and the colour / opacity of the mode.
  std::vector<ost_meshing::BooleanMesh> meshes;
  try {
    meshes = ost_meshing::generate_view_meshes(_project, _selected3DPageUIDs,
                                               _renderConfig);
  } catch (const std::exception &error) {
    // Windows: a failed scene generation leaves the view empty. The reason is logged, so that an empty view can be explained.
    NSLog(@"3D scene generation failed, the view stays empty: %s",
          error.what());
    return;
  }

  for (const auto &m : meshes) {
    NSString *condUID =
        [NSString stringWithUTF8String:m.takeoff->condition_uid.c_str()];
    NSString *tkUID = [NSString stringWithUTF8String:m.takeoff->uid.c_str()];
    [renderer addMeshWithVertices:(const double *)m.mesh.vertices.data()
                      vertexCount:(int)m.mesh.vertices.size()
                            faces:(const uint32_t *)m.mesh.faces.data()
                        faceCount:(int)m.mesh.faces.size()
                           colorR:m.color.r
                                g:m.color.g
                                b:m.color.b
                            alpha:m.color.opacity
                     conditionUID:condUID
                       takeoffUID:tkUID];
  }
}

@end
