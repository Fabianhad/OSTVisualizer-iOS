// The Set Scale form (plan 5.4, touch adaptation). The layout follows the Windows dialog (set_scale_dialog.py:51-132): the Scale Style, the "Pre-defined
// scale" row with the presets of that style, the "Custom scale" row with the two numbers, "Apply to all pages". Everything the form decides is a Core function
// (ost_domain::ScaleFormSession, validate_scale_form, scale_form_after_style_change, scale_form_after_preset_pick, scale_apply_all_confirmation,
// scale_form_footer_text); this file shows the rows, converts the types and hands a Save to the delegate.
#import "PageScaleViewController.h"
#include "../Core/Domain/PageScale.hpp"
#include "../Core/Domain/PageScaleApply.hpp"
#include "../Core/Domain/PageScaleRules.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

static NSString *OSTScaleText(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

static std::string OSTScaleStdString(NSString *text) {
  const char *utf8 = text.UTF8String;
  return utf8 ? std::string(utf8) : std::string();
}

typedef NS_ENUM(NSInteger, PageScaleSection) {
  PageScaleSectionStyle = 0,
  PageScaleSectionScale = 1,
  PageScaleSectionApplyAll = 2,
  PageScaleSectionCount = 3
};

// The rows of the scale section (the Windows grid rows 1 and 2: the Pre-defined radio with its presets, the Custom radio with its two numbers).
typedef NS_ENUM(NSInteger, PageScaleRow) {
  PageScaleRowPredefined = 0,
  PageScaleRowCustom = 1,
  PageScaleRowCustomFields = 2,
  PageScaleRowCount = 3
};

static const NSInteger kCustomField1Tag = 1;
static const NSInteger kCustomField2Tag = 2;
// Room, in points, that the table gets below its last row while one of the two numbers is edited: the keyboard covers the lower part of a small phone
static const CGFloat kKeyboardRoom = 300.0;

@interface PageScaleViewController () <UITableViewDataSource,
                                       UITableViewDelegate, UITextFieldDelegate>
- (void)_cancel;
- (void)_save;
- (void)_styleChanged:(UISegmentedControl *)control;
- (void)_applyAllChanged:(UISwitch *)control;
- (void)_customFieldChanged:(UITextField *)field;
@end

@implementation PageScaleViewController {
  // the baseline and the dirty state of the Windows dialog (_saved_form_state)
  std::unique_ptr<ost_domain::ScaleFormSession> _session;
  // what the form shows now (the tuple of _current_form_state)
  ost_domain::ScaleFormState _state;
  NSString *_pageName;
  UITableView *_tableView;
  // the two custom fields of the visible row: the alert of a refused value focuses one of them
  UITextField *_customField1;
  UITextField *_customField2;
  // saved, the form is closing: a second tap on Save does nothing
  BOOL _finished;
}

@synthesize delegate = _delegate;

#pragma mark - Init

- (instancetype)initWithPageUID:(NSString *)pageUID
                       pageName:(NSString *)pageName
                         bidUID:(NSString *)bidUID
                   scaleFactor1:(double)scaleFactor1
                   scaleFactor2:(double)scaleFactor2 {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _pageUID = [pageUID copy];
    _pageName = [pageName copy] ?: @"";
    _bidUID = [bidUID copy];
    _openedScaleFactor1 = scaleFactor1;
    _openedScaleFactor2 = scaleFactor2;
    // the state at open and its baseline (set_scale_dialog.py:134-156); a page with no usable scale opens on Custom
    _session = std::make_unique<ost_domain::ScaleFormSession>(scaleFactor1,
                                                              scaleFactor2);
    _state = _session->initial();
  }
  return self;
}

#pragma mark - Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  // the Windows title (set_scale_dialog.py:_setup_ui)
  self.title = @"Set Scale";
  self.view.backgroundColor = [UIColor systemGroupedBackgroundColor];

  // Cancel + Save only (N-M1-21): the Windows OK is Save, Apply has no use over a modal form; the form stays open when a save is refused
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(_cancel)];
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemSave
                           target:self
                           action:@selector(_save)];

  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStyleInsetGrouped];
  _tableView.dataSource = self;
  _tableView.delegate = self;
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
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

#pragma mark - Table data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  return PageScaleSectionCount;
}

- (NSInteger)tableView:(UITableView *)tableView
    numberOfRowsInSection:(NSInteger)section {
  return section == PageScaleSectionScale ? PageScaleRowCount : 1;
}

