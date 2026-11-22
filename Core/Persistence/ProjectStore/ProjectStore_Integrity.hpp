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
