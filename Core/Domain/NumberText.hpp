// Layer: Domain (pure C++)
//
// Exact, locale independent number-to-text primitives behind DimensionFormat.cpp and QuantityDisplay.cpp. They
// reproduce what Python's round(), int() / str(int), format(x, ",.0f") and format(x, "g") do on a float: ties to
// even on the exact binary value, arbitrary-size integers, '.' as the decimal point whatever the C locale says.
#pragma once

#include <string>

namespace ost_domain
{
    // Python round(x) for a finite x >= 0: nearest integer, a tie goes to the even one (no dependence on the FPU
    // rounding mode). The result is an integer-valued double.
    double round_half_even(double x);

    // The exact decimal digits of a finite, integer-valued double >= 0 ("0" for 0): Python's str(int(x)) for any
    // magnitude (1e22 -> "10000000000000000000000", 1e300 -> its 301 exact digits).
    std::string integer_digits(double x);

    // snprintf(format, value) with the decimal point of the current C locale replaced by '.'. For formats with one
    // double argument such as "%.2f" and "%g".
    std::string format_double(const char *format, double value);

    // ---- Python formatters of Set Scale (plan 4.6; M3 reuses them). Locale independent: the decimal point is always '.'.
    // Where C printf differs from Python the Python text is returned: "inf" / "-inf" / "nan" for the non-finite values (never
    // "1.#INF", "nan(ind)" or "-nan"), at least two exponent digits ("1e-05"), "-0" for a negative zero.

    // Python format(value, "g") = f"{value:g}": six significant digits, exponent form when the exponent is below -4 or at least 6, no
    // trailing zeros. 100000.0 -> "100000", 1e6 -> "1e+06", 0.0001 -> "0.0001", 1e-5 -> "1e-05", 12.0000001 -> "12".
    // (set_scale_dialog.py:348-350 fills the custom fields with it; the iOS form does NOT, see SS-5 in plan section 8.)
    std::string format_g(double value);

    // Python format(value, ".<precision>g") (precision < 1 is read as 1, like Python): "%.15g" digits with the Python spellings above.
    // scales.py:58-74 format_custom_scale uses precision 15.
    std::string format_general(double value, int precision);

    // Python f"{value:.<decimals>f}": the EXACT decimal expansion of the binary value, correctly rounded (a tie of an exactly
    // representable value goes to the even digit), however large the value ("1e22" -> "10000000000000000000000.000000",
    // 1e23 -> "99999999999999991611392.000000"); "-0.000000" for -0.0 and for a tiny negative value; "inf" / "-inf" / "nan".
    // overlay_rect.py:44-48 writes the overlay rectangle with decimals = 6.
    std::string format_fixed(double value, int decimals);

    // Python f"{round(float(value), decimals):.<decimals>f}".rstrip("0").rstrip("."): the text of a stored position coordinate
    // (serialization.py:18-28 with decimals = 3, legend_position.py:25 without the round()). 0.5 -> "0.5", 2.0 -> "2", 370.3701 ->
    // "370.37", 0.0004 -> "0", -0.0004 -> "-0" (the sign of a negative zero is kept), 0.0625 -> "0.062" (an exact tie rounds to
    // the even digit), "inf" / "-inf" / "nan" are returned as they are.
    std::string format_fixed_trimmed(double value, int decimals);
}
