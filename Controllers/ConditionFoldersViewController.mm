#import "ConditionFoldersViewController.h"
#include "../Core/Domain/ConditionFolderRules.hpp"
#include "../Core/Domain/ConditionFolderUi.hpp"
#include "../Core/Persistence/ProjectStore/ProjectStore_ConditionFolderAlerts.hpp"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

NSString *OSTText(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

// The exact UTF-8 bytes of an NSString for Core. nil is the empty string. A NUL is kept (Core refuses such a name) and text that has no UTF-8 form
// (a lone surrogate) becomes a byte that is not valid UTF-8 (Core refuses it): never an empty or a shortened name. (-UTF8String stops at a NUL.)
std::string OSTExactUTF8(NSString *text) {
  if (!text) return std::string();
  if (![text isKindOfClass:[NSString class]]) return std::string("\xFF");
  NSData *data = [text dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
  if (!data) return std::string("\xFF");
  return std::string(static_cast<const char *>(data.bytes), data.length);
}

// The indentation of a row stops growing at this depth (plan D7: no cap in Core, the screen only keeps a deep chain readable).
const NSInteger kMaxIndentLevel = 8;

}  // namespace

@interface ConditionFoldersViewController () <UITableViewDataSource, UITableViewDelegate, UITextFieldDelegate>
@end

@implementation ConditionFoldersViewController {
  UITableView *_tableView;
  UILabel *_emptyLabel;
  UIBarButtonItem *_addButton;
  UIBarButtonItem *_deleteSelectedButton;
  ConditionFoldersMode _mode;
  std::unordered_map<std::string, ost_domain::ConditionFolder> _folders;
  std::unordered_map<std::string, ost_domain::Condition> _conditions;
  std::unordered_map<std::string, ost_domain::CdnType> _cdnTypes;
  std::vector<ost_domain::ConditionFolderRow> _rows;
  NSString *_pickConditionUID;
  // Pick mode: the folder the condition is listed under now ("" = no folder), which gets the checkmark
  std::string _currentFolderUID;
}

@synthesize delegate = _delegate;

#pragma mark - Init and data

- (instancetype)initWithProject:(const ost_data::ProjectData &)project
                           mode:(ConditionFoldersMode)mode
               pickConditionUID:(NSString *)conditionUID {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _mode = mode;
    _pickConditionUID = [conditionUID copy];
    [self _readProject:project];
  }
  return self;
}

- (void)_readProject:(const ost_data::ProjectData &)project {
  _folders = project.condition_folders;
  _conditions = project.conditions;
  _cdnTypes = project.cdn_types;
  _rows = ost_domain::condition_folder_rows(_folders, _conditions, _cdnTypes);
  _currentFolderUID.clear();
  if (_mode == ConditionFoldersModePickFolder && _pickConditionUID) {
    auto condition = _conditions.find(OSTExactUTF8(_pickConditionUID));
    if (condition != _conditions.end())
      _currentFolderUID = ost_domain::condition_listed_folder_uid(_folders, condition->second);
  }
}

- (void)loadFoldersFromProject:(const ost_data::ProjectData &)project {
  [self _readProject:project];
  [self _refreshView];
}

- (void)_refreshView {
  if (!_tableView)
    return;
  [_tableView reloadData];
  _emptyLabel.hidden = !(_mode == ConditionFoldersModeManage && _rows.empty());
  [self _updateBarItems];
}

