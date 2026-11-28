// The M1 test helpers of Persistence (plan 4.12, S0.d): snapshot_all (PersistenceTestSupport.hpp) and seed_pages (StoreTestSupport.hpp).
// seed_locked_bid / seed_open_bid / seed_lock_oracle_case are exercised by test_bid_lock.cpp. These tests pin what the other M1 streams
// rely on: a whole-database snapshot sees every table (so "nothing else changed" and "a refused write leaves the database untouched"
// mean what they say), and the page fixture is a bid the store loads, with the pages in order, the scales and the takeoffs asked for.
// No Windows oracle: these are test-infrastructure checks.
#include "StoreTestSupport.hpp"

#include "Data/ProjectData.hpp"

#include <string>
#include <vector>

using ost_ptest::SeedPage;
using ost_ptest::TempDb;

// snapshot_all is a port of Conditions/ConditionsTestSupport.hpp snapshot_all: EVERY user table, in name order, exact text.
TEST(StoreSeedHelpers, SnapshotAllSeesAChangeInAnyTable)
{
    TempDb db("seed_snapshot_all");
    REQUIRE(db.schema_ok());
    const std::string empty = db.snapshot_all();
    CHECK(empty.find("[Bids]") != std::string::npos);
    CHECK(empty.find("[BidPages]") != std::string::npos);
    CHECK(empty.find("[JobStatuses]") != std::string::npos);
    CHECK(empty.find("[BidAnnotationClouds]") != std::string::npos); // a table the world fixtures never fill
    CHECK(empty.find("sqlite_") == std::string::npos);
    CHECK_EQ(db.snapshot_all(), empty); // deterministic

    // a row in a table that no fixture names (world_tables() would miss it)
    db.must("INSERT INTO BidAnnotationClouds (UID) VALUES (5)");
    const std::string one_row = db.snapshot_all();
    CHECK_NE(one_row, empty);
    db.must("UPDATE BidAnnotationClouds SET UID = 6 WHERE UID = 5");
    CHECK_NE(db.snapshot_all(), one_row);
    db.must("DELETE FROM BidAnnotationClouds");
    CHECK_EQ(db.snapshot_all(), empty);

    // NULL and the empty text are two different states, and a number is a change too
    ost_ptest::seed_two_bids(db);
    const std::string seeded = db.snapshot_all();
    db.must("UPDATE BidPages SET Name = NULL WHERE UID = 100");
    const std::string null_name = db.snapshot_all();
    CHECK_NE(null_name, seeded);
    db.must("UPDATE BidPages SET Name = '' WHERE UID = 100");
    const std::string empty_name = db.snapshot_all();
    CHECK_NE(empty_name, null_name);
    db.must("UPDATE BidPages SET Name = 'P100' WHERE UID = 100");
    CHECK_EQ(db.snapshot_all(), seeded);
    db.must("UPDATE BidPages SET ScaleFactor1 = 0.5 WHERE UID = 100");
    CHECK_NE(db.snapshot_all(), seeded);
}

// A trigger-blocked write leaves no trace in the snapshot: the pattern every refusal test of the M1 writers uses.
TEST(StoreSeedHelpers, SnapshotAllProvesAFailedWriteLeftNothingBehind)
{
    TempDb db("seed_snapshot_blocked");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db);
    const std::string trigger = db.block("BidPages", "UPDATE");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(db.exec("UPDATE BidPages SET Name = 'x' WHERE UID = 100"));
    CHECK_EQ(db.snapshot_all(), before);
    db.unblock(trigger);
    CHECK(db.exec("UPDATE BidPages SET Name = 'x' WHERE UID = 100"));
    CHECK_NE(db.snapshot_all(), before);
}

