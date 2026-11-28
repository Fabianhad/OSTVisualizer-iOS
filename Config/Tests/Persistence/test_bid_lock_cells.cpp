// The bid lock read of the CELLS the Windows reader hands to its rule as text (plan 4.1, S0 implementer's additions to test_bid_lock.cpp):
// a Locked cell of the REAL and BLOB storage classes and a Sequence cell that Python int() refuses. Windows converts every cell with
// serialize_value (ost_visualizer/infrastructure/parsers/ost_serializer.py:18-50: NULL -> "NULL", a real that is whole and below 1e15 -> its
// integer digits, any other real -> %.15f without trailing zeros, bytes -> decoded text) and then applies the rule on that text:
//   settings_reader.py:71   locked = row.get("Locked", "0") not in ("0", "False", "")
//   settings_reader.py:72   sequence = int(_clean_optional_text(row.get("Sequence")) or 0)   (a text int() refuses RAISES)
//   settings_reader.py:361-370  get_job_statuses: any raise gives an EMPTY list = every status unresolved = locked
// Every expected value below is the answer of the REAL Windows reader + ProjectReadService.is_bid_locked (Windows 49a2d59), executed
// read-only with python -B -I on an in-memory sqlite database with the DDL of the Windows tests (the generator is a throw-away script, not
// in this repository, like the generator of BidLockOracleData.hpp). Not covered, a deliberate safe deviation: a non-ASCII digit in a Sequence
// text (Python accepts an Arabic-Indic digit, iOS counts it as not an integer: the bid reads locked); the missing columns are in the last test.
#include "StoreTestSupport.hpp"

#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore_BidLock.hpp"
#include "Persistence/ProjectStore/ProjectStore_Integrity.hpp"

#include <string>
#include <vector>

using ost_ptest::TempDb;

namespace
{
    // The two Windows-test tables (Locked and JobStatusUID untyped), bid 10 pointing at status 5.
    void make_tables(const TempDb &db, const std::string &bid_status_sql)
    {
        db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
        db.must("CREATE TABLE JobStatuses (UID INTEGER, Name TEXT, Locked, Sequence INTEGER)");
        db.must("INSERT INTO Bids VALUES (10, " + bid_status_sql + ")");
    }

    // Both entry points, both pre-set values of `locked`: the answer must be `want_locked` and the read must have succeeded.
    void expect_locked(const TempDb &db, bool want_locked)
    {
        for (const bool initial : {false, true})
        {
            ost_db::OstDatabase connection;
            REQUIRE(connection.open(db.path()));
            bool inside = initial;
            CHECK(ost_store::integrity::bid_is_locked(connection, 10, inside));
            CHECK_EQ(inside, want_locked);
            bool wrapper = initial;
            CHECK(ost_store::bid_is_locked(db.path(), "10", wrapper));
            CHECK_EQ(wrapper, want_locked);
        }
    }

    struct Cell
    {
        const char *literal; // SQL literal of the cell
        bool locked;         // is_bid_locked of the real Windows code
    };
} // namespace

// Table A: one status (uid 5, Sequence 1) with this Locked cell, the bid points at it. A real is read as the text Windows makes of it
// (0.0 is "0": open; 1e-20 is "0.000000000000000" trimmed to "0": open; 0.5 is "0.5": locked), bytes as their text (X'30' is "0": open;
// X'00' loses its NUL and is empty: open), a real that is not finite makes the whole read fail (locked).
TEST(BidLockCells, TheLockedCellIsReadAsTheTextWindowsMakesOfIt)
{
    const std::vector<Cell> cells = {
        {"0.0", false},   {"1.0", true},   {"1.5", true},  {"-0.0", false},          {"1e-20", false}, {"0.5", true},
        {"-1.0", true},   {"9e999", true}, {"-9e999", true}, {"X'30'", false},       {"X'46616c7365'", false},
        {"X'31'", true},  {"X'00'", false},
    };
    int index = 0;
    for (const Cell &cell : cells)
    {
        SCOPED_CONTEXT(std::string("Locked = ") + cell.literal);
        TempDb db("bidlock_cell_locked_" + std::to_string(index++), false);
        make_tables(db, "5");
        db.must(std::string("INSERT INTO JobStatuses VALUES (5, 'Status', ") + cell.literal + ", 1)");
        expect_locked(db, cell.locked);
    }
}

