#import "../ViewController_Internal.h"

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

- (void)_shareExportURLs:(NSArray<NSURL *> *)urls {
  if (!urls) {
    UIAlertController *err = [UIAlertController
        alertControllerWithTitle:@"Nothing to Export"
                         message:@"No visible meshes to export."
                  preferredStyle:UIAlertControllerStyleAlert];
    [err addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
    [self presentViewController:err animated:YES completion:nil];
    return;
  }
  UIActivityViewController *vc =
      [[UIActivityViewController alloc] initWithActivityItems:urls
                                        applicationActivities:nil];
  vc.popoverPresentationController.sourceView = self.toolbar;
  vc.popoverPresentationController.sourceRect = self.toolbar.bounds;
  [self presentViewController:vc animated:YES completion:nil];
}

- (void)_exportOBJ {
  [self _shareExportURLs:[ExportService exportOBJ:self.projectService.project
                                    selectedPages:self.projectService
                                                      .selected3DPageUIDs]];
}

- (void)_exportDXF {
  [self _shareExportURLs:[ExportService exportDXF:self.projectService.project
                                    selectedPages:self.projectService
                                                      .selected3DPageUIDs]];
}

- (void)_exportThreeJS {
  [self
      _shareExportURLs:[ExportService exportThreeJS:self.projectService.project
                                      selectedPages:self.projectService
                                                        .selected3DPageUIDs]];
}

@end
