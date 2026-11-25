// ProjectStore component - layers: create, rename, delete, reorder, the usage read and the layers-only reload (plan 5.3).
//
// Windows source of truth (49a2d59): infrastructure/mdb/components/layer_operations.py insert_layer :78-114, delete_layer :153-217,
// update_layer_name :385-410, swap_layer_sequence :432-465; identity_allocation.py:5-39; bid_owned_identity.py:105-152, :205-235;
// constants.py:122-134 (LAYER_REFERENCE_TABLES); settings_reader.py:450-482 (get_layer_uids_in_use). The contract of every function and
// the deliberate differences (register LY-1 to LY-14) are in ProjectStore_Layers.hpp.
//
// The write pattern of plan 4.4: every writer is ONE transaction (BEGIN IMMEDIATE ... COMMIT) in this order: parse the uids, the bid
// exists, the rows belong to the OPEN bid and are editable, the bid lock (on the same connection), the name / value rules (re-run here on
// the rows read inside the transaction), the 32-bit limits, the writes, commit. ANY failed statement is a rollback and a negative result;
// nothing throws: every function below is a function-try-block, so an exception (std::bad_alloc, std::length_error) that passes through the
// body ends the call with the same negative result a failed statement gives. The locals of the body (the connection and its statements) are
// destroyed before the handler runs, and closing the connection rolls an open transaction back, so a throw never leaves one open or half a
// write behind.
#include "../ProjectStore_Internal.hpp"
#include "../../../Domain/LayerEdit.hpp"

#include <cctype>
#include <cstring>
#include <initializer_list>

namespace ost_store
{

    namespace
    {
        // constants.py:122-134 LAYER_REFERENCE_TABLES, in the Windows order: the tables whose BidLayerUID names a BidLayers row.
        const char *const kReferenceTables[] = {"BidConditions",       "BidZones",         "BidComments",   "BidAnnotationClouds",
                                                "BidAnnotationOvals",  "BidAnnotationPolygons", "BidAnnotationRects", "BidTexts",
                                                "BidHighlights",       "BidHotLinks",      "BidCallOuts"};

        enum class Probe
        {
            Failed,  // the lookup itself failed: never read as "the table is missing"
            Missing, // the table does not exist
            Present
        };

        // The columns of `table` (pragma_table_info). A statement that fails (a file that is not a database, a busy database) is Failed, so
        // a caller never treats an unreadable database as one that simply lacks the table (OstDatabase::table_exists does: "a failed lookup
        // reports not found").
        Probe table_columns(ost_db::OstDatabase &db, const char *table, std::vector<std::string> &columns, std::string *why = nullptr)
        {
            columns.clear();
            ost_db::Statement info = db.prepare(std::string("SELECT name FROM pragma_table_info('") + table + "')");
            if (!info.valid())
            {
                if (why)
                    *why = "Cannot read " + std::string(table) + ": " + info.error_message();
                return Probe::Failed;
            }
            while (info.step())
                columns.push_back(info.column_text(0));
            if (info.failed())
            {
                if (why)
                    *why = "Cannot read " + std::string(table) + ": " + info.error_message();
                return Probe::Failed;
            }
            return columns.empty() ? Probe::Missing : Probe::Present;
        }

        bool has_column(const std::vector<std::string> &columns, const char *wanted)
        {
            for (const std::string &column : columns)
            {
                if (column.size() != std::strlen(wanted))
                    continue;
                bool same = true;
                for (std::size_t i = 0; i < column.size() && same; ++i)
                    same = std::tolower(static_cast<unsigned char>(column[i])) == std::tolower(static_cast<unsigned char>(wanted[i]));
                if (same)
                    return true;
            }
            return false;
        }

        // D13 (research 9): a BidLayers table without a column the write needs is refused, nothing is written (Windows
        // _require_write_columns: "Missing BidLayers.<col>").
        bool layer_columns_present(ost_db::OstDatabase &db, std::initializer_list<const char *> required)
        {
            for (const char *column : required)
            {
                if (!db.column_exists("BidLayers", column))
                    return false;
            }
            return true;
        }

        // plan 4.1: the bid lock on the writer's own connection. A lock that cannot be read refuses (fail closed).
        bool bid_allows_writes(ost_db::OstDatabase &db, long long bid)
        {
            bool locked = true;
            if (!integrity::bid_is_locked(db, bid, locked))
                return false;
            return !locked;
        }