// Table B: one OPEN status (Locked 0) whose Sequence cell is as written, the bid points at it. Locked here can only mean that the
// whole JobStatuses read was refused (Windows: int() raised, the list is empty, the status is unresolved); open means int() accepted the
// text (NULL, the empty text and the text "NULL" are 0; surrounding blanks, a sign, single underscores between digits are fine).
TEST(BidLockCells, ASequenceThatPythonIntRefusesLocksEveryBidWithAStatus)
{
    const std::vector<Cell> cells = {
        {"NULL", false},   {"''", false},       {"'abc'", true},  {"'1.5'", true},    {"' '", true},      {"'1_0'", false},
        {"'1__0'", true},  {"'_1'", true},      {"'1_'", true},   {"'0x10'", true},   {"'NULL'", false},  {"'+5'", false},
        {"'-5'", false},   {"'  7  '", false},  {"'1e3'", false}, {"2.5", true},      {"9e999", true},    {"'12abc'", true},
        {"'--1'", true},   {"'+'", true},       {"'-'", true},
    };
    int index = 0;
    for (const Cell &cell : cells)
    {
        SCOPED_CONTEXT(std::string("Sequence = ") + cell.literal);
        TempDb db("bidlock_cell_sequence_" + std::to_string(index++), false);
        make_tables(db, "5");
        db.must(std::string("INSERT INTO JobStatuses VALUES (5, 'Status', 0, ") + cell.literal + ")");
        expect_locked(db, cell.locked);
    }
}

// Table C: the malformed Sequence is on ANOTHER row (status 6, locked), the bid points at status 5 (open): the whole list is refused, so
// the open status is unresolved too. A bid without a status never reads the table, so the same table leaves it open.
TEST(BidLockCells, AMalformedRowSpoilsTheWholeListButNotABidWithoutAStatus)
{
    {
        SCOPED_CONTEXT("the bid points at the open status 5");
        TempDb db("bidlock_cell_other_row", false);
        make_tables(db, "5");
        db.must("INSERT INTO JobStatuses VALUES (5, 'Open', 0, 1)");
        db.must("INSERT INTO JobStatuses VALUES (6, 'Locked', -1, 'abc')");
        expect_locked(db, true);
    }
    {
        SCOPED_CONTEXT("the same table, a bid without a status");
        TempDb db("bidlock_cell_other_row_no_status", false);
        make_tables(db, "NULL");
        db.must("INSERT INTO JobStatuses VALUES (5, 'Open', 0, 1)");
        db.must("INSERT INTO JobStatuses VALUES (6, 'Locked', -1, 'abc')");
        expect_locked(db, false);
    }
}

// iOS safe deviation (recorded in ProjectStore+BidLock.cpp): Windows reads a MISSING Locked column as "0" (open) and a missing JobStatusUID
// column as no status (test_settings_reader.py:912-920); iOS has always had both, so a table without them is damaged: the lock read
// FAILS (false) and the bid is locked, through both entry points. A missing UID column fails the same way.
TEST(BidLockCells, AMissingColumnIsAFailedReadNotAnOpenBid)
{
    struct Case
    {
        const char *name;
        const char *bids_ddl;
        const char *statuses_ddl;
    };
    const Case cases[] = {
        {"no Locked column", "CREATE TABLE Bids (UID INTEGER, JobStatusUID)",
         "CREATE TABLE JobStatuses (UID INTEGER, Name TEXT, Sequence INTEGER)"},
        {"no UID column in JobStatuses", "CREATE TABLE Bids (UID INTEGER, JobStatusUID)",
         "CREATE TABLE JobStatuses (Name TEXT, Locked, Sequence INTEGER)"},
        {"no JobStatusUID column in Bids", "CREATE TABLE Bids (UID INTEGER)",
         "CREATE TABLE JobStatuses (UID INTEGER, Name TEXT, Locked, Sequence INTEGER)"},
    };
    int index = 0;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.name);
        TempDb db("bidlock_cell_missing_column_" + std::to_string(index++), false);
        db.must(c.bids_ddl);
        db.must(c.statuses_ddl);
        db.must("INSERT INTO Bids (UID) VALUES (10)");
        if (std::string(c.bids_ddl).find("JobStatusUID") != std::string::npos)
            db.must("UPDATE Bids SET JobStatusUID = 5 WHERE UID = 10");
        for (const bool initial : {false, true})
        {
            ost_db::OstDatabase connection;
            REQUIRE(connection.open(db.path()));
            bool inside = initial;
            CHECK_FALSE(ost_store::integrity::bid_is_locked(connection, 10, inside));
            CHECK(inside);
            bool wrapper = initial;
            CHECK_FALSE(ost_store::bid_is_locked(db.path(), "10", wrapper));
            CHECK(wrapper);
        }
    }
}
