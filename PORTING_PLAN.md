# OST Visualizer - iOS Porting Plan

Last updated: 2026-11-15

Remaining work, in order. Facts this plan rests on (all from `PARITY_MAP.md`, Phase 1, Windows `dceb47d` vs iOS `4a1302d`):

- 216 divergences between the iOS app and the Windows app: 171 `fix`, 44 `needs-decision`, 1 `cannot-reproduce`. Every item is `open`; no Phase 3 fix has been made yet.
- Nothing has been built or run on iOS: no Xcode, macOS or device was available. Obj-C/UIKit/bgfx code was read, not compiled. The host C++ harness (`Config/Tests`) has only its own self-test and no iOS behaviour tests.
- Per-item status lives in `PARITY_MAP.md` (Phase 3 updates it). This plan has no checkboxes so it cannot go stale; current state of each area is in `PORTING_STATUS.md` and `PORTING_MAP.md`.

## 0. Phase 3: parity fixes (171 `fix` items)

Rule: test first. Port the Windows test named in `PARITY_MAP.md` into `Config/Tests/<Area>/test_*.cpp` with the same inputs and expected values (conventions in `Config/Tests/README.md`), watch it fail on iOS code, then fix. Never relax a tolerance to make a port pass; a failing port is an iOS bug or a Windows ambiguity to report.

How the 171 `fix` items can be tested (all 216 items: 108 / 95 / 13):

| Testable | Items | Meaning |
|---|---:|---|
| `core-cpp` | 83 | host unit test against `Core/` with the existing harness |
| `needs-extraction-from-objc` | 76 | logic lives in `.mm` files; move it into `Core/` first, then test |
| `objc-only` | 12 | UIKit/manual or device test |

Work in global rank order (sections A-D of `PARITY_MAP.md`), inside each owner area. Ordering constraints from the item notes:

- MSH-01 (page scale) before MSH-02 (page rotation/flip); MSH-02 also needs GEO-06 (page transform) and a page-size source.
- LP-16 (PDF page index, rotation, flip) ships with GEO-06, RPV-02, RPV-03; RPV-M04 follows from the same page transform.
- QTY-07, QTY-08, QTY-09 together (curved length); QTY-05 before the metric half of QTY-06.
- ELV-01 to ELV-06: replace `extract_z_value` in `Core/Persistence/ProjectStore/ProjectStore.cpp` in one change; patching single cases will not converge.
- IMP-03 and IMP-06 before IMP-04; BRW-05 before BRW-09 (a dangling OrigBidProjectUID becomes rare once BRW-05 is fixed).
- EXP-03 and MSH-14 share one duplicated hole-map helper (`ProjectService.mm`, `ExportService.mm`): move it into `Core/` and fix both. EXP-01 and EXP-02 start from the same Y-up mesh space but need different mappings: Three.js needs the identity (remove the swap in `threejs_writer.cpp`), DXF needs (X, -Z, Y) applied in the `exportDXF` caller while `DXFWriter` stays unchanged.
- Files edited by more than one owner area (for example `ViewController.mm`, `ProjectStore+Load.cpp`, `ExportService.mm`, `MeshFactory.cpp`) must be changed serially; the list is in the Summary of `PARITY_MAP.md`.

Prerequisite for the test-first rule: the expected values (`testPlan`) exist only in the machine-readable copy of the parity map, `divergences.json`, which is outside both repositories (Phase 1 scratchpad). Move it somewhere durable, or regenerate it, before Phase 3 starts.

### 0.1 Wrong numbers (62 fix, 4 decisions)

A number, coordinate, exported or stored value differs from Windows.

