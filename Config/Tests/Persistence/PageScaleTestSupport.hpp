// PageScaleTestSupport.hpp - fixtures and helpers of the Set Scale store tests (plan 4.12: a stream puts its helpers in its own <Feature>TestSupport.hpp).
//
// Two kinds of database:
//   - make_windows_scale_db: the loosely typed database of tests/infrastructure/mdb/components/test_page_operations.py:1781-1852 (PageScaleChangeContentTests.database): only
//     the tables and columns the Windows test creates (tables missing, columns missing, Position BLOB columns, a legend that is a BLOB), so the "skipped silently" rules and the
//     end states can be ported 1:1. Bids carries JobStatusUID (NULL = unlocked) because the lock guard reads it;
//   - seed_scale_world: the real iOS schema (create_schema) with bids 10 and 20, their pages and ONE row with a position in each of the 17 tables for each page, so the
//     whole-database snapshot tests see every table.
#pragma once

#include "StoreTestSupport.hpp"

#include "Domain/CoreLog.hpp"

#include <string>
#include <utility>
#include <vector>

namespace ost_ptest
{
    // ost_log records of a scope (what the Windows tests call assertLogs / the fake logger of scale_position_support._Logger).
    class ScaleLogCapture
    {
    public:
        struct Record
        {
            ost_log::Level level;
            std::string message;
        };

        ScaleLogCapture()
        {
            ost_log::set_sink([this](ost_log::Level level, const std::string &message, const std::string &)
                              { records.push_back(Record{level, message}); });
        }
        ~ScaleLogCapture() { ost_log::set_sink(ost_log::Sink()); }
        ScaleLogCapture(const ScaleLogCapture &) = delete;
        ScaleLogCapture &operator=(const ScaleLogCapture &) = delete;

        std::vector<std::string> warnings() const
        {
            std::vector<std::string> out;
            for (const Record &record : records)
                if (record.level == ost_log::Level::Warning)
                    out.push_back(record.message);
            return out;
        }

        std::vector<Record> records;
    };

    // The 12 tables the Windows PageScaleChangeContentTests fixture creates (_SCALED_TABLES; BidALines and BidHighlights are deliberately never created: absent tables are
    // skipped without ending the scan of the tables that follow them).
    inline const std::vector<std::string> &windows_scaled_tables()
    {
        static const std::vector<std::string> tables = {"BidTakeoffs",        "BidDimensions", "BidArrows",    "BidCallOuts",  "BidAnnotationPolygons", "BidAnnotationClouds",
                                                        "BidAnnotationRects", "BidAnnotationOvals", "BidAnnoInk", "BidTexts", "BidComments", "BidTypGroupViews"};
        return tables;
    }

    // test_page_operations.py:1775 _LEGEND_XML.
    inline const char *windows_legend_xml()
    {
        return "<Legends dX=\"30.804\" dY=\"1022.679\" Visible=\"255\"><Legend ConditionUID=\"11334\" dX=\"30.804\" dY=\"30.804\" Visible=\"1\"/></Legends>";
    }

    struct WindowsScaleOptions
    {
        bool overlay_columns = true;  // OverlayRect, OverlayOffsetX, OverlayOffsetY on BidPages
        bool broken_legend = false;   // legend 1 of page 20 is the BLOB X'3C4C6567656E6473' ("<Legends")
        bool bid_status_column = true; // Bids.JobStatusUID (the lock guard reads it; NULL = unlocked)
    };

