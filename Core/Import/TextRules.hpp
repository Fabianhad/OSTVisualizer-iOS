// Layer: Import (pure C++)
// Text rules of the Windows importer that Python provides for free: str.strip(), str.lower(), str.casefold(),
// int(text) and float(text). The master-data identity keys (CdnTypes.Name = strip().lower(), employee keys =
// strip().casefold()), Settings.NextBidNo, UID floors and typed column conversion must give the same result as
// Windows for every input, so they are implemented here instead of with tolower()/stoi()/stod().
//
// All strings are UTF-8. A byte sequence that is not valid UTF-8 is passed through unchanged (it is neither
// whitespace nor a cased letter).
#pragma once
#include <string>

namespace ost_import
{
    namespace text_rules
    {
        // str.strip() without arguments: removes leading/trailing Unicode whitespace (str.isspace()).
        std::string py_strip(const std::string &text);

        // str.lower(), including the Final_Sigma rule and the full (multi code point) lower-casing of U+0130.
        std::string py_lower(const std::string &text);

        // str.casefold() (full case folding, e.g. U+00DF -> "ss").
        std::string py_casefold(const std::string &text);

        enum class NumberParse
        {
            Ok,
            Invalid,  // Python raises ValueError
            Overflow, // a valid integer that does not fit in 64 bits (SQLite cannot store it)
        };

        // int(text) for a str: surrounding whitespace, optional sign, ASCII digits with single underscores between
        // digits. (Non-ASCII Unicode decimal digits, which Python also accepts, are not.)
        NumberParse py_parse_int(const std::string &text, long long &out);

        // float(text) for a str that contains a '.': surrounding whitespace, optional sign, digits with an optional
        // fraction and exponent, single underscores between digits. ("inf"/"nan" words cannot contain a '.'.)
        bool py_parse_float(const std::string &text, double &out);
    }
}
