# OST Visualizer - iOS Porting Blockers

Last updated: 2026-11-15

Scope: blockers that are real in the code as of 2026-11-15 (iOS HEAD `4a1302d` plus uncommitted Phase 0/1 files), partial gaps with `PARITY_MAP.md` ids, and what was resolved or replaced. Status vocabulary is the one in `PORTING_STATUS.md` (Implemented-matches-Windows, Implemented-diverges, Implemented-unverified, Replaced, Not implemented, Obsolete), plus 'Needs verification' for items that need a build or device run, which cannot be done on the authoring machine (Windows, no Xcode). Divergence ids (QTY-01, MSH-01, ...) are defined in `PARITY_MAP.md`.

## Active blockers

### B-004: PDF annotation export
- **Status**: Not implemented
- **Current state**: No PDF writer on iOS. Search found no PDFKit, UIGraphicsPDFRenderer, CGPDFContext, qpdf or pdf_writer; the only PDF code is CGPDFDocument reading in PlanView/Components/PDFCanvasView.mm. ExportService exposes OBJ, DXF and Three.js only. iOS also has no annotation model (annotation rows are written only by the generic .ost import and deleted only on permanent bid delete and together with the takeoff or condition that links them).
- **Impact**: Blocks exporting takeoffs and annotations as PDF markup. Does not block viewing, 3D rendering, OBJ/DXF/Three.js export or import.
- **Windows evidence**: `cpp_extensions/src/pdf/pdf_writer.cpp`, `cpp_extensions/src/pdf/bluebeam_annotation.cpp`, `ost_visualizer/presentation/visualization/exporters/pdf_exporter.py`, `cpp_extensions/CMakeLists.txt (QPDF)`
- **iOS evidence**: `Services/ExportService.h`, `Core/Export/ (OBJ, DXF, Three.js writers only)`, `PlanView/Components/PDFCanvasView.mm`

### B-009: OST/OSP export (round trip)
- **Status**: Not implemented
- **Current state**: No .ost or .osp writer and no CAB writer (no deflate/compress call anywhere; CabExtractor.cpp only inflates). A writer would also need data iOS does not keep: BidEmployees and PayClasses rows are never parsed (IMP-11) and column types are reduced to INTEGER/TEXT/REAL/BLOB (IMP-05, IMP-20).
- **Impact**: iOS can import .ost/.osp but cannot write an edited project back to a desktop-compatible package.
- **Windows evidence**: `ost_visualizer/infrastructure/mdb/exporters/ost_exporter.py`, `ost_visualizer/presentation/visualization/exporters/osp_exporter.py`
- **iOS evidence**: `Services/ExportService.h`, `Core/Import/CabExtractor.cpp (read only)`
- **PARITY_MAP ids**: IMP-05, IMP-11, IMP-20
- **Correction**: Earlier docs cited presentation/visualization/exporters/ost_exporter.py, which does not exist.

### B-010: No iOS build or device run on record
- **Status**: Needs verification
- **Current state**: Phase 0 added project.yml, build.yml and BUILD.md, all uncommitted. Nothing was built or launched: the authoring machine is Windows (no Xcode, xcodebuild, XcodeGen, ldid, macOS or device). Every statement about Objective-C/UIKit/bgfx behaviour in these docs comes from reading source.
- **Impact**: Unknown whether project.yml generates a project that compiles and links, and whether the app runs.
- **iOS evidence**: `project.yml`, `.github/workflows/build.yml`, `BUILD.md`

### B-011: No iOS behaviour tests or fixtures
- **Status**: Not implemented
- **Current state**: Config/Tests has a harness with 10 self-tests and 0 iOS behaviour tests; Fixtures/ is empty; the repo has no .ost/.osp sample. 95 of the 216 divergences cannot be host-tested until their logic is moved from .mm files into Core; 13 are objc-only.
- **Impact**: No regression protection for any fix; parity can only be re-measured with throw-away harnesses.
- **iOS evidence**: `Config/Tests/`, `Config/Tests/Fixtures/`

