#import "ImportService.h"
#import "ImportService_Internal.h"
#include "../Core/Domain/FailureText.hpp"
#include "../Core/Import/OspFileSystem.hpp"
#include "../Core/Import/OspPackage.hpp"
#include "../Core/Import/OstXmlParser.hpp"
#include "../Core/Import/RawBidIntegrity.hpp"
#include "../Core/Persistence/ProjectStore/ProjectStore.hpp"
#include <exception>
#include <string>

NSErrorDomain const OSTImportErrorDomain = @"OSTImport";
NSNotificationName const OSTProjectImportedNotification = @"OSTProjectImported";
NSNotificationName const OSTProjectDeletedNotification = @"OSTProjectDeleted";
NSNotificationName const OSTRenderConfigChangedNotification =
    @"OSTRenderConfigChanged";

@implementation ImportService

+ (NSURL *)databaseURL {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSString *docsDir = paths.firstObject;
  return [NSURL fileURLWithPath:[docsDir stringByAppendingPathComponent:
                                             @"ost_projects.sqlite"]];
}

+ (std::string)_dbPath {
  return std::string([[[self databaseURL] path] UTF8String]);
}

// The error of a failed import (EXP-21): the code is the ost_domain::ImportFailureKind and the description is the ONE user sentence of the
// Windows local import ("The OST file is damaged or ..."), which the picker shows as it is. The technical reason (parser text, validation
// issues, store text, Cocoa error) is only logged by the caller. The open-in alert builds its own line from the code (SceneDelegate).
+ (NSError *)_importFailureOfKind:(ost_domain::ImportFailureKind)kind
                           format:(ost_domain::ImportFormat)format {
  const ost_domain::DialogText text = ost_domain::import_failure_text(
      kind, format, ost_domain::ImportContext::Picker, std::string());
  return [NSError errorWithDomain:OSTImportErrorDomain
                             code:static_cast<NSInteger>(kind)
                         userInfo:@{
                           NSLocalizedDescriptionKey : OSTString(text.message)
                         }];
}

// The kind of a file that could not be read: one that is not there (the open-in alert says "File does not exist."), or any other read
// failure, which Windows reports like a damaged file.
+ (ost_domain::ImportFailureKind)_kindOfReadError:(NSError *)readError {
  if ([readError.domain isEqualToString:NSCocoaErrorDomain] &&
      (readError.code == NSFileReadNoSuchFileError ||
       readError.code == NSFileNoSuchFileError)) {
    return ost_domain::ImportFailureKind::FileNotFound;
  }
  return ost_domain::ImportFailureKind::Damaged;
}

// Everything after the parse that both formats share: Windows validates (and repairs) the raw rows before the first write (IMP-09), then
// the project is stored in one transaction with new UIDs; the notification carries the NEW bid UID (IMP-06). Every import is an
// independent copy (IMP-19): there is no "already imported" check.
+ (BOOL)_storeRawData:(ost_import::OstRawData &)raw
                label:(NSString *)label
               format:(ost_domain::ImportFormat)format
                error:(NSError **)error {
  std::string validationError;
  std::string validationDetails;
  if (!ost_import::validate_raw_data(raw, validationError, &validationDetails)) {
    NSLog(@"ImportService: %@ rejected: %s %s", label, validationError.c_str(),
          validationDetails.c_str());
    if (error) {
      *error = [self
          _importFailureOfKind:ost_domain::classify_import_failure(
                                   validationError)
                        format:format];
    }
    return NO;
  }

  std::string db_path = [self _dbPath];
  if (!ost_store::create_schema(db_path)) {
    NSLog(@"ImportService: %@: the database schema could not be created", label);
    if (error) {
      *error = [self _importFailureOfKind:ost_domain::ImportFailureKind::Damaged
                                   format:format];
    }
    return NO;
  }

  ost_store::ImportResult result;
  if (!ost_store::import_project(db_path, raw, &result)) {
    NSLog(@"ImportService: %@: storing the project failed: %s", label,
          result.error.c_str());
    if (error) {
      *error = [self _importFailureOfKind:ost_domain::ImportFailureKind::Damaged
                                   format:format];
    }
    return NO;
  }

  NSString *bidUIDStr = OSTString(result.bid_uid);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:OSTProjectImportedNotification
                    object:nil
                  userInfo:@{@"bid_uid" : bidUIDStr}];

  NSLog(@"ImportService: imported %@ (bid_uid=%@)", label, bidUIDStr);
  return YES;
}

