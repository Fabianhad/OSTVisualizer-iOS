// Layer: Services (Objective-C++)
// The thin bridge of Rename Page (plan 4.4 item 3, 5.5): converts the types and calls ost_store::rename_page. The name rule
// (ost_domain::decide_page_rename: strip, blank, length, NUL, unchanged) is the caller's first step and is checked again inside the store;
// no rule lives here.
#ifndef IMPORTSERVICE_PAGERENAME_H
#define IMPORTSERVICE_PAGERENAME_H

#import "ImportService.h"

@interface ImportService (PageRename)

// Writes `newName` as the name of the page `pageUID` of the bid `bidUID` (Windows save_page_name, page_operations.py:130-152) in one
// transaction. YES when the name was written. NO, and nothing written, when the store refuses: a uid that is nil or not a positive integer,
// a page that is not a page of THIS bid, a locked bid (also when the lock cannot be read: fail closed), a name that is blank, not stripped,
// over 255 UTF-16 units, with U+0000 or not valid UTF-8 (the store never trusts the screen), or a statement that fails or does not change
// exactly one row. `*errorText` (optional) is nil on success and the reason (for the log, not for the user) on a refusal; the user text of
// every failure is ost_domain::page_rename_failed_text(). The caller applies the name in memory only after a YES.
+ (BOOL)renamePageWithUID:(NSString *)pageUID
                    inBid:(NSString *)bidUID
                   toName:(NSString *)newName
                    error:(NSString **)errorText;

// The same write without the reason.
+ (BOOL)renamePageWithUID:(NSString *)pageUID
                    inBid:(NSString *)bidUID
                   toName:(NSString *)newName;

@end

#endif
