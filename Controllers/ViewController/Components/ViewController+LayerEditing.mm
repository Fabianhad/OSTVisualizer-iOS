// Layer: Controllers (Objective-C++)
// The editing half of the Layers sheet (M1, plan 5.3): New Layer, Rename, New Layer Below, Move Up / Down and Delete, reached from the "+" of the
// sheet's navigation bar, the trailing swipe and the long-press menu of a row (ViewController+Layers.mm owns the sheet, the visibility and the
// table; it calls the builders below). This file only converts types, shows alerts and menus and calls Core: every rule is Core's.
//   - what a name may be, where a new layer goes, which rows can move, what a delete asks and says: Core/Domain/LayerEdit.hpp
//   - what a row offers, the failure and refresh texts: Core/Domain/LayerSheet.hpp
//   - what a rename means for what is drawn: Core/Domain/LayerChangeImpact.hpp
//   - the writes, the usage read and the layers-only reload: Services/ImportService+Layers.h (ost_store, Core/Persistence)
// Every write action starts with [self _refuseIfBidLocked] (plan 4.1, N-M1-02: S0's helper shows "This bid is locked." on the topmost controller,
// so the alert appears over the Layers sheet): a refusal returns before any form, alert or menu action runs. The store checks the lock again
// inside its transaction. There is no action sheet and no popover here (plan 4.3): names are asked with an alert that has a text field, the
// per-row actions are a UIMenu (context menu: system-anchored on iPhone and iPad) and swipe actions, so nothing needs an anchor.
// Windows source of truth for texts and rules (49a2d59): presentation/dialogs/layers_dialog.py, presentation/components/layers_sidebar.py,
// presentation/utils/messagebox.py:78-119, presentation/coordinators/ui_event_coordinator.py:6541-6918; see LayerEdit.hpp / LayerSheet.hpp.
#import "../ViewController_Internal.h"
#import "../../../Services/ImportService+Layers.h"
#include "../../../Core/Domain/BrowserRules.hpp"
#include "../../../Core/Domain/Layer.hpp"
#include "../../../Core/Domain/LayerChangeImpact.hpp"
#include "../../../Core/Domain/LayerEdit.hpp"
#include "../../../Core/Domain/LayerSheet.hpp"
#include <set>
#include <string>
#include <vector>

namespace {

// The std::string of a text by its BYTE length, not up to the first NUL: a pasted U+0000 reaches Core, whose name check refuses it, instead of
// being cut off silently (the cut text would be accepted and stored). nil, or text that cannot be encoded, is the empty string (a blank name).
std::string OSTLayerStd(NSString *text) {
  if (!text)
    return std::string();
  const NSUInteger byteCount = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
  const char *utf8 = text.UTF8String;
  return (utf8 && byteCount > 0) ? std::string(utf8, byteCount) : std::string();
}

NSString *OSTLayerNS(const std::string &text) {
  NSString *converted = [NSString stringWithUTF8String:text.c_str()];
  return converted ? converted : @"";
}

// An alert on the topmost presented controller: the Layers sheet is presented over the main controller, and an alert presented on a controller
// that already presents something is dropped.
UIViewController *OSTLayerPresenter(UIViewController *root) {
  UIViewController *presenter = root;
  while (presenter.presentedViewController)
    presenter = presenter.presentedViewController;
  return presenter;
}

void OSTLayerPresentAlert(UIViewController *root, NSString *title, NSString *message) {
  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [OSTLayerPresenter(root) presentViewController:alert animated:YES completion:nil];
}

void OSTLayerPresentDialog(UIViewController *root, const ost_domain::DialogText &text) {
  OSTLayerPresentAlert(root, OSTLayerNS(text.title), OSTLayerNS(text.message));
}

const ost_domain::BidLayer *OSTListedLayer(const std::vector<ost_domain::BidLayer> &listed, const std::string &uid) {
  for (const ost_domain::BidLayer &entry : listed) {
    if (entry.uid == uid)
      return &entry;
  }
  return nullptr;
}

// The one text field alert of New Layer and Rename: one field, Cancel, and the action, which stays disabled while the typed text is blank
// (`isBlank`, Core's check). `onAction` gets the text as typed; the rules run there, on the project as it is when the button is pressed. Both
// blocks capture only a weak self (the caller makes them), so the alert, its actions and the field's action form no cycle: the action the
// field's block touches is captured weakly.
void OSTLayerPresentNameAlert(UIViewController *root, NSString *title, NSString *actionTitle, NSString *initialText,
                              BOOL (^isBlank)(NSString *typed), void (^onAction)(NSString *typed)) {
  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addTextFieldWithConfigurationHandler:^(UITextField *textField) {
    textField.text = initialText;
    textField.placeholder = @"Layer name";
    textField.autocapitalizationType = UITextAutocapitalizationTypeWords;
    textField.clearButtonMode = UITextFieldViewModeWhileEditing;
    textField.returnKeyType = UIReturnKeyDone;
  }];
  __weak UIAlertController *weakAlert = alert;
  UIAlertAction *cancel = [UIAlertAction actionWithTitle:@"Cancel"
                                                   style:UIAlertActionStyleCancel
                                                 handler:nil];
  UIAlertAction *action = [UIAlertAction actionWithTitle:actionTitle
                                                   style:UIAlertActionStyleDefault
                                                 handler:^(UIAlertAction *chosen) {
    UIAlertController *strongAlert = weakAlert;
    NSString *typed = strongAlert.textFields.firstObject.text;
    onAction(typed ? typed : @"");
  }];
  action.enabled = !isBlank(initialText);
  [alert addAction:cancel];
  [alert addAction:action];
  alert.preferredAction = action;