- (NSString *)tableView:(UITableView *)tableView
    titleForHeaderInSection:(NSInteger)section {
  switch (section) {
  case PageScaleSectionStyle:
    return @"Scale style:";
  case PageScaleSectionScale:
    return @"Scale";
  default:
    return nil;
  }
}

// The first line is the current scale of the page with the label of the Windows page bar, the second says what the two numbers are (Core text)
- (NSString *)tableView:(UITableView *)tableView
    titleForFooterInSection:(NSInteger)section {
  if (section != PageScaleSectionScale)
    return nil;
  return OSTScaleText(ost_domain::scale_form_footer_text(
      OSTScaleStdString(_pageName), _openedScaleFactor1, _openedScaleFactor2));
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  switch (indexPath.section) {
  case PageScaleSectionStyle:
    return [self _styleCell];
  case PageScaleSectionApplyAll:
    return [self _applyAllCell];
  default:
    break;
  }
  switch (indexPath.row) {
  case PageScaleRowPredefined:
    return [self _predefinedCell];
  case PageScaleRowCustom:
    return [self _customCell];
  default:
    return [self _customFieldsCell];
  }
}

#pragma mark - Cells

// The Scale Style (Windows combo: 1 Architectural, 2 Civil, 3 Metric)
- (UITableViewCell *)_styleCell {
  UITableViewCell *cell =
      [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                             reuseIdentifier:nil];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  UISegmentedControl *control = [[UISegmentedControl alloc]
      initWithItems:@[ @"Architectural", @"Civil", @"Metric" ]];
  control.translatesAutoresizingMaskIntoConstraints = NO;
  control.selectedSegmentIndex = MAX(0, MIN(2, _state.style - 1));
  [control addTarget:self
                action:@selector(_styleChanged:)
      forControlEvents:UIControlEventValueChanged];
  [cell.contentView addSubview:control];

  UILayoutGuide *margins = cell.contentView.layoutMarginsGuide;
  [NSLayoutConstraint activateConstraints:@[
    [control.topAnchor constraintEqualToAnchor:cell.contentView.topAnchor
                                      constant:8],
    [control.bottomAnchor constraintEqualToAnchor:cell.contentView.bottomAnchor
                                         constant:-8],
    [control.leadingAnchor constraintEqualToAnchor:margins.leadingAnchor],
    [control.trailingAnchor constraintEqualToAnchor:margins.trailingAnchor],
  ]];
  return cell;
}

// "Predefined scale" and the presets of the style. The radio is the checkmark; the preset is a pull-down menu on the right (a UIMenu is anchored by the
// system, no popover anchor is needed), the row marked in it is the preset of the state.
- (UITableViewCell *)_predefinedCell {
  UITableViewCell *cell =
      [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                             reuseIdentifier:nil];
  cell.accessoryType = _state.mode == ost_domain::ScaleInputMode::Predefined
                           ? UITableViewCellAccessoryCheckmark
                           : UITableViewCellAccessoryNone;

  UILabel *title = [[UILabel alloc] init];
  title.text = @"Predefined scale";
  [title setContentHuggingPriority:UILayoutPriorityDefaultLow
                           forAxis:UILayoutConstraintAxisHorizontal];

  const std::vector<ost_domain::ScalePreset> &presets =
      ost_domain::scale_presets(_state.style);
  const int markedRow = ost_domain::scale_form_preset_index(_state);
  NSMutableArray<UIMenuElement *> *actions = [NSMutableArray array];
  __weak typeof(self) weakSelf = self;
  for (size_t i = 0; i < presets.size(); ++i) {
    const int row = static_cast<int>(i);
    UIAction *action = [UIAction
        actionWithTitle:OSTScaleText(presets[i].label)
                  image:nil
             identifier:nil
                handler:^(__unused UIAction *picked) {
                  [weakSelf _pickPresetAtRow:row];
                }];
    action.state =
        row == markedRow ? UIMenuElementStateOn : UIMenuElementStateOff;
    [actions addObject:action];
  }
  UIButton *picker = [UIButton buttonWithType:UIButtonTypeSystem];
  [picker setTitle:markedRow >= 0 ? OSTScaleText(presets[markedRow].label)
                                  : @"Select"
          forState:UIControlStateNormal];
  picker.showsMenuAsPrimaryAction = YES;
  picker.menu = [UIMenu menuWithTitle:@"" children:actions];
  picker.enabled = !presets.empty();

  UIStackView *stack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ title, picker ]];
  stack.axis = UILayoutConstraintAxisHorizontal;
  stack.alignment = UIStackViewAlignmentCenter;
  stack.spacing = 12;
  [self _pinStack:stack inCell:cell];
  return cell;
}