| Owner area | Fix | Top ids (rank order) | Scope |
|---|---:|---|---|
| quantities | 13 | QTY-01, QTY-04, QTY-10, QTY-06, QTY-07, QTY-08 | attachments on areas count 0, rounding never applied, area slope, metric normalization, curved length, sidebar page scope (QTY-11) |
| meshing | 12 | MSH-01, MSH-02, MSH-14, GEO-05, MSH-05, MSH-06 | page scale applied to meshes, page rotation/flip ignored, hole classification, area slope, curved walls, ellipse/triangle/rhombus/hexagon/octagon shapes (MSH-08 to MSH-11) |
| rendering-planview | 10 | RPV-03, RPV-04, RPV-08, RPV-07, RPV-06, RPV-10 | placement and drag math, 22.5 vs 15 degree snap, rounding rules, grayscale quantisation (COL-02), 3D alpha (COL-03) |
| parsers-units | 9 | ELV-01, COL-01, ELV-02, ELV-03, ELV-04, ELV-05 | elevation grammar in condition names, negative ColorFill, display rounding (FMT-01), position text (DBP-07) |
| export-formats | 5 | EXP-02, EXP-01, EXP-03, EXP-05, EXP-06 | DXF and Three.js axes, mirrored export holes, negatives per type, colour pipeline |
| import-ost-osp | 5 | IMP-01, IMP-02, IMP-03, IMP-04, IMP-05 | BidNo, backslash rewrite, UID '0', BidPageSettings, typed values |
| geometry | 3 | GEO-06, RPV-02, GEO-07 | page rotation/flip transform, canvas delta, signed area precision |
| database-persistence | 3 | DBP-04, QTY-13, DBP-03 | NULL defaults: snap increment, Curve, DisplaySize |
| conditions | 2 | QTY-12, DLG-PREC-01 | quantity and rise/run/size number formatting |

### 0.2 Data safety (41 fix, 4 decisions)

Data can be lost, overwritten, corrupted or left inconsistent.

| Owner area | Fix | Top ids (rank order) | Scope |
|---|---:|---|---|
| conditions | 10 | COND-DEL-01, COND-UPD-02, COND-UPD-01, COND-UPD-03, COND-DUP-01, COND-STYLE-01 | incomplete delete cascade, update rewrites every column and writes '' for cleared fields, save failures unreported, duplicate keeps GUID, style change wipes edits |
| import-ost-osp | 9 | IMP-06, IMP-08, IMP-07, IMP-09, IMP-12, IMP-13 | no UID remap (INSERT OR REPLACE overwrites other projects), non-atomic import, no validation, OSP image collisions, corrupt CAB |
| database-persistence | 8 | DBP-11, DBP-12, DBP-14, DBP-17, DBP-18, DBP-20 | swallowed SQL errors, orphan rows on delete, no ownership checks, UID allocation, lost nested folders |
| settings-undo-camera | 4 | UND-02, UND-03, UND-06, UND-08 | undo advances on failed write, delete does not clear history, partial restore, cascade depth |
| project-browser | 3 | BRW-01, BRW-02, BRW-05 | delete without confirmation, Deleted Bids identified by name, dangling OrigBidProjectUID |
| rendering-planview | 2 | RPV-15, RPV-M01 | children do not follow area drag, edge drag unvalidated |
| export-formats | 2 | EXP-08, EXP-09 | unescaped HTML payload, raw file name |
| geometry, layers-pages, parsers-units | 1 each | GEO-09, RPV-16, DBP-08 | degenerate polygons accepted, 2D selection survives page switch, partially numeric position tokens |

### 0.3 Behaviour (57 fix, 29 decisions)

Ordering, validation or flow differs.