### B-012: Data-safety divergences in import, persistence and delete
- **Status**: Implemented-diverges
- **Current state**: 45 of the 216 divergences are data-safety (41 fix, 4 needs-decision). Examples: importing a second project can overwrite rows of the first (IMP-06, INSERT OR REPLACE with original UIDs, host-verified in Phase 1); import commits and reports success after SQL failures (IMP-08); every SQL error is swallowed (DBP-11); cascades leave orphans (DBP-12, DBP-14, COND-DEL-01); swipe-delete has no confirmation (BRW-01).
- **Impact**: Silent data loss or partial import is possible on real projects.
- **iOS evidence**: `Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp`, `Core/Persistence/ProjectStore/ProjectStore.cpp`, `Controllers/DatabaseViewController.mm`
- **PARITY_MAP ids**: IMP-06, DBP-11, IMP-08, DBP-12, COND-DEL-01, IMP-07, IMP-09, BRW-01, BRW-02, DBP-14, DBP-17, DBP-18, DBP-20, GEO-09, UND-02, UND-03, COND-UPD-02, COND-UPD-01, COND-UPD-03, COND-DUP-01, RPV-15, RPV-16, RPV-M01, IMP-12, IMP-13, IMP-10, IMP-11, DBP-35, DBP-M02, DBP-08, EXP-08, EXP-09, BRW-05, COND-STYLE-01, COND-REF-01, COND-ELEV-01, UND-06, UND-08, COL-PICK-01, COND-M05, IMP-M02, GEO-M02, POS-M01, IMP-M01, DBP-31

## Resolved or replaced

| Id | Item | Status | Verified reason | Remaining caveat |
|---|---|---|---|---|
| B-001 | GLM availability | Replaced | No glm reference exists in Core, Rendering, Services, Controllers or PlanView; curved geometry uses portable C++ math (Core/Meshing/gen_curved.cpp, Core/Domain/UomService.hpp). | Dependency resolved, parity not: the curve code is a reimplementation and differs from Windows (QTY-07, QTY-08, QTY-09, MSH-05, MSH-06, MSH-07). Earlier docs called this a fixture-only check. |
| B-002 | Manifold / boolean operations | Replaced (dependency) | Libs/Manifold/include and lib/libmanifold.a are tracked, project.yml and Config/Project.plist link -lmanifold, Core/Geometry/boolean_ops.cpp differs from the Windows file only in include lines and a stale TODO comment on line 1, ProjectService.mm and ExportService.mm call boolean_difference_batch. | Link and runtime on iOS not verified (B-010). Orchestration diverges (MSH-14, MSH-15, MSH-16, MSH-19, MSH-19b, EXP-05). |
| B-003 | PDFium / PDF viewing | Replaced | CoreGraphics/CGPDF in PlanView/Components/PDFCanvasView.mm; no PDFium dependency. | Draws page 1 only and ignores page_index, rotation and flip (LP-16). The earlier 'only if multi-page PDFs are required' condition is wrong: Windows always uses page_index = Index1-1 (per PARITY_MAP). |
| B-005 | MDB / Access database access | Replaced | Direct .ost XML and .osp CAB import into local SQLite (Core/Import, Core/Persistence, ImportService.mm); no MDB/pyodbc on device. | Import diverges (B-012, IMP-*); BidEmployees and PayClasses are not imported (IMP-11). |
| B-006 | PySide6 / Qt UI | Replaced | UIKit controllers and views under Controllers/ and PlanView/, AppDelegate.m, SceneDelegate.mm. | Windows menus and dialogs are broader (see Not implemented in PORTING_STATUS.md). |
| B-007 | File import and non-project export workflow | Superseded | Import (.ost/.osp UTTypes, document picker, open-in) and OBJ/DXF/Three.js share flows exist (Info.plist, DatabaseViewController.mm, ViewController+Export.mm). | Remaining export gaps are B-004 and B-009; content divergences are in the gaps table. |
| B-008 | Renderer backend | Replaced | Rendering/Renderer.mm initialises bgfx RendererType::Metal; BgfxView.mm uses CAMetalLayer; generated shader headers are included; project.yml links the bgfx libraries plus Metal and MetalKit. | Never run on a device (B-010). |

Obsolete or historical claims (do not reuse):

- 'Metal is not available / software rasterizer is production': obsolete (bgfx on Metal).
- 'Desktop OpenGL renderer must be ported directly': obsolete (replaced by bgfx/Metal).
- 'PDFium must be built for PDF viewing': obsolete for viewing (CGPDF).
- 'MDB/pyodbc required on iOS': obsolete (SQLite plus direct import).
- 'PySide6 blocks UI': obsolete (UIKit).
- 'GLM blocks curved meshes': obsolete (no GLM use).
- 'No .xcodeproj, CI or test target': no longer accurate, see Corrections.

## Partial gaps

Ids are canonical `PARITY_MAP.md` ids (aliases are listed in its appendix). Counts per owner area are in `PORTING_STATUS.md`.

