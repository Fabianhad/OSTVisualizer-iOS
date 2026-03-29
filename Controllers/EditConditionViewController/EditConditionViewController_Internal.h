#ifndef EDITCONDITIONVIEWCONTROLLER_INTERNAL_H
#define EDITCONDITIONVIEWCONTROLLER_INTERNAL_H

#import "EditConditionViewController.h"
#include "../../Core/Domain/UomService.hpp"
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

// --- Elevation parsing result ---
struct ElevationParts
{
    std::string baseName;
    int type;
    std::string value;
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
    BOOL _dirty;
    BOOL _styleDisabled;
    NSInteger _currentIndex;

    UITextField *_nameField;
    UITextField *_refNoField;
    UITextView *_notesView;
    UISegmentedControl *_elevTypeSeg;
    UITextField *_elevValueField;
    NSMutableDictionary<NSNumber *, NSString *> *_fieldTextCache;
    NSString *_notesTextCache;
    ElevationParts _elevParts;
    UIBarButtonItem *_prevButton;
    UIBarButtonItem *_nextButton;
}
@end

// Methods on the main implementation that categories call back into.
@interface EditConditionViewController (Internal)
- (void)_rebuildDimRows;
- (void)_rebuildAdvRows;
- (void)_syncFieldCacheFromCondition;
- (void)_applyStyleDefaults:(ost_domain::ConditionType)conditionType;
- (BOOL)_validateAndCollectTextFields;
- (NSString *)_cachedTextForTag:(NSInteger)tag;
- (UITableViewCell *)_detailCell:(UITableView *)table id:(NSString *)cellId;
- (UITableViewCell *)_textFieldCell:(UITableView *)table
                              label:(NSString *)label
                              value:(NSString *)value
                                tag:(NSInteger)tag;
@end

#endif
