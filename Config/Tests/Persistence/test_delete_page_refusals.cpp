// M1 Delete Page (plan 5.6): the data-safety cases of ost_store::delete_pages that Windows does not have as tests, written from the plan
// (5.6 tests-first plan, research map section 4): every refusal returns false, sets `error` and leaves the WHOLE database byte-identical
// (snapshot_all, every table); the last page of a bid is refused in the Core (register DP-5); a locked bid keeps every row (plan 4.1); a
// schema that lacks a table or column is skipped like Windows does; names and repeats.
// The texts of the Windows refusals are the oracle values of DeletePageOracleData.hpp (the real Windows delete_pages was executed). Where
// iOS refuses an input that Windows accepts (a uid with spaces, a sign, a non-ASCII digit: integrity::parse_uid, register DP oddity 11) only
// "refused, nothing written, a text is given" is pinned, because no Windows text exists for it.
#include "DeletePageTestSupport.hpp"

#include <string>

using ost_delpage::run;
using ost_delpage::Uids;
using ost_ptest::TempDb;

namespace
{
    // seed_two_bids (bids 10 and 20 in project 2; bid 10: pages 100, 101, conditions 30, 31; bid 20: page 200, takeoff 900) plus page 102 of
    // bid 10, so that bid 10 has three pages and bid 20 has exactly one.
    void world(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        ost_ptest::seed_two_bids(db);
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (102, 10, 'P102')");
    }

    // The text of a refusal that the plan does not pin (the lock refusal, a failed statement or read, a malformed bid argument): any text, or none.
    const char *const kAnyText = "*";

    // The call must refuse and leave the database exactly as it was. `expected_error`: the exact text; empty = SOME text (the refusals of
    // delete_pages name their cause: the Windows message where Windows has one); kAnyText = the text is not part of the contract.
    void expect_refused(const TempDb &db, const std::string &bid, const Uids &uids, const std::string &expected_error = "")
    {
        const std::string before = db.snapshot_all();
        std::string error = "<unset>";
        CHECK_FALSE(run(db, bid, uids, error));
        if (expected_error == kAnyText)
            CHECK(error != "<unset>"); // always written
        else if (expected_error.empty())
            CHECK(!error.empty() && error != "<unset>");
        else
            CHECK_STR_EQ(error, expected_error);
        CHECK_EQ(db.snapshot_all(), before);
    }

