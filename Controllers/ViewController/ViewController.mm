#include "../../Core/Geometry/coord_transform.hpp"
#import "../../Rendering/BgfxView.h"
#import "Components/CameraController.h"
#import "Components/DragController.h"
#import "Components/UndoRedoController.h"
#import "ViewController_Internal.h"
#import <QuartzCore/QuartzCore.h>
#include <algorithm>
#include <unordered_map>

static const int kToolbar3DOnlyTag = 30000;
static const int kToolbar2DOnlyTag = 40000;

@interface ViewController () <PlanViewEditDelegate, FloatingModeButtonDelegate,
                              UndoRedoDelegate, DragDelegate>
@property(nonatomic, strong) UIView *renderView;
@property(nonatomic, strong) CADisplayLink *displayLink;
@property(nonatomic) CFTimeInterval startTime;
@property(nonatomic, strong) UIButton *undoButton;
@property(nonatomic, strong) UIButton *redoButton;
@property(nonatomic, strong) UIButton *deleteButton;
@property(nonatomic, strong) UIButton *snapToggle;
@property(nonatomic, strong) UIButton *placeActionButton;
@end

@implementation ViewController {
  std::string _selectedTakeoffUID;
  std::string _placeConditionUID;
  NSString *_selectedConditionUID;
  int _activeHandleIndex;
  UndoRedoController *_undoRedoController;
  DragController *_dragController;
  CameraController *_cameraController;
  BOOL _hasShown2DView;
  ost_domain::EditMode _cameraEditMode;
  __weak ConditionsViewController *_conditionsVC;
  std::unordered_map<std::string, std::vector<double>> _multiDragStartPositions;
}

#pragma mark - Ivar Accessors (for categories)

- (std::string &)selectedTakeoffUID {
  return _selectedTakeoffUID;
}
- (std::string &)placeConditionUID {
  return _placeConditionUID;
}
- (NSString *)selectedConditionUID {
  return _selectedConditionUID;
}
- (void)setSelectedConditionUID:(NSString *)uid {
  _selectedConditionUID = [uid copy];
}
- (int &)activeHandleIndex {
  return _activeHandleIndex;
}
- (__weak ConditionsViewController *)conditionsVC {
  return _conditionsVC;
}
- (void)setConditionsVC:(ConditionsViewController *)vc {
  _conditionsVC = vc;
}

#pragma mark - Init