| Owner area | Fix | Top ids (rank order) | Scope |
|---|---:|---|---|
| rendering-planview | 10 | LP-16, LP-15, RPV-13, RPV-17, RPV-20, RPV-25 | PDF page index/rotation/size, Image layer, attachments dropped as holes, count shapes and minimum sizes |
| conditions | 10 | COND-FOLDER-01, COND-LIST-01, COND-PLACE-01, COND-LAYER-01, COND-NAV-01, COND-VAL-01 | condition folders, row order, hidden-layer conditions, layer picker, edit navigation |
| settings-undo-camera | 8 | UND-01, UND-07, UND-09, UND-11, SET-01, SET-02 | undo bounds and grouping, default display mode, render-config saves, camera fit (CAM-01) |
| layers-pages | 5 | LP-08, LP-10, LP-11, LP-12, LP-18 | page order, folder selection, 2D page re-tap, initial page, Comments layer |
| meshing | 5 | MSH-15, MSH-20, MSH-19, EXP-13, GEO-14 | fully negated meshes stay visible, mesh order, stub union cleanup, OBJ winding, area tessellation |
| export-formats | 5 | EXP-14, EXP-11, EXP-12, EXP-16, EXP-21 | no pages selected, OBJ/DXF grouping and naming, MTL text, error messages |
| project-browser | 5 | BRW-09, BRW-13, BRW-10, BRW-12, BRW-19 | restore, bid order, trash bookkeeping, existence checks, folder names |
| database-persistence | 4 | LP-20, BRW-15, BRW-17, DBP-19 | page field loading, folder pruning and order, empty-string vs NULL |
| quantities | 2 | QTY-16, QTY-17 | UOM labels and area list, unsupported condition types |
| import-ost-osp | 2 | IMP-14, IMP-16 | OSP structure validation, image reference resolution |
| parsers-units | 1 | DLG-FINITE-01 | non-finite numeric text |

### 0.4 UI and cosmetic (11 fix, 7 decisions)

| Owner area | Fix | Ids |
|---|---:|---|
| rendering-planview | 4 | GEO-18, RPV-27, RPV-29, RPV-32 |
| conditions | 3 | COND-SHAPE-01, COND-CDNPICK-01, UI-MSG-01 |
| settings-undo-camera | 2 | CAM-03, UND-10 |
| project-browser | 1 | BRW-21 |
| layers-pages | 1 | LP-25 |

## 1. Decisions needed (44 items)

`PARITY_MAP.md` section "(E) Needs your decision" holds the 44 `needs-decision` items with the question and options for each. They stay `open` and are not touched in Phase 3 until answered (4 wrong numbers, 4 data safety, 29 behaviour, 7 UI). One more item, EXP-24 (DXF LINE order), cannot be reproduced on iOS (section F).

| Owner area | Items | Ids |
|---|---:|---|
| database-persistence | 7 | DBP-37, DBP-31, QTY-19, BRW-16, DBP-33, DBP-34, DBP-M05 |
| import-ost-osp | 7 | IMP-M01, IMP-15, IMP-17, IMP-18, IMP-19, IMP-20, IMP-M03 |
| export-formats | 7 | EXP-15, EXP-18, EXP-20, EXP-22, EXP-M02, MSH-16, EXP-19 |
| conditions | 6 | COND-METRIC-01, COND-EXTRA-01, QTY-22, COND-M08, COND-STYLE-02, QTY-23 |
| rendering-planview | 5 | RPV-05, COL-04, GEO-M02, RPV-22, MSH-21 |
| meshing | 3 | EXP-17, GEO-15, MSH-19b |
| project-browser | 3 | BRW-08, BRW-11, BRW-M01 |
| layers-pages | 2 | LP-22, LP-24 |
| settings-undo-camera | 2 | CAM-04, CAM-02 |
| parsers-units | 1 | POS-M01 |
| quantities | 1 | QTY-18 |

The 8 decisions that touch numbers or data safety: RPV-05 (grid snap gate), COL-04 (2D pattern fills and hatch rendering), DBP-37 (scale ratio for sf1 <= 0), COND-METRIC-01 (metric bids end to end), GEO-M02 (hole and attachment containment when editing a parent area), POS-M01 (non-finite position values), IMP-M01 and DBP-31 (reject or load invalid files).

## 2. Features Windows has and iOS does not (87, list only)

Source: `PARITY_MAP.md` section (H). Not scheduled. Marker `[P]` = tied to the Windows platform per the Phase 1 mappers; `[~]` = plausibly platform-only in the writer's judgement (touch or sandbox makes it unnecessary, or it needs a server stack), a product decision either way. `PORTING_MAP.md` sections 2 to 12 name the Windows modules.

