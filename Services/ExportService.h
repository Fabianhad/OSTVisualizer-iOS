#import <Foundation/Foundation.h>
#include "../Core/Data/ProjectData.hpp"
#include <set>
#include <string>

@interface ExportService : NSObject
+ (NSArray<NSURL *> *)exportOBJ:(const ost_data::ProjectData &)project
                  selectedPages:(const std::set<std::string> &)selectedPages;
+ (NSArray<NSURL *> *)exportDXF:(const ost_data::ProjectData &)project
                  selectedPages:(const std::set<std::string> &)selectedPages;
+ (NSArray<NSURL *> *)exportThreeJS:(const ost_data::ProjectData &)project
                      selectedPages:(const std::set<std::string> &)selectedPages;

@end
