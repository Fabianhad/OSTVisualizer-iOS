// Self-test of the test harness itself (TestHarness.hpp / TestMain.cpp). Contains NO iOS behavior tests.
//
// Failing checks must be provable without failing the run, so they are executed under a
// ost_test::FailureCapture inside failures_of(); the assertions about what was captured are made
// OUTSIDE the capture (a CHECK inside a capture would be swallowed by it).
#include "TestHarness.hpp"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    enum class Color
    {
        Red = 1,
        Green = 2
    };
    struct Opaque
    {
        int v;
        bool operator==(const Opaque &o) const { return v == o.v; }
    };

    template <class F>
    std::vector<std::string> failures_of(F body)
    {
        ost_test::FailureCapture cap;
        body();
        return cap.messages;
    }

    bool contains(const std::string &hay, const char *needle) { return hay.find(needle) != std::string::npos; }
} // namespace

TEST(HarnessSelfTest, PassingChecksDoNotFail)
{
    auto msgs = failures_of([] {
        CHECK(true);
        CHECK(1 + 1 == 2);
        CHECK_FALSE(false);
        CHECK_EQ(3, 3);
        CHECK_EQ(std::string("abc"), "abc");
        CHECK_EQ(std::vector<int>({1, 2, 3}), std::vector<int>({1, 2, 3}));
        CHECK_EQ(Color::Red, Color::Red);
        CHECK_EQ(Opaque{4}, Opaque{4});
        CHECK_EQ(std::optional<int>(2), std::optional<int>(2));
        CHECK_NE(1, 2);
        CHECK_NEAR(1.0, 1.0, 0.0);
        CHECK_NEAR(1.0f, 1.05f, 0.1);
        CHECK_STR_EQ("x", std::string("x"));
        REQUIRE(true);
        REQUIRE_EQ(5u, 5u);
    });
    CHECK_EQ(msgs.size(), 0u);
    CHECK(msgs.empty());
}

TEST(HarnessSelfTest, EveryFailingMacroIsDetected)
{
    auto msgs = failures_of([] {
        CHECK(false);
        CHECK_FALSE(true);
        CHECK_EQ(1, 2);
        CHECK_NE(7, 7);
        CHECK_NEAR(1.0, 2.0, 0.5);
        CHECK_STR_EQ("abc", "abd");
    });
    CHECK_EQ(msgs.size(), 6u);
}

TEST(HarnessSelfTest, FailureMessagesContainBothValuesAndLocation)
{
    auto msgs = failures_of([] {
        CHECK_EQ(1, 2);
        CHECK_NEAR(1.0, 1.5, 0.25);
        CHECK_STR_EQ("abc", "abd");
        CHECK_EQ(std::vector<int>({1, 2}), std::vector<int>({1, 3}));
        CHECK_EQ(Color::Red, Color::Green);
    });
    REQUIRE_EQ(msgs.size(), 5u);
    CHECK(contains(msgs[0], "1 != 2"));
    CHECK(contains(msgs[0], "test_harness_selftest.cpp:"));
    CHECK(contains(msgs[1], "1 vs 1.5"));
    CHECK(contains(msgs[1], "tol 0.25"));
    CHECK(contains(msgs[2], "\"abc\" vs \"abd\""));
    CHECK(contains(msgs[2], "first difference at index 2"));
    CHECK(contains(msgs[3], "[1, 2] != [1, 3]"));
    CHECK(contains(msgs[4], "1 != 2")); // enum class printed as its underlying value
}

TEST(HarnessSelfTest, RequireAbortsTheTest)
{
    bool aborted = false;
    bool reached_after = false;
    auto msgs = failures_of([&] {
        try
        {
            REQUIRE(1 == 2);
            reached_after = true;
        }
        catch (const ost_test::AbortTest &)
        {
            aborted = true;
        }
    });
    CHECK(aborted);
    CHECK(!reached_after);
    CHECK_EQ(msgs.size(), 1u);
}