| Group | Features |
|---|---|
| Export (9) | OST export (.ost); OSP export (.osp package); PDF export with takeoff and annotation markup (QPDF vs PDFKit/CoreGraphics writer unresolved); PDF annotation captions (Bluebeam-compatible); FBX export; Summary CSV export; full HTML 3D viewer (2D plan tab, visibility panel, hover tips); elevation callouts in HTML and PDF; drop condition elevation from names on export |
| Page settings (12) | Set Scale dialog and page settings bar; rename and delete page; Plan Organizer (add, duplicate, import, move pages and folders); Adjust Images (invert, bitonal); rotate and flip page image; overlay image (select, move, resize, show; fields are loaded but never drawn); page show mode (Original, Overlay, Both); multi-page PDF page index and TIFF pages; page image plane in 3D; persisted page view state and selected page; missing-file and render-progress notices; page name display options |
| Editing (20) | Cut, copy, paste, duplicate takeoffs and annotations; Paste Backout; Backout mode; rotate and flip takeoffs; slope direction handle; curved segment and control points; assign to current area; reassign condition; marquee selection and Select All; snap to PDF lines, takeoffs and right angles; zoom tool and percentage selector `[~]`; PDF text selection; complete undo/redo history; create conditions and condition folders; condition copy, cut and paste across bids; duplicate-and-reassign; Renumber Conditions; layer management and default layers; edit actions in the 3D context menu; cursor and pointer preferences `[P]` |
| Annotation (7) | Annotation drawing, persistence and styles; hotlinks; named views; takeoff condition labels on the 2D plan; detached Annotation and View windows `[~]` |
| Business (12) | Bid comparison; Cover Sheet; Summary tab; project tree editing (new, duplicate, rename, copy/cut/paste bids); job status grouping and bid lock; Employees, Payroll classes, Job statuses and Condition types master data; bid areas; metric units, dimension display and quantity formatting; quantity rounding and attachment aggregation (metric units and these two also appear in sections 0 and 1) |
| Other (8) | Count shape meshes (ellipse, triangle, rhombus, hexagon, octagon; also MSH-08 to MSH-11); 3D section cuts; 3D feature-edge overlay; saved per-bid 3D camera state; separate 2D and 3D display modes with sync toggle; tower crane visualization; multi-database registry `[~]`; pan sidebar (mini-map) `[~]` |
| AI takeoff (6) | AI takeoff analysis; proposals (scale and slab elements); changeset review, apply, undo; assumptions sidecar and audit log; AI takeoff MCP helper `[P]`; read-only MCP database server `[P]` |
| Collaboration (3) | Microsoft SQL Server backend `[~]`; multi-user collaboration engine `[~]`; SQL credentials in Windows Credential Manager `[P]` |
| Platform (10) | Access .mdb databases `[P]`; realtime sync with On-Screen Takeoff `[P]`; licensing `[P]`; update check `[P]`; About dialog `[P]`; Windows installer and file associations `[P]`; desktop menus, toolbars, status bar, shortcuts `[P]`; takeoff toolbar customization `[P]`; Options dialog and config `[P]`; Windows theme following `[P]` |

Two of these were the old plan's top blockers and are still open: round-tripping an edited project back to `.ost`/`.osp` (B-009 in `PORTING_BLOCKERS.md`) and PDF export with takeoff markup (B-004). Whether the MVP needs them is a product decision; none is started.

## 3. Validation still required (needs a Mac, CI or a device)

See B-010 (no iOS build or device run on record) and B-011 (no behaviour tests or fixtures) in `PORTING_BLOCKERS.md`.

### 3.1 First run of the CI build

`.github/workflows/build.yml` ("Build IPA", macos-15, Xcode 16.4) has never run. Details in `BUILD.md`. Check on the first run, in job order:

