#ifndef EDITCONDITIONVIEWCONTROLLER_INTERNAL_H
#define EDITCONDITIONVIEWCONTROLLER_INTERNAL_H

#import "EditConditionViewController.h"
#include "../../Core/Domain/PositionParser.hpp"
#include "../../Core/Domain/UomService.hpp"
#include <optional>
#include <vector>
#include <string>
#include <unordered_map>

// --- Section indices ---
static const NSInteger kSectionTop = 0;
static const NSInteger kSectionDimensions = 1;
static const NSInteger kSectionAppearance = 2;
static const NSInteger kSectionResults = 3;
static const NSInteger kSectionNotes = 4;
static const NSInteger kSectionAdvanced = 5;
static const NSInteger kSectionCount = 6;

// --- Top section row enum ---
enum class TopRow : int
{
    Style = 0,
    CdnType,
    Name,
    Elevation,
    Layer,
    RefNo,
    Count
};

// --- Tag constants ---
static const NSInteger kTagName = 100;
static const NSInteger kTagRefNo = 101;
static const NSInteger kTagElevVal = 102;
static const NSInteger kTagHeight = 200;
static const NSInteger kTagWidth = 201;
static const NSInteger kTagThickness = 202;
static const NSInteger kTagDepth = 203;
static const NSInteger kTagDispSize = 204;
static const NSInteger kTagRise = 205;
static const NSInteger kTagRun = 206;
static const NSInteger kTagNotes = 300;
static const NSInteger kTagTolerance = 400;
static const NSInteger kTagRoundTo = 401;
static const NSInteger kTagAddLength = 402;
static const NSInteger kTagTileX = 403;
static const NSInteger kTagTileY = 404;
static const NSInteger kTagGap = 405;
static const NSInteger kTagSpacing = 406;

// Labels of the Advanced switches (Windows texts.py EDIT_CONDITION_DIALOG_OPTION_*, sentence case). They are also the dispatch keys
// of _advToggleChanged:; the row builders use the same constants, so a label is changed in one place.
static NSString *const kAdvLabelDisplayName = @"Display name";
static NSString *const kAdvLabelRoundQuantity = @"Round quantity";
static NSString *const kAdvLabelDisplayDimension = @"Display dimension";
static NSString *const kAdvLabelCurvedSegment = @"Set as curved segment";
static NSString *const kAdvLabelDisplayPattern = @"Display pattern while drawing";

// --- Dimension row model ---
enum class DimFormat
{
    FeetInches,
    Plain,
    Percent
};

struct DimRow
{
    NSString *label;
    NSInteger tag;
    double value;
    DimFormat format;
};

// --- Advanced row model ---
enum class AdvRowType
{
    Toggle,
    Dimension
};

struct AdvRow
{
    NSString *label;
    AdvRowType type;
    NSInteger tag;
    bool boolVal;
    double dimVal;
};

// --- Class extension — ivars visible to all category .mm files ---
@interface EditConditionViewController () <UITableViewDataSource, UITableViewDelegate, UITextFieldDelegate, UITextViewDelegate>
{
@package
    UITableView *_tableView;
    ost_domain::Condition _condition;
    std::unordered_map<std::string, ost_domain::Layer> _layers;
    std::unordered_map<std::string, ost_domain::CdnType> _cdnTypes;
    std::unordered_map<std::string, ost_domain::Condition> _conditionsMap;
    std::vector<ost_domain::Takeoff> _takeoffs;
    std::vector<std::string> _conditionUIDs;
    std::vector<std::string> _sortedLayerUIDs;
    std::vector<std::string> _sortedCdnTypeUIDs;
    std::vector<DimRow> _dimRows;
    std::vector<AdvRow> _advRows;
    // The bid's measurement system (project.bid.measure_base == 1): dimensions are millimetres (COND-METRIC-01)
    BOOL _metric;
    // The form differs from the state it had when it was loaded or last saved (Windows _dirty / _saved_form_state)
    BOOL _dirty;
    NSArray *_savedFormState;
    BOOL _styleDisabled;
    // The stored type of the loaded condition when it is outside Linear / Area / Count / Attachment: the style choice then lists a
    // disabled "Unknown (N)" entry after Attachment (COND-M08). It is set at every load (init and Previous / Next, both through
    // _normalizeLoadedCondition), so loading another condition replaces or removes it.
    std::optional<ost_domain::ConditionType> _unknownStyleType;
    NSInteger _currentIndex;

    UITextField *_nameField;
    UITextField *_refNoField;
    UITextView *_notesView;
    UISegmentedControl *_elevTypeSeg;
    UITextField *_elevValueField;
    NSMutableDictionary<NSNumber *, NSString *> *_fieldTextCache;
    NSString *_notesTextCache;
    // The elevation of the name in the form: ost_domain::parse_elevation of the loaded name (base_name, type 0 top / 1 bottom, value)
    ost_domain::ElevationParts _elevParts;
    UIBarButtonItem *_prevButton;
    UIBarButtonItem *_nextButton;
    UIBarButtonItem *_saveButton;

    // --- Create mode (New Condition, plan 5.1): nothing of this is used by the Edit mode ---
    BOOL _createMode;
    // A Save is being written / was written: a second tap (a double tap) creates nothing
    BOOL _createInFlight;
    BOOL _createDone;
    // The Name field took the focus once, when the form first appeared
    BOOL _createFocused;
    // The synthetic condition (ost_domain::new_condition_form): the "stored" condition a Save is compared with, the folder the condition is
    // created in ("" = the top level), the bid's own default layer ("" = none) and the text of the read-only Cond. No. row
    ost_domain::Condition _createForm;
    std::string _createFolderUID;
    std::string _createDefaultLayerUID;
    NSString *_nextNumberText;
}
@end

// Methods on the main implementation that categories call back into.
@interface EditConditionViewController (Internal)
- (void)_rebuildDimRows;
- (void)_rebuildAdvRows;
- (void)_syncFieldCacheFromCondition;
// The Style picker: a different style gets the defaults of its controls, everything else stays as typed (COND-STYLE-01).
- (void)_applyStyleChangeTo:(ost_domain::ConditionType)conditionType;
// Recomputes the dirty flag after a change of a text, a switch or a picker.
- (void)_updateDirty;
- (BOOL)_validateAndCollectTextFields;
// Create mode: the form notices a text while it is typed (a UIControlEventEditingChanged action on the field); no-op in the Edit mode.
- (void)_noticeTypingInField:(UITextField *)field;
- (NSString *)_cachedTextForTag:(NSInteger)tag;
- (UITableViewCell *)_detailCell:(UITableView *)table id:(NSString *)cellId;
- (UITableViewCell *)_textFieldCell:(UITableView *)table
                              label:(NSString *)label
                              value:(NSString *)value
                                tag:(NSInteger)tag;
@end

#endif