- (instancetype)initWithBidUID:(NSString *)bidUID {
  self = [super init];
  if (self) {
    _bidUID = [bidUID copy];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor blackColor];
  self.navigationController.navigationBarHidden = NO;
  self.navigationController.navigationBar.barStyle = UIBarStyleBlack;
  self.navigationController.navigationBar.translucent = YES;

  self.navigationItem.title = @"";

  Class bgfxViewClass = NSClassFromString(@"BgfxView");
  _renderView = [[bgfxViewClass alloc] initWithFrame:self.view.bounds];
  _renderView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.view addSubview:_renderView];

  _planView = [[PlanView alloc] initWithFrame:self.view.bounds];
  _planView.translatesAutoresizingMaskIntoConstraints = NO;
  _planView.hidden = YES;
  _planView.editDelegate = self;
  [self.view addSubview:_planView];

  _pageBar3D = [[UIScrollView alloc] init];
  _pageBar3D.showsHorizontalScrollIndicator = NO;
  _pageBar3D.translatesAutoresizingMaskIntoConstraints = NO;
  _pageBar3D.hidden = YES;
  [self.view addSubview:_pageBar3D];

  _pageBar2D = [[UIScrollView alloc] init];
  _pageBar2D.showsHorizontalScrollIndicator = NO;
  _pageBar2D.translatesAutoresizingMaskIntoConstraints = NO;
  _pageBar2D.hidden = YES;
  [self.view addSubview:_pageBar2D];

  [self _buildToolbar];
  [NSLayoutConstraint activateConstraints:@[
    [_planView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_planView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_planView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [_planView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],

    [_pageBar3D.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:6],
    [_pageBar3D.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                             constant:8],
    [_pageBar3D.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                              constant:-8],
    [_pageBar3D.heightAnchor constraintEqualToConstant:32],

    [_pageBar2D.topAnchor constraintEqualToAnchor:_pageBar3D.bottomAnchor
                                         constant:4],
    [_pageBar2D.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                             constant:8],
    [_pageBar2D.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                              constant:-8],
    [_pageBar2D.heightAnchor constraintEqualToConstant:32],

    [_toolbar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_toolbar.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [_toolbar.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [_toolbar.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-44],
  ]];

  _fab = [[FloatingModeButton alloc] initWithFrame:CGRectZero];
  _fab.translatesAutoresizingMaskIntoConstraints = NO;
  _fab.delegate = self;
  _fab.cameraMode = YES;
  [self.view addSubview:_fab];
  [_fab installOverlayInView:self.view];
  [NSLayoutConstraint activateConstraints:@[
    [_fab.widthAnchor constraintEqualToConstant:56],
    [_fab.heightAnchor constraintEqualToConstant:56],
    [_fab.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [_fab.bottomAnchor constraintEqualToAnchor:_toolbar.topAnchor constant:-16],
  ]];

  _snapToggle = [UIButton buttonWithType:UIButtonTypeCustom];
  _snapToggle.translatesAutoresizingMaskIntoConstraints = NO;
  _snapToggle.hidden = YES;
  _snapToggle.layer.cornerRadius = 18;
  _snapToggle.clipsToBounds = YES;
  _snapToggle.backgroundColor = [UIColor colorWithRed:0.2
                                                green:0.5
                                                 blue:1.0
                                                alpha:0.9];
  UIImageSymbolConfiguration *snapCfg = [UIImageSymbolConfiguration
      configurationWithPointSize:16
                          weight:UIImageSymbolWeightMedium];
  [_snapToggle setImage:[UIImage systemImageNamed:@"angle"
                                withConfiguration:snapCfg]
               forState:UIControlStateNormal];
  _snapToggle.tintColor = [UIColor whiteColor];
  [_snapToggle addTarget:self
                  action:@selector(_handleSnapToggle)
        forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_snapToggle];
  [NSLayoutConstraint activateConstraints:@[
    [_snapToggle.widthAnchor constraintEqualToConstant:36],
    [_snapToggle.heightAnchor constraintEqualToConstant:36],
    [_snapToggle.leadingAnchor constraintEqualToAnchor:_fab.trailingAnchor
                                              constant:12],
    [_snapToggle.centerYAnchor constraintEqualToAnchor:_fab.centerYAnchor],
  ]];

  _placeActionButton = [UIButton buttonWithType:UIButtonTypeCustom];
  _placeActionButton.translatesAutoresizingMaskIntoConstraints = NO;
  _placeActionButton.hidden = YES;
  _placeActionButton.layer.cornerRadius = 18;
  _placeActionButton.clipsToBounds = YES;
  _placeActionButton.backgroundColor = [UIColor colorWithWhite:0.2 alpha:0.9];
  UIImageSymbolConfiguration *placeBtnCfg = [UIImageSymbolConfiguration
      configurationWithPointSize:16
                          weight:UIImageSymbolWeightMedium];
  [_placeActionButton setImage:[UIImage systemImageNamed:@"arrow.uturn.backward"
                                       withConfiguration:placeBtnCfg]
                      forState:UIControlStateNormal];
  _placeActionButton.tintColor = [UIColor whiteColor];
  [_placeActionButton addTarget:self
                         action:@selector(_handlePlaceUndo)
               forControlEvents:UIControlEventTouchUpInside];
  UILongPressGestureRecognizer *placeLong =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(_handlePlaceActionLongPress:)];
  placeLong.minimumPressDuration = 0.4;
  [_placeActionButton addGestureRecognizer:placeLong];
  [self.view addSubview:_placeActionButton];
  [NSLayoutConstraint activateConstraints:@[
    [_placeActionButton.widthAnchor constraintEqualToConstant:36],
    [_placeActionButton.heightAnchor constraintEqualToConstant:36],
    [_placeActionButton.trailingAnchor
        constraintEqualToAnchor:_fab.leadingAnchor
                       constant:-12],
    [_placeActionButton.centerYAnchor
        constraintEqualToAnchor:_fab.centerYAnchor],
  ]];

  _renderer = [[Renderer alloc] initWithView:_renderView];
  _cameraController = [[CameraController alloc] init];
  _cameraController.renderer = _renderer;
  [_cameraController installOnView:self.view
                     excludedViews:@[ _toolbar, _pageBar3D, _pageBar2D ]];

  UITapGestureRecognizer *doubleTap = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(_handleDoubleTapMesh:)];
  doubleTap.numberOfTapsRequired = 2;
  doubleTap.delaysTouchesEnded = NO;
  [self.view addGestureRecognizer:doubleTap];

  UITapGestureRecognizer *singleTap = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(_handleSingleTapMesh:)];
  singleTap.numberOfTapsRequired = 1;
  [self.view addGestureRecognizer:singleTap];

  UILongPressGestureRecognizer *longPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(_handle3DLongPress:)];
  longPress.minimumPressDuration = 0.5;
  [self.view addGestureRecognizer:longPress];

  _undoRedoController = [[UndoRedoController alloc] init];
  _undoRedoController.delegate = self;
  _dragController = [[DragController alloc] init];
  _dragController.delegate = self;
  _projectService = [[ProjectService alloc] init];
  if (_bidUID.length > 0) {
    [self _loadProjectFromDatabase:_bidUID];
  }

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(_handleRenderConfigChanged)
             name:OSTRenderConfigChangedNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(_handleDidEnterBackground)
             name:UIApplicationDidEnterBackgroundNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(_handleWillEnterForeground)
             name:UIApplicationWillEnterForegroundNotification
           object:nil];

  _startTime = CACurrentMediaTime();
}

