# OST Visualizer - iOS Porting Status

Last updated: 2026-11-19

Basis: iOS repo, branch `ios-parity-sync`, HEAD `42b46c3` (level with `origin/ios-parity-sync`) plus the uncommitted work of the parity job (2026-11-15 to 2026-11-18) and of the decisions job of 2026-11-19: the Core fixes, the Objective-C wiring, the host tests under `Config/Tests/`, `PARITY_DIVERGENCES.json` and these documents. Nothing of it is committed (the user commits). Windows reference: `OSTVisualizerLicense` HEAD `6d4786f` (2026-11-17), working tree clean (checked read-only again on 2026-11-19), read-only; that commit holds the same 18 paths that were uncommitted in the Windows tree at the 2026-11-16 re-baseline (the export boolean rule and the drop-stub fallback). **Nothing was built, signed or run on iOS in this job.** The authoring machine has no Xcode, macOS, xcodebuild, XcodeGen, ldid, simulator or device. What was run is stated under "What was verified and how"; what was not is under "Not verified".

## Summary

- The iOS app has an end-to-end import, browse, view, edit and export path in source: `.ost`/`.osp` import into SQLite, project browser, bgfx/Metal 3D view, 2D plan view over a CoreGraphics PDF page, takeoff placement/drag/delete with undo, condition editing, layer visibility, OBJ/DXF/Three.js export.
- This job brought the existing features in line with the Windows app. All **220** parity records (216 from the Phase 1 map plus 4 from the 2026-11-16 re-baseline) now have a final status, none is open: **172 fixed**, **25 fixed-ui-unverified** (Core part tested, the UIKit/CoreGraphics part wired but never seen running), **21 wont-fix** (user decision: iOS behaviour kept), 1 cannot-reproduce, 1 no-longer-divergent. Nothing is blocked any more: the 8 items that were `blocked-needs-decision` on 2026-11-18 (DBP-04, BRW-09, EXP-12, EXP-13, EXP-16, IMP-20, LP-24, RPV-29) were decided on 2026-11-19 and are wont-fix. Detail: `PARITY_MAP.md`, `PARITY_DIVERGENCES.json`.
- All **44 of 44** earlier user decisions are recorded (33 answered "port", 11 "keep"; GEO-15 and EXP-17 chose the option that keeps outward-facing meshes and ended as wont-fix). On **2026-11-19** the user decided every formal item and every candidate: **25 decisions**, 21 that keep the iOS behaviour (no code change) and 4 code changes (see "Decisions of 2026-11-19").
- The 4 code changes of 2026-11-19, each with host tests: COND-STYLE-02 and COND-ELEV-01 deviate from the literal Windows behaviour on purpose (metric type defaults on a style change in a metric bid; is_top and z_value derived from the saved name); DLG-FINITE-01 (whitespace-only Rise and Run refused like Windows) and UND-09 (Count as Negative Quantity changes only the pressed takeoff, in one undoable entry) follow Windows. For every kept item that looks like a Windows defect the Windows file:line was researched read-only: 18 records, listed under "Possible Windows defects".
- Tests: before the parity job the host harness held 10 self-tests and **0 iOS behaviour tests**. Now `Config/Tests` holds **1598 tests** (10 self-tests + 1588 behaviour tests) in 12 areas; 1598 of 1598 pass on MSVC and on a clang build with fused multiply-add, 1606 of 1606 with `-DOST_TEST_BOOLEAN=ON` (real Manifold). The job of 2026-11-19 started from 1576 (1584 with Manifold) and added 22 tests (Conditions +16, Settings +6). Per-area counts below.
- The Objective-C/Objective-C++ layer (27 files) was only compiled with `-fsyntax-only` against hand-written stub headers: 27 of 27 files, 0 errors, 4 baseline warnings. That is not a build against the real SDK.
- Build state: the GitHub Actions run of 2026-11-16 (the only one so far, on the code pushed on 2026-11-15) failed in the host unit-test step (one Meshing test), so every later step (bgfx build, xcodegen, xcodebuild, ldid, IPA) has never run. The fix was checked on the host only. The first full CI run on the new code is the next thing to look at.
- Not implemented: OST/OSP export, PDF/annotation export, annotations, TIFF page images, most Windows editing tools, bid business workflows (list below).
- Still open (not decided by the user): 20 entries, 3 questions (the condition number above the int range and two COND-PLACE-01 notes), 10 notes for information and 7 code-hygiene notes; see "Still open". First full GitHub run and the device checks: `PORTING_PLAN.md`.
- Open items: `PORTING_PLAN.md`. Decisions, possible Windows defects and what only GitHub/macOS/a device can verify: `PORTING_BLOCKERS.md`.

## What each phase delivered

| Phase | When | Delivered | State |
|---|---|---|---|
| 0: CI/IPA pipeline and host harness | 2026-11-15 (commits `a950f9a`, `c73b6d0`) | `project.yml` (XcodeGen, one iphoneos arm64 app target), `.github/workflows/build.yml` (macos-15, Xcode 16.4: host unit tests, libmanifold arm64 check, plist lint, bgfx/bx/bimg from pinned SHAs, minos check of the five prebuilt libraries, xcodegen, xcodebuild unsigned Release, bundle checks, `ldid` fake-signing with `Config/Entitlements.fakesign.plist`, IPA upload), `BUILD.md`, `Config/Info.plist`, `Config/Tests/` (CMake host harness with `TestHarness.hpp`, 10 self-tests). On 2026-11-16 the user decisions set minimum iOS 16.0 and `-Os` (`GCC_OPTIMIZATION_LEVEL: s`) everywhere and kept the `Info.plist` additions (empty `UILaunchScreen`, iPhone and iPad). | Pushed. First CI run 2026-11-16 stopped at the unit-test step (see "Not verified"). |
| 1: parity map | 2026-11-15, re-baseline 2026-11-16 | 12 area mappers and 12 verifiers compared iOS with the Windows app (iOS C++ compiled with MSVC and compared with the Windows Python/native modules; Obj-C read). 369 reported items merged into 216 divergences; the re-baseline against the changed Windows tree added 4 (RB-01 to RB-04: one export boolean rule, no-surviving-mesh failure, post-boolean-only writers, drop-stub union failure) = 220. `PARITY_MAP.md` (human) and `PARITY_DIVERGENCES.json` (machine, with test plans and the recorded decisions). All 44 `needs-decision` items were answered on 2026-11-16. | Committed (map, `42b46c3`); final statuses added in this job. |
| 2: documentation | 2026-11-15, synced 2026-11-18 and 2026-11-19 | `PORTING_MAP.md`, `PORTING_PLAN.md`, `PORTING_STATUS.*`, `PORTING_BLOCKERS.md` and `PARITY_MAP.md` were rewritten from the code on 2026-11-15; on 2026-11-18 the status documents were rewritten again to match the final results of the fix waves, and on 2026-11-19 they were synced with the decisions, the four code changes, the possible Windows defects and the new counts. | Phase 2 committed (`42b46c3`); the syncs of 2026-11-18 and 2026-11-19 are uncommitted. |
| 3a: Core fixes (host-testable C++) | 2026-11-15 to 2026-11-17 | Test first: each Windows test named in the map was ported to `Config/Tests/<Area>` with the same inputs and expected values, watched fail on the iOS code, then fixed. 12 test areas (counts below). New Core modules hold logic that used to live in `.mm` files (page transform and page view, placement and drag rules, takeoff footprint, hole grouping, condition choices, form rules, style change, dimension and number formatting, export text and file names, export mesh sets, OSP package rules, store integrity checks, browser rules, undo history, camera framing). Wave 1 (2026-11-15, commit `42ad10d`) was stopped early; the rest was finished on 2026-11-16/17 after the re-baseline. | Uncommitted (except wave 1). The host tests were green at the end of this wave; the final counts are under "What was verified and how". |
| 3b: Objective-C wiring wave | 2026-11-17 and 2026-11-18 | Six clusters wired the Core results into the platform layer: U1 services (`ProjectService`, `ExportService`, export UI), U2a placement and drag numerics, U2b plan view visuals (page transform, holes, handles), U3 `ViewController` (page switching, undo, camera, renderer settings), U4 conditions UI, U5 import and browser. 108 wiring entries for 57 item ids, 108 done, 0 open (final audit). COND-PLACE-01, the last open hand-off (layer hide/show against an active placement), was finished at the end. | Uncommitted. `.mm` only syntax-checked. |
| 4: close-out of the open blockers | 2026-11-19 | The user decided every formal blocked item and every candidate (25 decisions: 21 keep iOS, 4 code changes). Code changes with host tests: COND-STYLE-02 (metric type defaults on a style change in a metric bid), COND-ELEV-01 (is_top and z_value derived from the saved name), DLG-FINITE-01 (whitespace-only Rise and Run refused like Windows) and UND-09 (Count as Negative Quantity changes only the pressed takeoff). The 8 former blocked items are wont-fix, the DBP-33 decision text was corrected, and the Windows file:line of every possible Windows defect was researched read-only. Host tests 1576 -> 1598 (1584 -> 1606 with Manifold). | Uncommitted. Host-tested on MSVC and clang+FMA; the three `.mm` changes are only syntax-checked. |
| Independent review | throughout | Every fix cluster in 3a and 3b was followed by a separate reviewing agent. Reviewers re-ran the tests on MSVC and on the clang+FMA build, wrote their own differential drivers against the Windows Python/native modules, built mutation variants of the changed Core code to check that the new tests catch them, and sent findings back for rework (the `-pass2` reports). A final global verification and wiring audit re-read all 220 records and all 108 wiring entries. The Windows repo was checked clean before and after each wave. The changes of 2026-11-19 were reviewed as described under "The four code changes". | Review is by agents; there was no human code review and no device test. |

## Status vocabulary

| Status | Meaning |
|---|---|
| Implemented-matches-Windows | Every parity id of the system is fixed (or closed as no-longer-divergent / cannot-reproduce) and its Core logic is covered by host tests. The platform wiring is syntax-checked only; ids whose UI part needs a device check are listed in the last column. |
| Implemented-matches-Windows-except-decisions | As above, but at least one id is `wont-fix` (user decision: iOS keeps its own behaviour on purpose) or carries a recorded keep-iOS decision (the Windows behaviour was not ported). |
| Implemented-unverified | Present; no run was possible. |
| Replaced | Platform replacement of a Windows technology. |
| Not implemented | No iOS code (verified by search). |
| Obsolete | Windows-only concern with no iOS equivalent needed. |

