#import "../EditConditionViewController_Internal.h"
#import "EditConditionHelpers.h"
#include "../../../Core/Domain/ConditionFormRules.hpp"
#include "../../../Core/Domain/DimensionFormat.hpp"
#include <cmath>

// The feet-inch, millimetre and elevation fields only parse the ASCII ' and " (and plain hyphens): with Smart Punctuation on, iOS
// types curly quotes and en dashes that every parser refuses (UI-M01).
static void OSTDisableSmartPunctuation(UITextField *field) {
  field.smartQuotesType = UITextSmartQuotesTypeNo;
  field.smartDashesType = UITextSmartDashesTypeNo;
}

@interface EditConditionViewController (TableView)
@end

@implementation EditConditionViewController (TableView)

#pragma mark - UITableView DataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  return kSectionCount;
}

- (NSString *)tableView:(UITableView *)tableView
    titleForHeaderInSection:(NSInteger)section {
  switch (section) {
  case kSectionDimensions:
    return @"Dimensions";
  case kSectionAppearance:
    return @"Appearance";
  case kSectionResults:
    return @"Results";
  case kSectionNotes:
    return @"Notes";
  case kSectionAdvanced:
    return @"Advanced";
  default:
    return nil;
  }
}

- (NSInteger)tableView:(UITableView *)tableView
    numberOfRowsInSection:(NSInteger)section {
  switch (section) {
  case kSectionTop:
    return static_cast<int>(TopRow::Count);
  case kSectionDimensions:
    return (NSInteger)_dimRows.size();
  case kSectionAppearance:
    return [self _appearanceRowCount];
  case kSectionResults:
    return 3;
  case kSectionNotes:
    return 1;
  case kSectionAdvanced:
    return (NSInteger)_advRows.size();
  default:
    return 0;
  }
}

- (NSInteger)_appearanceRowCount {
  bool isCount =
      (_condition.condition_type == ost_domain::ConditionType::Count ||
       _condition.condition_type == ost_domain::ConditionType::Attachment);
  NSInteger count = 2; // Color + Pattern
  if (isLinePattern(_condition.pattern))
    count++; // Spacing
  if (isCount)
    count++; // Shape
  return count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  if (indexPath.section == kSectionTop)
    return [self _topCellForTable:tableView row:indexPath.row];
  if (indexPath.section == kSectionDimensions)
    return [self _dimCellForTable:tableView row:indexPath.row];
  if (indexPath.section == kSectionAppearance)
    return [self _appearanceCellForTable:tableView row:indexPath.row];
  if (indexPath.section == kSectionResults)
    return [self _resultsCellForTable:tableView row:indexPath.row];
  if (indexPath.section == kSectionNotes)
    return [self _notesCellForTable:tableView];
  if (indexPath.section == kSectionAdvanced)
    return [self _advancedCellForTable:tableView row:indexPath.row];
  return [[UITableViewCell alloc] init];
}

#pragma mark - Top Section Cells