    // test_page_operations.py:1784-1852 PageScaleChangeContentTests.database(): Bids (1); pages 20 and 21, both (0.125, 12.0) (page 20 with the rect "10,20,30,40" and the
    // offsets 10.0 / 20.0, page 21 with "" / 0.0 / 0.0); in each of the 12 tables the rows 3 (page 20, NULL), 4 (page 20, 'not-numeric'), 1 (page 20, "10;20;30;40", BidTexts
    // "5;6;7;8;0.5"), 2 (page 21, the same position), BidTexts also row 5 (page 20, "1;2;3;4;0.5;9;10"); BidLegends rows 3 (page 20, NULL), 1 (page 20, the legend), 4 (page 20,
    // the legend), 2 (page 21, the legend); BidHotLinks without BidPageUID and BidNamedViews without Position (both skipped by the rewrite). `db` must be made WITHOUT the schema.
    inline void make_windows_scale_db(const TempDb &db, const WindowsScaleOptions &options = WindowsScaleOptions())
    {
        db.must(options.bid_status_column ? "CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)" : "CREATE TABLE Bids (UID INTEGER PRIMARY KEY)");
        db.must("INSERT INTO Bids (UID) VALUES (1)");
        std::string pages = "CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, ScaleFactor1 REAL, ScaleFactor2 REAL";
        pages += options.overlay_columns ? ", OverlayRect TEXT, OverlayOffsetX REAL, OverlayOffsetY REAL)" : ")";
        db.must(pages);
        db.must(std::string("INSERT INTO BidPages VALUES (20, 1, 0.125, 12.0") + (options.overlay_columns ? ", '10,20,30,40', 10.0, 20.0)" : ")"));
        db.must(std::string("INSERT INTO BidPages VALUES (21, 1, 0.125, 12.0") + (options.overlay_columns ? ", '', 0.0, 0.0)" : ")"));
        for (const std::string &table : windows_scaled_tables())
        {
            const std::string position = table == "BidTexts" ? "5;6;7;8;0.5" : "10;20;30;40";
            db.must("CREATE TABLE [" + table + "] (UID INTEGER, BidPageUID INTEGER, Position BLOB)");
            // unusable rows come first so that skipping them must not end the table
            db.must("INSERT INTO [" + table + "] VALUES (3, 20, NULL)");
            db.must("INSERT INTO [" + table + "] VALUES (4, 20, 'not-numeric')");
            db.must("INSERT INTO [" + table + "] VALUES (1, 20, '" + position + "')");
            db.must("INSERT INTO [" + table + "] VALUES (2, 21, '" + position + "')");
        }
        db.must("INSERT INTO BidTexts VALUES (5, 20, '1;2;3;4;0.5;9;10')");
        db.must("CREATE TABLE BidLegends (UID INTEGER, BidPageUID INTEGER, Position BLOB)");
        db.must("INSERT INTO BidLegends VALUES (3, 20, NULL)");
        db.must(std::string("INSERT INTO BidLegends VALUES (1, 20, ") + (options.broken_legend ? "X'3C4C6567656E6473'" : std::string("'") + windows_legend_xml() + "'") + ")");
        db.must(std::string("INSERT INTO BidLegends VALUES (4, 20, '") + windows_legend_xml() + "')");
        db.must(std::string("INSERT INTO BidLegends VALUES (2, 21, '") + windows_legend_xml() + "')");
        db.must("CREATE TABLE BidHotLinks (UID INTEGER, Position BLOB)");
        db.must("INSERT INTO BidHotLinks VALUES (1, '1;2;3;4')");
        db.must("CREATE TABLE BidNamedViews (UID INTEGER, BidPageUID INTEGER)");
        db.must("INSERT INTO BidNamedViews VALUES (1, 20)");
    }

    // The Position of one row as stored ("<none>" for no row, "NULL" for a NULL cell).
    inline std::string position_of(const TempDb &db, const std::string &table, long long uid)
    {
        return db.text("SELECT ifnull(Position, 'NULL') FROM [" + table + "] WHERE UID = " + std::to_string(uid));
    }

    // The SQLite storage class of the cell (text / blob / real / null / integer; "<none>" for no row).
    inline std::string type_of(const TempDb &db, const std::string &table, const std::string &column, long long uid)
    {
        return db.text("SELECT typeof([" + column + "]) FROM [" + table + "] WHERE UID = " + std::to_string(uid));
    }

    // The 17 tables of the Windows rewrite in the Windows order (constants.py PAGE_CONTENT_TABLES + BidTypGroupViews).
    inline const std::vector<std::string> &scale_position_tables()
    {
        static const std::vector<std::string> tables = {"BidTakeoffs",         "BidDimensions",      "BidALines",          "BidArrows",         "BidHighlights",
                                                        "BidTexts",            "BidCallOuts",        "BidAnnotationRects", "BidAnnotationOvals", "BidAnnotationPolygons",
                                                        "BidAnnotationClouds", "BidAnnoInk",         "BidLegends",         "BidComments",       "BidNamedViews",
                                                        "BidHotLinks",         "BidTypGroupViews"};
        return tables;
    }

