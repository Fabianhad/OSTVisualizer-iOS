// SettingsTestSupport.hpp - shared helpers for the Settings host tests (render config, undo snapshots, camera limits).
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

namespace ost_stest
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
        // with_schema=false gives an empty database file (no _AppPreferences table).
        explicit TempDb(const std::string &name, bool with_schema = true)
            : _path(ost_test::temp_path("settings_" + run_token() + "_" + name + ".db"))
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

        // Runs raw SQL on a fresh connection; false when it failed.
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
        int count(const std::string &sql) const
        {
            ost_db::OstDatabase db;
            if (!db.open(_path))
                return -1;
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid() || !stmt.step())
                return -1;
            return stmt.column_int(0);
        }

        // First column of the first row as text; "<none>" when the query failed or returned no row (a NULL column reads as "").
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

        // Stored value of one _AppPreferences key; "<missing>" when the row (or the table) does not exist.
        std::string pref(const std::string &key) const
        {
            return text("SELECT Value FROM _AppPreferences WHERE Key = '" + key + "'");
        }

        // "<NULL>" for a NULL column, else the SQL-quoted value (quote()), of one column of one BidTakeoffs row.
        std::string takeoff_cell(long long uid, const std::string &column) const
        {
            return text("SELECT quote([" + column + "]) FROM BidTakeoffs WHERE UID = " + std::to_string(uid));
        }

        // A small world on top of create_schema (project 1 = 'Deleted Bids'): bid 10 owns page 100, area 60, conditions 30
        // and 31 and no takeoffs; bid 11 owns page 101 and condition 32 (the "other bid" of the cross-bid cases).
        void seed_world() const
        {
            must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
            must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (10, 1, 'Job A', 10)");
            must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (11, 2, 'Job B', 11)");
            must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (100, 10, 'P1')");
            must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (101, 11, 'P2')");
            must("INSERT INTO BidAreas (UID, BidUID, Name) VALUES (60, 10, 'A1')");
            must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (30, 10, 1, 'Cdn', 0)");
            must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (31, 10, 2, 'Cdn2', 0)");
            must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (32, 11, 1, 'Other', 0)");
        }

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
    };
}
