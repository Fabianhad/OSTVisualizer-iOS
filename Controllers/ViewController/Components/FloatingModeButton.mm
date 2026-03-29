#import "FloatingModeButton.h"

static const CGFloat kFABSize = 56.0;
static const CGFloat kRingButtonSize = 44.0;
static const CGFloat kRingRadius = 80.0;

static const double kRingAngles3[] = {-M_PI / 4.0, -M_PI / 2.0,
                                      -3.0 * M_PI / 4.0};
static const double kRingAngles2[] = {-M_PI / 3.0, -2.0 * M_PI / 3.0};

static NSString *iconForMode(ost_domain::EditMode mode, BOOL cameraMode) {
  if (cameraMode) {
    return (mode == ost_domain::EditMode::Select)
               ? @"arrow.up.and.down.and.arrow.left.and.right"
               : @"rotate.3d";
  }
  switch (mode) {
  case ost_domain::EditMode::Select:
    return @"scope";
  case ost_domain::EditMode::PlaceTakeoff:
    return @"plus.viewfinder";
  default:
    return @"hand.raised.fill";
  }
}

static NSString *labelForMode(ost_domain::EditMode mode, BOOL cameraMode) {
  if (cameraMode) {
    return (mode == ost_domain::EditMode::Select) ? @"Pan" : @"Orbit";
  }
  switch (mode) {
  case ost_domain::EditMode::Select:
    return @"Select";
  case ost_domain::EditMode::PlaceTakeoff:
    return @"Place";
  default:
    return @"View";
  }
}

static ost_domain::EditMode kRingModes[] = {
    ost_domain::EditMode::View,
    ost_domain::EditMode::Select,
    ost_domain::EditMode::PlaceTakeoff,
};

@implementation FloatingModeButton {
  UIButton *_mainButton;
  UIView *_ringBackdrop;
  UIView *_ringContainer;
  NSMutableArray<UIButton *> *_ringButtons;
  NSMutableArray<UILabel *> *_ringLabels;
  BOOL _ringVisible;
  __weak UIView *_parentView;
  ost_domain::EditMode _currentMode;
}

- (int)_ringCount {
  return _cameraMode ? 2 : 3;
}
- (const double *)_ringAngles {
  return _cameraMode ? kRingAngles2 : kRingAngles3;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:CGRectMake(frame.origin.x, frame.origin.y,
                                         kFABSize, kFABSize)];
  if (self) {
    self.clipsToBounds = NO;
    _currentMode = ost_domain::EditMode::View;
    [self _buildMainButton];
  }
  return self;
}

- (ost_domain::EditMode)currentMode {
  return _currentMode;
}

#pragma mark - Public

- (void)setCurrentMode:(ost_domain::EditMode)mode animated:(BOOL)animated {
  _currentMode = mode;
  [self _updateMainButtonAppearance:animated];
  if (_ringVisible)
    [self _updateRingHighlight];
}

- (void)setCameraMode:(BOOL)cameraMode {
  _cameraMode = cameraMode;
  [self _updateMainButtonAppearance:NO];
  if (_ringContainer) {
    [_ringBackdrop removeFromSuperview];
    [_ringContainer removeFromSuperview];
    _ringBackdrop = nil;
    _ringContainer = nil;
    _ringButtons = nil;
    _ringLabels = nil;
    _ringVisible = NO;
  }
}

- (void)installOverlayInView:(UIView *)parent {
  _parentView = parent;
}

#pragma mark - Main Button

