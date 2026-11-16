// TestHarness.hpp - tiny header-only unit-test framework (no external dependencies).
// Works with MSVC, AppleClang/clang and gcc, C++17.
//
//   TEST(Suite, Name) { ... }          auto-registered test (static initializers)
//   CHECK(cond)                        non-fatal
//   CHECK_FALSE(cond)                  non-fatal
//   CHECK_EQ(a, b) / CHECK_NE(a, b)    non-fatal, prints both values (exact ==; use CHECK_NEAR for floats)
//   CHECK_NEAR(a, b, tol)              non-fatal, |a-b| <= tol
//   CHECK_STR_EQ(a, b)                 non-fatal, const char* / std::string / std::string_view
//   REQUIRE(cond) / REQUIRE_EQ(a, b)   fatal: abort the current test
//   FAIL_TEST(msg)                     fatal unconditional failure
//   SCOPED_CONTEXT(msg)                text appended to every failure in the enclosing scope
//                                      (use inside loops: SCOPED_CONTEXT("case " + std::to_string(i));)
//
// Runner (TestMain.cpp): '--filter <substr>' (repeatable, matches "Suite.Name"), '--list', '--help'.
// Prints one PASS/FAIL line per test and a final line 'TESTS: <total> PASSED: <p> FAILED: <f>'.
// Exit code is non-zero if any test failed.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ost_test
{
    struct TestCase
    {
        const char *suite;
        const char *name;
        void (*fn)();
        const char *file;
        int line;
    };

    inline std::vector<TestCase> &registry()
    {
        static std::vector<TestCase> r;
        return r;
    }

    struct Registrar
    {
        Registrar(const char *suite, const char *name, void (*fn)(), const char *file, int line)
        {
            registry().push_back(TestCase{suite, name, fn, file, line});
        }
    };

    // Thrown by REQUIRE*/FAIL_TEST to abort the current test; caught by the runner.
    struct AbortTest
    {
    };

    // Implemented in TestMain.cpp.
    void report_failure(const char *file, int line, const std::string &message);
    void push_context(const std::string &text);
    void pop_context();

    // While alive, failures are collected here instead of failing the running test.
    // Used only by the harness self-test to prove that failing checks are detected.
    struct FailureCapture
    {
        std::vector<std::string> messages;
        FailureCapture();
        ~FailureCapture();
        FailureCapture(const FailureCapture &) = delete;
        FailureCapture &operator=(const FailureCapture &) = delete;
    };

    struct ScopedContext
    {
        explicit ScopedContext(const std::string &text) { push_context(text); }
        ~ScopedContext() { pop_context(); }
        ScopedContext(const ScopedContext &) = delete;
        ScopedContext &operator=(const ScopedContext &) = delete;
    };

    namespace detail
    {
        // ---------- value printing ----------
        inline std::string quote(std::string_view s)
        {
            std::string out = "\"";
            for (unsigned char c : s)
            {
                switch (c)
                {
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                default:
                    if (c < 0x20 || c == 0x7f)
                    {
                        char buf[8];
                        std::snprintf(buf, sizeof buf, "\\x%02x", c);
                        out += buf;
                    }
                    else
                    {
                        out += static_cast<char>(c);
                    }
                }
            }
            out += '"';
            return out;
        }

        // Shortest decimal representation that round-trips.
        template <class F>
        std::string format_float(F v)
        {
            if (std::isnan(v))
                return "nan";
            if (std::isinf(v))
                return v < 0 ? "-inf" : "inf";
            char buf[64];
            const int maxp = std::numeric_limits<F>::max_digits10;
            for (int p = 1; p <= maxp; ++p)
            {
                std::snprintf(buf, sizeof buf, "%.*g", p, static_cast<double>(v));
                F back = std::is_same_v<F, float> ? static_cast<F>(std::strtof(buf, nullptr))
                                                  : static_cast<F>(std::strtod(buf, nullptr));
                if (back == v)
                    break;
            }
            return buf;
        }

        inline std::string exact_float(double v)
        {
            char buf[64];
            std::snprintf(buf, sizeof buf, "%.17g", v);
            return buf;
        }

        template <class T, class = void>
        struct is_streamable : std::false_type
        {
        };
        template <class T>
        struct is_streamable<T, std::void_t<decltype(std::declval<std::ostream &>() << std::declval<const T &>())>>
            : std::true_type
        {
        };

        template <class T, class = void>
        struct Printer
        {
            static std::string str(const T &v)
            {
                if constexpr (std::is_same_v<T, bool>)
                    return v ? "true" : "false";
                else if constexpr (std::is_same_v<T, std::nullptr_t>)
                    return "nullptr";
                else if constexpr (std::is_same_v<T, char>)
                {
                    std::string s = quote(std::string_view(&v, 1));
                    s[0] = '\'';
                    s[s.size() - 1] = '\'';
                    return s + " (" + std::to_string(static_cast<int>(static_cast<unsigned char>(v))) + ")";
                }
                else if constexpr (std::is_same_v<T, signed char> || std::is_same_v<T, unsigned char>)
                    return std::to_string(static_cast<int>(v));
                else if constexpr (std::is_floating_point_v<T>)
                    return format_float<T>(v);
                else if constexpr (std::is_enum_v<T>)
                    return std::to_string(static_cast<long long>(static_cast<std::underlying_type_t<T>>(v)));
                else if constexpr (std::is_same_v<T, const char *> || std::is_same_v<T, char *>)
                    return v ? quote(v) : std::string("(null)");
                else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>)
                    return quote(v);
                else if constexpr (std::is_pointer_v<T> || std::is_member_pointer_v<T>)
                {
                    std::ostringstream os;
                    if constexpr (std::is_pointer_v<T>)
                        os << static_cast<const void *>(v);
                    else
                        os << "<member pointer>";
                    return os.str();
                }
                else if constexpr (is_streamable<T>::value)
                {
                    std::ostringstream os;
                    os << v;
                    return os.str();
                }
                else
                    return "<unprintable " + std::to_string(sizeof(T)) + " bytes>";
            }
        };

        template <class T>
        std::string to_str(const T &v)
        {
            return Printer<std::remove_cv_t<std::remove_reference_t<T>>>::str(v);
        }

        // char arrays (string literals) print as strings
        template <std::size_t N>
        struct Printer<char[N], void>
        {
            static std::string str(const char (&v)[N]) { return quote(std::string_view(v, N ? std::strlen(v) : 0)); }
        };

        template <class T, class A>
        struct Printer<std::vector<T, A>, void>
        {
            static std::string str(const std::vector<T, A> &v)
            {
                std::string out = "[";
                std::size_t n = 0;
                for (const auto &e : v)
                {
                    if (n++)
                        out += ", ";
                    if (n > 32)
                    {
                        out += "... (" + std::to_string(v.size()) + " elements)";
                        break;
                    }
                    out += to_str(e);
                }
                return out + "]";
            }
        };

        template <class T1, class T2>
        struct Printer<std::pair<T1, T2>, void>
        {
            static std::string str(const std::pair<T1, T2> &p)
            {
                return "(" + to_str(p.first) + ", " + to_str(p.second) + ")";
            }
        };

        template <class T>
        struct Printer<std::optional<T>, void>
        {
            static std::string str(const std::optional<T> &o) { return o ? to_str(*o) : std::string("nullopt"); }
        };

        template <>
        struct Printer<std::nullopt_t, void>
        {
            static std::string str(const std::nullopt_t &) { return "nullopt"; }
        };

        // ---------- comparison ----------
        template <class T>
        inline constexpr bool is_plain_int_v = std::is_integral_v<T> && !std::is_same_v<T, bool>;

        template <class A, class B>
        bool equal(const A &a, const B &b)
        {
            if constexpr (is_plain_int_v<A> && is_plain_int_v<B> && (std::is_signed_v<A> != std::is_signed_v<B>))
            {
                if constexpr (std::is_signed_v<A>)
                    return a >= 0 && static_cast<std::make_unsigned_t<A>>(a) == b;
                else
                    return b >= 0 && a == static_cast<std::make_unsigned_t<B>>(b);
            }
            else
            {
                return a == b;
            }
        }

        // ---------- failure helpers ----------
        template <class A, class B>
        void fail_cmp(const char *file, int line, const char *macro, const char *ea, const char *eb,
                      const char *op, const A &a, const B &b, bool fatal)
        {
            std::string sa = to_str(a), sb = to_str(b);
            if (sa == sb)
            {
                // e.g. 0.1f vs 0.1: identical shortest text, different values -> show full precision
                if constexpr (std::is_floating_point_v<A>)
                    sa = exact_float(a);
                if constexpr (std::is_floating_point_v<B>)
                    sb = exact_float(b);
            }
            std::string msg = std::string(macro) + "(" + ea + ", " + eb + ") failed: " + sa + " " + op + " " + sb;
            report_failure(file, line, msg);
            if (fatal)
                throw AbortTest{};
        }

        template <class A, class B, class T>
        void check_near(const char *file, int line, const char *ea, const char *eb, const char *et,
                        const A &a, const B &b, const T &tol)
        {
            const double da = static_cast<double>(a), db = static_cast<double>(b), dt = static_cast<double>(tol);
            const bool ok = (da == db) || (std::fabs(da - db) <= dt);
            if (!ok)
            {
                std::string msg = std::string("CHECK_NEAR(") + ea + ", " + eb + ", " + et + ") failed: " +
                                  to_str(a) + " vs " + to_str(b) + " (|diff| = " +
                                  format_float<double>(std::fabs(da - db)) + " > tol " +
                                  format_float<double>(dt) + ")";
                report_failure(file, line, msg);
            }
        }

        struct StrArg
        {
            bool is_null = false;
            std::string_view sv;
        };
        inline StrArg str_arg(const char *p)
        {
            StrArg a;
            a.is_null = (p == nullptr);
            if (p)
                a.sv = p;
            return a;
        }
        inline StrArg str_arg(char *p) { return str_arg(static_cast<const char *>(p)); }
        inline StrArg str_arg(const std::string &s)
        {
            StrArg a;
            a.sv = s;
            return a;
        }
        inline StrArg str_arg(std::string_view s)
        {
            StrArg a;
            a.sv = s;
            return a;
        }

        inline std::string excerpt(std::string_view s, std::size_t diff)
        {
            const std::size_t kMax = 240, kHalf = 100;
            if (s.size() <= kMax)
                return quote(s);
            std::size_t from = diff > kHalf ? diff - kHalf : 0;
            std::size_t to = std::min(s.size(), diff + kHalf);
            std::string out;
            if (from > 0)
                out += "...";
            out += quote(s.substr(from, to - from));
            if (to < s.size())
                out += "...";
            return out + " (length " + std::to_string(s.size()) + ")";
        }

        inline void check_str_eq(const char *file, int line, const char *ea, const char *eb,
                                 const StrArg &a, const StrArg &b, bool fatal)
        {
            if (a.is_null == b.is_null && (a.is_null || a.sv == b.sv))
                return;
            std::string msg = std::string("CHECK_STR_EQ(") + ea + ", " + eb + ") failed: ";
            if (a.is_null || b.is_null)
            {
                msg += (a.is_null ? std::string("(null)") : quote(a.sv)) + " vs " +
                       (b.is_null ? std::string("(null)") : quote(b.sv));
            }
            else
            {
                std::size_t i = 0;
                const std::size_t n = std::min(a.sv.size(), b.sv.size());
                while (i < n && a.sv[i] == b.sv[i])
                    ++i;
                msg += excerpt(a.sv, i) + " vs " + excerpt(b.sv, i) + " (first difference at index " +
                       std::to_string(i) + ")";
            }
            report_failure(file, line, msg);
            if (fatal)
                throw AbortTest{};
        }
    } // namespace detail

    // ---------- file helpers ----------
