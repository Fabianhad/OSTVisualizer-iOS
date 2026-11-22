// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The field rules of the Edit Condition dialog: what a typed text means and which alert a refused text shows, extracted from
// EditConditionViewController.mm (_validateAndCollectTextFields, _parseDimensionText, _parsePlainText, the colour and the
// Trim / Curved Segment switches) so that the wording and the edge cases are the Windows ones and testable on the host.
//
// Windows source of truth: ost_visualizer/presentation/dialogs/edit_condition_dialog.py
//   _parse_dimension_field (:1411-1426)            '"x" is not a valid dimension for <label>.'
//   _validate_and_build_dto (:1428-1742)           the name, elevation, condition number, Display Size, Rise / Run rules
//   _validate_and_build_dto elevation (:1437-1461)   validated with elevation_text_inches (b9902e6); placeholder (:389-392)
//   _TEXT_AFTER_FOOT_MARK (:72) and the elevation block of _validate_and_build_dto (:1437-1461): the closing-inch-mark rule on top
//                                                  of it (Windows 49a2d59; the rule itself is :1444-1445)
//   domain/services/elevation_text.py:131-144      elevation_text_inches, the loader's parser (PositionParser.hpp)
//   _on_color_changed (:333-336)                   ColorFill = red | green << 8 | blue << 16 (never ColorLine)
//   _on_trim_changed / _on_curved_changed / _sync_linear_trim_curved_state (:1327-1345)
//   application/use_cases/project/update_condition_use_case.py        "Failed to save condition to database."
// All text is UTF-8; Python's str.strip(), int() and float() are reproduced where they are observable (the shared
// ost_import::text_rules and ost_domain::parse_plain_number).
#pragma once
#include "Condition.hpp"
#include <climits>
#include <optional>
#include <string>

namespace ost_domain
{

    // The alert of a refused value: show_warning(parent, title, message). The dialog stays open and focuses the field.
    struct FormIssue
    {
        std::string title;
        std::string message;
    };

    inline constexpr const char *kFormInvalidValueTitle = "Invalid Value";
    inline constexpr const char *kFormValidationErrorTitle = "Validation Error";

    // The highest condition number (update_condition_dto.py MAX_CONDITION_NUMBER): the limit of an Access Long and of Condition::ref_no
    // (an int). The dialog refuses a number above it, and the store refuses a renumber whose shift would carry another condition past it
    // and a duplicate whose new number would pass it.
    inline constexpr int kMaxConditionNumber = 2147483647;
    static_assert(kMaxConditionNumber == INT_MAX, "a condition number is an int: the highest one is INT_MAX");

    // The two refusals of the Cond. No. field (edit_condition_dialog.py :1481-1506), both under "Invalid Value".
    inline constexpr const char *kConditionNumberPositiveMessage = "Condition number must be a positive whole number.";
    inline constexpr const char *kConditionNumberRangeMessage = "Condition number must be between 1 and 2147483647.";

    // Display Size of a Count / Attachment symbol, in percent: a blank field is the default; outside the bounds is refused.
    inline constexpr double kDisplaySizeDefault = 100.0;
    inline constexpr double kDisplaySizeMin = 10.0;
    inline constexpr double kDisplaySizeMax = 500.0;

    // Every parser below returns nullopt and sets its output on success, or the alert of the refusal (the output is untouched).

    // name = text.strip(); an empty name is "Validation Error" / "Condition name cannot be empty.".
    std::optional<FormIssue> parse_condition_name(const std::string &text, std::string &name);

    // The hint that ends the alert of a refused elevation (edit_condition_dialog.py:1447-1451): the forms that are accepted, in the
    // measurement system of the bid. Only the hint and the placeholder depend on the system: the same texts are accepted in both.
    inline constexpr const char *kElevationHintMetric = "Use metres or centimetres, for example \"3.2 m\" or \"320 cm\".";
    inline constexpr const char *kElevationHintImperial = "Use feet and inches with unit marks, for example 10' 6\".";

