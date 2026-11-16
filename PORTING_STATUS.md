# OST Visualizer - iOS Porting Status

Last updated: 2026-11-15

Basis: iOS repo at HEAD `4a1302d` plus uncommitted Phase 0/1 files (`project.yml`, `.github/workflows/build.yml`, `BUILD.md`, `Config/Info.plist`, `Config/Entitlements.fakesign.plist`, `Config/Tests/`, `PARITY_MAP.md`; `.gitignore` modified). No iOS source file differs from HEAD. Windows reference: `OSTVisualizerLicense` HEAD `dceb47d` (read-only). Every claim here was checked by opening the file or by search on 2026-11-15. Nothing was built or run on iOS: the authoring machine has no Xcode, macOS, xcodebuild, XcodeGen, ldid or device.

## Summary

- The iOS app has an end-to-end import, browse, view, edit and export path in source: `.ost`/`.osp` import into SQLite, project browser, bgfx/Metal 3D view, 2D plan view over a CoreGraphics PDF page, takeoff placement/drag/delete with undo, condition editing, layer visibility, OBJ/DXF/Three.js export.
- It is not at parity with Windows. Phase 1 recorded **216 divergences** (171 fix, 44 needs-decision, 1 cannot-reproduce; 66 wrong-numbers, 45 data-safety, 86 behavior, 19 ui-cosmetic), all open. Detail: `PARITY_MAP.md`. Earlier 'Ported' labels in this file were wrong for most areas and are replaced by the vocabulary below.
- Not implemented: OST/OSP export, PDF/annotation export, annotations, metric measurement, quantity rounding, attachment aggregation, page-settings editing, most Windows editing tools (list below).
- Build state: an XcodeGen spec, a GitHub Actions IPA workflow and a host unit-test harness exist (Phase 0), all uncommitted. None of the iOS build has been run. The harness holds 10 self-tests and **0 iOS behaviour tests**.
- Active blockers and partial gaps: `PORTING_BLOCKERS.md`. Fix order and plan: `PORTING_PLAN.md`.

## Status vocabulary

| Status | Meaning |
|---|---|
| Implemented-matches-Windows | Compared with Windows (host differential run or reading both sides) and no divergence recorded. |
| Implemented-diverges | Present in source; at least one `PARITY_MAP.md` divergence (ids cited). |
| Implemented-unverified | Present in source; no comparison or run was possible. |
| Replaced | Platform replacement of a Windows technology. |
| Not implemented | No iOS code (verified by search). |
| Obsolete | Windows-only concern with no iOS equivalent needed. |

Evidence levels: **host-run** = the iOS `Core/` C++ was compiled with MSVC and compared with the Windows Python/native modules (Phase 1, throw-away harnesses outside the repo). **read** = both sides read and the arithmetic re-implemented (Objective-C/UIKit cannot be compiled here). No result comes from a device.

## Implemented systems

