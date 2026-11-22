// StoreTestSupport.hpp - query helpers on top of PersistenceTestSupport.hpp for the load/writer parity tests.
#pragma once

#include "PersistenceTestSupport.hpp"

#include "Domain/Takeoff.hpp"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace ost_ptest
{
    // The first column of every row of `select_one_column` in the order the query returns them, joined with ','. "" when
    // there is no row; a NULL reads as "", so select an expression that renders NULL when it matters (ifnull(x, 'N')).
    inline std::string list(const TempDb &db, const std::string &select_one_column)
    {
        ost_db::OstDatabase connection;
        if (!connection.open(db.path()))
            return "<cannot open>";
        ost_db::Statement stmt = connection.prepare(select_one_column);
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

    // 'text' with the quotes SQL needs.
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

    // A bid world for the writer tests (project 1 = 'Deleted Bids' from create_schema):
    //   BidProjects 2 'Folder A'
    //   Bid 10 (project 2) with: condition 30, 31; page 100, 101; area 60; layer 70
    //   Bid 20 (project 2) with: condition 40; page 200; area 80; layer 90; takeoff 900 on (40, 200)
    //   BidLayers 3 is a template layer of no bid (create_schema's 'Image' layer).
    inline void seed_two_bids(const TempDb &db)
    {
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (10, 2, 'Job ten', 10)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (20, 2, 'Job twenty', 20)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (30, 10, 1, 'C30', 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (31, 10, 2, 'C31', 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (40, 20, 1, 'C40', 0)");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (100, 10, 'P100')");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (101, 10, 'P101')");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (200, 20, 'P200')");
        db.must("INSERT INTO BidAreas (UID, BidUID, Name) VALUES (60, 10, 'A60')");
        db.must("INSERT INTO BidAreas (UID, BidUID, Name) VALUES (80, 20, 'A80')");
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (70, 10, 'L70', -1, 1)");
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (90, 20, 'L90', -1, 1)");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) "
                "VALUES (900, 20, 40, 200, '1;2;3;4')");
    }

    // ---------------------------------------------------------------------------------------------------------------------------
    // M1 seed helpers (plan 4.12 / 6.1, owned by S0): the bid lock fixtures and the page fixtures. They take an ost_ptest::TempDb (a
    // database made by create_schema); they never assume an empty database (INSERT OR IGNORE for the shared rows), so they combine with
    // seed_two_bids as long as the UIDs they use are not taken (the defaults below avoid the UIDs of seed_two_bids).
    // ---------------------------------------------------------------------------------------------------------------------------

    // `value` as the shortest text that reads back exactly (%.17g), for a REAL column.
    inline std::string real_text(double value)
    {
        char buffer[40];
        std::snprintf(buffer, sizeof buffer, "%.17g", value);
        return std::string(buffer);
    }

    // BidProjects 2 'Folder A' (the project seed_two_bids creates), when the database has none.
    inline void ensure_project_two(const TempDb &db)
    {
        db.must("INSERT OR IGNORE INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
    }

    // The bid exists (project 2, BidNo = uid; an existing bid keeps its row) and its JobStatusUID is `status_sql`, a SQL literal
    // ("NULL", "5", "''", "'007'", ...). The status row is NOT created: that is the point of the unresolved-status variants.
    inline void seed_bid_with_status(const TempDb &db, long long bid_uid, const std::string &status_sql)
    {
        ensure_project_two(db);
        const std::string uid = std::to_string(bid_uid);
        db.must("INSERT OR IGNORE INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (" + uid + ", 2, 'Job " + uid + "', " + uid + ")");
        db.must("UPDATE Bids SET JobStatusUID = " + status_sql + " WHERE UID = " + uid);
    }

    // A JobStatuses row; `locked_sql` is the SQL literal of the Locked column ("-1" is what the importer writes for Yes, "0" for No),
    // Sequence = uid. The window of the Windows rule: a NULL / 1 / 2 / 'True' Locked is locked, 0 / 'False' / '' is not.
    inline void seed_job_status(const TempDb &db, long long uid, const std::string &locked_sql, const std::string &name = "Status")
    {
        db.must("INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (" + std::to_string(uid) + ", " + quoted(name) + ", " +
                locked_sql + ", " + std::to_string(uid) + ")");
    }

    // A bid whose JobStatusUID points at a JobStatuses row with Locked set (-1, what the importer stores for Yes): status `status_uid`
    // 'Locked' (and bid `bid_uid` in project 2, created when missing). Bid 10 is the bid of seed_two_bids: the call then locks that bid
    // (its other rows stay as they are).
    inline void seed_locked_bid(const TempDb &db, long long bid_uid = 10, long long status_uid = 5)
    {
        seed_job_status(db, status_uid, "-1", "Locked");
        seed_bid_with_status(db, bid_uid, std::to_string(status_uid));
    }

    // The counterpart: a bid with a status that is NOT locked (Locked 0): status `status_uid` 'Open'.
    inline void seed_open_bid(const TempDb &db, long long bid_uid = 10, long long status_uid = 4)
    {
        seed_job_status(db, status_uid, "0", "Open");
        seed_bid_with_status(db, bid_uid, std::to_string(status_uid));
    }

    // Variant for the Windows lock oracle (BidLockOracleData.hpp): the loosely typed tables of the Windows tests
    // (tests/infrastructure/mdb/components/test_settings_reader.py:892-910), so that any stored value survives as written (the iOS DDL's
    // INTEGER affinity would turn '0' into 0, ' 0' into 0 and keep 'False' as text). For a TempDb made WITHOUT the schema:
    //   CREATE TABLE Bids (UID INTEGER, JobStatusUID)
    //   CREATE TABLE JobStatuses (UID INTEGER, Name TEXT, Locked, Sequence INTEGER)      (not created when with_status_table is false)
    // Bid `bid_uid` gets JobStatusUID = `bid_status_sql`; `statuses` are (uid literal, locked literal) pairs in row order, Sequence 1, 2, ...
    inline void seed_lock_oracle_case(const TempDb &db, long long bid_uid, const std::string &bid_status_sql,
                                      const std::vector<std::pair<std::string, std::string>> &statuses, bool with_status_table = true)
    {
        db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
        db.must("INSERT INTO Bids VALUES (" + std::to_string(bid_uid) + ", " + bid_status_sql + ")");
        if (!with_status_table)
            return;
        db.must("CREATE TABLE JobStatuses (UID INTEGER, Name TEXT, Locked, Sequence INTEGER)");
        int sequence = 1;
        for (const auto &status : statuses)
        {
            db.must("INSERT INTO JobStatuses VALUES (" + status.first + ", 'Status " + std::to_string(sequence) + "', " + status.second + ", " +
                    std::to_string(sequence) + ")");
            ++sequence;
        }
    }

    // One page of seed_pages: the stored Name (any text), the page scale (ScaleFactor1 / ScaleFactor2: 0.25 / 12 is the Windows default,
    // 1 / 12 a "1" = 1' sheet) and how many takeoffs sit on it.
    struct SeedPage
    {
        std::string name;
        double scale_factor1 = 0.25;
        double scale_factor2 = 12.0;
        int takeoffs = 0;
    };

    // A bid with these pages (created when missing, project 2): page uid = first_page_uid + i, Sequence = Index1 = i + 1 (the order of the
    // page bars), ScaleStyle 1, IsCustomScale 0, Width 42, Height 30, Show -1, ScaleFactor1/2 and Name as given. When a page has takeoffs the
    // bid gets ONE condition (uid `condition_uid`, RefNo 1, linear, INSERT OR IGNORE) and each takeoff is a BidTakeoffs row on that page and
    // condition with Position '1;2;3;4', uid = first_takeoff_uid, +1, +2 ... in page order. The defaults avoid the UIDs of seed_two_bids
    // (pages 100, 101, 200; takeoff 900; conditions 30, 31, 40).
    inline void seed_pages(const TempDb &db, long long bid_uid, const std::vector<SeedPage> &pages, long long first_page_uid = 1000,
                           long long first_takeoff_uid = 9000, long long condition_uid = 7000)
    {
        ensure_project_two(db);
        const std::string bid = std::to_string(bid_uid);
        db.must("INSERT OR IGNORE INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (" + bid + ", 2, 'Job " + bid + "', " + bid + ")");
        bool any_takeoffs = false;
        for (const SeedPage &page : pages)
            any_takeoffs = any_takeoffs || page.takeoffs > 0;
        if (any_takeoffs)
            db.must("INSERT OR IGNORE INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (" + std::to_string(condition_uid) + ", " +
                    bid + ", 1, 'Seed condition', 0)");
        long long takeoff_uid = first_takeoff_uid;
        for (std::size_t i = 0; i < pages.size(); ++i)
        {
            const SeedPage &page = pages[i];
            const std::string uid = std::to_string(first_page_uid + static_cast<long long>(i));
            const std::string order = std::to_string(i + 1);
            db.must("INSERT INTO BidPages (UID, BidUID, Name, Sequence, Index1, ScaleStyle, IsCustomScale, ScaleFactor1, ScaleFactor2, Width, "
                    "Height, Show) VALUES (" + uid + ", " + bid + ", " + quoted(page.name) + ", " + order + ", " + order + ", 1, 0, " +
                    real_text(page.scale_factor1) + ", " + real_text(page.scale_factor2) + ", 42, 30, -1)");
            for (int t = 0; t < page.takeoffs; ++t)
                db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (" + std::to_string(takeoff_uid++) +
                        ", " + bid + ", " + std::to_string(condition_uid) + ", " + uid + ", '1;2;3;4')");
        }
    }

    // `count` pages named "Page 1" .. "Page <count>" at the default scale 0.25 / 12, `takeoffs_per_page` takeoffs on each.
    inline void seed_pages(const TempDb &db, long long bid_uid, int count, int takeoffs_per_page = 0, long long first_page_uid = 1000,
                           long long first_takeoff_uid = 9000, long long condition_uid = 7000)
    {
        std::vector<SeedPage> pages;
        for (int i = 1; i <= count; ++i)
        {
            SeedPage page;
            page.name = "Page " + std::to_string(i);
            page.takeoffs = takeoffs_per_page;
            pages.push_back(page);
        }
        seed_pages(db, bid_uid, pages, first_page_uid, first_takeoff_uid, condition_uid);
    }

    inline ost_domain::Takeoff make_takeoff(const std::string &condition_uid, const std::string &page_uid,
                                            const std::string &area_uid = "0", const std::string &parent_uid = "0",
                                            const std::string &uid = "")
    {
        ost_domain::Takeoff t;
        t.uid = uid;
        t.condition_uid = condition_uid;
        t.page_uid = page_uid;
        t.area_uid = area_uid;
        t.parent_uid = parent_uid;
        t.position = {0.0, 0.0, 1.0, 1.0};
        return t;
    }
}
