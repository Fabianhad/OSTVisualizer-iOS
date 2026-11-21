#import "../ViewController_Internal.h"
#include "../../../Core/Domain/BrowserRules.hpp"
#include "../../../Core/Domain/ConditionChoices.hpp"
#include "../../../Core/Domain/ConditionFormRules.hpp"

@interface ViewController (Conditions) <ConditionsViewControllerDelegate,
                                        EditConditionViewControllerDelegate>
@end

@implementation ViewController (Conditions)

- (void)_handleConditionsToggle {
  if (self.projectService.project.empty())
    return;

  ConditionsViewController *condVC = [[ConditionsViewController alloc] init];
  condVC.delegate = self;
  [condVC loadConditionsFromProject:self.projectService.project];
  [self setConditionsVC:condVC];

  if (self.selectedConditionUID) {
    [condVC selectConditionWithUID:self.selectedConditionUID animated:NO];
  }

  [self _updateConditionQuantities];

  UINavigationController *nav =
      [[UINavigationController alloc] initWithRootViewController:condVC];
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

  [self presentViewController:nav animated:YES completion:nil];
}

#pragma mark - ConditionsViewControllerDelegate

- (void)conditionsViewController:(ConditionsViewController *)vc
           didSelectConditionUID:(NSString *)conditionUID {
  if (!conditionUID)
    return;
  std::string uid = std::string(conditionUID.UTF8String);

  auto it = self.projectService.project.conditions.find(uid);
  if (it == self.projectService.project.conditions.end())
    return;

  // A condition on a hidden layer is not placeable: selecting it leaves the placement tool instead of entering it (COND-PLACE-01)
  const bool placeable =
      self.projectService.project.is_condition_visible(it->second);
  [self _selectConditionForPlacement:conditionUID
                              action:ost_domain::placement_for_selected_condition(
                                         placeable)];
}

// The Conditions list selected a condition (Windows PlacementCoordinator.enter, force_exit for a condition that cannot be placed).
- (void)_selectConditionForPlacement:(NSString *)conditionUID
                              action:(ost_domain::PlacementAction)action {
  std::string uid = std::string(conditionUID.UTF8String);

  self.selectedTakeoffUID.clear();
  self.activeHandleIndex = -1;

  if (action == ost_domain::PlacementAction::Enter) {
    self.placeConditionUID = uid;
    self.fab.placeModeAvailable = YES;
  } else {
    self.placeConditionUID.clear();
    self.fab.placeModeAvailable = NO;
  }

  [self setSelectedConditionUID:conditionUID];
  [self _syncSelection];

  if (action == ost_domain::PlacementAction::Enter) {
    [self _setEditMode:ost_domain::EditMode::PlaceTakeoff];
  } else if (self.editMode == ost_domain::EditMode::PlaceTakeoff) {
    [self _setEditMode:ost_domain::EditMode::View];
  }
}

// PlacementCoordinator.force_exit: the placement tool is left and the app is back in the view / select mode.
- (void)_leavePlacement {
  self.placeConditionUID.clear();
  self.fab.placeModeAvailable = NO;
  if (self.editMode == ost_domain::EditMode::PlaceTakeoff)
    [self _setEditMode:ost_domain::EditMode::View];
}

// An action of the Conditions list that did not go through. Windows only logs most of these; the phone has to say it.
- (void)_presentConditionAlertWithTitle:(NSString *)title
                                message:(NSString *)message {
  UIViewController *presenter = self;
  while (presenter.presentedViewController)
    presenter = presenter.presentedViewController;
  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [presenter presentViewController:alert animated:YES completion:nil];
}

- (void)conditionsViewController:(ConditionsViewController *)vc
      didRequestEditConditionUID:(NSString *)conditionUID {
  [self _presentEditConditionWithUID:conditionUID fromController:vc];
}

