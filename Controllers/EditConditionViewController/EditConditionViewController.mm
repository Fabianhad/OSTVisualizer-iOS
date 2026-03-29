#import "Components/EditConditionHelpers.h"
#import "EditConditionViewController_Internal.h"
#include <algorithm>
#include <cmath>

static NSString *OSTTrimmedText(NSString *text) {
  return [(text ?: @"")
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
}

static NSString *OSTPlainDisplay(double value) {
  return (value == 0.0) ? @"" : [NSString stringWithFormat:@"%g", value];
}

static bool OSTParseStrictDouble(NSString *text, double *outValue) {
  NSString *trimmed = OSTTrimmedText(text);
  if (trimmed.length == 0) {
    *outValue = 0.0;
    return true;
  }

  NSScanner *scanner = [NSScanner scannerWithString:trimmed];
  double value = 0.0;
  if (![scanner scanDouble:&value])
    return false;
  if (!scanner.isAtEnd)
    return false;
  *outValue = value;
  return true;
}

@implementation EditConditionViewController

@synthesize delegate = _delegate;

#pragma mark - Init

- (BOOL)_conditionHasTakeoffs:(const std::string &)condUID {
  for (const auto &tk : _takeoffs) {
    if (tk.condition_uid == condUID)
      return YES;
  }
  return NO;
}

- (instancetype)
    initWithCondition:(const ost_domain::Condition &)condition
        conditionUIDs:(const std::vector<std::string> &)orderedUIDs
        conditionsMap:
            (const std::unordered_map<std::string, ost_domain::Condition> &)
                conditions
             takeoffs:(const std::vector<ost_domain::Takeoff> &)takeoffs
               layers:
                   (const std::unordered_map<std::string, ost_domain::Layer> &)
                       layers
             cdnTypes:
                 (const std::unordered_map<std::string, ost_domain::CdnType> &)
                     cdnTypes {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _condition = condition;
    _elevParts = parseElevation(condition.name);
    _fieldTextCache = [[NSMutableDictionary alloc] init];
    _notesTextCache = @"";

    _layers = layers;
    _cdnTypes = cdnTypes;
    _conditionsMap = conditions;
    _takeoffs = takeoffs;
    _conditionUIDs = orderedUIDs;
    _styleDisabled = [self _conditionHasTakeoffs:condition.uid];

    _currentIndex = -1;
    for (size_t i = 0; i < _conditionUIDs.size(); i++) {
      if (_conditionUIDs[i] == condition.uid) {
        _currentIndex = (NSInteger)i;
        break;
      }
    }

    std::string defaultLayerUID;
    for (const auto &pair : _layers) {
      if (pair.second.name == "Default") {
        defaultLayerUID = pair.first;
        break;
      }
    }
    if (!defaultLayerUID.empty()) {
      _sortedLayerUIDs.push_back(defaultLayerUID);
    }
    for (const auto &pair : _layers) {
      const std::string &name = pair.second.name;
      if (pair.first == defaultLayerUID || name == "Image" ||
          name == "Annotation" || name == "Comments") {
        continue;
      }
      _sortedLayerUIDs.push_back(pair.first);
    }
    if (_sortedLayerUIDs.size() > (defaultLayerUID.empty() ? 0u : 1u)) {
      auto begin = _sortedLayerUIDs.begin() + (defaultLayerUID.empty() ? 0 : 1);
      std::sort(begin, _sortedLayerUIDs.end(),
                [&](const std::string &a, const std::string &b) {
                  return _layers.at(a).name < _layers.at(b).name;
                });
    }
    for (const auto &pair : _cdnTypes)
      _sortedCdnTypeUIDs.push_back(pair.first);
    std::sort(_sortedCdnTypeUIDs.begin(), _sortedCdnTypeUIDs.end(),
              [&](const std::string &a, const std::string &b) {
                return _cdnTypes.at(a).name < _cdnTypes.at(b).name;
              });

    [self _syncFieldCacheFromCondition];
    [self _rebuildDimRows];
    [self _rebuildAdvRows];
  }
  return self;
}

