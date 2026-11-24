// Set Scale store writer (plan 4.4 and 5.4). See ProjectStore_PageScale.hpp for the contract.
// Windows source of truth: ost_visualizer/infrastructure/mdb/components/page_operations.py:28-217 (save_page_scale, the factor, the overlay rectangle, the 17 tables, the
// final UPDATE), application/services/project_write_service.py:6306-6433 and :6885-6892, infrastructure/database/bid_owned_identity.py:205-235.
// One transaction (BEGIN IMMEDIATE) for the whole list of pages; every refusal or failed statement rolls it back, so the database is byte-identical afterwards.
#include "../ProjectStore_Internal.hpp"

#include "../../../Domain/CoreLog.hpp"
#include "../../../Domain/OverlayRect.hpp"
#include "../../../Domain/PageScaleTransform.hpp"

#include <unordered_set>

namespace ost_store
{
    namespace
    {
        // The reason of the refusal in progress. Every helper below returns false after filling it; save_page_scales rolls back and hands it out.
        struct Refusal
        {
            PageScaleRefusal reason = PageScaleRefusal::None;
            std::string text;
        };

        bool refuse(Refusal &out, PageScaleRefusal reason, const std::string &text)
        {
            out.reason = reason;
            out.text = text;
            return false;
        }

        // "Page 20 requires finite positive scale factors" (page_operations.py:75-77).
        std::string bad_scale_text(long long page)
        {
            return "Page " + std::to_string(page) + " requires finite positive scale factors";
        }

        enum class ColumnState
        {
            Present,
            Missing,
            Failed
        };

        // Whether `table` has `column`. A failed lookup is Failed, NOT Missing: a table that merely could not be read must never be skipped silently (the scale would then
        // change while the positions stay in the old units).
        ColumnState column_state(ost_db::OstDatabase &db, const std::string &table, const std::string &column)
        {
            ost_db::Statement stmt = db.prepare("SELECT [" + column + "] FROM [" + table + "] LIMIT 0");
            if (stmt.valid())
                return ColumnState::Present;
            return stmt.schema_mismatch() ? ColumnState::Missing : ColumnState::Failed;
        }

        // The cell of a stored scale as a number: INTEGER / REAL as they are, TEXT read like Python float(), NULL and BLOB are no number.
        bool read_scale_cell(const ost_db::Statement &stmt, int index, double &value)
        {
            switch (stmt.column_type(index))
            {
            case ost_db::Statement::TypeInteger:
            case ost_db::Statement::TypeFloat:
                value = stmt.column_double(index);
                return true;
            case ost_db::Statement::TypeText:
                return ost_domain::parse_python_float(stmt.column_text_exact(index), value);
            default:
                return false;
            }
        }

        // Windows require_single_bid_scope_for_uids, held to the OPEN bid (register SS-12): the page exists (NoSuchPage), has a BidUID and it is `bid` (ForeignBid).
        bool check_page_of_bid(ost_db::OstDatabase &db, long long page, long long bid, Refusal &out)
        {
            ost_db::Statement stmt = db.prepare("SELECT [BidUID] FROM [BidPages] WHERE [UID] = ?");
            if (!stmt.valid())
                return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read BidPages: " + stmt.error_message());
            stmt.bind_int64(1, page);
            const ost_db::StepResult result = stmt.step_result();
            if (result == ost_db::StepResult::Error)
                return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read BidPages: " + stmt.error_message());
            if (result == ost_db::StepResult::Done)
                return refuse(out, PageScaleRefusal::NoSuchPage, "BidPages has no row for UID " + std::to_string(page));
            if (stmt.column_type(0) == ost_db::Statement::TypeNull || stmt.column_int64(0) != bid)
                return refuse(out, PageScaleRefusal::ForeignBid, "Page " + std::to_string(page) + " does not belong to bid " + std::to_string(bid));
            return true;
        }

