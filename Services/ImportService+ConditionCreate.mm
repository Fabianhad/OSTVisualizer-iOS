// Layer: Services (Objective-C++). A thin bridge: converts the bid UID and the result and calls ost_store::insert_condition (plan 4.4 item 3,
// plan 5.1); no rule lives here.
#import "ImportService+ConditionCreate.h"
#import "ImportService_Internal.h"
#include "../Core/Persistence/ProjectStore/ProjectStore.hpp"
#include <string>

@implementation ImportService (ConditionCreate)

+ (NSString *)createCondition:(const ost_domain::CreateConditionSpec &)spec
                        inBid:(NSString *)bidUID
                        error:(NSString **)errorText {
  // A nil UID would make std::string(NULL) undefined: it becomes the empty string, which the store refuses (not a positive integer).
  const char *uidText = bidUID ? bidUID.UTF8String : nullptr;
  const std::string uid = uidText ? std::string(uidText) : std::string();

  std::string reason;
  const std::string newUID = ost_store::insert_condition([self _dbPath], uid, spec, &reason);
  // only the two limit refusals and the name-length refusal have a text; every other failure is the generic "Failed to create condition."
  if (errorText)
    *errorText = (newUID.empty() && !reason.empty()) ? OSTString(reason) : nil;
  if (newUID.empty())
    return nil;
  return OSTString(newUID);
}

@end
