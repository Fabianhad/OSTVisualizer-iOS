#import "../EditConditionViewController_Internal.h"
#import "EditConditionHelpers.h"

@interface EditConditionViewController (Pickers) <
    UIColorPickerViewControllerDelegate>
@end

@implementation EditConditionViewController (Pickers)

#pragma mark - UITableView Delegate

- (void)tableView:(UITableView *)tableView
    didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  if (indexPath.section == kSectionTop) {
    TopRow row = static_cast<TopRow>(indexPath.row);
    switch (row) {
    case TopRow::Style:
      if (!_styleDisabled)
        [self _showPickerForStyle];
      break;
    case TopRow::CdnType:
      [self _showPickerForCdnType];
      break;
    case TopRow::Layer:
      [self _showPickerForLayer];
      break;
    default:
      break;
    }
  } else if (indexPath.section == kSectionAppearance) {
    if (indexPath.row == 0)
      [self _showColorPicker];
    else if (indexPath.row == 1)
      [self _showPickerForPattern];
    else {
      NSInteger shapeRow = isLinePattern(_condition.pattern) ? 3 : 2;
      if (indexPath.row == shapeRow)
        [self _showPickerForShape];
    }
  } else if (indexPath.section == kSectionResults) {
    [self _showPickerForQuantity:indexPath.row];
  }
}

#pragma mark - Style / Type / Layer Pickers

