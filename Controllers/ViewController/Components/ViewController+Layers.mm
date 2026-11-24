#include "../../../Core/Domain/Layer.hpp"
#import "../ViewController_Internal.h"

@interface ViewController (Layers) <UITableViewDataSource, UITableViewDelegate>
@end

@implementation ViewController (Layers)

- (void)_handleLayerToggle {
  // The layer list is the Windows one: the bid's layers merged with the locked
  // template layers, in Sequence order with stable ties, without the Comments
  // layer (Core: merge_layers_for_bid, LP-24, LP-18). The sheet also opens for
  // a bid whose list is empty: Windows allows New Layer there (after_sequence 0,
  // test_layers_sidebar.py:221-246), and "+" creates the first one (M1, 5.3).
  // Nothing opens while the main controller already presents something (a second tap on Layers, or an
  // alert): UIKit would refuse the second presentation, and the table stored below would then be an
  // orphan that _layersTableView hands to the editing flows instead of the one on screen.
  if (self.presentedViewController)
    return;
  if (self.projectService.project.empty())
    return;
  [self _rebuildSortedLayerUIDs];
  UITableView *table =
      [[UITableView alloc] initWithFrame:CGRectZero
                                   style:UITableViewStyleInsetGrouped];
  table.dataSource = self;
  table.delegate = self;
  table.translatesAutoresizingMaskIntoConstraints = NO;
  [table registerClass:[UITableViewCell class]
      forCellReuseIdentifier:@"LayerCell"];

  UIViewController *sheetVC = [[UIViewController alloc] init];
  sheetVC.title = @"Layers";
  sheetVC.view.backgroundColor = [UIColor systemGroupedBackgroundColor];
  [sheetVC.view addSubview:table];
  [NSLayoutConstraint activateConstraints:@[
    [table.topAnchor constraintEqualToAnchor:sheetVC.view.topAnchor],
    [table.bottomAnchor constraintEqualToAnchor:sheetVC.view.bottomAnchor],
    [table.leadingAnchor constraintEqualToAnchor:sheetVC.view.leadingAnchor],
    [table.trailingAnchor constraintEqualToAnchor:sheetVC.view.trailingAnchor],
  ]];
  UIBarButtonItem *showAll =
      [[UIBarButtonItem alloc] initWithTitle:@"Show All"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(_layersShowAll:)];
  UIBarButtonItem *hideAll =
      [[UIBarButtonItem alloc] initWithTitle:@"Hide All"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(_layersHideAll:)];
  sheetVC.navigationItem.leftBarButtonItems = @[ showAll, hideAll ];

  UIBarButtonItem *done = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(_layersDone:)];
  // "+" = New Layer at the end of the list (Windows New with no row selected,
  // layers_dialog.py:315-322). The first item is the rightmost.
  UIBarButtonItem *add = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
                           target:self
                           action:@selector(_layersAddTapped:)];
  sheetVC.navigationItem.rightBarButtonItems = @[ done, add ];

  UINavigationController *nav =
      [[UINavigationController alloc] initWithRootViewController:sheetVC];
  nav.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationControllerDetent *medium =
      [UISheetPresentationControllerDetent mediumDetent];
  UISheetPresentationControllerDetent *large =
      [UISheetPresentationControllerDetent largeDetent];
  nav.sheetPresentationController.detents = @[ medium, large ];
  nav.sheetPresentationController.selectedDetentIdentifier =
      UISheetPresentationControllerDetentIdentifierLarge;
  nav.sheetPresentationController.prefersGrabberVisible = YES;
  nav.sheetPresentationController.largestUndimmedDetentIdentifier =
      UISheetPresentationControllerDetentIdentifierMedium;
  objc_setAssociatedObject(self, "layerTable", table,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);

  [self presentViewController:nav animated:YES completion:nil];
}

- (void)_layersShowAll:(id)sender {
  [self _setAllLayersVisible:YES];
}

- (void)_layersHideAll:(id)sender {
  [self _setAllLayersVisible:NO];
}

