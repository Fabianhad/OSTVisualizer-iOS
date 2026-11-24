// Layer: Services (Objective-C++)
// The thin bridge of Delete Page (plan 4.4 item 3, 5.6): converts the types and calls ost_store::delete_pages and
// ost_store::pages_with_delete_content. The rules (the last page, the table cascade, the lock, the uid checks) live in Core and the store;
// the decision "may this page be deleted, which page is shown next" is ost_data::plan_page_delete, the texts are
// ost_domain::page_delete_confirmation / page_delete_failure_text (Core/Domain/PageDeletion.hpp).
#ifndef IMPORTSERVICE_PAGEDELETE_H
#define IMPORTSERVICE_PAGEDELETE_H

#import "ImportService.h"

@interface ImportService (PageDelete)

// Deletes the pages of the bid `bidUID` with everything on them (Windows delete_pages, settings_operations.py:562-577 and the cascade
// :698-891) in ONE transaction, all of them or none. The list loses empty texts and repeats first (Windows _unique_nonempty_uids).
// YES when the delete was committed. NO, and nothing written, when the store refuses: an empty list, a uid that is not a positive integer, a
// page that is not a page of THIS bid, pages of two bids, a locked bid (also when the lock cannot be read: fail closed), a delete that would
// leave the bid without a page, or a statement that fails (a database locked by another application).
// `*errorText` (optional) is nil on success. On a refusal it is the reason: the text ost_domain::LAST_PAGE_MESSAGE ("Cannot delete the last
// page in the bid.", shown with the title ost_domain::LAST_PAGE_TITLE) for the last page, the Windows message of the failed identity check
// for the others, and nil for a failed statement, a failed read and a locked bid; for every refusal that is not the last page the user text
// is ost_domain::page_delete_failure_text(Delete), and the lock text "This bid is locked." comes from the lock helper BEFORE the action.
+ (BOOL)deletePages:(NSArray<NSString *> *)pageUIDs
              inBid:(NSString *)bidUID
              error:(NSString **)errorText;

// The uids of the pages of the bid `bidUID` that hold takeoffs, annotations, comments, named views or hot links (the pages whose delete asks
// for the content confirmation; a page with only a legend is not among them: Windows get_pages_with_delete_content, bid_data_reader.py:
// 528-573). Reads only. nil when the pages cannot be read (a failed read is never a partial set; Windows returns None): `*errorText` (optional)
// then names the cause and the caller shows ost_domain::page_delete_failure_text(VerifyContent) and deletes nothing. An empty set is a
// success (a bid without content, or a bid without pages).
+ (NSSet<NSString *> *)pagesWithDeleteContentInBid:(NSString *)bidUID error:(NSString **)errorText;

@end

#endif
