#include "OstDatabase.hpp"
#include "sqlite3.h"

namespace ost_db
{

    Statement::~Statement()
    {
        if (_stmt)
            sqlite3_finalize(_stmt);
    }

    Statement::Statement(Statement &&other) noexcept
        : _stmt(other._stmt)
    {
        other._stmt = nullptr;
    }

    Statement &Statement::operator=(Statement &&other) noexcept
    {
        if (this != &other)
        {
            if (_stmt)
                sqlite3_finalize(_stmt);
            _stmt = other._stmt;
            other._stmt = nullptr;
        }
        return *this;
    }

    void Statement::bind_int(int idx, int value)
    {
        if (_stmt)
            sqlite3_bind_int(_stmt, idx, value);
    }

    void Statement::bind_double(int idx, double value)
    {
        if (_stmt)
            sqlite3_bind_double(_stmt, idx, value);
    }

    void Statement::bind_text(int idx, const std::string &value)
    {
        if (_stmt)
            sqlite3_bind_text(_stmt, idx, value.c_str(),
                              static_cast<int>(value.size()), SQLITE_TRANSIENT);
    }

    void Statement::bind_null(int idx)
    {
        if (_stmt)
            sqlite3_bind_null(_stmt, idx);
    }

    bool Statement::step()
    {
        if (!_stmt)
            return false;
        int rc = sqlite3_step(_stmt);
        return rc == SQLITE_ROW;
    }

    void Statement::reset()
    {
        if (_stmt)
        {
            sqlite3_reset(_stmt);
            sqlite3_clear_bindings(_stmt);
        }
    }

    int Statement::column_int(int idx) const
    {
        return _stmt ? sqlite3_column_int(_stmt, idx) : 0;
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

    OstDatabase::~OstDatabase()
    {
        close();
    }

    bool OstDatabase::open(const std::string &path)
    {
        close();
        int rc = sqlite3_open(path.c_str(), &_db);
        if (rc != SQLITE_OK)
        {
            if (_db)
            {
                sqlite3_close(_db);
                _db = nullptr;
            }
            return false;
        }
        exec("PRAGMA journal_mode=WAL");
        exec("PRAGMA foreign_keys=ON");
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
            return false;
        char *err = nullptr;
        int rc = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, &err);
        if (err)
            sqlite3_free(err);
        return rc == SQLITE_OK;
    }

    Statement OstDatabase::prepare(const std::string &sql)
    {
        Statement stmt;
        if (_db)
            sqlite3_prepare_v2(_db, sql.c_str(),
                               static_cast<int>(sql.size()), &stmt._stmt, nullptr);
        return stmt;
    }

    bool OstDatabase::begin_transaction()
    {
        return exec("BEGIN TRANSACTION");
    }

    bool OstDatabase::commit()
    {
        return exec("COMMIT");
    }

    bool OstDatabase::rollback()
    {
        return exec("ROLLBACK");
    }

}