    std::string pages(const TempDb &db, const std::string &where = "1")
    {
        return ost_ptest::list(db, "SELECT UID FROM BidPages WHERE " + where + " ORDER BY UID");
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------------------------
// The uid list
// ---------------------------------------------------------------------------------------------------------------------------------

// Windows: the service returns False for an empty list (project_write_service.py:6931-6932) and the persistence layer would raise "BidPages mutation
// requires at least one authoritative target." (bid_owned_identity.py:216-218): the iOS store takes no empty list.
TEST(DelPageRefusals, AnEmptyListIsRefused)
{
    TempDb db("delpage_ref_empty_list");
    world(db);
    expect_refused(db, "10", {}, ost_delpage_oracle::kMsgEmptyList);
}

// research 4: a list of only empty texts (ost_domain::normalize_page_uids drops them for the caller; the store itself takes no empty uid).
TEST(DelPageRefusals, OnlyEmptyTextsAreRefused)
{
    TempDb db("delpage_ref_empty_texts");
    world(db);
    expect_refused(db, "10", {""});
    expect_refused(db, "10", {"", ""});
}

// A uid that is not an ASCII decimal number above zero (integrity::parse_uid): refused, nothing written. Windows int() would accept the
// spellings with spaces, a plus sign and non-ASCII digits and the numbers beyond 63 bits; the iOS list comes from its own project, so the
// stricter parser is kept (research 2.4 and oddity 11).
TEST(DelPageRefusals, AMalformedUidIsRefusedAndNothingIsDeleted)
{
    const std::string nul_seven("7\0", 2);
    const std::string cases[] = {" 100", "100 ", "1.0", "abc", "\xD9\xA1\xD9\xA0\xD9\xA0" /* Arabic-Indic 100 */, "\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90" /* fullwidth 100 */,
                                 "9223372036854775808", "+100", "0x64", "1e2", nul_seven, "-", "--5", "100,101", "\t100", "100\n"};
    for (const std::string &uid : cases)
    {
        SCOPED_CONTEXT("uid [" + uid + "]");
        TempDb db("delpage_ref_malformed");
        world(db);
        expect_refused(db, "10", {uid});
    }
}

// Port of the Windows messages for a uid that is zero or negative (bid_owned_identity.py:112-117) and for text int() refuses (bulk_write_helpers.py:31):
// "0" and "-5" give "BidPages contains malformed UID <uid>; authoritative bid-owned UIDs must be positive integers.", "x" gives "Invalid BidPages UID: 'x'".
TEST(DelPageRefusals, ZeroAndNegativeUidsGiveTheWindowsMalformedText)
{
    TempDb db("delpage_ref_zero_negative");
    world(db);
    expect_refused(db, "10", {"0"}, ost_delpage_oracle::kMsgMalformed0);
    expect_refused(db, "10", {"-5"}, ost_delpage_oracle::kMsgMalformedMinus5);
    expect_refused(db, "10", {"x"}, ost_delpage_oracle::kMsgInvalidX);
}

// research 4: one bad uid among good ones: the good ones must NOT be deleted (all or nothing), whichever order the list has.
TEST(DelPageRefusals, OneBadUidAmongGoodOnesDeletesNothing)
{
    for (const Uids &uids : {Uids{"100", "abc"}, Uids{"abc", "100"}, Uids{"100", "0"}, Uids{"0", "100"}, Uids{"100", "", "101"}, Uids{"101", "-1", "100"}})
    {
        std::string label;
        for (const std::string &uid : uids)
            label += "[" + uid + "]";
        SCOPED_CONTEXT("list " + label);
        TempDb db("delpage_ref_one_bad");
        world(db);
        expect_refused(db, "10", uids);
        CHECK_STR_EQ(pages(db, "BidUID = 10"), "100,101,102");
    }
}

// Windows int() reads leading zeros: "0101" is page 101 and "101" + "0101" are ONE page (_normalize_int_uids drops the repeat by value). A repeat in
// the list is dropped silently (bulk_write_helpers.py:32-35).
TEST(DelPageRefusals, RepeatsAndLeadingZerosNameOnePageOnce)
{
    {
        TempDb db("delpage_ref_leading_zero");
        world(db);
        CHECK(run(db, "10", {"0101"}));
        CHECK_STR_EQ(pages(db, "BidUID = 10"), "100,102");
    }
    {
        TempDb db("delpage_ref_repeat");
        world(db);
        CHECK(run(db, "10", {"101", "101", "0101", "101"}));
        CHECK_STR_EQ(pages(db, "BidUID = 10"), "100,102");
    }
}

// Windows require_single_bid_scope_for_uids: "BidPages has no row for UID <the smallest missing uid>; the requested authoritative owner does not exist.".
TEST(DelPageRefusals, AUidWithoutARowIsRefusedAndTheSmallestMissingOneIsNamed)
{
    TempDb db("delpage_ref_not_found");
    world(db);
    expect_refused(db, "10", {"99"}, ost_delpage_oracle::kMsgNoRow99);
    expect_refused(db, "10", {"100", "99", "7"}, ost_delpage_oracle::kMsgNoRow7Of99And7);
    expect_refused(db, "10", {"2147483647"}, "BidPages has no row for UID 2147483647; the requested authoritative owner does not exist.");
    expect_refused(db, "10", {"9223372036854775807"}, "BidPages has no row for UID 9223372036854775807; the requested authoritative owner does not exist.");
}

// research 4: the same page deleted twice: the second call is refused ("has no row") and changes nothing.
TEST(DelPageRefusals, TheSamePageDeletedTwiceIsRefusedTheSecondTime)
{
    TempDb db("delpage_ref_twice");
    world(db);
    CHECK(run(db, "10", {"101"}));
    const std::string after_first = db.snapshot_all();

    std::string error;
    CHECK_FALSE(run(db, "10", {"101"}, error));
    CHECK_STR_EQ(error, "BidPages has no row for UID 101; the requested authoritative owner does not exist.");
    CHECK_EQ(db.snapshot_all(), after_first);
    CHECK_STR_EQ(pages(db, "BidUID = 10"), "100,102");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// The bid
// ---------------------------------------------------------------------------------------------------------------------------------

// The bid argument is an iOS addition (register DP-6): a page of ANOTHER bid is refused with the Windows cover-sheet text (settings_operations.py:102-104,
// bid_owned_identity.py:164-202): "BidPages.UID=200 does not belong to Bids.UID=10." (oracle: the real require_existing_bid_scoped_uid_match).
TEST(DelPageRefusals, APageOfAnotherBidIsRefusedWithTheBidScopedText)
{
    TempDb db("delpage_ref_foreign_bid");
    world(db);
    expect_refused(db, "10", {"200"}, ost_delpage_oracle::kMsgForeignBid200Of10);
    expect_refused(db, "20", {"100"}, "BidPages.UID=100 does not belong to Bids.UID=20.");
    expect_refused(db, "20", {"101"}, "BidPages.UID=101 does not belong to Bids.UID=20.");
}

// A batch that mixes two bids (Windows: "BidPages mutation targets do not belong to one authoritative Bid.", test_ownership_and_atomicity.py:51):
// refused whichever bid the call names.
TEST(DelPageRefusals, ABatchOfTwoBidsIsRefusedWhicheverBidTheCallNames)
{
    TempDb db("delpage_ref_mixed");
    world(db);
    expect_refused(db, "10", {"100", "200"});
    expect_refused(db, "20", {"100", "200"});
    expect_refused(db, "10", {"200", "100"});
}

// A page whose BidUID is NULL (Windows IncoherentBidOwnedScopeError, "BidPages mutation targets do not belong to one authoritative Bid.").
TEST(DelPageRefusals, APageWithoutABidIsRefused)
{
    TempDb db("delpage_ref_null_bid");
    world(db);
    db.must("UPDATE BidPages SET BidUID = NULL WHERE UID = 101");
    expect_refused(db, "10", {"101"});
    expect_refused(db, "20", {"101"});
    expect_refused(db, "10", {"100", "101"});
}

// A page whose bid has no Bids row (test_ownership_and_atomicity.py:448): refused whichever bid the call names.
TEST(DelPageRefusals, APageWhoseBidHasNoBidsRowIsRefused)
{
    TempDb db("delpage_ref_orphan");
    world(db);
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (300, 99, 'orphan')");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (301, 99, 'orphan too')");
    expect_refused(db, "99", {"300"}, ost_delpage_oracle::kMsgOrphanBid99);
    expect_refused(db, "10", {"300"});
    expect_refused(db, "20", {"300"});
}

// The bid argument is checked before anything is written: a text that is no bid uid, and a bid that does not exist, are refused.
TEST(DelPageRefusals, TheBidArgumentMustBeAKnownBid)
{
    for (const std::string &bid : {std::string(""), std::string("0"), std::string("-10"), std::string("x"), std::string("10 "), std::string(" 10"), std::string("1.0"),
                                   std::string("99"), std::string("9223372036854775808"), std::string("11")})
    {
        SCOPED_CONTEXT("bid [" + bid + "]");
        TempDb db("delpage_ref_bid_arg");
        world(db);
        expect_refused(db, bid, {"100"}, kAnyText);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Rows of the page that make the cascade unsafe (the duplicates and malformed uids Windows refuses with the table label)
// ---------------------------------------------------------------------------------------------------------------------------------

// Windows require_valid_unique_bid_owned_uids over the page's takeoffs and comments: a uid <= 0 or NULL is "malformed UID <uid>" (a NULL is shown as
// <missing>), a repeated uid is "duplicate UID <n>". Oracle: the real delete_pages. BidTakeoffs / BidComments are recreated loosely (no primary key) so that
// such rows can exist.
TEST(DelPageRefusals, ATakeoffOrCommentOfThePageWithAMalformedUidIsRefusedWithTheTableLabel)
{
    struct Case
    {
        const char *table;
        const char *uid_sql;
        const char *expected;
    };
    const Case cases[] = {{"BidTakeoffs", "0", ost_delpage_oracle::kMsgTakeoffUid0},
                          {"BidTakeoffs", "-3", ost_delpage_oracle::kMsgTakeoffUidMinus3},
                          {"BidTakeoffs", "NULL", ost_delpage_oracle::kMsgTakeoffUidNull},
                          {"BidComments", "0", ost_delpage_oracle::kMsgCommentUid0},
                          {"BidComments", "NULL", ost_delpage_oracle::kMsgCommentUidNull}};
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string(c.table) + " uid " + c.uid_sql);
        TempDb db("delpage_ref_malformed_child");
        world(db);
        db.must("DROP TABLE [" + std::string(c.table) + "]");
        if (std::string(c.table) == "BidTakeoffs")
            db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER)");
        else
            db.must("CREATE TABLE BidComments (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, ParentCommentUID INTEGER)");
        db.must("INSERT INTO [" + std::string(c.table) + "] (UID, BidUID, BidPageUID) VALUES (" + c.uid_sql + ", 10, 100)");
        expect_refused(db, "10", {"100"}, c.expected);
    }
}

// The select of the page's takeoffs and comments is bid AND page scoped: a duplicate uid among the rows of another page, or of another bid, does not matter.
TEST(DelPageRefusals, DuplicateChildUidsOutsideThePageDoNotMatter)
{
    TempDb db("delpage_ref_dup_elsewhere");
    world(db);
    db.must("DROP TABLE BidTakeoffs");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER)");
    db.must("INSERT INTO BidTakeoffs VALUES (8, 10, 100)");
    db.must("INSERT INTO BidTakeoffs VALUES (9, 10, 101)");
    db.must("INSERT INTO BidTakeoffs VALUES (9, 10, 101)");
    db.must("INSERT INTO BidTakeoffs VALUES (7, 20, 200)");
    db.must("INSERT INTO BidTakeoffs VALUES (7, 20, 200)");
    db.must("DROP TABLE BidComments");
    db.must("CREATE TABLE BidComments (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, ParentCommentUID INTEGER)");
    db.must("INSERT INTO BidComments VALUES (5, 10, 101, NULL)");
    db.must("INSERT INTO BidComments VALUES (5, 10, 101, NULL)");

