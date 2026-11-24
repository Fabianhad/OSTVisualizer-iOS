// Layer: Persistence
// Depends on: Domain (ProjectData, DatabaseHierarchy), OstXmlParser
// Forbidden: UIKit, Foundation, Renderer
// Public declarations of the store functions of one M1 feature: Rename Page.
// Included once at the end of ProjectStore.hpp (plan 4.4 and 6.1). The declaration of rename_page has the signature style of
// duplicate_condition: db_path, bid_uid, ..., std::string *error = nullptr. Implemented in Components/ProjectStore+PageRename.cpp.
#pragma once
#include <string>

namespace ost_store
{
    // Windows save_page_name (infrastructure/mdb/components/page_operations.py:130-152): UPDATE [BidPages] SET [Name]=? WHERE [UID]=? in
    // ONE transaction (plan 4.4, 5.5; register RP-1 to RP-9). Never throws; nothing is written unless the function returns true.
    //
    // Refused (returns false, the database is byte-identical to before, `error` says why):
    //   - a bid uid or a page uid that is not a positive integer (integrity::parse_uid: "abc", "0", "-5", "", " 7", "1.0", "+7" ...;
    //     Windows bid_owned_identity.py:105-124); "007" is page 7;
    //   - a bid that does not exist, a page that does not exist, a page that belongs to another bid or to no bid (Windows
    //     require_single_bid_scope_for_uids checks only that the page has ONE existing bid; the bid argument is stricter, RP-7);
    //   - a locked bid (plan 4.1: integrity::bid_is_locked INSIDE the transaction, after the ownership checks; an unresolved status
    //     and a failed read count as locked);
    //   - a `new_name` that is not already what ost_domain::decide_page_rename would write: not stripped (normalize_page_name), blank,
    //     longer than ost_domain::MAX_PAGE_NAME_UNITS UTF-16 units, containing NUL or not valid UTF-8 (the store never trusts the
    //     screen; a name equal to the stored name is accepted: the rule layer says Unchanged first, the store must still take it);
    //   - a BidPages table without the Name column; a statement that fails; an UPDATE that does not change exactly ONE row (RP-8:
    //     Windows ignores the row count), and a database that is busy or cannot be opened.
    // Only BidPages.Name of that one row changes (Sequence, Index1, BidPageFolderUID, the takeoffs and every other table stay).
    // `error` (optional): cleared on entry; set to the reason (for the log) on every refusal, empty on success.
    bool rename_page(const std::string &db_path,
                     const std::string &bid_uid,
                     const std::string &page_uid,
                     const std::string &new_name,
                     std::string *error = nullptr);
}
