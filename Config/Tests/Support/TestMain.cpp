// TestMain.cpp - runner for TestHarness.hpp.
//   <exe> [--filter <substr>]... [--list] [--help]
// Output: one '[PASS] Suite.Name' / '[FAIL] Suite.Name' line per test (FAIL is followed by one indented
// 'file:line: message' line per failed check), then exactly one final line
//   TESTS: <total> PASSED: <p> FAILED: <f>
// Exit code: 0 all passed, 1 some failed, 2 usage error, 3 crash (signal).
#include "TestHarness.hpp"

#include <algorithm>
#include <csignal>
#include <exception>
#include <map>
#include <string>
#include <vector>

#include <cstdlib>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <crtdbg.h>
#include <windows.h>
#endif

namespace ost_test
{
    namespace
    {
        struct RunState
        {
            const TestCase *current = nullptr;
            std::vector<std::string> failures; // failure lines of the running test
            std::vector<std::string> context;
            std::vector<FailureCapture *> captures;
            int selected = 0;
            int passed = 0;
        };

        RunState &state()
        {
            static RunState s;
            return s;
        }

        std::string full_name(const TestCase &t) { return std::string(t.suite) + "." + t.name; }

        std::string context_suffix()
        {
            const auto &ctx = state().context;
            if (ctx.empty())
                return "";
            std::string s = " [";
            for (std::size_t i = 0; i < ctx.size(); ++i)
            {
                if (i)
                    s += " / ";
                s += ctx[i];
            }
            return s + "]";
        }

        void print_final_line(int total, int passed, int failed)
        {
            std::printf("TESTS: %d PASSED: %d FAILED: %d\n", total, passed, failed);
            std::fflush(stdout);
        }

        void crash_handler(int sig)
        {
            RunState &s = state();
            std::printf("[CRASH] %s (signal %d)\n", s.current ? full_name(*s.current).c_str() : "<outside test>", sig);
            // Everything that did not pass (the crashed test and the tests that never ran) counts as failed.
            print_final_line(s.selected, s.passed, s.selected - s.passed);
            std::fflush(stderr);
            std::_Exit(3);
        }

        void install_crash_handling()
        {
#ifdef _WIN32
            // No modal dialogs on crash / assert / abort: agents and CI run unattended.
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
            _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
            std::signal(SIGSEGV, crash_handler);
#ifdef SIGBUS
            std::signal(SIGBUS, crash_handler); // macOS reports some bad accesses as SIGBUS
#endif
            std::signal(SIGABRT, crash_handler);
            std::signal(SIGFPE, crash_handler);
            std::signal(SIGILL, crash_handler);
        }

        void print_usage(const char *exe)
        {
            std::printf("usage: %s [--filter <substring>]... [--list] [--help]\n", exe);
        }
    } // namespace

    void report_failure(const char *file, int line, const std::string &message)
    {
        RunState &s = state();
        std::string text = std::string(file) + ":" + std::to_string(line) + ": " + message + context_suffix();
        if (!s.captures.empty())
            s.captures.back()->messages.push_back(text);
        else
            s.failures.push_back(text);
    }

    void push_context(const std::string &text) { state().context.push_back(text); }
    void pop_context()
    {
        auto &c = state().context;
        if (!c.empty())
            c.pop_back();
    }

    FailureCapture::FailureCapture() { state().captures.push_back(this); }
    FailureCapture::~FailureCapture()
    {
        auto &c = state().captures;
        if (!c.empty() && c.back() == this)
            c.pop_back();
    }
} // namespace ost_test

int main(int argc, char **argv)
{
    using namespace ost_test;

    std::vector<std::string> filters;
    bool list_only = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--list")
            list_only = true;
        else if (a == "--filter" && i + 1 < argc)
            filters.push_back(argv[++i]);
        else if (a.rfind("--filter=", 0) == 0)
            filters.push_back(a.substr(9));
        else if (a == "--help" || a == "-h")
        {
            print_usage(argv[0]);
            return 0;
        }
        else
        {
            std::printf("unknown or incomplete argument: %s\n", a.c_str());
            print_usage(argv[0]);
            return 2;
        }
    }

    // Deterministic order independent of static-initialization order and platform: by (file, line).
    std::vector<TestCase> tests = registry();
    std::stable_sort(tests.begin(), tests.end(), [](const TestCase &x, const TestCase &y) {
        int c = std::strcmp(x.file, y.file);
        return c != 0 ? c < 0 : x.line < y.line;
    });

    std::vector<const TestCase *> selected;
    for (const TestCase &t : tests)
    {
        if (!filters.empty())
        {
            const std::string name = full_name(t);
            bool match = false;
            for (const std::string &f : filters)
                if (name.find(f) != std::string::npos)
                    match = true;
            if (!match)
                continue;
        }
        selected.push_back(&t);
    }

    if (list_only)
    {
        for (const TestCase *t : selected)
            std::printf("%s\n", full_name(*t).c_str());
        std::fflush(stdout);
        return 0;
    }

    RunState &s = state();
    s.selected = static_cast<int>(selected.size());
    install_crash_handling();

    std::map<std::string, const TestCase *> seen;
    int failed = 0;
    for (const TestCase *t : selected)
    {
        s.current = t;
        s.failures.clear();
        s.context.clear();
        s.captures.clear();

        const std::string name = full_name(*t);
        auto dup = seen.find(name);
        if (dup != seen.end())
        {
            s.failures.push_back(std::string(t->file) + ":" + std::to_string(t->line) +
                                 ": duplicate test name (first defined at " + dup->second->file + ":" +
                                 std::to_string(dup->second->line) + ")");
        }
        else
        {
            seen[name] = t;
            try
            {
                t->fn();
            }
            catch (const AbortTest &)
            {
                // failure already recorded by REQUIRE / FAIL_TEST
            }
            catch (const std::exception &e)
            {
                s.failures.push_back(std::string(t->file) + ":" + std::to_string(t->line) +
                                     ": unexpected exception: " + e.what() + context_suffix());
            }
            catch (...)
            {
                s.failures.push_back(std::string(t->file) + ":" + std::to_string(t->line) +
                                     ": unexpected non-std exception" + context_suffix());
            }
        }

        if (s.failures.empty())
        {
            ++s.passed;
            std::printf("[PASS] %s\n", name.c_str());
        }
        else
        {
            ++failed;
            std::printf("[FAIL] %s\n", name.c_str());
            for (const std::string &f : s.failures)
                std::printf("    %s\n", f.c_str());
        }
        std::fflush(stdout);
    }
    s.current = nullptr;

    print_final_line(s.selected, s.passed, failed);
    return failed == 0 ? 0 : 1;
}
