#import "Components/EditConditionHelpers.h"
#import "EditConditionViewController_Internal.h"
#include "../../Core/Domain/ConditionChoices.hpp"
#include "../../Core/Domain/ConditionFormRules.hpp"
#include "../../Core/Domain/ConditionStyleChange.hpp"
#include "../../Core/Domain/DimensionFormat.hpp"
#include "../../Core/Import/TextRules.hpp"
#include <algorithm>
#include <cmath>

static NSString *OSTString(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

static std::string OSTStdString(NSString *text) {
  const char *utf8 = text.UTF8String;
  return utf8 ? std::string(utf8) : std::string();
}

// The dimension fields (feet-inch text, or millimetres in a metric bid), the ones that normalise their text when they lose focus
// (Windows _DimensionLineEdit). The elevation is a plain field and keeps what was typed (COND-ELEV-01).
static BOOL OSTIsDimensionTag(NSInteger tag) {
  return tag == kTagHeight || tag == kTagWidth || tag == kTagThickness ||
         tag == kTagDepth || tag == kTagTolerance || tag == kTagRoundTo ||
         tag == kTagAddLength || tag == kTagTileX || tag == kTagTileY ||
         tag == kTagGap || tag == kTagSpacing;
}

// The text field a style change resets (ost_domain::StyleField)
static NSInteger OSTTagForStyleField(ost_domain::StyleField field) {
  switch (field) {
  case ost_domain::StyleField::Height:
    return kTagHeight;
  case ost_domain::StyleField::Width:
    return kTagWidth;
  case ost_domain::StyleField::Depth:
    return kTagDepth;
  case ost_domain::StyleField::Thickness:
    return kTagThickness;
  case ost_domain::StyleField::DisplaySize:
    return kTagDispSize;
  case ost_domain::StyleField::Rise:
    return kTagRise;
  case ost_domain::StyleField::Run:
    return kTagRun;
  case ost_domain::StyleField::Tolerance:
    return kTagTolerance;
  case ost_domain::StyleField::RoundTo:
    return kTagRoundTo;
  case ost_domain::StyleField::AddLength:
    return kTagAddLength;
  case ost_domain::StyleField::TileX:
    return kTagTileX;
  case ost_domain::StyleField::TileY:
    return kTagTileY;
  case ost_domain::StyleField::Gap:
    return kTagGap;
  }
  return kTagHeight;
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
         mergedLayers:(const std::vector<ost_domain::BidLayer> &)mergedLayers
             cdnTypes:
                 (const std::unordered_map<std::string, ost_domain::CdnType> &)
                     cdnTypes
               metric:(BOOL)metric {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _metric = metric;
    _condition = condition;
    [self _normalizeLoadedCondition];
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

    // The layer list of Windows (COND-LAYER-01) and the type list in its
    // case-insensitive order (COND-CDNPICK-01)
    _sortedLayerUIDs = ost_domain::condition_layer_choices(mergedLayers, _layers);
    _sortedCdnTypeUIDs = ost_domain::sorted_cdn_type_uids(_cdnTypes);

    [self _syncFieldCacheFromCondition];
    [self _rebuildDimRows];
    [self _rebuildAdvRows];
    [self _markFormSaved];
  }
  return self;
}

// What the form shows of a condition that is loaded into it (Windows _load_condition, edit_condition_dialog.py :839-847 of the
// Windows 49a2d59): the stored style stays as it is (COND-M08). A type outside Linear / Area / Count /
// Attachment adds the disabled "Unknown (N)" entry to the style choice (_unknownStyleType, replaced or removed at the next load) and
// is built Area-like, and a Save without a style choice keeps it. Trim and Curved Segment never show both (COND-TRIM-01). A Save
// writes these values only when something else makes the form dirty.
- (void)_normalizeLoadedCondition {
  _condition = ost_domain::condition_form_for_edit(_condition);
  _unknownStyleType =
      ost_domain::unknown_style_type(_condition.condition_type);
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
  // a type outside Linear / Area / Count / Attachment is Area-like (COND-M08, edit_condition_dialog.py :547)
  const ost_domain::EditorLayout layout =
      ost_domain::editor_layout(_condition.condition_type);
  const bool isCount = (layout == ost_domain::EditorLayout::CountLike);
  const bool isLinear = (layout == ost_domain::EditorLayout::Linear);
  const bool isArea = (layout == ost_domain::EditorLayout::Area);

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
    // Windows shows abs(rise) / abs(run) for the Area style (DLG-RISE-01)
    _dimRows.push_back(
        {@"Rise", kTagRise, std::fabs(_condition.rise), DimFormat::Plain});
    _dimRows.push_back(
        {@"Run", kTagRun, std::fabs(_condition.run), DimFormat::Plain});
  }
}

