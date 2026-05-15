# OST Visualizer — iOS Porting Log

> Chronological record of porting decisions, changes, and progress.

---

## 2026-05-15 - Dashboard Coverage Expansion

### What was re-inventoried
- Compared major iOS directories (`Config`, `Controllers`, `Core`, `PlanView`, `Rendering`, `Services`, `.github/workflows`) against desktop `application`, `domain`, `infrastructure`, `presentation`, `cpp_extensions`, and `tests`.
- Verified previously under-documented iOS systems: project/folder/takeoff/condition/render-config persistence, import error alerts, OBJ/DXF/Three.js share UI, dependency artifact workflows, and config/entitlement files.
- Checked desktop-only or broader desktop systems that were not clearly tracked: use-case/DTO layers, schema compatibility tests, settings operations, clipboard/menu commands, undo scope, page settings, export failure handling, packaging, and tests.

### Corrections and additions
- Added tracker coverage for missing iOS automated app tests/fixtures and clarified that existing GitHub workflows build bgfx/Manifold artifacts only.
- Added build/signing/distribution validation risk for `Config/Project.plist` and nonstandard `Config/Entitlements.plist` entries.
- Clarified import error handling as source-present through `NSError`, alerts, and `NSLog`, while export error/progress behavior remains partial.
- Clarified condition calculation status: rounding fields are modeled, editable, and persisted, but source-confirmed application in calculated totals was not found.
- Expanded the porting map for desktop application/use-case layers, settings operations, render config, undo/clipboard/page settings, tests, workflows, and iOS-only replacement/config systems.

### Still uncertain
- Full app clean build, signing, and device run.
- Dependency artifact workflow outputs integrated into a reproducible app build.
- Fixture parity for import/schema/persistence/quantities/mesh/PDF/page/export behavior.
- Runtime share-sheet behavior and export failure handling for OBJ/DXF/Three.js.

---

## 2026-05-15 - Claim-By-Claim Tracker Validation

### What was re-checked
- Read the current Markdown/JSON trackers and re-checked major claims against both the iOS repo and the desktop/source repo.
- Verified the renderer claim at source level: `Renderer.mm` initializes bgfx Metal, `BgfxView.mm` uses `CAMetalLayer`, generated shaders are included, and `Config/Project.plist` links bgfx plus Metal/MetalKit.
- Verified Manifold is not merely present: `boolean_ops.cpp` includes Manifold and render/export services call `boolean_difference_batch`.
- Verified `.ost` and `.osp` import wiring through UTTypes, scene URL handling, `ImportService`, `OstXmlParser`, `CabExtractor`, and SQLite import.
- Verified static SQLite table-name coverage against desktop `database_creator.py`: iOS has all 64 desktop table names plus `_AppMeta`.
- Verified OBJ/DXF/Three.js writer and share-flow source paths.

### Corrections made
- Changed broad "complete/ported" wording to source-confirmed status where build/device/runtime behavior has not been validated.
- Downgraded PDF/page rendering wording: iOS uses CoreGraphics but `PDFCanvasView` draws page 1 only; multi-page PDF selection and overlay compositing are not implemented/proven.
- Tightened schema wording from broad parity to table-name coverage plus fixture validation.
- Clarified that OSP import is source-wired but still needs real package validation.
- Clarified that Manifold is source-wired and linked, but robustness still needs clean build and stress testing.
- Clarified export status: OBJ/DXF/Three.js source paths are wired, while OST/OSP export and PDF annotation export remain missing.

### Remaining uncertain items
- Clean build/link on a fresh checkout.
- Device validation for bgfx/Metal, picking readback, camera controls, and share sheets.
- Real `.ost`/`.osp` fixture comparison with desktop.
- PDF page mapping, overlay compositing, rotations, and OSP path handling.
- Quantity parity for metric UOMs, attachments, rounding, holes, negatives, slope, grid, and curved lengths.

---

## 2026-05-15 - Deeper Desktop/iOS Source Comparison

### What was verified
- Compared the iOS repo against the desktop/source repo at `C:\Users\fjimeno\Documents\Programming\Python\OSTVisualizerLicense`.
- Verified the iOS renderer is still bgfx/Metal, replacing the earlier software-renderer/OpenGL/Metal blocker notes.
- Verified `.ost` XML import, `.osp` package import, SQLite persistence, project listing/loading/deletion, and UIKit document import/export workflows are implemented on iOS.
- Verified the iOS SQLite schema mirrors the desktop OST table set closely, with iOS metadata added separately, but full fixture-level schema round-trip validation is still needed.
- Verified core domain entities and shape IDs are ported for the visualization/editing subset.
- Verified Manifold is linked and used for boolean subtraction in render/export mesh generation.

### Gaps found or clarified
- PDF annotation export remains blocked: no QPDF integration or replacement annotation writer was found.
- OST/OSP export round-trip is not ported; iOS currently exports OBJ, DXF, and Three.js HTML only.
- Count shape mesh parity is partial: circle/ellipse and generic boxes exist, but desktop-specific rhombus, triangle, hexagon, octagon, and ellipse width/depth behavior need parity work.
- Metric UOM handling is partial: desktop supports metric normalization, while the iOS domain service currently covers the imperial OST UOM codes.
- Attachment quantities are partial: footprint/perimeter inputs are still passed as TODO zero values in page quantity aggregation.
- Desktop snap/backout workflows are only partially represented by iOS grid/angle snapping, editing gestures, undo/redo, and negative toggles.
- PDF/page viewing exists through CoreGraphics, but multi-page, overlay, rotation, and coordinate mapping need real OSP validation.

### Tracking files updated
- `PORTING_BLOCKERS.md`: added OST/OSP export round-trip as an active parity blocker and split partial gaps from true blockers.
- `PORTING_STATUS.md`: expanded source-confirmed, partial, missing, risk, and validation sections from the cross-codebase comparison.
- `PORTING_MAP.md`: replaced the high-level map with a source-to-source module matrix.
- `PORTING_PLAN.md`: reordered remaining work around validation, round-trip export, PDF export, geometry/quantity parity, and editing parity.
- `PORTING_STATUS.json`: synchronized with the updated Markdown dashboard.

### Remaining manual verification
- Clean build from a fresh checkout and device run.
- Fixture comparison for desktop vs iOS imports, schema rows, loaded project model, and quantities.
- Large project performance and memory testing on target iOS hardware.
- Real OSP validation for PDFs, overlays, rotations, and page coordinate mapping.
- Boolean stress tests with dense negative takeoffs and mixed condition types.

---

## 2026-05-15 - Porting Tracker Refresh

### What was verified
- Renderer pipeline is now bgfx/Metal: `Rendering/Renderer.mm` initializes `bgfx::RendererType::Metal`, `Rendering/BgfxView.mm` uses `CAMetalLayer`, generated bgfx shaders are used, and `Config/Project.plist` links bgfx plus Metal/MetalKit.
- GLM is not required by current iOS source. Curved and linear mesh generation is implemented in `Core/Meshing` with portable C++ math.
- Manifold is present and linked: `Libs/Manifold/lib/libmanifold.a`, Manifold headers, `-lmanifold` in `Config/Project.plist`, and `Core/Geometry/boolean_ops.cpp` uses Manifold operations.
- PDF viewing no longer depends on PDFium. OSP import extracts PDFs and `PlanView/Components/PDFCanvasView.mm` renders PDFs with CoreGraphics/CGPDF.
- QPDF/PDF annotation export remains open. No QPDF or replacement PDF writer implementation was found.
- MDB/Access access is replaced on iOS by `.ost` XML parsing, `.osp` CAB extraction, and SQLite storage through `Core/Import`, `Core/Persistence`, and `Services/ImportService.mm`.
- PySide6/Qt UI is replaced by UIKit controllers and views under `Controllers/` and `PlanView/`.
- File import/export workflows are implemented through UTType registration, document picker/scene URL handling, `ImportService`, `ExportService`, and UIKit sharing.

### Tracking files updated
- `PORTING_BLOCKERS.md`: split current state into resolved blockers, active blockers, deferred items, obsolete historical blockers, and current risks.
- `PORTING_STATUS.md`: refreshed high-level state and explicitly documented bgfx/Metal as the current renderer.
- `PORTING_MAP.md`: refreshed module mapping from desktop concepts to current iOS equivalents.
- `PORTING_PLAN.md`: marked completed work complete and reduced remaining work to validation, PDF annotation export, polish, and deferred parity.
- `PORTING_STATUS.json`: refreshed to match the Markdown statuses and blocker names.

### Remaining manual verification
- Clean build on a fresh environment or CI.
- Device testing with large real `.ost`/`.osp` projects.
- Manifold boolean stress testing for dense negative takeoffs.
- Multi-page PDF/overlay mapping validation.
- PDF annotation export design/implementation path.

---

## 2026-03-23 — Initial Analysis & Planning

### What was examined
- Full Python source tree: 289 files across 4 layers (presentation, application, domain, infrastructure)
- Full C++ extensions: 41 source files across 12 nanobind modules
- Existing iOS project: 10 source files (working software 3D renderer demo)
- Entry point: `Visualizer.pyw` → `main.py` → Qt app + DI container → MainWindow
- Rendering pipeline: Takeoffs → MeshBuilder → Boolean ops → Geometries → ost_renderer (OpenGL)
- Data pipeline: pyodbc → MDB/Access databases → Domain entities
- Dependencies: PySide6 6.10.2, pyodbc, Nuitka, C++17, Qt6, GLAD, GLM, Manifold, PDFium, QPDF

### Key findings

1. **Rendering uses two paths**:
   - **Native**: `mesh_view.py` → `ost_renderer` C++ module (OpenGL via GLAD/GLM/Qt6)
   - **Export**: Three.js HTML renderer for file export
   - iOS will use the native path approach with a new renderer

2. **The C++ mesh pipeline is the core value**:
   - `mesh_builder.py` (Python) orchestrates mesh creation
   - `boolean_ops.cpp` (C++) performs boolean operations
   - `mesh_repair.cpp` (C++) validates and repairs meshes
   - `earcut.hpp` (C++) triangulates polygons
   - These are the highest-value pieces to port

3. **9 of 12 C++ modules are portable** with no Windows dependencies:
   - vec_types, mesh_data, mesh_repair, feature_edges, geom_utils, coord_transform, earcut, dxf, image
   - 2 need external libraries: linear_geom (GLM), boolean_ops (Manifold)
   - 2 are Windows-only: winevent, cab

4. **The iOS project already has a working foundation**:
   - Software 3D rasterizer with Phong shading
   - Touch controls (orbit, pan, zoom)
   - CADisplayLink render loop
   - `MathUtils.hpp` provides Vec3/Vec4/Mat4/Color types similar to C++ renderer

5. **Python cannot run on iOS practically**:
   - PySide6 has no iOS support
   - pyodbc has no iOS support
   - Embedding CPython is possible but complex and heavyweight
   - Decision: Port essential logic to C++, skip Python embedding

### Architecture decisions made

