#import "CameraController.h"
#import "../../../Rendering/Renderer.h"
#include "../../../Core/Domain/CameraLimits.hpp"

@implementation CameraController {
  CGPoint _lastPanPoint;
  CGFloat _lastPinchScale;
  BOOL _isPanning;
  NSArray<UIView *> *_excludedViews;
}

- (void)installOnView:(UIView *)view
        excludedViews:(NSArray<UIView *> *)excludedViews {
  _excludedViews = excludedViews;
  _enabled = YES;
  _panMode = NO;

  UIPanGestureRecognizer *pan =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(_handlePan:)];
  pan.maximumNumberOfTouches = 1;
  pan.delegate = self;
  [view addGestureRecognizer:pan];

  UIPinchGestureRecognizer *pinch = [[UIPinchGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(_handlePinch:)];
  pinch.delegate = self;
  [view addGestureRecognizer:pinch];
}

- (void)fitScene {
  // Windows reset_view: frames the scene like Camera.show_object (CAM-01); with nothing shown the camera is left as it is
  // (CAM-03), the renderer does nothing then.
  [_renderer fitCameraToMeshes];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gr
       shouldReceiveTouch:(UITouch *)touch {
  if (!_enabled)
    return NO;
  UIView *v = touch.view;
  if ([v isKindOfClass:[UIButton class]])
    return NO;
  for (UIView *excluded in _excludedViews) {
    if (excluded && !excluded.hidden && [v isDescendantOfView:excluded])
      return NO;
  }
  return YES;
}

#pragma mark - Gesture Handlers

- (void)_handlePan:(UIPanGestureRecognizer *)pan {
  UIView *view = pan.view;
  CGPoint loc = [pan locationInView:view];

  if (pan.state == UIGestureRecognizerStateBegan) {
    _lastPanPoint = loc;
    _isPanning = YES;
  } else if (pan.state == UIGestureRecognizerStateChanged && _isPanning) {
    CGFloat dx = loc.x - _lastPanPoint.x;
    CGFloat dy = loc.y - _lastPanPoint.y;
    _lastPanPoint = loc;

    if (_panMode) {
      float dist = _renderer.cameraDistance;
      float scale = dist * 0.002f;
      float yaw = _renderer.cameraYaw;
      float pitch = _renderer.cameraPitch;

      float rightX = cosf(yaw);
      float rightZ = -sinf(yaw);

      float cosPitch = cosf(pitch);
      float sinPitch = sinf(pitch);

      float forwardX = sinf(yaw) * sinPitch;
      float forwardZ = cosf(yaw) * sinPitch;
      float upAmount = cosPitch;

      float fwd = (pitch > 0.0f) ? -1.0f : 1.0f;

      _renderer.cameraTargetX +=
          (float)(-dx * rightX + fwd * dy * forwardX) * scale;
      _renderer.cameraTargetY += (float)(dy * upAmount) * scale;
      _renderer.cameraTargetZ +=
          (float)(-dx * rightZ + fwd * dy * forwardZ) * scale;
    } else {
      _renderer.cameraYaw -= dx * 0.008f;
      _renderer.cameraPitch += dy * 0.005f;

      if (_renderer.cameraPitch < -1.48f)
        _renderer.cameraPitch = -1.48f;
      if (_renderer.cameraPitch > 1.48f)
        _renderer.cameraPitch = 1.48f;
    }
  } else {
    _isPanning = NO;
  }
}

- (void)_handlePinch:(UIPinchGestureRecognizer *)pinch {
  if (pinch.state == UIGestureRecognizerStateBegan) {
    _lastPinchScale = pinch.scale;
  } else if (pinch.state == UIGestureRecognizerStateChanged) {
    CGFloat delta = pinch.scale - _lastPinchScale;
    _lastPinchScale = pinch.scale;

    // The zoom limits of the Windows camera, relative to the scene (CAM-04): 2 * near .. 0.5 * far with the planes of the
    // scene size, or of an empty scene when nothing is shown.
    const ost_domain::CameraClipPlanes planes =
        _renderer.meshTriangleCount > 0
            ? ost_domain::camera_clip_planes_for_extent(_renderer.sceneMaxExtent)
            : ost_domain::default_camera_clip_planes();
    _renderer.cameraDistance = ost_domain::zoom_camera_distance(
        _renderer.cameraDistance, (float)delta, ost_domain::kPinchZoomRate,
        planes);
  }
}

@end
