// Layer: Services (Objective-C++)
// The UI bridge of New Condition (plan 5.1, M1): ONE method that hands a Core spec to ost_store::insert_condition. The spec itself is built by
// Core (ost_domain::build_create_spec), never here. The bid lock is asked BEFORE the form opens through +isBidLocked: (ImportService+BidLock.h,
// the only UI lock bridge: there is no second lock method here); the writer checks it again inside its transaction.
#ifndef IMPORTSERVICE_CONDITIONCREATE_H
#define IMPORTSERVICE_CONDITIONCREATE_H

#import "ImportService.h"
#include "../Core/Domain/ConditionCreate.hpp"

@interface ImportService (ConditionCreate)

// Windows insert_condition (condition_operations.py:254-350): inserts ONE condition into the bid in one transaction and returns its new uid,
// or nil with nothing written (a refusal: a layer / folder / type that does not belong, a locked bid, a name or a number the writer refuses, a
// failed statement). *errorText (errorText may be NULL) is a text the user must see and is nil for every other failure (the caller then shows
// ost_domain::condition_create_failed_issue()): the Windows text when the new number would pass the highest condition number 2147483647, the
// text when no uid is left below 2147483647, and the name-length text of N-M1-13. nil on success.
+ (NSString *)createCondition:(const ost_domain::CreateConditionSpec &)spec
                        inBid:(NSString *)bidUID
                        error:(NSString **)errorText;

@end

#endif