| Gap | Status | PARITY_MAP ids | Current state |
|---|---|---|---|
| Data-safety (import, persistence, delete) | Implemented-diverges | IMP-06, DBP-11, IMP-08, DBP-12, COND-DEL-01, IMP-07, IMP-09, BRW-01, BRW-02, DBP-14, DBP-17, DBP-18, DBP-20, GEO-09, UND-02, UND-03, COND-UPD-02, COND-UPD-01, COND-UPD-03, COND-DUP-01, RPV-15, RPV-16, RPV-M01, IMP-12, IMP-13, IMP-10, IMP-11, DBP-35, DBP-M02, DBP-08, EXP-08, EXP-09, BRW-05, COND-STYLE-01, COND-REF-01, COND-ELEV-01, UND-06, UND-08, COL-PICK-01, COND-M05, IMP-M02, GEO-M02, POS-M01, IMP-M01, DBP-31 | See B-012. 45 ids; the full list is shown here because it is the highest-risk class. |
| Metric measurement | Not implemented | QTY-05, QTY-06, QTY-12, QTY-16, COND-METRIC-01, ELV-02 | UomService.hpp defines UOM codes 0-9. Codes 11-16 fall through convert_to_uom (divisor 1.0, raw inches) and get_uom_label (empty string). Bid.measure_base is loaded but only snap_increments_ost reads it. Labels for codes 4 and 7 differ and roofing is missing from the area picker (QTY-16). |
| Attachment and hole aggregation | Implemented-diverges | QTY-01, QTY-02, QTY-03, QTY-18, RPV-17, MSH-14 | compute_page_quantities skips every takeoff with a parent_uid (UomService.hpp:767), so attachments placed on areas count 0, and passes 0.0 as attachment footprint and perimeter (UomService.hpp:790). Any child counts as a hole. 2D, 3D and exports drop attachments that have a parent. |
| Quantity rounding | Not implemented | QTY-04 | round_quantity and round_up are edited, persisted and loaded, never applied. Not deferrable under the parity rule. |
| Quantity scope, formatting and arcs | Implemented-diverges | QTY-07, QTY-08, QTY-09, QTY-10, QTY-11, QTY-12, QTY-14, QTY-15, QTY-22, QTY-23 | Sidebar totals mix the 2D page and the 3D page set (QTY-11) and are not refreshed on page or mode change (QTY-15). Arc length, slope factor and number formatting differ. |
| 3D mesh pipeline | Implemented-diverges | MSH-01, MSH-02, MSH-14, MSH-15, MSH-20, GEO-05, GEO-14, GEO-15 | Page-scale division of all geometry (MSH-01), no page rotation/flip (MSH-02), area slope and tessellation. |
| Curved wall meshes | Implemented-diverges | MSH-05, MSH-06, MSH-07 | Segment count, arc side and straight-chord rule differ. |
| Count shape meshes | Implemented-diverges | MSH-08, MSH-09, MSH-10, MSH-11, MSH-13, RPV-25 | Only a box and a 24-segment cylinder exist. Ellipse draws as a circular cylinder (depth ignored); triangle, rhombus, hexagon and octagon draw as boxes. |
| PDF and page rendering fidelity | Implemented-diverges | LP-15, LP-16, GEO-06, RPV-02, RPV-03, RPV-M04 | Page 1 only, stored page size, no rotation/flip of the PDF; Image layer ignored; overlay fields loaded but never drawn; no bitonal/invert/adjust; no TIFF. |
| Annotations | Not implemented | - | No annotation model, renderer, editor or export. Tables exist in the schema. Display Name / Display Dimension flags are stored but not drawn. |
| Placement, drag and snapping | Implemented-diverges | RPV-04, RPV-05, RPV-06, RPV-07, RPV-08, RPV-09, RPV-10, RPV-13, RPV-15, RPV-20, RPV-M01, RPV-M03, GEO-09, GEO-M02 | Angle snap 22.5 degrees (Windows 15), unconditional per-axis grid rounding, no endpoint/midpoint/PDF-line snap, no backout mode or paste backout. |
| Write and use-case surface | Not implemented | DBP-17, DBP-18, DBP-35 | No page writes, layer create/rename/delete, condition create, condition-folder CRUD, bid create/duplicate or cover-sheet editing (searched). Writers do not validate existence or ownership. |
| Condition editor correctness | Implemented-diverges | COND-UPD-01, COND-UPD-02, COND-UPD-03, COND-STYLE-01, COND-REF-01, COND-ELEV-01, COND-M05, COL-PICK-01, DLG-PREC-01, DLG-RISE-01, DLG-FINITE-01, UI-M01 | Update rewrites every column and ColorLine, clears to '' instead of NULL, style change wipes notes and edits, ref number is coerced, elevation text is reformatted on blur. |
| Elevation and position text | Implemented-diverges | ELV-01, ELV-02, ELV-03, ELV-04, ELV-05, ELV-06, FMT-01, DBP-07, DBP-08, POS-M01 | extract_z_value is not the Windows grammar (ELV-01 to ELV-06); inchesToDisplay rounds half away from zero (FMT-01); position text is written with %.10g (DBP-07) and parsed leniently (DBP-08, POS-M01). |
| Export content | Implemented-diverges | EXP-01, EXP-02, EXP-03, EXP-05, EXP-06, EXP-08, EXP-09, EXP-11, EXP-12, EXP-13, EXP-14, EXP-15, EXP-16, EXP-19, EXP-20, EXP-21, MSH-16 | Three.js and DXF axes, mirrored export holes, per-takeoff instead of per-condition groups, no grayscale/opacity, unescaped HTML payload, raw bid name as file name; one alert for failure and empty export. |
| OSP package rules | Implemented-diverges | IMP-12, IMP-13, IMP-14, IMP-15, IMP-16, IMP-17, IMP-18, IMP-M02, IMP-M03 | Basename-only staging collides, corrupt CAB blocks skipped silently, no package validation, only stored/MSZIP, member names not decoded. |
| Layers and page selection | Implemented-diverges | LP-08, LP-10, LP-11, LP-12, LP-18, LP-22, LP-24, LP-25, RPV-16 | Pages ordered by Index1/Name instead of Sequence/Name/UID; folder tap replaces selection; Comments layer listed; initial page differs. |
| Render settings and colour | Implemented-diverges | SET-01, SET-02, SET-03, COL-01, COL-02, COL-03, COL-04 | Default mode Solid (Windows Original), one mode for 2D and 3D, negative ColorFill drawn grey, alpha rules differ. |
| Undo and redo | Implemented-diverges | UND-01, UND-02, UND-03, UND-06, UND-07, UND-08, UND-09, UND-10, UND-11 | Move, place and delete only; unbounded history; delete cascade covers direct children only. |
| Camera | Implemented-diverges | CAM-01, CAM-02, CAM-03, CAM-04 | Fit/Reset framing and pinch clamp differ; camera is refit instead of kept per bid. |
| Project browser | Implemented-diverges | BRW-01, BRW-02, BRW-05, BRW-08, BRW-09, BRW-10, BRW-11, BRW-12, BRW-13, BRW-19, BRW-21, BRW-M01 | Deleted Bids folder is identified by name (UID '1' on Windows); restore ignores OrigBidProjectUID; non-empty folders can be deleted. |
| Build and signing configuration | Needs verification | - | Config/Entitlements.plist (Nyxian) carries 25 com.nyxian.pe.* keys; the CI path signs with a minimal get-task-allowed set; minimum iOS 14.8 vs libraries built for 16.0. See B-010. |