| System | Status | iOS evidence | Matches Windows? | PARITY_MAP ids |
|---|---|---|---|---|
| App shell and URL routing | Replaced | `Main.m`<br>`AppDelegate.m`<br>`SceneDelegate.mm`<br>`Config/Info.plist` | n/a (Windows is Python/Qt) | - |
| Import .ost | Implemented-diverges | `Services/ImportService.mm (importOSTFile)`<br>`Core/Import/OstXmlParser.cpp`<br>`Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp` | No. Bid-less XML is rejected like Windows, but malformed XML the parser tolerates is accepted (IMP-M01) and the write path differs. | IMP-01, IMP-02, IMP-03, IMP-04, IMP-05, IMP-06, IMP-07, IMP-08, IMP-09, IMP-10, IMP-11, IMP-19, ... (+2 more) |
| Import .osp | Implemented-diverges | `Services/ImportService.mm (importOSPFile)`<br>`Core/Import/CabExtractor.cpp` | No. | IMP-12, IMP-13, IMP-14, IMP-15, IMP-16, IMP-17, IMP-18, IMP-M02, IMP-M03 |
| SQLite schema and seed rows | Implemented-matches-Windows | `Core/Persistence/OstSchema.cpp`<br>`Core/Persistence/ProjectStore/ProjectStore.cpp (create_schema)`<br>`Core/Persistence/OstDatabase.cpp` | Yes for DDL and seed rows: the 64 Windows tables have identical column names in the same order (script comparison, re-run in the 2026-11-15 doc review); the seed rows match (26 SchemaRegistry versions, the four default layers). Indexes: iOS creates all 129 non-unique indexes and the 2 unique Locking indexes listed in reference_schema_metadata.py EXPLICIT_INDEXES plus 8 more foreign-key indexes that are not in that list (BidConditions.CdnTypeUID, BidEmployees.PayClassUID, BidLaborCostCodes.CostCodeUID, BidPageSettings.BidAreaUID, BidPageSettings.BidPageUID, BidPageSettings.BidTypAreaUID, BidSettings.BidPageSelectedUID, ConditionSets.EmployeeUID; Access may create them from relationships, not checked). iOS adds _AppMeta and _AppPreferences (66 tables). | DBP-33, DBP-34, IMP-05, IMP-20 |
| Store behaviour (load, write, delete, cascades) | Implemented-diverges | `Core/Persistence/ProjectStore/ProjectStore.cpp`<br>`Core/Persistence/ProjectStore/Components/ProjectStore+{Import,Load,Folders,Takeoffs,Conditions}.cpp`<br>`Core/Persistence/ProjectStore/ProjectStore_Internal.hpp` | No. | DBP-11, DBP-12, DBP-14, DBP-17, DBP-18, DBP-20, DBP-31, DBP-35, DBP-M02, DBP-03, DBP-04, DBP-19, ... (+8 more) |
| Project browser | Implemented-diverges | `Controllers/DatabaseViewController.mm`<br>`Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp`<br>`Core/Persistence/ProjectStore/ProjectStore.cpp (delete_project, permanently_delete_project)` | No. Hierarchy load matches; folder order (SQLite BINARY, Windows is locale-aware: BRW-M01, decision), bid order, delete, restore and the Deleted Bids identity do not. | BRW-01, BRW-02, BRW-05, BRW-08, BRW-09, BRW-10, BRW-11, BRW-12, BRW-13, BRW-19, BRW-21, BRW-M01 |
| Project and edit persistence (what is written) | Implemented-diverges | `Services/ImportService.mm`<br>`ProjectStore+Takeoffs.cpp`<br>`ProjectStore+Conditions.cpp`<br>`ProjectStore+Folders.cpp` | Partly: set_layer_visibility, set_takeoff_negative and shift_condition_ref_nos match (set_takeoff_negative stores 1 where Windows stores the Access boolean -1: DBP-34, decision); the other writers do not. | DBP-07, DBP-14, DBP-17, DBP-18, DBP-35, COND-UPD-01, COND-UPD-02, COND-UPD-03, COND-DUP-01, COND-DEL-01, COND-M05 |
| Domain model (loaded subset) | Implemented-diverges | `Core/Domain/*`<br>`Core/Data/ProjectData.hpp`<br>`Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp` | Partly: quantity-relevant Condition/Takeoff fields match; several load rules differ. | LP-20, QTY-13, QTY-19, DBP-03, DBP-04, DBP-31, DBP-37, DBP-M05, COND-FOLDER-01, COND-EXTRA-01 |
| Elevation and position text parsing | Implemented-diverges | `Core/Persistence/ProjectStore/ProjectStore.cpp (parse_position, extract_z_value)`<br>`Controllers/EditConditionViewController/Components/EditConditionHelpers.mm` | No, except the elevation name split/join (parseElevation, reassembleElevation) which matches (reassembleElevation trims only ' ' where Windows strip() trims any whitespace; not recorded in PARITY_MAP). | ELV-01, ELV-02, ELV-03, ELV-04, ELV-05, ELV-06, FMT-01, DLG-FINITE-01, DBP-07, DBP-08, POS-M01 |
| Quantities | Implemented-diverges | `Core/Domain/UomService.hpp`<br>`Controllers/ConditionsViewController.mm`<br>`Controllers/ViewController/ViewController.mm (_updateConditionQuantities)` | Partly. Imperial divisors, raw formulas for count/linear/area, straight polylines, grid and negative sums matched Windows in 5000 random cases (polygon area is summed about the origin and loses precision far from it: QTY-14); aggregation, page scope, arcs, slope, metric and rounding do not. | QTY-01, QTY-02, QTY-03, QTY-04, QTY-05, QTY-06, QTY-07, QTY-08, QTY-09, QTY-10, QTY-11, QTY-12, ... (+3 more) |
| Mesh pipeline (scale, rotation/flip, ordering) | Implemented-diverges | `Services/ProjectService.mm (generateMeshesIntoRenderer)`<br>`Core/Meshing/MeshFactory.cpp` | No. | MSH-01, MSH-02, MSH-14, MSH-20 |
| Linear wall meshes (straight, rise/run slope, drop stub) | Implemented-matches-Windows | `Core/Meshing/gen_linear.cpp`<br>`Core/Meshing/mesh_helpers.hpp` | Yes at generator level. Pipeline-level MSH-01/MSH-02 still change the result in the app. | - |
| Curved linear wall meshes | Implemented-diverges | `Core/Meshing/gen_curved.cpp` | No (reimplementation, not a port of linear_geom.cpp). | MSH-05, MSH-06, MSH-07 |
| Area prism meshes (earcut, holes, slope) | Implemented-diverges | `Core/Meshing/gen_area.cpp`<br>`Core/Geometry/earcut.hpp`<br>`Core/Geometry/earcut_impl.cpp` | No (earcut itself matches). | GEO-05, GEO-14, GEO-15, MSH-14 |
| Count and attachment meshes | Implemented-diverges | `Core/Meshing/gen_count.cpp`<br>`Core/Meshing/MeshFactory.cpp (create_count)` | Square, rectangle (except a negative depth, MSH-13) and circle match; ellipse, triangle, rhombus, hexagon and octagon do not. | MSH-08, MSH-09, MSH-10, MSH-11, MSH-13 |
| Boolean operations (Manifold) | Implemented-diverges | `Core/Geometry/boolean_ops.cpp`<br>`Libs/Manifold/ (headers, lib/libmanifold.a)`<br>`Services/ProjectService.mm`<br>`Services/ExportService.mm`<br>`Core/Meshing/MeshFactory.cpp` | Library code: yes (diff against Windows boolean_ops.cpp shows include lines and a stale TODO comment on line 1 only). Orchestration: no. | MSH-14, MSH-15, MSH-16, MSH-19, MSH-19b, EXP-05 |
| 3D renderer | Replaced | `Rendering/Renderer.mm`<br>`Rendering/BgfxView.mm`<br>`Rendering/MeshLoader.cpp`<br>`Rendering/Shaders/generated/*.h (8 headers)`<br>`Rendering/MathUtils.hpp` | Replaced technology (Windows: OpenGL/Qt). Mesh-ID picking and the selection highlight read as matching; lighting and shading differ. | RPV-29, MSH-21, COL-02, COL-03 |
| 2D plan view and PDF background | Implemented-diverges | `PlanView/PlanView.mm`<br>`PlanView/Components/PDFCanvasView.mm`<br>`PlanView/Components/TakeoffOverlayView.mm`<br>`PlanView/Components/PlanView+Placement.mm`<br>`Core/Geometry/coord_transform.cpp` | No. | LP-15, LP-16, GEO-06, RPV-02, RPV-03, RPV-17, RPV-22, RPV-25, RPV-26, RPV-27, RPV-31, RPV-32, ... (+4 more) |
| Condition list and editor | Implemented-diverges | `Controllers/ConditionsViewController.mm`<br>`Controllers/EditConditionViewController/*` | No (owner area conditions; 31 ids). | COND-DEL-01, COND-UPD-01, COND-UPD-02, COND-STYLE-01, COND-REF-01, COND-ELEV-01, COND-METRIC-01, COND-FOLDER-01, COND-LIST-01, COND-PLACE-01, QTY-22, QTY-23 |
| Layers and page selection | Implemented-diverges | `Controllers/ViewController/Components/ViewController+Layers.mm`<br>`Controllers/ViewController/Components/ViewController+PageSelector.mm`<br>`Services/ProjectService.mm` | Layer Show flag, visibility rule and its persistence match; page order, folder selection and initial page do not (owner area layers-pages; 9 ids). | LP-08, LP-10, LP-11, LP-12, LP-18, LP-22, LP-24, LP-25, RPV-16 |
| OBJ, DXF and Three.js HTML export | Implemented-diverges | `Services/ExportService.mm`<br>`Core/Export/{obj_writer,dxf_writer,threejs_writer}.cpp`<br>`Controllers/ViewController/Components/ViewController+Export.mm` | Writers' file layout matches (OBJ text, DXF writer: diff vs Windows is include lines, LayeredMesh declared in mesh_data.hpp instead of dxf_writer.hpp, and a write_eof helper that Windows declares but never calls; both write the EOF group inside save). Exported content does not (axes, holes, naming, colours). | EXP-01, EXP-02, EXP-03, EXP-05, EXP-06, EXP-08, EXP-09, EXP-11, EXP-12, EXP-13, EXP-14, EXP-15, ... (+5 more) |
| Undo and redo | Implemented-diverges | `Controllers/ViewController/Components/UndoRedoController.mm` | No. | UND-01, UND-02, UND-03, UND-06, UND-07, UND-08, UND-09, UND-10, UND-11 |
| 3D camera | Replaced | `Controllers/ViewController/Components/CameraController.mm`<br>`Rendering/Renderer.mm (fitCameraToMeshes)` | Platform-adapted gestures (one-finger orbit, pan-mode toggle on the floating button, pinch zoom). Fit/Reset framing differs. | CAM-01, CAM-02, CAM-03, CAM-04 |
| Placement, drag and snapping | Implemented-diverges | `PlanView/Components/PlanView+Placement.mm`<br>`Controllers/ViewController/Components/DragController.mm`<br>`Core/Geometry/geom_utils.cpp`<br>`Core/Domain/Bid.hpp (snap_increments_ost)` | Partly: Bid::snap_increments_ost (metric mm to inches) matches; snapping rules do not. | RPV-04, RPV-05, RPV-06, RPV-07, RPV-08, RPV-09, RPV-10, RPV-13, RPV-15, RPV-20, RPV-M01, RPV-M03, ... (+4 more) |
| Render settings (colour mode, grayscale) | Implemented-diverges | `Core/Domain/RenderConfig.hpp`<br>`ProjectStore+Conditions.cpp (load_render_config, save_render_config)`<br>`Controllers/DatabaseViewController.mm (_settingsTapped)`<br>`ViewController.mm (_applyRenderConfig)` | No. | SET-01, SET-02, SET-03, COL-01, COL-02, COL-03, COL-04 |
| Build, CI and tests (Phase 0) | Implemented-unverified | `project.yml`<br>`.github/workflows/build.yml`<br>`BUILD.md`<br>`Config/Info.plist`<br>`Config/Entitlements.fakesign.plist`<br>`Config/Tests/`<br>`Config/Project.plist`<br>`Config/Entitlements.plist` | n/a | - |

