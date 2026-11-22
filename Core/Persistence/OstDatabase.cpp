#include "OstDatabase.hpp"
#include "sqlite3.h"
#include <utility>

namespace ost_db
{

    Statement::~Statement()
    {
        if (_stmt)
            sqlite3_finalize(_stmt);
    }

    Statement::Statement(Statement &&other) noexcept
        : _stmt(other._stmt),
          _rc(other._rc),
          _bind_failed(other._bind_failed),
          _error(std::move(other._error))
    {
        other._stmt = nullptr;
        other._rc = 0;
        other._bind_failed = false;
    }

    Statement &Statement::operator=(Statement &&other) noexcept
    {
        if (this != &other)
        {
            if (_stmt)
                sqlite3_finalize(_stmt);
            _stmt = other._stmt;
            _rc = other._rc;
            _bind_failed = other._bind_failed;
            _error = std::move(other._error);
            other._stmt = nullptr;
            other._rc = 0;
            other._bind_failed = false;
        }
        return *this;
    }

    void Statement::note_failure(int rc)
    {
        _rc = rc;
        _error = _stmt ? sqlite3_errmsg(sqlite3_db_handle(_stmt)) : "statement is not valid";
    }

    void Statement::note_bind(int rc)
    {
        if (rc == SQLITE_OK || _bind_failed)
            return;
        _bind_failed = true;
        note_failure(rc);
    }

    void Statement::bind_int(int idx, int value)
    {
        if (_stmt)
            note_bind(sqlite3_bind_int(_stmt, idx, value));
    }

    void Statement::bind_int64(int idx, long long value)
    {
        if (_stmt)
            note_bind(sqlite3_bind_int64(_stmt, idx, value));
    }

    void Statement::bind_double(int idx, double value)
    {
        if (_stmt)
            note_bind(sqlite3_bind_double(_stmt, idx, value));
    }

    void Statement::bind_text(int idx, const std::string &value)
    {
        if (_stmt)
            note_bind(sqlite3_bind_text(_stmt, idx, value.c_str(),
                                        static_cast<int>(value.size()), SQLITE_TRANSIENT));
    }

    void Statement::bind_null(int idx)
    {
        if (_stmt)
            note_bind(sqlite3_bind_null(_stmt, idx));
    }

    StepResult Statement::step_result()
    {
        if (!_stmt)
        {
            // A statement that failed to prepare keeps its prepare error; a default-constructed one has none yet.
            if (_rc == 0)
                note_failure(SQLITE_MISUSE);
            return StepResult::Error;
        }
        if (_bind_failed)
            return StepResult::Error;

        int rc = sqlite3_step(_stmt);
        if (rc == SQLITE_ROW)
            return StepResult::Row;
        if (rc == SQLITE_DONE)
            return StepResult::Done;
        note_failure(rc);
        return StepResult::Error;
    }

    bool Statement::step()
    {
        return step_result() == StepResult::Row;
    }

    bool Statement::run()
    {
        for (;;)
        {
            StepResult result = step_result();
            if (result == StepResult::Error)
                return false;
            if (result == StepResult::Done)
                return true;
        }
    }

    bool Statement::schema_mismatch() const
    {
        return !_stmt && _rc == SQLITE_ERROR;
    }

    void Statement::reset()
    {
        if (_stmt)
        {
            sqlite3_reset(_stmt);
            sqlite3_clear_bindings(_stmt);
            _rc = 0;
            _bind_failed = false;
            _error.clear();
        }
    }

    int Statement::column_int(int idx) const
    {
        return _stmt ? sqlite3_column_int(_stmt, idx) : 0;
    }

    long long Statement::column_int64(int idx) const
    {
        return _stmt ? sqlite3_column_int64(_stmt, idx) : 0;
    }

    Statement::ColumnType Statement::column_type(int idx) const
    {
        return _stmt ? static_cast<ColumnType>(sqlite3_column_type(_stmt, idx)) : TypeNull;
    }

    double Statement::column_double(int idx) const
    {
        return _stmt ? sqlite3_column_double(_stmt, idx) : 0.0;
    }

    std::string Statement::column_text(int idx) const
    {
        if (!_stmt)
            return "";
        const unsigned char *txt = sqlite3_column_text(_stmt, idx);
        if (!txt)
            return "";
        return reinterpret_cast<const char *>(txt);
    }