  UITextField *field = alert.textFields.firstObject;
  __weak UIAlertAction *weakAction = action;
  __weak UITextField *weakField = field;
  [field addAction:[UIAction actionWithHandler:^(__kindof UIAction *changed) {
          UIAlertAction *strongAction = weakAction;
          UITextField *strongField = weakField;
          NSString *typed = strongField.text;
          strongAction.enabled = !isBlank(typed ? typed : @"");
        }]
      forControlEvents:UIControlEventEditingChanged];
  [OSTLayerPresenter(root) presentViewController:alert animated:YES completion:nil];
}

} // namespace

// The private half of this file: the flows behind the actions. Nothing outside this file calls or names these (the ones that do are declared in
// the block "M1 layers (S2)" of ViewController_Internal.h).
@interface ViewController (LayerEditing)
- (BOOL)_layerNameIsBlank:(NSString *)typed;
- (void)_presentNewLayerAlertAfterUID:(NSString *)afterUID;
- (void)_createLayerNamed:(NSString *)typed afterUID:(NSString *)afterUID;
- (void)_presentRenameAlertForUID:(NSString *)layerUID;
- (void)_renameLayerUID:(NSString *)layerUID toTyped:(NSString *)typed;
- (void)_moveLayerUID:(NSString *)layerUID direction:(int)direction;
- (void)_startDeleteOfLayerUID:(NSString *)layerUID;
- (void)_deleteLayerUIDs:(NSArray<NSString *> *)layerUIDs;
- (void)_layerRenameTapped:(NSString *)layerUID;
- (void)_layerAddBelowTapped:(NSString *)layerUID;
- (void)_layerMoveTapped:(NSString *)layerUID direction:(int)direction;
- (void)_layerDeleteTapped:(NSString *)layerUID;
- (BOOL)_reloadLayerLists;
- (void)_layerSheetChangedSelectingUID:(NSString *)layerUID;
- (void)_layerSheetRefreshAfterFailedWrite;
@end

@implementation ViewController (LayerEditing)

#pragma mark - Sheet state

// ProjectService.sortedLayerUIDs is the row order of the sheet: the uids of merged_layers, in list order (the list is already merged and sorted,
// Core merge_layers_for_bid, LP-24, LP-18). Rebuilt after every reload of the lists, and when the sheet opens.
- (void)_rebuildSortedLayerUIDs {
  std::vector<std::string> &sorted = self.projectService.sortedLayerUIDs;
  sorted.clear();
  for (const ost_domain::BidLayer &layer : self.projectService.project.merged_layers)
    sorted.push_back(layer.uid);
}