        // page_operations.py:84-128 _rescale_page_overlay_rect: the rectangle and its two offsets, when the page has a real rectangle.
        bool rescale_overlay(ost_db::OstDatabase &db, long long page, double factor, Refusal &out)
        {
            const ColumnState rect_column = column_state(db, "BidPages", "OverlayRect");
            if (rect_column == ColumnState::Failed)
                return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read BidPages: " + db.last_error_message());
            if (rect_column == ColumnState::Missing)
                return true; // no overlay columns: nothing to rewrite (page_operations.py:91-92)
            std::string stored;
            {
                ost_db::Statement stmt = db.prepare("SELECT [OverlayRect] FROM [BidPages] WHERE [UID] = ?");
                if (!stmt.valid())
                    return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read OverlayRect: " + stmt.error_message());
                stmt.bind_int64(1, page);
                const ost_db::StepResult result = stmt.step_result();
                if (result == ost_db::StepResult::Error)
                    return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read OverlayRect: " + stmt.error_message());
                if (result == ost_db::StepResult::Done)
                    return refuse(out, PageScaleRefusal::NoSuchPage, "Page " + std::to_string(page) + " does not exist");
                if (stmt.column_type(0) != ost_db::Statement::TypeNull)
                    stored = stmt.column_text_exact(0);
            }
            if (stored.empty())
                return true; // NULL / empty: no rectangle (page_operations.py:101-102)
            const ost_domain::OverlayRescale rect = ost_domain::rescale_overlay_rect(stored, factor);
            switch (rect.kind)
            {
            case ost_domain::OverlayRescaleKind::Untouched:
                return true;
            case ost_domain::OverlayRescaleKind::Malformed:
                return refuse(out, PageScaleRefusal::BadOverlayRect, "Page " + std::to_string(page) + ": " + rect.error);
            case ost_domain::OverlayRescaleKind::NotFinite:
                return refuse(out, PageScaleRefusal::NonFiniteResult, "Page " + std::to_string(page) + ": the overlay rectangle does not stay finite at this scale");
            case ost_domain::OverlayRescaleKind::Underflow:
                return refuse(out, PageScaleRefusal::Underflow, "Page " + std::to_string(page) + ": a value of the overlay rectangle would become 0 at this scale");
            case ost_domain::OverlayRescaleKind::BadFactor:
                return refuse(out, PageScaleRefusal::BadNewScale, bad_scale_text(page));
            case ost_domain::OverlayRescaleKind::Rescaled:
                break;
            }
            const ColumnState offset_x = column_state(db, "BidPages", "OverlayOffsetX");
            const ColumnState offset_y = column_state(db, "BidPages", "OverlayOffsetY");
            if (offset_x == ColumnState::Failed || offset_y == ColumnState::Failed)
                return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read BidPages: " + db.last_error_message());
            std::string sql = "UPDATE [BidPages] SET [OverlayRect] = ?";
            if (offset_x == ColumnState::Present)
                sql += ", [OverlayOffsetX] = ?";
            if (offset_y == ColumnState::Present)
                sql += ", [OverlayOffsetY] = ?";
            sql += " WHERE [UID] = ?";
            ost_db::Statement update = db.prepare(sql);
            if (!update.valid())
                return refuse(out, PageScaleRefusal::StatementFailed, "Cannot write OverlayRect: " + update.error_message());
            int index = 1;
            update.bind_text(index++, rect.rect_text);
            if (offset_x == ColumnState::Present)
                update.bind_double(index++, rect.offset_x);
            if (offset_y == ColumnState::Present)
                update.bind_double(index++, rect.offset_y);
            update.bind_int64(index, page);
            if (!update.run())
                return refuse(out, PageScaleRefusal::StatementFailed, "Cannot write OverlayRect: " + update.error_message());
            return true;
        }

        struct PositionRow
        {
            long long uid = 0;
            bool has_text = false;
            std::string text;
        };

