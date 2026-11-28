// Rename Page (plan 5.5, decisions N-M1-05, N-M1-24): the action of the page menu item "Rename Page...". An alert with one text field
// (Windows: a modal dialog with the read-only "Old name:" and the editable "New name:"). The rules are Core's: the open guard
// ost_data::page_menu_state / ost_domain::can_rename_page, the name rule ost_domain::decide_page_rename (strip, blank, length, NUL,
// unchanged), the texts. The write is the bridge +[ImportService renamePageWithUID:inBid:toName:error:]; the name is applied in memory
// only after it said YES. A rename touches nothing else: no mesh regeneration, no plan reload, no selection change, no undo entry.
#include "../../../Core/Data/PageMenuRules.hpp"
#include "../../../Core/Domain/PageDialogText.hpp"
#include "../../../Core/Domain/PageRules.hpp"
#import "../../../Services/ImportService+PageRename.h"
#import "../ViewController_Internal.h"

static NSString *OSTPageRenameNS(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

// The UTF-8 bytes of `text` with their explicit length (a NUL inside is kept for Core to refuse, UTF8String would cut the text there).
// NO when the text cannot be represented in UTF-8 (a lone surrogate).
static BOOL OSTPageRenameUTF8(NSString *text, std::string &utf8) {
  NSData *data = [text dataUsingEncoding:NSUTF8StringEncoding];
  if (!data)
    return NO;
  utf8.assign(static_cast<const char *>(data.bytes), (size_t)data.length);
  return YES;
}

// The bars were rebuilt: a button the user had scrolled to must stay where it was (the contentSize assignment of _populateBar: can move
// the offset). The offset is kept inside the new content.
static void OSTPageRenameRestoreOffset(UIScrollView *bar, CGPoint offset) {
  const CGFloat maxX = MAX(bar.contentSize.width - bar.bounds.size.width, 0);
  bar.contentOffset = CGPointMake(MIN(MAX(offset.x, 0), maxX), offset.y);
}

@implementation ViewController (PageRename)

// The Windows dialog opens with the whole name selected (rename_page_dialog.py:131-134). The system places the caret after the editing
// event, so the selection is made on the next turn of the run loop. Named in addTarget:action: below, declared in ViewController_Internal.h.
// The range is set directly: selectAll: can also bring up the Cut / Copy / Paste menu over the alert.
- (void)_pageRenameFieldDidBeginEditing:(UITextField *)field {
  __weak UITextField *weakField = field;
  dispatch_async(dispatch_get_main_queue(), ^{
    UITextField *strongField = weakField;
    if (!strongField)
      return;
    strongField.selectedTextRange =
        [strongField textRangeFromPosition:strongField.beginningOfDocument
                                toPosition:strongField.endOfDocument];
  });
}

// The rename alert for the page `pageUID`, the text field filled with `draft` (the stored name when it opens, the typed text when it comes
// back after a refusal or a failed write: Windows keeps the dialog open with the draft). Cancel closes it; Rename is the preferred action.
- (void)_presentRenameAlertForPageUID:(NSString *)pageUID
                                draft:(NSString *)draft {
  std::string uid;
  if (!OSTPageRenameUTF8(pageUID, uid))
    return;
  const auto page = self.projectService.project.pages.find(uid);
  if (page == self.projectService.project.pages.end())
    return;
  const ost_domain::DialogText prompt =
      ost_domain::page_rename_prompt_text(page->second.name);

  UIAlertController *alert = [UIAlertController
      alertControllerWithTitle:OSTPageRenameNS(prompt.title)
                       message:OSTPageRenameNS(prompt.message)
                preferredStyle:UIAlertControllerStyleAlert];
  __weak ViewController *weakSelf = self;
  __block UITextField *nameField = nil;
  [alert addTextFieldWithConfigurationHandler:^(UITextField *field) {
    nameField = field;
    field.text = draft;
    // The "New name:" label of the Windows dialog (the old name is in the message).
    field.placeholder = @"New name";
    // Sheet names such as "A-101": no capital letters, no corrections, no smart quotes or dashes.
    field.autocapitalizationType = UITextAutocapitalizationTypeNone;
    field.autocorrectionType = UITextAutocorrectionTypeNo;
    field.smartQuotesType = UITextSmartQuotesTypeNo;
    field.smartDashesType = UITextSmartDashesTypeNo;
    field.returnKeyType = UIReturnKeyDone;
    field.clearButtonMode = UITextFieldViewModeWhileEditing;
    [field addTarget:weakSelf
                  action:@selector(_pageRenameFieldDidBeginEditing:)
        forControlEvents:UIControlEventEditingDidBegin];
    // The Done key of the software keyboard puts the keyboard away (a text field of an alert does nothing on Return by itself), so the
    // Rename button is in view again. The block holds the field weakly: the field holds the action.
    __weak UITextField *weakField = field;
    [field addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
                       [weakField resignFirstResponder];
                     }]
        forControlEvents:UIControlEventEditingDidEndOnExit];
  }];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];
  UIAlertAction *rename = [UIAlertAction
      actionWithTitle:@"Rename"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction *action) {
                [weakSelf _commitRenameOfPageUID:pageUID
                                       typedText:nameField.text ?: @""];
              }];
  [alert addAction:rename];
  alert.preferredAction = rename;
  [[self _pagePresenter] presentViewController:alert
                                      animated:YES
                                    completion:nil];
}

