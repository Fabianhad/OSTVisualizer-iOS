# OST Visualizer - iOS Porting Blockers

Last updated: 2026-05-15

This file tracks source-confirmed blockers, source-confirmed replacements, partial parity gaps, and items that still require build/device or fixture validation. Status names match the other tracking files: **Ported**, **Partial**, **Missing**, **Replaced**, **Obsolete**, **Needs verification**.

## Active Blockers

### B-004: PDF Annotation Export
- **Status**: Missing
- **Desktop evidence**: `cpp_extensions/src/pdf/pdf_writer.cpp`, `cpp_extensions/src/pdf/bluebeam_annotation.cpp`, `ost_visualizer/presentation/visualization/exporters/pdf_exporter.py`, QPDF references in `cpp_extensions/CMakeLists.txt`.
- **iOS evidence**: No QPDF, `pdf_writer`, PDFKit writer, or PDF annotation export path found. `Core/Export` contains OBJ, DXF, and Three.js writers only.
- **Impact**: Blocks PDF annotation export and Bluebeam-style annotation round trip. Does not block PDF viewing, 3D rendering, OBJ/DXF/Three.js export, or `.ost`/`.osp` import.

### B-009: OST/OSP Export Round Trip
- **Status**: Missing
- **Desktop evidence**: `ost_visualizer/presentation/visualization/exporters/ost_exporter.py`, `osp_exporter.py`, and export interfaces under `ost_visualizer/application/interfaces`.
- **iOS evidence**: `Services/ExportService.mm` exposes only `exportOBJ`, `exportDXF`, and `exportThreeJS`; no `.ost` or `.osp` writer was found.
- **Impact**: iOS can import `.ost`/`.osp` but cannot export edited projects back to desktop-compatible OST/OSP packages.
- **Notes**: This is an implementation blocker, not an external-library blocker.

## Resolved / Replaced Blockers

### B-001: GLM Availability
- **Status**: Replaced
- **Source-confirmed state**: Current iOS meshing and curved-length code uses portable C++ math in `Core/Meshing/*` and `Core/Domain/UomService.hpp`. No active iOS source dependency on GLM was found.
- **Remaining validation**: Geometry parity for curved/sloped cases still needs fixture comparison against desktop.

### B-002: Manifold / Boolean Operations
- **Status**: Ported
- **Source-confirmed state**: Manifold headers and `Libs/Manifold/lib/libmanifold.a` are present, `Config/Project.plist` links `-lmanifold`, `Core/Geometry/boolean_ops.cpp` includes Manifold headers, and `Services/ProjectService.mm` / `Services/ExportService.mm` call `boolean_difference_batch`.
- **Remaining validation**: Clean build/link and stress tests for dense negative takeoffs, drop stubs, and complex meshes.

### B-003: PDFium / PDF Viewing
- **Status**: Partial
- **Source-confirmed state**: PDF viewing uses CoreGraphics/CGPDF in `PlanView/Components/PDFCanvasView.mm`; no iOS PDFium dependency was found.
- **Important limitation**: `PDFCanvasView.mm` always draws `CGPDFDocumentGetPage(_pdfDoc, 1)`. Multi-page PDF page selection is not source-confirmed.
- **Remaining validation**: Device validation for page scaling, rotation, and OSP file path rewriting.

### B-005: MDB / Access Database Access
- **Status**: Replaced
- **Source-confirmed state**: iOS imports `.ost` XML and `.osp` CAB packages, then stores data in SQLite through `Core/Import`, `Core/Persistence`, and `Services/ImportService.mm`. MDB/pyodbc is not used on device.
- **Remaining validation**: Fixture comparison against desktop imports and confirmation that non-loaded imported tables survive iOS edits.

### B-006: PySide6 / Qt UI Replacement
- **Status**: Replaced
- **Source-confirmed state**: UIKit controllers and views under `Controllers/`, `PlanView/`, `AppDelegate.*`, and `SceneDelegate.mm` replace desktop Qt/PySide for the iOS scope.

### B-007: File Import And Non-Project Export Workflow
- **Status**: Partial
- **Source-confirmed state**: `.ost` and `.osp` UTTypes, Files/share-sheet import, security-scoped reads, SQLite import, and OBJ/DXF/Three.js share flows are present.
- **Limitation**: Project round-trip export remains blocked by B-009; PDF annotation export remains blocked by B-004.

### B-008: Renderer Backend Limitations
- **Status**: Replaced
- **Source-confirmed state**: `Rendering/Renderer.mm` initializes `bgfx::RendererType::Metal`, `Rendering/BgfxView.mm` uses `CAMetalLayer`, generated shader headers are included, and `Config/Project.plist` links bgfx plus Metal/MetalKit.
- **Remaining validation**: Clean build/link on a fresh checkout and device GPU testing.

