// Layer: Domain. See ConditionFormRules.hpp.
#include "ConditionFormRules.hpp"

#include "../Import/TextRules.hpp"
#include "DimensionFormat.hpp"
#include "PositionParser.hpp"

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
        // `try: parsed = elevation_text_inches(elev_value) except ValueError: parsed = None`, then `parsed is None or not
        // isfinite(parsed)`: None, a text that raises (a zero denominator, a number beyond the float range: ValueError since Windows
        // 0a24294) and a non-finite number are the same refusal. Between the two Windows has
        // `if not IMPERIAL_FEET_INCHES.match(normalize_inch_marks(elev_value)) and _TEXT_AFTER_FOOT_MARK.search(elev_value):
        // parsed = None` (edit_condition_dialog.py :1626-1630): a text the shared parser accepted is refused too when anything
        // follows a foot mark and the text is no real feet-and-inches value ("5'6" would be saved as written but reloaded as 5
        // feet). Same alert; the loader does not call the rule.
        double inches = 0.0;
        if (elevation_text_inches(stripped, inches) != ElevationTextStatus::Value || !std::isfinite(inches) ||
            elevation_text_needs_closing_inch_mark(stripped))
            return invalid_value(quoted(stripped) + " is not a valid elevation. " +
                                 (metric ? kElevationHintMetric : kElevationHintImperial));
        z_inches = inches;
        return std::nullopt;
    }

    bool elevation_text_needs_closing_inch_mark(const std::string &text)
    {
        // `not IMPERIAL_FEET_INCHES.match(normalize_inch_marks(text))`: a text that starts with a real feet-and-inches value is never
        // refused, whatever follows it (and a quote elsewhere in the text no longer switches the rule off)
        if (elevation_text_starts_with_feet_and_inches(text))
            return false;
        const std::u32string chars = py_decode_utf8(text);
        // the characters a lone x may be followed by besides white space: U+180E U+200B U+200C U+200D U+2060 U+FEFF
        const auto invisible = [](char32_t c)
        { return c == 0x180E || c == 0x200B || c == 0x200C || c == 0x200D || c == 0x2060 || c == 0xFEFF; };
        const size_t count = chars.size();
        // _TEXT_AFTER_FOOT_MARK.search(text) = [apostrophe, U+2032](?![xX][\s, U+180E U+200B U+200C U+200D U+2060 U+FEFF]*$)\s*\S:
        // every position is tried, so every foot mark counts
        for (size_t mark = 0; mark < count; ++mark)
        {
            if (chars[mark] != U'\'' && chars[mark] != 0x2032) // U+0027 and U+2032, nothing else (not U+2019, U+02BC, U+00B4)
                continue;
            size_t next = mark + 1;
            // (?![xX][\s...]*$) sits directly after the mark: a lone x / X and then white space or invisible format characters only
            // up to the end exempts THIS mark (`$` adds nothing after the class, which already takes a final line feed)
            if (next < count && (chars[next] == U'x' || chars[next] == U'X'))
            {
                size_t after = next + 1;
                while (after < count && (py_is_space(chars[after]) || invisible(chars[after])))
                    ++after;
                if (after == count)
                    continue;
            }
            // \s*\S: white space (Python's \s, also none), then ANY character that is not white space (a letter or digit of any
            // script, an underscore, a quote, another mark, an invalid byte read as U+FFFD ...). White space and non-white-space
            // are disjoint sets, so no backtracking can change the answer.
            while (next < count && py_is_space(chars[next]))
                ++next;
            if (next < count)
                return true;
        }
        return false;
    }

    std::string elevation_placeholder(bool metric)
    {
        return metric ? "e.g. 3.2 m" : "e.g. 10' 6\"";
    }

    std::optional<FormIssue> parse_condition_number(const std::string &text, int &ref_no)
    {
        // int(ref_no_text) with ref_no_text = text.strip(), then Windows' three checks in this order (:1481-1506): ValueError and
        // ref_no < 1 give the same "positive whole number" alert, ref_no > MAX_CONDITION_NUMBER the range alert. str.strip() removes
        // the C0 separators U+001C..U+001F too, which int() itself does not accept around a number: strip first, like the dialog.
        using ost_import::text_rules::NumberParse;
        const std::string stripped = ost_import::text_rules::py_strip(text);
        long long value = 0;
        switch (ost_import::text_rules::py_parse_int(stripped, value))
        {
        case NumberParse::Ok:
            break;
        case NumberParse::Invalid:
            return invalid_value(kConditionNumberPositiveMessage);
        case NumberParse::Overflow:
            // a whole number beyond 64 bits (Python int() has no such limit). The parser reports it without a sign, so the sign
            // decides which check Windows would have reached first: a negative number is below 1, a positive one above the highest.
            return invalid_value(!stripped.empty() && stripped.front() == '-' ? kConditionNumberPositiveMessage
                                                                              : kConditionNumberRangeMessage);
        }
        if (value < 1)
            return invalid_value(kConditionNumberPositiveMessage);
        if (value > kMaxConditionNumber)
            return invalid_value(kConditionNumberRangeMessage);
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

    std::string condition_number_overflow_message(int new_ref_no)
    {
        const std::string number = std::to_string(new_ref_no);
        return "Condition number " + number + " cannot be used: the conditions numbered " + number +
               " and up would move up by one, and one of them is already " + std::to_string(kMaxConditionNumber) +
               ", the highest condition number.";
    }

    std::string condition_numbers_overflow_message(long long count, unsigned long long last_ref_no)
    {
        // f"Cannot add {count} condition{'' if count == 1 else 's'}: the new condition numbers would reach {last_ref_no}, past
        // {MAX_CONDITION_NUMBER}, the highest condition number." (condition_operations.py :57-62)
        return "Cannot add " + std::to_string(count) + (count == 1 ? " condition" : " conditions") +
               ": the new condition numbers would reach " + std::to_string(last_ref_no) + ", past " +
               std::to_string(kMaxConditionNumber) + ", the highest condition number.";
    }

    std::optional<std::string> refuse_new_condition_numbers(long long highest, long long count)
    {
        // last_ref_no = highest + count > MAX_CONDITION_NUMBER (:55-57), compared without forming the sum so that nothing overflows
        if (highest <= static_cast<long long>(kMaxConditionNumber) - count)
            return std::nullopt;
        // Here highest > kMaxConditionNumber - count >= 0 for every count up to kMaxConditionNumber, so the unsigned sum is exact
        return condition_numbers_overflow_message(
            count, static_cast<unsigned long long>(highest) + static_cast<unsigned long long>(count));
    }

    FormIssue condition_duplicate_failed_issue(const std::string &detail)
    {
        // the store's text for the overflow, the generic text for every other failure (nothing was written either way)
        return FormIssue{"Duplicate Error", detail.empty() ? std::string("Failed to duplicate the condition.") : detail};
    }

    FormIssue condition_save_failed_issue(const std::string &detail)
    {
        // `result.error or ...`: an empty error is the generic text
        return FormIssue{"Save Failed", detail.empty() ? std::string("Failed to save condition to database.") : detail};
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