    std::string Statement::column_text_exact(int idx) const
    {
        if (!_stmt)
            return "";
        const void *data = sqlite3_column_type(_stmt, idx) == SQLITE_BLOB ? sqlite3_column_blob(_stmt, idx)
                                                                         : static_cast<const void *>(sqlite3_column_text(_stmt, idx));
        const int bytes = sqlite3_column_bytes(_stmt, idx);
        if (!data || bytes <= 0)
            return "";
        return std::string(static_cast<const char *>(data), static_cast<std::size_t>(bytes));
    }

    OstDatabase::~OstDatabase()
    {
        close();
    }

    void OstDatabase::note_error(int rc, const char *message)
    {
        _last_error_code = rc;
        _last_error = message ? message : "";
    }

    bool OstDatabase::open(const std::string &path)
    {
        close();
        _last_error_code = 0;
        _last_error.clear();
        int rc = sqlite3_open(path.c_str(), &_db);
        if (rc != SQLITE_OK)
        {
            note_error(rc, _db ? sqlite3_errmsg(_db) : "out of memory");
            if (_db)
            {
                sqlite3_close(_db);
                _db = nullptr;
            }
            return false;
        }
        // Before the first statement, so even the PRAGMAs below wait for a lock instead of failing.
        sqlite3_busy_timeout(_db, kBusyTimeoutMs);
        // Best effort: a database that cannot switch to WAL (or enforce foreign keys) still works.
        exec("PRAGMA journal_mode=WAL");
        exec("PRAGMA foreign_keys=ON");
        _last_error_code = 0;
        _last_error.clear();
        return true;
    }

    void OstDatabase::close()
    {
        if (_db)
        {
            sqlite3_close(_db);
            _db = nullptr;
        }
    }

    bool OstDatabase::exec(const std::string &sql)
    {
        if (!_db)
        {
            note_error(SQLITE_MISUSE, "database is not open");
            return false;
        }
        char *err = nullptr;
        int rc = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, &err);
        if (rc != SQLITE_OK)
            note_error(rc, err ? err : sqlite3_errmsg(_db));
        if (err)
            sqlite3_free(err);
        return rc == SQLITE_OK;
    }

    Statement OstDatabase::prepare(const std::string &sql)
    {
        Statement stmt;
        if (!_db)
        {
            note_error(SQLITE_MISUSE, "database is not open");
            stmt.note_failure(SQLITE_MISUSE);
            stmt._error = _last_error;
            return stmt;
        }
        int rc = sqlite3_prepare_v2(_db, sql.c_str(),
                                    static_cast<int>(sql.size()), &stmt._stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            // sqlite leaves *ppStmt NULL on failure, so the statement is invalid; keep the reason with it.
            stmt._rc = rc;
            stmt._error = sqlite3_errmsg(_db);
            note_error(rc, stmt._error.c_str());
        }
        else if (!stmt._stmt)
        {
            // Empty SQL or only a comment: nothing to run, which a caller asking for a statement must not mistake for success.
            stmt._rc = SQLITE_MISUSE;
            stmt._error = "empty SQL statement";
            note_error(SQLITE_MISUSE, stmt._error.c_str());
        }
        return stmt;
    }

    bool OstDatabase::begin_transaction()
    {
        // IMMEDIATE: take the write lock up front, so the busy timeout makes a second writer wait. A DEFERRED
        // transaction that reads first and writes later fails at once with SQLITE_BUSY (the busy timeout is not
        // used) when another connection committed in between, which Windows' serialized writers never do.
        return exec("BEGIN IMMEDIATE TRANSACTION");
    }

    bool OstDatabase::commit()
    {
        if (exec("COMMIT"))
            return true;
        // A failed COMMIT (deferred constraint, lock timeout) can leave the transaction open. Roll it back so nothing
        // half-applied survives, and keep the COMMIT error as the one reported.
        const int code = _last_error_code;
        const std::string message = _last_error;
        if (in_transaction())
            exec("ROLLBACK");
        _last_error_code = code;
        _last_error = message;
        return false;
    }

    bool OstDatabase::rollback()
    {
        return exec("ROLLBACK");
    }

    bool OstDatabase::in_transaction() const
    {
        return _db && sqlite3_get_autocommit(_db) == 0;
    }

    bool OstDatabase::table_exists(const std::string &table)
    {
        Statement stmt = prepare(
            "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? COLLATE NOCASE");
        if (!stmt.valid())
            return false;
        stmt.bind_text(1, table);
        return stmt.step();
    }

    bool OstDatabase::column_exists(const std::string &table, const std::string &column)
    {
        Statement stmt = prepare(
            "SELECT 1 FROM pragma_table_info(?) WHERE name = ? COLLATE NOCASE");
        if (!stmt.valid())
            return false;
        stmt.bind_text(1, table);
        stmt.bind_text(2, column);
        return stmt.step();
    }

}