#pragma mark - Bottom Toolbar

- (void)_buildToolbar {
  _toolbar = [[UIView alloc] init];
  _toolbar.backgroundColor = [UIColor colorWithWhite:0 alpha:0.6];
  _toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_toolbar];

  UIView *border = [[UIView alloc] init];
  border.backgroundColor = [UIColor colorWithWhite:1 alpha:0.15];
  border.translatesAutoresizingMaskIntoConstraints = NO;
  [_toolbar addSubview:border];

  NSArray *titles = @[
    @"Reset", @"Undo", @"Redo", @"Delete", @"Conds", @"Layers", @"Export"
  ];
  SEL actions[] = {
      @selector(_handleFitScene),
      @selector(_handleUndo),
      @selector(_handleRedo),
      @selector(_handleDeleteTakeoff),
      @selector(_handleConditionsToggle),
      @selector(_handleLayerToggle),
      @selector(_handleExport),
  };

  int tagOverride[] = {kToolbar3DOnlyTag,
                       kToolbar2DOnlyTag,
                       kToolbar2DOnlyTag,
                       kToolbar2DOnlyTag,
                       0,
                       0,
                       0};

  UIStackView *stack = [[UIStackView alloc] init];
  stack.axis = UILayoutConstraintAxisHorizontal;
  stack.distribution = UIStackViewDistributionFillEqually;
  stack.translatesAutoresizingMaskIntoConstraints = NO;

  for (NSUInteger i = 0; i < titles.count; i++) {
    UIButton *btn = [UIButton buttonWithType:UIButtonTypeCustom];
    [btn setTitle:titles[i] forState:UIControlStateNormal];
    btn.titleLabel.font = [UIFont systemFontOfSize:14
                                            weight:UIFontWeightMedium];
    [btn setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [btn addTarget:self
                  action:actions[i]
        forControlEvents:UIControlEventTouchUpInside];
    if (tagOverride[i]) {
      btn.tag = tagOverride[i];
      btn.hidden = (tagOverride[i] == kToolbar2DOnlyTag);
    }
    if (actions[i] == @selector(_handleUndo))
      _undoButton = btn;
    if (actions[i] == @selector(_handleRedo))
      _redoButton = btn;
    if (actions[i] == @selector(_handleDeleteTakeoff))
      _deleteButton = btn;
    [stack addArrangedSubview:btn];
  }

  [_toolbar addSubview:stack];
  [NSLayoutConstraint activateConstraints:@[
    [border.topAnchor constraintEqualToAnchor:_toolbar.topAnchor],
    [border.leadingAnchor constraintEqualToAnchor:_toolbar.leadingAnchor],
    [border.trailingAnchor constraintEqualToAnchor:_toolbar.trailingAnchor],
    [border.heightAnchor constraintEqualToConstant:0.5],

    [stack.leadingAnchor constraintEqualToAnchor:_toolbar.leadingAnchor
                                        constant:8],
    [stack.trailingAnchor constraintEqualToAnchor:_toolbar.trailingAnchor
                                         constant:-8],
    [stack.topAnchor constraintEqualToAnchor:_toolbar.topAnchor],
    [stack.heightAnchor constraintEqualToConstant:44],
  ]];
}

- (void)_updateToolbarFor2DMode:(BOOL)is2D {
  for (UIView *sub in _toolbar.subviews) {
    if (![sub isKindOfClass:[UIStackView class]])
      continue;
    for (UIView *item in ((UIStackView *)sub).arrangedSubviews) {
      if (item.tag == kToolbar3DOnlyTag)
        item.hidden = is2D;
      if (item.tag == kToolbar2DOnlyTag)
        item.hidden = !is2D;
    }
  }
}

#pragma mark - View Mode Switching

- (void)_switchToMode2D:(BOOL)want2D {
  if (_is2DMode == want2D)
    return;
  _is2DMode = want2D;

  _cameraController.enabled = !_is2DMode;
  _renderView.hidden = _is2DMode;
  _planView.hidden = !_is2DMode;
  _fab.cameraMode = !_is2DMode;
  [_fab setCurrentMode:(_is2DMode ? _editMode : _cameraEditMode) animated:NO];
  BOOL showSnap =
      _is2DMode && (_editMode == ost_domain::EditMode::Select ||
                    _editMode == ost_domain::EditMode::PlaceTakeoff);
  _snapToggle.hidden = !showSnap;
  [self _updateToolbarFor2DMode:_is2DMode];
  [self _highlightModeLabels];

  if (_is2DMode) {
    BOOL resetZoom = !_hasShown2DView;
    _hasShown2DView = YES;
    [self _loadCurrentPageIntoPlanViewResetZoom:resetZoom];
  }
}

- (void)_loadCurrentPageIntoPlanView {
  [self _loadCurrentPageIntoPlanViewResetZoom:YES];
}

