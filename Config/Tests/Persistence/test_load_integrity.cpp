// Opening a bid fails with an error that names the cause (user decision, no silent drops):
//   DBP-31 (+BRW-20, LP-M04)  malformed / duplicate UIDs, a takeoff that references a missing condition, page or parent, a
//                             hole on another page than its parent, a ParentUID cycle, a takeoff that breaks the contract
//   POS-M01                   a non-finite position (nan / inf) or rotation fails the open and names the takeoff UID
//   BRW-16                    a ParentUID cycle of the page folders fails the open and names the folder UID
//   QTY-19 (+POS-M02)         takeoffs with an empty / unparsable / short position are KEPT (counts match Windows)
// Windows oracle: bid_data_reader.py (get_bid_data, _parse_bid_takeoffs_for_bid), bid_owned_identity.py (require_*),
// domain/entities/takeoff.py (has_valid_contract, find_takeoff_parent_*), hierarchy_reader.py (page folders) and their
// tests in tests/infrastructure/mdb/components/test_bid_data_reader.py / test_hierarchy_reader.py. The wording of every
// message is the Windows one (LoadOracleData.hpp holds the outcomes of the real Windows reader). Where Windows has no
// wording of its own (a NULL reference: Python int('None') raises a bare ValueError) the message keeps the Windows
// "references missing" form and names the stored value.
#include "StoreTestSupport.hpp"

#include "LoadOracleData.hpp"

#include "Domain/UomService.hpp"
#include "Meshing/TakeoffMeshBuilder.hpp"

#include <set>
#include <string>
#include <vector>

using ost_ptest::TempDb;

namespace
{
    struct Outcome
    {
        bool ok = false;
        ost_data::ProjectData project;
        std::string error;
    };

    Outcome load_checked(const TempDb &db, const std::string &bid = "1")
    {
        Outcome out;
        out.ok = ost_store::load_project(db.path(), bid, out.project, &out.error);
        return out;
    }

    // The failure contract: false, the cause as text, nothing loaded; the legacy call hands out an empty project too.
    void check_failed(const TempDb &db, const std::string &message)
    {
        const Outcome out = load_checked(db);
        CHECK_FALSE(out.ok);
        CHECK_STR_EQ(out.error, message);
        CHECK(out.project.empty());
        CHECK(out.project.takeoffs.empty());
        CHECK(out.project.pages.empty());
        CHECK(out.project.conditions.empty());
        CHECK(ost_store::load_project(db.path(), "1").empty());
    }