#pragma mark - Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor systemGroupedBackgroundColor];
  self.title = _mode == ConditionFoldersModeManage ? OSTText(ost_domain::kFoldersScreenTitle)
                                                   : OSTText(ost_domain::kMoveToFolderTitle);

  _tableView = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStyleInsetGrouped];
  _tableView.dataSource = self;
  _tableView.delegate = self;
  _tableView.allowsMultipleSelectionDuringEditing = YES;
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_tableView];
  [NSLayoutConstraint activateConstraints:@[
    [_tableView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_tableView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [_tableView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_tableView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  _emptyLabel = [[UILabel alloc] init];
  _emptyLabel.text = OSTText(ost_domain::kNoFoldersText);
  _emptyLabel.textColor = [UIColor secondaryLabelColor];
  _emptyLabel.textAlignment = NSTextAlignmentCenter;
  _emptyLabel.numberOfLines = 0;
  _emptyLabel.hidden = YES;
  _emptyLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_emptyLabel];
  [NSLayoutConstraint activateConstraints:@[
    [_emptyLabel.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [_emptyLabel.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
    [_emptyLabel.leadingAnchor constraintGreaterThanOrEqualToAnchor:self.view.layoutMarginsGuide.leadingAnchor],
    [_emptyLabel.trailingAnchor constraintLessThanOrEqualToAnchor:self.view.layoutMarginsGuide.trailingAnchor],
  ]];

  if (_mode == ConditionFoldersModeManage) {
    _addButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
                                                               target:self
                                                               action:@selector(_addTapped:)];
    _deleteSelectedButton = [[UIBarButtonItem alloc] initWithTitle:@"Delete"
                                                             style:UIBarButtonItemStylePlain
                                                            target:self
                                                            action:@selector(_deleteSelectedTapped:)];
    _deleteSelectedButton.tintColor = [UIColor systemRedColor];
    _deleteSelectedButton.enabled = NO;
  }
  [self _refreshView];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  [_tableView setEditing:editing animated:animated];
  [self _updateBarItems];
}

- (void)_updateBarItems {
  if (_mode != ConditionFoldersModeManage)
    return;
  if (self.editing) {
    _deleteSelectedButton.enabled = _tableView.indexPathsForSelectedRows.count > 0;
    self.navigationItem.rightBarButtonItems = @[ self.editButtonItem, _deleteSelectedButton ];
  } else {
    self.navigationItem.rightBarButtonItems = @[ self.editButtonItem, _addButton ];
  }
}

#pragma mark - Row helpers

- (NSInteger)_rowOffset {
  return _mode == ConditionFoldersModePickFolder ? 1 : 0;
}

// The folder of a table row; NULL for the "(No folder)" row of the picker and for a row that is not there.
- (const ost_domain::ConditionFolderRow *)_folderRowAtIndexPath:(NSIndexPath *)indexPath {
  const NSInteger index = indexPath.row - [self _rowOffset];
  if (index < 0 || (size_t)index >= _rows.size())
    return nullptr;
  return &_rows[(size_t)index];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
  return (NSInteger)_rows.size() + [self _rowOffset];
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
  if (_mode != ConditionFoldersModeManage || _rows.empty())
    return nil;
  return @"Swipe a folder to rename or delete it, or to add a folder inside it; touch and hold it for the same choices. "
         @"A folder that holds a condition cannot be deleted. Deleting a folder moves the folders inside it to the top level.";
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"FolderCell"];
  if (!cell)
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"FolderCell"];
  cell.accessoryType = UITableViewCellAccessoryNone;
  cell.indentationLevel = 0;
  cell.indentationWidth = 18;
  cell.detailTextLabel.text = nil;

  const ost_domain::ConditionFolderRow *row = [self _folderRowAtIndexPath:indexPath];
  if (!row) {
    // the picker's first row: the top level
    cell.textLabel.text = OSTText(ost_domain::kNoFolderRowTitle);
    cell.imageView.image = [UIImage systemImageNamed:@"tray"];
    cell.accessoryType = _currentFolderUID.empty() ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone;
    return cell;
  }
  cell.textLabel.text = OSTText(row->name);
  cell.detailTextLabel.text = OSTText(ost_domain::condition_folder_count_text(row->condition_count));
  cell.imageView.image = [UIImage systemImageNamed:@"folder"];
  cell.indentationLevel = std::min<NSInteger>(row->depth, kMaxIndentLevel);
  if (_mode == ConditionFoldersModePickFolder && row->uid == _currentFolderUID)
    cell.accessoryType = UITableViewCellAccessoryCheckmark;
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  if (tableView.isEditing) {
    [self _updateBarItems];
    return;
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  if (_mode == ConditionFoldersModePickFolder)
    [self _pickRowAtIndexPath:indexPath];
}

- (void)tableView:(UITableView *)tableView didDeselectRowAtIndexPath:(NSIndexPath *)indexPath {
  if (tableView.isEditing)
    [self _updateBarItems];
}

// The multi-select edit mode shows selection circles, not the red minus of a single delete.
- (UITableViewCellEditingStyle)tableView:(UITableView *)tableView editingStyleForRowAtIndexPath:(NSIndexPath *)indexPath {
  return UITableViewCellEditingStyleNone;
}

- (BOOL)tableView:(UITableView *)tableView shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath *)indexPath {
  return NO;
}

- (UISwipeActionsConfiguration *)tableView:(UITableView *)tableView
    trailingSwipeActionsConfigurationForRowAtIndexPath:(NSIndexPath *)indexPath {
  const ost_domain::ConditionFolderRow *row = [self _folderRowAtIndexPath:indexPath];
  if (_mode != ConditionFoldersModeManage || tableView.isEditing || !row)
    return nil;
  NSString *uid = OSTText(row->uid);
  __weak ConditionFoldersViewController *weakSelf = self;
  UIContextualAction *remove = [UIContextualAction
      contextualActionWithStyle:UIContextualActionStyleDestructive
                          title:@"Delete"
                        handler:^(UIContextualAction *action, UIView *sourceView, void (^completion)(BOOL)) {
                          completion(NO);  // the row is redrawn from the database after the write, never removed by the swipe
                          // the alert comes after the swipe row has closed, not during its animation
                          dispatch_async(dispatch_get_main_queue(), ^{
                            [weakSelf _confirmAndDeleteFolderUIDs:@[ uid ]];
                          });
                        }];
  UIContextualAction *rename = [UIContextualAction
      contextualActionWithStyle:UIContextualActionStyleNormal
                          title:@"Rename"
                        handler:^(UIContextualAction *action, UIView *sourceView, void (^completion)(BOOL)) {
                          completion(NO);
                          dispatch_async(dispatch_get_main_queue(), ^{
                            [weakSelf _beginRenameFolderUID:uid];
                          });
                        }];
  rename.backgroundColor = [UIColor systemBlueColor];
  UISwipeActionsConfiguration *configuration = [UISwipeActionsConfiguration configurationWithActions:@[ remove, rename ]];
  configuration.performsFirstActionWithFullSwipe = NO;  // a full swipe must not start a delete
  return configuration;
}

- (UISwipeActionsConfiguration *)tableView:(UITableView *)tableView
    leadingSwipeActionsConfigurationForRowAtIndexPath:(NSIndexPath *)indexPath {
  const ost_domain::ConditionFolderRow *row = [self _folderRowAtIndexPath:indexPath];
  if (_mode != ConditionFoldersModeManage || tableView.isEditing || !row)
    return nil;
  NSString *uid = OSTText(row->uid);
  __weak ConditionFoldersViewController *weakSelf = self;
  UIContextualAction *inside = [UIContextualAction
      contextualActionWithStyle:UIContextualActionStyleNormal
                          title:@"New Folder Inside"
                        handler:^(UIContextualAction *action, UIView *sourceView, void (^completion)(BOOL)) {
                          completion(NO);
                          dispatch_async(dispatch_get_main_queue(), ^{
                            [weakSelf _beginNewFolderInParent:uid];
                          });
                        }];
  inside.backgroundColor = [UIColor systemGreenColor];
  UISwipeActionsConfiguration *configuration = [UISwipeActionsConfiguration configurationWithActions:@[ inside ]];
  configuration.performsFirstActionWithFullSwipe = NO;
  return configuration;
}

// The long-press menu: a UIMenu is anchored by the system (no popover anchor to set, plan 4.3). Each item presents an alert, so, like the swipe
// actions, it goes on in the next turn of the run loop: the menu may still be closing when the handler runs, and an alert presented during that
// would be dropped.
- (UIContextMenuConfiguration *)tableView:(UITableView *)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath *)indexPath
                                        point:(CGPoint)point {
  const ost_domain::ConditionFolderRow *row = [self _folderRowAtIndexPath:indexPath];
  if (_mode != ConditionFoldersModeManage || tableView.isEditing || !row)
    return nil;
  NSString *uid = OSTText(row->uid);
  NSString *name = OSTText(row->name);
  __weak ConditionFoldersViewController *weakSelf = self;
  return [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu *(NSArray<UIMenuElement *> *suggested) {
                     UIAction *inside = [UIAction actionWithTitle:@"New Folder Inside"
                                                            image:[UIImage systemImageNamed:@"folder.badge.plus"]
                                                       identifier:nil
                                                          handler:^(UIAction *action) {
                                                            dispatch_async(dispatch_get_main_queue(), ^{
                                                              [weakSelf _beginNewFolderInParent:uid];
                                                            });
                                                          }];
                     UIAction *rename = [UIAction actionWithTitle:@"Rename"
                                                            image:[UIImage systemImageNamed:@"pencil"]
                                                       identifier:nil
                                                          handler:^(UIAction *action) {
                                                            dispatch_async(dispatch_get_main_queue(), ^{
                                                              [weakSelf _beginRenameFolderUID:uid];
                                                            });
                                                          }];
                     UIAction *remove = [UIAction actionWithTitle:@"Delete"
                                                            image:[UIImage systemImageNamed:@"trash"]
                                                       identifier:nil
                                                          handler:^(UIAction *action) {
                                                            dispatch_async(dispatch_get_main_queue(), ^{
                                                              [weakSelf _confirmAndDeleteFolderUIDs:@[ uid ]];
                                                            });
                                                          }];
                     remove.attributes = UIMenuElementAttributesDestructive;
                     return [UIMenu menuWithTitle:name children:@[ inside, rename, remove ]];
                   }];
}