- (void)_loadCurrentPageIntoPlanViewResetZoom:(BOOL)resetZoom {
  if (_projectService.project.pages.empty())
    return;

  const std::string &pageUID = _projectService.selected2DPageUID;

  if (pageUID.empty()) {
    std::string empty;
    [_planView loadPageWithWidth:0
                          height:0
                        rotation:0
                    scaleFactor1:1.0
                    scaleFactor2:1.0
                       imagePath:empty
                       resetZoom:YES];
    return;
  }

  auto it = _projectService.project.pages.find(pageUID);
  if (it == _projectService.project.pages.end())
    return;

  const auto &page = it->second;
  [_planView loadPageWithWidth:page.width
                        height:page.height
                      rotation:page.rotation
                  scaleFactor1:page.scale_factor1
                  scaleFactor2:page.scale_factor2
                     imagePath:page.image_path
                     resetZoom:resetZoom];

  [self _refreshOverlays];
}

#pragma mark - Toolbar Actions

- (void)_handleFitScene {
  [_cameraController fitScene];
}

- (void)_handleDoubleTapMesh:(UITapGestureRecognizer *)tap {
  if (tap.state != UIGestureRecognizerStateRecognized)
    return;
  if (_editMode == ost_domain::EditMode::PlaceTakeoff)
    return;

  if (_is2DMode) {
    [self _handle2DDoubleTapAtPoint:[tap locationInView:_planView]];
    return;
  }

  CGPoint pt = [tap locationInView:self.view];
  CGFloat scale = [UIScreen mainScreen].scale;
  NSString *condUID = [_renderer conditionUIDAtScreenX:pt.x * scale
                                                     y:pt.y * scale];
  if (!condUID)
    return;

  [self _presentEditConditionWithUID:condUID fromController:self];
}

- (void)_handleSingleTapMesh:(UITapGestureRecognizer *)tap {
  if (tap.state != UIGestureRecognizerStateRecognized)
    return;
  if (_is2DMode)
    return;

  CGPoint pt = [tap locationInView:self.view];
  CGFloat scale = [UIScreen mainScreen].scale;
  _selectedConditionUID = [_renderer conditionUIDAtScreenX:pt.x * scale
                                                         y:pt.y * scale];
  NSString *tkUID = [_renderer takeoffUIDAtScreenX:pt.x * scale y:pt.y * scale];
  _selectedTakeoffUID = tkUID ? std::string(tkUID.UTF8String) : "";
  _activeHandleIndex = -1;

  [self _syncSelection];
}

- (void)_handle2DDoubleTapAtPoint:(CGPoint)planViewPoint {
  NSString *takeoffUID = [_planView takeoffUIDAtPoint:planViewPoint];
  if (!takeoffUID)
    return;

  std::string tUID = std::string(takeoffUID.UTF8String);
  for (const auto &t : _projectService.project.takeoffs) {
    if (t.uid == tUID) {
      NSString *condUID =
          [NSString stringWithUTF8String:t.condition_uid.c_str()];
      [self _presentEditConditionWithUID:condUID fromController:self];
      return;
    }
  }
}

- (void)_handle3DLongPress:(UILongPressGestureRecognizer *)lp {
  if (lp.state != UIGestureRecognizerStateBegan)
    return;
  if (_is2DMode)
    return;

  CGPoint pt = [lp locationInView:self.view];
  CGFloat scale = [UIScreen mainScreen].scale;
  NSString *takeoffUID = [_renderer takeoffUIDAtScreenX:pt.x * scale
                                                      y:pt.y * scale];

  if (takeoffUID) {
    [self _showTakeoffContextMenu:takeoffUID atPoint:pt inView:self.view];
  } else if (_selectedConditionUID) {
    [self _selectAllTakeoffsForCondition];
  }
}

- (void)_showTakeoffContextMenu:(NSString *)takeoffNS
                        atPoint:(CGPoint)screenPoint
                         inView:(UIView *)sourceView {
  std::string takeoffUID = std::string(takeoffNS.UTF8String);
  const ost_domain::Takeoff *tk = nullptr;
  for (const auto &t : _projectService.project.takeoffs) {
    if (t.uid == takeoffUID) {
      tk = &t;
      break;
    }
  }
  if (!tk)
    return;

  bool currentNeg = tk->is_negative;
  NSString *title = currentNeg ? @"\u2713 Count as Negative Quantity"
                               : @"Count as Negative Quantity";

  UIAlertController *sheet = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  std::string uid = takeoffUID;
  [sheet
      addAction:[UIAlertAction
                    actionWithTitle:title
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction *a) {
                              bool newNeg = !currentNeg;
                              [ImportService
                                  setTakeoffNegative:newNeg
                                          forTakeoff:[NSString
                                                         stringWithUTF8String:
                                                             uid.c_str()]
                                               inBid:self->_bidUID];

                              for (auto &t : self->_projectService
                                                 .mutableProject.takeoffs) {
                                if (t.uid == uid) {
                                  t.is_negative = newNeg;
                                  break;
                                }
                              }

                              [self _refreshOverlays];
                              [self _generateMeshesForSelectedPages];
                              [self _updateConditionQuantities];
                            }]];

  [sheet addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];

  if (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad) {
    sheet.popoverPresentationController.sourceView = sourceView;
    sheet.popoverPresentationController.sourceRect =
        CGRectMake(screenPoint.x, screenPoint.y, 1, 1);
  }

  [self presentViewController:sheet animated:YES completion:nil];
}