| # | Decision | Rationale |
|---|----------|-----------|
| 1 | Port C++ core first | Already compiled, portable, highest value |
| 2 | Keep software renderer for MVP | Already working; Metal later |
| 3 | Skip Python embedding | Too complex; port logic to C++ instead |
| 4 | Use JSON for data loading | Avoids MDB/pyodbc; simple to implement |
| 5 | Defer licensing | Not needed for prototype |
| 6 | Defer PDF features | Needs PDFium/QPDF iOS builds |
| 7 | Defer boolean operations | Needs Manifold iOS build |

### What was created
- `PORTING_PLAN.md` — 7-phase porting strategy
- `PORTING_STATUS.md` — Module-by-module status tracking
- `PORTING_STATUS.json` — Machine-readable status
- `PORTING_MAP.md` — Desktop source → iOS target mapping
- `PORTING_BLOCKERS.md` — Blockers, platform issues, risks
- `PORTING_LOG.md` — This file

### What changed
- No code changes yet — analysis phase only

### What remains (next steps)
1. ~~**Phase 1**: Copy portable C++ modules into iOS project~~ → DONE (see below)
2. **Phase 2**: Integrate C++ mesh data with iOS renderer
3. **Phase 3**: Port Python domain entities to C++
4. **Phase 4**: Create data loading pipeline (MDB → JSON → iOS)

---

## 2026-03-23 — Phase 1: Portable C++ Core Copied

### What was done
Copied 9 portable C++ modules (17 files) from the desktop C++ extensions into the iOS project.

### Directory structure created
```
Core/
├── Geometry/
│   ├── vec_types.hpp          (direct copy)
│   ├── mesh_data.hpp          (direct copy)
│   ├── mesh_repair.hpp        (direct copy)
│   ├── mesh_repair.cpp        (direct copy)
│   ├── feature_edges.hpp      (direct copy)
│   ├── feature_edges.cpp      (direct copy)
│   ├── geom_utils.hpp         (include path adjusted: ../common/vec_types.hpp → vec_types.hpp)
│   ├── geom_utils.cpp         (direct copy)
│   ├── coord_transform.hpp    (include path adjusted: ../common/vec_types.hpp → vec_types.hpp)
│   ├── coord_transform.cpp    (direct copy)
│   └── earcut.hpp             (direct copy, header-only third-party)
├── Export/
│   ├── dxf_writer.hpp         (include path adjusted: mesh_data.hpp → ../Geometry/mesh_data.hpp)
│   └── dxf_writer.cpp         (direct copy)
└── Image/
    ├── image_tint.hpp          (direct copy)
    └── image_tint.cpp          (direct copy)
```

### Include path adjustments made
- `geom_utils.hpp`: `#include "../common/vec_types.hpp"` → `#include "vec_types.hpp"` (same directory now)
- `coord_transform.hpp`: `#include "../common/vec_types.hpp"` → `#include "vec_types.hpp"` (same directory now)
- `dxf_writer.hpp`: `#include "mesh_data.hpp"` → `#include "../Geometry/mesh_data.hpp"` (cross-directory reference)

### Constraint noted
- Metal framework is NOT available in this build environment
- Only CoreGraphics and QuartzCore are available
- Software rasterizer (Renderer.mm) is the production renderer

### What remains
1. ~~Add these C++ files to the Xcode build (Project.plist or equivalent)~~ → see Phase 2 below
2. ~~Verify compilation on iOS target~~ → PENDING BUILD TEST
3. ~~**Phase 2**: Create a `MeshLoader` bridge~~ → DONE (see below)
4. **Phase 3**: Port Python domain entities (Takeoff, Condition, Page) to C++ structs
5. **Phase 4**: Create data loading pipeline

---

## 2026-03-23 — Phase 2: MeshLoader Bridge & Namespace Fix

### What was done

1. **Fixed Vec3 name collision** in `vec_types.hpp`:
   - `vec_types.hpp` defined `using Vec3 = std::array<double, 3>` in the global namespace
   - `MathUtils.hpp` defines `struct Vec3 { float x, y, z; }` in the global namespace
   - These would conflict in any translation unit including both
   - **Fix**: Wrapped vec_types.hpp aliases in `ost_types` namespace
   - Updated `geom_utils.hpp` and `coord_transform.hpp` with `using ost_types::Vec2;`

2. **Created MeshLoader bridge** (`MeshLoader.hpp` + `MeshLoader.cpp`):
   - Converts `CppMeshData` (double-precision vertices, uint32 face indices) to renderer format
   - Produces `LoadedMesh` struct with float positions, smooth-shaded normals, and color
   - Uses raw pointers to avoid including `vec_types.hpp` (sidesteps name collision entirely)
   - Computes per-vertex normals by accumulating area-weighted face normals

3. **Extended Renderer** (`Renderer.h` + `Renderer.mm`):
   - Added `addMeshWithVertices:vertexCount:faces:faceCount:colorR:g:b:` — accepts raw CppMeshData arrays
   - Added `clearMeshes`, `fitCameraToMeshes`, `meshTriangleCount`
   - Added `showDemoCubes` toggle to switch between demo and mesh rendering
   - Added `_cameraTarget` to orbit around mesh center (not hardcoded origin)
   - Added `_renderLoadedMesh:vp:lightPos:lightColor:eye:` — iterates triangles with back-face culling, projection, Phong shading
   - Disabled auto-orbit when meshes are loaded
   - Camera auto-fits to mesh bounding box via `fitCameraToMeshes`

4. **Added test meshes** in `ViewController.mm`:
   - Created `makeBoxMesh()` helper using `ost_geometry::CppMeshData` (proves the ported types work)
   - Loads 3 test meshes: wall section (teal), column (orange), floor slab (grey)
   - Disables demo cubes, fits camera to meshes
   - Stats label shows mesh triangle count
   - Tap toggles between mesh view and cube demo

### Files created
```
MeshLoader.hpp     — LoadedMesh struct + loadMeshFromRaw() declaration
MeshLoader.cpp     — double→float conversion + normal computation
```

### Files modified
```
Core/Geometry/vec_types.hpp       — wrapped in ost_types namespace
Core/Geometry/geom_utils.hpp      — added using ost_types::Vec2
Core/Geometry/coord_transform.hpp — added using ost_types::Vec2
Renderer.h                        — added mesh loading API + showDemoCubes
Renderer.mm                       — mesh rendering loop + camera target + mesh storage
ViewController.mm                 — test mesh loading via CppMeshData → bridge → renderer
```

### Type system note
The project now has two Vec3 types that coexist safely:
- `ost_types::Vec3` = `std::array<double, 3>` — used by the ported C++ geometry core
- `::Vec3` = `struct { float x, y, z; }` — used by MathUtils.hpp and the software renderer
The `MeshLoader` bridge converts between them (double→float) at the boundary.

### What remains
1. ~~**Build verification**: Confirm all files compile on the LDE build system~~ → DONE
2. ~~**Phase 3**: Port Python domain entities to C++ structs~~ → DONE (see below)
3. **Phase 4**: Create data loading pipeline (MDB → JSON → iOS)
4. **Phase 5**: Optimize software rasterizer for production mesh sizes

---

## 2026-03-23 — Phase 3: Domain Model & Mesh Pipeline

### What was done

1. **Created domain structs** (Core/Domain/):
   - `Types.hpp` — ConditionType (Linear/Area/Count/Attachment), ShapeId (Square/Circle/Triangle/Rectangle/Rhombus/Ellipse/Hexagon/Octagon), Pattern enums
   - `Condition.hpp` — geometry-relevant fields from `condition.py`: uid, name, condition_type, thickness, height, width, depth, rise, run, shape, color_fill, z_value, is_top, pattern, display_size, drop_run, drop_value, layer_visible. Includes `get_rgb()` for BGR int → RGB float conversion.
   - `Takeoff.hpp` — from `takeoff.py`: uid, condition_uid, page_uid, area_uid, position, rotation, curve, parent_uid, is_negative. Includes `is_hole()` and `has_valid_position()` helpers.
   - `Page.hpp` — from `page.py`: uid, name, width, height, scale_factor1/2, rotation, layer_visible, page_index, takeoffs vector.

2. **Created MeshGenerator** (Core/Meshing/MeshGenerator.hpp/.cpp):
   - `generate_linear_mesh()` — wall box along a line. Ported from `calc_linear_mesh_verts` in `linear_geom.cpp`. Adapted from Z-up to Y-up coordinate system.
   - `generate_count_mesh()` — rectangular prism at a point. Ported from `calc_count_mesh_verts` in `radial.py`. Supports rotation.
   - `generate_cylinder_mesh()` — circular prism. Ported from `calc_radial_mesh_verts` in `radial.py`. Includes center vertices and fan caps.
   - `generate_area_mesh()` — polygon extrusion. Ported from `calc_area_mesh_verts` + `calc_area_faces` in `area.py`. Uses mapbox earcut for triangulation. Handles polygon orientation for correct winding.
   - All functions output `ost_geometry::CppMeshData` directly.

3. **Created MeshFactory** (Core/Meshing/MeshFactory.hpp/.cpp):
   - `create_mesh_for_takeoff()` dispatches to linear/count/area based on condition type. Ported from `mesh_factory.py`.
   - Handles dimension defaults (min 1.0), display_size scaling for count items, is_top z-offset adjustment.
   - Returns `MeshResult` with CppMeshData + RGB color extracted from condition.
   - Accepts `scale_ratio` for coordinate unit conversion.

4. **Updated ViewController.mm**:
   - Removed `makeBoxMesh()` hardcoded helper.
   - `_loadTestMeshes` now creates domain objects (Takeoff + Condition) and generates geometry through `ost_meshing::create_mesh_for_takeoff()`.
   - 3 test meshes: wall (linear, teal), column (count, orange), floor slab (area, grey).
   - Full pipeline: `Domain → MeshFactory → CppMeshData → MeshLoader → Renderer`

### Python source files analyzed
- `domain/entities/takeoff.py` — 9 fields ported (out of ~12)
- `domain/entities/condition.py` — 18 fields ported (out of ~40)
- `domain/entities/page.py` — 9 fields ported (out of ~20)
- `infrastructure/visualization/meshing/mesh_factory.py` — dispatch logic ported
- `infrastructure/visualization/core/mesh_generator.py` — 4 generators ported (linear, count, cylinder, area)
- `infrastructure/visualization/core/geometry/linear.py` → C++ native (`calc_linear_mesh_verts`)
- `infrastructure/visualization/core/geometry/radial.py` — `calc_count_mesh_verts`, `calc_radial_mesh_verts` ported
- `infrastructure/visualization/core/geometry/area.py` — `calc_area_mesh_verts`, `calc_area_faces` ported
- `infrastructure/visualization/utils/mesh.py` — `get_box_faces`, face generation ported inline
- `infrastructure/visualization/core/shapes.py` — ShapeId enum + basic dispatch ported
- `infrastructure/visualization/services/color_service.py` — BGR→RGB extraction ported