#pragma mark - UITextFieldDelegate

// The name alerts open with their text selected: typing replaces "New Folder" or the old name. The range is set directly, as in the page
// rename and the layer rename alerts: selectAll: can also bring up the Cut / Copy / Paste menu over the alert.
- (void)textFieldDidBeginEditing:(UITextField *)textField {
  __weak UITextField *weakField = textField;
  dispatch_async(dispatch_get_main_queue(), ^{
    UITextField *strongField = weakField;
    if (!strongField)
      return;
    strongField.selectedTextRange =
        [strongField textRangeFromPosition:strongField.beginningOfDocument
                                toPosition:strongField.endOfDocument];
  });
}

#pragma mark - Alerts

- (void)_presentDialog:(const ost_domain::DialogText &)dialog then:(void (^)(void))then {
  UIAlertController *alert = [UIAlertController alertControllerWithTitle:OSTText(dialog.title)
                                                                 message:OSTText(dialog.message)
                                                          preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction *action) {
                                            if (then)
                                              then();
                                          }]];
  [self presentViewController:alert animated:YES completion:nil];
}

// The alert of a write that did not go through (Core picks the text, plan CF-7); nothing when it did, and nothing for a blank name.
- (void)_showFailureOfOutcome:(OSTConditionFolderOutcome *)outcome
                       action:(ost_store::ConditionFolderAction)action
                        items:(const std::vector<ost_domain::FolderDeleteItem> &)items {
  if (!outcome || outcome.ok)
    return;
  ost_store::ConditionFolderResult result;
  result.ok = false;
  result.refusal = outcome.refusal;
  result.message = OSTExactUTF8(outcome.message);
  for (NSString *uid in outcome.blocked)
    result.blocked.push_back(OSTExactUTF8(uid));
  if (const auto dialog = ost_store::condition_folder_failure_dialog(action, result, items))
    [self _presentDialog:*dialog then:nil];
}