Notes (facts verified in code; 'Verification' states how far each system was checked):

- **App shell and URL routing.** UIKit lifecycle. Root is DatabaseViewController in a UINavigationController. .ost/.osp URLs (cold start and openURLContexts) go to ImportService and a failure shows an alert. Info.plist declares both document types. Verification: read only; not launched on a device.
- **Import .ost.** Parses with pugixml, creates the schema, refuses a Bids.UID that is already stored, inserts rows with INSERT OR REPLACE and keeps the file's UIDs (IMP-06). import_project commits and returns true whatever the SQL did (IMP-08). BidEmployees and PayClasses are not parsed (IMP-11). Verification: read only; no import was run and no .ost fixture exists in the repo.
- **Import .osp.** Own CAB reader (stored and MSZIP via zlib only). Picks the .ost member, extracts only .pdf members into Documents/OST/<bid name>/ by file basename and rewrites ImagePath/OverlayImagePath. No TIFF handling. Verification: read only; no import was run and no .osp fixture exists in the repo.
- **SQLite schema and seed rows.** Not replicated: DEFAULT/REFERENCES metadata (DBP-33), Access boolean values (DBP-34), typed columns (IMP-05, IMP-20). Verification: column sets compared by script in Phase 1; schema creation not run on iOS.
- **Store behaviour (load, write, delete, cascades).** One database file, Documents/ost_projects.sqlite (ImportService.databaseURL). SQL errors are swallowed (for example set_layer_visibility and import_project return true after unchecked statements, DBP-11). Cascades are incomplete (DBP-12, DBP-14). Page-folder tree loses nested folders (DBP-20). Verification: compiled and run on the host with MSVC in Phase 1; not run on iOS.
- **Project browser.** Folders (create, rename, delete), bids (move, remove from folder, trash, permanent delete from 'Deleted Bids', 'Restore as Unassigned'), document-picker import, Visualization Settings sheet. Swipe-delete runs at once with no confirmation (BRW-01). Verification: ProjectStore parts host-run in Phase 1; UIKit read only.
- **Project and edit persistence (what is written).** Writes: takeoff position, insert, delete, negative flag; condition update, duplicate, delete, RefNo shift; layer Show flag; project folders and bid trash/restore/move; render config (_AppPreferences). No page write, layer insert/rename/delete, condition create or annotation write exists (searched). Verification: ProjectStore host-run in Phase 1; UIKit callers read only.
- **Domain model (loaded subset).** Loads 9 of the 66 tables: Bids, BidProjects (name), BidPages, BidLayers, CdnTypes, BidConditionFolders, BidConditions, BidTakeoffs, BidPageFolders. All other tables are stored but not modelled (annotations, areas, cover-sheet data, named views, hot links, employees). Verification: read, plus host-run of the loader in Phase 1; not run on iOS.
- **Elevation and position text parsing.** extract_z_value is a hand-written parser, not the Windows grammar: 145 of 342 test-derived names gave a different z_value/is_top in Phase 1 (ELV-01..06). Verification: host-run in Phase 1; no committed test.
- **Quantities.** UOM codes 0-9 only (Windows has 11-16 metric). compute_page_quantities skips every takeoff with a parent_uid and passes 0.0 for attachment footprint/perimeter. round_quantity/round_up are modelled and persisted but never applied. Total: QTY-01..QTY-19, QTY-22, QTY-23. Verification: host-run in Phase 1; no committed test.
- **Mesh pipeline (scale, rotation/flip, ordering).** Every coordinate and dimension is multiplied by 1/(sf2/sf1) of its page (Windows meshes in raw OST units, MSH-01). Page rotation and flip are never applied to 3D meshes (MSH-02). Any takeoff with a parent is treated as a hole (MSH-14). Verification: Core host-run in Phase 1; Objective-C orchestration read only.
- **Linear wall meshes (straight, rise/run slope, drop stub).** Drop-stub union with the wall is a boolean step (MSH-19). Verification: host-run in Phase 1; no committed test.
- **Curved linear wall meshes.** Segment count rounding, arc side above about 180 degrees and the straight-chord rule differ. The curved drop-stub geometry matches. Verification: host-run in Phase 1; no committed test.
- **Area prism meshes (earcut, holes, slope).** Slope direction (GEO-05), tessellation frame (GEO-14) and face winding (GEO-15, needs-decision) differ. Verification: host-run in Phase 1; no committed test.
- **Count and attachment meshes.** Only two generators exist: a box (generate_count_mesh) and a 24-segment cylinder (generate_cylinder_mesh). Shape table below. Verification: host-run in Phase 1; no committed test.
- **Boolean operations (Manifold).** 3D view subtracts negatives per condition type; export subtracts all negatives from all positives. A positive removed completely by negatives stays visible (MSH-15). Verification: Manifold was not linked or run in this job (the host harness uses a throwing stub unless -DOST_TEST_BOOLEAN=ON); libmanifold.a not run on iOS.
- **3D renderer.** bgfx on Metal (RendererType::Metal), CAMetalLayer view, opaque pass plus weighted-blended OIT (accum, revealage, composite), mesh-ID pick pass with readback, selection mask and post pass. Verification: read only; no GPU or device check of rendering, OIT, picking readback or frame time.
- **2D plan view and PDF background.** PDFCanvasView draws page 1 only (CGPDFDocumentGetPage(_pdfDoc, 1)) at the stored page size; page_index, rotation and flip are not applied to the PDF. Overlay image fields are loaded and rewritten on OSP import but never drawn. Pattern only changes fill alpha (no hatching). Curved linears are drawn as straight chords. Display Name / Display Dimension flags are editable and persisted but not drawn. Verification: read only; not run on a device.
- **Condition list and editor.** List with folder/type sections, quantities and long-press Duplicate/Delete; edit dialog with dimension, appearance, results (3 quantity slots), advanced options and Previous/Next. Condition create and condition-folder CRUD do not exist. Verification: read only; not run on a device.
- **Layers and page selection.** Layers sheet toggles layers and Show All / Hide All; two page bars (3D multi-select, 2D single). Only the Show flag is ever written (no layer create/rename/delete). Verification: read only; not run on a device.
- **OBJ, DXF and Three.js HTML export.** Exports visible meshes of the selected 3D pages into NSTemporaryDirectory and shares them with UIActivityViewController. Failure and 'nothing to export' show the same alert (EXP-21). The HTML imports three@0.152.0 from unpkg.com, so viewing it needs network. Verification: writers host-run in Phase 1; share sheet, large exports and write failures not run.
- **Undo and redo.** Three actions only (enum UndoAction { Move, Place, Delete }); history is unbounded (UND-01). Condition, layer and folder edits are not undoable (Windows has no undo for them either). Verification: read only; not run on a device.
- **3D camera.** Windows keeps the 3D camera per bid; iOS refits (CAM-02). Verification: read only; not run on a device.
- **Placement, drag and snapping.** Grid snap from Bid.takeoff_increments and angle snap of pi/8 = 22.5 degrees (Windows default 15, RPV-04). No endpoint, midpoint, PDF-line or right-angle snapping exists. Verification: read only; not run on a device.
- **Render settings (colour mode, grayscale).** Modes Solid / OST / Transparent plus grayscale, default Solid, one setting for 2D and 3D, stored in _AppPreferences. Windows has separate 2D and 3D modes (Original / Solid / Transparent). Verification: read only; not run on a device.
- **Build, CI and tests (Phase 0).** See the Build, CI and tests section. All Phase 0 files are uncommitted. Verification: XcodeGen, xcodebuild, ldid and the workflow were never run.