// seed_pages(db, bid, count, takeoffs_per_page): the pages in bar order with the default scale, the takeoffs on their page, loadable.
TEST(StoreSeedHelpers, SeedPagesMakesABidThatLoads)
{
    TempDb db("seed_pages_plain");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_pages(db, 10, 3, 2);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE BidUID = 10"), 3);
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT Name FROM BidPages WHERE BidUID = 10 ORDER BY Sequence"), "Page 1,Page 2,Page 3");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidPages WHERE BidUID = 10 ORDER BY Sequence"), "1000,1001,1002");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT Sequence || '/' || Index1 FROM BidPages WHERE BidUID = 10 ORDER BY UID"), "1.0/1,2.0/2,3.0/3");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT ScaleFactor1 || '/' || ScaleFactor2 FROM BidPages WHERE BidUID = 10 ORDER BY UID"),
                 "0.25/12.0,0.25/12.0,0.25/12.0");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE BidUID = 10"), 6);
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT BidPageUID || ':' || COUNT(*) FROM BidTakeoffs GROUP BY BidPageUID ORDER BY BidPageUID"),
                 "1000:2,1001:2,1002:2");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE BidUID = 10"), 1);

    ost_data::ProjectData project;
    std::string error;
    REQUIRE(ost_store::load_project(db.path(), "10", project, &error));
    CHECK_STR_EQ(error, "");
    REQUIRE_EQ(project.page_order.size(), static_cast<std::size_t>(3));
    CHECK_STR_EQ(project.page_order[0], "1000");
    CHECK_STR_EQ(project.page_order[2], "1002");
    CHECK_STR_EQ(project.pages.at("1001").name, "Page 2");
    CHECK_EQ(project.takeoffs.size(), static_cast<std::size_t>(6));
}

// seed_pages with a list: any stored name (quotes, spaces, a name that looks like a number), any page scale, a page without takeoffs.
TEST(StoreSeedHelpers, SeedPagesTakesNamesAndScalesAsGiven)
{
    TempDb db("seed_pages_custom");
    REQUIRE(db.schema_ok());
    std::vector<SeedPage> pages(3);
    pages[0].name = "O'Brien";
    pages[0].takeoffs = 1;
    pages[1].name = "  padded  ";
    pages[1].scale_factor1 = 1.0;
    pages[1].scale_factor2 = 8.0;
    pages[2].name = "007";
    pages[2].scale_factor1 = 0.1;
    pages[2].scale_factor2 = 0.3;
    ost_ptest::seed_pages(db, 11, pages, 2000, 8000, 6000);
    CHECK_STR_EQ(db.text("SELECT Name FROM BidPages WHERE UID = 2000"), "O'Brien");
    CHECK_STR_EQ(db.text("SELECT Name FROM BidPages WHERE UID = 2001"), "  padded  ");
    CHECK_STR_EQ(db.text("SELECT typeof(Name) || ':' || Name FROM BidPages WHERE UID = 2002"), "text:007");
    CHECK_STR_EQ(db.text("SELECT ScaleFactor1 || '/' || ScaleFactor2 FROM BidPages WHERE UID = 2001"), "1.0/8.0");
    CHECK_STR_EQ(db.text("SELECT ScaleFactor1 || '/' || ScaleFactor2 FROM BidPages WHERE UID = 2002"), "0.1/0.3"); // exact round trip
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 1);
    CHECK_EQ(db.count("SELECT BidPageUID FROM BidTakeoffs WHERE UID = 8000"), 2000);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 6000 AND BidUID = 11"), 1);
    // a list without takeoffs makes no condition
    TempDb bare("seed_pages_bare");
    REQUIRE(bare.schema_ok());
    ost_ptest::seed_pages(bare, 12, 2);
    CHECK_EQ(bare.count("SELECT COUNT(*) FROM BidConditions"), 0);
    CHECK_EQ(bare.count("SELECT COUNT(*) FROM BidTakeoffs"), 0);
    CHECK_EQ(bare.count("SELECT COUNT(*) FROM BidPages"), 2);
}

// seed_pages combines with seed_two_bids (its default UIDs avoid that world's pages 100, 101, 200, takeoff 900, conditions 30, 31, 40)
// and with a second bid of its own.
TEST(StoreSeedHelpers, SeedPagesCombinesWithTheOtherFixtures)
{
    TempDb db("seed_pages_combined");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db);
    ost_ptest::seed_pages(db, 10, 2, 1);
    ost_ptest::seed_pages(db, 20, 1, 1, 3000, 9100, 7100);
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidPages WHERE BidUID = 10 ORDER BY UID"), "100,101,1000,1001");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidPages WHERE BidUID = 20 ORDER BY UID"), "200,3000");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 2); // the bids were not duplicated
    ost_data::ProjectData project;
    std::string error;
    CHECK(ost_store::load_project(db.path(), "10", project, &error));
    CHECK_STR_EQ(error, "");
    CHECK(ost_store::load_project(db.path(), "20", project, &error));
    CHECK_STR_EQ(error, "");
}
