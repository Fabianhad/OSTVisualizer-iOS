// ProjectStore component - the bid lock guard (see ProjectStore_BidLock.hpp and ProjectStore_Integrity.hpp, plan 4.1).
// Windows: cover_sheet.py:15-23 (the rule: ost_domain::resolve_bid_status_lock), project_read_service.py:81-94 (is_bid_locked),
// settings_reader.py:63-77 (_parse_job_statuses) and :361-370 (get_job_statuses).
#include "../ProjectStore_BidLock.hpp"
#include "../ProjectStore_Integrity.hpp"
#include "../../../Domain/BidLock.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <set>
#include <vector>

namespace ost_store
{
    namespace integrity
    {

        namespace
        {
            // Python's strict UTF-8 decode (bytes.decode("utf-8")): no overlong form, no surrogate, nothing past U+10FFFF, no truncation.
            bool valid_utf8_strict(const std::string &s)
            {
                std::size_t i = 0;
                while (i < s.size())
                {
                    const unsigned char c = static_cast<unsigned char>(s[i]);
                    std::size_t need = 0;
                    unsigned long cp = 0;
                    if (c < 0x80)
                    {
                        ++i;
                        continue;
                    }
                    if (c >= 0xC2 && c <= 0xDF) { need = 1; cp = c & 0x1F; }
                    else if (c >= 0xE0 && c <= 0xEF) { need = 2; cp = c & 0x0F; }
                    else if (c >= 0xF0 && c <= 0xF4) { need = 3; cp = c & 0x07; }
                    else return false;
                    if (i + need >= s.size()) return false; // truncated sequence
                    for (std::size_t k = 1; k <= need; ++k)
                    {
                        const unsigned char cc = static_cast<unsigned char>(s[i + k]);
                        if ((cc & 0xC0) != 0x80) return false;
                        cp = (cp << 6) | (cc & 0x3F);
                    }
                    if ((need == 2 && cp < 0x800) || (need == 3 && cp < 0x10000) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
                        return false;
                    i += need + 1;
                }
                return true;
            }

            // Windows serialize_value (ost_serializer.py:18-34) of one stored cell: the reader hands every value to the rule as TEXT, a NULL
            // as "NULL", an integer as its digits, a real as its shortest %.15f form without a trailing ".0" (an integral real below 1e15
            // as an integer). A real that is not finite makes Windows raise (int(inf)): false, the whole JobStatuses read is then unusable.
            // A BLOB is decoded as strict UTF-8 (invalid bytes raise: false) and every NUL is removed; a TEXT keeps an embedded NUL, and TEXT
            // that is not valid UTF-8 (Python's sqlite3 cannot decode it either) is also false: the safe side, the bid reads locked.
            bool serialized_cell(ost_db::Statement &stmt, int column, std::string &out)
            {
                switch (stmt.column_type(column))
                {
                case ost_db::Statement::TypeNull:
                    out = "NULL";
                    return true;
                case ost_db::Statement::TypeInteger:
                    out = std::to_string(stmt.column_int64(column));
                    return true;
                case ost_db::Statement::TypeFloat:
                {
                    const double value = stmt.column_double(column);
                    if (!std::isfinite(value))
                        return false;
                    if (value == std::trunc(value) && std::fabs(value) < 1e15)
                    {
                        out = std::to_string(static_cast<long long>(value));
                        return true;
                    }
                    char buffer[400];
                    std::snprintf(buffer, sizeof buffer, "%.15f", value);
                    out = buffer;
                    while (!out.empty() && out.back() == '0')
                        out.pop_back();
                    if (!out.empty() && out.back() == '.')
                        out.pop_back();
                    return true;
                }
                case ost_db::Statement::TypeBlob:
                {
                    // ost_serializer.py:24-26: bytes.decode("utf-8") is strict (invalid UTF-8 raises), then every NUL is removed.
                    const std::string bytes = stmt.column_text_exact(column);
                    if (!valid_utf8_strict(bytes))
                        return false;
                    out.clear();
                    for (const char c : bytes)
                        if (c != '\0')
                            out.push_back(c);
                    return true;
                }
                default: // TEXT: str(value), an embedded NUL stays; invalid UTF-8 cannot be decoded by Python's sqlite3 either (a raise)
                    out = stmt.column_text_exact(column);
                    return valid_utf8_strict(out);
                }
            }

            // Python int(text) for the Sequence cell (settings_reader.py:72 int(_clean_optional_text(row.get("Sequence")) or 0)): surrounding
            // whitespace, an optional sign, decimal digits with single underscores between them. The text is never used, only whether
            // Windows would have raised (and so returned an empty status list: every status unresolved). Non-ASCII digits, which Python
            // accepts, count as not an integer (the safe side: the bid reads locked).
            bool python_int_accepts(const std::string &text)
            {
                auto is_space = [](char c)
                { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; };
                std::size_t begin = 0;
                std::size_t end = text.size();
                while (begin < end && is_space(text[begin]))
                    ++begin;
                while (end > begin && is_space(text[end - 1]))
                    --end;
                if (begin < end && (text[begin] == '+' || text[begin] == '-'))
                    ++begin;
                if (begin == end)
                    return false;
                bool previous_digit = false;
                for (std::size_t i = begin; i < end; ++i)
                {
                    const bool digit = text[i] >= '0' && text[i] <= '9';
                    if (!digit && !(text[i] == '_' && previous_digit && i + 1 < end))
                        return false;
                    previous_digit = digit;
                }
                return previous_digit;
            }

            // The JobStatuses table as Windows get_job_statuses returns it, or false when a statement failed. When the Windows read would
            // have raised (a duplicate UID, a Sequence that int() refuses, a real that is not finite, a cell that cannot be decoded) the
            // list is EMPTY (settings_reader.py:361-370), so every status is unresolved. The Name column is read as well when it exists,
            // because Windows serializes every column of every row. A missing table is an empty list as well
            // (optional_table_missing). A missing column fails the statement: false (Windows would read a missing Locked as unlocked; the
            // iOS schema always has the columns, so the safe side wins).
            bool read_job_statuses(ost_db::OstDatabase &db, std::vector<ost_domain::JobStatusLock> &statuses)
            {
                statuses.clear();
                if (!db.table_exists("JobStatuses"))
                    return true;
                const bool has_name = db.column_exists("JobStatuses", "Name");
                ost_db::Statement stmt = db.prepare(has_name ? "SELECT [UID], [Locked], [Sequence], [Name] FROM [JobStatuses]"
                                                             : "SELECT [UID], [Locked], [Sequence] FROM [JobStatuses]");
                if (!stmt.valid())
                    return false;
                std::set<std::string> seen;
                bool usable = true;
                while (usable && stmt.step())
                {
                    std::string uid;
                    std::string flag;
                    std::string sequence;
                    std::string name;
                    if (!serialized_cell(stmt, 0, uid) || !serialized_cell(stmt, 1, flag) || !serialized_cell(stmt, 2, sequence) ||
                        (has_name && !serialized_cell(stmt, 3, name)))
                    {
                        usable = false;
                        break;
                    }
                    // require_unique_master_data_uids runs on the serialized text, a NULL included (master_data_identity.py:19-30)
                    if (!seen.insert(uid).second)
                    {
                        usable = false;
                        break;
                    }
                    // _clean_optional_text: a NULL cell is the empty text for the uid and the sequence
                    if (sequence != "NULL" && !sequence.empty() && !python_int_accepts(sequence))
                    {
                        usable = false;
                        break;
                    }
                    ost_domain::JobStatusLock status;
                    status.uid = (uid == "NULL") ? std::string() : uid;
                    status.locked = !(flag == "0" || flag == "False" || flag.empty()); // settings_reader.py:71
                    statuses.push_back(status);
                }
                if (usable && stmt.failed())
                    return false;
                if (!usable)
                    statuses.clear();
                return true;
            }
        } // namespace

