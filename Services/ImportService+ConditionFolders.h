// Layer: Services (Objective-C++)
// The thin bridge of the condition folder writers (plan 5.2, 4.4 item 3): it converts the types and calls ost_store; no rule, no SQL and
// no text of a rule lives here. Imported only by .mm files (ImportService.h already pulls in C++).
#ifndef IMPORTSERVICE_CONDITIONFOLDERS_H
#define IMPORTSERVICE_CONDITIONFOLDERS_H

#import "ImportService.h"
#include "../Core/Persistence/ProjectStore/ProjectStore_ConditionFolders.hpp"

// What one folder write did (ost_store::ConditionFolderResult with the text converted). Nothing was written unless `ok` is YES.
@interface OSTConditionFolderOutcome : NSObject

@property(nonatomic, readonly) BOOL ok;
// Why nothing was written (ConditionFolderRefusal::None when `ok`). The user-facing wording of a refusal that carries no `message`
// (BidLocked, BlankName, FolderNotFound, WriteFailed ...) is the UI's: this class holds no text of its own.
@property(nonatomic, readonly) ost_store::ConditionFolderRefusal refusal;
// create: the new folder's uid; rename and move: the folder's uid ("" for "no folder"); delete: "".
@property(nonatomic, readonly, copy) NSString *uid;
// The text the user must see where Core has one (a duplicate, a name that is too long, the UID limit, a file without folder hierarchy),
// else "".
@property(nonatomic, readonly, copy) NSString *message;
// delete, refusal InUse: the requested folders that hold a condition, or have a sub-folder that does, in request order; else empty.
@property(nonatomic, readonly, copy) NSArray<NSString *> *blocked;

@end

@interface ImportService (ConditionFolders)

// Creates a folder named `name` (stripped by Core) in the bid; `parentFolderUID` nil or "" = a top-level folder. Every write re-checks the
// bid lock, the ownership and the name rules inside its own transaction (ost_store::create_condition_folder).
+ (OSTConditionFolderOutcome *)createConditionFolderNamed:(NSString *)name
                                                    inBid:(NSString *)bidUID
                                             parentFolder:(NSString *)parentFolderUID;

// Renames the folder; the same name (after the strip) is a success that writes nothing (ost_store::rename_condition_folder).
+ (OSTConditionFolderOutcome *)renameConditionFolder:(NSString *)folderUID
                                              toName:(NSString *)name
                                               inBid:(NSString *)bidUID;

// Deletes the folders all or nothing; a folder (or one below it) that holds a condition refuses the whole call with `blocked` filled.
// The sub-folders of a deleted folder move to the top level. An empty list is a success without opening the database
// (ost_store::delete_condition_folders).
+ (OSTConditionFolderOutcome *)deleteConditionFolders:(NSArray<NSString *> *)folderUIDs
                                                inBid:(NSString *)bidUID;

// "Move to Folder": the condition goes into the folder, `folderUID` nil or "" = no folder (ost_store::move_condition_to_folder; it
// re-checks the bid lock, unlike updateConditionWithUID:...).
+ (OSTConditionFolderOutcome *)moveCondition:(NSString *)conditionUID
                                    toFolder:(NSString *)folderUID
                                       inBid:(NSString *)bidUID;

@end

#endif
