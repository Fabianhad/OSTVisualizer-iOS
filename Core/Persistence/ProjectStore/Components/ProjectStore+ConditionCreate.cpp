// ProjectStore component: New Condition (insert_condition) and the identity allocation of several conditions.
//
// Windows 49a2d59: infrastructure/mdb/components/condition_operations.py :254-350 (insert_condition), :39-66 (_allocate_condition_identity(ies) and the
// condition-number guard, :56-62), identity_allocation.py :5-47 (the UID), infrastructure/database/bid_owned_identity.py :105-202 (uids and ownership),
// infrastructure/mdb/mdb_writer.py :63-105 (one transaction per call), :221-274 (required and optional columns, the INSERT text).
// The write pattern is plan 4.4: one transaction, ownership, bid lock, name / value rules, UID limit, writes, commit; any failure is a rollback.
#include "../ProjectStore_Internal.hpp"
#include "../../../Domain/ConditionFormRules.hpp"
#include "../../../Domain/CoreLog.hpp"

#include <cstddef>
#include <exception>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

namespace ost_store
{

    namespace
    {
        // Python int() of a uid string: optional surrounding whitespace, optional sign, decimal digits (condition_operations.py:261
        // `int(bid_uid)`; the same reading as update_condition and duplicate_condition, which keep their own private copy). false when the text
        // is not an integer or does not fit 64 bits.
        bool parse_python_int(const std::string &text, long long &out)
        {
            auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; };
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

        // A condition number allocated per call is bounded far above any real use (a bid holds some thousand conditions) and far below what a
        // vector of identities can hold: a count that both limits allow but that no caller means (2147483647 on an empty bid) is refused instead of
        // being tried (it would need gigabytes and as many GUIDs).
        constexpr long long kMaxIdentitiesPerCall = 1000000;

        // MAX([RefNo]) of the bid read as 64 bit (a number beyond an Access Long must not wrap), 0 when the bid has none (MAX ignores NULL).
        // false when a statement failed. A schema without the RefNo column has no number: `has_numbers` is false and `highest` stays 0.
        bool highest_condition_number(ost_db::OstDatabase &db, long long bid_uid, bool &has_numbers, long long &highest)
        {
            has_numbers = false;
            highest = 0;
            if (!db.column_exists("BidConditions", "RefNo"))
                return true;
            has_numbers = true;
            ost_db::Statement stmt = db.prepare("SELECT MAX([RefNo]) FROM [BidConditions] WHERE [BidUID] = ?");
            if (!stmt.valid())
                return false;
            stmt.bind_int64(1, bid_uid);
            const ost_db::StepResult result = stmt.step_result();
            if (result == ost_db::StepResult::Error)
                return false;
            if (result == ost_db::StepResult::Row && stmt.column_type(0) != ost_db::Statement::TypeNull)
                highest = stmt.column_int64(0);
            return true;
        }

        // One column of the INSERT and how its value is bound.
        struct Assignment
        {
            const char *column;
            std::function<void(ost_db::Statement &, int)> bind;
        };

        // The reference uids of the spec (an empty uid is NULL). A layer, a folder and a condition type are ASCII decimal digits greater than
        // zero (bid_owned_identity.py:105-117), surrounding blanks and signs are malformed.
        bool parse_reference(const std::string &uid, std::optional<long long> &out)
        {
            out.reset();
            if (uid.empty())
                return true;
            long long id = 0;
            if (!integrity::parse_uid(uid, id))
                return false;
            out = id;
            return true;
        }