TEST(HarnessSelfTest, CheckNearTolerance)
{
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    auto inside = failures_of([&] {
        CHECK_NEAR(10.0, 10.0 + 1e-9, 1e-8); // inside
        CHECK_NEAR(10.0, 10.0 - 1e-9, 1e-8); // inside, other side
        CHECK_NEAR(1.0, 1.25, 0.25);         // boundary is inclusive
        CHECK_NEAR(-3, -3, 0);               // integers
        CHECK_NEAR(inf, inf, 0.0);           // equal infinities
    });
    CHECK_EQ(inside.size(), 0u);

    auto outside = failures_of([&] {
        CHECK_NEAR(10.0, 10.0 + 1e-6, 1e-8); // outside
        CHECK_NEAR(0.0, nan, 1.0);           // NaN never near
        CHECK_NEAR(1.0, 1.26, 0.25);         // just outside the boundary
    });
    CHECK_EQ(outside.size(), 3u);
}

TEST(HarnessSelfTest, StringCompare)
{
    const char *p = "hello";
    std::string s = "hello";
    std::string_view sv = "hello";
    auto same = failures_of([&] {
        CHECK_STR_EQ(p, s);
        CHECK_STR_EQ(s, sv);
        CHECK_STR_EQ("", "");
        CHECK_STR_EQ(static_cast<const char *>(nullptr), static_cast<const char *>(nullptr));
    });
    CHECK_EQ(same.size(), 0u);

    auto diff = failures_of([&] {
        CHECK_STR_EQ("hello", "hellO");                        // case sensitive
        CHECK_STR_EQ("abc", "abcd");                           // prefix
        CHECK_STR_EQ("", static_cast<const char *>(nullptr));  // null != empty
        CHECK_STR_EQ(std::string("a\nb"), "a\r\nb");           // line endings are significant
    });
    REQUIRE_EQ(diff.size(), 4u);
    CHECK(contains(diff[0], "first difference at index 4"));
    CHECK(contains(diff[1], "first difference at index 3"));
    CHECK(contains(diff[2], "(null)"));
    CHECK(contains(diff[3], "\\r\\n")); // escapes are printed
}

TEST(HarnessSelfTest, LongStringsAreExcerpted)
{
    std::string a(5000, 'x'), b = a;
    b[4000] = 'y';
    auto msgs = failures_of([&] { CHECK_STR_EQ(a, b); });
    REQUIRE_EQ(msgs.size(), 1u);
    CHECK(msgs[0].size() < 1000);
    CHECK(contains(msgs[0], "first difference at index 4000"));
    CHECK(contains(msgs[0], "length 5000"));
}

TEST(HarnessSelfTest, MixedSignednessComparesByValue)
{
    auto msgs = failures_of([] {
        CHECK_EQ(-1, 4294967295u); // equal under naive C++ promotion; must be reported as different
        CHECK_EQ(static_cast<std::int64_t>(5), static_cast<std::uint32_t>(5));
        CHECK_EQ(5u, 5);
        CHECK_NE(-1, std::numeric_limits<std::size_t>::max());
    });
    CHECK_EQ(msgs.size(), 1u);
}

TEST(HarnessSelfTest, ScopedContextIsAppendedToFailures)
{
    auto msgs = failures_of([] {
        for (int i = 0; i < 2; ++i)
        {
            SCOPED_CONTEXT("case " + std::to_string(i));
            CHECK_EQ(i, 5);
        }
        CHECK_EQ(0, 1); // outside the loop: no context
    });
    REQUIRE_EQ(msgs.size(), 3u);
    CHECK(contains(msgs[0], "[case 0]"));
    CHECK(contains(msgs[1], "[case 1]"));
    CHECK(!contains(msgs[2], "[case"));
}

TEST(HarnessSelfTest, FixtureAndTempPathsAreUsable)
{
#ifdef OST_FIXTURE_DIR
    std::string fp = ost_test::fixture_path("x.bin");
    CHECK(fp.size() > std::string("/x.bin").size());
    CHECK(fp.find('\\') == std::string::npos); // forward slashes only
    CHECK(contains(fp, "/Tests/Fixtures/x.bin"));
#else
    FAIL_TEST("OST_FIXTURE_DIR must be defined for every test area");
#endif
    std::string tp = ost_test::temp_path("selftest.tmp");
    {
        std::FILE *f = std::fopen(tp.c_str(), "wb");
        REQUIRE(f != nullptr);
        std::fputs("abc", f);
        std::fclose(f);
    }
    std::string back;
    CHECK(ost_test::read_file(tp, back));
    CHECK_STR_EQ(back, "abc");
    std::remove(tp.c_str());
    std::string missing;
    CHECK(!ost_test::read_file(tp, missing));
}
