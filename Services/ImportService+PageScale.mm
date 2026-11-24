// Layer: Services (Objective-C++). A thin bridge: converts the bid UID and calls ost_store::save_page_scales (plan 5.4); no rule lives here.
#import "ImportService+PageScale.h"
#import "ImportService_Internal.h"
#include "../Core/Persistence/ProjectStore/ProjectStore.hpp"
#include <string>
#include <vector>

@implementation ImportService (PageScale)

+ (BOOL)savePageScales:(const std::vector<std::string> &)pageUIDs
                  inBid:(NSString *)bidUID
           scaleFactor1:(double)scaleFactor1
           scaleFactor2:(double)scaleFactor2
                 locked:(BOOL *)locked
              errorText:(NSString **)errorText {
  // A nil UID would make std::string(NULL) undefined: it becomes the empty string, which the store refuses as a malformed uid.
  const char *uidText = bidUID ? bidUID.UTF8String : nullptr;
  const std::string bid = uidText ? std::string(uidText) : std::string();

  ost_store::PageScaleOutcome outcome;
  const bool saved = ost_store::save_page_scales([self _dbPath], bid, pageUIDs, scaleFactor1, scaleFactor2, &outcome);
  if (locked)
    *locked = (!saved && outcome.refusal == ost_store::PageScaleRefusal::Locked) ? YES : NO;
  if (errorText)
    *errorText = saved ? nil : OSTString(outcome.error);
  return saved ? YES : NO;
}

@end
