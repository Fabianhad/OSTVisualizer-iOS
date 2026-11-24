// M1 Delete Page (plan 5.6), REVIEW ADDITIONS: the ORDER of the refusals of ost_store::delete_pages inside its transaction (plan 4.4 item 2:
// parse the uids, the bid exists, the rows belong to the OPEN bid, THEN the bid lock, THEN the value rules such as the last page, then the writes).
// The earlier tests pin that every refusal leaves the whole database identical, but all of them accept any text for the lock refusal, so a store
// that checked the lock before the ownership rules (or after the last-page rule) passed every one of them. The text of a refusal is the only
// thing that tells the order: an ownership refusal carries the Windows message of the failed identity check, the lock refusal carries NO text
// (the UI shows its own lock text, ProjectStore_PageDelete.hpp), the last-page refusal carries ost_domain::LAST_PAGE_MESSAGE.
#include "DeletePageTestSupport.hpp"

#include <string>

using ost_delpage::run;
using ost_delpage::Uids;
using ost_ptest::TempDb;

namespace
{
    // seed_two_bids (bid 10: pages 100, 101, conditions 30, 31; bid 20: page 200, takeoff 900) plus page 102 of bid 10.
    void world(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        ost_ptest::seed_two_bids(db);
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (102, 10, 'P102')");
    }

    // The call refuses, the error is EXACTLY `expected_error` (empty = no text), and the whole database is as it was.
    void expect_refused_with(const TempDb &db, const std::string &bid, const Uids &uids, const std::string &expected_error)
    {
        const std::string before = db.snapshot_all();
        std::string error = "<unset>";
        CHECK_FALSE(run(db, bid, uids, error));
        CHECK_STR_EQ(error, expected_error);
        CHECK_EQ(db.snapshot_all(), before);
    }
} // namespace

// The lock refusal alone has no text; every refusal of the ownership rules keeps its Windows text although the bid is locked, because the lock is
// read AFTER them (plan 4.4 item 2). A locked bid and a page that is missing / of another bid / of two bids, a malformed uid and an empty list.
TEST(DelPageOrder, TheOwnershipRefusalsComeBeforeTheLockRefusal)
{
    TempDb db("delpage_ord_ownership_first");
    world(db);
    ost_ptest::seed_locked_bid(db, 10, 5);

    expect_refused_with(db, "10", {"100"}, ""); // the lock alone: no text
    expect_refused_with(db, "10", {"100", "101"}, "");
    expect_refused_with(db, "10", {"99"}, ost_delpage_oracle::kMsgNoRow99);
    expect_refused_with(db, "10", {"100", "99"}, ost_delpage_oracle::kMsgNoRow99);
    expect_refused_with(db, "10", {"200"}, ost_delpage_oracle::kMsgForeignBid200Of10);
    expect_refused_with(db, "10", {"100", "200"}, ost_delpage_oracle::kMsgMixedBids);
    expect_refused_with(db, "10", {"0"}, ost_delpage_oracle::kMsgMalformed0);
    expect_refused_with(db, "10", {"x"}, ost_delpage_oracle::kMsgInvalidX);
    expect_refused_with(db, "10", {}, ost_delpage_oracle::kMsgEmptyList);
}

// The same on a page whose bid has no Bids row and on a page without a bid: the ownership text, not the silence of the lock.
TEST(DelPageOrder, AnOrphanOrBidlessPageKeepsItsOwnershipTextInALockedBid)
{
    TempDb db("delpage_ord_orphan_locked");
    world(db);
    ost_ptest::seed_locked_bid(db, 10, 5);
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (300, 99, 'orphan')");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (301, 99, 'orphan too')");
    db.must("UPDATE BidPages SET BidUID = NULL WHERE UID = 101");

    expect_refused_with(db, "99", {"300"}, ost_delpage_oracle::kMsgOrphanBid99); // the open bid does not exist
    expect_refused_with(db, "10", {"101"}, ost_delpage_oracle::kMsgNullBid);
    expect_refused_with(db, "10", {"300"}, ost_delpage_oracle::kMsgOrphanBid99); // the page's bid is not a bid at all: that text wins over "does not belong"
}

