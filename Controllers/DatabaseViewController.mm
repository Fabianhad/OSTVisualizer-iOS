#import "DatabaseViewController.h"
#import "../Services/ImportService.h"
#import "ViewController/ViewController.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface DatabaseViewController () <
    UITableViewDataSource, UITableViewDelegate, UIDocumentPickerDelegate>
@property(nonatomic, strong) UITableView *tableView;
@property(nonatomic, strong) NSArray<NSDictionary *> *folderGroups;
@property(nonatomic, strong) NSArray<NSDictionary *> *orphanBids;
@property(nonatomic, strong) NSMutableSet<NSString *> *expandedFolderUIDs;
@end

static NSString *const OSTDeletedBidsFolderName = @"Deleted Bids";

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

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - Data Loading

- (void)_reloadHierarchy {
  NSDictionary *hierarchy = [ImportService loadHierarchy];
  _folderGroups = hierarchy[@"folders"] ?: @[];
  _orphanBids = hierarchy[@"orphan_bids"] ?: @[];
  [_tableView reloadData];
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
      UIAlertController *alert = [UIAlertController
          alertControllerWithTitle:@"Import Failed"
                           message:error.localizedDescription
                                       ?: @"Unknown error"
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
  [alert addAction:[UIAlertAction
                       actionWithTitle:@"Create"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction *a) {
                                 NSString *name =
                                     alert.textFields.firstObject.text;
                                 if (name.length > 0) {
                                   NSString *uid = [ImportService
                                       createFolderWithName:name];
                                   if (uid) {
                                     [self->_expandedFolderUIDs addObject:uid];
                                   }
                                   [self _reloadHierarchy];
                                 }
                               }]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_renameFolderAtSection:(NSInteger)section {
  if (section >= (NSInteger)_folderGroups.count)
    return;
  NSDictionary *group = _folderGroups[section];
  NSString *folderUID = group[@"uid"];
  NSString *currentName = group[@"name"];

  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:@"Rename Folder"
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addTextFieldWithConfigurationHandler:^(UITextField *tf) {
    tf.text = currentName;
    tf.autocapitalizationType = UITextAutocapitalizationTypeWords;
  }];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [alert addAction:[UIAlertAction
                       actionWithTitle:@"Rename"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction *a) {
                                 NSString *newName =
                                     alert.textFields.firstObject.text;
                                 if (newName.length > 0) {
                                   [ImportService renameFolderWithUID:folderUID
                                                               toName:newName];
                                   [self _reloadHierarchy];
                                 }
                               }]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_deleteFolderAtSection:(NSInteger)section {
  if (section >= (NSInteger)_folderGroups.count)
    return;
  NSDictionary *group = _folderGroups[section];
  NSString *folderUID = group[@"uid"];
  NSString *name = group[@"name"] ?: @"this folder";
  NSArray *bids = group[@"bids"];

  NSString *message =
      bids.count > 0
          ? [NSString
                stringWithFormat:@"%lu project(s) will become unassigned.",
                                 (unsigned long)bids.count]
          : nil;

  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:[NSString
                                   stringWithFormat:@"Delete \"%@\"?", name]
                       message:message
                preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Delete"
                                            style:UIAlertActionStyleDestructive
                                          handler:^(UIAlertAction *a) {
                                            [self->_expandedFolderUIDs
                                                removeObject:folderUID];
                                            [ImportService
                                                deleteFolderWithUID:folderUID];
                                            [self _reloadHierarchy];
                                          }]];
  [self presentViewController:alert animated:YES completion:nil];
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
  NSDictionary *group = _folderGroups[section];
  return [group[@"name"] isEqualToString:OSTDeletedBidsFolderName];
}

- (void)_deleteBid:(NSString *)bidUID atIndexPath:(NSIndexPath *)indexPath {
  if ([self _isDeletedBidsSection:indexPath.section]) {
    [ImportService permanentlyDeleteProjectWithUID:bidUID];
  } else {
    [ImportService deleteProjectWithUID:bidUID];
  }
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
    NSString *name = bid[@"name"];
    cell.textLabel.text = (name.length > 0) ? name : @"Untitled Bid";
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

  return [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu *(
                       NSArray<UIMenuElement *> *suggestedActions) {
                     NSMutableArray<UIMenuElement *> *elements =
                         [NSMutableArray array];

                     if (self->_folderGroups.count > 0) {
                       NSMutableArray<UIAction *> *folderActions =
                           [NSMutableArray array];
                       for (NSDictionary *group in self->_folderGroups) {
                         NSString *folderUID = group[@"uid"];
                         NSString *folderName = group[@"name"] ?: @"Untitled";
                         if ([folderName isEqualToString:OSTDeletedBidsFolderName])
                           continue;
                         [folderActions
                             addObject:[UIAction
                                           actionWithTitle:folderName
                                                     image:[UIImage
                                                               systemImageNamed:
                                                                   @"folder"]
                                                identifier:nil
                                                   handler:^(UIAction *a) {
                                                     [ImportService
                                                         assignBid:bidUID
                                                          toFolder:folderUID];
                                                     [self _reloadHierarchy];
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
                                   actionWithTitle:inDeletedBids
                                                       ? @"Restore as Unassigned"
                                                       : @"Remove from Folder"
                                             image:
                                                 [UIImage
                                                     systemImageNamed:
                                                         @"folder.badge.minus"]
                                        identifier:nil
                                           handler:^(UIAction *a) {
                                             [ImportService
                                                 removeBidFromFolder:bidUID];
                                             [self _reloadHierarchy];
                                           }]];
                     }

                     UIAction *deleteAction = [UIAction
                         actionWithTitle:inDeletedBids ? @"Permanently Delete"
                                                       : @"Delete"
                                   image:[UIImage systemImageNamed:@"trash"]
                              identifier:nil
                                 handler:^(UIAction *a) {
                                   [self _deleteBid:bidUID
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
  NSString *name = group[@"name"];
  if (!name || name.length == 0)
    name = @"Untitled Folder";

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

  UILongPressGestureRecognizer *longPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(_folderHeaderLongPress:)];
  [headerButton addGestureRecognizer:longPress];

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
  NSString *folderName = group[@"name"] ?: @"Folder";

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
  NSString *currentMode = cfg[@"color_mode"] ?: @"Solid";
  BOOL grayscale = [cfg[@"grayscale_enabled"] boolValue];

  UIAlertController *sheet = [UIAlertController
      alertControllerWithTitle:@"Visualization Settings"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  NSArray *modes = @[ @"Solid", @"OST", @"Transparent" ];
  for (NSString *mode in modes) {
    NSString *label = [mode isEqualToString:currentMode]
                          ? [NSString stringWithFormat:@"\u2713 %@", mode]
                          : [NSString stringWithFormat:@"   %@", mode];
    [sheet addAction:[UIAlertAction
                         actionWithTitle:label
                                   style:UIAlertActionStyleDefault
                                 handler:^(UIAlertAction *a) {
                                   [ImportService
                                       saveRenderConfigWithColorMode:mode
                                                    grayscaleEnabled:grayscale];
                                 }]];
  }

  NSString *grayLabel = grayscale ? @"\u2713 Grayscale" : @"   Grayscale";
  [sheet addAction:[UIAlertAction
                       actionWithTitle:grayLabel
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction *a) {
                                 [ImportService
                                     saveRenderConfigWithColorMode:currentMode
                                                  grayscaleEnabled:!grayscale];
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