- (void)_presentEditConditionWithUID:(NSString *)conditionUID
                      fromController:(UIViewController *)presenter {
  if (!conditionUID)
    return;
  std::string uid = std::string(conditionUID.UTF8String);

  const ost_data::ProjectData &project = self.projectService.project;
  auto it = project.conditions.find(uid);
  if (it == project.conditions.end())
    return;

  // A condition on a hidden layer cannot be edited (Windows disables Edit for it, COND-PLACE-01)
  if (!project.is_condition_visible(it->second))
    return;

  // Edit Previous / Next walk the conditions in the order of the list (COND-NAV-01)
  std::vector<std::string> orderedUIDs = ost_domain::condition_uids_in_list_order(
      project.conditions, project.condition_folders, project.cdn_types);

  // The dialog reads dimensions as millimetres in a metric bid (COND-METRIC-01)
  EditConditionViewController *editVC = [[EditConditionViewController alloc]
      initWithCondition:it->second
          conditionUIDs:orderedUIDs
          conditionsMap:project.conditions
               takeoffs:project.takeoffs
                 layers:project.layers
           mergedLayers:project.merged_layers
               cdnTypes:project.cdn_types
                 metric:project.bid.measure_base == 1];
  editVC.delegate = self;

  UINavigationController *nav =
      [[UINavigationController alloc] initWithRootViewController:editVC];
  nav.modalPresentationStyle = UIModalPresentationFormSheet;

  [presenter presentViewController:nav animated:YES completion:nil];
}

#pragma mark - EditConditionViewControllerDelegate

- (BOOL)editConditionViewController:(EditConditionViewController *)vc
                   didSaveCondition:(const ost_domain::Condition &)condition
                            changes:(const ost_domain::ConditionChanges &)changes
                     failureMessage:(NSString **)failureMessage {
  // The write comes first and writes only the changed fields, together with the shift of the neighbours' numbers when the number
  // is taken (COND-UPD-02). When it fails nothing in memory changes and the dialog says so (COND-UPD-03), with the store's text when
  // it gave one: a renumber that would push another condition past the highest condition number (DLG-FINITE-01).
  NSString *conditionUID = [NSString stringWithUTF8String:condition.uid.c_str()];
  if (![ImportService updateConditionWithUID:conditionUID
                                       inBid:self.bidUID
                                     changes:changes
                                       error:failureMessage])
    return NO;

  ost_data::ProjectData &project = self.projectService.mutableProject;
  project.shift_conflicting_ref_nos(condition.uid, condition.ref_no);
  project.conditions[condition.uid] = condition;

  [self _generateMeshesForSelectedPages];
  [self _refreshOverlays];

  ConditionsViewController *condVC = self.conditionsVC;
  if (condVC) {
    [condVC loadConditionsFromProject:self.projectService.project];
  }
  [self _updateConditionQuantities];
  return YES;
}

- (void)editConditionViewController:(EditConditionViewController *)vc
             didNavigateToCondition:(NSString *)conditionUID {
  ConditionsViewController *condVC = self.conditionsVC;
  if (condVC && conditionUID) {
    [condVC selectConditionWithUID:conditionUID animated:YES];
  }
}

#pragma mark - Condition Duplicate / Delete

