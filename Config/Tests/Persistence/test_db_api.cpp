// DBP-11: OstDatabase / Statement error reporting.
//
// Root cause of the swallowed SQL errors was Statement::step() returning `rc == SQLITE_ROW` only, so DONE and every error
// looked the same. These tests pin the new contract that every ProjectStore writer builds on:
//   * step_result() tells Row / Done / Error apart; step() stays the "a row is available" read-loop call;
//   * run() is true only when a write statement finished without error;
//   * failed()/error_code()/error_message() say why; a statement that failed to prepare carries its prepare error;
//   * a rejected bind blocks execution; reset() clears the failure;
//   * commit() never leaves a transaction open when it fails; in_transaction() reports the transaction state;
//   * table_exists()/column_exists() mirror MdbSchemaInspector (Windows mdb/schema_compatibility.py);
//   * connections wait for a lock (busy timeout) instead of failing at once.
// Windows contract being matched: mdb_writer.py _connection (every execute error raises, the scope rolls back).
#include "TestHarness.hpp"

#include "PersistenceTestSupport.hpp"

#include "Persistence/OstDatabase.hpp"
#include "Persistence/sqlite3.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

using ost_db::OstDatabase;
using ost_db::Statement;
using ost_db::StepResult;

namespace
{
    // In-memory database with t(id INTEGER PRIMARY KEY, v TEXT NOT NULL).
    bool open_memory_with_table(OstDatabase &db)
    {
        return db.open(":memory:") && db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT NOT NULL)");
    }

    int scalar(OstDatabase &db, const std::string &sql)
    {
        Statement stmt = db.prepare(sql);
        if (!stmt.valid() || !stmt.step())
            return -1;
        return stmt.column_int(0);
    }

    bool contains(const std::string &haystack, const std::string &needle)
    {
        return haystack.find(needle) != std::string::npos;
    }

    struct TempFile
    {
        // (the run token keeps two concurrent runs of this test from sharing, and deleting, one file)
        explicit TempFile(const std::string &name) : path(ost_test::temp_path(ost_ptest::run_token() + "_" + name))
        {
            remove_all();
        }
        ~TempFile() { remove_all(); }
        void remove_all() const
        {
            std::remove(path.c_str());
            std::remove((path + "-wal").c_str());
            std::remove((path + "-shm").c_str());
        }
        std::string path;
    };
}

TEST(DbApi, StepResultTellsRowDoneAndErrorApart)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));

    {
        Statement select = db.prepare("SELECT id FROM t");
        REQUIRE(select.valid());
        CHECK(select.step_result() == StepResult::Done); // empty table: finished, no row
        CHECK_FALSE(select.failed());
    }

    {
        Statement insert = db.prepare("INSERT INTO t (id, v) VALUES (1, 'a')");
        REQUIRE(insert.valid());
        CHECK(insert.step_result() == StepResult::Done);
        CHECK_FALSE(insert.failed());
    }

    {
        Statement select = db.prepare("SELECT id FROM t");
        REQUIRE(select.valid());
        CHECK(select.step_result() == StepResult::Row);
        CHECK_EQ(select.column_int(0), 1);
        CHECK(select.step_result() == StepResult::Done);
        CHECK_FALSE(select.failed());
    }

    {
        Statement bad = db.prepare("INSERT INTO t (id, v) VALUES (2, NULL)"); // NOT NULL violated
        REQUIRE(bad.valid());
        CHECK(bad.step_result() == StepResult::Error);
        CHECK(bad.failed());
        CHECK_EQ(bad.error_code(), SQLITE_CONSTRAINT);
        CHECK(contains(bad.error_message(), "NOT NULL constraint failed: t.v"));
    }
}

