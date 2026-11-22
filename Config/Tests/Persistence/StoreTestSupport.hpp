// StoreTestSupport.hpp - query helpers on top of PersistenceTestSupport.hpp for the load/writer parity tests.
#pragma once

#include "PersistenceTestSupport.hpp"

#include "Domain/Takeoff.hpp"

#include <string>
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
