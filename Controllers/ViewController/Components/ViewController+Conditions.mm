#import "../ViewController_Internal.h"
#import "../../ConditionFoldersViewController.h"
#import "../../../Services/ImportService+BidLock.h"
#import "../../../Services/ImportService+ConditionCreate.h"
#import "../../../Services/ImportService+ConditionFolders.h"
#include "../../../Core/Domain/BrowserRules.hpp"
#include "../../../Core/Domain/ConditionChoices.hpp"
#include "../../../Core/Domain/ConditionCreate.hpp"
#include "../../../Core/Domain/ConditionFolderUi.hpp"
#include "../../../Core/Domain/ConditionFormRules.hpp"

@interface ViewController (Conditions) <ConditionsViewControllerDelegate,
                                        EditConditionViewControllerDelegate,
                                        ConditionFoldersViewControllerDelegate>
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

#pragma mark - New Condition (plan 5.1)

static NSString *OSTConditionText(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

// The "+" button and the "New Condition in <folder>" item of the Conditions sheet. The bid lock guard comes first (plan 4.1): on a locked bid
// "This bid is locked." is shown and no form opens. `folderUID` "" = the top level.
- (void)conditionsViewController:(ConditionsViewController *)vc
    didRequestNewConditionInFolderUID:(NSString *)folderUID {
  if ([self _refuseIfBidLocked])
    return;
  [self _presentNewConditionInFolderUID:folderUID fromController:vc];
}

// The Edit Condition dialog in its create mode (Windows on_create_requested, condition_action_handler.py:434-781): a form sheet, a centred
// sheet on iPad (M1-19). The synthetic condition, the default layer, the metric defaults and the "Next number" line are Core's.
- (void)_presentNewConditionInFolderUID:(NSString *)folderUID
                         fromController:(UIViewController *)presenter {
  const ost_data::ProjectData &project = self.projectService.project;
  if (project.empty())
    return;
  EditConditionViewController *form = [[EditConditionViewController alloc]
      initForNewConditionInFolder:folderUID ?: @""
                     mergedLayers:project.merged_layers
                           layers:project.layers
                         cdnTypes:project.cdn_types
                           metric:project.bid.measure_base == 1
                       nextNumber:OSTConditionText(ost_domain::next_condition_number_text(project.conditions))];
  form.delegate = self;

  UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:form];
  nav.modalPresentationStyle = UIModalPresentationFormSheet;
  // An EDITED form cannot be swiped away without the "Discard this new condition?" question (M1-06): the form sets modalInPresentation while it is
  // edited and, as the delegate of this presentation, answers the attempt like its Cancel button. The delegate is weak, the nav keeps the form.
  nav.presentationController.delegate = form;
  [presenter presentViewController:nav animated:YES completion:nil];
}

// Save of the New Condition form: the writer (one transaction, one condition), then what Windows does after a create: the project is read
// again, the list reloads and the new row is selected and revealed. No placement starts (N-M1-10, the opposite of Duplicate), no mesh
// refresh (a new condition has no takeoffs), no undo entry and the undo history stays (plan 4.7, 4.11).
- (EditConditionCreateOutcome)editConditionViewController:(EditConditionViewController *)vc
                                didRequestCreateCondition:(const ost_domain::CreateConditionSpec &)spec
                                                  message:(NSString **)message {
  NSString *refusal = nil;
  NSString *newUID = [ImportService createCondition:spec inBid:self.bidUID error:&refusal];
  if (!newUID) {
    // a text of the store: the Windows text of a condition number past 2147483647, the name-length text, the limit text; the form shows it
    if (refusal.length > 0) {
      if (message)
        *message = refusal;
      return EditConditionCreateOutcomeFailed;
    }
    // no text: a locked bid is refused without one (plan 4.1): say so; any other failure is the generic "Failed to create condition."
    if ([self _refuseIfBidLocked])
      return EditConditionCreateOutcomeReported;
    return EditConditionCreateOutcomeFailed;
  }

  // The condition is in the database; the project is read again to get its row. A bid that cannot be read again keeps the project as it is
  // and the form tells the user once it has closed (Windows "Refresh Error", condition_action_handler.py:764-781).
  ost_data::ProjectData fresh;
  NSString *loadError = nil;
  if (![ImportService loadProjectWithUID:self.bidUID project:fresh error:&loadError]) {
    if (message)
      *message = OSTConditionText(ost_domain::kConditionCreatedRefreshFailedText);
    return EditConditionCreateOutcomeCreated;
  }
  const std::string key = std::string(newUID.UTF8String ?: "");
  auto created = fresh.conditions.find(key);
  if (created != fresh.conditions.end())
    self.projectService.mutableProject.conditions[key] = created->second;

  ConditionsViewController *condVC = self.conditionsVC;
  if (condVC) {
    [condVC loadConditionsFromProject:self.projectService.project];
    [condVC selectConditionWithUID:newUID animated:YES];
  }
  [self _updateConditionQuantities];
  return EditConditionCreateOutcomeCreated;
}