TEST(DbApi, StepStaysTheReadLoopCallAndDoesNotHideErrors)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));
    REQUIRE(db.exec("INSERT INTO t (id, v) VALUES (1, 'a'), (2, 'b')"));

    // normal read loop: rows, then false, and failed() stays false
    Statement select = db.prepare("SELECT id FROM t ORDER BY id");
    REQUIRE(select.valid());
    int rows = 0;
    while (select.step())
        ++rows;
    CHECK_EQ(rows, 2);
    CHECK_FALSE(select.failed());

    // an erroring statement also makes step() return false; failed() is what tells the loop ended in error
    Statement bad = db.prepare("INSERT INTO t (id, v) VALUES (1, 'dup')"); // PRIMARY KEY violated
    REQUIRE(bad.valid());
    CHECK_FALSE(bad.step());
    CHECK(bad.failed());
    CHECK_EQ(bad.error_code(), SQLITE_CONSTRAINT);
}

TEST(DbApi, RunIsTrueOnlyWhenTheWriteFinishedWithoutError)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));

    Statement insert = db.prepare("INSERT INTO t (id, v) VALUES (?, ?)");
    REQUIRE(insert.valid());
    insert.bind_int(1, 1);
    insert.bind_text(2, "a");
    CHECK(insert.run());
    CHECK_FALSE(insert.failed());
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM t"), 1);

    insert.reset();
    insert.bind_int(1, 1); // duplicate primary key
    insert.bind_text(2, "again");
    CHECK_FALSE(insert.run());
    CHECK(insert.failed());
    CHECK_EQ(insert.error_code(), SQLITE_CONSTRAINT);
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM t"), 1);

    // run() on a SELECT consumes the rows and succeeds
    Statement select = db.prepare("SELECT id FROM t");
    REQUIRE(select.valid());
    CHECK(select.run());
}

TEST(DbApi, ATriggerAbortIsAnErrorNotDone)
{
    // Port of the failure device of tests/integration/mdb/test_ownership_and_atomicity.py
    // (BEFORE DELETE trigger with RAISE(ABORT)).
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));
    REQUIRE(db.exec("INSERT INTO t (id, v) VALUES (1, 'a')"));
    REQUIRE(db.exec("CREATE TRIGGER blocked BEFORE DELETE ON t BEGIN SELECT RAISE(ABORT, 'blocked'); END"));

    Statement del = db.prepare("DELETE FROM t WHERE id = 1");
    REQUIRE(del.valid());
    CHECK_FALSE(del.run());
    CHECK(del.failed());
    CHECK(contains(del.error_message(), "blocked"));
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM t"), 1);
}

TEST(DbApi, AStatementThatFailedToPrepareCarriesItsError)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));

    Statement missing_table = db.prepare("SELECT * FROM no_such_table");
    CHECK_FALSE(missing_table.valid());
    CHECK(missing_table.failed());
    CHECK_EQ(missing_table.error_code(), SQLITE_ERROR);
    CHECK(contains(missing_table.error_message(), "no such table: no_such_table"));
    CHECK(missing_table.schema_mismatch());
    CHECK_EQ(db.last_error_code(), SQLITE_ERROR);
    CHECK(contains(db.last_error_message(), "no such table"));
    CHECK_FALSE(missing_table.step());
    CHECK(missing_table.step_result() == StepResult::Error);
    CHECK_FALSE(missing_table.run());

    Statement missing_column = db.prepare("SELECT nope FROM t");
    CHECK_FALSE(missing_column.valid());
    CHECK(missing_column.schema_mismatch());

    // a valid statement is never a schema mismatch
    Statement fine = db.prepare("SELECT id FROM t");
    CHECK(fine.valid());
    CHECK_FALSE(fine.schema_mismatch());
    CHECK_FALSE(fine.failed());
}

TEST(DbApi, EmptySqlIsNotMistakenForASuccessfulPrepare)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));

    Statement empty = db.prepare("");
    CHECK_FALSE(empty.valid());
    CHECK(empty.failed());
    CHECK_FALSE(empty.schema_mismatch()); // not a schema problem: callers must not skip it
    CHECK_FALSE(empty.run());

    Statement comment = db.prepare("-- nothing here");
    CHECK_FALSE(comment.valid());
    CHECK(comment.failed());
}