- (UITableViewCell *)_topCellForTable:(UITableView *)tableView
                                  row:(NSInteger)row {
  TopRow r = static_cast<TopRow>(row);
  switch (r) {
  case TopRow::Style: {
    UITableViewCell *cell = [self _detailCell:tableView id:@"StyleCell"];
    cell.textLabel.text = @"Style";
    cell.detailTextLabel.text =
        styleLabels()[static_cast<int>(_condition.condition_type)];
    if (_styleDisabled) {
      cell.accessoryType = UITableViewCellAccessoryNone;
      cell.textLabel.textColor = [UIColor tertiaryLabelColor];
      cell.detailTextLabel.textColor = [UIColor tertiaryLabelColor];
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
    } else {
      cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      cell.textLabel.textColor = [UIColor labelColor];
      cell.selectionStyle = UITableViewCellSelectionStyleDefault;
    }
    return cell;
  }
  case TopRow::CdnType: {
    UITableViewCell *cell = [self _detailCell:tableView id:@"TypeCell"];
    cell.textLabel.text = @"Type";
    cell.detailTextLabel.text =
        _condition.cdn_type_name.empty()
            ? @"(none)"
            : [NSString stringWithUTF8String:_condition.cdn_type_name.c_str()];
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    return cell;
  }
  case TopRow::Name: {
    UITableViewCell *cell = [self
        _textFieldCell:tableView
                 label:@"Name"
                 value:[NSString
                           stringWithUTF8String:_elevParts.baseName.c_str()]
                   tag:kTagName];
    _nameField = (UITextField *)[cell viewWithTag:kTagName];
    return cell;
  }
  case TopRow::Elevation: {
    UITableViewCell *cell =
        [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                               reuseIdentifier:nil];
    cell.selectionStyle = UITableViewCellSelectionStyleNone;

    UILabel *lbl = [[UILabel alloc] init];
    lbl.text = @"Elevation";
    lbl.font = [UIFont systemFontOfSize:16];
    lbl.translatesAutoresizingMaskIntoConstraints = NO;
    [lbl setContentHuggingPriority:UILayoutPriorityRequired
                           forAxis:UILayoutConstraintAxisHorizontal];
    [cell.contentView addSubview:lbl];

    UISegmentedControl *seg =
        [[UISegmentedControl alloc] initWithItems:@[ @"Top", @"Bottom" ]];
    seg.selectedSegmentIndex = _elevParts.type;
    seg.translatesAutoresizingMaskIntoConstraints = NO;
    [seg addTarget:self
                  action:@selector(_elevTypeChanged:)
        forControlEvents:UIControlEventValueChanged];
    _elevTypeSeg = seg;
    [cell.contentView addSubview:seg];

    UITextField *field = [[UITextField alloc] init];
    // the text as typed: a reload (a picker, a style change) must not drop an elevation that is not saved yet (COND-ELEV-01)
    NSString *cachedElevation = _fieldTextCache[@(kTagElevVal)];
    field.text = cachedElevation
                     ?: [NSString
                            stringWithUTF8String:_elevParts.value.c_str()];
    field.font = [UIFont systemFontOfSize:16];
    field.textAlignment = NSTextAlignmentRight;
    field.textColor = [UIColor secondaryLabelColor];
    field.placeholder = _metric ? @"e.g. 3200" : @"e.g. 10' 6\"";
    field.tag = kTagElevVal;
    field.delegate = self;
    field.returnKeyType = UIReturnKeyDone;
    field.autocorrectionType = UITextAutocorrectionTypeNo;
    field.autocapitalizationType = UITextAutocapitalizationTypeNone;
    OSTDisableSmartPunctuation(field);
    field.translatesAutoresizingMaskIntoConstraints = NO;
    _elevValueField = field;
    [cell.contentView addSubview:field];

    [NSLayoutConstraint activateConstraints:@[
      [lbl.leadingAnchor
          constraintEqualToAnchor:cell.contentView.layoutMarginsGuide
                                      .leadingAnchor],
      [lbl.centerYAnchor
          constraintEqualToAnchor:cell.contentView.centerYAnchor],
      [seg.leadingAnchor constraintEqualToAnchor:lbl.trailingAnchor
                                        constant:12],
      [seg.centerYAnchor
          constraintEqualToAnchor:cell.contentView.centerYAnchor],
      [seg.widthAnchor constraintEqualToConstant:120],
      [field.leadingAnchor constraintEqualToAnchor:seg.trailingAnchor
                                          constant:8],
      [field.trailingAnchor
          constraintEqualToAnchor:cell.contentView.layoutMarginsGuide
                                      .trailingAnchor],
      [field.centerYAnchor
          constraintEqualToAnchor:cell.contentView.centerYAnchor],
    ]];
    return cell;
  }
  case TopRow::Layer: {
    UITableViewCell *cell = [self _detailCell:tableView id:@"LayerCell"];
    cell.textLabel.text = @"Layer";
    // an empty layer is shown and saved as no layer (COND-LAYER-01)
    if (!_condition.layer_uid.empty()) {
      auto it = _layers.find(_condition.layer_uid);
      cell.detailTextLabel.text =
          (it != _layers.end())
              ? [NSString stringWithUTF8String:it->second.name.c_str()]
              : @"(none)";
    } else {
      cell.detailTextLabel.text = @"(none)";
    }
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    return cell;
  }
  case TopRow::RefNo: {
    UITableViewCell *cell = [self
        _textFieldCell:tableView
                 label:@"Cond. No."
                 value:[NSString stringWithFormat:@"%d", _condition.ref_no]
                   tag:kTagRefNo];
    _refNoField = (UITextField *)[cell viewWithTag:kTagRefNo];
    _refNoField.keyboardType = UIKeyboardTypeNumberPad;
    return cell;
  }
  default:
    return [[UITableViewCell alloc] init];
  }
}

