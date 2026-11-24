// ProjectStore component - Delete Page (plan 5.6; declarations in ProjectStore_PageDelete.hpp, the table lists and texts in
// Domain/PageDeletion.hpp).
//
// Windows sources (HEAD 49a2d59):
//   settings_operations.py:562-577   delete_pages (normalise the uids, derive the bid from the pages, cascade, one transaction)
//   settings_operations.py:698-891   _delete_pages_cascade (the 15 steps, each skipped when its table or column is missing)
//   bid_owned_identity.py:105-124, :205-235   the uid rules and the single-bid scope (and their messages)
//   bulk_write_helpers.py:22-101     the chunks of 50 uids
//   bid_data_reader.py:528-573       get_pages_with_delete_content
//
// Write pattern of plan 4.4: parse -> bid exists -> the pages belong to the OPEN bid -> bid lock -> last page -> the writes -> commit, in ONE
// transaction; any failed statement is a rollback and `false` (strict: Windows warns and goes on only in a test double, register DP-1).
#include "../ProjectStore_PageDelete.hpp"
#include "../ProjectStore_Integrity.hpp"
#include "../../OstDatabase.hpp"
#include "../../../Domain/PageDeletion.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace ost_store
{
    namespace
    {
        using ost_db::OstDatabase;
        using ost_db::Statement;

        bool has_column(OstDatabase &db, const char *table, const char *column)
        {
            return db.table_exists(table) && db.column_exists(table, column);
        }

        std::string placeholders(std::size_t count)
        {
            std::string out;
            for (std::size_t i = 0; i < count; ++i)
                out += i == 0 ? "?" : ",?";
            return out;
        }

        // bulk_write_helpers._iter_access_chunks: the values in chunks of ost_domain::kBidOwnedUidChunk, in order.
        template <class F>
        bool each_chunk(const std::vector<long long> &values, F &&f)
        {
            for (std::size_t i = 0; i < values.size(); i += ost_domain::kBidOwnedUidChunk)
            {
                const std::size_t n = std::min(ost_domain::kBidOwnedUidChunk, values.size() - i);
                if (!f(values.data() + i, n))
                    return false;
            }
            return true;
        }

        // `prefix` + "(?, ?, ...)" + `suffix`, once per chunk of `values`; the `leading` values are bound first, then the chunk. true when
        // every chunk ran (an empty `values` runs nothing, like Windows); false at the first statement that failed.
        bool run_chunked(OstDatabase &db, const std::string &prefix, const std::string &suffix, const std::vector<long long> &leading,
                         const std::vector<long long> &values)
        {
            return each_chunk(values, [&](const long long *chunk, std::size_t n) {
                Statement statement = db.prepare(prefix + "(" + placeholders(n) + ")" + suffix);
                if (!statement.valid())
                    return false;
                int index = 1;
                for (const long long value : leading)
                    statement.bind_int64(index++, value);
                for (std::size_t j = 0; j < n; ++j)
                    statement.bind_int64(index++, chunk[j]);
                return statement.run();
            });
        }

        // The same for a SELECT of one column: the cells (NULL as "") are appended to `out` in row order.
        bool select_chunked(OstDatabase &db, const std::string &prefix, const std::vector<long long> &leading,
                            const std::vector<long long> &values, std::vector<std::string> &out)
        {
            return each_chunk(values, [&](const long long *chunk, std::size_t n) {
                Statement statement = db.prepare(prefix + "(" + placeholders(n) + ")");
                if (!statement.valid())
                    return false;
                int index = 1;
                for (const long long value : leading)
                    statement.bind_int64(index++, value);
                for (std::size_t j = 0; j < n; ++j)
                    statement.bind_int64(index++, chunk[j]);
                while (statement.step())
                    out.push_back(statement.column_type(0) == Statement::TypeNull ? std::string() : statement.column_text(0));
                return !statement.failed();
            });
        }

        // The Python int() text of a decimal string with an optional minus sign, without leading zeros ("-007" is "-7", "000" is "0");
        // "" when the text is not that (Python int() would refuse it).
        std::string python_int_text(const std::string &text)
        {
            std::size_t i = 0;
            bool negative = false;
            if (i < text.size() && text[i] == '-')
            {
                negative = true;
                ++i;
            }
            if (i >= text.size())
                return "";
            for (std::size_t j = i; j < text.size(); ++j)
                if (!std::isdigit(static_cast<unsigned char>(text[j])))
                    return "";
            while (i + 1 < text.size() && text[i] == '0')
                ++i;
            const std::string digits = text.substr(i);
            if (digits == "0")
                return "0";
            return (negative ? "-" : "") + digits;
        }

        // The Windows message for a page uid that integrity::parse_uid refuses: a number that is not positive is "malformed UID" of
        // require_valid_unique_bid_owned_uids (bid_owned_identity.py:112-117), anything Python int() refuses is "Invalid BidPages UID"
        // (bulk_write_helpers.py:31).
        std::string bad_page_uid_text(const std::string &uid)
        {
            const std::string number = python_int_text(uid);
            if (!number.empty() && (number[0] == '-' || number == "0"))
                return "BidPages contains malformed UID " + number + "; authoritative bid-owned UIDs must be positive integers.";
            return "Invalid BidPages UID: '" + uid + "'";
        }

        // The ids of the cells `uids` (each already checked by integrity::uid_list_problem).
        std::vector<long long> parsed_uids(const std::vector<std::string> &uids)
        {
            std::vector<long long> out;
            out.reserve(uids.size());
            for (const std::string &uid : uids)
            {
                long long value = 0;
                integrity::parse_uid(uid, value);
                out.push_back(value);
            }
            return out;
        }

        // The body of delete_pages inside its open transaction: check() (the refusals), then run() (the 15 steps). Both return false
        // when the transaction must be rolled back; `refusal` then holds the text for the caller ("" for a failed statement, a failed
        // read and the lock).
        class PageDeleteCascade
        {
        public:
            PageDeleteCascade(OstDatabase &db, long long bid, const std::vector<long long> &pages, std::string &refusal)
                : _db(db), _bid(bid), _pages(pages), _refusal(refusal), _with_bid{bid}
            {
            }

            // Windows require_single_bid_scope_for_uids + the iOS bid argument (settings_operations.py:568-570, :102-104) + the iOS
            // lock and last-page rules. false: refused (`_refusal` holds the text, "" for the lock) or a statement failed (`_refusal`
            // empty).
            bool check()
            {
                if (!integrity::bid_exists(_db, _bid))
                    return refuse("Bids has no row for UID " + std::to_string(_bid) + "; the requested authoritative owner does not exist.");

                // _fetch_uid_rows: the rows of the requested pages, in chunks.
                std::map<long long, std::string> bid_of; // page uid -> BidUID text ("" = NULL)
                std::vector<std::string> row_uids;
                const bool read = each_chunk(_pages, [&](const long long *chunk, std::size_t n) {
                    Statement statement = _db.prepare("SELECT [UID], [BidUID] FROM [BidPages] WHERE [UID] IN (" + placeholders(n) + ")");
                    if (!statement.valid())
                        return false;
                    for (std::size_t j = 0; j < n; ++j)
                        statement.bind_int64(static_cast<int>(j) + 1, chunk[j]);
                    while (statement.step())
                    {
                        const std::string uid = statement.column_text(0);
                        row_uids.push_back(uid);
                        long long value = 0;
                        if (integrity::parse_uid(uid, value))
                            bid_of[value] = statement.column_type(1) == Statement::TypeNull ? std::string() : statement.column_text(1);
                    }
                    return !statement.failed();
                });
                if (!read)
                    return fail();
                // two physical rows with one uid (and a NULL / malformed uid) are refused with the Windows text (test :609).
                const std::string problem = integrity::uid_list_problem(row_uids, "BidPages");
                if (!problem.empty())
                    return refuse(problem);

                std::set<long long> found;
                for (const long long uid : parsed_uids(row_uids))
                    found.insert(uid);
                long long smallest_missing = 0;
                for (const long long page : _pages)
                    if (!found.count(page) && (smallest_missing == 0 || page < smallest_missing))
                        smallest_missing = page;
                if (smallest_missing != 0)
                    return refuse("BidPages has no row for UID " + std::to_string(smallest_missing) +
                                  "; the requested authoritative owner does not exist.");

                // one authoritative bid: no NULL BidUID and exactly one distinct value (:228-232)
                std::set<std::string> bids;
                bool any_null = false;
                for (const auto &entry : bid_of)
                {
                    if (entry.second.empty())
                        any_null = true;
                    else
                        bids.insert(entry.second);
                }
                if (any_null || bids.size() != 1)
                    return refuse("BidPages mutation targets do not belong to one authoritative Bid.");
                long long owner = 0;
                if (!integrity::parse_uid(*bids.begin(), owner) || !integrity::bid_exists(_db, owner))
                    return refuse("Bids has no row for UID " + *bids.begin() + "; the requested authoritative owner does not exist.");
                // the iOS bid argument: the pages must be rows of THE OPEN bid (the Cover Sheet path does the same, :102-104 and
                // bid_owned_identity.py:198-201).
                if (owner != _bid)
                {
                    for (const long long page : _pages)
                    {
                        long long page_bid = 0;
                        integrity::parse_uid(bid_of[page], page_bid);
                        if (page_bid != _bid)
                            return refuse("BidPages.UID=" + std::to_string(page) + " does not belong to Bids.UID=" + std::to_string(_bid) + ".");
                    }
                }

                // The bid lock (plan 4.1), inside the transaction, after the ownership checks. A lock read that fails counts as locked.
                bool locked = true;
                integrity::bid_is_locked(_db, _bid, locked);
                if (locked)
                    return refuse(std::string());

                // The last page (iOS, register DP-5; Windows keeps this rule in the UI only): the pages are unique rows of this bid, so the
                // bid keeps a page when it has more pages than the delete names.
                Statement count = _db.prepare("SELECT COUNT(*) FROM [BidPages] WHERE [BidUID] = ?");
                if (!count.valid())
                    return fail();
                count.bind_int64(1, _bid);
                if (!count.step())
                    return fail();
                if (count.column_int64(0) - static_cast<long long>(_pages.size()) <= 0)
                    return refuse(ost_domain::LAST_PAGE_MESSAGE);
                return true;
            }

            // settings_operations.py:698-891, step by step in the Windows order. false: a statement failed or an identity check of the
            // page's own rows refused (the caller rolls back).
            bool run()
            {
                // step 1 (:701-711): pages of the SAME bid that used a deleted page as master lose the reference
                if (has_column(_db, "BidPages", "MasterPageUID"))
                    if (!run_chunked(_db, "UPDATE [BidPages] SET [MasterPageUID] = NULL WHERE [BidUID] = ? AND [MasterPageUID] IN ", "",
                                     _with_bid, _pages))
                        return fail();

                // step 2 (:712-741): the replies to a comment that is about to go become root comments
                if (has_column(_db, "BidComments", "UID") && has_column(_db, "BidComments", "BidUID") &&
                    has_column(_db, "BidComments", "BidPageUID") && has_column(_db, "BidComments", "ParentCommentUID"))
                {
                    std::vector<std::string> comment_cells;
                    if (!select_chunked(_db, "SELECT [UID] FROM [BidComments] WHERE [BidUID] = ? AND [BidPageUID] IN ", _with_bid, _pages,
                                        comment_cells))
                        return fail();
                    const std::string problem = integrity::uid_list_problem(comment_cells, "BidComments");
                    if (!problem.empty())
                        return refuse(problem);
                    if (!run_chunked(_db, "UPDATE [BidComments] SET [ParentCommentUID] = NULL WHERE [BidUID] = ? AND [ParentCommentUID] IN ",
                                     "", _with_bid, parsed_uids(comment_cells)))
                        return fail();
                }

                // step 3 (:742-762): the uids of the page's takeoffs; step 4 (:763-775): the survivors lose their references to them
                std::vector<long long> takeoffs;
                if (has_column(_db, "BidTakeoffs", "UID") && has_column(_db, "BidTakeoffs", "BidUID") &&
                    has_column(_db, "BidTakeoffs", "BidPageUID"))
                {
                    std::vector<std::string> takeoff_cells;
                    if (!select_chunked(_db, "SELECT [UID] FROM [BidTakeoffs] WHERE [BidUID] = ? AND [BidPageUID] IN ", _with_bid, _pages,
                                        takeoff_cells))
                        return fail();
                    const std::string problem = integrity::uid_list_problem(takeoff_cells, "BidTakeoffs");
                    if (!problem.empty())
                        return refuse(problem);
                    takeoffs = parsed_uids(takeoff_cells);
                    for (const char *column : {"ParentUID", "TypGroupTakeoffUID", "TypPageTakeoffUID", "TypGroupMarkerUID"})
                        if (_db.column_exists("BidTakeoffs", column))
                            if (!run_chunked(_db,
                                             std::string("UPDATE [BidTakeoffs] SET [") + column + "] = NULL WHERE [BidUID] = ? AND [" + column +
                                                 "] IN ",
                                             "", _with_bid, takeoffs))
                                return fail();
                }

                // step 5 (:776-784): the percents of those takeoffs
                if (has_column(_db, "BidPercents", "BidTakeoffUID"))
                    if (!run_chunked(_db, "DELETE FROM [BidPercents] WHERE [BidTakeoffUID] IN ", "", _none, takeoffs))
                        return fail();

                // step 6 (:785-796): dimensions, lines and arrows that START or END at one of those takeoffs, wherever they are drawn
                for (const char *table : {"BidDimensions", "BidALines", "BidArrows"})
                    if (_db.table_exists(table))
                        for (const char *column : {"BidTakeoffFromUID", "BidTakeoffToUID"})
                            if (_db.column_exists(table, column))
                                if (!run_chunked(_db, std::string("DELETE FROM [") + table + "] WHERE [" + column + "] IN ", "", _none, takeoffs))
                                    return fail();

                // step 7 (:797-813): before step 8 deletes BidTypGroupViews
                if (has_column(_db, "AffectDPCTypGroupViews", "BidTypGroupViewUID") && has_column(_db, "BidTypGroupViews", "UID") &&
                    has_column(_db, "BidTypGroupViews", "BidPageUID"))
                    if (!run_chunked(_db,
                                     "DELETE FROM [AffectDPCTypGroupViews] WHERE [BidTypGroupViewUID] IN (SELECT [UID] FROM [BidTypGroupViews] "
                                     "WHERE [BidPageUID] IN ",
                                     ")", _none, _pages))
                        return fail();

                // step 8 (:814-831): the page's own children, NOT bid-scoped. Strict: a failing table fails the whole delete (DP-1).
                for (std::size_t i = 0; i < ost_domain::PAGE_DELETE_CHILD_TABLE_COUNT; ++i)
                {
                    const char *table = ost_domain::PAGE_DELETE_CHILD_TABLES[i];
                    if (has_column(_db, table, "BidPageUID"))
                        if (!run_chunked(_db, std::string("DELETE FROM [") + table + "] WHERE [BidPageUID] IN ", "", _none, _pages))
                            return fail();
                }

                // step 9 (:832-837): the page's takeoffs
                if (has_column(_db, "BidTakeoffs", "BidPageUID"))
                    if (!run_chunked(_db, "DELETE FROM [BidTakeoffs] WHERE [BidPageUID] IN ", "", _none, _pages))
                        return fail();

                // step 10 (:838-853): hot links of SURVIVING pages that jump to a named view of a deleted page
                if (has_column(_db, "BidHotLinks", "BidPageViewUID") && has_column(_db, "BidNamedViews", "UID") &&
                    has_column(_db, "BidNamedViews", "BidPageUID"))
                    if (!run_chunked(_db,
                                     "DELETE FROM [BidHotLinks] WHERE [BidPageViewUID] IN (SELECT [UID] FROM [BidNamedViews] WHERE [BidPageUID] IN ",
                                     ")", _none, _pages))
                        return fail();

                // step 11 (:854-859): the hot links on the page; step 12 (:860-865): its named views
                if (has_column(_db, "BidHotLinks", "BidPageUID"))
                    if (!run_chunked(_db, "DELETE FROM [BidHotLinks] WHERE [BidPageUID] IN ", "", _none, _pages))
                        return fail();
                if (has_column(_db, "BidNamedViews", "BidPageUID"))
                    if (!run_chunked(_db, "DELETE FROM [BidNamedViews] WHERE [BidPageUID] IN ", "", _none, _pages))
                        return fail();

                // step 13 (:866-875): the saved "last page" of ANY bid that pointed at a deleted page (Windows does not scope it to the bid)
                if (has_column(_db, "BidSettings", "BidPageSelectedUID"))
                    if (!run_chunked(_db, "UPDATE [BidSettings] SET [BidPageSelectedUID] = NULL WHERE [BidPageSelectedUID] IN ", "", _none, _pages))
                        return fail();

                // step 14 (:876-889): the Cover Sheet selection of type "page" (COVER_SHEET_PAGE_SELECTION_TYPE = 1), any bid, other types stay
                if (has_column(_db, "Bids", "CoverSheetSelItemType") && _db.column_exists("Bids", "CoverSheetSelItemUID"))
                    if (!run_chunked(_db,
                                     "UPDATE [Bids] SET [CoverSheetSelItemUID] = NULL WHERE [CoverSheetSelItemType] = 1 AND [CoverSheetSelItemUID] IN ",
                                     "", _none, _pages))
                        return fail();

                // step 15 (:890-891): the page rows (_require_write_columns: BidPages.UID must exist)
                if (!has_column(_db, "BidPages", "UID"))
                    return fail();
                if (!run_chunked(_db, "DELETE FROM [BidPages] WHERE [UID] IN ", "", _none, _pages))
                    return fail();
                return true;
            }

        private:
            // A refusal of a check: the Windows message for the caller ("" for the lock).
            bool refuse(const std::string &message)
            {
                _refusal = message;
                return false;
            }

            // A failed statement or read: no text (the UI shows page_delete_failure_text(Delete)).
            bool fail()
            {
                _refusal.clear();
                return false;
            }

            OstDatabase &_db;
            long long _bid;
            const std::vector<long long> &_pages;
            std::string &_refusal;
            std::vector<long long> _with_bid;
            std::vector<long long> _none;
        };
    } // namespace

    bool delete_pages(const std::string &db_path, const std::string &bid_uid, const std::vector<std::string> &page_uids, std::string *error)
    {
        if (error)
            error->clear();
        auto refuse = [&](const std::string &message) {
            if (error)
                *error = message;
            return false;
        };
        try
        {
            long long bid = 0;
            if (!integrity::parse_uid(bid_uid, bid))
                return refuse("Invalid Bids UID: '" + bid_uid + "'");
            if (page_uids.empty())
                return refuse("BidPages mutation requires at least one authoritative target.");
            // bulk_write_helpers._normalize_int_uids: a repeat is dropped silently, the first order stays.
            std::vector<long long> pages;
            std::set<long long> seen;
            for (const std::string &uid : page_uids)
            {
                long long value = 0;
                if (!integrity::parse_uid(uid, value))
                    return refuse(bad_page_uid_text(uid));
                if (seen.insert(value).second)
                    pages.push_back(value);
            }

            OstDatabase db;
            if (!db.open(db_path))
                return false;
            if (!db.begin_transaction())
                return false;
            std::string refusal;
            PageDeleteCascade cascade(db, bid, pages, refusal);
            if (!cascade.check() || !cascade.run())
            {
                db.rollback();
                return refuse(refusal);
            }
            return db.commit();
        }
        catch (...)
        {
            // nothing may throw across the Objective-C boundary (std::bad_alloc, std::length_error): the transaction dies with the
            // connection, which the OstDatabase destructor closes (an open transaction is rolled back by sqlite3_close).
            return refuse(std::string());
        }
    }

    bool pages_with_delete_content(const std::string &db_path, const std::string &bid_uid, std::set<std::string> &pages, std::string *error)
    {
        pages.clear();
        if (error)
            error->clear();
        auto fail = [&](const std::string &message) {
            pages.clear();
            if (error)
                *error = message.empty() ? std::string("The pages could not be read.") : message;
            return false;
        };
        try
        {
            OstDatabase db;
            if (!db.open(db_path))
                return fail("The database cannot be opened.");
            {
                // sqlite opens a file that is not a database without complaint: the first read tells.
                Statement probe = db.prepare("SELECT COUNT(*) FROM sqlite_master");
                if (!probe.valid() || !probe.step())
                    return fail("The database cannot be read.");
            }
            // bid_data_reader.py:547-550: no BidPages table is no content; BidPages must carry UID and BidUID.
            if (!db.table_exists("BidPages"))
                return true;
            if (!db.column_exists("BidPages", "UID"))
                return fail("Missing BidPages.UID");
            if (!db.column_exists("BidPages", "BidUID"))
                return fail("Missing BidPages.BidUID");

            std::vector<std::string> uids;
            {
                Statement statement = db.prepare("SELECT [UID] FROM [BidPages] WHERE [BidUID] = ?");
                if (!statement.valid())
                    return fail("The pages could not be read.");
                statement.bind_text(1, bid_uid);
                while (statement.step())
                    uids.push_back(statement.column_type(0) == Statement::TypeNull ? std::string() : statement.column_text(0));
                if (statement.failed())
                    return fail(statement.error_message());
            }
            const std::string problem = integrity::uid_list_problem(uids, "BidPages");
            if (!problem.empty())
                return fail(problem);
            const std::set<std::string> bid_pages(uids.begin(), uids.end());
            if (bid_pages.empty())
                return true; // a bid without pages reads no content table (:558-559)

            std::set<std::string> result;
            for (std::size_t i = 0; i < ost_domain::PAGE_DELETE_CONFIRMATION_TABLE_COUNT; ++i)
            {
                const char *table = ost_domain::PAGE_DELETE_CONFIRMATION_TABLES[i];
                if (!db.table_exists(table) || !db.column_exists(table, "BidPageUID"))
                    continue;
                Statement statement = db.prepare(std::string("SELECT DISTINCT [BidPageUID] FROM [") + table + "] WHERE [BidPageUID] IS NOT NULL");
                if (!statement.valid())
                    return fail("The page content could not be read.");
                while (statement.step())
                {
                    const std::string page = statement.column_text(0);
                    if (bid_pages.count(page))
                        result.insert(page);
                }
                if (statement.failed())
                    return fail(statement.error_message().empty() ? std::string("The page content could not be read.") : statement.error_message());
            }
            pages = result;
            return true;
        }
        catch (...)
        {
            return fail("The pages could not be read.");
        }
    }
}
