#import "SceneDelegate.h"
#import "Controllers/DatabaseViewController.h"
#import "Services/ImportService.h"
#include "Core/Domain/FailureText.hpp"
#include <string>

@implementation SceneDelegate

- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
                 options:(UISceneConnectionOptions *)connectionOptions {
  if (![scene isKindOfClass:[UIWindowScene class]])
    return;

  UIWindowScene *windowScene = (UIWindowScene *)scene;
  self.window = [[UIWindow alloc] initWithWindowScene:windowScene];

  DatabaseViewController *vc = [[DatabaseViewController alloc] init];
  UINavigationController *nav =
      [[UINavigationController alloc] initWithRootViewController:vc];

  self.window.rootViewController = nav;
  [self.window makeKeyAndVisible];

  if (connectionOptions.URLContexts.count > 0) {
    [self _handleURLContexts:connectionOptions.URLContexts];
  }
}

- (void)scene:(UIScene *)scene
    openURLContexts:(NSSet<UIOpenURLContext *> *)URLContexts {
  [self _handleURLContexts:URLContexts];
}

- (void)_handleURLContexts:(NSSet<UIOpenURLContext *> *)contexts {
  for (UIOpenURLContext *ctx in contexts) {
    NSURL *url = ctx.URL;
    if (!url)
      continue;

    NSString *ext = url.pathExtension.lowercaseString;
    BOOL isOST = [ext isEqualToString:@"ost"];
    BOOL isOSP = [ext isEqualToString:@"osp"];
    if (!isOST && !isOSP)
      continue;

    NSError *error = nil;
    BOOL ok = isOSP ? [ImportService importOSPFile:url error:&error]
                    : [ImportService importOSTFile:url error:&error];
    if (!ok) {
      NSLog(@"SceneDelegate: failed to import %@: %@", url.lastPathComponent,
            error.localizedDescription);
      // The Windows "args" import (EXP-21): one line "<file name>: <sentence>" under "Import Error". The kind is the code of the
      // ImportService error; anything that is not one of its errors is an unexpected failure.
      const ost_domain::ImportFailureKind kind =
          [error.domain isEqualToString:OSTImportErrorDomain]
              ? static_cast<ost_domain::ImportFailureKind>(error.code)
              : ost_domain::ImportFailureKind::Unexpected;
      const ost_domain::DialogText text = ost_domain::import_failure_text(
          kind, isOSP ? ost_domain::ImportFormat::Osp : ost_domain::ImportFormat::Ost,
          ost_domain::ImportContext::OpenIn,
          std::string(url.lastPathComponent.UTF8String ?: ""));
      NSString *title = [NSString stringWithUTF8String:text.title.c_str()];
      NSString *message = [NSString stringWithUTF8String:text.message.c_str()];
      dispatch_async(dispatch_get_main_queue(), ^{
        [self _showImportErrorWithTitle:title message:message];
      });
    }
  }
}

- (void)_showImportErrorWithTitle:(NSString *)title message:(NSString *)message {
  UIViewController *root = self.window.rootViewController;
  while (root.presentedViewController)
    root = root.presentedViewController;

  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [root presentViewController:alert animated:YES completion:nil];
}

@end