#pragma mark - Dimension Section Cells

- (UITableViewCell *)_dimCellForTable:(UITableView *)tableView
                                  row:(NSInteger)row {
  if ((size_t)row >= _dimRows.size())
    return [[UITableViewCell alloc] init];
  const auto &dr = _dimRows[row];

  // The texts are the Windows ones: str(int(display_size)), str(value) for Rise / Run (the whole value, not six digits), the feet-inch
  // text (millimetres in a metric bid) for the dimensions (DLG-PREC-01, COND-METRIC-01)
  NSString *valStr;
  switch (dr.format) {
  case DimFormat::Percent:
    valStr = [NSString
        stringWithUTF8String:ost_domain::display_size_text(dr.value).c_str()];
    break;
  case DimFormat::Plain:
    valStr = [NSString
        stringWithUTF8String:ost_domain::plain_number_text(dr.value).c_str()];
    break;
  case DimFormat::FeetInches:
  default:
    valStr = inchesToDisplay(dr.value, _metric);
    break;
  }

  UITableViewCell *cell = [self _textFieldCell:tableView
                                         label:dr.label
                                         value:valStr
                                           tag:dr.tag];
  UITextField *field = (UITextField *)[cell viewWithTag:dr.tag];
  if (dr.format == DimFormat::FeetInches) {
    field.keyboardType = UIKeyboardTypeDefault;
    field.autocorrectionType = UITextAutocorrectionTypeNo;
    field.autocapitalizationType = UITextAutocapitalizationTypeNone;
    OSTDisableSmartPunctuation(field);
  } else {
    field.keyboardType = UIKeyboardTypeDecimalPad;
  }
  return cell;
}

#pragma mark - Appearance Section Cells

