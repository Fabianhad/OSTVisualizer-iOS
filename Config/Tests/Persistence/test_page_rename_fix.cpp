// Rename Page, tests added by the FIXER after the mutation check of the new Core (they only add; nothing of the other test_page_rename_*.cpp
// files is changed). Each test kills a mutant that the earlier tests survived: a refusal that the store reaches through TWO guards gave the
// same data result with either guard, so only the reason text (which is for the log, not for the user) tells which guard ran.
//
//   1. the explicit bid check of ProjectStore+PageRename.cpp (Windows require_existing_unique_bid_owned_uid_matches(Bids)): an unknown bid
//      is refused with "The bid does not exist."; without the check the ownership guard and the lock guard would refuse the same call with
//      other reasons ("The page does not exist in this bid." / "This bid is locked."), and no test saw the difference.
//   2. the explicit column check (Windows _execute_update_values: require_column(BidPages, Name)): a missing Name column is refused with
//      its own reason; without the check the UPDATE itself fails to prepare and the call is refused with "The rename statement could not
//      be prepared.", again the same data result.
#include "StoreTestSupport.hpp"

#include "Persistence/ProjectStore/ProjectStore_PageRename.hpp"

#include <string>

using ost_ptest::TempDb;

namespace
{
    std::string page_name(const TempDb &db, const std::string &uid)
    {
        return db.text("SELECT Name FROM BidPages WHERE UID = " + uid);
    }
} // namespace

// An unknown bid is refused for what it is (the bid check runs first), and a page that is not a page of an existing bid is refused with
// the ownership reason. The database stays identical; the control renames.
TEST(PageRenameFix, Reason_AnUnknownBidIsNamedBeforeThePageIsLookedAt)
{
    TempDb db("rename_fix_unknown_bid");
    ost_ptest::seed_two_bids(db);
    const std::string before = db.snapshot_all();

    std::string error = "stale";
    CHECK_FALSE(ost_store::rename_page(db.path(), "99", "100", "Renamed", &error)); // bid 99 does not exist
    CHECK_EQ(error, std::string("The bid does not exist."));
    CHECK_EQ(db.snapshot_all(), before);

    error = "stale";
    CHECK_FALSE(ost_store::rename_page(db.path(), "99", "9999", "Renamed", &error)); // neither the bid nor the page exists
    CHECK_EQ(error, std::string("The bid does not exist."));

    error = "stale";
    CHECK_FALSE(ost_store::rename_page(db.path(), "10", "9999", "Renamed", &error)); // the bid exists, the page does not
    CHECK_EQ(error, std::string("The page does not exist in this bid."));
    CHECK_EQ(db.snapshot_all(), before);

    CHECK(ost_store::rename_page(db.path(), "10", "100", "Renamed")); // control
    CHECK_EQ(page_name(db, "100"), std::string("Renamed"));
}

// A BidPages table without its Name column is refused with the reason of the column check; the lock read and the ownership checks all
// work on the full schema, so only this guard can answer. Control: the column is back and the same call renames.
TEST(PageRenameFix, Reason_AMissingNameColumnIsNamed)
{
    TempDb db("rename_fix_no_name_column");
    ost_ptest::seed_two_bids(db);
    db.must("ALTER TABLE BidPages DROP COLUMN Name");
    const std::string before = db.snapshot_all();

    std::string error = "stale";
    CHECK_FALSE(ost_store::rename_page(db.path(), "10", "100", "Renamed", &error));
    CHECK_EQ(error, std::string("BidPages has no Name column."));
    CHECK_EQ(db.snapshot_all(), before);

    db.must("ALTER TABLE BidPages ADD COLUMN Name TEXT");
    CHECK(ost_store::rename_page(db.path(), "10", "100", "Renamed")); // control
    CHECK_EQ(page_name(db, "100"), std::string("Renamed"));
}
