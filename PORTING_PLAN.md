# OST Visualizer - iOS Porting Plan

Last updated: 2026-11-19

What is left to do, in order. The parity fixes themselves are done: this plan no longer lists them. Facts it rests on (details in `PORTING_STATUS.md`; item records in `PARITY_MAP.md` and `PARITY_DIVERGENCES.json`; Windows `6d4786f` vs iOS branch `ios-parity-sync`, HEAD `42b46c3` plus uncommitted work):

- All 220 parity records have a final status and none is open: 172 fixed, 25 fixed-ui-unverified, 21 wont-fix (user decision; this includes the 8 items that were blocked-needs-decision until 2026-11-19), 1 cannot-reproduce, 1 no-longer-divergent. All 44 earlier user decisions and the 25 decisions of 2026-11-19 are recorded.
- The host unit tests (`Config/Tests`, 12 areas) hold 1598 tests, 1588 of them iOS behaviour tests (0 before the parity job); 1598 of 1598 pass on MSVC and on a clang build with fused multiply-add, 1606 of 1606 with `-DOST_TEST_BOOLEAN=ON`. The decisions job of 2026-11-19 started from 1576 (1584 with Manifold) and added 22 tests.
- Nothing was built or run on iOS: no Xcode, macOS, XcodeGen, ldid, simulator or device was available. The `.m`/`.mm` files were only syntax-checked (27 of 27, 0 errors, 4 baseline warnings) against hand-written stub headers that are not in the repo. The only GitHub run (2026-11-16) stopped at the host unit tests, before the fix described in section 1.
- The status documents have no checkboxes so they cannot go stale; this plan lists what is still open, not what was done.

## 1. First full CI run on GitHub

`.github/workflows/build.yml` ("Build IPA", macos-15, Xcode 16.4, iPhoneOS 18.5 SDK, Apple clang 17) has never completed. Details in `BUILD.md`. Push the branch and check, in job order:

1. Xcode selection (`/Applications/Xcode_16.4.app` must exist on the runner image; the step fails with a listing if not) and the Homebrew install of xcodegen and ldid. Both worked on 2026-11-16.
2. **Unit tests on AppleClang** (`cmake -S Config/Tests -B build/tests -DCMAKE_BUILD_TYPE=Debug`, `ctest --no-tests=error`; Debug flags plus `-Os`). This is where the run of 2026-11-16 failed: Meshing 75 of 76, `CurvedMesh.CentreLineMatchesTheWindowsMeshOnTheOracleCases` (zero-length case `zero_len_7`, sample 3), a result that differs between MSVC and AppleClang because of fused multiply-add on arm64. The fix compiles the Core meshing code, the earcut instantiation and some geometry code without FMA contraction (`Core/Geometry/strict_fp.hpp`, `#pragma clang fp contract(off)`, plus `ost_fp::hypot`). It passes on the clang+FMA host build; only this step shows whether it holds on AppleClang arm64. If another test differs by one bit, the same file is the place to look; never relax a tolerance.
3. libmanifold arm64 check and plist lint.
4. bgfx, bx and bimg built from the pinned commits (`BGFX_SHA`, `BX_SHA`, `BIMG_SHA`) with `make ios-arm64-release`. The generated makefiles hard-code clang flags that were verified only with Xcode 16.4 (workflow comment). The first run is the slow one; check that the cache is saved and a second run restores it. The pins are the commits the committed headers were taken from, per the workflow comment; not re-checked.
5. The minos check of the five prebuilt libraries (`libmanifold.a` and the four bgfx libraries) against `EXPECTED_MIN_OS` 16.0: the step fails on any object with a higher minos.
6. `xcodegen generate` from `project.yml`, then the readback of `-Os` (`GCC_OPTIMIZATION_LEVEL` s) and the 16.0 deployment target from the generated project for both configurations.
7. `xcodebuild` (iphoneos, Release, signing off). This is the first time Xcode compiles the Objective-C/Objective-C++ sources against the real SDK and links `libmanifold.a` plus the four bgfx libraries: expect to fix compile or link errors (see section 2). No "built for newer iOS version" linker warnings are expected: the deployment target is 16.0 and the libraries are minos 16.0.
8. Bundle checks: executable `OST Visualizer`, bundle id `com.fabian.ostvisualizer`, `MinimumOSVersion` 16.0, arm64 only, no `*.sc` in the bundle.
9. `ldid` fake-sign with `Config/Entitlements.fakesign.plist` (only `get-task-allowed`); the step fails if any entitlement key matches `memory|jetsam|extended-virtual-addressing`.
10. IPA packaging and artifact upload (`OST-Visualizer-ipa`, 30 days).

