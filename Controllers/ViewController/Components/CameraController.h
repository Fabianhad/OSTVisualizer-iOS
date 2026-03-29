#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#import <UIKit/UIKit.h>

@class Renderer;

@interface CameraController : NSObject <UIGestureRecognizerDelegate>

@property(nonatomic, weak) Renderer *renderer;

/// Set to NO in 2D mode to disable all 3D camera gestures.
@property(nonatomic) BOOL enabled;

/// When YES, single-finger pan translates instead of orbiting.
@property(nonatomic) BOOL panMode;

/// Creates and installs gesture recognizers on the view.
/// excludedViews: views (toolbar, page bars) whose touches are ignored.
- (void)installOnView:(UIView *)view
        excludedViews:(NSArray<UIView *> *)excludedViews;

/// Reset camera to default orbit and fit to meshes.
- (void)fitScene;

@end

#endif