### Files created
```
Core/Domain/Types.hpp          — enums (ConditionType, ShapeId, Pattern)
Core/Domain/Condition.hpp      — Condition struct with get_rgb()
Core/Domain/Takeoff.hpp        — Takeoff struct with position vector
Core/Domain/Page.hpp           — Page struct with takeoffs
Core/Meshing/MeshGenerator.hpp — geometry generation API
Core/Meshing/MeshGenerator.cpp — linear, count, cylinder, area generators
Core/Meshing/MeshFactory.hpp   — MeshResult + create_mesh_for_takeoff()
Core/Meshing/MeshFactory.cpp   — dispatch + dimension handling
```

### Files modified
```
ViewController.mm              — domain-driven mesh generation replaces hardcoded boxes
```

### Coordinate system note
The iOS MeshGenerator uses Y-up convention (matching the renderer):
- X, Z = horizontal plane (ground)
- Y = vertical (up)

The Python/desktop pipeline uses Z-up. The MeshGenerator adapts the geometry algorithms from Z-up to Y-up during generation. When real data flows in (Phase 4), the coordinate swap will be handled at the data loading boundary.

### What was NOT ported (kept minimal per Phase 3 scope)
- Curved linear meshes (needs GLM or curve subdivision)
- Area mesh holes and slope
- Rhombus, isosceles triangle, polygon prisms (only square box and cylinder)
- Full ShapeSpec with polygon_params
- Bid, Project, Layer, Config, NamedView entities
- MeshBuilder orchestrator (process_takeoffs_to_meshes)
- Full coordinate transformation system (using simple scale_ratio instead)
- Full ColorService (grayscale, opacity, pattern-based color modes)

### What remains
1. ~~**Build verification**: Confirm Phase 3 .cpp files compile on LDE~~ → DONE (see below)
2. **Phase 4**: Create data loading pipeline (MDB → JSON → iOS)
3. **Phase 5**: Optimize software rasterizer for production mesh sizes
4. **Phase 6**: iOS UI (project selector, page selector, toolbar)

---

## 2026-03-23 — Phase 3 Build Verification & Fix

### What was done

