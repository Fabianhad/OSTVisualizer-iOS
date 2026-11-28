// ProjectStore component - Rename Page (see ProjectStore_PageRename.hpp, plan 4.4 and 5.5).
// Windows: page_operations.py:130-152 save_page_name (require_single_bid_scope_for_uids, then ONE `UPDATE [BidPages] SET [Name]=? WHERE
// [UID]=?`), bid_owned_identity.py:105-124 / :144-152 / :205-235 (the uid and ownership refusals), mdb_writer.py:276-302
// (_execute_update_values ignores the row count: register RP-8), active_bid_write_guard.py:21-36 (the locked-bid refusal, plan 4.1).
#include "../ProjectStore_Internal.hpp"
#include "../ProjectStore_PageRename.hpp"
#include "../../../Domain/PageRules.hpp"

namespace ost_store
{
    namespace
    {
        // Everything after the argument check; the one place that opens the database. Every refusal rolls the transaction back (nothing is
        // written unless the function returns true) and sets `reason`.
        bool rename_page_in_database(const std::string &db_path,
                                     const std::string &bid_uid,
                                     const std::string &page_uid,
                                     const std::string &new_name,
                                     std::string &reason)
        {
            // Plan 4.4 item 2, in this order: parse the uids, the bid exists, the row belongs to the OPEN bid, the bid lock, the name rule,
            // the write, the row count, commit.
            long long bid = 0;
            if (!integrity::parse_uid(bid_uid, bid))
            {
                reason = "The bid uid is not a positive integer.";
                return false;
            }
            long long page = 0;
            if (!integrity::parse_uid(page_uid, page))
            {
                reason = "The page uid is not a positive integer.";
                return false;
            }

            ost_db::OstDatabase db;
            if (!db.open(db_path))
            {
                reason = "The database could not be opened.";
                return false;
            }
            if (!db.begin_transaction())
            {
                reason = "The database is busy or cannot be written.";
                return false;
            }

            // From here on every refusal ends the transaction: `refuse` rolls back and reports.
            const auto refuse = [&db, &reason](const char *text)
            {
                db.rollback();
                reason = text;
                return false;
            };

            // Windows require_existing_unique_bid_owned_uid_matches(Bids) and require_single_bid_scope_for_uids: the bid exists, the page
            // exists once and belongs to it. The Windows writer has no bid argument (RP-7): the page must belong to THIS bid. A page with
            // a NULL BidUID and an orphan page are refused (row_belongs_to_bid), and so is a uid that more than one row carries (below).
            if (!integrity::bid_exists(db, bid))
                return refuse("The bid does not exist.");
            if (!integrity::row_belongs_to_bid(db, "BidPages", page, bid))
                return refuse("The page does not exist in this bid.");

            // Windows DuplicateBidOwnedUidError (bid_owned_identity.py:205-235): a page uid that two rows carry, in this bid or in another, is
            // ambiguous and nothing is renamed. row_belongs_to_bid reads only ONE row, so it does not see this; with the UID primary key of
            // create_schema it cannot happen, only a hand-built BidPages table can hold it.
            {
                ost_db::Statement count = db.prepare("SELECT COUNT(*) FROM [BidPages] WHERE [UID] = ?");
                if (!count.valid())
                    return refuse("The page could not be looked up.");
                count.bind_int64(1, page);
                if (!count.step() || count.column_int64(0) != 1)
                    return refuse("More than one page has this uid.");
            }

            // Plan 4.1: a locked bid writes nothing; an unresolved status and a failed read count as locked (fail closed).
            bool locked = true;
            if (!integrity::bid_is_locked(db, bid, locked))
                return refuse("The lock of the bid could not be read.");
            if (locked)
                return refuse("This bid is locked.");

            // The store never trusts the screen (plan 4.4 item 4): the name must be exactly what decide_page_rename would write. The stored
            // name is not compared (an unchanged name is accepted: the rule layer says Unchanged first, the store must still take it), so
            // the rule runs against an empty stored name, with which a name that is not blank can only be Write, TooLong or Invalid.
            const ost_domain::PageRenamePlan plan = ost_domain::decide_page_rename(new_name, std::string());
            if (plan.decision == ost_domain::PageRenameDecision::Refused || plan.decision == ost_domain::PageRenameDecision::TooLong ||
                plan.decision == ost_domain::PageRenameDecision::Invalid)
            {
                db.rollback();
                reason = ost_domain::page_name_refusal_text(plan.decision).message;
                return false;
            }
            if (plan.new_name != new_name)
                return refuse("The page name must not start or end with whitespace.");

            // Windows _execute_update_values: require_column(BidPages, Name) before the UPDATE.
            if (!db.column_exists("BidPages", "Name"))
                return refuse("BidPages has no Name column.");

            // The ONE statement: only BidPages.Name of that row (RP-7: AND BidUID = ?).
            {
                ost_db::Statement update = db.prepare("UPDATE [BidPages] SET [Name] = ? WHERE [UID] = ? AND [BidUID] = ?");
                if (!update.valid())
                    return refuse("The rename statement could not be prepared.");
                update.bind_text(1, new_name);
                update.bind_int64(2, page);
                update.bind_int64(3, bid);
                if (!update.run())
                    return refuse("The rename statement failed.");
            }

            // RP-8: Windows ignores the row count (a page deleted in the meantime gives True with nothing changed). Exactly ONE row must
            // have been changed (sqlite3_changes of the UPDATE; rows changed by a trigger do not count), else the transaction is rolled
            // back. A row that the UPDATE only matched counts as changed even when its name was already the new one.
            {
                ost_db::Statement changes = db.prepare("SELECT changes()");
                if (!changes.valid() || !changes.step() || changes.column_int64(0) != 1)
                    return refuse("The rename did not change exactly one page.");
            }

            if (!db.commit()) // a failed COMMIT rolls itself back (OstDatabase::commit)
            {
                reason = "The rename could not be committed.";
                return false;
            }
            return true;
        }
    } // namespace

    bool rename_page(const std::string &db_path,
                     const std::string &bid_uid,
                     const std::string &page_uid,
                     const std::string &new_name,
                     std::string *error)
    {
        if (error)
            error->clear();
        std::string reason;
        bool renamed = false;
        try
        {
            renamed = rename_page_in_database(db_path, bid_uid, page_uid, new_name, reason);
        }
        catch (...)
        {
            // RP-9: every failure is a return value, nothing throws across the Objective-C boundary. The connection closes with the object,
            // which rolls an open transaction back.
            renamed = false;
            reason = "The rename failed unexpectedly.";
        }
        if (!renamed && error)
            *error = reason.empty() ? std::string("The rename failed.") : reason;
        return renamed;
    }
}