- (BOOL)_mayWrite {
  return !_delegate || [_delegate conditionFoldersViewControllerMayWrite:self];
}

#pragma mark - New Folder

- (void)_addTapped:(UIBarButtonItem *)sender {
  [self _beginNewFolderInParent:@""];
}

// Name first (plan N-M1-15): an alert prefilled with the first free sibling name ("New Folder", "New Folder (2)", ...), Cancel creates nothing. `parentFolderUID` "" = the top level.
- (void)_beginNewFolderInParent:(NSString *)parentFolderUID {
  if (![self _mayWrite])
    return;
  [self _promptNewFolderInParent:parentFolderUID draft:nil];
}

- (void)_promptNewFolderInParent:(NSString *)parentFolderUID draft:(NSString *)draft {
  UIAlertController *alert = [UIAlertController alertControllerWithTitle:OSTText(ost_domain::kNewFolderTitle)
                                                                 message:nil
                                                          preferredStyle:UIAlertControllerStyleAlert];
  __weak ConditionFoldersViewController *weakSelf = self;
  // A fresh prompt starts with the first free sibling name (Windows _free_folder_name: "New Folder", "New Folder (2)", ...); a draft wins.
  NSString *prefill = draft ?: OSTText(ost_domain::free_condition_folder_name(_folders, OSTExactUTF8(parentFolderUID)));
  [alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
    field.text = prefill;
    field.placeholder = @"Folder name";
    field.autocapitalizationType = UITextAutocapitalizationTypeWords;
    field.clearButtonMode = UITextFieldViewModeWhileEditing;
    field.delegate = weakSelf;
  }];
  __weak UIAlertController *weakAlert = alert;
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Create"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction *action) {
                                            [weakSelf _createFolderNamed:weakAlert.textFields.firstObject.text
                                                                inParent:parentFolderUID];
                                          }]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_createFolderNamed:(NSString *)typed inParent:(NSString *)parentFolderUID {
  // The name rule of Core (strip, blank, 50 units, a sibling with the name): a refusal is shown and the alert comes back with the draft; a
  // blank name is silent. The store applies the same rules again from the database.
  const ost_domain::FolderNameCheck check = ost_domain::check_condition_folder_name(
      _folders, OSTExactUTF8(parentFolderUID), std::string(), OSTExactUTF8(typed));
  if (const auto dialog = ost_domain::condition_folder_name_refusal_dialog(check)) {
    __weak ConditionFoldersViewController *weakSelf = self;
    [self _presentDialog:*dialog
                    then:^{
                      [weakSelf _promptNewFolderInParent:parentFolderUID draft:typed];
                    }];
    return;
  }
  if (check.issue == ost_domain::FolderNameIssue::Blank)
    return;
  OSTConditionFolderOutcome *outcome = [_delegate conditionFoldersViewController:self
                                                               createFolderNamed:typed ?: @""
                                                               inParentFolderUID:parentFolderUID ?: @""];
  [self _showFailureOfOutcome:outcome action:ost_store::ConditionFolderAction::Create items:std::vector<ost_domain::FolderDeleteItem>()];
}