// A C++ exception that nothing classified is the Windows "unexpected" case (an exception outside import_ost): the reason is only logged.
+ (BOOL)_failedUnexpectedly:(const char *)what
                     format:(ost_domain::ImportFormat)format
                      error:(NSError **)error {
  NSLog(@"ImportService: the import stopped with an unexpected exception: %s",
        what);
  if (error) {
    *error = [self _importFailureOfKind:ost_domain::ImportFailureKind::Unexpected
                                 format:format];
  }
  return NO;
}

+ (BOOL)importOSTFile:(NSURL *)fileURL error:(NSError **)error {
  try {
    return [self _importOSTFile:fileURL error:error];
  } catch (const std::exception &e) {
    return [self _failedUnexpectedly:e.what()
                              format:ost_domain::ImportFormat::Ost
                               error:error];
  } catch (...) {
    return [self _failedUnexpectedly:"unknown exception"
                              format:ost_domain::ImportFormat::Ost
                               error:error];
  }
}

+ (BOOL)importOSPFile:(NSURL *)fileURL error:(NSError **)error {
  try {
    return [self _importOSPFile:fileURL error:error];
  } catch (const std::exception &e) {
    return [self _failedUnexpectedly:e.what()
                              format:ost_domain::ImportFormat::Osp
                               error:error];
  } catch (...) {
    return [self _failedUnexpectedly:"unknown exception"
                              format:ost_domain::ImportFormat::Osp
                               error:error];
  }
}

+ (BOOL)_importOSTFile:(NSURL *)fileURL error:(NSError **)error {
  BOOL accessing = [fileURL startAccessingSecurityScopedResource];

  // `bytes` points into the data: it must live until the parse is done (ARC may otherwise release it after its last message).
  NSError *readError = nil;
  NSData *data __attribute__((objc_precise_lifetime)) =
      [NSData dataWithContentsOfURL:fileURL
                            options:NSDataReadingMappedIfSafe
                              error:&readError];
  if (accessing)
    [fileURL stopAccessingSecurityScopedResource];

  if (!data) {
    NSLog(@"ImportService: OST file '%@' cannot be read: %@",
          fileURL.lastPathComponent, readError);
    if (error) {
      *error = [self _importFailureOfKind:[self _kindOfReadError:readError]
                                   format:ost_domain::ImportFormat::Ost];
    }
    return NO;
  }

  const char *bytes = static_cast<const char *>(data.bytes);
  size_t length = data.length;

  // The parse is as strict as the Windows one (IMP-M01): malformed XML is rejected with the reason, a windows-1252 file is transcoded.
  ost_import::OstRawData raw = ost_import::parse_ost_xml(bytes, length);
  if (raw.empty()) {
    NSLog(@"ImportService: OST file '%@' rejected: %s",
          fileURL.lastPathComponent, raw.error.c_str());
    if (error) {
      *error = [self _importFailureOfKind:ost_domain::classify_import_failure(
                                              raw.error)
                                   format:ost_domain::ImportFormat::Ost];
    }
    return NO;
  }

  return [self _storeRawData:raw
                       label:[NSString stringWithFormat:@"OST file '%@'",
                                                        fileURL.lastPathComponent]
                      format:ost_domain::ImportFormat::Ost
                       error:error];
}

+ (BOOL)_importOSPFile:(NSURL *)fileURL error:(NSError **)error {
  BOOL accessing = [fileURL startAccessingSecurityScopedResource];

  // `bytes` points into the (memory mapped) data: it must live until the package is staged.
  NSError *readError = nil;
  NSData *data __attribute__((objc_precise_lifetime)) =
      [NSData dataWithContentsOfURL:fileURL
                            options:NSDataReadingMappedIfSafe
                              error:&readError];
  if (accessing)
    [fileURL stopAccessingSecurityScopedResource];

  if (!data) {
    NSLog(@"ImportService: OSP file '%@' cannot be read: %@",
          fileURL.lastPathComponent, readError);
    if (error) {
      *error = [self _importFailureOfKind:[self _kindOfReadError:readError]
                                   format:ost_domain::ImportFormat::Osp];
    }
    return NO;
  }

  const char *bytes = static_cast<const char *>(data.bytes);
  size_t length = data.length;

  // Package checks, one streaming extraction into a temporary folder, image staging and the parse of the embedded OST are one Core
  // function (IMP-14, IMP-15, IMP-16, IMP-12, IMP-M02, IMP-13, IMP-M03, IMP-M01). Documents/OST/<ost name> is the iOS place of the staged
  // drawings (the Windows working directory); nothing is written before the package passed its checks, and the temporary folder is
  // always removed.
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSString *docsDir = paths.firstObject;
  std::string images_root = std::string([docsDir UTF8String]) + "/OST";
  ost_import::StdOspFileSystem files;
  ost_import::OspStageResult staged;
  if (!ost_import::stage_osp_package(bytes, length,
                                     std::string([NSTemporaryDirectory() UTF8String]),
                                     images_root, files, staged)) {
    NSLog(@"ImportService: OSP file '%@' rejected: %s",
          fileURL.lastPathComponent, staged.error.c_str());
    if (error) {
      *error = [self _importFailureOfKind:ost_domain::classify_import_failure(
                                              staged.error)
                                   format:ost_domain::ImportFormat::Osp];
    }
    return NO;
  }

  return [self _storeRawData:staged.raw
                       label:[NSString stringWithFormat:@"OSP file '%@' (%zu images staged)",
                                                        fileURL.lastPathComponent,
                                                        staged.staged_image_count]
                      format:ost_domain::ImportFormat::Osp
                       error:error];
}