    // The oracle world (see LoadOracleData.hpp): bids 1 (opened) and 2, pages 10, 11 | 20, conditions 30, 31 | 40 and the
    // takeoff 900 of bid 2.
    void seed_world(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, JobName, BidNo, MeasureBase) VALUES (1, 'J1', 1, 0)");
        db.must("INSERT INTO Bids (UID, JobName, BidNo, MeasureBase) VALUES (2, 'J2', 2, 0)");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (10, 1, 'P10')");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (11, 1, 'P11')");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (20, 2, 'F20')");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (30, 1, 1, 'C30', 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (31, 1, 2, 'C31', 2)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (40, 2, 1, 'C40', 0)");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) "
                "VALUES (900, 2, 40, 20, '1;2;3;4')");
    }

    // (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position, Rotation) value tuples of bid 1, as in the oracle.
    void add_takeoffs(const TempDb &db, const std::string &tuples)
    {
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position, Rotation) VALUES " +
                tuples);
    }

    // One takeoff of bid 1.
    void add_takeoff(const TempDb &db, int uid, const std::string &condition_sql, const std::string &page_sql,
                     const std::string &parent_sql = "0", const std::string &position_sql = "'1;2;3;4'",
                     const std::string &rotation_sql = "0")
    {
        add_takeoffs(db, "(" + std::to_string(uid) + ", 1, " + condition_sql + ", " + page_sql + ", " + parent_sql + ", " +
                             position_sql + ", " + rotation_sql + ")");
    }

    // The table without its PRIMARY KEY, so that a UID can be duplicated, zero or NULL (the Windows tests use tables like this).
    void drop_primary_key(const TempDb &db, const std::string &table)
    {
        db.must("ALTER TABLE " + table + " RENAME TO " + table + "_pk");
        db.must("CREATE TABLE " + table + " AS SELECT * FROM " + table + "_pk");
        db.must("DROP TABLE " + table + "_pk");
    }

    void run_statements(const TempDb &db, const std::string &statements)
    {
        size_t start = 0;
        while (start < statements.size())
        {
            size_t end = statements.find('\n', start);
            if (end == std::string::npos)
                end = statements.size();
            if (end > start)
                db.must(statements.substr(start, end - start));
            start = end + 1;
        }
    }

    // "uid=<coordinate count>,..." in load order (the oracle's success format).
    std::string takeoff_summary(const ost_data::ProjectData &project)
    {
        std::string out;
        for (const ost_domain::Takeoff &t : project.takeoffs)
            out += (out.empty() ? "" : ",") + t.uid + "=" + std::to_string(t.position.size());
        return out;
    }

    // The first quantity of a condition, by value (compute_page_quantities returns its map by value).
    double first_quantity(const ost_data::ProjectData &project, const std::string &condition_uid)
    {
        const auto quantities = ost_domain::compute_page_quantities(project.conditions, project.takeoffs);
        auto found = quantities.find(condition_uid);
        if (found == quantities.end())
            FAIL_TEST("no quantity for condition " + condition_uid);
        return found->second.q1;
    }

    // `message` after the exception class of an oracle outcome "Class|message".
    std::string oracle_message(const std::string &expected)
    {
        const size_t bar = expected.find('|');
        return bar == std::string::npos ? expected : expected.substr(bar + 1);
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// DBP-31: differential run against the real Windows reader
// ---------------------------------------------------------------------------------------------------------------

// Every case of LoadOracleData.hpp (hand written ports of the Windows tests, the order of the checks and 75 random
// combinations): same rows, the iOS outcome is the Windows outcome - the failure text exactly, or the loaded takeoffs.
TEST(Dbp31_LoadIntegrity, EveryWindowsOracleCaseGivesTheWindowsOutcome)
{
    int index = 0;
    for (const ost_ptest::LoadOracleCase &c : ost_ptest::kLoadOracleCases)
    {
        SCOPED_CONTEXT("oracle case " + std::to_string(index++) + " setup=[" + c.setup + "] rows=" + c.rows);
        TempDb db("load_integrity_oracle");
        seed_world(db);
        run_statements(db, c.setup);
        add_takeoffs(db, c.rows);

        const std::string expected = c.expected;
        const Outcome out = load_checked(db);
        if (expected.rfind("OK:", 0) == 0)
        {
            CHECK(out.ok);
            CHECK_STR_EQ(out.error, "");
            CHECK_STR_EQ(takeoff_summary(out.project), expected.substr(3));
        }
        else
        {
            CHECK_FALSE(out.ok);
            CHECK_STR_EQ(out.error, oracle_message(expected));
            CHECK(out.project.empty());
            CHECK(out.project.takeoffs.empty());
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// DBP-31 hand written ports (same inputs and messages as the Windows tests)
// ---------------------------------------------------------------------------------------------------------------

TEST(Dbp31_LoadIntegrity, ControlWorldLoadsEveryTakeoffInUidOrder)
{
    TempDb db("load_integrity_control");
    seed_world(db);
    add_takeoff(db, 2, "31", "11", "NULL", "'5;6'");
    add_takeoff(db, 1, "30", "10");
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    CHECK_STR_EQ(out.error, "");
    CHECK_STR_EQ(takeoff_summary(out.project), "1=4,2=2");
    CHECK(ost_store::load_project(db.path(), "1").takeoffs.size() == 2);
}

// Port of test_bid_data_reader.py::test_bid_load_rejects_takeoff_with_missing_required_owner (takeoff 7 -> condition 99).
TEST(Dbp31_LoadIntegrity, ATakeoffOfAMissingConditionFailsTheOpen)
{
    TempDb db("load_integrity_cond");
    seed_world(db);
    add_takeoff(db, 7, "99", "10", "0", "'0;0;1;1'");
    check_failed(db, "BidTakeoffs.UID=7 references missing BidConditions.UID=99 through BidConditionUID.");
}

// Port of ...::test_bid_load_rejects_takeoff_with_missing_page_owner.
TEST(Dbp31_LoadIntegrity, ATakeoffOfAMissingPageFailsTheOpen)
{
    TempDb db("load_integrity_page");
    seed_world(db);
    add_takeoff(db, 7, "30", "99", "0", "'0;0;1;1'");
    check_failed(db, "BidTakeoffs.UID=7 references missing BidPages.UID=99 through BidPageUID.");
}

// Port of ...::test_bid_load_rejects_takeoff_with_missing_parent.
TEST(Dbp31_LoadIntegrity, AHoleWithAMissingParentFailsTheOpen)
{
    TempDb db("load_integrity_parent");
    seed_world(db);
    add_takeoff(db, 7, "30", "10", "99", "'0;0;1;1'");
    check_failed(db, "BidTakeoffs.UID=7 references missing BidTakeoffs.UID=99 through ParentUID.");
}

// Port of ...::test_takeoffs_may_not_reference_conditions_or_pages_of_another_bid (and a parent of another bid).
TEST(Dbp31_LoadIntegrity, ReferencesToRowsOfAnotherBidAreMissingReferences)
{
    {
        TempDb db("load_integrity_foreign_cond");
        seed_world(db);
        add_takeoff(db, 60, "40", "10");
        check_failed(db, "BidTakeoffs.UID=60 references missing BidConditions.UID=40 through BidConditionUID.");
    }
    {
        TempDb db("load_integrity_foreign_page");
        seed_world(db);
        add_takeoff(db, 60, "30", "20");
        check_failed(db, "BidTakeoffs.UID=60 references missing BidPages.UID=20 through BidPageUID.");
    }
    {
        TempDb db("load_integrity_foreign_parent");
        seed_world(db);
        add_takeoff(db, 7, "30", "10", "900");
        check_failed(db, "BidTakeoffs.UID=7 references missing BidTakeoffs.UID=900 through ParentUID.");
    }
}

// Port of ...::test_bid_load_rejects_self_parented_takeoff, test_bid_load_rejects_multi_takeoff_parent_cycle and
// test_a_takeoff_parent_cycle_is_reported_by_its_numerically_smallest_uid (9 and 10 are a cycle: 9 < 10 numerically).
TEST(Dbp31_LoadIntegrity, AParentCycleFailsTheOpenAndNamesItsSmallestUid)
{
    {
        TempDb db("load_integrity_self_parent");
        seed_world(db);
        add_takeoff(db, 7, "30", "10", "7", "'0;0;1;1'");
        check_failed(db, "BidTakeoffs.UID=7 participates in a ParentUID cycle.");
    }
    {
        TempDb db("load_integrity_three_cycle");
        seed_world(db);
        add_takeoff(db, 7, "30", "10", "8", "'0;0;1;1'");
        add_takeoff(db, 8, "30", "10", "9", "'0;0;1;1'");
        add_takeoff(db, 9, "30", "10", "7", "'0;0;1;1'");
        check_failed(db, "BidTakeoffs.UID=7 participates in a ParentUID cycle.");
    }
    {
        TempDb db("load_integrity_numeric_smallest");
        seed_world(db);
        add_takeoff(db, 9, "30", "10", "10", "'1;2'");
        add_takeoff(db, 10, "30", "10", "9", "'1;2'");
        add_takeoff(db, 100, "30", "10", "9", "'1;2'");
        check_failed(db, "BidTakeoffs.UID=9 participates in a ParentUID cycle.");
    }
}

// Port of ...::test_reader_rejects_parent_on_another_page.
TEST(Dbp31_LoadIntegrity, AHoleOnAnotherPageThanItsParentFailsTheOpen)
{
    TempDb db("load_integrity_page_mismatch");
    seed_world(db);
    add_takeoff(db, 7, "30", "10", "0", "'0;0;1;1'");
    add_takeoff(db, 8, "30", "11", "7", "'0;0;1;1'");
    check_failed(db, "A takeoff and its parent must belong to the same page.");
}

// Port of ...::test_bid_load_accepts_valid_multi_level_takeoff_parent_chain.
TEST(Dbp31_LoadIntegrity, AValidMultiLevelParentChainLoads)
{
    TempDb db("load_integrity_chain");
    seed_world(db);
    add_takeoff(db, 7, "30", "10", "0", "'0;0;1;1'");
    add_takeoff(db, 8, "30", "10", "7", "'0;0;1;1'");
    add_takeoff(db, 9, "30", "10", "8", "'0;0;1;1'");
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    CHECK_STR_EQ(takeoff_summary(out.project), "7=4,8=4,9=4");
    CHECK_STR_EQ(out.project.takeoffs[2].parent_uid, "8");
}

// The takeoffs of ANOTHER bid are never judged: a broken takeoff of bid 2 does not stop bid 1 from opening.
TEST(Dbp31_LoadIntegrity, TheProblemsOfAnotherBidDoNotMatter)
{
    TempDb db("load_integrity_other_bid");
    seed_world(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position) "
            "VALUES (901, 2, 999, 999, 998, 'nan;1')");
    add_takeoff(db, 1, "30", "10");
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    CHECK_STR_EQ(takeoff_summary(out.project), "1=4");
    CHECK_FALSE(load_checked(db, "2").ok); // the broken bid itself does not open
}

// Windows int('None') raises a bare ValueError for a NULL reference; iOS reports it in the "references missing" form.
TEST(Dbp31_LoadIntegrity, ANullOrNonNumericReferenceIsAMissingReference)
{
    {
        TempDb db("load_integrity_null_cond");
        seed_world(db);
        add_takeoff(db, 7, "NULL", "10");
        check_failed(db, "BidTakeoffs.UID=7 references missing BidConditions.UID=<missing> through BidConditionUID.");
    }
    {
        TempDb db("load_integrity_null_page");
        seed_world(db);
        add_takeoff(db, 7, "30", "NULL");
        check_failed(db, "BidTakeoffs.UID=7 references missing BidPages.UID=<missing> through BidPageUID.");
    }
    {
        TempDb db("load_integrity_text_cond");
        seed_world(db);
        add_takeoff(db, 7, "'abc'", "10");
        check_failed(db, "BidTakeoffs.UID=7 references missing BidConditions.UID=abc through BidConditionUID.");
    }
    {
        TempDb db("load_integrity_text_parent");
        seed_world(db);
        add_takeoff(db, 7, "30", "10", "'abc'");
        check_failed(db, "BidTakeoffs.UID=7 references missing BidTakeoffs.UID=abc through ParentUID.");
    }
}

// An empty TEXT reference is not NULL: str('') is empty and the Takeoff contract (bool(condition_uid)) fails first.
TEST(Dbp31_LoadIntegrity, AnEmptyTextReferenceBreaksTheTakeoffContract)
{
    {
        TempDb db("load_integrity_blank_cond");
        seed_world(db);
        add_takeoff(db, 7, "''", "10");
        check_failed(db, "BidTakeoffs.UID=7 does not satisfy the Takeoff domain contract");
    }
    {
        TempDb db("load_integrity_blank_page");
        seed_world(db);
        add_takeoff(db, 7, "30", "''");
        check_failed(db, "BidTakeoffs.UID=7 does not satisfy the Takeoff domain contract");
    }
}

// Port of test_access_reader_rejects_duplicate_and_contract_violating_takeoffs (duplicate UID 1) and the UID rules of
// require_valid_unique_bid_owned_uids: positive integers only, one row per UID, for every table the bid load reads.
TEST(Dbp31_LoadIntegrity, MalformedAndDuplicateUidsFailTheOpenForEveryTable)
{
    struct Case
    {
        const char *table;
        const char *insert; // appended after the primary key was dropped
        const char *message;
    };
    const Case cases[] = {
        {"BidTakeoffs",
         "INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (1, 1, 30, 10, '1;2;3;4'), "
         "(1, 1, 30, 10, '1;2;3;4')",
         "BidTakeoffs contains duplicate UID 1; authoritative bid-owned UIDs must be unique."},
        {"BidTakeoffs",
         "INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (0, 1, 30, 10, '1;2;3;4')",
         "BidTakeoffs contains malformed UID 0; authoritative bid-owned UIDs must be positive integers."},
        {"BidTakeoffs",
         "INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (-5, 1, 30, 10, '1;2;3;4')",
         "BidTakeoffs contains malformed UID -5; authoritative bid-owned UIDs must be positive integers."},
        {"BidTakeoffs",
         "INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (NULL, 1, 30, 10, '1;2;3;4')",
         "BidTakeoffs contains malformed UID <missing>; authoritative bid-owned UIDs must be positive integers."},
        // Port of test_mdb_reader_rejects_page_uid_zero_instead_of_loading_a_page ("BidPages contains malformed UID 0")
        {"BidPages", "INSERT INTO BidPages (UID, BidUID, Name) VALUES (0, 1, 'zero')",
         "BidPages contains malformed UID 0; authoritative bid-owned UIDs must be positive integers."},
        {"BidPages", "INSERT INTO BidPages (UID, BidUID, Name) VALUES (10, 1, 'dup')",
         "BidPages contains duplicate UID 10; authoritative bid-owned UIDs must be unique."},
        {"BidConditions", "INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (30, 1, 9, 'dup', 0)",
         "BidConditions contains duplicate UID 30; authoritative bid-owned UIDs must be unique."},
        {"BidConditions", "INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (-1, 1, 9, 'neg', 0)",
         "BidConditions contains malformed UID -1; authoritative bid-owned UIDs must be positive integers."},
        // Port of ...::test_..._reject_duplicate_uid ("BidLayers contains duplicate UID 1")
        {"BidLayers",
         "INSERT INTO BidLayers (UID, BidUID, Name, Show) VALUES (1, 1, 'a', -1), (1, 1, 'b', -1)",
         "BidLayers contains duplicate UID 1; authoritative bid-owned UIDs must be unique."},
        {"BidConditionFolders",
         "INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (7, 1, 'a'), (7, 1, 'b')",
         "BidConditionFolders contains duplicate UID 7; authoritative bid-owned UIDs must be unique."},
        {"BidPageFolders", "INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (5, 1, 'a'), (5, 1, 'b')",
         "BidPageFolders contains duplicate UID 5; authoritative bid-owned UIDs must be unique."},
        {"BidPageFolders", "INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (0, 1, 'zero')",
         "BidPageFolders contains malformed UID 0; authoritative bid-owned UIDs must be positive integers."},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string(c.table) + ": " + c.insert);
        TempDb db("load_integrity_uids");
        seed_world(db);
        drop_primary_key(db, c.table);
        db.must(c.insert);
        check_failed(db, c.message);
    }
}

// The rows of another bid and the stale rows of other bids are not part of the check (a dropped key never matters there).
TEST(Dbp31_LoadIntegrity, UidProblemsOfAnotherBidAreNotRead)
{
    TempDb db("load_integrity_uids_other_bid");
    seed_world(db);
    drop_primary_key(db, "BidPages");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (0, 2, 'zero of bid 2'), (20, 2, 'dup of bid 2')");
    add_takeoff(db, 1, "30", "10");
    CHECK(load_checked(db).ok);
}

// The tables are checked in the Windows order: layers, pages, conditions, takeoffs, then condition folders.
TEST(Dbp31_LoadIntegrity, TheChecksRunInTheWindowsOrder)
{
    {
        TempDb db("load_integrity_order_layers_pages");
        seed_world(db);
        drop_primary_key(db, "BidLayers");
        drop_primary_key(db, "BidPages");
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show) VALUES (0, 1, 'a', -1)");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (0, 1, 'zero')");
        check_failed(db, "BidLayers contains malformed UID 0; authoritative bid-owned UIDs must be positive integers.");
    }
    {
        // takeoff problems come before a condition folder cycle (Windows reads the folders after the takeoffs)
        TempDb db("load_integrity_order_folders");
        seed_world(db);
        db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (7, 1, 'a', 8), (8, 1, 'b', 7)");
        add_takeoff(db, 7, "99", "10");
        check_failed(db, "BidTakeoffs.UID=7 references missing BidConditions.UID=99 through BidConditionUID.");
    }
    {
        // the folder cycle alone still fails the open with its own message
        TempDb db("load_integrity_folder_cycle");
        seed_world(db);
        db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (7, 1, 'a', 8), (8, 1, 'b', 7)");
        add_takeoff(db, 1, "30", "10");
        check_failed(db, "BidConditionFolders.UID=7 participates in a ParentUID cycle.");
    }
}