        // The whole insert, inside the transaction the caller has begun. Returns the new uid, 0 for a refusal or a failure (the caller rolls back).
        // `error` is set only for a text the user must see.
        long long insert_in_transaction(ost_db::OstDatabase &db, long long bid_int, const ost_domain::CreateConditionSpec &spec,
                                        const std::optional<long long> &cdn_type_id, const std::optional<long long> &layer_id,
                                        const std::optional<long long> &folder_id, std::string *error)
        {
            using ost_db::Statement;

            // (1) the bid exists (condition_operations.py:262-264)
            if (!integrity::bid_exists(db, bid_int))
                return 0;
            // (2) a layer and a folder must belong to THIS bid (:265-279: checked only when the column exists; a shared template row, another bid's
            // row and a row that is not there are refused; the bid's own locked copy of a template layer is its layer)
            if (layer_id && db.column_exists("BidConditions", "BidLayerUID") && !integrity::row_belongs_to_bid(db, "BidLayers", *layer_id, bid_int))
                return 0;
            if (folder_id && db.column_exists("BidConditions", "BidConditionFolderUID") &&
                !integrity::row_belongs_to_bid(db, "BidConditionFolders", *folder_id, bid_int))
                return 0;
            // iOS-new (NC-8): the condition type must be a CdnTypes row
            if (cdn_type_id && db.column_exists("BidConditions", "CdnTypeUID") && !integrity::row_exists(db, "CdnTypes", *cdn_type_id))
                return 0;
            // (3) the bid lock (plan 4.1, NC-12): a locked bid, and a status that cannot be read, refuse (fail closed)
            bool locked = true;
            if (!integrity::bid_is_locked(db, bid_int, locked) || locked)
                return 0;
            // (4) the name and number rules (N-M1-13, NC-4, NC-5, NC-10): the form refuses the same values with the Windows texts first
            if (const std::optional<ost_domain::FormIssue> issue = ost_domain::validate_create_spec(spec))
            {
                if (error && issue->message == ost_domain::kConditionNameTooLongMessage)
                    *error = issue->message;
                return 0;
            }
            // the required columns (mdb_writer.py _require_write_columns: UID, BidUID, Name, Type), before the allocation reads them
            if (!db.table_exists("BidConditions"))
                return 0;
            for (const char *required : {"UID", "BidUID", "Name", "Type"})
                if (!db.column_exists("BidConditions", required))
                    return 0;

            // (5) the identity: UID, number and GUID, both limits checked before the INSERT (:280-282)
            std::vector<ConditionIdentity> identity;
            if (!allocate_condition_identities(db, bid_int, 1, identity, error) || identity.size() != 1)
                return 0;
            const ConditionIdentity &fresh = identity.front();

            // (6) the column values in the Windows order (:283-334); a column the table lacks is skipped and logged under the insert's label
            std::vector<Assignment> assignments;
            auto integer = [&](const char *column, long long value)
            { assignments.push_back(Assignment{column, [value](Statement &s, int i) { s.bind_int64(i, value); }}); };
            auto real = [&](const char *column, double value)
            { assignments.push_back(Assignment{column, [value](Statement &s, int i) { s.bind_double(i, value); }}); };
            auto flag = [&](const char *column, bool value) { integer(column, value ? 1 : 0); };
            // the text is copied: a literal passed here is a temporary that is gone when the INSERT is bound
            auto text = [&](const char *column, const std::string &value)
            { assignments.push_back(Assignment{column, [value](Statement &s, int i) { s.bind_text(i, value); }}); };
            auto reference = [&](const char *column, const std::optional<long long> &id)
            {
                assignments.push_back(Assignment{column,
                                                 [id](Statement &s, int i)
                                                 {
                                                     if (id)
                                                         s.bind_int64(i, *id);
                                                     else
                                                         s.bind_null(i);
                                                 }});
            };

            integer("UID", fresh.uid);
            integer("BidUID", bid_int);
            text("GUID", fresh.guid);
            integer("RefNo", fresh.ref_no);
            integer("IsTemplate", 0);
            integer("Curve", 0);
            integer("SnapToGrid", 0);
            integer("ManualLength", 0);
            integer("MatAmount", 0);
            integer("LabAmount", 0);
            integer("SubAmount", 0);
            text("FontName", "Arial");
            integer("FontSize", -16);
            integer("FontBold", 0);
            integer("FontItalic", 0);
            integer("ShowTakeoff", -1);
            integer("Backout", spec.backout ? -1 : 0);
            text("Name", spec.name);
            integer("Type", spec.condition_type);
            reference("CdnTypeUID", cdn_type_id);
            reference("BidLayerUID", layer_id);
            real("Height", spec.height);
            real("Width", spec.width);
            real("Depth", spec.depth);
            real("Thickness", spec.thickness);
            real("Rise", spec.rise);
            real("Run", spec.run);
            real("DisplaySize", spec.display_size);
            integer("Shape", spec.shape);
            integer("Pattern", spec.pattern);
            real("Spacing", spec.spacing);
            integer("ColorFill", spec.color_fill);
            integer("UOM1", spec.uom1);
            integer("UOM2", spec.uom2);
            integer("UOM3", spec.uom3);
            integer("Quantity1", spec.calc_type1);
            integer("Quantity2", spec.calc_type2);
            integer("Quantity3", spec.calc_type3);
            // Notes: the text, NULL when empty (Windows encode_text_blob(text) if text else None; TEXT instead of the BLOB, plan 4.5)
            assignments.push_back(Assignment{"Notes",
                                             [&spec](Statement &s, int i)
                                             {
                                                 if (spec.notes.empty())
                                                     s.bind_null(i);
                                                 else
                                                     s.bind_text(i, spec.notes);
                                             }});
            flag("DropRun", spec.drop_run);
            real("DropValue", spec.drop_value);
            flag("RoundQuantity", spec.round_quantity);
            real("RoundUp", spec.round_up);
            flag("Trim", spec.trim);
            flag("IsCurvedSegment", spec.is_curved_segment);
            flag("Grid", spec.grid);
            real("GridSize1", spec.grid_size1);
            real("GridSize2", spec.grid_size2);
            real("Gap", spec.gap);
            flag("DisplayDimension", spec.display_dimension);
            flag("DisplayName", spec.display_name);
            flag("DisplayGridWhileDrawing", spec.display_grid_while_drawing);
            reference("BidConditionFolderUID", folder_id);
            // The iOS-only columns (N-M1-12): written only when the spec sets them, NULL otherwise (the Windows insert writes none of them)
            if (spec.connect)
                flag("Connect", *spec.connect);
            if (spec.connect_tolerance)
                real("ConnectTolerance", *spec.connect_tolerance);
            if (spec.snap_to_linear)
                integer("SnapToLinear", *spec.snap_to_linear);

            std::vector<const Assignment *> usable;
            std::string skipped;
            for (const Assignment &assignment : assignments)
            {
                if (db.column_exists("BidConditions", assignment.column))
                    usable.push_back(&assignment);
                else
                    skipped += (skipped.empty() ? "" : ", ") + std::string(assignment.column);
            }
            if (!skipped.empty())
                ost_log::warning("insert_condition skipped the columns that BidConditions does not have: " + skipped);

            // (7) INSERT INTO [BidConditions] (<existing columns>) VALUES (...) (mdb_writer.py:221-274)
            std::string sql = "INSERT INTO [BidConditions] (";
            std::string marks;
            for (std::size_t i = 0; i < usable.size(); ++i)
            {
                sql += std::string(i == 0 ? "[" : ", [") + usable[i]->column + "]";
                marks += i == 0 ? "?" : ", ?";
            }
            sql += ") VALUES (" + marks + ")";
            Statement insert = db.prepare(sql);
            if (!insert.valid())
                return 0;
            int index = 1;
            for (const Assignment *assignment : usable)
                assignment->bind(insert, index++);
            if (!insert.run())
                return 0;
            return fresh.uid;
        }
    }

