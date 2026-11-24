// M1 Delete Page (plan 5.6, tests-first plan "the schema-driven catalog test"): the table list of the cascade must cover EVERY table that references a
// page, so that a table added to the schema later fails the build until it is added to the list (an uncovered table would leave orphan rows that
// the loader rejects or that a reused UID adopts).
// Windows: tests/integration/mdb/test_relationship_catalog.py:66-80 test_page_delete_catalog_covers_every_direct_page_child (PAGE_DELETE_CHILD_TABLES
// plus BidTakeoffs, BidNamedViews, BidHotLinks equal the tables with a BidPageUID column), constants.py:83-121 (the two lists, in order), and
// raw_bid_integrity.py:141-268 BID_RELATIONSHIPS (mirrored by ost_schema::BID_RELATIONSHIPS).
// The Windows lists are the oracle (DeletePageOracleData.hpp: the real constants and the real reference schema model).
#include "DeletePageTestSupport.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

using ost_delpage::run;
using ost_delpage::Uids;
using ost_ptest::TempDb;

namespace
{
    std::vector<std::string> core_child_tables()
    {
        std::vector<std::string> out;
        for (std::size_t i = 0; i < ost_domain::PAGE_DELETE_CHILD_TABLE_COUNT; ++i)
            out.push_back(ost_domain::PAGE_DELETE_CHILD_TABLES[i]);
        return out;
    }

    std::vector<std::string> core_confirmation_tables()
    {
        std::vector<std::string> out;
        for (std::size_t i = 0; i < ost_domain::PAGE_DELETE_CONFIRMATION_TABLE_COUNT; ++i)
            out.push_back(ost_domain::PAGE_DELETE_CONFIRMATION_TABLES[i]);
        return out;
    }

    std::vector<std::string> as_strings(const std::vector<const char *> &list)
    {
        return std::vector<std::string>(list.begin(), list.end());
    }

    // The three tables of the cascade that are not in PAGE_DELETE_CHILD_TABLES because they have steps of their own (9-12).
    const std::vector<std::string> &specially_ordered()
    {
        static const std::vector<std::string> list = {"BidTakeoffs", "BidNamedViews", "BidHotLinks"};
        return list;
    }
} // namespace

// constants.py:83-110 and :111-121: the Core lists are the Windows lists, entry by entry, in the Windows order (the order of the 23 deletes is the
// order of steps 8): 23 child tables and 15 confirmation tables; BidLegends deletes with the page but never asks for a confirmation.
TEST(DelPageCatalog, TheCoreListsAreTheWindowsConstantsInOrder)
{
    CHECK_EQ(ost_domain::PAGE_DELETE_CHILD_TABLE_COUNT, static_cast<std::size_t>(23));
    CHECK_EQ(ost_domain::PAGE_DELETE_CONFIRMATION_TABLE_COUNT, static_cast<std::size_t>(15));
    CHECK_EQ(core_child_tables(), as_strings(ost_delpage_oracle::page_delete_child_tables()));
    CHECK_EQ(core_confirmation_tables(), as_strings(ost_delpage_oracle::page_delete_confirmation_tables()));
    const std::vector<std::string> child = core_child_tables();
    const std::vector<std::string> confirm = core_confirmation_tables();
    CHECK(std::find(child.begin(), child.end(), "BidLegends") != child.end());
    CHECK(std::find(confirm.begin(), confirm.end(), "BidLegends") == confirm.end());
    CHECK_EQ(ost_domain::kBidOwnedUidChunk, static_cast<std::size_t>(50));
    // no table twice, and the three tables with steps of their own are not in the 23
    CHECK_EQ(std::set<std::string>(child.begin(), child.end()).size(), child.size());
    for (const std::string &table : specially_ordered())
        CHECK(std::find(child.begin(), child.end(), table) == child.end());
}

// test_relationship_catalog.py:66 (the iOS twin): the tables of the iOS database that have a BidPageUID column, read from the database itself (PRAGMA
// table_info over create_schema), are EXACTLY the 23 child tables plus BidTakeoffs, BidNamedViews and BidHotLinks. A table added to the schema with a
// BidPageUID column makes this test fail until the Core list (and the cascade) knows it.
TEST(DelPageCatalog, EveryTableWithAPageColumnIsInTheCascadeList)
{
    TempDb db("delpage_cat_schema");
    REQUIRE(db.schema_ok());
    std::set<std::string> expected;
    for (const std::string &table : core_child_tables())
        expected.insert(table);
    for (const std::string &table : specially_ordered())
        expected.insert(table);
    const std::set<std::string> actual = ost_delpage::tables_with_page_column(db);
    CHECK_EQ(expected.size(), static_cast<std::size_t>(26));
    CHECK_STR_EQ(ost_delpage::joined(actual), ost_delpage::joined(expected));
}

// The iOS database against the WINDOWS schema: the tables with a BidPageUID column are the 26 of the Windows reference schema model (the oracle). A guard of
// the iOS DDL, not of the delete: it holds before the implementation exists.
TEST(DelPageCatalog, TheIosSchemaHasTheSamePageChildrenAsTheWindowsReferenceSchema)
{
    TempDb db("delpage_cat_windows_schema");
    REQUIRE(db.schema_ok());
    const std::set<std::string> actual = ost_delpage::tables_with_page_column(db);
    const std::vector<std::string> windows = as_strings(ost_delpage_oracle::reference_schema_page_children());
    CHECK_EQ(windows.size(), static_cast<std::size_t>(26));
    CHECK_STR_EQ(ost_delpage::joined(actual), ost_delpage::joined(std::set<std::string>(windows.begin(), windows.end())));
}

