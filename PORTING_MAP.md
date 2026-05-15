# OST Visualizer - iOS Porting Map

Last updated: 2026-05-15

Status values: **Ported**, **Partial**, **Missing**, **Replaced**, **Obsolete**, **Needs verification**.

## App Architecture

| Desktop path/module | iOS path/module | Status | Notes |
|---|---|---|---|
| `Visualizer.py`, `ost_visualizer/main.py`, `application/app_controller.py` | `Main.m`, `AppDelegate.*`, `SceneDelegate.mm` | Replaced | Python/Qt bootstrap replaced by native iOS lifecycle and scene URL handling. |
| `application/service_container.py`, builders/orchestrators | `Services/*`, controller-owned services | Partial | iOS has direct service classes, not the full desktop DI/use-case architecture. |
| `application/orchestrators/*` | `Services/ProjectService.mm`, `Services/ImportService.mm`, `Controllers/ViewController/*` | Partial | Current render/import/export orchestration exists, but desktop orchestration for multi-window sync and broader UI workflows is not ported. |
| `application/use_cases/project/*`, `application/dtos/*` | `ImportService.mm`, `Core/Persistence/ProjectStore/Components/*`, ObjC method params | Partial | Current-scope folder, condition, takeoff, layer, and render-config operations exist. Desktop use-case/DTO surface is broader. |
| `application/events`, `infrastructure/events` | `NSNotificationCenter` notifications in `ImportService.mm` | Partial | Import/delete/render-config notifications exist; the full desktop event bus is not ported. |
| Desktop logging/error services | `NSError` returns, `UIAlertController`, `NSLog` in services/controllers | Partial | Import failures surface alerts. Export has basic empty-export handling; progress/cancellation and broad failure UX need validation. |

## Import, Persistence, And Schema

| Desktop path/module | iOS path/module | Status | Notes |
|---|---|---|---|
| `infrastructure/mdb/importers/ost_importer.py` | `Core/Import/OstXmlParser.cpp` | Ported | Parser walks the same bid/page/bid-tail/global section model. Fixture comparison is still needed. |
| `infrastructure/mdb/importers/osp_importer.py` | `Services/ImportService.mm`, `Core/Import/CabExtractor.cpp` | Partial | iOS lists CAB entries, extracts `.ost`, extracts PDFs, rewrites `ImagePath`/`OverlayImagePath`, and imports to SQLite. Needs validation against real OSP variants. |
| `infrastructure/mdb/database_creator.py`, `infrastructure/mdb/ost_schema.py`, `domain/ost_schema.py` | `Core/Persistence/OstSchema.cpp/.hpp` | Ported | Static comparison found the 64 desktop table names in iOS plus `_AppMeta`. Column/type semantics and schema-version behavior need fixture validation. |
| `infrastructure/mdb/mdb_reader.py`, `components/bid_data_reader.py` | `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp` | Partial | Loads the visualization/editing subset: bid, pages, layers, CDN types, condition folders, conditions, takeoffs, and page folders. Labor, annotations, hotlinks, named views, timecards, and broader settings are not loaded into UI models. |
| `infrastructure/mdb/mdb_writer.py`, project use cases | `Core/Persistence/ProjectStore/*`, `Services/ImportService.mm` | Partial | iOS writes common layer visibility, takeoff position/insert/delete/negative, condition, folder, and render-config changes. Full desktop write surface is not ported. |
| `infrastructure/mdb/components/settings_reader.py`, `settings_operations.py` | `Core/Domain/RenderConfig.hpp`, `ProjectStore+RenderConfig.cpp`, `ImportService.mm` | Partial | Render config load/save exists. Desktop metric conversion, page/condition settings, and broader settings operations are not fully matched. |
| Desktop schema compatibility/import tests | none found | Missing | No iOS fixture harness or automated schema/row parity test was found. |
| Desktop MDB/pyodbc runtime | SQLite in app Documents (`ost_projects.sqlite`) | Replaced | iOS does not use MDB/pyodbc on device. |
| `presentation/visualization/exporters/ost_exporter.py` | none found | Missing | iOS cannot currently export `.ost`. |
| `presentation/visualization/exporters/osp_exporter.py` | none found | Missing | iOS cannot currently export `.osp` packages. |