        // The layer is one of the OPEN bid's own rows (LY-9: Windows checks "one bid", not the open one) with IsTemplate = 0 AND IsLocked = 0
        // (layer_operations.py:168-172, :389-393): a shared template row (no bid), a layer of another bid, the bid's own locked copy and a row
        // whose flag is NULL or nonzero (LY-10: `= 0` does not match NULL, fail closed) are all refused. `stored_name`, when given, receives the
        // name exactly as stored.
        bool read_editable_layer(ost_db::OstDatabase &db, long long layer, long long bid, std::string *stored_name)
        {
            if (!integrity::row_belongs_to_bid(db, "BidLayers", layer, bid))
                return false;
            ost_db::Statement stmt = db.prepare(
                "SELECT [Name] FROM [BidLayers] WHERE [UID] = ? AND [BidUID] = ? AND [IsTemplate] = 0 AND [IsLocked] = 0");
            if (!stmt.valid())
                return false;
            stmt.bind_int64(1, layer);
            stmt.bind_int64(2, bid);
            const ost_db::StepResult result = stmt.step_result();
            if (result != ost_db::StepResult::Row)
                return false;
            if (stored_name)
                *stored_name = stmt.column_text_exact(0);
            return true;
        }

        enum class SequenceRead
        {
            Failed,
            Null,
            Value
        };

        // The Sequence of one row as a 64-bit integer: BidLayer::sequence is an int and column_int truncates, so a stored value beyond 32 bit
        // must be read as 64 bit. A value that is neither NULL nor an integer is Failed (fail closed).
        SequenceRead read_sequence(ost_db::OstDatabase &db, long long layer, long long &value)
        {
            ost_db::Statement stmt = db.prepare("SELECT [Sequence] FROM [BidLayers] WHERE [UID] = ?");
            if (!stmt.valid())
                return SequenceRead::Failed;
            stmt.bind_int64(1, layer);
            if (stmt.step_result() != ost_db::StepResult::Row)
                return SequenceRead::Failed;
            switch (stmt.column_type(0))
            {
            case ost_db::Statement::TypeNull:
                return SequenceRead::Null;
            case ost_db::Statement::TypeInteger:
                value = stmt.column_int64(0);
                return SequenceRead::Value;
            default:
                return SequenceRead::Failed;
            }
        }

        // Hardening beyond Windows (Access cannot hold it): no row of the bid has a Sequence that is neither NULL nor an integer, because the
        // shift (`Sequence + 1`) and the renumber (`Sequence - 1`) of text or real values would not mean what the limit checks assume.
        bool bid_sequences_are_integers(ost_db::OstDatabase &db, long long bid)
        {
            ost_db::Statement stmt = db.prepare(
                "SELECT COUNT(*) FROM [BidLayers] WHERE [BidUID] = ? AND [Sequence] IS NOT NULL AND typeof([Sequence]) <> 'integer'");
            if (!stmt.valid())
                return false;
            stmt.bind_int64(1, bid);
            if (stmt.step_result() != ost_db::StepResult::Row)
                return false;
            return stmt.column_int64(0) == 0;
        }
    } // namespace

