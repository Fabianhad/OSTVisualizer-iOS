// Internal header shared by the ProjectStore component files (not part of the public API).
// The identity and ownership rules every Windows writer applies before it touches a row, and the UID allocator:
//   ost_visualizer/infrastructure/database/bid_owned_identity.py   require_* (existence, one authoritative bid)
//   ost_visualizer/infrastructure/mdb/components/identity_allocation.py   _next_uid_preserving_references
//   ost_visualizer/infrastructure/mdb/raw_bid_integrity.py   BID_RELATIONSHIPS (which column references which table)
// Every function reports a failed statement the same way as a refused check (false / 0), so a caller that gets "no" writes nothing.
#pragma once
#include "../OstDatabase.hpp"
#include <string>
#include <utility>
#include <vector>

namespace ost_store
{
    namespace integrity
    {

        // A bid-owned UID as Windows accepts it (require_valid_unique_bid_owned_uids): ASCII decimal digits only, greater than zero
        // ("0", "-5", " 7", "7 ", "1.0", "abc", "" and values beyond 63 bits are malformed). Leading zeros are allowed ("007" is 7).
        bool parse_uid(const std::string &text, long long &out);

        // `table` has a row with this UID.
        bool row_exists(ost_db::OstDatabase &db, const char *table, long long uid);

        // The bid exists (Windows require_existing_unique_bid_owned_uid_matches(Bids)).
        bool bid_exists(ost_db::OstDatabase &db, long long bid_uid);

        // Windows require_existing_bid_scoped_uid_match: `table` has the row, the row belongs to `bid_uid` (BidUID is not NULL and
        // equal) and the bid itself exists.
        bool row_belongs_to_bid(ost_db::OstDatabase &db, const char *table, long long uid, long long bid_uid);

        // Windows require_single_bid_scope_for_uids for one target: the row exists, its BidUID is not NULL and that bid exists.
        // `bid_uid` is the authoritative bid of the row.
        bool owner_bid_of(ost_db::OstDatabase &db, const char *table, long long uid, long long &bid_uid);

        // Windows _next_uid_preserving_references: MAX(UID) + 1 of `table` (1 when it is empty), raised above the largest value of
        // every column that references the table (BID_RELATIONSHIPS), so a new row never adopts a dangling reference. 0 when a
        // statement failed. Must run inside the transaction that inserts the row.
        long long next_uid_preserving_references(ost_db::OstDatabase &db, const char *table);

        // The UID allocator for a Windows Long column (Access COUNTER, 32 bit; plan 4.2, owner decision N-M1-04): `count` consecutive UIDs
        // starting at next_uid_preserving_references(db, table), refused BEFORE anything is written when the last one would pass
        // 2147483647 (first + count - 1 > 2147483647, computed in 64 bit: nothing wraps, whatever `count` is). Windows keeps no UID ceiling
        // (identity_allocation.py:5-47, only RefNo is guarded: condition_operations.py:56-62): this is a deliberate safe deviation, so that
        // a row the iOS store creates can still be exported into a Windows database (M3).
        // true:  `first_out` is the first UID; the UIDs first_out .. first_out + count - 1 are free and within the limit.
        // false: `first_out` is 0, nothing was written (the helper never writes): `count` < 1, the allocation would pass the limit, or a
        //        statement failed (a missing table, an unusable MAX(UID) beyond 63 bits). The callers show their own text
        //        ("Cannot add a <noun>: no identity is left below 2147483647.", no_identity_left_text). Must run inside the transaction
        //        that inserts the rows.
        // `no_identity_left` (optional) tells the two kinds of refusal apart: true only when the refusal IS the limit (first + count - 1 >
        // 2147483647), false for `count` < 1 and for a failed statement, so that a caller shows the "no identity" text only when it is
        // true and its generic failure text otherwise. Always written when given.
        bool next_uid_within_access_limit(ost_db::OstDatabase &db, const char *table, long long count, long long &first_out,
                                          bool *no_identity_left = nullptr);

        // The largest UID of a Windows Long column (Access COUNTER, 32 bit): 2147483647.
        constexpr long long kAccessLongMax = 2147483647LL;

