#include "../../Core/Data/PageNavigation.hpp"
#include "../../Core/Domain/BrowserRules.hpp"
#include "../../Core/Domain/FailureText.hpp"
#include "../../Core/Domain/NegativeChange.hpp"
#include "../../Core/Domain/QuantityPages.hpp"
#include "../../Core/Domain/TakeoffSelection.hpp"
#include "../../Core/Geometry/coord_transform.hpp"
#include "../../Core/Geometry/drag_rules.hpp"
#import "../../Rendering/BgfxView.h"
#import "Components/CameraController.h"
#import "Components/DragController.h"
#import "Components/UndoRedoController.h"
#import "ViewController_Internal.h"
#import <QuartzCore/QuartzCore.h>
#include <algorithm>
#include <cmath>
#include <set>
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
  // The placement that hiding its layer left, put back when the layer is shown again (COND-PLACE-01), and the number of changes of
  // the edit mode, Windows PlanView.tool_revision: a suspension only comes back while no other tool was chosen in between
  std::optional<ost_domain::SuspendedPlacement> _suspendedPlacement;
  unsigned long long _toolRevision;
  // A group drag: the position each moved takeoff started from (the selected
  // takeoffs and the descendants of the selected areas). Empty between gestures.
  ost_drag::PositionMap _dragStartPositions;
  // Why the bid could not be loaded; shown once the screen is up (DBP-M02)
  NSString *_loadErrorToPresent;
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
- (std::optional<ost_domain::SuspendedPlacement> &)suspendedPlacement {
  return _suspendedPlacement;
}
- (unsigned long long)toolRevision {
  return _toolRevision;
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
    // The 3D view picks takeoffs of any selected page: one that is not on the page shown now is dropped, so that Delete can
    // never act on a takeoff the user does not see (RPV-16, TakeoffSelection.hpp keep_selection_on_page).
    const std::string pickedInThe3DView = _selectedTakeoffUID;
    [self _applySelectionState:ost_domain::keep_selection_on_page(
                                   _projectService.project.takeoffs,
                                   [self _selectionState],
                                   _projectService.selected2DPageUID)];
    if (_selectedTakeoffUID != pickedInThe3DView)
      [_planView clearTapSelection];
    BOOL resetZoom = !_hasShown2DView;
    _hasShown2DView = YES;
    [self _loadCurrentPageIntoPlanViewResetZoom:resetZoom];
    [self _syncSelection];
  }
  // The totals follow the view that is open now (QTY-11, QTY-15)
  [self _updateConditionQuantities];
}

// Windows PlanView.load_page drops the selection when the page changes
// (RPV-16): nothing stays selected, so Delete can no longer act on a takeoff of
// the previous page. A placement in progress survives the switch (Windows
// preserve_place), so the condition being placed stays selected then.
- (void)_clearSelectionForPageSwitch {
  [self _applySelectionState:ost_domain::clear_selection_for_page_switch(
                                 [self _selectionState])];
  [_planView clearTapSelection];
  _renderer.selectedConditionUID = _selectedConditionUID;
  _renderer.selectedTakeoffUID = nil;
  _planView.selectedConditionUID = _selectedConditionUID;
  if (_conditionsVC)
    [_conditionsVC selectConditionWithUID:_selectedConditionUID animated:YES];
  [self _updateUndoRedoButtons];
}

- (void)_loadCurrentPageIntoPlanView {
  [self _loadCurrentPageIntoPlanViewResetZoom:YES];
}

