#import "DatabaseViewController.h"
#import "../Services/ImportService.h"
#import "ViewController/ViewController.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "../Core/Domain/BrowserRules.hpp"
#include "../Core/Domain/DatabaseHierarchy.hpp"
#include "../Core/Domain/FailureText.hpp"
#include "../Core/Domain/RenderConfig.hpp"
#include <string>

@interface DatabaseViewController () <
    UITableViewDataSource, UITableViewDelegate, UIDocumentPickerDelegate>
@property(nonatomic, strong) UITableView *tableView;
@property(nonatomic, strong) NSArray<NSDictionary *> *folderGroups;
@property(nonatomic, strong) NSArray<NSDictionary *> *orphanBids;
@property(nonatomic, strong) NSMutableSet<NSString *> *expandedFolderUIDs;
// Why the project list could not be read (nil when it could): shown once the screen is visible.
@property(nonatomic, copy) NSString *loadError;
@end

// An NSString for text from Core (valid UTF-8; never nil).
static NSString *OSTNS(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

@implementation DatabaseViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Projects";
  self.view.backgroundColor = [UIColor systemGroupedBackgroundColor];

  _expandedFolderUIDs = [NSMutableSet set];

  UIBarButtonItem *importBtn =
      [[UIBarButtonItem alloc] initWithTitle:@"Import"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(_importTapped)];

  UIBarButtonItem *addFolderBtn = [[UIBarButtonItem alloc]
      initWithImage:[UIImage systemImageNamed:@"folder.badge.plus"]
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(_createFolderTapped)];

  UIBarButtonItem *settingsBtn = [[UIBarButtonItem alloc]
      initWithImage:[UIImage systemImageNamed:@"gearshape"]
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(_settingsTapped)];

  self.navigationItem.leftBarButtonItem = settingsBtn;
  self.navigationItem.rightBarButtonItems = @[ importBtn, addFolderBtn ];

  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStyleInsetGrouped];
  _tableView.dataSource = self;
  _tableView.delegate = self;
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  [_tableView registerClass:[UITableViewCell class]
      forCellReuseIdentifier:@"Cell"];
  [self.view addSubview:_tableView];

  [NSLayoutConstraint activateConstraints:@[
    [_tableView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_tableView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [_tableView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_tableView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(_dataChanged)
             name:OSTProjectImportedNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(_dataChanged)
             name:OSTProjectDeletedNotification
           object:nil];

  [self _reloadHierarchy];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.navigationController.navigationBarHidden = NO;
  [self _reloadHierarchy];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self _presentLoadErrorIfVisible];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - Data Loading

- (void)_reloadHierarchy {
  // The store diagnostic is logged by ImportService; the alert shows the one user sentence (EXP-21).
  NSDictionary *hierarchy = [ImportService loadHierarchyWithError:nil];
  // A project list that cannot be read completely is an error, never an empty list (DBP-M02).
  self.loadError = hierarchy ? nil
                             : OSTNS(ost_domain::load_failure_reason(
                                   ost_domain::LoadTarget::Database));
  _folderGroups = hierarchy[@"folders"] ?: @[];
  _orphanBids = hierarchy[@"orphan_bids"] ?: @[];
  [_tableView reloadData];
  [self _presentLoadErrorIfVisible];
}

- (void)_presentLoadErrorIfVisible {
  if (!self.loadError || !self.isViewLoaded || !self.view.window ||
      self.presentedViewController)
    return;
  [self _presentDialogText:ost_domain::load_failure_text(
                               "Projects", std::string([self.loadError UTF8String]))];
}

#pragma mark - Dialogs

- (void)_presentAlertWithTitle:(NSString *)title message:(NSString *)message {
  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_presentDialogText:(const ost_domain::DialogText &)text {
  [self _presentAlertWithTitle:OSTNS(text.title) message:OSTNS(text.message)];
}

// The critical dialog of a browser write that failed (the Windows "Rename Error", "Delete Error", ...): the write was rolled back.
- (void)_presentFailure:(ost_domain::BrowserFailure)failure {
  [self _presentDialogText:ost_domain::browser_failure_text(failure)];
}

// The Windows confirm() (a Yes/No QMessageBox) as an alert: the title and text are the Windows ones, "Cancel" does nothing.
- (void)_confirmWithText:(const ost_domain::DialogText &)text
            confirmTitle:(NSString *)confirmTitle
             destructive:(BOOL)destructive
                 confirm:(void (^)(void))confirm
                  cancel:(void (^)(void))cancel {
  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:OSTNS(text.title)
                                          message:OSTNS(text.message)
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:^(UIAlertAction *a) {
                                            if (cancel)
                                              cancel();
                                          }]];
  [alert addAction:[UIAlertAction
                       actionWithTitle:confirmTitle
                                 style:destructive
                                           ? UIAlertActionStyleDestructive
                                           : UIAlertActionStyleDefault
                               handler:^(UIAlertAction *a) {
                                 confirm();
                               }]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (NSString *)_displayNameOfFolder:(NSDictionary *)group {
  NSString *uid = group[@"uid"];
  NSString *name = group[@"name"];
  return OSTNS(ost_domain::display_name_for_folder(
      std::string([uid UTF8String] ?: ""), std::string([name UTF8String] ?: "")));
}

- (NSString *)_displayNameOfBid:(NSDictionary *)bid {
  NSString *uid = bid[@"uid"];
  NSString *name = bid[@"name"];
  return OSTNS(ost_domain::display_name_for_bid(
      std::string([uid UTF8String] ?: ""), std::string([name UTF8String] ?: "")));
}

- (BOOL)_isDeletedBidsFolderUID:(NSString *)folderUID {
  const char *uid = [folderUID UTF8String];
  return uid && ost_domain::is_deleted_bids_folder_uid(uid);
}

- (void)_dataChanged {
  [self _reloadHierarchy];
}

#pragma mark - Import

- (void)_importTapped {
  UIDocumentPickerViewController *picker =
      [[UIDocumentPickerViewController alloc]
          initForOpeningContentTypes:@[ UTTypeItem ]
                              asCopy:YES];
  picker.delegate = self;
  picker.allowsMultipleSelection = NO;
  [self presentViewController:picker animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller
    didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
  for (NSURL *url in urls) {
    NSError *error = nil;
    NSString *ext = url.pathExtension.lowercaseString;
    BOOL ok;
    if ([ext isEqualToString:@"osp"]) {
      ok = [ImportService importOSPFile:url error:&error];
    } else {
      ok = [ImportService importOSTFile:url error:&error];
    }
    if (!ok) {
      // One user sentence under "Import Error" (the Windows local import, EXP-21); ImportService logs the technical reason. An error
      // without a description is an unexpected failure.
      const ost_domain::DialogText fallback = ost_domain::import_failure_text(
          ost_domain::ImportFailureKind::Unexpected,
          [ext isEqualToString:@"osp"] ? ost_domain::ImportFormat::Osp
                                       : ost_domain::ImportFormat::Ost,
          ost_domain::ImportContext::Picker, std::string());
      UIAlertController *alert = [UIAlertController
          alertControllerWithTitle:OSTNS(ost_domain::IMPORT_ERROR_TITLE)
                           message:error.localizedDescription
                                       ?: OSTNS(fallback.message)
                    preferredStyle:UIAlertControllerStyleAlert];
      [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                                style:UIAlertActionStyleDefault
                                              handler:nil]];
      [self presentViewController:alert animated:YES completion:nil];
    }
  }
}

#pragma mark - Folder Actions

- (void)_createFolderTapped {
  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:@"New Folder"
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addTextFieldWithConfigurationHandler:^(UITextField *tf) {
    tf.placeholder = @"Folder name";
    tf.autocapitalizationType = UITextAutocapitalizationTypeWords;
  }];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  __weak DatabaseViewController *weakSelf = self;
  [alert addAction:[UIAlertAction
                       actionWithTitle:@"Create"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction *a) {
                                 DatabaseViewController *strongSelf = weakSelf;
                                 if (!strongSelf)
                                   return;
                                 const char *typed =
                                     [alert.textFields.firstObject.text UTF8String];
                                 // The name is stripped; a blank one is refused and nothing is written (BRW-19).
                                 std::string name =
                                     ost_domain::normalize_folder_name(typed ?: "");
                                 if (name.empty())
                                   return;
                                 NSString *uid = [ImportService
                                     createFolderWithName:OSTNS(name)];
                                 if (uid) {
                                   [strongSelf->_expandedFolderUIDs addObject:uid];
                                 } else {
                                   [strongSelf _presentFailure:
                                                   ost_domain::BrowserFailure::
                                                       CreateFolder];
                                 }
                                 [strongSelf _reloadHierarchy];
                               }]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_renameFolderAtSection:(NSInteger)section {
  if (section >= (NSInteger)_folderGroups.count)
    return;
  NSDictionary *group = _folderGroups[section];
  NSString *folderUID = group[@"uid"];
  // The system folder Deleted Bids is never renamed (BRW-02).
  if ([self _isDeletedBidsFolderUID:folderUID])
    return;
  // The editor starts with the name the list shows (BRW-21).
  NSString *shownName = [self _displayNameOfFolder:group];

  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:@"Rename Folder"
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addTextFieldWithConfigurationHandler:^(UITextField *tf) {
    tf.text = shownName;
    tf.autocapitalizationType = UITextAutocapitalizationTypeWords;
  }];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  __weak DatabaseViewController *weakSelf = self;
  [alert addAction:[UIAlertAction
                       actionWithTitle:@"Rename"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction *a) {
                                 DatabaseViewController *strongSelf = weakSelf;
                                 if (!strongSelf)
                                   return;
                                 const char *typed =
                                     [alert.textFields.firstObject.text UTF8String];
                                 // Stripped; a blank name and an unchanged name write nothing (BRW-19).
                                 std::string newName;
                                 if (ost_domain::decide_folder_rename(
                                         typed ?: "", [shownName UTF8String],
                                         newName) !=
                                     ost_domain::FolderRenameDecision::Write)
                                   return;
                                 if (![ImportService
                                         renameFolderWithUID:folderUID
                                                      toName:OSTNS(newName)]) {
                                   [strongSelf _presentFailure:
                                                   ost_domain::BrowserFailure::
                                                       RenameFolder];
                                 }
                                 [strongSelf _reloadHierarchy];
                               }]];
  [self presentViewController:alert animated:YES completion:nil];
}

// Windows _delete_projects: only an EMPTY folder is deleted. A folder that still has bids gets the warning and nothing is written;
// an empty one is deleted after the confirmation. The system folder is never offered (BRW-08, BRW-02).
- (void)_deleteFolderAtSection:(NSInteger)section {
  if (section >= (NSInteger)_folderGroups.count)
    return;
  NSDictionary *group = _folderGroups[section];
  NSString *folderUID = group[@"uid"];
  if ([self _isDeletedBidsFolderUID:folderUID])
    return;

  NSString *reason = nil;
  if (![ImportService canDeleteFolderWithUID:folderUID message:&reason]) {
    // Only "the project still has bids" is shown as the store words it; any other reason is the Windows "Delete Error" and is logged
    // (EXP-21).
    NSLog(@"DatabaseViewController: folder %@ cannot be deleted: %@", folderUID,
          reason ?: @"(no reason)");
    [self _presentDialogText:ost_domain::folder_delete_alert(
                                 std::string(reason.UTF8String ?: ""))];
    return;
  }

  __weak DatabaseViewController *weakSelf = self;
  [self _confirmWithText:ost_domain::delete_folders_confirmation(1)
            confirmTitle:@"Delete"
             destructive:YES
                 confirm:^{
                   DatabaseViewController *strongSelf = weakSelf;
                   if (!strongSelf)
                     return;
                   [strongSelf->_expandedFolderUIDs removeObject:folderUID];
                   NSString *deleteError = nil;
                   if (![ImportService deleteFolderWithUID:folderUID
                                                     error:&deleteError]) {
                     NSLog(@"DatabaseViewController: folder %@ was not deleted: %@",
                           folderUID, deleteError ?: @"(no reason)");
                     [strongSelf
                         _presentDialogText:ost_domain::folder_delete_alert(
                                                std::string(deleteError.UTF8String
                                                                ?: ""))];
                   }
                   [strongSelf _reloadHierarchy];
                 }
                  cancel:nil];
}

#pragma mark - Section Helpers

- (NSInteger)_folderCount {
  return (NSInteger)_folderGroups.count;
}

- (BOOL)_hasOrphanSection {
  return _orphanBids.count > 0;
}

- (BOOL)_isOrphanSection:(NSInteger)section {
  return [self _hasOrphanSection] && section == [self _folderCount];
}

- (BOOL)_isFolderExpanded:(NSDictionary *)group {
  return [_expandedFolderUIDs containsObject:group[@"uid"]];
}

- (NSDictionary *)_bidAtIndexPath:(NSIndexPath *)indexPath {
  if ([self _isOrphanSection:indexPath.section]) {
    if (indexPath.row < (NSInteger)_orphanBids.count)
      return _orphanBids[indexPath.row];
  } else if (indexPath.section < [self _folderCount]) {
    NSDictionary *group = _folderGroups[indexPath.section];
    NSArray *bids = group[@"bids"];
    if (indexPath.row < (NSInteger)bids.count)
      return bids[indexPath.row];
  }
  return nil;
}

- (BOOL)_isBidInFolder:(NSIndexPath *)indexPath {
  return indexPath.section < [self _folderCount];
}

- (BOOL)_isDeletedBidsSection:(NSInteger)section {
  if (section < 0 || section >= [self _folderCount])
    return NO;
  // The system folder is the one with UID "1", whatever its name (BRW-02).
  return [self _isDeletedBidsFolderUID:_folderGroups[section][@"uid"]];
}

// Windows _delete_bids: a bid in Deleted Bids is deleted for good, any other bid is moved there, both only after a confirmation; cancelling
// writes nothing (BRW-01). A failed write is reported, not hidden (DBP-11).
- (void)_deleteBid:(NSString *)bidUID atIndexPath:(NSIndexPath *)indexPath {
  BOOL permanent = [self _isDeletedBidsSection:indexPath.section];
  __weak DatabaseViewController *weakSelf = self;
  [self _confirmWithText:permanent ? ost_domain::delete_bids_confirmation(1)
                                   : ost_domain::trash_bids_confirmation(1)
            confirmTitle:permanent ? @"Delete" : @"Move"
             destructive:permanent
                 confirm:^{
                   DatabaseViewController *strongSelf = weakSelf;
                   if (!strongSelf)
                     return;
                   BOOL ok = permanent ? [ImportService
                                             permanentlyDeleteProjectWithUID:bidUID]
                                       : [ImportService deleteProjectWithUID:bidUID];
                   if (!ok) {
                     [strongSelf
                         _presentFailure:permanent
                                             ? ost_domain::BrowserFailure::DeleteBids
                                             : ost_domain::BrowserFailure::TrashBids];
                     [strongSelf _reloadHierarchy];
                   }
                 }
                  cancel:^{
                    // a swiped row stays in its "delete" state until the table reloads
                    [weakSelf.tableView reloadData];
                  }];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  NSInteger count = [self _folderCount];
  if ([self _hasOrphanSection])
    count++;
  if (count == 0)
    count = 1;
  return count;
}

- (NSInteger)tableView:(UITableView *)tableView
    numberOfRowsInSection:(NSInteger)section {
  if (_folderGroups.count == 0 && _orphanBids.count == 0) {
    return 0;
  }

  if ([self _isOrphanSection:section]) {
    return (NSInteger)_orphanBids.count;
  }

  if (section < [self _folderCount]) {
    NSDictionary *group = _folderGroups[section];
    if ([self _isFolderExpanded:group]) {
      NSArray *bids = group[@"bids"];
      return (NSInteger)bids.count;
    }
    return 0;
  }

  return 0;
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell =
      [tableView dequeueReusableCellWithIdentifier:@"Cell"
                                      forIndexPath:indexPath];
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  cell.imageView.image = nil;
  cell.indentationLevel = 0;

  NSDictionary *bid = [self _bidAtIndexPath:indexPath];
  if (bid) {
    // An empty name shows "Bid <uid>" like the Windows tree (BRW-21).
    cell.textLabel.text = [self _displayNameOfBid:bid];
    cell.textLabel.font = [UIFont systemFontOfSize:16];
    cell.textLabel.textColor = [UIColor labelColor];
  }

  return cell;
}

- (BOOL)tableView:(UITableView *)tableView
    canEditRowAtIndexPath:(NSIndexPath *)indexPath {
  return YES;
}

- (void)tableView:(UITableView *)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath *)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete)
    return;
  NSDictionary *bid = [self _bidAtIndexPath:indexPath];
  if (bid) {
    [self _deleteBid:bid[@"uid"] atIndexPath:indexPath];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView
    didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  NSDictionary *bid = [self _bidAtIndexPath:indexPath];
  if (bid) {
    NSString *bidUID = bid[@"uid"];
    // A bid that cannot be read completely, or whose data is inconsistent, is not opened: the open fails with an alert instead of
    // showing an empty plan (DBP-M02, DBP-31, BRW-16, POS-M01); the cause (the missing reference, the takeoff UID, the cycle) is only logged.
    {
      ost_data::ProjectData probe;
      // The diagnostic is logged by ImportService; the alert shows the one user sentence (EXP-21).
      if (![ImportService loadProjectWithUID:bidUID project:probe error:nil]) {
        [self _presentDialogText:ost_domain::load_failure_text(
                                     std::string([[self _displayNameOfBid:bid] UTF8String]),
                                     ost_domain::load_failure_reason(
                                         ost_domain::LoadTarget::Bid))];
        return;
      }
    }
    ViewController *vc = [[ViewController alloc] initWithBidUID:bidUID];
    [self.navigationController pushViewController:vc animated:YES];
  }
}

