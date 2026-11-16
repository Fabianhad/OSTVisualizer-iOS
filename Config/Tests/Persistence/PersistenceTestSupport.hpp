// PersistenceTestSupport.hpp - shared helpers for the Persistence host tests.
//
// ProjectStore opens its own sqlite connection on every call, so a test that drives ProjectStore needs a database FILE
// (":memory:" would give every call an empty private database). TempDb owns such a file in the system temp dir (never in
// the repo) and offers raw-SQL helpers that open a short-lived connection of their own.
#pragma once

#include "TestHarness.hpp"

#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore.hpp"

#include <string>
#include <vector>

namespace ost_ptest
{
    class TempDb
    {
    public:
        // with_schema=true runs ost_store::create_schema (all tables, the 'Deleted Bids' project with UID 1, default layers).
        explicit TempDb(const std::string &name, bool with_schema = true)
            : _path(ost_test::temp_path("persistence_" + name + ".db"))
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

        // Whole table as text (rows in rowid order, non-NULL columns in declaration order, values SQL-quoted), for exact
        // before/after comparisons. "<missing>" when the table does not exist.
        std::string dump(const std::string &table) const
        {
            ost_db::OstDatabase db;
            if (!db.open(_path))
                return "<cannot open>";
            std::vector<std::string> columns;
            {
                ost_db::Statement info = db.prepare("SELECT name FROM pragma_table_info(?)");
                if (!info.valid())
                    return "<pragma failed>";
                info.bind_text(1, table);
                while (info.step())
                    columns.push_back(info.column_text(0));
            }
            if (columns.empty())
                return "<missing>";
            // "rowid: col=value col=value" - NULL columns are left out so a failure message stays readable.
            std::string row_expr = "rowid || ':'";
            for (const std::string &column : columns)
                row_expr += " || CASE WHEN [" + column + "] IS NULL THEN '' ELSE ' " + column + "=' || quote([" +
                            column + "]) END";
            ost_db::Statement rows = db.prepare(
                "SELECT group_concat(r, char(10)) FROM (SELECT " + row_expr + " AS r FROM [" + table +
                "] ORDER BY rowid)");
            if (!rows.valid() || !rows.step())
                return "<select failed>";
            return rows.column_text(0);
        }

        // dump() of several tables in one string.
        std::string snapshot(const std::vector<std::string> &tables) const
        {
            std::string out;
            for (const std::string &table : tables)
                out += "[" + table + "]\n" + dump(table) + "\n";
            return out;
        }

        // CREATE TRIGGER that makes every <event> ("INSERT" / "UPDATE" / "DELETE") on <table> fail with RAISE(ABORT).
        // Returns the trigger name so a test can DROP it again (control run).
        std::string block(const std::string &table, const std::string &event)
        {
            const std::string name = "blk_" + std::to_string(_trigger_seq++) + "_" + event + "_" + table;
            must("CREATE TRIGGER [" + name + "] BEFORE " + event + " ON [" + table +
                 "] BEGIN SELECT RAISE(ABORT, 'blocked'); END");
            return name;
        }

        void unblock(const std::string &trigger_name) const { must("DROP TRIGGER [" + trigger_name + "]"); }

        // A small bid world on top of create_schema (project 1 = 'Deleted Bids'):
        //   BidProjects 2 'Folder A'
        //   Bid 10 in 'Deleted Bids' (project 1)   Bid 11 in 'Folder A' (project 2)
        //   Bid 10 owns: page folder 90, page 100, condition folder 80, layer 70, area 60,
        //                conditions 30 (RefNo 1) and 31 (RefNo 2), takeoffs 500 (cond 30) and 501 (cond 31),
        //                dimension 1000 (500 -> 501), labor activity 3000 (cond 30), percent 2000 (activity 3000, takeoff 500),
        //                condition style 4000 (ConditionStyleUID 30)
        void seed_world() const
        {
            must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
            must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (10, 1, 'Deleted job', 10)");
            must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (11, 2, 'Live job', 11)");
            must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (90, 10, 'pf')");
            must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (100, 10, 'P1')");
            must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (80, 10, 'cf')");
            must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (70, 10, 'L1', -1, 1)");
            must("INSERT INTO BidAreas (UID, BidUID, Name) VALUES (60, 10, 'A1')");
            must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (30, 10, 1, 'Cdn', 0)");
            must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (31, 10, 2, 'Cdn2', 0)");
            must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) "
                 "VALUES (500, 10, 30, 100, '1;2')");
            must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) "
                 "VALUES (501, 10, 31, 100, '3;4')");
            must("INSERT INTO BidDimensions (UID, BidUID, BidPageUID, BidTakeoffFromUID, BidTakeoffToUID) "
                 "VALUES (1000, 10, 100, 500, 501)");
            must("INSERT INTO BidLaborActivity (UID, BidUID, BidConditionUID) VALUES (3000, 10, 30)");
            must("INSERT INTO BidPercents (UID, BidLaborActivityUID, BidTakeoffUID) VALUES (2000, 3000, 500)");
            must("INSERT INTO ConditionSetStyles (UID, ConditionSetUID, ConditionStyleUID) VALUES (4000, 1, 30)");
        }

        // Every table seed_world() fills (plus BidProjects), for whole-database before/after comparisons.
        static const std::vector<std::string> &world_tables()
        {
            static const std::vector<std::string> tables = {
                "BidProjects", "Bids", "BidPageFolders", "BidPages", "BidConditionFolders", "BidLayers",
                "BidAreas", "BidConditions", "BidTakeoffs", "BidDimensions", "BidLaborActivity",
                "BidPercents", "ConditionSetStyles"};
            return tables;
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
        int _trigger_seq = 0;
    };
}
