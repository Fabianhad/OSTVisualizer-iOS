#import "ImportService.h"
#include "../Core/Import/CabExtractor.hpp"
#include "../Core/Import/OstXmlParser.hpp"
#include "../Core/Import/pugixml.hpp"
#include "../Core/Persistence/ProjectStore/ProjectStore.hpp"
#include <algorithm>
#include <sstream>

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

+ (BOOL)importOSTFile:(NSURL *)fileURL error:(NSError **)error {
  BOOL accessing = [fileURL startAccessingSecurityScopedResource];

  NSData *data = [NSData dataWithContentsOfURL:fileURL
                                       options:NSDataReadingMappedIfSafe
                                         error:error];
  if (accessing)
    [fileURL stopAccessingSecurityScopedResource];

  if (!data) {
    if (error && !*error) {
      *error =
          [NSError errorWithDomain:@"OSTImport"
                              code:1
                          userInfo:@{
                            NSLocalizedDescriptionKey : @"Could not read file"
                          }];
    }
    return NO;
  }

  const char *bytes = static_cast<const char *>(data.bytes);
  size_t length = data.length;

  ost_import::OstRawData raw = ost_import::parse_ost_xml(bytes, length);
  if (raw.empty()) {
    if (error) {
      *error = [NSError
          errorWithDomain:@"OSTImport"
                     code:2
                 userInfo:@{
                   NSLocalizedDescriptionKey : @"Invalid OST file format"
                 }];
    }
    return NO;
  }

  std::string db_path = [self _dbPath];
  ost_store::create_schema(db_path);

  std::string bid_uid;
  auto it = raw.bid_row.find("UID");
  if (it != raw.bid_row.end())
    bid_uid = it->second;

  if (!bid_uid.empty() && ost_store::is_bid_imported(db_path, bid_uid)) {
    if (error) {
      *error = [NSError errorWithDomain:@"OSTImport"
                                   code:3
                               userInfo:@{
                                 NSLocalizedDescriptionKey :
                                     @"This project is already imported"
                               }];
    }
    return NO;
  }

  bool ok = ost_store::import_project(db_path, raw);
  if (!ok) {
    if (error) {
      *error = [NSError
          errorWithDomain:@"OSTImport"
                     code:4
                 userInfo:@{
                   NSLocalizedDescriptionKey : @"Failed to store project data"
                 }];
    }
    return NO;
  }

  NSString *bidUIDStr =
      bid_uid.empty() ? @"" : [NSString stringWithUTF8String:bid_uid.c_str()];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:OSTProjectImportedNotification
                    object:nil
                  userInfo:@{@"bid_uid" : bidUIDStr}];

  NSLog(@"ImportService: imported OST file '%@' (bid_uid=%@)",
        fileURL.lastPathComponent, bidUIDStr);
  return YES;
}

