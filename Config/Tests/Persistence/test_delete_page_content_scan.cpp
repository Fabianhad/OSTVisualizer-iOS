// M1 Delete Page (plan 5.6): ost_store::pages_with_delete_content, the scan that decides whether a page "holds user content" (takeoffs, annotations,
// comments, named views, hot links; never a legend). Windows 49a2d59: bid_data_reader.py:528-573 get_pages_with_delete_content /
// _parse_pages_with_delete_content; constants.py:111-121 PAGE_DELETE_CONFIRMATION_TABLES.
// Windows tests ported (the same inputs and expected sets): tests/infrastructure/mdb/components/test_bid_data_reader.py DeleteContentScanTests (:2003-2057),
// test_delete_content_scan_reports_only_this_bids_pages_with_user_content (:756), test_delete_content_scan_rejects_duplicate_page_uids (:775),
// test_delete_content_scan_discards_partial_results_after_failure (:744) and DeleteContentScanScopeTests (:4640). The Windows fakes are SQLite-backed
// with loosely typed tables; the ports create the same loose tables (a TempDb without the schema). Windows returns None on a failure, iOS returns false with
// `pages` empty and `error` set ("never a partial set").
// The failure of a read is injected by overwriting the root pages of a table (DeletePageTestSupport.hpp corrupt_table).
#include "DeletePageTestSupport.hpp"

#include <fstream>
#include <iterator>
#include <set>
#include <string>

using ost_ptest::TempDb;

namespace
{
    using Pages = std::set<std::string>;

    // The scan; `pages` starts with a stale entry so that a call that merges instead of replacing, or leaves the caller's set alone on a failure, is seen.
    bool scan(const TempDb &db, const std::string &bid, Pages &pages, std::string &error)
    {
        pages = {"stale"};
        error = "<unset>";
        return ost_store::pages_with_delete_content(db.path(), bid, pages, &error);
    }

    // A loose database: BidPages (UID, BidUID) and the named content tables with (UID, BidPageUID), as the Windows tests make them.
    void loose_pages(const TempDb &db, std::initializer_list<std::pair<int, int>> pages)
    {
        db.must("CREATE TABLE BidPages (UID INTEGER, BidUID INTEGER)");
        for (const auto &page : pages)
            db.must("INSERT INTO BidPages VALUES (" + std::to_string(page.first) + ", " + std::to_string(page.second) + ")");
    }

    void loose_table(const TempDb &db, const std::string &table)
    {
        db.must("CREATE TABLE [" + table + "] (UID INTEGER, BidPageUID INTEGER)");
    }
} // namespace

// Port of :2004 test_every_confirmation_table_marks_its_page_and_legends_do_not: for each of the 15 PAGE_DELETE_CONFIRMATION_TABLES a database with BidPages
// (1, bid 7), (2, bid 7) and one row (50, page 2) in that table gives {"2"}; BidLegends is not one of them.
TEST(DelPageScan, EveryConfirmationTableMarksItsPage)
{
    REQUIRE_EQ(ost_domain::PAGE_DELETE_CONFIRMATION_TABLE_COUNT, static_cast<std::size_t>(15));
    for (std::size_t i = 0; i < ost_domain::PAGE_DELETE_CONFIRMATION_TABLE_COUNT; ++i)
    {
        const std::string table = ost_domain::PAGE_DELETE_CONFIRMATION_TABLES[i];
        SCOPED_CONTEXT("table " + table);
        CHECK(table != "BidLegends");
        TempDb db("delpage_scan_table", /*with_schema=*/false);
        loose_pages(db, {{1, 7}, {2, 7}});
        loose_table(db, table);
        db.must("INSERT INTO [" + table + "] VALUES (50, 2)");
        Pages pages;
        std::string error;
        CHECK(scan(db, "7", pages, error));
        CHECK_STR_EQ(error, "");
        CHECK_STR_EQ(ost_delpage::joined(pages), "2");
    }
}