+ (NSDictionary *)loadHierarchyWithError:(NSString **)errorText {
  std::string db_path = [self _dbPath];
  // create_schema only prepares a fresh database. When it fails, the read below says why the database cannot be used (DBP-11, DBP-M02).
  if (!ost_store::create_schema(db_path)) {
    NSLog(@"ImportService: the project database could not be prepared");
  }

  ost_domain::DatabaseHierarchy hierarchy;
  std::string loadError;
  if (!ost_store::load_hierarchy(db_path, hierarchy, &loadError)) {
    NSLog(@"ImportService: the project list cannot be read: %s",
          loadError.c_str());
    if (errorText)
      *errorText = OSTString(loadError);
    return nil;
  }

  NSMutableArray *folders =
      [NSMutableArray arrayWithCapacity:hierarchy.folders.size()];
  for (const auto &group : hierarchy.folders) {
    NSMutableArray *bids = [NSMutableArray arrayWithCapacity:group.bids.size()];
    for (const auto &bid : group.bids) {
      [bids addObject:@{
        @"uid" : OSTString(bid.bid_uid),
        @"name" : OSTString(bid.name),
        @"folder_uid" : OSTString(bid.folder_uid),
      }];
    }
    [folders addObject:@{
      @"uid" : OSTString(group.folder.uid),
      @"name" : OSTString(group.folder.name),
      @"bids" : bids,
    }];
  }

  NSMutableArray *orphanBids =
      [NSMutableArray arrayWithCapacity:hierarchy.orphan_bids.size()];
  for (const auto &bid : hierarchy.orphan_bids) {
    [orphanBids addObject:@{
      @"uid" : OSTString(bid.bid_uid),
      @"name" : OSTString(bid.name),
    }];
  }

  return @{
    @"folders" : folders,
    @"orphan_bids" : orphanBids,
  };
}

+ (BOOL)loadProjectWithUID:(NSString *)bidUID
                   project:(ost_data::ProjectData &)project
                     error:(NSString **)errorText {
  std::string uid = [bidUID UTF8String];
  ost_data::ProjectData loaded;
  std::string loadError;
  if (!ost_store::load_project([self _dbPath], uid, loaded, &loadError)) {
    // The reason is for the log (EXP-21): the alerts show the Core load_failure_reason sentence, never this diagnostic.
    NSLog(@"ImportService: bid %@ cannot be loaded: %s", bidUID,
          loadError.c_str());
    project = ost_data::ProjectData();
    if (errorText)
      *errorText = OSTString(loadError);
    return NO;
  }
  project = std::move(loaded);
  return YES;
}

+ (ost_data::ProjectData)loadProjectWithUID:(NSString *)bidUID {
  ost_data::ProjectData project;
  NSString *errorText = nil;
  [self loadProjectWithUID:bidUID project:project error:&errorText];
  return project;
}

+ (BOOL)deleteProjectWithUID:(NSString *)bidUID {
  std::string uid = [bidUID UTF8String];
  bool ok = ost_store::delete_project([self _dbPath], uid);
  if (ok) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:OSTProjectDeletedNotification
                      object:nil
                    userInfo:@{@"bid_uid" : bidUID}];
  }
  return ok ? YES : NO;
}

