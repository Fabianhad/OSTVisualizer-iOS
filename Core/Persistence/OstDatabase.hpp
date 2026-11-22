#pragma once
#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace ost_db
{

    // Outcome of one sqlite3_step. DONE (finished) and ERROR must never be conflated: a write that failed is not a
    // write that finished.
    enum class StepResult
    {
        Row,
        Done,
        Error
    };

    class Statement
    {
    public:
        Statement() = default;
        ~Statement();

        Statement(const Statement &) = delete;
        Statement &operator=(const Statement &) = delete;
        Statement(Statement &&other) noexcept;
        Statement &operator=(Statement &&other) noexcept;

        bool valid() const { return _stmt != nullptr; }

        // A bind that sqlite rejects (index out of range, ...) is remembered: step_result()/step()/run() then fail
        // without executing the statement, so a half-bound statement can never run.
        void bind_int(int idx, int value);
        void bind_int64(int idx, long long value);
        void bind_double(int idx, double value);
        void bind_text(int idx, const std::string &value);
        void bind_null(int idx);

        // Executes one step.
        //   Row   - a result row is available (SELECT).
        //   Done  - the statement finished successfully.
        //   Error - the step (or an earlier bind / the prepare) failed; see failed(), error_code(), error_message().
        StepResult step_result();

        // Read loops: true only while a result row is available. false for BOTH "no more rows" and "error": after
        // the loop check failed() before trusting the rows that were read.
        bool step();

        // Write statements (INSERT / UPDATE / DELETE / DDL): runs the statement to completion. true only when it
        // finished without error; false when it failed (failed() is true, error_code()/error_message() say why).
        bool run();

        // True when the statement could not be prepared, a bind was rejected or a step returned an error
        // (cleared by reset()).
        bool failed() const { return _rc != 0; }
        // sqlite primary result code of the failure (0 = no failure) and sqlite's message for it.
        int error_code() const { return _rc; }
        const std::string &error_message() const { return _error; }
        // True when prepare failed because the statement does not match the schema (SQLITE_ERROR: unknown table or
        // column), as opposed to a real failure (I/O error, busy, out of memory, ...).
        bool schema_mismatch() const;

        void reset();

        int column_int(int idx) const;
        long long column_int64(int idx) const;
        double column_double(int idx) const;
        // Storage class of the value of the current row (sqlite: 1 INTEGER, 2 FLOAT, 3 TEXT, 4 BLOB, 5 NULL).
        enum ColumnType
        {
            TypeInteger = 1,
            TypeFloat = 2,
            TypeText = 3,
            TypeBlob = 4,
            TypeNull = 5
        };
        ColumnType column_type(int idx) const;
        std::string column_text(int idx) const;

    private:
        friend class OstDatabase;
        void note_bind(int rc);
        void note_failure(int rc);

        sqlite3_stmt *_stmt = nullptr;
        int _rc = 0;
        bool _bind_failed = false;
        std::string _error;
    };

    class OstDatabase
    {
    public:
        OstDatabase() = default;
        ~OstDatabase();

        OstDatabase(const OstDatabase &) = delete;
        OstDatabase &operator=(const OstDatabase &) = delete;

        // Connections wait up to kBusyTimeoutMs for a lock held by another connection (a write from the UI while an
        // import runs) instead of failing at once. iOS platform adaptation: Windows serializes writers per database
        // behind an in-process lock (MdbConnectionManager path lock), which SQLite has no equivalent for.
        static constexpr int kBusyTimeoutMs = 5000;

        bool open(const std::string &path);
        void close();
        bool is_open() const { return _db != nullptr; }

        // false when the SQL failed; last_error_code()/last_error_message() say why.
        bool exec(const std::string &sql);
        // A statement that cannot be prepared is returned invalid (valid() == false) and carries the prepare error
        // (failed(), error_code(), error_message(), schema_mismatch()); last_error_*() reports it too.
        Statement prepare(const std::string &sql);

        // BEGIN IMMEDIATE: waits (busy timeout) for another connection's write lock, then holds it until COMMIT/ROLLBACK.
        // Every caller is a writer.
        bool begin_transaction();
        // false when COMMIT failed. A failed COMMIT never leaves the transaction open: it is rolled back here, so the
        // caller only has to report the failure.
        bool commit();
        bool rollback();
        // True between a successful BEGIN and its COMMIT/ROLLBACK. A failed statement normally leaves the transaction
        // open (RAISE(ABORT), constraint); some errors (I/O, full disk, RAISE(ROLLBACK)) roll it back by themselves.
        bool in_transaction() const;

        // Schema inspection, same meaning as MdbSchemaInspector.table_exists/column_exists (names compare
        // case-insensitively). A failed lookup reports "not found", like the Windows inspector.
        bool table_exists(const std::string &table);
        bool column_exists(const std::string &table, const std::string &column);

        // sqlite primary result code and message of the last failed exec/prepare/commit on this connection (0 and ""
        // when none failed). Failed steps are reported by their Statement.
        int last_error_code() const { return _last_error_code; }
        const std::string &last_error_message() const { return _last_error; }

    private:
        void note_error(int rc, const char *message);

        sqlite3 *_db = nullptr;
        int _last_error_code = 0;
        std::string _last_error;
    };

}