// The lock comes BEFORE the last-page rule (a value rule): the only page of a locked bid is refused without a text, not with the last-page text. The
// bid is unlocked afterwards (an open status) and the same call is refused as the last page, so the two refusals are told apart by their text only.
TEST(DelPageOrder, TheLockRefusalComesBeforeTheLastPageRefusal)
{
    TempDb db("delpage_ord_lock_then_last");
    world(db);
    ost_ptest::seed_locked_bid(db, 20, 5); // bid 20 has exactly one page, 200
    expect_refused_with(db, "20", {"200"}, "");

    ost_ptest::seed_open_bid(db, 20, 4);
    expect_refused_with(db, "20", {"200"}, ost_domain::LAST_PAGE_MESSAGE);
}

// A refusal in the MIDDLE of the cascade (the identity rules of the page's own comments and takeoffs run after step 1, and after step 2, have written) must
// roll those writes back: page 100 is the master of page 101 (step 1 clears it), the reply 6 on page 101 hangs under the comment 5 on page 100 (step 2
// detaches it), and then a duplicate takeoff uid (or a malformed one) on page 100 refuses the delete. Every table is identical afterwards, the master
// and the reply parent included. (The earlier refusal tests have no reference for steps 1 and 2 to write, so a store that COMMITTED on a refusal passed them.)
TEST(DelPageOrder, ARefusalInTheMiddleOfTheCascadeUndoesTheEarlierSteps)
{
    struct Case
    {
        const char *name;
        const char *takeoff_rows; // loose BidTakeoffs rows (UID, BidUID, BidPageUID) of page 100
        const char *expected_error;
    };
    const Case cases[] = {{"duplicate takeoff uid", "(8, 10, 100), (8, 10, 100)", ost_delpage_oracle::kMsgDuplicateTakeoff8},
                          {"takeoff uid zero", "(0, 10, 100)", ost_delpage_oracle::kMsgTakeoffUid0},
                          {"takeoff without uid", "(NULL, 10, 100)", ost_delpage_oracle::kMsgTakeoffUidNull}};
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("case: ") + c.name);
        TempDb db("delpage_ord_mid_refusal");
        world(db);
        db.must("UPDATE BidPages SET MasterPageUID = 100 WHERE UID = 101");
        db.must("INSERT INTO BidComments (UID, BidUID, BidPageUID, ParentCommentUID) VALUES (5, 10, 100, NULL)");
        db.must("INSERT INTO BidComments (UID, BidUID, BidPageUID, ParentCommentUID) VALUES (6, 10, 101, 5)");
        db.must("DROP TABLE BidTakeoffs");
        db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER)");
        db.must(std::string("INSERT INTO BidTakeoffs VALUES ") + c.takeoff_rows);

        expect_refused_with(db, "10", {"100"}, c.expected_error);
        CHECK_EQ(db.count("SELECT MasterPageUID FROM BidPages WHERE UID = 101"), 100);
        CHECK_EQ(db.count("SELECT ParentCommentUID FROM BidComments WHERE UID = 6"), 5);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE UID = 100"), 1);
    }
}

// A page WITHOUT a bid (BidUID NULL) named together with pages of the open bid is refused as a whole and is never deleted along with them. The bid has a
// spare page, so the last-page rule cannot be the reason for the refusal (with only two pages left the earlier test passed through that rule: a store that
// let the NULL-bid page through the single-bid check was refused later as "the last page" and the gap stayed hidden). Same for a page of ANOTHER bid in a list of
// the open bid's pages: the bid with the lower uid is the open one, so only the single-bid rule stands between that page and the delete.
TEST(DelPageOrder, APageWithoutABidOrOfAnotherBidIsNeverDeletedAlongWithTheOpenBidsPages)
{
    TempDb db("delpage_ord_bidless_with_spare");
    world(db);
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (103, 10, 'P103')"); // bid 10 now has 100, 101, 102, 103
    db.must("UPDATE BidPages SET BidUID = NULL WHERE UID = 101");                  // ... and 101 has no bid
    ost_ptest::seed_pages(db, 20, 2, 0, 2000);                                      // bid 20 has 200, 2000, 2001: deleting 200 is no last page

    expect_refused_with(db, "10", {"100", "101"}, ost_delpage_oracle::kMsgNullBid);
    expect_refused_with(db, "10", {"101", "100"}, ost_delpage_oracle::kMsgNullBid);
    expect_refused_with(db, "10", {"100", "200"}, ost_delpage_oracle::kMsgMixedBids);
    expect_refused_with(db, "10", {"200", "100", "102"}, ost_delpage_oracle::kMsgMixedBids);
    expect_refused_with(db, "10", {"2000", "100"}, ost_delpage_oracle::kMsgMixedBids);
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidPages WHERE BidUID IS NULL OR BidUID = 20 ORDER BY UID"), "101,200,2000,2001");
}