// The user pressed Rename with `typed` in the field: the rule, then the write, then the memory and the bars.
- (void)_commitRenameOfPageUID:(NSString *)pageUID
                     typedText:(NSString *)typed {
  // The write asks the lock guard itself, whoever calls it (the store re-checks inside its transaction as well).
  if ([self _refuseIfBidLocked])
    return;
  std::string uid;
  if (!OSTPageRenameUTF8(pageUID, uid))
    return;
  ost_data::ProjectData &project = self.projectService.mutableProject;
  const auto page = project.pages.find(uid);
  if (page == project.pages.end())
    return; // the page is gone: nothing to rename

  // The text typed is stripped and judged by Core against the STORED name (rename_page_dialog.py:141-148, register RP-1). A text that
  // cannot be UTF-8 is Core's Invalid case.
  std::string entered;
  ost_domain::PageRenamePlan plan;
  if (OSTPageRenameUTF8(typed, entered))
    plan = ost_domain::decide_page_rename(entered, page->second.name);
  else
    plan.decision = ost_domain::PageRenameDecision::Invalid;

  __weak ViewController *weakSelf = self;
  switch (plan.decision) {
  case ost_domain::PageRenameDecision::Refused:
  case ost_domain::PageRenameDecision::TooLong:
  case ost_domain::PageRenameDecision::Invalid: {
    // "Validation Error" and its text; the alert comes back with what was typed.
    const ost_domain::DialogText refusal =
        ost_domain::page_name_refusal_text(plan.decision);
    [self _presentPageAlertWithTitle:OSTPageRenameNS(refusal.title)
                             message:OSTPageRenameNS(refusal.message)
                          completion:^{
                            [weakSelf _presentRenameAlertForPageUID:pageUID
                                                              draft:typed];
                          }];
    return;
  }
  case ost_domain::PageRenameDecision::Unchanged:
    return; // closes without a write (Windows accept() without a save)
  case ost_domain::PageRenameDecision::Write:
    break;
  }

  NSString *reason = nil;
  const BOOL written =
      [ImportService renamePageWithUID:pageUID
                                 inBid:self.bidUID
                                toName:OSTPageRenameNS(plan.new_name)
                                 error:&reason];
  if (!written) {
    // "Save Failed" / "Failed to rename page."; the alert comes back with the draft. The reason is for the log only.
    NSLog(@"Rename Page was refused: %@", reason ?: @"(no reason)");
    const ost_domain::DialogText failed = ost_domain::page_rename_failed_text();
    [self _presentPageAlertWithTitle:OSTPageRenameNS(failed.title)
                             message:OSTPageRenameNS(failed.message)
                          completion:^{
                            [weakSelf _presentRenameAlertForPageUID:pageUID
                                                              draft:typed];
                          }];
    return;
  }

  // Written. Memory first (the one copy of the name), then the names of the page list, then the bars: _populatePageSelector ends in
  // _highlightSelection, which dims the 2D / 3D chips, so _highlightModeLabels runs after it (the pattern of _loadProject:). The
  // selection, the zoom, the meshes and the plan are untouched (plan 4.11).
  project.apply_page_name(uid, plan.new_name);
  [self.projectService refreshPageNames];
  const CGPoint offset3D = self.pageBar3D.contentOffset;
  const CGPoint offset2D = self.pageBar2D.contentOffset;
  [self _populatePageSelector];
  [self _highlightModeLabels];
  OSTPageRenameRestoreOffset(self.pageBar3D, offset3D);
  OSTPageRenameRestoreOffset(self.pageBar2D, offset2D);
}

// The action of the item "Rename Page..." (the menu of ViewController+PageMenu.mm): the pressed page. The lock guard comes first, so a
// locked bid explains itself before any alert opens. The open guard of Core is asked again, because the menu was built before the tap.
- (void)_renamePageWithUID:(NSString *)pageUID {
  if ([self _refuseIfBidLocked])
    return;
  std::string uid;
  if (!OSTPageRenameUTF8(pageUID, uid))
    return;
  const ost_data::ProjectData &project = self.projectService.project;
  // The Rename item is the same on both bars (shown, enabled by the same rule), so either bar answers.
  const ost_data::PageMenuState state = ost_data::page_menu_state(
      project, uid, ost_data::PageMenuBar::Bar3D,
      self.areaPlacementInProgress);
  if (!state.rename_page.enabled)
    return;
  const auto page = project.pages.find(uid);
  if (page == project.pages.end())
    return;
  [self _presentRenameAlertForPageUID:pageUID
                                draft:OSTPageRenameNS(page->second.name)];
}

@end
