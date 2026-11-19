// Layer: Domain (pure C++, no UIKit/Foundation/SQLite)
//
// Dimension and plain-number text of the Edit Condition dialog (feet-inch / millimetre text <-> inches, Rise / Run /
// Display Size text), extracted from EditConditionHelpers.mm (inchesToDisplay, displayToInches) and
// EditConditionViewController.mm (OSTPlainDisplay, OSTParseStrictDouble).
//
// Windows source of truth:
//   ost_visualizer/domain/services/dimension_format_service.py   inches_to_display, mm_to_display, display_to_inches,
//                                                                display_to_mm, mm_to_inches, inches_to_mm
//   ost_visualizer/presentation/dialogs/edit_condition_dialog.py _load_condition (str(cond.rise), str(abs(cond.rise)),
//                                                                str(int(cond.display_size))) and
//                                                                _validate_and_build_dto (float(text), isfinite)
//
// The Windows code relies on Python semantics (round() = ties to even, int() on huge floats, str.strip(), the re module
// on Unicode str, float(), repr(float)); this port reproduces them where they are observable. Text is UTF-8
// (std::string). Nothing here depends on the C locale (no NSNumberFormatter / NSScanner semantics).
#pragma once

#include <optional>
#include <string>

namespace ost_domain
{
    // dimension_format_service.MM_PER_INCH
    inline constexpr double DIM_MM_PER_INCH = 25.4;

    // mm / MM_PER_INCH and inches * MM_PER_INCH
    double mm_to_inches(double mm);
    double inches_to_mm(double inches);

    // inches_to_display(inches, metric). Imperial: eighths = round(|inches| * 8) with Python's round (ties to even),
    // then "5\"", "5 1/2\"", "1/8\"", "1' 0\"", "-1' 2 1/4\"" ... ("" for 0 and for anything that rounds to 0 eighths).
    // Metric: mm_to_display(inches * 25.4). Non-finite input gives "". The feet digits are exact for any magnitude
    // (Python ints); beyond the double range of inches * 8 Windows raises OverflowError, here the result is "".
    std::string inches_to_display(double inches, bool metric = false);

    // mm_to_display(mm): "" for 0 and non-finite, the integer digits for an integral value ("-12", "1234567"),
    // else Python's format(mm, "g") (six significant digits: "25.4", "1.23457", "1e-07").
    std::string mm_to_display(double mm);

    // display_to_mm(text): blank -> 0.0; otherwise float() of the text without trailing 'm' / 'M' characters
    // ("25.4 mm", "-3  m "). nullopt where Windows returns None (not a number, nan, inf, overflow).
    std::optional<double> display_to_mm(const std::string &text);

    // display_to_inches(text, metric). Imperial layouts (first match wins): feet+inches+fraction, feet+inches
    // (decimals allowed), feet+fraction, feet only, inches+fraction, fraction only, inches only, then plain float()
    // ("1e2", ".5", "+7"). Fractions are the seven eighths with a single-digit numerator and denominator. Blank text
    // is 0.0. nullopt where Windows returns None, including nan/inf/overflow/huge digit runs.
    std::optional<double> display_to_inches(const std::string &text, bool metric = false);

    // Plain number fields (Rise, Run, Display Size).
    //
    // str(value) for a non-zero value, "" for 0.0 / -0.0: Python repr, so float(text) == value. For the Area style
    // Windows shows abs(rise) / abs(run): pass std::fabs(value) (0 stays "").
    std::string plain_number_text(double value);

    // str(int(display_size)) when display_size is truthy, else "". The integer part is exact for any magnitude; nan and
    // inf (Windows raises) give "".
    std::string display_size_text(double value);

    // float(text) with the finite check of the dialog (_validate_and_build_dto): no stripping or blank rule of its own
    // (float("") raises, so "" is NotANumber; callers decide what a blank field means, Windows: Rise/Run "" -> 0).
    enum class PlainNumberStatus
    {
        Ok,         // out holds the value
        NotANumber, // Python ValueError: "Rise must be a number."
        NotFinite   // nan, inf, -inf or overflow (1e400): "Rise must be a finite number."
    };
    PlainNumberStatus parse_plain_number(const std::string &text, double &out);
}
