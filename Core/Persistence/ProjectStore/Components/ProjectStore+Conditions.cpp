// ProjectStore component — condition CRUD and render config.
#include "../ProjectStore_Internal.hpp"
#include "../../../Domain/ConditionFormRules.hpp"

#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <random>

namespace ost_store
{

    namespace
    {
        // Python int() of a uid string: optional surrounding whitespace, optional sign, decimal digits. Windows runs
        // int(uid) on every uid it writes or matches; false when the text is not an integer.
        bool parse_uid_int(const std::string &text, long long &out)
        {
            auto is_space = [](char c)
            { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; };
            std::size_t begin = 0;
            std::size_t end = text.size();
            while (begin < end && is_space(text[begin]))
                ++begin;
            while (end > begin && is_space(text[end - 1]))
                --end;
            if (begin == end)
                return false;
            bool negative = false;
            if (text[begin] == '+' || text[begin] == '-')
            {
                negative = text[begin] == '-';
                ++begin;
            }
            if (begin == end)
                return false;
            unsigned long long value = 0;
            const unsigned long long limit = static_cast<unsigned long long>(std::numeric_limits<long long>::max());
            for (std::size_t i = begin; i < end; ++i)
            {
                if (text[i] < '0' || text[i] > '9')
                    return false;
                const unsigned long long digit = static_cast<unsigned long long>(text[i] - '0');
                if (value > (limit - digit) / 10ull)
                    return false;
                value = value * 10ull + digit;
            }
            out = negative ? -static_cast<long long>(value) : static_cast<long long>(value);
            return true;
        }

        // Windows bid_owned_identity.require_valid_unique_bid_owned_uids: a uid handed over as TEXT must be ASCII decimal
        // digits only (no sign, no blanks); duplicate_conditions passes the condition uids on as text, while update and
        // delete turn them into integers first (int(uid)).
        bool is_ascii_decimal(const std::string &text)
        {
            if (text.empty())
                return false;
            for (const char c : text)
                if (c < '0' || c > '9')
                    return false;
            return true;
        }

