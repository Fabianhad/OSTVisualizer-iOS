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
}