// Re-reads the bid's layer lists (ProjectData::layers AND ProjectData::merged_layers, together) from the file after a write that committed, and
// nothing else: not the bid, not the undo history (4.11). NO leaves both lists as they were (the bridge replaces them together, only on success).
// The Edit Condition layer picker takes copies of both lists when it opens (ViewController+Conditions.mm), so a created layer is offered at once.
- (BOOL)_reloadLayerLists {
  ost_data::ProjectData &project = self.projectService.mutableProject;
  NSString *reason = nil;
  return [ImportService reloadLayersForBid:self.bidUID
                                 ownLayers:project.layers
                              mergedLayers:project.merged_layers
                                     error:&reason];
}

// After a write and a successful reload: the rows of the sheet follow the lists, the row of `layerUID` (the new layer, the renamed or moved one;
// nil after a delete) is selected and scrolled into view, and the Conditions list is loaded again (it greys the conditions of hidden layers; a
// layer change reaches it through the same path as a visibility change, 4.11). Selecting by code does not call didSelectRow, so nothing toggles.
- (void)_layerSheetChangedSelectingUID:(NSString *)layerUID {
  [self _rebuildSortedLayerUIDs];
  UITableView *table = [self _layersTableView];
  [table reloadData];
  [table layoutIfNeeded];
  if (layerUID.length > 0) {
    const std::vector<std::string> &sorted = self.projectService.sortedLayerUIDs;
    const std::string uid = OSTLayerStd(layerUID);
    for (size_t row = 0; row < sorted.size(); row++) {
      if (sorted[row] != uid)
        continue;
      NSIndexPath *path = [NSIndexPath indexPathForRow:(NSInteger)row inSection:0];
      [table selectRowAtIndexPath:path animated:NO scrollPosition:UITableViewScrollPositionNone];
      [table scrollToRowAtIndexPath:path atScrollPosition:UITableViewScrollPositionNone animated:YES];
      break;
    }
  }
  [self _refreshConditionsListForLayerVisibility];
}

// A write that failed or was refused changed nothing, but what the sheet shows may be out of date. Windows loads the list again after a failed
// write: the dialog after every move, worked or not (layers_dialog.py:652), the sidebar after a failed create, delete or rename
// (ui_event_coordinator.py:6578-6580, :6647-6648, :6917-6918). The lists are read again, quietly: a read that fails too keeps the lists as they
// are and says nothing more (the failure alert is already up).
- (void)_layerSheetRefreshAfterFailedWrite {
  if ([self _reloadLayerLists])
    [self _layerSheetChangedSelectingUID:nil];
}

- (BOOL)_layerNameIsBlank:(NSString *)typed {
  const ost_data::ProjectData &project = self.projectService.project;
  const ost_domain::LayerNameCheck check =
      ost_domain::check_layer_name(OSTLayerStd(typed), "", project.merged_layers, project.layers, "");
  return check.verdict == ost_domain::LayerNameVerdict::Blank;
}

#pragma mark - New Layer

// The "+" of the sheet: Windows New with no row selected, so the layer goes after the highest sequence (layers_dialog.py:315-322).
- (void)_layersAddTapped:(id)sender {
  if ([self _refuseIfBidLocked])
    return;
  [self _presentNewLayerAlertAfterUID:nil];
}

// "New Layer Below": Windows New with that row selected, so the layer goes after that row's sequence, whatever kind of row it is.
- (void)_layerAddBelowTapped:(NSString *)layerUID {
  if ([self _refuseIfBidLocked])
    return;
  if (!OSTListedLayer(self.projectService.project.merged_layers, OSTLayerStd(layerUID)))
    return;
  [self _presentNewLayerAlertAfterUID:layerUID];
}

- (void)_presentNewLayerAlertAfterUID:(NSString *)afterUID {
  __weak ViewController *weakSelf = self;
  NSString *selectedUID = [afterUID copy];
  OSTLayerPresentNameAlert(
      self, @"New Layer", @"Create", @"",
      ^BOOL(NSString *typed) {
        ViewController *strongSelf = weakSelf;
        return strongSelf ? [strongSelf _layerNameIsBlank:typed] : YES;
      },
      ^(NSString *typed) {
        [weakSelf _createLayerNamed:typed afterUID:selectedUID];
      });
}

