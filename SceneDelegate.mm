#import "SceneDelegate.h"
#import "Controllers/DatabaseViewController.h"
#import "Services/ImportService.h"

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
      dispatch_async(dispatch_get_main_queue(), ^{
        [self _showImportError:error.localizedDescription ?: @"Unknown error"];
      });
    }
  }
}

- (void)_showImportError:(NSString *)message {
  UIViewController *root = self.window.rootViewController;
  while (root.presentedViewController)
    root = root.presentedViewController;

  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:@"Import Failed"
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [root presentViewController:alert animated:YES completion:nil];
}

@end