- (UITableViewCell *)_appearanceCellForTable:(UITableView *)tableView
                                         row:(NSInteger)row {
  if (row == 0) {
    UITableViewCell *cell = [self _detailCell:tableView id:@"ColorCell"];
    cell.textLabel.text = @"Color";
    cell.detailTextLabel.text = @"";
    // ColorFill is read like Windows int_to_hex: the low 24 bits, a negative value is not grey (COL-01)
    float cr, cg, cb;
    ost_domain::Condition::rgb_from_color_fill(_condition.color_fill, cr, cg,
                                               cb);
    UIView *swatch = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 28, 28)];
    swatch.backgroundColor = [UIColor colorWithRed:cr
                                             green:cg
                                              blue:cb
                                             alpha:1.0];
    swatch.layer.cornerRadius = 4;
    swatch.layer.borderWidth = 0.5;
    swatch.layer.borderColor = [UIColor separatorColor].CGColor;
    cell.accessoryView = swatch;
    return cell;
  }
  if (row == 1) {
    UITableViewCell *cell = [self _detailCell:tableView id:@"PatternCell"];
    cell.textLabel.text = @"Pattern";
    int patIdx = static_cast<int>(_condition.pattern);
    NSArray *labels = patternLabels();
    cell.detailTextLabel.text =
        (patIdx >= 0 && patIdx < (int)labels.count) ? labels[patIdx] : @"None";
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    return cell;
  }
  NSInteger nextRow = 2;
  if (isLinePattern(_condition.pattern)) {
    if (row == nextRow) {
      UITableViewCell *cell =
          [self _textFieldCell:tableView
                         label:@"Spacing"
                         value:inchesToDisplay(_condition.spacing, _metric)
                           tag:kTagSpacing];
      UITextField *field = (UITextField *)[cell viewWithTag:kTagSpacing];
      field.keyboardType = UIKeyboardTypeDefault;
      field.autocorrectionType = UITextAutocorrectionTypeNo;
      field.autocapitalizationType = UITextAutocapitalizationTypeNone;
      OSTDisableSmartPunctuation(field);
      return cell;
    }
    nextRow++;
  }
  if (row == nextRow) {
    UITableViewCell *cell = [self _detailCell:tableView id:@"ShapeCell"];
    cell.textLabel.text = @"Shape";
    int idx = shapeDisplayIndex(_condition.shape);
    cell.detailTextLabel.text = shapeLabels()[idx];
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    return cell;
  }
  return [[UITableViewCell alloc] init];
}

#pragma mark - Results Section Cells

- (UITableViewCell *)_resultsCellForTable:(UITableView *)tableView
                                      row:(NSInteger)row {
  UITableViewCell *cell = [self _detailCell:tableView id:@"ResultCell"];
  cell.textLabel.text =
      [NSString stringWithFormat:@"Quantity %ld", (long)(row + 1)];

  int calcType = 0, uom = 0;
  if (row == 0) {
    calcType = _condition.calc_type1;
    uom = _condition.uom1;
  } else if (row == 1) {
    calcType = _condition.calc_type2;
    uom = _condition.uom2;
  } else if (row == 2) {
    calcType = _condition.calc_type3;
    uom = _condition.uom3;
  }

  if (calcType == 0) {
    cell.detailTextLabel.text = @"(none)";
  } else {
    NSString *calcLabel = @"(none)";
    auto qtyOpts =
        ost_domain::get_quantity_options_for_type(_condition.condition_type);
    for (const auto &opt : qtyOpts) {
      if (opt.first == calcType) {
        calcLabel = [NSString stringWithUTF8String:opt.second.c_str()];
        break;
      }
    }
    // the labels are UTF-8 (IN², m², ...): converted as text, never printed through %s (QTY-16)
    std::string uomLabel = ost_domain::get_uom_label(uom);
    if (!uomLabel.empty()) {
      cell.detailTextLabel.text = [NSString
          stringWithFormat:@"%@ (%@)", calcLabel,
                           [NSString stringWithUTF8String:uomLabel.c_str()]];
    } else {
      cell.detailTextLabel.text = calcLabel;
    }
  }
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return cell;
}

#pragma mark - Notes Section Cell

- (UITableViewCell *)_notesCellForTable:(UITableView *)tableView {
  UITableViewCell *cell =
      [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                             reuseIdentifier:nil];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  UITextView *tv = [[UITextView alloc] init];
  tv.text = _notesTextCache
                ?: [NSString stringWithUTF8String:_condition.notes.c_str()];
  tv.font = [UIFont systemFontOfSize:15];
  tv.backgroundColor = [UIColor clearColor];
  tv.tag = kTagNotes;
  tv.delegate = self;
  tv.translatesAutoresizingMaskIntoConstraints = NO;
  tv.scrollEnabled = NO;
  [cell.contentView addSubview:tv];
  _notesView = tv;

  [NSLayoutConstraint activateConstraints:@[
    [tv.topAnchor constraintEqualToAnchor:cell.contentView.topAnchor
                                 constant:8],
    [tv.bottomAnchor constraintEqualToAnchor:cell.contentView.bottomAnchor
                                    constant:-8],
    [tv.leadingAnchor
        constraintEqualToAnchor:cell.contentView.layoutMarginsGuide
                                    .leadingAnchor],
    [tv.trailingAnchor
        constraintEqualToAnchor:cell.contentView.layoutMarginsGuide
                                    .trailingAnchor],
    [tv.heightAnchor constraintGreaterThanOrEqualToConstant:80],
  ]];
  return cell;
}

