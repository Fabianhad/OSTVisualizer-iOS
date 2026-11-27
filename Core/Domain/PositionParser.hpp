// Layer: Domain (pure C++, no UIKit/Foundation/SQLite)
// Windows source of truth:
//   ost_visualizer/domain/utils/position.py                   parse_position
//   ost_visualizer/domain/utils/text_cleanup.py                strip_xml_newline_entities
//   ost_visualizer/infrastructure/mdb/components/serialization.py   encode_position (preserve every index)
//   ost_visualizer/domain/services/elevation.py                parse_elevation
//   ost_visualizer/domain/services/elevation_text.py           elevation validity patterns and the helpers that read them
//                                                              (since b9902e6: _fraction_value, _parse_fractional_inches_text,
//                                                              _parse_imperial_elevation, _parse_metric_elevation, cm_to_inches,
//                                                              elevation_text_inches)
//   ost_visualizer/infrastructure/parsers/position_parser.py   extract_z_value_from_name (:21-31); it only imports those helpers
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

    // The Python text primitives behind the above, for other Domain code that ports a Windows regex or strip()
    // (DimensionFormat.cpp): UTF-8 -> code points (an invalid byte becomes U+FFFD), str.isspace() (== the regex \s)
    // and the value 0..9 of a Unicode decimal digit (str.isdecimal() == the regex \d), or -1.
    std::u32string py_decode_utf8(const std::string &text);
    bool py_is_space(char32_t c);
    int py_decimal_value(char32_t c);

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

    // elevation.py::reassemble_elevation (:62-67; plan 5.1 / 4.13, replaces the Objective-C copy reassembleElevation of EditConditionHelpers.mm,
    // which trims only U+0020): trimmed = value.strip() (Python str.strip: every Unicode white space, ost_import::text_rules::py_strip); a blank
    // value gives `base_name` unchanged; else base_name + " @T " + trimmed for elevation_type 0 and base_name + " @B " + trimmed for every other type
    // (1, 5, -1 ...).
    std::string reassemble_elevation(const std::string &base_name, int elevation_type, const std::string &value);

    // elevation_text.py::is_elevation_text: the elevation grammar prefix patterns (feet+inches, feet only,
    // inches only, metres+centimetres, metres, centimetres), case-insensitive, matched at the start of the text. U+2033 and U+201D
    // are read as the inch mark '"' first (normalize_inch_marks, Windows 0a24294), so "5'6<U+2033>" and "6<U+201D>" are elevation texts.
    bool is_elevation_text(const std::string &text);

    // elevation_text.py::IMPERIAL_FEET_INCHES.match(normalize_inch_marks(text)): the text STARTS with a complete feet-and-inches value
    // (5'6", 5' 6<U+2033>, 10'6 1/2"). The Edit Condition dialog skips its closing-inch-mark rule for such a text
    // (elevation_text_needs_closing_inch_mark, edit_condition_dialog.py:1628-1630). No strip: the caller passes the stripped text.
    bool elevation_text_starts_with_feet_and_inches(const std::string &text);

    // elevation_text.py::_parse_fractional_inches_text (:54-76; "6", "1/2", "3 1/2", "-3 1/2"). Returns false where
    // the Windows function raises ValueError (zero denominator "zero denominator in fraction 1/0", unreadable
    // number); *error then holds the Windows message.
    bool parse_fractional_inches_text(const std::string &text, double &out, std::string *error = nullptr);

    // elevation_text.py::_parse_imperial_elevation (:83-106; inches; unanchored search, 0.0 when nothing matches).
    // False where Windows raises (ValueError from the fraction, or OverflowError for feet beyond float range).
    bool parse_imperial_elevation(const std::string &text, double &out, std::string *error = nullptr);

    // elevation_text.py::_parse_metric_elevation (:109-128; centimetres; unanchored search on the lower-cased text with
    // ',' read as '.', 0.0 when nothing matches).
    double parse_metric_elevation(const std::string &text);

    // elevation_text.py::cm_to_inches (:40-41)
    double cm_to_inches(double cm);

    // What elevation_text.py::elevation_text_inches did with a text: a number (Value), None (NotAnElevation), or an exception
    // (Raises: ValueError for a zero denominator, a feet run beyond Python's 4300-digit int() limit, or, since Windows 0a24294, a
    // result beyond the float range: the OverflowError of a feet run and any non-finite inches-only / metre / centimetre figure
    // become ValueError("elevation out of range: <first 40 characters>")).
    enum class ElevationTextStatus
    {
        Value,
        NotAnElevation,
        Raises
    };

    // elevation_text.py::elevation_text_inches (b9902e6, 0a24294): THE function that reads an elevation text, in inches.
    // normalize_inch_marks(text).strip() (U+2033 and U+201D become '"'), then the six-pattern gate (is_elevation_text, anchored
    // prefix) or None; a text with an imperial mark and no letter m is read by the imperial reader, one with a letter m and no mark
    // by the metric reader (centimetres / 2.54), one with both is None. `inches` is written only for Value, and is always finite (a
    // result beyond the float range is Raises); for Raises `error` (when given) holds the Windows message. The loader (extract_z_value_from_name,
    // position_parser.py:21-31) and the Edit Condition dialog (parse_elevation_z, edit_condition_dialog.py:1437-1461) both call it.
    ElevationTextStatus elevation_text_inches(const std::string &text, double &inches, std::string *error = nullptr);

    // position_parser.py::extract_z_value_from_name = parse_elevation + elevation_text_inches of the value (position_parser.py:21-31).
    // On success z_inches/is_top hold the Windows result ((0.0, false) when the name has no valid elevation). Returns false where
    // Windows raises (a zero denominator, e.g. "Wall @B 1/0\"", or a number beyond the float range since 0a24294): the Windows MDB reader
    // then logs a warning and stores (0.0, false).
    bool extract_z_value_from_name(const std::string &name, double &z_inches, bool &is_top,
                                   std::string *error = nullptr);

    // The elevation a condition carries in memory, DERIVED FROM ITS NAME: the one derivation shared by the loader (ProjectStore load,
    // bid_data_reader.py:896-906) and by a Save of the Edit Condition dialog (condition_to_save, COND-ELEV-01), so that the condition
    // a Save leaves in memory has the is_top and z_value a reload gives. = extract_z_value_from_name, and (0.0, false) for a name
    // Windows cannot read (the ValueError the Windows reader catches, logs and replaces by (0.0, False)). The dialog's own
    // validation of the elevation text (parse_elevation_z) calls the same elevation_text_inches, so a text it accepts has, in the
    // form, the number this derivation gives for the saved name.
    void derive_elevation_from_name(const std::string &name, double &z_inches, bool &is_top);
}