#pragma mark - Context Menus

- (UIContextMenuConfiguration *)tableView:(UITableView *)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath *)indexPath
                                        point:(CGPoint)point {
  NSDictionary *bid = [self _bidAtIndexPath:indexPath];
  if (!bid)
    return nil;

  NSString *bidUID = bid[@"uid"];
  BOOL inFolder = [self _isBidInFolder:indexPath];
  BOOL inDeletedBids = [self _isDeletedBidsSection:indexPath.section];
  __weak DatabaseViewController *weakSelf = self;

  return [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu *(
                       NSArray<UIMenuElement *> *suggestedActions) {
                     DatabaseViewController *strongSelf = weakSelf;
                     if (!strongSelf)
                       return nil;
                     NSMutableArray<UIMenuElement *> *elements =
                         [NSMutableArray array];

                     if (inDeletedBids) {
                       // Windows: a bid in Deleted Bids cannot be dragged anywhere; the only way out is Restore, which puts it back where
                       // it came from (BRW-09).
                       [elements
                           addObject:
                               [UIAction
                                   actionWithTitle:@"Restore"
                                             image:
                                                 [UIImage
                                                     systemImageNamed:
                                                         @"arrow.uturn.backward"]
                                        identifier:nil
                                           handler:^(UIAction *a) {
                                             if (![ImportService
                                                     restoreBidWithUID:bidUID]) {
                                               [strongSelf
                                                   _presentFailure:
                                                       ost_domain::BrowserFailure::
                                                           RestoreBids];
                                             }
                                             [strongSelf _reloadHierarchy];
                                           }]];
                     } else {
                       if (strongSelf->_folderGroups.count > 0) {
                         NSMutableArray<UIAction *> *folderActions =
                             [NSMutableArray array];
                         for (NSDictionary *group in strongSelf->_folderGroups) {
                           NSString *folderUID = group[@"uid"];
                           // The trash folder is no target of a move (found by its UID, BRW-02).
                           if ([strongSelf _isDeletedBidsFolderUID:folderUID])
                             continue;
                           [folderActions
                               addObject:
                                   [UIAction
                                       actionWithTitle:[strongSelf
                                                           _displayNameOfFolder:group]
                                                 image:
                                                     [UIImage systemImageNamed:
                                                                  @"folder"]
                                            identifier:nil
                                               handler:^(UIAction *a) {
                                                 if (![ImportService
                                                         assignBid:bidUID
                                                          toFolder:folderUID]) {
                                                   [strongSelf
                                                       _presentFailure:
                                                           ost_domain::
                                                               BrowserFailure::
                                                                   MoveBids];
                                                 }
                                                 [strongSelf _reloadHierarchy];
                                               }]];
                         }
                         if (folderActions.count > 0) {
                           UIMenu *moveMenu = [UIMenu
                               menuWithTitle:@"Move to Folder"
                                       image:[UIImage systemImageNamed:@"folder"]
                                  identifier:nil
                                     options:0
                                    children:folderActions];
                           [elements addObject:moveMenu];
                         }
                       }

                       if (inFolder) {
                         [elements
                             addObject:
                                 [UIAction
                                     actionWithTitle:@"Remove from Folder"
                                               image:
                                                   [UIImage
                                                       systemImageNamed:
                                                           @"folder.badge.minus"]
                                          identifier:nil
                                             handler:^(UIAction *a) {
                                               if (![ImportService
                                                       removeBidFromFolder:bidUID]) {
                                                 [strongSelf
                                                     _presentFailure:
                                                         ost_domain::
                                                             BrowserFailure::
                                                                 MoveBids];
                                               }
                                               [strongSelf _reloadHierarchy];
                                             }]];
                       }
                     }

                     UIAction *deleteAction = [UIAction
                         actionWithTitle:inDeletedBids ? @"Permanently Delete"
                                                       : @"Delete"
                                   image:[UIImage systemImageNamed:@"trash"]
                              identifier:nil
                                 handler:^(UIAction *a) {
                                   [strongSelf _deleteBid:bidUID
                                              atIndexPath:indexPath];
                                 }];
                     deleteAction.attributes =
                         UIMenuElementAttributesDestructive;
                     [elements addObject:deleteAction];

                     return [UIMenu menuWithTitle:@"" children:elements];
                   }];
}