- (void)_buildMainButton {
  _mainButton = [UIButton buttonWithType:UIButtonTypeCustom];
  _mainButton.frame = CGRectMake(0, 0, kFABSize, kFABSize);
  _mainButton.layer.cornerRadius = kFABSize / 2;
  _mainButton.clipsToBounds = YES;
  _mainButton.backgroundColor = [UIColor colorWithWhite:0.15 alpha:0.85];

  UIImageSymbolConfiguration *config = [UIImageSymbolConfiguration
      configurationWithPointSize:22
                          weight:UIImageSymbolWeightMedium];
  UIImage *icon =
      [UIImage systemImageNamed:iconForMode(_currentMode, _cameraMode)
              withConfiguration:config];
  [_mainButton setImage:icon forState:UIControlStateNormal];
  _mainButton.tintColor = [UIColor whiteColor];

  [_mainButton addTarget:self
                  action:@selector(_handleTap)
        forControlEvents:UIControlEventTouchUpInside];

  UILongPressGestureRecognizer *lp = [[UILongPressGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(_handleLongPress:)];
  lp.minimumPressDuration = 0.3;
  [_mainButton addGestureRecognizer:lp];

  [self addSubview:_mainButton];
}

- (void)_updateMainButtonAppearance:(BOOL)animated {
  UIColor *bg = [UIColor colorWithWhite:0.15 alpha:0.85];

  UIImageSymbolConfiguration *config = [UIImageSymbolConfiguration
      configurationWithPointSize:22
                          weight:UIImageSymbolWeightMedium];
  UIImage *icon =
      [UIImage systemImageNamed:iconForMode(_currentMode, _cameraMode)
              withConfiguration:config];

  if (animated) {
    [UIView transitionWithView:_mainButton
                      duration:0.2
                       options:UIViewAnimationOptionTransitionCrossDissolve
                    animations:^{
                      self->_mainButton.backgroundColor = bg;
                      [self->_mainButton setImage:icon
                                         forState:UIControlStateNormal];
                    }
                    completion:nil];
  } else {
    _mainButton.backgroundColor = bg;
    [_mainButton setImage:icon forState:UIControlStateNormal];
  }
}

#pragma mark - Gestures

- (void)_handleTap {
  if (_ringVisible) {
    [self _dismissRing];
    return;
  }
  ost_domain::EditMode next;
  if (_cameraMode) {
    next = (_currentMode == ost_domain::EditMode::View)
               ? ost_domain::EditMode::Select
               : ost_domain::EditMode::View;
  } else if (_currentMode == ost_domain::EditMode::View) {
    next = ost_domain::EditMode::Select;
  } else if (_currentMode == ost_domain::EditMode::Select &&
             _placeModeAvailable) {
    next = ost_domain::EditMode::PlaceTakeoff;
  } else {
    next = ost_domain::EditMode::View;
  }
  [self setCurrentMode:next animated:YES];
  [_delegate floatingModeButton:self didSelectMode:_currentMode];
}

- (void)_handleLongPress:(UILongPressGestureRecognizer *)lp {
  if (lp.state == UIGestureRecognizerStateBegan) {
    if (_ringVisible) {
      [self _dismissRing];
    } else {
      [self _showRing];
    }
  }
}

#pragma mark - Ring

- (void)_ensureRingBuilt {
  if (_ringContainer)
    return;

  UIView *parent = _parentView;
  if (!parent)
    return;

  _ringBackdrop = [[UIView alloc] initWithFrame:parent.bounds];
  _ringBackdrop.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  _ringBackdrop.backgroundColor = [UIColor colorWithWhite:0 alpha:0.3];
  _ringBackdrop.alpha = 0;
  _ringBackdrop.hidden = YES;
  UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(_backdropTapped)];
  [_ringBackdrop addGestureRecognizer:tap];
  [parent insertSubview:_ringBackdrop belowSubview:self];

  _ringContainer = [[UIView alloc] initWithFrame:parent.bounds];
  _ringContainer.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  _ringContainer.userInteractionEnabled = YES;
  _ringContainer.hidden = YES;
  [parent insertSubview:_ringContainer aboveSubview:_ringBackdrop];

  _ringButtons = [NSMutableArray array];
  _ringLabels = [NSMutableArray array];

  UIImageSymbolConfiguration *iconCfg = [UIImageSymbolConfiguration
      configurationWithPointSize:18
                          weight:UIImageSymbolWeightMedium];

  int count = [self _ringCount];
  for (int i = 0; i < count; i++) {
    UIButton *btn = [UIButton buttonWithType:UIButtonTypeCustom];
    btn.frame = CGRectMake(0, 0, kRingButtonSize, kRingButtonSize);
    btn.layer.cornerRadius = kRingButtonSize / 2;
    btn.clipsToBounds = YES;
    btn.tag = i;
    btn.tintColor = [UIColor whiteColor];

    UIImage *img =
        [UIImage systemImageNamed:iconForMode(kRingModes[i], _cameraMode)
                withConfiguration:iconCfg];
    [btn setImage:img forState:UIControlStateNormal];
    [btn addTarget:self
                  action:@selector(_ringButtonTapped:)
        forControlEvents:UIControlEventTouchUpInside];

    btn.transform = CGAffineTransformMakeScale(0.01, 0.01);
    btn.alpha = 0;
    [_ringContainer addSubview:btn];
    [_ringButtons addObject:btn];

    UILabel *lbl = [[UILabel alloc] init];
    lbl.text = labelForMode(kRingModes[i], _cameraMode);
    lbl.font = [UIFont systemFontOfSize:10 weight:UIFontWeightMedium];
    lbl.textColor = [UIColor whiteColor];
    lbl.textAlignment = NSTextAlignmentCenter;
    lbl.alpha = 0;
    [lbl sizeToFit];
    [_ringContainer addSubview:lbl];
    [_ringLabels addObject:lbl];
  }
}