- (UITableViewCell *)_customCell {
  UITableViewCell *cell =
      [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                             reuseIdentifier:nil];
  cell.textLabel.text = @"Custom scale";
  cell.accessoryType = _state.mode == ost_domain::ScaleInputMode::Custom
                           ? UITableViewCellAccessoryCheckmark
                           : UITableViewCellAccessoryNone;
  return cell;
}

// The two numbers, "first : second" (Windows custom_row); numbers and punctuation, so that a '.' can be typed in every locale (N-M1-22): a decimal comma
// is typed as ',' and refused with the Windows text. Enabled only with the Custom radio on.
- (UITableViewCell *)_customFieldsCell {
  UITableViewCell *cell =
      [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                             reuseIdentifier:nil];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  _customField1 = [self _customFieldWithTag:kCustomField1Tag
                                       text:_state.custom1];
  _customField2 = [self _customFieldWithTag:kCustomField2Tag
                                       text:_state.custom2];
  UILabel *colon = [[UILabel alloc] init];
  colon.text = @":";
  [colon setContentHuggingPriority:UILayoutPriorityRequired
                           forAxis:UILayoutConstraintAxisHorizontal];
  UIStackView *stack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _customField1, colon, _customField2 ]];
  stack.axis = UILayoutConstraintAxisHorizontal;
  stack.alignment = UIStackViewAlignmentCenter;
  stack.distribution = UIStackViewDistributionFill;
  stack.spacing = 8;
  [self _pinStack:stack inCell:cell];
  [NSLayoutConstraint activateConstraints:@[
    [_customField1.widthAnchor constraintEqualToAnchor:_customField2.widthAnchor]
  ]];
  return cell;
}

- (UITextField *)_customFieldWithTag:(NSInteger)tag text:(const std::string &)text {
  UITextField *field = [[UITextField alloc] init];
  field.tag = tag;
  field.text = OSTScaleText(text);
  field.borderStyle = UITextBorderStyleRoundedRect;
  field.textAlignment = NSTextAlignmentCenter;
  field.keyboardType = UIKeyboardTypeNumbersAndPunctuation;
  field.autocorrectionType = UITextAutocorrectionTypeNo;
  field.autocapitalizationType = UITextAutocapitalizationTypeNone;
  field.smartDashesType = UITextSmartDashesTypeNo;
  field.smartQuotesType = UITextSmartQuotesTypeNo;
  field.returnKeyType =
      tag == kCustomField1Tag ? UIReturnKeyNext : UIReturnKeyDone;
  field.clearButtonMode = UITextFieldViewModeWhileEditing;
  field.delegate = self;
  const BOOL custom = _state.mode == ost_domain::ScaleInputMode::Custom;
  field.enabled = custom;
  field.alpha = custom ? 1.0 : 0.4;
  [field addTarget:self
                action:@selector(_customFieldChanged:)
      forControlEvents:UIControlEventEditingChanged];
  return field;
}

- (UITableViewCell *)_applyAllCell {
  UITableViewCell *cell =
      [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                             reuseIdentifier:nil];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.textLabel.text = @"Apply to all pages";
  UISwitch *toggle = [[UISwitch alloc] init];
  toggle.on = _state.apply_to_all;
  [toggle addTarget:self
                action:@selector(_applyAllChanged:)
      forControlEvents:UIControlEventValueChanged];
  cell.accessoryView = toggle;
  return cell;
}

- (void)_pinStack:(UIStackView *)stack inCell:(UITableViewCell *)cell {
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  [cell.contentView addSubview:stack];
  UILayoutGuide *margins = cell.contentView.layoutMarginsGuide;
  [NSLayoutConstraint activateConstraints:@[
    [stack.topAnchor constraintEqualToAnchor:cell.contentView.topAnchor
                                    constant:6],
    [stack.bottomAnchor constraintEqualToAnchor:cell.contentView.bottomAnchor
                                       constant:-6],
    [stack.leadingAnchor constraintEqualToAnchor:margins.leadingAnchor],
    [stack.trailingAnchor constraintEqualToAnchor:margins.trailingAnchor],
  ]];
}

#pragma mark - Table delegate