#pragma mark - Section Headers

- (UIView *)tableView:(UITableView *)tableView
    viewForHeaderInSection:(NSInteger)section {
  if (_folderGroups.count == 0 && _orphanBids.count == 0)
    return nil;
  if ([self _isOrphanSection:section])
    return nil;
  if (section >= [self _folderCount])
    return nil;

  NSDictionary *group = _folderGroups[section];
  BOOL expanded = [self _isFolderExpanded:group];
  NSArray *bids = group[@"bids"];
  // An empty name shows "Project <uid>" like the Windows tree (BRW-21).
  NSString *name = [self _displayNameOfFolder:group];

  NSString *title = [NSString stringWithFormat:@"  %@ %@ (%lu)",
                                               expanded ? @"\u25BE" : @"\u25B8",
                                               name, (unsigned long)bids.count];

  UIButton *headerButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [headerButton setTitle:title forState:UIControlStateNormal];
  headerButton.contentHorizontalAlignment =
      UIControlContentHorizontalAlignmentLeft;
  headerButton.titleLabel.font = [UIFont systemFontOfSize:14
                                                   weight:UIFontWeightSemibold];
  [headerButton setTitleColor:[UIColor labelColor]
                     forState:UIControlStateNormal];
  headerButton.tag = section;
  [headerButton addTarget:self
                   action:@selector(_toggleSection:)
         forControlEvents:UIControlEventTouchUpInside];

  // Rename and Delete are the only actions of the folder menu and neither applies to the system folder Deleted Bids (BRW-02).
  if (![self _isDeletedBidsFolderUID:group[@"uid"]]) {
    UILongPressGestureRecognizer *longPress =
        [[UILongPressGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(_folderHeaderLongPress:)];
    [headerButton addGestureRecognizer:longPress];
  }

  return headerButton;
}

- (NSString *)tableView:(UITableView *)tableView
    titleForHeaderInSection:(NSInteger)section {
  if (_folderGroups.count == 0 && _orphanBids.count == 0)
    return nil;
  if ([self _isOrphanSection:section]) {
    return @"Unassigned Projects";
  }
  return nil;
}

- (NSString *)tableView:(UITableView *)tableView
    titleForFooterInSection:(NSInteger)section {
  if (_folderGroups.count == 0 && _orphanBids.count == 0 && section == 0) {
    return @"No projects imported yet.\nTap Import to add an .ost or .osp "
           @"file.";
  }
  return nil;
}

- (CGFloat)tableView:(UITableView *)tableView
    heightForHeaderInSection:(NSInteger)section {
  if (_folderGroups.count == 0 && _orphanBids.count == 0)
    return 0;
  return 44;
}

#pragma mark - Section Toggle & Folder Actions

- (void)_toggleSection:(UIButton *)sender {
  NSInteger section = sender.tag;
  if (section >= [self _folderCount])
    return;

  NSDictionary *group = _folderGroups[section];
  NSString *uid = group[@"uid"];

  if ([_expandedFolderUIDs containsObject:uid]) {
    [_expandedFolderUIDs removeObject:uid];
  } else {
    [_expandedFolderUIDs addObject:uid];
  }

  [_tableView reloadSections:[NSIndexSet indexSetWithIndex:section]
            withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (void)_folderHeaderLongPress:(UILongPressGestureRecognizer *)gesture {
  if (gesture.state != UIGestureRecognizerStateBegan)
    return;

  UIButton *button = (UIButton *)gesture.view;
  NSInteger section = button.tag;
  if (section >= [self _folderCount])
    return;

  NSDictionary *group = _folderGroups[section];
  // The system folder Deleted Bids is neither renamed nor deleted (Windows: Rename is disabled for UID 1, the writer refuses to delete
  // it): no menu (BRW-02).
  if ([self _isDeletedBidsFolderUID:group[@"uid"]])
    return;
  NSString *folderName = [self _displayNameOfFolder:group];

  UIAlertController *sheet = [UIAlertController
      alertControllerWithTitle:folderName
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  [sheet addAction:[UIAlertAction actionWithTitle:@"Rename"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction *a) {
                                            [self
                                                _renameFolderAtSection:section];
                                          }]];

  [sheet addAction:[UIAlertAction actionWithTitle:@"Delete Folder"
                                            style:UIAlertActionStyleDestructive
                                          handler:^(UIAlertAction *a) {
                                            [self
                                                _deleteFolderAtSection:section];
                                          }]];

  [sheet addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];

  if (sheet.popoverPresentationController) {
    sheet.popoverPresentationController.sourceView = button;
    sheet.popoverPresentationController.sourceRect = button.bounds;
  }

  [self presentViewController:sheet animated:YES completion:nil];
}

#pragma mark - Settings

- (void)_settingsTapped {
  NSDictionary *cfg = [ImportService loadRenderConfig];
  // The display modes are the Windows ones and the default is Original (SET-01); a stored mode outside them shows as the default.
  NSString *currentMode =
      cfg[@"color_mode"] ?: OSTNS(ost_domain::RenderConfig::DEFAULT_MODE);
  BOOL grayscale = [cfg[@"grayscale_enabled"] boolValue];
  __weak DatabaseViewController *weakSelf = self;

  UIAlertController *sheet = [UIAlertController
      alertControllerWithTitle:@"Visualization Settings"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  // The change is announced only when it was stored (DBP-11): a failed save is reported.
  void (^save)(NSString *, BOOL) = ^(NSString *mode, BOOL gray) {
    if (![ImportService saveRenderConfigWithColorMode:mode
                                     grayscaleEnabled:gray]) {
      [weakSelf
          _presentFailure:ost_domain::BrowserFailure::SaveSettings];
    }
  };

  NSArray *modes = @[
    OSTNS(ost_domain::RenderConfig::MODE_SOLID),
    OSTNS(ost_domain::RenderConfig::MODE_ORIGINAL),
    OSTNS(ost_domain::RenderConfig::MODE_TRANSPARENT)
  ];
  for (NSString *mode in modes) {
    NSString *label = [mode isEqualToString:currentMode]
                          ? [NSString stringWithFormat:@"\u2713 %@", mode]
                          : [NSString stringWithFormat:@"   %@", mode];
    [sheet addAction:[UIAlertAction actionWithTitle:label
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction *a) {
                                              save(mode, grayscale);
                                            }]];
  }

  NSString *grayLabel = grayscale ? @"\u2713 Grayscale" : @"   Grayscale";
  [sheet addAction:[UIAlertAction actionWithTitle:grayLabel
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction *a) {
                                            save(currentMode, !grayscale);
                                          }]];

  [sheet addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];

  if (sheet.popoverPresentationController) {
    sheet.popoverPresentationController.barButtonItem =
        self.navigationItem.leftBarButtonItem;
  }

  [self presentViewController:sheet animated:YES completion:nil];
}

@end
