// Layer: Services (Objective-C++). A thin bridge: converts the types and calls ost_store (plan 5.2); no rule and no SQL live here.
#import "ImportService+ConditionFolders.h"
#import "ImportService_Internal.h"
#include "../Core/Persistence/ProjectStore/ProjectStore.hpp"
#include <string>
#include <vector>

namespace {

// The exact UTF-8 bytes of an NSString for Core. nil becomes the empty string (the root for a parent or folder uid; Core reads an empty
// name as blank and an empty bid / condition uid as not a positive integer). `UTF8String` is not used: it stops at a U+0000 and answers
// NULL for a string with a lone surrogate. Both stay visible to Core: the NUL is kept (the name is refused) and text that has no UTF-8
// form becomes a byte that is not valid UTF-8 (a name is refused, a uid is no integer), never an empty or a shortened string.
std::string UTF8Bytes(NSString *text) {
  if (!text) return std::string();
  if (![text isKindOfClass:[NSString class]]) return std::string("\xFF");  // not a string at all: refused like text with no UTF-8 form
  NSData *data = [text dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
  if (!data) return std::string("\xFF");
  return std::string(static_cast<const char *>(data.bytes), data.length);
}

}  // namespace

@interface OSTConditionFolderOutcome ()
@property(nonatomic, readwrite) BOOL ok;
@property(nonatomic, readwrite) ost_store::ConditionFolderRefusal refusal;
@property(nonatomic, readwrite, copy) NSString *uid;
@property(nonatomic, readwrite, copy) NSString *message;
@property(nonatomic, readwrite, copy) NSArray<NSString *> *blocked;
+ (instancetype)outcomeFromResult:(const ost_store::ConditionFolderResult &)result;
@end

@implementation OSTConditionFolderOutcome

+ (instancetype)outcomeFromResult:(const ost_store::ConditionFolderResult &)result {
  OSTConditionFolderOutcome *outcome = [[OSTConditionFolderOutcome alloc] init];
  outcome.ok = result.ok ? YES : NO;
  outcome.refusal = result.refusal;
  outcome.uid = OSTString(result.uid);
  outcome.message = OSTString(result.message);
  NSMutableArray<NSString *> *blocked = [NSMutableArray arrayWithCapacity:result.blocked.size()];
  for (const std::string &uid : result.blocked) [blocked addObject:OSTString(uid)];
  outcome.blocked = blocked;
  return outcome;
}

@end

@implementation ImportService (ConditionFolders)

+ (OSTConditionFolderOutcome *)createConditionFolderNamed:(NSString *)name
                                                    inBid:(NSString *)bidUID
                                             parentFolder:(NSString *)parentFolderUID {
  const ost_store::ConditionFolderResult result =
      ost_store::create_condition_folder([self _dbPath], UTF8Bytes(bidUID), UTF8Bytes(name), UTF8Bytes(parentFolderUID));
  return [OSTConditionFolderOutcome outcomeFromResult:result];
}

+ (OSTConditionFolderOutcome *)renameConditionFolder:(NSString *)folderUID
                                              toName:(NSString *)name
                                               inBid:(NSString *)bidUID {
  const ost_store::ConditionFolderResult result =
      ost_store::rename_condition_folder([self _dbPath], UTF8Bytes(bidUID), UTF8Bytes(folderUID), UTF8Bytes(name));
  return [OSTConditionFolderOutcome outcomeFromResult:result];
}

+ (OSTConditionFolderOutcome *)deleteConditionFolders:(NSArray<NSString *> *)folderUIDs
                                                inBid:(NSString *)bidUID {
  std::vector<std::string> uids;
  uids.reserve(folderUIDs.count);
  // An element that is not a string must not vanish from the request (the call would then delete less than was asked): UTF8Bytes turns
  // it into a uid that is no integer, which Core refuses as a whole.
  for (id item in folderUIDs) uids.push_back(UTF8Bytes(item));
  const ost_store::ConditionFolderResult result = ost_store::delete_condition_folders([self _dbPath], UTF8Bytes(bidUID), uids);
  return [OSTConditionFolderOutcome outcomeFromResult:result];
}

+ (OSTConditionFolderOutcome *)moveCondition:(NSString *)conditionUID
                                    toFolder:(NSString *)folderUID
                                       inBid:(NSString *)bidUID {
  const ost_store::ConditionFolderResult result =
      ost_store::move_condition_to_folder([self _dbPath], UTF8Bytes(bidUID), UTF8Bytes(conditionUID), UTF8Bytes(folderUID));
  return [OSTConditionFolderOutcome outcomeFromResult:result];
}

@end