// Choosing a row of the radios (Windows: the two radio buttons); the presets and the numbers are changed in the row itself
- (void)tableView:(UITableView *)tableView
    didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  if (indexPath.section != PageScaleSectionScale)
    return;
  [self.view endEditing:YES];
  if (indexPath.row == PageScaleRowPredefined) {
    _state.mode = ost_domain::ScaleInputMode::Predefined;
    [self _reloadScaleSection];
  } else if (indexPath.row == PageScaleRowCustom) {
    _state.mode = ost_domain::ScaleInputMode::Custom;
    [self _reloadScaleSection];
    // the reason to choose Custom is to type: the first number takes the keyboard
    [self _focusCustomField:1];
  }
}

- (void)_reloadScaleSection {
  [_tableView reloadSections:[NSIndexSet indexSetWithIndex:PageScaleSectionScale]
            withRowAnimation:UITableViewRowAnimationNone];
  // the cells (and the two custom fields the alert focuses) are made now, not at the next layout pass
  [_tableView layoutIfNeeded];
}

#pragma mark - Form changes

// The user changed the Scale Style (Windows _on_style_changed): the presets of that style, the previous pair kept when the new list has it, the Pre-defined
// radio on (Core)
- (void)_styleChanged:(UISegmentedControl *)control {
  [self.view endEditing:YES];
  _state = ost_domain::scale_form_after_style_change(
      _state, static_cast<int>(control.selectedSegmentIndex) + 1);
  [self _reloadScaleSection];
}

// A row of the preset menu was chosen (Core: the pair of that row, the Pre-defined radio on)
- (void)_pickPresetAtRow:(int)row {
  [self.view endEditing:YES];
  _state = ost_domain::scale_form_after_preset_pick(_state, row);
  [self _reloadScaleSection];
}

- (void)_applyAllChanged:(UISwitch *)control {
  _state.apply_to_all = control.on;
}

// The text as typed; the strip, the number grammar and the messages are Core (validate_scale_form)
- (void)_customFieldChanged:(UITextField *)field {
  const std::string text = OSTScaleStdString(field.text);
  if (field.tag == kCustomField1Tag)
    _state.custom1 = text;
  else
    _state.custom2 = text;
}

// The text of the two numbers as it stands now: Windows reads edit.text() when it validates (set_scale_dialog.py:202-203). _customFieldChanged: keeps the state
// current while typing; this covers a change that sent no editing event (the clear button). Only a field that is on screen counts: the field of a cell the
// table dropped (a reload while the row was scrolled out of sight) holds an old text, and the state is newer.
- (void)_syncCustomTextFromFields {
  if (_customField1.window)
    _state.custom1 = OSTScaleStdString(_customField1.text);
  if (_customField2.window)
    _state.custom2 = OSTScaleStdString(_customField2.text);
}

#pragma mark - Text field delegate

// The table is a plain view controller's, so it does not make room for the keyboard by itself: while a number is edited the table can scroll its row above
// the keyboard, and the row is brought into view. The row goes to the TOP of the table (not the middle): with a short visible height (a phone in
// landscape) the middle of the table is where the keyboard starts, and the room below the content lets the table scroll far enough for the top.
- (void)textFieldDidBeginEditing:(UITextField *)textField {
  UIEdgeInsets inset = _tableView.contentInset;
  inset.bottom = kKeyboardRoom;
  _tableView.contentInset = inset;
  [_tableView scrollToRowAtIndexPath:[NSIndexPath indexPathForRow:PageScaleRowCustomFields
                                                        inSection:PageScaleSectionScale]
                    atScrollPosition:UITableViewScrollPositionTop
                            animated:YES];
}

// The room is given back when neither number is being edited any more (moving from the first to the second ends one edit and starts the next)
- (void)textFieldDidEndEditing:(UITextField *)textField {
  __weak typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf _releaseKeyboardRoomIfIdle];
  });
}

- (void)_releaseKeyboardRoomIfIdle {
  if (_customField1.isFirstResponder || _customField2.isFirstResponder)
    return;
  UIEdgeInsets inset = _tableView.contentInset;
  inset.bottom = 0;
  _tableView.contentInset = inset;
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
  if (textField.tag == kCustomField1Tag) {
    [_customField2 becomeFirstResponder];
  } else {
    [textField resignFirstResponder];
  }
  return NO;
}

#pragma mark - Cancel and Save

// Windows Cancel: closes without writing, a dirty draft is dropped. The form is finished from the moment Cancel is accepted (the same flag as
// a Save that closes it), so a Save that arrives while the dismissal animation still runs writes nothing: the user cancelled this form. Like
// Save it does nothing while an alert is up (the dismissal would then close the alert and leave the form open with the flag set).
- (void)_cancel {
  if (_finished || self.presentedViewController)
    return;
  [self.view endEditing:YES];
  _finished = YES;
  [self dismissViewControllerAnimated:YES completion:nil];
}