## 2. Compile the platform layer against the real SDK

The syntax check used hand-written stubs for Foundation, CoreFoundation, CoreGraphics, QuartzCore, UIKit and UniformTypeIdentifiers, and a macOS target (so `TARGET_OS_IPHONE` is 0; the sources contain no such conditionals). It cannot see wrong API availability, ARC problems, selector or protocol mismatches against the real headers, or anything in the bgfx/Metal code beyond what compiles against the stubs. When `xcodebuild` first runs, compile errors are expected and are the next work item; the files with the most new code are `Controllers/ViewController/ViewController.mm` and its components, `PlanView/PlanView.mm` and its components, `Services/ProjectService.mm`, `Services/ExportService.mm`, `Services/ImportService.mm` and the `EditConditionViewController` files.

## 3. Install and run on a device

Not done. The IPA is fake-signed, so stock iOS refuses it; an installer that accepts fake-signed IPAs (TrollStore is one example in `BUILD.md`) is needed. Then:

- Launch, import a real `.ost` and a real `.osp`. No sample `.ost`/`.osp` files were found in either repository (`Config/Tests/Fixtures` holds only oracle tables and two test cabinets), so real files must be supplied: pages, layers and folders; linear, area, count and attachment conditions; holes, negatives, slope, curves; PDFs (multi-page, rotated, flipped) and overlays; metric bids; annotations.
- The 25 fixed-ui-unverified ids, one by one: the checklist is in `PORTING_STATUS.md`, section "Not verified" (page rotation and flip on the PDF background, attachments and holes in 2D, handle visuals, edit-pan start, page-switch selection, sidebar refresh, layer hide/show against an active placement, condition editor pickers and Smart Punctuation, save-failure alerts, undo grouping and Count as Negative Quantity on the pressed takeoff only, Edit Condition in a metric bid and the elevation after a save, browser alerts, export alerts).
- Rendering on device: bgfx/Metal start-up, opaque and OIT passes, mesh-ID picking and readback, selection highlight, orbit/pan/pinch, fit and reset; memory and frame-time baseline on a large project.
- Takeoff placement, drag and undo on touch; iPad popovers.
- OBJ, DXF and HTML export through the share sheet; export write failures; empty and large projects; import errors and cancellations.
- iRAM+ still recognises the app after a reinstall (bundle id and executable name unchanged between builds).

Compare each real file with the Windows app: the host tests only cover the cases they were built from.

## 4. Decisions: closed on 2026-11-19, and what is still open

On 2026-11-19 the user decided every item that was blocked (8) and every candidate found while fixing (16 entries; DLG-FINITE-01 only for its Rise/Run half): 25 decisions, 21 that keep the iOS behaviour (no code change) and 4 code changes. Nothing waits on a decision to run the app. The decisions with the Windows file:line are in `PORTING_STATUS.md`, section "Decisions of 2026-11-19", and in `PORTING_BLOCKERS.md`.

Still open (the user has not decided these; iOS keeps its behaviour meanwhile):

| Id | Question |
|---|---|
| DLG-FINITE-01, condition-number half | Widen `Condition::ref_no` and the column reads to 64 bits, or keep refusing 2147483648 and up? Windows accepts any whole number the column holds (`edit_condition_dialog.py:1449-1469`). |
| COND-PLACE-01, note 1 | Hiding the Image layer: copy the Windows exemption, so a placement of a condition on the Image layer keeps running? Judged an edge case, not a Windows defect (`ui_event_coordinator.py:6473-6476`, `:6503-6504`). |
| COND-PLACE-01, note 2 | Hide All with an armed placement of a layer-less condition: nothing to port unless the Windows behaviour is wanted byte for byte; it is a possible Windows defect with the same root as LP-22 (`ui_event_coordinator.py:1473-1489`, `project_data_service.py:664-678`). |

Besides these, 10 notes for information and 7 code-hygiene notes are listed in `PORTING_STATUS.md`, section "Still open" (20 entries in all).

