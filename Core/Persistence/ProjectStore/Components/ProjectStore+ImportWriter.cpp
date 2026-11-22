// ProjectStore component — import writer: UID floor, bid number, typed row INSERT, write order.
// Windows counterpart: ost_visualizer/infrastructure/mdb/components/import_operations.py
// (_get_max_uid, _assign_next_bid_no, _next_table_uid, _get_table_info, _insert_raw_row, _convert_access_value,
// _write_to_db) with settings_cardinality.py and identity_allocation.py.
#include "../ProjectStore_ImportInternal.hpp"
#include "../../../Import/TextRules.hpp"
#include <algorithm>

namespace ost_store
{
    namespace
    {
        using ost_import::Row;
        namespace text_rules = ost_import::text_rules;

        // Windows constants.NUMERIC_TYPE_SUBSTRINGS.
        bool is_numeric_type(const std::string &type_name)
        {
            static const char *const substrings[] = {"int", "long", "short", "double", "float", "real", "bit",
                                                     "byte", "counter", "currency", "single", "numeric", "decimal"};
            for (const char *s : substrings)
                if (type_name.find(s) != std::string::npos)
                    return true;
            return false;
        }

        // Python repr() of a list of str, as in the Windows log line: ['Name', 'UID'].
        std::string repr_sorted_keys(const Row &row)
        {
            std::vector<std::string> keys;
            keys.reserve(row.size());
            for (const auto &field : row)
                keys.push_back(field.first);
            std::sort(keys.begin(), keys.end());
            std::string out = "[";
            for (size_t i = 0; i < keys.size(); ++i)
            {
                if (i)
                    out += ", ";
                out += "'" + keys[i] + "'";
            }
            return out + "]";
        }

        // Reads column idx of the current row as Python's int(row[0]) would: NULL -> no value; INTEGER; FLOAT
        // truncates toward zero; TEXT must be a Python int literal. Anything else (text 'abc', BLOB) is the
        // ValueError the Windows code lets escape; a literal that exceeds 64 bits cannot be a SQLite UID either.
        bool read_integer(const ost_db::Statement &stmt, int idx, bool &has_value, long long &value,
                          const std::string &what, std::string &error)
        {
            has_value = false;
            switch (stmt.column_type(idx))
            {
            case ost_db::Statement::TypeNull:
                return true;
            case ost_db::Statement::TypeInteger:
                has_value = true;
                value = stmt.column_int64(idx);
                return true;
            case ost_db::Statement::TypeFloat:
            {
                // int(float): truncates toward zero; nan/inf raise and a float beyond 64 bits is a big integer that
                // SQLite cannot store (the Windows import fails either way). The cast of an out-of-range double is
                // undefined, so the range is checked first.
                const double real = stmt.column_double(idx);
                if (!(real >= -9223372036854775808.0 && real < 9223372036854775808.0))
                {
                    error = "int() of a float outside the 64-bit range (" + what + ")";
                    return false;
                }
                has_value = true;
                value = static_cast<long long>(real);
                return true;
            }
            case ost_db::Statement::TypeText:
            {
                const std::string text = stmt.column_text(idx);
                long long parsed = 0;
                if (text_rules::py_parse_int(text, parsed) != text_rules::NumberParse::Ok)
                {
                    error = "invalid literal for int() with base 10: '" + text + "' (" + what + ")";
                    return false;
                }
                has_value = true;
                value = parsed;
                return true;
            }
            default:
                error = "int() argument must be a string or a real number (" + what + ")";
                return false;
            }
        }

        // Prepare + first step of "SELECT MAX([col]) FROM [table]". `skipped` is set when the table or column does
        // not exist (the Windows code swallows that pyodbc.Error). false = a real failure.
        bool select_max(ost_db::OstDatabase &db, const std::string &table, const std::string &column,
                        bool &skipped, bool &has_value, long long &value, std::string &error)
        {
            skipped = false;
            has_value = false;
            ost_db::Statement stmt = db.prepare("SELECT MAX([" + column + "]) FROM [" + table + "]");
            if (!stmt.valid())
            {
                if (stmt.schema_mismatch())
                {
                    skipped = true;
                    return true;
                }
                error = "SELECT MAX([" + column + "]) FROM [" + table + "] failed: " + stmt.error_message();
                return false;
            }
            const ost_db::StepResult step = stmt.step_result();
            if (step == ost_db::StepResult::Error)
            {
                error = "SELECT MAX([" + column + "]) FROM [" + table + "] failed: " + stmt.error_message();
                return false;
            }
            if (step == ost_db::StepResult::Row)
                return read_integer(stmt, 0, has_value, value, table + "." + column, error);
            return true;
        }