// Windows OK (_on_ok / _apply_changes, set_scale_dialog.py:264-319): not dirty closes; a refused value shows its alert and the form stays open; then, when
// "Apply to all pages" reaches more than one page, one confirmation (N-M1-21); then the delegate writes.
- (void)_save {
  if (_finished || self.presentedViewController)
    return;
  [self.view endEditing:YES];
  [self _syncCustomTextFromFields];
  if (!_session->dirty(_state)) {
    [self _finishAndDismiss];
    return;
  }

  ost_domain::ScaleSettings settings;
  int focusField = 0;
  const std::optional<ost_domain::FormIssue> issue =
      ost_domain::validate_scale_form(_state, settings, &focusField);
  if (issue) {
    [self _presentIssue:*issue focusField:focusField];
    return;
  }

  if (settings.apply_to_all_pages) {
    const NSUInteger pages =
        [_delegate pageScaleViewControllerPageCountForApplyToAll:self];
    const std::optional<ost_domain::ScaleConfirmation> confirmation =
        ost_domain::scale_apply_all_confirmation(
            static_cast<std::size_t>(pages));
    if (confirmation) {
      [self _presentConfirmation:*confirmation];
      return;
    }
  }
  [self _commit];
}

// The write. The Core session validates again, calls the delegate once, and moves the baseline when it worked.
- (void)_commit {
  id<PageScaleViewControllerDelegate> delegate = _delegate;
  PageScaleViewController *controller = self;
  PageScaleSaveResult delegateResult = PageScaleSaveResultFailed;
  std::optional<ost_domain::FormIssue> issue;
  int focusField = 0;
  const ost_domain::ScaleApplyResult result = _session->apply(
      _state,
      [&](const ost_domain::ScaleSettings &settings) {
        if (!delegate)
          return false;
        delegateResult = [delegate pageScaleViewController:controller
                                            didRequestSave:settings];
        return delegateResult == PageScaleSaveResultSaved;
      },
      issue, &focusField);

  switch (result) {
  case ost_domain::ScaleApplyResult::NothingToSave:
  case ost_domain::ScaleApplyResult::Saved:
    [self _finishAndDismiss];
    break;
  case ost_domain::ScaleApplyResult::Invalid:
    if (issue)
      [self _presentIssue:*issue focusField:focusField];
    break;
  case ost_domain::ScaleApplyResult::SaveFailed:
    // "This bid is locked." has been shown by the delegate; every other refusal is the Windows "Save Failed" alert
    if (delegateResult != PageScaleSaveResultLocked && issue) {
      if (delegateResult == PageScaleSaveResultUnstorable)
        issue = ost_domain::scale_save_failed_issue(true);
      [self _presentIssue:*issue focusField:0];
    }
    break;
  }
}

- (void)_finishAndDismiss {
  _finished = YES;
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - Alerts

// The Windows warning (title + message + OK); the field the Windows dialog focuses takes the keyboard after OK
- (void)_presentIssue:(const ost_domain::FormIssue &)issue
           focusField:(int)focusField {
  __weak typeof(self) weakSelf = self;
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:OSTScaleText(issue.title)
                       message:OSTScaleText(issue.message)
                preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:^(__unused UIAlertAction *a) {
                                            [weakSelf _focusCustomField:focusField];
                                          }]];
  [self presentViewController:alert animated:YES completion:nil];
}

// One confirmation before "Apply to all pages" writes more than one page; Cancel keeps the form as it is
- (void)_presentConfirmation:(const ost_domain::ScaleConfirmation &)confirmation {
  __weak typeof(self) weakSelf = self;
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:OSTScaleText(confirmation.title)
                       message:OSTScaleText(confirmation.message)
                preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:OSTScaleText(confirmation.cancel_title)
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  [alert addAction:[UIAlertAction actionWithTitle:OSTScaleText(confirmation.confirm_title)
                                            style:UIAlertActionStyleDefault
                                          handler:^(__unused UIAlertAction *a) {
                                            [weakSelf _commit];
                                          }]];
  [self presentViewController:alert animated:YES completion:nil];
}

// 1 and 2 are the custom fields (validate_scale_form's focus_field), anything else focuses nothing
- (void)_focusCustomField:(int)focusField {
  UITextField *field = focusField == 1   ? _customField1
                       : focusField == 2 ? _customField2
                                         : nil;
  [field becomeFirstResponder];
}

@end
