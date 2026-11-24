// LayerEditTestSupport.hpp - fixtures and helpers of the layer editing tests (test_layer_edit_*.cpp, plan 5.3 / 6.1, stream S2).
//
// Everything here is test-side only. The databases are made by create_schema, so BidLayers has the four template rows of the schema
// (UID 1 'Default' seq 2, 2 'Annotation' seq 1, 3 'Image' seq 0, 4 'Comments' seq 3: IsTemplate -1, IsLocked -1, no bid = shared rows).
// The Windows tests build their own bare tables, so the ports clear those rows first (clear_layers) and seed the Windows rows verbatim.
#pragma once

#include "StoreTestSupport.hpp"

#include "Domain/LayerEdit.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ost_ltest
{
    using ost_ptest::TempDb;

    // LAYER_REFERENCE_TABLES, in the Windows order (infrastructure/mdb/components/constants.py:122-134).
    inline const std::vector<std::string> &reference_tables()
    {
        static const std::vector<std::string> tables = {
            "BidConditions",       "BidZones",       "BidComments",     "BidAnnotationClouds", "BidAnnotationOvals", "BidAnnotationPolygons",
            "BidAnnotationRects",  "BidTexts",       "BidHighlights",   "BidHotLinks",         "BidCallOuts"};
        return tables;
    }

    // The same tables plus BidLayers: what a layer write may touch.
    inline std::vector<std::string> layer_family_tables()
    {
        std::vector<std::string> tables = reference_tables();
        tables.push_back("BidLayers");
        return tables;
    }

    inline std::vector<std::string> split_columns(const std::string &columns)
    {
        std::vector<std::string> out;
        std::string current;
        for (const char c : columns)
        {
            if (c == ',')
            {
                out.push_back(current);
                current.clear();
            }
            else if (c != ' ')
                current += c;
        }
        out.push_back(current);
        return out;
    }

    // The rows of `table` as "(v1,v2,...);(v1,v2,...)" with sqlite quote() (NULL as NULL, text in single quotes), ordered by `order`,
    // optionally filtered: the shape of Python's fetchall() of the Windows tests. "" for no row.
    inline std::string table_text(const TempDb &db, const std::string &table, const std::string &columns, const std::string &where = "",
                                  const std::string &order = "UID")
    {
        const std::vector<std::string> cols = split_columns(columns);
        std::string expr = "'('";
        for (std::size_t i = 0; i < cols.size(); ++i)
            expr += std::string(i == 0 ? " || " : " || ',' || ") + "quote([" + cols[i] + "])";
        expr += " || ')'";
        const std::string sql = "SELECT group_concat(r, ';') FROM (SELECT " + expr + " AS r FROM [" + table + "]" +
                                (where.empty() ? "" : " WHERE " + where) + " ORDER BY " + order + ")";
        return db.text(sql);
    }

    // BidLayers rows as the Windows tests read them: SELECT UID, BidUID, Name, Show, Sequence, IsTemplate, IsLocked ... ORDER BY UID.
    inline std::string layers_text(const TempDb &db, const std::string &where = "")
    {
        return table_text(db, "BidLayers", "UID, BidUID, Name, Show, Sequence, IsTemplate, IsLocked", where);
    }

    // SELECT UID, Sequence ... ORDER BY UID.
    inline std::string sequences_text(const TempDb &db, const std::string &where = "")
    {
        return table_text(db, "BidLayers", "UID, Sequence", where);
    }

    // SELECT UID, Name ... ORDER BY UID.
    inline std::string names_text(const TempDb &db, const std::string &where = "")
    {
        return table_text(db, "BidLayers", "UID, Name", where);
    }

    // Every reference table with its rows, for "this write touched no reference" comparisons.
    inline std::string references_text(const TempDb &db)
    {
        std::string out;
        for (const std::string &table : reference_tables())
            out += table + ":" + table_text(db, table, "UID, BidUID, BidLayerUID") + "\n";
        return out;
    }

    // snapshot_all() of every user table EXCEPT `skip`: "nothing outside the layer family changed".
    inline std::string snapshot_except(const TempDb &db, const std::vector<std::string> &skip)
    {
        std::vector<std::string> tables;
        {
            ost_db::OstDatabase connection;
            if (!connection.open(db.path()))
                return "<cannot open>";
            ost_db::Statement stmt = connection.prepare(
                "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name");
            if (!stmt.valid())
                return "<sqlite_master failed>";
            while (stmt.step())
            {
                const std::string name = stmt.column_text(0);
                if (std::find(skip.begin(), skip.end(), name) == skip.end())
                    tables.push_back(name);
            }
        }
        return db.snapshot(tables);
    }

    // The rows of the 11 reference tables one table per line, for per-table comparisons.
    inline std::vector<std::string> reference_rows(const TempDb &db)
    {
        std::vector<std::string> out;
        for (const std::string &table : reference_tables())
            out.push_back(table_text(db, table, "UID, BidUID, BidLayerUID"));
        return out;
    }

    // `text` with every pair (from -> to) replaced once, in order; a missing `from` fails the test (the expected text is built from the seed text).
    inline std::string edited(std::string text, const std::vector<std::pair<std::string, std::string>> &edits)
    {
        for (const auto &edit : edits)
        {
            const std::size_t at = text.find(edit.first);
            if (at == std::string::npos)
                FAIL_TEST("expected text does not contain: " + edit.first);
            text.replace(at, edit.first.size(), edit.second);
        }
        return text;
    }

    inline void clear_layers(const TempDb &db)
    {
        db.must("DELETE FROM BidLayers");
    }

    // Bids (project 1 = 'Deleted Bids' of create_schema) with these UIDs; an existing bid keeps its row.
    inline void add_bids(const TempDb &db, std::initializer_list<long long> uids)
    {
        for (const long long uid : uids)
            db.must("INSERT OR IGNORE INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (" + std::to_string(uid) + ", 1, 'Bid " +
                    std::to_string(uid) + "', " + std::to_string(uid) + ", 0)");
    }

    // One BidLayers row; every column is a SQL literal ("NULL", "-1", "7"), so any stored shape can be seeded.
    inline void put_layer(const TempDb &db, long long uid, const std::string &bid_sql, const std::string &name, const std::string &show_sql,
                          const std::string &sequence_sql, const std::string &template_sql = "0", const std::string &locked_sql = "0")
    {
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence, IsTemplate, IsLocked) VALUES (" + std::to_string(uid) + ", " + bid_sql +
                ", " + ost_ptest::quoted(name) + ", " + show_sql + ", " + sequence_sql + ", " + template_sql + ", " + locked_sql + ")");
    }

    // A row of a reference table: UID, BidUID and BidLayerUID (SQL literals).
    inline void put_reference(const TempDb &db, const std::string &table, long long uid, const std::string &bid_sql, const std::string &layer_sql)
    {
        db.must("INSERT INTO [" + table + "] (UID, BidUID, BidLayerUID) VALUES (" + std::to_string(uid) + ", " + bid_sql + ", " + layer_sql + ")");
    }

    // The bid and layer world of the M1 device pack (plan 6.3), with the UIDs the tests quote:
    //   Bid 10 (open bid)    101 Image (locked copy, seq 0)  102 Annotation (locked, 1)  103 Default (locked, 2)  104 Comments (locked, 3, not listed)
    //                        105 Walls (own, 4)  106 Floors (own, 5)  107 Unused (own, 6)
    //   Shared template rows 1 Default (seq 2)  2 Annotation (1)  3 Image (0)  4 Comments (3) from create_schema, and 8 'Shared Template' (seq 7);
    //                        all IsTemplate -1, IsLocked -1, no bid
    //   Bid 20 (other bid)   201 Other (own, 1)  202 Zebra (own, 2)
    //   References           condition 301 (bid 10) on Walls 105, annotation rectangle 501 (bid 10) on Floors 106 (the layer a condition does not use:
    //                        in use only through an annotation), condition 302 (bid 20) on 201
    // The next layer UID is 203. All Show values are -1.
    inline void seed_m1(const TempDb &db)
    {
        add_bids(db, {10, 20});
        put_layer(db, 101, "10", "Image", "-1", "0", "0", "-1");
        put_layer(db, 102, "10", "Annotation", "-1", "1", "0", "-1");
        put_layer(db, 103, "10", "Default", "-1", "2", "0", "-1");
        put_layer(db, 104, "10", "Comments", "-1", "3", "0", "-1");
        put_layer(db, 105, "10", "Walls", "-1", "4");
        put_layer(db, 106, "10", "Floors", "-1", "5");
        put_layer(db, 107, "10", "Unused", "-1", "6");
        put_layer(db, 8, "NULL", "Shared Template", "-1", "7", "-1", "-1");
        put_layer(db, 201, "20", "Other", "-1", "1");
        put_layer(db, 202, "20", "Zebra", "-1", "2");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, BidLayerUID) VALUES (301, 10, 1, 'Wall cond', 0, 105)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, BidLayerUID) VALUES (302, 20, 1, 'Other cond', 0, 201)");
        db.must("INSERT INTO BidAnnotationRects (UID, BidUID, BidLayerUID) VALUES (501, 10, 106)");
    }

    // The M1 world as the exact text the tests compare against after a write that must not change it.
    inline const char *kM1Layers =
        "(1,NULL,'Default',-1,2,-1,-1);(2,NULL,'Annotation',-1,1,-1,-1);(3,NULL,'Image',-1,0,-1,-1);(4,NULL,'Comments',-1,3,-1,-1);"
        "(8,NULL,'Shared Template',-1,7,-1,-1);(101,10,'Image',-1,0,0,-1);(102,10,'Annotation',-1,1,0,-1);(103,10,'Default',-1,2,0,-1);"
        "(104,10,'Comments',-1,3,0,-1);(105,10,'Walls',-1,4,0,0);(106,10,'Floors',-1,5,0,0);(107,10,'Unused',-1,6,0,0);"
        "(201,20,'Other',-1,1,0,0);(202,20,'Zebra',-1,2,0,0)";

    // CREATE TRIGGER that makes every <event> on <table> that matches `when_sql` (a SQL expression over NEW / OLD, "" = every row) fail with
    // RAISE(ABORT). Returns the trigger name for unblock.
    inline std::string block_when(const TempDb &db, const std::string &table, const std::string &event, const std::string &when_sql)
    {
        static int sequence = 0;
        const std::string name = "blkw_" + std::to_string(sequence++) + "_" + event + "_" + table;
        db.must("CREATE TRIGGER [" + name + "] BEFORE " + event + " ON [" + table + "]" + (when_sql.empty() ? "" : " WHEN " + when_sql) +
                " BEGIN SELECT RAISE(ABORT, 'blocked'); END");
        return name;
    }

    // Makes the COMMIT of any write that fires <event> on <table> fail: an AFTER trigger inserts a row that violates a DEFERRED foreign key,
    // which sqlite only checks at COMMIT (the device of test_store_error_propagation.cpp ImportReturnsFalseWhenTheCommitFails). The helper
    // tables are created here, so call it BEFORE the "before" snapshot.
    inline void make_commit_fail(const TempDb &db, const std::string &table, const std::string &event)
    {
        db.must("CREATE TABLE IF NOT EXISTS fk_parent (id INTEGER PRIMARY KEY)");
        db.must("CREATE TABLE IF NOT EXISTS fk_child (pid INTEGER REFERENCES fk_parent(id) DEFERRABLE INITIALLY DEFERRED)");
        db.must("CREATE TRIGGER fk_violation_" + event + "_" + table + " AFTER " + event + " ON [" + table + "] BEGIN INSERT INTO fk_child VALUES (999); END");
    }

    // A path whose directory does not exist: sqlite cannot open it, so every store call must fail cleanly.
    inline std::string unopenable_db_path()
    {
        return ost_test::temp_path("no_such_dir_" + ost_ptest::run_token() + "/layers.sqlite");
    }

    // A file that is not a database: opening works, the first statement fails ("file is not a database"). The caller deletes it.
    inline std::string garbage_db_path(const std::string &name)
    {
        const std::string path = ost_test::temp_path("layers_garbage_" + ost_ptest::run_token() + "_" + name + ".db");
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "This is not a SQLite database file. It is long enough to look like one at the first read, but it is only text, "
               "so every statement on it fails. Layers tests, failed read.";
        return path;
    }

    // Rebuilds BidLayers without the column `column` (the schema of a database that lacks it, plan D13): a table WITHOUT the indexes.
    inline void rebuild_layers_without(const TempDb &db, const std::string &column)
    {
        const std::vector<std::string> all = {"UID", "BidUID", "IsTemplate", "Name", "Show", "IsLocked", "Sequence"};
        std::string defs;
        for (const std::string &name : all)
        {
            if (name == column)
                continue;
            defs += std::string(defs.empty() ? "" : ", ") + name + (name == "UID" ? " INTEGER PRIMARY KEY" : (name == "Name" ? " TEXT" : " INTEGER"));
        }
        db.must("DROP TABLE BidLayers");
        db.must("CREATE TABLE BidLayers (" + defs + ")");
    }

    // A compact line of a layer list for comparisons: "uid:name:show:sequence" joined by ','.
    inline std::string merged_text(const std::vector<ost_domain::BidLayer> &layers)
    {
        std::string out;
        for (const ost_domain::BidLayer &layer : layers)
            out += (out.empty() ? "" : ",") + layer.uid + ":" + layer.name + ":" + (layer.show ? "1" : "0") + ":" + std::to_string(layer.sequence);
        return out;
    }

    // "uid:name:visible:sequence" of an own-layers map, sorted by uid text, joined by ','.
    inline std::string own_text(const std::unordered_map<std::string, ost_domain::Layer> &layers)
    {
        std::vector<std::string> parts;
        for (const auto &entry : layers)
            parts.push_back(entry.first + ":" + entry.second.name + ":" + (entry.second.visible ? "1" : "0") + ":" + std::to_string(entry.second.sequence));
        std::sort(parts.begin(), parts.end());
        std::string out;
        for (const std::string &part : parts)
            out += (out.empty() ? "" : ",") + part;
        return out;
    }

    inline bool starts_with(const std::string &text, const std::string &prefix)
    {
        return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
    }

    inline bool contains(const std::string &text, const std::string &part)
    {
        return text.find(part) != std::string::npos;
    }

    // n copies of a UTF-8 text.
    inline std::string repeat(const std::string &unit, std::size_t n)
    {
        std::string out;
        for (std::size_t i = 0; i < n; ++i)
            out += unit;
        return out;
    }

    // U+1F600 (grinning face): 4 UTF-8 bytes, 2 UTF-16 units.
    inline const std::string kEmoji = "\xF0\x9F\x98\x80";
}