        // The refusal text of every writer that allocates a UID (plan 4.2 / 4.14, owner decision N-M1-03 may reword it):
        // "Cannot add a <noun>: no identity is left below 2147483647." (noun: "condition", "folder", "layer", ...).
        std::string no_identity_left_text(const std::string &noun);

        // The bid lock rule on one connection (plan 4.1; ost_domain::resolve_bid_status_lock; the writers call it INSIDE their transaction,
        // after the ownership checks). Reads Bids.JobStatusUID of the bid and, only when the bid has a status, the JobStatuses rows, and
        // sets `locked` ALWAYS (never left as it was). The rule is the Windows one, read as the stored values' TEXT:
        //   - the bid has no status (JobStatusUID NULL or the empty text): unlocked, without reading JobStatuses (a broken JobStatuses
        //     table does not matter then);
        //   - the first JobStatuses row whose UID text equals the bid's status text wins (verbatim compare: "01" is not "1", "7" equals 7);
        //     its `Locked` is set unless its stored text is "0", "False" or "" (a NULL, -1, 1, 2, "True", "false", " 0" are all set);
        //   - a status that no row names, an empty or missing JobStatuses table, duplicate JobStatuses UIDs anywhere in the table
        //     (Windows: the list is empty = unresolved), a Sequence cell that Python int() refuses or a real that is not finite in any row
        //     (the Windows read raises there too: settings_reader.py:72, :361-370), a cell of any JobStatuses column (Name included) that is
        //     not valid UTF-8 (a BLOB: Windows decodes it strictly and raises; a TEXT: the safe side), a bid that does not exist, a
        //     Bids.JobStatusUID that is a BLOB: locked. The cells are read as the text Windows makes of them (ost_serializer.py
        //     serialize_value: NULL = "NULL", a whole real = its integer digits, a BLOB = its UTF-8 text without any NUL, a TEXT keeps an
        //     embedded NUL).
        // Returns false and sets `locked = true` when the read itself failed (a missing Bids table or column, a JobStatuses table without
        // UID, Locked or Sequence - Windows would read a missing Locked as unlocked, iOS fails closed -, any failed statement). Whether
        // the unresolved cases above also return false is not part of the contract: callers refuse on `locked`. Reads only.
        bool bid_is_locked(ost_db::OstDatabase &db, long long bid_uid, bool &locked);

        // The checks of a bid load (bid_data_reader.py get_bid_data, hierarchy_reader.py): a problem is reported as the Windows message.
        //
        // Windows require_valid_unique_bid_owned_uids over the stored UIDs of one table in row order ("" stands for a NULL UID): ""
        // when every UID is a positive integer and none repeats (compared as numbers, "007" is 7), else the message of the FIRST
        // problem: "<table> contains malformed UID <uid>; authoritative bid-owned UIDs must be positive integers." (a NULL UID is
        // shown as <missing>) or "<table> contains duplicate UID <n>; authoritative bid-owned UIDs must be unique.".
        std::string uid_list_problem(const std::vector<std::string> &uids, const char *table);

        // Windows require_unique_master_data_uids (master_data_identity.py, CdnTypes): the UIDs only have to be unique, compared as
        // the stored text (NULL is ""; 3 and "3" are the same, 5 and 5.0 are not). "" when none repeats, else
        // "<table> contains duplicate UID <uid>; authoritative master-data UIDs must be unique." for the first repeated one.
        std::string master_data_uid_problem(const std::vector<std::string> &uids, const char *table);

        // Windows require_acyclic_bid_owned_parent_graph / find_takeoff_parent_cycle_uids over (uid, ParentUID) pairs: the uid of a
        // cycle's member that Windows names, i.e. the smallest uid of all the cycles (decimal uids numerically, before any other
        // uid), or "" when the parents form no cycle. A parent of "", "0" or one that is no uid of the list ends the chain; a row
        // that is its own parent is a cycle.
        std::string find_parent_cycle_uid(const std::vector<std::pair<std::string, std::string>> &uid_and_parent);

    }
}