// The name rules run here, on the lists as they are now (Core check_layer_name: strip, blank and the refusals below; the store runs them again
// inside its transaction): blank is silent, a duplicate (also "Comments", which is always taken, LY-1), a name over 50 UTF-16 units and a control
// character or line break each show their text. Then the write; the new row is Show -1 (visible whatever Hide All did before).
- (void)_createLayerNamed:(NSString *)typed afterUID:(NSString *)afterUID {
  const ost_data::ProjectData &project = self.projectService.project;
  const ost_domain::LayerNameCheck check =
      ost_domain::check_layer_name(OSTLayerStd(typed), "", project.merged_layers, project.layers, "");
  if (check.verdict != ost_domain::LayerNameVerdict::Accept) {
    if (!check.title.empty() || !check.message.empty())
      OSTLayerPresentAlert(self, OSTLayerNS(check.title), OSTLayerNS(check.message));
    return;
  }
  const long long afterSequence = ost_domain::new_layer_after_sequence(
      project.merged_layers, afterUID ? OSTLayerStd(afterUID) : std::string());
  NSString *storeText = nil;
  NSString *newUID = [ImportService insertLayerNamed:OSTLayerNS(check.name)
                                       afterSequence:afterSequence
                                               inBid:self.bidUID
                                               error:&storeText];
  if (!newUID) {
    OSTLayerPresentDialog(self, ost_domain::layer_write_failure_text(ost_domain::LayerAction::Create, OSTLayerStd(storeText)));
    [self _layerSheetRefreshAfterFailedWrite];
    return;
  }
  // The layer is in the file. A list that cannot be read again keeps the new state and says so (Windows: "Refresh Error").
  if (![self _reloadLayerLists]) {
    OSTLayerPresentDialog(self, ost_domain::layer_refresh_failure_text(ost_domain::LayerAction::Create));
    return;
  }
  [self _layerSheetChangedSelectingUID:newUID];
}

#pragma mark - Rename

- (void)_layerRenameTapped:(NSString *)layerUID {
  if ([self _refuseIfBidLocked])
    return;
  [self _presentRenameAlertForUID:layerUID];
}

- (void)_presentRenameAlertForUID:(NSString *)layerUID {
  const ost_domain::BidLayer *row = OSTListedLayer(self.projectService.project.merged_layers, OSTLayerStd(layerUID));
  // Only the bid's own layer that is neither a template nor locked is renamed (Windows _can_modify; the swipe and the menu do not offer it for
  // another row, this is the guard behind them).
  if (!row || !ost_domain::layer_row_actions(self.projectService.project.merged_layers, OSTLayerStd(self.bidUID), row->uid).can_rename)
    return;
  __weak ViewController *weakSelf = self;
  NSString *uid = [layerUID copy];
  OSTLayerPresentNameAlert(
      self, @"Rename Layer", @"Rename", OSTLayerNS(row->name),
      ^BOOL(NSString *typed) {
        ViewController *strongSelf = weakSelf;
        return strongSelf ? [strongSelf _layerNameIsBlank:typed] : YES;
      },
      ^(NSString *typed) {
        [weakSelf _renameLayerUID:uid toTyped:typed];
      });
}

