// Differential table for ELV-01..ELV-06: every row of ElevationOracleData.hpp was produced by running the Windows
// infrastructure/parsers/position_parser.py::extract_z_value_from_name (Python 3.11.4) on the name. The table
// covers the Windows test inputs, the Phase 1 plan cases, combinatorial samples of the imperial and metric grammars
// (separators, minus signs, lookalike marks, Unicode whitespace) and seeded random token soups.
// Compared exactly (the C++ code performs the same IEEE operations as the Python code).
#include "ParsersTestSupport.hpp"
#include "ElevationOracleData.hpp"

#include "Persistence/ProjectStore/ProjectStore_Internal.hpp"

#include <cmath>

TEST(ElevationOracle, ExtractZValueMatchesWindowsOnEveryRow)
{
    size_t rows = 0, unreadable = 0, nontrivial = 0;
    for (const auto &row : ost_parsers_test::kElevationOracle)
    {
        SCOPED_CONTEXT(std::string("name=") + row.name);
        const std::pair<double, bool> got = ost_store::extract_z_value(row.name);
        // status 1: Windows raised ValueError and its reader stores (0.0, False); row.z/is_top are exactly that.
        CHECK_EQ(got.first, row.z);
        CHECK_EQ(got.second, row.is_top);
        CHECK_EQ(std::signbit(got.first), std::signbit(row.z));
        ++rows;
        if (row.status == 1)
            ++unreadable;
        if (row.z != 0.0 || row.is_top)
            ++nontrivial;
    }
    // guard against an accidentally truncated table
    CHECK(rows > 3000);
    CHECK(unreadable > 20);
    CHECK(nontrivial > 1000);
}
