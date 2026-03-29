#pragma once
#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace ost_db
{

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

        void bind_int(int idx, int value);
        void bind_double(int idx, double value);
        void bind_text(int idx, const std::string &value);
        void bind_null(int idx);

        bool step();
        void reset();

        int column_int(int idx) const;
        double column_double(int idx) const;
        std::string column_text(int idx) const;

    private:
        friend class OstDatabase;
        sqlite3_stmt *_stmt = nullptr;
    };

    class OstDatabase
    {
    public:
        OstDatabase() = default;
        ~OstDatabase();

        OstDatabase(const OstDatabase &) = delete;
        OstDatabase &operator=(const OstDatabase &) = delete;

        bool open(const std::string &path);
        void close();
        bool is_open() const { return _db != nullptr; }

        bool exec(const std::string &sql);
        Statement prepare(const std::string &sql);

        bool begin_transaction();
        bool commit();
        bool rollback();

    private:
        sqlite3 *_db = nullptr;
    };

}