        // page_operations.py:154-217 for ONE of the 17 tables.
        bool rescale_table(ost_db::OstDatabase &db, const std::string &table, long long page, double factor, Refusal &out)
        {
            std::vector<PositionRow> rows;
            {
                // One prepare covers "the table exists and has UID, BidPageUID and Position": a schema mismatch (SQLITE_ERROR) means the table is skipped silently
                // (page_operations.py:162-169); any other failure is a failed read and refuses.
                ost_db::Statement select = db.prepare("SELECT [UID], [Position] FROM [" + table + "] WHERE [BidPageUID] = ?");
                if (!select.valid())
                {
                    if (select.schema_mismatch())
                        return true;
                    return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read " + table + ": " + select.error_message());
                }
                select.bind_int64(1, page);
                for (;;)
                {
                    const ost_db::StepResult result = select.step_result();
                    if (result == ost_db::StepResult::Error)
                        return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read " + table + ": " + select.error_message());
                    if (result == ost_db::StepResult::Done)
                        break;
                    PositionRow row;
                    if (select.column_type(0) != ost_db::Statement::TypeInteger)
                        return refuse(out, PageScaleRefusal::StatementFailed, table + " has a row on page " + std::to_string(page) + " without a numeric UID");
                    row.uid = select.column_int64(0);
                    if (select.column_type(1) != ost_db::Statement::TypeNull)
                    {
                        row.has_text = true;
                        row.text = select.column_text_exact(1);
                    }
                    rows.push_back(std::move(row));
                }
            }
            if (rows.empty())
                return true;
            ost_db::Statement update = db.prepare("UPDATE [" + table + "] SET [Position] = ? WHERE [UID] = ? AND [BidPageUID] = ?");
            if (!update.valid())
                return refuse(out, PageScaleRefusal::StatementFailed, "Cannot write " + table + ": " + update.error_message());
            for (const PositionRow &row : rows)
            {
                if (!row.has_text || row.text.empty())
                    continue; // NULL / empty: `if not raw_position: continue`
                const std::string label = table + " UID " + std::to_string(row.uid);
                std::string text;
                if (table == "BidLegends")
                {
                    std::string error;
                    if (!ost_domain::rescale_legend_xml(row.text, factor, text, &error))
                        return refuse(out, PageScaleRefusal::BadLegend, label + ": " + error);
                }
                else
                {
                    const ost_domain::PositionRescale rescaled = ost_domain::rescale_position_text(table, row.text, factor);
                    switch (rescaled.kind)
                    {
                    case ost_domain::PositionRescaleKind::Empty:
                        continue;
                    case ost_domain::PositionRescaleKind::NotNumeric:
                        // SS-13 (parity): the row stays in its old units, Windows warns and goes on.
                        ost_log::warning("Skipping page-scale rescale for " + table + " UID " + std::to_string(row.uid) + " because Position is not numeric");
                        continue;
                    case ost_domain::PositionRescaleKind::NotFinite:
                        return refuse(out, PageScaleRefusal::NonFiniteResult, label + ": a coordinate is not finite or does not stay finite at this scale");
                    case ost_domain::PositionRescaleKind::Underflow:
                        return refuse(out, PageScaleRefusal::Underflow, label + ": a coordinate would become 0 at this scale");
                    case ost_domain::PositionRescaleKind::BadFactor:
                        return refuse(out, PageScaleRefusal::BadNewScale, bad_scale_text(page));
                    case ost_domain::PositionRescaleKind::Rescaled:
                        text = rescaled.text;
                        break;
                    }
                }
                update.reset();
                update.bind_text(1, text);
                update.bind_int64(2, row.uid);
                update.bind_int64(3, page);
                if (!update.run())
                    return refuse(out, PageScaleRefusal::StatementFailed, "Cannot write " + label + ": " + update.error_message());
            }
            return true;
        }

        // page_operations.py:28-82 for one page, inside the transaction.
        bool save_one_page(ost_db::OstDatabase &db, long long page, double sf1, double sf2, Refusal &out)
        {
            double stored1 = 0.0;
            double stored2 = 0.0;
            {
                ost_db::Statement stmt = db.prepare("SELECT [ScaleFactor1], [ScaleFactor2] FROM [BidPages] WHERE [UID] = ?");
                if (!stmt.valid())
                    return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read the scale of page " + std::to_string(page) + ": " + stmt.error_message());
                stmt.bind_int64(1, page);
                const ost_db::StepResult result = stmt.step_result();
                if (result == ost_db::StepResult::Error)
                    return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read the scale of page " + std::to_string(page) + ": " + stmt.error_message());
                if (result == ost_db::StepResult::Done)
                    return refuse(out, PageScaleRefusal::NoSuchPage, "Page " + std::to_string(page) + " does not exist");
                if (!read_scale_cell(stmt, 0, stored1) || !read_scale_cell(stmt, 1, stored2))
                    return refuse(out, PageScaleRefusal::BadStoredScale, bad_scale_text(page));
            }
            // overlay.py:7-19, the strict rule, for the stored and the new pair (SS-1: a stored scale that is not usable refuses every Set Scale of the page, no repair).
            const std::optional<double> source = ost_domain::units_per_sheet_inch(stored1, stored2);
            if (!source)
                return refuse(out, PageScaleRefusal::BadStoredScale, bad_scale_text(page));
            const std::optional<double> target = ost_domain::units_per_sheet_inch(sf1, sf2);
            if (!target)
                return refuse(out, PageScaleRefusal::BadNewScale, bad_scale_text(page));
            const double factor = *target / *source;
            if (!std::isfinite(factor) || !(factor > 0.0))
                return refuse(out, PageScaleRefusal::BadNewScale, bad_scale_text(page)); // the ratio of the two ratios overflows or underflows (SS-2)

            if (ost_domain::factor_needs_rescale(factor))
            {
                if (!rescale_overlay(db, page, factor, out))
                    return false;
                for (const std::string &table : ost_domain::page_position_tables())
                {
                    if (!rescale_table(db, table, page, factor, out))
                        return false;
                }
            }
            // The new pair exactly as given, also when the factor was within the epsilon (page_operations.py:44-47).
            ost_db::Statement update = db.prepare("UPDATE [BidPages] SET [ScaleFactor1] = ?, [ScaleFactor2] = ? WHERE [UID] = ?");
            if (!update.valid())
                return refuse(out, PageScaleRefusal::StatementFailed, "Cannot write the scale of page " + std::to_string(page) + ": " + update.error_message());
            update.bind_double(1, sf1);
            update.bind_double(2, sf2);
            update.bind_int64(3, page);
            if (!update.run())
                return refuse(out, PageScaleRefusal::StatementFailed, "Cannot write the scale of page " + std::to_string(page) + ": " + update.error_message());
            return true;
        }