#pragma mark - Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Edit Condition";
  self.view.backgroundColor = [UIColor systemGroupedBackgroundColor];

  UIBarButtonItem *cancel = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(_cancel)];
  self.navigationItem.leftBarButtonItem = cancel;

  UIBarButtonItem *save = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemSave
                           target:self
                           action:@selector(_save)];

  _prevButton = [[UIBarButtonItem alloc]
      initWithImage:[UIImage systemImageNamed:@"chevron.up"]
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(_prev)];
  _nextButton = [[UIBarButtonItem alloc]
      initWithImage:[UIImage systemImageNamed:@"chevron.down"]
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(_next)];
  [self _updateNavButtons];

  self.navigationItem.rightBarButtonItems = @[ save, _nextButton, _prevButton ];

  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStyleInsetGrouped];
  _tableView.dataSource = self;
  _tableView.delegate = self;
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableView.allowsSelection = YES;
  _tableView.keyboardDismissMode = UIScrollViewKeyboardDismissModeInteractive;
  [self.view addSubview:_tableView];

  [NSLayoutConstraint activateConstraints:@[
    [_tableView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_tableView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [_tableView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_tableView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
}

#pragma mark - Data Builders

- (void)_rebuildDimRows {
  _dimRows.clear();
  bool isCount =
      (_condition.condition_type == ost_domain::ConditionType::Count ||
       _condition.condition_type == ost_domain::ConditionType::Attachment);
  bool isLinear =
      (_condition.condition_type == ost_domain::ConditionType::Linear);
  bool isArea = (_condition.condition_type == ost_domain::ConditionType::Area);

  if (isCount) {
    _dimRows.push_back(
        {@"Height", kTagHeight, _condition.height, DimFormat::FeetInches});
    _dimRows.push_back(
        {@"Width", kTagWidth, _condition.width, DimFormat::FeetInches});
    int sv = static_cast<int>(_condition.shape);
    if (sv == 3 || sv == 13) {
      _dimRows.push_back(
          {@"Depth", kTagDepth, _condition.depth, DimFormat::FeetInches});
    }
    _dimRows.push_back({@"Display Size %", kTagDispSize,
                        _condition.display_size, DimFormat::Percent});
  } else if (isLinear) {
    _dimRows.push_back(
        {@"Height", kTagHeight, _condition.height, DimFormat::FeetInches});
    _dimRows.push_back({@"Thickness", kTagThickness, _condition.thickness,
                        DimFormat::FeetInches});
    _dimRows.push_back({@"Rise", kTagRise, _condition.rise, DimFormat::Plain});
    _dimRows.push_back({@"Run", kTagRun, _condition.run, DimFormat::Plain});
  } else if (isArea) {
    _dimRows.push_back({@"Thickness", kTagThickness, _condition.thickness,
                        DimFormat::FeetInches});
    _dimRows.push_back({@"Rise", kTagRise, _condition.rise, DimFormat::Plain});
    _dimRows.push_back({@"Run", kTagRun, _condition.run, DimFormat::Plain});
  }
}

- (void)_rebuildAdvRows {
  _advRows.clear();
  bool isCount =
      (_condition.condition_type == ost_domain::ConditionType::Count ||
       _condition.condition_type == ost_domain::ConditionType::Attachment);
  bool isLinear =
      (_condition.condition_type == ost_domain::ConditionType::Linear);

  if (isCount) {
    _advRows.push_back(
        {@"Connect", AdvRowType::Toggle, 0, _condition.connect, 0});
    _advRows.push_back({@"Tolerance", AdvRowType::Dimension, kTagTolerance,
                        false, _condition.connect_tolerance});
    _advRows.push_back(
        {@"Display Name", AdvRowType::Toggle, 0, _condition.display_name, 0});
  } else if (isLinear) {
    _advRows.push_back({@"Round Quantity", AdvRowType::Toggle, 0,
                        _condition.round_quantity, 0});
    _advRows.push_back({@"Round to", AdvRowType::Dimension, kTagRoundTo, false,
                        _condition.round_up});
    _advRows.push_back(
        {@"Drop/Run", AdvRowType::Toggle, 0, _condition.drop_run, 0});
    _advRows.push_back({@"Add Length", AdvRowType::Dimension, kTagAddLength,
                        false, _condition.drop_value});
    _advRows.push_back(
        {@"Connect", AdvRowType::Toggle, 0, _condition.connect, 0});
    _advRows.push_back({@"Tolerance", AdvRowType::Dimension, kTagTolerance,
                        false, _condition.connect_tolerance});
    _advRows.push_back({@"Trim", AdvRowType::Toggle, 0, _condition.trim, 0});
    _advRows.push_back({@"Curved Segment", AdvRowType::Toggle, 0,
                        _condition.is_curved_segment, 0});
  } else {
    _advRows.push_back({@"Round Quantity", AdvRowType::Toggle, 0,
                        _condition.round_quantity, 0});
    _advRows.push_back({@"Round to", AdvRowType::Dimension, kTagRoundTo, false,
                        _condition.round_up});
    _advRows.push_back({@"Grid", AdvRowType::Toggle, 0, _condition.grid, 0});
    _advRows.push_back({@"Tile X", AdvRowType::Dimension, kTagTileX, false,
                        _condition.grid_size1});
    _advRows.push_back({@"Tile Y", AdvRowType::Dimension, kTagTileY, false,
                        _condition.grid_size2});
    _advRows.push_back(
        {@"Gap", AdvRowType::Dimension, kTagGap, false, _condition.gap});
    _advRows.push_back({@"Display Pattern", AdvRowType::Toggle, 0,
                        _condition.display_grid_while_drawing, 0});
    _advRows.push_back({@"Display Dimension", AdvRowType::Toggle, 0,
                        _condition.display_dimension, 0});
    _advRows.push_back(
        {@"Display Name", AdvRowType::Toggle, 0, _condition.display_name, 0});
    _advRows.push_back({@"Snap to Linear", AdvRowType::Toggle, 0,
                        _condition.snap_to_linear != -1, 0});
  }
}

#pragma mark - Actions

- (void)_cancel {
  [self.view endEditing:YES];

  if (_dirty) {
    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"Unsaved Changes"
                         message:@"Save changes before closing?"
                  preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Save"
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction *a) {
                                              [self _save];
                                            }]];
    [alert addAction:[UIAlertAction
                         actionWithTitle:@"Discard"
                                   style:UIAlertActionStyleDestructive
                                 handler:^(UIAlertAction *a) {
                                   [self dismissViewControllerAnimated:YES
                                                            completion:nil];
                                 }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                              style:UIAlertActionStyleCancel
                                            handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
    return;
  }

  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)_elevTypeChanged:(UISegmentedControl *)seg {
  _elevParts.type = (int)seg.selectedSegmentIndex;
  _dirty = YES;
}

- (void)_save {
  [self.view endEditing:YES];
  if (![self _validateAndCollectTextFields]) {
    return;
  }
  [self _shiftConflictingRefNosIfNeeded];
  _conditionsMap[_condition.uid] = _condition;

  if (_delegate) {
    [_delegate editConditionViewController:self didSaveCondition:_condition];
  }
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (NSString *)_cachedTextForTag:(NSInteger)tag {
  NSString *cached = _fieldTextCache[@(tag)];
  return cached ?: @"";
}

- (void)_syncFieldCacheFromCondition {
  _fieldTextCache[@(kTagName)] =
      [NSString stringWithUTF8String:_elevParts.baseName.c_str()];
  _fieldTextCache[@(kTagRefNo)] =
      [NSString stringWithFormat:@"%d", _condition.ref_no];
  _fieldTextCache[@(kTagElevVal)] =
      [NSString stringWithUTF8String:_elevParts.value.c_str()];

  _fieldTextCache[@(kTagHeight)] = inchesToDisplay(_condition.height);
  _fieldTextCache[@(kTagWidth)] = inchesToDisplay(_condition.width);
  _fieldTextCache[@(kTagThickness)] = inchesToDisplay(_condition.thickness);
  _fieldTextCache[@(kTagDepth)] = inchesToDisplay(_condition.depth);
  _fieldTextCache[@(kTagDispSize)] = OSTPlainDisplay(_condition.display_size);
  _fieldTextCache[@(kTagRise)] = OSTPlainDisplay(_condition.rise);
  _fieldTextCache[@(kTagRun)] = OSTPlainDisplay(_condition.run);
  _fieldTextCache[@(kTagTolerance)] =
      inchesToDisplay(_condition.connect_tolerance);
  _fieldTextCache[@(kTagRoundTo)] = inchesToDisplay(_condition.round_up);
  _fieldTextCache[@(kTagAddLength)] = inchesToDisplay(_condition.drop_value);
  _fieldTextCache[@(kTagTileX)] = inchesToDisplay(_condition.grid_size1);
  _fieldTextCache[@(kTagTileY)] = inchesToDisplay(_condition.grid_size2);
  _fieldTextCache[@(kTagGap)] = inchesToDisplay(_condition.gap);
  _fieldTextCache[@(kTagSpacing)] = inchesToDisplay(_condition.spacing);

  _notesTextCache = [NSString stringWithUTF8String:_condition.notes.c_str()];
}

- (void)_applyStyleDefaults:(ost_domain::ConditionType)conditionType {
  _condition.condition_type = conditionType;

  _condition.pattern = ost_domain::Pattern::Transparent;
  _condition.spacing = 4.0;
  _condition.notes.clear();
  _condition.calc_type2 = 0;
  _condition.calc_type3 = 0;
  _condition.uom2 = -1;
  _condition.uom3 = -1;

  switch (conditionType) {
  case ost_domain::ConditionType::Linear:
    _condition.thickness = 4.0;
    _condition.backout = false;
    _condition.height = 0.0;
    _condition.rise = 0.0;
    _condition.run = 0.0;
    _condition.calc_type1 = 1;
    _condition.uom1 = 2;
    _condition.round_quantity = false;
    _condition.round_up = 0.0;
    _condition.drop_run = false;
    _condition.drop_value = 0.0;
    _condition.connect = true;
    _condition.connect_tolerance = 6.0;
    _condition.trim = false;
    _condition.is_curved_segment = false;
    break;
  case ost_domain::ConditionType::Area:
    _condition.thickness = 4.0;
    _condition.backout = false;
    _condition.rise = 0.0;
    _condition.run = 0.0;
    _condition.calc_type1 = 11;
    _condition.uom1 = 5;
    _condition.round_quantity = false;
    _condition.round_up = 0.0;
    _condition.grid = false;
    _condition.grid_size1 = 0.0;
    _condition.grid_size2 = 0.0;
    _condition.gap = 0.0;
    _condition.display_grid_while_drawing = true;
    _condition.display_dimension = false;
    _condition.display_name = false;
    _condition.snap_to_linear = -1;
    break;
  case ost_domain::ConditionType::Count:
    _condition.thickness = 4.0;
    _condition.backout = true;
    _condition.height = 0.0;
    _condition.width = 12.0;
    _condition.depth = 0.0;
    _condition.display_size = 100.0;
    _condition.shape = ost_domain::ShapeId::Square;
    _condition.calc_type1 = 23;
    _condition.uom1 = 0;
    _condition.connect = true;
    _condition.connect_tolerance = 6.0;
    _condition.display_name = false;
    break;
  case ost_domain::ConditionType::Attachment:
    _condition.thickness = 0.0;
    _condition.backout = true;
    _condition.height = 0.0;
    _condition.width = 12.0;
    _condition.depth = 0.0;
    _condition.display_size = 100.0;
    _condition.shape = ost_domain::ShapeId::Square;
    _condition.calc_type1 = 23;
    _condition.uom1 = 0;
    _condition.connect = true;
    _condition.connect_tolerance = 6.0;
    _condition.display_name = false;
    break;
  }

  [self _syncFieldCacheFromCondition];
}

- (void)_shiftConflictingRefNosIfNeeded {
  auto currentIt = _conditionsMap.find(_condition.uid);
  int oldRefNo =
      (currentIt != _conditionsMap.end()) ? currentIt->second.ref_no : 0;
  if (_condition.ref_no == oldRefNo) {
    return;
  }

  bool hasConflict = false;
  for (const auto &pair : _conditionsMap) {
    if (pair.first != _condition.uid &&
        pair.second.ref_no == _condition.ref_no) {
      hasConflict = true;
      break;
    }
  }
  if (!hasConflict) {
    return;
  }

  for (auto &pair : _conditionsMap) {
    if (pair.first != _condition.uid &&
        pair.second.ref_no >= _condition.ref_no) {
      pair.second.ref_no += 1;
    }
  }
}

- (void)_showValidationAlertWithTitle:(NSString *)title
                              message:(NSString *)message
                             focusTag:(NSInteger)focusTag {
  __weak typeof(self) weakSelf = self;
  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alert
      addAction:[UIAlertAction
                    actionWithTitle:@"OK"
                              style:UIAlertActionStyleDefault
                            handler:^(__unused UIAlertAction *action) {
                              __strong typeof(weakSelf) strongSelf = weakSelf;
                              if (!strongSelf)
                                return;

                              UIView *focusView = nil;
                              switch (focusTag) {
                              case kTagName:
                                focusView = strongSelf->_nameField;
                                break;
                              case kTagRefNo:
                                focusView = strongSelf->_refNoField;
                                break;
                              case kTagElevVal:
                                focusView = strongSelf->_elevValueField;
                                break;
                              default:
                                focusView =
                                    [strongSelf.view viewWithTag:focusTag];
                                break;
                              }

                              if ([focusView
                                      respondsToSelector:
                                          @selector(becomeFirstResponder)]) {
                                [(UIResponder *)focusView becomeFirstResponder];
                              }
                            }]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (BOOL)_parseDimensionText:(NSString *)text
                      label:(NSString *)label
                   focusTag:(NSInteger)focusTag
                   outValue:(double *)outValue {
  NSString *trimmed = OSTTrimmedText(text);
  if (trimmed.length == 0) {
    *outValue = 0.0;
    return YES;
  }

  bool valid = true;
  double value = displayToInches(trimmed, &valid);
  if (!valid) {
    NSString *message = [NSString
        stringWithFormat:@"\"%@\" is not a valid %@.", trimmed, label];
    [self _showValidationAlertWithTitle:@"Invalid Value"
                                message:message
                               focusTag:focusTag];
    return NO;
  }
  *outValue = value;
  return YES;
}

- (BOOL)_parsePlainText:(NSString *)text
                  label:(NSString *)label
               focusTag:(NSInteger)focusTag
               outValue:(double *)outValue {
  NSString *trimmed = OSTTrimmedText(text);
  if (trimmed.length == 0) {
    *outValue = 0.0;
    return YES;
  }

  double value = 0.0;
  if (!OSTParseStrictDouble(trimmed, &value)) {
    NSString *message = [NSString
        stringWithFormat:@"\"%@\" is not a valid %@.", trimmed, label];
    [self _showValidationAlertWithTitle:@"Invalid Value"
                                message:message
                               focusTag:focusTag];
    return NO;
  }
  *outValue = value;
  return YES;
}

- (BOOL)_validateAndCollectTextFields {
  NSString *baseName = OSTTrimmedText([self _cachedTextForTag:kTagName]);
  if (baseName.length == 0) {
    [self _showValidationAlertWithTitle:@"Validation Error"
                                message:@"Condition name cannot be empty."
                               focusTag:kTagName];
    return NO;
  }

  NSString *elevValue = OSTTrimmedText([self _cachedTextForTag:kTagElevVal]);
  _elevParts.baseName = std::string(baseName.UTF8String);
  _elevParts.type =
      _elevTypeSeg ? (int)_elevTypeSeg.selectedSegmentIndex : _elevParts.type;
  _elevParts.value = std::string(elevValue.UTF8String);

  _condition.is_top = (_elevParts.type == 0);
  if (elevValue.length > 0) {
    bool valid = true;
    _condition.z_value = displayToInches(elevValue, &valid);
    if (!valid) {
      NSString *message = [NSString
          stringWithFormat:@"\"%@\" is not a valid elevation.", elevValue];
      [self _showValidationAlertWithTitle:@"Invalid Value"
                                  message:message
                                 focusTag:kTagElevVal];
      return NO;
    }
  } else {
    _condition.z_value = 0.0;
  }

  _condition.name = reassembleElevation(_elevParts.baseName, _elevParts.type,
                                        _elevParts.value);

  NSString *refText = [self _cachedTextForTag:kTagRefNo];
  _condition.ref_no = refText.length > 0 ? refText.intValue : 0;
  if (_condition.ref_no < 1)
    _condition.ref_no = 1;
  if (_condition.layer_uid.empty() && !_sortedLayerUIDs.empty()) {
    _condition.layer_uid = _sortedLayerUIDs.front();
  }

  if (![self _parseDimensionText:[self _cachedTextForTag:kTagSpacing]
                           label:@"Spacing"
                        focusTag:kTagSpacing
                        outValue:&_condition.spacing]) {
    return NO;
  }

  bool isCount =
      (_condition.condition_type == ost_domain::ConditionType::Count ||
       _condition.condition_type == ost_domain::ConditionType::Attachment);
  bool isLinear =
      (_condition.condition_type == ost_domain::ConditionType::Linear);
  bool isArea = (_condition.condition_type == ost_domain::ConditionType::Area);

  if (isCount) {
    if (![self _parseDimensionText:[self _cachedTextForTag:kTagHeight]
                             label:@"Height"
                          focusTag:kTagHeight
                          outValue:&_condition.height] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagWidth]
                             label:@"Width"
                          focusTag:kTagWidth
                          outValue:&_condition.width] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagDepth]
                             label:@"Depth"
                          focusTag:kTagDepth
                          outValue:&_condition.depth] ||
        ![self _parsePlainText:[self _cachedTextForTag:kTagDispSize]
                         label:@"Display Size"
                      focusTag:kTagDispSize
                      outValue:&_condition.display_size] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagTolerance]
                             label:@"Tolerance"
                          focusTag:kTagTolerance
                          outValue:&_condition.connect_tolerance]) {
      return NO;
    }

    if (_condition.display_size < 10.0 || _condition.display_size > 500.0) {
      [self _showValidationAlertWithTitle:@"Validation Error"
                                  message:@"Display Size must be between 10% "
                                          @"and 500%."
                                 focusTag:kTagDispSize];
      return NO;
    }
  } else if (isLinear) {
    if (![self _parseDimensionText:[self _cachedTextForTag:kTagHeight]
                             label:@"Height"
                          focusTag:kTagHeight
                          outValue:&_condition.height] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagThickness]
                             label:@"Thickness"
                          focusTag:kTagThickness
                          outValue:&_condition.thickness] ||
        ![self _parsePlainText:[self _cachedTextForTag:kTagRise]
                         label:@"Rise"
                      focusTag:kTagRise
                      outValue:&_condition.rise] ||
        ![self _parsePlainText:[self _cachedTextForTag:kTagRun]
                         label:@"Run"
                      focusTag:kTagRun
                      outValue:&_condition.run] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagRoundTo]
                             label:@"Round to"
                          focusTag:kTagRoundTo
                          outValue:&_condition.round_up] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagAddLength]
                             label:@"Add Length"
                          focusTag:kTagAddLength
                          outValue:&_condition.drop_value] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagTolerance]
                             label:@"Tolerance"
                          focusTag:kTagTolerance
                          outValue:&_condition.connect_tolerance]) {
      return NO;
    }
  } else if (isArea) {
    if (![self _parseDimensionText:[self _cachedTextForTag:kTagThickness]
                             label:@"Thickness"
                          focusTag:kTagThickness
                          outValue:&_condition.thickness] ||
        ![self _parsePlainText:[self _cachedTextForTag:kTagRise]
                         label:@"Rise"
                      focusTag:kTagRise
                      outValue:&_condition.rise] ||
        ![self _parsePlainText:[self _cachedTextForTag:kTagRun]
                         label:@"Run"
                      focusTag:kTagRun
                      outValue:&_condition.run] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagRoundTo]
                             label:@"Round to"
                          focusTag:kTagRoundTo
                          outValue:&_condition.round_up] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagTileX]
                             label:@"Tile X"
                          focusTag:kTagTileX
                          outValue:&_condition.grid_size1] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagTileY]
                             label:@"Tile Y"
                          focusTag:kTagTileY
                          outValue:&_condition.grid_size2] ||
        ![self _parseDimensionText:[self _cachedTextForTag:kTagGap]
                             label:@"Gap"
                          focusTag:kTagGap
                          outValue:&_condition.gap]) {
      return NO;
    }
  }

  NSString *notes = _notesTextCache ?: @"";
  _condition.notes = std::string(notes.UTF8String);
  [self _syncFieldCacheFromCondition];
  return YES;
}