    // A snapshot (PersistenceTestSupport.hpp snapshot_all, same text form) of EVERY user table except `excluded`: the "nothing else changed" proof of a change of scale
    // (the tables of register SS-8 / SS-9 - ScaleStyle, Quantity, the totals, BidAreaTranslations - stay byte-identical). "<cannot open>" when the table list cannot be read.
    inline std::string snapshot_except(const TempDb &db, const std::vector<std::string> &excluded)
    {
        std::vector<std::string> tables;
        {
            ost_db::OstDatabase connection;
            if (!connection.open(db.path()))
                return "<cannot open>";
            ost_db::Statement stmt = connection.prepare("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name");
            if (!stmt.valid())
                return "<sqlite_master failed>";
            while (stmt.step())
            {
                const std::string name = stmt.column_text(0);
                bool skip = false;
                for (const std::string &other : excluded)
                    skip = skip || other == name;
                if (!skip)
                    tables.push_back(name);
            }
        }
        return db.snapshot(tables);
    }

    // The Position text of a row of the world below: one per table, so that a test knows what it should read after a change of scale. BidTexts has a rotation slot.
    inline std::string world_position(const std::string &table)
    {
        if (table == "BidLegends")
            return windows_legend_xml();
        if (table == "BidTexts")
            return "10;20;30;40;0.5";
        if (table == "BidAnnoInk")
            return "0.5;10;20;30;40";
        return "10;20;30;40";
    }

    // The uid of the row of `table` on the page `page_index` (0: page 100 of bid 10, 1: page 101 of bid 10, 2: page 200 of bid 20): 10000 * (table index + 1) + page_index.
    inline long long world_row_uid(const std::string &table, int page_index)
    {
        long long index = 0;
        for (const std::string &name : scale_position_tables())
        {
            if (name == table)
                break;
            ++index;
        }
        return 10000 * (index + 1) + page_index;
    }

    // seed_two_bids (bid 10: pages 100 and 101; bid 20: page 200; takeoff 900 on page 200) with the three pages at (0.125, 12.0), the rectangle "10,20,30,40" with the offsets
    // 10 / 20 on page 100, and ONE row in each of the 17 tables for each of the three pages (uid world_row_uid, Position world_position, BidUID the page's bid). The
    // database comes from create_schema (the real iOS schema).
    inline void seed_scale_world(const TempDb &db)
    {
        seed_two_bids(db);
        db.must("UPDATE BidPages SET ScaleFactor1 = 0.125, ScaleFactor2 = 12.0, ScaleStyle = 1, IsCustomScale = 0, Scale = 96.0");
        db.must("UPDATE BidPages SET OverlayRect = '10,20,30,40', OverlayOffsetX = 10.0, OverlayOffsetY = 20.0 WHERE UID = 100");
        const struct
        {
            int index;
            long long page;
            long long bid;
        } pages[] = {{0, 100, 10}, {1, 101, 10}, {2, 200, 20}};
        for (const std::string &table : scale_position_tables())
        {
            for (const auto &page : pages)
            {
                db.must("INSERT INTO [" + table + "] (UID, BidUID, BidPageUID, Position) VALUES (" + std::to_string(world_row_uid(table, page.index)) + ", " +
                        std::to_string(page.bid) + ", " + std::to_string(page.page) + ", " + quoted(world_position(table)) + ")");
            }
        }
    }

    // The stored scale pair of a page as text "sf1;sf2" (REAL columns: %.17g), "<none>" for no page.
    inline std::string scale_of(const TempDb &db, long long page_uid)
    {
        return db.text("SELECT ifnull(ScaleFactor1, 'NULL') || ';' || ifnull(ScaleFactor2, 'NULL') FROM BidPages WHERE UID = " + std::to_string(page_uid));
    }

    // The exact doubles of a page's scale pair (a NULL cell reads as 0).
    inline std::pair<double, double> scale_values(const TempDb &db, long long page_uid)
    {
        ost_db::OstDatabase connection;
        if (!connection.open(db.path()))
            return {-1.0, -1.0};
        ost_db::Statement stmt = connection.prepare("SELECT ScaleFactor1, ScaleFactor2 FROM BidPages WHERE UID = ?");
        if (!stmt.valid())
            return {-1.0, -1.0};
        stmt.bind_int64(1, page_uid);
        if (!stmt.step())
            return {-1.0, -1.0};
        return {stmt.column_double(0), stmt.column_double(1)};
    }
}
