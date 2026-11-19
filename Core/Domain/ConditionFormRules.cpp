// Layer: Domain. See ConditionFormRules.hpp.
#include "ConditionFormRules.hpp"

#include "../Import/TextRules.hpp"
#include "DimensionFormat.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

namespace ost_domain
{

    namespace
    {
        std::string quoted(const std::string &text)
        {
            return "\"" + text + "\"";
        }

        FormIssue invalid_value(const std::string &message)
        {
            return FormIssue{kFormInvalidValueTitle, message};
        }

        // display_to_inches(text, metric) that is a finite number (the dialog's `val is None or not isfinite(val)` refusal)
        std::optional<double> finite_inches(const std::string &stripped, bool metric)
        {
            const std::optional<double> value = display_to_inches(stripped, metric);
            if (!value || !std::isfinite(*value))
                return std::nullopt;
            return value;
        }
    }

    std::optional<FormIssue> parse_condition_name(const std::string &text, std::string &name)
    {
        const std::string stripped = ost_import::text_rules::py_strip(text);
        if (stripped.empty())
            return FormIssue{kFormValidationErrorTitle, "Condition name cannot be empty."};
        name = stripped;
        return std::nullopt;
    }

    std::optional<FormIssue> parse_elevation_z(const std::string &text, bool metric, double &z_inches)
    {
        const std::string stripped = ost_import::text_rules::py_strip(text);
        if (stripped.empty())
        {
            z_inches = 0.0;
            return std::nullopt;
        }
        const std::optional<double> value = finite_inches(stripped, metric);
        if (!value)
            return invalid_value(quoted(stripped) + " is not a valid elevation.");
        z_inches = *value;
        return std::nullopt;
    }

    std::optional<FormIssue> parse_condition_number(const std::string &text, int &ref_no)
    {
        // int(ref_no_text) with ref_no_text = text.strip(); ValueError and ref_no < 1 give the same alert. str.strip() removes the
        // C0 separators U+001C..U+001F too, which int() itself does not accept around a number: strip first, like the dialog.
        long long value = 0;
        const ost_import::text_rules::NumberParse parsed =
            ost_import::text_rules::py_parse_int(ost_import::text_rules::py_strip(text), value);
        if (parsed != ost_import::text_rules::NumberParse::Ok || value < 1 || value > INT_MAX)
            return invalid_value("Condition number must be a positive whole number.");
        ref_no = static_cast<int>(value);
        return std::nullopt;
    }

    std::optional<FormIssue> parse_dimension_value(const std::string &text, const std::string &label, bool metric, double &inches)
    {
        const std::string stripped = ost_import::text_rules::py_strip(text);
        if (stripped.empty())
        {
            inches = 0.0;
            return std::nullopt;
        }
        const std::optional<double> value = finite_inches(stripped, metric);
        if (!value)
            return invalid_value(quoted(stripped) + " is not a valid dimension for " + label + ".");
        inches = *value;
        return std::nullopt;
    }

    std::optional<FormIssue> parse_display_size(const std::string &text, double &percent)
    {
        const std::string stripped = ost_import::text_rules::py_strip(text);
        double value = kDisplaySizeDefault;
        if (!stripped.empty())
        {
            // float(ds_text), then isfinite: a ValueError and a non-finite number read the same
            if (parse_plain_number(stripped, value) != PlainNumberStatus::Ok)
                return invalid_value(quoted(stripped) + " is not a valid number for Display Size.");
        }
        if (value < kDisplaySizeMin || value > kDisplaySizeMax)
            return FormIssue{kFormValidationErrorTitle, "Display Size must be between 10% and 500%."};
        percent = value;
        return std::nullopt;
    }

    std::optional<FormIssue> parse_slope_value(const std::string &text, const std::string &label, bool absolute, double &value)
    {
        double parsed = 0.0;
        if (!text.empty())
        {
            // float(text or "0"): the dialog does not strip the field, so float() alone decides what surrounds the number (it does
            // not accept the C0 separators U+001C..U+001F that str.strip() removes) and only an EMPTY field is blank. A field of
            // white space reaches float() and is a ValueError: "Rise must be a number." (DLG-FINITE-01, decided 2026-11-19).
            switch (parse_plain_number(text, parsed))
            {
            case PlainNumberStatus::Ok:
                break;
            case PlainNumberStatus::NotANumber:
                return invalid_value(label + " must be a number.");
            case PlainNumberStatus::NotFinite:
                return invalid_value(label + " must be a finite number.");
            }
        }
        value = absolute ? std::fabs(parsed) : parsed;
        return std::nullopt;
    }

    FormIssue condition_save_failed_issue()
    {
        return FormIssue{"Save Failed", "Failed to save condition to database."};
    }

    void set_trim(Condition &condition, bool on)
    {
        condition.trim = on;
        if (on)
            condition.is_curved_segment = false;
    }

    void set_curved_segment(Condition &condition, bool on)
    {
        condition.is_curved_segment = on;
        if (on)
            condition.trim = false;
    }

    void normalize_trim_curved(Condition &condition)
    {
        if (condition.trim && condition.is_curved_segment)
            condition.is_curved_segment = false;
    }

    int pack_bgr(int red8, int green8, int blue8)
    {
        return (red8 & 0xFF) | ((green8 & 0xFF) << 8) | ((blue8 & 0xFF) << 16);
    }

    int color_fill_from_rgb(double red, double green, double blue)
    {
        // the nearest 8-bit value (Qt's qRound(c * 255)); channels outside 0..1 (extended colour spaces) and NaN are clamped
        auto to_8bit = [](double channel)
        {
            if (!(channel > 0.0))
                return 0;
            if (channel >= 1.0)
                return 255;
            return static_cast<int>(std::lround(channel * 255.0));
        };
        return pack_bgr(to_8bit(red), to_8bit(green), to_8bit(blue));
    }

}