        // Everything after BEGIN: the checks of plan 4.4 in order, then every page in the order given. true = commit.
        bool save_in_transaction(ost_db::OstDatabase &db, long long bid, const std::vector<long long> &pages, double sf1, double sf2, Refusal &out)
        {
            if (!integrity::bid_exists(db, bid))
                return refuse(out, PageScaleRefusal::NoSuchBid, "Bids has no row for UID " + std::to_string(bid));
            for (const long long page : pages)
            {
                if (!check_page_of_bid(db, page, bid, out))
                    return false;
            }
            bool locked = true;
            const bool read_ok = integrity::bid_is_locked(db, bid, locked);
            if (!read_ok || locked)
                return refuse(out, PageScaleRefusal::Locked, read_ok ? "This bid is locked." : "The lock state of this bid could not be read.");
            for (const char *column : {"ScaleFactor1", "ScaleFactor2"})
            {
                const ColumnState state = column_state(db, "BidPages", column);
                if (state == ColumnState::Missing)
                    return refuse(out, PageScaleRefusal::MissingColumns, std::string("Missing BidPages.") + column);
                if (state == ColumnState::Failed)
                    return refuse(out, PageScaleRefusal::StatementFailed, "Cannot read BidPages: " + db.last_error_message());
            }
            for (const long long page : pages)
            {
                if (!save_one_page(db, page, sf1, sf2, out))
                    return false;
            }
            return true;
        }
    }

    bool save_page_scales(const std::string &db_path, const std::string &bid_uid, const std::vector<std::string> &page_uids, double sf1, double sf2,
                          PageScaleOutcome *outcome)
    {
        Refusal refusal;
        auto finish = [&](bool saved)
        {
            if (outcome)
            {
                outcome->refusal = saved ? PageScaleRefusal::None : refusal.reason;
                outcome->error = saved ? std::string() : refusal.text;
            }
            return saved;
        };

        // 1. the uids: the bid and every non-empty page uid must be positive integers; empty uids are dropped, a page named twice (by value: "7" and "007") counts once.
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return finish(refuse(refusal, PageScaleRefusal::MalformedUid, "The bid uid \"" + bid_uid + "\" is not a positive whole number"));
        std::vector<long long> pages;
        std::unordered_set<long long> seen;
        for (const std::string &text : page_uids)
        {
            if (text.empty())
                continue; // project_write_service.py:6885-6892 _unique_nonempty_uids
            long long page = 0;
            if (!integrity::parse_uid(text, page))
                return finish(refuse(refusal, PageScaleRefusal::MalformedUid, "The page uid \"" + text + "\" is not a positive whole number"));
            if (seen.insert(page).second)
                pages.push_back(page);
        }
        if (pages.empty())
            return finish(refuse(refusal, PageScaleRefusal::EmptyBatch, "No page to scale")); // the database is not even opened

        try
        {
            ost_db::OstDatabase db;
            if (!db.open(db_path))
                return finish(refuse(refusal, PageScaleRefusal::StatementFailed, "Cannot open the database"));
            if (!db.begin_transaction())
                return finish(refuse(refusal, PageScaleRefusal::StatementFailed, "Cannot start a transaction: " + db.last_error_message()));
            if (!save_in_transaction(db, bid, pages, sf1, sf2, refusal))
            {
                db.rollback();
                return finish(false);
            }
            if (!db.commit())
                return finish(refuse(refusal, PageScaleRefusal::StatementFailed, "Cannot commit: " + db.last_error_message())); // a failed COMMIT rolls itself back
            return finish(true);
        }
        catch (...)
        {
            // nothing throws across the Objective-C boundary; the connection of the scope above was closed by its destructor (an open transaction is rolled back)
            return finish(refuse(refusal, PageScaleRefusal::StatementFailed, "The change of scale failed unexpectedly"));
        }
    }

    bool save_page_scale(const std::string &db_path, const std::string &bid_uid, const std::string &page_uid, double sf1, double sf2, PageScaleOutcome *outcome)
    {
        return save_page_scales(db_path, bid_uid, std::vector<std::string>{page_uid}, sf1, sf2, outcome);
    }
}