        bool equals_ignore_case(const std::string &a, const char *b)
        {
            const std::string other(b);
            if (a.size() != other.size())
                return false;
            for (std::size_t i = 0; i < a.size(); ++i)
                if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(other[i])))
                    return false;
            return true;
        }

        // Windows bid_owned_identity.require_existing_bid_scoped_uid_match: the uid and the bid are positive integers,
        // `table` holds the uid exactly once, that row belongs to the bid, and the bid has exactly one Bids row. false
        // for every other case (missing, foreign, duplicated, malformed): the caller fails and writes nothing.
        bool require_bid_scoped_uid(ost_db::OstDatabase &db, const char *table, long long uid, long long bid_uid)
        {
            if (uid <= 0 || bid_uid <= 0)
                return false;
            {
                ost_db::Statement stmt = db.prepare(std::string("SELECT [UID], [BidUID] FROM [") + table +
                                                    "] WHERE [UID] = ?");
                if (!stmt.valid())
                    return false;
                stmt.bind_int64(1, uid);
                int rows = 0;
                bool in_bid = false;
                while (stmt.step())
                {
                    ++rows;
                    in_bid = stmt.column_type(1) != ost_db::Statement::TypeNull && stmt.column_int64(1) == bid_uid;
                }
                if (stmt.failed() || rows != 1 || !in_bid)
                    return false;
            }
            {
                ost_db::Statement stmt = db.prepare("SELECT [UID] FROM [Bids] WHERE [UID] = ?");
                if (!stmt.valid())
                    return false;
                stmt.bind_int64(1, bid_uid);
                int rows = 0;
                while (stmt.step())
                    ++rows;
                if (stmt.failed() || rows != 1)
                    return false;
            }
            return true;
        }

        // How the reference-number shift of an update ended: Failed is any statement problem (the caller rolls back and
        // reports the generic failure), Overflow is Windows' ConditionNumberOverflowError (the caller rolls back and reports
        // its text).
        enum class RefShift
        {
            Done,
            Failed,
            Overflow
        };

        // Windows _shift_conflicting_ref_nos_for_update: when the condition's number really changes and another
        // condition of the bid already holds the new number, every condition of the bid from that number up (except this
        // one) moves up by one. A stored RefNo of NULL is "different from the new number" (b9902e6; before it Windows
        // raised on it, COND-UPD-02): the conflict check runs as for any number, and a row without a number is never
        // shifted (NULL >= n is not true, NULL + 1 stays NULL). Before the shift, MAX(RefNo) of the rows it would move
        // (the bid's rows from the new number up, this condition excluded) must be below the highest condition number,
        // else the shift would carry that row past it: Overflow, nothing written (DLG-FINITE-01). The MAX of no row, or of
        // rows without a number, is NULL: no overflow.
        RefShift shift_conflicting_ref_nos(ost_db::OstDatabase &db, long long bid_uid, long long condition_uid,
                                           long long new_ref_no)
        {
            if (!db.column_exists("BidConditions", "RefNo"))
                return RefShift::Failed;
            {
                ost_db::Statement stmt = db.prepare("SELECT [RefNo] FROM [BidConditions] WHERE [UID] = ? AND [BidUID] = ?");
                if (!stmt.valid())
                    return RefShift::Failed;
                stmt.bind_int64(1, condition_uid);
                stmt.bind_int64(2, bid_uid);
                if (!stmt.step())
                    return stmt.failed() ? RefShift::Failed : RefShift::Done; // no such row: nothing to shift
                if (stmt.column_type(0) != ost_db::Statement::TypeNull && stmt.column_int64(0) == new_ref_no)
                    return RefShift::Done;
            }
            bool taken = false;
            {
                ost_db::Statement stmt = db.prepare(
                    "SELECT [UID] FROM [BidConditions] WHERE [BidUID] = ? AND [RefNo] = ? AND [UID] <> ?");
                if (!stmt.valid())
                    return RefShift::Failed;
                stmt.bind_int64(1, bid_uid);
                stmt.bind_int64(2, new_ref_no);
                stmt.bind_int64(3, condition_uid);
                taken = stmt.step();
                if (stmt.failed())
                    return RefShift::Failed;
            }
            if (!taken)
                return RefShift::Done;
            {
                ost_db::Statement stmt = db.prepare(
                    "SELECT MAX([RefNo]) FROM [BidConditions] WHERE [BidUID] = ? AND [RefNo] >= ? AND [UID] <> ?");
                if (!stmt.valid())
                    return RefShift::Failed;
                stmt.bind_int64(1, bid_uid);
                stmt.bind_int64(2, new_ref_no);
                stmt.bind_int64(3, condition_uid);
                const ost_db::StepResult result = stmt.step_result();
                if (result == ost_db::StepResult::Error)
                    return RefShift::Failed;
                if (result == ost_db::StepResult::Row && stmt.column_type(0) != ost_db::Statement::TypeNull &&
                    stmt.column_int64(0) >= ost_domain::kMaxConditionNumber)
                    return RefShift::Overflow;
            }
            ost_db::Statement stmt = db.prepare(
                "UPDATE [BidConditions] SET [RefNo] = [RefNo] + 1 WHERE [BidUID] = ? AND [RefNo] >= ? AND [UID] <> ?");
            if (!stmt.valid())
                return RefShift::Failed;
            stmt.bind_int64(1, bid_uid);
            stmt.bind_int64(2, new_ref_no);
            stmt.bind_int64(3, condition_uid);
            return stmt.run() ? RefShift::Done : RefShift::Failed;
        }

        // new_ost_guid (Windows _new_ost_guid) lives in ProjectStore_Internal.hpp: the takeoff restore needs it as well.
    }

    // Windows update_condition(db_path, bid_uid, condition_uid, UpdateConditionDto) (condition_operations.py): only the
    // columns of the fields in the change set are written, a cleared reference or note is NULL, the condition (and a new
    // layer or folder) must belong to the bid, and the reference-number shift runs in the same transaction.
    bool update_condition(const std::string &db_path,
                          const std::string &bid_uid,
                          const std::string &condition_uid,
                          const ost_domain::ConditionChanges &changes,
                          std::string *error)
    {
        using ost_domain::ConditionField;
        using ost_db::Statement;

        if (error)
            error->clear();
        // `if not changes: return True`, before the database is even opened.
        if (changes.empty())
            return true;

        const ost_domain::Condition &v = changes.values;

        // CdnTypeUID / BidLayerUID / BidConditionFolderUID: int(value) if value else None. A value that is not an
        // integer fails the whole update before anything is written (Windows: int() raises).
        std::optional<long long> cdn_type_id, layer_id, folder_id;
        auto reference = [&changes](ConditionField field, const std::string &uid, std::optional<long long> &out) -> bool
        {
            out.reset();
            if (!changes.has(field) || uid.empty())
                return true;
            long long id = 0;
            if (!parse_uid_int(uid, id))
                return false;
            out = id;
            return true;
        };
        if (!reference(ConditionField::CdnTypeUid, v.cdn_type_uid, cdn_type_id) ||
            !reference(ConditionField::LayerUid, v.layer_uid, layer_id) ||
            !reference(ConditionField::FolderUid, v.condition_folder_uid, folder_id))
            return false;
        long long bid_int = 0;
        long long condition_int = 0;
        if (!parse_uid_int(bid_uid, bid_int) || !parse_uid_int(condition_uid, condition_int))
            return false;

        struct Assignment
        {
            const char *column;
            std::function<void(Statement &, int)> bind;
        };
        std::vector<Assignment> assignments;
        auto add = [&](ConditionField field, const char *column, std::function<void(Statement &, int)> bind)
        {
            if (changes.has(field))
                assignments.push_back(Assignment{column, std::move(bind)});
        };
        auto text = [&](ConditionField field, const char *column, const std::string &value)
        { add(field, column, [&value](Statement &s, int i) { s.bind_text(i, value); }); };
        auto integer = [&](ConditionField field, const char *column, int value)
        { add(field, column, [value](Statement &s, int i) { s.bind_int(i, value); }); };
        auto real = [&](ConditionField field, const char *column, double value)
        { add(field, column, [value](Statement &s, int i) { s.bind_double(i, value); }); };
        auto flag = [&](ConditionField field, const char *column, bool value)
        { add(field, column, [value](Statement &s, int i) { s.bind_int(i, value ? 1 : 0); }); };
        auto reference_value = [&](ConditionField field, const char *column, const std::optional<long long> &id)
        {
            add(field, column,
                [id](Statement &s, int i)
                {
                    if (id)
                        s.bind_int64(i, *id);
                    else
                        s.bind_null(i);
                });
        };

        text(ConditionField::Name, "Name", v.name);
        integer(ConditionField::ConditionType, "Type", static_cast<int>(v.condition_type));
        integer(ConditionField::Backout, "Backout", v.backout ? -1 : 0);
        integer(ConditionField::RefNo, "RefNo", v.ref_no);
        reference_value(ConditionField::CdnTypeUid, "CdnTypeUID", cdn_type_id);
        reference_value(ConditionField::LayerUid, "BidLayerUID", layer_id);
        real(ConditionField::Height, "Height", v.height);
        real(ConditionField::Width, "Width", v.width);
        real(ConditionField::Depth, "Depth", v.depth);
        real(ConditionField::Thickness, "Thickness", v.thickness);
        real(ConditionField::Rise, "Rise", v.rise);
        real(ConditionField::Run, "Run", v.run);
        real(ConditionField::DisplaySize, "DisplaySize", v.display_size);
        integer(ConditionField::Shape, "Shape", static_cast<int>(v.shape));
        integer(ConditionField::Pattern, "Pattern", static_cast<int>(v.pattern));
        real(ConditionField::Spacing, "Spacing", v.spacing);
        integer(ConditionField::ColorFill, "ColorFill", v.color_fill);
        integer(ConditionField::Uom1, "UOM1", v.uom1);
        integer(ConditionField::Uom2, "UOM2", v.uom2);
        integer(ConditionField::Uom3, "UOM3", v.uom3);
        integer(ConditionField::CalcType1, "Quantity1", v.calc_type1);
        integer(ConditionField::CalcType2, "Quantity2", v.calc_type2);
        integer(ConditionField::CalcType3, "Quantity3", v.calc_type3);
        // Notes: the text, or NULL when empty (Windows encode_text_blob(text) if text else None)
        add(ConditionField::Notes, "Notes",
            [&v](Statement &s, int i)
            {
                if (v.notes.empty())
                    s.bind_null(i);
                else
                    s.bind_text(i, v.notes);
            });
        flag(ConditionField::DropRun, "DropRun", v.drop_run);
        real(ConditionField::DropValue, "DropValue", v.drop_value);
        flag(ConditionField::RoundQuantity, "RoundQuantity", v.round_quantity);
        real(ConditionField::RoundUp, "RoundUp", v.round_up);
        flag(ConditionField::Trim, "Trim", v.trim);
        flag(ConditionField::IsCurvedSegment, "IsCurvedSegment", v.is_curved_segment);
        flag(ConditionField::Grid, "Grid", v.grid);
        real(ConditionField::GridSize1, "GridSize1", v.grid_size1);
        real(ConditionField::GridSize2, "GridSize2", v.grid_size2);
        real(ConditionField::Gap, "Gap", v.gap);
        flag(ConditionField::DisplayDimension, "DisplayDimension", v.display_dimension);
        flag(ConditionField::DisplayName, "DisplayName", v.display_name);
        flag(ConditionField::DisplayGridWhileDrawing, "DisplayGridWhileDrawing", v.display_grid_while_drawing);
        reference_value(ConditionField::FolderUid, "BidConditionFolderUID", folder_id);
        // iOS-only controls (COND-EXTRA-01): written like every other field, only when they changed
        flag(ConditionField::Connect, "Connect", v.connect);
        real(ConditionField::ConnectTolerance, "ConnectTolerance", v.connect_tolerance);
        integer(ConditionField::SnapToLinear, "SnapToLinear", v.snap_to_linear);

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

        // Windows requires the table and its identity columns, then the ownership of the condition and of a new layer /
        // folder, all before the first write.
        if (!db.table_exists("BidConditions") || !db.column_exists("BidConditions", "UID") ||
            !db.column_exists("BidConditions", "BidUID"))
            return fail();
        if (!require_bid_scoped_uid(db, "BidConditions", condition_int, bid_int))
            return fail();
        if (layer_id && !require_bid_scoped_uid(db, "BidLayers", *layer_id, bid_int))
            return fail();
        if (folder_id && !require_bid_scoped_uid(db, "BidConditionFolders", *folder_id, bid_int))
            return fail();

        // The shift runs before the UPDATE of the columns, in the same transaction: an overflow rolls back everything, the other
        // fields of the same save included (Windows raises ConditionNumberOverflowError before _execute_update_values).
        if (changes.has(ConditionField::RefNo))
        {
            switch (shift_conflicting_ref_nos(db, bid_int, condition_int, v.ref_no))
            {
            case RefShift::Done:
                break;
            case RefShift::Overflow:
                if (error)
                    *error = ost_domain::condition_number_overflow_message(v.ref_no);
                return fail();
            case RefShift::Failed:
                return fail();
            }
        }

        // A column the schema lacks is skipped (Windows _filter_existing_write_values); with none left there is nothing
        // to write, which Windows reports as an unsupported schema.
        std::vector<const Assignment *> usable;
        for (const Assignment &assignment : assignments)
            if (db.column_exists("BidConditions", assignment.column))
                usable.push_back(&assignment);
        if (usable.empty())
            return fail();

        {
            std::string sql = "UPDATE [BidConditions] SET ";
            for (std::size_t i = 0; i < usable.size(); ++i)
                sql += std::string(i == 0 ? "[" : ", [") + usable[i]->column + "] = ?";
            sql += " WHERE [UID] = ? AND [BidUID] = ?";
            Statement stmt = db.prepare(sql);
            if (!stmt.valid())
                return fail();
            int index = 1;
            for (const Assignment *assignment : usable)
                assignment->bind(stmt, index++);
            stmt.bind_int64(index++, condition_int);
            stmt.bind_int64(index, bid_int);
            if (!stmt.run())
                return fail();
        }
        return db.commit();
    }

    bool shift_condition_ref_nos(const std::string &db_path,
                                 const std::string &bid_uid,
                                 int from_ref_no,
                                 const std::string &exclude_uid)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidConditions "
            "SET RefNo = RefNo + 1 "
            "WHERE BidUID = ? AND RefNo >= ? AND UID <> ?");
        if (!stmt.valid())
            return false;

        stmt.bind_text(1, bid_uid);
        stmt.bind_int(2, from_ref_no);
        stmt.bind_text(3, exclude_uid);
        return stmt.run();
    }

    std::string duplicate_condition(const std::string &db_path,
                                    const std::string &bid_uid,
                                    const std::string &condition_uid)
    {
        long long bid_int = 0;
        long long condition_int = 0;
        if (!parse_uid_int(bid_uid, bid_int) || !is_ascii_decimal(condition_uid) ||
            !parse_uid_int(condition_uid, condition_int))
            return "";

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return "";

        // Every read below is finished (scoped) before the transaction starts.
        // RefNo = MAX(RefNo of the bid) + 1 (1 for a bid without numbers); a schema without the column has no number.
        int new_ref = 1;
        if (db.column_exists("BidConditions", "RefNo"))
        {
            ost_db::Statement ref_stmt = db.prepare(
                "SELECT COALESCE(MAX(RefNo), 0) + 1 FROM BidConditions WHERE BidUID = ?");
            if (!ref_stmt.valid())
                return "";
            ref_stmt.bind_int64(1, bid_int);
            const ost_db::StepResult ref_result = ref_stmt.step_result();
            if (ref_result == ost_db::StepResult::Error)
                return "";
            if (ref_result == ost_db::StepResult::Row)
                new_ref = ref_stmt.column_int(0);
        }

        if (!db.begin_transaction())
            return "";

        auto fail = [&db]() -> std::string
        {
            db.rollback();
            return "";
        };

        // Windows condition_operations._allocate_condition_identities: the new UID is MAX(UID) + 1 raised above every column
        // that references BidConditions (BidConditionUser, BidLaborActivity, BidTakeoffTotals, BidTakeoffs, BidTypGroupViews,
        // BidTypicalGroupTotals), so the copy never adopts a dangling reference.
        const long long new_uid = integrity::next_uid_preserving_references(db, "BidConditions");
        if (new_uid <= 0)
            return fail();

        // Windows copies every column of the source row (the row must be a condition of the bid, else nothing is
        // inserted) and replaces UID, GUID and RefNo; a column the table lacks is simply not part of the copy.
        std::vector<std::string> columns;
        {
            ost_db::Statement info = db.prepare("SELECT name FROM pragma_table_info('BidConditions')");
            if (!info.valid())
                return fail();
            while (info.step())
                columns.push_back(info.column_text(0));
            if (info.failed())
                return fail();
        }
        bool has_uid = false;
        bool has_bid_uid = false;
        for (const std::string &column : columns)
        {
            has_uid = has_uid || equals_ignore_case(column, "UID");
            has_bid_uid = has_bid_uid || equals_ignore_case(column, "BidUID");
        }
        if (!has_uid || !has_bid_uid)
            return fail();
        if (!require_bid_scoped_uid(db, "BidConditions", condition_int, bid_int))
            return fail();

        const std::string guid = new_ost_guid();
        std::string insert_columns;
        std::string select_columns;
        std::vector<char> replaced; // 'U' = new uid, 'G' = new guid, 'R' = new ref number, in select-list order
        for (const std::string &column : columns)
        {
            insert_columns += (insert_columns.empty() ? "[" : ", [") + column + "]";
            std::string item = "[" + column + "]";
            if (equals_ignore_case(column, "UID"))
            {
                item = "?";
                replaced.push_back('U');
            }
            else if (equals_ignore_case(column, "GUID"))
            {
                item = "?";
                replaced.push_back('G');
            }
            else if (equals_ignore_case(column, "RefNo"))
            {
                item = "?";
                replaced.push_back('R');
            }
            select_columns += (select_columns.empty() ? "" : ", ") + item;
        }

        {
            ost_db::Statement copy = db.prepare("INSERT INTO [BidConditions] (" + insert_columns + ") SELECT " +
                                                select_columns + " FROM [BidConditions] WHERE [UID] = ? AND [BidUID] = ?");
            if (!copy.valid())
                return fail();
            int index = 1;
            for (const char kind : replaced)
            {
                if (kind == 'U')
                    copy.bind_int64(index, new_uid);
                else if (kind == 'G')
                    copy.bind_text(index, guid);
                else
                    copy.bind_int(index, new_ref);
                ++index;
            }
            copy.bind_int64(index++, condition_int);
            copy.bind_int64(index, bid_int);
            if (!copy.run())
                return fail();
        }

        if (!db.commit())
            return "";

        return std::to_string(new_uid);
    }

    bool delete_condition(const std::string &db_path,
                          const std::string &bid_uid,
                          const std::string &condition_uid)
    {
        long long bid_int = 0;
        long long condition_int = 0;
        if (!parse_uid_int(bid_uid, bid_int) || !parse_uid_int(condition_uid, condition_int))
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

        // One statement of the delete with integer parameters. Any failure fails the whole delete (and rolls it back),
        // except for a best-effort cleanup: Windows only logs a failing ConditionSetStyles cleanup and the delete goes on
        // (condition_operations.py _delete_condition_cascade_chunk). A failure that already rolled the transaction
        // back by itself (I/O error, RAISE(ROLLBACK)) still fails the delete.
        auto run_statement = [&](const std::string &sql, std::initializer_list<long long> params, bool best_effort) -> bool
        {
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid())
                return best_effort;
            int index = 1;
            for (const long long param : params)
                stmt.bind_int64(index++, param);
            if (stmt.run())
                return true;
            return best_effort && db.in_transaction();
        };

        // Windows leaves a dependent table alone when the schema lacks the table or the column the cleanup matches on
        // (schema.optional_table_missing / schema.column_exists guards).
        auto has_column = [&db](const char *table, const char *column) -> bool
        {
            return db.table_exists(table) && db.column_exists(table, column);
        };

        // The condition must exist exactly once and belong to the bid (require_existing_bid_scoped_uid_matches).
        if (!has_column("BidConditions", "UID") || !db.column_exists("BidConditions", "BidUID"))
            return fail();
        if (!require_bid_scoped_uid(db, "BidConditions", condition_int, bid_int))
            return fail();

        // The takeoffs of the condition, as a sub-select: [BidConditionUID] = ? AND [BidUID] = ? (condition, bid).
        const std::string takeoffs_of_condition =
            "(SELECT [UID] FROM [BidTakeoffs] WHERE [BidConditionUID] = ? AND [BidUID] = ?)";

        // Surviving takeoffs that point at a takeoff about to be deleted lose that reference (ParentUID,
        // TypGroupTakeoffUID, TypPageTakeoffUID, TypGroupMarkerUID become NULL). Windows first checks that every
        // takeoff uid involved is a positive integer held by exactly one row, and runs no UPDATE when the condition has
        // no takeoffs.
        if (has_column("BidTakeoffs", "UID") && db.column_exists("BidTakeoffs", "BidUID") &&
            db.column_exists("BidTakeoffs", "BidConditionUID"))
        {
            int takeoff_count = 0;
            {
                ost_db::Statement stmt = db.prepare("SELECT [UID] FROM [BidTakeoffs] WHERE [BidConditionUID] = ? AND [BidUID] = ?");
                if (!stmt.valid())
                    return fail();
                stmt.bind_int64(1, condition_int);
                stmt.bind_int64(2, bid_int);
                while (stmt.step())
                {
                    ++takeoff_count;
                    if (stmt.column_type(0) == ost_db::Statement::TypeNull || stmt.column_int64(0) <= 0)
                        return fail();
                }
                if (stmt.failed())
                    return fail();
            }
            if (takeoff_count > 0)
            {
                {
                    ost_db::Statement stmt = db.prepare("SELECT COUNT(*), COUNT(DISTINCT [UID]) FROM [BidTakeoffs] WHERE [UID] IN " +
                                                        takeoffs_of_condition);
                    if (!stmt.valid())
                        return fail();
                    stmt.bind_int64(1, condition_int);
                    stmt.bind_int64(2, bid_int);
                    if (!stmt.step() || stmt.column_int64(0) != stmt.column_int64(1))
                        return fail();
                }
                const char *self_references[] = {"ParentUID", "TypGroupTakeoffUID", "TypPageTakeoffUID",
                                                 "TypGroupMarkerUID"};
                for (const char *column : self_references)
                {
                    if (!db.column_exists("BidTakeoffs", column))
                        continue;
                    const std::string sql = "UPDATE [BidTakeoffs] SET [" + std::string(column) +
                                            "] = NULL WHERE [BidUID] = ? AND [" + column + "] IN " + takeoffs_of_condition;
                    if (!run_statement(sql, {bid_int, condition_int, bid_int}, false))
                        return fail();
                }
            }
        }

        // Annotations that start at OR end at a takeoff of the condition.
        const char *takeoff_children[] = {"BidDimensions", "BidALines", "BidArrows"};
        const char *takeoff_references[] = {"BidTakeoffFromUID", "BidTakeoffToUID"};
        for (const char *table : takeoff_children)
        {
            if (!db.table_exists(table))
                continue;
            for (const char *reference : takeoff_references)
            {
                if (!db.column_exists(table, reference))
                    continue;
                const std::string sql = "DELETE FROM [" + std::string(table) + "] WHERE [" + reference + "] IN " +
                                        takeoffs_of_condition;
                if (!run_statement(sql, {condition_int, bid_int}, false))
                    return fail();
            }
        }

        // Percents of the condition's takeoffs, then percents of the condition's labor activities.
        if (has_column("BidPercents", "BidTakeoffUID"))
        {
            if (!run_statement("DELETE FROM [BidPercents] WHERE [BidTakeoffUID] IN " + takeoffs_of_condition,
                               {condition_int, bid_int}, false))
                return fail();
        }
        if (has_column("BidPercents", "BidLaborActivityUID") && has_column("BidLaborActivity", "UID") &&
            db.column_exists("BidLaborActivity", "BidConditionUID") && db.column_exists("BidLaborActivity", "BidUID"))
        {
            if (!run_statement("DELETE FROM [BidPercents] WHERE [BidLaborActivityUID] IN "
                               "(SELECT [UID] FROM [BidLaborActivity] WHERE [BidConditionUID] = ? AND [BidUID] = ?)",
                               {condition_int, bid_int}, false))
                return fail();
        }

        // The condition's type group views (and what hangs off them), then its totals.
        if (has_column("AffectDPCTypGroupViews", "BidTypGroupViewUID") && has_column("BidTypGroupViews", "UID") &&
            db.column_exists("BidTypGroupViews", "BidConditionUID") && db.column_exists("BidTypGroupViews", "BidUID"))
        {
            if (!run_statement("DELETE FROM [AffectDPCTypGroupViews] WHERE [BidTypGroupViewUID] IN "
                               "(SELECT [UID] FROM [BidTypGroupViews] WHERE [BidConditionUID] = ? AND [BidUID] = ?)",
                               {condition_int, bid_int}, false))
                return fail();
        }
        if (has_column("BidTypGroupViews", "BidConditionUID") && db.column_exists("BidTypGroupViews", "BidUID"))
        {
            if (!run_statement("DELETE FROM [BidTypGroupViews] WHERE [BidConditionUID] = ? AND [BidUID] = ?",
                               {condition_int, bid_int}, false))
                return fail();
        }
        const char *totals_tables[] = {"BidTakeoffTotals", "BidTypicalGroupTotals"};
        for (const char *table : totals_tables)
        {
            if (!has_column(table, "BidConditionUID") || !db.column_exists(table, "BidUID"))
                continue;
            if (!run_statement("DELETE FROM [" + std::string(table) + "] WHERE [BidConditionUID] = ? AND [BidUID] = ?",
                               {condition_int, bid_int}, false))
                return fail();
        }
        if (has_column("BidConditionUser", "ConditionUID"))
        {
            if (!run_statement("DELETE FROM [BidConditionUser] WHERE [ConditionUID] = ?", {condition_int}, false))
                return fail();
        }

        // BidTakeoffs is optional as a table, but once it exists it must carry the columns the delete matches on.
        if (db.table_exists("BidTakeoffs"))
        {
            if (!db.column_exists("BidTakeoffs", "BidConditionUID") || !db.column_exists("BidTakeoffs", "BidUID"))
                return fail();
            if (!run_statement("DELETE FROM [BidTakeoffs] WHERE [BidConditionUID] = ? AND [BidUID] = ?",
                               {condition_int, bid_int}, false))
                return fail();
        }

        if (has_column("BidLaborActivity", "BidConditionUID"))
        {
            if (!run_statement("DELETE FROM [BidLaborActivity] WHERE [BidConditionUID] = ? AND [BidUID] = ?",
                               {condition_int, bid_int}, false))
                return fail();
        }

        if (has_column("ConditionSetStyles", "ConditionStyleUID"))
        {
            if (!run_statement("DELETE FROM [ConditionSetStyles] WHERE [ConditionStyleUID] = ?", {condition_int}, true))
                return fail();
        }

        if (!run_statement("DELETE FROM [BidConditions] WHERE [UID] = ? AND [BidUID] = ?", {condition_int, bid_int},
                           false))
            return fail();

        return db.commit();
    }

    // Both preferences are one operation: when either write fails neither is kept. false when anything failed.
    static bool write_render_config(ost_db::OstDatabase &db, const ost_domain::RenderConfig &config)
    {
        if (!db.begin_transaction())
            return false;
        if (!set_pref(db, "color_mode", config.color_mode) ||
            !set_pref(db, "grayscale_enabled", config.grayscale_enabled ? "1" : "0"))
        {
            db.rollback();
            return false;
        }
        return db.commit();
    }

    ost_domain::RenderConfig load_render_config(const std::string &db_path)
    {
        ost_domain::RenderConfig config;
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return config;

        // Windows ConfigAggregate._apply_config / _validated_display_mode: a stored mode outside the vocabulary is
        // replaced by the default (Original) and the correction is saved. It is saved once, because the stored value is
        // valid afterwards. A correction that cannot be saved is not an error: the corrected value is returned anyway
        // (Windows logs "Failed to save corrected config" and keeps the corrected config). A missing row is not an
        // invalid value: the default applies and nothing is written.
        std::string mode;
        const bool has_mode = get_pref_if_present(db, "color_mode", mode);
        if (has_mode)
            config.color_mode = ost_domain::RenderConfig::validated_mode(mode);

        std::string gray = get_pref(db, "grayscale_enabled", "0");
        config.grayscale_enabled = (gray == "1");

        if (has_mode && config.color_mode != mode)
            write_render_config(db, config);

        return config;
    }

    bool save_render_config(const std::string &db_path,
                            const ost_domain::RenderConfig &config)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        // Windows update_options: the configuration is validated first (a mode outside the vocabulary is saved as the
        // default, never as it was given) and a failed write is reported to the caller instead of being ignored.
        ost_domain::RenderConfig validated = config;
        validated.color_mode = ost_domain::RenderConfig::validated_mode(config.color_mode);
        return write_render_config(db, validated);
    }

    bool update_render_config(const std::string &db_path,
                              const ost_domain::RenderConfig &config,
                              bool *changed)
    {
        // Windows update_options: the new configuration is validated, compared with the current one (as load_render_config
        // reads it, an invalid stored mode already corrected) and written only when a value differs.
        if (changed)
            *changed = false;
        ost_domain::RenderConfig validated = config;
        validated.color_mode = ost_domain::RenderConfig::validated_mode(config.color_mode);
        const ost_domain::RenderConfig current = load_render_config(db_path);
        if (current.color_mode == validated.color_mode && current.grayscale_enabled == validated.grayscale_enabled)
            return true;
        if (!save_render_config(db_path, validated))
            return false;
        if (changed)
            *changed = true;
        return true;
    }

}