#pragma mark - Advanced Section Cells

- (UITableViewCell *)_advancedCellForTable:(UITableView *)tableView
                                       row:(NSInteger)row {
  if ((size_t)row >= _advRows.size())
    return [[UITableViewCell alloc] init];
  const auto &ar = _advRows[row];

  if (ar.type == AdvRowType::Toggle) {
    UITableViewCell *cell =
        [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                               reuseIdentifier:nil];
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    cell.textLabel.text = ar.label;
    cell.textLabel.font = [UIFont systemFontOfSize:16];

    UISwitch *sw = [[UISwitch alloc] init];
    sw.on = ar.boolVal;
    sw.tag = row;
    // Trim disables Curved Segment (COND-TRIM-01)
    if ([ar.label isEqualToString:@"Curved Segment"])
      sw.enabled = ost_domain::curved_segment_enabled(_condition);
    [sw addTarget:self
                  action:@selector(_advToggleChanged:)
        forControlEvents:UIControlEventValueChanged];
    cell.accessoryView = sw;
    return cell;
  }

  UITableViewCell *cell =
      [self _textFieldCell:tableView
                     label:ar.label
                     value:inchesToDisplay(ar.dimVal, _metric)
                       tag:ar.tag];
  UITextField *field = (UITextField *)[cell viewWithTag:ar.tag];
  field.keyboardType = UIKeyboardTypeDefault;
  field.autocorrectionType = UITextAutocorrectionTypeNo;
  field.autocapitalizationType = UITextAutocapitalizationTypeNone;
  OSTDisableSmartPunctuation(field);
  return cell;
}

- (void)_advToggleChanged:(UISwitch *)sw {
  NSInteger row = sw.tag;
  if ((size_t)row >= _advRows.size())
    return;
  _advRows[row].boolVal = sw.isOn;

  NSString *label = _advRows[row].label;
  if ([label isEqualToString:@"Connect"])
    _condition.connect = sw.isOn;
  else if ([label isEqualToString:@"Display Name"])
    _condition.display_name = sw.isOn;
  else if ([label isEqualToString:@"Round Quantity"])
    _condition.round_quantity = sw.isOn;
  else if ([label isEqualToString:@"Drop/Run"])
    _condition.drop_run = sw.isOn;
  else if ([label isEqualToString:@"Trim"] ||
           [label isEqualToString:@"Curved Segment"]) {
    // Trim and Curved Segment exclude each other (COND-TRIM-01): the switches are rebuilt from the condition so the other one
    // shows off (and Curved Segment disabled while Trim is on)
    [self.view endEditing:YES];
    if ([label isEqualToString:@"Trim"])
      ost_domain::set_trim(_condition, sw.isOn);
    else
      ost_domain::set_curved_segment(_condition, sw.isOn);
    [self _rebuildAdvRows];
    [self _updateDirty];
    [_tableView reloadSections:[NSIndexSet indexSetWithIndex:kSectionAdvanced]
              withRowAnimation:UITableViewRowAnimationNone];
    return;
  } else if ([label isEqualToString:@"Grid"])
    _condition.grid = sw.isOn;
  else if ([label isEqualToString:@"Display Pattern"])
    _condition.display_grid_while_drawing = sw.isOn;
  else if ([label isEqualToString:@"Display Dimension"])
    _condition.display_dimension = sw.isOn;
  else if ([label isEqualToString:@"Snap to Linear"])
    _condition.snap_to_linear = sw.isOn ? 0 : -1;
  [self _updateDirty];
}

@end