// The same tables, driven by the Windows list itself (the oracle) on a database made by create_schema: each of the 15 marks page 2, and a row in BidLegends
// alone marks nothing (the legend is deleted with the page, but never asks for a confirmation: register DP-3, research 3.2 test 23).
TEST(DelPageScan, TheFifteenWindowsTablesMarkTheirPageAndALegendNeverDoes)
{
    for (const char *table : ost_delpage_oracle::page_delete_confirmation_tables())
    {
        SCOPED_CONTEXT(std::string("table ") + table);
        TempDb db("delpage_scan_schema_table");
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO BidPages (UID, BidUID) VALUES (1, 7)");
        db.must("INSERT INTO BidPages (UID, BidUID) VALUES (2, 7)");
        db.must(std::string("INSERT INTO [") + table + "] (UID, BidPageUID) VALUES (50, 2)");
        Pages pages;
        std::string error;
        CHECK(scan(db, "7", pages, error));
        CHECK_STR_EQ(ost_delpage::joined(pages), "2");
    }
    TempDb db("delpage_scan_legend");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (1, 7)");
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (2, 7)");
    db.must("INSERT INTO BidLegends (UID, BidPageUID) VALUES (50, 2)");
    Pages pages;
    std::string error;
    CHECK(scan(db, "7", pages, error));
    CHECK_STR_EQ(ost_delpage::joined(pages), "");
    CHECK_STR_EQ(error, "");
}

// One database, one row in each of the 15 tables on pages 1..15 and a legend on page 16: exactly the pages 1..15 are reported (in a set, any order).
TEST(DelPageScan, AllFifteenKindsOfContentAndALegendInOneBid)
{
    TempDb db("delpage_scan_all_kinds");
    REQUIRE(db.schema_ok());
    int page = 1;
    Pages expected;
    for (const char *table : ost_delpage_oracle::page_delete_confirmation_tables())
    {
        db.must("INSERT INTO BidPages (UID, BidUID) VALUES (" + std::to_string(page) + ", 7)");
        db.must(std::string("INSERT INTO [") + table + "] (UID, BidPageUID) VALUES (" + std::to_string(100 + page) + ", " + std::to_string(page) + ")");
        expected.insert(std::to_string(page));
        ++page;
    }
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (16, 7)");
    db.must("INSERT INTO BidLegends (UID, BidPageUID) VALUES (200, 16)");
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (17, 7)"); // an empty page
    Pages pages;
    std::string error;
    CHECK(scan(db, "7", pages, error));
    CHECK_STR_EQ(ost_delpage::joined(pages), ost_delpage::joined(expected));
    CHECK_EQ(expected.size(), static_cast<std::size_t>(15));
}