// Windows update_all_layers_visibility_deferred: the layers of the bid in the
// list that do not have the requested visibility are written in ONE transaction
// (all or none, DBP-35); nothing is written when none differs; the Comments
// layer is not in the list and never changes (LP-18); a shared template row (a
// locked template layer of no bid or of another bid) is never changed, in the
// write, in memory or in the list (LP-24 (a); the plan is
// ost_domain::plan_bulk_layer_visibility). When the bid has no layer of its own
// the plan is NoOwnLayers: Windows returns False there and the sidebar path
// ignores it, so nothing is written, nothing changes and no alert is shown. The
// project follows only after the write worked, a refused write is reported and
// changes nothing (DBP-11, DBP-17, Windows restore_authoritative).
- (void)_setAllLayersVisible:(BOOL)show {
  const auto &project = self.projectService.project;
  const ost_domain::BulkLayerPlan plan = ost_domain::plan_bulk_layer_visibility(
      project.merged_layers, std::string(self.bidUID.UTF8String ?: ""),
      project.layers, show);
  if (plan.outcome != ost_domain::BulkLayerOutcome::Write)
    return;
  if (self.bidUID.length > 0 &&
      ![ImportService setLayersVisibility:show
                                forLayers:plan.uids
                                    inBid:self.bidUID]) {
    [self _presentLayerVisibilityFailure];
    return;
  }
  // Windows update_all_layers_visibility_deferred: Hide All leaves the placement tool before the layers change, Show All puts it
  // back at the end (COND-PLACE-01). Unlike the single toggle there is no Image layer exemption here (no layer is named), also
  // for a condition on the Image layer.
  if (!show)
    [self _suspendPlacementForLayerHide:nil layerIsImage:NO];
  self.projectService.mutableProject.apply_layer_visibility(plan.uids, show);
  [self _generateMeshesForSelectedPages];
  [self _refreshOverlays];
  UITableView *table = objc_getAssociatedObject(self, "layerTable");
  [table reloadData];
  [self _refreshConditionsListForLayerVisibility];
  if (show)
    [self _restorePlacementAfterLayerShow:nil layerIsImage:NO];
}

#pragma mark - Conditions on a hidden layer (COND-PLACE-01)

// The Conditions list greys the rows of hidden layers when it loads, so it loads again after a layer was shown or hidden
// (Windows _refresh_conditions_sidebar_layer_visibility_from_memory).
- (void)_refreshConditionsListForLayerVisibility {
  ConditionsViewController *condVC = self.conditionsVC;
  if (!condVC)
    return;
  [condVC loadConditionsFromProject:self.projectService.project];
  if (self.selectedConditionUID)
    [condVC selectConditionWithUID:self.selectedConditionUID animated:NO];
}

// Windows _suspend_active_layer_tool: when the layer of the condition that is being placed is hidden (`layerUID` nil = every layer,
// Hide All) the placement tool is left and remembered, to come back when the layer is shown again. `layerIsImage` is the single
// toggle of the Image layer (Core is_image_layer_uid): Windows does not suspend for it (`if not show and not image_layer`), so the
// tool stays armed on a condition that is now hidden; its presses are ignored from the first touch (Core
// ost_data::placement_tap_accepted, asked by PlanView through planView:canPlaceWithCondition:, by the floating mode button and
// in planView:didPlaceTakeoffWithCondition:position:) and place again when the Image layer is shown. Hide All names no layer and
// passes NO.
- (void)_suspendPlacementForLayerHide:(NSString *)layerUID
                         layerIsImage:(BOOL)layerIsImage {
  if (self.editMode != ost_domain::EditMode::PlaceTakeoff ||
      self.placeConditionUID.empty())
    return;
  const ost_data::ProjectData &project = self.projectService.project;
  auto placing = project.conditions.find(self.placeConditionUID);
  if (placing == project.conditions.end())
    return;

  std::optional<std::string> hiddenLayer;
  if (layerUID)
    hiddenLayer = std::string(layerUID.UTF8String);
  std::optional<ost_domain::SuspendedPlacement> suspended =
      ost_domain::suspended_placement_for_layer_hide(
          std::string(self.bidUID.UTF8String ?: ""), placing->second,
          hiddenLayer, layerIsImage);
  if (!suspended)
    return;

  [self _leavePlacement];
  // taken after the tool was left, as Windows does
  suspended->tool_revision = self.toolRevision;
  self.suspendedPlacement = suspended;
}