Decided on 2026-11-19, iOS behaviour kept (21 records, no code change): DBP-04 (with DBP-05, SNP-01, LP-19, LOAD-01), BRW-09, EXP-12, EXP-13, EXP-16, LP-24, RPV-29, IMP-20 (three sub-rules), COND-UPD-02, COND-FOLDER-01, DBP-33, EXP-21, EXP-09, RPV-26, RPV-27 (a and b), RPV-16, UND-02, GEO-M02, IMP-12. The 8 items that were blocked-needs-decision (DBP-04, BRW-09, EXP-12, EXP-13, EXP-16, IMP-20, LP-24, RPV-29) are now `wont-fix`; the others keep their status.

Code changes of 2026-11-19 (4, with host tests): COND-STYLE-02 and COND-ELEV-01 (deliberate deviations from the literal Windows behaviour), DLG-FINITE-01 (whitespace-only Rise and Run refused, like Windows) and UND-09 (Count as Negative Quantity changes only the pressed takeoff). Their device checks are part of the checklist in section 3.

Possible Windows defects found while researching the kept items (read-only, for you to fix on the Windows side; 18 records, Windows file:line in `PORTING_STATUS.md`, section "Possible Windows defects"): DBP-05 (decision DBP-04), EXP-12, EXP-13, EXP-16, IMP-20 (DATETIME overflow and latin-1 annotation names), IMP-12, COND-UPD-02, COND-FOLDER-01, COND-STYLE-02 (two), COND-ELEV-01, LP-24, RPV-27 (b), and for earlier decisions EXP-17, EXP-M02 and LP-22, plus COND-PLACE-01 note 2.

Kept by the earlier user decisions of 2026-11-16 (13 `wont-fix`, listed for reference): EXP-15, COND-EXTRA-01, LP-22, EXP-17, EXP-18, EXP-M02, GEO-15, IMP-17, RPV-22, CAM-02, DBP-34, DBP-M05, MSH-21.

## 5. Features Windows has and iOS does not (84, list only)

Source: `PARITY_MAP.md` section (H), 87 features at the 2026-11-15 map. Three of them were implemented by this job and are removed here: metric units with dimension display and quantity formatting, quantity rounding with attachment aggregation, and count condition shape meshes (ellipse, triangle, rhombus, hexagon, octagon); the PDF page index half of "multi-page PDF page index and TIFF raster pages" is done too (LP-16, fixed-ui-unverified). Not scheduled. Marker `[P]` = tied to the Windows platform per the Phase 1 mappers; `[~]` = plausibly platform-only in the writer's judgement (touch or sandbox makes it unnecessary, or it needs a server stack), a product decision either way. `PORTING_MAP.md` names the Windows modules.

| Group | Features |
|---|---|
| Export (9) | OST export (.ost); OSP export (.osp package); PDF export with takeoff and annotation markup (QPDF vs PDFKit/CoreGraphics writer unresolved); PDF annotation captions (Bluebeam-compatible); FBX export; Summary CSV export; full HTML 3D viewer (2D plan tab, visibility panel, hover tips); elevation callouts in HTML and PDF; drop condition elevation from names on export |
| Page settings (12) | Set Scale dialog and page settings bar; rename and delete page; Plan Organizer (add, duplicate, import, move pages and folders); Adjust Images (invert, bitonal); rotate and flip page image; overlay image (select, move, resize, show; fields are loaded but never drawn); page show mode (Original, Overlay, Both); TIFF raster pages; page image plane in 3D; persisted page view state and selected page; missing-file and render-progress notices; page name display options |
| Editing (20) | Cut, copy, paste, duplicate takeoffs and annotations; Paste Backout; Backout mode; rotate and flip takeoffs; slope direction handle; curved segment and control points (curved placement and the centre handle were declined, RPV-22; existing curves are drawn); assign to current area; reassign condition; marquee selection and Select All; snap to PDF lines, takeoffs and right angles; zoom tool and percentage selector `[~]`; PDF text selection; complete undo/redo history (Windows also undoes rotate, paste, property edits and annotations; iOS covers move, group move, place, delete and negative toggle); create conditions and condition folders; condition copy, cut and paste across bids; duplicate-and-reassign; Renumber Conditions; layer management and default layers; edit actions in the 3D context menu; cursor and pointer preferences `[P]` |
| Annotation (7) | Annotation drawing, persistence and styles; hotlinks; named views; takeoff condition labels on the 2D plan; detached Annotation and View windows `[~]` |
| Business (10) | Bid comparison; Cover Sheet; Summary tab; project tree editing (new, duplicate, rename, copy/cut/paste bids); job status grouping and bid lock; Employees, Payroll classes, Job statuses and Condition types master data; bid areas (areas are not surfaced by decision, DBP-M05) |
| Other (7) | 3D section cuts; 3D feature-edge overlay; saved per-bid 3D camera state (the refit is kept, CAM-02); separate 2D and 3D display modes with sync toggle; tower crane visualization; multi-database registry `[~]`; pan sidebar (mini-map) `[~]` |
| AI takeoff (6) | AI takeoff analysis; proposals (scale and slab elements); changeset review, apply, undo; assumptions sidecar and audit log; AI takeoff MCP helper `[P]`; read-only MCP database server `[P]` |
| Collaboration (3) | Microsoft SQL Server backend `[~]`; multi-user collaboration engine `[~]`; SQL credentials in Windows Credential Manager `[P]` |
| Platform (10) | Access .mdb databases `[P]`; realtime sync with On-Screen Takeoff `[P]`; licensing `[P]`; update check `[P]`; About dialog `[P]`; Windows installer and file associations `[P]`; desktop menus, toolbars, status bar, shortcuts `[P]`; takeoff toolbar customization `[P]`; Options dialog and config `[P]`; Windows theme following `[P]` |