- (void)_selectAllTakeoffsForCondition {
  _selectedTakeoffUID.clear();
  _activeHandleIndex = -1;
  [self _syncSelection];
}

#pragma mark - Edit Mode

- (void)_setEditMode:(ost_domain::EditMode)mode {
  _editMode = mode;
  [_fab setCurrentMode:mode animated:YES];

  BOOL isSelect = (mode == ost_domain::EditMode::Select);
  BOOL isPlace = (mode == ost_domain::EditMode::PlaceTakeoff);
  _planView.editEnabled = isSelect;

  if (isPlace && !_placeConditionUID.empty()) {
    auto cdnIt = _projectService.project.conditions.find(_placeConditionUID);
    if (cdnIt != _projectService.project.conditions.end()) {
      [_planView enterPlacementModeWithCondition:cdnIt->second
                                   snapIncrement:_projectService.project.bid
                                                     .snap_increments_ost()];
    }
  } else {
    [_planView exitPlacementMode];
  }

  _snapToggle.hidden = !(isSelect || isPlace);

  if (!isSelect && !isPlace) {
    _activeHandleIndex = -1;
    [self _refreshOverlays];
  }
}

- (void)_handleSnapToggle {
  BOOL enabled = !_planView.angleSnappingEnabled;
  _planView.angleSnappingEnabled = enabled;
  UIColor *onColor = [UIColor colorWithRed:0.2 green:0.5 blue:1.0 alpha:0.9];
  UIColor *offColor = [UIColor colorWithWhite:0.3 alpha:0.85];
  [UIView animateWithDuration:0.15
                   animations:^{
                     self->_snapToggle.backgroundColor =
                         enabled ? onColor : offColor;
                   }];
}

- (void)_handlePlaceUndo {
  [_planView undoLastAreaPoint];
}

- (void)_handlePlaceActionLongPress:(UILongPressGestureRecognizer *)gr {
  if (gr.state != UIGestureRecognizerStateBegan)
    return;

  UIColor *origColor = _placeActionButton.backgroundColor;
  UIImageSymbolConfiguration *cfg = [UIImageSymbolConfiguration
      configurationWithPointSize:16
                          weight:UIImageSymbolWeightMedium];
  [_placeActionButton setImage:[UIImage systemImageNamed:@"xmark"
                                       withConfiguration:cfg]
                      forState:UIControlStateNormal];
  _placeActionButton.backgroundColor = [UIColor colorWithRed:0.8
                                                       green:0.2
                                                        blue:0.2
                                                       alpha:0.9];
  [_planView cancelAreaPlacement];
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        self->_placeActionButton.backgroundColor = origColor;
        [self->_placeActionButton
            setImage:[UIImage systemImageNamed:@"arrow.uturn.backward"
                             withConfiguration:cfg]
            forState:UIControlStateNormal];
      });
}

- (void)planView:(PlanView *)planView areaPlacementInProgress:(BOOL)inProgress {
  _fab.userInteractionEnabled = !inProgress;
  _fab.alpha = inProgress ? 0.3 : 1.0;
  _snapToggle.userInteractionEnabled = !inProgress;
  _snapToggle.alpha = inProgress ? 0.3 : 1.0;
  _placeActionButton.hidden = !inProgress;
  for (UIView *sub in _toolbar.subviews) {
    if ([sub isKindOfClass:[UIStackView class]]) {
      for (UIView *item in ((UIStackView *)sub).arrangedSubviews) {
        if ([item isKindOfClass:[UIButton class]]) {
          ((UIButton *)item).enabled = !inProgress;
          item.alpha = inProgress ? 0.3 : 1.0;
        }
      }
    }
  }
  if (!inProgress)
    [self _updateUndoRedoButtons];
}

#pragma mark - FloatingModeButtonDelegate

- (void)floatingModeButton:(FloatingModeButton *)fab
             didSelectMode:(ost_domain::EditMode)mode {
  if (!_is2DMode) {
    _cameraEditMode = mode;
    _cameraController.panMode = (mode == ost_domain::EditMode::Select);
    return;
  }
  [self _setEditMode:mode];
}

- (void)_refreshOverlays {
  if (_projectService.project.empty())
    return;
  const std::string &pageUID = _projectService.selected2DPageUID;
  if (pageUID.empty())
    return;

  std::string selCondition = _selectedConditionUID
                                 ? std::string(_selectedConditionUID.UTF8String)
                                 : "";

  [_planView renderOverlaysForPage:pageUID
                           project:_projectService.project
                      renderConfig:_projectService.renderConfig
                   selectedTakeoff:_selectedTakeoffUID
                 selectedCondition:selCondition
                      activeHandle:_activeHandleIndex];
}