- (void)_rebuildAdvRows {
  _advRows.clear();
  // everything that is not Linear or Count / Attachment is the Area rows, a type outside the four included (COND-M08)
  const ost_domain::EditorLayout layout =
      ost_domain::editor_layout(_condition.condition_type);
  const bool isCount = (layout == ost_domain::EditorLayout::CountLike);
  const bool isLinear = (layout == ost_domain::EditorLayout::Linear);

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
  [self _updateDirty];
}

- (void)_save {
  [self.view endEditing:YES];
  if (![self _applyChanges])
    return;
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - Dirty state and saving

// Windows _current_form_state: everything the form shows. The switches and pickers live in _condition; the typed texts (not yet
// parsed) live in the caches.
- (NSArray *)_formState {
  const ost_domain::Condition &c = _condition;
  return @[
    @(static_cast<int>(c.condition_type)), OSTString(c.cdn_type_uid),
    OSTString(c.layer_uid), @(c.color_fill), @(static_cast<int>(c.pattern)),
    @(static_cast<int>(c.shape)), @(c.calc_type1), @(c.uom1), @(c.calc_type2),
    @(c.uom2), @(c.calc_type3), @(c.uom3), @(c.round_quantity), @(c.drop_run),
    @(c.trim), @(c.is_curved_segment), @(c.grid),
    @(c.display_grid_while_drawing), @(c.display_dimension), @(c.display_name),
    @(c.connect), @(c.snap_to_linear), @(_elevParts.type),
    [_fieldTextCache copy], _notesTextCache ?: @""
  ];
}

- (void)_markFormSaved {
  _savedFormState = [self _formState];
  _dirty = NO;
}

// Windows _mark_dirty: the form is dirty when its state differs from the state it was loaded or last saved with, so changing a field
// back (or a focus change that changes nothing) leaves it clean.
- (void)_updateDirty {
  _dirty = ![[self _formState] isEqualToArray:_savedFormState];
}

// Windows _apply_changes: YES when the form was saved or had nothing to save; NO when a value was refused or the save failed (the
// alert is shown and nothing changes). Only the fields that differ from the stored condition are handed on (COND-UPD-02).
- (BOOL)_applyChanges {
  if (!_dirty)
    return YES;
  if (![self _validateAndCollectTextFields])
    return NO;

  auto storedIt = _conditionsMap.find(_condition.uid);
  const ost_domain::Condition stored =
      (storedIt != _conditionsMap.end()) ? storedIt->second : _condition;
  // Core builds the whole save: the condition to store is changes.values, the fields are those that differ from the stored condition
  // plus, on a style change, Spacing / Thickness / Backout even when equal (COND-STYLE-02).
  const ost_domain::ConditionChanges changes =
      ost_domain::condition_changes_for_save(stored, _condition, _metric);
  const ost_domain::Condition toSave = changes.values;
  if (changes.empty()) {
    // an empty change set is a success without a write
    [self _markFormSaved];
    return YES;
  }

  NSString *failureMessage = nil;
  if (_delegate && ![_delegate editConditionViewController:self
                                          didSaveCondition:toSave
                                                   changes:changes
                                            failureMessage:&failureMessage]) {
    [self _showSaveFailedAlertWithMessage:failureMessage];
    return NO;
  }

  // The write went through: the neighbours of a renumbered condition shift like the database did, then the condition is stored
  ost_domain::shift_conflicting_ref_nos(_conditionsMap, toSave.uid,
                                        toSave.ref_no);
  _conditionsMap[toSave.uid] = toSave;
  _condition = toSave;
  [self _syncFieldCacheFromCondition];
  [self _rebuildDimRows];
  [self _rebuildAdvRows];
  [self _markFormSaved];
  return YES;
}

// "Save Failed" with the reason the store gave (the Windows text of a renumber that would push another condition past the highest
// condition number, DLG-FINITE-01), or the generic text when it gave none (COND-UPD-03); Core picks (condition_save_failed_issue).
- (void)_showSaveFailedAlertWithMessage:(NSString *)message {
  const ost_domain::FormIssue issue =
      ost_domain::condition_save_failed_issue(OSTStdString(message));
  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:OSTString(issue.title)
                       message:OSTString(issue.message)
                preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (NSString *)_cachedTextForTag:(NSInteger)tag {
  NSString *cached = _fieldTextCache[@(tag)];
  return cached ?: @"";
}

// The text a dimension / plain field shows for the value of _condition: Core inches_to_display, str(value) (Python repr, so the text
// is the stored value, not six digits: DLG-PREC-01), str(int(display_size)); the Area style shows abs(rise) / abs(run) (DLG-RISE-01).
- (NSString *)_textForTag:(NSInteger)tag {
  // abs(rise) / abs(run) for every Area-like layout, a type outside the four included (COND-M08)
  const bool area = (ost_domain::editor_layout(_condition.condition_type) ==
                     ost_domain::EditorLayout::Area);
  switch (tag) {
  case kTagHeight:
    return inchesToDisplay(_condition.height, _metric);
  case kTagWidth:
    return inchesToDisplay(_condition.width, _metric);
  case kTagThickness:
    return inchesToDisplay(_condition.thickness, _metric);
  case kTagDepth:
    return inchesToDisplay(_condition.depth, _metric);
  case kTagDispSize:
    return OSTString(ost_domain::display_size_text(_condition.display_size));
  case kTagRise:
    return OSTString(ost_domain::plain_number_text(
        area ? std::fabs(_condition.rise) : _condition.rise));
  case kTagRun:
    return OSTString(ost_domain::plain_number_text(
        area ? std::fabs(_condition.run) : _condition.run));
  case kTagTolerance:
    return inchesToDisplay(_condition.connect_tolerance, _metric);
  case kTagRoundTo:
    return inchesToDisplay(_condition.round_up, _metric);
  case kTagAddLength:
    return inchesToDisplay(_condition.drop_value, _metric);
  case kTagTileX:
    return inchesToDisplay(_condition.grid_size1, _metric);
  case kTagTileY:
    return inchesToDisplay(_condition.grid_size2, _metric);
  case kTagGap:
    return inchesToDisplay(_condition.gap, _metric);
  case kTagSpacing:
    return inchesToDisplay(_condition.spacing, _metric);
  default:
    return @"";
  }
}

- (void)_syncFieldCacheFromCondition {
  _fieldTextCache[@(kTagName)] = OSTString(_elevParts.baseName);
  _fieldTextCache[@(kTagRefNo)] =
      [NSString stringWithFormat:@"%d", _condition.ref_no];
  _fieldTextCache[@(kTagElevVal)] = OSTString(_elevParts.value);

  static const NSInteger kValueTags[] = {
      kTagHeight,    kTagWidth,     kTagThickness, kTagDepth,    kTagDispSize,
      kTagRise,      kTagRun,       kTagTolerance, kTagRoundTo,  kTagAddLength,
      kTagTileX,     kTagTileY,     kTagGap,       kTagSpacing};
  for (NSInteger tag : kValueTags)
    _fieldTextCache[@(tag)] = [self _textForTag:tag];

  _notesTextCache = OSTString(_condition.notes);
}

// The Style picker (Windows _on_style_changed): the controls of the new style get their defaults, name, Cond. No., elevation, layer,
// type, notes, colour, pattern and the spacing stay as they are, the options both styles have are carried over, and the texts
// that were typed into fields the change does not reset stay as typed (COND-STYLE-01).
- (void)_applyStyleChangeTo:(ost_domain::ConditionType)conditionType {
  if (conditionType == _condition.condition_type)
    return; // the Windows combo reports a change only when the style differs
  // a text that is still being typed is part of the form: commit it before the texts of the new style are reset
  [self.view endEditing:YES];
  const ost_domain::StyleChange change =
      ost_domain::apply_style_change(_condition, conditionType, _metric);
  _condition = change.condition;
  for (ost_domain::StyleField field : change.reset_fields) {
    NSInteger tag = OSTTagForStyleField(field);
    _fieldTextCache[@(tag)] = [self _textForTag:tag];
  }
  [self _rebuildDimRows];
  [self _rebuildAdvRows];
  [self _updateDirty];
  [_tableView reloadData];
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

// Shows the alert of a refused value (the wording of Windows _validate_and_build_dto, Core ConditionFormRules.hpp) and gives the
// focus back to the field.
- (BOOL)_refuseWithIssue:(const ost_domain::FormIssue &)issue
                focusTag:(NSInteger)focusTag {
  [self _showValidationAlertWithTitle:OSTString(issue.title)
                              message:OSTString(issue.message)
                             focusTag:focusTag];
  return NO;
}

// A dimension field: blank is 0, else the feet-inch text (millimetres in a metric bid) must be a finite number (UI-MSG-01).
- (BOOL)_parseDimensionTag:(NSInteger)tag
                     label:(NSString *)label
                  outValue:(double *)outValue {
  double value = 0.0;
  const auto issue = ost_domain::parse_dimension_value(
      OSTStdString([self _cachedTextForTag:tag]), OSTStdString(label), _metric,
      value);
  if (issue)
    return [self _refuseWithIssue:*issue focusTag:tag];
  *outValue = value;
  return YES;
}

// Rise / Run: an EMPTY field is 0, else float() text that is finite; the raw field text goes to Core (never trimmed: float(" ") is
// "Rise must be a number." like Windows, DLG-FINITE-01); the Area style keeps abs() (DLG-RISE-01).
- (BOOL)_parseSlopeTag:(NSInteger)tag
                 label:(NSString *)label
              absolute:(BOOL)absolute
              outValue:(double *)outValue {
  double value = 0.0;
  const auto issue = ost_domain::parse_slope_value(
      OSTStdString([self _cachedTextForTag:tag]), OSTStdString(label),
      absolute, value);
  if (issue)
    return [self _refuseWithIssue:*issue focusTag:tag];
  *outValue = value;
  return YES;
}

// Windows _validate_and_build_dto: every text of the form is parsed into `form`; the first refused value shows its alert and stops.
// `_condition` is replaced only when everything parsed, so a refusal leaves the form model as it was.
- (BOOL)_validateAndCollectTextFields {
  ost_domain::Condition form = _condition;

  std::string baseName;
  if (const auto issue = ost_domain::parse_condition_name(
          OSTStdString([self _cachedTextForTag:kTagName]), baseName)) {
    return [self _refuseWithIssue:*issue focusTag:kTagName];
  }

  // The elevation text is kept as typed: it becomes the suffix of the name (COND-ELEV-01). It is validated with the loader's own
  // parser (parse_elevation_z calls elevation_text_inches, Windows b9902e6): a text the loader cannot read (a bare number, mm, units
  // of both systems, "5'" alone, a zero denominator) is refused with the forms that are accepted, like Windows. The dialog also
  // refuses a text without a closing inch mark in which a letter or digit follows the foot mark ("5'6", "10'6", "5'abc"; "5'x" and
  // "5'6\"" pass), which the loader would read as the feet alone (elevation_text_needs_closing_inch_mark, called by parse_elevation_z;
  // Windows 49a2d59, edit_condition_dialog.py :72, :1444); the alert is the same. The saved condition
  // does not take is_top or z from the form: condition_to_save derives both from the saved NAME like a reload does (decided
  // 2026-11-19); for a text that passed, the number checked here is the one that derivation gives, so memory equals a reload.
  const std::string elevValue = ost_import::text_rules::py_strip(
      OSTStdString([self _cachedTextForTag:kTagElevVal]));
  const int elevType =
      _elevTypeSeg ? (int)_elevTypeSeg.selectedSegmentIndex : _elevParts.type;
  _elevParts.baseName = baseName;
  _elevParts.type = elevType;
  _elevParts.value = elevValue;

  double checkedElevation = 0.0;
  if (const auto issue = ost_domain::parse_elevation_z(elevValue, _metric,
                                                       checkedElevation)) {
    return [self _refuseWithIssue:*issue focusTag:kTagElevVal];
  }
  form.name = reassembleElevation(baseName, elevType, elevValue);

  // Cond. No. is validated, never coerced (COND-REF-01). The layer stays as it is: an empty layer is no layer (COND-LAYER-01).
  if (const auto issue = ost_domain::parse_condition_number(
          OSTStdString([self _cachedTextForTag:kTagRefNo]), form.ref_no)) {
    return [self _refuseWithIssue:*issue focusTag:kTagRefNo];
  }

  // The fields to parse are those of the layout the editor built (a type outside the four is Area-like, COND-M08), so no field that
  // was never built is read.
  const ost_domain::EditorLayout layout =
      ost_domain::editor_layout(form.condition_type);
  const bool isCount = (layout == ost_domain::EditorLayout::CountLike);
  const bool isLinear = (layout == ost_domain::EditorLayout::Linear);

  if (isCount) {
    if (![self _parseDimensionTag:kTagHeight label:@"Height" outValue:&form.height] ||
        ![self _parseDimensionTag:kTagWidth label:@"Width" outValue:&form.width] ||
        ![self _parseDimensionTag:kTagDepth label:@"Depth" outValue:&form.depth]) {
      return NO;
    }
    if (const auto issue = ost_domain::parse_display_size(
            OSTStdString([self _cachedTextForTag:kTagDispSize]),
            form.display_size)) {
      return [self _refuseWithIssue:*issue focusTag:kTagDispSize];
    }
  } else if (isLinear) {
    if (![self _parseDimensionTag:kTagHeight label:@"Height" outValue:&form.height] ||
        ![self _parseDimensionTag:kTagThickness label:@"Thickness" outValue:&form.thickness] ||
        ![self _parseSlopeTag:kTagRise label:@"Rise" absolute:NO outValue:&form.rise] ||
        ![self _parseSlopeTag:kTagRun label:@"Run" absolute:NO outValue:&form.run]) {
      return NO;
    }
  } else {
    if (![self _parseDimensionTag:kTagThickness label:@"Thickness" outValue:&form.thickness] ||
        ![self _parseSlopeTag:kTagRise label:@"Rise" absolute:YES outValue:&form.rise] ||
        ![self _parseSlopeTag:kTagRun label:@"Run" absolute:YES outValue:&form.run]) {
      return NO;
    }
  }

  if (![self _parseDimensionTag:kTagSpacing label:@"Spacing" outValue:&form.spacing])
    return NO;

  if (isCount) {
    if (![self _parseDimensionTag:kTagTolerance label:@"Tolerance" outValue:&form.connect_tolerance])
      return NO;
  } else if (isLinear) {
    if (![self _parseDimensionTag:kTagRoundTo label:@"Round to" outValue:&form.round_up] ||
        ![self _parseDimensionTag:kTagAddLength label:@"Add Length" outValue:&form.drop_value] ||
        ![self _parseDimensionTag:kTagTolerance label:@"Tolerance" outValue:&form.connect_tolerance]) {
      return NO;
    }
  } else {
    if (![self _parseDimensionTag:kTagRoundTo label:@"Round to" outValue:&form.round_up] ||
        ![self _parseDimensionTag:kTagTileX label:@"Tile X" outValue:&form.grid_size1] ||
        ![self _parseDimensionTag:kTagTileY label:@"Tile Y" outValue:&form.grid_size2] ||
        ![self _parseDimensionTag:kTagGap label:@"Gap" outValue:&form.gap]) {
      return NO;
    }
  }

  form.notes = OSTStdString(_notesTextCache ?: @"");
  _condition = form;
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
  [self _normalizeLoadedCondition];
  _elevParts = parseElevation(_condition.name);
  _styleDisabled = [self _conditionHasTakeoffs:uid];
  _nameField = nil;
  _refNoField = nil;
  _notesView = nil;
  _elevTypeSeg = nil;
  _elevValueField = nil;
  [self _syncFieldCacheFromCondition];

  [self _rebuildDimRows];
  [self _rebuildAdvRows];
  [self _markFormSaved];
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
                              // a refused value or a failed save keeps the form and does not navigate
                              if ([self _applyChanges]) {
                                [self _loadConditionAtIndex:index];
                              }
                            }]];

  [alert addAction:[UIAlertAction actionWithTitle:@"Discard"
                                            style:UIAlertActionStyleDestructive
                                          handler:^(UIAlertAction *a) {
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
  NSInteger tag = textField.tag;

  if (OSTIsDimensionTag(tag)) {
    // Windows _DimensionLineEdit.focusOutEvent: a text that parses is shown in the canonical form. The elevation is not one of
    // these fields, its text stays as typed (COND-ELEV-01).
    const std::string text = OSTStdString(textField.text);
    if (!ost_import::text_rules::py_strip(text).empty()) {
      if (const auto value = ost_domain::display_to_inches(text, _metric))
        textField.text = inchesToDisplay(*value, _metric);
    }
  }

  _fieldTextCache[@(tag)] = textField.text ?: @"";
  [self _updateDirty];
}

- (void)textViewDidChange:(UITextView *)textView {
  if (textView.tag != kTagNotes)
    return;
  _notesTextCache = textView.text ?: @"";
  [self _updateDirty];
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
