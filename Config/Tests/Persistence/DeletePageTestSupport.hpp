// DeletePageTestSupport.hpp - helpers of the Delete Page tests (Persistence area; plan 4.12: a stream's own <Feature>TestSupport.hpp).
//
// Builds on ost_ptest::TempDb (PersistenceTestSupport.hpp: snapshot_all, block / unblock) and the seed helpers of StoreTestSupport.hpp.
// The Windows oracle (DeletePageOracleData.hpp) is seeded and projected through the functions below.
#pragma once

#include "StoreTestSupport.hpp"

#include "DeletePageOracleData.hpp"
#include "Domain/PageDeletion.hpp"
#include "Persistence/OstSchema.hpp"
#include "Persistence/ProjectStore/ProjectStore.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace ost_delpage
{
    using ost_ptest::TempDb;
    using Uids = std::vector<std::string>;

    // delete_pages with the error text always returned (the store writes `error` on every call, success included).
    inline bool run(const TempDb &db, const std::string &bid, const Uids &uids, std::string *error_out = nullptr)
    {
        std::string error = "<unset>";
        const bool ok = ost_store::delete_pages(db.path(), bid, uids, &error);
        if (error_out)
            *error_out = error;
        return ok;
    }

    inline bool run(const TempDb &db, const std::string &bid, const Uids &uids, std::string &error_out)
    {
        return run(db, bid, uids, &error_out);
    }

    // The elements of a set as one text ("a,b,c" in set order), so that a failure of a set comparison is readable (the harness cannot print a std::set).
    inline std::string joined(const std::set<std::string> &values)
    {
        std::string out;
        for (const std::string &value : values)
            out += (out.empty() ? "" : ",") + value;
        return out;
    }

    // ---- the Windows oracle world --------------------------------------------------------------------------------------------

    inline const ost_delpage_oracle::Scenario &scenario(const std::string &name)
    {
        for (const ost_delpage_oracle::Scenario &candidate : ost_delpage_oracle::scenarios())
            if (name == candidate.name)
                return candidate;
        static const ost_delpage_oracle::Scenario none{"<none>", {}, {}, {}};
        return none;
    }

    // Runs the statements of the scenario on a database made by create_schema; bids 1, 2, 3 ... of the oracle have no BidProjects row in
    // the iOS schema (there are no foreign keys), only the rows the statements insert exist.
    inline void seed_scenario(const TempDb &db, const ost_delpage_oracle::Scenario &scenario)
    {
        REQUIRE(db.schema_ok());
        REQUIRE(!scenario.seed.empty());
        // one connection for the whole seed (the statements hold no ';' inside a text)
        std::string all;
        for (const char *statement : scenario.seed)
            all += std::string(statement) + ";\n";
        db.must(all);
    }

    inline Uids scenario_uids(const ost_delpage_oracle::Scenario &scenario)
    {
        Uids uids;
        for (const char *uid : scenario.uids)
            uids.push_back(uid);
        return uids;
    }

    // One text per projection of the oracle: the rows of that table as the oracle's select renders them. "<bad sql: ...>" when the iOS
    // table lacks a column the oracle uses.
    inline std::vector<std::string> project_all(const TempDb &db)
    {
        std::vector<std::string> out;
        for (const ost_delpage_oracle::Projection &projection : ost_delpage_oracle::projections())
            out.push_back(ost_ptest::list(db, projection.select_sql));
        return out;
    }

    // The names of the tables of the oracle whose projection differs from `expected` (empty: everything agrees). `skip_table` is left out.
    inline std::string projection_differences(const TempDb &db, const std::vector<const char *> &expected, const std::string &skip_table = "")
    {
        const std::vector<std::string> actual = project_all(db);
        const auto &projections = ost_delpage_oracle::projections();
        std::string out;
        for (std::size_t i = 0; i < projections.size() && i < expected.size(); ++i)
        {
            if (skip_table == projections[i].table)
                continue;
            if (actual[i] != expected[i])
                out += std::string("[") + projections[i].table + "] windows=" + expected[i] + " ios=" + actual[i] + "\n";
        }
        return out;
    }

    // ---- row and schema helpers ----------------------------------------------------------------------------------------------

    // The table recreated without `column` (CREATE TABLE AS keeps the rows, drops the primary key and the indexes: the sparse schemas of
    // the Windows tests have neither). The rows keep their rowid order.
    inline void drop_column(const TempDb &db, const std::string &table, const std::string &column)
    {
        std::string columns;
        {
            ost_db::OstDatabase connection;
            REQUIRE(connection.open(db.path()));
            ost_db::Statement info = connection.prepare("SELECT name FROM pragma_table_info(?)");
            REQUIRE(info.valid());
            info.bind_text(1, table);
            while (info.step())
            {
                if (info.column_text(0) == column)
                    continue;
                columns += (columns.empty() ? "[" : ", [") + info.column_text(0) + "]";
            }
        }
        REQUIRE(!columns.empty());
        db.must("CREATE TABLE [" + table + "__tmp] AS SELECT " + columns + " FROM [" + table + "] ORDER BY rowid");
        db.must("DROP TABLE [" + table + "]");
        db.must("ALTER TABLE [" + table + "__tmp] RENAME TO [" + table + "]");
    }

    // Every column of one row (or of the rows matching `where_sql`) as text: "col=value" with the SQL quoting, NULL columns left out.
    inline std::string row_dump(const TempDb &db, const std::string &table, const std::string &where_sql)
    {
        ost_db::OstDatabase connection;
        if (!connection.open(db.path()))
            return "<cannot open>";
        std::vector<std::string> columns;
        {
            ost_db::Statement info = connection.prepare("SELECT name FROM pragma_table_info(?)");
            if (!info.valid())
                return "<pragma failed>";
            info.bind_text(1, table);
            while (info.step())
                columns.push_back(info.column_text(0));
        }
        if (columns.empty())
            return "<missing>";
        std::string expr = "''";
        for (const std::string &column : columns)
            expr += " || CASE WHEN [" + column + "] IS NULL THEN '' ELSE '" + column + "=' || quote([" + column + "]) || ' ' END";
        ost_db::Statement rows = connection.prepare("SELECT group_concat(r, char(10)) FROM (SELECT " + expr + " AS r FROM [" + table + "] WHERE " +
                                                    where_sql + " ORDER BY rowid)");
        if (!rows.valid() || !rows.step())
            return "<select failed>";
        return rows.column_text(0);
    }

    // The rows of `table` with `where_sql`, counted ("<none>" when the query fails).
    inline std::string count_of(const TempDb &db, const std::string &table, const std::string &where_sql = "1")
    {
        const int n = db.count("SELECT COUNT(*) FROM [" + table + "] WHERE " + where_sql);
        return n < 0 ? "<none>" : std::to_string(n);
    }

    // A trigger that makes the delete of a row of `parent` fail while a row of `child` still references its UID through `column`: what
    // PRAGMA foreign_keys=ON does in the Windows tests that declare REFERENCES (test_settings_operations.py:243, :394), which the iOS DDL
    // does not have. The cascade must clear or delete the referencing rows BEFORE the parent.
    inline void forbid_orphans(const TempDb &db, const std::string &parent, const std::string &child, const std::string &column)
    {
        db.must("CREATE TRIGGER [fk_" + parent + "_" + child + "_" + column + "] BEFORE DELETE ON [" + parent + "] WHEN EXISTS (SELECT 1 FROM [" +
                child + "] WHERE [" + column + "] = OLD.UID) BEGIN SELECT RAISE(ABORT, 'FOREIGN KEY constraint failed'); END");
    }

    // A failed READ: the root pages of the table and of its indexes are overwritten, so that the first statement that reads the table fails
    // with "database disk image is malformed". Every other table stays readable (and snapshot_all shows the broken one as "<select failed>"
    // before and after, so a before / after comparison still works). The file is TempDb's own.
    inline bool corrupt_table(const TempDb &db, const std::string &table)
    {
        if (!db.exec("PRAGMA wal_checkpoint(TRUNCATE)"))
            return false;
        const int page_size = db.count("PRAGMA page_size");
        const std::string roots = ost_ptest::list(db, "SELECT rootpage FROM sqlite_master WHERE tbl_name = '" + table + "' AND rootpage > 0");
        if (page_size <= 0 || roots.empty())
            return false;
        std::fstream file(db.path(), std::ios::in | std::ios::out | std::ios::binary);
        if (!file)
            return false;
        std::size_t start = 0;
        while (start <= roots.size())
        {
            std::size_t comma = roots.find(',', start);
            if (comma == std::string::npos)
                comma = roots.size();
            const long long root = std::stoll(roots.substr(start, comma - start));
            file.seekp(static_cast<std::streamoff>((root - 1) * page_size));
            const std::string junk(64, '\xFF');
            file.write(junk.data(), static_cast<std::streamsize>(junk.size()));
            start = comma + 1;
        }
        file.flush();
        return static_cast<bool>(file);
    }

    // ---- small worlds --------------------------------------------------------------------------------------------------------

    // Bid `bid` (project 2, created when missing) with pages `first_uid` .. `first_uid + count - 1`, named "Page <n>".
    inline void bid_with_pages(const TempDb &db, long long bid, int count, long long first_uid)
    {
        ost_ptest::seed_pages(db, bid, count, 0, first_uid);
    }

    // The 26 tables of the cascade that carry a BidPageUID column, from the database itself (PRAGMA table_info over sqlite_master).
    inline std::set<std::string> tables_with_page_column(const TempDb &db)
    {
        std::set<std::string> out;
        ost_db::OstDatabase connection;
        if (!connection.open(db.path()))
            return out;
        ost_db::Statement stmt = connection.prepare(
            "SELECT m.name FROM sqlite_master m, pragma_table_info(m.name) c WHERE m.type = 'table' AND m.name NOT LIKE 'sqlite_%' AND c.name = 'BidPageUID'");
        if (!stmt.valid())
            return out;
        while (stmt.step())
            out.insert(stmt.column_text(0));
        return out;
    }

    // The loaded project as text (pages in order with name and folder, the takeoffs of each page, the conditions, the folder tree, the
    // saved page), for "the project after the delete equals the project before minus the page".
    inline std::string describe(const ost_data::ProjectData &project)
    {
        std::string out;
        for (const std::string &uid : project.page_order)
        {
            const auto it = project.pages.find(uid);
            if (it == project.pages.end())
                continue;
            out += "page " + uid + " name=" + it->second.name + " folder=" + it->second.folder_uid + " index=" + std::to_string(it->second.page_index) +
                   " seq=" + std::to_string(it->second.sequence) + " image=" + it->second.image_path + "\n";
        }
        std::vector<std::string> takeoffs;
        for (const ost_domain::Takeoff &t : project.takeoffs)
            takeoffs.push_back("takeoff " + t.uid + " page=" + t.page_uid + " cond=" + t.condition_uid + " parent=" + t.parent_uid);
        std::sort(takeoffs.begin(), takeoffs.end());
        for (const std::string &line : takeoffs)
            out += line + "\n";
        std::vector<std::string> conditions;
        for (const auto &pair : project.conditions)
            conditions.push_back("condition " + pair.first + " " + pair.second.name);
        std::sort(conditions.begin(), conditions.end());
        for (const std::string &line : conditions)
            out += line + "\n";
        for (const auto &pair : project.folders)
        {
            out += "folder " + pair.first + " pages=";
            for (const std::string &page : pair.second.all_page_uids())
                out += page + ",";
            out += "\n";
        }
        out += "selected=" + project.selected_page_uid + "\n";
        return out;
    }
}
