// Set Scale on the main controller (plan 5.4). The entry point (the page menu item "Set Scale..." calls it), the delegate of the form that writes through
// ImportService+PageScale and brings the project and the views up to date (plan 4.11), and the two optional UndoRedoDelegate methods that make the undo
// history survive a scale change (plan 4.7, N-M1-06). No rule lives here: the page list, the context check, the memory twin, the refresh plan and the capture
// lookup are Core functions (PageScaleRules, PageScaleTransform, PageScaleApply, PageScaleCapture); this file converts types, shows UI and calls them.
#import "../ViewController_Internal.h"
#import "../../../Services/ImportService+PageScale.h"
#import "../../PageScaleViewController.h"
#import "UndoRedoController.h"
#include "../../../Core/Domain/PageScale.hpp"
#include "../../../Core/Domain/PageScaleApply.hpp"
#include "../../../Core/Domain/PageScaleCapture.hpp"
#include "../../../Core/Domain/PageScaleRules.hpp"
#include "../../../Core/Domain/PageScaleTransform.hpp"
#include <optional>
#include <string>
#include <vector>

// The main controller is the delegate of the form; the adoption is declared here, in this file only (a category of its own, so that no shared header changes).
@interface ViewController (PageScale) <PageScaleViewControllerDelegate>
@end