#ifdef OST_FIXTURE_DIR
    // Absolute path of a file under Config/Tests/Fixtures (forward slashes).
    inline std::string fixture_path(const std::string &relative)
    {
        return std::string(OST_FIXTURE_DIR) + "/" + relative;
    }
#endif

    // Reads a whole file as bytes; returns false if it cannot be opened.
    inline bool read_file(const std::string &path, std::string &out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return false;
        out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        return true;
    }

    // A path in the system temp directory (never inside the repo): <tmp>/ost_core_tests/<name>.
    // The directory is created; the caller deletes the file when done.
    inline std::string temp_path(const std::string &name)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path dir = fs::temp_directory_path(ec) / "ost_core_tests";
        fs::create_directories(dir, ec);
        return (dir / name).generic_string();
    }
} // namespace ost_test

#define OST_TEST_CONCAT2_(a, b) a##b
#define OST_TEST_CONCAT_(a, b) OST_TEST_CONCAT2_(a, b)

#define TEST(Suite, Name)                                                                              \
    static void ost_test_##Suite##_##Name();                                                           \
    static ::ost_test::Registrar ost_test_reg_##Suite##_##Name(#Suite, #Name, &ost_test_##Suite##_##Name, \
                                                               __FILE__, __LINE__);                    \
    static void ost_test_##Suite##_##Name()