## Domain And Data Model

| Desktop path/module | iOS path/module | Status | Notes |
|---|---|---|---|
| `domain/entities/bid.py` | `Core/Domain/Bid.hpp` | Partial | Core bid metadata and snap increments are modeled. Full desktop bid state is not. |
| `domain/entities/page.py` | `Core/Domain/Page.hpp` | Partial | Dimensions, scale, rotation, folder, image/PDF path, and overlay metadata fields exist. Overlay rendering/settings parity is not implemented/proven. |
| `domain/entities/condition.py` | `Core/Domain/Condition.hpp` | Partial | Geometry, display, layer/type, UOM, rounding, grid, trim/connect, slope, and curved fields are modeled for current iOS workflows. Desktop entity coverage is broader. |
| `domain/entities/takeoff.py` | `Core/Domain/Takeoff.hpp` | Partial | UID, condition/page/area, position, rotation, curve, parent/hole, and negative fields are modeled. Broader desktop takeoff workflow parity needs validation. |
| `domain/entities/layer.py`, `cdn_type.py`, `condition_folder.py`, `folder.py` | `Core/Domain/*` | Ported | Used by project list, conditions, page folders, layer visibility, and condition editing for current iOS scope. |
| `domain/entities/annotation.py`, `annotation_view.py`, hotlinks/named views | no iOS domain/UI equivalent found | Missing | Annotation/hotlink/named-view data can be stored in SQLite tables but is not surfaced as iOS app state. |
| Desktop workspace/file state entities | no direct iOS equivalent found | Obsolete | Desktop multi-document/window state is outside current iOS scope unless iPad multiwindow support is added. |
| Employee/payroll/cover sheet/licensing entities | none found | Obsolete | Desktop business systems are outside current iOS MVP. |

## Geometry, Meshing, And Booleans

| Desktop path/module | iOS path/module | Status | Notes |
|---|---|---|---|
| `cpp_extensions/src/common`, `mesh`, `geom_utils`, `coord_transform`, `earcut` | `Core/Geometry/*` | Ported | Portable C++ geometry modules are present. Build validation still applies. |
| `presentation/visualization/meshing/mesh_factory.py` | `Core/Meshing/MeshFactory.cpp` | Partial | Linear, curved linear, area, holes, count, attachment dispatch exists. Advanced count-shape parity is incomplete. |
| `presentation/visualization/core/mesh_generator.py` | `Core/Meshing/gen_*.cpp` | Partial | Linear, curved, area, count box, and circular cylinder generators are present. Desktop elliptical cylinder, triangle prism, rhombus prism, hexagon, and octagon prism generation is not ported. |
| `domain/entities/shape.py` | `Core/Domain/Types.hpp`, `MeshFactory.cpp` | Partial | Shape IDs are defined. iOS only routes circle/ellipse to cylinder and all other count shapes to box mesh. |
| `cpp_extensions/src/linear_geom` | `Core/Meshing/gen_curved.cpp`, `Core/Domain/UomService.hpp` | Partial | Curved mesh and curved length code exist without GLM. Desktop-vs-iOS geometry/quantity fixture parity is not proven. |
| `cpp_extensions/src/boolean` | `Core/Geometry/boolean_ops.cpp`, `Libs/Manifold`, `Services/*` | Ported | Manifold is present, linked in `Config/Project.plist`, and called by render/export paths. Runtime stress validation remains. |
| `cpp_extensions/src/snap`, desktop plan-view snap helpers | `PlanView+Placement.mm`, `DragController.mm` | Partial | Grid and angle snapping exist. Endpoint/midpoint/perpendicular snap index is not ported. |

## Quantity And Measurement