- (void)_syncSelection {
  _renderer.selectedConditionUID = _selectedConditionUID;
  _renderer.selectedTakeoffUID =
      _selectedTakeoffUID.empty()
          ? nil
          : [NSString stringWithUTF8String:_selectedTakeoffUID.c_str()];
  _planView.selectedConditionUID =
      (_selectedTakeoffUID.empty() ? _selectedConditionUID : nil);

  if (_conditionsVC) {
    [_conditionsVC selectConditionWithUID:_selectedConditionUID animated:YES];
  }

  [self _refreshOverlays];
}

- (void)_updateConditionQuantities {
  if (!_conditionsVC)
    return;
  if (_projectService.project.empty())
    return;

  // Collect takeoffs from all visible pages (2D + 3D selections combined)
  const std::string &page2D = _projectService.selected2DPageUID;
  const auto &pages3D = _projectService.selected3DPageUIDs;

  std::vector<ost_domain::Takeoff> pageTakeoffs;
  for (const auto &tk : _projectService.project.takeoffs) {
    if (tk.page_uid == page2D || pages3D.count(tk.page_uid))
      pageTakeoffs.push_back(tk);
  }

  auto quantities = ost_domain::compute_page_quantities(
      _projectService.project.conditions, pageTakeoffs);
  [_conditionsVC updateQuantities:quantities];
}

- (void)_updateUndoRedoButtons {
  _undoButton.enabled = _undoRedoController.canUndo;
  _redoButton.enabled = _undoRedoController.canRedo;
  _undoButton.alpha = _undoButton.enabled ? 1.0 : 0.3;
  _redoButton.alpha = _redoButton.enabled ? 1.0 : 0.3;
  BOOL hasSelection = !_selectedTakeoffUID.empty();
  _deleteButton.enabled = hasSelection;
  _deleteButton.alpha = hasSelection ? 1.0 : 0.3;
}

- (void)_applyPosition:(const std::vector<double> &)position
             toTakeoff:(const std::string &)uid {
  for (auto &tk : _projectService.mutableProject.takeoffs) {
    if (tk.uid != uid)
      continue;
    tk.position = position;
    break;
  }
  if (_bidUID.length > 0) {
    [ImportService
        saveTakeoffPosition:position
                 forTakeoff:[NSString stringWithUTF8String:uid.c_str()]
                      inBid:_bidUID];
  }
  [self _refreshOverlays];
  [self _generateMeshesForSelectedPages];
}

- (void)_removeTakeoffFromProject:(const std::string &)uid {
  auto &takeoffs = _projectService.mutableProject.takeoffs;
  takeoffs.erase(std::remove_if(takeoffs.begin(), takeoffs.end(),
                                [&uid](const ost_domain::Takeoff &t) {
                                  return t.uid == uid;
                                }),
                 takeoffs.end());
  if (_bidUID.length > 0) {
    [ImportService deleteTakeoff:[NSString stringWithUTF8String:uid.c_str()]];
  }
}

- (void)_addTakeoffToProject:(ost_domain::Takeoff &)takeoff {
  if (_bidUID.length > 0) {
    NSString *uid = [ImportService insertTakeoff:takeoff inBid:_bidUID];
    if (uid)
      takeoff.uid = [uid UTF8String];
  }
  _projectService.mutableProject.takeoffs.push_back(takeoff);
}

- (void)_handleUndo {
  [_undoRedoController performUndo];
}

- (void)_handleRedo {
  [_undoRedoController performRedo];
}

- (void)_handleDeleteTakeoff {
  if (_selectedTakeoffUID.empty())
    return;
  _placeConditionUID.clear();
  _fab.placeModeAvailable = NO;
  [_undoRedoController deleteSelection:_selectedTakeoffUID
                          fromTakeoffs:_projectService.project.takeoffs];
}

#pragma mark - UndoRedoDelegate

- (void)undoRedoApplyPosition:(const std::vector<double> &)position
                    toTakeoff:(const std::string &)uid {
  [self _applyPosition:position toTakeoff:uid];
}

- (void)undoRedoRemoveTakeoff:(const std::string &)uid {
  [self _removeTakeoffFromProject:uid];
}

- (void)undoRedoAddTakeoff:(ost_domain::Takeoff &)takeoff {
  [self _addTakeoffToProject:takeoff];
}

- (void)undoRedoSelectTakeoff:(const std::string &)uid {
  _selectedTakeoffUID = uid;
  _activeHandleIndex = -1;
}

- (void)undoRedoClearSelection {
  _selectedTakeoffUID.clear();
  _activeHandleIndex = -1;
}

- (void)undoRedoDidModify {
  [self _refreshOverlays];
  [self _generateMeshesForSelectedPages];
  [self _updateUndoRedoButtons];
  [self _updateConditionQuantities];
}

