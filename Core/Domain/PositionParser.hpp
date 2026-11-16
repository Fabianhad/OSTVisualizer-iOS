// Layer: Domain (pure C++, no UIKit/Foundation/SQLite)
// Windows source of truth:
//   ost_visualizer/domain/utils/position.py                   parse_position
//   ost_visualizer/domain/utils/text_cleanup.py                strip_xml_newline_entities
//   ost_visualizer/infrastructure/mdb/components/serialization.py   encode_position (preserve every index)
//   ost_visualizer/domain/services/elevation.py                parse_elevation
//   ost_visualizer/domain/services/elevation_text.py           elevation validity patterns
//   ost_visualizer/infrastructure/parsers/position_parser.py   extract_z_value_from_name and its helpers
//
// The Windows code relies on Python semantics (str.split()/strip(), the re module on Unicode str, float(),
// repr(float)). This port reproduces them where they are observable: Unicode whitespace (str.isspace), Unicode
// decimal digits and word characters (tables generated from Python 3.11.4, Unicode 14.0.0), float() grammar
// (underscores, inf/nan, exponents), shortest round-trip repr(). Text is UTF-8 (std::string).
#pragma once

#include <string>
#include <vector>

namespace ost_domain
{
    // ---------------------------------------------------------------------------------------------------------
    // Position text
    // ---------------------------------------------------------------------------------------------------------

    // domain/utils/position.py::parse_position. Strips the XML newline entities (&#xa; &#x0A; &#10;, any case),
    // splits on ';', drops blank tokens and reads each token like Python float(token). ANY token that is not
    // entirely a float (e.g. "1.5abc", "1,5", "0x10", "1 2") gives an empty position. "1e400" -> inf.
    std::vector<double> parse_position(const std::string &position_str);

    // serialization.py::encode_position(position, preserve_indices=frozenset(range(len(position)))): the form of
    // every takeoff write (takeoff_operations.py save_takeoff_positions / insert): ';'.join(str(float(v))) + "\n".
    std::string encode_position(const std::vector<double> &position);

    // Python repr(float(value)): shortest round-trip digits, "1.0", "1e-05", "1e+16", "inf", "nan", "-0.0".
    std::string python_float_repr(double value);

    // Python float(text) for a UTF-8 string: surrounding whitespace allowed, Unicode decimal digits, single
    // underscores between digits, "inf"/"infinity"/"nan" in any case with an optional sign, exponent forms,
    // overflow gives +-inf and underflow 0.0. Returns false where Python raises ValueError.
    bool parse_python_float(const std::string &text, double &out);

    // ---------------------------------------------------------------------------------------------------------
    // Condition-name elevations ("Wall @T 12' 6 1/2\"")
    // ---------------------------------------------------------------------------------------------------------

    // elevation.py::ElevationParts. type: 0 = top (@T), 1 = bottom (@B or the legacy bare marker).
    struct ElevationParts
    {
        std::string base_name;
        int type = 0;
        std::string value;
    };

    // elevation.py::parse_elevation. The marker must be preceded by a space; the LAST " @T"/" @B" wins; without
    // one, the last " @ " (else " @") is a legacy bare marker (bottom). T/B are case-sensitive.
    ElevationParts parse_elevation(const std::string &name);

    // elevation_text.py::is_elevation_text: the elevation grammar prefix patterns (feet+inches, feet only,
    // inches only, metres+centimetres, metres, centimetres), case-insensitive, matched at the start of the text.
    bool is_elevation_text(const std::string &text);

    // position_parser.py::_parse_fractional_inches_text ("6", "1/2", "3 1/2", "-3 1/2"). Returns false where
    // the Windows function raises ValueError (zero denominator "zero denominator in fraction 1/0", unreadable
    // number); *error then holds the Windows message.
    bool parse_fractional_inches_text(const std::string &text, double &out, std::string *error = nullptr);

    // position_parser.py::_parse_imperial_elevation (inches; unanchored search, 0.0 when nothing matches).
    // False where Windows raises (ValueError from the fraction, or OverflowError for feet beyond float range).
    bool parse_imperial_elevation(const std::string &text, double &out, std::string *error = nullptr);

    // position_parser.py::_parse_metric_elevation (centimetres; unanchored search on the lower-cased text with
    // ',' read as '.', 0.0 when nothing matches).
    double parse_metric_elevation(const std::string &text);

    // position_parser.py::cm_to_inches
    double cm_to_inches(double cm);

    // position_parser.py::extract_z_value_from_name. On success z_inches/is_top hold the Windows result
    // ((0.0, false) when the name has no valid elevation). Returns false where Windows raises ValueError (zero
    // denominator, e.g. "Wall @B 1/0\""): the Windows MDB reader then logs a warning and stores (0.0, false).
    bool extract_z_value_from_name(const std::string &name, double &z_inches, bool &is_top,
                                   std::string *error = nullptr);
}