// Windows steps 3 to 6 are BID scoped (settings_operations.py:742-796: SELECT [UID] FROM [BidTakeoffs] WHERE [BidUID]=? AND [BidPageUID] IN (...)), step 9 is
// not (:832-837). A takeoff of ANOTHER bid that sits on the deleted page (invalid data) is therefore not in the list of deleted takeoffs: the references to it
// (a survivor's ParentUID, an arrow that starts at it, a percent of it) are NOT cleared or deleted, and the takeoff itself goes with the page. Pinned as
// Windows does it, like DP-7 and DP-11 (register: kept, documented); a store that selected the page's takeoffs without the bid would clear and delete them.
TEST(DelPageOrder, ATakeoffOfAnotherBidOnTheDeletedPageIsDeletedButNoReferenceToItIsTouched)
{
    TempDb db("delpage_ord_foreign_takeoff");
    world(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (700, 20, 30, 100, '1;2;3;4')"); // bid 20 on page 100 of bid 10
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, TypGroupTakeoffUID, Position) VALUES (701, 10, 30, 101, 700, 700, '1;2;3;4')");
    db.must("INSERT INTO BidArrows (UID, BidUID, BidPageUID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (1, 10, 101, 700, NULL)");
    db.must("INSERT INTO BidPercents (UID, BidPageUID, BidTakeoffUID) VALUES (1, 101, 700)");

    std::string error;
    CHECK(run(db, "10", {"100"}, error));
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 700"), 0); // step 9: deleted with the page, whatever its bid
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(ParentUID, 'N') || ',' || ifnull(TypGroupTakeoffUID, 'N') FROM BidTakeoffs WHERE UID = 701"), "701:700,700");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidArrows WHERE UID = 1"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPercents WHERE UID = 1"), 1);
}

// The bid cascade (permanently_delete_project) shares the 23-table list with Delete Page and deletes each table TWICE over: through the bid's pages and by the
// table's own BidUID, and six of the tables (BidTakeoffTotals ... DPCCalcFilter) are also in a second by-bid loop. A row that carries the bid's page but NOT the
// bid (BidUID NULL: invalid data) is reached only through the pages of the first loop, so this pins that EVERY table of the shared list still takes part in it
// (the earlier cascade test seeds BidUID = the bid, which the second loop alone empties for the last six tables: a list that lost its last entry passed it).
TEST(DelPageOrder, TheBidCascadeReachesEveryTableOfTheSharedListThroughThePagesAlone)
{
    TempDb db("delpage_ord_bid_cascade_pages_only");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db);
    db.must("DELETE FROM BidTakeoffs");
    db.must("UPDATE Bids SET BidProjectUID = 1 WHERE UID = 10"); // only a bid in the system folder 'Deleted Bids' can be deleted for good
    int uid = 8000;
    for (std::size_t i = 0; i < ost_domain::PAGE_DELETE_CHILD_TABLE_COUNT; ++i)
    {
        const std::string table = ost_domain::PAGE_DELETE_CHILD_TABLES[i];
        const bool has_bid = db.count("SELECT COUNT(*) FROM pragma_table_info('" + table + "') WHERE name = 'BidUID'") > 0;
        db.must("INSERT INTO [" + table + "] (UID, " + (has_bid ? "BidUID, " : "") + "BidPageUID) VALUES (" + std::to_string(uid++) + ", " + (has_bid ? "NULL, " : "") + "100)");
        db.must("INSERT INTO [" + table + "] (UID, " + (has_bid ? "BidUID, " : "") + "BidPageUID) VALUES (" + std::to_string(uid++) + ", " + (has_bid ? "NULL, " : "") + "200)");
    }

    CHECK(ost_store::permanently_delete_project(db.path(), "10"));
    for (std::size_t i = 0; i < ost_domain::PAGE_DELETE_CHILD_TABLE_COUNT; ++i)
    {
        const std::string table = ost_domain::PAGE_DELETE_CHILD_TABLES[i];
        SCOPED_CONTEXT("table " + table);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM [" + table + "] WHERE BidPageUID = 100"), 0);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM [" + table + "] WHERE BidPageUID = 200"), 1);
    }
}
