#pragma once
// Log hook of the platform-independent Core. The Windows app logs through the Python logging module; Core has no
// logging framework, so a Core function that must report something Windows logs (for example MeshFactory when the
// drop-stub union fails) calls ost_log::warning(...). The record goes to the installed sink; without one it is written
// to stderr (Xcode console on iOS). The Obj-C layer can install a sink that forwards to NSLog, the host tests install
// one that collects the records.
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <utility>

namespace ost_log
{

    enum class Level
    {
        Debug,
        Info,
        Warning,
        Error
    };

    // message = the formatted record text (Python record.getMessage()); exception_text = what() of the exception the
    // record is about, empty when there is none (Python exc_info).
    using Sink = std::function<void(Level level, const std::string &message, const std::string &exception_text)>;

    inline const char *level_name(Level level)
    {
        switch (level)
        {
        case Level::Debug:
            return "DEBUG";
        case Level::Info:
            return "INFO";
        case Level::Warning:
            return "WARNING";
        case Level::Error:
            return "ERROR";
        }
        return "";
    }

    namespace detail
    {
        struct State
        {
            std::mutex mutex;
            Sink sink; // empty = default (stderr)
        };

        inline State &state()
        {
            static State s;
            return s;
        }
    } // namespace detail

    // Installs the sink; an empty function restores the default (stderr).
    inline void set_sink(Sink sink)
    {
        auto &s = detail::state();
        std::lock_guard<std::mutex> lock(s.mutex);
        s.sink = std::move(sink);
    }

    inline void log(Level level, const std::string &message, const std::string &exception_text = std::string())
    {
        Sink sink;
        {
            auto &s = detail::state();
            std::lock_guard<std::mutex> lock(s.mutex);
            sink = s.sink;
        }
        if (sink)
        {
            sink(level, message, exception_text);
            return;
        }
        if (exception_text.empty())
            std::fprintf(stderr, "%s: %s\n", level_name(level), message.c_str());
        else
            std::fprintf(stderr, "%s: %s (%s)\n", level_name(level), message.c_str(), exception_text.c_str());
    }

    inline void warning(const std::string &message, const std::string &exception_text = std::string())
    {
        log(Level::Warning, message, exception_text);
    }

} // namespace ost_log