Two of these were the old plan's top blockers and are still open: round-tripping an edited project back to `.ost`/`.osp` (B-009 in `PORTING_BLOCKERS.md`) and PDF export with takeoff markup (B-004). Whether the MVP needs them is a product decision; none is started. IMP-20 (kept as iOS does, decided 2026-11-19) and DBP-34 would need another look only if an OST export is added.

## 6. Not verified in BUILD.md

- iOS below 16: not supported (minimum is 16.0 everywhere; the sources have no pre-16 availability checks or fallbacks). The first CI run of the minos and `-Os` checks is still to be seen.
- UI chrome differences when linked against the iPhoneOS 18.5 SDK instead of Nyxian's device SDK 26.4: untested.
- Nyxian still builds the app with `Config/Tests/` present: relies on Nyxian's public source skipping the top-level `Config/` and `Resources/` folders; the installed Nyxian version was not inspected. Do not move the tests out of `Config/` (duplicate symbols from their own `main()` and boolean stub).
- `Config/Info.plist` leaves out `UIRequiresFullScreen` and the iPad orientation keys that Nyxian's public source adds; defaults on a device not checked.
- `Config/Project.plist` (Nyxian) only had `LDEMinimumVersion` raised to 16.0 and `-Os` added to `LDECompilerFlags`, and that was not run in Nyxian; `Config/Entitlements.plist` was not changed or re-verified.

## 7. Test infrastructure and follow-ups

- Run the host tests on the macOS CI runner with each push (already in `build.yml`); add a run with `-DOST_TEST_BOOLEAN=ON` there if real Manifold should be covered on AppleClang (the default CI run uses the boolean stub; 9 Export and Meshing tests only run with it, and 1 Meshing test only runs without it, so the total goes from 1598 to 1606). There is no iOS Simulator test target and no automated UI test.
- Real `.ost` and `.osp` fixtures (none in the repo) would turn the oracle-table tests into end-to-end import tests.
- Cleanup noted by the wiring wave, no behaviour change: four near-identical "present an OK alert from the top-most presented controller" helpers in `ViewController` categories; `PlanView.mm` `isCountLike()` repeats a test in `Core/Geometry/takeoff_footprint.cpp`; functions with no caller kept by tests (`ost_store::shift_condition_ref_nos`, `ImportService +loadProjectWithUID:`, `ost_meshing::generate_takeoff_meshes`, `RenderConfig::is_ost()`); a project is loaded twice on open (preflight in `DatabaseViewController`, then `ViewController`); the edit pan hit-tests at the recognition location, not at touch-down.
- Rule for any further parity work: test first. Port the Windows test into `Config/Tests/<Area>/test_*.cpp` with the same inputs and expected values (conventions in `Config/Tests/README.md`), watch it fail on the iOS code, then fix. Never relax a tolerance to make a port pass; a failing port is an iOS bug or a Windows ambiguity to report. If a Windows behaviour looks accidental, inconsistent or unsafe, do not port it: keep the iOS behaviour and list it with the Windows file:line.
- Commit: nothing of this job is committed. The user commits.