// Windows rename path (layers_dialog.py:380-427): strip; blank and a name equal to the stored one are silent (Core: Blank, Unchanged); another
// layer with that name by strip().lower() is "Duplicate Layer" (the layer's own row never counts, so a change of letter case is written); then the
// write. A rename that touches annotation / comments / image (old or new name) draws the page and the meshes again, any other rename only the
// lists (layer_rename_preserves_rendering).
- (void)_renameLayerUID:(NSString *)layerUID toTyped:(NSString *)typed {
  const ost_data::ProjectData &project = self.projectService.project;
  const std::string uid = OSTLayerStd(layerUID);
  const ost_domain::BidLayer *row = OSTListedLayer(project.merged_layers, uid);
  if (!row)
    return;
  const ost_domain::LayerNameCheck check =
      ost_domain::check_layer_name(OSTLayerStd(typed), row->name, project.merged_layers, project.layers, uid);
  if (check.verdict != ost_domain::LayerNameVerdict::Accept) {
    if (!check.title.empty() || !check.message.empty())
      OSTLayerPresentAlert(self, OSTLayerNS(check.title), OSTLayerNS(check.message));
    return;
  }
  const std::vector<ost_domain::BidLayer> before = project.merged_layers;
  NSString *storeText = nil;
  if (![ImportService renameLayer:layerUID toName:OSTLayerNS(check.name) inBid:self.bidUID error:&storeText]) {
    OSTLayerPresentDialog(self, ost_domain::layer_write_failure_text(ost_domain::LayerAction::Rename, OSTLayerStd(storeText)));
    [self _layerSheetRefreshAfterFailedWrite];
    return;
  }
  if (![self _reloadLayerLists]) {
    OSTLayerPresentDialog(self, ost_domain::layer_refresh_failure_text(ost_domain::LayerAction::Rename));
    return;
  }
  const bool preserved = ost_domain::layer_rename_preserves_rendering(before, self.projectService.project.merged_layers);
  [self _layerSheetChangedSelectingUID:layerUID];
  if (!preserved) {
    // the page background follows the layer named "image" (PlanView renderOverlaysForPage), the meshes are built from the visible layers
    [self _generateMeshesForSelectedPages];
    [self _refreshOverlays];
  }
}

#pragma mark - Move

- (void)_layerMoveTapped:(NSString *)layerUID direction:(int)direction {
  if ([self _refuseIfBidLocked])
    return;
  [self _moveLayerUID:layerUID direction:direction];
}

// Move Up / Down = an exchange of the Sequence of the row and its neighbour in the LISTED order (layers_dialog.py:628-654); Core's plan names
// the neighbour and refuses a locked or shared row, the ends of the list and a shared neighbour (LY-6). The moved row stays selected.
- (void)_moveLayerUID:(NSString *)layerUID direction:(int)direction {
  const ost_domain::LayerMovePlan plan = ost_domain::plan_layer_move(
      self.projectService.project.merged_layers, OSTLayerStd(self.bidUID), OSTLayerStd(layerUID), direction);
  if (!plan.allowed)
    return;
  if (![ImportService swapLayer:layerUID withLayer:OSTLayerNS(plan.neighbor_uid) inBid:self.bidUID]) {
    OSTLayerPresentDialog(self, ost_domain::layer_write_failure_text(ost_domain::LayerAction::Move, std::string()));
    [self _layerSheetRefreshAfterFailedWrite];
    return;
  }
  if (![self _reloadLayerLists]) {
    OSTLayerPresentDialog(self, ost_domain::layer_refresh_failure_text(ost_domain::LayerAction::Move));
    return;
  }
  [self _layerSheetChangedSelectingUID:layerUID];
}

#pragma mark - Delete

- (void)_layerDeleteTapped:(NSString *)layerUID {
  if ([self _refuseIfBidLocked])
    return;
  [self _startDeleteOfLayerUID:layerUID];
}