    CHECK(run(db, "10", {"100"}));
    CHECK_STR_EQ(pages(db, "BidUID = 10"), "101,102");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 4);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// The last page (register DP-5)
// ---------------------------------------------------------------------------------------------------------------------------------

// Windows has the rule in the UI only (ui_event_coordinator.py:5893, :5995-6001, "Cannot delete the last page in the bid."); the writer would delete every
// page. iOS: the Core refuses, with the Windows text, and writes nothing.
TEST(DelPageRefusals, TheOnlyPageOfABidIsRefusedWithTheWindowsText)
{
    TempDb db("delpage_ref_only_page");
    world(db);
    // bid 20 has exactly one page (200) although bid 10 has three
    expect_refused(db, "20", {"200"}, "Cannot delete the last page in the bid.");
    expect_refused(db, "20", {"200", "200"}, "Cannot delete the last page in the bid.");
    CHECK_STR_EQ(pages(db, "BidUID = 20"), "200");
}

// A list that names every page of the bid is the same refusal, whatever the order and the repeats, and writes nothing (not even the steps before the
// last statement: the whole cascade is behind the check).
TEST(DelPageRefusals, AListNamingEveryPageOfTheBidIsRefused)
{
    for (const Uids &uids : {Uids{"100", "101", "102"}, Uids{"102", "100", "101"}, Uids{"100", "101", "102", "100"}, Uids{"0100", "101", "102"}})
    {
        std::string label;
        for (const std::string &uid : uids)
            label += "[" + uid + "]";
        SCOPED_CONTEXT("list " + label);
        TempDb db("delpage_ref_all_pages");
        world(db);
        expect_refused(db, "10", uids, "Cannot delete the last page in the bid.");
    }
}

