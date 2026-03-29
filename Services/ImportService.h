#ifndef IMPORTSERVICE_H
#define IMPORTSERVICE_H

#import <Foundation/Foundation.h>
#include "../Core/Data/ProjectData.hpp"

extern NSNotificationName const OSTProjectImportedNotification;
extern NSNotificationName const OSTProjectDeletedNotification;
extern NSNotificationName const OSTRenderConfigChangedNotification;

@interface ImportService : NSObject

+ (NSURL *)databaseURL;
+ (BOOL)importOSTFile:(NSURL *)fileURL error:(NSError **)error;
+ (BOOL)importOSPFile:(NSURL *)fileURL error:(NSError **)error;
+ (NSDictionary *)loadHierarchy;
+ (ost_data::ProjectData)loadProjectWithUID:(NSString *)bidUID;
+ (BOOL)deleteProjectWithUID:(NSString *)bidUID;
+ (BOOL)permanentlyDeleteProjectWithUID:(NSString *)bidUID;

+ (NSString *)createFolderWithName:(NSString *)name;
+ (BOOL)renameFolderWithUID:(NSString *)folderUID toName:(NSString *)newName;
+ (BOOL)deleteFolderWithUID:(NSString *)folderUID;
+ (BOOL)assignBid:(NSString *)bidUID toFolder:(NSString *)folderUID;
+ (BOOL)removeBidFromFolder:(NSString *)bidUID;

+ (BOOL)setLayerVisibility:(BOOL)visible
                  forLayer:(NSString *)layerUID
                     inBid:(NSString *)bidUID;

+ (BOOL)saveTakeoffPosition:(const std::vector<double> &)position
                 forTakeoff:(NSString *)takeoffUID
                      inBid:(NSString *)bidUID;

+ (NSString *)insertTakeoff:(const ost_domain::Takeoff &)takeoff
                      inBid:(NSString *)bidUID;

+ (BOOL)deleteTakeoff:(NSString *)takeoffUID;

+ (BOOL)setTakeoffNegative:(BOOL)negative
                forTakeoff:(NSString *)takeoffUID
                     inBid:(NSString *)bidUID;

+ (BOOL)updateCondition:(const ost_domain::Condition &)condition;
+ (BOOL)shiftConditionRefNosFrom:(int)fromRefNo
              excludingCondition:(NSString *)conditionUID
                           inBid:(NSString *)bidUID;

+ (NSString *)duplicateCondition:(NSString *)conditionUID
                           inBid:(NSString *)bidUID;

+ (BOOL)deleteCondition:(NSString *)conditionUID
                  inBid:(NSString *)bidUID;

+ (NSDictionary *)loadRenderConfig;
+ (BOOL)saveRenderConfigWithColorMode:(NSString *)colorMode
                     grayscaleEnabled:(BOOL)grayscale;

@end

#endif
