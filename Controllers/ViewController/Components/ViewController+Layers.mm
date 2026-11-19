#include "../../../Core/Domain/Layer.hpp"
#import "../ViewController_Internal.h"

@interface ViewController (Layers) <UITableViewDataSource, UITableViewDelegate>
@end

@implementation ViewController (Layers)

- (void)_handleLayerToggle {
  // The layer list is the Windows one: the bid's layers merged with the locked
  // template layers, in Sequence order with stable ties, without the Comments
  // layer (Core: merge_layers_for_bid, LP-24, LP-18).
  if (self.projectService.project.empty() ||
      self.projectService.project.merged_layers.empty())
    return;
  self.projectService.sortedLayerUIDs.clear();
  for (const auto &layer : self.projectService.project.merged_layers) {
    self.projectService.sortedLayerUIDs.push_back(layer.uid);
  }
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
  sheetVC.navigationItem.rightBarButtonItem = done;

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

// Windows update_all_layers_visibility_deferred: the layers of the list that do
// not have the requested visibility are written in ONE transaction (all or
// none, DBP-35); nothing is written when none differs; the Comments layer is
// not in the list and never changes (LP-18). The project follows only after the
// write worked, a refused write is reported and changes nothing (DBP-11,
// DBP-17, Windows restore_authoritative).
- (void)_setAllLayersVisible:(BOOL)show {
  const auto &project = self.projectService.project;
  const std::vector<std::string> changed =
      ost_domain::layers_to_change_for_bulk_visibility(project.merged_layers,
                                                       project.layers, show);
  if (changed.empty())
    return;
  if (self.bidUID.length > 0 &&
      ![ImportService setLayersVisibility:show
                                forLayers:changed
                                    inBid:self.bidUID]) {
    [self _presentLayerVisibilityFailure];
    return;
  }
  // Windows update_all_layers_visibility_deferred: Hide All leaves the placement tool before the layers change, Show All puts it
  // back at the end (COND-PLACE-01)
  if (!show)
    [self _suspendPlacementForLayerHide:nil];
  for (const auto &uid : changed) {
    auto it = self.projectService.mutableProject.layers.find(uid);
    if (it != self.projectService.mutableProject.layers.end())
      it->second.visible = show;
  }
  [self _generateMeshesForSelectedPages];
  [self _refreshOverlays];
  UITableView *table = objc_getAssociatedObject(self, "layerTable");
  [table reloadData];
  [self _refreshConditionsListForLayerVisibility];
  if (show)
    [self _restorePlacementAfterLayerShow:nil];
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
// Hide All) the placement tool is left and remembered, to come back when the layer is shown again. Windows leaves the
// placement of an Image layer condition running (`not image_layer`), here a hidden condition is never placed.
- (void)_suspendPlacementForLayerHide:(NSString *)layerUID {
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
          hiddenLayer);
  if (!suspended)
    return;

  [self _leavePlacement];
  // taken after the tool was left, as Windows does
  suspended->tool_revision = self.toolRevision;
  self.suspendedPlacement = suspended;
}

// Windows _restore_suspended_layer_tool: `layerUID` is the layer that was shown (nil = Show All).
- (void)_restorePlacementAfterLayerShow:(NSString *)layerUID {
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

  switch (ost_domain::restore_of_suspended_placement(suspended, shownLayer,
                                                     now)) {
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

// What the list shows for a layer: the live visibility of a layer of the bid
// (the toggles made since the load), the stored Show of a template layer that
// belongs to no bid (it cannot be changed from here).
- (BOOL)_visibilityOfListedLayer:(const std::string &)uid {
  const auto &project = self.projectService.project;
  auto own = project.layers.find(uid);
  if (own != project.layers.end())
    return own->second.visible;
  for (const auto &entry : project.merged_layers) {
    if (entry.uid == uid)
      return entry.show;
  }
  return YES;
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
    // Windows update_layer_visibility_deferred: hiding the layer of the condition that is being placed leaves the placement tool
    // (COND-PLACE-01)
    if (!show)
      [self _suspendPlacementForLayerHide:layerUIDString];
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
      [self _restorePlacementAfterLayerShow:layerUIDString];
  }
}

@end