#pragma mark - PlanViewEditDelegate

- (void)planView:(PlanView *)planView
    didTapTakeoff:(const std::string &)takeoffUID {
  _selectedTakeoffUID = takeoffUID;
  _activeHandleIndex = -1;

  _placeConditionUID.clear();
  for (const auto &tk : _projectService.project.takeoffs) {
    if (tk.uid == takeoffUID) {
      _placeConditionUID = tk.condition_uid;
      break;
    }
  }
  _fab.placeModeAvailable = !_placeConditionUID.empty();

  _selectedConditionUID =
      _placeConditionUID.empty()
          ? nil
          : [NSString stringWithUTF8String:_placeConditionUID.c_str()];

  [self _syncSelection];
  [self _updateUndoRedoButtons];
}

- (void)planView:(PlanView *)planView
    didTapHandle:(int)handleIndex
       ofTakeoff:(const std::string &)takeoffUID {
  _activeHandleIndex = (_activeHandleIndex == handleIndex) ? -1 : handleIndex;
  [self _refreshOverlays];
}

- (void)planView:(PlanView *)planView
    didTapEmptySpaceAtCanvasPoint:(CGPoint)point {
  if (!_selectedTakeoffUID.empty() || _selectedConditionUID) {
    _selectedTakeoffUID.clear();
    _placeConditionUID.clear();
    _selectedConditionUID = nil;
    _activeHandleIndex = -1;
    _fab.placeModeAvailable = NO;

    [self _syncSelection];
    [self _updateUndoRedoButtons];
  }
}

- (void)planView:(PlanView *)planView
    didLongPressTakeoff:(const std::string &)takeoffUID
                atPoint:(CGPoint)screenPoint {
  [self
      _showTakeoffContextMenu:[NSString stringWithUTF8String:takeoffUID.c_str()]
                      atPoint:screenPoint
                       inView:planView];
}

- (void)planView:(PlanView *)planView
    didLongPressEmptySpaceAtPoint:(CGPoint)screenPoint {
  if (_selectedConditionUID) {
    [self _selectAllTakeoffsForCondition];
  }
}

- (void)planView:(PlanView *)planView
    didDragTakeoff:(const std::string &)takeoffUID
       handleIndex:(int)handleIndex
       canvasDelta:(CGPoint)delta {
  const std::string &pageUID = _projectService.selected2DPageUID;
  if (pageUID.empty())
    return;
  auto pageIt = _projectService.project.pages.find(pageUID);
  if (pageIt == _projectService.project.pages.end())
    return;
  const auto &page = pageIt->second;

  bool isMulti = _selectedTakeoffUID.empty() && _selectedConditionUID != nil;

  if (isMulti) {
    std::string condUID = std::string(_selectedConditionUID.UTF8String);
    auto ostDelta =
        ost_coord::canvas_delta_to_ost(delta.x, delta.y, page.scale_factor1,
                                       page.scale_factor2, page.rotation);

    for (auto &tk : _projectService.mutableProject.takeoffs) {
      if (tk.condition_uid != condUID || tk.page_uid != pageUID)
        continue;
      if (_multiDragStartPositions.find(tk.uid) ==
          _multiDragStartPositions.end()) {
        _multiDragStartPositions[tk.uid] = tk.position;
      }
      const auto &startPos = _multiDragStartPositions[tk.uid];
      for (size_t i = 0; i + 1 < tk.position.size(); i += 2) {
        tk.position[i] = startPos[i] + ostDelta[0];
        tk.position[i + 1] = startPos[i + 1] + ostDelta[1];
      }
    }
    [self _refreshOverlays];
    [self _generateMeshesForSelectedPages];
  } else {
    for (auto &tk : _projectService.mutableProject.takeoffs) {
      if (tk.uid != takeoffUID)
        continue;
      [_dragController
             handleDrag:takeoffUID
               position:tk.position
            handleIndex:_activeHandleIndex
            canvasDelta:delta
           scaleFactor1:page.scale_factor1
           scaleFactor2:page.scale_factor2
           pageRotation:page.rotation
          snapIncrement:_projectService.project.bid.snap_increments_ost()
          angleSnapping:_planView.angleSnappingEnabled];
      break;
    }
  }
}

- (void)planView:(PlanView *)planView
    didEndDragTakeoff:(const std::string &)takeoffUID {
  bool isMulti = !_multiDragStartPositions.empty();

  if (isMulti) {
    for (const auto &pair : _multiDragStartPositions) {
      for (const auto &tk : _projectService.project.takeoffs) {
        if (tk.uid != pair.first)
          continue;
        if (pair.second != tk.position) {
          [_undoRedoController pushMoveForTakeoff:pair.first
                                      oldPosition:pair.second
                                      newPosition:tk.position];
          if (_bidUID.length > 0) {
            [ImportService
                saveTakeoffPosition:tk.position
                         forTakeoff:[NSString
                                        stringWithUTF8String:pair.first.c_str()]
                              inBid:_bidUID];
          }
        }
        break;
      }
    }
    _multiDragStartPositions.clear();
    [self _updateUndoRedoButtons];
    [self _updateConditionQuantities];
  } else {
    for (const auto &tk : _projectService.project.takeoffs) {
      if (tk.uid != takeoffUID)
        continue;
      [_dragController endDrag:takeoffUID position:tk.position];
      break;
    }
  }
}

