#import "../ViewController_Internal.h"

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
  if (@available(iOS 15.0, *)) {
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
  }

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

  self.selectedTakeoffUID.clear();
  self.activeHandleIndex = -1;

  self.placeConditionUID = uid;
  self.fab.placeModeAvailable = YES;

  [self setSelectedConditionUID:conditionUID];
  [self _syncSelection];

  [self _setEditMode:ost_domain::EditMode::PlaceTakeoff];
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

  auto it = self.projectService.project.conditions.find(uid);
  if (it == self.projectService.project.conditions.end())
    return;

  std::vector<std::string> orderedUIDs;
  for (const auto &pair : self.projectService.project.conditions) {
    orderedUIDs.push_back(pair.first);
  }
  const auto &conditions = self.projectService.project.conditions;
  std::sort(orderedUIDs.begin(), orderedUIDs.end(),
            [&conditions](const std::string &a, const std::string &b) {
              return conditions.at(a).name < conditions.at(b).name;
            });

  EditConditionViewController *editVC = [[EditConditionViewController alloc]
      initWithCondition:it->second
          conditionUIDs:orderedUIDs
          conditionsMap:self.projectService.project.conditions
               takeoffs:self.projectService.project.takeoffs
                 layers:self.projectService.project.layers
               cdnTypes:self.projectService.project.cdn_types];
  editVC.delegate = self;

  UINavigationController *nav =
      [[UINavigationController alloc] initWithRootViewController:editVC];
  nav.modalPresentationStyle = UIModalPresentationFormSheet;

  [presenter presentViewController:nav animated:YES completion:nil];
}

#pragma mark - EditConditionViewControllerDelegate

- (void)editConditionViewController:(EditConditionViewController *)vc
                   didSaveCondition:(const ost_domain::Condition &)condition {
  auto &conditions = self.projectService.mutableProject.conditions;
  auto currentIt = conditions.find(condition.uid);
  int oldRefNo = (currentIt != conditions.end()) ? currentIt->second.ref_no : 0;
  if (condition.ref_no != oldRefNo) {
    bool hasConflict = false;
    for (const auto &pair : conditions) {
      if (pair.first != condition.uid &&
          pair.second.ref_no == condition.ref_no) {
        hasConflict = true;
        break;
      }
    }
    if (hasConflict) {
      NSString *conditionUID =
          [NSString stringWithUTF8String:condition.uid.c_str()];
      if ([ImportService shiftConditionRefNosFrom:condition.ref_no
                               excludingCondition:conditionUID
                                            inBid:self.bidUID]) {
        for (auto &pair : conditions) {
          if (pair.first != condition.uid &&
              pair.second.ref_no >= condition.ref_no) {
            pair.second.ref_no += 1;
          }
        }
      }
    }
  }

  conditions[condition.uid] = condition;

  [ImportService updateCondition:condition];

  [self _generateMeshesForSelectedPages];
  [self _refreshOverlays];

  ConditionsViewController *condVC = self.conditionsVC;
  if (condVC) {
    [condVC loadConditionsFromProject:self.projectService.project];
  }
  [self _updateConditionQuantities];
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
  NSString *newUID = [ImportService duplicateCondition:conditionUID
                                                 inBid:self.bidUID];
  if (!newUID)
    return;

  ost_data::ProjectData fresh = [ImportService loadProjectWithUID:self.bidUID];
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
}

- (void)conditionsViewController:(ConditionsViewController *)vc
    didRequestDeleteConditionUID:(NSString *)conditionUID {
  if (![ImportService deleteCondition:conditionUID inBid:self.bidUID])
    return;

  std::string uidStr = std::string([conditionUID UTF8String]);

  self.projectService.mutableProject.conditions.erase(uidStr);

  auto &takeoffs = self.projectService.mutableProject.takeoffs;
  takeoffs.erase(std::remove_if(takeoffs.begin(), takeoffs.end(),
                                [&uidStr](const ost_domain::Takeoff &t) {
                                  return t.condition_uid == uidStr;
                                }),
                 takeoffs.end());

  if ([conditionUID
          isEqualToString:[NSString stringWithUTF8String:self.placeConditionUID
                                                             .c_str()]]) {
    self.placeConditionUID.clear();
    self.selectedTakeoffUID.clear();
    self.activeHandleIndex = -1;
    self.fab.placeModeAvailable = NO;
    [self _setEditMode:ost_domain::EditMode::View];
  }

  [self _generateMeshesForSelectedPages];
  [self _refreshOverlays];

  ConditionsViewController *condVC = self.conditionsVC;
  if (condVC) {
    [condVC loadConditionsFromProject:self.projectService.project];
  }
  [self _updateConditionQuantities];
}

@end