        std::vector<std::string> bid_tables_write_order()
        {
            // Windows constants.BID_TABLES_WRITE_ORDER.
            std::vector<std::string> order;
            for (const char *table : ost_schema::BID_SECTIONS)
            {
                const std::string name = table;
                if (name != "BidSettings" && name != "BidTypGroupViews")
                    order.push_back(name);
            }
            order.push_back("BidPages");
            order.push_back("BidSettings");
            order.push_back("BidTypGroupViews");
            order.push_back("BidNamedViews");
            order.push_back("BidHotLinks");
            return order;
        }

        bool is_leap_year(long long year)
        {
            return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        }

        // Windows _convert_access_value for a DATETIME column: parts = value.split(); exactly 6 parts ->
        // datetime.datetime(int(parts[0]), ..., int(parts[5])); true when that datetime exists (year 1..9999, month 1..12, day
        // of the month, hour 0..23, minute 0..59, second 0..59), false when Windows returns None (not six parts, a part that is
        // not an integer, a date that does not exist: ValueError) and also for an integer outside the C int range: datetime then
        // raises OverflowError, which Windows catches together with ValueError and TypeError since b9902e6 (parse_ost_datetime, ost_text_values.py:15-22, called at import_operations.py:597,
        // test_a_datetime_text_beyond_the_c_int_range_reads_as_null), so the value is None there too and the import goes on, like here.
        // Such a value never passes the range checks below anyway.
        bool is_valid_datetime(const std::string &value)
        {
            const std::vector<std::string> parts = text_rules::py_split(value);
            if (parts.size() != 6)
                return false;
            long long n[6] = {0, 0, 0, 0, 0, 0};
            for (size_t i = 0; i < 6; ++i)
                if (text_rules::py_parse_int(parts[i], n[i]) != text_rules::NumberParse::Ok)
                    return false; // not an integer, or one of more than 64 bits
            static const int days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            if (n[0] < 1 || n[0] > 9999 || n[1] < 1 || n[1] > 12)
                return false;
            const long long days = days_in_month[n[1] - 1] + ((n[1] == 2 && is_leap_year(n[0])) ? 1 : 0);
            return n[2] >= 1 && n[2] <= days && n[3] >= 0 && n[3] <= 23 && n[4] >= 0 && n[4] <= 59 && n[5] >= 0 && n[5] <= 59;
        }
    }

    bool ImportTableInfo::has_column(const std::string &name) const
    {
        if (!column_set.empty())
            return column_set.count(name) > 0;
        return std::find(columns.begin(), columns.end(), name) != columns.end();
    }

    bool import_convert_value(const std::string &value, const std::string &type_name, ImportValue &out, std::string *failure)
    {
        out = ImportValue();
        if (value == "NULL")
            return true;
        if (type_name == "yesno")
        {
            out.kind = ImportValue::Integer;
            out.integer = (value == "True" || value == "true" || value == "-1" || value == "1") ? -1 : 0;
            return true;
        }
        if (type_name == "datetime")
        {
            // IMP-20 (the user's decision): a valid six part date keeps its raw text, everything else is NULL.
            if (is_valid_datetime(value))
            {
                out.kind = ImportValue::Text;
                out.text = value;
            }
            return true;
        }
        if (type_name.find("longbinary") != std::string::npos || type_name.find("memo") != std::string::npos)
        {
            // Windows: '' -> NULL, text -> its UTF-8 bytes (kept as text here; the latin-1 name encoding of BidTexts /
            // BidCallOuts is not ported, see IMP-20). Checked before the numeric types: "longbinary" contains "long".
            if (value.empty())
                return true;
            out.kind = ImportValue::Text;
            out.text = value;
            return true;
        }
        if (is_numeric_type(type_name))
        {
            if (value.empty())
                return true;
            if (value == "0")
            {
                out.kind = ImportValue::Integer;
                out.integer = 0;
                return true;
            }
            if (value.find('.') != std::string::npos)
            {
                double parsed = 0.0;
                if (text_rules::py_parse_float(value, parsed))
                {
                    out.kind = ImportValue::Real;
                    out.real = parsed;
                }
                return true;
            }
            long long parsed = 0;
            switch (text_rules::py_parse_int(value, parsed))
            {
            case text_rules::NumberParse::Ok:
                out.kind = ImportValue::Integer;
                out.integer = parsed;
                return true;
            case text_rules::NumberParse::Overflow:
                if (failure)
                    *failure = "Python int too large to convert to SQLite INTEGER";
                return false;
            default:
                return true; // ValueError reads as NULL
            }
        }
        out.kind = ImportValue::Text;
        out.text = value;
        return true;
    }