#define CHECK(cond)                                                                      \
    do                                                                                   \
    {                                                                                    \
        if (!(cond))                                                                     \
            ::ost_test::report_failure(__FILE__, __LINE__, "CHECK(" #cond ") failed");   \
    } while (0)

#define CHECK_FALSE(cond)                                                                       \
    do                                                                                          \
    {                                                                                           \
        if (cond)                                                                               \
            ::ost_test::report_failure(__FILE__, __LINE__, "CHECK_FALSE(" #cond ") failed");    \
    } while (0)

#define REQUIRE(cond)                                                                     \
    do                                                                                    \
    {                                                                                     \
        if (!(cond))                                                                      \
        {                                                                                 \
            ::ost_test::report_failure(__FILE__, __LINE__, "REQUIRE(" #cond ") failed");  \
            throw ::ost_test::AbortTest{};                                                \
        }                                                                                 \
    } while (0)

#define OST_TEST_CMP_(macro, op, want_equal, fatal, a, b) \
    do \
    { \
        auto &&ost_a_ = (a); \
        auto &&ost_b_ = (b); \
        if (::ost_test::detail::equal(ost_a_, ost_b_) != (want_equal)) \
            ::ost_test::detail::fail_cmp(__FILE__, __LINE__, macro, #a, #b, op, ost_a_, ost_b_, fatal); \
    } while (0)

// a == b expected; fails when not equal
#define CHECK_EQ(a, b) OST_TEST_CMP_("CHECK_EQ", "!=", true, false, a, b)
#define REQUIRE_EQ(a, b) OST_TEST_CMP_("REQUIRE_EQ", "!=", true, true, a, b)
// a != b expected; fails when equal
#define CHECK_NE(a, b) OST_TEST_CMP_("CHECK_NE", "==", false, false, a, b)

#define CHECK_NEAR(a, b, tol)                                                                          \
    do                                                                                                 \
    {                                                                                                  \
        auto &&ost_a_ = (a);                                                                           \
        auto &&ost_b_ = (b);                                                                           \
        auto &&ost_t_ = (tol);                                                                         \
        ::ost_test::detail::check_near(__FILE__, __LINE__, #a, #b, #tol, ost_a_, ost_b_, ost_t_);      \
    } while (0)

#define CHECK_STR_EQ(a, b)                                                                              \
    do                                                                                                  \
    {                                                                                                   \
        auto &&ost_a_ = (a);                                                                            \
        auto &&ost_b_ = (b);                                                                            \
        ::ost_test::detail::check_str_eq(__FILE__, __LINE__, #a, #b, ::ost_test::detail::str_arg(ost_a_), \
                                         ::ost_test::detail::str_arg(ost_b_), false);                   \
    } while (0)

#define FAIL_TEST(msg)                                                              \
    do                                                                              \
    {                                                                               \
        ::ost_test::report_failure(__FILE__, __LINE__, std::string("FAIL_TEST: ") + (msg)); \
        throw ::ost_test::AbortTest{};                                              \
    } while (0)

#define SCOPED_CONTEXT(msg) \
    ::ost_test::ScopedContext OST_TEST_CONCAT_(ost_test_ctx_, __LINE__)(std::string(msg))