#pragma mark - Condition folders (plan 5.2)

// A folder write went through but the bid could not be read again (register CF-8, the safe behaviour reports what was committed): the "Refresh
// Error" title and a Core text that says the change was saved and what to do, not the "Failed to load" text, which says nothing of the write.
// The text names the action that was saved (Windows condition_action_handler.py, texts of the refresh warnings): "The folder was created|renamed|deleted"
// ("The folders were deleted" for several) or "The condition was moved", then ", but the conditions list could not be refreshed. Reopen the database ...".
- (void)_presentConditionFolderRefreshFailureForAction:(ost_domain::ConditionFolderRefreshAction)action count:(NSUInteger)count {
  [self _presentConditionAlertWithTitle:OSTConditionText(ost_domain::kFolderChangeSavedRefreshFailedTitle)
                                message:OSTConditionText(ost_domain::condition_folder_refresh_failed_text(action, (std::size_t)count))];
}

// The "Folders" button: the folder manager is pushed inside the Conditions sheet (iPhone and iPad alike, N-M1-25). Opening it writes nothing,
// so there is no lock check here; every action inside it asks conditionFoldersViewControllerMayWrite: first.
- (void)conditionsViewControllerDidRequestFolders:(ConditionsViewController *)vc {
  const ost_data::ProjectData &project = self.projectService.project;
  if (project.empty())
    return;
  // One tap, one screen: a second tap while the first push is still animating finds the folder manager already on top of the stack (the
  // stack changes when the push is issued), so it pushes nothing. Two managers would leave the lower one stale after a write.
  if ([vc.navigationController.topViewController isKindOfClass:[ConditionFoldersViewController class]])
    return;
  ConditionFoldersViewController *screen =
      [[ConditionFoldersViewController alloc] initWithProject:project
                                                         mode:ConditionFoldersModeManage
                                             pickConditionUID:nil];
  screen.delegate = self;
  [vc.navigationController pushViewController:screen animated:YES];
}

// "Move to Folder..." of the long-press sheet: the same list as a picker. The lock is asked before the picker opens, and the pick asks again.
- (void)conditionsViewController:(ConditionsViewController *)vc
      didRequestMoveConditionUID:(NSString *)conditionUID {
  if ([self _refuseIfBidLocked])
    return;
  const ost_data::ProjectData &project = self.projectService.project;
  if (project.empty() || !conditionUID)
    return;
  ConditionFoldersViewController *screen =
      [[ConditionFoldersViewController alloc] initWithProject:project
                                                         mode:ConditionFoldersModePickFolder
                                             pickConditionUID:conditionUID];
  screen.delegate = self;
  [vc.navigationController pushViewController:screen animated:YES];
}

#pragma mark - ConditionFoldersViewControllerDelegate

// Plan 4.1: the folder manager is not a ViewController category, so it reaches the one lock helper through this method. YES = go on.
- (BOOL)conditionFoldersViewControllerMayWrite:(ConditionFoldersViewController *)vc {
  return ![self _refuseIfBidLocked];
}

