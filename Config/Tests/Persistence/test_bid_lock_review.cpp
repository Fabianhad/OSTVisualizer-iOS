// Independent review additions for S0 (plan 4.1): cases the first tests let a lazy implementation survive, found by mutation thinking.
// Additions only. Expected values are the answers of the REAL Windows reader + ProjectReadService.is_bid_locked
// (Windows 49a2d59: cover_sheet.py:15-23, project_read_service.py:81-94, settings_reader.py:63-77 and :361-370, ost_serializer.py:18-34),
// executed read-only with python -B -I on an in-memory sqlite database with the DDL of the Windows tests (a throw-away generator, not in
// this repository).
//
// Survivors this file kills:
//   - a store that TRIMS the bid's status text before the lookup (a lone " " reads as "no status" = unlocked, " 1" resolves to status 1);
//   - a store that reads the bid's status as a number (7.0, "7.0", "NULL" and "None" must NOT resolve to status 7; 2^63 - 1 must);
//   - a Locked REAL cell read with another text form than Windows' (-1e-16 is "-0", 0.1 + 0.2 keeps its digits, 1e300 has 301 digits);
//   - a Sequence parser that is stricter or looser than Python int() (tab and newline, vertical tab, form feed, "+0", "1_000";
//     "nan", "inf", "0b1", "1,0" and the unit separator are refused);
//   - a read that stops at the first NUL byte of a cell, or that does not decode a BLOB as strict UTF-8 (a locked bid reads open);
//   - duplicate NULL / empty status UIDs not counted, and JobStatuses read although the bid has no status.
#include "StoreTestSupport.hpp"

#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore_BidLock.hpp"
#include "Persistence/ProjectStore/ProjectStore_Integrity.hpp"

#include <string>
#include <utility>
#include <vector>

using ost_ptest::TempDb;

namespace
{
    using Statuses = std::vector<std::pair<std::string, std::string>>;

    // Both entry points, both pre-set values of `locked`; an unlocked answer must also be a successful read.
    void expect_bid_10(const TempDb &db, bool want_locked)
    {
        for (const bool initial : {false, true})
        {
            ost_db::OstDatabase connection;
            REQUIRE(connection.open(db.path()));
            bool inside = initial;
            const bool inside_ok = ost_store::integrity::bid_is_locked(connection, 10, inside);
            CHECK_EQ(inside, want_locked);
            bool wrapper = initial;
            const bool wrapper_ok = ost_store::bid_is_locked(db.path(), "10", wrapper);
            CHECK_EQ(wrapper, want_locked);
            if (!want_locked)
            {
                CHECK(inside_ok);
                CHECK(wrapper_ok);
            }
        }
    }

    // The two Windows-test tables (Locked and JobStatusUID untyped), bid 10 pointing at status 5, one status 5 whose cells are given.
    void one_status(const TempDb &db, const std::string &locked_sql, const std::string &sequence_sql)
    {
        db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
        db.must("CREATE TABLE JobStatuses (UID INTEGER, Name TEXT, Locked, Sequence INTEGER)");
        db.must("INSERT INTO Bids VALUES (10, 5)");
        db.must("INSERT INTO JobStatuses VALUES (5, 'Status', " + locked_sql + ", " + sequence_sql + ")");
    }

    struct Cell
    {
        const char *sql;
        bool locked;
    };
} // namespace