+ (BOOL)permanentlyDeleteProjectWithUID:(NSString *)bidUID {
  std::string uid = [bidUID UTF8String];
  bool ok = ost_store::permanently_delete_project([self _dbPath], uid);
  if (ok) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:OSTProjectDeletedNotification
                      object:nil
                    userInfo:@{@"bid_uid" : bidUID}];
  }
  return ok ? YES : NO;
}

+ (BOOL)restoreBidWithUID:(NSString *)bidUID {
  return ost_store::restore_bid([self _dbPath], [bidUID UTF8String]) ? YES : NO;
}

#pragma mark - Folder Management

+ (NSString *)createFolderWithName:(NSString *)name {
  std::string result =
      ost_store::create_folder([self _dbPath], [name UTF8String]);
  return result.empty() ? nil : [NSString stringWithUTF8String:result.c_str()];
}

+ (BOOL)renameFolderWithUID:(NSString *)folderUID toName:(NSString *)newName {
  return ost_store::rename_folder([self _dbPath], [folderUID UTF8String],
                                  [newName UTF8String])
             ? YES
             : NO;
}

+ (BOOL)deleteFolderWithUID:(NSString *)folderUID error:(NSString **)errorText {
  std::string reason;
  if (ost_store::delete_folder([self _dbPath], [folderUID UTF8String],
                               &reason)) {
    if (errorText)
      *errorText = nil;
    return YES;
  }
  if (errorText)
    *errorText = reason.empty() ? nil : OSTString(reason);
  return NO;
}

+ (BOOL)canDeleteFolderWithUID:(NSString *)folderUID message:(NSString **)message {
  std::string reason;
  if (ost_store::can_delete_folder([self _dbPath], [folderUID UTF8String],
                                   &reason)) {
    if (message)
      *message = nil;
    return YES;
  }
  if (message)
    *message = reason.empty() ? nil : OSTString(reason);
  return NO;
}

+ (BOOL)assignBid:(NSString *)bidUID toFolder:(NSString *)folderUID {
  return ost_store::assign_bid_to_folder([self _dbPath], [bidUID UTF8String],
                                         [folderUID UTF8String])
             ? YES
             : NO;
}

+ (BOOL)removeBidFromFolder:(NSString *)bidUID {
  return ost_store::remove_bid_from_folder([self _dbPath], [bidUID UTF8String])
             ? YES
             : NO;
}

#pragma mark - Layer Visibility Persistence

+ (BOOL)setLayerVisibility:(BOOL)visible
                  forLayer:(NSString *)layerUID
                     inBid:(NSString *)bidUID {
  return ost_store::set_layer_visibility([self _dbPath], [bidUID UTF8String],
                                         [layerUID UTF8String], visible)
             ? YES
             : NO;
}

+ (BOOL)setLayersVisibility:(BOOL)visible
                  forLayers:(const std::vector<std::string> &)layerUIDs
                      inBid:(NSString *)bidUID {
  return ost_store::set_layers_visibility([self _dbPath], [bidUID UTF8String],
                                          layerUIDs, visible)
             ? YES
             : NO;
}

+ (BOOL)saveTakeoffPosition:(const std::vector<double> &)position
                 forTakeoff:(NSString *)takeoffUID
                      inBid:(NSString *)bidUID {
  return ost_store::save_takeoff_position([self _dbPath], [bidUID UTF8String],
                                          [takeoffUID UTF8String], position)
             ? YES
             : NO;
}

+ (BOOL)saveTakeoffPositions:
            (const std::vector<std::pair<std::string, std::vector<double>>> &)
                positions
                       inBid:(NSString *)bidUID {
  return ost_store::save_takeoff_positions([self _dbPath], [bidUID UTF8String],
                                           positions)
             ? YES
             : NO;
}

+ (NSString *)insertTakeoff:(const ost_domain::Takeoff &)takeoff
                      inBid:(NSString *)bidUID {
  std::string uid =
      ost_store::insert_takeoff([self _dbPath], [bidUID UTF8String], takeoff);
  if (uid.empty())
    return nil;
  return [NSString stringWithUTF8String:uid.c_str()];
}

+ (BOOL)deleteTakeoff:(NSString *)takeoffUID {
  return ost_store::delete_takeoff([self _dbPath], [takeoffUID UTF8String])
             ? YES
             : NO;
}

+ (BOOL)deleteTakeoffs:(const std::vector<std::string> &)takeoffUIDs {
  return ost_store::delete_takeoffs([self _dbPath], takeoffUIDs) ? YES : NO;
}