1. `/Applications/Xcode_16.4.app` exists on the runner image; the step fails with a listing if not.
2. Unit-test step on AppleClang with the system zlib (`cmake -S Config/Tests ...`, `ctest --no-tests=error`). Only MSVC has been used so far (harness self-test 10 of 10 pass; `Core/` (with `boolean_ops.cpp` stubbed by default) plus `Rendering/MeshLoader.cpp` compile there).
3. bgfx, bx and bimg build from the pinned commits (`BGFX_SHA`, `BX_SHA`, `BIMG_SHA`) with `make ios-arm64-release`. The generated makefiles hard-code clang flags that were verified only with Xcode 16.4 (workflow comment). First run is the slow one; check the cache is saved and a second run restores it. The pins are the commits the committed headers were taken from, per the workflow comment; not re-checked here.
4. `xcodegen generate` from `project.yml` and the unsigned Release `xcodebuild`. This is the first time Xcode compiles the Obj-C/Obj-C++ sources and links `libmanifold.a` plus the four bgfx libraries: expect to fix compile or link errors. "Built for newer iOS version" linker warnings are expected (deployment target 14.8, libraries built for 16.0).
5. Bundle checks: executable `OST Visualizer`, bundle id `com.fabian.ostvisualizer`, `MinimumOSVersion` 14.8, arm64 only, no `*.sc` in the bundle.
6. `ldid` fake-sign with `Config/Entitlements.fakesign.plist` (only `get-task-allowed`); the step fails if any entitlement key matches `memory|jetsam|extended-virtual-addressing`.
7. IPA packaging and artifact upload (`OST-Visualizer-ipa`, 30 days).

### 3.2 Install and run on a device

Not done. The IPA is fake-signed, so stock iOS refuses it; an installer that accepts fake-signed IPAs (TrollStore is one example in `BUILD.md`) is needed. Then:

- Launch, import a real `.ost` and a real `.osp`. No sample `.ost`/`.osp` files were found in either repository (`Config/Tests/Fixtures` holds only `.gitkeep`), so fixtures must be supplied: pages, layers and folders; linear, area, count and attachment conditions; holes, negatives, slope, curves; PDFs and overlays; metric bids; annotations.
- Rendering on device: bgfx/Metal start-up, opaque and OIT passes, mesh-ID picking and readback, selection highlight, orbit/pan/pinch, fit and reset; memory and frame-time baseline.
- PDF drawing (CoreGraphics) including page rotation; takeoff placement, drag, undo on touch; iPad popovers.
- OBJ, DXF and HTML export through the share sheet; export write failures; empty and large projects; import errors and cancellations.
- iRAM+ still recognises the app after a reinstall (bundle id and executable name unchanged between builds).

### 3.3 Not verified in `BUILD.md`

- iOS below 16 (libraries target 16.0, app 14.8): untested.
- UI chrome differences when linked against the iPhoneOS 18.5 SDK instead of Nyxian's device SDK 26.4: untested.
- Nyxian still builds the app with `Config/Tests/` present: relies on Nyxian's public source skipping the top-level `Config/` and `Resources/` folders; the installed Nyxian version was not inspected. Do not move the tests out of `Config/` (duplicate symbols from their own `main()` and boolean stub).
- `Config/Info.plist` leaves out `UIRequiresFullScreen` and the iPad orientation keys that Nyxian's public source adds; defaults on a device not checked.
- `Config/Project.plist` and `Config/Entitlements.plist` (Nyxian) were not changed or re-verified.

### 3.4 Test infrastructure

- Add the per-area differential tests of Phase 3 under `Config/Tests/<Area>/`; today there are none. Boolean-operation tests need `-DOST_TEST_BOOLEAN=ON` (downloads Manifold v3.0.0).
- The 76 `needs-extraction-from-objc` items need their logic moved into `Core/` before they can be tested on the host; the 12 `objc-only` items need device or manual checks.
- Run the host tests on the macOS CI runner with each push (already in `build.yml`); no iOS Simulator test target exists.
