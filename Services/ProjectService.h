#import <Foundation/Foundation.h>
#import "../Rendering/Renderer.h"
#include "../Core/Data/ProjectData.hpp"
#include "../Core/Domain/RenderConfig.hpp"
#include <set>
#include <string>
#include <vector>
struct PageEntry
{
    std::string uid;
    std::string name;
    int index;
    std::string folder_uid;
};

@interface ProjectService : NSObject

@property(nonatomic, readonly) const ost_data::ProjectData &project;
@property(nonatomic, readonly) ost_data::ProjectData &mutableProject;
@property(nonatomic, readonly) const std::set<std::string> &selected3DPageUIDs;
@property(nonatomic, readonly) const std::string &selected2DPageUID;
@property(nonatomic, readonly) const std::vector<PageEntry> &pageList;
@property(nonatomic, readonly) const std::vector<std::string> &folderOrder;
@property(nonatomic, readonly) std::vector<std::string> &sortedLayerUIDs;
@property(nonatomic) ost_domain::RenderConfig renderConfig;
- (void)loadProject:(const ost_data::ProjectData &)project;
// loadProject: that starts on `pageUID` (2D and 3D) when ost_data::page_selection_after_reload keeps it, else on the page loadProject:
// opens. Delete Page reloads the bid and shows the page that followed the deleted one (plan 5.6).
- (void)loadProject:(const ost_data::ProjectData &)project
      selectingPage:(const std::string &)pageUID;
// Copies the page names of the project into pageList after ProjectData::apply_page_name (Rename Page, plan 5.5). The order, the 3D selection,
// the active 2D page and the folders are not touched: use loadProject: only for a reload.
- (void)refreshPageNames;
- (BOOL)toggle3DPage:(const std::string &)uid;
// YES when the active 2D page changed (selecting the active page keeps it,
// LP-11).
- (BOOL)select2DPage:(const std::string &)uid;
- (void)select3DFolder:(const std::string &)folderUID;
- (void)generateMeshesIntoRenderer:(Renderer *)renderer;

@end