    std::string insert_layer(const std::string &db_path,
                             const std::string &bid_uid,
                             const std::string &name,
                             long long after_sequence,
                             std::string *error)
    try
    {
        if (error)
            error->clear();
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return "";

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return "";
        if (!db.begin_transaction())
            return "";
        auto fail = [&db]() -> std::string
        {
            db.rollback();
            return "";
        };
        // A refusal the user must see: the text goes to `error`, nothing is written.
        auto refuse = [&](const std::string &text) -> std::string
        {
            if (error)
                *error = text;
            return fail();
        };

        // layer_operations.py:83 required columns UID, BidUID, Name (D13)
        if (!layer_columns_present(db, {"UID", "BidUID", "Name"}))
            return fail();
        // layer_operations.py:85 the bid must exist
        if (!integrity::bid_exists(db, bid))
            return fail();
        // plan 4.1: the bid lock, inside the transaction
        if (!bid_allows_writes(db, bid))
            return fail();

        // The name rules (LY-1, LY-8, LY-14 and the Windows dialog rules) on the rows as they are NOW, so that no screen can bypass them.
        const std::string bid_text = std::to_string(bid);
        std::string stripped_name; // the name to store
        {
            std::vector<ost_domain::BidLayer> listed;
            std::unordered_map<std::string, ost_domain::Layer> own_layers;
            std::vector<std::string> own_uids;
            std::string why;
            if (!layer_reads::read_merged_layers(db, bid_text, listed, why) ||
                !layer_reads::read_own_layers(db, bid_text, own_layers, own_uids, why))
                return fail();
            const ost_domain::LayerNameCheck check = ost_domain::check_layer_name(name, "", listed, own_layers, "");
            if (check.verdict == ost_domain::LayerNameVerdict::Blank || check.verdict == ost_domain::LayerNameVerdict::Unchanged)
                return fail(); // nothing to say: the screens ignore a blank name
            if (check.verdict != ost_domain::LayerNameVerdict::Accept)
                return refuse(check.message);
            stripped_name = check.name;
        }

        // LY-7: refuse BEFORE anything is shifted. The new row gets after_sequence + 1; a later row of the bid (Sequence > after) moves up by
        // one. All arithmetic is 64 bit and the stored values are read as 64 bit.
        if (!bid_sequences_are_integers(db, bid))
            return fail();
        long long highest_later = -1; // -1: no later row (layer_sequence_would_overflow)
        {
            ost_db::Statement later = db.prepare("SELECT MAX([Sequence]) FROM [BidLayers] WHERE [BidUID] = ? AND [Sequence] > ?");
            if (!later.valid())
                return fail();
            later.bind_int64(1, bid);
            later.bind_int64(2, after_sequence);
            if (later.step_result() != ost_db::StepResult::Row)
                return fail();
            if (later.column_type(0) == ost_db::Statement::TypeInteger)
                highest_later = later.column_int64(0);
            else if (later.column_type(0) != ost_db::Statement::TypeNull)
                return fail();
        }
        if (ost_domain::layer_sequence_would_overflow(after_sequence, highest_later))
            return refuse(ost_domain::kLayerSequenceLimitText);

        // identity_allocation.py:5-39 MAX(UID) + 1 raised above every dangling BidLayerUID; plan 4.2: refused past 2147483647
        long long new_uid = 0;
        bool no_identity_left = false;
        if (!integrity::next_uid_within_access_limit(db, "BidLayers", 1, new_uid, &no_identity_left))
            return no_identity_left ? refuse(integrity::no_identity_left_text("layer")) : fail();

        // layer_operations.py:87-93 the later rows of THIS bid move up by one (shared template rows have no bid, a NULL Sequence never
        // moves, other bids never move)
        {
            ost_db::Statement shift = db.prepare("UPDATE [BidLayers] SET [Sequence] = [Sequence] + 1 WHERE [BidUID] = ? AND [Sequence] > ?");
            if (!shift.valid())
                return fail();
            shift.bind_int64(1, bid);
            shift.bind_int64(2, after_sequence);
            if (!shift.run())
                return fail();
        }
        // layer_operations.py:96-113 the new row: Show -1, Sequence after + 1, IsTemplate 0, IsLocked 0
        {
            ost_db::Statement insert = db.prepare(
                "INSERT INTO [BidLayers] ([UID], [BidUID], [Name], [Show], [Sequence], [IsTemplate], [IsLocked]) "
                "VALUES (?, ?, ?, -1, ?, 0, 0)");
            if (!insert.valid())
                return fail();
            insert.bind_int64(1, new_uid);
            insert.bind_int64(2, bid);
            insert.bind_text(3, stripped_name);
            insert.bind_int64(4, after_sequence + 1);
            if (!insert.run())
                return fail();
        }
        if (!db.commit())
            return "";
        return std::to_string(new_uid);
    }
    catch (...)
    {
        // a failed write that has nothing to tell the user: no text, nothing written (the connection of the body is closed, which rolled back)
        if (error)
            error->clear();
        return "";
    }

