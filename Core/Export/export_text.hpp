#pragma once
// Text helpers shared by the OBJ / DXF / Three.js writers. Pure C++, no Foundation.
//
// Each function reproduces a Python / Windows-app routine, so the text the iOS writers produce for the same data is
// the text the Windows exporters produce:
//   sanitize_material_name  BaseExporter._sanitize_material_name (exporters/base_exporter.py)
//   python_float_repr       float.__repr__, the number format of json.dumps (the Windows scene JSON)
//   json_string             json.dumps(str) with '<' written as the JSON escape u003c (threejs_renderer._generate_html)
//   html_escape             html.escape(s, quote=False) (the <title> of the exported page)
#include <string>

namespace ost_geometry
{
    namespace export_text
    {

        // Material / OBJ group / DXF layer name: ' ' '/' '\\' ':' ';' '*' '?' '<' '>' '|' '.' '(' ')' '[' ']' '{' '}' become
        // '_', '"' '\'' ',' are removed, every other character (non-ASCII too) is kept. A name that starts with a digit
        // (Python str.isdigit(): also superscripts and other scripts' digits) gets the prefix "Mat_"; an empty result is
        // "Unknown". `name` is UTF-8. One deliberate difference: the ASCII control characters (U+0000..U+001F, U+007F),
        // which Windows keeps, are dropped (they would split the OBJ / MTL / DXF line of the name).
        std::string sanitize_material_name(const std::string &name);

        // repr(float): the shortest digits that read back as the same double; fixed notation for decimal exponents in
        // [-4, 16), otherwise 'd.ddde+XX'; whole numbers keep ".0" ("1.0", "-0.0"); NaN / infinities as json.dumps writes
        // them ("NaN", "Infinity", "-Infinity").
        std::string python_float_repr(double value);

        // A UTF-8 string as a JSON string literal the way json.dumps(ensure_ascii=True) writes it, with '<' as the escape
        // backslash-u003c so the text can sit inside a script block: quote, backslash and the control characters escaped,
        // everything outside printable ASCII as backslash-u plus four hex digits (surrogate pairs above U+FFFF). Bytes
        // that are not valid UTF-8 become the escape of U+FFFD.
        std::string json_string(const std::string &utf8);

        // '&' '<' '>' to &amp; &lt; &gt; (quotes stay).
        std::string html_escape(const std::string &text);

        // A colour component that came from a colour byte as float(byte) / 255.0f (Condition::rgb_from_color_fill) as the
        // double the Windows exporters have: byte / 255.0 (ColorService.hex_to_rgb). A float that is not such a byte
        // fraction is widened unchanged.
        double colour_channel(float value);

        // A float typed as a decimal ("0.35f") as the double of that decimal: the shortest decimal that reads back as the
        // same float, parsed as a double (0.35f -> 0.35, not 0.3499999940395355).
        double shortest_double_of(float value);

    } // namespace export_text
} // namespace ost_geometry
