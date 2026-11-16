# Core unit tests (host)

C++17 tests of the platform-independent `Core/` (+ `Rendering/MeshLoader.cpp`) on MSVC (Windows) and AppleClang (macOS CI). No UIKit, no simulator.

Run (same as CI): `cmake -S Config/Tests -B build/tests -DCMAKE_BUILD_TYPE=Debug && cmake --build build/tests --parallel && ctest --test-dir build/tests --output-on-failure`
One area: `cmake --build build/tests --target test_<area>`, then `build/tests/test_<area> [--filter <substr>] [--list]` (last line: `TESTS: n PASSED: p FAILED: f`).

Add an area: create `Config/Tests/<Area>/test_*.cpp` (CMake globs it; target `test_<Area>`, ctest name `<Area>`; no CMake edits).
Extra helper sources/defines for an area: `Config/Tests/<Area>/area.cmake` (runs after the target exists; `OST_AREA_TARGET` / `OST_AREA_DIR` are set).
Add a test: `#include "TestHarness.hpp"`, then `TEST(Suite, Name) { CHECK_EQ(a, b); }`.
Macros: `CHECK` `CHECK_FALSE` `CHECK_EQ` `CHECK_NE` `CHECK_NEAR(a,b,tol)` `CHECK_STR_EQ` (non-fatal); `REQUIRE` `REQUIRE_EQ` `FAIL_TEST(msg)` (abort the test); `SCOPED_CONTEXT("case " + std::to_string(i))` names the loop case in failures.
Add a fixture: put the file in `Config/Tests/Fixtures/`, read it with `ost_test::fixture_path("name.ost")` + `ost_test::read_file(path, out)`. Write scratch files only via `ost_test::temp_path("name")` (system temp dir, never the repo).

Conventions:
- Every ported test cites its Windows origin in a comment (`// Port of tests/test_x.py::test_y`) and uses the same inputs and expected outputs. Never relax a tolerance or change an expected value to make a test pass: a failing port means an iOS core bug or a Windows ambiguity to report.
- Floats: `CHECK_NEAR` with the tolerance of the Windows test (`CHECK_EQ` on floats is exact).
- Boolean ops (Manifold) are off by default: configure with `-DOST_TEST_BOOLEAN=ON` and wrap such tests in `#if OST_HAVE_BOOLEAN`. With it OFF, boolean calls throw (test fails loudly).
- Host-incompatible Core sources go in `OST_CORE_EXCLUDE` in `Config/Tests/CMakeLists.txt` with the reason. Prefer shims in `Config/Tests/` over editing iOS sources.
- Windows has no system zlib, so CMake downloads zlib v1.3.1 there (`OST_FETCH_ZLIB`); macOS uses the system zlib.
- `Config/Tests/Harness/` tests only the harness itself; it must stay free of iOS behavior tests.