// Windows _restore_suspended_layer_tool: `layerUID` is the layer that was shown (nil = Show All). `layerIsImage` is the single
// toggle of the Image layer: Windows does not restore for it (`if show and not image_layer`), an existing suspension stays.
- (void)_restorePlacementAfterLayerShow:(NSString *)layerUID
                           layerIsImage:(BOOL)layerIsImage {
  if (!self.suspendedPlacement)
    return;
  const ost_domain::SuspendedPlacement suspended = *self.suspendedPlacement;
  const ost_data::ProjectData &project = self.projectService.project;

  std::optional<std::string> shownLayer;
  if (layerUID)
    shownLayer = std::string(layerUID.UTF8String);

  ost_domain::PlacementRestoreState now;
  now.bid_uid = std::string(self.bidUID.UTF8String ?: "");
  // Windows reads "cursor mode is select"; after leaving the placement tool the phone is in the view mode
  now.tool_in_view_mode = (self.editMode == ost_domain::EditMode::View);
  now.tool_revision = self.toolRevision;
  auto found = project.conditions.find(suspended.condition_uid);
  if (found != project.conditions.end()) {
    now.condition = &found->second;
    now.condition_visible = project.is_condition_visible(found->second);
  }
  now.takeoff_2d_view_active = self.is2DMode;

  switch (ost_domain::restore_of_suspended_placement(suspended, shownLayer, now,
                                                     layerIsImage)) {
  case ost_domain::SuspendedPlacementOutcome::Keep:
    return;
  case ost_domain::SuspendedPlacementOutcome::Drop:
    self.suspendedPlacement.reset();
    return;
  case ost_domain::SuspendedPlacementOutcome::Restore:
    self.suspendedPlacement.reset();
    [self _selectConditionForPlacement:
              [NSString stringWithUTF8String:suspended.condition_uid.c_str()]
                                action:ost_domain::PlacementAction::Enter];
    return;
  }
}

// Windows layers_dialog._on_show_changed: "Layer Visibility" / "Failed to update
// layer visibility."; the check marks are put back to what the project holds.
- (void)_presentLayerVisibilityFailure {
  UITableView *table = objc_getAssociatedObject(self, "layerTable");
  [table reloadData];
  UIViewController *presenter = self;
  while (presenter.presentedViewController)
    presenter = presenter.presentedViewController;
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:@"Layer Visibility"
                       message:@"Failed to update layer visibility."
                preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [presenter presentViewController:alert animated:YES completion:nil];
}

// What the list shows for a layer (ost_data::ProjectData::is_listed_layer_visible):
// the live visibility of a layer of the bid (the toggles and Show All / Hide All
// made since the load), the stored Show of a shared template row (a locked
// template layer of no bid or of another bid: Show All / Hide All never change
// it), so the sheet equals a reload after a bulk change.
- (BOOL)_visibilityOfListedLayer:(const std::string &)uid {
  return self.projectService.project.is_listed_layer_visible(uid) ? YES : NO;
}

- (NSString *)_nameOfListedLayer:(const std::string &)uid {
  for (const auto &entry : self.projectService.project.merged_layers) {
    if (entry.uid == uid)
      return [NSString stringWithUTF8String:entry.name.c_str()];
  }
  return @"";
}

- (void)_layersDone:(id)sender {
  [self dismissViewControllerAnimated:YES completion:nil];
}

// The table of the Layers sheet that was opened last (nil when it was never opened; after Done it stays, and reloading it is harmless):
// ViewController+LayerEditing.mm reloads it after a write. The associated object is set in _handleLayerToggle, in this file only, so the key
// is the same literal. _handleLayerToggle does not run while something is presented, so this is always the sheet on screen when one is up.
- (UITableView *)_layersTableView {
  return objc_getAssociatedObject(self, "layerTable");
}

#pragma mark - UITableView DataSource/Delegate (Layers)

