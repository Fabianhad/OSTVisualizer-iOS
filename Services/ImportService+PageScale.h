// Layer: Services (Objective-C++)
// The UI bridge of the Set Scale writer (plan 4.4 item 3, 5.4). A thin category: it converts the types and calls ost_store::save_page_scales;
// every rule (the uids, the lock, the factor, the 17 tables, the rollback) lives in Core and the store.
#ifndef IMPORTSERVICE_PAGESCALE_H
#define IMPORTSERVICE_PAGESCALE_H

#import "ImportService.h"

#include <string>
#include <vector>

@interface ImportService (PageScale)

// Sets the scale of the listed pages of the bid to (scaleFactor1, scaleFactor2): every stored coordinate of those pages (the 17 position tables, the
// legend XML, the overlay rectangle and its offsets) is multiplied by new ratio / old ratio and the pair is written, in ONE transaction, all pages
// or none (Windows page_operations.py save_page_scale, project_write_service.py save_page_scales). Empty and repeated page UIDs are dropped; the
// UI passes ost_domain::page_setting_uids (the pressed page, or every page of the bid in page-bar order for "Apply to all pages").
// YES when written. NO, nothing written, when the change is refused or a statement fails: *locked (optional) is YES when the reason is the
// bid lock (a locked bid, or a lock state that could not be read: fail closed), *unstorableContent (optional) is YES when the reason is that the takeoffs and
// annotations of a page cannot be stored at the new scale (ost_store::page_scale_refusal_is_unstorable_content: Windows PageScaleRangeError), *errorText
// (optional) names the reason (the Windows wording "Page <uid> requires finite positive scale factors" where Windows has one). The UI shows the Windows
// alert "Save Failed" / "Failed to save page scale." for every NO (the reason is for the log), "Save Failed" / "The scale was not changed because the takeoffs
// and annotations on the page cannot be stored at that scale." when *unstorableContent is YES, or "This bid is locked." when *locked is YES. After a YES the caller updates ProjectData
// with ost_domain::apply_page_scales (the memory twin) and refreshes the views (plan 4.11).
+ (BOOL)savePageScales:(const std::vector<std::string> &)pageUIDs
                  inBid:(NSString *)bidUID
           scaleFactor1:(double)scaleFactor1
           scaleFactor2:(double)scaleFactor2
                 locked:(BOOL *)locked
        unstorableContent:(BOOL *)unstorableContent
              errorText:(NSString **)errorText;

@end

#endif
