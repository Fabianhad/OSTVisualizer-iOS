#ifndef IMPORTSERVICE_H
#define IMPORTSERVICE_H

#import <Foundation/Foundation.h>
#include "../Core/Data/ProjectData.hpp"

extern NSNotificationName const OSTProjectImportedNotification;
extern NSNotificationName const OSTProjectDeletedNotification;
extern NSNotificationName const OSTRenderConfigChangedNotification;

@interface ImportService : NSObject

+ (NSURL *)databaseURL;
+ (BOOL)importOSTFile:(NSURL *)fileURL error:(NSError **)error;
+ (BOOL)importOSPFile:(NSURL *)fileURL error:(NSError **)error;

// The project browser's hierarchy. nil when it could not be read completely (a missing table, a failing read): *errorText then names the
// cause; a hierarchy is never returned half read (DBP-M02).
+ (NSDictionary *)loadHierarchyWithError:(NSString **)errorText;

// Loads one bid. NO when the bid could not be read completely or its data is inconsistent (DBP-M02, DBP-31, BRW-16, POS-M01): *errorText
// names the cause in the Windows wording (the missing reference, the takeoff UID, the cycle, the malformed UID) and `project` stays empty.
+ (BOOL)loadProjectWithUID:(NSString *)bidUID
                   project:(ost_data::ProjectData &)project
                     error:(NSString **)errorText;
// The same load without the reason: an empty project on every failure (the reason is logged). Prefer the method above.
+ (ost_data::ProjectData)loadProjectWithUID:(NSString *)bidUID;
+ (BOOL)deleteProjectWithUID:(NSString *)bidUID;
+ (BOOL)permanentlyDeleteProjectWithUID:(NSString *)bidUID;
// Moves a bid out of Deleted Bids back to the folder it came from (the Windows Restore), or to no folder.
+ (BOOL)restoreBidWithUID:(NSString *)bidUID;

// nil when nothing was written (the insert failed).
+ (NSString *)createFolderWithName:(NSString *)name;
+ (BOOL)renameFolderWithUID:(NSString *)folderUID toName:(NSString *)newName;
// Only an empty folder is deleted. NO, nothing written: *errorText is the Windows text (a folder that still has bids, the system folder)
// or nil when a statement failed.
+ (BOOL)deleteFolderWithUID:(NSString *)folderUID error:(NSString **)errorText;
// The same rule without writing: NO with *message = the reason when the folder cannot be deleted.
+ (BOOL)canDeleteFolderWithUID:(NSString *)folderUID message:(NSString **)message;
+ (BOOL)assignBid:(NSString *)bidUID toFolder:(NSString *)folderUID;
+ (BOOL)removeBidFromFolder:(NSString *)bidUID;

+ (BOOL)setLayerVisibility:(BOOL)visible
                  forLayer:(NSString *)layerUID
                     inBid:(NSString *)bidUID;

// Show All / Hide All: the visibility of every listed layer in one transaction, all of them or none (Windows update_all_layers_show).
// NO, nothing written, when a layer is refused or a statement fails (DBP-35).
+ (BOOL)setLayersVisibility:(BOOL)visible
                  forLayers:(const std::vector<std::string> &)layerUIDs
                      inBid:(NSString *)bidUID;

+ (BOOL)saveTakeoffPosition:(const std::vector<double> &)position
                 forTakeoff:(NSString *)takeoffUID
                      inBid:(NSString *)bidUID;

/// Writes the positions one gesture changed (a group drag, an area with its
/// children) in one transaction: all of them or none (Windows
/// save_takeoff_positions). Returns NO, and writes nothing, when any row is
/// refused or fails.
+ (BOOL)saveTakeoffPositions:
            (const std::vector<std::pair<std::string, std::vector<double>>> &)
                positions
                       inBid:(NSString *)bidUID;

+ (NSString *)insertTakeoff:(const ost_domain::Takeoff &)takeoff
                      inBid:(NSString *)bidUID;

// Reads the other columns of the stored takeoff row into takeoff.extras (Windows get_takeoff_extras) BEFORE it is deleted, so that an undo
// re-inserts the whole row (UND-06). A takeoff that is not a row of the bid is not an error: the extras stay empty.
+ (void)loadExtrasForTakeoff:(ost_domain::Takeoff &)takeoff
                       inBid:(NSString *)bidUID;

+ (BOOL)deleteTakeoff:(NSString *)takeoffUID;

// The takeoffs one delete / redo removes (a takeoff with all its descendants) in one transaction, all or none (Windows delete_takeoffs).
+ (BOOL)deleteTakeoffs:(const std::vector<std::string> &)takeoffUIDs;

// The takeoffs one undo / redo restores in one transaction, all or none, each with its own uid and extras; a takeoff may name an
// earlier one of the list as its parent (Windows insert_takeoffs).
+ (BOOL)insertTakeoffs:(const std::vector<ost_domain::Takeoff> &)takeoffs
                 inBid:(NSString *)bidUID;

+ (BOOL)setTakeoffNegative:(BOOL)negative
                forTakeoff:(NSString *)takeoffUID
                     inBid:(NSString *)bidUID;

// The negative flag of several takeoffs (one user action, or its undo with the old values) in one transaction, all or none.
+ (BOOL)setTakeoffsNegative:(const std::vector<std::pair<std::string, bool>> &)updates
                      inBid:(NSString *)bidUID;

// Windows update_condition: writes only the fields in `changes`, scoped to the bid (the condition and any new layer / folder must belong to
// it). NO, nothing written, when that is refused or a statement fails; an empty change set is a success without a write (COND-UPD-02,
// COND-M05). *errorText (errorText may be NULL) is the Windows text when a renumber would push another condition past the highest
// condition number, 2147483647 (DLG-FINITE-01: nothing is written then, the other fields of the save included), and nil for every other
// failure (the caller shows the generic text) and on success.
+ (BOOL)updateConditionWithUID:(NSString *)conditionUID
                         inBid:(NSString *)bidUID
                       changes:(const ost_domain::ConditionChanges &)changes
                         error:(NSString **)errorText;

+ (NSString *)duplicateCondition:(NSString *)conditionUID
                           inBid:(NSString *)bidUID;

+ (BOOL)deleteCondition:(NSString *)conditionUID
                  inBid:(NSString *)bidUID;

+ (NSDictionary *)loadRenderConfig;
+ (BOOL)saveRenderConfigWithColorMode:(NSString *)colorMode
                     grayscaleEnabled:(BOOL)grayscale;

@end

#endif
