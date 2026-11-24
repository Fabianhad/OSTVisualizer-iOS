// Layer: Persistence
// Depends on: Domain (PageDeletion), OstDatabase
// Forbidden: UIKit, Foundation, Renderer
// Public declarations of the store functions of one M1 feature: Delete Page.
// Included once at the end of ProjectStore.hpp (plan 4.4 and 6.1); the definitions are in Components/ProjectStore+PageDelete.cpp
// (signature style of duplicate_condition: db_path, bid_uid, ..., std::string *error = nullptr).
#pragma once
#include <set>
#include <string>
#include <vector>

// Delete Page (plan 5.6, owner decision N-M1-23). Windows: ost_visualizer/infrastructure/mdb/components/settings_operations.py:562-577
// (delete_pages) and the cascade :698-891; ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:528-573
// (get_pages_with_delete_content). The page tables, the chunk of 50 and the texts are in Domain/PageDeletion.hpp.
namespace ost_store
{
    // Windows delete_pages on ONE bid, in ONE transaction, all or nothing. `bid_uid` is an iOS addition (Windows derives the bid from
    // the pages; register DP-6): every page must be a row of that bid (the Cover Sheet path does the same, settings_operations.py:102-104)
    // in addition to the Windows single-bid rule. The cascade is the 15 Windows steps in the Windows order (research map 2.2), the
    // uids in chunks of ost_domain::kBidOwnedUidChunk, any failed statement = rollback and false (strict; Windows warns and goes on
    // only in a test double, register DP-1).
    //
    // Returns false and writes nothing when: `page_uids` is empty; a uid is not a positive integer (integrity::parse_uid: ASCII digits,
    // no spaces, no sign, within 63 bits; an empty text is refused, ost_domain::normalize_page_uids is for the caller); `bid_uid` is
    // not a positive integer or names no bid; a page does not exist, belongs to another bid, has no bid, or its bid has no Bids row;
    // two rows share a page uid, or two of the page's comments (BidComments.BidUID = bid and BidPageUID in the pages) or takeoffs share a
    // uid or hold a uid that is not a positive integer (NULL included); the bid is locked (integrity::bid_is_locked, inside the
    // transaction, after the ownership checks; a lock read that fails refuses too); the delete would leave the bid without a page
    // (every page of the bid named); any statement fails. A repeat inside `page_uids` is dropped silently (Windows _normalize_int_uids).
    //
    // `error`, when given, is ALWAYS written: the Windows message of the refusal (the text Windows logs: "BidPages has no row for UID 7;
    // the requested authoritative owner does not exist.", "BidPages.UID=200 does not belong to Bids.UID=10." for a page of another
    // bid, "BidPages contains duplicate UID 7; authoritative bid-owned UIDs must be unique.", ...), ost_domain::LAST_PAGE_MESSAGE for
    // the last page, and the EMPTY text for a success, for a failed statement or a failed read and for the lock refusal (the UI shows
    // ost_domain::page_delete_failure_text(Delete) or its lock text for those).
    bool delete_pages(const std::string &db_path,
                      const std::string &bid_uid,
                      const std::vector<std::string> &page_uids,
                      std::string *error = nullptr);

    // Windows get_pages_with_delete_content: the uids (decimal text, as stored) of the pages OF THE BID that have a row in a table of
    // ost_domain::PAGE_DELETE_CONFIRMATION_TABLES (takeoffs, annotations, comments, named views, hot links; never a legend), read only.
    // `pages` is REPLACED (cleared first): empty on every failure, never a partial set. Windows reads the bid uid as text and does not
    // check that the bid exists: a bid without pages (or one that does not exist) is a success with an empty set; a table or column the
    // schema lacks is skipped; a database that cannot be read, a BidPages table without UID or BidUID ("Missing BidPages.UID"), two BidPages
    // rows with the same uid ("BidPages contains duplicate UID 1; ...") or any failed statement is false with `error` set (never empty on
    // a failure, empty on a success).
    bool pages_with_delete_content(const std::string &db_path,
                                   const std::string &bid_uid,
                                   std::set<std::string> &pages,
                                   std::string *error = nullptr);
}
