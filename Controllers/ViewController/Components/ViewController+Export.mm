#import "../ViewController_Internal.h"
#include "../../../Core/Domain/FailureText.hpp"

// An NSString for a Core text (valid UTF-8).
static NSString *OSTExportNS(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

@implementation ViewController (Export)

- (void)_handleExport {
  if (self.projectService.project.empty())
    return;

  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:@"Export"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  [alert addAction:[UIAlertAction actionWithTitle:@"Export OBJ (3D mesh)"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction *a) {
                                            [self _exportOBJ];
                                          }]];

  [alert addAction:[UIAlertAction actionWithTitle:@"Export DXF (wireframe)"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction *a) {
                                            [self _exportDXF];
                                          }]];

  [alert addAction:[UIAlertAction actionWithTitle:@"Export HTML (3D viewer)"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction *a) {
                                            [self _exportThreeJS];
                                          }]];

  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];

  alert.popoverPresentationController.sourceView = self.toolbar;
  alert.popoverPresentationController.sourceRect = self.toolbar.bounds;

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_presentExportAlertWithTitle:(NSString *)title
                             message:(NSString *)message {
  UIAlertController *err = [UIAlertController
      alertControllerWithTitle:title
                       message:message
                preferredStyle:UIAlertControllerStyleAlert];
  [err addAction:[UIAlertAction actionWithTitle:@"OK"
                                          style:UIAlertActionStyleDefault
                                        handler:nil]];
  [self presentViewController:err animated:YES completion:nil];
}

- (void)_shareExportURLs:(NSArray<NSURL *> *)urls
                 outcome:(ExportOutcome)outcome {
  if (!urls) {
    // Like Windows, a missing selection or takeoff, an export without any
    // surviving mesh and a failed write are three different results.
    switch (outcome) {
    case ExportOutcomeNoTakeoffs:
      [self _presentExportAlertWithTitle:@"No Data"
                                 message:@"No takeoffs found for any of the "
                                         @"selected pages."];
      break;
    // The Windows wording (EXP-21): the title of every failure is "Export Error", the alert shows one user sentence and the technical
    // reason stays in the log (ExportService). The writers return no errno yet, so a write failure is the generic sentence.
    case ExportOutcomeNothingToExport: {
      const ost_domain::DialogText text = ost_domain::export_failure_text(
          ost_domain::ExportFailureKind::NoFileWritten);
      [self _presentExportAlertWithTitle:OSTExportNS(text.title)
                                 message:OSTExportNS(text.message)];
      break;
    }
    case ExportOutcomeFailed: {
      const ost_domain::DialogText text = ost_domain::export_failure_text(
          ost_domain::ExportFailureKind::Unexpected);
      [self _presentExportAlertWithTitle:OSTExportNS(text.title)
                                 message:OSTExportNS(text.message)];
      break;
    }
    default: {
      const ost_domain::DialogText text = ost_domain::export_failure_text(
          ost_domain::ExportFailureKind::WriteFailed);
      [self _presentExportAlertWithTitle:OSTExportNS(text.title)
                                 message:OSTExportNS(text.message)];
      break;
    }
    }
    return;
  }
  UIActivityViewController *vc =
      [[UIActivityViewController alloc] initWithActivityItems:urls
                                        applicationActivities:nil];
  vc.popoverPresentationController.sourceView = self.toolbar;
  vc.popoverPresentationController.sourceRect = self.toolbar.bounds;
  [self presentViewController:vc animated:YES completion:nil];
}

// The display mode and grayscale of the 3D view colour the export too, and
// only the pages selected for 3D are exported.
- (void)_exportOBJ {
  ExportOutcome outcome = ExportOutcomeOK;
  NSArray<NSURL *> *urls =
      [ExportService exportOBJ:self.projectService.project
                 selectedPages:self.projectService.selected3DPageUIDs
                  renderConfig:self.projectService.renderConfig
                       outcome:&outcome];
  [self _shareExportURLs:urls outcome:outcome];
}

- (void)_exportDXF {
  ExportOutcome outcome = ExportOutcomeOK;
  NSArray<NSURL *> *urls =
      [ExportService exportDXF:self.projectService.project
                 selectedPages:self.projectService.selected3DPageUIDs
                  renderConfig:self.projectService.renderConfig
                       outcome:&outcome];
  [self _shareExportURLs:urls outcome:outcome];
}

- (void)_exportThreeJS {
  ExportOutcome outcome = ExportOutcomeOK;
  NSArray<NSURL *> *urls =
      [ExportService exportThreeJS:self.projectService.project
                     selectedPages:self.projectService.selected3DPageUIDs
                      renderConfig:self.projectService.renderConfig
                           outcome:&outcome];
  [self _shareExportURLs:urls outcome:outcome];
}

@end