// settings_reader.py / project_read_service.py:81-94 on the bid's status as TEXT: str(JobStatusUID) is compared with str(JobStatuses.UID)
// verbatim. Oracle: the real Windows code on each row (is_bid_locked).
TEST(BidLockReview, TheBidStatusIsComparedAsTheTextWindowsMakesOfIt)
{
    struct Case
    {
        const char *name;
        const char *bid_status_sql; // a literal of the untyped Bids.JobStatusUID column
        Statuses statuses;
        bool locked;
    };
    const Case cases[] = {
        // a status of only blanks is NOT "no status" (only None and the empty text are), and a blank next to a digit is another text
        {"a lone space, open and locked statuses present", "' '", {{"1", "0"}, {"2", "-1"}}, true},
        {"a leading space, status 1 is open", "' 1'", {{"1", "0"}}, true},
        {"a trailing space, status 1 is open", "'1 '", {{"1", "0"}}, true},
        {"the plain text 1 resolves (control)", "'1'", {{"1", "0"}}, false},
        // str(7.0) is "7.0", never "7"; the texts NULL and None are not "no status" either
        {"a REAL 7.0 is the text 7.0", "7.0", {{"7", "0"}}, true},
        {"a REAL 1.0 is the text 1.0", "1.0", {{"1", "0"}}, true},
        {"the text 7.0", "'7.0'", {{"7", "0"}}, true},
        {"the text NULL is a status uid, not a missing one", "'NULL'", {{"7", "0"}}, true},
        {"the text None is a status uid, not a missing one", "'None'", {{"7", "0"}}, true},
        {"the text 0 with an open status 0 (control)", "'0'", {{"0", "0"}}, false},
        // 64-bit uids resolve exactly (nothing is narrowed to 32 bit or compared as a double)
        {"uid 2147483648 (past 32 bit signed)", "2147483648", {{"2147483648", "0"}}, false},
        {"uid 4294967297", "4294967297", {{"4294967297", "0"}}, false},
        {"uid 2^63 - 1", "9223372036854775807", {{"9223372036854775807", "0"}}, false},
        {"uid 2^63 - 1 is not 2^63 - 2", "9223372036854775806", {{"9223372036854775807", "0"}}, true},
    };
    int index = 0;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.name);
        TempDb db("bidlock_rev_text_" + std::to_string(index++), false);
        ost_ptest::seed_lock_oracle_case(db, 10, c.bid_status_sql, c.statuses);
        expect_bid_10(db, c.locked);
    }
}

// ost_serializer.py:18-34 _format_float on the Locked cell: whole and below 1e15 is its integer digits, else %.15f without trailing zeros
// ("-0" for -1e-16 and "0" for 1e-16: so -1e-16 is LOCKED and 1e-16 is not). Oracle: the real Windows reader on each value.
TEST(BidLockReview, ARealLockedCellKeepsTheTextFormOfWindows)
{
    const Cell cells[] = {
        {"1e300", true},                // 301 digits
        {"5e-324", false},              // the smallest denormal prints as 0.000000000000000 -> "0"
        {"0.30000000000000004", true},  // "0.3"
        {"1e15", true},                 // not below 1e15: the %.15f form "1000000000000000"
        {"-1e-16", true},               // "-0"
        {"1e-16", false},               // "0"
        {"4e-16", false},               // "0"
        {"6e-16", true},                // "0.000000000000001"
    };
    int index = 0;
    for (const Cell &cell : cells)
    {
        SCOPED_CONTEXT(std::string("Locked = ") + cell.sql);
        TempDb db("bidlock_rev_real_" + std::to_string(index++), false);
        one_status(db, cell.sql, "1");
        expect_bid_10(db, cell.locked);
    }
}

// settings_reader.py:72 int(text): the Sequence texts Python accepts keep the bid readable (open), those it refuses empty the whole status
// list (locked). Oracle: the real Windows reader. SQL expressions use char() so no control character sits in the source.
TEST(BidLockReview, TheSequenceFormsPythonIntAcceptsAndRefuses)
{
    const Cell cells[] = {
        {"char(9) || '5' || char(10)", false}, // tab and newline are blanks for int()
        {"'5' || char(11)", false},            // vertical tab
        {"'5' || char(12)", false},            // form feed
        {"'+0'", false},
        {"'1_000'", false},
        {"''", false},
        {"1e15", false},                       // a whole REAL is read as its digits
        {"1e22", false},
        {"1e-5", true},                        // the text "0.00001" is not an integer
        {"'5' || char(31)", true},             // the unit separator is not a blank for int()
        {"'nan'", true},
        {"'inf'", true},
        {"'0b1'", true},
        {"'1,0'", true},
        {"' '", true},                         // blanks only: int("") raises
    };
    int index = 0;
    for (const Cell &cell : cells)
    {
        SCOPED_CONTEXT(std::string("Sequence = ") + cell.sql);
        TempDb db("bidlock_rev_seq_" + std::to_string(index++), false);
        one_status(db, "0", cell.sql);
        expect_bid_10(db, cell.locked);
    }
}