+ (BOOL)importOSPFile:(NSURL *)fileURL error:(NSError **)error {
  BOOL accessing = [fileURL startAccessingSecurityScopedResource];

  NSData *data = [NSData dataWithContentsOfURL:fileURL
                                       options:NSDataReadingMappedIfSafe
                                         error:error];
  if (accessing)
    [fileURL stopAccessingSecurityScopedResource];

  if (!data) {
    if (error && !*error) {
      *error =
          [NSError errorWithDomain:@"OSTImport"
                              code:1
                          userInfo:@{
                            NSLocalizedDescriptionKey : @"Could not read file"
                          }];
    }
    return NO;
  }

  const char *bytes = static_cast<const char *>(data.bytes);
  size_t length = data.length;

  auto entries = ost_import::list_cab(bytes, length);
  if (entries.empty()) {
    if (error) {
      *error = [NSError
          errorWithDomain:@"OSTImport"
                     code:2
                 userInfo:@{
                   NSLocalizedDescriptionKey : @"Invalid OSP file format"
                 }];
    }
    return NO;
  }

  std::string ost_entry_name;
  for (const auto &entry : entries) {
    std::string name_lower = entry.name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                   ::tolower);
    if (name_lower.size() >= 4 &&
        name_lower.substr(name_lower.size() - 4) == ".ost") {
      ost_entry_name = entry.name;
      break;
    }
  }

  if (ost_entry_name.empty()) {
    if (error) {
      *error = [NSError
          errorWithDomain:@"OSTImport"
                     code:2
                 userInfo:@{
                   NSLocalizedDescriptionKey : @"No OST file found in package"
                 }];
    }
    return NO;
  }

  std::string ost_filename = ost_entry_name;
  auto slash_pos = ost_filename.find_last_of("/\\");
  if (slash_pos != std::string::npos)
    ost_filename = ost_filename.substr(slash_pos + 1);
  std::string bid_name = ost_filename;
  auto dot_pos = bid_name.rfind('.');
  if (dot_pos != std::string::npos)
    bid_name = bid_name.substr(0, dot_pos);

  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSString *docsDir = paths.firstObject;
  std::string dest_dir = std::string([docsDir UTF8String]) + "/OST/" + bid_name;
  NSString *destDirNS = [NSString stringWithUTF8String:dest_dir.c_str()];

  bool has_pdfs = false;
  for (const auto &entry : entries) {
    std::string name_lower = entry.name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                   ::tolower);
    if (name_lower.size() >= 4 &&
        name_lower.substr(name_lower.size() - 4) == ".pdf") {
      if (!has_pdfs) {

        [[NSFileManager defaultManager] createDirectoryAtPath:destDirNS
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:nil];
        has_pdfs = true;
      }

      auto pdf_data = ost_import::extract_cab_file(bytes, length, entry.name);
      if (!pdf_data.empty()) {

        std::string pdf_filename = entry.name;
        auto pdf_slash = pdf_filename.find_last_of("/\\");
        if (pdf_slash != std::string::npos)
          pdf_filename = pdf_filename.substr(pdf_slash + 1);

        std::string pdf_path = dest_dir + "/" + pdf_filename;
        NSData *pdfNSData = [NSData dataWithBytes:pdf_data.data()
                                           length:pdf_data.size()];
        [pdfNSData writeToFile:[NSString stringWithUTF8String:pdf_path.c_str()]
                    atomically:YES];
      }
    }
  }

  auto ost_data_vec =
      ost_import::extract_cab_file(bytes, length, ost_entry_name);
  if (ost_data_vec.empty()) {
    if (error) {
      *error = [NSError errorWithDomain:@"OSTImport"
                                   code:2
                               userInfo:@{
                                 NSLocalizedDescriptionKey :
                                     @"Failed to extract OST from package"
                               }];
    }
    return NO;
  }

  pugi::xml_document doc;
  pugi::xml_parse_result parse_result =
      doc.load_buffer(ost_data_vec.data(), ost_data_vec.size());

  if (parse_result) {
    bool modified = false;

    pugi::xml_node root_node = doc.first_child();
    pugi::xml_node bid_node = root_node.name() == std::string("Bid")
                                  ? root_node
                                  : root_node.child("Bid");
    pugi::xml_node pages_node = bid_node.child("BidPages");
    for (pugi::xml_node page = pages_node.first_child(); page;
         page = page.next_sibling()) {
      const char *attr_names[] = {"ImagePath", "OverlayImagePath"};
      for (const char *attr_name : attr_names) {
        pugi::xml_attribute attr = page.attribute(attr_name);
        if (attr && attr.value()[0] != '\0') {
          std::string old_path = attr.value();

          auto pos = old_path.find_last_of("/\\");
          std::string filename =
              (pos != std::string::npos) ? old_path.substr(pos + 1) : old_path;
          std::string new_path = dest_dir + "/" + filename;
          attr.set_value(new_path.c_str());
          modified = true;
        }
      }
    }

    if (modified) {
      std::ostringstream ss;
      doc.save(ss, "", pugi::format_raw);
      std::string xml_str = ss.str();
      ost_data_vec.assign(xml_str.begin(), xml_str.end());
    }
  }

  ost_import::OstRawData raw =
      ost_import::parse_ost_xml(ost_data_vec.data(), ost_data_vec.size());
  if (raw.empty()) {
    if (error) {
      *error = [NSError
          errorWithDomain:@"OSTImport"
                     code:2
                 userInfo:@{
                   NSLocalizedDescriptionKey : @"Invalid OST data in package"
                 }];
    }
    return NO;
  }

  std::string db_path = [self _dbPath];
  ost_store::create_schema(db_path);

  std::string bid_uid;
  auto it = raw.bid_row.find("UID");
  if (it != raw.bid_row.end())
    bid_uid = it->second;

  if (!bid_uid.empty() && ost_store::is_bid_imported(db_path, bid_uid)) {
    if (error) {
      *error = [NSError errorWithDomain:@"OSTImport"
                                   code:3
                               userInfo:@{
                                 NSLocalizedDescriptionKey :
                                     @"This project is already imported"
                               }];
    }
    return NO;
  }

  bool ok = ost_store::import_project(db_path, raw);
  if (!ok) {
    if (error) {
      *error = [NSError
          errorWithDomain:@"OSTImport"
                     code:4
                 userInfo:@{
                   NSLocalizedDescriptionKey : @"Failed to store project data"
                 }];
    }
    return NO;
  }

  NSString *bidUIDStr =
      bid_uid.empty() ? @"" : [NSString stringWithUTF8String:bid_uid.c_str()];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:OSTProjectImportedNotification
                    object:nil
                  userInfo:@{@"bid_uid" : bidUIDStr}];

  NSLog(
      @"ImportService: imported OSP file '%@' (bid_uid=%@, %zu PDFs extracted)",
      fileURL.lastPathComponent, bidUIDStr, has_pdfs ? entries.size() : 0UL);
  return YES;
}