1. **Verified all Phase 3 source files compile on the LDE build system** — confirmed the app builds correctly with the new Core/Meshing/*.cpp files.

2. **Fixed portability issue** in `Core/Meshing/MeshGenerator.cpp`:
   - **Problem**: Line 17 included `<corecrt_math_defines.h>`, a Windows-only Microsoft CRT header that provides `M_PI`. This header does not exist on iOS/clang.
   - **Fix**: Replaced with a portable `#ifndef M_PI` / `#define M_PI` fallback. On Apple platforms, `M_PI` is available from `<cmath>` by default, so the fallback acts as a safety net.

3. **Full code audit** — reviewed all include paths, namespace usage, API consistency, and portability:
   - All relative include paths resolve correctly (e.g., `"../Geometry/mesh_data.hpp"` from `Core/Meshing/`)
   - No other Windows-specific or desktop-specific dependencies found
   - `ost_domain`, `ost_geometry`, `ost_meshing` namespaces used consistently
   - ViewController.mm API calls match Renderer.h declarations
   - LDE build system auto-discovers .cpp files recursively — no manual registration needed

### Files modified
```
Core/Meshing/MeshGenerator.cpp — replaced #include <corecrt_math_defines.h> with portable M_PI define
```

### Audit results — no other issues found
- Domain headers (Types.hpp, Condition.hpp, Takeoff.hpp, Page.hpp): header-only, pure C++17, no platform deps
- MeshGenerator.hpp/.cpp: correct include paths, earcut integration works, Y-up convention consistent
- MeshFactory.hpp/.cpp: correct domain type usage, dimension defaults match Python source
- MeshLoader bridge: API matches between ViewController.mm → Renderer.h → MeshLoader.hpp
- CppMeshData: `vector<array<double,3>>` layout compatible with `const double *` cast in addMeshWithVertices

### What remains
1. ~~**Phase 4**: Create data loading pipeline (MDB → JSON → iOS)~~ → DONE (see below)
2. **Phase 5**: Optimize software rasterizer for production mesh sizes
3. **Phase 6**: iOS UI (project selector, page selector, toolbar)

---

## 2026-03-23 — Phase 4: Data Loading Pipeline

### What was done

1. **Created Python MDB → JSON converter** (`Tools/export_ost.py`):
   - Standalone script: `python export_ost.py <file.ost> [--bid <uid>] [--output <file.json>]`
   - Opens .ost files via pyodbc/ODBC
   - Reads pages (dimensions, scale factors), conditions (geometry fields + z_value extraction from name), takeoffs (parsed positions)
   - Includes position parser ported from `infrastructure/parsers/position_parser.py` (semicolon-separated floats, XML entity cleanup, trailing zero trim)
   - Includes z_value extractor ported from `extract_z_value_from_name` (imperial feet/inches, metric m/cm → inches)
   - Handles layer visibility (BidLayers → BidConditions join)
   - Filters to valid takeoffs only (known condition + non-empty position)
   - `--list` flag to enumerate bids without exporting

2. **Created C++ project data container** (`Core/Data/ProjectData.hpp`):
   - `ost_data::ProjectData` struct with `pages`, `conditions` (unordered_map by UID), `takeoffs` (vector)
   - `page_scale_ratio(page_uid)` helper computes `sf2/sf1`

3. **Created ObjC++ JSON loader** (`ProjectLoader.h` + `ProjectLoader.mm`):
   - Uses `NSJSONSerialization` — no external dependencies
   - `loadBundledResource:` for app-bundled JSON files
   - `loadFromFile:` for arbitrary paths
   - `loadFromData:` for raw NSData
   - Populates `ost_domain::Condition`, `ost_domain::Takeoff`, `ost_domain::Page` structs from JSON dictionaries
   - Type-safe helpers: `nsstr()`, `nsdbl()`, `nsint()`, `nsbool()`

4. **Created sample project data** (`Resources/sample_project.json`):
   - "Sample Office Building" — a small construction scene
   - 1 page, 6 conditions, 14 takeoffs
   - 4 exterior walls (linear, teal-blue), 2 interior partitions (linear, green)
   - 4 square columns + 2 round columns (count, dark blue)
   - 1 floor slab + 1 ceiling (area, grey/light)
   - All in world units (scale_factor1 = scale_factor2 = 1.0)

5. **Updated ViewController.mm**:
   - `_loadProjectData` tries: bundled resource → Documents directory → built-in fallback
   - `_generateMeshesFromProject:` iterates all takeoffs, looks up conditions, computes `sr = 1.0 / page_scale_ratio`, generates meshes via MeshFactory
   - Skips invisible layers, holes (needs boolean ops), and negative takeoffs
   - `_loadBuiltInTestScene` preserves the Phase 3 hardcoded test data as fallback
   - Pinch zoom now scales proportionally to camera distance (works at any scale)
   - Camera distance limits increased: 1.0 – 500.0 (supports real building scales)

6. **Updated Renderer.mm** for variable-scale scenes:
   - Far plane scales with camera distance: `max(dist * 4, 100)`
   - Light position orbits around camera target (not origin) and scales with distance
   - Fog start/range scale with far plane instead of hardcoded values

### JSON format (version 1)
```json
{
  "format_version": 1,
  "project_name": "...",
  "bid_name": "...",
  "pages": { "<uid>": { "name", "width", "height", "scale_factor1", "scale_factor2", "rotation", "page_index" } },
  "conditions": { "<uid>": { "name", "type", "thickness", "height", "width", "depth", "rise", "run", "shape", "color_fill", "z_value", "is_top", "pattern", "display_size", "drop_run", "drop_value", "layer_visible" } },
  "takeoffs": [ { "uid", "condition_uid", "page_uid", "area_uid", "position", "rotation", "curve", "parent_uid", "is_negative" } ]
}
```

### Coordinate transformation approach
- Positions in JSON are in raw OST page coordinates (same as database)
- Page scale factors (`scale_factor1`, `scale_factor2`) are included per page
- iOS computes `scale_ratio = sf2 / sf1`, passes `sr = 1.0 / scale_ratio` to MeshFactory
- MeshFactory multiplies all coordinates and dimensions by `sr`, effectively dividing by scale_ratio
- This matches the desktop pipeline: `transform_to_3d(ost_x, ost_y, scale_ratio) = {-(ost_x/ratio), ost_y/ratio}`
- Note: X negation from the desktop `transform_to_3d` is NOT applied yet (deferred — affects mirroring only)

### Files created
```
Tools/export_ost.py        — Python MDB → JSON converter (standalone, requires pyodbc)
Core/Data/ProjectData.hpp  — C++ project data container
ProjectLoader.h            — ObjC++ JSON loader API
ProjectLoader.mm           — NSJSONSerialization → C++ domain structs
Resources/sample_project.json — Sample office building (14 takeoffs, 6 conditions)
```

### Files modified
```
ViewController.mm          — JSON-driven mesh loading with fallback to built-in scene
Renderer.mm                — Dynamic far plane, scaled light/fog for variable-size scenes
```

### Python source files analyzed
- `infrastructure/mdb/mdb_reader.py` — connect_to_mdb, parse_file, hierarchy parsing
- `infrastructure/mdb/components/bid_data_reader.py` — conditions, takeoffs, pages SQL queries
- `infrastructure/mdb/ost_schema.py` — table names (BidConditions, BidTakeoffs, BidPages, etc.)
- `infrastructure/parsers/position_parser.py` — parse_position, extract_z_value_from_name
- `infrastructure/parsers/utils/parser.py` — decode_value, parse_float helpers
- `domain/entities/condition.py` — full Condition dataclass (40 fields)
- `domain/entities/takeoff.py` — full Takeoff dataclass (9 fields)
- `domain/entities/bid_page_info.py` — full BidPageInfo dataclass
- `infrastructure/visualization/meshing/mesh_builder.py` — process_takeoffs_to_meshes coordinate flow
- `domain/services/coordinate_transformation_service.py` — OSTCoordinateSystem, scale_ratio = sf2/sf1

### What was NOT implemented (kept minimal per Phase 4 scope)
- X coordinate negation from `transform_to_3d` (only affects mirroring — cosmetic)
- Multi-page rendering (all takeoffs rendered together regardless of page)
- Page image/overlay loading (needs PDF/image rendering)
- Annotation loading (highlights, dimensions, text — not needed for 3D mesh view)
- Area selections / typed areas (filter which takeoffs to show per area)
- Hierarchy browsing (project → bid selection UI)
- OSP file format support (alternative to OST)

### What remains
1. ~~**Build verification**: Confirm ProjectLoader.mm compiles on LDE~~ → DONE
2. ~~**Phase 5**: Optimize software rasterizer for production mesh sizes~~ → DONE (see below)
3. **Phase 6**: iOS UI (project selector, page selector, toolbar)

---

## 2026-03-23 — Phase 5: Renderer Optimization & Scene Polish

### What was done

1. **Cached bounding volumes at load time** (MeshLoader.hpp/.cpp):
   - Added `boundsMin`, `boundsMax`, `center`, `radius` fields to `LoadedMesh`
   - Computed in `loadMeshFromRaw()` after position conversion — no per-frame recomputation
   - Removed `computeBounds()` method (was O(n) per call, now O(1) field access)
   - `fitCameraToMeshes` uses cached bounds directly

2. **Dynamic near/far planes** (Renderer.mm):
   - Near plane scales with camera distance in mesh mode: `max(dist * 0.005, 0.01)` — prevents z-fighting on large scenes
   - Demo mode keeps near = 0.1 (fine for small cube grid)
   - Far plane tightened around scene: `max(dist + sceneRadius * 2, sceneRadius * 4)` — better z-buffer precision
   - Cached `_sceneRadius` (half maxExtent) computed once at mesh load time

3. **Camera-follow light for mesh mode** (Renderer.mm):
   - Mesh mode: light positioned above eye (`eye.y + dist * 0.5`) — consistent lighting as user orbits
   - Demo mode: orbiting light preserved (animated shadows)
   - Eliminates distracting shadow motion in construction viewing mode

4. **Per-frame fog precomputation** (Renderer.mm):
   - `_fogStart` and `_invFogRange` computed once per frame, stored as ivars
   - Rasterizer inner loop uses `(fogDist - _fogStart) * _invFogRange` — eliminates per-pixel division and two multiplications
   - Fog start tied to scene radius when available: `max(sceneRadius * 3, farPlane * 0.4)` — keeps scene content fog-free
   - Demo mode fog uses `farPlane * 0.3` (unchanged behavior)

5. **Mesh-level depth culling** (Renderer.mm `_renderLoadedMesh:`):
   - Before iterating triangles, checks mesh bounding sphere against near/far planes
   - Uses `dot(forward, meshCenter - eye)` vs `nearPlane ± radius`
   - Skips entire mesh (all per-triangle work) when off-screen depth-wise
   - Cheap: one normalize + one dot product per mesh per frame

6. **Demo cube cleanup** (ViewController.mm):
   - `[_cubes removeAllObjects]` after `fitCameraToMeshes` — frees 49 CubeInstance objects
   - Tap handler guarded: no toggle to demo mode when cubes are cleared

7. **Initial camera angle** (Renderer.mm `fitCameraToMeshes`):
   - Pitch increased from 0.35 to 0.45 radians (~26° vs ~20°) — slightly more elevated for construction plan viewing

### Files modified
```
MeshLoader.hpp     — cached boundsMin/Max/center/radius, removed computeBounds()
MeshLoader.cpp     — bounds computation at end of loadMeshFromRaw()
Renderer.mm        — dynamic near/far, camera-follow light, per-frame fog, mesh culling, scene radius cache
ViewController.mm  — demo cube cleanup + tap handler guard
```

### Performance characteristics
- For the 14-takeoff sample scene (~200-300 triangles): no visible speedup (already fast)
- For larger scenes (100+ takeoffs, 2000+ triangles): reduced per-frame overhead from cached bounds, fog precomputation, and mesh culling
- Z-buffer precision improved by tighter near/far planes — eliminates z-fighting artifacts on large scenes

### What was NOT done (deferred)
- Multi-threaded rasterization via GCD (larger scope, defer to Phase 5b)
- Full frustum culling (left/right/top/bottom planes — near/far only for now)
- Transparency/sorted rendering (needs back-to-front order, defer)
- LOD for distant meshes (not needed at current triangle counts)
- Render-to-texture at reduced resolution for interactive frame rates (not needed yet)

### What remains
1. ~~**Build verification**: Confirm Phase 5 changes compile on LDE~~ → DONE
2. ~~**Phase 6**: iOS UI (project selector, page selector, toolbar)~~ → DONE (see below)

---

## 2026-03-23 — Phase 6: Minimum iOS UI

### What was done

1. **Page selector** (horizontal scrolling bar at top):
   - UIScrollView with pill-shaped UIButtons below the stats label
   - "All" button shows all takeoffs from all pages
   - Per-page buttons filter takeoffs by `page_uid`
   - Selected page highlighted in blue; others in semi-transparent dark
   - Hidden when no project is loaded or project has no pages
   - Sorted by `page_index` then by name

2. **Bottom toolbar** (5 actions):
   - Semi-transparent dark bar with thin top border, above safe area
   - "Reset" — resets camera yaw/pitch to default (0.5/0.45)
   - "−" — zoom out (distance × 1.25, max 500)
   - "+" — zoom in (distance × 0.8, min 1)
   - "Fit" — calls `fitCameraToMeshes` to frame all visible geometry
   - "Project" — opens project picker action sheet
   - UIStackView with evenly distributed UIButton (custom type, white text)

3. **Project picker** (UIAlertController action sheet):
   - Scans bundled JSON resources via `[[NSBundle mainBundle] pathsForResourcesOfType:@"json"]`
   - Scans Documents directory for user-transferred JSON files
   - "Built-in Test Scene" option loads the hardcoded fallback scene
   - "Cancel" dismisses
   - iPad-compatible via `popoverPresentationController.sourceView` anchor
   - Each selection calls `_loadProject:` which clears meshes, rebuilds page list, regenerates

4. **Page-filtered mesh generation** (`_generateMeshesForCurrentPage`):
   - Replaces old `_generateMeshesFromProject:` which loaded all takeoffs unconditionally
   - Filters takeoffs by `_currentPageUID` when set (empty string = all pages)
   - Clears and regenerates meshes on each page switch
   - Calls `fitCameraToMeshes` after generation for automatic framing

5. **Gesture recognizer isolation**:
   - ViewController conforms to `UIGestureRecognizerDelegate`
   - Pan and pinch gestures configured with delegate
   - `gestureRecognizer:shouldReceiveTouch:` prevents gestures on toolbar buttons, page buttons, and scroll view
   - Eliminates conflict between orbit/zoom and UI button taps

6. **Tap handler removed**:
   - Old tap handler toggled demo cubes; now replaced by Project picker and toolbar controls
   - Demo cube toggle is available via "Built-in Test Scene" in project picker

### Data flow for page switching
```
User taps page button → _pageButtonTapped:
  → sets _currentPageUID (or "" for All)
  → _highlightSelectedPage (updates button colors)
  → _generateMeshesForCurrentPage
    → [_renderer clearMeshes]
    → iterates _currentProject.takeoffs (filtered by page_uid)
    → creates meshes via MeshFactory
    → [_renderer fitCameraToMeshes]
```

### Data flow for project switching
```
User taps "Project" → _handleProjectPicker
  → UIAlertController lists bundled + Documents + test scene
  → User selects → _loadProject:
    → stores _currentProject
    → builds _pageList (sorted by page_index)
    → _renderer.showDemoCubes = NO
    → _populatePageSelector (creates page buttons)
    → _generateMeshesForCurrentPage (loads "All" by default)
```

### Files modified
```
ViewController.mm  — Phase 6: page selector, toolbar, project picker, page-filtered mesh generation, gesture isolation
```

### Files unchanged
```
ViewController.h   — no API changes needed (ViewController interface is minimal)
Renderer.h/mm      — no changes (existing API sufficient)
ProjectLoader.h/mm — no changes (existing API sufficient)
MeshLoader.hpp/cpp — no changes
MathUtils.hpp      — no changes
```

### What was NOT done (deferred)
- Condition visibility sidebar (toggle individual conditions on/off)
- Bid tree browser (multi-project hierarchy)
- Export dialog (DXF/OBJ/HTML)
- Settings/preferences screen
- Annotation tools
- Undo/redo
- Full SwiftUI rewrite

### What remains
1. **Build verification**: Confirm Phase 6 changes compile on LDE
2. **Phase 7+**: Extended features (PDF, export, licensing, boolean ops)

---

## 2026-03-23 — Phase 5c: Visual Style (Gray Background + Directional Lights)

### What was done

1. **Gray background** (matching desktop dark theme):
   - Changed clear color from `0xFF1A1A2E` (dark blue-grey) to `0xFF353535` (neutral gray, R=G=B=53)
   - Updated fog color to match background (`kBgGray = 53/255 ≈ 0.208`)

2. **Three-point directional lighting** (desktop renderer style):
   - Key light: (0.577, 0.577, 0.577) intensity 0.6 — upper-right-front
   - Fill light: (-0.577, 0.577, -0.577) intensity 0.3 — upper-left-back
   - Rim light: (0, -0.707, -0.707) intensity 0.2 — lower-back
   - Ambient: 0.2
   - Matches desktop's `opaque_fragment_shader` 3-light setup (adapted from Z-up to Y-up)
   - No specular component (desktop also uses pure Lambert diffuse)

### Desktop renderer reference
- Source: `cpp_extensions/src/renderer/renderer.cpp` lines 301-326
- Shader: `opaque_fragment_shader` — 3 directional lights, ambient 0.2, key/fill/rim at 0.6/0.3/0.2
- Background: default (0.08, 0.08, 0.08), overridden by Qt palette (~#353535 dark theme)

---

## 2026-03-23 — Phase 5c (revision): Smooth Lambert Shading

### What was done

1. **Removed true flat shading** — per-face constant-color rasterization caused visible triangulation artifacts:
   - Diagonal seams on quads (two triangles per quad had slightly different normals)
   - Faceting on cylinders and curved meshes
   - Inconsistent shading across coplanar earcut-triangulated polygons
   - Removed `FlatVert` struct and `_rasterizeFlatTriangle:` method entirely

2. **Implemented smooth-normal Lambert rasterizer** (`_rasterizeLambertTriangle:`):
   - Interpolates smooth vertex normals per pixel (from MeshLoader's area-weighted normals)
   - Normalizes interpolated normal before lighting
   - Lambert diffuse only: `ambient + sum(max(dot(norm, lightDir), 0) * intensity)`
   - No specular, no pow(), no view vector, no half-vector, no reflection
   - Per-pixel fog using interpolated world position
   - 3 fixed directional lights (key/fill/rim) — static, do not move with camera

3. **Updated `_renderLoadedMesh:`**:
   - Builds `ScreenVert` with smooth vertex normals from `mesh.normals[]` (not face normals)
   - Face normal still computed for back-face culling only (unnormalized dot — cheaper)
   - Degenerate triangle skip via `area2 < 1e-8f`
   - Calls `_rasterizeLambertTriangle:` instead of flat rasterizer

4. **Performance vs old Phong path**:
   - Removed: specular (pow, reflection vector, view vector, half-vector)
   - Removed: camera-follow point light distance computation
   - Kept: per-pixel normal interpolation + normalize + 3 dot products + fog
   - ~25 float ops/pixel (was ~90+ with Phong, ~5 with flat)
   - Acceptable for 0.5x resolution software rasterizer

### What was preserved
- Gray background (0xFF353535) and fog matching background
- Demo cubes still use old `_rasterizeTriangle:` with Phong (demo path untouched)
- All Phase 5 optimizations (cached bounds, dynamic planes, mesh culling, fog precomputation)
- Camera controls, mesh loading, page/project UI all unchanged

### Files modified
```
Renderer.mm  — removed FlatVert + flat rasterizer, added smooth Lambert rasterizer, rewrote _renderLoadedMesh
```

### Files unchanged
```
Renderer.h         — no API changes
ViewController.mm  — no changes
MeshLoader.hpp/cpp — no changes (smooth normals already computed here)
MathUtils.hpp      — no changes
ProjectLoader.h/mm — no changes
```

---

## 2026-03-23 — Phase 5c (unification): Single Lambert Shading Path

### What was done

1. **Unified all rendering to single Lambert rasterizer**:
   - Demo cubes, floor, and loaded meshes now all use `_rasterizeLambertTriangle:`
   - Consistent lighting across all geometry — no visual differences between demo and project
   - Added `checkerboard:(BOOL)` parameter for floor pattern support

2. **Removed Phong shading entirely**:
   - Deleted `_rasterizeTriangle:` method (old Phong rasterizer with specular, pow, reflection)
   - Deleted `phongShade()` function from MathUtils.hpp
   - Deleted `Vertex` and `Triangle` structs from MathUtils.hpp (unused)

3. **Removed dynamic light computation**:
   - Deleted camera-follow light logic (mesh mode)
   - Deleted orbiting demo light animation
   - Deleted `lightPos` and `lightColor` variables from `renderFrameWithTime:`
   - Removed `lightPos`/`lightColor` parameters from all internal render methods

4. **Simplified method signatures**:
   - `_renderFloorWithVP:eye:` (was `_renderFloorWithVP:lightPos:lightColor:eye:time:`)
   - `_renderCubeWithModel:vp:baseColor:eye:` (was `...lightPos:lightColor:eye:`)
   - `_renderLoadedMesh:vp:eye:` (was `...lightPos:lightColor:eye:`)
   - `_rasterizeLambertTriangle:b:c:eye:checkerboard:` (single unified rasterizer)

### Code removed (lines saved)
- ~90 lines of Phong rasterizer
- ~35 lines of phongShade/Vertex/Triangle definitions
- ~15 lines of dynamic light computation
- Total: ~140 lines removed, renderer reduced from 665 to 563 lines

### What was preserved
- Gray background and fog (unchanged)
- Back-face culling (unchanged)
- Smooth vertex normals for loaded meshes (unchanged)
- Per-face normals for demo cubes (same visual — cube faces are flat)
- Checkerboard floor pattern (ported to Lambert rasterizer)
- All Phase 5 optimizations (cached bounds, dynamic planes, mesh culling)
- Camera controls, mesh loading, page/project UI unchanged

### Files modified
```
Renderer.mm   — unified to single Lambert rasterizer, removed Phong path + dynamic lights
MathUtils.hpp — removed phongShade(), Vertex, Triangle (dead code)
```

### Files unchanged
```
Renderer.h         — no API changes
ViewController.mm  — no changes
MeshLoader.hpp/cpp — no changes
ProjectLoader.h/mm — no changes
```

---

## 2026-03-23 — Phase 5c (flat shading): Quantized Flat Shading

### Problem

Smooth Lambert shading (per-pixel interpolated vertex normals) introduced:
- Visible gradients on flat surfaces (half-dark/half-light meshes)
- Corner shading artifacts from area-weighted normal averaging
- Poor performance (~25 ops/pixel for per-pixel normalize + 3 dot products + fog)
- Previous flat shading attempt (pre-smooth) had diagonal seams on quads and earcut polygons
  due to floating-point differences in face normals of coplanar triangles

### Solution

Per-face flat shading with **quantized normals** — combines seam-free flat appearance with
minimal per-pixel cost:

1. Compute geometric face normal from cross product
2. Normalize, then quantize: `roundf(n * 2048) / 2048`, re-normalize
3. Coplanar triangles get identical quantized normals → identical lighting → no seams
4. Compute Lambert lighting once per triangle (ambient + 3 directional lights)
5. Rasterizer inner loop: only barycentric interpolation + depth test + write constant packed color (~5 ops/pixel)
6. Per-face fog computed from triangle centroid (not per-pixel)

### What was done

1. **Added normal quantization constant**: `kNormSnap = 2048.0f`
2. **Replaced `_rasterizeLambertTriangle:` with `_rasterizeTriangle:b:c:lighting:eye:checkerboard:`**:
   - Accepts pre-computed `lighting` float instead of doing per-pixel lighting
   - Non-checker path: pre-packs constant ARGB color, inner loop is just depth test + write
   - Checker path: retains per-pixel position interpolation for floor pattern + per-pixel fog
3. **Updated `_renderFloorWithVP:eye:`**: computes per-face lighting for floor normal (0,1,0)
4. **Updated `_renderCubeWithModel:vp:baseColor:eye:`**: computes per-face lighting per cube face
5. **Updated `_renderLoadedMesh:vp:eye:`**: quantized face normal → per-face lighting, no vertex normals
6. **Updated pipeline comment**: `Per-face Lambert → Constant-color Rasterize → Z-buffer`

### Performance impact

- Per-pixel: ~5 ops (was ~25 with smooth Lambert)
- Per-triangle: +6 ops for quantization + lighting (negligible vs saved per-pixel work)
- Net: ~5x fewer ops per pixel for non-checkerboard geometry

### Files modified
```
Renderer.mm — quantized flat shading: new rasterizer, updated all render methods, removed smooth Lambert path
```

### Files unchanged
```
Renderer.h         — no API changes
MathUtils.hpp      — no changes (worldNorm field in ScreenVert kept for compatibility)
ViewController.mm  — no changes
MeshLoader.hpp/cpp — smooth normals still computed but unused by renderer
ProjectLoader.h/mm — no changes
```

---

## 2026-03-23 — Phase 5c (fog removal): Remove Fog for Clean Flat Shading

### Problem

Per-face fog caused triangulation artifacts on flat-shaded area elements because fog distance
varied slightly across coplanar triangles (different centroids → different fog factors →
visible seams). Fog also added unnecessary per-triangle computation and obscured the CAD-style
rendering goal.

### What was done

1. **Removed all fog computation**: deleted `_fogStart`, `_invFogRange` ivars, per-frame fog
   distance calculation, per-face fog blending in rasterizer, per-pixel fog in checkerboard path
2. **Removed `kBgGray` constant**: only used for fog blending
3. **Removed `eye` parameter from rasterizer**: `_rasterizeTriangle:b:c:lighting:checkerboard:`
   (was `...lighting:eye:checkerboard:`), since eye was only used for fog distance
4. **Removed `worldNorm` from ScreenVert struct**: completely unused after quantized flat shading
5. **Simplified rasterizer**: non-checker inner loop is now purely barycentric + depth + write constant color
6. **Updated `fitCameraToMeshes` comment**: "fog/culling/far-plane" → "culling/far-plane"

### What was preserved
- Gray background (kBgARGB = 0xFF353535)
- Per-face quantized Lambert lighting (kNormSnap=2048, 3-point directional)
- Checkerboard floor pattern (per-pixel position interpolation for pattern only)
- All camera, UI, mesh loading, depth buffer unchanged

### Files modified
```
Renderer.mm — removed all fog logic, eye param from rasterizer, worldNorm from ScreenVert
```

### Files unchanged
```
Renderer.h         — no API changes
MathUtils.hpp      — no changes
ViewController.mm  — no changes
MeshLoader.hpp/cpp — no changes
ProjectLoader.h/mm — no changes
```

---

## 2026-03-23 — Phase 5d: Rasterizer Optimization (Incremental + Parallel)

### Problem

Software rasterizer FPS dropped significantly when zoomed in because each triangle covered more
pixels, and the per-pixel inner loop recomputed barycentric coordinates from scratch (~11 float
ops), tested all 3 edge functions, and branched between solid/checker paths every pixel.

### What was done

1. **Incremental barycentric + depth stepping**: Pre-compute barycentric gradients (dw/dx, dw/dy)
   and depth gradients once per triangle. Seed at (x0+0.5, y0+0.5), then step row seeds by dy
   per scanline and pixel values by dx across. Inner loop reduced from ~11 ops to ~3 adds.

2. **Scanline span clipping**: Analytically compute valid X range per scanline where all 3
   barycentric weights are non-negative. Eliminates ~50% of iterations (all outside-triangle
   pixels) and removes the 3-comparison inside-triangle test from the inner loop entirely.
   Uses `clipScanline()` static inline helper.

3. **Separate solid/checker methods**:
   - `_rasterizeTriangleSolid:b:c:packedColor:` — depth step + compare + write (~4 ops/pixel).
     No worldPos, no barycentric weights after clipping, no branching.
   - `_rasterizeTriangleChecker:b:c:darkColor:lightColor:` — incremental worldPos X/Z stepping
     for pattern, pre-packed dark/light colors (~8 ops/pixel). No per-pixel color math.
   - Deleted old `_rasterizeTriangle:b:c:lighting:checkerboard:` method.

4. **Local pointers + pre-packed colors**: Cached `_framebuffer`, `_depthBuffer`, `_width` as
   locals before the loop. Color packing moved to call sites (`packARGB()` helper). Checker
   dark/light colors pre-packed once per floor render.

5. **GCD parallel scanline bands**: Triangles with ≥64 scanlines dispatched to 4 parallel bands
   via `dispatch_apply`. Each band writes to independent rows — no data races, no locks.
   Small triangles use serial path (no dispatch overhead).

6. **Call site cleanup**:
   - `_renderFloorWithVP:` — removed unused `eye` parameter, pre-packs checker colors
   - `_renderCubeWithModel:` — packs color at call site, removed worldPos matrix multiply per vertex
   - `_renderLoadedMesh:` — packs color at call site, removed worldPos copy per vertex

### Performance summary

| Path | Before | After | Improvement |
|------|--------|-------|-------------|
| Solid inner loop | ~11 ops/pixel + 50% waste | ~4 ops/pixel, no waste | ~5x |
| Checker inner loop | ~25 ops/pixel + 50% waste | ~8 ops/pixel, no waste | ~6x |
| Large triangles (GCD) | 1 core | 4 cores | ~3-4x additional |
| Cube vertex setup | matrix multiply for worldPos | removed | saves ~600 mat muls/frame |

### Files modified
```
Renderer.mm — complete rasterizer rewrite: incremental stepping, scanline clipping, solid/checker
              split, GCD parallel bands, packARGB/clipScanline helpers, call site updates
```

### Files unchanged
```
Renderer.h         — no API changes
MathUtils.hpp      — no changes
ViewController.mm  — no changes
MeshLoader.hpp/cpp — no changes
ProjectLoader.h/mm — no changes
```

---

## 2026-03-23 — Phase 6b: Folder Hierarchy & Multi-Page Selection

### What was done

1. **Created `Folder` domain entity** (`Core/Domain/Folder.hpp`):
   - Ported from `domain/entities/folder.py`
   - Fields: `uid`, `name`, `description`, `page_uids` (child pages), `subfolders` (recursive `unordered_map<string, Folder>`)
   - `all_page_uids()` — recursively collects all page UIDs in folder and subfolders
   - `empty()` — recursive check for pruning empty folders
   - MDB source: `BidPageFolders` table (UID, Name, Description, ParentUID)

2. **Added `folder_uid` field to `Page`** (`Core/Domain/Page.hpp`):
   - Maps to `BidPageFolderUID` column in MDB
   - Empty string for pages not assigned to any folder

3. **Updated `ProjectData`** (`Core/Data/ProjectData.hpp`):
   - Added `folders` — root-level folder hierarchy (`unordered_map<string, Folder>`)
   - Added `pages_without_folder` — UIDs of pages not in any folder

4. **Updated `export_ost.py`** (`Tools/export_ost.py`):
   - `read_pages()` now reads `BidPageFolderUID` column, includes `folder_uid` per page
   - New `read_folders()` function:
     - Queries `BidPageFolders` table (`UID, Name, Description, ParentUID`)
     - Assigns pages to folders via `folder_uid`
     - Builds parent→subfolder tree from `ParentUID`
     - Prunes empty folders (no pages, no non-empty subfolders)
   - JSON output now includes `folders` and `pages_without_folder`
   - Format version bumped from 1 to 2

5. **Updated `ProjectLoader.mm`**:
   - Parses `folder_uid` from page JSON
   - New `_parseFolder:uid:` recursive parser builds `ost_domain::Folder` tree from JSON
   - Parses `pages_without_folder` array
   - Backward compatibility: if no folder data in JSON, all pages treated as folderless

6. **Rewrote page selector in `ViewController.mm`**:
   - Replaced single-page `_currentPageUID` with multi-select `_selectedPageUIDs` (`std::set<string>`)
   - Page bar shows folder buttons (bold, ▸ prefix, orange highlight) followed by their pages
   - Tapping a folder toggles all its pages (recursive `all_page_uids()`)
   - Tapping a page toggles that individual page
   - "All" button clears selection (shows everything)
   - Backward compatible: projects without folders show flat page list as before
   - Folder buttons get `kFolderTagBase` tags, pages get `kPageTagBase` tags
   - `_generateMeshesForSelectedPages` filters by `_selectedPageUIDs` set

### JSON format (version 2 additions)
```json
{
  "format_version": 2,
  "folders": {
    "<uid>": {
      "uid": "...",
      "name": "...",
      "description": "...",
      "page_uids": ["<page_uid>", ...],
      "subfolders": { "<uid>": { ... } }
    }
  },
  "pages_without_folder": ["<page_uid>", ...],
  "pages": { "<uid>": { ..., "folder_uid": "<folder_uid>" } }
}
```

### Data flow for folder selection
```
User taps folder button → _selectorButtonTapped:
  → folder.all_page_uids() collects all recursive page UIDs
  → if all already selected: deselect all → else: select all
  → _highlightSelection (updates button colors, folder=orange, page=blue)
  → _generateMeshesForSelectedPages
    → [_renderer clearMeshes]
    → iterates _currentProject.takeoffs (filtered by _selectedPageUIDs set)
    → creates meshes via MeshFactory
    → [_renderer fitCameraToMeshes]
```

### Files created
```
Core/Domain/Folder.hpp — Folder entity with recursive subfolder hierarchy
```

### Files modified
```
Core/Domain/Page.hpp       — added folder_uid field
Core/Data/ProjectData.hpp  — added folders map + pages_without_folder
Tools/export_ost.py        — folder reading, page folder_uid, format v2
ProjectLoader.mm           — folder parsing, backward compat
ViewController.mm          — multi-select page/folder UI, set-based filtering
```

### Files unchanged
```
Renderer.h/mm      — no changes
MathUtils.hpp      — no changes
MeshLoader.hpp/cpp — no changes
ProjectLoader.h    — no API changes (internal _parseFolder: is private)
ViewController.h   — no API changes
```

### What remains
1. **Build verification**: Confirm Phase 6b changes compile on LDE
2. **Re-export sample_project.json**: Run export_ost.py on a real project with folders to test
3. **Phase 7+**: Extended features (PDF, export, licensing, boolean ops)

---

## 2026-03-23 — Phase 6b (revision): Selection UX Fixes

### What was done

1. **Fixed `this` capture error** (`ViewController.mm`):
   - ObjC++ methods cannot capture `this` in C++ lambdas
   - Changed folder sort lambda to capture `const auto &folders` local reference instead

2. **Default to first page on project load** (`ViewController.mm`):
   - `_loadProject:` now pre-selects the first page (by `page_index` then name)
   - Previously showed all pages on project switch

3. **Removed "All" button** (`ViewController.mm`):
   - Deleted `kAllTag` constant and "All" button creation from `_populatePageSelector`
   - Removed `kAllTag` handling from `_selectorButtonTapped` and `_highlightSelection`

4. **Folder tap replaces selection** (`ViewController.mm`):
   - Tapping a folder now clears `_selectedPageUIDs` and inserts only that folder's pages
   - Previously toggled: second tap deselected all folder pages → empty set → showed everything
   - Clicking the same folder twice is now idempotent (re-selects same pages)

5. **Fixed dangling folder_uid edge case** (`Tools/export_ost.py`):
   - Pages whose `BidPageFolderUID` points to a non-existent folder now get `folder_uid` cleared to `""`
   - Previously such pages had non-empty `folder_uid` but appeared in `pages_without_folder` →
     iOS UI would not show them under any folder AND skip them in the orphan section

### Files modified
```
ViewController.mm  — this capture fix, first-page default, removed All button, folder replaces selection
Tools/export_ost.py — clear dangling folder_uid
```

---

## 2026-03-23 — Phase 7: Deferred Feature Porting

### What was done

1. **Reviewed and classified all deferred items** from Phases 3–6:
   - Multi-page rendering: marked as DONE (was completed in Phase 6b)
   - Removed from all deferred lists across all tracking files
   - Classified remaining items into: ready to port / blocked / no longer needed / low priority

2. **Ported OBJ export** (`Core/Export/obj_writer.hpp` + `obj_writer.cpp`):
   - Wavefront OBJ + MTL file writer following the same `ost_geometry` namespace as `dxf_writer`
   - Supports grouped meshes with per-group materials (colors)
   - `add_mesh()` accepts CppMeshData + group name + RGB color
   - `add_layered_meshes()` for batch export
   - `save()` writes both .obj and .mtl files
   - Sanitizes group/material names (spaces → underscores, special chars removed)
   - OBJ 1-based vertex indexing with correct global offsets across groups

3. **Added condition visibility toggle** (`ViewController.mm` — `_handleLayerToggle`):
   - "Layers" button added to bottom toolbar
   - Action sheet lists all conditions with checkmark/cross prefix showing visibility
   - Tapping a condition toggles `layer_visible` and regenerates meshes
   - Re-shows the action sheet after toggle so user can change multiple layers
   - "Show All" / "Hide All" bulk actions
   - Conditions sorted alphabetically by name

4. **Added OBJ export UI** (`ViewController.mm` — `_handleExport` + `_exportOBJ`):
   - "Export" button added to bottom toolbar
   - Generates meshes matching current page selection and layer visibility
   - Writes to temp directory, then shares via UIActivityViewController
   - Shares both .obj and .mtl files (AirDrop, Files, etc.)
   - Shows "Nothing to Export" alert if no visible meshes

5. **Updated all 6 tracking files** to reflect Phase 7 progress.

### Deferred items classification result

| Classification | Items |
|---------------|-------|
| **Ready to port** | DXF export UI, X coord negation, subfolder indentation, Three.js HTML export |
| **Blocked (needs library)** | PDF rendering (PDFium/PDFKit), PDF annotation (QPDF), boolean ops (Manifold), curved meshes (GLM), FBX export (FBX SDK) |
| **No longer needed for iOS** | License validation, update checking, database monitoring, employee/payroll dialogs |
| **Deferred (low priority)** | Bid tree view, settings/preferences, annotation views, cover sheet dialog |

### Files created
```
Core/Export/obj_writer.hpp  — OBJ+MTL writer API (OBJWriter, OBJMaterial)
Core/Export/obj_writer.cpp  — OBJ+MTL writer implementation
```

### Files modified
```
ViewController.mm   — Layers toggle button + handler, Export button + OBJ share sheet
PORTING_PLAN.md     — Phase 7 section, multi-page removed from deferred lists
PORTING_STATUS.md   — Phase 7 summary, condition toggle + OBJ export done
PORTING_STATUS.json — Phase 7 metadata, updated counts and notes
PORTING_MAP.md      — OBJ writer mapping, conditions_sidebar marked done
PORTING_BLOCKERS.md — B-005 partially resolved
PORTING_LOG.md      — This entry
```

### What remains
1. ~~**DXF export UI**: Wire existing dxf_writer to share sheet~~ → DONE (see below)
2. ~~**X coordinate negation**: Apply negation in MeshFactory~~ → DONE (see below)
3. ~~**Subfolder indentation**: Indent page buttons under their parent folder~~ → DONE (see below)
4. **Build verification**: Confirm all Phase 7 changes compile on LDE
5. **Phase 7+**: Evaluate GLM header-only inclusion for curved linear meshes

---

## 2026-03-23 — Phase 7 (continued): DXF Export, X Negation, Subfolder Indent, Scope Cleanup

### What was done

1. **Fixed LayeredMesh compile error** (`Core/Geometry/mesh_data.hpp` + `Core/Export/dxf_writer.hpp`):
   - `LayeredMesh` struct was defined in `dxf_writer.hpp` but `obj_writer.hpp` used it without including that header
   - Moved `LayeredMesh` to `mesh_data.hpp` (shared location) since both OBJ and DXF writers need it
   - Added `#include <string>` to `mesh_data.hpp` for `std::string` dependency
   - Removed duplicate definition from `dxf_writer.hpp`

2. **Added DXF export UI** (`ViewController.mm`):
   - Added "Export DXF (wireframe)" option to the Export action sheet
   - `_exportDXF` method generates meshes matching current page selection + layer visibility
   - Creates per-condition layers with correct RGB colors
   - Writes wireframe DXF (mesh edges as lines) to temp directory
   - Shares via UIActivityViewController

3. **Implemented X coordinate negation** (`Core/Meshing/MeshFactory.cpp`):
   - Desktop `transform_to_3d` computes `{-(ost_x / ratio), ost_y / ratio}`
   - Added negation to all 3 mesh creation functions: `create_linear`, `create_count`, `create_area`
   - Linear: `x1 = -t.position[0] * sr`, `x2 = -t.position[2] * sr`
   - Count: `x = -t.position[0] * sr`
   - Area: `verts.emplace_back(-t.position[j] * sr, ...)`
   - This corrects the mirroring difference between iOS and desktop rendering

4. **Added subfolder indentation in page selector** (`ViewController.mm`):
   - `_addSubfolderButtons:` now takes a `depth:` parameter for recursive indentation
   - Each subfolder gets a dimmed label with `▹` prefix before its pages
   - Labels use smaller/semibold font and 50% white text to distinguish from buttons
   - Deeper nesting adds proportional spacing prefix
   - Updated call site to pass `depth:0` for first level

5. **Cleaned up porting plan scope**:
   - Removed all Windows-specific items (winevent, cab, HWID, monitoring)
   - Removed licensing, update checking, employee/payroll from scope entirely
   - Removed cover sheet, menu controller from deferred lists
   - Updated decision log with scope decisions
   - Simplified Python modules assessment to focus on visualization + export only
   - Cleaned up platform differences (removed resolved items P-003 through P-006)
   - Updated all blocker statuses (B-005 partially resolved, B-006 resolved)

### Files created
```
(none — all changes to existing files)
```

### Files modified
```
Core/Geometry/mesh_data.hpp — added LayeredMesh struct + #include <string>
Core/Export/dxf_writer.hpp  — removed LayeredMesh (moved to mesh_data.hpp)
Core/Export/obj_writer.hpp  — updated comment about LayeredMesh location
Core/Meshing/MeshFactory.cpp — X coordinate negation in all 3 mesh creators
ViewController.mm           — DXF export UI, subfolder indentation with depth param, dxf_writer include
PORTING_PLAN.md             — scope cleanup, removed Windows/desktop-only items, updated Phase 7
PORTING_STATUS.md           — updated summary counts, Phase 7 items, removed out-of-scope subsystems
PORTING_STATUS.json         — updated counts, phase7_notes, phase6_ui entries
PORTING_MAP.md              — removed desktop-only infrastructure entries
PORTING_BLOCKERS.md         — resolved B-006, cleaned up platform differences and risks
PORTING_LOG.md              — this entry
```

### What remains
1. ~~**Layer system**~~ → DONE (see below)
2. **Build verification**: Confirm all Phase 7 changes compile on LDE
3. **Three.js HTML export**: Only remaining "ready to port" item (pure string generation)
4. **Phase 8+**: GLM headers for curved meshes, PDFKit for PDF rendering

---

## 2026-03-23 — Phase 7 (continued): Real Layer-Based Visibility System

### What was done

1. **Inspected desktop Layer/CdnType schema**:
   - `BidLayers` table: UID, Name, Show, Sequence — per-bid visibility layers
   - `CdnTypes` table: UID, Name — global condition type categories
   - `BidConditions.BidLayerUID` → links condition to its layer
   - `BidConditions.CdnTypeUID` → links condition to its type category
   - Desktop `is_layer_visible()` checks layer's `visible` flag; defaults to true if no layer

2. **Created C++ domain structs**:
   - `Core/Domain/Layer.hpp` — uid, name, visible, sequence
   - `Core/Domain/CdnType.hpp` — uid, name

3. **Updated Condition.hpp**:
   - Added `layer_uid`, `cdn_type_uid`, `cdn_type_name` fields
   - `layer_visible` retained for backward compat with v1/v2 JSON

4. **Updated ProjectData.hpp**:
   - Added `layers` and `cdn_types` maps
   - Added `is_condition_visible(condition)` helper that checks layer visibility

5. **Updated export_ost.py** (JSON v3):
   - New `read_layers()` reads BidLayers with Name, Show, Sequence
   - New `read_cdn_types()` reads CdnTypes with Name
   - `read_conditions()` now receives layers + cdn_types, stores `layer_uid`, `cdn_type_uid`, `cdn_type_name`
   - `format_version` bumped to 3
   - Output includes `layers` and `cdn_types` top-level keys

6. **Updated ProjectLoader.mm**:
   - Parses `layers` dict → `ost_domain::Layer` structs
   - Parses `cdn_types` dict → `ost_domain::CdnType` structs
   - Conditions now load `layer_uid`, `cdn_type_uid`, `cdn_type_name`
   - Backward compat: v1/v2 JSONs without layers/cdn_types load fine

7. **Replaced Layers UI** (`ViewController.mm`):
   - When layers exist: shows real layers sorted by sequence, toggle shows/hides all linked conditions
   - When no layers (v1/v2 JSON): falls back to per-condition visibility (old behavior)
   - Show All / Hide All toggle all layers (not conditions)

8. **Updated mesh generation + export visibility**:
   - `_generateMeshesForSelectedPages`, `_exportOBJ`, `_exportDXF` all use `is_condition_visible()` when layers exist
   - Falls back to `condition.layer_visible` for v1/v2 JSON

### Files created
```
Core/Domain/Layer.hpp   — Layer struct (uid, name, visible, sequence)
Core/Domain/CdnType.hpp — CdnType struct (uid, name)
```

### Files modified
```
Core/Domain/Condition.hpp  — added layer_uid, cdn_type_uid, cdn_type_name
Core/Data/ProjectData.hpp  — added layers, cdn_types maps + is_condition_visible()
Tools/export_ost.py        — read_layers(), read_cdn_types(), JSON v3 with layer/type export
ProjectLoader.mm           — parse layers + cdn_types, condition layer_uid/cdn_type fields
ViewController.mm          — real layer-based Layers UI, layer visibility in mesh gen + exports
PORTING_PLAN.md            — Phase 7 steps 7-10, Layer/CdnType in entities
PORTING_STATUS.md          — updated summary counts, domain structs table
PORTING_STATUS.json        — updated domain_model, phase7_notes, counts
PORTING_MAP.md             — Layer.hpp and CdnType.hpp mappings
PORTING_LOG.md             — this entry
```

### What remains
1. **Build verification**: Confirm all changes compile
2. **Re-export sample_project.json**: Run export_ost.py with v3 format on real project to test layers
3. ~~**Three.js HTML export**~~ → DONE (see below)

---

## 2026-03-23 — Phase 7 (continued): Three.js Export + Layers UX Overhaul

### What was done

1. **Layers UX overhaul** (`ViewController.mm`):
   - Replaced the re-opening action sheet with a persistent UITableView in a sheet
   - Each layer row has a checkmark (visible) or no accessory (hidden)
   - Tapping a row toggles visibility instantly — mesh regenerates in place
   - "Show All" and "Hide All" buttons in the navigation bar
   - "Done" button to dismiss — user stays in the panel until they explicitly close it
   - Sheet uses `UISheetPresentationControllerDetent` (medium/large) with grabber on iOS 15+
   - UITableViewDataSource/Delegate implemented directly on ViewController

2. **Removed backward compatibility code**:
   - Removed `_handleLayerToggleFallback:` method entirely
   - Removed all `_currentProject.layers.empty()` → `condition.layer_visible` fallback branches
   - All 3 visibility checks (mesh gen, OBJ export, DXF export) now use `is_condition_visible()` uniformly
   - `is_condition_visible()` already handles missing layer_uid gracefully (returns true)

3. **Three.js HTML export** (`Core/Export/threejs_writer.hpp/.cpp`):
   - Ported from desktop `infrastructure/visualization/renderers/threejs/`
   - Self-contained HTML file with embedded JSON scene data
   - Three.js loaded from unpkg CDN (v0.152.0)
   - Coordinate transform: `(x, y, z)` → `(x, z, -y)` matching desktop
   - Per-vertex normals computed from face geometry
   - Camera auto-positioned based on scene bounds (center + 1.5x size offset)
   - 3-point lighting: ambient + key + fill + rim
   - MeshPhongMaterial with flat shading, double-sided, per-mesh color
   - OrbitControls for mouse/touch interaction
   - Responsive canvas resizing
   - `ThreeJSWriter::add_mesh()` / `save()` API matching OBJ/DXF pattern

4. **Export menu updated** (`ViewController.mm`):
   - Added "Export HTML (3D viewer)" option
   - `_exportThreeJS` generates meshes, writes HTML to temp dir, shares via UIActivityViewController

### Files created
```
Core/Export/threejs_writer.hpp — ThreeJSWriter API
Core/Export/threejs_writer.cpp — HTML template + JSON scene builder
```

### Files modified
```
ViewController.mm   — Layers UX (UITableView sheet), backward compat removal, Three.js export, objc/runtime import
PORTING_PLAN.md     — Phase 7 steps 11-13
PORTING_STATUS.md   — Updated counts
PORTING_STATUS.json — Updated counts and phase7_notes
PORTING_MAP.md      — Three.js mapping
PORTING_LOG.md      — This entry
```

### What remains
1. ~~**Architecture refactor**~~ → DONE (see below)
2. **Build verification**: Confirm all changes compile
3. **Re-export sample_project.json** with v3 format to test layer system
4. **Phase 8+**: GLM headers for curved meshes, PDFKit for PDF rendering

---

## 2026-03-23 — Phase 8: Architecture Refactor

### What was done

Refactored the largest file (`ViewController.mm`, 1148 lines) into clean architecture layers.

**Analysis of ViewController.mm before refactor:**
- UI setup + layout (presentation) ✓ stays
- Toolbar + camera actions (presentation) ✓ stays
- Layer toggle UI + table view (presentation) ✓ stays
- Export orchestration: OBJ, DXF, Three.js (~240 lines) → **extracted to ExportService**
- Project loading + built-in test scene (~110 lines) → **extracted to ProjectService**
- Page selection + mesh generation (~90 lines) → **extracted to ProjectService**
- Page/folder selector UI (presentation) ✓ stays
- Render loop + gestures (presentation) ✓ stays

**New files created:**

1. **`ExportService.h/.mm`** (application layer, 171 lines):
   - `+exportOBJ:selectedPages:` → returns `NSArray<NSURL *>` or nil
   - `+exportDXF:selectedPages:` → returns `NSArray<NSURL *>` or nil
   - `+exportThreeJS:selectedPages:` → returns `NSArray<NSURL *>` or nil
   - Shared `generateVisibleMeshes()` function eliminates the triplicated takeoff loop
   - Returns file URLs — ViewController only handles the share sheet presentation

2. **`ProjectService.h/.mm`** (application layer, 247 lines):
   - Owns `ProjectData`, `selectedPageUIDs`, `pageList`, `folderOrder`, `sortedLayerUIDs`
   - `loadDefaultProjectIntoRenderer:` — tries bundled → Documents → built-in test
   - `loadProject:` — sets project, builds page/folder lists, default selection
   - `togglePage:` / `selectFolder:` — page selection mutations
   - `generateMeshesIntoRenderer:` — iterates visible takeoffs, sends to renderer
   - `loadBuiltInTestSceneIntoRenderer:` — fallback test scene

**ViewController.mm after refactor (807 lines, -30%):**
- Pure presentation: UI setup, toolbar, page bar, layer panel, project picker, gestures, render loop
- All export methods are 3-line wrappers: `[self _shareExportURLs:[ExportService export...]]`
- Mesh generation is a 1-line wrapper: `[_projectService generateMeshesIntoRenderer:_renderer]`
- Project loading delegates to `_projectService`

**Architecture now matches the target:**
```
Presentation:  ViewController.mm — UI, touch, render loop
Application:   ProjectService.mm — project/selection/mesh orchestration
               ExportService.mm  — file export orchestration
Infrastructure: Core/Export/      — OBJ, DXF, Three.js file writers
                Core/Meshing/     — mesh generation
                ProjectLoader.mm  — JSON parsing
Domain:         Core/Domain/      — Takeoff, Condition, Page, Layer, CdnType, Folder
                Core/Data/        — ProjectData container
```

### Files created
```
ProjectService.h    — application layer API
ProjectService.mm   — project loading, page selection, mesh generation
ExportService.h     — application layer API
ExportService.mm    — OBJ, DXF, Three.js export orchestration
```

### Files modified
```
ViewController.mm   — reduced from 1148 → 807 lines, delegates to services
PORTING_PLAN.md     — Phase 7 step 14 (architecture refactor)
PORTING_STATUS.md   — updated counts, new files in iOS project table
PORTING_STATUS.json — updated counts, new files
PORTING_MAP.md      — ProjectService + ExportService mappings
PORTING_LOG.md      — this entry
```

### What remains
1. ~~**Advanced meshing**~~ → DONE (see below)
2. **Build verification**: Confirm all changes compile
3. **Re-export sample_project.json** with v3 format

---

## 2026-03-23 — Phase 9: Advanced Meshing (Curves, Slopes, Holes)

### What was done

Ported the three remaining deferred meshing features from the desktop pipeline.

1. **Linear slope** (`MeshGenerator.cpp`):
   - `generate_linear_mesh` now accepts `rise`/`run` parameters
   - Computes `v_chg = chord_length * |rise| / |run|`
   - Adjusts Y at start/end vertices: ascending shifts end up, descending shifts start up
   - Both bottom and top vertex rows are sloped in parallel

2. **Area slope** (`MeshGenerator.cpp`):
   - `generate_area_mesh` now accepts `rise`/`run`/`rotation` parameters
   - Projects each vertex onto a rotated axis to compute slope progress
   - Per-vertex Y: `y = y_offset + thickness + progress * slope_height`
   - Ascending/descending logic matches desktop `calc_area_mesh_verts`
   - Hole vertices also get the same per-vertex slope Y

3. **Area holes** (`MeshGenerator.cpp`):
   - `generate_area_mesh` now accepts a vector of hole polygons
   - Earcut triangulation receives outer ring + hole rings via `vector<vector<Point>>`
   - Earcut flat index remapping maps to our vertex layout (outer_bot, outer_top, hole0_bot, hole0_top, ...)
   - Side walls generated for each hole ring with reversed winding
   - Hole polygon orientation detected via signed area

4. **Curved linear mesh** (`MeshGenerator.cpp`):
   - New `generate_curved_linear_mesh` function
   - **Arc fit**: Finds circle center via perpendicular bisectors of two chords (P1→C, C→P2). Validates by checking radii equality (tolerance 0.1).
   - **Bezier fallback**: Quadratic Bezier `B(t) = (1-t)²P1 + 2(1-t)tC + t²P2` when arc fit fails.
   - **Adaptive segmentation**: 6–64 segments based on chord distance, curve distance, and offset.
   - **4-strip wall generation**: inner_bot, outer_bot, inner_top, outer_top strips with averaged tangent vectors for smooth normals.
   - **Slope on curves**: Per-point Y offset interpolated along curve parameter.
   - **End caps**: Triangle pairs at both ends connecting all 4 strips.
   - **No GLM dependency**: Pure C++ math (atan2, hypot, cos, sin, clamp). B-001 blocker resolved.

5. **MeshFactory updates** (`MeshFactory.cpp/.hpp`):
   - `create_mesh_for_takeoff` now accepts optional holes parameter
   - `create_linear` detects curved path (`curve >= 0`, position.size() >= 7)
   - Computes control point from midpoint + perpendicular offset (negated per desktop)
   - Passes rise/run to both straight and curved linear generators
   - `create_area` passes holes, rise, run, rotation to area generator

6. **Hole grouping** (`ProjectService.mm`, `ExportService.mm`):
   - New `build_hole_map` function scans takeoffs for children (`is_hole() == true`)
   - Groups by `parent_uid` → list of scaled/negated hole polygons
   - Both rendering and export paths pass holes to `create_mesh_for_takeoff`

### Files modified
```
Core/Meshing/MeshGenerator.hpp — new signatures: slope params, curved linear, area holes
Core/Meshing/MeshGenerator.cpp — slope helpers, generate_curved_linear_mesh, area slope+holes
Core/Meshing/MeshFactory.hpp   — holes parameter on create_mesh_for_takeoff
Core/Meshing/MeshFactory.cpp   — curve detection, slope passthrough, holes passthrough
ProjectService.mm              — build_hole_map, pass holes to mesh generation
ExportService.mm               — build_hole_map, pass holes to export mesh generation
PORTING_PLAN.md                — Phase 9 section, deferred items marked done
PORTING_STATUS.md              — updated counts, Phase 9 row
PORTING_STATUS.json            — linear_geom status: done, blocked count: 4
PORTING_BLOCKERS.md            — B-001 resolved (GLM not needed)
PORTING_LOG.md                 — this entry
```

### What remains
1. **Build verification**: Confirm all Phase 9 changes compile
2. **Re-export sample_project.json** with v3 format to test layers + curved meshes

---

## 2026-03-24 — Phase 10: OST/XML Import with SQLite Storage

### Compilation fixes (pre-Phase 10)
- earcut template instantiation causing compiler hang: isolated into earcut_impl.cpp/hpp
- MeshGenerator.cpp (589 lines) split into 4 files: gen_linear.cpp, gen_count.cpp, gen_curved.cpp, gen_area.cpp + mesh_helpers.hpp
- IDE multi-thread compilation bug resolved by user (single-thread setting)
- Removed sacrificial zzz_build_anchor.cpp, reverted MeshFactory.mm back to .cpp

### Camera improvements (pre-Phase 10)
- Two-finger pan: added UIPanGestureRecognizer with min/maxNumberOfTouches=2, camera-relative world-space panning
- Exposed cameraTargetX/Y/Z properties on Renderer for pan support
- Extended pitch rotation limits from [0.05, 1.4] to [-1.48, 1.48] (allows viewing below meshes)
- View reset only on no-pages→some-pages transition (not on page switching)
- Simultaneous gesture recognition for pinch + 2-finger pan

### Phase 10 implementation

**Goal**: Replace JSON project loading with direct .ost (XML) import on iOS, backed by local SQLite.

**New files created (13):**

Core/Persistence/ (pure C++):
- sqlite3.h + sqlite3.c — SQLite amalgamation (vendored)
- OstDatabase.hpp/.cpp — RAII sqlite3* wrapper, Statement, transactions, WAL mode
- OstSchema.hpp/.cpp — 40+ CREATE TABLE IF NOT EXISTS DDL (SQLite types), section name arrays
- ProjectStore.hpp/.cpp — import_project(), list_projects(), load_project(), delete_project(), parse_position(), extract_z_value()

Core/Import/ (pure C++):
- pugixml.hpp + pugixml.cpp + pugiconfig.hpp — vendored pugixml library
- OstXmlParser.hpp/.cpp — parse_ost_xml() replicating Python _parse_ost_xml() via pugixml DOM

ObjC++ bridge:
- ImportService.h/.mm — security-scoped URL access, XML parse, SQLite import, NSNotification
- ProjectListViewController.h/.mm — UITableView of imported projects, UIDocumentPicker import, swipe-to-delete

**Files modified (4):**
- Config/Project.plist — UTImportedTypeDeclarations + CFBundleDocumentTypes for .ost, UniformTypeIdentifiers framework
- SceneDelegate.m → .mm — scene:openURLContexts: for file import via Files/share sheet
- ViewController.mm — _handleProjectPicker now uses ProjectListViewController, added _loadProjectFromDatabase:, OSTProjectImported notification observer
- ProjectService.mm — loadDefaultProjectIntoRenderer: checks SQLite first, falls back to bundled JSON

**Architecture:**
```
.ost file → Files app / share sheet
  → SceneDelegate.openURLContexts:
  → ImportService.importOSTFile:
  → OstXmlParser (pugixml) → OstRawData
  → ProjectStore.import_project() → SQLite (40+ tables)
  → NSNotification "OSTProjectImported"
  → ViewController._handleProjectImported:
  → ProjectStore.load_project() → ProjectData
  → existing mesh pipeline (unchanged)
```

**Key design decisions:**
- Full OST schema (40+ tables) stored in SQLite, not just geometry subset
- pugixml for XML parsing (header-only, MIT, portable C++)
- SQLite amalgamation vendored (compiled as C), no system library linking
- Multiple projects supported: all stored in Documents/ost_projects.sqlite
- UID collision check prevents duplicate imports
- JSON fallback preserved: if no SQLite projects, loads bundled JSON as before
- UTTypeData added to document picker to avoid .ost files being grayed out

**Reference files ported from Python:**
- ost_importer.py._parse_ost_xml() → OstXmlParser.cpp
- ost_schema.py → OstSchema.cpp (section arrays + singular())
- database_creator.py._TABLE_DDL → OstSchema.cpp (40+ DDL strings)
- export_ost.py.parse_position() → ProjectStore.cpp
- export_ost.py.extract_z_value() → ProjectStore.cpp
- export_ost.py SQL queries → ProjectStore.cpp.load_project()

### Files changed
```
NEW  Core/Persistence/sqlite3.h           — SQLite amalgamation header
NEW  Core/Persistence/sqlite3.c           — SQLite amalgamation source
NEW  Core/Persistence/OstDatabase.hpp     — RAII SQLite wrapper API
NEW  Core/Persistence/OstDatabase.cpp     — RAII SQLite wrapper impl
NEW  Core/Persistence/OstSchema.hpp       — DDL + section constants API
NEW  Core/Persistence/OstSchema.cpp       — 40+ table DDL, section arrays, singular()
NEW  Core/Persistence/ProjectStore.hpp    — Project store API
NEW  Core/Persistence/ProjectStore.cpp    — Import, list, load, delete + parse helpers
NEW  Core/Import/pugixml.hpp              — pugixml header (vendored)
NEW  Core/Import/pugixml.cpp              — pugixml implementation (vendored)
NEW  Core/Import/pugiconfig.hpp           — pugixml config (vendored)
NEW  Core/Import/OstXmlParser.hpp         — XML parser API
NEW  Core/Import/OstXmlParser.cpp         — XML parser implementation
NEW  ImportService.h                      — ObjC++ import bridge API
NEW  ImportService.mm                     — ObjC++ import bridge impl
NEW  ProjectListViewController.h          — Project list UI API
NEW  ProjectListViewController.mm         — Project list UI impl
MOD  Config/Project.plist                 — UTType, document types, UniformTypeIdentifiers framework
MOD  SceneDelegate.mm                     — Renamed .m→.mm, URL handling
MOD  ViewController.mm                    — ProjectListViewController, import notification
MOD  ProjectService.mm                    — SQLite-first loading
MOD  PORTING_PLAN.md                      — Phase 10 section + decision log entries
MOD  PORTING_STATUS.md                    — Phase 10 counts, updated file table
MOD  PORTING_MAP.md                       — Updated infrastructure/mdb mapping
MOD  PORTING_BLOCKERS.md                  — B-005 resolved, R-003/P-001 updated
MOD  PORTING_LOG.md                       — This entry
```