// Deliberate safe deviation (ProjectStore+BidLock.cpp python_int_accepts, recorded by the implementer): Python int() also accepts
// non-ASCII digits and Unicode blanks (Arabic-Indic digits, full-width digits, no-break space, em space, NEL: Windows reads each of these
// as an integer and leaves the bid open), iOS counts them as not an integer, so the whole list is refused and the bid reads LOCKED.
// Pinned so that a change of this safe side is a decision, not an accident.
TEST(BidLockReview, ANonAsciiSequenceTextReadsLockedOnIosWhereWindowsAcceptsIt)
{
    const char *const cells[] = {
        "char(1633, 1634)",       // two Arabic-Indic digits
        "char(65301)",            // a full-width 5
        "char(160) || '5'",       // no-break space
        "char(8195) || '5'",      // em space
        "'5' || char(133)",       // NEL
    };
    int index = 0;
    for (const char *cell : cells)
    {
        SCOPED_CONTEXT(std::string("Sequence = ") + cell);
        TempDb db("bidlock_rev_nonascii_" + std::to_string(index++), false);
        one_status(db, "0", cell);
        expect_bid_10(db, true);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the BYTES of a cell (review round 2): what Windows makes of text and blobs that a C-string read gets wrong
// ---------------------------------------------------------------------------------------------------------------
// ost_serializer.py:18-34 serialize_value: a text cell is str(value) (an embedded NUL STAYS in it), a blob is value.decode("utf-8")
// (STRICT: invalid UTF-8 raises) with every NUL removed. settings_reader.py:361-370 turns any raise into an EMPTY status list (= every
// status unresolved = locked), and _select_all_unfiltered (bid_data_reader.py:448-468) serializes EVERY column of EVERY JobStatuses row,
// so a bad blob in the Name column of an unrelated row spoils the whole list too. A read that stops at the first NUL (std::string built
// from a const char*) turns "0" + NUL + "1" into "0" and an embedded-NUL text into "": a LOCKED bid reads OPEN. Expected values: the real
// Windows reader + ProjectReadService.is_bid_locked executed read-only (python -B -I, in-memory sqlite, the DDL of the Windows tests).

namespace
{
    // Status 5 (open, Sequence 1) and a second status whose four cells are given as SQL literals; the bid points at 5 (or at `bid_status`).
    void two_statuses(const TempDb &db, const std::string &uid6, const std::string &name6, const std::string &locked6,
                      const std::string &sequence6, const std::string &bid_status = "5")
    {
        db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
        db.must("CREATE TABLE JobStatuses (UID INTEGER, Name, Locked, Sequence INTEGER)");
        db.must("INSERT INTO Bids VALUES (10, " + bid_status + ")");
        db.must("INSERT INTO JobStatuses VALUES (5, 'Open', 0, 1)");
        db.must("INSERT INTO JobStatuses VALUES (" + uid6 + ", " + name6 + ", " + locked6 + ", " + sequence6 + ")");
    }
} // namespace

// settings_reader.py:71 on the serialized text of a Locked cell with a NUL in it: text keeps the NUL (locked unless the whole text is
// "0" / "False" / ""), a blob loses it first (X'3000' is "0": open, X'0031' is "1": locked).
TEST(BidLockReview, AnEmbeddedNulInALockedCellIsKeptInTextAndStrippedFromBlobsLikeWindows)
{
    const Cell cells[] = {
        {"char(0)", true},                 // the text NUL: not ""
        {"'0' || char(0)", true},          // "0" + NUL is not "0"
        {"char(0) || '0'", true},
        {"'0' || char(0) || '1'", true},   // a read that stops at the NUL sees "0": open
        {"'False' || char(0)", true},
        {"X'0031'", true},                 // blob: NUL stripped -> "1"; a C-string read sees "" : open
        {"X'300030'", true},               // "00"
        {"X'3100'", true},                 // control: "1"
        {"X'3000'", false},                // blob "0" + NUL -> "0": open (a read that keeps the NUL would lock)
        {"X'0030'", false},
        {"X'46616c736500'", false},        // "False" + NUL -> "False"
        {"X'00'", false},                  // control: "" -> open
    };
    int index = 0;
    for (const Cell &cell : cells)
    {
        SCOPED_CONTEXT(std::string("Locked = ") + cell.sql);
        TempDb db("bidlock_rev_nul_" + std::to_string(index++), false);
        one_status(db, cell.sql, "1");
        expect_bid_10(db, cell.locked);
    }
}

// serialize_value decodes a blob STRICTLY: invalid UTF-8 (a lone 0xFF, an overlong C0 80, a surrogate, a code point past U+10FFFF) raises,
// and the raise empties the list, so the open status 5 is unresolved: locked. Whatever the column (UID, Name, Locked, Sequence) and
// whichever row (the bid's own or another): the Windows reader converts them all. Valid UTF-8 blobs are the controls (open).
TEST(BidLockReview, AnInvalidUtf8BlobAnywhereInTheStatusTableLocksEveryBidWithAStatus)
{
    struct Row
    {
        const char *name;
        const char *uid6;
        const char *name6;
        const char *locked6;
        const char *sequence6;
        bool locked;
    };
    const Row rows[] = {
        {"Locked X'FF'", "6", "'Other'", "X'FF'", "2", true},
        {"Locked overlong X'C080'", "6", "'Other'", "X'C080'", "2", true},
        {"Locked surrogate X'EDA080'", "6", "'Other'", "X'EDA080'", "2", true},
        {"Locked past U+10FFFF X'F4908080'", "6", "'Other'", "X'F4908080'", "2", true},
        {"UID X'FF'", "X'FF'", "'Other'", "0", "2", true},
        {"Sequence X'FF'", "6", "'Other'", "0", "X'FF'", true},
        {"Name X'FF' (a column the lock rule never uses)", "6", "X'FF'", "0", "2", true},
        {"control: Locked X'30' (valid, \"0\")", "6", "'Other'", "X'30'", "2", false},
        {"control: Locked X'C3A9' (valid e-acute)", "6", "'Other'", "X'C3A9'", "2", false},
        {"control: Locked X'EFBBBF30' (valid, BOM + 0)", "6", "'Other'", "X'EFBBBF30'", "2", false},
        {"control: Name X'C3A9' (valid)", "6", "X'C3A9'", "0", "2", false},
    };
    int index = 0;
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(row.name);
        TempDb db("bidlock_rev_utf8_" + std::to_string(index++), false);
        two_statuses(db, row.uid6, row.name6, row.locked6, row.sequence6);
        expect_bid_10(db, row.locked);
    }
    {
        // the same spoiled table, a bid WITHOUT a status: JobStatuses is never read (project_read_service.py:82-83), so it stays open
        SCOPED_CONTEXT("the bid has no status");
        TempDb db("bidlock_rev_utf8_nostatus", false);
        two_statuses(db, "6", "'Other'", "X'FF'", "2", "NULL");
        expect_bid_10(db, false);
    }
}

// The bid's OWN status cell is compared as the text Windows makes of it: str(value). A NUL inside the text, an empty or non-text blob
// are never "no status" and never equal a status uid, whatever a C-string read would see ("" for NUL / empty, "5" for X'35' and for
// "5" + NUL + "x"). Same for a JobStatuses UID cell with a NUL: "5" + NUL + "x" is not "5".
TEST(BidLockReview, ABidStatusOrStatusUidWithNulOrBytesNeverResolvesByATruncatedRead)
{
    const Cell bid_status_cells[] = {
        {"char(0)", true},               // a C-string read: "" = no status = open
        {"X''", true},                   // an empty blob: str(b"") is "b''", not ""
        {"X'35'", true},                 // a blob never names status 5
        {"X'00'", true},
        {"'5' || char(0) || 'x'", true}, // a C-string read: "5" = status 5 = open
        {"'5'", false},                  // control
    };
    int index = 0;
    for (const Cell &cell : bid_status_cells)
    {
        SCOPED_CONTEXT(std::string("Bids.JobStatusUID = ") + cell.sql);
        TempDb db("bidlock_rev_bidstatus_nul_" + std::to_string(index++), false);
        ost_ptest::seed_lock_oracle_case(db, 10, cell.sql, {{"5", "0"}});
        expect_bid_10(db, cell.locked);
    }
    {
        SCOPED_CONTEXT("JobStatuses.UID = '5' + NUL + 'x'");
        TempDb db("bidlock_rev_uid_nul", false);
        ost_ptest::seed_lock_oracle_case(db, 10, "5", {{"'5' || char(0) || 'x'", "0"}});
        expect_bid_10(db, true);
    }
}

// require_unique_master_data_uids (master_data_identity.py:19-30) runs on the SERIALIZED uid ("NULL" for a NULL cell, "" for the empty
// text): two NULL uids, a NULL and the text "NULL", or two empty texts are duplicates and empty the whole list, so the OPEN status 5 is
// unresolved (locked). A NULL and an empty text are two different keys, and one NULL uid alone is no duplicate. The older duplicate test
// asks for status 3 with two NULL uids, which is unresolved WITHOUT the duplicate rule, so it cannot tell a missing rule from a present one.
// Oracle: the real Windows reader + is_bid_locked.
TEST(BidLockReview, DuplicateNullOrEmptyJobStatusUidsSpoilAnOpenStatusThatWouldResolve)
{
    struct Case
    {
        const char *name;
        Statuses statuses;
        bool locked;
    };
    const Case cases[] = {
        {"two NULL uids", {{"5", "0"}, {"NULL", "0"}, {"NULL", "0"}}, true},
        {"a NULL uid and the text NULL", {{"5", "0"}, {"NULL", "0"}, {"'NULL'", "0"}}, true},
        {"two empty uids", {{"5", "0"}, {"''", "0"}, {"''", "0"}}, true},
        {"a NULL uid and an empty uid are not duplicates", {{"5", "0"}, {"NULL", "0"}, {"''", "0"}}, false},
        {"one NULL uid alone is no duplicate", {{"5", "0"}, {"NULL", "0"}}, false},
        {"the integer 5 and the real 5.0 are one uid", {{"5", "0"}, {"5.0", "0"}}, true},
    };
    int index = 0;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.name);
        TempDb db("bidlock_rev_dupnull_" + std::to_string(index++), false);
        ost_ptest::seed_lock_oracle_case(db, 10, "5", c.statuses);
        expect_bid_10(db, c.locked);
    }
}