### Count and attachment shapes actually generated

Shape ids match Windows (`Core/Domain/Types.hpp`). Attachments use the same path as count conditions (`MeshFactory.cpp`, `create_count`).

| Shape (id) | iOS mesh | Matches Windows? | PARITY_MAP ids |
|---|---|---|---|
| Square (0) | box, depth = width | Yes | - |
| Rectangle (3) | box, width x depth (a non-positive depth falls back to width; Windows clamps a negative depth to 1.0) | Yes, except negative depth | MSH-13 |
| Circle (1) | 24-segment cylinder, diameter = width | Yes | - |
| Ellipse (13) | the same circular cylinder (depth ignored) | No | MSH-08 |
| Triangle (2) | box | No | MSH-09 |
| Rhombus (12) | box | No | MSH-10 |
| Hexagon (14) | box | No | MSH-11 |
| Octagon (15) | box | No | MSH-11 |

## Divergences from Windows (Phase 1)

Source: `PARITY_MAP.md` (human) and `divergences.json` (machine, Phase 1 scratchpad). Counts are unique ids after merging aliases; every item has status `open`. `fix` = Windows behaviour is clear; `needs-decision` = ambiguous or a product call (PARITY_MAP section E); `cannot-reproduce` = PARITY_MAP section F.

| Impact | Count |
|---|---:|
| wrong-numbers | 66 |
| data-safety | 45 |
| behavior | 86 |
| ui-cosmetic | 19 |
| **total** | **216** |