- (void)_loadCurrentPageIntoPlanViewResetZoom:(BOOL)resetZoom {
  if (_projectService.project.pages.empty())
    return;

  const std::string &pageUID = _projectService.selected2DPageUID;

  if (pageUID.empty()) {
    // no page selected: a blank default page
    [_planView loadPage:ost_domain::Page{} resetZoom:YES];
    return;
  }

  auto it = _projectService.project.pages.find(pageUID);
  if (it == _projectService.project.pages.end())
    return;

  // the page as stored: PDF page (Index1), size, rotation, flips and scale (LP-16)
  [_planView loadPage:it->second resetZoom:resetZoom];

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
  // A long press that starts on a page button of either bar opens the page menu only: this whole-view recogniser has no delegate and
  // both bars stay visible in 3D mode, so without this guard the press would also pick the takeoff under the finger or select all
  // takeoffs of the selected condition (plan 4.8, 5.5; device check M1-14).
  for (UIScrollView *bar in @[ _pageBar3D, _pageBar2D ]) {
    if (!bar.hidden &&
        CGRectContainsPoint(bar.bounds, [lp locationInView:bar]))
      return;
  }

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

  // The item and its check mark come from ost_domain::negative_toggle_for_press
  // (host-tested). iOS has no multi-selection and no paste before M2, so the
  // group is EMPTY and the single takeoff path applies: the item is offered, it
  // is checked when that takeoff is negative. M2 passes the pasted takeoff uids
  // here and in _toggleNegativeForPressedTakeoff: (N-M2-04, owner decision 3).
  const ost_domain::NegativeToggle toggle =
      ost_domain::negative_toggle_for_press(_projectService.project.takeoffs,
                                            std::vector<std::string>(),
                                            takeoffUID);
  if (!toggle.offered)
    return;
  NSString *title = toggle.checked ? @"\u2713 Count as Negative Quantity"
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
                              [self _toggleNegativeForPressedTakeoff:uid];
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

// Windows on_set_negative with the plan-view selection of iOS, which is the one
// pressed takeoff: the negative flag of that takeoff alone, in ONE history
// entry, so one undo restores it and one redo applies it again (UND-09). A
// condition that is merely selected in the sidebar widens nothing; the decision
// is ost_domain::negative_toggle_for_press (host-tested). The group is empty
// until M2: the paste wiring passes the pasted takeoff uids, and then one press
// sets the whole group in this same ONE entry (owner decision 3).
- (void)_toggleNegativeForPressedTakeoff:(const std::string &)pressedUID {
  const ost_domain::NegativeToggle toggle =
      ost_domain::negative_toggle_for_press(_projectService.project.takeoffs,
                                            std::vector<std::string>(),
                                            pressedUID);
  if (!toggle.offered || toggle.changes.empty())
    return;
  const std::vector<NegativeChange> &changes = toggle.changes;
  const std::vector<std::pair<std::string, bool>> updates =
      ost_domain::negative_values(changes, false);

  // Stored first: a refused write changes nothing, adds no history and is
  // reported (DBP-11, DBP-17, UND-02).
  if (![self _applyNegatives:updates]) {
    [self _presentSaveFailure];
    return;
  }
  [_undoRedoController pushNegativeChanges:changes];
  [self _refreshOverlays];
  [self _generateMeshesForSelectedPages];
  [self _updateConditionQuantities];
  [self _updateUndoRedoButtons];
}

// Writes and applies the negative flags of one step: all of them or none.
- (BOOL)_applyNegatives:
    (const std::vector<std::pair<std::string, bool>> &)updates {
  if (_bidUID.length > 0 &&
      ![ImportService setTakeoffsNegative:updates inBid:_bidUID])
    return NO;
  for (const auto &update : updates) {
    for (auto &t : _projectService.mutableProject.takeoffs) {
      if (t.uid == update.first) {
        t.is_negative = update.second;
        break;
      }
    }
  }
  return YES;
}

- (void)_selectAllTakeoffsForCondition {
  _selectedTakeoffUID.clear();
  _activeHandleIndex = -1;
  [self _syncSelection];
}

#pragma mark - Edit Mode

- (void)_setEditMode:(ost_domain::EditMode)mode {
  if (mode != _editMode)
    _toolRevision++;
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

// Windows handle_place_press (placement_mode.py:1648) refuses at the first press: for a condition that is gone or not visible no
// preview, no point of a polygon and no in-progress state is made. PlanView asks this at the start of every placement gesture
// (a tap, the start of a drag). The Image layer toggle leaves the tool armed on a condition that is then hidden (COND-PLACE-01).
// The decision is Core ost_data::placement_tap_accepted.
- (BOOL)planView:(PlanView *)planView
    canPlaceWithCondition:(const std::string &)conditionUID {
  return ost_data::placement_tap_accepted(_projectService.project,
                                          conditionUID);
}

- (void)planView:(PlanView *)planView areaPlacementInProgress:(BOOL)inProgress {
  self.areaPlacementInProgress = inProgress; // read by the page menu when it is shown (ViewController+PageMenu.mm)
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
  // Windows enter_place_mode_for_condition (placement_mode.py:1869) and PlacementCoordinator._is_condition_placeable: a condition
  // that is gone or on a hidden layer is not entered. The button keeps offering the condition of the last takeoff or list
  // selection, and the Image layer toggle leaves the tool armed on a condition that is then hidden (COND-PLACE-01), so the way
  // back into the Place mode would arm a hidden condition. The decision is Core ost_data::placement_tap_accepted (the "exists and
  // is visible" of placement_for_selected_condition, which the Conditions list applies). A refusal is Windows' force_exit, like
  // the Conditions list: the offered condition is dropped (_leavePlacement clears it and switches the Place button off, and goes
  // back to View when the tool was in Place), so the tap cycle goes Select -> View and the ring shows Place disabled instead of
  // proposing the refused Place again; selecting the condition from the list offers it again. The button then shows the mode the
  // app is really in.
  if (mode == ost_domain::EditMode::PlaceTakeoff &&
      !ost_data::placement_tap_accepted(_projectService.project,
                                        _placeConditionUID)) {
    [self _leavePlacement];
    [fab setCurrentMode:_editMode animated:YES];
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

  [self _updateDeleteButton];
  [self _refreshOverlays];
}

// The selection of the 2D view as one value for the Core rules (TakeoffSelection.hpp) and back. The condition that is highlighted
// and the condition the placement tool places are kept apart there, as the Windows sidebar projection keeps them.
- (ost_domain::SelectionState)_selectionState {
  ost_domain::SelectionState state;
  state.mode = _editMode;
  state.takeoff_uid = _selectedTakeoffUID;
  state.condition_uid = _selectedConditionUID
                            ? std::string(_selectedConditionUID.UTF8String)
                            : std::string();
  state.place_condition_uid = _placeConditionUID;
  state.active_handle = _activeHandleIndex;
  return state;
}

- (void)_applySelectionState:(const ost_domain::SelectionState &)state {
  _selectedTakeoffUID = state.takeoff_uid;
  _selectedConditionUID =
      state.condition_uid.empty()
          ? nil
          : [NSString stringWithUTF8String:state.condition_uid.c_str()];
  _placeConditionUID = state.place_condition_uid;
  _activeHandleIndex = state.active_handle;
  _fab.placeModeAvailable = !_placeConditionUID.empty();
}

- (void)_updateConditionQuantities {
  if (!_conditionsVC)
    return;
  if (_projectService.project.empty())
    return;

  // The totals of the open view only: the selected 3D pages in the 3D view,
  // the active 2D page in the 2D view, nothing without a page (Windows
  // update_conditions_quantities, QTY-11).
  auto quantities = ost_domain::compute_view_quantities(
      _projectService.project.conditions, _projectService.project.takeoffs,
      !_is2DMode, _projectService.selected2DPageUID,
      ost_data::selected_pages_in_tree_order(
          _projectService.project, _projectService.selected3DPageUIDs));
  [_conditionsVC updateQuantities:quantities];
}

- (void)_updateUndoRedoButtons {
  _undoButton.enabled = _undoRedoController.canUndo;
  _redoButton.enabled = _undoRedoController.canRedo;
  _undoButton.alpha = _undoButton.enabled ? 1.0 : 0.3;
  _redoButton.alpha = _redoButton.enabled ? 1.0 : 0.3;
  [self _updateDeleteButton];
}

// Delete is on while a takeoff that exists is selected. A highlighted condition is not a selection of takeoffs (Windows
// plan_view.has_selection), so it never enables Delete.
- (void)_updateDeleteButton {
  BOOL hasSelection = ost_domain::delete_enabled(
      _projectService.project.takeoffs, [self _selectionState]);
  _deleteButton.enabled = hasSelection;
  _deleteButton.alpha = hasSelection ? 1.0 : 0.3;
}

// Stored first, then applied: a refused or failed write leaves the project
// exactly as it was and answers NO (DBP-11, DBP-17, UND-02).
- (BOOL)_applyPosition:(const std::vector<double> &)position
             toTakeoff:(const std::string &)uid {
  if (_bidUID.length > 0 &&
      ![ImportService
          saveTakeoffPosition:position
                   forTakeoff:[NSString stringWithUTF8String:uid.c_str()]
                        inBid:_bidUID])
    return NO;
  for (auto &tk : _projectService.mutableProject.takeoffs) {
    if (tk.uid != uid)
      continue;
    tk.position = position;
    break;
  }
  [self _refreshOverlays];
  [self _generateMeshesForSelectedPages];
  return YES;
}

// The takeoffs of one delete, together (UND-02: one transaction): they leave
// the database first and the project only when that worked.
- (BOOL)_removeTakeoffsFromProject:(const std::vector<std::string> &)uids {
  if (_bidUID.length > 0 && ![ImportService deleteTakeoffs:uids])
    return NO;
  const std::set<std::string> doomed(uids.begin(), uids.end());
  auto &takeoffs = _projectService.mutableProject.takeoffs;
  takeoffs.erase(std::remove_if(takeoffs.begin(), takeoffs.end(),
                                [&doomed](const ost_domain::Takeoff &t) {
                                  return doomed.count(t.uid) != 0;
                                }),
                 takeoffs.end());
  return YES;
}

// The takeoffs of one undo / redo step, together, with their own uids: they
// enter the database first and the project only when that worked. The list the
// project holds stays light: the extras of a restored row stay on the history
// entry, which needs them again after a redo (UND-06).
- (BOOL)_addTakeoffsToProject:(std::vector<ost_domain::Takeoff> &)takeoffs {
  if (_bidUID.length > 0 &&
      ![ImportService insertTakeoffs:takeoffs inBid:_bidUID])
    return NO;
  for (const auto &takeoff : takeoffs) {
    ost_domain::Takeoff stored = takeoff;
    stored.extras = ost_domain::TakeoffExtras();
    _projectService.mutableProject.takeoffs.push_back(std::move(stored));
  }
  return YES;
}

// A takeoff that was just placed: NO, and it is not added to the project, when
// the database refuses it (unknown condition / page, ...): it would stay in
// memory with an empty uid (DBP-11, DBP-17).
- (BOOL)_addTakeoffToProject:(ost_domain::Takeoff &)takeoff {
  if (_bidUID.length > 0) {
    NSString *uid = [ImportService insertTakeoff:takeoff inBid:_bidUID];
    if (!uid)
      return NO;
    takeoff.uid = [uid UTF8String];
  }
  ost_domain::Takeoff stored = takeoff;
  stored.extras = ost_domain::TakeoffExtras();
  _projectService.mutableProject.takeoffs.push_back(std::move(stored));
  return YES;
}

// A forward action (place, delete, negative flag) whose write was refused: the
// change was not made. Windows only logs this; the phone has to say it.
- (void)_presentSaveFailure {
  UIViewController *presenter = self;
  while (presenter.presentedViewController)
    presenter = presenter.presentedViewController;
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:@"Not Saved"
                       message:@"The change could not be saved and was not made."
                preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [presenter presentViewController:alert animated:YES completion:nil];
}

- (void)_clearUndoHistory {
  [_undoRedoController clear];
  [self _updateUndoRedoButtons];
}

- (void)_handleUndo {
  [_undoRedoController performUndo];
}

- (void)_handleRedo {
  [_undoRedoController performRedo];
}

- (void)_handleDeleteTakeoff {
  // Delete removes the selected takeoff and its descendants (UND-08) and nothing else: it takes no mode and no condition
  // (Windows delete_selected / on_elements_deleted), and it never deletes a condition. The selection after a successful delete is
  // set by undoRedoClearSelection (nothing selected, no condition highlighted, a running placement keeps its tool).
  if (!ost_domain::delete_enabled(_projectService.project.takeoffs,
                                  [self _selectionState]))
    return;
  // A failed delete keeps the selection (Windows selects the takeoffs again)
  if (![_undoRedoController deleteSelection:_selectedTakeoffUID
                               fromTakeoffs:_projectService.project.takeoffs]) {
    [self _presentSaveFailure];
    return;
  }
}

#pragma mark - UndoRedoDelegate

- (BOOL)undoRedoApplyPosition:(const std::vector<double> &)position
                    toTakeoff:(const std::string &)uid {
  return [self _applyPosition:position toTakeoff:uid];
}

- (BOOL)undoRedoApplyNegatives:
    (const std::vector<std::pair<std::string, bool>> &)negatives {
  return [self _applyNegatives:negatives];
}

- (BOOL)undoRedoRemoveTakeoffs:(const std::vector<std::string> &)uids {
  return [self _removeTakeoffsFromProject:uids];
}

- (BOOL)undoRedoAddTakeoffs:(std::vector<ost_domain::Takeoff> &)takeoffs {
  return [self _addTakeoffsToProject:takeoffs];
}

- (void)undoRedoSnapshotTakeoff:(ost_domain::Takeoff &)takeoff {
  // the whole stored row, taken before the delete (UND-06)
  if (_bidUID.length > 0)
    [ImportService loadExtrasForTakeoff:takeoff inBid:_bidUID];
}

// Windows selects the restored / moved takeoffs only while their page is the one
// on screen (plan_view_action_handler _history_selection_owner): a takeoff of
// another page stays unselected, so that Delete can never act on a takeoff the
// user does not see (RPV-16, UND-10).
- (void)undoRedoSelectTakeoff:(const std::string &)uid {
  [self _applySelectionState:ost_domain::select_after_history(
                                 _projectService.project.takeoffs,
                                 [self _selectionState], uid,
                                 _projectService.selected2DPageUID)];
  [self _syncSelection];
}

// Windows delete_selected / _redo_delete: nothing is selected afterwards and no condition stays highlighted, so the overlay draws
// no group selection on the other takeoffs of the condition.
- (void)undoRedoClearSelection {
  [self _applySelectionState:ost_domain::clear_selection([self _selectionState])];
  [self _syncSelection];
}

// Windows HistoryTargetUnavailableError: the page or a takeoff the replay needs is gone from the project (the rule is Core
// ost_domain::history_targets_unavailable). The undo / redo is then refused before it writes and the entry stays.
- (BOOL)undoRedoReplayTargetsUnavailable:(const ost_domain::HistoryReplayTargets &)targets {
  return ost_domain::history_targets_unavailable(_projectService.project, targets);
}

// A user-triggered undo / redo that did not run (Windows show_error("Undo" / "Redo", ...)): same alert shape as -_presentSaveFailure.
- (void)undoRedoPresentRefusalWithTitle:(NSString *)title message:(NSString *)message {
  // A locked bid is told so ("This bid is locked."), not that the change "could not be undone" (practically unreachable today: takeoff writes are not
  // lock-guarded yet, N-M1-01; kept so that a later lock retrofit says "This bid is locked." instead of "could not be undone").
  if ([self _refuseIfBidLocked])
    return;
  UIViewController *presenter = self;
  while (presenter.presentedViewController)
    presenter = presenter.presentedViewController;
  UIAlertController *alert = [UIAlertController alertControllerWithTitle:title
                                                                 message:message
                                                          preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
  [presenter presentViewController:alert animated:YES completion:nil];
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
  [self _applySelectionState:ost_domain::select_takeoff(
                                 _projectService.project.takeoffs,
                                 [self _selectionState], takeoffUID)];

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
    [self _applySelectionState:ost_domain::clear_selection(
                                   [self _selectionState])];

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
  const double snap = _projectService.project.bid.snap_increments_ost();

  bool isMulti = _selectedTakeoffUID.empty() && _selectedConditionUID != nil;

  if (isMulti) {
    std::string condUID = std::string(_selectedConditionUID.UTF8String);
    auto ostDelta = ost_coord::canvas_delta_to_ost(
        delta.x, delta.y, page.scale_factor1, page.scale_factor2, page.rotation,
        page.flip_x, page.flip_y);
    if (!std::isfinite(ostDelta[0]) || !std::isfinite(ostDelta[1]))
      return;

    const auto &project = _projectService.project;
    ost_hierarchy::HierarchyModel model(project.takeoffs, project.conditions);
    if (_dragStartPositions.empty()) {
      // First step of the gesture: the takeoffs of the selected condition on
      // this page, and the descendants of the selected areas that move along.
      std::set<std::string> selected;
      for (const auto &tk : project.takeoffs) {
        if (tk.condition_uid == condUID && tk.page_uid == pageUID)
          selected.insert(tk.uid);
      }
      std::set<std::string> dragged = selected;
      for (const auto &link : ost_drag::area_child_parent_map(model, selected))
        dragged.insert(link.first);
      for (const auto &tk : project.takeoffs) {
        if (dragged.count(tk.uid) != 0 && !tk.position.empty())
          _dragStartPositions[tk.uid] = tk.position;
      }
    }

    // The Windows group drag: snapped delta, children follow their selected
    // area, the whole move is refused when a hole or attachment would end up
    // outside its parent (then every position of the group stays as it was).
    ost_drag::PositionMap moved = ost_drag::compute_group_translation_positions(
        model, _dragStartPositions, ostDelta[0], ostDelta[1], snap);
    for (auto &tk : _projectService.mutableProject.takeoffs) {
      auto it = moved.find(tk.uid);
      if (it != moved.end())
        tk.position = it->second;
    }
    [self _refreshOverlays];
    [self _generateMeshesForSelectedPages];
  } else {
    ost_domain::Takeoff *dragged = nullptr;
    for (auto &tk : _projectService.mutableProject.takeoffs) {
      if (tk.uid == takeoffUID) {
        dragged = &tk;
        break;
      }
    }
    if (!dragged)
      return;
    auto cdnIt = _projectService.project.conditions.find(dragged->condition_uid);
    if (cdnIt == _projectService.project.conditions.end())
      return;
    // Windows _drag_handle_corner_count: only an area has corners
    int cornerCount =
        cdnIt->second.is_area() ? (int)(dragged->position.size() / 2) : 0;
    if ([_dragController handleDrag:takeoffUID
                           position:dragged->position
                        handleIndex:_activeHandleIndex
                        cornerCount:cornerCount
                        canvasDelta:delta
                       scaleFactor1:page.scale_factor1
                       scaleFactor2:page.scale_factor2
                       pageRotation:page.rotation
                              flipX:page.flip_x
                              flipY:page.flip_y
                      snapIncrement:snap
                      angleSnapping:_planView.angleSnappingEnabled]) {
      [self _refreshOverlays];
      [self _generateMeshesForSelectedPages];
    }
  }
}

- (void)planView:(PlanView *)planView
    didEndDragTakeoff:(const std::string &)takeoffUID {
  bool isMulti = !_dragStartPositions.empty();

  if (isMulti) {
    // One history entry and one transaction for every takeoff the group drag
    // moved; a takeoff that did not move (within 1e-9) is put back exactly.
    std::vector<ost_drag::PositionChange> changes;
    for (auto &tk : _projectService.mutableProject.takeoffs) {
      auto start = _dragStartPositions.find(tk.uid);
      if (start == _dragStartPositions.end())
        continue;
      if (ost_drag::positions_meaningfully_different(start->second,
                                                     tk.position)) {
        changes.push_back({tk.uid, start->second, tk.position});
      } else {
        tk.position = start->second;
      }
    }
    _dragStartPositions.clear();
    [self _commitPositionChanges:changes];
    [self _refreshOverlays];
    [self _generateMeshesForSelectedPages];
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

// Writes and records the positions one gesture changed: one transaction and
// one history entry for all of them (Windows on_positions_flushed). When the
// write fails nothing was stored, so the in-memory positions are put back and
// no history entry is made.
- (void)_commitPositionChanges:
    (const std::vector<ost_drag::PositionChange> &)changes {
  if (changes.empty())
    return;
  if (_bidUID.length > 0) {
    std::vector<std::pair<std::string, std::vector<double>>> rows;
    rows.reserve(changes.size());
    for (const auto &change : changes)
      rows.push_back({change.uid, change.new_position});
    if (![ImportService saveTakeoffPositions:rows inBid:_bidUID]) {
      ost_drag::PositionMap before;
      for (const auto &change : changes)
        before[change.uid] = change.old_position;
      for (auto &tk : _projectService.mutableProject.takeoffs) {
        auto it = before.find(tk.uid);
        if (it != before.end())
          tk.position = it->second;
      }
      return;
    }
  }
  if (changes.size() == 1) {
    [_undoRedoController pushMoveForTakeoff:changes[0].uid
                                oldPosition:changes[0].old_position
                                newPosition:changes[0].new_position];
  } else {
    [_undoRedoController pushMoveForTakeoffs:changes];
  }
  [self _updateUndoRedoButtons];
}

#pragma mark - DragDelegate

- (BOOL)dragShouldAcceptPosition:(const std::vector<double> &)candidate
                      forTakeoff:(const std::string &)uid
                     handleIndex:(int)handleIndex
               lastValidPosition:(const std::vector<double> &)lastValidPosition {
  const auto &project = _projectService.project;
  ost_hierarchy::HierarchyModel model(project.takeoffs, project.conditions);
  const ost_domain::Takeoff *takeoff = model.find_takeoff(uid);
  if (!takeoff)
    return NO;
  const ost_domain::Condition *condition =
      model.find_condition(takeoff->condition_uid);
  if (!condition)
    return NO;
  return ost_drag::drag_candidate_valid(model, *takeoff, *condition, candidate,
                                        handleIndex, lastValidPosition)
             ? YES
             : NO;
}

- (void)dragDidEndForTakeoff:(const std::string &)uid
                 oldPosition:(const std::vector<double> &)oldPos
                 newPosition:(const std::vector<double> &)newPos {
  auto &takeoffs = _projectService.mutableProject.takeoffs;
  if (ost_drag::positions_meaningfully_different(oldPos, newPos)) {
    // The dragged takeoff, then (area dragged by its body) the descendants that
    // follow it by the displacement of its first point; they are still where
    // the drag found them.
    std::vector<ost_drag::PositionChange> changes = {{uid, oldPos, newPos}};
    {
      const auto &project = _projectService.project;
      ost_hierarchy::HierarchyModel model(project.takeoffs, project.conditions);
      for (auto &follower : ost_drag::descendants_following_body_drag(
               model, uid, _activeHandleIndex, oldPos, newPos))
        changes.push_back(std::move(follower));
    }
    for (size_t i = 1; i < changes.size(); ++i) {
      for (auto &tk : takeoffs) {
        if (tk.uid == changes[i].uid) {
          tk.position = changes[i].new_position;
          break;
        }
      }
    }
    [self _commitPositionChanges:changes];
  } else {
    // within the 1e-9 tolerance nothing changed: no write, no history entry
    for (auto &tk : takeoffs) {
      if (tk.uid == uid) {
        tk.position = oldPos;
        break;
      }
    }
  }
  [self _refreshOverlays];
  [self _generateMeshesForSelectedPages];
  [self _updateConditionQuantities];
}

#pragma mark - UndoRedoDelegate (grouped move)

// One undo / redo step of a grouped move: stored in one transaction first; when
// that fails nothing changes and the history does not advance (UND-02).
- (BOOL)undoRedoApplyPositions:
    (const std::vector<std::pair<std::string, std::vector<double>>> &)positions {
  if (_bidUID.length > 0 &&
      ![ImportService saveTakeoffPositions:positions inBid:_bidUID])
    return NO;
  for (const auto &entry : positions) {
    for (auto &tk : _projectService.mutableProject.takeoffs) {
      if (tk.uid == entry.first) {
        tk.position = entry.second;
        break;
      }
    }
  }
  return YES;
}

- (void)planView:(PlanView *)planView
    didPlaceTakeoffWithCondition:(const std::string &)conditionUID
                        position:(const std::vector<double> &)position {
  // Windows handle_place_press (placement_mode.py:1648): a tap for a condition that is gone or not visible places nothing, no
  // takeoff, no history entry, no selection change. The Image layer toggle leaves the tool armed on a condition that is then
  // hidden (COND-PLACE-01). PlanView already asked planView:canPlaceWithCondition: at the start of the gesture, so no preview
  // was drawn; this is the last line of defence at the one place every placement (tap, drag, polygon) comes through. The
  // decision is Core ost_data::placement_tap_accepted.
  if (!ost_data::placement_tap_accepted(_projectService.project, conditionUID))
    return;
  if (_bidUID.length == 0)
    return;
  const std::string &pageUID = _projectService.selected2DPageUID;
  if (pageUID.empty())
    return;

  ost_domain::Takeoff tk;
  tk.condition_uid = conditionUID;
  tk.page_uid = pageUID;
  tk.position = position;

  // A refused insert places nothing and adds no history (UND-02)
  if (![self _addTakeoffToProject:tk]) {
    [self _presentSaveFailure];
    return;
  }
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
  // A bid that cannot be read completely or whose data is inconsistent is not
  // entered: an alert is shown instead of an empty project (DBP-M02,
  // DBP-31, BRW-16, POS-M01); the cause is only logged.
  ost_data::ProjectData proj;
  // ImportService logs the diagnostic; the alert shows the one user sentence (EXP-21).
  if (![ImportService loadProjectWithUID:bidUID project:proj error:nil]) {
    _loadErrorToPresent = [NSString
        stringWithUTF8String:ost_domain::load_failure_reason(
                                 ost_domain::LoadTarget::Bid)
                                 .c_str()];
    return;
  }
  if (!proj.empty()) {
    [self _loadProject:proj];
  }
}

// Windows file_operation_handler: "Error Loading File" / "Failed to load <bid>:
// <reason>"; the bid is left when the dialog is dismissed.
- (void)_presentLoadErrorIfNeeded {
  if (!_loadErrorToPresent || self.presentedViewController)
    return;
  NSString *reason = _loadErrorToPresent;
  _loadErrorToPresent = nil;
  const ost_domain::DialogText text = ost_domain::load_failure_text(
      ost_domain::display_name_for_bid(std::string(_bidUID.UTF8String), ""),
      std::string(reason.UTF8String));
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:[NSString stringWithUTF8String:text.title.c_str()]
                       message:[NSString stringWithUTF8String:text.message.c_str()]
                preferredStyle:UIAlertControllerStyleAlert];
  __weak ViewController *weakSelf = self;
  [alert addAction:[UIAlertAction
                       actionWithTitle:@"OK"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction *a) {
                                 [weakSelf.navigationController
                                     popViewControllerAnimated:YES];
                               }]];
  [self presentViewController:alert animated:YES completion:nil];
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
  [self _presentLoadErrorIfNeeded];
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
