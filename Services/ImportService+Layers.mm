// Layer: Services (Objective-C++). A thin bridge: converts types and calls the layer functions of ost_store (plan 5.3); no rule lives here.
#import "ImportService+Layers.h"
#import "ImportService_Internal.h"
#include "../Core/Persistence/ProjectStore/ProjectStore.hpp"
#include <set>
#include <string>
#include <vector>

namespace {
// A nil NSString would make std::string(NULL) undefined: it becomes the empty string, which the store reads as "not a positive integer" (a
// refusal). The name is passed on as it is (the store strips and checks it). The std::string is built with the byte length, not up to the
// first NUL: a pasted U+0000 reaches the store, which refuses it, instead of being cut off silently (the cut text would be stored).
std::string OSTLayerText(NSString *text) {
  if (!text)
    return std::string();
  const NSUInteger byteCount = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
  const char *utf8 = text.UTF8String;
  return (utf8 && byteCount > 0) ? std::string(utf8, byteCount) : std::string();
}

// *errorText is set only when the store has a text for the user; it is nil otherwise (the screen then shows its generic failure text).
void OSTSetLayerError(NSString **errorText, const std::string &text) {
  if (errorText)
    *errorText = text.empty() ? nil : OSTString(text);
}
} // namespace

@implementation ImportService (Layers)

+ (NSString *)insertLayerNamed:(NSString *)name
                 afterSequence:(long long)afterSequence
                         inBid:(NSString *)bidUID
                         error:(NSString **)errorText {
  std::string error;
  const std::string uid = ost_store::insert_layer([self _dbPath], OSTLayerText(bidUID), OSTLayerText(name), afterSequence, &error);
  OSTSetLayerError(errorText, error);
  return uid.empty() ? nil : OSTString(uid);
}

+ (BOOL)renameLayer:(NSString *)layerUID
              toName:(NSString *)name
               inBid:(NSString *)bidUID
               error:(NSString **)errorText {
  std::string error;
  const bool renamed =
      ost_store::rename_layer([self _dbPath], OSTLayerText(bidUID), OSTLayerText(layerUID), OSTLayerText(name), &error);
  OSTSetLayerError(errorText, error);
  return renamed ? YES : NO;
}

+ (BOOL)deleteLayers:(NSArray<NSString *> *)layerUIDs inBid:(NSString *)bidUID {
  std::vector<std::string> uids;
  for (NSString *uid in layerUIDs)
    uids.push_back(OSTLayerText(uid));
  return ost_store::delete_layers([self _dbPath], OSTLayerText(bidUID), uids) ? YES : NO;
}

+ (BOOL)swapLayer:(NSString *)layerUID withLayer:(NSString *)otherLayerUID inBid:(NSString *)bidUID {
  return ost_store::swap_layer_sequence([self _dbPath], OSTLayerText(bidUID), OSTLayerText(layerUID), OSTLayerText(otherLayerUID))
             ? YES
             : NO;
}

+ (BOOL)layerUIDsInUse:(NSSet<NSString *> **)used inBid:(NSString *)bidUID {
  std::set<std::string> uids;
  if (!ost_store::read_layer_uids_in_use([self _dbPath], OSTLayerText(bidUID), uids))
    return NO;
  if (used) {
    NSMutableSet<NSString *> *set = [NSMutableSet setWithCapacity:uids.size()];
    for (const std::string &uid : uids)
      [set addObject:OSTString(uid)];
    *used = set;
  }
  return YES;
}

+ (BOOL)reloadLayersForBid:(NSString *)bidUID
                 ownLayers:(std::unordered_map<std::string, ost_domain::Layer> &)ownLayers
              mergedLayers:(std::vector<ost_domain::BidLayer> &)mergedLayers
                     error:(NSString **)errorText {
  std::string error;
  const bool reloaded = ost_store::reload_layers([self _dbPath], OSTLayerText(bidUID), ownLayers, mergedLayers, &error);
  OSTSetLayerError(errorText, error);
  return reloaded ? YES : NO;
}

@end