// The same refusal on the full oracle world: the cascade must not have started (every table, steps 1-14, is still as it was).
TEST(DelPageRefusals, TheLastPageRefusalOnAFullWorldWritesNothingAtAll)
{
    TempDb db("delpage_ref_last_full");
    ost_delpage::seed_scenario(db, ost_delpage::scenario("A_DeleteOnePage"));
    // bid 1 has the pages 10, 11, 12
    expect_refused(db, "1", {"10", "11", "12"}, "Cannot delete the last page in the bid.");
    expect_refused(db, "1", {"12", "10", "11"}, "Cannot delete the last page in the bid.");
}

// One page less than the whole bid is fine; two pages of three leave the third; the survivor keeps its row.
TEST(DelPageRefusals, AllButOnePageCanBeDeleted)
{
    {
        TempDb db("delpage_ref_all_but_one");
        world(db);
        CHECK(run(db, "10", {"100", "101"}));
        CHECK_STR_EQ(pages(db, "BidUID = 10"), "102");
        CHECK_STR_EQ(pages(db, "BidUID = 20"), "200");
    }
    {
        TempDb db("delpage_ref_two_of_three");
        world(db);
        CHECK(run(db, "10", {"102", "100"}));
        CHECK_STR_EQ(pages(db, "BidUID = 10"), "101");
    }
}