| Desktop path/module | iOS path/module | Status | Notes |
|---|---|---|---|
| `domain/services/uom_service.py`, `uom_service_impl.py` | `Core/Domain/UomService.hpp` | Partial | Imperial formula paths, holes, negatives, slope, grid, and curved length are source-present. Metric UOM codes 11-16 and metric/imperial normalization are missing. |
| `domain/services/condition_quantity_service.py` | `Core/Domain/UomService.hpp`, `ConditionsViewController.mm` | Partial | Page condition totals are displayed. Attachment footprint/perimeter aggregation is TODO-zeroed, and formula parity needs fixtures. |
| `presentation/dialogs/edit_condition_dialog.py` | `Controllers/EditConditionViewController/*` | Partial | iOS edits many geometry/UOM/display fields and persists conditions. Desktop dialog has broader options and workflows. |
| Desktop rounding/metric settings paths | `Condition.hpp`, `EditConditionViewController/*`, `ProjectStore+Conditions.cpp`, `UomService.hpp` | Partial | Rounding fields are modeled, edited, and persisted. Source-confirmed application of `round_quantity`/`round_up` in calculated totals was not found; metric UOMs are missing. |

## 2D Plan/PDF Rendering And Editing

| Desktop path/module | iOS path/module | Status | Notes |
|---|---|---|---|
| `presentation/visualization/pdf/renderers/page_renderer.py`, PDFium extension | `PlanView/Components/PDFCanvasView.mm` | Partial | iOS uses CoreGraphics/CGPDF instead of PDFium. Source draws page 1 only; multi-page selection is not implemented/proven. |
| Desktop overlay/page settings handling | `Page.hpp`, `ProjectStore+Load.cpp`, `PlanView.mm` | Partial | Overlay path/offset/rotation fields are loaded, but `PlanView` only draws `image_path`; overlay compositing, bitonal/invert/raster modes are not implemented. |
| `presentation/visualization/pdf/renderers/takeoff_renderer.py` | `PlanView/PlanView.mm`, `TakeoffOverlayView.mm` | Partial | Linear, area, count overlays, holes, negative markers, and patterns/opacities are represented. Desktop style parity needs validation. |
| `presentation/components/plan_view/view.py` | `PlanView/*`, `Controllers/ViewController/*` | Partial | iOS supports selection, drag, handles, placement, deletion, undo/redo, and negative toggle. Backout mode, paste-backout, detached window sync, and advanced snap index are not ported. |
| Desktop page settings/scale dialogs | `Page.hpp`, `ProjectStore+Load.cpp`, `ViewController+PageSelector.mm` | Partial | Page metadata is loaded and pages/folders are selectable. Editing page scale/rotation/raster settings is not source-confirmed. |
| Desktop condition/layer sidebars | `ConditionsViewController.mm`, `EditConditionViewController/*`, `ViewController+Layers.mm` | Partial | Current condition edit/duplicate/delete and layer visibility UI paths exist. Desktop sidebar actions, hotkeys, filters, and bulk workflows are broader. |
| Desktop clipboard/edit menu commands | none found | Missing | Copy/cut/paste, paste-backout, and broad desktop menu actions were not found in iOS source. |
| Desktop undo/redo manager | `Controllers/ViewController/Components/UndoRedoController.*` | Partial | iOS undo/redo covers focused add/delete/move operations, not condition/layer/folder/project actions. |
| Desktop annotation renderers (`annotation_renderer.py`, `annotation_item_renderer.py`) | none found | Missing | Imported annotation tables are not rendered or edited in iOS. |

## 3D Renderer And Navigation

| Desktop path/module | iOS path/module | Status | Notes |
|---|---|---|---|
| `cpp_extensions/src/renderer` OpenGL/Qt/GLAD renderer | `Rendering/Renderer.mm`, `Rendering/BgfxView.mm` | Replaced | iOS uses bgfx/Metal with `CAMetalLayer`, generated shaders, GPU buffers, OIT passes, selection mask, mesh-ID picking, and final composite paths. Build/device validation is still required. |
| `presentation/components/mesh_view.py` | `Controllers/ViewController/*`, `Rendering/*`, `CameraController.mm` | Partial | Orbit, pan mode, pinch zoom, fit/reset, selection picking, and selected mesh highlighting paths exist. Desktop Qt windowing/theme behavior is not applicable. |
| Desktop Three.js renderer/export | `Core/Export/threejs_writer.cpp`, `ExportService.mm` | Ported | iOS exports Three.js HTML from visible selected-page meshes. Share/device validation still applies. |

## Export And Sharing