    bool rename_layer(const std::string &db_path,
                      const std::string &bid_uid,
                      const std::string &layer_uid,
                      const std::string &name,
                      std::string *error)
    try
    {
        if (error)
            error->clear();
        long long bid = 0;
        long long layer = 0;
        if (!integrity::parse_uid(bid_uid, bid) || !integrity::parse_uid(layer_uid, layer))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        if (!db.begin_transaction())
            return false;
        auto fail = [&db]() -> bool
        {
            db.rollback();
            return false;
        };

        // layer_operations.py:388 required columns UID, Name (D13)
        if (!layer_columns_present(db, {"UID", "BidUID", "Name"}))
            return fail();
        if (!integrity::bid_exists(db, bid))
            return fail();
        // the row belongs to the open bid and is editable: BEFORE the lock and the name rules (a locked copy is refused even with its own name)
        std::string stored_name;
        if (!read_editable_layer(db, layer, bid, &stored_name))
            return fail();
        if (!bid_allows_writes(db, bid))
            return fail();

        const std::string bid_text = std::to_string(bid);
        std::vector<ost_domain::BidLayer> listed;
        std::unordered_map<std::string, ost_domain::Layer> own_layers;
        std::vector<std::string> own_uids;
        std::string why;
        if (!layer_reads::read_merged_layers(db, bid_text, listed, why) || !layer_reads::read_own_layers(db, bid_text, own_layers, own_uids, why))
            return fail();
        const ost_domain::LayerNameCheck check =
            ost_domain::check_layer_name(name, stored_name, listed, own_layers, std::to_string(layer));
        switch (check.verdict)
        {
        case ost_domain::LayerNameVerdict::Accept:
            break;
        case ost_domain::LayerNameVerdict::Unchanged:
            // layers_dialog.py:401-402: the same name (exact, after the strip) is a success that writes nothing
            db.rollback();
            return true;
        case ost_domain::LayerNameVerdict::Blank:
            return fail();
        default:
            if (error)
                *error = check.message;
            return fail();
        }

        // layer_operations.py:396-401: only Name changes (the SQL guard is repeated, the row was read under the write lock just above)
        ost_db::Statement update = db.prepare(
            "UPDATE [BidLayers] SET [Name] = ? WHERE [UID] = ? AND [BidUID] = ? AND [IsTemplate] = 0 AND [IsLocked] = 0");
        if (!update.valid())
            return fail();
        update.bind_text(1, check.name);
        update.bind_int64(2, layer);
        update.bind_int64(3, bid);
        if (!update.run())
            return fail();
        return db.commit();
    }
    catch (...)
    {
        // the same result as a failed statement: false, no text, nothing written
        if (error)
            error->clear();
        return false;
    }

    bool delete_layers(const std::string &db_path,
                       const std::string &bid_uid,
                       const std::vector<std::string> &layer_uids)
    try
    {
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return false;
        // _unique_nonempty_uids (project_write_service.py:6884-6892): each uid once, in order; "007" and "7" are the same layer
        std::vector<long long> uids;
        std::set<long long> seen;
        for (const std::string &text : layer_uids)
        {
            long long uid = 0;
            if (!integrity::parse_uid(text, uid))
                return false;
            if (seen.insert(uid).second)
                uids.push_back(uid);
        }

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        if (!db.begin_transaction())
            return false;
        auto fail = [&db]() -> bool
        {
            db.rollback();
            return false;
        };

        // layer_operations.py:156 required column UID (D13)
        if (!layer_columns_present(db, {"UID", "BidUID"}))
            return fail();
        if (!integrity::bid_exists(db, bid))
            return fail();
        // LY-4 / LY-9: EVERY uid must be an editable layer of the open bid, else nothing is deleted (Windows breaks at the first refused
        // layer and commits the earlier deletes)
        for (const long long uid : uids)
        {
            if (!read_editable_layer(db, uid, bid, nullptr))
                return fail();
        }
        if (!bid_allows_writes(db, bid))
            return fail();
        if (uids.empty())
        {
            db.rollback(); // an empty list is a success that writes nothing
            return true;
        }

        // layer_operations.py:184-188: the reference tables that exist and have a BidLayerUID column. A lookup that FAILS aborts (it is never
        // "the table is missing": the layer would then be deleted with a reference left in it).
        std::vector<const char *> clear_tables;
        for (const char *table : kReferenceTables)
        {
            std::vector<std::string> columns;
            const Probe probe = table_columns(db, table, columns);
            if (probe == Probe::Failed)
                return fail();
            if (probe == Probe::Present && has_column(columns, "BidLayerUID"))
                clear_tables.push_back(table);
        }
        if (!bid_sequences_are_integers(db, bid))
            return fail();

        for (const long long uid : uids)
        {
            // the Sequence the row has at THIS moment (earlier deletes of the call already renumbered), layer_operations.py:181
            long long deleted_sequence = 0;
            const SequenceRead sequence = read_sequence(db, uid, deleted_sequence);
            if (sequence == SequenceRead::Failed)
                return fail();

            // layer_operations.py:184-203 BidLayerUID = NULL in each table, NOT scoped to the bid. LY-5: a failed clear aborts everything
            // (the lenient branch of Windows is dead code in production).
            for (const char *table : clear_tables)
            {
                ost_db::Statement clear = db.prepare(std::string("UPDATE [") + table + "] SET [BidLayerUID] = NULL WHERE [BidLayerUID] = ?");
                if (!clear.valid())
                    return fail();
                clear.bind_int64(1, uid);
                if (!clear.run())
                    return fail();
            }
            {
                ost_db::Statement remove = db.prepare("DELETE FROM [BidLayers] WHERE [UID] = ?");
                if (!remove.valid())
                    return fail();
                remove.bind_int64(1, uid);
                if (!remove.run())
                    return fail();
            }
            // layer_operations.py:205-216 the later rows of the same bid move down by one; a row without a Sequence renumbers nothing
            if (sequence == SequenceRead::Value)
            {
                ost_db::Statement renumber = db.prepare("UPDATE [BidLayers] SET [Sequence] = [Sequence] - 1 WHERE [BidUID] = ? AND [Sequence] > ?");
                if (!renumber.valid())
                    return fail();
                renumber.bind_int64(1, bid);
                renumber.bind_int64(2, deleted_sequence);
                if (!renumber.run())
                    return fail();
            }
        }
        return db.commit();
    }
    catch (...)
    {
        return false; // nothing deleted (the connection of the body is closed, which rolled back)
    }

