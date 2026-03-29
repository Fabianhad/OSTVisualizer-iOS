#import "ConditionsViewController.h"
#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

static std::string stripElevation(const std::string &name) {
  auto posT = name.rfind(" @T");
  auto posB = name.rfind(" @B");
  auto pos = std::string::npos;
  if (posT != std::string::npos && posB != std::string::npos)
    pos = std::max(posT, posB);
  else if (posT != std::string::npos)
    pos = posT;
  else if (posB != std::string::npos)
    pos = posB;
  if (pos == std::string::npos) {
    auto posAt = name.rfind(" @ ");
    if (posAt == std::string::npos)
      posAt = name.rfind(" @");
    if (posAt != std::string::npos)
      pos = posAt;
  }
  if (pos == std::string::npos)
    return name;
  return name.substr(0, pos);
}

struct ConditionRow {
  std::string uid;
  std::string name;
  int ref_no;
  int color_fill;
  int uom1;
  int uom2;
  int uom3;
};

struct ConditionSection {
  std::string folder_name;
  std::string cdn_type_name;
  std::vector<ConditionRow> rows;
};

static NSString *formatQty(double val, int uomCode) {
  if (val == 0.0)
    return nil;
  std::string label = ost_domain::get_uom_label(uomCode);
  NSNumberFormatter *fmt = [[NSNumberFormatter alloc] init];
  fmt.numberStyle = NSNumberFormatterDecimalStyle;
  fmt.maximumFractionDigits = 0;
  NSString *numStr = [fmt stringFromNumber:@(val)];
  if (label.empty())
    return numStr;
  return [NSString stringWithFormat:@"%@ %s", numStr, label.c_str()];
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
  self.navigationItem.rightBarButtonItem = done;

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

#pragma mark - Data Loading

- (void)loadConditionsFromProject:(const ost_data::ProjectData &)project {
  _sections.clear();

  const auto &conditions = project.conditions;
  const auto &cdn_types = project.cdn_types;
  const auto &cond_folders = project.condition_folders;

  std::string fallback_folder_uid;
  std::vector<std::string> folder_order;
  for (const auto &pair : cond_folders) {
    folder_order.push_back(pair.first);
  }
  std::sort(folder_order.begin(), folder_order.end(),
            [&cond_folders](const std::string &a, const std::string &b) {
              return cond_folders.at(a).name < cond_folders.at(b).name;
            });
  if (!folder_order.empty()) {
    fallback_folder_uid = folder_order.front();
  }

  std::map<std::string, std::vector<const ost_domain::Condition *>>
      conds_by_folder;
  for (const auto &pair : conditions) {
    const auto &cond = pair.second;
    std::string fuid = cond.condition_folder_uid;
    if (!fuid.empty() && cond_folders.count(fuid)) {
      conds_by_folder[fuid].push_back(&cond);
    } else {
      conds_by_folder[fallback_folder_uid].push_back(&cond);
    }
  }

  bool multiple_folders = cond_folders.size() > 1;

  auto build_sections_for_folder =
      [&](const std::string &folder_uid, const std::string &folder_name,
          const std::vector<const ost_domain::Condition *> &conds) {
        std::map<std::string, std::vector<const ost_domain::Condition *>>
            by_cdn;
        for (const auto *cond : conds) {
          std::string key =
              cond->cdn_type_uid.empty() ? std::string("") : cond->cdn_type_uid;
          by_cdn[key].push_back(cond);
        }

        std::vector<std::pair<std::string, std::string>> cdn_keys;
        for (const auto &pair : by_cdn) {
          std::string name;
          if (pair.first.empty()) {
            name = "(unassigned)";
          } else {
            auto it = cdn_types.find(pair.first);
            name = (it != cdn_types.end()) ? it->second.name : "(unassigned)";
          }
          cdn_keys.push_back({pair.first, name});
        }
        std::sort(
            cdn_keys.begin(), cdn_keys.end(),
            [](const auto &a, const auto &b) { return a.second < b.second; });

        for (const auto &ck : cdn_keys) {
          ConditionSection section;
          section.folder_name = multiple_folders ? folder_name : "";
          section.cdn_type_name = ck.second;

          const auto &group_conds = by_cdn[ck.first];
          for (const auto *cond : group_conds) {
            ConditionRow row;
            row.uid = cond->uid;
            row.name = cond->name;
            row.ref_no = cond->ref_no;
            row.color_fill = cond->color_fill;
            row.uom1 = cond->uom1;
            row.uom2 = cond->uom2;
            row.uom3 = cond->uom3;
            section.rows.push_back(std::move(row));
          }
          _sections.push_back(std::move(section));
        }
      };

  if (!folder_order.empty()) {
    for (const auto &fuid : folder_order) {
      std::string fname = cond_folders.at(fuid).name;
      auto it = conds_by_folder.find(fuid);
      if (it != conds_by_folder.end()) {
        build_sections_for_folder(fuid, fname, it->second);
      }
    }
  } else {

    std::vector<const ost_domain::Condition *> all;
    for (const auto &pair : conditions) {
      all.push_back(&pair.second);
    }
    build_sections_for_folder("", "", all);
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
  const auto &sec = _sections[section];
  if (sec.folder_name.empty()) {
    return [NSString stringWithUTF8String:sec.cdn_type_name.c_str()];
  }
  NSString *folder = [NSString stringWithUTF8String:sec.folder_name.c_str()];
  NSString *cdn = [NSString stringWithUTF8String:sec.cdn_type_name.c_str()];
  return [NSString stringWithFormat:@"%@ — %@", folder, cdn];
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

  NSString *refStr = [NSString stringWithFormat:@"%d", row.ref_no];
  std::string displayName = stripElevation(row.name);
  NSString *nameStr = [NSString stringWithUTF8String:displayName.c_str()];
  cell.textLabel.text = [NSString stringWithFormat:@"%@  %@", refStr, nameStr];
  cell.textLabel.font = [UIFont systemFontOfSize:15];

  auto qit = _quantities.find(row.uid);
  if (qit != _quantities.end()) {
    const auto &q = qit->second;
    NSMutableArray *parts = [NSMutableArray array];
    NSString *s;
    if ((s = formatQty(q.q1, row.uom1)))
      [parts addObject:s];
    if ((s = formatQty(q.q2, row.uom2)))
      [parts addObject:s];
    if ((s = formatQty(q.q3, row.uom3)))
      [parts addObject:s];
    cell.detailTextLabel.text =
        parts.count ? [parts componentsJoinedByString:@"  ·  "] : nil;
  } else {
    cell.detailTextLabel.text = nil;
  }
  cell.detailTextLabel.font = [UIFont systemFontOfSize:12];
  cell.detailTextLabel.textColor = [UIColor secondaryLabelColor];

  int cf = row.color_fill;
  CGFloat r = (cf & 0xFF) / 255.0;
  CGFloat g = ((cf >> 8) & 0xFF) / 255.0;
  CGFloat b = ((cf >> 16) & 0xFF) / 255.0;
  UIColor *color = (cf < 0) ? [UIColor grayColor]
                            : [UIColor colorWithRed:r green:g blue:b alpha:1.0];

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
  cell.accessoryType = isSelected
                           ? UITableViewCellAccessoryDetailDisclosureButton
                           : UITableViewCellAccessoryDetailButton;

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
  NSString *uid = [NSString stringWithUTF8String:row.uid.c_str()];

  if (_delegate &&
      [_delegate respondsToSelector:@selector(conditionsViewController:
                                              didRequestEditConditionUID:)]) {
    [_delegate conditionsViewController:self didRequestEditConditionUID:uid];
  }
}

- (void)_handleLongPress:(UILongPressGestureRecognizer *)gesture {
  if (gesture.state != UIGestureRecognizerStateBegan)
    return;

  CGPoint point = [gesture locationInView:_tableView];
  NSIndexPath *indexPath = [_tableView indexPathForRowAtPoint:point];
  if (!indexPath)
    return;
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
                                                 @"Deleting a Condition will "
                                                 @"delete all its Takeoffs "
                                                 @"on all Pages within this "
                                                 @"Bid.\n\nThis cannot be "
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