#pragma mark - Rename

- (void)_beginRenameFolderUID:(NSString *)folderUID {
  if (![self _mayWrite])
    return;
  [self _promptRenameFolderUID:folderUID draft:nil];
}

- (void)_promptRenameFolderUID:(NSString *)folderUID draft:(NSString *)draft {
  auto folder = _folders.find(OSTExactUTF8(folderUID));
  if (folder == _folders.end())
    return;
  NSString *storedName = OSTText(folder->second.name);
  UIAlertController *alert = [UIAlertController alertControllerWithTitle:OSTText(ost_domain::kRenameFolderTitle)
                                                                 message:nil
                                                          preferredStyle:UIAlertControllerStyleAlert];
  __weak ConditionFoldersViewController *weakSelf = self;
  [alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
    field.text = draft ?: storedName;
    field.placeholder = @"Folder name";
    field.autocapitalizationType = UITextAutocapitalizationTypeWords;
    field.clearButtonMode = UITextFieldViewModeWhileEditing;
    field.delegate = weakSelf;
  }];
  __weak UIAlertController *weakAlert = alert;
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Rename"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction *action) {
                                            [weakSelf _renameFolderUID:folderUID typed:weakAlert.textFields.firstObject.text];
                                          }]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_renameFolderUID:(NSString *)folderUID typed:(NSString *)typed {
  // conditions_sidebar.py:1455-1481 through Core: blank and unchanged are silent, a refusal is shown (the draft comes back), anything else is written
  std::string newName;
  ost_domain::FolderNameCheck check;
  const ost_domain::FolderRenameDecision decision = ost_domain::decide_condition_folder_rename(
      _folders, OSTExactUTF8(folderUID), OSTExactUTF8(typed), newName, &check);
  if (decision == ost_domain::FolderRenameDecision::Unchanged)
    return;
  if (decision == ost_domain::FolderRenameDecision::Refused) {
    if (const auto dialog = ost_domain::condition_folder_name_refusal_dialog(check)) {
      __weak ConditionFoldersViewController *weakSelf = self;
      [self _presentDialog:*dialog
                      then:^{
                        [weakSelf _promptRenameFolderUID:folderUID draft:typed];
                      }];
    }
    return;
  }
  OSTConditionFolderOutcome *outcome = [_delegate conditionFoldersViewController:self
                                                                 renameFolderUID:folderUID
                                                                          toName:typed ?: @""];
  [self _showFailureOfOutcome:outcome action:ost_store::ConditionFolderAction::Rename items:std::vector<ost_domain::FolderDeleteItem>()];
}

#pragma mark - Delete

- (void)_deleteSelectedTapped:(UIBarButtonItem *)sender {
  NSMutableArray<NSString *> *uids = [NSMutableArray array];
  for (NSIndexPath *indexPath in _tableView.indexPathsForSelectedRows) {
    const ost_domain::ConditionFolderRow *row = [self _folderRowAtIndexPath:indexPath];
    if (row)
      [uids addObject:OSTText(row->uid)];
  }
  [self _confirmAndDeleteFolderUIDs:uids];
}