// The Windows delete flow for one layer (layers_dialog.py:487-548, messagebox.py:78-119): the usage is read from the file when the action starts
// (the strict 11-table rule; an unreadable usage refuses the delete, LY-2: Windows would answer "nothing in use" and clear the references), then
// Core's plan decides: a layer in use is only refused with its Windows text, anything else asks "Delete 1 item(s)?" with No as the default.
// The store itself clears the references and deletes, so this check is what keeps a condition from losing its layer.
- (void)_startDeleteOfLayerUID:(NSString *)layerUID {
  const ost_data::ProjectData &project = self.projectService.project;
  const std::string uid = OSTLayerStd(layerUID);
  if (!ost_domain::layer_row_actions(project.merged_layers, OSTLayerStd(self.bidUID), uid).can_delete)
    return;
  NSSet<NSString *> *used = nil;
  if (![ImportService layerUIDsInUse:&used inBid:self.bidUID]) {
    OSTLayerPresentAlert(self, OSTLayerNS(ost_domain::kDeleteLayerTitle), OSTLayerNS(ost_domain::kLayerUsageUnreadable));
    return;
  }
  std::set<std::string> usedUIDs;
  for (NSString *usedUID in used)
    usedUIDs.insert(OSTLayerStd(usedUID));
  const ost_domain::LayerDeletePlan plan = ost_domain::plan_layer_delete(project.merged_layers, std::vector<std::string>{uid}, usedUIDs);
  if (plan.kind == ost_domain::LayerDeleteKind::NothingToDelete)
    return;
  NSString *title = OSTLayerNS(plan.title);
  NSString *message = OSTLayerNS(plan.message);
  if (plan.kind == ost_domain::LayerDeleteKind::OnlyInUse) {
    OSTLayerPresentAlert(self, title, message);
    return;
  }
  // Confirm / ConfirmMixed: only the layers that are not in use go on, never an empty list.
  NSMutableArray<NSString *> *toDelete = [NSMutableArray arrayWithCapacity:plan.deletable.size()];
  for (const ost_domain::LayerRef &ref : plan.deletable)
    [toDelete addObject:OSTLayerNS(ref.uid)];
  if (toDelete.count == 0)
    return;
  __weak ViewController *weakSelf = self;
  UIAlertController *alert = [UIAlertController alertControllerWithTitle:title
                                                                 message:message
                                                          preferredStyle:UIAlertControllerStyleAlert];
  // Windows: Yes / No, default No (messagebox.py:98-118). The cancel-style button is the bold default of an alert.
  [alert addAction:[UIAlertAction actionWithTitle:@"No" style:UIAlertActionStyleCancel handler:nil]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Yes"
                                            style:UIAlertActionStyleDestructive
                                          handler:^(UIAlertAction *chosen) {
                                            [weakSelf _deleteLayerUIDs:toDelete];
                                          }]];
  [OSTLayerPresenter(self) presentViewController:alert animated:YES completion:nil];
}

// One transaction, all or none (LY-4). The list that is passed is never empty.
- (void)_deleteLayerUIDs:(NSArray<NSString *> *)layerUIDs {
  if (layerUIDs.count == 0)
    return;
  if (![ImportService deleteLayers:layerUIDs inBid:self.bidUID]) {
    OSTLayerPresentDialog(self, ost_domain::layer_write_failure_text(ost_domain::LayerAction::Delete, std::string()));
    [self _layerSheetRefreshAfterFailedWrite];
    return;
  }
  // The layers are gone from the file. Layers only are re-read (never the whole bid: the undo history stays, 4.11).
  if (![self _reloadLayerLists]) {
    OSTLayerPresentDialog(self, ost_domain::layer_refresh_failure_text(ost_domain::LayerAction::Delete));
    return;
  }
  [self _layerSheetChangedSelectingUID:nil];
}

#pragma mark - Row actions (swipe and long-press menu)

// Declared in the block "M1 layers (S2)" of ViewController_Internal.h: ViewController+Layers.mm calls it from
// tableView:trailingSwipeActionsConfigurationForRowAtIndexPath:. Delete (destructive) and Rename, only for a row Core says can be changed; a row
// with neither gets no swipe at all. No full swipe: a swipe never starts a delete by itself, and the action only OPENS the confirmation, so the
// row stays until the list is reloaded after the write (completion NO).
- (UISwipeActionsConfiguration *)_layerSwipeActionsForUID:(NSString *)layerUID {
  if (layerUID.length == 0)
    return nil;
  const ost_domain::LayerRowActions actions = ost_domain::layer_row_actions(
      self.projectService.project.merged_layers, OSTLayerStd(self.bidUID), OSTLayerStd(layerUID));
  NSMutableArray<UIContextualAction *> *list = [NSMutableArray array];
  __weak ViewController *weakSelf = self;
  if (actions.can_delete) {
    UIContextualAction *deleteAction = [UIContextualAction
        contextualActionWithStyle:UIContextualActionStyleDestructive
                            title:@"Delete"
                          handler:^(UIContextualAction *action, __kindof UIView *sourceView, void (^completionHandler)(BOOL)) {
                            [weakSelf _layerDeleteTapped:layerUID];
                            completionHandler(NO);
                          }];
    [list addObject:deleteAction];
  }
  if (actions.can_rename) {
    UIContextualAction *renameAction = [UIContextualAction
        contextualActionWithStyle:UIContextualActionStyleNormal
                            title:@"Rename"
                          handler:^(UIContextualAction *action, __kindof UIView *sourceView, void (^completionHandler)(BOOL)) {
                            [weakSelf _layerRenameTapped:layerUID];
                            completionHandler(NO);
                          }];
    renameAction.backgroundColor = [UIColor systemBlueColor];
    [list addObject:renameAction];
  }
  if (list.count == 0)
    return nil;
  UISwipeActionsConfiguration *configuration = [UISwipeActionsConfiguration configurationWithActions:list];
  configuration.performsFirstActionWithFullSwipe = NO;
  return configuration;
}