| Owner area | fix | needs-decision | cannot-reproduce | total | Main iOS files |
|---|---:|---:|---:|---:|---|
| quantities | 15 | 1 | 0 | 16 | Core/Domain/UomService.hpp, ConditionsViewController.mm, ViewController.mm |
| geometry | 4 | 0 | 0 | 4 | Core/Geometry/coord_transform.cpp, geom_utils.cpp, DragController.mm |
| meshing | 17 | 3 | 0 | 20 | Core/Meshing/*, Services/ProjectService.mm |
| parsers-units | 11 | 1 | 0 | 12 | ProjectStore.cpp (parse_position, extract_z_value), EditConditionHelpers.mm, Condition.hpp |
| import-ost-osp | 16 | 7 | 0 | 23 | Services/ImportService.mm, Core/Import/*, ProjectStore+Import.cpp |
| export-formats | 12 | 7 | 1 | 20 | Services/ExportService.mm, Core/Export/* |
| database-persistence | 15 | 7 | 0 | 22 | Core/Persistence/* |
| conditions | 25 | 6 | 0 | 31 | ConditionsViewController.mm, EditConditionViewController* |
| layers-pages | 7 | 2 | 0 | 9 | ViewController+PageSelector.mm, ViewController+Layers.mm, ProjectService.mm |
| rendering-planview | 26 | 5 | 0 | 31 | PlanView/*, Rendering/Renderer.mm |
| project-browser | 9 | 3 | 0 | 12 | DatabaseViewController.mm, ProjectStore+Folders.cpp |
| settings-undo-camera | 14 | 2 | 0 | 16 | UndoRedoController.mm, CameraController.mm, RenderConfig.hpp |
| **total** | **171** | **44** | **1** | **216** | |

Testability: core-cpp 108 (host unit test against `Core/`), needs-extraction-from-objc 95 (logic must first move from `.mm` into `Core/`), objc-only 13 (UIKit or device).

Highest-ranked items: MSH-01 (all 3D geometry divided by the page scale ratio), QTY-01 (takeoffs with a parent count 0), QTY-04 (rounding never applied), ELV-01 (elevation parsing), QTY-10 (area slope factor), QTY-06 (UOM normalisation), GEO-06 (page rotation/flip model), MSH-02 (no page rotation/flip in 3D).

## Not implemented

Each row was searched in `Controllers`, `Core`, `Services`, `PlanView`, `Rendering` and `SceneDelegate.mm` on 2026-11-15. The Windows-side inventory is `PARITY_MAP.md` section H (87 features).

| Feature | Search result | Reference |
|---|---|---|
| OST export (.ost) | No writer: 0 hits for exportOST/ost_writer; ExportService exposes only exportOBJ, exportDXF, exportThreeJS. | B-009 |
| OSP export (.osp) and CAB writer | No deflateInit, deflate() or compress() call (the words occur only in reader-side names such as decompress_folder and deflate_data in CabExtractor.cpp); CabExtractor.cpp is read-only (inflate). | B-009 |
| PDF export with takeoff and annotation markup, Bluebeam captions | 0 hits for PDFKit, UIGraphicsPDFRenderer, CGPDFContext, qpdf; the only PDF code is CGPDFDocument reading in PDFCanvasView.mm. | B-004 |
| FBX export, Summary CSV export | 0 hits for fbx, csv. | deferred |
| Windows HTML viewer features (2D plan tab, visibility panels, section cut, hover tips, elevation callouts) | iOS writes a minimal viewer from visible meshes (threejs_writer.cpp). | EXP-15, EXP-19, EXP-20 |
| Annotations (draw, render, persist), hot links, named views, takeoff labels on the plan | No annotation domain or UI: no annotation read or write code; annotation rows are written only by the generic .ost import and deleted only on permanent bid delete (ProjectStore.cpp) and, for BidDimensions/BidALines/BidArrows rows linked from a takeoff (BidTakeoffFromUID), by delete_takeoff and delete_condition (ProjectStore+Takeoffs.cpp, ProjectStore+Conditions.cpp); display_name/display_dimension are edited and stored but not drawn. | PARITY_MAP H (annotation) |
| Detached plan/3D windows | UIWindow is created once, in SceneDelegate.mm; no secondary window or viewer-sync code. | PARITY_MAP H |
| Page settings: set scale, rename/delete page, plan organizer, image adjust (rotate, flip, invert, bitonal), overlay drawing and show mode, page view state | No page write exists (0 UPDATE BidPages); overlay fields are loaded (ProjectStore+Load.cpp) but never drawn. | LP-16, PARITY_MAP H |
| Multi-page PDF page index, TIFF page images | PDFCanvasView.mm always draws page 1; OSP import extracts only .pdf members; 0 hits for tif/tiff. | LP-16 |
| Layer create, rename, delete, reorder, default layers | The only BidLayers writes are the Show update, template seeding and the bid-delete cascade. | PARITY_MAP H |
| Condition create, condition-folder CRUD, condition copy/cut/paste across bids, renumber, reassign takeoffs, assign to area | 0 hits for reassign, renumber, insert_condition; iOS has edit, duplicate, delete and RefNo shift only; other items per PARITY_MAP H. | PARITY_MAP H |
| Takeoff editing tools: backout mode, paste backout, cut/copy/paste/duplicate takeoffs, rotate/flip takeoffs, slope handle, set curved segment and control points, marquee select, snap to PDF lines/takeoffs/right angles, PDF text selection, zoom tool | 0 hits for backout mode, paste, rotateTakeoff, marquee; the 3D long-press menu offers only Count as Negative Quantity; other tools per PARITY_MAP H. | RPV-22, PARITY_MAP H |
| Metric measurement mode (UOM codes 11-16, mm display and parsing, metric defaults) | UomService.hpp defines codes 0-9; Bid.measure_base is used only by snap_increments_ost. | QTY-05, QTY-06, COND-METRIC-01 |
| Quantity rounding and attachment aggregation | round_quantity/round_up never read after load; attachment_footprint/perimeter hard-coded 0.0 (UomService.hpp:790). | QTY-04, QTY-02 |
| Bid business workflows: cover sheet, summary tab, bid comparison, employees/payroll/job-status/condition-type master data, bid areas, job-status lock, new bid, duplicate bid, copy/cut/paste bids | No cover-sheet or Summary-tab code (the only hits are the project-browser struct BidSummary and the CoverSheetSel* column names in OstSchema.cpp); BidAreas appears only in the permanent-delete cascade. | PARITY_MAP H (business) |
| AI takeoff (analysis, proposals, review, assumptions) and MCP helpers | No ai_takeoff or propose_ code (the only MCP matches are memcpy calls). | PARITY_MAP H (ai-takeoff) |
| SQL Server backend and multi-user collaboration | 0 hits for sqlserver/odbc; storage is local SQLite only. | PARITY_MAP H (collaboration) |
| 3D features: tower crane, section cuts, feature-edge overlay toggle, saved per-bid camera, page image plane, separate 2D/3D display modes | 0 hits for crane, section cut, clip plane; one RenderConfig for both views; feature edges are used only for DXF export and the selection highlight. | PARITY_MAP H (other) |
| Multi-database registry, pan sidebar (mini-map) | One fixed database file, Documents/ost_projects.sqlite (ImportService.databaseURL); no pan sidebar code. | PARITY_MAP H (other) |

Windows-only, not candidates for iOS (Obsolete or Replaced):

- Microsoft Access .mdb access, Compact/Repair (Replaced by SQLite plus direct .ost/.osp import)
- Realtime sync with the OST desktop app and OST-active edit blocking
- Licensing, update check, About dialog
- Windows installer and registry file associations (iOS registers the UTTypes in Info.plist instead)
- Desktop menu bar, toolbars, sidebars, status bar, keyboard shortcuts, toolbar customization, Options dialog, cursor preferences, theme following (Replaced by UIKit screens)
- MCP stdio helpers and the local pipe bridge, SQL credentials in Windows Credential Manager

## Needs device or fixture validation

- A first build of project.yml with XcodeGen and xcodebuild, and a first run of build.yml on a runner (never done).
- Compilation of every Objective-C/Objective-C++ file (Controllers, PlanView, Services, Rendering/*.mm, SceneDelegate.mm): never compiled in this job.
- Install and launch of the fake-signed IPA on a device; .ost/.osp open-in and document-picker import.
- bgfx/Metal on device: rendering, OIT passes, picking readback, selection highlight, frame time and memory on large projects.
- Manifold on device: link of libmanifold.a, runtime robustness and time with many negatives and drop stubs; host run with -DOST_TEST_BOOLEAN=ON.
- Representative .ost and .osp fixtures compared with Windows rows and loaded fields (none in the repo).
- PDF background scaling, rotation, multi-page files and extracted OSP paths.
- Export on device: share sheet, large outputs, write failures; the HTML viewer needs network (unpkg.com).
- Behaviour tests for the 108 core-cpp divergences, and extraction of logic from .mm into Core for the 95 needs-extraction-from-objc ones.
- Entitlements and signing: Config/Entitlements.plist (Nyxian keys) and the minimal fake-sign set in Config/Entitlements.fakesign.plist, with the installer actually used.
- iPad layout (popovers are configured in the sources; not seen running).

## Build, CI and tests

Two build paths exist. Both read the same source folders; neither was run in this job.

| Path | Files | State |
|---|---|---|
| Nyxian (on-device build, original path) | `Config/Project.plist`<br>`Config/Entitlements.plist` | Unchanged by Phase 0. Not re-run in this job. Entitlements.plist carries com.nyxian.pe.* keys. |
| GitHub Actions IPA build (Phase 0) | `project.yml`<br>`.github/workflows/build.yml`<br>`BUILD.md`<br>`Config/Info.plist`<br>`Config/Entitlements.fakesign.plist` | Defined, uncommitted, never run. XcodeGen spec has one app target (iphoneos, arm64, min iOS 14.8, gnu++17, links libmanifold plus four bgfx libraries and the system frameworks). The workflow (macos-15, Xcode 16.4) runs host unit tests, checks libmanifold.a is arm64, lints plists, restores or builds bgfx/bx/bimg from pinned SHAs, runs xcodegen and xcodebuild (Release, unsigned), checks the bundle, fake-signs with ldid using Config/Entitlements.fakesign.plist (only get-task-allowed; fails on any memory/jetsam entitlement), zips OST-Visualizer.ipa and uploads it (30 days). Triggers: every push and manual dispatch. |
| Dependency workflows (older) | `.github/workflows/build-bgfx-ios.yml`<br>`.github/workflows/build-manifold-ios.yml` | Manual (workflow_dispatch) builds of bgfx and Manifold artifacts only. Unchanged. |

Tests:

- Config/Tests (CMake, C++17): builds Core/*.cpp, sqlite3.c, pugixml.cpp and Rendering/MeshLoader.cpp as ost_core, with a header-only harness (TestHarness.hpp, TestMain.cpp). Area folders Config/Tests/<Area>/test_*.cpp are picked up by CMake glob. Boolean ops are a throwing stub unless -DOST_TEST_BOOLEAN=ON; zlib is downloaded on Windows.
- Areas present: `Harness` only (10 self-tests). **iOS behaviour tests: 0.** Config/Tests/Fixtures is empty (.gitkeep). No .ost or .osp sample exists in the repo.
- Phase 0 build with MSVC on the authoring machine: 10 of 10 harness self-tests passed (session log, not committed). The macOS CI run of the same command was not done.
- Phase 1 compared iOS Core C++ (MSVC host build) with the Windows Python/native modules using throw-away harnesses outside the repo; none are committed as tests. Testability of the 216 divergences: core-cpp 108, needs-extraction-from-objc 95, objc-only 13.
- `build.yml` runs the host tests (`ctest --no-tests=error`) after selecting Xcode and installing XcodeGen and ldid and before any library or app build, and stops the job on failure. Nyxian compiles every source file outside the top-level `Config/` and `Resources/` folders (per `BUILD.md`, installed Nyxian not inspected), so the tests must stay under `Config/`.

Dependencies and settings:

- Libs/Manifold/lib/libmanifold.a is tracked (833,584 bytes; built for iOS 16.0 per BUILD.md); arm64 is checked only by the CI step.
- bgfx, bx, bimg headers are tracked under Libs/bgfx; the four bgfx static libraries are not tracked (.build/ is gitignored). Local copies dated 2026-03-29 exist in this working tree; CI rebuilds them from pinned SHAs.
- Shaders are compiled offline; the app includes the 8 generated headers in Rendering/Shaders/generated. The .sc sources are not compiled by project.yml.
- Resources/ is empty; the app ships no resources.
- Minimum iOS is 14.8 but libmanifold.a and the bgfx libraries target 16.0 (linker warnings expected; iOS below 16 untested, BUILD.md).
- .gitignore (modified, uncommitted) adds /build/, *.xcodeproj/, DerivedData/, /deps/. The Xcode project is generated, never committed.

## Related files

- `PARITY_MAP.md`: all divergences, per-area feature table (section G), Windows-only features (H), stale claims (I).
- `PORTING_BLOCKERS.md`: active blockers, resolved or replaced blockers, partial gaps with ids.
- `PORTING_STATUS.json`: the same facts in machine-readable form.
- `BUILD.md`: CI build, fake-signing, bgfx pin, local unit tests.
- `PORTING_LOG.md`: chronological history; entries before 2026-11-15 are not corrected and contain stale claims (see `PARITY_MAP.md` section I).
