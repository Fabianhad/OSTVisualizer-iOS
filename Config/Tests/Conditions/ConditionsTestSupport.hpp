// ConditionsTestSupport.hpp - shared helpers for the Conditions host tests.
//
// ProjectStore opens its own sqlite connection on every call, so the tests drive it through a database FILE in the system
// temp dir (never in the repo). TempDb owns such a file and offers raw-SQL helpers that open a short-lived connection.
#pragma once

#include "TestHarness.hpp"

#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore.hpp"

#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace ost_ctest
{
    // The temp dir is shared by every test run on the machine (and by concurrent runs of other areas): each process puts
    // its database files under a token of its own.
    inline const std::string &run_token()
    {
        static const std::string token = []
        {
            std::random_device device;
            char buffer[16];
            std::snprintf(buffer, sizeof buffer, "%08x", static_cast<unsigned>(device()));
            return std::string(buffer);
        }();
        return token;
    }

    class TempDb
    {
    public:
        // with_schema=true runs ost_store::create_schema (the iOS schema: every table, the 'Deleted Bids' project 1).
        // with_schema=false gives an empty database for the tests that build a minimal schema of their own (the Windows
        // tests create just the tables and columns they need).
        explicit TempDb(const std::string &name, bool with_schema = true)
            : _path(ost_test::temp_path("conditions_" + run_token() + "_" + name + ".db"))
        {
            remove_files();
            if (with_schema)
                _schema_ok = ost_store::create_schema(_path);
        }

        ~TempDb() { remove_files(); }

        TempDb(const TempDb &) = delete;
        TempDb &operator=(const TempDb &) = delete;

        const std::string &path() const { return _path; }
        bool schema_ok() const { return _schema_ok; }

        bool exec(const std::string &sql) const
        {
            ost_db::OstDatabase db;
            return db.open(_path) && db.exec(sql);
        }

        // exec() that aborts the test when the statement fails (fixture setup).
        void must(const std::string &sql) const
        {
            if (!exec(sql))
                FAIL_TEST("fixture SQL failed: " + sql);
        }

        // First column of the first row as int; -1 when the query failed or returned no row.
        long long count(const std::string &sql) const
        {
            ost_db::OstDatabase db;
            if (!db.open(_path))
                return -1;
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid() || !stmt.step())
                return -1;
            return stmt.column_int64(0);
        }

        // First column of the first row as text; "<none>" when the query failed or returned no row.
        std::string text(const std::string &sql) const
        {
            ost_db::OstDatabase db;
            if (!db.open(_path))
                return "<none>";
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid() || !stmt.step())
                return "<none>";
            return stmt.column_text(0);
        }

        // "a,b,c" of the first column of every row ("<failed>" when the query failed): lets a test compare a whole column
        // of UIDs in one CHECK_EQ.
        std::string list(const std::string &sql) const
        {
            ost_db::OstDatabase db;
            if (!db.open(_path))
                return "<cannot open>";
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid())
                return "<failed>";
            std::string out;
            while (stmt.step())
            {
                if (!out.empty())
                    out += ",";
                out += stmt.column_type(0) == ost_db::Statement::TypeNull ? "NULL" : stmt.column_text(0);
            }
            return stmt.failed() ? "<failed>" : out;
        }

        // UIDs of a table in ascending order, optionally filtered ("WHERE ...").
        std::string uids(const std::string &table, const std::string &where = "") const
        {
            return list("SELECT UID FROM [" + table + "] " + where + " ORDER BY UID");
        }

        // One row as "col=quote(value) ..." for every column except `skip` (case-sensitive names), "<none>" when the row
        // does not exist. Exact (type-aware) before/after comparison of a row.
        std::string row(const std::string &table, const std::string &uid, const std::vector<std::string> &skip = {}) const
        {
            std::vector<std::string> columns;
            {
                ost_db::OstDatabase db;
                if (!db.open(_path))
                    return "<cannot open>";
                ost_db::Statement info = db.prepare("SELECT name FROM pragma_table_info(?)");
                if (!info.valid())
                    return "<pragma failed>";
                info.bind_text(1, table);
                while (info.step())
                    columns.push_back(info.column_text(0));
            }
            std::string expr;
            for (const std::string &column : columns)
            {
                bool skipped = false;
                for (const std::string &s : skip)
                    skipped = skipped || s == column;
                if (skipped)
                    continue;
                if (!expr.empty())
                    expr += " || ' ' || ";
                expr += "'" + column + "=' || quote([" + column + "])";
            }
            if (expr.empty())
                return "<no columns>";
            return text("SELECT " + expr + " FROM [" + table + "] WHERE UID = " + uid);
        }

        // Whole table (rowid order) as text, "<missing>" when the table does not exist: exact whole-table comparison.
        std::string dump(const std::string &table) const
        {
            std::vector<std::string> columns;
            {
                ost_db::OstDatabase db;
                if (!db.open(_path))
                    return "<cannot open>";
                ost_db::Statement info = db.prepare("SELECT name FROM pragma_table_info(?)");
                if (!info.valid())
                    return "<pragma failed>";
                info.bind_text(1, table);
                while (info.step())
                    columns.push_back(info.column_text(0));
            }
            if (columns.empty())
                return "<missing>";
            std::string expr = "rowid";
            for (const std::string &column : columns)
                expr += " || ' " + column + "=' || quote([" + column + "])";
            return list("SELECT " + expr + " FROM [" + table + "] ORDER BY rowid");
        }

        std::string snapshot(const std::vector<std::string> &tables) const
        {
            std::string out;
            for (const std::string &table : tables)
                out += "[" + table + "]\n" + dump(table) + "\n";
            return out;
        }

        // CREATE TRIGGER that makes every <event> ("INSERT" / "UPDATE" / "DELETE") on <table> fail with RAISE(ABORT).
        std::string block(const std::string &table, const std::string &event)
        {
            const std::string name = "blk_" + std::to_string(_trigger_seq++) + "_" + event + "_" + table;
            must("CREATE TRIGGER [" + name + "] BEFORE " + event + " ON [" + table +
                 "] BEGIN SELECT RAISE(ABORT, 'blocked'); END");
            return name;
        }

        void unblock(const std::string &trigger_name) const { must("DROP TRIGGER [" + trigger_name + "]"); }

    private:
        void remove_files() const
        {
            std::remove(_path.c_str());
            std::remove((_path + "-wal").c_str());
            std::remove((_path + "-shm").c_str());
            std::remove((_path + "-journal").c_str());
        }

        std::string _path;
        bool _schema_ok = false;
        int _trigger_seq = 0;
    };
}