+ (BOOL)insertTakeoffs:(const std::vector<ost_domain::Takeoff> &)takeoffs
                 inBid:(NSString *)bidUID {
  std::vector<std::string> newUIDs;
  return ost_store::insert_takeoffs([self _dbPath], [bidUID UTF8String],
                                    takeoffs, newUIDs)
             ? YES
             : NO;
}

+ (BOOL)setTakeoffNegative:(BOOL)negative
                forTakeoff:(NSString *)takeoffUID
                     inBid:(NSString *)bidUID {
  return ost_store::set_takeoff_negative([self _dbPath], [bidUID UTF8String],
                                         [takeoffUID UTF8String], negative)
             ? YES
             : NO;
}

+ (BOOL)setTakeoffsNegative:(const std::vector<std::pair<std::string, bool>> &)updates
                      inBid:(NSString *)bidUID {
  return ost_store::set_takeoffs_negative([self _dbPath], [bidUID UTF8String],
                                          updates)
             ? YES
             : NO;
}

+ (BOOL)updateConditionWithUID:(NSString *)conditionUID
                         inBid:(NSString *)bidUID
                       changes:(const ost_domain::ConditionChanges &)changes
                         error:(NSString **)errorText {
  std::string reason;
  if (ost_store::update_condition([self _dbPath], [bidUID UTF8String],
                                  [conditionUID UTF8String], changes, &reason)) {
    if (errorText)
      *errorText = nil;
    return YES;
  }
  // only the overflow of a renumber has a text; every other failure is the generic "Failed to save condition to database."
  if (errorText)
    *errorText = reason.empty() ? nil : OSTString(reason);
  return NO;
}

+ (void)loadExtrasForTakeoff:(ost_domain::Takeoff &)takeoff
                       inBid:(NSString *)bidUID {
  // false is not an error: Windows get_takeoff_extras returns {} for an unknown takeoff, so the extras stay empty.
  ost_store::read_takeoff_extras([self _dbPath], [bidUID UTF8String],
                                 takeoff.uid, takeoff.extras);
}

+ (NSString *)duplicateCondition:(NSString *)conditionUID
                           inBid:(NSString *)bidUID
                           error:(NSString **)errorText {
  std::string reason;
  std::string newUID = ost_store::duplicate_condition(
      [self _dbPath], [bidUID UTF8String], [conditionUID UTF8String], &reason);
  // only the overflow of a new number has a text; every other failure is the generic "Failed to duplicate the condition."
  if (errorText)
    *errorText = (newUID.empty() && !reason.empty()) ? OSTString(reason) : nil;
  if (newUID.empty())
    return nil;
  return [NSString stringWithUTF8String:newUID.c_str()];
}

+ (BOOL)deleteCondition:(NSString *)conditionUID inBid:(NSString *)bidUID {
  return ost_store::delete_condition([self _dbPath], [bidUID UTF8String],
                                     [conditionUID UTF8String])
             ? YES
             : NO;
}

#pragma mark - Global User Preferences

+ (NSDictionary *)loadRenderConfig {
  std::string db_path = [self _dbPath];
  // The configuration falls back to the defaults when it cannot be read (Windows ConfigAggregate); a database that cannot be prepared is
  // reported where it is read for real (the project list).
  if (!ost_store::create_schema(db_path)) {
    NSLog(@"ImportService: the project database could not be prepared");
  }
  ost_domain::RenderConfig config = ost_store::load_render_config(db_path);
  return @{
    @"color_mode" : OSTString(config.color_mode),
    @"grayscale_enabled" : @(config.grayscale_enabled),
  };
}

+ (BOOL)saveRenderConfigWithColorMode:(NSString *)colorMode
                     grayscaleEnabled:(BOOL)grayscale {
  ost_domain::RenderConfig config;
  // A mode outside Solid / Original / Transparent becomes the default (Original), not Solid (SET-01: Windows _validated_display_mode).
  config.color_mode =
      ost_domain::RenderConfig::validated_mode([colorMode UTF8String]);
  config.grayscale_enabled = grayscale;
  // Saving the values that are stored already writes nothing and announces nothing (SET-03: Windows update_options returns only the
  // changed keys, ConfigService publishes only when there are some); a change makes the open plan rebuild its meshes.
  bool changed = false;
  bool ok =
      ost_store::update_render_config([self _dbPath], config, &changed);
  if (ok && changed) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:OSTRenderConfigChangedNotification
                      object:nil];
  }
  return ok ? YES : NO;
}

@end
