// Layer: Services (Objective-C++). A thin bridge: converts the uids and calls ost_store::delete_pages and
// ost_store::pages_with_delete_content (plan 5.6); no rule lives here.
#import "ImportService+PageDelete.h"
#import "ImportService_Internal.h"
#include "../Core/Domain/PageDeletion.hpp"
#include "../Core/Persistence/ProjectStore/ProjectStore.hpp"
#include <set>
#include <string>
#include <vector>

namespace {
// The UTF-8 text of an NSString with EVERY byte kept, for a uid. UTF8String is a C string and would cut "7<NUL>x" at the NUL, so a bad uid
// would turn into the page 7 and delete it; the bytes of the data object keep the NUL and the store refuses the uid as not a positive
// integer. nil is the empty text (dropped by the list normalisation); a string that has no UTF-8 form (an unpaired surrogate) becomes one
// invalid byte, which the store refuses as well: nothing is repaired or truncated.
std::string OSTPageDeleteUidText(NSString *text) {
  if (![text isKindOfClass:[NSString class]])
    return std::string();
  NSData *data = [text dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
  if (!data)
    return std::string("\xFF");
  return std::string(static_cast<const char *>(data.bytes), static_cast<size_t>(data.length));
}
}  // namespace

@implementation ImportService (PageDelete)

+ (BOOL)deletePages:(NSArray<NSString *> *)pageUIDs
              inBid:(NSString *)bidUID
              error:(NSString **)errorText {
  std::vector<std::string> uids;
  for (id uid in pageUIDs)
    uids.push_back(OSTPageDeleteUidText(uid));
  // project_write_service.delete_pages: _unique_nonempty_uids first; an empty list is then refused by the store.
  uids = ost_domain::normalize_page_uids(uids);

  std::string reason;
  const bool deleted = ost_store::delete_pages([self _dbPath], OSTPageDeleteUidText(bidUID), uids, &reason);
  if (errorText)
    *errorText = (deleted || reason.empty()) ? nil : OSTString(reason);
  return deleted ? YES : NO;
}

+ (NSSet<NSString *> *)pagesWithDeleteContentInBid:(NSString *)bidUID error:(NSString **)errorText {
  std::set<std::string> pages;
  std::string reason;
  if (!ost_store::pages_with_delete_content([self _dbPath], OSTPageDeleteUidText(bidUID), pages, &reason)) {
    if (errorText)
      *errorText = reason.empty() ? nil : OSTString(reason);
    return nil;
  }
  if (errorText)
    *errorText = nil;
  NSMutableSet<NSString *> *result = [NSMutableSet setWithCapacity:pages.size()];
  for (const std::string &page : pages)
    [result addObject:OSTString(page)];
  return result;
}

@end