        bool bid_is_locked(ost_db::OstDatabase &db, long long bid_uid, bool &locked)
        {
            locked = true; // fail closed until the answer is known

            // The status of the bid (hierarchy_reader.py:93-95: str(JobStatusUID), None for a NULL).
            std::string status_uid;
            {
                ost_db::Statement stmt = db.prepare("SELECT [JobStatusUID] FROM [Bids] WHERE [UID] = ?");
                if (!stmt.valid())
                    return false;
                stmt.bind_int64(1, bid_uid);
                const ost_db::StepResult result = stmt.step_result();
                if (result == ost_db::StepResult::Error)
                    return false;
                if (result != ost_db::StepResult::Row)
                    return true; // no such bid: locked (plan 4.1), the read itself worked
                switch (stmt.column_type(0))
                {
                case ost_db::Statement::TypeNull:
                    break;
                case ost_db::Statement::TypeBlob:
                    return true; // str(bytes) is "b'..'": never no status, never a status uid: unresolved = locked
                case ost_db::Statement::TypeText:
                    status_uid = stmt.column_text_exact(0); // an embedded NUL stays part of the text
                    break;
                default:
                    status_uid = stmt.column_text(0);
                    break;
                }
            }

            // project_read_service.py:82-83: a bid without a status is unlocked, and JobStatuses is not read at all.
            if (status_uid.empty())
            {
                locked = false;
                return true;
            }

            std::vector<ost_domain::JobStatusLock> statuses;
            if (!read_job_statuses(db, statuses))
                return false;

            // project_read_service.py:84-94: an unresolved status counts as locked.
            locked = ost_domain::bid_status_is_locked(ost_domain::resolve_bid_status_lock(statuses, status_uid));
            return true;
        }

    } // namespace integrity

    bool bid_is_locked(const std::string &db_path, const std::string &bid_uid, bool &locked)
    {
        locked = true;
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return false;
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        bool answer = true;
        const bool read_ok = integrity::bid_is_locked(db, bid, answer);
        locked = answer; // integrity::bid_is_locked always sets it, true when the read failed
        return read_ok;
    }

} // namespace ost_store