- (void)_showPickerForStyle {
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:@"Style"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  NSArray *labels = styleLabels();
  for (NSInteger i = 0; i < (NSInteger)labels.count; i++) {
    NSInteger typeVal = i;
    [alert
        addAction:[UIAlertAction
                      actionWithTitle:labels[i]
                                style:UIAlertActionStyleDefault
                              handler:^(UIAlertAction *a) {
                                [self
                                    _applyStyleDefaults:
                                        static_cast<ost_domain::ConditionType>(
                                            typeVal)];
                                self->_dirty = YES;
                                [self _rebuildDimRows];
                                [self _rebuildAdvRows];
                                [self->_tableView reloadData];
                              }]];
  }
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_showPickerForCdnType {
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:@"Type"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  [alert
      addAction:[UIAlertAction actionWithTitle:@"(none)"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction *a) {
                                         self->_condition.cdn_type_uid.clear();
                                         self->_condition.cdn_type_name.clear();
                                         self->_dirty = YES;
                                         [self->_tableView reloadData];
                                       }]];
  for (const auto &uid : _sortedCdnTypeUIDs) {
    NSString *name =
        [NSString stringWithUTF8String:_cdnTypes.at(uid).name.c_str()];
    std::string capUID = uid;
    std::string capName = _cdnTypes.at(uid).name;
    [alert addAction:[UIAlertAction
                         actionWithTitle:name
                                   style:UIAlertActionStyleDefault
                                 handler:^(UIAlertAction *a) {
                                   self->_condition.cdn_type_uid = capUID;
                                   self->_condition.cdn_type_name = capName;
                                   self->_dirty = YES;
                                   [self->_tableView reloadData];
                                 }]];
  }
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_showPickerForLayer {
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:@"Layer"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  for (const auto &uid : _sortedLayerUIDs) {
    NSString *name =
        [NSString stringWithUTF8String:_layers.at(uid).name.c_str()];
    std::string capUID = uid;
    [alert addAction:[UIAlertAction actionWithTitle:name
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction *a) {
                                              self->_condition.layer_uid =
                                                  capUID;
                                              self->_dirty = YES;
                                              [self->_tableView reloadData];
                                            }]];
  }
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

#pragma mark - Appearance Pickers

- (void)_showPickerForPattern {
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:@"Pattern"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  NSArray *labels = patternLabels();
  for (NSInteger i = 0; i < (NSInteger)labels.count; i++) {
    NSInteger val = i;
    [alert addAction:[UIAlertAction
                         actionWithTitle:labels[i]
                                   style:UIAlertActionStyleDefault
                                 handler:^(UIAlertAction *a) {
                                   self->_condition.pattern =
                                       static_cast<ost_domain::Pattern>(val);
                                   self->_dirty = YES;
                                   [self->_tableView reloadData];
                                 }]];
  }
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_showPickerForShape {
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:@"Shape"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  NSArray *labels = shapeLabels();
  for (NSInteger i = 0; i < (NSInteger)labels.count; i++) {
    int val = shapeValues[i];
    [alert addAction:[UIAlertAction
                         actionWithTitle:labels[i]
                                   style:UIAlertActionStyleDefault
                                 handler:^(UIAlertAction *a) {
                                   self->_condition.shape =
                                       static_cast<ost_domain::ShapeId>(val);
                                   self->_dirty = YES;
                                   [self _rebuildDimRows];
                                   [self->_tableView reloadData];
                                 }]];
  }
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_showColorPicker {
  if (@available(iOS 14.0, *)) {
    UIColorPickerViewController *picker =
        [[UIColorPickerViewController alloc] init];
    int cf = _condition.color_fill;
    CGFloat r = (cf & 0xFF) / 255.0;
    CGFloat g = ((cf >> 8) & 0xFF) / 255.0;
    CGFloat b = ((cf >> 16) & 0xFF) / 255.0;
    picker.selectedColor = [UIColor colorWithRed:r green:g blue:b alpha:1.0];
    picker.supportsAlpha = NO;
    picker.delegate = (id<UIColorPickerViewControllerDelegate>)self;
    [self presentViewController:picker animated:YES completion:nil];
  }
}

#pragma mark - UIColorPickerViewControllerDelegate

- (void)colorPickerViewControllerDidFinish:
    (UIColorPickerViewController *)viewController API_AVAILABLE(ios(14.0)) {
  UIColor *color = viewController.selectedColor;
  CGFloat r, g, b, a;
  [color getRed:&r green:&g blue:&b alpha:&a];
  int ri = (int)(r * 255.0) & 0xFF;
  int gi = (int)(g * 255.0) & 0xFF;
  int bi = (int)(b * 255.0) & 0xFF;
  _condition.color_fill = ri | (gi << 8) | (bi << 16);
  _condition.color_line = _condition.color_fill;
  _dirty = YES;
  [_tableView reloadData];
}

#pragma mark - Quantity / UOM Pickers

- (void)_showPickerForQuantity:(NSInteger)qtyIndex {
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:[NSString stringWithFormat:@"Quantity %ld",
                                                          (long)(qtyIndex + 1)]
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  [alert addAction:[UIAlertAction actionWithTitle:@"(none)"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction *a) {
                                            [self _setQuantity:qtyIndex
                                                      calcType:0
                                                           uom:0];
                                          }]];

  auto options =
      ost_domain::get_quantity_options_for_type(_condition.condition_type);
  for (const auto &opt : options) {
    int calcType = opt.first;
    NSString *label = [NSString stringWithUTF8String:opt.second.c_str()];
    [alert addAction:[UIAlertAction actionWithTitle:label
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction *a) {
                                              [self _showPickerForUom:qtyIndex
                                                             calcType:calcType];
                                            }]];
  }

  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_showPickerForUom:(NSInteger)qtyIndex calcType:(int)calcType {
  auto uomOptions = ost_domain::get_valid_uoms_for_calc_type(calcType);

  if (uomOptions.size() == 1) {
    [self _setQuantity:qtyIndex calcType:calcType uom:uomOptions[0].first];
    return;
  }

  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:@"Unit of Measure"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  for (const auto &opt : uomOptions) {
    int uomCode = opt.first;
    NSString *label = [NSString stringWithUTF8String:opt.second.c_str()];
    [alert addAction:[UIAlertAction actionWithTitle:label
                                              style:UIAlertActionStyleDefault
                                            handler:^(UIAlertAction *a) {
                                              [self _setQuantity:qtyIndex
                                                        calcType:calcType
                                                             uom:uomCode];
                                            }]];
  }

  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)_setQuantity:(NSInteger)qtyIndex calcType:(int)calcType uom:(int)uom {
  switch (qtyIndex) {
  case 0:
    _condition.calc_type1 = calcType;
    _condition.uom1 = (calcType == 0) ? -1 : uom;
    break;
  case 1:
    _condition.calc_type2 = calcType;
    _condition.uom2 = (calcType == 0) ? -1 : uom;
    break;
  case 2:
    _condition.calc_type3 = calcType;
    _condition.uom3 = (calcType == 0) ? -1 : uom;
    break;
  }
  _dirty = YES;
  [_tableView reloadData];
}

@end