// Declared in the same block: ViewController+Layers.mm calls it from tableView:contextMenuConfigurationForRowAtIndexPath:point:. The menu (a
// UIMenu, anchored by the system) lists Rename, New Layer Below, Move Up, Move Down and Delete in that order; an item Windows disables for the
// row is shown disabled (a locked copy, a shared row, the ends of the list, a shared neighbour). What each item offers is worked out when the
// menu opens, from the lists as they are then; each item re-checks the bid lock first and the plan again when it runs.
- (UIContextMenuConfiguration *)_layerContextMenuForUID:(NSString *)layerUID {
  if (layerUID.length == 0)
    return nil;
  __weak ViewController *weakSelf = self;
  return [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu *(NSArray<UIMenuElement *> *suggestedActions) {
                     ViewController *strongSelf = weakSelf;
                     if (!strongSelf)
                       return nil;
                     const ost_domain::LayerRowActions actions = ost_domain::layer_row_actions(
                         strongSelf.projectService.project.merged_layers, OSTLayerStd(strongSelf.bidUID), OSTLayerStd(layerUID));
                     UIAction *renameItem = [UIAction actionWithTitle:@"Rename"
                                                            image:[UIImage systemImageNamed:@"pencil"]
                                                       identifier:nil
                                                          handler:^(__kindof UIAction *picked) {
                                                            [weakSelf _layerRenameTapped:layerUID];
                                                          }];
                     if (!actions.can_rename)
                       renameItem.attributes = renameItem.attributes | UIMenuElementAttributesDisabled;
                     UIAction *addBelow = [UIAction actionWithTitle:@"New Layer Below"
                                                              image:[UIImage systemImageNamed:@"plus"]
                                                         identifier:nil
                                                            handler:^(__kindof UIAction *picked) {
                                                              [weakSelf _layerAddBelowTapped:layerUID];
                                                            }];
                     if (!actions.can_add_below)
                       addBelow.attributes = addBelow.attributes | UIMenuElementAttributesDisabled;
                     UIAction *moveUp = [UIAction actionWithTitle:@"Move Up"
                                                            image:[UIImage systemImageNamed:@"arrow.up"]
                                                       identifier:nil
                                                          handler:^(__kindof UIAction *picked) {
                                                            [weakSelf _layerMoveTapped:layerUID direction:-1];
                                                          }];
                     if (!actions.can_move_up)
                       moveUp.attributes = moveUp.attributes | UIMenuElementAttributesDisabled;
                     UIAction *moveDown = [UIAction actionWithTitle:@"Move Down"
                                                              image:[UIImage systemImageNamed:@"arrow.down"]
                                                         identifier:nil
                                                            handler:^(__kindof UIAction *picked) {
                                                              [weakSelf _layerMoveTapped:layerUID direction:1];
                                                            }];
                     if (!actions.can_move_down)
                       moveDown.attributes = moveDown.attributes | UIMenuElementAttributesDisabled;
                     UIAction *deleteItem = [UIAction actionWithTitle:@"Delete"
                                                            image:[UIImage systemImageNamed:@"trash"]
                                                       identifier:nil
                                                          handler:^(__kindof UIAction *picked) {
                                                            [weakSelf _layerDeleteTapped:layerUID];
                                                          }];
                     deleteItem.attributes = deleteItem.attributes | UIMenuElementAttributesDestructive;
                     if (!actions.can_delete)
                       deleteItem.attributes = deleteItem.attributes | UIMenuElementAttributesDisabled;
                     return [UIMenu menuWithTitle:@"" children:@[ renameItem, addBelow, moveUp, moveDown, deleteItem ]];
                   }];
}

@end