Item statuses (`PARITY_DIVERGENCES.json`): **fixed** = Core logic fixed and host-tested, wiring done where the item needed it; **fixed-ui-unverified** = the same, and part of the result is UIKit/CoreGraphics behaviour that has only been syntax-checked; **wont-fix** = closed by a user decision (the 8 items that were `blocked-needs-decision` on 2026-11-18 were decided on 2026-11-19); **blocked-needs-decision** = no item has this status any more; **cannot-reproduce**; **no-longer-divergent**.

Evidence levels: **host-run** = the iOS `Core/` C++ compiled and tested on the host (MSVC, and clang with fused multiply-add) against expected values taken from the Windows tests or from running the Windows Python/native modules read-only. **syntax-check** = the `.m`/`.mm` files compiled with `-fsyntax-only` against hand-written stubs. **read** = both sides read. **ci** = one GitHub run, stopped at the unit tests. **device** = none.

## Implemented systems

| System | Status | iOS evidence | After this job | Open ids (decision / kept) and UI check |
|---|---|---|---|---|
| App shell and URL routing | Replaced | `Main.m`<br>`AppDelegate.m`<br>`SceneDelegate.mm`<br>`Config/Info.plist` | n/a (Windows is Python/Qt). | - |
| Import .ost | Implemented-matches-Windows-except-decisions | `Services/ImportService.mm (importOSTFile)`<br>`Core/Import/OstXmlParser.cpp`<br>`Core/Import/*` (typed values, validation, transforms)<br>`Core/Persistence/ProjectStore/Components/ProjectStore+Import*.cpp` | Windows import rules ported and host-tested: BidNo, backslash rule, UID '0', page settings, typed values, UID remap, atomic import, validation, source-UID fields, employees and pay classes. | wont-fix: IMP-20 (DATETIME overflow stores NULL, UTF-8 annotation names, TEXT memo and IMAGE: kept) |
| Import .osp | Implemented-matches-Windows-except-decisions | `Services/ImportService.mm (importOSPFile)`<br>`Core/Import/CabExtractor.cpp`<br>`Core/Import/OspPackage.cpp`<br>`Core/Import/CabMemberName.cpp` | Own CAB reader (stored and MSZIP via zlib). Package structure, image reference resolution, staging without overwriting and abort-on-write-failure follow the Windows importer; member names are decoded like Windows. | wont-fix: IMP-17 (no LZX/Quantum); kept: IMP-12 (a staging destination that is a directory counts as different content) |
| SQLite schema and seed rows | Implemented-matches-Windows-except-decisions | `Core/Persistence/OstSchema.cpp`<br>`Core/Persistence/ProjectStore/ProjectStore.cpp (create_schema)`<br>`Core/Persistence/OstDatabase.cpp` | Phase 1 compared table and column names by script: the 64 Windows tables, same column order, same seed rows (26 SchemaRegistry versions, four default layers); iOS adds `_AppMeta` and `_AppPreferences` (66 tables). Since then DBP-33 added the Windows column DEFAULT values to the DDL; the script comparison was not re-run after that. | wont-fix: DBP-34 (mixed 1 / -1 booleans), IMP-20; kept: DBP-33 (NameFontColor = 0 is also written explicitly on insert) |
| Store behaviour (load, write, delete, cascades) | Implemented-matches-Windows-except-decisions | `Core/Persistence/ProjectStore/ProjectStore.cpp`<br>`Core/Persistence/ProjectStore/Components/ProjectStore+{Import,Load,Folders,Takeoffs,Conditions,Integrity}.cpp` | Failed SQL now propagates (DBP-11), cascades and ownership checks follow Windows, UIDs are allocated like Windows, nested page folders load, load refuses invalid data (DBP-31, BRW-16). | wont-fix: DBP-04 (a stored increment of 0 keeps snapping off), DBP-M05; UI check: DBP-31, BRW-16 |
| Project browser | Implemented-matches-Windows-except-decisions | `Controllers/DatabaseViewController.mm`<br>`Core/Domain/BrowserRules.cpp`<br>`Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp` | Folder and bid order, trash, restore, Deleted Bids identity (UID '1'), delete confirmation and folder rules follow Windows. | wont-fix: BRW-09 (restore without the original folder restores the bid unassigned); UI check: BRW-02, BRW-08 |
| Project and edit persistence (what is written) | Implemented-matches-Windows-except-decisions | `Services/ImportService.mm`<br>`ProjectStore+Takeoffs.cpp`<br>`ProjectStore+Conditions.cpp`<br>`ProjectStore+Folders.cpp` | Takeoff position text, insert, delete, negative flag; condition update (changed columns only, NULL for cleared fields), duplicate, delete with full cascade, RefNo shift; layer Show; folders and trash; render config (`_AppPreferences`). No page write, layer insert/rename/delete, condition create or annotation write exists (searched). | wont-fix: DBP-34; kept: COND-UPD-02 (a NULL RefNo counts as not the new number); UI check: COND-UPD-03 |
| Domain model (loaded subset) | Implemented-matches-Windows-except-decisions | `Core/Domain/*`<br>`Core/Data/ProjectData.hpp`<br>`ProjectStore+Load.cpp` | Loads Bids, BidProjects (name), BidPages, BidLayers, CdnTypes, BidConditionFolders, BidConditions, BidTakeoffs, BidPageFolders and, for the saved selected page (LP-12), BidSettings. Other tables are stored but not modelled (annotations, areas, cover-sheet data, named views, hot links, employees). | wont-fix: DBP-04, DBP-M05, COND-EXTRA-01; kept: COND-FOLDER-01 (type name '' for a missing type); UI check: DBP-31 |
| Elevation and position text parsing | Implemented-matches-Windows | `Core/Domain/PositionParser.cpp (parse_position, extract_z_value_from_name)`<br>`Core/Domain/{DimensionFormat,NumberText}.cpp`<br>`Controllers/EditConditionViewController/Components/EditConditionHelpers.mm` | The Windows elevation grammar replaces the hand-written parser (ELV-01 to ELV-06, host-tested against oracle tables); position text is written and parsed like Windows (DBP-07, DBP-08). A whitespace-only Rise or Run is refused like Windows (DLG-FINITE-01, 2026-11-19); the condition number above the int range is still open. | UI check: POS-M01 |
| Quantities | Implemented-matches-Windows | `Core/Domain/UomService.hpp`<br>`Core/Domain/QuantityPages.hpp`<br>`Core/Domain/QuantityDisplay.cpp`<br>`Controllers/ConditionsViewController.mm`<br>`Controllers/ViewController/ViewController.mm (_updateConditionQuantities)` | Attachment aggregation, rounding (`round_quantity` / `round_up`), metric UOM codes 11-16, curved length, slope factor, page scope and number formatting follow Windows (QTY-01 to QTY-19, QTY-22, QTY-23). | UI check: QTY-15 |
| Mesh pipeline (scale, rotation/flip, ordering) | Implemented-matches-Windows | `Services/ProjectService.mm (generateMeshesIntoRenderer)`<br>`Core/Meshing/TakeoffMeshBuilder.cpp`<br>`Core/Meshing/MeshFactory.cpp` | Meshes are built in raw OST units with the page rotation and flip applied; hole classification and mesh order follow Windows (MSH-01, MSH-02, MSH-14, MSH-20). The Obj-C service calls the Core builder. | - |
| Linear wall meshes (straight, rise/run slope, drop stub) | Implemented-matches-Windows | `Core/Meshing/gen_linear.cpp`<br>`Core/Meshing/mesh_helpers.hpp` | Matches at generator level; the drop-stub union is a boolean step that keeps the plain wall and logs a warning when the union fails (MSH-19b, RB-04). | - |
| Curved linear wall meshes | Implemented-matches-Windows | `Core/Meshing/gen_curved.cpp` | Segment count, arc side and straight-chord rule follow Windows (MSH-05 to MSH-07). The clang build with FMA is part of the evidence (the CI failure of 2026-11-16 was in this area). | - |
| Area prism meshes (earcut, holes, slope) | Implemented-matches-Windows-except-decisions | `Core/Meshing/gen_area.cpp`<br>`Core/Geometry/earcut.hpp`<br>`Core/Geometry/earcut_impl.cpp` | Slope direction (GEO-05) and tessellation frame (GEO-14) follow Windows. Face winding stays outward-facing by decision. | wont-fix: GEO-15 |
| Count and attachment meshes | Implemented-matches-Windows | `Core/Meshing/gen_count.cpp`<br>`Core/Meshing/MeshFactory.cpp (create_count)` | All eight shapes follow Windows (table below; MSH-08 to MSH-11, MSH-13). | - |
| Boolean operations (Manifold) | Implemented-matches-Windows | `Core/Geometry/boolean_ops.cpp`<br>`Libs/Manifold/ (headers, lib/libmanifold.a)`<br>`Services/ProjectService.mm`<br>`Services/ExportService.mm`<br>`Core/Meshing/ExportMeshSet.cpp` | One rule for the 3D view and all exports, as in the Windows tree at `6d4786f`: a negative cuts only positives of the same condition type, a fully consumed positive is dropped, an export without a surviving mesh fails before writing (MSH-14 to MSH-16, MSH-19, MSH-19b, EXP-05, RB-01 to RB-04). Host tests with real Manifold need `-DOST_TEST_BOOLEAN=ON` (1606 pass). | - |
| 3D renderer | Replaced | `Rendering/Renderer.mm`<br>`Rendering/BgfxView.mm`<br>`Rendering/MeshLoader.cpp`<br>`Rendering/Shaders/generated/*.h (8 headers)`<br>`Rendering/MathUtils.hpp` | Replaced technology (Windows: OpenGL/Qt). Lighting constants and directions are the Windows ones; grayscale and alpha rules follow Windows (COL-02, COL-03). Never run on a GPU or device. | wont-fix: RPV-29 (normal flip kept), MSH-21 (flat shading kept) |
| 2D plan view and PDF background | Implemented-matches-Windows-except-decisions | `PlanView/PlanView.mm`<br>`PlanView/Components/PDFCanvasView.mm`<br>`PlanView/Components/TakeoffOverlayView.mm`<br>`PlanView/Components/PlanView+Placement.mm`<br>`Core/Geometry/page_transform.hpp`<br>`Core/Geometry/{page_view,takeoff_footprint,coord_transform}.cpp` | PDF page index, page size, rotation and flip, attachments and holes in 2D and handle visuals follow Windows; the minimum interactive sizes use the constant `kInteractivePdfRenderScale`, accepted on 2026-11-19 (RPV-26), and the negative marker anchor and position stay as decided (RPV-27). Overlay image fields are still loaded but not drawn. | wont-fix: RPV-22 (no curved placement, no centre handle); kept: RPV-26, RPV-27; UI check: LP-16, GEO-06, RPV-17, RPV-31, RPV-32 |
| Condition list and editor | Implemented-matches-Windows-except-decisions | `Controllers/ConditionsViewController.mm`<br>`Controllers/EditConditionViewController/*`<br>`Core/Domain/{ConditionChoices,ConditionFormRules,ConditionStyleChange,ConditionTree,DimensionFormat}.cpp` | List order and folders, edit form rules (a whitespace-only Rise or Run is refused), layer and type pickers, style change (a metric bid persists the metric type defaults: deliberate deviation from the literal Windows behaviour), trim/curved exclusion, metric bids, the elevation derived from the saved name so memory equals a reload (deliberate deviation) and save-failure reporting follow Windows; iOS-only Connect / Tolerance / Snap-to-Linear controls kept by decision. Condition create and condition-folder CRUD do not exist. | wont-fix: COND-EXTRA-01; kept: COND-UPD-02, COND-FOLDER-01; deviates from Windows on purpose: COND-STYLE-02, COND-ELEV-01; UI check: COND-PLACE-01, COND-LAYER-01, COND-TRIM-01, UI-M01, COND-UPD-03, COND-STYLE-02, COND-ELEV-01 |
| Layers and page selection | Implemented-matches-Windows-except-decisions | `Controllers/ViewController/Components/ViewController+Layers.mm`<br>`Controllers/ViewController/Components/ViewController+PageSelector.mm`<br>`Core/Data/PageNavigation.cpp`<br>`Services/ProjectService.mm` | Page order, folder selection, 2D re-tap, initial page, Comments layer and layer merge follow Windows; hiding a layer suspends an active placement and showing it restores it (COND-PLACE-01). | wont-fix: LP-22, LP-24 (Show All / Hide All change only the bid's own layers); kept: RPV-16; UI check: RPV-16 |
| OBJ, DXF and Three.js HTML export | Implemented-matches-Windows-except-decisions | `Services/ExportService.mm`<br>`Core/Export/{obj_writer,dxf_writer,threejs_writer,export_text,export_filename}.cpp`<br>`Core/Meshing/ExportMeshSet.cpp`<br>`Controllers/ViewController/Components/ViewController+Export.mm` | Axes, holes, per-condition groups and naming, colours with grayscale and opacity, escaped HTML payload, file names, nothing-to-export handling and the Three.js shading constants follow Windows. The HTML viewer imports `three@0.150.0` from unpkg.com, so viewing it needs network. | wont-fix: EXP-12, EXP-13, EXP-15, EXP-16, EXP-17, EXP-18, EXP-M02; kept: EXP-09, EXP-21 (iOS wording); UI check: EXP-21 |
| Undo and redo | Implemented-matches-Windows-except-decisions | `Controllers/ViewController/Components/UndoRedoController.mm`<br>`Core/Domain/UndoHistory.hpp` | History bounded to 50 entries like Windows, one entry for a grouped move and one for Count as Negative Quantity, which changes only the pressed takeoff (UND-09, decided 2026-11-19), failed writes do not advance history, deleting a condition clears the history, undo of a delete selects the restored takeoff. Condition, layer and folder edits are not undoable (Windows has no undo for them either). | kept: UND-02 (the batch insert validates in list order); UI check: UND-03, UND-07, UND-09, UND-10 |
| 3D camera | Replaced | `Controllers/ViewController/Components/CameraController.mm`<br>`Core/Domain/{CameraFraming,CameraLimits}.hpp`<br>`Rendering/Renderer.mm (fitCameraToMeshes)` | Platform-adapted gestures (one-finger orbit, pan-mode toggle on the floating button, pinch zoom); fit/reset framing and zoom limits use the Windows constants (CAM-01, CAM-03, CAM-04). | wont-fix: CAM-02 (refit on first selection kept) |
| Placement, drag and snapping | Implemented-matches-Windows-except-decisions | `PlanView/Components/PlanView+Placement.mm`<br>`Controllers/ViewController/Components/DragController.mm`<br>`Core/Geometry/{placement_snap,drag_rules,geom_utils}.cpp`<br>`Core/Domain/Bid.hpp (snap_increments_ost)` | 15 degree angle snap, length-only grid snap gated by threshold, half-to-even rounding, group drag and area edge-handle rules follow Windows. No endpoint, midpoint, PDF-line or right-angle snapping exists. | wont-fix: DBP-04 (a stored increment of 0 keeps snapping off); kept: GEO-M02 (polygon validity in OST coordinates); UI check: RPV-M07 |
| Render settings (colour mode, grayscale) | Implemented-matches-Windows | `Core/Domain/RenderConfig.hpp`<br>`ProjectStore+Conditions.cpp (load_render_config, save_render_config)`<br>`Controllers/DatabaseViewController.mm (_settingsTapped)`<br>`ViewController.mm (_applyRenderConfig)` | Modes Solid / Original / Transparent (default Original, invalid stored values become the default) plus grayscale, stored in `_AppPreferences`; negative ColorFill and alpha rules follow Windows. One mode for 2D and 3D (Windows has separate 2D and 3D modes: not implemented). | - |
| Build, CI and tests | Implemented-unverified | `project.yml`<br>`.github/workflows/build.yml`<br>`BUILD.md`<br>`Config/Info.plist`<br>`Config/Entitlements.fakesign.plist`<br>`Config/Tests/`<br>`Config/Project.plist`<br>`Config/Entitlements.plist` | Host tests run on MSVC and clang. The workflow got as far as the unit-test step once (2026-11-16). XcodeGen, xcodebuild, ldid and the bgfx build have never run. | see "Not verified" |

Notes (facts verified in code):

- **App shell.** UIKit lifecycle. Root is `DatabaseViewController` in a `UINavigationController`. `.ost`/`.osp` URLs (cold start and `openURLContexts`) go to `ImportService` and a failure shows an alert. `Info.plist` declares both document types.
- **Store.** One database file, `Documents/ost_projects.sqlite` (`ImportService.databaseURL`).
- **3D renderer.** bgfx on Metal (`RendererType::Metal`), `CAMetalLayer` view, opaque pass plus weighted-blended OIT (accum, revealage, composite), mesh-ID pick pass with readback, selection mask and post pass. Read only; no GPU or device check of rendering, OIT, picking readback or frame time.
- **2D plan view.** `PDFCanvasView` takes the PDF page index and page size from Core (`page_view`) and draws the page with rotation and flip; the overlay image fields are loaded and rewritten on OSP import but never drawn. Display Name / Display Dimension flags are editable and persisted but not drawn.
- **Export.** Writes into `NSTemporaryDirectory` and shares with `UIActivityViewController`; a nothing-to-export result and a write failure show different alerts (wording differs from Windows: EXP-21, kept by decision of 2026-11-19).
- **Undo.** `UndoAction` covers move, group move, place, delete and set-negative.

### Count and attachment shapes actually generated

Shape ids match Windows (`Core/Domain/Types.hpp`). Attachments use the same path as count conditions (`MeshFactory.cpp`, `create_count`). All rows are host-tested against the Windows mesh factory oracle cases.

| Shape (id) | iOS mesh | Matches Windows? | PARITY_MAP ids |
|---|---|---|---|
| Square (0) | box, depth = width | Yes | - |
| Rectangle (3) | box, width x depth (a negative depth becomes 1.0 like Windows) | Yes | MSH-13 |
| Circle (1) | 24-segment cylinder, diameter = width | Yes | - |
| Ellipse (13) | elliptical cylinder (depth is the second radius) | Yes | MSH-08 |
| Triangle (2) | isosceles triangle prism | Yes | MSH-09 |
| Rhombus (12) | rhombus prism | Yes | MSH-10 |
| Hexagon (14) | polygon prism | Yes | MSH-11 |
| Octagon (15) | polygon prism | Yes | MSH-11 |

## Parity items: final status

Source: `PARITY_DIVERGENCES.json` (machine) and `PARITY_MAP.md` (human). 220 records: 216 from the Phase 1 map plus RB-01 to RB-04. After the 44 decisions of 2026-11-16 and before the fixes the dispositions were 205 fix, 13 wont-fix, 1 no-longer-divergent, 1 cannot-reproduce. At the end of 2026-11-18 the status was 174 fixed, 23 fixed-ui-unverified, 13 wont-fix, 8 blocked-needs-decision, 1 cannot-reproduce, 1 no-longer-divergent; the decisions of 2026-11-19 moved 8 items from blocked-needs-decision to wont-fix and 2 items from fixed to fixed-ui-unverified. None is open now.

| Final status | Count |
|---|---:|
| fixed | 172 |
| fixed-ui-unverified | 25 |
| wont-fix (user decision) | 21 |
| blocked-needs-decision | 0 |
| cannot-reproduce (EXP-24) | 1 |
| no-longer-divergent (MSH-19) | 1 |
| **total** | **220** |

By owner area:

| Owner area | fixed | fixed-ui-unverified | wont-fix | other | total |
|---|---:|---:|---:|---|---:|
| quantities | 15 | 1 | 0 | 0 | 16 |
| geometry | 3 | 1 | 0 | 0 | 4 |
| meshing | 18 | 0 | 3 | 1 (no-longer-divergent) | 22 |
| parsers-units | 11 | 1 | 0 | 0 | 12 |
| import-ost-osp | 21 | 0 | 2 | 0 | 23 |
| export-formats | 15 | 1 | 5 | 1 (cannot-reproduce) | 22 |
| database-persistence | 17 | 2 | 3 | 0 | 22 |
| conditions | 23 | 7 | 1 | 0 | 31 |
| layers-pages | 6 | 1 | 2 | 0 | 9 |
| rendering-planview | 23 | 5 | 3 | 0 | 31 |
| project-browser | 9 | 2 | 1 | 0 | 12 |
| settings-undo-camera | 11 | 4 | 1 | 0 | 16 |
| **total** | **172** | **25** | **21** | **2** | **220** |

(The meshing and export-formats rows include the four re-baseline records RB-01 to RB-04 under fixed.)

By impact:

| Impact | fixed | fixed-ui-unverified | wont-fix | other | total |
|---|---:|---:|---:|---:|---:|
| wrong-numbers | 63 | 2 | 1 | 0 | 66 |
| data-safety | 38 | 7 | 0 | 0 | 45 |
| behavior | 61 | 14 | 14 | 1 | 90 |
| ui-cosmetic | 10 | 2 | 6 | 1 | 19 |
| **total** | **172** | **25** | **21** | **2** | **220** |

Ids by status:

- **fixed-ui-unverified (25):** GEO-06, QTY-15, BRW-02, UND-03, COND-UPD-03, RPV-16, COND-ELEV-01, POS-M01, DBP-31, LP-16, COND-PLACE-01, COND-LAYER-01, COND-TRIM-01, RPV-17, RPV-31, UND-07, UND-09, RPV-M07, EXP-21, BRW-08, UI-M01, BRW-16, COND-STYLE-02, RPV-32, UND-10.
- **wont-fix (21):** DBP-04, EXP-13, EXP-12, EXP-16, BRW-09, EXP-15, COND-EXTRA-01, LP-22, EXP-17, EXP-18, EXP-M02, GEO-15, IMP-17, IMP-20, RPV-22, RPV-29, CAM-02, DBP-34, DBP-M05, LP-24, MSH-21.
  - decided on 2026-11-19 (formerly blocked-needs-decision): DBP-04, BRW-09, EXP-12, EXP-13, EXP-16, IMP-20, LP-24, RPV-29.
- **blocked-needs-decision (0):** none; the 8 items that had this status on 2026-11-18 are wont-fix.
- **cannot-reproduce:** EXP-24 (DXF LINE order is the iteration order of `std::unordered_set` of the STL in use).
- **no-longer-divergent:** MSH-19 (the degenerate-face part is in place; the union-failure part is MSH-19b, fixed).

The 44 decisions of 2026-11-16 ended as: 24 fixed, 5 fixed-ui-unverified, 15 wont-fix (the 13 of 2026-11-18 plus IMP-20 and LP-24, where one sub-point was not ported and which were decided on 2026-11-19), 0 blocked. COND-STYLE-02, one of the 44, moved from fixed to fixed-ui-unverified with the change of 2026-11-19.

## What was verified and how

All numbers below come from fresh runs on 2026-11-19 at the end of the decisions job (MSVC and clang, every run with 0 failures; the syntax check ran on the working tree of that day). The host runs were not repeated while these documents were written.

| Check | Result | What it proves | What it does not prove |
|---|---|---|---|
| Host unit tests, MSVC (Debug, Ninja), all 12 areas | **1598 of 1598 pass** | Core C++ logic against expected values from the Windows tests / oracle runs | arm64, AppleClang, the real `.mm` layer |
| Host unit tests, clang 22 (zig toolchain, x86-64 with FMA3, `-Os`, `-ffp-contract=on`, Debug flags like CI) | **1598 of 1598 pass** | Stability of the numeric results under fused multiply-add, the cause of the CI failure of 2026-11-16 | It is not AppleClang on arm64; it approximates it |
| Same, with `-DOST_TEST_BOOLEAN=ON` (real Manifold v3.0.0) | **1606 of 1606 pass** on clang and on MSVC | Boolean-dependent export and mesh tests | Manifold as linked into the app on arm64 (`libmanifold.a`) |
| Obj-C syntax check (`-fsyntax-only` of every platform `.m`/`.mm`, ARC, C++17, same include paths and defines as the build, against hand-written stub Foundation/UIKit/QuartzCore/CoreGraphics headers) | **27 of 27 files, 0 errors, 4 warnings** (baseline protocol warnings in `EditConditionViewController`) | Typos, wrong Core call signatures, missing includes and selectors (against the stubs). It already found 4 compile errors present at HEAD (Core-call signature drift in the drag, view controller and plan view code), now fixed | Anything about the real Apple SDK, ARC runtime behaviour, layout or UI. The stubs and the checker are not in the repo. Target is macOS (zig ships macOS headers), so `TARGET_OS_IPHONE` is 0; the sources contain no such conditionals |
| Windows repo | working tree clean at `6d4786f` before and after; checked read-only again on 2026-11-19 (HEAD `6d4786f`, no changed or untracked file) | The Windows source of truth was only read | - |

Per-area host tests (MSVC and clang, same numbers; `-Boolean` adds 5 in Export and 3 in Meshing):

| Area | Tests |
|---|---:|
| Browser | 67 |
| Conditions | 174 |
| Export | 97 (102 with -Boolean) |
| Formatting | 74 |
| Geometry | 171 |
| Harness (self-tests only) | 10 |
| Import | 304 |
| Meshing | 101 (104 with -Boolean) |
| Parsers | 114 |
| Persistence | 278 |
| Quantities | 134 |
| Settings | 74 |
| **total** | **1598** (1606 with -Boolean) |

iOS behaviour tests: 0 before the parity job, 1588 now. The decisions job of 2026-11-19 started from 1576 (1584 with Manifold) and added 22 tests: Conditions +16 (COND-STYLE-02 +6, one of them added by the reviewer; COND-ELEV-01 +8; DLG-FINITE-01 +2) and Settings +6 (UND-09). `Config/Tests/Fixtures` holds 12 files (10 oracle tables recorded from the Windows side and 2 cabinets, `makecab_mszip.cab` and `makecab_two_folders.cab`). The repo still contains no real `.ost` or `.osp` project; the import tests build their XML and packages in code.

## Not verified

Everything here must be checked on GitHub, on a Mac or on a device. Do not read any "fixed" status as "seen working in the app".

### First full CI run (GitHub)

The only run so far (2026-11-16 00:48 UTC, runner image macos-15-arm64, Xcode 16.4 / iPhoneOS 18.5 SDK, Apple clang 17.0.0, on the code pushed on 2026-11-15) got through checkout, Xcode selection, Homebrew install of xcodegen 2.46.0 and ldid 2.1.5_2, and the configure and build of the host tests. Then the unit tests failed: Meshing 75 of 76 (`CurvedMesh.CentreLineMatchesTheWindowsMeshOnTheOracleCases`, zero-length case `zero_len_7` sample 3), a result that differed between MSVC and AppleClang. The job stopped there. The cause was fused multiply-add on arm64: a Windows decision on the last bit of a distance (segment count `int(offset / 5)`, degenerate-loop tangent) came out differently. The Core meshing code, the earcut instantiation and some geometry code are now compiled without FMA contraction (`Core/Geometry/strict_fp.hpp`, `#pragma clang fp contract(off)`, plus an explicit `ost_fp::hypot`), and the whole suite passes on the clang+FMA host build, but **no run on the new code has happened on GitHub**. Check in order:

1. Unit-test step on AppleClang (1598 tests, Debug flags plus `-Os`). The Meshing fix is only checked on the host.
2. libmanifold arm64 check, plist lint (`Config/Info.plist`, `Config/Entitlements.fakesign.plist`).
3. bgfx, bx and bimg built from the pinned SHAs (`make ios-arm64-release`; first run slow, second run should restore the cache).
4. The minos check of the five prebuilt libraries against 16.0.
5. `xcodegen generate` from `project.yml`, then the readback of `-Os` and the 16.0 deployment target.
6. `xcodebuild` (iphoneos, Release, unsigned): the first time Xcode compiles the `.mm` files against the real SDK and links `libmanifold.a` and the four bgfx libraries. Expect to fix compile or link errors; the stub-based syntax check cannot predict them.
7. Bundle checks (executable `OST Visualizer`, bundle id `com.fabian.ostvisualizer`, `MinimumOSVersion` 16.0, arm64 only).
8. `ldid` fake-signing with `Config/Entitlements.fakesign.plist` and the entitlement verification (fails on any memory/jetsam key).
9. IPA packaging and upload.

### Compile against the real SDK

All `.m`/`.mm` files, including everything changed in the wiring wave, have never been compiled by Xcode. Rendering code (`Rendering/*.mm`, bgfx) cannot even be syntax-checked against the stubs beyond what the check covers.

### On device

Not run on any device: the fake-signed IPA install, import by open-in and document picker, bgfx/Metal rendering (OIT, picking readback, selection highlight), gestures, PDF drawing, share sheet, iPad popovers, memory and frame time. In particular the UI behaviour of the 25 fixed-ui-unverified ids, with what to look at:

| Id | What to check on a device |
|---|---|
| GEO-06, LP-16 | PDF background under every page rotation and flip, multi-page PDF page choice, page size and crop box; overlays and takeoffs on the rotated canvas |
| RPV-17, RPV-31 | Attachments and area holes drawn and hit-testable in 2D (even-odd hole clip, hole hit-testing) on an area with a backout |
| RPV-32 | Handle and stroke visuals (white handles, black outline, constant screen size at every zoom, orange handle while dragging) |
| RPV-M07 | Edit-pan start point is the touch-down point |
| RPV-16 | Page switch clears the 2D selection (and the place condition unless a placement is in progress) |
| QTY-15 | Open Conditions sheet follows the 2D page and the 2D/3D mode switch |
| COND-PLACE-01 | Place with a condition, hide its layer (mode goes to View, place button off, row greyed), show it again (placement resumes); hide, pick another tool, show (nothing resumes) |
| COND-LAYER-01, COND-TRIM-01, UI-M01 | Layer cell and (none) picker entry; Trim / Curved Segment switch reload; typed ' and " stay straight (Smart Punctuation off) in the elevation, feet-inch and spacing fields |
| COND-UPD-03 | Save Failed alert, dirty flag and save-before-switching flow |
| COND-STYLE-02, COND-ELEV-01 | Edit Condition in a metric bid: change the style and save (spacing and thickness are the metric type defaults, not 4 in / 6 in); save an elevation and compare the 3D position and the Top / Bottom state right after the save with the state after reopening the bid (they must be equal) |
| UND-03, UND-07, UND-09, UND-10 | Deleting a condition clears history; one entry per group move; Count as Negative Quantity changes only the pressed takeoff, as one undoable entry (a condition selected in the sidebar widens nothing); undo of a delete selects the restored takeoff |
| BRW-02, BRW-08, DBP-31, BRW-16, POS-M01 | Trash-folder menus, delete-folder alert flow, the 'Error Loading File' alert for invalid data and cyclic page folders |
| EXP-21 | The two export alerts (nothing to export vs write failure) and the no-selection alert |

Also open on device: the HTML viewer needs network (unpkg.com); Manifold robustness and time with many negatives and drop stubs; representative real `.ost`/`.osp` files (none in the repo) compared with Windows rows; the Nyxian on-device build (`Config/Project.plist`, not re-run) and its entitlements; iRAM+ still recognising the app after a reinstall; UI chrome differences linked against the iPhoneOS 18.5 SDK instead of Nyxian's device SDK 26.4.

## Decisions of 2026-11-19

On 2026-11-19 the user decided every item that was `blocked-needs-decision` (8) and every candidate found while fixing (16 entries for 15 items, RPV-27 has two entries; DLG-FINITE-01 only for its Rise/Run half). **25 decisions** are recorded: 21 keep the iOS behaviour (no code change) and 4 are code changes. Counting rule: IMP-20 is three records (its three sub-rules), DBP-04 and DBP-05 are one record and RPV-27 (a) and (b) are one record; counted apart, the keep decisions are 22. Each decision is recorded in `PORTING_BLOCKERS.md`, `PARITY_MAP.md` and as a `decisionUpdate` on its item in `PARITY_DIVERGENCES.json`. Paths under `ost_visualizer/` are in the Windows repo (`OSTVisualizerLicense`, HEAD `6d4786f`).

### Keep the iOS behaviour, no code change (21 records)

| Id | Decision | Status |
|---|---|---|
| DBP-04 (aliases DBP-05, SNP-01, LP-19, LOAD-01) | A stored Bids.TakeoffIncrements of exactly 0 (also 0.0 / -0.0) keeps meaning snapping off on iOS; a NULL value loads as 1.0 (like Windows). No code change. Covers the aliases DBP-05, SNP-01, LP-19 and LOAD-01 (LOAD-01 is also an alias of DBP-03, which stays as it is). | wont-fix |
| BRW-09 (alias DBP-22) | Restoring a bid from Deleted Bids whose OrigBidProjectUID folder no longer exists restores it unassigned (Windows refuses the whole restore group). No code change. Covers the alias DBP-22. | wont-fix |
| EXP-12 | Control characters (U+0000-U+001F, U+007F) are dropped from exported OBJ group / material names and DXF layer names; Windows keeps them. | wont-fix |
| EXP-13 (alias MSH-M02) | Every exported OBJ mesh stays outward-facing, with or without a negative of another type in the export. No code change. Covers the alias MSH-M02 (same line as the decided EXP-M02 / GEO-15 / EXP-17). | wont-fix |
| EXP-16 | CR and LF of a condition name are written as a space in the '# <name>' comment lines of the OBJ and the MTL (Windows writes the name raw, so a line break starts new statements). | wont-fix |
| LP-24 | Show All / Hide All change only the bid's own layers; the locked template layers shared with other bids are never written and a template-only entry cannot be toggled. | wont-fix |
| RPV-29 | The view-dependent normal flip towards the viewer in the 3D lighting stays (option a of the earlier options); the shaders are not changed. | wont-fix |
| IMP-20 (1) | A DATETIME integer outside the C int range (for example "99999999999 1 1 0 0 0") stores NULL and the import goes on (Windows: OverflowError, the whole import fails and rolls back). No code change; the decided date rule itself (raw 6-part string, NULL when invalid) is fixed and tested. | wont-fix |
| IMP-20 (2) | BidTexts.Name and BidCallOuts.Name are stored as UTF-8 text (Windows: latin-1 bytes, raising for characters outside latin-1). | wont-fix |
| IMP-20 (3) | Memo and IMAGE (longbinary) text columns stay TEXT (Windows: UTF-8 BLOB). | wont-fix |
| COND-UPD-02 (aliases DLG-SAVE-01, COND-M06) | A NULL stored RefNo counts as 'not the new number' and the condition update goes on (Windows: int(None) raises, update_condition returns False). No code change. Covers the aliases DLG-SAVE-01 and COND-M06. | fixed |
| COND-FOLDER-01 (aliases QTY-20, DBP-32, DBP-30) | Condition::cdn_type_name stays '' for a missing type (Windows: 'Unknown'); the dialog shows '(none)'. No code change. Covers the aliases QTY-20, DBP-32 and DBP-30; the type group title and the export material name still say 'Unknown' like Windows. | fixed |
| DBP-33 | NameFontColor = 0 is also written explicitly on insert (kept). The DECISION TEXT is corrected: NameFontColor IS a Windows default (reference_schema_metadata.py:113), so the columns defaulted explicitly on insert are Count, GridOffsetX/Y, GridRotation, FontColor and NameFontColor. IsNegativeQuantity (also a Windows default, :111) is not in that list because insert_takeoff always writes it as a typed column with the takeoff's own flag (0 or 1), and the schema DEFAULT 0 covers rows created without it. | fixed |
| EXP-21 | The iOS export error wording stays: 'Nothing to Export', 'Export Error ... The file could not be written.' and 'No Data' (Windows: 'Export Error: Error creating <fmt> export: Export function returned False'). | fixed-ui-unverified |
| EXP-09 | Exports use the sorted page order of the iOS selection (page names, file name and output order follow the page selector, not the Windows click order); EXP-18 stays. | fixed |
| RPV-26 | The constant ost_footprint::kInteractivePdfRenderScale (3.0) is accepted: the minimum interactive sizes are 8/3 pt (count item) and 2/3 pt (linear band) on the iOS canvas. | fixed |
| RPV-27 | (a) The negative marker anchor (area-weighted centroid of the region with a nearest-interior fallback) stays; (b) the marker position at the centroid of the displayed shape on rotated or flipped pages stays. | fixed |
| RPV-16 | A page switch also clears the selected / place condition (and the FAB placement availability) unless a placement is in progress; Windows keeps the sidebar condition highlighted. | fixed-ui-unverified |
| UND-02 | The batch insert of takeoffs validates in list order: a parent restored earlier in the same undo / redo step counts (Windows validates against the rows that exist before the batch). | fixed |
| GEO-M02 (alias RPV-M01) | Polygon validity for drag candidates is evaluated in OST coordinates (Windows: scene coordinates, which depend on the render view_scale). No code change. Also covers RPV-M01 where it names the same point. | fixed |
| IMP-12 | A staging destination that is a directory counts as different content (a non-regular destination is never reused); Windows compares a directory size with a file size. | fixed |

Status is the final status of the item after the decision: the 8 former blocked items are `wont-fix` (for IMP-20 the decided DATETIME rule itself, a raw 6-part string and NULL when invalid, is fixed and tested); the candidates keep their status (`fixed` or `fixed-ui-unverified`) and the question inside them is decided.

### The four code changes

- **COND-STYLE-02** (deliberate deviation from the literal Windows behaviour; status `fixed-ui-unverified`, was `fixed`). On a style change in a metric bid the metric type defaults are persisted, `style_dimension_defaults(type, metric)` (spacing 100 mm, Area 150 mm; thickness 100 / 100 / 100 / 0 mm for Linear / Area / Count / Attachment), not the imperial table Windows writes. Imperial bids are unchanged. Changed: `Core/Domain/ConditionStyleChange.{hpp,cpp}` and one line in `EditConditionViewController.mm` that passes the dialog's `_metric`. Tests: `CondStyle02_Spacing.*` and `CondM08_UnsupportedType.ASaveRewritesTheTypeWithTheMetricDefaultsInAMetricBid` (+6, one added by the reviewer); the test that pinned the literal Windows behaviour was renamed to `TheTypeChangeDefaultsFollowTheMeasurementSystemOfTheBid` and updated. Red before the change: 4 of 163 Conditions tests. Review: production code correct, one test gap closed by the reviewer. Not verified: the Edit Condition screen in a metric bid on a device.
- **COND-ELEV-01** (deliberate deviation; status `fixed-ui-unverified`, was `fixed`). After a Save, `is_top` and `z_value` are derived from the condition name with `extract_z_value_from_name` (`ost_domain::derive_elevation_from_name`; an unreadable name gives 0.0 / false), so the in-memory condition equals what a reload gives; Windows reloads the conditions after every save, so this matches its effective behaviour. Changed: `PositionParser.{hpp,cpp}`, `ConditionStyleChange.cpp`, `ProjectStore.cpp` (the loader helper wraps the same function) and `EditConditionViewController.mm`. Tests: `CondElev01_*` in `Config/Tests/Conditions/test_condition_elevation.cpp` (+8, including a round trip through the real store over 261 names); on the old code 244 of those 261 conditions differed between memory and reload and six of the first seven new tests failed. Review: confirmed fixed, no defect found. Not verified: the dialog, save and 3D redraw path on a device; an elevation text the loader cannot read reloads as 0.0 / not top (for example a metric "3200" without a unit).
- **DLG-FINITE-01** (port of the Windows behaviour, only the Rise/Run half; status stays `fixed`). A whitespace-only Rise or Run is refused with "Rise must be a number." / "Run must be a number."; only an empty field is 0. Changed: `parse_slope_value` in `Core/Domain/ConditionFormRules.cpp` (comments in the header and the `.mm`). Tests: `DlgFinite01_RiseRun.*` (+2) and the old `CondVal01_RiseRun` whitespace case replaced by an empty-field case; the new test was red before the change (5 tests in the filtered run, 1 failed). Review: independent reviewer, CONFIRMED-FIXED; a mutation that restores the old strip makes the new whitespace test fail on 120 assertions (60 rows), the guard tests keep passing, and the file was restored byte-identically. **The other half stays open:** iOS still refuses a condition number of 2147483648 and up, Windows accepts any whole number the column holds.
- **UND-09** (port; status `fixed-ui-unverified`, unchanged). "Count as Negative Quantity" changes only the pressed takeoff, in one undoable entry (`ost_domain::negative_changes_for_press` in `Core/Domain/NegativeChange.hpp`, used by `ViewController.mm` and `UndoRedoController.{h,mm}`); a condition that is merely selected in the sidebar no longer widens the change to every takeoff of the condition on the open pages. Tests: `Und09_NegativePress.*` (+6, Settings). Red run before the change: against the old rule placed in Core, 5 of the 6 new tests failed (TESTS: 6 PASSED: 1 FAILED: 5). Independent review: CONFIRMED-FIXED; three mutations of NegativeChange.hpp failed 5, 5 and 2 of the 6 tests and the file was restored byte-identically. The tests exercise Core only, the context menu, undo and redo are only syntax-checked.

### The DBP-33 decision text (corrected)

`NameFontColor` is a Windows default (`infrastructure/mdb/reference_schema_metadata.py:113`), so the decision now lists it with the others: DEFAULT clauses for new databases, plus explicit Windows defaults on insert (Count, GridOffsetX/Y, GridRotation, FontColor and NameFontColor = 0). `IsNegativeQuantity` (also a Windows default, `:111`) was checked against the code: `ProjectStore+Takeoffs.cpp` writes it on every insert as a typed column with the takeoff's own flag (0 or 1; DBP-34 keeps 1 instead of the Access -1), so it needs no explicit-default entry and the record is consistent; the decision was not widened. The text is corrected in `PARITY_MAP.md` section E, in `PARITY_DIVERGENCES.json` and in `PORTING_BLOCKERS.md`.

## Possible Windows defects

Found while researching the kept items and the two deliberate deviations. Researched read-only: the real Windows functions were re-executed headless where possible, an adversarial pass merged the findings, and every cited file and line range was checked to exist (seven citations were spot-checked again for this document). These are for you to fix on the Windows side; nothing was changed in the Windows repository. Each row gives the line to change; the further citations of a finding are in `decisions_2026_11_19.possible_windows_defects[].where` of `PORTING_STATUS.json`. Paths are under the Windows repo root. The scenario and the suggested direction of each fix are in `PORTING_BLOCKERS.md`, section "Possible Windows defects (for the Windows side)".

| Id | possible Windows defect: file:line | What is wrong | Severity (confidence) |
|---|---|---|---|
| DBP-05 (decision DBP-04) | `ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:126` (+11 further citations) | `or 1.0` turns a stored TakeoffIncrements of 0 into 1.0 for the plan view, while the Cover Sheet reader and dialog keep 0 and the plan view itself treats <= 0 as snapping off (`plan_view/view.py:3955-3962`): two Windows readers disagree on the same stored value. | inconsistent (medium) |
| EXP-12 | `ost_visualizer/presentation/visualization/exporters/base_exporter.py:175-201` (+5 further citations) | `_sanitize_material_name` keeps CR, LF, TAB, NUL and other control characters, so a condition name with a line break injects or corrupts statements in the OBJ, MTL and DXF files. | unsafe (high) |
| EXP-13 | `ost_visualizer/presentation/visualization/core/boolean_operations.py:10-12` (+8 further citations) | `_flip_manifold_faces` flips every mesh the native call returns, but the native code returns two different windings, so one OBJ mixes inward and outward meshes and a negative of another type turns untouched positives inside out. | inconsistent (medium) |
| EXP-16 | `ost_visualizer/presentation/visualization/exporters/obj_exporter.py:46` (+3 further citations) | the `# <name>` comment lines of the OBJ and MTL (also :70) are written from the raw condition name, so CR or LF ends the comment and the rest becomes statements. | unsafe (high) |
| IMP-20 (1) DATETIME overflow | `ost_visualizer/infrastructure/mdb/components/import_operations.py:594-610` (+3 further citations) | a DATETIME integer beyond the C long range raises an uncaught OverflowError (`except (ValueError, TypeError)` at :608) and aborts the whole import, while year 10000 or month 13 read as NULL. | inconsistent (low) |
| IMP-20 (2b) annotation text | `ost_visualizer/infrastructure/mdb/components/annotation_operations.py:255-257` (+4 further citations) | text and callout annotation names are encoded as latin-1 with no UI guard, so a pasted euro sign, curly quotes or CJK text cannot be saved and the warning blames the Page and Layer settings. | inconsistent (low) |
| IMP-12 | `ost_visualizer/infrastructure/mdb/importers/osp_importer.py:413-425` (+4 further citations) | `_files_have_same_content` compares the st_size of a directory with the incoming file; with equal sizes it opens the directory and raises PermissionError instead of choosing a suffixed name (on NTFS only a 0-byte member). | cosmetic (low) |
| COND-UPD-02 | `ost_visualizer/infrastructure/mdb/components/condition_operations.py:591-607` (+8 further citations) | `int(current_row[0])` raises on a NULL RefNo although the schema, the loader (reads 0) and renumber_conditions accept it, so such a condition can never be saved with a number. | inconsistent (medium) |
| COND-FOLDER-01 | `ost_visualizer/application/use_cases/project/condition_summary_service.py:436` (+3 further citations) | the loader always sets cdn_type_name to 'Unknown', so `or SUMMARY_UNASSIGNED_LABEL` never fires: the summary row and the CSV say 'Unknown' where the type group and the sidebar say '(unassigned)'. | cosmetic (medium) |
| COND-STYLE-02 (a) imperial defaults | `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1445-1448` (+5 further citations) | on a style change the DTO takes thickness and spacing from the imperial TYPE_DEFAULTS keys even in a metric bid (101.6 mm / 152.4 mm saved instead of the 100 / 150 mm shown). | wrong-number (high) |
| COND-STYLE-02 (b) spacing replaced | `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1270-1272` (+2 further citations) | the dialog keeps the old spacing text on a style change but the save writes the type default, also replacing a value the user retyped that equals the stored one (iOS still follows this, decision of 2026-11-16). | wrong-number (medium) |
| COND-ELEV-01 | `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:383-385` (+6 further citations) | the dialog accepts elevation texts that the loader reads differently or not at all (a metric '3200', as the placeholder suggests, reloads as z 0 and not top). | wrong-number (high) |
| LP-24 | `ost_visualizer/infrastructure/mdb/components/layer_operations.py:259-300` (+9 further citations) | Hide All writes the shared locked template rows while a single toggle of such a row is refused, so one bid's Hide All can hide a template layer in another bid that has no copy of its own. | inconsistent (low) |
| RPV-27 (b) | `ost_visualizer/presentation/visualization/pdf/renderers/takeoff_renderer.py:105-116` (+3 further citations) | the negative marker is positioned with ItemIgnoresTransformations but still receives the page transform, so on a rotated or flipped page it is drawn displaced from its shape. | cosmetic (medium) |
| EXP-17 (earlier decision) | `ost_visualizer/presentation/visualization/exporters/adapters/obj_writer.py:38-43` (+7 further citations) | the OBJ writer swaps every face but the generators wind differently (box, radial prism and area inward; triangle and rhombus prism outward), so triangle and rhombus count shapes come out inside-out in the OBJ. | inconsistent (medium) |
| EXP-M02 (earlier decision) | `ost_visualizer/presentation/visualization/core/boolean_operations.py:10-12` (+7 further citations) | same root cause as EXP-13 seen through the HTML export: the shading normal of a wall changes sign depending on whether a negative of another type exists. | inconsistent (medium) |
| LP-22 (earlier decision) | `ost_visualizer/domain/services/project_data_service.py:664-678` (+6 further citations) | Hide All hides layer-less conditions in memory only (visible again after a reload, untouched by a single-layer hide). | inconsistent (medium) |
| COND-PLACE-01 note 2 (not decided) | `ost_visualizer/presentation/coordinators/ui_event_coordinator.py:1473-1489` (+5 further citations) | consequence of the LP-22 root cause: Hide All with an armed placement of a layer-less condition leaves the tool armed on a condition that is hidden and unplaceable. | inconsistent (low) |

Counts: 18 records, 14 of them for items decided on 2026-11-19, 3 for earlier decisions (EXP-17, EXP-M02, LP-22) and 1 for an open note (COND-PLACE-01 note 2). COND-STYLE-02 (a) and COND-ELEV-01 are the Windows behaviour that the two deliberate deviations avoid; COND-STYLE-02 (b) is still followed by iOS (decision of 2026-11-16, the spacing rule is unchanged).

### Kept, researched, not a defect

Researched the same way and judged not a defect on the Windows side (28 entries):

| Id | Question | Why it is not a defect |
|---|---|---|
| DBP-04 | NULL / missing TakeoffIncrements loads as 1.0 (already ported) | Not a defect. |
| BRW-09 | restore from Deleted Bids with a vanished OrigBidProjectUID fails the whole restore group | Not a defect: a deliberate referential-integrity rule. |
| DBP-33 | NameFontColor default (iOS writes 0 on insert) | Not a defect. |
| EXP-21 | export error text 'Export function returned False' | Not a defect: deliberate and test-pinned wording. |
| EXP-09 | page order = click order in multi-page exports | Not a defect: a harmless design choice. |
| IMP-20 | (2) BidTexts.Name and BidCallOuts.Name encoded as latin-1 bytes, raising for characters outside latin-1 (iOS stores UTF-8 text) | Not a defect (on the import side): the legacy column is a latin-1 byte blob and Windows deliberately raises instead of silently replacing characters; decoding latin-1 never fails, so an OST/OSP produced by Windows always re-encodes. |
| IMP-20 | (3) memo and IMAGE (longbinary) text stored as a UTF-8 BLOB (iOS keeps TEXT) | Not a defect: deliberate and consistent. |
| COND-FOLDER-01 | cdn_type_name sentinel 'Unknown' in the loader (iOS keeps '') | Not a defect: 'Unknown' for no type and for a dangling type uid is a deliberate loader convention, mirrored by the entity default and several consumers; the dialog hides it (uid lookup first, so a real type named 'Unknown' still works). |
| COND-ELEV-01 | is_top / z_value taken from the dialog but memory comes from a reload | Nothing of consequence: the two DTO keys are never persisted and Windows reloads the conditions from the database after every save, so Windows memory always equals what a reload derives from the name. |
| RPV-29 | Normal flip in the 3D lighting (iOS flips towards the viewer, Windows orients normals to +z) | Not a defect on its own: orienting shading normals to +z is deliberate and test-pinned, and the shader lights with the raw normal on purpose. |
| RPV-26 | Minimum interactive sizes (8 px count item / 2 px linear band at Windows view_scale 3.0; iOS uses 8/3 pt and 2/3 pt) | Not a Windows defect: the minimums are fixed pixel constants in scene pixels and are consistent in the renderer and the drag/placement code. |
| RPV-27 | (a) negative marker anchor of an area with backouts (Qt subtracted-path point order versus iOS region centroid) | Not a defect: Windows feeds the points of the Qt-subtracted path (several subpaths concatenated) into one polygon centroid, which is not the area centroid of the visible fill, but the contract (anchor inside the visible fill, with an interior fallback) is test-pinned and met. |
| RPV-16 | Page switch also clears the selected / place condition and the FAB placement availability on iOS (Windows keeps the sidebar condition highlighted) | Not a defect: Windows drops the takeoff selection on a page switch (pinned) and leaves the sidebar condition highlighted; iOS clearing the condition as well is the safe direction. |
| UND-02 | insert_takeoffs validates every parent against rows that exist BEFORE the batch (iOS accepts a parent restored earlier in the same batch) | Not a Windows defect: Windows never restores a takeoff under its original uid (the writer allocates new uids), so a parent has to exist before the insert. |
| GEO-M02 | drag validity (polygon_is_valid) evaluated on scene coordinates instead of OST coordinates | Not a practical defect: validity is decided on coordinates scaled by OST * 72 * view_scale / scale_ratio, so for exactly collinear or touching (degenerate) polygons the answer can depend on floating-point rounding of that scale factor. |
| EXP-15 | HTML export includes hidden-layer conditions (iOS filters them) | Not a defect: deliberate and test-pinned. |
| EXP-18 | export page and takeoff order follows the user's selection order (iOS sorts) | Not a defect: deliberate and pinned. |
| IMP-17 | LZX and Quantum cabinet compression (iOS decodes stored and MSZIP only) | Not a Windows problem. |
| DBP-34 | boolean columns: Windows writes the Access Yes/No true value (-1), iOS mixes 1 and -1 | Not a Windows defect. |
| DBP-M05 | per-page selected area (BidPageSettings.BidAreaSelected) and inactive-area colouring | Not a defect: a deliberate Windows feature (areas are surfaced there, not on iOS). |
| COND-EXTRA-01 | iOS-only Connect, Tolerance, Snap-to-Linear controls (Windows has no UI for those columns) | Not a defect: the columns are part of the OST schema and Windows round-trips them untouched (its update writes only changed fields), it just has no UI for them. |
| GEO-15 | area meshes: Windows inward (negative signed volume), iOS outward | Not a separate defect: inward in-memory winding of the area, linear and count generators is the Windows convention (the OBJ writer's face swap and the Python flip after the boolean are built around it). |
| MSH-21 | flat quantized face shading on iOS instead of Windows' 60-degree crease-angle smooth shading | Not a Windows defect: crease-angle smooth shading is the intended Windows look; flat shading on iOS is a deliberate rendering-layer choice (PORTING_LOG 2026-03-23). |
| RPV-22 | curved linear takeoffs: no curved placement and no centre handle on iOS | Not a Windows defect: Windows supports curved linears (arc band, three handles, 6-value position); iOS scope decision only. |
| CAM-02 | camera refit on every empty-to-non-empty page selection on iOS; Windows keeps the camera within a bid | Not a Windows defect: the camera is fitted once per bid scene and kept across same-bid scene updates (pinned); the iOS transition refit is a deliberate iOS addition. |
| COND-PLACE-01 | note 1: placement left running when the Image layer is hidden | Only an edge case: a condition assigned to the Image layer is hidden and unplaceable after the Image layer is hidden, while the placement tool stays armed (neither suspended nor restored). |
| DLG-FINITE-01 | whitespace-only Rise / Run: Windows rejects, old iOS accepted as 0 | Not a defect: Windows rejects whitespace-only Rise and Run with 'Rise must be a number.' / 'Run must be a number.' (an empty field means 0). |
| UND-09 | 'Count as Negative Quantity' acts on the plan-view selected takeoffs only | Not a defect: Windows changes only the takeoffs passed in (the plan-view selection) and pushes one local history entry; this is the behaviour the iOS code change copies. |

## Still open

The user has not decided these. iOS keeps its behaviour meanwhile. 20 entries: 3 questions, 10 notes for information and 7 code-hygiene notes.

### Questions (3)

| Id | Windows (file:line) | What iOS does meanwhile | Question |
|---|---|---|---|
| DLG-FINITE-01 (condition-number half; the Rise/Run half is decided) | `presentation/dialogs/edit_condition_dialog.py:1449-1469`: `int(ref_no_text)` with no upper bound, only a value below 1 is refused, so any whole number the column holds is accepted (up to 64 bits). | iOS refuses a condition number of 2147483648 and up with "Condition number must be a positive whole number." (Core/Domain/ConditionFormRules.cpp parse_condition_number, value > INT_MAX). | Widen Condition::ref_no and the column reads to 64 bits, or keep refusing 2147483648 and up? |
| COND-PLACE-01 note 1: Image-layer exemption on hide | `presentation/coordinators/ui_event_coordinator.py:6473-6476` skips the suspend of the active layer tool when the hidden layer is the Image layer (and `:6503-6504` skips the restore), so a placement of a condition on the Image layer keeps running on a condition that is hidden and unplaceable (a tap is ignored, `presentation/components/plan_view/components/placement_mode.py:1648`). Judged an edge case, not a Windows defect (low confidence). | iOS leaves the placement tool when the Image layer of the placed condition is hidden (no exemption); the placement comes back on Show like for any other layer. | Copy the Windows exemption (leave the tool running)? Windows behavior judged an edge case, not a defect. |
| COND-PLACE-01 note 2: layer-less conditions on Hide All | `presentation/coordinators/ui_event_coordinator.py:1473-1489` never suspends the tool for a layer-less condition (pinned by a Windows test), but `domain/services/project_data_service.py:664-678` marks every condition hidden on Hide All, so the tool stays armed on an unplaceable condition. Possible Windows defect, same root as LP-22 (listed above). | A layer-less condition is never suspended (like Windows); Hide All writes only layer visibility, so a layer-less condition stays visible and placeable (LP-22 wont-fix). | Nothing to port unless Windows behavior is wanted byte for byte (possible Windows defect, same root as LP-22). |

### For information (10)

- IMP-14: an unreadable / non-cabinet package says "Invalid OSP file format" where Windows says "found 0" (iOS wording kept, wiring entry IMP-M03).
- DBP-11: iOS failure dialogs keep the Windows hint "The database may be locked by another application."; "New Project / Failed to create project.", "Save Failed / Failed to save the display settings.", "Duplicate Error", "Delete Error" and the "Not Saved" / "Layer Visibility" alerts for failed forward writes are iOS wording where Windows only logs.
- GEO-M02 (geometry2): Qt clipper tolerance band cannot be reproduced; iOS uses one tolerance (1e-12 of the candidate size + 8 ulp); identical to Windows for exactly touching geometry and beyond ~1e-7 relative.
- import-pass1: iOS is deliberately STRICTER than the Windows cabinet library on inconsistent cabinet headers.
- quantities-pass1: Windows math.ceil raising on a non-finite quotient is not reproduced (no throwing from the quantity path); parsers-pass1: feet digit runs of 309+ digits (Windows OverflowError) return (0.0,false) on iOS; formatting-pass1: Windows quirks ported as is (format_quantity_number prints -0 / -0.00, display_to_mm strips any run of trailing m).
- export-rules-pass1: a cut positive whose faces were all removed by remove_degenerate_faces is not counted as surviving (Windows would export an empty file); meshing2-pass1: the Windows ERROR log of a failed boolean_union is not ported (only the WARNING).
- objc-U2b: a stored negative or non-finite BidPages Width/Height is treated as missing (612 x 792) where Windows carries it into the transform; a rotation that is not a multiple of 90 draws an unrotated canvas and no takeoffs; image layers named image are decided by smallest sequence then uid.
- objc-U2b: Windows draws small mid handles on the in-progress area polyline preview; iOS only on the rectangle being dragged (RPV-32 text).
- objc-U1: LP-12 write-back of the saved page on pick is not ported (optional per item); load_project fails a bid with two BidSettings rows like Windows.
- objc-U3: undo of a delete selects only the deleted takeoff (iOS has one selected takeoff); pre-existing hazard: a takeoff selected in 3D stays selected after switching to 2D although its page may not be the 2D page.

### Code hygiene (7 notes)

Left by the wiring wave, no behaviour change, not fixed: four near-identical "present an OK alert from the top-most presented controller" helpers in `ViewController` categories (`ViewController+Conditions.mm`, `ViewController.mm`, `ViewController+Layers.mm`, `ViewController+Export.mm`); `PlanView.mm` `isCountLike()` repeats a test `Core/Geometry/takeoff_footprint.cpp` already makes; functions with no caller kept by tests (`ost_store::shift_condition_ref_nos`, `ImportService +loadProjectWithUID:`, `ost_meshing::generate_takeoff_meshes`, `RenderConfig::is_ost()`); a project is loaded twice on open (preflight in `DatabaseViewController`, then `ViewController`); hole grouping runs over all takeoffs of the page, not only the visible ones (`group_area_takeoffs_with_holes`, one implementation); the edit pan hit-tests at the recognition location, not at touch-down, and `PlanView clearTapSelection` also clears `_draggingUID`; one case of the syntax checker's own self-test has a stale anchor text (the checker is a tool outside the repo).

## Not implemented

Each row was searched in `Controllers`, `Core`, `Services`, `PlanView`, `Rendering` and `SceneDelegate.mm` on 2026-11-18. The Windows-side inventory is `PARITY_MAP.md` section H (87 features at the 2026-11-15 map). Three of those 87 are now implemented (metric units with dimension display and quantity formatting, quantity rounding with attachment aggregation, count condition shape meshes) and the PDF page index half of "multi-page PDF page index and TIFF raster pages" is done, so 84 remain. Only the platform-independent rows below are candidates for iOS; the platform-only rows are listed after the table.

| Feature | Search result | Reference |
|---|---|---|
| OST export (.ost) | No writer: 0 hits for exportOST/ost_writer; `ExportService` exposes only exportOBJ, exportDXF, exportThreeJS. | B-009 |
| OSP export (.osp) and CAB writer | No deflateInit, deflate() or compress() call; `CabExtractor.cpp` is read-only (inflate). | B-009 |
| PDF export with takeoff and annotation markup, Bluebeam captions | 0 hits for PDFKit, UIGraphicsPDFRenderer, CGPDFContext, qpdf; the only PDF code is CGPDFDocument reading in `PDFCanvasView.mm`. | B-004 |
| FBX export, Summary CSV export | 0 hits for fbx, csv. | PARITY_MAP H (export) |
| Windows HTML viewer features (2D plan tab, visibility panels, section cut, hover tips, elevation callouts) | iOS writes a minimal viewer from visible meshes (`threejs_writer.cpp`); only the Windows shading constants and numeric payload parts were adopted (EXP-19, EXP-20); hidden-layer conditions stay filtered (EXP-15, decision). | EXP-15, EXP-19, EXP-20 |
| Annotations (draw, render, persist), hot links, named views, takeoff labels on the plan | No annotation domain or UI; annotation rows are written only by the generic .ost import and deleted with their bid, takeoff or condition; display_name/display_dimension are edited and stored but not drawn. | PARITY_MAP H (annotation) |
| Detached plan/3D windows | `UIWindow` is created once, in `SceneDelegate.mm`. | PARITY_MAP H |
| Page settings: set scale, rename/delete page, plan organizer, image adjust (rotate, flip, invert, bitonal), overlay drawing and show mode, page view state | No page write exists (0 `UPDATE BidPages`); overlay fields are loaded but never drawn. | LP-16, PARITY_MAP H |
| TIFF page images | OSP import accepts and stages `.tif`/`.tiff` members like Windows (IMP-14, IMP-16), but `PDFCanvasView.mm` draws PDFs only (CGPDF); 0 hits for CGImage in `PlanView/`. | PARITY_MAP H (page-settings) |
| Layer create, rename, delete, reorder, default layers | The only BidLayers writes are the Show update, template seeding and the bid-delete cascade. | PARITY_MAP H |
| Condition create, condition-folder CRUD, condition copy/cut/paste across bids, renumber, reassign takeoffs, assign to area | 0 hits for reassign or insert_condition and no renumber function (the one `renumbered` hit is a comment on the RefNo shift in `EditConditionViewController.mm`); iOS has edit, duplicate, delete and RefNo shift only. | PARITY_MAP H |
| Takeoff editing tools: backout mode, paste backout, cut/copy/paste/duplicate takeoffs, rotate/flip takeoffs, slope handle, set curved segment and control points, marquee select, snap to PDF lines/takeoffs/right angles, PDF text selection, zoom tool | 0 hits for backout mode, paste, rotateTakeoff, marquee; the 3D long-press menu offers only Count as Negative Quantity. Curved placement and a centre handle are not implemented by decision (RPV-22); existing curves are drawn (GEO-18). | RPV-22, PARITY_MAP H |
| Bid business workflows: cover sheet, summary tab, bid comparison, employees/payroll/job-status/condition-type master data, bid areas, job-status lock, new bid, duplicate bid, copy/cut/paste bids | No cover-sheet or Summary-tab code (the only hits are the browser struct BidSummary and the CoverSheetSel* column names in `OstSchema.cpp`); areas are not surfaced by decision (DBP-M05). | PARITY_MAP H (business) |
| AI takeoff (analysis, proposals, review, assumptions) and MCP helpers | No ai_takeoff or propose_ code. | PARITY_MAP H (ai-takeoff) |
| SQL Server backend and multi-user collaboration | 0 hits for sqlserver/odbc in code (the only mentions are comments citing the Windows ODBC driver); storage is local SQLite only. | PARITY_MAP H (collaboration) |
| 3D features: tower crane, section cuts, feature-edge overlay toggle, saved per-bid camera, page image plane, separate 2D/3D display modes | 0 hits for crane or section cut; one `RenderConfig` for both views; the camera refits on the first selection by decision (CAM-02). | PARITY_MAP H (other) |
| Multi-database registry, pan sidebar (mini-map) | One fixed database file, `Documents/ost_projects.sqlite`; no pan sidebar code. | PARITY_MAP H (other) |

Windows-only, not candidates for iOS (Obsolete or Replaced):

- Microsoft Access .mdb access, Compact/Repair (Replaced by SQLite plus direct .ost/.osp import)
- Realtime sync with the OST desktop app and OST-active edit blocking
- Licensing, update check, About dialog
- Windows installer and registry file associations (iOS registers the UTTypes in `Info.plist` instead)
- Desktop menu bar, toolbars, sidebars, status bar, keyboard shortcuts, toolbar customization, Options dialog, cursor preferences, theme following (Replaced by UIKit screens)
- MCP stdio helpers and the local pipe bridge, SQL credentials in Windows Credential Manager

## Build, CI and tests

Two build paths exist. Both read the same source folders; neither was run in this job.

| Path | Files | State |
|---|---|---|
| Nyxian (on-device build, original path) | `Config/Project.plist`<br>`Config/Entitlements.plist` | `Project.plist` has `LDEMinimumVersion` 16.0 and `-Os` in `LDECompilerFlags` (nothing else changed; `Entitlements.plist` untouched). Not re-run. `Entitlements.plist` carries `com.nyxian.pe.*` keys. |
| GitHub Actions IPA build | `project.yml`<br>`.github/workflows/build.yml`<br>`BUILD.md`<br>`Config/Info.plist`<br>`Config/Entitlements.fakesign.plist` | Committed (`a950f9a`) and edited since (16.0, `-Os`, minos check; uncommitted). XcodeGen spec has one app target (iphoneos, arm64, min iOS 16.0, `-Os` in the base settings and in both configurations, gnu++17, links libmanifold plus four bgfx libraries and the system frameworks). The workflow (macos-15, Xcode 16.4) runs the host unit tests, checks libmanifold.a is arm64, lints plists, restores or builds bgfx/bx/bimg from pinned SHAs, fails if any of the five prebuilt libraries has a Mach-O minos above 16.0, runs xcodegen, reads `-Os` and the 16.0 deployment target back from the generated project, runs xcodebuild (Release, unsigned), checks the bundle, fake-signs with ldid (only `get-task-allowed`; fails on any memory/jetsam entitlement), zips `OST-Visualizer.ipa` and uploads it (30 days). Triggers: every push and manual dispatch. One run so far, stopped at the unit tests. |
| Dependency workflows (older) | `.github/workflows/build-bgfx-ios.yml`<br>`.github/workflows/build-manifold-ios.yml` | Manual (workflow_dispatch) builds of bgfx and Manifold artifacts only. Unchanged. |

Tests:

- `Config/Tests` (CMake, C++17): builds `Core/*.cpp`, `sqlite3.c`, `pugixml.cpp` and `Rendering/MeshLoader.cpp` as `ost_core`, with a header-only harness (`TestHarness.hpp`, `TestMain.cpp`). Area folders `Config/Tests/<Area>/test_*.cpp` are picked up by CMake glob. Boolean ops are a throwing stub unless `-DOST_TEST_BOOLEAN=ON`; zlib is downloaded on Windows. Run it the way CI does: `cmake -S Config/Tests -B build/tests -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build/tests --parallel`, `ctest --test-dir build/tests --output-on-failure`.
- The tests live in `Config/Tests`, **not** in a top-level `Tests/`: Nyxian compiles every `.c`, `.cpp`, `.m`, `.mm` and `.swift` file outside the top-level `Config/` and `Resources/` folders (per `BUILD.md`, from Nyxian's public source; the installed Nyxian was not inspected), and the tests have their own `main()` and a boolean stub.
- Areas: Browser, Conditions, Export, Formatting, Geometry, Harness, Import, Meshing, Parsers, Persistence, Quantities, Settings (counts above). `Config/Tests/README.md` has the conventions: every ported test cites its Windows origin and a tolerance is never relaxed to make a port pass.
- On non-MSVC compilers CMake adds `-Os` (like the app) on top of the Debug flags.
- `build.yml` runs the host tests (`ctest --no-tests=error`) after selecting Xcode and installing XcodeGen and ldid and before any library or app build, and stops the job on failure.

Dependencies and settings:

- `Libs/Manifold/lib/libmanifold.a` is tracked (minos 16.0, read from the Mach-O load commands in Phase 0); arm64 and minos are checked by CI steps.
- bgfx, bx, bimg headers are tracked under `Libs/bgfx`; the four bgfx static libraries are not tracked (`.build/` is gitignored); CI rebuilds them from pinned SHAs.
- Shaders are compiled offline; the app includes the 8 generated headers in `Rendering/Shaders/generated`. The `.sc` sources are not compiled by `project.yml`.
- `Resources/` is empty; the app ships no resources.
- **Minimum iOS is 16.0** in `project.yml`, `Config/Info.plist` (`MinimumOSVersion`), `Config/Project.plist` (`LDEMinimumVersion`) and `build.yml` (`EXPECTED_MIN_OS`). The sources contain no pre-iOS-16 availability checks. C/C++/Objective-C are compiled with **`-Os`** (`GCC_OPTIMIZATION_LEVEL: s` in `project.yml`; `-Os` in `Config/Project.plist`); there are no Swift sources.
- `.gitignore` adds `/build/`, `*.xcodeproj/`, `DerivedData/`, `/deps/`. The Xcode project is generated, never committed.

## Related files

- `PARITY_MAP.md`: all divergences, per-area feature table (section G), Windows-only features (H), stale claims (I), decisions (E).
- `PARITY_DIVERGENCES.json`: machine-readable twin of the map with test plans, the recorded decisions (including the 2026-11-19 `decisionUpdate` records) and the possible Windows defects (repo root).
- `PORTING_BLOCKERS.md`: the decisions of 2026-11-19 with the possible Windows defects (Windows file:line), what is still open, true blockers (B-004, B-009, B-010), what is only verifiable on GitHub, macOS or a device, resolved blockers, known leftovers.
- `PORTING_PLAN.md`: what is left to do and how to validate it.
- `PORTING_STATUS.json`: the same facts in machine-readable form.
- `BUILD.md`: CI build, fake-signing, bgfx pin, local unit tests.
- `Config/Tests/README.md`: how to run and add host tests.
- `PORTING_LOG.md`: chronological history; entries before 2026-11-15 are not corrected and contain stale claims.