// After a folder write (plan 4.11): the folders (and the moved condition) are read again from the database and ONLY the Conditions list and the
// folder screen redraw: no mesh or overlay refresh and no undo clear (Windows condition_change_impact.py:27-82, invalidates_undo False).
// A bid that cannot be read again keeps the project as it is and the user is told that the change was saved (the write itself went through, CF-8).
// The selection of the main view stays as it was (plan 4.11: a folder write does not select anything), also for the condition that was moved.
- (void)_refreshConditionFoldersFromScreen:(ConditionFoldersViewController *)screen
                         movedConditionUID:(NSString *)movedConditionUID
                                    action:(ost_domain::ConditionFolderRefreshAction)action
                                     count:(NSUInteger)count {
  ost_data::ProjectData fresh;
  NSString *loadError = nil;
  if (![ImportService loadProjectWithUID:self.bidUID project:fresh error:&loadError]) {
    [self _presentConditionFolderRefreshFailureForAction:action count:count];
    return;
  }
  ost_data::ProjectData &project = self.projectService.mutableProject;
  project.condition_folders = fresh.condition_folders;
  if (movedConditionUID) {
    // only the folder of that one condition changed
    const std::string key = std::string(movedConditionUID.UTF8String ?: "");
    auto now = fresh.conditions.find(key);
    auto kept = project.conditions.find(key);
    if (now != fresh.conditions.end() && kept != project.conditions.end())
      kept->second.condition_folder_uid = now->second.condition_folder_uid;
  }
  [screen loadFoldersFromProject:project];
  ConditionsViewController *condVC = self.conditionsVC;
  if (condVC) {
    [condVC loadConditionsFromProject:project];
    if (self.selectedConditionUID)
      [condVC selectConditionWithUID:self.selectedConditionUID animated:NO];
  }
}

- (OSTConditionFolderOutcome *)conditionFoldersViewController:(ConditionFoldersViewController *)vc
                                            createFolderNamed:(NSString *)name
                                            inParentFolderUID:(NSString *)parentFolderUID {
  OSTConditionFolderOutcome *outcome = [ImportService createConditionFolderNamed:name
                                                                           inBid:self.bidUID
                                                                    parentFolder:parentFolderUID];
  if (outcome.ok)
    [self _refreshConditionFoldersFromScreen:vc
                           movedConditionUID:nil
                                      action:ost_domain::ConditionFolderRefreshAction::Create
                                       count:1];
  return outcome;
}

- (OSTConditionFolderOutcome *)conditionFoldersViewController:(ConditionFoldersViewController *)vc
                                              renameFolderUID:(NSString *)folderUID
                                                       toName:(NSString *)name {
  OSTConditionFolderOutcome *outcome = [ImportService renameConditionFolder:folderUID
                                                                     toName:name
                                                                      inBid:self.bidUID];
  if (outcome.ok)
    [self _refreshConditionFoldersFromScreen:vc
                           movedConditionUID:nil
                                      action:ost_domain::ConditionFolderRefreshAction::Rename
                                       count:1];
  return outcome;
}

- (OSTConditionFolderOutcome *)conditionFoldersViewController:(ConditionFoldersViewController *)vc
                                              deleteFolderUIDs:(NSArray<NSString *> *)folderUIDs {
  OSTConditionFolderOutcome *outcome = [ImportService deleteConditionFolders:folderUIDs inBid:self.bidUID];
  if (outcome.ok)
    [self _refreshConditionFoldersFromScreen:vc
                           movedConditionUID:nil
                                      action:ost_domain::ConditionFolderRefreshAction::Delete
                                       count:folderUIDs.count];
  return outcome;
}

- (OSTConditionFolderOutcome *)conditionFoldersViewController:(ConditionFoldersViewController *)vc
                                               moveConditionUID:(NSString *)conditionUID
                                                    toFolderUID:(NSString *)folderUID {
  OSTConditionFolderOutcome *outcome = [ImportService moveCondition:conditionUID
                                                           toFolder:folderUID
                                                              inBid:self.bidUID];
  if (outcome.ok)
    [self _refreshConditionFoldersFromScreen:vc
                           movedConditionUID:conditionUID
                                      action:ost_domain::ConditionFolderRefreshAction::Move
                                       count:1];
  return outcome;
}

@end