// The takeoff contract is judged before any reference (Windows: while the rows are read), so the non-finite takeoff 2 wins
// over the missing condition of takeoff 1.
TEST(Dbp31_LoadIntegrity, TheTakeoffContractIsJudgedBeforeTheReferences)
{
    TempDb db("load_integrity_contract_first");
    seed_world(db);
    add_takeoff(db, 1, "99", "10");
    add_takeoff(db, 2, "30", "10", "0", "'nan;2;3;4'");
    check_failed(db, "BidTakeoffs.UID=2 does not satisfy the Takeoff domain contract");
}

// CdnTypes is read by the bid load as well (Windows get_bid_data reads it FIRST, mdb_reader._parse_cdn_types) and its UIDs go
// through require_unique_master_data_uids: only a repeated UID is refused (a master-data UID of 0, a negative one or a NULL one is
// not), compared as the stored text. Outcomes computed by running the real Windows MdbReader.get_bid_data read-only (python -B)
// on the same rows (CdnTypes without its primary key): (5, 5) and (NULL, NULL) fail with the messages below; (5, 5.5, 0, -1, NULL)
// opens; and the master-data check comes before the BidLayers UID check. Port of
// tests/infrastructure/database/test_master_data_identity.py (require_unique_master_data_uids([3, "3"]) is a duplicate).
TEST(Dbp31_LoadIntegrity, ARepeatedCdnTypeUidFailsTheOpen)
{
    {
        TempDb db("load_integrity_cdn_dup");
        seed_world(db);
        drop_primary_key(db, "CdnTypes");
        db.must("INSERT INTO CdnTypes (UID, Name) VALUES (5, 'a'), (5, 'b')");
        add_takeoff(db, 1, "30", "10");
        check_failed(db, "CdnTypes contains duplicate UID 5; authoritative master-data UIDs must be unique.");
    }
    {
        TempDb db("load_integrity_cdn_null");
        seed_world(db);
        drop_primary_key(db, "CdnTypes");
        db.must("INSERT INTO CdnTypes (UID, Name) VALUES (NULL, 'a'), (NULL, 'b')");
        check_failed(db, "CdnTypes contains duplicate UID ; authoritative master-data UIDs must be unique.");
    }
    {
        // not a repeat: 5 and 5.5, zero, negative and NULL are fine for master data
        TempDb db("load_integrity_cdn_fine");
        seed_world(db);
        drop_primary_key(db, "CdnTypes");
        db.must("INSERT INTO CdnTypes (UID, Name) VALUES (5, 'a'), (5.5, 'b'), (0, 'c'), (-1, 'd'), (NULL, 'e')");
        add_takeoff(db, 1, "30", "10");
        CHECK(load_checked(db).ok);
    }
    {
        // the CdnTypes check runs before the layers
        TempDb db("load_integrity_cdn_first");
        seed_world(db);
        drop_primary_key(db, "CdnTypes");
        drop_primary_key(db, "BidLayers");
        db.must("INSERT INTO CdnTypes (UID, Name) VALUES (5, 'a'), (5, 'b')");
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show) VALUES (0, 1, 'z', -1)");
        check_failed(db, "CdnTypes contains duplicate UID 5; authoritative master-data UIDs must be unique.");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// POS-M01: non-finite positions and rotations
// ---------------------------------------------------------------------------------------------------------------

// Port of test_access_reader_rejects_duplicate_and_contract_violating_takeoffs (b"nan;2;3;4\n" -> UID=2 does not satisfy
// the Takeoff domain contract); parse_position itself accepts nan / inf like Python float().
TEST(PosM01_NonFinitePositions, ANonFinitePositionFailsTheOpenAndNamesTheTakeoff)
{
    for (const char *position : {"'nan;2;3;4'", "'inf;1;2;3'", "'1;2;-inf;4'", "'1e400;1'", "'NaN;1'", "'-Infinity;1'"})
    {
        SCOPED_CONTEXT(std::string("Position ") + position);
        TempDb db("pos_m01_position");
        seed_world(db);
        add_takeoff(db, 1, "30", "10");
        add_takeoff(db, 2, "30", "10", "0", position);
        check_failed(db, "BidTakeoffs.UID=2 does not satisfy the Takeoff domain contract");
    }
}

// The valid control of the test plan: '1;2;3;4' loads as [1, 2, 3, 4] next to the rejected spellings.
TEST(PosM01_NonFinitePositions, TheFiniteControlLoadsItsCoordinates)
{
    TempDb db("pos_m01_control");
    seed_world(db);
    add_takeoff(db, 1, "30", "10", "0", "'1;2;3;4'");
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    REQUIRE_EQ(out.project.takeoffs.size(), static_cast<size_t>(1));
    CHECK(out.project.takeoffs[0].position == (std::vector<double>{1.0, 2.0, 3.0, 4.0}));
}

// Port of test_a_non_finite_rotation_violates_the_takeoff_contract_with_its_uid (a 'nan' rotation) and the other
// non-finite spellings: stored infinity, text nan / inf. A finite rotation as number or text is fine.
TEST(PosM01_NonFinitePositions, ANonFiniteRotationFailsTheOpenAndNamesTheTakeoff)
{
    for (const char *rotation : {"'nan'", "1e999", "-1e999", "'inf'"})
    {
        SCOPED_CONTEXT(std::string("Rotation ") + rotation);
        TempDb db("pos_m01_rotation");
        seed_world(db);
        add_takeoff(db, 9, "30", "10", "0", "'1;2;3;4'", rotation);
        check_failed(db, "BidTakeoffs.UID=9 does not satisfy the Takeoff domain contract");
    }
    TempDb db("pos_m01_rotation_finite");
    seed_world(db);
    add_takeoff(db, 9, "30", "10", "0", "'1;2;3;4'", "NULL");
    add_takeoff(db, 10, "30", "10", "0", "'1;2;3;4'", "'12.5'");
    add_takeoff(db, 11, "30", "10", "0", "'1;2;3;4'", "1.5");
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    REQUIRE_EQ(out.project.takeoffs.size(), static_cast<size_t>(3));
    CHECK_EQ(out.project.takeoffs[0].rotation, 0.0);
    CHECK_EQ(out.project.takeoffs[1].rotation, 12.5);
    CHECK_EQ(out.project.takeoffs[2].rotation, 1.5);
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-16: page-folder parent cycles
// ---------------------------------------------------------------------------------------------------------------

// Port of test_hierarchy_reader.py::test_page_hierarchy_rejects_folder_parent_cycles ((10 parent 10), (10 <-> 11),
// (10 -> 11 -> 12 -> 10), each "BidPageFolders.UID=10 participates in a ParentUID cycle") plus 47 more cases, every one the
// outcome of the real Windows _get_bid_folder_page_structure: tails into a cycle, two cycles, parents of another bid.
TEST(Brw16_PageFolderCycles, EveryWindowsOracleCaseGivesTheWindowsOutcome)
{
    int index = 0;
    int failures = 0;
    for (const ost_ptest::FolderCycleOracleCase &c : ost_ptest::kFolderCycleOracleCases)
    {
        SCOPED_CONTEXT("oracle case " + std::to_string(index++) + " folders=" + c.folders);
        TempDb db("brw16_oracle");
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, JobName, BidNo, MeasureBase) VALUES (1, 'J1', 1, 0)");
        db.must("INSERT INTO Bids (UID, JobName, BidNo, MeasureBase) VALUES (2, 'J2', 2, 0)");
        db.must(std::string("INSERT INTO BidPageFolders (UID, BidUID, Name, Description, ParentUID) VALUES ") + c.folders);

        const std::string expected = c.expected;
        const Outcome out = load_checked(db);
        if (expected == "OK")
        {
            CHECK(out.ok);
            CHECK_STR_EQ(out.error, "");
        }
        else
        {
            ++failures;
            CHECK_FALSE(out.ok);
            CHECK_STR_EQ(out.error, oracle_message(expected));
            CHECK(out.project.empty());
        }
    }
    CHECK(failures >= 35); // the table really holds cycles (a generator that lost them would pass vacuously)
}

// The host-run example of the item: cycle 18 <-> 19 and page 203 in folder 18. The folders no longer vanish and the page is
// not left unreachable: the bid does not open and the message names the cycle.
TEST(Brw16_PageFolderCycles, ACycleDoesNotHideTheFoldersAndTheirPagesItFailsTheOpen)
{
    TempDb db("brw16_item_example");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, JobName, BidNo, MeasureBase) VALUES (1, 'J1', 1, 0)");
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name, ParentUID) VALUES (18, 1, 'A', 19), (19, 1, 'B', 18), (20, 1, 'Fine', NULL)");
    db.must("INSERT INTO BidPages (UID, BidUID, Name, BidPageFolderUID) VALUES (203, 1, 'P203', 18), (204, 1, 'P204', 20)");
    check_failed(db, "BidPageFolders.UID=18 participates in a ParentUID cycle.");
}