    bool swap_layer_sequence(const std::string &db_path,
                             const std::string &bid_uid,
                             const std::string &uid_a,
                             const std::string &uid_b)
    try
    {
        long long bid = 0;
        long long a = 0;
        long long b = 0;
        if (!integrity::parse_uid(bid_uid, bid) || !integrity::parse_uid(uid_a, a) || !integrity::parse_uid(uid_b, b))
            return false;
        // layer_operations.py:448-449 a == b is refused; the uids are numbers, so "7" and "007" are the same layer (LY-13)
        if (a == b)
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        if (!db.begin_transaction())
            return false;
        auto fail = [&db]() -> bool
        {
            db.rollback();
            return false;
        };

        // layer_operations.py:435 required columns UID, Sequence (D13)
        if (!layer_columns_present(db, {"UID", "BidUID", "Sequence"}))
            return fail();
        if (!integrity::bid_exists(db, bid))
            return fail();
        // LY-6 / LY-9: both rows are the open bid's own (a shared template row has no bid and is refused); the bid's own locked copies are
        // allowed on either side (N-M1-19), Windows has no lock or template filter on the two rows
        if (!integrity::row_belongs_to_bid(db, "BidLayers", a, bid) || !integrity::row_belongs_to_bid(db, "BidLayers", b, bid))
            return fail();
        if (!bid_allows_writes(db, bid))
            return fail();

        long long sequence_a = 0;
        long long sequence_b = 0;
        // LY-12: a NULL Sequence on either row refuses (the list shows it as 0)
        if (read_sequence(db, a, sequence_a) != SequenceRead::Value || read_sequence(db, b, sequence_b) != SequenceRead::Value)
            return fail();
        if (sequence_a == sequence_b)
        {
            db.rollback(); // equal Sequences: a success, nothing visibly moves (LY-12)
            return true;
        }

        // layer_operations.py:455-464 the two values are exchanged
        const long long targets[2][2] = {{a, sequence_b}, {b, sequence_a}};
        for (const auto &target : targets)
        {
            ost_db::Statement update = db.prepare("UPDATE [BidLayers] SET [Sequence] = ? WHERE [UID] = ?");
            if (!update.valid())
                return fail();
            update.bind_int64(1, target[1]);
            update.bind_int64(2, target[0]);
            if (!update.run())
                return fail();
        }
        return db.commit();
    }
    catch (...)
    {
        return false; // nothing moved (the connection of the body is closed, which rolled back)
    }