// condition_action_handler.py:1514-1575: the in-use rule first (Core, THE service rule), then the confirmation with the Windows texts plus the
// iOS line that the folders inside a deleted folder move to the top level, then the delete of the folders that are not in use.
- (void)_confirmAndDeleteFolderUIDs:(NSArray<NSString *> *)folderUIDs {
  if (![self _mayWrite])
    return;
  std::vector<std::string> requested;
  for (NSString *uid in folderUIDs)
    requested.push_back(OSTExactUTF8(uid));
  if (requested.empty())
    return;

  const ost_domain::ConditionFolderDeletePlan plan = ost_domain::plan_condition_folder_delete(_folders, _conditions, requested);
  if (plan.unknown_folder)
    return;  // a folder that is not there: Windows does nothing (:1526-1530)
  std::vector<ost_domain::FolderDeleteItem> items;
  for (const std::string &uid : requested) {
    auto folder = _folders.find(uid);
    if (folder != _folders.end())
      items.push_back(ost_domain::FolderDeleteItem{folder->second.name, uid});
  }
  const ost_domain::FolderDeleteConfirmation confirmation =
      ost_domain::condition_folder_delete_confirmation(items, plan.blocked, plan.subfolders_move);
  if (confirmation.kind == ost_domain::FolderDeleteDialogKind::Warn) {
    // every folder is in use: only a warning, nothing is asked and nothing is deleted
    [self _presentDialog:confirmation.dialog then:nil];
    return;
  }

  NSMutableArray<NSString *> *deletable = [NSMutableArray array];
  for (const std::string &uid : confirmation.deletable)
    [deletable addObject:OSTText(uid)];
  // A failed delete is reported for the folders that were SENT (the in-use ones left the request at the confirmation): "Failed to delete the
  // condition folder." for one, "Failed to delete N condition folders." for several, and a folder that turned in use meanwhile is named.
  std::vector<ost_domain::FolderDeleteItem> sentItems;
  for (const ost_domain::FolderDeleteItem &item : items)
    if (std::find(confirmation.deletable.begin(), confirmation.deletable.end(), item.uid) != confirmation.deletable.end())
      sentItems.push_back(item);
  UIAlertController *alert = [UIAlertController alertControllerWithTitle:OSTText(confirmation.dialog.title)
                                                                 message:OSTText(confirmation.dialog.message)
                                                          preferredStyle:UIAlertControllerStyleAlert];
  __weak ConditionFoldersViewController *weakSelf = self;
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Delete"
                                            style:UIAlertActionStyleDestructive
                                          handler:^(UIAlertAction *action) {
                                            [weakSelf _deleteFolderUIDs:deletable items:sentItems];
                                          }]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_deleteFolderUIDs:(NSArray<NSString *> *)folderUIDs items:(const std::vector<ost_domain::FolderDeleteItem> &)items {
  OSTConditionFolderOutcome *outcome = [_delegate conditionFoldersViewController:self deleteFolderUIDs:folderUIDs];
  if (outcome.ok) {
    if (self.editing)
      [self setEditing:NO animated:YES];
    return;
  }
  [self _showFailureOfOutcome:outcome action:ost_store::ConditionFolderAction::Delete items:items];
}

#pragma mark - Move to Folder (picker mode)

- (void)_pickRowAtIndexPath:(NSIndexPath *)indexPath {
  const ost_domain::ConditionFolderRow *row = [self _folderRowAtIndexPath:indexPath];
  const std::string target = row ? row->uid : std::string();
  if (!row && indexPath.row != 0)
    return;  // not a row of the list
  if (target == _currentFolderUID) {
    // already there: nothing to write
    [self.navigationController popViewControllerAnimated:YES];
    return;
  }
  if (![self _mayWrite] || !_pickConditionUID)
    return;
  OSTConditionFolderOutcome *outcome = [_delegate conditionFoldersViewController:self
                                                                moveConditionUID:_pickConditionUID
                                                                     toFolderUID:OSTText(target)];
  if (outcome.ok) {
    [self.navigationController popViewControllerAnimated:YES];
    return;
  }
  [self _showFailureOfOutcome:outcome action:ost_store::ConditionFolderAction::Move items:std::vector<ost_domain::FolderDeleteItem>()];
}

@end
