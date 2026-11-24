#ifndef VIEWCONTROLLER_INTERNAL_H
#define VIEWCONTROLLER_INTERNAL_H

#import "ViewController.h"
#import "../../Services/ProjectService.h"
#import "../../PlanView/PlanView.h"
#import "../../Rendering/Renderer.h"
#import "../../Services/ImportService.h"
#import "../../Services/ExportService.h"
#import "Components/FloatingModeButton.h"
#import "../ConditionsViewController.h"
#import "../EditConditionViewController/EditConditionViewController.h"
#import <objc/runtime.h>
#include "../../Core/Domain/ConditionChoices.hpp"
#include "../../Core/Domain/EditMode.hpp"
#include <optional>
#include <string>
#include <vector>

static const int kFolderTagBase3D = 100;
static const int kPageTagBase3D = 10000;
static const int kPageTagBase2D = 20000;
static const int kModeLabelTag3D = 50000;
static const int kModeLabelTag2D = 50001;

@interface ViewController ()

@property(nonatomic, strong) ProjectService *projectService;
@property(nonatomic, strong) UIView *toolbar;
@property(nonatomic, strong) UIScrollView *pageBar3D;
@property(nonatomic, strong) UIScrollView *pageBar2D;
@property(nonatomic, strong) PlanView *planView;
@property(nonatomic, strong) Renderer *renderer;
@property(nonatomic, strong) FloatingModeButton *fab;
@property(nonatomic) BOOL is2DMode;
@property(nonatomic) ost_domain::EditMode editMode;
@property(nonatomic, copy) NSString *bidUID;
// An area placement is in progress (the page menu is disabled meanwhile, plan 4.8 and 5.5). The callback
// planView:areaPlacementInProgress: in ViewController.mm sets it (S3, one line; nothing sets it yet) and
// ViewController+PageMenu.mm reads it when the menu is shown. It lives here, not in a marker block below: the category
// ViewController (Internal) cannot hold a property, and the ivar-plus-accessor pattern of ViewController.mm would need more edits there.
@property(nonatomic) BOOL areaPlacementInProgress;

@end

// Methods on the main ViewController that categories call back into.
@interface ViewController (Internal)

- (void)_refreshOverlays;
- (void)_generateMeshesForSelectedPages;
- (void)_updateUndoRedoButtons;
- (void)_updateConditionQuantities;
// Drops the whole undo / redo history (a deleted condition invalidates it, UND-03).
- (void)_clearUndoHistory;
// Nothing stays selected after the 2D page changed (RPV-16).
- (void)_clearSelectionForPageSwitch;
- (void)_switchToMode2D:(BOOL)want2D;
- (void)_setEditMode:(ost_domain::EditMode)mode;
- (void)_loadCurrentPageIntoPlanViewResetZoom:(BOOL)resetZoom;
- (void)_highlightSelection;
- (void)_highlightModeLabels;
- (void)_populatePageSelector;
- (void)_presentEditConditionWithUID:(NSString *)conditionUID
                      fromController:(UIViewController *)presenter;

- (std::string &)selectedTakeoffUID;
- (std::string &)placeConditionUID;
- (NSString *)selectedConditionUID;
- (void)setSelectedConditionUID:(NSString *)uid;
- (int &)activeHandleIndex;
- (__weak ConditionsViewController *)conditionsVC;
- (void)setConditionsVC:(ConditionsViewController *)vc;
// The placement a hidden layer suspended and the tool revision it is checked against (COND-PLACE-01)
- (std::optional<ost_domain::SuspendedPlacement> &)suspendedPlacement;
- (unsigned long long)toolRevision;
- (void)_selectConditionForPlacement:(NSString *)conditionUID
                              action:(ost_domain::PlacementAction)action;
- (void)_leavePlacement;
- (void)_syncSelection;

// M1 (plan 6.1): each UI stream adds the declarations of its own category in its own block below (adjacent appends only).
// This category has NO storage: a block holds METHOD declarations only, never a property (a property declared here would have no ivar
// and no accessor). State goes into the class extension at the top of this file (areaPlacementInProgress is the one M1 flag).
// A stream declares EVERY method of its categories that another file calls or names in @selector(...) / addTarget:action:.

// ---- M1 bid lock (S0) ----

// The bid lock guard (plan 4.1, N-M1-02). S1 to S4 call it at the top of every blocked action: when it answers YES the action stops
// (nothing is written, "This bid is locked." has been shown). The only lock helper; no stream adds one of its own.
// Implemented in Components/ViewController+BidLock.mm.
- (BOOL)_refuseIfBidLocked;

// ---- M1 conditions (S1) ----

// ---- M1 layers (S2) ----

// ---- M1 pages A (S3) ----

// The page menu (plan 4.8), ViewController+PageMenu.mm: attaches the long-press menu to a page button of the 3D or the 2D bar (called by
// _selectorButtonWithTitle:tag:font:action: in ViewController+PageSelector.mm; the 3D folder buttons and the mode chips get none).
- (void)_attachPageMenuToButton:(UIButton *)button;
// The list of items of the menu, built each time the menu is shown (Set Scale, Rename Page, Delete Page: the order is documented there).
- (NSArray<UIMenuElement *> *)_pageMenuItemsForPageUID:(NSString *)pageUID
                                               onBar2D:(BOOL)onBar2D;
// The controller the page alerts are presented on (the topmost presented one) and an OK alert on it; `completion` runs after OK.
- (UIViewController *)_pagePresenter;
- (void)_presentPageAlertWithTitle:(NSString *)title
                           message:(NSString *)message
                        completion:(void (^)(void))completion;
// Rename Page (ViewController+PageRename.mm): the menu item's action, the alert, the commit of the typed name, and the text field's
// select-all, which addTarget:action: names.
- (void)_renamePageWithUID:(NSString *)pageUID;
- (void)_presentRenameAlertForPageUID:(NSString *)pageUID
                                draft:(NSString *)draft;
- (void)_commitRenameOfPageUID:(NSString *)pageUID
                     typedText:(NSString *)typed;
- (void)_pageRenameFieldDidBeginEditing:(UITextField *)field;
// Delete Page (ViewController+PageDelete.mm): the menu item's action, the write after the confirmation, and the reload that selects the
// page that followed the deleted one (YES when the bid was read again).
- (void)_deletePageWithUID:(NSString *)pageUID;
- (void)_performDeleteOfPageUID:(NSString *)pageUID;
- (BOOL)_reloadProjectSelectingPage:(NSString *)pageUID;

// ---- M1 pages B (S4) ----

@end

#endif
