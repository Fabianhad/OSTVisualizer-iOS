# OST Visualizer - iOS Porting Status

Last updated: 2026-05-15

## Current Summary

The iOS port has a source-confirmed MVP path for importing `.ost`/`.osp`, storing projects in SQLite, browsing projects, viewing/editing takeoffs, generating meshes, rendering through bgfx/Metal, applying Manifold boolean subtraction in render/export mesh generation, and sharing OBJ/DXF/Three.js files.

This is not desktop parity. Runtime/build validation was not performed in this documentation pass, and several source-present systems still need fixture or device validation before they should be treated as complete.

## Source-Confirmed Systems

| System | Status | Evidence | Notes |
|---|---|---|---|
| iOS app shell | Replaced | `Main.m`, `AppDelegate.*`, `SceneDelegate.mm` | Native UIKit lifecycle and URL import handling replace Python/Qt bootstrap. |
| `.ost` import path | Ported | `SceneDelegate.mm`, `ImportService.mm`, `OstXmlParser.cpp` | File URL handling and parser are wired into SQLite import. Needs fixture validation. |
| `.osp` import path | Partial | `SceneDelegate.mm`, `ImportService.mm`, `CabExtractor.cpp` | CAB listing/extraction, `.ost` extraction, PDF extraction, and path rewrite code exist. Needs package-variant validation. |
| SQLite table coverage | Ported | `OstSchema.cpp`, desktop `database_creator.py` | Static comparison found all 64 desktop table names plus iOS `_AppMeta`. Exact semantic parity needs fixture tests. |
| Project browser | Ported | `DatabaseViewController.mm`, `ImportService.mm` | Project/folder list, import, delete, and folder actions are source-present. |
| Project/edit persistence | Partial | `ProjectStore+Folders.cpp`, `ProjectStore+Conditions.cpp`, `ProjectStore+Takeoffs.cpp`, `ProjectStore+RenderConfig.cpp`, `ImportService.mm` | Current-scope folder, layer visibility, takeoff, condition, and render-config writes exist. Desktop write/use-case coverage is broader. |
| Loaded app model | Partial | `Core/Domain/*`, `ProjectData.hpp`, `ProjectStore+Load.cpp` | Loads visualization/editing subset; many desktop tables remain stored but not modeled in UI. |
| Mesh generation | Partial | `Core/Meshing/*`, `Core/Geometry/*` | Linear, curved linear, area with holes, count/attachment base meshes, slope, and drop stubs are source-present. Advanced count shapes are partial. |
| Boolean operations | Ported | `boolean_ops.cpp`, `Libs/Manifold`, `ProjectService.mm`, `ExportService.mm`, `Config/Project.plist` | Manifold is linked and called in source. Runtime robustness needs stress testing. |
| 3D renderer | Ported | `Renderer.mm`, `BgfxView.mm`, `Shaders/generated`, `Config/Project.plist` | bgfx/Metal, shader programs, GPU buffers, OIT passes, selection/picking paths are present. Needs clean build/device validation. |
| 2D takeoff view | Partial | `PlanView/*`, `TakeoffOverlayView.mm` | Takeoff overlays, handles, placement, drag, selection, negative markers, and patterns/opacities are represented. Desktop style parity needs validation. |
| UIKit UI | Replaced | `Controllers/*`, `PlanView/*` | Project browser, visualization, conditions, layers, camera/export controls exist. Desktop Qt menu/dialog coverage is broader. |
| OBJ/DXF/Three.js export | Ported | `Core/Export/*`, `ExportService.mm`, `ViewController+Export.mm` | Writers and `UIActivityViewController` share flow are source-wired. Device/share validation still needed. |
| Import error handling | Partial | `ImportService.mm`, `DatabaseViewController.mm`, `SceneDelegate.mm` | Parser/import failures produce `NSError` and alerts for picker/URL imports. Progress, cancellation, and broad fixture errors need validation. |
| Dependency build workflows | Needs verification | `.github/workflows/build-bgfx-ios.yml`, `.github/workflows/build-manifold-ios.yml` | Manual workflows build dependency artifacts. They are not proof that the full iOS app builds. |

## Partial Or Missing Systems