    bool read_layer_uids_in_use(const std::string &db_path,
                                const std::string &bid_uid,
                                std::set<std::string> &used)
    try
    {
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return false;
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        // One read transaction: the 11 tables are read as one consistent view (a deferred BEGIN, ended by ROLLBACK, writes nothing).
        if (!db.exec("BEGIN"))
            return false;
        auto done = [&db](bool ok) -> bool
        {
            db.rollback();
            return ok;
        };

        std::set<std::string> found;
        for (const char *table : kReferenceTables)
        {
            // settings_reader.py:457-459 a missing table or a table without BidLayerUID is skipped; a lookup that fails is a failed read
            std::vector<std::string> columns;
            const Probe probe = table_columns(db, table, columns);
            if (probe == Probe::Failed)
                return done(false);
            if (probe == Probe::Missing || !has_column(columns, "BidLayerUID"))
                continue;
            // :461-471 a table with a BidUID column counts the bid's rows only, a table without one counts every row; NULL never counts
            const bool per_bid = has_column(columns, "BidUID");
            ost_db::Statement stmt = db.prepare(std::string("SELECT DISTINCT [BidLayerUID] FROM [") + table + "] WHERE " +
                                                (per_bid ? "[BidUID] = ? AND " : "") + "[BidLayerUID] IS NOT NULL");
            if (!stmt.valid())
                return done(false);
            if (per_bid)
                stmt.bind_int64(1, bid);
            while (stmt.step())
                found.insert(stmt.column_text(0));
            if (stmt.failed())
                return done(false);
        }
        used = std::move(found);
        return done(true);
    }
    catch (...)
    {
        return false; // a failed read: the caller refuses the delete (LY-2); `used` is not meaningful after a false
    }

    bool reload_layers(const std::string &db_path,
                       const std::string &bid_uid,
                       std::unordered_map<std::string, ost_domain::Layer> &own_layers,
                       std::vector<ost_domain::BidLayer> &merged_layers,
                       std::string *error)
    try
    {
        if (error)
            error->clear();
        auto report = [error](const std::string &text) -> bool
        {
            if (error)
                *error = text;
            return false;
        };
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return report("Cannot reload the layers: the bid UID is not valid.");
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return report("Cannot open the database: " + db.last_error_message());
        if (!db.exec("BEGIN"))
            return report("Cannot read the database: " + db.last_error_message());
        // a failure: the read transaction ends and the reason is reported; the outputs are not touched
        auto fail_with = [&db, &report](const std::string &reason) -> bool
        {
            db.rollback();
            return report(reason);
        };

        // Like load_project: the layers are read only when BidLayers exists (a database without the table has an empty family); but a
        // lookup that FAILS is an unreadable database, never an empty one.
        std::vector<std::string> columns;
        std::string why;
        const Probe probe = table_columns(db, "BidLayers", columns, &why);
        if (probe == Probe::Failed)
            return fail_with(why);

        std::unordered_map<std::string, ost_domain::Layer> own;
        std::vector<ost_domain::BidLayer> merged;
        if (probe == Probe::Present)
        {
            const std::string bid_text = std::to_string(bid);
            std::vector<std::string> own_uids;
            if (!layer_reads::read_own_layers(db, bid_text, own, own_uids, why))
                return fail_with(why);
            // load_project fails a bid whose layer UIDs break the Windows contract; the same rows give no list here either
            const std::string problem = integrity::uid_list_problem(own_uids, "BidLayers");
            if (!problem.empty())
                return fail_with(problem);
            if (!layer_reads::read_merged_layers(db, bid_text, merged, why))
                return fail_with(why.empty() ? std::string("Cannot read the layer list.") : why);
        }
        db.rollback();
        // both lists are replaced together, only on success
        own_layers = std::move(own);
        merged_layers = std::move(merged);
        return true;
    }
    catch (...)
    {
        // a failed read like any other: false, the reason in `error`, both outputs as they were (they are replaced by two moves, which do not
        // throw, as the last step of the body). Assigning the text can itself throw, and nothing may escape: then the text stays empty.
        if (error)
        {
            try
            {
                *error = "Cannot reload the layers: the read failed unexpectedly.";
            }
            catch (...)
            {
                error->clear();
            }
        }
        return false;
    }

}
