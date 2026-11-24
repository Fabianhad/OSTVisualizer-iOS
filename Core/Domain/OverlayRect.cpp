// Layer: Domain. See OverlayRect.hpp.
// Windows source of truth: ost_visualizer/infrastructure/mdb/components/overlay_rect.py:8-48.
#include "OverlayRect.hpp"

#include "../Import/TextRules.hpp"
#include "NumberText.hpp"
#include "PositionParser.hpp"

#include <cmath>

namespace ost_domain
{
    namespace
    {
        bool refuse(std::string *error, const char *text)
        {
            if (error)
                *error = text;
            return false;
        }

        // _validated_overlay_rect (overlay_rect.py:17-30) once the four values are numbers: finite first, then a negative width or height.
        bool validate_values(const double (&values)[4], std::string *error)
        {
            for (const double value : values)
                if (!std::isfinite(value))
                    return refuse(error, "OverlayRect values must be finite");
            if (values[2] < 0.0 || values[3] < 0.0)
                return refuse(error, "OverlayRect width and height cannot be negative");
            return true;
        }
    }

    bool parse_rect_storage(const std::string &stored, OverlayRect &out, std::string *error)
    {
        const std::string text = ost_import::text_rules::py_strip(stored);
        if (text.empty() || text == kEmptyOverlayRectMarker)
        {
            out = OverlayRect{};
            return true;
        }
        // text.split(","): empty parts are kept ("1,,2" has three parts).
        std::vector<std::string> parts;
        size_t start = 0;
        for (;;)
        {
            const size_t comma = text.find(',', start);
            if (comma == std::string::npos)
            {
                parts.push_back(text.substr(start));
                break;
            }
            parts.push_back(text.substr(start, comma - start));
            start = comma + 1;
        }
        if (parts.size() != 4)
            return refuse(error, "OverlayRect must contain exactly four values");
        double values[4] = {0.0, 0.0, 0.0, 0.0};
        for (size_t i = 0; i < 4; ++i)
        {
            if (!parse_python_float(ost_import::text_rules::py_strip(parts[i]), values[i]))
                return refuse(error, "OverlayRect values must be numeric");
        }
        if (!validate_values(values, error))
            return false;
        out.x = values[0];
        out.y = values[1];
        out.width = values[2];
        out.height = values[3];
        return true;
    }

    bool serialize_rect_storage(const OverlayRect &rect, std::string &out, std::string *error)
    {
        const double values[4] = {rect.x, rect.y, rect.width, rect.height};
        if (!validate_values(values, error))
            return false;
        std::string text;
        for (size_t i = 0; i < 4; ++i)
        {
            if (i > 0)
                text += ',';
            text += format_fixed(values[i], 6);
        }
        out = text;
        return true;
    }
}