#pragma mark - Navigation

- (void)_updateNavButtons {
  _prevButton.enabled = (_currentIndex > 0);
  _nextButton.enabled = (_currentIndex >= 0 &&
                         _currentIndex < (NSInteger)_conditionUIDs.size() - 1);
}

- (void)_loadConditionAtIndex:(NSInteger)index {
  _currentIndex = index;
  const std::string &uid = _conditionUIDs[index];
  auto it = _conditionsMap.find(uid);
  if (it == _conditionsMap.end())
    return;

  _condition = it->second;
  _elevParts = parseElevation(_condition.name);
  _dirty = NO;
  _styleDisabled = [self _conditionHasTakeoffs:uid];
  _nameField = nil;
  _refNoField = nil;
  _notesView = nil;
  _elevTypeSeg = nil;
  _elevValueField = nil;
  [self _syncFieldCacheFromCondition];

  [self _rebuildDimRows];
  [self _rebuildAdvRows];
  [self _updateNavButtons];
  [_tableView reloadData];
  [_tableView
      setContentOffset:CGPointMake(0, -_tableView.adjustedContentInset.top)
              animated:NO];

  if (_delegate &&
      [_delegate respondsToSelector:@selector(editConditionViewController:
                                              didNavigateToCondition:)]) {
    [_delegate
        editConditionViewController:self
             didNavigateToCondition:[NSString
                                        stringWithUTF8String:uid.c_str()]];
  }
}

