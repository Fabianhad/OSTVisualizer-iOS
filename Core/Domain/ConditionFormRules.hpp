// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The field rules of the Edit Condition dialog: what a typed text means and which alert a refused text shows, extracted from
// EditConditionViewController.mm (_validateAndCollectTextFields, _parseDimensionText, _parsePlainText, the colour and the
// Trim / Curved Segment switches) so that the wording and the edge cases are the Windows ones and testable on the host.
//
// Windows source of truth: ost_visualizer/presentation/dialogs/edit_condition_dialog.py
//   _parse_dimension_field (:1389-1403)            '"x" is not a valid dimension for <label>.'
//   _validate_and_build_dto (:1405-1600)           the name, elevation, condition number, Display Size, Rise / Run rules
//   _on_color_changed (:326-329)                   ColorFill = red | green << 8 | blue << 16 (never ColorLine)
//   _on_trim_changed / _on_curved_changed / _sync_linear_trim_curved_state (:1305-1331)
//   application/use_cases/project/update_condition_use_case.py        "Failed to save condition to database."
// All text is UTF-8; Python's str.strip(), int() and float() are reproduced where they are observable (the shared
// ost_import::text_rules and ost_domain::parse_plain_number).
#pragma once
#include "Condition.hpp"
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

    // Display Size of a Count / Attachment symbol, in percent: a blank field is the default; outside the bounds is refused.
    inline constexpr double kDisplaySizeDefault = 100.0;
    inline constexpr double kDisplaySizeMin = 10.0;
    inline constexpr double kDisplaySizeMax = 500.0;

    // Every parser below returns nullopt and sets its output on success, or the alert of the refusal (the output is untouched).

    // name = text.strip(); an empty name is "Validation Error" / "Condition name cannot be empty.".
    std::optional<FormIssue> parse_condition_name(const std::string &text, std::string &name);

    // The Elevation field: text.strip(); blank is 0.0 (and the name gets no suffix); else display_to_inches(text, metric) must be a
    // finite number, or "Invalid Value" / '"<text>" is not a valid elevation.'. The typed text itself is kept verbatim as the name
    // suffix (COND-ELEV-01): nothing here rewrites it. The number only validates the text: the saved condition's z_value and is_top
    // come from the saved NAME (derive_elevation_from_name, see condition_to_save), as a reload gives them.
    std::optional<FormIssue> parse_elevation_z(const std::string &text, bool metric, double &z_inches);

    // Cond. No.: int(text.strip()) (Python int(): sign, underscores, Unicode digits) that is >= 1, else "Invalid Value" /
    // "Condition number must be a positive whole number." (COND-REF-01). Nothing is coerced: "", "abc", "1.5", "0", "-1" and
    // "12abc" are refused. A number beyond the int range of a Condition is refused too (it cannot be stored).
    std::optional<FormIssue> parse_condition_number(const std::string &text, int &ref_no);

    // A dimension field (_parse_dimension_field): text.strip(); blank is 0.0; else display_to_inches(text, metric) must be finite,
    // or "Invalid Value" / '"<text>" is not a valid dimension for <label>.'.
    std::optional<FormIssue> parse_dimension_value(const std::string &text, const std::string &label, bool metric, double &inches);

    // Display Size (Count / Attachment): blank -> kDisplaySizeDefault; float(text) that is finite, else "Invalid Value" /
    // '"<text>" is not a valid number for Display Size.' (for text that is not a number and for nan / inf alike); a value outside
    // 10..500 is "Validation Error" / "Display Size must be between 10% and 500%." (COND-VAL-01).
    std::optional<FormIssue> parse_display_size(const std::string &text, double &percent);

    // Rise / Run: float(text or "0") like Windows (:1539 / :1551 Linear, :1569 / :1581 Area): the text is NOT stripped and only an
    // EMPTY text is 0.0; any other text must be float() text that is finite, else "Invalid Value" / "<label> must be a number." (not
    // a number, which includes a field of white space only: float(" ") is a ValueError, DLG-FINITE-01) or "<label> must be a finite
    // number." (nan, inf, overflow). `absolute` applies abs() (the Area style, DLG-RISE-01); the Linear style keeps the sign. The
    // caller hands over the raw field text; trimming it first would turn a whitespace-only field into an empty one.
    std::optional<FormIssue> parse_slope_value(const std::string &text, const std::string &label, bool absolute, double &value);

    // "Save Failed" / "Failed to save condition to database.": the save was refused or a statement failed (COND-UPD-03).
    FormIssue condition_save_failed_issue();

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
