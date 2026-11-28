// Layer: Services (Objective-C++)
// The ONE UI bridge of the bid lock check (plan 4.1, N-M1-01, N-M1-02). Every UI stream (S1 to S4, later M2 to M4) asks the lock through
// this method, or through the ViewController helper -_refuseIfBidLocked, which calls it; nobody calls ost_store::bid_is_locked from the UI
// directly and nobody adds a lock method of their own. The writers check the lock again inside their own transaction (plan 4.4 item 2).
#ifndef IMPORTSERVICE_BIDLOCK_H
#define IMPORTSERVICE_BIDLOCK_H

#import "ImportService.h"

@interface ImportService (BidLock)

// YES when the bid is locked (its job status is Locked), and ALSO YES whenever the answer cannot be read: a database that cannot be
// opened, a failed statement, a bid UID that is nil, empty or not a positive integer, a bid that does not exist, a job status that no
// JobStatuses row names (Windows project_read_service.py:81-94 is_bid_locked: unresolved = locked, fail closed). NO only when the read
// succeeded and the bid is not locked. Opens its own connection and writes nothing; meant to show "This bid is locked." BEFORE an action.
+ (BOOL)isBidLocked:(NSString *)bidUID;

@end

#endif