    std::string insert_condition(const std::string &db_path,
                                 const std::string &bid_uid,
                                 const ost_domain::CreateConditionSpec &spec,
                                 std::string *error)
    {
        if (error)
            error->clear();
        try
        {
            // The uids are read before the database is opened: a bid uid like Python int(), the references as ASCII digits (an unreadable one
            // fails the insert before anything is written, as Windows' int() / require_valid_unique_bid_owned_uids do).
            long long bid_int = 0;
            std::optional<long long> cdn_type_id, layer_id, folder_id;
            if (!parse_python_int(bid_uid, bid_int) || bid_int <= 0 || !parse_reference(spec.cdn_type_uid, cdn_type_id) ||
                !parse_reference(spec.layer_uid, layer_id) || !parse_reference(spec.folder_uid, folder_id))
                return "";

            ost_db::OstDatabase db;
            if (!db.open(db_path))
                return "";
            if (!db.begin_transaction())
                return "";

            const long long new_uid = insert_in_transaction(db, bid_int, spec, cdn_type_id, layer_id, folder_id, error);
            if (new_uid <= 0)
            {
                db.rollback();
                return "";
            }
            if (!db.commit()) // a failed COMMIT is rolled back by commit() itself
                return "";
            return std::to_string(new_uid);
        }
        catch (const std::exception &)
        {
            // nothing throws across the Objective-C boundary; the connection of the failed attempt is closed by its destructor
            if (error)
                error->clear();
            return "";
        }
    }

    bool allocate_condition_identities(ost_db::OstDatabase &db,
                                       long long bid_uid,
                                       long long count,
                                       std::vector<ConditionIdentity> &out,
                                       std::string *error)
    {
        out.clear();
        if (error)
            error->clear();
        try
        {
            // a count below one is a caller bug; one above kMaxIdentitiesPerCall is refused before any identity is built
            if (count < 1 || count > kMaxIdentitiesPerCall || bid_uid <= 0)
                return false;
            if (!db.table_exists("BidConditions") || !db.column_exists("BidConditions", "UID") || !db.column_exists("BidConditions", "BidUID"))
                return false;

            // The number: MAX(RefNo) of the bid + 1 (:49-55: NULL ignored, 1 for a bid without numbers or without the column), the whole set refused
            // before any write when its last number would pass 2147483647 (:56-62, ConditionNumberOverflowError).
            bool has_numbers = false;
            long long highest = 0;
            if (!highest_condition_number(db, bid_uid, has_numbers, highest))
                return false;
            if (has_numbers)
            {
                if (const std::optional<std::string> refusal = ost_domain::refuse_new_condition_numbers(highest, count))
                {
                    if (error)
                        *error = *refusal;
                    return false;
                }
            }

            // The UID: MAX(UID) + 1 raised above every referencing column, the whole set refused when its last UID would pass 2147483647
            // (plan 4.2, NC-9: Windows keeps no ceiling).
            long long first_uid = 0;
            bool no_identity_left = false;
            if (!integrity::next_uid_within_access_limit(db, "BidConditions", count, first_uid, &no_identity_left))
            {
                if (no_identity_left && error)
                    *error = integrity::no_identity_left_text("condition");
                return false;
            }

            out.reserve(static_cast<std::size_t>(count));
            for (long long i = 0; i < count; ++i)
            {
                ConditionIdentity identity;
                identity.uid = first_uid + i;
                identity.guid = new_ost_guid();
                identity.ref_no = highest + 1 + i; // cannot overflow: highest + count <= kMaxConditionNumber here
                out.push_back(std::move(identity));
            }
            return true;
        }
        catch (const std::exception &)
        {
            out.clear();
            if (error)
                error->clear();
            return false;
        }
    }

}
