// The page menu (plan 4.8, 5.5, 5.6, decision N-M1-05): a long press on a page button of the 3D bar or of the 2D bar opens a UIMenu
// (system-anchored, so it needs no popover anchor on iPad) that acts on the PRESSED page. The decisions (which items, which of them are
// enabled) are Core's ost_data::page_menu_state; this file only builds the widgets. The items call the actions of
// ViewController+PageScale.mm, ViewController+PageRename.mm and ViewController+PageDelete.mm, each of which asks the bid lock guard first.
#include "../../../Core/Data/PageMenuRules.hpp"
#import "../ViewController_Internal.h"

static NSString *OSTPageMenuNS(const std::string &text) {
  return [NSString stringWithUTF8String:text.c_str()] ?: @"";
}

// One item of the menu. A disabled item stays visible and greyed out (plan 4.8); the destructive one is red. `subtitle` (nil for none) is the
// second line of the item.
static UIAction *OSTPageMenuAction(NSString *title, NSString *subtitle,
                                   NSString *symbolName, BOOL enabled,
                                   BOOL destructive, void (^handler)(void)) {
  UIAction *action = [UIAction actionWithTitle:title
                                         image:[UIImage systemImageNamed:symbolName]
                                    identifier:nil
                                       handler:^(__kindof UIAction *chosen) {
                                         handler();
                                       }];
  action.subtitle = subtitle;
  NSUInteger attributes = 0;
  if (!enabled)
    attributes |= UIMenuElementAttributesDisabled;
  if (destructive)
    attributes |= UIMenuElementAttributesDestructive;
  action.attributes = (UIMenuElementAttributes)attributes;
  return action;
}

@implementation ViewController (PageMenu)

// THE item list of the page menu: the one place that decides which items exist and in which order:
//
//   1. Set Scale...   (2D bar only; the subtitle is the current scale label of the pressed page)
//   2. Rename Page... (both bars)
//   3. Delete Page    (2D bar only, destructive, disabled with one page)
//
// It runs every time the menu is SHOWN (see _attachPageMenuToButton:), so the enabled states are current: an area placement in progress
// disables the items, and they are enabled again after it (Windows _PLACEMENT_BLOCKED). The items stay visible on a locked bid; each
// action asks the bid lock guard first (plan 4.1, N-M1-02).
- (NSArray<UIMenuElement *> *)_pageMenuItemsForPageUID:(NSString *)pageUID
                                               onBar2D:(BOOL)onBar2D {
  const std::string uid = pageUID.UTF8String ? pageUID.UTF8String : "";
  const ost_data::PageMenuState state = ost_data::page_menu_state(
      self.projectService.project, uid,
      onBar2D ? ost_data::PageMenuBar::Bar2D : ost_data::PageMenuBar::Bar3D,
      self.areaPlacementInProgress);
  __weak ViewController *weakSelf = self;
  NSMutableArray<UIMenuElement *> *items = [NSMutableArray array];

  // "Set Scale..." acts on the PRESSED page (not the active one): the presenter takes the uid, and it asks the bid lock guard itself.
  if (state.set_scale.shown) {
    [items addObject:OSTPageMenuAction(
                         @"Set Scale...", [self _setScaleSubtitleForPageUID:pageUID],
                         @"ruler", state.set_scale.enabled, NO, ^{
                           [weakSelf _presentSetScaleForPageUID:pageUID];
                         })];
  }
  if (state.rename_page.shown) {
    [items addObject:OSTPageMenuAction(@"Rename Page...", nil, @"pencil",
                                       state.rename_page.enabled, NO, ^{
                                         [weakSelf _renamePageWithUID:pageUID];
                                       })];
  }
  if (state.delete_page.shown) {
    [items addObject:OSTPageMenuAction(@"Delete Page", nil, @"trash",
                                       state.delete_page.enabled, YES, ^{
                                         [weakSelf _deletePageWithUID:pageUID];
                                       })];
  }
  return items;
}

// Called by _selectorButtonWithTitle:tag:font:action: (ViewController+PageSelector.mm) for every button the bars make. Only the page
// buttons get the menu (the tag ranges of ViewController_Internal.h): not the 3D folder buttons and not the 3D / 2D mode chips.
// The button keeps its tap (it selects the page); the menu appears on a long press (showsMenuAsPrimaryAction = NO). The menu holds one
// deferred element with an uncached provider (iOS 15), so the items are built again each time the menu is shown.
- (void)_attachPageMenuToButton:(UIButton *)button {
  const int tag = (int)button.tag;
  const BOOL onBar2D = tag >= kPageTagBase2D && tag < kModeLabelTag3D;
  const BOOL onBar3D = tag >= kPageTagBase3D && tag < kPageTagBase2D;
  if (!onBar2D && !onBar3D)
    return;
  const int index = tag - (onBar2D ? kPageTagBase2D : kPageTagBase3D);
  if (index < 0 || index >= (int)self.projectService.pageList.size())
    return;
  // The button stands for this page, whatever the list looks like when the menu is shown (the bars are rebuilt on every list change).
  NSString *pageUID =
      OSTPageMenuNS(self.projectService.pageList[(size_t)index].uid);

  __weak ViewController *weakSelf = self;
  UIDeferredMenuElement *deferred = [UIDeferredMenuElement
      elementWithUncachedProvider:^(
          void (^completion)(NSArray<UIMenuElement *> *elements)) {
        ViewController *strongSelf = weakSelf;
        completion(strongSelf
                       ? [strongSelf _pageMenuItemsForPageUID:pageUID
                                                      onBar2D:onBar2D]
                       : @[]);
      }];
  button.menu = [UIMenu menuWithChildren:@[ deferred ]];
  button.showsMenuAsPrimaryAction = NO;
}

// The controller the page alerts are presented on: the topmost presented one. The Conditions and the Layers sheets are presented over
// the main controller and leave it interactive (medium detent), and an alert presented on a controller that already presents something
// is dropped. Same rule as the lock alert of ViewController+BidLock.mm.
- (UIViewController *)_pagePresenter {
  UIViewController *presenter = self;
  while (presenter.presentedViewController)
    presenter = presenter.presentedViewController;
  return presenter;
}

// A message with one OK button; `completion` (optional) runs after OK. Used by the rename and delete flows for their refusals and
// failures.
- (void)_presentPageAlertWithTitle:(NSString *)title
                           message:(NSString *)message
                        completion:(void (^)(void))completion {
  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction *action) {
                                            if (completion)
                                              completion();
                                          }]];
  [[self _pagePresenter] presentViewController:alert
                                      animated:YES
                                    completion:nil];
}

@end
