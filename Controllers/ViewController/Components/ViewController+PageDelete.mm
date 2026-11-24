// Delete Page (plan 5.6, decisions N-M1-05, N-M1-23): the action of the page menu item "Delete Page" (2D bar only). The rules are Core's:
// the plan ost_data::plan_page_delete (the last page cannot go, which page is shown next), the content scan of the bridge, the wording
// ost_domain::page_delete_confirmation, the failure texts. The write is +[ImportService deletePages:inBid:error:] (one transaction, the
// whole cascade or nothing). A page that is deleted cannot be brought back, so the confirmation is ALWAYS asked and Cancel is its default.
#include "../../../Core/Data/PageDeletePlan.hpp"
#include "../../../Core/Data/PageMenuRules.hpp"
#include "../../../Core/Domain/PageDeletion.hpp"
#include "../../../Core/Domain/PageDialogText.hpp"
#import "../../../Services/ImportService+PageDelete.h"
#import "../ViewController_Internal.h"
#include <set>

static NSString *OSTPageDeleteNS(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

static BOOL OSTPageDeleteUTF8(NSString *text, std::string &utf8) {
  NSData *data = [text dataUsingEncoding:NSUTF8StringEncoding];
  if (!data)
    return NO;
  utf8.assign(static_cast<const char *>(data.bytes), (size_t)data.length);
  return YES;
}

@implementation ViewController (PageDelete)

// The bid is read again after the delete and the page that followed the deleted one is shown, the only page selected in 2D and 3D
// (Windows _stage_selection_after_page_delete and _stage_takeoff_restore, register DP-10). Not _loadProject:, which opens the saved
// page: this one is told which page to select. The undo history is cleared (its entries name takeoffs of the deleted page, DP-8), the
// placement is left, the selected takeoff is dropped, the totals follow the new page.
//
// NO when the bid could not be read again: the delete was COMMITTED, so the user is told the page is gone (register DP-2: Windows says
// "Failed to delete page" there) and the bid closes when the alert is dismissed, because the memory still holds the deleted page.
- (BOOL)_reloadProjectSelectingPage:(NSString *)pageUID {
  ost_data::ProjectData reloaded;
  NSString *reason = nil;
  const BOOL read = [ImportService loadProjectWithUID:self.bidUID
                                              project:reloaded
                                                error:&reason];
  std::string next;
  if (!read || reloaded.empty() || !OSTPageDeleteUTF8(pageUID, next)) {
    NSLog(@"Delete Page: the bid could not be read again: %@",
          reason ?: @"(no reason)");
    std::string reasonText;
    if (reason.length > 0)
      OSTPageDeleteUTF8(reason, reasonText);
    const ost_domain::DialogText text =
        ost_domain::page_delete_reload_failed_text(reasonText);
    __weak ViewController *weakSelf = self;
    [self _presentPageAlertWithTitle:OSTPageDeleteNS(text.title)
                             message:OSTPageDeleteNS(text.message)
                          completion:^{
                            [weakSelf.navigationController
                                popViewControllerAnimated:YES];
                          }];
    return NO;
  }

  [self.projectService loadProject:reloaded selectingPage:next];
  [self _leavePlacement];
  [self _clearSelectionForPageSwitch];
  [self _clearUndoHistory];
  // _populatePageSelector ends in _highlightSelection, which dims the 2D / 3D chips: _highlightModeLabels runs after it, like _loadProject:.
  [self _populatePageSelector];
  [self _highlightModeLabels];
  [self _generateMeshesForSelectedPages];
  [self.renderer fitCameraToMeshes];
  [self _loadCurrentPageIntoPlanViewResetZoom:YES];
  [self _updateConditionQuantities];
  return YES;
}

// The user confirmed: the plan is made again (nothing may have changed, but the write never trusts the question), then the write.
- (void)_performDeleteOfPageUID:(NSString *)pageUID {
  std::string uid;
  if (!OSTPageDeleteUTF8(pageUID, uid))
    return;
  const ost_data::PageDeletePlan plan =
      ost_data::plan_page_delete(self.projectService.project, uid);
  if (!plan.allowed)
    return;

  NSString *reason = nil;
  const BOOL deleted = [ImportService deletePages:@[ pageUID ]
                                            inBid:self.bidUID
                                            error:&reason];
  if (!deleted) {
    // Nothing was written and the selection is unchanged. The last page has its own text; every other refusal is the generic one with
    // the database-locked hint (ui_event_coordinator.py:5962-5968).
    NSLog(@"Delete Page was refused: %@", reason ?: @"(no reason)");
    const BOOL lastPage =
        [reason isEqualToString:OSTPageDeleteNS(ost_domain::LAST_PAGE_MESSAGE)];
    const ost_domain::DialogText failed =
        lastPage ? ost_domain::DialogText{ost_domain::LAST_PAGE_TITLE,
                                          ost_domain::LAST_PAGE_MESSAGE}
                 : ost_domain::page_delete_failure_text(
                       ost_domain::PageDeleteFailure::Delete);
    [self _presentPageAlertWithTitle:OSTPageDeleteNS(failed.title)
                             message:OSTPageDeleteNS(failed.message)
                          completion:nil];
    return;
  }
  [self _reloadProjectSelectingPage:OSTPageDeleteNS(plan.next_page_uid)];
}

// The action of the item "Delete Page" (the menu of ViewController+PageMenu.mm): the pressed page. The lock guard comes first, so a
// locked bid explains itself before any alert opens.
- (void)_deletePageWithUID:(NSString *)pageUID {
  if ([self _refuseIfBidLocked])
    return;
  std::string uid;
  if (!OSTPageDeleteUTF8(pageUID, uid))
    return;
  // The menu disables the item during an area placement; the state is asked again here because the menu was built before the tap.
  if (self.areaPlacementInProgress)
    return;
  const ost_data::ProjectData &project = self.projectService.project;
  const ost_data::PageDeletePlan plan = ost_data::plan_page_delete(project, uid);
  if (!plan.allowed) {
    // Windows warns about the last page and says nothing about a page it does not know (ui_event_coordinator.py:5995-6001).
    if (plan.refusal == ost_data::PageDeleteRefusal::LastPage)
      [self _presentPageAlertWithTitle:OSTPageDeleteNS(ost_domain::LAST_PAGE_TITLE)
                               message:OSTPageDeleteNS(ost_domain::LAST_PAGE_MESSAGE)
                            completion:nil];
    return;
  }

  // What the page holds comes from the database. A read that fails refuses the delete before any question (nothing is written).
  NSString *scanReason = nil;
  NSSet<NSString *> *scanned =
      [ImportService pagesWithDeleteContentInBid:self.bidUID error:&scanReason];
  if (!scanned) {
    NSLog(@"Delete Page: the content scan failed: %@", scanReason ?: @"(no reason)");
    const ost_domain::DialogText failed = ost_domain::page_delete_failure_text(
        ost_domain::PageDeleteFailure::VerifyContent);
    [self _presentPageAlertWithTitle:OSTPageDeleteNS(failed.title)
                             message:OSTPageDeleteNS(failed.message)
                          completion:nil];
    return;
  }
  std::set<std::string> pagesWithContent;
  for (NSString *scannedUID in scanned) {
    std::string scannedText;
    if (OSTPageDeleteUTF8(scannedUID, scannedText))
      pagesWithContent.insert(scannedText);
  }
  const ost_domain::DialogText question = ost_domain::page_delete_confirmation(
      plan.display_name,
      ost_data::page_delete_has_content(project, uid, pagesWithContent));

  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:OSTPageDeleteNS(question.title)
                       message:OSTPageDeleteNS(question.message)
                preferredStyle:UIAlertControllerStyleAlert];
  __weak ViewController *weakSelf = self;
  UIAlertAction *cancel = [UIAlertAction actionWithTitle:@"Cancel"
                                                   style:UIAlertActionStyleCancel
                                                 handler:nil];
  [alert addAction:cancel];
  [alert addAction:[UIAlertAction
                       actionWithTitle:@"Delete"
                                 style:UIAlertActionStyleDestructive
                               handler:^(UIAlertAction *action) {
                                 [weakSelf _performDeleteOfPageUID:pageUID];
                               }]];
  // Cancel is the default (Windows: the question defaults to No): it is the action of a hardware Return key.
  alert.preferredAction = cancel;
  [[self _pagePresenter] presentViewController:alert
                                      animated:YES
                                    completion:nil];
}

@end
