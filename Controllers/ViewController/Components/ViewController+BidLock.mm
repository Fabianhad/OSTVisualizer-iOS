// The UI half of the bid lock guard (plan 4.1, N-M1-01, N-M1-02). The rule and the read live in Core (ost_store::bid_is_locked) behind the
// one bridge +[ImportService isBidLocked:]; this category only asks it about the open bid and says so when the bid is locked.
#import "../ViewController_Internal.h"
#import "../../../Services/ImportService+BidLock.h"

@implementation ViewController (BidLock)

// The alert of the lock guard: title, message, OK. The wording is the plan's (N-M1-02, "This bid is locked."; the title is only a heading
// and the owner may reword both, N-M1-03). It is presented on the TOPMOST presented controller: the Conditions and the Layers sheets are
// presented over the main controller, and an alert presented on a controller that already presents something is dropped. Same shape as
// the alert helper of ViewController+Conditions.mm, but private to this file on purpose: this file (S0's) depends on no method of the
// file of another stream, so that file can change without breaking the lock guard.
static void OSTPresentBidLockedAlert(UIViewController *root) {
  UIViewController *presenter = root;
  while (presenter.presentedViewController)
    presenter = presenter.presentedViewController;
  UIAlertController *alert =
      [UIAlertController alertControllerWithTitle:@"Bid Locked"
                                          message:@"This bid is locked."
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [presenter presentViewController:alert animated:YES completion:nil];
}

// Declared in the block "M1 bid lock (S0)" of ViewController_Internal.h. The streams S1 to S4 call it at the TOP of every action that
// writes (the page menu items, the "+" and "Folders" actions of the Conditions sheet, the Layers "+"): when it answers YES the action
// stops, nothing was written and the alert has been shown. The controls stay visible (N-M1-02); Windows disables them and refuses
// silently (project_write_service.py:701-704).
//
// Fail closed: +isBidLocked: also answers YES when the lock cannot be read (the database cannot be opened, a failed statement, an empty
// bidUID), so the alert then says the bid is locked and nothing is written.
- (BOOL)_refuseIfBidLocked {
  if (![ImportService isBidLocked:self.bidUID])
    return NO;
  OSTPresentBidLockedAlert(self);
  return YES;
}

@end