TEST(DbApi, PreparingOnAClosedDatabaseFails)
{
    OstDatabase db; // never opened
    Statement stmt = db.prepare("SELECT 1");
    CHECK_FALSE(stmt.valid());
    CHECK(stmt.failed());
    CHECK_EQ(stmt.error_code(), SQLITE_MISUSE);
    CHECK_FALSE(stmt.schema_mismatch());
    CHECK_FALSE(stmt.run());
    CHECK_FALSE(db.exec("SELECT 1"));
    CHECK_EQ(db.last_error_code(), SQLITE_MISUSE);
}

TEST(DbApi, ARejectedBindBlocksExecution)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));

    Statement insert = db.prepare("INSERT INTO t (id, v) VALUES (?, ?)");
    REQUIRE(insert.valid());
    insert.bind_int(1, 7);
    insert.bind_text(2, "x");
    insert.bind_text(3, "no such parameter"); // SQLITE_RANGE
    CHECK(insert.failed());
    CHECK_EQ(insert.error_code(), SQLITE_RANGE);
    CHECK_FALSE(insert.run());
    CHECK_FALSE(insert.step());
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM t"), 0); // never executed

    // reset() clears the failure; a correctly bound statement then runs
    insert.reset();
    CHECK_FALSE(insert.failed());
    CHECK_EQ(insert.error_code(), 0);
    insert.bind_int(1, 7);
    insert.bind_text(2, "x");
    CHECK(insert.run());
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM t"), 1);
}

TEST(DbApi, ResetClearsAStepFailure)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));
    REQUIRE(db.exec("INSERT INTO t (id, v) VALUES (1, 'a')"));

    Statement insert = db.prepare("INSERT INTO t (id, v) VALUES (?, 'b')");
    REQUIRE(insert.valid());
    insert.bind_int(1, 1);
    CHECK_FALSE(insert.run());
    CHECK(insert.failed());

    insert.reset();
    CHECK_FALSE(insert.failed());
    insert.bind_int(1, 2);
    CHECK(insert.run());
}

TEST(DbApi, AMovedStatementKeepsItsFailure)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));

    Statement missing = db.prepare("SELECT * FROM no_such_table");
    Statement moved(std::move(missing));
    CHECK_FALSE(moved.valid());
    CHECK(moved.failed());
    CHECK(moved.schema_mismatch());

    Statement bad = db.prepare("INSERT INTO t (id, v) VALUES (1, NULL)");
    CHECK_FALSE(bad.run());
    Statement assigned;
    assigned = std::move(bad);
    CHECK(assigned.failed());
    CHECK_EQ(assigned.error_code(), SQLITE_CONSTRAINT);
}

TEST(DbApi, ExecReportsItsFailure)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));

    CHECK_FALSE(db.exec("INSERT INTO nope VALUES (1)"));
    CHECK_EQ(db.last_error_code(), SQLITE_ERROR);
    CHECK(contains(db.last_error_message(), "no such table: nope"));

    CHECK_FALSE(db.exec("INSERT INTO t (id, v) VALUES (1, NULL)"));
    CHECK_EQ(db.last_error_code(), SQLITE_CONSTRAINT);

    CHECK(db.exec("INSERT INTO t (id, v) VALUES (1, 'ok')"));
}

TEST(DbApi, AFailedCommitNeverLeavesTheTransactionOpen)
{
    // A deferred foreign key is only checked at COMMIT (open() turns foreign_keys on).
    OstDatabase db;
    REQUIRE(db.open(":memory:"));
    REQUIRE(db.exec("CREATE TABLE parent (id INTEGER PRIMARY KEY)"));
    REQUIRE(db.exec("CREATE TABLE child (pid INTEGER REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED)"));

    REQUIRE(db.begin_transaction());
    CHECK(db.in_transaction());
    REQUIRE(db.exec("INSERT INTO child VALUES (99)")); // fine until COMMIT
    CHECK_FALSE(db.commit());
    CHECK_EQ(db.last_error_code(), SQLITE_CONSTRAINT);
    CHECK_FALSE(db.in_transaction());                  // rolled back by commit()
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM child"), 0);

    // the connection is usable again
    REQUIRE(db.begin_transaction());
    REQUIRE(db.exec("INSERT INTO parent VALUES (1)"));
    REQUIRE(db.exec("INSERT INTO child VALUES (1)"));
    CHECK(db.commit());
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM child"), 1);
}