// project_read_service.py:82-83: a bid with no status (None or the empty text) is unlocked BEFORE anything of JobStatuses is read, so a
// JobStatuses table that cannot be read at all (a column missing: the statement would fail) must not matter to it, through both entry
// points, with a successful read. A bid WITH a status on the same table is the failed read of test_bid_lock_cells.cpp (locked, false).
// A store that reads the status table first (and fails closed on its error) survives every other test: the damaged tables there only
// carry a bid with a status.
TEST(BidLockReview, ABidWithoutAStatusNeverTouchesADamagedStatusTable)
{
    struct Case
    {
        const char *name;
        const char *statuses_ddl;
    };
    const Case tables[] = {
        {"no Locked column", "CREATE TABLE JobStatuses (UID INTEGER, Name TEXT, Sequence INTEGER)"},
        {"no UID column", "CREATE TABLE JobStatuses (Name TEXT, Locked, Sequence INTEGER)"},
        {"no Sequence column", "CREATE TABLE JobStatuses (UID INTEGER, Name TEXT, Locked)"},
    };
    int index = 0;
    for (const Case &table : tables)
    {
        for (const char *bid_status : {"NULL", "''"})
        {
            SCOPED_CONTEXT(std::string(table.name) + ", Bids.JobStatusUID = " + bid_status);
            TempDb db("bidlock_rev_damaged_nostatus_" + std::to_string(index++), false);
            db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
            db.must(table.statuses_ddl);
            db.must(std::string("INSERT INTO Bids VALUES (10, ") + bid_status + ")");
            expect_bid_10(db, false); // unlocked AND a successful read, both entry points
        }
    }
}