    ImportTableInfo import_get_table_info(ost_db::OstDatabase &db, const std::string &table)
    {
        ImportTableInfo info;
        ost_db::Statement stmt = db.prepare("SELECT name, type, pk FROM pragma_table_info(?)");
        if (!stmt.valid())
            return info;
        stmt.bind_text(1, table);
        while (stmt.step())
        {
            const std::string name = stmt.column_text(0);
            info.columns.push_back(name);
            info.column_set.insert(name);
            // The Windows column type (the iOS DDL flattened YESNO / DATETIME / IMAGE): the ODBC driver reports them as
            // yesno, datetime and longbinary; any other column is judged by its declared type.
            info.types[name] = ost_schema::is_yesno_column(table, name)      ? std::string("yesno")
                               : ost_schema::is_datetime_column(table, name) ? std::string("datetime")
                               : ost_schema::is_image_column(table, name)    ? std::string("longbinary")
                                                                             : text_rules::py_lower(stmt.column_text(1));
            info.primary_key[name] = stmt.column_int(2) > 0;
        }
        if (stmt.failed())
            return ImportTableInfo();
        return info;
    }

    bool import_should_omit_autonumber_uid(const ImportTableInfo &info, const std::string &column, const std::string &value)
    {
        if (column != "UID")
            return false;
        auto type = info.types.find(column);
        if (type == info.types.end())
            return false;
        auto pk = info.primary_key.find(column);
        const bool autonumber = type->second == "counter" ||
                                (type->second == "integer" && pk != info.primary_key.end() && pk->second);
        return autonumber && (value.empty() || value == "0" || value == "NULL");
    }

    bool import_insert_raw_row(ost_db::OstDatabase &db,
                               const std::string &table,
                               const Row &row,
                               const ImportTableInfo &info,
                               std::string &error)
    {
        std::vector<const std::pair<const std::string, std::string> *> filtered;
        for (const auto &field : row)
        {
            if (!info.has_column(field.first))
                continue;
            if (import_should_omit_autonumber_uid(info, field.first, field.second))
                continue;
            filtered.push_back(&field);
        }
        if (filtered.empty())
            return true;
        std::sort(filtered.begin(), filtered.end(),
                  [](const std::pair<const std::string, std::string> *a, const std::pair<const std::string, std::string> *b)
                  { return a->first < b->first; });

        std::string columns, placeholders;
        for (size_t i = 0; i < filtered.size(); ++i)
        {
            if (i)
            {
                columns += ", ";
                placeholders += ", ";
            }
            columns += "[" + filtered[i]->first + "]";
            placeholders += "?";
        }

        auto uid_it = row.find("UID");
        const std::string failure_prefix = "Failed inserting into [" + table + "], row_uid=" +
                                           (uid_it == row.end() ? std::string() : uid_it->second) +
                                           ", columns=" + repr_sorted_keys(row) + ", error=";

        ost_db::Statement stmt = db.prepare("INSERT INTO [" + table + "] (" + columns + ") VALUES (" + placeholders + ")");
        if (!stmt.valid())
        {
            error = failure_prefix + stmt.error_message();
            return false;
        }
        for (size_t i = 0; i < filtered.size(); ++i)
        {
            auto type = info.types.find(filtered[i]->first);
            ImportValue value;
            std::string conversion_failure;
            if (!import_convert_value(filtered[i]->second, type == info.types.end() ? std::string() : type->second, value, &conversion_failure))
            {
                error = failure_prefix + conversion_failure;
                return false;
            }
            const int idx = static_cast<int>(i) + 1;
            switch (value.kind)
            {
            case ImportValue::Null:
                stmt.bind_null(idx);
                break;
            case ImportValue::Integer:
                stmt.bind_int64(idx, value.integer);
                break;
            case ImportValue::Real:
                stmt.bind_double(idx, value.real);
                break;
            case ImportValue::Text:
                stmt.bind_text(idx, value.text);
                break;
            }
        }
        if (!stmt.run())
        {
            error = failure_prefix + stmt.error_message();
            return false;
        }
        return true;
    }