## Partial Porting Gaps

| Gap | Status | Source-confirmed state |
|---|---|---|
| Count shape mesh parity | Partial | iOS defines shape IDs and generates box/cylinder count meshes. Desktop has ellipse width/depth, triangle, rhombus, hexagon, and octagon-specific paths that iOS does not implement. |
| Metric UOM support | Missing | Desktop UOM service supports codes 11-16 and metric/imperial normalization. iOS `UomService.hpp` defines only imperial codes 0-9. |
| Attachment quantity parity | Partial | iOS quantity formulas accept attachment footprint/perimeter, but `compute_page_quantities` passes TODO zero values. |
| PDF/page rendering fidelity | Partial | PDF drawing exists for the first page of the referenced PDF path. Overlay path fields are loaded but not rendered/composited by `PlanView`. Bitonal/invert/raster modes are not implemented. |
| Annotation data/UI | Missing | Desktop has annotation entities, renderers, write services, and PDF export. iOS schema stores annotation tables, but no iOS domain/UI renderer for annotations was found. |
| Desktop snap/backout workflows | Partial | iOS has grid and angle snapping, placement, drag, delete, negative toggle, and focused undo/redo. Desktop endpoint/midpoint/perpendicular snap index, backout mode, and paste-backout are not ported. |
| Full desktop write/use-case surface | Partial | iOS writes common takeoff, condition, layer, folder, and render-config changes. Desktop project/labor/annotation/settings workflows are broader. |
| Condition calculation settings | Partial | iOS loads/edits/persists `round_quantity` and `round_up`, but no source-confirmed rounding application was found in `UomService.hpp`; desktop also has metric defaults and settings conversion paths. |
| Export error/progress UX | Partial | OBJ/DXF/Three.js share flows exist and show a basic "no visible meshes" alert. File-write failures return no URLs; device share-target behavior and user-visible failure coverage need validation. |
| Automated tests and fixtures | Missing | Desktop has pytest coverage under `tests/`. No iOS app test target, fixture suite, or app CI workflow was found; existing GitHub workflows build dependency artifacts only. |
| Build/signing configuration | Needs verification | `Config/Project.plist` carries manual link/resource settings and `Config/Entitlements.plist` contains nonstandard `com.nyxian.pe.*` entitlements. No `.xcodeproj`/workspace was found in this repo inventory. |

## Needs Manual Validation

- Clean build/link from a fresh checkout with current bgfx and Manifold libraries.
- Manual bgfx and Manifold dependency artifact workflows under `.github/workflows/` and integration of their outputs into the app build.
- Device run for bgfx/Metal rendering, picking readback, and memory/frame-time behavior.
- Representative `.ost` and `.osp` imports, including CAB variants, PDFs, duplicate globals, folders, layers, and page paths.
- Desktop-vs-iOS schema/import comparisons for row counts, key fields, and preservation of tables not loaded into `ProjectData`.
- Desktop-vs-iOS quantity comparisons for metric UOMs, rounding, attachments, holes, negatives, slope, grid, and curved lengths.
- Manifold stress tests for dense negative takeoffs and drop-run unions.
- OBJ/DXF/Three.js exports on device, including share-sheet delivery, write failures, empty projects, and large mesh exports.
- Import/export user-facing error handling, progress behavior, and cancellation behavior.
- Build signing, entitlements, and distribution viability for the current `Config/*.plist` setup.
- Fixture-backed regression tests or CI for import, persistence, quantities, PDF/page mapping, mesh generation, and exports.

## Deferred / Non-MVP Items

- **FBX export**: Desktop has an exporter; iOS has no FBX SDK/replacement.
- **Desktop licensing/update systems**: Obsolete/replaced for current iOS scope; no iOS equivalent found.
- **Windows transaction monitoring**: Obsolete on iOS.
- **Payroll/employees/job statuses/cover sheet dialogs**: Desktop business workflows are outside current MVP.
- **Detached desktop windows and viewer sync**: Not applicable unless iPad multiwindow scope is added.

## Obsolete / Historical Blockers

- **"Metal is not available / software rasterizer is production"**: Obsolete. Current source uses bgfx/Metal.
- **"Desktop OpenGL renderer must be ported directly"**: Obsolete. Replaced by bgfx/Metal.
- **"PDFium must be built for PDF viewing"**: Obsolete for basic viewing. CoreGraphics PDF drawing is present.
- **"MDB/pyodbc required on iOS"**: Obsolete. Direct `.ost`/`.osp` import and SQLite storage are present.
- **"PySide6 blocks UI"**: Obsolete. UIKit UI is present.
- **"GLM blocks curved meshes"**: Obsolete. Curved mesh code does not depend on GLM.