TEST(DbApi, CommitWithoutATransactionFails)
{
    OstDatabase db;
    REQUIRE(db.open(":memory:"));
    CHECK_FALSE(db.in_transaction());
    CHECK_FALSE(db.commit());
}

TEST(DbApi, InTransactionTracksBeginCommitAndRollback)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));

    CHECK_FALSE(db.in_transaction());
    REQUIRE(db.begin_transaction());
    CHECK(db.in_transaction());
    REQUIRE(db.exec("INSERT INTO t (id, v) VALUES (1, 'a')"));
    CHECK(db.rollback());
    CHECK_FALSE(db.in_transaction());
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM t"), 0);

    REQUIRE(db.begin_transaction());
    REQUIRE(db.exec("INSERT INTO t (id, v) VALUES (1, 'a')"));
    CHECK(db.commit());
    CHECK_FALSE(db.in_transaction());
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM t"), 1);
}

// RAISE(ABORT) undoes only the failing statement; the surrounding transaction stays open (this is what lets Windows'
// best-effort ConditionSetStyles cleanup carry on). RAISE(ROLLBACK) ends the transaction itself.
TEST(DbApi, AFailedStatementKeepsTheTransactionOpenUnlessItRolledBack)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));
    REQUIRE(db.exec("CREATE TABLE guarded (id INTEGER)"));
    REQUIRE(db.exec("CREATE TABLE harsh (id INTEGER)"));
    REQUIRE(db.exec("CREATE TRIGGER t_guarded BEFORE INSERT ON guarded BEGIN SELECT RAISE(ABORT, 'no'); END"));
    REQUIRE(db.exec("CREATE TRIGGER t_harsh BEFORE INSERT ON harsh BEGIN SELECT RAISE(ROLLBACK, 'no'); END"));

    REQUIRE(db.begin_transaction());
    REQUIRE(db.exec("INSERT INTO t (id, v) VALUES (1, 'kept')"));
    Statement abort_insert = db.prepare("INSERT INTO guarded VALUES (1)");
    REQUIRE(abort_insert.valid());
    CHECK_FALSE(abort_insert.run());
    CHECK(db.in_transaction());
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM t"), 1); // earlier work of the transaction is still there

    Statement rollback_insert = db.prepare("INSERT INTO harsh VALUES (1)");
    REQUIRE(rollback_insert.valid());
    CHECK_FALSE(rollback_insert.run());
    CHECK_FALSE(db.in_transaction());                  // the failure ended the transaction
    CHECK_EQ(scalar(db, "SELECT COUNT(*) FROM t"), 0);
}

TEST(DbApi, TableAndColumnExistMirrorTheWindowsSchemaInspector)
{
    OstDatabase db;
    REQUIRE(open_memory_with_table(db));
    REQUIRE(db.exec("CREATE VIEW v_t AS SELECT id FROM t"));

    CHECK(db.table_exists("t"));
    CHECK(db.table_exists("T")); // names compare case-insensitively
    CHECK_FALSE(db.table_exists("missing"));
    CHECK_FALSE(db.table_exists("v_t")); // a view is not a table
    CHECK_FALSE(db.table_exists(""));

    CHECK(db.column_exists("t", "id"));
    CHECK(db.column_exists("T", "V"));
    CHECK_FALSE(db.column_exists("t", "missing"));
    CHECK_FALSE(db.column_exists("missing", "id"));
}

TEST(DbApi, ConnectionsWaitForALockByDefault)
{
    OstDatabase db;
    REQUIRE(db.open(":memory:"));
    CHECK_EQ(OstDatabase::kBusyTimeoutMs, 5000);
    CHECK_EQ(scalar(db, "PRAGMA busy_timeout"), OstDatabase::kBusyTimeoutMs);
}

