# Core unit tests (host)

C++17 tests of the platform-independent `Core/` (plus `Rendering/MeshLoader.cpp`). They are built with CMake for the host (MSVC on Windows during development, AppleClang on the macOS runner of `.github/workflows/build.yml`). No UIKit, no simulator, no device.

What they cover: the logic that was moved out of the Objective-C++ files into `Core/` and brought in line with the Windows app (quantities, meshing, geometry, import, persistence, conditions, browser, export, settings, formatting, parsers). What they do not cover: the Objective-C/Objective-C++ layer (`Controllers/`, `PlanView/`, `Services/`, `Rendering/*.mm`, `SceneDelegate.mm`), UIKit behaviour, CoreGraphics PDF drawing, bgfx/Metal rendering and the shaders. Those files are only compiled by the CI `xcodebuild` step (Release, iphoneos, unsigned), which compiled and linked them without an error on 2026-11-20 UTC (the first compile against the real SDK); nothing in them has been run.

Where they live: `Config/Tests/`, not a top-level `Tests/`. Nyxian compiles every `.cpp` outside the top-level `Config/` and `Resources/` folders, and the tests contain their own `main()`; moving them out of `Config/` breaks an on-device Nyxian build with duplicate symbols (see `BUILD.md`).

## Counts

Before the parity job there were 0 iOS behaviour tests (only the 10-test harness self-test existed). A fresh run on 2026-11-19 on MSVC (Debug) gives 1598 tests, all passing, in 12 areas (1588 behaviour tests plus the 10 harness self-tests), and 1606 with `-DOST_TEST_BOOLEAN=ON`. The same 1598 (1606 with booleans) pass on a clang build (zig clang 22, x86-64, `-Os -g`, `-mfma -ffp-contract=on`; see "FMA and arm64" below). The first CI fix (2026-11-20) added one test (1599), and the fixes of the two device bugs added 33 (selection and Delete 16, scene rendering 11, the scene of a realistic bid 6): the suite now has 1632 tests (1640 with booleans, all passing on MSVC and on the clang build); the counts in the other documents are those of 2026-11-19.

| Area | Tests | Covers |
|---|---:|---|
| Browser | 67 | Project browser rules: folder names, trash and restore, Deleted Bids folder (UID 1), delete folder, move bid, hierarchy order |
| Conditions | 174 | Condition update, duplicate, delete, folders and tree, form rules, style change, elevation derived from the name after a save, choices (layers, shapes, types), placement after layer hide/show |
| Export | 97 (102 with booleans) | OBJ/MTL, DXF and Three.js writers, export text rules, file names, mesh set and boolean rule, display colour and opacity, grayscale |
| Formatting | 74 | Feet-inch and millimetre text, plain numbers, quantity text, condition labels, style defaults |
| Geometry | 172 | Page transform and canvas mapping, takeoff footprints, placement snapping, drag rules, hole/attachment validation, polygon predicates |
| Harness | 10 | The harness itself (`Support/TestHarness.hpp`); must stay free of iOS behaviour tests |
| Import | 304 | .ost XML front end and parser, import transform and writer, column types, master data, raw integrity, OSP package, CAB reader and member names, text rules, digests |
| Meshing | 107 (110 with booleans) | Wall, curved wall, area and count shape meshes, hole grouping, boolean pass contract, drop stub, geometric equivalence, a realistic bid in front of the camera (bounds, frustum, far plane, pass of every mesh) |
| Parsers | 114 | Elevation text, position text, condition colour |
| Persistence | 278 | Schema defaults, load rules and errors, write validation, UID allocation, takeoff insert/delete/positions, delete cascades, layer merge and bulk, page navigation, DB wrapper |
| Quantities | 134 | UOM tables and normalisation, curve lengths, page and view quantities |
| Settings | 101 | Render config, camera framing and limits, undo history, takeoff restore and descendants, Count as Negative Quantity on the pressed takeoff, the selection of the 2D view and what Delete removes (and its data safety), the render pass of a mesh and the clip planes |
| Total | 1632 (1640 with booleans) | |

With `-DOST_TEST_BOOLEAN=ON` the Export area runs 5 more tests and the Meshing area 4 more (the cases that need the real Manifold library), while 1 Meshing test that only makes sense without Manifold is skipped; all 1640 pass on MSVC and on the clang build.

These numbers are a snapshot of the run named above: re-run the tests for the current figure. The first CI run of the host-test step is described under "FMA and arm64" below.

## Run

Same as CI, except that CI gives `--parallel` a job count (see `BUILD.md`), from the repository root:

```sh
cmake -S Config/Tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure --no-tests=error
```

Windows (MSVC): do this from an "x64 Native Tools Command Prompt for VS" and add `-G Ninja` to the first command. More in `BUILD.md`.

ctest registers one entry per area. To run one area: `cmake --build build/tests --target test_<Area>`, then `build/tests/test_<Area> [--filter <substr>]... [--list]`. `--filter` matches `Suite.Name` and can be repeated; `--list` prints the test names. The last output line is `TESTS: n PASSED: p FAILED: f`; the exit code is non-zero if a test failed.

Build flags: Debug adds `-g` and does not define `NDEBUG` (the app does not either). On every compiler except MSVC the whole test build also gets `-Os`, like the app (`GCC_OPTIMIZATION_LEVEL = s`); MSVC stays at its Debug level (`/Od`). Test targets compile with `-Wall -Wextra` on non-MSVC compilers.

## The boolean option

Boolean operations (Manifold) are off by default. `-DOST_TEST_BOOLEAN=ON` downloads Manifold v3.0.0 (git and network needed), compiles `Core/Geometry/boolean_ops.cpp` and defines `OST_HAVE_BOOLEAN=1`. With it OFF, `Support/BooleanOpsStub.cpp` provides the same symbols and every call throws, so a test that reaches a boolean call fails loudly instead of passing on wrong data. Wrap tests that need real booleans in `#if OST_HAVE_BOOLEAN`.

CI does not set this option: the CI host-test step runs the OFF configuration. Run the ON configuration locally when you touch `Core/Geometry/boolean_ops.cpp`, `Core/Meshing/BooleanPass.cpp`, `MeshFactory.cpp` or the export mesh set.

## Add a test

1. Pick (or create) an area directory `Config/Tests/<Area>/`. CMake globs `test_*.cpp`: the target is `test_<Area>`, the ctest name `<Area>`, no CMake edit. Every directory except `Support/` and `Fixtures/` is an area. Optional `Config/Tests/<Area>/area.cmake` runs after the target exists (`OST_AREA`, `OST_AREA_DIR`, `OST_AREA_TARGET` are set); no area uses one today.
2. `#include "TestHarness.hpp"`, then `TEST(Suite, Name) { CHECK_EQ(a, b); }`.
3. Macros: `CHECK` `CHECK_FALSE` `CHECK_EQ` `CHECK_NE` `CHECK_NEAR(a, b, tol)` `CHECK_STR_EQ` (non-fatal); `REQUIRE` `REQUIRE_EQ` `FAIL_TEST(msg)` (abort the test); `SCOPED_CONTEXT("case " + std::to_string(i))` names the loop case in failures. `CHECK_EQ` on floats is exact: use `CHECK_NEAR` with the tolerance of the Windows test.
4. Write the test first and watch it fail for the right reason, then fix the Core code.
5. Scratch files only through `ost_test::temp_path("name")` (system temp directory, never the repository).

Conventions:

- Every ported test cites its Windows origin in a comment (`// Port of tests/test_x.py::test_y`, or the Windows source file and lines) and uses the same inputs and expected values. Never relax a tolerance or change an expected value to make a test pass: a failing port is an iOS core bug or a Windows ambiguity to report (see `PORTING_BLOCKERS.md`).
- A test that pins a deliberate difference from Windows (a kept iOS behaviour) says so in its name, for example `...UnlikeWindows` or `Dbp05_TakeoffIncrementsZero`. These are the tests to flip if a kept or deliberate difference recorded in `PORTING_BLOCKERS.md` is ever reversed.
- Host-incompatible Core sources go in `OST_CORE_EXCLUDE` in `Config/Tests/CMakeLists.txt` with the reason. Prefer shims in `Config/Tests/` over editing iOS sources.
- Windows has no system zlib (`Core/Import/CabExtractor.cpp` needs one), so CMake downloads zlib v1.3.1 there (`OST_FETCH_ZLIB`); macOS uses the system zlib.
- `Config/Tests/Harness/` tests only the harness itself.

## Oracles and fixtures

Many tests compare iOS output with values produced by running the Windows implementation (read-only, Python 3.11 and, for the Qt rules, PySide6). They come in two forms:

- `Config/Tests/<Area>/*OracleData.hpp` (Meshing: `WindowsOracles.hpp` and `Equivalence*Oracles.hpp`): tables compiled into the test (Browser, Conditions, Export, Geometry, Meshing, Parsers, Persistence, Settings). The header comment says which Windows function produced the table.
- `Config/Tests/Fixtures/*_oracle.txt` and the two `makecab_*.cab` files: data files read at run time with `ost_test::fixture_path("name")` and `ost_test::read_file(path, out)` (used by the Conditions, Formatting and Import areas). The text oracles are tab separated and mostly hex-encoded (doubles as `float.hex()`, texts as hex UTF-8) so that no character set or locale can change them; the header of each file describes its columns. The two CAB files were written by Microsoft's `makecab.exe` (MSZIP, real checksums; one of them has two cabinet folders); the comments in `Import/test_cab.cpp` and `Import/test_cab_status.cpp` record how they were made and that they were checked once with Windows `expand.exe`.

The scripts that generated the oracles were throw-away scripts run against the Windows source tree and are not in this repository, so an oracle cannot be regenerated from here. Comments inside some oracle files mention those script paths: they do not exist in the repository. If the Windows behaviour changes, the oracle must be regenerated from the Windows sources by someone who has them.

## FMA and arm64

The Windows geometry is computed without fused multiply-add (`a * b + c` is two rounded operations). AppleClang on arm64 contracts such expressions by default (`-ffp-contract=on`), MSVC on x64 does not. A few Windows decisions sit on the last bit of the result (the segment count of a curved wall, the tangent of a degenerate loop, the drop stub length test, an earcut ear test), so the same source can give different meshes on the two compilers.

This already happened: the first GitHub run (2026-11-16 UTC, image macos-15-arm64) failed in the Meshing area on one curved-wall test (`CurvedMesh.CentreLineMatchesTheWindowsMeshOnTheOracleCases`) that passed on MSVC; the other six areas of that time passed. `Core/Geometry/strict_fp.hpp` (`ost_fp`) now switches contraction off (`#pragma clang fp contract(off)`) in the translation units that include it (the mesh generators, `MeshFactory.cpp` and `TakeoffMeshBuilder.cpp` in `Core/Meshing`, and `earcut_impl.cpp`, `takeoff_footprint.cpp`, `drag_rules.cpp`, `placement_snap.cpp`, `boolean_ops.cpp`, `DisplayColor.cpp` and `threejs_writer.cpp`) and provides the `hypot` the Windows build returns, so the results of those files no longer depend on the compiler.

What was checked here: the full suite passes on MSVC and on a clang build with FMA contraction enabled (x86-64, `-mfma -ffp-contract=on`), which approximates the arm64 behaviour. The first GitHub run on this code (2026-11-20 UTC, run 44642251350, AppleClang 17.0.0, macos-15) built all 12 test executables and passed 11 areas, Meshing among them: the curved-wall test that failed on 2026-11-16 passes now, which is consistent with `strict_fp.hpp` doing what it was written for. Geometry failed one assertion, `MarkerAnchor.CountTriangleAndCurvedBand`, and it is not contraction but a tie. The marker anchor of a mirror-symmetric curved band comes from a nearest-point search whose two best grid centres are mirror images; in the doubles their squared distances come out bit-identical on the x86-64 builds here, so the first (left) one wins, which is what Windows gives, and a last-bit change in the arc points breaks the tie. The arm64 run picked the mirror one (77.4244 instead of 72.5756, one grid cell away). The only calls in that path whose result can differ between platforms are `std::atan2`, `std::cos` and `std::sin` (on Apple targets clang also turns a `cos` and `sin` of one argument into one `__sincos_stret` call), so the cause is inferred to be Apple's libm against the Windows CRT. It was not observed on arm64. What supports it: no fused multiply-add is emitted for `takeoff_footprint.cpp` and `Meshing/gen_curved.cpp` (checked on an aarch64-macos build with zig clang 22, not AppleClang: 0 fused instructions with `strict_fp.hpp`, 20 and 31 without it), and nudging one of the 53 libm results of the test case by one ulp flips the answer in 23 of 106 cases, to the value CI reported (20 of the 23 bit for bit, the rest within 2 ulp). Matching the MSVC CRT's `atan2`, `sin` and `cos` bit for bit was not attempted (it would mean in-tree implementations and change numerics everywhere); the case is recorded in `PORTING_BLOCKERS.md`, "Not reproducible on iOS". That assertion now accepts the Windows point and its mirror image, and a new test with dyadic inputs and no libm call (`MarkerAnchor.AnExactTieKeepsTheFirstCandidateOfTheWindowsOrder`) pins the rule that an exact tie keeps the first candidate (replacing `<` by `<=` in the search makes it fail). If a test fails in Meshing or Geometry on the macOS runner, suspect contraction in a file that does not include `strict_fp.hpp`, then a result decided by the last bit of a libm function, before suspecting the test.