| System | Status | Source-confirmed state | Remaining gap |
|---|---|---|---|
| OST/OSP export | Missing | No iOS `.ost`/`.osp` writer found. | Blocks project/package round trip. |
| PDF annotation export | Missing | No QPDF, PDFKit writer, or replacement annotation writer found. | Blocks PDF annotation export. |
| PDF viewing/page mapping | Partial | `PDFCanvasView.mm` uses CoreGraphics and draws page 1 of the referenced PDF. | Multi-page selection, overlay compositing, bitonal/invert/raster modes, and coordinate parity are not implemented or not proven. |
| Annotation UI/rendering | Missing | Annotation tables exist in schema; no iOS annotation domain/UI renderer found. | Desktop annotation render/edit/export workflows are absent. |
| Count condition meshes | Partial | Box and circular cylinder paths exist; ellipse currently uses the cylinder path with one diameter. | Desktop ellipse width/depth, triangle, rhombus, hexagon, and octagon prism behavior is missing. |
| Quantity calculations | Partial | Imperial formulas, holes, slope, curved length, grid, and negative aggregation exist in `UomService.hpp`. | Metric UOMs, metric/imperial normalization, attachment footprint/perimeter aggregation, rounding parity, and fixture tests are missing. |
| Layers/pages | Partial | Layer visibility, page selection, page folders, page dimensions, scale, rotation, image path, and overlay metadata fields are loaded. | Overlay rendering and full page settings parity are not implemented/proven. |
| Plan editing | Partial | Placement, drag, delete, undo/redo, negative toggle, and condition edits exist. | Desktop backout mode, paste-backout, endpoint/midpoint/perpendicular snap index, detached view sync, and broad menu actions are not ported. |
| Color/material handling | Partial | BGR-to-RGB conversion, render color modes, transparency handling, and export colors are present. | Desktop pattern/material hierarchy and page-area/grayscale parity need validation. |
| Export UX/error handling | Partial | Export menu, writers, share sheet, and empty-mesh alert exist. | Device share-target behavior, write-failure messaging, progress, and large-export behavior need validation. |
| Automated tests/fixtures | Missing | Desktop has pytest coverage; no iOS test target, fixture suite, app build script, or app CI workflow was found in the repo inventory. | Add fixture-driven import, quantity, mesh, PDF/page, persistence, and export validation. |
| Build/signing package config | Needs verification | `Config/Project.plist`, `Config/Entitlements.plist` | Link/resource settings and nonstandard entitlements need clean-build, signing, and distribution review. |
| Licensing/update systems | Obsolete | No iOS equivalent found. | Desktop licensing/updater is outside current iOS MVP and expected to be replaced by iOS distribution policy. |

## Active Blockers

| Blocker | Status | Impact |
|---|---|---|
| B-004 PDF annotation export | Missing | Blocks PDF annotation export only. |
| B-009 OST/OSP export round trip | Missing | Blocks desktop-compatible project/package export from iOS. |

## Needs Runtime Or Fixture Validation

- Clean iOS build/link from a fresh checkout with bgfx and Manifold.
- Device smoke test for bgfx/Metal rendering, picking readback, camera controls, and sharing.
- Representative `.ost` and `.osp` fixture imports compared against desktop rows and loaded model fields.
- Quantity parity fixtures for imperial, metric, holes, negative takeoffs, slope, curved length, grid, and attachments.
- PDF fixture validation for scaling, rotations, multi-page source files, and extracted OSP paths.
- Large project memory/frame-time testing and Manifold boolean stress testing.
- Manual dependency artifact workflow validation for bgfx and Manifold.
- Signing/entitlements validation for the current config files.
- Device export validation for OBJ/DXF/Three.js, including failure cases and large outputs.
- App-level CI/test target or fixture harness validation; current workflows only build dependency artifacts.

## Recommended Next Steps

1. Run a clean iOS build and device smoke test.
2. Decide MVP scope for B-009 `.ost`/`.osp` export and B-004 PDF annotation export.
3. Build a desktop-vs-iOS import fixture suite with representative `.ost` and `.osp` files.
4. Compare table row counts, loaded page/condition/takeoff/layer fields, and preservation of non-loaded tables.
5. Compare desktop vs iOS quantity totals for the high-risk formula cases.
6. Add app-level test/CI coverage or a repeatable fixture validation harness.
7. Implement missing count-shape and metric/attachment quantity parity according to product priority.