- (void)_navigateToIndex:(NSInteger)index {
  if (index < 0 || index >= (NSInteger)_conditionUIDs.size())
    return;

  if (!_dirty) {
    [self _loadConditionAtIndex:index];
    return;
  }

  [self.view endEditing:YES];
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:@"Unsaved Changes"
                       message:@"Save changes before switching?"
                preferredStyle:UIAlertControllerStyleAlert];

  [alert
      addAction:[UIAlertAction
                    actionWithTitle:@"Save"
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction *a) {
                              if ([self _validateAndCollectTextFields]) {
                                [self _shiftConflictingRefNosIfNeeded];
                                self->_conditionsMap[self->_condition.uid] =
                                    self->_condition;
                                if (self->_delegate) {
                                  [self->_delegate
                                      editConditionViewController:self
                                                 didSaveCondition:self->
                                                                  _condition];
                                }
                                [self _loadConditionAtIndex:index];
                              }
                            }]];

  [alert addAction:[UIAlertAction actionWithTitle:@"Discard"
                                            style:UIAlertActionStyleDestructive
                                          handler:^(UIAlertAction *a) {
                                            self->_dirty = NO;
                                            [self _loadConditionAtIndex:index];
                                          }]];

  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_prev {
  [self _navigateToIndex:_currentIndex - 1];
}