// A page-folder cycle of ANOTHER bid does not stop this bid from opening.
TEST(Brw16_PageFolderCycles, ACycleOfAnotherBidIsNotRead)
{
    TempDb db("brw16_other_bid");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, JobName, BidNo, MeasureBase) VALUES (1, 'J1', 1, 0)");
    db.must("INSERT INTO Bids (UID, JobName, BidNo, MeasureBase) VALUES (2, 'J2', 2, 0)");
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name, ParentUID) VALUES (10, 1, 'Mine', NULL), (60, 2, 'X', 61), (61, 2, 'Y', 60)");
    db.must("INSERT INTO BidPages (UID, BidUID, Name, BidPageFolderUID) VALUES (100, 1, 'P', 10)");
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    CHECK_EQ(out.project.folders.size(), static_cast<size_t>(1));
    CHECK_FALSE(load_checked(db, "2").ok);
}

// ---------------------------------------------------------------------------------------------------------------
// QTY-19 (+POS-M02): takeoffs without geometry are kept
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    // Bid 1 with a Count (calc 23, EA), a Linear (length, inches) and an Area (square inches) condition on page 10.
    void seed_geometry_world(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, JobName, BidNo, MeasureBase) VALUES (1, 'J1', 1, 0)");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (10, 1, 'P10')");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, Quantity1, UOM1) VALUES (30, 1, 1, 'Count', 2, 23, 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, Quantity1, UOM1) VALUES (31, 1, 2, 'Linear', 0, 1, 1)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, Quantity1, UOM1) VALUES (32, 1, 3, 'Area', 1, 1, 4)");
    }
} // namespace

