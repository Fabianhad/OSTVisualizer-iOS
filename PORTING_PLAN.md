# OST Visualizer - iOS Porting Plan

Last updated: 2026-05-15

The current source contains an MVP implementation path, but two project-output blockers, clean build/device validation, and desktop fixture parity are still required. Remaining work is ordered as blockers first, then parity gaps, then repeatable validation.

## Priority 0 - Active Blockers

- [ ] Decide whether iOS must export edited projects as `.ost`/`.osp` for MVP.
- [ ] If required, port a scoped OST exporter from desktop `ost_exporter.py`.
- [ ] If required, port OSP package export or define a server/desktop handoff.
- [ ] Add round-trip fixture tests for import, edit, export, and desktop reopen.
- [ ] Choose implementation path: QPDF iOS build, PDFKit/CoreGraphics writer, or custom scoped annotation writer.
- [ ] Port the minimum takeoff annotation export needed for product workflow.
- [ ] Add PDF export fixture comparisons against desktop Bluebeam/QPDF output.
- [ ] Add share workflow and user-visible errors/progress for PDF export.

## Priority 1 - Build, Device, And Fixture Validation

- [ ] Run a clean build from a fresh checkout with bgfx and Manifold linked.
- [ ] Validate the manual bgfx and Manifold artifact workflows and document how their outputs are consumed by the app build.
- [ ] Review `Config/Project.plist` and `Config/Entitlements.plist` for signing/distribution viability.
- [ ] Run on target iOS hardware and capture baseline memory/frame-time numbers.
- [ ] Validate bgfx/Metal rendering, mesh-ID picking/readback, camera controls, and share sheets on device.
- [ ] Create a shared desktop/iOS fixture set of `.ost` and `.osp` files covering pages, layers, folders, linear/area/count/attachment, holes, negatives, slope, curves, PDFs, overlays, metric UOMs, and annotations.
- [ ] Compare imported table row counts and key fields against desktop for representative fixtures.
- [ ] Verify non-loaded SQLite tables are preserved after iOS edits/deletes.
- [ ] Add an app-level CI/test target or repeatable local fixture harness; current workflows cover dependency artifacts only.

## Priority 2 - PDF/Page And Annotation Fidelity

- [ ] Replace first-page-only `PDFCanvasView` behavior with page-index-aware PDF drawing if multi-page PDFs are required.
- [ ] Validate or implement overlay image/PDF compositing using loaded overlay path, offset, rotation, resized rect, and deskew fields.
- [ ] Decide scope for bitonal/invert/raster draw modes.
- [ ] Add manual or automated checks for page rotation and coordinate transforms.
- [ ] Decide whether annotation data model/UI rendering is in scope separately from PDF annotation export.
- [ ] If annotation viewing is in scope, port a minimal annotation domain model and renderer before implementing editing.

## Priority 3 - Geometry And Quantity Parity

- [ ] Implement missing count-shape mesh paths: ellipse with width/depth, triangle prism, rhombus prism, hexagon prism, octagon prism.
- [ ] Add desktop-vs-iOS mesh comparison fixtures for all condition types and count shapes.
- [ ] Add metric UOM codes and metric/imperial normalization to `UomService.hpp`.
- [ ] Implement attachment footprint/perimeter aggregation or explicitly defer attachment calc parity.
- [ ] Apply or explicitly defer `round_quantity` / `round_up` behavior in calculated totals; fields are currently modeled, edited, and persisted.
- [ ] Verify rounding, grid counts, holes, negative quantities, slope, and curved length calculations against desktop.

## Priority 4 - Editing, Workflow, And Error Handling Parity

- [ ] Decide whether desktop backout mode and paste-backout are required on iOS.
- [ ] Port endpoint/midpoint/perpendicular snap index if precision placement parity is required.
- [ ] Expand undo/redo scope if condition edits, layer edits, folder actions, or project actions need undo parity.
- [ ] Validate condition duplicate/delete and takeoff delete cascades against desktop behavior.
- [ ] Validate import errors, export write failures, cancellations, and empty/large project behavior on device.
- [ ] Decide scope for page settings editing, desktop clipboard/menu commands, and broader project write workflows.

## Priority 5 - Deferred Desktop Features

- [ ] Evaluate FBX export only if product scope requires it.
- [ ] Keep desktop licensing/update, Windows monitoring, payroll, employee, cover sheet, and detached-window workflows out of MVP unless scope changes.

## Source-Confirmed Work

- [x] Native UIKit app shell and scene URL handling.
- [x] `.ost` XML parser wired into import service.
- [x] `.osp` CAB extraction path with `.ost` and PDF extraction.
- [x] SQLite project store with all desktop table names plus iOS metadata.
- [x] Project browser and folder organization source paths.
- [x] Domain structs for current visualization/editing scope.
- [x] Linear, curved linear, area-with-holes, count/attachment base mesh generation.
- [x] Manifold boolean code linked and called from render/export paths.
- [x] bgfx/Metal renderer source path with generated shaders, buffers, selection, and picking.
- [x] 2D plan view with takeoff overlays and first-page PDF canvas.
- [x] Takeoff placement, drag, delete, negative toggle, and focused undo/redo.
- [x] Layer visibility and condition editing source paths.
- [x] OBJ, DXF, and Three.js writer/share source paths.

## Current Decisions

| Decision | Current answer |
|---|---|
| Renderer backend | bgfx with Metal backend in source; clean build/device validation pending |
| Desktop OpenGL renderer | Replaced on iOS |
| Software rasterizer | Historical only; no current claim as production path |
| GLM | Not required by current iOS source |
| Manifold | Source-present, linked, and called; runtime stress validation pending |
| PDFium | Not required for basic PDF viewing; replaced by CoreGraphics first-page drawing |
| QPDF | Still unresolved for PDF annotation export |
| MDB/Access | Replaced by direct `.ost`/`.osp` import and SQLite |
| UI framework | UIKit |
| MVP export formats | OBJ, DXF, Three.js source paths only; share/device validation pending |
