// ImportTestSupport.hpp - shared helpers for the Import host tests.
//
// The importer runs against a SQLite FILE (ProjectStore opens its own connection per call), so a test that drives it
// needs a database file. ScratchDb owns one in the system temp dir (never in the repo). It starts EMPTY (the Windows
// tests build small hand-written schemas, e.g. tests/helpers/mdb/import_export_support.py _create_import_schema) or
// with the real iOS schema (create_schema).
#pragma once

#include "TestHarness.hpp"

#include "Import/OstXmlParser.hpp"
#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace ost_itest
{
    class ScratchDb
    {
    public:
        explicit ScratchDb(const std::string &name, bool with_ios_schema = false)
            : _path(ost_test::temp_path("import_" + name + ".db"))
        {
            remove_files();
            if (with_ios_schema)
                _schema_ok = ost_store::create_schema(_path);
        }

        ~ScratchDb() { remove_files(); }

        ScratchDb(const ScratchDb &) = delete;
        ScratchDb &operator=(const ScratchDb &) = delete;

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

        // Runs several statements, each must succeed.
        void must_all(const std::vector<std::string> &statements) const
        {
            for (const std::string &sql : statements)
                must(sql);
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

        // Every row of the query as "c1|c2|c3" lines joined with ';' (NULL prints as NULL, text as is). "<failed>" when
        // the query could not run.
        std::string rows(const std::string &sql, int columns) const
        {
            ost_db::OstDatabase db;
            if (!db.open(_path))
                return "<cannot open>";
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid())
                return "<failed: " + stmt.error_message() + ">";
            std::string out;
            bool first_row = true;
            while (stmt.step())
            {
                if (!first_row)
                    out += ";";
                first_row = false;
                for (int c = 0; c < columns; ++c)
                {
                    if (c)
                        out += "|";
                    out += stmt.column_type(c) == ost_db::Statement::TypeNull ? std::string("NULL") : stmt.column_text(c);
                }
            }
            if (stmt.failed())
                return "<failed: " + stmt.error_message() + ">";
            return out;
        }

        // Whole table as text (rowid order, non-NULL columns in declaration order, values SQL-quoted) for exact
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
            std::string row_expr = "rowid || ':'";
            for (const std::string &column : columns)
                row_expr += " || CASE WHEN [" + column + "] IS NULL THEN '' ELSE ' " + column + "=' || quote([" + column +
                            "]) END";
            ost_db::Statement rows_stmt = db.prepare(
                "SELECT group_concat(r, char(10)) FROM (SELECT " + row_expr + " AS r FROM [" + table + "] ORDER BY rowid)");
            if (!rows_stmt.valid() || !rows_stmt.step())
                return "<select failed>";
            return rows_stmt.column_text(0);
        }

        std::string snapshot(const std::vector<std::string> &tables) const
        {
            std::string out;
            for (const std::string &table : tables)
                out += "[" + table + "]\n" + dump(table) + "\n";
            return out;
        }

        // BEFORE <event> trigger that makes every INSERT/UPDATE/DELETE on <table> fail with RAISE(ABORT).
        void block(const std::string &table, const std::string &event)
        {
            must("CREATE TRIGGER [blk_" + std::to_string(_trigger_seq++) + "_" + event + "_" + table + "] BEFORE " + event +
                 " ON [" + table + "] BEGIN SELECT RAISE(ABORT, 'blocked'); END");
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

    // Opens a connection for the step-level functions (they take an OstDatabase, like the import transaction does).
    class Connection
    {
    public:
        explicit Connection(const ScratchDb &db)
        {
            if (!_db.open(db.path()))
                FAIL_TEST("cannot open " + db.path());
        }
        ost_db::OstDatabase &get() { return _db; }
        ost_db::OstDatabase *operator->() { return &_db; }

    private:
        ost_db::OstDatabase _db;
    };

    inline ost_import::Row row(std::initializer_list<std::pair<const std::string, std::string>> fields)
    {
        return ost_import::Row(fields);
    }

    // "UID=5,Name=x" style compact description of a Row (sorted), for readable failure messages.
    inline std::string describe(const ost_import::Row &r)
    {
        std::vector<std::string> keys;
        for (const auto &f : r)
            keys.push_back(f.first);
        std::sort(keys.begin(), keys.end());
        std::string out;
        for (const std::string &k : keys)
            out += (out.empty() ? "" : ",") + k + "=" + r.at(k);
        return out;
    }

    // Column values of one table of a raw data set, in row order: values_of(rows, "UID") -> {"101","102"}.
    inline std::vector<std::string> values_of(const ost_import::RowList &rows, const std::string &column)
    {
        std::vector<std::string> out;
        for (const auto &r : rows)
        {
            auto it = r.find(column);
            out.push_back(it == r.end() ? std::string("<absent>") : it->second);
        }
        return out;
    }
}
