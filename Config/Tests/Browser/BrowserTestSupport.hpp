// BrowserTestSupport.hpp - shared helpers for the project browser host tests (folders, trash, bid order).
//
// ProjectStore opens its own sqlite connection on every call, so the tests drive it through a database FILE in the system
// temp dir (never in the repo). TempDb owns such a file and offers raw-SQL helpers that open a short-lived connection.
#pragma once

#include "TestHarness.hpp"

#include "Domain/DatabaseHierarchy.hpp"
#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore.hpp"

#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace ost_btest
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
        // with_schema=true runs ost_store::create_schema (every iOS table and the 'Deleted Bids' project with UID 1).
        explicit TempDb(const std::string &name, bool with_schema = true)
            : _path(ost_test::temp_path("browser_" + run_token() + "_" + name + ".db"))
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

        // The first column of every row of `select_one_column`, in the order the query returns them, joined with ','.
        // "" when there is no row (a NULL reads as ""; select ifnull(x, 'N') when it matters).
        std::string list(const std::string &select_one_column) const
        {
            ost_db::OstDatabase db;
            if (!db.open(_path))
                return "<cannot open>";
            ost_db::Statement stmt = db.prepare(select_one_column);
            if (!stmt.valid())
                return "<bad sql: " + stmt.error_message() + ">";
            std::string out;
            bool first = true;
            while (stmt.step())
            {
                out += (first ? "" : ",") + stmt.column_text(0);
                first = false;
            }
            if (stmt.failed())
                return "<read failed: " + stmt.error_message() + ">";
            return out;
        }

        // Every bid as "<uid>:<BidProjectUID>/<OrigBidProjectUID>" (N for NULL) in uid order, joined with ' '.
        std::string bids() const
        {
            std::string text = list("SELECT UID || ':' || ifnull(BidProjectUID, 'N') || '/' || ifnull(OrigBidProjectUID, 'N') "
                                    "FROM Bids ORDER BY UID");
            for (char &c : text)
                if (c == ',')
                    c = ' ';
            return text;
        }

        // Every folder as "<uid>:<name>" in uid order, joined with ','.
        std::string folders() const { return list("SELECT UID || ':' || ifnull(Name, 'N') FROM BidProjects ORDER BY UID"); }

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
            std::string row_expr = "rowid || ':'";
            for (const std::string &column : columns)
                row_expr += " || CASE WHEN [" + column + "] IS NULL THEN '' ELSE ' " + column + "=' || quote([" + column +
                            "]) END";
            ost_db::Statement rows = db.prepare("SELECT group_concat(r, char(10)) FROM (SELECT " + row_expr + " AS r FROM [" +
                                                table + "] ORDER BY rowid)");
            if (!rows.valid() || !rows.step())
                return "<select failed>";
            return rows.column_text(0);
        }

        // dump() of the tables the browser writes (BidProjects, Bids), for whole-database before/after comparisons.
        std::string snapshot() const { return "[BidProjects]\n" + dump("BidProjects") + "\n[Bids]\n" + dump("Bids") + "\n"; }

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

    // "<folder uid>{<bid uids>}" per folder in hierarchy order joined by '|', then "||o{<orphan bid uids>}": the shape of the
    // Windows oracle (HierarchyOrderOracleData.hpp) and of the display order of the browser.
    inline std::string hierarchy_text(const ost_domain::DatabaseHierarchy &hierarchy)
    {
        std::string out;
        bool first_folder = true;
        for (const ost_domain::FolderGroup &group : hierarchy.folders)
        {
            out += (first_folder ? "" : "|") + std::string("p") + group.folder.uid + "{";
            first_folder = false;
            bool first = true;
            for (const ost_domain::BidSummary &bid : group.bids)
            {
                out += (first ? "" : ",") + bid.bid_uid;
                first = false;
            }
            out += "}";
        }
        out += "||o{";
        bool first = true;
        for (const ost_domain::BidSummary &bid : hierarchy.orphan_bids)
        {
            out += (first ? "" : ",") + bid.bid_uid;
            first = false;
        }
        return out + "}";
    }

    // The folder names of the hierarchy in order, joined with ','.
    inline std::string folder_names(const ost_domain::DatabaseHierarchy &hierarchy)
    {
        std::string out;
        bool first = true;
        for (const ost_domain::FolderGroup &group : hierarchy.folders)
        {
            out += (first ? "" : ",") + group.folder.name;
            first = false;
        }
        return out;
    }

    // A SQL string literal.
    inline std::string quoted(const std::string &text)
    {
        std::string out = "'";
        for (const char c : text)
        {
            out += c;
            if (c == '\'')
                out += '\'';
        }
        return out + "'";
    }
}
