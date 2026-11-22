// Shared helpers of the Formatting host tests: loader of Fixtures/formatting_oracle.txt.
//
// The fixture was produced by running the REAL Windows services read-only (CPython 3.11, python -B):
// dimension_format_service.inches_to_display / mm_to_display / display_to_inches / display_to_mm,
// application/utils/quantity_display.format_quantity_number / format_quantity_with_uom (labels from
// uom_service.get_uom_label) and the dialog expressions str(v), str(abs(v)), str(int(v)), float(text) + isfinite.
// Generator: scratchpad/formatting/gen_oracle.py (seeded; every record is one Windows call).
// Line format: see the header comment of the fixture. Doubles are float.hex() (exact) or nan/inf/-inf, texts are hex UTF-8.
#pragma once

#include "TestHarness.hpp"

#include <clocale>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace fmt_test
{
    struct InchesCase
    {
        double inches;
        std::string imperial;
        std::string metric;
    };
    struct MmCase
    {
        double mm;
        std::string text;
    };
    struct OptDouble
    {
        bool ok = false;
        double value = 0.0;
    };
    struct ParseCase
    {
        std::string text;
        OptDouble imperial, metric, mm;
    };
    struct PlainCase
    {
        double value;
        std::string text, abs_text;
        bool display_size_ok;
        std::string display_size;
    };
    struct PlainParseCase
    {
        std::string text;
        int status; // 0 value, 1 ValueError, 2 not finite
        double value;
    };
    struct QuantityCase
    {
        double value;
        int uom;
        std::string number, number_zero_none, with_label;
    };

    struct Oracle
    {
        bool loaded = false;
        std::vector<InchesCase> inches;
        std::vector<double> overflow;
        std::vector<MmCase> mm;
        std::vector<ParseCase> parse;
        std::vector<PlainCase> plain;
        std::vector<PlainParseCase> plain_parse;
        std::vector<QuantityCase> quantity;
    };

    inline std::vector<std::string> split_tabs(const std::string &line)
    {
        std::vector<std::string> out;
        size_t start = 0;
        for (;;)
        {
            const size_t pos = line.find('\t', start);
            if (pos == std::string::npos)
            {
                out.push_back(line.substr(start));
                return out;
            }
            out.push_back(line.substr(start, pos - start));
            start = pos + 1;
        }
    }

    inline std::string unhex(const std::string &hex)
    {
        auto nibble = [](char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; };
        std::string out;
        for (size_t i = 0; i + 1 < hex.size(); i += 2)
            out.push_back(static_cast<char>(nibble(hex[i]) * 16 + nibble(hex[i + 1])));
        return out;
    }

    inline double parse_double(const std::string &token)
    {
        if (token == "nan")
            return std::nan("");
        if (token == "inf")
            return HUGE_VAL;
        if (token == "-inf")
            return -HUGE_VAL;
        return std::strtod(token.c_str(), nullptr);
    }

    inline OptDouble parse_opt_double(const std::string &token)
    {
        OptDouble d;
        if (token == "-")
            return d;
        d.ok = true;
        d.value = parse_double(token);
        return d;
    }

    inline const Oracle &oracle()
    {
        static const Oracle data = []
        {
            Oracle o;
            std::string content;
            if (!ost_test::read_file(ost_test::fixture_path("formatting_oracle.txt"), content))
                return o;
            size_t start = 0;
            while (start < content.size())
            {
                size_t end = content.find('\n', start);
                if (end == std::string::npos)
                    end = content.size();
                std::string line = content.substr(start, end - start);
                start = end + 1;
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.empty() || line[0] == '#')
                    continue;
                const std::vector<std::string> f = split_tabs(line);
                if (f[0] == "INCHES")
                    o.inches.push_back({parse_double(f[1]), unhex(f[2]), unhex(f[3])});
                else if (f[0] == "OVERFLOW")
                    o.overflow.push_back(parse_double(f[1]));
                else if (f[0] == "MM")
                    o.mm.push_back({parse_double(f[1]), unhex(f[2])});
                else if (f[0] == "PARSE")
                    o.parse.push_back({unhex(f[1]), parse_opt_double(f[2]), parse_opt_double(f[3]), parse_opt_double(f[4])});
                else if (f[0] == "PLAIN")
                    o.plain.push_back({parse_double(f[1]), unhex(f[2]), unhex(f[3]), f[4] != "-",
                                       f[4] == "-" ? std::string() : unhex(f[4])});
                else if (f[0] == "PLAINPARSE")
                    o.plain_parse.push_back({unhex(f[1]), std::atoi(f[2].c_str()), parse_double(f[3])});
                else if (f[0] == "QTY")
                    o.quantity.push_back({parse_double(f[1]), std::atoi(f[2].c_str()), unhex(f[3]), unhex(f[4]), unhex(f[5])});
            }
            o.loaded = true;
            return o;
        }();
        return data;
    }

    // printable form of a text with control / non-ASCII bytes escaped, for SCOPED_CONTEXT
    inline std::string show(const std::string &s)
    {
        std::string out;
        for (unsigned char c : s)
        {
            if (c >= 0x20 && c < 0x7f)
                out.push_back(static_cast<char>(c));
            else
            {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\x%02x", c);
                out += buf;
            }
        }
        if (out.size() > 60)
            out = out.substr(0, 60) + "...(" + std::to_string(s.size()) + " bytes)";
        return out;
    }

    // Switches the C locale to a decimal-comma locale for the lifetime of the object, when the machine has one.
    class ScopedDecimalCommaLocale
    {
    public:
        ScopedDecimalCommaLocale()
        {
            const char *previous = std::setlocale(LC_ALL, nullptr);
            if (previous)
                _previous = previous;
            for (const char *name : {"de_DE.UTF-8", "de_DE.utf8", "de_DE", "German_Germany.1252", "de-DE", "fr_FR.UTF-8",
                                     "fr_FR", "French_France.1252", "fr-FR"})
            {
                if (std::setlocale(LC_ALL, name))
                {
                    const std::lconv *conv = std::localeconv();
                    if (conv && conv->decimal_point && std::string(conv->decimal_point) != ".")
                    {
                        _active = true;
                        return;
                    }
                }
            }
            std::setlocale(LC_ALL, _previous.c_str());
        }
        ~ScopedDecimalCommaLocale()
        {
            if (_active)
                std::setlocale(LC_ALL, _previous.c_str());
        }
        bool active() const { return _active; }

    private:
        std::string _previous = "C";
        bool _active = false;
    };

    // exact (bitwise-equal for finite values; -0.0 == 0.0 as in Python)
    inline bool same_double(double a, double b)
    {
        return a == b;
    }
}