    // The Elevation field (_validate_and_build_dto :1437-1461 in Windows 49a2d59):
    // text.strip(); blank is 0.0 (and the name gets no suffix) and is never validated; else the text must be one the LOADER reads:
    // elevation_text_inches (PositionParser.hpp, the function behind extract_z_value_from_name) must give a finite number, so a bare
    // number, "mm", units of both systems in one text, "5'" alone, a zero denominator (the ValueError Windows catches) and a feet run
    // beyond the float range (an OverflowError Windows does NOT catch: refused here instead of copying the crash) are refused with
    // "Invalid Value" / '"<stripped text>" is not a valid elevation. <hint>' (kElevationHintMetric in a metric bid,
    // kElevationHintImperial otherwise). So is a text the closing-inch-mark rule refuses (elevation_text_needs_closing_inch_mark): "5'6",
    // "10'6", "5'6x", "5'abc" and "-2'3" are refused, "5'x", "5' 6\"" and "5'6\"" are accepted; same alert, no new wording. The typed
    // text itself is kept verbatim as the name suffix (COND-ELEV-01): nothing here rewrites it, and the number is the one a reload of
    // that name gives, so the form's z_value equals what condition_to_save derives from the saved NAME (derive_elevation_from_name).
    std::optional<FormIssue> parse_elevation_z(const std::string &text, bool metric, double &z_inches);

    // The rule of the Edit Condition dialog on a text the shared parser already accepted (Windows 49a2d59:
    // edit_condition_dialog.py :72 `_TEXT_AFTER_FOOT_MARK = re.compile(r"['\u2032](?![xX]\s*$)\s*[0-9A-Za-z]")` and :1444
    // `if '"' not in elev_value and _TEXT_AFTER_FOOT_MARK.search(elev_value): parsed = None`). The feet-only gate of the loader reads
    // "5'6" as 5 feet and drops the rest; the dialog refuses such a text instead of saving 5 feet silently. True (= refuse) when the
    // text has NO ASCII double quote U+0022 anywhere and some foot mark (U+0027 or U+2032) is followed by optional Unicode white space
    // (Python's \s = str.isspace) and then ONE ASCII letter or digit [0-9A-Za-z], except a foot mark directly followed by a lone x or X
    // and nothing but white space up to the end (5'x). Every foot mark is checked. Non-ASCII letters and digits, the underscore, U+201D
    // and U+2033 do not count. It mirrors the regex on the text it is given: parse_elevation_z passes the STRIPPED text (as the dialog
    // does), so trailing white space never reaches it there. Invalid UTF-8 never counts as white space or as an alphanumeric. The
    // loader (elevation_text_inches, extract_z_value_from_name, derive_elevation_from_name) does NOT call it: a stored name such as
    // "Footing @T 5'6" still reads as 60 inches, top.
    bool elevation_text_needs_closing_inch_mark(const std::string &text);

    // The placeholder of the Elevation field (:389-392): a form the loader reads, 'e.g. 3.2 m' in a metric bid and 'e.g. 10' 6"' in
    // an imperial one.
    std::string elevation_placeholder(bool metric);

    // Cond. No. (:1480-1508): int(text.strip()) (Python int(): sign, underscores, Unicode digits), checked in Windows' order: text that
    // is not a whole number, then a value below 1, are "Invalid Value" / "Condition number must be a positive whole number."
    // (COND-REF-01; nothing is coerced: "", "abc", "1.5", "0", "-1" and "12abc" are refused, and so is a huge NEGATIVE number, which
    // is below 1 before it is above anything); then a value above kMaxConditionNumber, beyond 64 bits as well, is "Invalid Value" /
    // "Condition number must be between 1 and 2147483647." (DLG-FINITE-01, b9902e6). kMaxConditionNumber itself is accepted.
    std::optional<FormIssue> parse_condition_number(const std::string &text, int &ref_no);

    // The refusal of a renumber whose shift would carry another condition past kMaxConditionNumber
    // (ConditionNumberOverflowError, condition_operations.py :650-655): the conditions numbered `new_ref_no` and up would move up by one
    // and one of them already holds the highest number. ost_store::update_condition returns it as its error text.
    std::string condition_number_overflow_message(int new_ref_no);