    bool import_write_to_db(ost_db::OstDatabase &db, const ost_import::OstRawData &remapped, std::string &error)
    {
        ImportTableInfo info = import_get_table_info(db, "Bids");
        if (!import_insert_raw_row(db, "Bids", remapped.bid_row, info, error))
            return false;

        for (const std::string &table : bid_tables_write_order())
        {
            auto it = remapped.bid_tables.find(table);
            if (it == remapped.bid_tables.end() || it->second.empty())
                continue;
            info = import_get_table_info(db, table);
            for (const auto &row : it->second)
                if (!import_insert_raw_row(db, table, row, info, error))
                    return false;
        }
        for (const char *table_name : ost_schema::PAGE_SECTIONS)
        {
            const std::string table = table_name;
            auto it = remapped.page_tables.find(table);
            if (it == remapped.page_tables.end() || it->second.empty())
                continue;
            info = import_get_table_info(db, table);
            for (const auto &row : it->second)
                if (!import_insert_raw_row(db, table, row, info, error))
                    return false;
        }
        return true;
    }

    bool import_get_max_uid(ost_db::OstDatabase &db, long long &max_uid, std::string &error)
    {
        max_uid = 0;
        std::vector<std::string> tables_to_check = {"Bids", "CdnTypes", "JobStatuses"};
        for (const char *table : ost_schema::BID_SECTIONS)
            tables_to_check.push_back(table);
        tables_to_check.push_back("BidPages");
        for (const char *table : ost_schema::BID_TAIL_SECTIONS)
            tables_to_check.push_back(table);
        for (const char *table : ost_schema::PAGE_SECTIONS)
            tables_to_check.push_back(table);

        for (const std::string &table : tables_to_check)
        {
            bool skipped = false, has_value = false;
            long long value = 0;
            if (!select_max(db, table, "UID", skipped, has_value, value, error))
                return false;
            if (has_value && value > max_uid)
                max_uid = value;
        }

        std::vector<std::pair<std::string, std::string>> checked;
        for (const auto &relationship : ost_schema::BID_RELATIONSHIPS)
        {
            if (std::find(tables_to_check.begin(), tables_to_check.end(), std::string(relationship.parent_table)) ==
                tables_to_check.end())
                continue;
            const std::pair<std::string, std::string> reference(relationship.child_table, relationship.child_column);
            if (std::find(checked.begin(), checked.end(), reference) != checked.end())
                continue;
            checked.push_back(reference);
            if (!db.table_exists(reference.first) || !db.column_exists(reference.first, reference.second))
                continue;
            bool skipped = false, has_value = false;
            long long value = 0;
            if (!select_max(db, reference.first, reference.second, skipped, has_value, value, error))
                return false;
            if (has_value && value > max_uid)
                max_uid = value;
        }
        return true;
    }

    bool import_assign_next_bid_no(ost_db::OstDatabase &db, Row &bid_row, std::string &error)
    {
        // require_writable_bid_number_allocator
        if (!db.table_exists("Settings"))
        {
            error = "This database can be read but cannot create bids because the Settings table is unavailable.";
            return false;
        }
        if (!db.column_exists("Settings", "NextBidNo"))
        {
            error = "This database can be read but cannot create bids because Settings.NextBidNo is unavailable.";
            return false;
        }

        long long next_bid_no = 1;
        bool has_row = false;
        {
            // fetch_optional_global_settings_row: at most one row.
            ost_db::Statement stmt = db.prepare("SELECT [NextBidNo] FROM [Settings]");
            if (!stmt.valid())
            {
                error = "SELECT [NextBidNo] FROM [Settings] failed: " + stmt.error_message();
                return false;
            }
            const ost_db::StepResult first = stmt.step_result();
            if (first == ost_db::StepResult::Error)
            {
                error = "SELECT [NextBidNo] FROM [Settings] failed: " + stmt.error_message();
                return false;
            }
            if (first == ost_db::StepResult::Row)
            {
                has_row = true;
                // The first row is read before the second step replaces it; its value is judged after the
                // cardinality check, like Windows (a second row fails before the value is looked at).
                const ost_db::Statement::ColumnType type = stmt.column_type(0);
                const bool blank = (type == ost_db::Statement::TypeNull) ||
                                   (type == ost_db::Statement::TypeText && stmt.column_text(0).empty());
                bool has_value = false;
                long long value = 0;
                std::string value_error;
                bool value_ok = true;
                if (!blank)
                    value_ok = read_integer(stmt, 0, has_value, value, "Settings.NextBidNo", value_error);

                const ost_db::StepResult second = stmt.step_result();
                if (second == ost_db::StepResult::Error)
                {
                    error = "SELECT [NextBidNo] FROM [Settings] failed: " + stmt.error_message();
                    return false;
                }
                if (second == ost_db::StepResult::Row)
                {
                    error = "Settings has multiple rows; expected at most one.";
                    return false;
                }
                if (!value_ok)
                {
                    error = value_error;
                    return false;
                }
                // normalize_next_bid_number: None / "" -> 1, 0 -> 1.
                if (!blank && has_value)
                    next_bid_no = (value != 0) ? value : 1;
            }
        }

        // persist_next_bid_number
        {
            ost_db::Statement stmt = db.prepare(has_row ? "UPDATE [Settings] SET [NextBidNo] = ?"
                                                         : "INSERT INTO [Settings] ([NextBidNo]) VALUES (?)");
            if (!stmt.valid())
            {
                error = "Settings.NextBidNo could not be written: " + stmt.error_message();
                return false;
            }
            stmt.bind_int64(1, next_bid_no + 1);
            if (!stmt.run())
            {
                error = "Settings.NextBidNo could not be written: " + stmt.error_message();
                return false;
            }
        }
        bid_row["BidNo"] = std::to_string(next_bid_no);
        return true;
    }