// Port of :2019 test_tables_without_a_page_column_are_skipped_without_error: BidPages (1, 7); BidTakeoffs (UID only); BidTexts (2, page 1): the table without
// the page column is skipped, no error, {"1"}.
TEST(DelPageScan, ATableWithoutAPageColumnIsSkippedWithoutError)
{
    TempDb db("delpage_scan_no_page_column", /*with_schema=*/false);
    loose_pages(db, {{1, 7}});
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER)");
    db.must("INSERT INTO BidTakeoffs VALUES (1)");
    loose_table(db, "BidTexts");
    db.must("INSERT INTO BidTexts VALUES (2, 1)");
    Pages pages;
    std::string error;
    CHECK(scan(db, "7", pages, error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(ost_delpage::joined(pages), "1");
}

// Port of :2033 test_a_bid_without_pages_scans_no_content_tables: BidPages (1, bid 8) and BidTexts (2, page 1): the scan for bid 7 is an empty set. And of
// :2043 test_a_file_without_pages_has_no_delete_content: a file with only a Bids table (no BidPages at all) gives an empty set. Windows does not check that
// the bid exists: a bid that is not in the file is an empty set, a success.
TEST(DelPageScan, ABidWithoutPagesAndAFileWithoutPagesHaveNoDeleteContent)
{
    {
        TempDb db("delpage_scan_other_bid_only", /*with_schema=*/false);
        loose_pages(db, {{1, 8}});
        loose_table(db, "BidTexts");
        db.must("INSERT INTO BidTexts VALUES (2, 1)");
        Pages pages;
        std::string error;
        CHECK(scan(db, "7", pages, error));
        CHECK_STR_EQ(error, "");
        CHECK_STR_EQ(ost_delpage::joined(pages), "");
    }
    {
        TempDb db("delpage_scan_no_page_table", /*with_schema=*/false);
        db.must("CREATE TABLE Bids (UID INTEGER)");
        Pages pages;
        std::string error;
        CHECK(scan(db, "7", pages, error));
        CHECK_STR_EQ(error, "");
        CHECK_STR_EQ(ost_delpage::joined(pages), "");
    }
}

// Port of :2047 test_the_page_table_must_carry_uid_and_bid_columns: BidPages with only BidUID gives a failure and the text "Missing BidPages.UID"; with only
// UID, "Missing BidPages.BidUID". Never a partial set.
TEST(DelPageScan, ThePageTableMustCarryUidAndBidColumns)
{
    for (const auto &missing : {std::pair<const char *, const char *>{"UID", "BidUID"}, std::pair<const char *, const char *>{"BidUID", "UID"}})
    {
        SCOPED_CONTEXT(std::string("missing ") + missing.first);
        TempDb db("delpage_scan_page_columns", /*with_schema=*/false);
        db.must(std::string("CREATE TABLE BidPages (") + missing.second + " INTEGER)");
        db.must(std::string("INSERT INTO BidPages VALUES (1)"));
        Pages pages;
        std::string error;
        CHECK_FALSE(scan(db, "7", pages, error));
        CHECK(error.find(std::string("Missing BidPages.") + missing.first) != std::string::npos);
        CHECK_STR_EQ(ost_delpage::joined(pages), "");
    }
}

// Port of :756 test_delete_content_scan_reports_only_this_bids_pages_with_user_content: BidPages (1, 7), (2, 7), (3, 7), (4, 8); BidTakeoffs (10, page 1),
// BidLegends (11, page 2), BidComments (12, page 4), BidTexts (13, page NULL). Bid 7: {"1"}; after a BidTexts (14, page 3): {"1", "3"}; bid 8: {"4"};
// bid 99: empty.
TEST(DelPageScan, OnlyThisBidsPagesWithUserContentAreReported)
{
    TempDb db("delpage_scan_this_bid", /*with_schema=*/false);
    loose_pages(db, {{1, 7}, {2, 7}, {3, 7}, {4, 8}});
    for (const char *table : {"BidTakeoffs", "BidLegends", "BidTexts", "BidComments"})
        loose_table(db, table);
    db.must("INSERT INTO BidTakeoffs VALUES (10, 1)");
    db.must("INSERT INTO BidLegends VALUES (11, 2)");
    db.must("INSERT INTO BidComments VALUES (12, 4)");
    db.must("INSERT INTO BidTexts VALUES (13, NULL)");
    Pages pages;
    std::string error;
    CHECK(scan(db, "7", pages, error));
    CHECK_STR_EQ(ost_delpage::joined(pages), "1");
    db.must("INSERT INTO BidTexts VALUES (14, 3)");
    CHECK(scan(db, "7", pages, error));
    CHECK_STR_EQ(ost_delpage::joined(pages), "1,3");
    CHECK(scan(db, "8", pages, error));
    CHECK_STR_EQ(ost_delpage::joined(pages), "4");
    CHECK(scan(db, "99", pages, error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(ost_delpage::joined(pages), "");
}

// Port of :775 test_delete_content_scan_rejects_duplicate_page_uids: BidPages (1, 7) twice: a failure, and the text holds "BidPages contains duplicate UID 1".
TEST(DelPageScan, DuplicatePageUidsAreAFailedRead)
{
    TempDb db("delpage_scan_duplicate_pages", /*with_schema=*/false);
    loose_pages(db, {{1, 7}, {1, 7}});
    Pages pages;
    std::string error;
    CHECK_FALSE(scan(db, "7", pages, error));
    CHECK(error.find("BidPages contains duplicate UID 1") != std::string::npos);
    CHECK_STR_EQ(ost_delpage::joined(pages), "");
}

// Ports of :744 test_delete_content_scan_discards_partial_results_after_failure and :4366: a query that fails after another table already marked a page gives a
// failure and an EMPTY set, never the partial set. BidTakeoffs (read first) marks page 1; BidTexts (read later) is unreadable. For a bid WITHOUT pages the
// unreadable BidTexts is never read (DeleteContentScanScopeTests :4640): the scan is a success with an empty set.
TEST(DelPageScan, AFailedReadDiscardsThePartialSetAndABidWithoutPagesNeverReadsTheContentTables)
{
    TempDb db("delpage_scan_partial");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (1, 7)");
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (2, 8)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID) VALUES (10, 7, 1)");
    db.must("INSERT INTO BidTexts (UID, BidUID, BidPageUID) VALUES (11, 7, 1)");
    Pages pages;
    std::string error;
    REQUIRE(scan(db, "7", pages, error));
    REQUIRE_EQ(ost_delpage::joined(pages), std::string("1"));
    REQUIRE(ost_delpage::corrupt_table(db, "BidTexts"));

    CHECK_FALSE(scan(db, "7", pages, error));
    CHECK(!error.empty() && error != "<unset>");
    CHECK_STR_EQ(ost_delpage::joined(pages), ""); // never {"1"}
    CHECK_FALSE(scan(db, "8", pages, error));
    CHECK_STR_EQ(ost_delpage::joined(pages), "");

    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (9, 1, 'no pages')");
    CHECK(scan(db, "9", pages, error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(ost_delpage::joined(pages), "");
}

// The set the caller passes in is REPLACED: stale entries are dropped on a success (also an empty one) and on a failure; the scan never merges.
TEST(DelPageScan, TheResultReplacesWhatTheCallerHad)
{
    TempDb db("delpage_scan_replaces");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (1, 7)");
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (2, 7)");
    db.must("INSERT INTO BidHotLinks (UID, BidUID, BidPageUID) VALUES (5, 7, 2)");
    Pages pages = {"stale", "1", "99"};
    CHECK(ost_store::pages_with_delete_content(db.path(), "7", pages));
    CHECK_STR_EQ(ost_delpage::joined(pages), "2");
    pages = {"stale"};
    CHECK(ost_store::pages_with_delete_content(db.path(), "99", pages));
    CHECK_STR_EQ(ost_delpage::joined(pages), "");
}

// The scan reads and writes nothing: every table is identical afterwards, and an unknown bid or an empty bid is a success, not an error.
TEST(DelPageScan, TheScanWritesNothing)
{
    TempDb db("delpage_scan_readonly");
    ost_delpage::seed_scenario(db, ost_delpage::scenario("A_DeleteOnePage"));
    const std::string before = db.snapshot_all();
    Pages pages;
    std::string error;
    CHECK(scan(db, "1", pages, error));
    CHECK_STR_EQ(error, "");
    // pages 10 (takeoffs, comments ...), 11 and 12 have content in the oracle world; a page with only a legend or a total would not count
    CHECK_STR_EQ(ost_delpage::joined(pages), "10,11,12");
    CHECK(scan(db, "2", pages, error));
    CHECK_STR_EQ(ost_delpage::joined(pages), "20");
    CHECK_EQ(db.snapshot_all(), before);
}

// A database that is not an SQLite file is a failed read: false, an empty set, a text, and the file is not changed by the scan.
TEST(DelPageScan, ADatabaseThatCannotBeReadIsAFailedRead)
{
    TempDb db("delpage_scan_garbage", /*with_schema=*/false);
    const std::string garbage(4096, 'x');
    {
        std::ofstream file(db.path(), std::ios::binary);
        file << garbage;
    }
    Pages pages;
    std::string error;
    CHECK_FALSE(scan(db, "7", pages, error));
    CHECK(!error.empty() && error != "<unset>");
    CHECK_STR_EQ(ost_delpage::joined(pages), "");
    std::ifstream in(db.path(), std::ios::binary);
    const std::string after((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK_EQ(after, garbage);

    CHECK_FALSE(scan(db, "7", pages, error));
    Pages other = {"stale"};
    CHECK_FALSE(ost_store::pages_with_delete_content("/nonexistent_delpage_dir/db.sqlite", "7", other, &error));
    CHECK_STR_EQ(ost_delpage::joined(other), "");
    CHECK(!error.empty());
}