+ (NSDictionary *)loadHierarchy {
  std::string db_path = [self _dbPath];
  ost_store::create_schema(db_path);

  ost_domain::DatabaseHierarchy hierarchy = ost_store::load_hierarchy(db_path);

  NSMutableArray *folders =
      [NSMutableArray arrayWithCapacity:hierarchy.folders.size()];
  for (const auto &group : hierarchy.folders) {
    NSMutableArray *bids = [NSMutableArray arrayWithCapacity:group.bids.size()];
    for (const auto &bid : group.bids) {
      [bids addObject:@{
        @"uid" : [NSString stringWithUTF8String:bid.bid_uid.c_str()],
        @"name" : [NSString stringWithUTF8String:bid.name.c_str()],
        @"folder_uid" : [NSString stringWithUTF8String:bid.folder_uid.c_str()],
      }];
    }
    [folders addObject:@{
      @"uid" : [NSString stringWithUTF8String:group.folder.uid.c_str()],
      @"name" : [NSString stringWithUTF8String:group.folder.name.c_str()],
      @"bids" : bids,
    }];
  }

  NSMutableArray *orphanBids =
      [NSMutableArray arrayWithCapacity:hierarchy.orphan_bids.size()];
  for (const auto &bid : hierarchy.orphan_bids) {
    [orphanBids addObject:@{
      @"uid" : [NSString stringWithUTF8String:bid.bid_uid.c_str()],
      @"name" : [NSString stringWithUTF8String:bid.name.c_str()],
    }];
  }

  return @{
    @"folders" : folders,
    @"orphan_bids" : orphanBids,
  };
}

+ (ost_data::ProjectData)loadProjectWithUID:(NSString *)bidUID {
  std::string uid = [bidUID UTF8String];
  return ost_store::load_project([self _dbPath], uid);
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

+ (BOOL)deleteFolderWithUID:(NSString *)folderUID {
  return ost_store::delete_folder([self _dbPath], [folderUID UTF8String]) ? YES
                                                                          : NO;
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

+ (BOOL)saveTakeoffPosition:(const std::vector<double> &)position
                 forTakeoff:(NSString *)takeoffUID
                      inBid:(NSString *)bidUID {
  return ost_store::save_takeoff_position([self _dbPath], [bidUID UTF8String],
                                          [takeoffUID UTF8String], position)
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

+ (BOOL)setTakeoffNegative:(BOOL)negative
                forTakeoff:(NSString *)takeoffUID
                     inBid:(NSString *)bidUID {
  return ost_store::set_takeoff_negative([self _dbPath], [bidUID UTF8String],
                                         [takeoffUID UTF8String], negative)
             ? YES
             : NO;
}

+ (BOOL)updateCondition:(const ost_domain::Condition &)condition {
  return ost_store::update_condition([self _dbPath], condition) ? YES : NO;
}

+ (BOOL)shiftConditionRefNosFrom:(int)fromRefNo
              excludingCondition:(NSString *)conditionUID
                           inBid:(NSString *)bidUID {
  return ost_store::shift_condition_ref_nos([self _dbPath], [bidUID UTF8String],
                                            fromRefNo,
                                            [conditionUID UTF8String])
             ? YES
             : NO;
}

+ (NSString *)duplicateCondition:(NSString *)conditionUID
                           inBid:(NSString *)bidUID {
  std::string newUID = ost_store::duplicate_condition(
      [self _dbPath], [bidUID UTF8String], [conditionUID UTF8String]);
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
  ost_store::create_schema(db_path);
  ost_domain::RenderConfig config = ost_store::load_render_config(db_path);
  return @{
    @"color_mode" : [NSString stringWithUTF8String:config.color_mode.c_str()],
    @"grayscale_enabled" : @(config.grayscale_enabled),
  };
}

+ (BOOL)saveRenderConfigWithColorMode:(NSString *)colorMode
                     grayscaleEnabled:(BOOL)grayscale {
  ost_domain::RenderConfig config;
  config.color_mode = [colorMode UTF8String];
  config.grayscale_enabled = grayscale;
  if (!config.is_valid_mode(config.color_mode))
    config.color_mode = ost_domain::RenderConfig::MODE_SOLID;
  bool ok = ost_store::save_render_config([self _dbPath], config);
  if (ok) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:OSTRenderConfigChangedNotification
                      object:nil];
  }
  return ok ? YES : NO;
}

@end
