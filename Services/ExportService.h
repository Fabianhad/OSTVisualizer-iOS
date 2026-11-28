#import <Foundation/Foundation.h>
#include "../Core/Data/ProjectData.hpp"
#include "../Core/Domain/RenderConfig.hpp"
#include <set>
#include <string>

// Why an export returned no file (EXP-21), like the Windows export result.
typedef NS_ENUM(NSInteger, ExportOutcome) {
  ExportOutcomeOK = 0,
  // No page selected, or no visible takeoff on the selected pages
  // (Windows ExportErrorCode.NO_DATA).
  ExportOutcomeNoTakeoffs,
  // There are takeoffs but no mesh survives (negative-only export, every
  // positive consumed by negatives of its type): Windows WORKER_FAILED, shown
  // as "No file was written. ..." (ost_domain::export_failure_text). No file
  // is written.
  ExportOutcomeNothingToExport,
  // The file could not be written.
  ExportOutcomeWriteFailed,
  // The export failed with an unexpected error (Windows UNEXPECTED).
  ExportOutcomeFailed,
};

// Each export returns the files to share, or nil with the reason in *outcome.
// The display mode and grayscale of renderConfig colour the export like the 3D
// view.
@interface ExportService : NSObject
+ (NSArray<NSURL *> *)exportOBJ:(const ost_data::ProjectData &)project
                  selectedPages:(const std::set<std::string> &)selectedPages
                   renderConfig:(const ost_domain::RenderConfig &)renderConfig
                        outcome:(ExportOutcome *)outcome;
+ (NSArray<NSURL *> *)exportDXF:(const ost_data::ProjectData &)project
                  selectedPages:(const std::set<std::string> &)selectedPages
                   renderConfig:(const ost_domain::RenderConfig &)renderConfig
                        outcome:(ExportOutcome *)outcome;
+ (NSArray<NSURL *> *)exportThreeJS:(const ost_data::ProjectData &)project
                      selectedPages:
                          (const std::set<std::string> &)selectedPages
                       renderConfig:
                           (const ost_domain::RenderConfig &)renderConfig
                            outcome:(ExportOutcome *)outcome;

@end