| Desktop path/module | iOS path/module | Status | Notes |
|---|---|---|---|
| `presentation/visualization/exporters/obj_exporter.py`, adapters | `Core/Export/obj_writer.*`, `Services/ExportService.mm`, `ViewController+Export.mm` | Ported | OBJ+MTL writer and share flow are wired. |
| `presentation/visualization/exporters/dxf_exporter.py`, `cpp_extensions/src/dxf` | `Core/Export/dxf_writer.*`, `ExportService.mm`, `ViewController+Export.mm` | Ported | DXF writer and share flow are wired. |
| `presentation/visualization/renderers/threejs` | `Core/Export/threejs_writer.*`, `ExportService.mm`, `ViewController+Export.mm` | Ported | HTML writer and share flow are wired. |
| `presentation/visualization/exporters/pdf_exporter.py`, `cpp_extensions/src/pdf/pdf_writer.cpp`, `bluebeam_annotation.cpp` | none found | Missing | PDF annotation export remains B-004. |
| `presentation/visualization/exporters/fbx_exporter.py` | none found | Missing | Deferred unless product scope requires it. |
| Desktop export dialogs/handlers | `ViewController+Export.mm`, `UIActivityViewController` | Partial | iOS has a simple share workflow for OBJ/DXF/Three.js only. |
| Desktop export error/progress handling | `ExportService.mm`, `ViewController+Export.mm` | Partial | Source returns URL arrays and presents the share sheet; no broad progress UI or detailed write-failure messaging was found. |

## Build, Configuration, Tests, And Resources

| Desktop path/module | iOS path/module | Status | Notes |
|---|---|---|---|
| Desktop build scripts and extension CMake files | `.github/workflows/build-bgfx-ios.yml`, `.github/workflows/build-manifold-ios.yml`, `Config/Project.plist` | Needs verification | Manual workflows build bgfx/Manifold artifacts and config links static libraries. They do not prove the app builds from a clean checkout. |
| Desktop pytest suite under `tests/` | none found | Missing | No iOS app test target, fixture validation suite, or app CI workflow was found in the repo inventory. |
| Desktop resource/icon/theme files | `Resources/`, UIKit system images, `Config/Project.plist` resources | Partial | iOS uses native UI and generated shader resources; broader desktop resource/theme parity is not applicable or not implemented. |
| Desktop packaging/distribution scripts | `Config/Entitlements.plist`, `Config/Project.plist` | Needs verification | Entitlements include nonstandard `com.nyxian.pe.*` keys and need signing/distribution validation. No `.xcodeproj` or workspace was found in this repo inventory. |

## Platform, Licensing, And Desktop-Only Systems

| Desktop path/module | iOS path/module | Status | Notes |
|---|---|---|---|
| `presentation/*` PySide6/Qt windows/dialogs | `Controllers/*`, `PlanView/*` | Replaced | UIKit replaces Qt/PySide for current iOS scope. |
| `application/use_cases/license`, `infrastructure/external/license_api_client.py`, `hardware/hwid_generator.py` | none found | Obsolete | Desktop licensing/update model is not applicable to the current iOS MVP. |
| `application/services/update_check_service.py`, update dialog | none found | Obsolete | Desktop updater is not applicable to iOS distribution. |
| `infrastructure/monitoring/transaction_monitor.py`, `cpp_extensions/src/module_winevent.cpp` | none found | Obsolete | Windows file/transaction monitoring is not applicable on iOS. |
| Payroll/employees/job status/cover sheet dialogs | none found | Missing | Data tables may be stored, but UI workflows are outside current iOS MVP. |

## Needs Verification

- Clean build from a fresh checkout using current bgfx/Manifold libraries.
- Runtime import validation for real `.ost` and `.osp` fixtures, including CAB variants and duplicate global tables.
- Schema/row comparison with desktop for imported fixtures and non-loaded table preservation.
- PDF page scaling, page selection, rotations, and extracted OSP paths on device.
- Quantity parity against desktop for metric projects, attachments, rounding, grid, holes, slope, negatives, and curved lengths.
- Count condition mesh parity for ellipse/triangle/rhombus/hexagon/octagon.
