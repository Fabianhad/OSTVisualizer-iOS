// Layer: Services (Objective-C++). A thin bridge: converts the bid UID and calls ost_store::bid_is_locked (plan 4.1); no rule lives here.
#import "ImportService+BidLock.h"
#import "ImportService_Internal.h"
#include "../Core/Persistence/ProjectStore/ProjectStore.hpp"
#include <string>

@implementation ImportService (BidLock)

+ (BOOL)isBidLocked:(NSString *)bidUID {
  // A nil UID would make std::string(NULL) undefined: it becomes the empty string, which the store reads as "not a positive integer"
  // (a failed read, so locked).
  const char *uidText = bidUID ? bidUID.UTF8String : nullptr;
  const std::string uid = uidText ? std::string(uidText) : std::string();

  bool locked = true;
  const bool read = ost_store::bid_is_locked([self _dbPath], uid, locked);
  // Fail closed: a read that failed counts as locked whatever `locked` holds (the store also sets it true, but this does not depend on it).
  if (!read)
    return YES;
  return locked ? YES : NO;
}

@end