// Alias POS-M02 test plan: three takeoffs of one Count condition with Position NULL, '' and '5;6'. Windows count: 3
// takeoffs, positions [], [], [5, 6]; the quantity of CALC_COUNT is 3 (test_position_text_and_bytes_decode_to_the_same_
// coordinates keeps NULL and b'' as []).
TEST(Qty19_EmptyPositions, EmptyAndMissingPositionsAreKeptAndCounted)
{
    TempDb db("qty19_count");
    seed_geometry_world(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (1, 1, 30, 10, NULL)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (2, 1, 30, 10, '')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (3, 1, 30, 10, '5;6')");
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    REQUIRE_EQ(out.project.takeoffs.size(), static_cast<size_t>(3));
    CHECK(out.project.takeoffs[0].position.empty());
    CHECK(out.project.takeoffs[1].position.empty());
    CHECK(out.project.takeoffs[2].position == (std::vector<double>{5.0, 6.0}));
    CHECK_EQ(first_quantity(out.project, "30"), 3.0);
}

// Unparsable and short positions are kept as well (parse_position gives [] for any bad token, one value stays one value).
TEST(Qty19_EmptyPositions, UnparsableAndShortPositionsAreKept)
{
    TempDb db("qty19_unparsable");
    seed_geometry_world(db);
    const char *positions[] = {"'abc'", "'5'", "' '", "'1;2;;3;4'", "'1;x'"};
    int uid = 1;
    for (const char *position : positions)
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (" +
                std::to_string(uid++) + ", 1, 30, 10, " + position + ")");
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    CHECK_STR_EQ(takeoff_summary(out.project), "1=0,2=1,3=0,4=4,5=0");
    CHECK_EQ(first_quantity(out.project, "30"), 5.0);
}

// Port of test_condition_quantity_service.py::test_a_takeoff_without_geometry_totals_zero (linear and area): a kept
// takeoff without geometry measures 0, it does not disturb the other takeoffs of the condition.
TEST(Qty19_EmptyPositions, LinearAndAreaTakeoffsWithoutGeometryMeasureZero)
{
    TempDb db("qty19_linear_area");
    seed_geometry_world(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (1, 1, 31, 10, '')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (2, 1, 32, 10, NULL)");
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    REQUIRE_EQ(out.project.takeoffs.size(), static_cast<size_t>(2));
    CHECK_EQ(first_quantity(out.project, "31"), 0.0);
    CHECK_EQ(first_quantity(out.project, "32"), 0.0);

    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (3, 1, 31, 10, '0;0;30;40')");
    const Outcome both = load_checked(db);
    REQUIRE(both.ok);
    CHECK_EQ(first_quantity(both.project, "31"), 50.0);
}

// The mesh code skips a kept takeoff without usable geometry (no crash, no mesh) and still meshes the others.
TEST(Qty19_EmptyPositions, MeshGenerationToleratesTakeoffsWithoutGeometry)
{
    TempDb db("qty19_meshes");
    seed_geometry_world(db);
    const char *rows[] = {
        "(1, 1, 30, 10, '')",                      // count without geometry
        "(2, 1, 30, 10, '5;6')",                   // count: meshed
        "(3, 1, 31, 10, '')",                      // linear without geometry
        "(4, 1, 31, 10, '5')",                     // linear with one value
        "(5, 1, 31, 10, '0;0;10;0')",              // linear: meshed
        "(6, 1, 32, 10, NULL)",                    // area without geometry
        "(7, 1, 32, 10, '1;2')",                   // area with one point
        "(8, 1, 32, 10, '0;0;10;0;10;10;0;10')"};  // area: meshed
    for (const char *row : rows)
        db.must(std::string("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES ") + row);
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    CHECK_EQ(out.project.takeoffs.size(), static_cast<size_t>(8));

    for (const bool transform : {false, true})
    {
        const std::vector<ost_meshing::TakeoffMesh> meshes = ost_meshing::generate_takeoff_meshes(out.project, {}, transform);
        std::set<std::string> meshed;
        for (const ost_meshing::TakeoffMesh &mesh : meshes)
            meshed.insert(mesh.takeoff->uid);
        CHECK(meshed == (std::set<std::string>{"2", "5", "8"}));
    }
}

// The loaded takeoffs keep their order and every column (a kept takeoff is a whole takeoff, not a placeholder).
TEST(Qty19_EmptyPositions, AKeptTakeoffKeepsItsOtherColumns)
{
    TempDb db("qty19_columns");
    seed_geometry_world(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position, Rotation, Curve, IsNegativeQuantity) "
            "VALUES (4, 1, 30, 10, '', 1.5, 3, 1)");
    const Outcome out = load_checked(db);
    REQUIRE(out.ok);
    REQUIRE_EQ(out.project.takeoffs.size(), static_cast<size_t>(1));
    const ost_domain::Takeoff &t = out.project.takeoffs[0];
    CHECK_STR_EQ(t.uid, "4");
    CHECK_STR_EQ(t.condition_uid, "30");
    CHECK_STR_EQ(t.page_uid, "10");
    CHECK_EQ(t.rotation, 1.5);
    CHECK_EQ(t.curve, 3);
    CHECK(t.is_negative);
    CHECK(t.position.empty());
}