## Needs manual validation

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

## Deferred or not planned

- FBX export, Summary CSV export, AI takeoff, SQL Server collaboration, detached windows, desktop business workflows (payroll, employees, job statuses, cover sheet): Not implemented, outside the current iOS scope (`PORTING_STATUS.md`, Not implemented).
- Windows-only platform features (Access/MDB, desktop licensing and updates, Windows installer, realtime sync with the OST desktop app): Obsolete on iOS.

## Corrections to the previous version of this file

| Earlier claim | Reality on 2026-11-15 |
|---|---|
| 'No .xcodeproj/CI/test target' | Partly outdated. project.yml generates an Xcode project (not committed, *.xcodeproj/ is gitignored), build.yml builds a fake-signed IPA, Config/Tests is a host harness. All uncommitted and never run; there is no Xcode test target and 0 iOS behaviour tests. |
| Render config lives in ProjectStore+RenderConfig.cpp | That file does not exist. load_render_config and save_render_config are in Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp (lines 263-290), using _AppPreferences. |
| 'iOS UomService.hpp defines only imperial codes 0-9' (metric) | Confirmed, and understated: codes 11-16 do not error, they convert with divisor 1.0 and an empty label. UomService.hpp has no metric lists, no normalisation; Bid.measure_base is read only by snap_increments_ost. |
| 'compute_page_quantities passes TODO zero values' (attachments) | Confirmed (UomService.hpp:790), and incomplete: the same function also skips every takeoff with a parent_uid (UomService.hpp:767), so an attachment's own count is 0 (QTY-01). |
| 'No source-confirmed rounding application' (round_quantity/round_up) | Confirmed: the fields are only loaded, edited and saved. It is a parity bug, not a deferral (QTY-04). |
| OST exporter evidence path presentation/visualization/exporters/ost_exporter.py | Does not exist. The Windows OST exporter is ost_visualizer/infrastructure/mdb/exporters/ost_exporter.py; the OSP exporter is presentation/visualization/exporters/osp_exporter.py. |
| Ported / Partial labels for import, schema, persistence, meshing, booleans, exports, project browser | Replaced by the vocabulary in PORTING_STATUS.md. Phase 1 found divergences in every one of these areas (PARITY_MAP.md section I lists the stale claims). |
| Dependency workflows are the only CI | Still true in HEAD; build.yml now exists in the working tree but is untracked. |