    bool import_next_table_uid(ost_db::OstDatabase &db, const std::string &table, long long &next_uid, std::string &error)
    {
        // _next_uid: MAX([UID]) + 1, 1 for an empty table. Failures are not swallowed here.
        next_uid = 1;
        {
            ost_db::Statement stmt = db.prepare("SELECT MAX([UID]) FROM [" + table + "]");
            if (!stmt.valid())
            {
                error = "SELECT MAX([UID]) FROM [" + table + "] failed: " + stmt.error_message();
                return false;
            }
            const ost_db::StepResult step = stmt.step_result();
            if (step == ost_db::StepResult::Error)
            {
                error = "SELECT MAX([UID]) FROM [" + table + "] failed: " + stmt.error_message();
                return false;
            }
            bool has_value = false;
            long long value = 0;
            if (step == ost_db::StepResult::Row && !read_integer(stmt, 0, has_value, value, table + ".UID", error))
                return false;
            if (has_value)
                next_uid = value + 1;
        }

        std::vector<std::pair<std::string, std::string>> checked;
        for (const auto &relationship : ost_schema::BID_RELATIONSHIPS)
        {
            if (table != relationship.parent_table)
                continue;
            const std::pair<std::string, std::string> reference(relationship.child_table, relationship.child_column);
            if (std::find(checked.begin(), checked.end(), reference) != checked.end())
                continue;
            checked.push_back(reference);
            if (!db.table_exists(reference.first) || !db.column_exists(reference.first, reference.second))
                continue;
            ost_db::Statement stmt = db.prepare("SELECT MAX([" + reference.second + "]) FROM [" + reference.first + "]");
            if (!stmt.valid())
            {
                error = "SELECT MAX([" + reference.second + "]) FROM [" + reference.first + "] failed: " + stmt.error_message();
                return false;
            }
            const ost_db::StepResult step = stmt.step_result();
            if (step == ost_db::StepResult::Error)
            {
                error = "SELECT MAX([" + reference.second + "]) FROM [" + reference.first + "] failed: " + stmt.error_message();
                return false;
            }
            if (step != ost_db::StepResult::Row)
                continue;
            // row is None or row[0] in (None, "", 0, "0") -> nothing to preserve.
            const ost_db::Statement::ColumnType type = stmt.column_type(0);
            if (type == ost_db::Statement::TypeNull)
                continue;
            if (type == ost_db::Statement::TypeText)
            {
                const std::string text = stmt.column_text(0);
                if (text.empty() || text == "0")
                    continue;
            }
            else if ((type == ost_db::Statement::TypeInteger && stmt.column_int64(0) == 0) ||
                     (type == ost_db::Statement::TypeFloat && stmt.column_double(0) == 0.0))
            {
                continue;
            }
            bool has_value = false;
            long long referenced = 0;
            if (!read_integer(stmt, 0, has_value, referenced, reference.first + "." + reference.second, error))
                return false;
            if (has_value && referenced >= next_uid)
                next_uid = referenced + 1;
        }
        return true;
    }
}