    // The refusal of NEW conditions whose numbers would pass kMaxConditionNumber (Windows 49a2d59:
    // condition_operations.py _allocate_condition_identities :56-62, ConditionNumberOverflowError):
    // "Cannot add N condition(s): the new condition numbers would reach X, past 2147483647, the highest condition number." with
    // "condition" for N == 1 and "conditions" for every other N. X = highest + N, the last of the new numbers; it is only ever shown when
    // it is above kMaxConditionNumber, so it is an unsigned 64 bit value (a stored number of up to LLONG_MAX plus N does not overflow it).
    std::string condition_numbers_overflow_message(long long count, unsigned long long last_ref_no);

    // The decision (Windows :48-62): `count` (1 .. kMaxConditionNumber) new conditions of a bid whose highest condition number is
    // `highest` get highest + 1 ... highest + count. nullopt when they all fit (<= kMaxConditionNumber), else the message above (all or
    // nothing: one check for the whole set, before anything is written). The highest number of a bid without numbers (no row, only
    // NULL numbers, or no number column) is passed as 0: Windows starts at 1 for None and 0 + 1 is the same. A stored number above
    // kMaxConditionNumber (SQLite can hold what an Access Long cannot) refuses as well. Never overflows: it compares
    // highest > kMaxConditionNumber - count. Condition numbers are allocated by insert_condition and duplicate_condition.
    std::optional<std::string> refuse_new_condition_numbers(long long highest, long long count);

    // The alert of a failed Duplicate: title "Duplicate Error" (the iOS wording, DBP-11; Windows titles the same text "Duplicate Conditions"
    // and shows nothing for a failure without text) and as message `detail`, the text above when the store gave one, else the generic
    // "Failed to duplicate the condition.".
    FormIssue condition_duplicate_failed_issue(const std::string &detail = std::string());

    // A dimension field (_parse_dimension_field): text.strip(); blank is 0.0; else display_to_inches(text, metric) must be finite,
    // or "Invalid Value" / '"<text>" is not a valid dimension for <label>.'.
    std::optional<FormIssue> parse_dimension_value(const std::string &text, const std::string &label, bool metric, double &inches);

    // Display Size (Count / Attachment): blank -> kDisplaySizeDefault; float(text) that is finite, else "Invalid Value" /
    // '"<text>" is not a valid number for Display Size.' (for text that is not a number and for nan / inf alike); a value outside
    // 10..500 is "Validation Error" / "Display Size must be between 10% and 500%." (COND-VAL-01).
    std::optional<FormIssue> parse_display_size(const std::string &text, double &percent);

    // Rise / Run: float(text or "0") like Windows (:1578 / :1590 Linear, :1608 / :1620 Area): the text is NOT stripped and only an
    // EMPTY text is 0.0; any other text must be float() text that is finite, else "Invalid Value" / "<label> must be a number." (not
    // a number, which includes a field of white space only: float(" ") is a ValueError, DLG-FINITE-01) or "<label> must be a finite
    // number." (nan, inf, overflow). `absolute` applies abs() (the Area style, DLG-RISE-01); the Linear style keeps the sign. The
    // caller hands over the raw field text; trimming it first would turn a whitespace-only field into an empty one.
    std::optional<FormIssue> parse_slope_value(const std::string &text, const std::string &label, bool absolute, double &value);

    // "Save Failed" / the reason of the refusal, or "Failed to save condition to database." when there is none: the save was refused
    // or a statement failed (COND-UPD-03). Windows' use case answers a ConditionNumberOverflowError with its own text and every other
    // failure with the generic one; the dialog shows result.error under "Save Failed" (edit_condition_dialog.py :1810-1817).
    FormIssue condition_save_failed_issue(const std::string &detail = std::string());

    // Trim and Curved Segment exclude each other (Linear style, COND-TRIM-01): checking Trim unchecks Curved Segment and disables
    // it; checking Curved Segment unchecks Trim; a loaded condition with both set shows Curved Segment off.
    void set_trim(Condition &condition, bool on);
    void set_curved_segment(Condition &condition, bool on);
    void normalize_trim_curved(Condition &condition);
    inline bool curved_segment_enabled(const Condition &condition) { return !condition.trim; }

    // The ColorFill of a colour (COL-PICK-01): red | green << 8 | blue << 16 from 8-bit channels (0..255, masked), and from
    // float channels 0..1 by rounding to the nearest 8-bit value (a truncating conversion turns 0.6 into 152).
    int pack_bgr(int red8, int green8, int blue8);
    int color_fill_from_rgb(double red, double green, double blue);

}
