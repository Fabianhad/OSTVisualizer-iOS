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

@end

#endif