// The pages of another bid never count as "a page is left": bid 10 with one page and bid 20 with many is still refused.
TEST(DelPageRefusals, PagesOfOtherBidsDoNotCountAsRemainingPages)
{
    TempDb db("delpage_ref_other_bids_pages");
    world(db);
    db.must("DELETE FROM BidPages WHERE UID IN (101, 102)");
    ost_ptest::seed_pages(db, 20, 3, 0, 2000);
    expect_refused(db, "10", {"100"}, "Cannot delete the last page in the bid.");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// The bid lock (plan 4.1): inside the transaction, after the ownership checks
// ---------------------------------------------------------------------------------------------------------------------------------

// A bid whose job status is Locked keeps every row: nothing is deleted, cleared or touched in any table.
TEST(DelPageRefusals, ALockedBidIsRefusedAndKeepsEveryRow)
{
    TempDb db("delpage_ref_locked");
    world(db);
    ost_ptest::seed_locked_bid(db, 10, 5);
    expect_refused(db, "10", {"100"}, kAnyText);
    expect_refused(db, "10", {"100", "101"}, kAnyText);
}

// The lock is read for the bid of the write: the full oracle world with its bid locked is untouched, with a status uid no JobStatuses row names
// (unresolved = locked, fail closed) as well.
TEST(DelPageRefusals, ALockedBidOfTheFullWorldAndAnUnresolvedStatusKeepEveryRow)
{
    {
        TempDb db("delpage_ref_locked_full");
        ost_delpage::seed_scenario(db, ost_delpage::scenario("A_DeleteOnePage"));
        ost_ptest::seed_locked_bid(db, 1, 5);
        expect_refused(db, "1", {"10"}, kAnyText);
    }
    {
        TempDb db("delpage_ref_unresolved_full");
        ost_delpage::seed_scenario(db, ost_delpage::scenario("A_DeleteOnePage"));
        ost_ptest::seed_bid_with_status(db, 1, "77");
        expect_refused(db, "1", {"10"}, kAnyText);
    }
}

// A bid with no job status, and a bid whose status is not locked, are deletable; a locked OTHER bid does not block this one.
TEST(DelPageRefusals, UnlockedBidsAreDeletableAndAnotherBidsLockDoesNotMatter)
{
    {
        TempDb db("delpage_ref_open_status");
        world(db);
        ost_ptest::seed_open_bid(db, 10, 4);
        CHECK(run(db, "10", {"100"}));
        CHECK_STR_EQ(pages(db, "BidUID = 10"), "101,102");
    }
    {
        TempDb db("delpage_ref_other_locked");
        world(db);
        ost_ptest::seed_locked_bid(db, 20, 5);
        CHECK(run(db, "10", {"100"}));
        CHECK_STR_EQ(pages(db, "BidUID = 10"), "101,102");
        CHECK_STR_EQ(pages(db, "BidUID = 20"), "200");
    }
}

// The lock read fails closed: a Bids table without JobStatusUID (or without Bids at all) is a failed read, so the delete is refused (Windows would
// read a missing column as unlocked; the plan keeps the safe side, 4.1).
TEST(DelPageRefusals, AFailedLockReadRefusesTheDelete)
{
    TempDb db("delpage_ref_lock_read");
    world(db);
    ost_delpage::drop_column(db, "Bids", "JobStatusUID");
    expect_refused(db, "10", {"100"}, kAnyText);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Names, schema drift
// ---------------------------------------------------------------------------------------------------------------------------------

// Two pages with the same name and two with an empty name: only the targeted uid disappears; the other pages keep their rows.
TEST(DelPageRefusals, DuplicateAndEmptyNamesDeleteOnlyTheTargetedUid)
{
    TempDb db("delpage_ref_names");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_pages(db, 10, std::vector<ost_ptest::SeedPage>{{"Same"}, {"Same"}, {""}, {""}, {"Other"}});
    // uids 1000 .. 1004
    const std::string survivor_before = ost_delpage::row_dump(db, "BidPages", "UID = 1000");
    CHECK(run(db, "10", {"1001"}));
    CHECK_STR_EQ(pages(db, "BidUID = 10"), "1000,1002,1003,1004");
    CHECK(run(db, "10", {"1003"}));
    CHECK_STR_EQ(pages(db, "BidUID = 10"), "1000,1002,1004");
    CHECK_EQ(ost_delpage::row_dump(db, "BidPages", "UID = 1000"), survivor_before);
}

// A table of the cascade that does not exist in the file is skipped, like Windows (schema.optional_table_missing): the delete succeeds, the page goes, and
// every other table is what the Windows oracle holds. One database per dropped table (the 23 child tables and five more tables the cascade writes).
TEST(DelPageRefusals, EveryOptionalTableOfTheCascadeIsSkippedWhenTheFileLacksIt)
{
    Uids tables;
    for (const char *table : ost_delpage_oracle::page_delete_child_tables())
        tables.push_back(table);
    for (const char *table : {"BidTakeoffs", "BidNamedViews", "BidHotLinks", "AffectDPCTypGroupViews", "BidSettings"})
        tables.push_back(table);
    const ost_delpage_oracle::Scenario &scenario = ost_delpage::scenario("A_DeleteOnePage");
    for (const std::string &table : tables)
    {
        SCOPED_CONTEXT("dropped table " + table);
        TempDb db("delpage_ref_drop_table");
        ost_delpage::seed_scenario(db, scenario);
        db.must("DROP TABLE [" + table + "]");
        std::string error;
        CHECK(run(db, "1", {"10"}, error));
        CHECK_STR_EQ(error, "");
        CHECK_STR_EQ(pages(db, "BidUID = 1"), "11,12");
    }
}

// A column the cascade needs for one step is missing: the step is skipped (Windows column_exists guards), the page still goes and the page rows of
// the other tables are untouched. The tables are recreated without the column.
TEST(DelPageRefusals, EveryReferencingColumnOfTheCascadeIsSkippedWhenTheFileLacksIt)
{
    struct Drop
    {
        const char *table;
        const char *column;
    };
    std::vector<Drop> drops = {{"BidPages", "MasterPageUID"},        {"BidComments", "ParentCommentUID"},        {"BidTakeoffs", "ParentUID"},
                               {"BidTakeoffs", "TypGroupTakeoffUID"}, {"BidTakeoffs", "TypPageTakeoffUID"},       {"BidTakeoffs", "TypGroupMarkerUID"},
                               {"BidPercents", "BidTakeoffUID"},      {"BidDimensions", "BidTakeoffFromUID"},     {"BidDimensions", "BidTakeoffToUID"},
                               {"BidALines", "BidTakeoffFromUID"},    {"BidArrows", "BidTakeoffToUID"},           {"BidHotLinks", "BidPageViewUID"},
                               {"BidSettings", "BidPageSelectedUID"}, {"Bids", "CoverSheetSelItemType"},          {"Bids", "CoverSheetSelItemUID"},
                               {"BidTypGroupViews", "BidPageUID"},    {"AffectDPCTypGroupViews", "BidTypGroupViewUID"}};
    const ost_delpage_oracle::Scenario &scenario = ost_delpage::scenario("A_DeleteOnePage");
    for (const Drop &drop : drops)
    {
        SCOPED_CONTEXT(std::string("dropped column ") + drop.table + "." + drop.column);
        TempDb db("delpage_ref_drop_column");
        ost_delpage::seed_scenario(db, scenario);
        ost_delpage::drop_column(db, drop.table, drop.column);
        CHECK(run(db, "1", {"10"}));
        CHECK_STR_EQ(pages(db, "BidUID = 1"), "11,12");
        CHECK_STR_EQ(pages(db, "BidUID = 2"), "20");
    }
}

// The page column of a child table is missing: that table cannot be matched, so its rows stay (Windows skips it); nothing else is affected.
TEST(DelPageRefusals, ATableWithoutItsPageColumnKeepsAllItsRows)
{
    for (const char *table : {"BidHighlights", "BidLegends", "Boost", "DPCCalcFilter", "BidMarkedPages"})
    {
        SCOPED_CONTEXT(std::string("table ") + table);
        TempDb db("delpage_ref_no_page_column");
        ost_delpage::seed_scenario(db, ost_delpage::scenario("A_DeleteOnePage"));
        ost_delpage::drop_column(db, table, "BidPageUID");
        CHECK(run(db, "1", {"10"}));
        CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM [") + table + "]"), 3);
        CHECK_STR_EQ(pages(db, "BidUID = 1"), "11,12");
    }
}

// The page table itself: no BidPages table, no BidPages.BidUID column (the ownership check cannot be made) are refusals; nothing is written.
TEST(DelPageRefusals, ADatabaseWithoutAUsablePageTableIsRefused)
{
    {
        TempDb db("delpage_ref_no_pages_table");
        world(db);
        db.must("DROP TABLE BidPages");
        expect_refused(db, "10", {"100"}, kAnyText);
    }
    {
        TempDb db("delpage_ref_no_bid_column");
        world(db);
        ost_delpage::drop_column(db, "BidPages", "BidUID");
        expect_refused(db, "10", {"100"}, kAnyText);
    }
}