TEST(DbApi, AWriteFailsWithBusyWhenAnotherConnectionKeepsTheWriteLock)
{
    TempFile file("persistence_dbapi_busy_fail.db");
    OstDatabase holder, writer;
    REQUIRE(holder.open(file.path));
    REQUIRE(writer.open(file.path));
    REQUIRE(holder.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT)"));
    REQUIRE(writer.exec("PRAGMA busy_timeout=50")); // keep the test fast; the default is kBusyTimeoutMs

    REQUIRE(holder.exec("BEGIN IMMEDIATE"));

    Statement insert = writer.prepare("INSERT INTO t (id, v) VALUES (1, 'w')");
    REQUIRE(insert.valid());
    CHECK_FALSE(insert.run());
    CHECK(insert.failed());
    CHECK_EQ(insert.error_code(), SQLITE_BUSY);

    REQUIRE(holder.rollback());
    insert.reset();
    CHECK(insert.run()); // lock released: the same statement now succeeds
    CHECK_EQ(scalar(writer, "SELECT COUNT(*) FROM t"), 1);
}

TEST(DbApi, AWriteWaitsForTheLockInsteadOfFailingAtOnce)
{
    TempFile file("persistence_dbapi_busy_wait.db");
    OstDatabase holder, writer;
    REQUIRE(holder.open(file.path));
    REQUIRE(writer.open(file.path));
    REQUIRE(holder.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT)"));
    REQUIRE(writer.exec("PRAGMA busy_timeout=5000"));

    REQUIRE(holder.exec("BEGIN IMMEDIATE"));
    REQUIRE(holder.exec("INSERT INTO t (id, v) VALUES (1, 'holder')"));

    std::thread releaser([&holder]()
                         {
                             std::this_thread::sleep_for(std::chrono::milliseconds(200));
                             holder.commit();
                         });

    Statement insert = writer.prepare("INSERT INTO t (id, v) VALUES (2, 'writer')");
    REQUIRE(insert.valid());
    const bool ran = insert.run(); // blocks until the holder commits, then succeeds
    releaser.join();

    CHECK(ran);
    CHECK_FALSE(insert.failed());
    CHECK_EQ(scalar(writer, "SELECT COUNT(*) FROM t"), 2);
}

// begin_transaction() takes the write lock at BEGIN. A DEFERRED transaction that first reads and writes later would fail
// with SQLITE_BUSY at once (the busy timeout is not consulted) when another connection committed in between; with the
// lock held from BEGIN no other writer can get in, so a read-then-write transaction (delete_project, delete_takeoff,
// delete_condition, import_project ...) cannot lose that race.
TEST(DbApi, BeginTransactionTakesTheWriteLockUpFront)
{
    TempFile file("persistence_dbapi_begin_lock.db");
    OstDatabase tx, other;
    REQUIRE(tx.open(file.path));
    REQUIRE(other.open(file.path));
    REQUIRE(tx.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT)"));
    REQUIRE(other.exec("PRAGMA busy_timeout=50")); // keep the test fast

    REQUIRE(tx.begin_transaction());
    CHECK_EQ(scalar(tx, "SELECT COUNT(*) FROM t"), 0); // read first, write later

    Statement insert = other.prepare("INSERT INTO t (id, v) VALUES (1, 'other')");
    REQUIRE(insert.valid());
    CHECK_FALSE(insert.run()); // the transaction already owns the write lock although it has not written yet
    CHECK_EQ(insert.error_code(), SQLITE_BUSY);

    Statement own = tx.prepare("INSERT INTO t (id, v) VALUES (2, 'tx')");
    REQUIRE(own.valid());
    CHECK(own.run());
    CHECK(tx.commit());

    insert.reset();
    CHECK(insert.run()); // lock released
    CHECK_EQ(scalar(other, "SELECT COUNT(*) FROM t"), 2);
}