- (void)_showRing {
  [self _ensureRingBuilt];
  if (!_ringContainer)
    return;

  _ringVisible = YES;
  _ringBackdrop.hidden = NO;
  _ringContainer.hidden = NO;

  CGPoint fabCenter = [self.superview convertPoint:self.center
                                            toView:_parentView];

  [self _updateRingHighlight];

  int count = [self _ringCount];
  const double *angles = [self _ringAngles];
  for (int i = 0; i < count; i++) {
    _ringButtons[i].center = fabCenter;
    _ringButtons[i].transform = CGAffineTransformMakeScale(0.01, 0.01);
    _ringButtons[i].alpha = 0;
    _ringLabels[i].center =
        CGPointMake(fabCenter.x, fabCenter.y + kRingButtonSize / 2 + 10);
    _ringLabels[i].alpha = 0;
  }

  [UIView animateWithDuration:0.35
                        delay:0
       usingSpringWithDamping:0.7
        initialSpringVelocity:0.5
                      options:0
                   animations:^{
                     self->_ringBackdrop.alpha = 1.0;
                     for (int i = 0; i < count; i++) {
                       CGFloat angle = angles[i];
                       CGFloat tx = fabCenter.x + kRingRadius * cos(angle);
                       CGFloat ty = fabCenter.y + kRingRadius * sin(angle);
                       self->_ringButtons[i].center = CGPointMake(tx, ty);
                       self->_ringButtons[i].transform =
                           CGAffineTransformIdentity;
                       self->_ringButtons[i].alpha = 1.0;
                       self->_ringLabels[i].center =
                           CGPointMake(tx, ty + kRingButtonSize / 2 + 10);
                       self->_ringLabels[i].alpha = 1.0;
                     }
                   }
                   completion:nil];
}

- (void)_dismissRing {
  if (!_ringVisible)
    return;

  CGPoint fabCenter = [self.superview convertPoint:self.center
                                            toView:_parentView];

  int count = [self _ringCount];
  [UIView animateWithDuration:0.2
      animations:^{
        self->_ringBackdrop.alpha = 0;
        for (int i = 0; i < count; i++) {
          self->_ringButtons[i].center = fabCenter;
          self->_ringButtons[i].transform =
              CGAffineTransformMakeScale(0.01, 0.01);
          self->_ringButtons[i].alpha = 0;
          self->_ringLabels[i].alpha = 0;
        }
      }
      completion:^(BOOL finished) {
        self->_ringBackdrop.hidden = YES;
        self->_ringContainer.hidden = YES;
        self->_ringVisible = NO;
      }];
}

- (void)_updateRingHighlight {
  UIColor *activeBg = [UIColor colorWithRed:0.2 green:0.5 blue:1.0 alpha:0.9];
  UIColor *inactiveBg = [UIColor colorWithWhite:0.2 alpha:0.9];
  UIColor *disabledBg = [UIColor colorWithWhite:0.2 alpha:0.4];
  int count = [self _ringCount];
  for (int i = 0; i < count; i++) {
    BOOL disabled = (kRingModes[i] == ost_domain::EditMode::PlaceTakeoff &&
                     !_placeModeAvailable);
    if (disabled) {
      _ringButtons[i].backgroundColor = disabledBg;
      _ringButtons[i].enabled = NO;
      _ringLabels[i].alpha = 0.4;
    } else {
      _ringButtons[i].backgroundColor =
          (kRingModes[i] == _currentMode) ? activeBg : inactiveBg;
      _ringButtons[i].enabled = YES;
      _ringLabels[i].alpha = 1.0;
    }
  }
}

- (void)_ringButtonTapped:(UIButton *)btn {
  int idx = (int)btn.tag;
  if (idx < 0 || idx >= [self _ringCount])
    return;

  ost_domain::EditMode mode = kRingModes[idx];
  [self setCurrentMode:mode animated:YES];
  [self _dismissRing];
  [_delegate floatingModeButton:self didSelectMode:_currentMode];
}

- (void)_backdropTapped {
  [self _dismissRing];
}

- (CGSize)intrinsicContentSize {
  return CGSizeMake(kFABSize, kFABSize);
}

@end
