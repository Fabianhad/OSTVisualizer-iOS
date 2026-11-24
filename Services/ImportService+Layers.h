// Layer: Services (Objective-C++)
// The thin bridge of the M1 layer editing (plan 5.3, 4.4 item 3): converts the types and calls ost_store (Core/Persistence/ProjectStore/
// ProjectStore_Layers.hpp). No rule lives here: the name rules, the limits, the bid lock and the ownership checks are Core's and run inside the store's
// transaction; the screens decide what to show with Core/Domain/LayerEdit.hpp (check_layer_name, plan_layer_move, plan_layer_delete, ...).
// Every method works on the file given by +[ImportService _dbPath] and never throws. A nil or empty UID is a refusal (NO / nil), never a crash.
#ifndef IMPORTSERVICE_LAYERS_H
#define IMPORTSERVICE_LAYERS_H

#import "ImportService.h"
#include "../Core/Domain/Layer.hpp"
#include <string>
#include <unordered_map>
#include <vector>

@interface ImportService (Layers)

// New Layer: a row of the bid with the stripped name, Show -1, Sequence afterSequence + 1 (the bid's later rows move up by one). Returns the new
// layer's UID, or nil when nothing was written. *errorText (when given) is set only for a text the user must see (the duplicate text, "Cannot add a
// layer: ...", the name hints of the name rules, each with the Windows or iOS wording of LayerEdit.hpp); for every other refusal (a blank name,
// a locked bid, a failed statement or read) it is nil and the screen shows its generic failure text (kNewLayerFailed). Use
// ost_domain::new_layer_after_sequence for afterSequence.
+ (NSString *)insertLayerNamed:(NSString *)name
                 afterSequence:(long long)afterSequence
                         inBid:(NSString *)bidUID
                         error:(NSString **)errorText;

// Rename Layer: YES when the layer now has that name (a name equal to the stored one is a success that writes nothing). NO, nothing written:
// *errorText as above (kRenameLayerFailed is the generic text).
+ (BOOL)renameLayer:(NSString *)layerUID
              toName:(NSString *)name
               inBid:(NSString *)bidUID
               error:(NSString **)errorText;

// Delete Layer: the layers in ONE transaction, all or none. The store clears the layers' references and deletes them: it does NOT refuse an
// in-use layer, the screen refuses first with -layerUIDsInUse:inBid: and ost_domain::plan_layer_delete. NO, nothing deleted, when any UID is
// not an editable layer of the bid, the bid is locked or a statement fails.
+ (BOOL)deleteLayers:(NSArray<NSString *> *)layerUIDs inBid:(NSString *)bidUID;

// Move Up / Move Down: exchanges the Sequence of the two layers (ost_domain::plan_layer_move names the neighbour). NO, nothing written, when
// either is not the bid's own layer, a Sequence is NULL, the bid is locked or a statement fails.
+ (BOOL)swapLayer:(NSString *)layerUID withLayer:(NSString *)otherLayerUID inBid:(NSString *)bidUID;

// The UIDs that any reference table names for the bid (the usage read that guards a delete). NO when it could not be read: the caller MUST refuse
// the delete (Windows answers an empty set there, which lets a delete clear references). *used is replaced on YES only.
+ (BOOL)layerUIDsInUse:(NSSet<NSString *> **)used inBid:(NSString *)bidUID;

// Re-reads the bid's layer lists without loading the rest of the bid: `ownLayers` (ProjectData::layers) and `mergedLayers`
// (ProjectData::merged_layers), both replaced together and only on YES. NO leaves both untouched: *errorText says why and the screen keeps its
// old state (a write that committed stays: the "layer list could not be refreshed" texts of LayerEdit.hpp).
+ (BOOL)reloadLayersForBid:(NSString *)bidUID
                 ownLayers:(std::unordered_map<std::string, ost_domain::Layer> &)ownLayers
              mergedLayers:(std::vector<ost_domain::BidLayer> &)mergedLayers
                     error:(NSString **)errorText;

@end

#endif