- (void)_next {
  [self _navigateToIndex:_currentIndex + 1];
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidEndEditing:(UITextField *)textField {
  _dirty = YES;

  NSInteger tag = textField.tag;

  BOOL isDimensionField =
      (tag == kTagHeight || tag == kTagWidth || tag == kTagThickness ||
       tag == kTagDepth || tag == kTagElevVal || tag == kTagTolerance ||
       tag == kTagRoundTo || tag == kTagAddLength || tag == kTagTileX ||
       tag == kTagTileY || tag == kTagGap || tag == kTagSpacing);
  if (isDimensionField) {
    NSString *text = OSTTrimmedText(textField.text);
    if (text.length > 0) {
      bool valid = true;
      double val = displayToInches(text, &valid);
      if (valid) {
        textField.text = inchesToDisplay(val);
      }
    }
  }

  _fieldTextCache[@(tag)] = textField.text ?: @"";
}

- (void)textViewDidChange:(UITextView *)textView {
  if (textView.tag != kTagNotes)
    return;
  _dirty = YES;
  _notesTextCache = textView.text ?: @"";
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - Cell Helpers

- (UITableViewCell *)_detailCell:(UITableView *)table id:(NSString *)cellId {
  UITableViewCell *cell = [table dequeueReusableCellWithIdentifier:cellId];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1
                                  reuseIdentifier:cellId];
  }
  cell.accessoryView = nil;
  cell.accessoryType = UITableViewCellAccessoryNone;
  return cell;
}