// Every COLUMN that references a page (ost_schema::BID_RELATIONSHIPS with BidPages as the parent) is handled by the cascade: the BidPageUID columns of the 26
// tables are deleted by page (steps 8-12), BidPages.MasterPageUID is cleared (step 1) and BidSettings.BidPageSelectedUID is cleared (step 13). A new column that
// references BidPages (a new kind of page reference) fails this test until it is classified. (The cover sheet selection, Bids.CoverSheetSelItemUID with type 1,
// is not a declared relationship; step 14 handles it by hand.)
TEST(DelPageCatalog, EveryColumnThatReferencesAPageIsHandledByAStepOfTheCascade)
{
    std::set<std::string> expected; // "Table.Column"
    for (const std::string &table : core_child_tables())
        expected.insert(table + ".BidPageUID");
    for (const std::string &table : specially_ordered())
        expected.insert(table + ".BidPageUID");
    expected.insert("BidPages.MasterPageUID");
    expected.insert("BidSettings.BidPageSelectedUID");

    std::set<std::string> actual;
    for (const ost_schema::BidRelationship &relationship : ost_schema::BID_RELATIONSHIPS)
        if (std::string(relationship.parent_table) == "BidPages")
            actual.insert(std::string(relationship.child_table) + "." + relationship.child_column);
    CHECK_STR_EQ(ost_delpage::joined(actual), ost_delpage::joined(expected));
    CHECK_EQ(expected.size(), static_cast<std::size_t>(28));
}

// The BEHAVIOUR behind the catalog, driven by the database: every table that has a BidPageUID column holds one row for the deleted page, one for a surviving
// page of the same bid and one for a page of another bid (the UID, and the BidUID where the table has the column, are filled); after the delete NO row of the
// deleted page is left in ANY of the 26 tables, and the rows of the other two pages are byte-identical.
TEST(DelPageCatalog, EveryTableWithAPageColumnLosesExactlyTheRowsOfTheDeletedPage)
{
    TempDb db("delpage_cat_rows");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db); // bid 10: pages 100, 101; bid 20: page 200 (takeoff 900 on it)
    db.must("DELETE FROM BidTakeoffs"); // the fixture's own takeoff would collide with the generic rows below
    const std::set<std::string> tables = ost_delpage::tables_with_page_column(db);
    REQUIRE(!tables.empty());
    int uid = 5000;
    for (const std::string &table : tables)
    {
        const bool has_bid = db.count("SELECT COUNT(*) FROM pragma_table_info('" + table + "') WHERE name = 'BidUID'") > 0;
        const std::pair<int, int> rows[] = {{100, 10}, {101, 10}, {200, 20}};
        for (const auto &row : rows)
        {
            db.must("INSERT INTO [" + table + "] (UID, " + (has_bid ? "BidUID, " : "") + "BidPageUID) VALUES (" + std::to_string(uid++) + ", " +
                    (has_bid ? std::to_string(row.second) + ", " : "") + std::to_string(row.first) + ")");
        }
    }
    std::string kept_before;
    for (const std::string &table : tables)
        kept_before += table + ":" + ost_delpage::row_dump(db, table, "BidPageUID IN (101, 200)") + "\n";

    CHECK(run(db, "10", {"100"}));
    for (const std::string &table : tables)
    {
        SCOPED_CONTEXT("table " + table);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM [" + table + "] WHERE BidPageUID = 100"), 0);
    }
    std::string kept_after;
    for (const std::string &table : tables)
        kept_after += table + ":" + ost_delpage::row_dump(db, table, "BidPageUID IN (101, 200)") + "\n";
    CHECK_EQ(kept_after, kept_before);
}

// The bid cascade (permanently_delete_project, DBP-12) shares the page list with delete_pages (plan 5.6: BidCascade reads PAGE_DELETE_CHILD_TABLES): for every
// table of the Windows list a row of the bid's page and a row of another bid's page exist; the cascade of the bid removes the first and keeps the second. This
// holds today (the list is hard-coded in ProjectStore.cpp) and must keep holding when the list is shared ("the behaviour of the bid cascade must not change").
// The first check pins that the Core list has all 23 names, so the table list that drives this test is the one the cascade will read.
TEST(DelPageCatalog, TheBidCascadeEmptiesTheSameTablesForTheBidsPages)
{
    CHECK_EQ(ost_domain::PAGE_DELETE_CHILD_TABLE_COUNT, static_cast<std::size_t>(23));
    TempDb db("delpage_cat_bid_cascade");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db);
    db.must("DELETE FROM BidTakeoffs");
    // permanently_delete_project only takes a bid that is in the system folder 'Deleted Bids' (BidProjects UID 1 of create_schema)
    db.must("UPDATE Bids SET BidProjectUID = 1 WHERE UID = 10");
    int uid = 6000;
    for (const char *table : ost_delpage_oracle::page_delete_child_tables())
    {
        const std::string t = table;
        const bool has_bid = db.count("SELECT COUNT(*) FROM pragma_table_info('" + t + "') WHERE name = 'BidUID'") > 0;
        db.must("INSERT INTO [" + t + "] (UID, " + (has_bid ? "BidUID, " : "") + "BidPageUID) VALUES (" + std::to_string(uid++) + ", " + (has_bid ? "10, " : "") + "100)");
        db.must("INSERT INTO [" + t + "] (UID, " + (has_bid ? "BidUID, " : "") + "BidPageUID) VALUES (" + std::to_string(uid++) + ", " + (has_bid ? "20, " : "") + "200)");
    }

    CHECK(ost_store::permanently_delete_project(db.path(), "10"));
    for (const char *table : ost_delpage_oracle::page_delete_child_tables())
    {
        SCOPED_CONTEXT(std::string("table ") + table);
        CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM [") + table + "] WHERE BidPageUID = 100"), 0);
        CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM [") + table + "] WHERE BidPageUID = 200"), 1);
    }
}
