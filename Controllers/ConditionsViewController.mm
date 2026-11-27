#import "ConditionsViewController.h"
#include "../Core/Domain/Condition.hpp"
#include "../Core/Domain/ConditionCreate.hpp"
#include "../Core/Domain/ConditionFolderUi.hpp"
#include "../Core/Domain/ConditionTree.hpp"
#include "../Core/Domain/QuantityDisplay.hpp"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

struct ConditionRow {
  std::string uid;
  std::string name;
  int ref_no;
  int color_fill;
  int uom1;
  int uom2;
  int uom3;
  // The layer of the condition is shown. A condition on a hidden layer is greyed, cannot be placed and has no Edit button (COND-PLACE-01).
  bool placeable;
};

// A section of the list: a condition-type group (titled with the folders around it) or a folder with nothing in it.
struct ConditionSection {
  std::string title;
  std::vector<ConditionRow> rows;
  // The folder the section sits in: the uid and the name of the nearest folder at or above its node of the Conditions tree
  // (ost_domain::folder_context_uid, Windows _resolve_folder_context_uid); "" for a section at the top level. "New Condition" from a section
  // creates in this folder.
  std::string folder_uid;
  std::string folder_name;
};

static NSString *OSTText(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

@interface ConditionsViewController () <UITableViewDataSource,
                                        UITableViewDelegate>
@end

@implementation ConditionsViewController {
  UITableView *_tableView;
  std::vector<ConditionSection> _sections;
  std::unordered_map<std::string, ost_domain::QuantityResult> _quantities;
  NSString *_selectedUID;
  BOOL _blockSelectionSignal;
  BOOL _needsScrollToSelection;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Conditions";
  self.view.backgroundColor = [UIColor systemGroupedBackgroundColor];

  UIBarButtonItem *done = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(_dismiss)];
  // "+" = New Condition (in the folder of the selected condition), "Folders" = the folder manager (plan 4.9: no new toolbar button, these two
  // live in the bar of the sheet). On iPad the form is a centred sheet and the manager is pushed inside this sheet: no popover is opened here.
  UIBarButtonItem *add = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
                           target:self
                           action:@selector(_addTapped:)];
  self.navigationItem.rightBarButtonItems = @[ done, add ];
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:OSTText(ost_domain::kFoldersButtonTitle)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(_foldersTapped:)];

  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStyleInsetGrouped];
  _tableView.dataSource = self;
  _tableView.delegate = self;
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_tableView];

  UILongPressGestureRecognizer *longPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(_handleLongPress:)];
  [_tableView addGestureRecognizer:longPress];

  [NSLayoutConstraint activateConstraints:@[
    [_tableView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_tableView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [_tableView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_tableView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if (_selectedUID) {
    _needsScrollToSelection = YES;
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (_needsScrollToSelection && _selectedUID) {
    _needsScrollToSelection = NO;
    [self selectConditionWithUID:_selectedUID animated:NO];
  }
}

- (void)_dismiss {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - New Condition and Folders

// Windows _on_new_clicked (conditions_sidebar.py:1392-1395): the new condition goes into the folder of the current item, the top level when
// there is none. Only conditions are selectable here, so that is the folder of the section of the selected condition.
- (void)_addTapped:(UIBarButtonItem *)sender {
  [self _requestNewConditionInFolderUID:[self _folderUIDOfSelectedCondition]];
}

- (void)_foldersTapped:(UIBarButtonItem *)sender {
  if (_delegate && [_delegate respondsToSelector:@selector(conditionsViewControllerDidRequestFolders:)])
    [_delegate conditionsViewControllerDidRequestFolders:self];
}

- (void)_requestNewConditionInFolderUID:(NSString *)folderUID {
  if (_delegate && [_delegate respondsToSelector:@selector(conditionsViewController:didRequestNewConditionInFolderUID:)])
    [_delegate conditionsViewController:self didRequestNewConditionInFolderUID:folderUID ?: @""];
}

// The folder of the section that holds the selected condition ("" = the top level, and when nothing (or a condition that is gone) is selected).
- (NSString *)_folderUIDOfSelectedCondition {
  if (!_selectedUID)
    return @"";
  const std::string target = std::string(_selectedUID.UTF8String ?: "");
  for (const ConditionSection &section : _sections)
    for (const ConditionRow &row : section.rows)
      if (row.uid == target)
        return OSTText(section.folder_uid);
  return @"";
}

#pragma mark - Data Loading

- (void)loadConditionsFromProject:(const ost_data::ProjectData &)project {
  _sections.clear();

  // The rows of the Windows tree (COND-FOLDER-01, COND-LIST-01): folders nest through ParentUID, a folderless condition sits at the
  // root, empty folders stay visible, folders and type groups sort by name, the conditions of a group by reference number.
  const std::vector<ost_domain::ConditionTreeNode> nodes =
      ost_domain::build_condition_tree(project.conditions,
                                       project.condition_folders,
                                       project.cdn_types);

  // The nodes are a pre-order list, this screen is a flat table: every type group is a section whose title names the folders around
  // it (a folder with content is visible through that path), a folder with nothing in it is a section of its own with no rows.
  std::vector<std::string> folder_path;
  auto path_title = [&folder_path]() {
    std::string title;
    for (const std::string &name : folder_path) {
      if (!title.empty())
        title += " > ";
      title += name;
    }
    return title;
  };

  for (size_t i = 0; i < nodes.size(); i++) {
    const ost_domain::ConditionTreeNode &node = nodes[i];
    switch (node.kind) {
    case ost_domain::ConditionTreeNode::Kind::Folder: {
      folder_path.resize(std::min(folder_path.size(), (size_t)node.depth));
      folder_path.push_back(node.name);
      const bool is_empty =
          (i + 1 >= nodes.size() || nodes[i + 1].depth <= node.depth);
      if (is_empty) {
        ConditionSection section;
        section.title = path_title();
        section.folder_uid = ost_domain::folder_context_uid(nodes, i);
        section.folder_name = node.name;
        _sections.push_back(std::move(section));
      }
      break;
    }
    case ost_domain::ConditionTreeNode::Kind::TypeGroup: {
      folder_path.resize(std::min(folder_path.size(), (size_t)node.depth));
      ConditionSection section;
      section.title = folder_path.empty()
                          ? node.name
                          : path_title() + " — " + node.name;
      // the folder the group sits in: the one rule of Core, over the tree nodes (the folder list of this load is the project's)
      section.folder_uid = ost_domain::folder_context_uid(nodes, i);
      if (!section.folder_uid.empty()) {
        auto folder = project.condition_folders.find(section.folder_uid);
        if (folder != project.condition_folders.end())
          section.folder_name = folder->second.name;
      }
      _sections.push_back(std::move(section));
      break;
    }
    case ost_domain::ConditionTreeNode::Kind::Condition: {
      auto found = project.conditions.find(node.uid);
      if (found == project.conditions.end() || _sections.empty())
        break;
      const ost_domain::Condition &cond = found->second;
      ConditionRow row;
      row.uid = cond.uid;
      row.name = cond.name;
      row.ref_no = cond.ref_no;
      row.color_fill = cond.color_fill;
      row.uom1 = cond.uom1;
      row.uom2 = cond.uom2;
      row.uom3 = cond.uom3;
      row.placeable = project.is_condition_visible(cond);
      _sections.back().rows.push_back(std::move(row));
      break;
    }
    }
  }

  [_tableView reloadData];
}

- (void)updateQuantities:
    (const std::unordered_map<std::string, ost_domain::QuantityResult> &)
        quantities {
  _quantities = quantities;
  [_tableView reloadData];
}

#pragma mark - Selection

- (NSString *)selectedConditionUID {
  return _selectedUID;
}

- (void)selectConditionWithUID:(NSString *)uid animated:(BOOL)animated {
  _blockSelectionSignal = YES;
  _selectedUID = [uid copy];

  std::string target = uid ? std::string(uid.UTF8String) : "";
  NSIndexPath *targetPath = nil;

  for (size_t s = 0; s < _sections.size(); s++) {
    for (size_t r = 0; r < _sections[s].rows.size(); r++) {
      if (_sections[s].rows[r].uid == target) {
        targetPath = [NSIndexPath indexPathForRow:(NSInteger)r
                                        inSection:(NSInteger)s];
        break;
      }
    }
    if (targetPath)
      break;
  }

  [_tableView reloadData];

  if (targetPath) {
    [_tableView selectRowAtIndexPath:targetPath
                            animated:NO
                      scrollPosition:UITableViewScrollPositionNone];
    [_tableView scrollToRowAtIndexPath:targetPath
                      atScrollPosition:UITableViewScrollPositionMiddle
                              animated:animated];
  }

  _blockSelectionSignal = NO;
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  return (NSInteger)_sections.size();
}

- (NSInteger)tableView:(UITableView *)tableView
    numberOfRowsInSection:(NSInteger)section {
  if ((size_t)section >= _sections.size())
    return 0;
  return (NSInteger)_sections[section].rows.size();
}

- (NSString *)tableView:(UITableView *)tableView
    titleForHeaderInSection:(NSInteger)section {
  if ((size_t)section >= _sections.size())
    return nil;
  return [NSString stringWithUTF8String:_sections[section].title.c_str()];
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell =
      [tableView dequeueReusableCellWithIdentifier:@"CondCell"];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                  reuseIdentifier:@"CondCell"];
  }
  if ((size_t)indexPath.section >= _sections.size())
    return cell;
  const auto &sec = _sections[indexPath.section];
  if ((size_t)indexPath.row >= sec.rows.size())
    return cell;
  const auto &row = sec.rows[indexPath.row];

  // "No.  Name": the full name of the condition, the elevation suffix included (QTY-23)
  NSString *refStr = [NSString stringWithFormat:@"%d", row.ref_no];
  NSString *nameStr = [NSString stringWithUTF8String:row.name.c_str()];
  cell.textLabel.text = [NSString stringWithFormat:@"%@  %@", refStr, nameStr];
  cell.textLabel.font = [UIFont systemFontOfSize:15];

  // The three quantity texts of Windows update_quantities: "12.35 m", "1,234 LF", "0 EA" for a quantity of 0, and the numbers of a
  // condition without a computed quantity are (0, 0, 0). A slot whose unit has no label and no quantity has no text (QTY-12, QTY-22).
  auto qit = _quantities.find(row.uid);
  const std::array<std::string, 3> texts =
      ost_domain::format_condition_quantity_texts(
          qit != _quantities.end() ? &qit->second : nullptr, row.uom1,
          row.uom2, row.uom3);
  NSMutableArray *parts = [NSMutableArray array];
  for (const std::string &text : texts) {
    if (!text.empty())
      [parts addObject:[NSString stringWithUTF8String:text.c_str()]];
  }
  cell.detailTextLabel.text =
      parts.count ? [parts componentsJoinedByString:@"  ·  "] : nil;
  cell.detailTextLabel.font = [UIFont systemFontOfSize:12];

  // A condition on a hidden layer is greyed out (COND-PLACE-01)
  cell.textLabel.textColor =
      row.placeable ? [UIColor labelColor] : [UIColor tertiaryLabelColor];
  cell.detailTextLabel.textColor = row.placeable
                                       ? [UIColor secondaryLabelColor]
                                       : [UIColor tertiaryLabelColor];

  // ColorFill is read like Windows int_to_hex: the low 24 bits, a negative value is not grey (COL-01)
  float r, g, b;
  ost_domain::Condition::rgb_from_color_fill(row.color_fill, r, g, b);
  UIColor *color = [UIColor colorWithRed:r green:g blue:b alpha:1.0];

  UIGraphicsBeginImageContextWithOptions(CGSizeMake(12, 12), NO, 0);
  [color setFill];
  UIRectFill(CGRectMake(0, 0, 12, 12));
  UIImage *swatch = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  cell.imageView.image = swatch;

  BOOL isSelected =
      (_selectedUID &&
       [_selectedUID
           isEqualToString:[NSString stringWithUTF8String:row.uid.c_str()]]);
  // the Edit button is not offered for a condition on a hidden layer (Windows disables Edit for it)
  if (!row.placeable) {
    cell.accessoryType = UITableViewCellAccessoryNone;
  } else {
    cell.accessoryType = isSelected
                             ? UITableViewCellAccessoryDetailDisclosureButton
                             : UITableViewCellAccessoryDetailButton;
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView
    didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  if ((size_t)indexPath.section >= _sections.size())
    return;
  const auto &sec = _sections[indexPath.section];
  if ((size_t)indexPath.row >= sec.rows.size())
    return;

  const auto &row = sec.rows[indexPath.row];
  _selectedUID = [NSString stringWithUTF8String:row.uid.c_str()];

  [tableView reloadData];

  [tableView selectRowAtIndexPath:indexPath
                         animated:NO
                   scrollPosition:UITableViewScrollPositionNone];

  if (!_blockSelectionSignal && _delegate) {
    [_delegate conditionsViewController:self
                  didSelectConditionUID:_selectedUID];
  }
}

- (void)tableView:(UITableView *)tableView
    accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)indexPath {
  if ((size_t)indexPath.section >= _sections.size())
    return;
  const auto &sec = _sections[indexPath.section];
  if ((size_t)indexPath.row >= sec.rows.size())
    return;

  const auto &row = sec.rows[indexPath.row];
  if (!row.placeable)
    return;
  NSString *uid = [NSString stringWithUTF8String:row.uid.c_str()];

  if (_delegate &&
      [_delegate respondsToSelector:@selector(conditionsViewController:
                                              didRequestEditConditionUID:)]) {
    [_delegate conditionsViewController:self didRequestEditConditionUID:uid];
  }
}

// A long press on a section HEADER (plan 5.1, N-M1-14): the only item is "New Condition in <folder>" for the folder the section sits in (an empty
// folder is a section with no rows, so its header is the only thing to press). The sheet is anchored on the header (iPad popover, plan 4.3).
- (void)_presentHeaderSheetAtPoint:(CGPoint)point {
  for (size_t s = 0; s < _sections.size(); s++) {
    const CGRect rect = [_tableView rectForHeaderInSection:(NSInteger)s];
    if (CGRectIsEmpty(rect) || !CGRectContainsPoint(rect, point))
      continue;
    const ConditionSection &section = _sections[s];
    NSString *folderUID = OSTText(section.folder_uid);
    UIAlertController *sheet = [UIAlertController
        alertControllerWithTitle:OSTText(section.title)
                         message:nil
                  preferredStyle:UIAlertControllerStyleActionSheet];
    __weak ConditionsViewController *weakSelf = self;
    [sheet addAction:[UIAlertAction
                         actionWithTitle:OSTText(ost_domain::new_condition_action_title(section.folder_uid, section.folder_name))
                                   style:UIAlertActionStyleDefault
                                 handler:^(UIAlertAction *action) {
                                   [weakSelf _requestNewConditionInFolderUID:folderUID];
                                 }]];
    [sheet addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                              style:UIAlertActionStyleCancel
                                            handler:nil]];
    if (sheet.popoverPresentationController) {
      sheet.popoverPresentationController.sourceView = _tableView;
      sheet.popoverPresentationController.sourceRect = rect;
    }
    [self presentViewController:sheet animated:YES completion:nil];
    return;
  }
}

- (void)_handleLongPress:(UILongPressGestureRecognizer *)gesture {
  if (gesture.state != UIGestureRecognizerStateBegan)
    return;

  CGPoint point = [gesture locationInView:_tableView];
  NSIndexPath *indexPath = [_tableView indexPathForRowAtPoint:point];
  if (!indexPath) {
    // not on a row: a section header (an empty folder is a header with no rows) offers New Condition in its folder
    [self _presentHeaderSheetAtPoint:point];
    return;
  }
  if ((size_t)indexPath.section >= _sections.size())
    return;
  const auto &sec = _sections[indexPath.section];
  if ((size_t)indexPath.row >= sec.rows.size())
    return;

  const auto &row = sec.rows[indexPath.row];
  NSString *uid = [NSString stringWithUTF8String:row.uid.c_str()];
  NSString *name = [NSString stringWithUTF8String:row.name.c_str()];

  UIAlertController *sheet = [UIAlertController
      alertControllerWithTitle:name
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  [sheet addAction:
             [UIAlertAction
                 actionWithTitle:@"Duplicate"
                           style:UIAlertActionStyleDefault
                         handler:^(UIAlertAction *action) {
                           if (self->_delegate &&
                               [self->_delegate
                                   respondsToSelector:
                                       @selector(
                                           conditionsViewController:
                                           didRequestDuplicateConditionUID:)]) {
                             [self->_delegate conditionsViewController:self
                                       didRequestDuplicateConditionUID:uid];
                           }
                         }]];

  // "New Condition in <folder>" (the folder of the pressed row) and "Move to Folder..." (plan 5.1, 5.2; the delegate checks the bid lock first).
  // Both sit before Delete so the destructive item stays last.
  NSString *rowFolderUID = OSTText(sec.folder_uid);
  __weak ConditionsViewController *weakSelf = self;
  [sheet addAction:[UIAlertAction
                       actionWithTitle:OSTText(ost_domain::new_condition_action_title(sec.folder_uid, sec.folder_name))
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction *action) {
                                 [weakSelf _requestNewConditionInFolderUID:rowFolderUID];
                               }]];
  [sheet addAction:[UIAlertAction
                       actionWithTitle:OSTText(ost_domain::kMoveToFolderAction)
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction *action) {
                                 ConditionsViewController *strongSelf = weakSelf;
                                 if (strongSelf && strongSelf->_delegate &&
                                     [strongSelf->_delegate respondsToSelector:@selector(conditionsViewController:
                                                                                         didRequestMoveConditionUID:)])
                                   [strongSelf->_delegate conditionsViewController:strongSelf
                                                        didRequestMoveConditionUID:uid];
                               }]];

  [sheet
      addAction:
          [UIAlertAction
              actionWithTitle:@"Delete"
                        style:UIAlertActionStyleDestructive
                      handler:^(UIAlertAction *action) {
                        UIAlertController *confirm = [UIAlertController
                            alertControllerWithTitle:
                                [NSString
                                    stringWithFormat:@"Delete \"%@\"?", name]
                                             message:
                                                 @"Deleting a condition will "
                                                 @"delete all its takeoffs "
                                                 @"on all pages within this "
                                                 @"bid.\n\nThis cannot be "
                                                 @"undone."
                                      preferredStyle:
                                          UIAlertControllerStyleAlert];
                        [confirm
                            addAction:
                                [UIAlertAction
                                    actionWithTitle:@"Cancel"
                                              style:UIAlertActionStyleCancel
                                            handler:nil]];
                        [confirm
                            addAction:
                                [UIAlertAction
                                    actionWithTitle:@"Delete"
                                              style:
                                                  UIAlertActionStyleDestructive
                                            handler:^(UIAlertAction *a) {
                                              if (self->_delegate &&
                                                  [self->_delegate
                                                      respondsToSelector:
                                                          @selector(
                                                              conditionsViewController:
                                                              didRequestDeleteConditionUID:)]) {
                                                [self->_delegate
                                                        conditionsViewController:
                                                            self
                                                    didRequestDeleteConditionUID:
                                                        uid];
                                              }
                                            }]];
                        [self presentViewController:confirm
                                           animated:YES
                                         completion:nil];
                      }]];

  [sheet addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];

  // iPad popover anchor
  if (sheet.popoverPresentationController) {
    UITableViewCell *cell = [_tableView cellForRowAtIndexPath:indexPath];
    sheet.popoverPresentationController.sourceView = cell ?: _tableView;
    sheet.popoverPresentationController.sourceRect =
        cell ? cell.bounds : CGRectMake(point.x, point.y, 1, 1);
  }

  [self presentViewController:sheet animated:YES completion:nil];
}

@end