- (void)conditionsViewController:(ConditionsViewController *)vc
    didRequestDuplicateConditionUID:(NSString *)conditionUID {
  NSString *refusal = nil;
  NSString *newUID = [ImportService duplicateCondition:conditionUID
                                                 inBid:self.bidUID
                                                 error:&refusal];
  if (!newUID) {
    // The existing "Duplicate Error" alert: the Windows text when the copy would get a number past the highest condition number,
    // 2147483647 (COND-ALLOC-01), the generic text for every other failure (Core decides: condition_duplicate_failed_issue). Nothing was
    // written either way, and the method returns before the project or the list are touched.
    const ost_domain::FormIssue issue = ost_domain::condition_duplicate_failed_issue(
        std::string(refusal.length > 0 ? refusal.UTF8String : ""));
    [self _presentConditionAlertWithTitle:[NSString stringWithUTF8String:issue.title.c_str()]
                                  message:[NSString stringWithUTF8String:issue.message.c_str()]];
    return;
  }

  // The duplicate is in the database; the project is read again to get its row. A bid that cannot be read again is left as it is
  // and the cause is shown, the project is never replaced by an empty one (DBP-31, BRW-16, POS-M01, DBP-M02).
  ost_data::ProjectData fresh;
  NSString *loadError = nil;
  if (![ImportService loadProjectWithUID:self.bidUID
                                 project:fresh
                                   error:&loadError]) {
    const ost_domain::DialogText text = ost_domain::load_failure_text(
        ost_domain::display_name_for_bid(
            std::string(self.bidUID.UTF8String),
            self.projectService.project.bid_name),
        std::string((loadError.length > 0 ? loadError
                                          : @"The bid could not be read.")
                        .UTF8String));
    [self _presentConditionAlertWithTitle:[NSString
                                              stringWithUTF8String:text.title
                                                                       .c_str()]
                                  message:[NSString
                                              stringWithUTF8String:text.message
                                                                       .c_str()]];
    return;
  }
  std::string newKey = std::string([newUID UTF8String]);
  auto it = fresh.conditions.find(newKey);
  if (it != fresh.conditions.end()) {
    self.projectService.mutableProject.conditions[newKey] = it->second;
  }

  [self _generateMeshesForSelectedPages];
  [self _refreshOverlays];

  ConditionsViewController *condVC = self.conditionsVC;
  if (condVC) {
    [condVC loadConditionsFromProject:self.projectService.project];
    [condVC selectConditionWithUID:newUID animated:YES];
  }
  [self _updateConditionQuantities];

  // Windows _finish_condition_duplicate (COND-M07): in the 2D takeoff view placement is entered for the new condition; outside it the
  // current placement is kept and only the list selection moved
  const auto &project = self.projectService.project;
  auto duplicate = project.conditions.find(newKey);
  const bool placeable = duplicate != project.conditions.end() &&
                         project.is_condition_visible(duplicate->second);
  switch (ost_domain::placement_after_duplicate(self.is2DMode, placeable)) {
  case ost_domain::PlacementAction::Enter:
    [self _selectConditionForPlacement:newUID
                                action:ost_domain::PlacementAction::Enter];
    break;
  case ost_domain::PlacementAction::Exit:
    [self _leavePlacement];
    break;
  case ost_domain::PlacementAction::Keep:
    break;
  }
}

- (void)conditionsViewController:(ConditionsViewController *)vc
    didRequestDeleteConditionUID:(NSString *)conditionUID {
  // The delete is all-or-nothing (COND-DEL-01): a refusal or a failed statement leaves the database as it was, and so the project
  if (![ImportService deleteCondition:conditionUID inBid:self.bidUID]) {
    [self _presentConditionAlertWithTitle:@"Delete Error"
                                  message:@"Failed to delete the condition."];
    return;
  }

  // Deleting a condition invalidates the whole undo / redo history: an undo of
  // an earlier delete or a redo of a place would bring back takeoffs of the
  // condition that is gone (Windows delete_conditions invalidates_undo, UND-03).
  [self _clearUndoHistory];

  std::string uidStr = std::string([conditionUID UTF8String]);

  // The condition and its takeoffs go, the survivors of the removed takeoffs become root takeoffs: the database after the delete
  self.projectService.mutableProject.remove_condition(uidStr);

  // Windows: placement.force_exit() and ensure_select_mode() after every successful delete, whichever condition was deleted
  // (COND-M07)
  const BOOL wasPlacing =
      (self.editMode == ost_domain::EditMode::PlaceTakeoff);
  const BOOL deletedWasPlaced = [conditionUID
      isEqualToString:[NSString stringWithUTF8String:self.placeConditionUID
                                                         .c_str()]];
  if (deletedWasPlaced) {
    self.selectedTakeoffUID.clear();
    self.activeHandleIndex = -1;
  }
  if (wasPlacing || deletedWasPlaced)
    [self _leavePlacement];

  [self _generateMeshesForSelectedPages];
  [self _refreshOverlays];

  ConditionsViewController *condVC = self.conditionsVC;
  if (condVC) {
    [condVC loadConditionsFromProject:self.projectService.project];
  }
  [self _updateConditionQuantities];
}

@end