- (UITableViewCell *)_textFieldCell:(UITableView *)table
                              label:(NSString *)label
                              value:(NSString *)value
                                tag:(NSInteger)tag {
  UITableViewCell *cell =
      [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                             reuseIdentifier:nil];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  UILabel *lbl = [[UILabel alloc] init];
  lbl.text = label;
  lbl.font = [UIFont systemFontOfSize:16];
  lbl.translatesAutoresizingMaskIntoConstraints = NO;
  [lbl setContentHuggingPriority:UILayoutPriorityRequired
                         forAxis:UILayoutConstraintAxisHorizontal];
  [lbl
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [cell.contentView addSubview:lbl];

  UITextField *field = [[UITextField alloc] init];
  NSString *cachedValue = _fieldTextCache[@(tag)];
  field.text = cachedValue ?: value;
  field.font = [UIFont systemFontOfSize:16];
  field.textAlignment = NSTextAlignmentRight;
  field.textColor = [UIColor secondaryLabelColor];
  field.tag = tag;
  field.delegate = self;
  field.returnKeyType = UIReturnKeyDone;
  field.translatesAutoresizingMaskIntoConstraints = NO;
  field.adjustsFontSizeToFitWidth = YES;
  field.minimumFontSize = 11;
  [cell.contentView addSubview:field];

  [NSLayoutConstraint activateConstraints:@[
    [lbl.leadingAnchor
        constraintEqualToAnchor:cell.contentView.layoutMarginsGuide
                                    .leadingAnchor],
    [lbl.centerYAnchor constraintEqualToAnchor:cell.contentView.centerYAnchor],
    [field.leadingAnchor constraintEqualToAnchor:lbl.trailingAnchor
                                        constant:12],
    [field.trailingAnchor
        constraintEqualToAnchor:cell.contentView.layoutMarginsGuide
                                    .trailingAnchor],
    [field.centerYAnchor
        constraintEqualToAnchor:cell.contentView.centerYAnchor],
  ]];

  return cell;
}

@end