static NSString *OSTPageScaleText(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

static std::string OSTPageScaleStdString(NSString *text) {
  const char *utf8 = text.UTF8String;
  return utf8 ? std::string(utf8) : std::string();
}

@implementation ViewController (PageScale)

#pragma mark - Entry points (declared in the block "M1 pages B (S4)" of ViewController_Internal.h)

// The page menu item "Set Scale..." acts on the PRESSED page (plan 4.8, 5.4), which is not always the active one. The lock guard comes first: a locked bid
// shows "This bid is locked." and no form opens (plan 4.1, N-M1-02).
- (void)_presentSetScaleForPageUID:(NSString *)pageUID {
  if ([self _refuseIfBidLocked])
    return;
  // Windows blocks the page settings while an area is being placed (ui_access_manager.py:166-191); the page menu is disabled then, this is the second line
  if (self.areaPlacementInProgress)
    return;

  const auto &pages = self.projectService.project.pages;
  const auto found = pages.find(OSTPageScaleStdString(pageUID));
  if (found == pages.end())
    return;
  const ost_domain::Page &page = found->second;

  PageScaleViewController *form =
      [[PageScaleViewController alloc] initWithPageUID:pageUID
                                              pageName:OSTPageScaleText(page.name)
                                                bidUID:self.bidUID
                                          scaleFactor1:page.scale_factor1
                                          scaleFactor2:page.scale_factor2];
  form.delegate = self;
  UINavigationController *nav =
      [[UINavigationController alloc] initWithRootViewController:form];
  nav.modalPresentationStyle = UIModalPresentationFormSheet;

  // a form sheet on a centred sheet on iPad, full width on iPhone; no popover inside the form (N-M1-25)
  UIViewController *presenter = self;
  while (presenter.presentedViewController)
    presenter = presenter.presentedViewController;
  [presenter presentViewController:nav animated:YES completion:nil];
}

// The page the 2D view shows now.
- (void)_presentSetScaleForCurrentPage {
  const std::string &active = self.projectService.selected2DPageUID;
  if (active.empty())
    return;
  [self _presentSetScaleForPageUID:OSTPageScaleText(active)];
}

// The subtitle of the menu item: the label of the page bar of Windows (page_settings_bar.py:159-206): the preset label, else the custom label; nil when the
// page's pair has no label (nothing is selected on the Windows bar) or the page is unknown.
- (NSString *)_setScaleSubtitleForPageUID:(NSString *)pageUID {
  const auto &pages = self.projectService.project.pages;
  const auto found = pages.find(OSTPageScaleStdString(pageUID));
  if (found == pages.end())
    return nil;
  const std::string label = ost_domain::scale_display_label(
      found->second.scale_factor1, found->second.scale_factor2);
  return label.empty() ? nil : OSTPageScaleText(label);
}

#pragma mark - PageScaleViewControllerDelegate

// "Apply to all pages" confirms only when it writes more than one page (N-M1-21): the pages are the Core list of the Save
- (NSUInteger)pageScaleViewControllerPageCountForApplyToAll:
    (PageScaleViewController *)controller {
  return ost_domain::page_setting_uids(
             self.projectService.project,
             OSTPageScaleStdString(controller.pageUID), true)
      .size();
}

// The write of a Save: lock, context, pages, ONE all-or-nothing transaction, then the memory twin and the refresh. Nothing in memory changes unless the
// database was written (plan 4.11).
- (PageScaleSaveResult)pageScaleViewController:(PageScaleViewController *)controller
                                didRequestSave:(const ost_domain::ScaleSettings &)settings {
  // The bid may have been locked since the form opened: the guard shows "This bid is locked." and the form stays open
  if ([self _refuseIfBidLocked])
    return PageScaleSaveResultLocked;

  ost_data::ProjectData &project = self.projectService.mutableProject;
  const std::string pageUID = OSTPageScaleStdString(controller.pageUID);

  // Windows _page_dialog_context_is_current (:5401-5409) adapted to the pressed page: the same bid, the page still there and still holding the pair the
  // form was opened on; the bid is unlocked here (the guard above). A refused context is the Windows "Save Failed" (the save closure answers False).
  const ost_domain::PageDialogContext context = ost_domain::page_dialog_context(
      project, OSTPageScaleStdString(self.bidUID),
      OSTPageScaleStdString(controller.bidUID), pageUID,
      controller.openedScaleFactor1, controller.openedScaleFactor2, true);
  if (!context.current()) {
    NSLog(@"Set Scale refused: selection_current=%d page_present=%d page_current=%d edit_allowed=%d",
          context.selection_current, context.page_present, context.page_current,
          context.edit_allowed);
    return PageScaleSaveResultFailed;
  }

  // the pressed page, or every page of the bid in page-bar order (Core)
  const std::vector<std::string> uids =
      ost_domain::page_setting_uids(project, pageUID, settings.apply_to_all_pages);
  if (uids.empty())
    return PageScaleSaveResultFailed;

  BOOL locked = NO;
  NSString *errorText = nil;
  if (![ImportService savePageScales:uids
                               inBid:self.bidUID
                        scaleFactor1:settings.scale_factor1
                        scaleFactor2:settings.scale_factor2
                              locked:&locked
                           errorText:&errorText]) {
    // the store refuses a locked bid again inside its transaction; the guard says so (a bid that unlocked meanwhile is a plain failure)
    if (locked && [self _refuseIfBidLocked])
      return PageScaleSaveResultLocked;
    NSLog(@"Failed to save page scale: %@", errorText ?: @"");
    return PageScaleSaveResultFailed;
  }

  // The memory twin: it equals a reload of the pages from the database (positions rescaled, the page's pair, the overlay rectangle). It refuses for every
  // reason the store refuses, so a mismatch cannot happen after a write that worked; if it ever does, the project is read again rather than left stale.
  const std::vector<std::string> changed = ost_domain::apply_page_scales(
      project.pages, project.takeoffs, uids, settings.scale_factor1,
      settings.scale_factor2);
  if (changed.size() != uids.size())
    [self _reloadProjectAfterPageScaleMismatch];
  else
    [self _refreshAfterPageScaleChange:changed];
  return PageScaleSaveResultSaved;
}

#pragma mark - Refresh (plan 4.11)

// What a Set Scale refreshes (Windows ui_event_coordinator.py:3318-3359), as the Core plan says: the plan view of the active page when it changed (the zoom
// is kept: PlanView caches the page scale at loadPage), the 3D scene when a selected page changed, the quantities when the active or a selected page changed.
// The undo history is NOT cleared: its entries carry the page scale they were made under.
- (void)_refreshAfterPageScaleChange:(const std::vector<std::string> &)changedPageUIDs {
  const ost_domain::PageScaleRefresh refresh = ost_domain::plan_page_scale_refresh(
      changedPageUIDs, self.projectService.selected2DPageUID,
      self.projectService.selected3DPageUIDs, self.is2DMode);
  if (refresh.regenerate_meshes)
    [self _generateMeshesForSelectedPages];
  if (refresh.reload_plan_view)
    [self _loadCurrentPageIntoPlanViewResetZoom:NO];
  if (refresh.refresh_quantities)
    [self _updateConditionQuantities];
}

// The safety net of the memory twin: read the bid again, drop the history (its captured scales would not match) and the selection, and redraw everything.
- (void)_reloadProjectAfterPageScaleMismatch {
  ost_data::ProjectData reloaded;
  NSString *error = nil;
  if (![ImportService loadProjectWithUID:self.bidUID
                                 project:reloaded
                                   error:&error]) {
    NSLog(@"Set Scale: the bid could not be read again: %@", error ?: @"");
    return;
  }
  [self.projectService loadProject:reloaded];
  [self _clearUndoHistory];
  [self _clearSelectionForPageSwitch];
  [self _populatePageSelector];
  [self _highlightModeLabels];
  [self _generateMeshesForSelectedPages];
  if (self.is2DMode)
    [self _loadCurrentPageIntoPlanViewResetZoom:NO];
  [self _updateConditionQuantities];
}

#pragma mark - UndoRedoDelegate, the page scale capture (plan 4.7, 5.4, N-M1-06)

// The two OPTIONAL methods of UndoRedoDelegate (UndoRedoController.h), implemented here so that ViewController.mm is not edited. A takeoff's history entry
// stores the page and the page scale pair it was made under; at replay the controller asks the pair of that page NOW and rescales the positions
// (Core replay_entry_positions). The lookups are Core (PageScaleCapture).
- (std::optional<ost_domain::PageScale>)undoRedoPageScaleForTakeoff:(const std::string &)uid
                                                            pageUID:(std::string &)pageUID {
  const std::optional<ost_domain::TakeoffPageScale> captured =
      ost_domain::page_scale_of_takeoff(self.projectService.project, uid);
  if (!captured)
    return std::nullopt;
  pageUID = captured->page_uid;
  return captured->scale;
}

- (std::optional<ost_domain::PageScale>)undoRedoPageScaleForPage:(const std::string &)pageUID {
  return ost_domain::page_scale_of_page(self.projectService.project, pageUID);
}

@end