- (NSInteger)tableView:(UITableView *)tableView
    numberOfRowsInSection:(NSInteger)section {
  return (NSInteger)self.projectService.sortedLayerUIDs.size();
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell =
      [tableView dequeueReusableCellWithIdentifier:@"LayerCell"
                                      forIndexPath:indexPath];
  if ((size_t)indexPath.row < self.projectService.sortedLayerUIDs.size()) {
    const std::string &layerUID =
        self.projectService.sortedLayerUIDs[indexPath.row];
    const BOOL visible = [self _visibilityOfListedLayer:layerUID];
    cell.textLabel.text = [self _nameOfListedLayer:layerUID];
    cell.accessoryType = visible ? UITableViewCellAccessoryCheckmark
                                 : UITableViewCellAccessoryNone;
    cell.textLabel.textColor =
        visible ? [UIColor labelColor] : [UIColor tertiaryLabelColor];
  }
  return cell;
}

- (void)tableView:(UITableView *)tableView
    didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  if ((size_t)indexPath.row < self.projectService.sortedLayerUIDs.size()) {
    const std::string layerUID =
        self.projectService.sortedLayerUIDs[indexPath.row];
    const BOOL show = ![self _visibilityOfListedLayer:layerUID];
    // Stored first (Windows layers_dialog._on_show_changed): a refused write
    // (a layer that is no layer of a bid, a failed statement) changes nothing
    // and is reported (DBP-11, DBP-17).
    if (self.bidUID.length > 0 &&
        ![ImportService
            setLayerVisibility:show
                      forLayer:[NSString stringWithUTF8String:layerUID.c_str()]
                         inBid:self.bidUID]) {
      [self _presentLayerVisibilityFailure];
      return;
    }
    NSString *layerUIDString = [NSString stringWithUTF8String:layerUID.c_str()];
    // Windows update_layer_visibility_deferred (image_layer = is_image_layer_uid, COND-PLACE-01): hiding the layer of the
    // condition that is being placed leaves the placement tool, and showing it puts it back, except for the Image layer, which
    // neither suspends nor restores (the flag is worked out once and goes to both helpers)
    const ost_data::ProjectData &listed = self.projectService.project;
    const BOOL layerIsImage = ost_domain::is_image_layer_uid(
                                  listed.merged_layers, listed.layers, layerUID)
                                  ? YES
                                  : NO;
    if (!show)
      [self _suspendPlacementForLayerHide:layerUIDString
                             layerIsImage:layerIsImage];
    // Only a layer of the bid has a state to change; this never adds a layer to
    // the project (a template layer of no bid is refused above).
    auto it = self.projectService.mutableProject.layers.find(layerUID);
    if (it != self.projectService.mutableProject.layers.end())
      it->second.visible = show;
    [tableView reloadRowsAtIndexPaths:@[ indexPath ]
                     withRowAnimation:UITableViewRowAnimationNone];
    [self _generateMeshesForSelectedPages];
    [self _refreshOverlays];
    [self _refreshConditionsListForLayerVisibility];
    // ... and showing that layer again puts it back, last, after the views follow
    if (show)
      [self _restorePlacementAfterLayerShow:layerUIDString
                               layerIsImage:layerIsImage];
  }
}

// M1 layer editing (plan 5.3): a tap still toggles the visibility (above); the
// per-row actions are the trailing swipe (Delete, Rename) and the long-press
// menu (Rename, New Layer Below, Move Up, Move Down, Delete), both built by
// ViewController+LayerEditing.mm from Core's row rules. A row that offers
// nothing returns nil.
- (UISwipeActionsConfiguration *)tableView:(UITableView *)tableView
    trailingSwipeActionsConfigurationForRowAtIndexPath:(NSIndexPath *)indexPath {
  if (indexPath.row < 0 ||
      (size_t)indexPath.row >= self.projectService.sortedLayerUIDs.size())
    return nil;
  NSString *layerUID = [NSString
      stringWithUTF8String:self.projectService.sortedLayerUIDs[indexPath.row]
                               .c_str()];
  return [self _layerSwipeActionsForUID:layerUID];
}

- (UIContextMenuConfiguration *)tableView:(UITableView *)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath *)indexPath
                                        point:(CGPoint)point {
  if (indexPath.row < 0 ||
      (size_t)indexPath.row >= self.projectService.sortedLayerUIDs.size())
    return nil;
  NSString *layerUID = [NSString
      stringWithUTF8String:self.projectService.sortedLayerUIDs[indexPath.row]
                               .c_str()];
  return [self _layerContextMenuForUID:layerUID];
}

@end