#pragma mark - DragDelegate

- (void)dragDidUpdatePosition {
  [self _refreshOverlays];
  [self _generateMeshesForSelectedPages];
}

- (void)dragDidEndForTakeoff:(const std::string &)uid
                 oldPosition:(const std::vector<double> &)oldPos
                 newPosition:(const std::vector<double> &)newPos {
  if (oldPos != newPos) {
    [_undoRedoController pushMoveForTakeoff:uid
                                oldPosition:oldPos
                                newPosition:newPos];
    [self _updateUndoRedoButtons];
    if (_bidUID.length > 0) {
      [ImportService
          saveTakeoffPosition:newPos
                   forTakeoff:[NSString stringWithUTF8String:uid.c_str()]
                        inBid:_bidUID];
    }
  }
  [self _updateConditionQuantities];
}

- (void)planView:(PlanView *)planView
    didPlaceTakeoffWithCondition:(const std::string &)conditionUID
                        position:(const std::vector<double> &)position {
  if (_bidUID.length == 0)
    return;
  const std::string &pageUID = _projectService.selected2DPageUID;
  if (pageUID.empty())
    return;

  ost_domain::Takeoff tk;
  tk.condition_uid = conditionUID;
  tk.page_uid = pageUID;
  tk.position = position;

  [self _addTakeoffToProject:tk];
  [_undoRedoController pushPlaceForTakeoff:tk];

  _selectedTakeoffUID = tk.uid;
  _activeHandleIndex = -1;

  [self _refreshOverlays];
  [self _generateMeshesForSelectedPages];
  [self _updateConditionQuantities];
  [self _updateUndoRedoButtons];
}

#pragma mark - Project Loading

- (void)_loadProjectFromDatabase:(NSString *)bidUID {
  ost_data::ProjectData proj = [ImportService loadProjectWithUID:bidUID];
  if (!proj.empty()) {
    [self _loadProject:proj];
  }
}

- (void)_loadProject:(const ost_data::ProjectData &)project {

  [_projectService loadProject:project];

  [_undoRedoController clear];
  [self _updateUndoRedoButtons];

  self.title = [NSString stringWithUTF8String:project.bid_name.c_str()];

  [self _applyRenderConfig];

  [self _populatePageSelector];
  [self _highlightModeLabels];
  [_projectService generateMeshesIntoRenderer:_renderer];
  [_renderer fitCameraToMeshes];

  if (_is2DMode) {
    [self _loadCurrentPageIntoPlanView];
  }
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)_handleRenderConfigChanged {
  if (_projectService.project.empty())
    return;
  [self _applyRenderConfig];
  [self _generateMeshesForSelectedPages];

  if (_is2DMode) {
    [self _loadCurrentPageIntoPlanViewResetZoom:NO];
  }
}

- (void)_handleDidEnterBackground {
  _displayLink.paused = YES;
}

- (void)_handleWillEnterForeground {
  _startTime = CACurrentMediaTime();
  _displayLink.paused = NO;
}

- (void)_applyRenderConfig {
  NSDictionary *cfg = [ImportService loadRenderConfig];
  ost_domain::RenderConfig rc;
  NSString *mode = cfg[@"color_mode"];
  if (mode)
    rc.color_mode = [mode UTF8String];
  rc.grayscale_enabled = [cfg[@"grayscale_enabled"] boolValue];
  _projectService.renderConfig = rc;
}

#pragma mark - Mesh Generation

- (void)_generateMeshesForSelectedPages {
  [_projectService generateMeshesIntoRenderer:_renderer];
}

#pragma mark - Lifecycle

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_renderer startBgfx];
  _displayLink = [CADisplayLink displayLinkWithTarget:self
                                             selector:@selector(_renderLoop:)];
  [_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                     forMode:NSRunLoopCommonModes];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [_displayLink invalidate];
  _displayLink = nil;
  [_renderer shutdown];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Defer resize until after the first frame has been rendered
  if (!_displayLink)
    return;
  CGSize size = self.view.bounds.size;
  CGFloat scale = [UIScreen mainScreen].scale;
  [_renderer resize:(int)(size.width * scale)
             height:(int)(size.height * scale)];
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}
- (BOOL)prefersHomeIndicatorAutoHidden {
  return YES;
}

#pragma mark - Render Loop

- (void)_renderLoop:(CADisplayLink *)link {
  if (_is2DMode)
    return;

  CFTimeInterval now = CACurrentMediaTime();
  float time = (float)(now - _startTime);
  [_renderer renderFrameWithTime:time];
}

@end
