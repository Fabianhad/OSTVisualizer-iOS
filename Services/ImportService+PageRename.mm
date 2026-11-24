// Layer: Services (Objective-C++). A thin bridge: converts the uids and the name and calls ost_store::rename_page (plan 5.5); no rule lives
// here.
#import "ImportService+PageRename.h"
#import "ImportService_Internal.h"
#include "../Core/Persistence/ProjectStore/ProjectStore.hpp"
#include <string>

namespace {
// The UTF-8 text of an NSString with EVERY byte kept, for the uids and the page name alike. UTF8String is a C string and would cut the text
// at a U+0000: the name "A<NUL>B" would be written as "A" without a word and the uid "7<NUL>x" would rename page 7. The bytes of the data
// object keep the NUL, and the store refuses it (N-M1-13 for the name; a uid is digits only). nil becomes the empty text, which the store
// reads as "not a positive integer" or "blank" (a nil would make std::string(NULL) undefined). A string that has no UTF-8 form (an unpaired
// surrogate) becomes one invalid byte, which the store refuses as well: nothing is repaired or truncated.
std::string OSTUtf8Bytes(NSString *text) {
  if (!text)
    return std::string();
  NSData *data = [text dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
  if (!data)
    return std::string("\xFF");
  return std::string(static_cast<const char *>(data.bytes), static_cast<size_t>(data.length));
}
}  // namespace

@implementation ImportService (PageRename)

+ (BOOL)renamePageWithUID:(NSString *)pageUID
                    inBid:(NSString *)bidUID
                   toName:(NSString *)newName
                    error:(NSString **)errorText {
  std::string reason;
  const bool renamed = ost_store::rename_page([self _dbPath], OSTUtf8Bytes(bidUID), OSTUtf8Bytes(pageUID),
                                              OSTUtf8Bytes(newName), &reason);
  if (errorText)
    *errorText = renamed ? nil : OSTString(reason);
  return renamed ? YES : NO;
}

+ (BOOL)renamePageWithUID:(NSString *)pageUID
                    inBid:(NSString *)bidUID
                   toName:(NSString *)newName {
  return [self renamePageWithUID:pageUID inBid:bidUID toName:newName error:NULL];
}

@end
