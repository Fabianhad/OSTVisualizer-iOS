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
- (BOOL)toggle3DPage:(const std::string &)uid;
- (void)select2DPage:(const std::string &)uid;
- (void)select3DFolder:(const std::string &)folderUID;
- (void)generateMeshesIntoRenderer:(Renderer *)renderer;

@end
