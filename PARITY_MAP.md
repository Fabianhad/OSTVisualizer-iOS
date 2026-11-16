# iOS vs Windows parity map

- Date: 2026-11-15
- Windows repo (source of truth, read-only): `OSTVisualizerLicense`, HEAD `dceb47d`
- iOS repo: `OSTVisualizer`, HEAD `4a1302d` (working tree has the uncommitted Phase 0 files; no iOS source was edited by the mapping)
- Machine-readable twin: `divergences.json` in the Phase 1 scratchpad (`.../scratchpad/parity/divergences.json`); every item below carries the same id, rank and fields, and the JSON adds the full `testPlan` (expected values), `aliasTestPlans`, `files` and, for decisions, `question`/`options`. Test plans are not repeated here.

## Method

1. Twelve area mappers read the iOS code and the Windows code and tests, and executed both sides wherever the behaviour was numeric: the real iOS C++ sources were compiled on the host with MSVC (plus the vendored sqlite3.c and pugixml) and compared with the Windows Python modules and native `.pyd` extensions run with `python -B` and `-p no:cacheprovider`. Obj-C/UIKit code could not be compiled (no Xcode); those items are established by reading both sides and re-implementing the arithmetic.
2. Twelve independent verifiers re-checked every mapper divergence (re-running the numbers), corrected citations and test plans, and added missed items.
3. This synthesis merged mapper and verifier per area: CONFIRMED items are kept with the verifier corrections; AMBIGUOUS items become `needs-decision`; CANNOT_REPRODUCE items become `cannot-reproduce`; REFUTED items are dropped and counted; verifier "missed" items are added as CONFIRMED. All 12 verifiers returned results, so no item is flagged `unverified`.
4. Cross-area duplicates (the same underlying difference reported by several areas) were merged: one id is kept (its original mapper id), the other ids are listed as aliases and their test plans are kept under `aliasTestPlans` in the JSON. A merged item takes the highest impact of its members. Each kept item has ONE owner area for Phase 3 (the area whose primary iOS file it lives in); when the primary file is shared, the owner is the area that owns the logic being changed.
5. Ranking: impact class first (wrong-numbers, data-safety, behavior, ui-cosmetic), then by how many users and flows are affected and how central the code is (explicit order for the big classes, then number of reporting areas), then fix before needs-decision.

Impact classes: **wrong-numbers** = a number, measurement, coordinate, exported value or stored value differs from Windows; **data-safety** = data can be lost, corrupted, overwritten or left inconsistent; **behavior** = observable logic/ordering/validation/flow differs; **ui-cosmetic** = presentation only.

Disposition: `fix` = Windows behaviour is clear and reproducible; `needs-decision` = Windows is ambiguous or self-contradictory, or the behaviour is a product call (see section E); `cannot-reproduce` = cannot be reproduced on iOS (section F). Testability: `core-cpp` = host unit test against Core with the Config/Tests/ harness; `needs-extraction-from-objc` = logic must first be moved from .mm into Core; `objc-only` = UIKit/manual or device test. Verdict: result of the independent verifier. Status: `open` for every item (Phase 3 updates it).
Feature status in section G is as reported by the mapper: `matches`, `diverges`, `unverified`, `platform-adapted`.

## Summary

Unique divergences after merge: **216** (from 369 reported items; 153 merged as aliases; 1 refuted; 1 synthetic split MSH-19/MSH-19b).

| Impact | fix | needs-decision | cannot-reproduce | total |
|---|---:|---:|---:|---:|
| wrong-numbers | 62 | 4 | 0 | 66 |
| data-safety | 41 | 4 | 0 | 45 |
| behavior | 57 | 29 | 0 | 86 |
| ui-cosmetic | 11 | 7 | 1 | 19 |
| **total** | 171 | 44 | 1 | 216 |

| Owner area (Phase 3) | fix | needs-decision | cannot-reproduce | total |
|---|---:|---:|---:|---:|
| quantities | 15 | 1 | 0 | 16 |
| geometry | 4 | 0 | 0 | 4 |
| meshing | 17 | 3 | 0 | 20 |
| parsers-units | 11 | 1 | 0 | 12 |
| import-ost-osp | 16 | 7 | 0 | 23 |
| export-formats | 12 | 7 | 1 | 20 |
| database-persistence | 15 | 7 | 0 | 22 |
| conditions | 25 | 6 | 0 | 31 |
| layers-pages | 7 | 2 | 0 | 9 |
| rendering-planview | 26 | 5 | 0 | 31 |
| project-browser | 9 | 3 | 0 | 12 |
| settings-undo-camera | 14 | 2 | 0 | 16 |

Per source area (before dedupe):

| Source area | mapper items | verifier-added | refuted | ambiguous | cannot-reproduce |
|---|---:|---:|---:|---:|---:|
| quantities | 23 | 2 | 0 | 1 | 0 |
| geometry | 18 | 4 | 0 | 1 | 0 |
| meshing | 21 | 4 | 0 | 1 | 0 |
| parsers-units | 34 | 6 | 0 | 0 | 0 |
| import-ost-osp | 21 | 4 | 1 | 3 | 0 |
| export-formats | 24 | 3 | 0 | 1 | 1 |
| database-persistence | 39 | 5 | 0 | 1 | 0 |
| conditions | 31 | 8 | 0 | 1 | 0 |
| layers-pages | 25 | 4 | 0 | 2 | 0 |
| rendering-planview | 32 | 7 | 0 | 0 | 0 |
| project-browser | 21 | 2 | 0 | 0 | 0 |
| settings-undo-camera | 29 | 2 | 0 | 0 | 0 |

Refuted: IMP-21 (import-ost-osp) (IMP-21: Windows picks the format by menu entry, so routing by extension on iOS is a platform adaptation; nothing is imported either way).

iOS files edited by more than one owner area (Phase 3 must serialize these; computed from the files cited by `fix` items):

- `Controllers/ConditionsViewController.mm`: conditions, parsers-units
- `Controllers/DatabaseViewController.mm`: project-browser, settings-undo-camera
- `Controllers/EditConditionViewController/Components/EditConditionHelpers.mm`: conditions, parsers-units
- `Controllers/EditConditionViewController/Components/EditConditionViewController+Pickers.mm`: conditions, quantities
- `Controllers/EditConditionViewController/Components/EditConditionViewController+TableView.mm`: conditions, parsers-units
- `Controllers/EditConditionViewController/EditConditionViewController.mm`: conditions, parsers-units
- `Controllers/ViewController/Components/DragController.mm`: geometry, rendering-planview
- `Controllers/ViewController/Components/ViewController+Conditions.mm`: conditions, database-persistence, settings-undo-camera
- `Controllers/ViewController/Components/ViewController+Layers.mm`: database-persistence, layers-pages, rendering-planview
- `Controllers/ViewController/Components/ViewController+PageSelector.mm`: database-persistence, import-ost-osp, layers-pages, quantities
- `Controllers/ViewController/ViewController.mm`: database-persistence, geometry, layers-pages, quantities, rendering-planview, settings-undo-camera
- `Core/Domain/Bid.hpp`: database-persistence, quantities
- `Core/Export/obj_writer.cpp`: export-formats, meshing
- `Core/Geometry/coord_transform.cpp`: geometry, meshing
- `Core/Meshing/MeshFactory.cpp`: database-persistence, export-formats, meshing
- `Core/Persistence/OstDatabase.cpp`: database-persistence, import-ost-osp
- `Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp`: conditions, database-persistence, settings-undo-camera
- `Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp`: database-persistence, import-ost-osp, project-browser
- `Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp`: database-persistence, import-ost-osp
- `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp`: conditions, database-persistence, import-ost-osp, meshing, parsers-units, quantities
- `Core/Persistence/ProjectStore/Components/ProjectStore+Takeoffs.cpp`: database-persistence, parsers-units, settings-undo-camera
- `Core/Persistence/ProjectStore/ProjectStore.cpp`: database-persistence, import-ost-osp, parsers-units, project-browser
- `Core/Persistence/ProjectStore/ProjectStore_Internal.hpp`: import-ost-osp, parsers-units, settings-undo-camera
- `PlanView/Components/PlanView+Placement.mm`: geometry, rendering-planview
- `PlanView/PlanView.mm`: geometry, rendering-planview
- `Rendering/Renderer.mm`: rendering-planview, settings-undo-camera
- `Services/ExportService.mm`: export-formats, meshing, rendering-planview
- `Services/ImportService.mm`: conditions, database-persistence, import-ost-osp, settings-undo-camera
- `Services/ProjectService.mm`: export-formats, layers-pages, meshing, rendering-planview

## (A) Wrong numbers

66 items, global rank order. Items with disposition `needs-decision` also appear in section E; `cannot-reproduce` in section F.

#### 1. MSH-01: iOS divides all mesh coordinates and dimensions by the page scale ratio; Windows meshes in raw OST units
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: GEO-01, LP-01, EXP-04
- Windows: `infrastructure/visualization_provider.py:114,131-139; meshing/mesh_builder.py:106; exporters/base_exporter.py:26; renderers/threejs/mesh_processor.py:45; ...`: transform_to_3d(x,y) = (-(x/1.0), y/1.0) and ost_to_real_units(v) = v/1.0 for every page. The page sf2/sf1 never scales vertices, heights, thickness or offsets; it is only used for the page size in the flip/rotate transform. 3D coordinates are therefore OST (real-world) units for all pages.
- iOS: `Services/ProjectService.mm:119,170; Services/ExportService.mm:30,57 (sr = 1.0 / project.page_scale_ratio(page_uid), Core/Data/ProjectData.hpp:30-38) feeding Core/Meshing/ ...`: Every coordinate, height, thickness, width, depth and z offset is multiplied by 1/(sf2/sf1) of the takeoff's page. Model is paper-scale, different pages get different scales, and absolute tolerances (boolean kSnapTol 0.05, h_len < 0.001, arc radius tolerance 0.1, segment counts chord/5) act at a different relative size.
- Windows tests: tests/integration/rendering/test_page_flip_mesh.py::test_mesh_pipeline_preserves_model_scale_when_flipping; tests/integration/rendering/test_page_flip_mesh.py::PageFlip3DMeshTests::test_mesh_pipeline_preserves_model_scale_when_flipping; tests/domain/services/test_coordinate_transformation_service.py::OSTCoordinateSystemThreeDTests
- note: Earlier iOS log claims this matches desktop transform_to_3d(..., scale_ratio); it does not match the current Windows pipeline (see staleDocClaims). Fixing this also removes the sr = inf risk for sf2 = 0. Fix first: MSH-02 depends on it. Call sites: ProjectService.mm, ExportService.mm (sr = 1/ratio).

#### 2. QTY-01: Takeoffs with a parent_uid are skipped for every condition type (attachments placed on areas count 0)
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: COND-M01, LP-05
- Windows: `ost_visualizer/domain/services/condition_quantity_service.py:85-91`: primaries = takeoffs where (not condition.is_area) or parent_uid is root ('', '0', 'None'). Only AREA takeoffs with a parent are treated as holes. Non-area conditions count every takeoff even when parent_uid is set, so an attachment placed on an area contributes its own quantity (count 1).
- iOS: `Core/Domain/UomService.hpp:767-768`: 'if (t->is_hole()) continue;' is applied for every condition type, so Attachment (and any non-area) takeoffs that have a parent are dropped. Imported attachments therefore report 0. Verified: lifecycle fixture gives attachment q1=0 (Windows 1) and linear [5,7] with a parent gives 5 (Windows 12).
- Windows tests: tests/domain/services/test_condition_quantity_service.py::TakeoffLifecycleQuantityTests::test_attachment_contributes_its_own_quantity; tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_non_area_conditions_count_every_takeoff_even_with_a_parent_uid; tests/domain/services/test_condition_quantity_service.py (parent/hole cases) (+2 more, see divergences.json)
- note: Same is_hole skip exists in ProjectService.mm:167 for mesh generation (mesh area, unverified). LP-05 is the umbrella for compute_page_quantities: it also covers QTY-02, QTY-03, QTY-04 and QTY-18. COND-M01 is the same defect seen from the conditions list.

#### 3. QTY-04: round_quantity / round_up are modeled and persisted but never applied
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: COND-QTYROUND-01
- Windows: `ost_visualizer/domain/services/uom_service.py:322-333,770-777`: convert_and_round_quantity(raw, uom, round_quantity, round_up): value = convert_to_uom(raw, uom); if round_quantity and round_up > 0: increment = convert_to_uom(round_up, uom) (round_up is in inches); if increment > 0: value = ceil(value/increment)*increment. ...
- iOS: `Core/Domain/UomService.hpp:659-721,784-793`: calculate_condition_quantities has no rounding parameters and returns convert_to_uom(raw, uom) only. compute_page_quantities does not pass cdn.round_quantity/round_up.
- Windows tests: tests/domain/services/test_uom_service.py::UomConversionTests::test_rounding_goes_up_to_the_next_increment_expressed_in_the_same_unit; tests/domain/services/test_uom_service.py::UomConversionTests::test_rounding_is_skipped_without_the_flag_or_with_a_non_positive_increment; tests/domain/services/test_uom_service.py::UomAreaQuantityTests::test_area_converts_to_the_requested_unit_and_rounds_up (+7 more, see divergences.json)

#### 4. ELV-01: Fractional/mixed-number inches in condition-name elevations are not parsed; zero denominators and 1e20-scale values differ
- owner: parsers-units | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: COND-ZVAL-01, DBP-01, MSH-M04
- Windows: `ost_visualizer/infrastructure/parsers/position_parser.py:31-53 (_parse_fractional_inches_text), 60-83 (_parse_imperial_elevation), 108-129 (extract_z_value_from_name); ...`: After the marker, the value text must pass is_elevation_text (prefix regexes in domain/services/elevation_text.py:3-33), then _parse_imperial_elevation searches: feet' [-] inches [n/d]" -> feet*12 + whole + n/d; bare inches-only forms 'whole n/d"' and 'n/d"' are accepted; fractions are float(n)/float(d); ...
- iOS: `Core/Persistence/ProjectStore/ProjectStore.cpp:291-369 (extract_z_value) and 206-255 (parse_fractional_inches); ...`: Reads one number then one unit char. The fraction branch after feet+whole inches is dead code (skip_spaces at :342 already consumed the space, so the 'name[i] == space' test at :350 never fires): "Slab @T 12' 6 1/2\"" -> 150 (Windows 150.5), "Wall @T 5' 6 11/16\"" -> 66 (66.6875). ...
- Windows tests: tests/infrastructure/parsers/test_position_parser.py::ExtractZValueCharacterizationTests; ::ZeroDenominatorElevationTests; ::ElevationValueBoundaryTests (+8 more, see divergences.json)
- note: Because the iOS scanner and the Windows grammar are structurally different, patching individual cases will not converge; replace the function. ELV-02 to ELV-06 share the same fix and the same test file. ELV-01..ELV-06 are the six defects of one function (extract_z_value, ProjectStore.cpp:291-407). ...

#### 5. QTY-10: Area slope factor must use absolute rise and run (iOS uses signed values)
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: COND-AREAABS-01
- Windows: `ost_visualizer/domain/services/uom_service.py:676-682,731-735`: _calc_slope_factor(rise, run, use_absolute=(condition_type == AREA)): for areas abs(rise), abs(run) are used. Linear: only strictly positive rise AND run give sqrt(rise²+run²)/run; negative values are not a slope.
- iOS: `Core/Domain/UomService.hpp:518-523,673`: Always signed: for Area, a negative rise or run gives factor 1.0. Verified: 120x120 area, rise=-3, run=4, calc 11, SF -> 100.0 (Windows 125.0); also affects perimeter calc 14/15/33, calc 17, 21, 22, 34 and 35 and the hole area/perimeter scaling.
- Windows tests: tests/domain/services/test_uom_service.py::AreaSlopeBehaviorTests::test_area_quantity_uses_absolute_slope_magnitude; tests/domain/services/test_uom_service.py::UomGeometryHelperTests::test_slope_factor_is_the_hypotenuse_over_run_and_ignores_flat_or_unset_slopes; tests/domain/services/test_uom_service.py::UomAreaQuantityTests::test_slope_scales_area_perimeter_and_hole_measures_and_uses_magnitudes (+5 more, see divergences.json)
- note: The dialog-side half (store abs() for Area rise/run) is DLG-RISE-01.

#### 6. QTY-06: UOM normalization by measurement system (bid.measure_base) and calc-type compatibility is missing
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: COND-UOMNORM-01, DBP-06, UNITS-M01
- Windows: `ost_visualizer/domain/services/uom_service.py:246-312; loaded at ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:1005; project_data_service.py:275,327`: On every condition load (and when conditions are replaced) normalize_condition_uoms_for_system(condition, metric = bid.measure_base == 1) rewrites uom1-3: imperial->metric {1,2,3->13; 4,5,6,7->14; 8,9->16}, metric->imperial {11,13->2; 12,14->5; 15,16->8}, others unchanged; ...
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:201-203,233; Core/Domain/Bid.hpp:19`: Bid.measure_base is loaded (0/1) but never used for quantities. UOM codes are used exactly as stored. In a metric bid, imperial codes display LF/SF/CF. A Count condition whose stored uom is LF reports 1/12 = 0.0833 'LF' instead of 1 EA (Windows normalizes first).
- Windows tests: tests/domain/services/test_uom_service.py::UomValidUnitTests::test_system_normalization_maps_each_unit_to_its_counterpart; tests/domain/services/test_uom_service.py::UomValidUnitTests::test_tuple_normalization_applies_per_element; tests/domain/services/test_uom_service.py::UomValidUnitTests::test_calculation_normalization_keeps_valid_units_and_falls_back_to_the_first_valid (+10 more, see divergences.json)
- note: Windows normalizes in memory on load and also on cross-bid condition copy and measurement-system change (not an iOS feature). The imperial invalid-pair repair applies to every bid, not only metric ones. The metric half depends on QTY-05.

#### 7. GEO-06: ost_to_pdf_coordinates: iOS uses a stale legacy rotation/flip model; results differ for every rotation != 0
- owner: geometry | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: RPV-01, LP-03, GEO-16
- Windows: `cpp_extensions/src/coord_transform/coord_transform.cpp:52-85; cpp_extensions/src/common/page_transform.hpp:21-56`: points_per_unit=72*sf1/sf2. The point (x,y)*ppu in the UNROTATED page (top-left origin, width W, height H in pts) is flipped first (flip_x: W-x, flip_y: H-y), then rotated CCW: 90 gives (y, W-x), 180 gives (W-x, H-y), 270 gives (H-y, x). The result is returned with y' = output_height - y (Y-up PDF); output dims swap W/H for 90/270. ...
- iOS: `Core/Geometry/coord_transform.cpp:8-36,61-137; PlanView/PlanView.mm:258-268,300-306`: The legacy 14-argument function (coord_scale, is_page_rotated, auto_rotate_180, offsets) is retained. It applies flip against the rotated output size in OST units, then a reverse_rotation that is a reflection for 90/180/270. Same inputs: rot90 (-120,360),(720,360),(-120,440),(720,440); rot180 (240,80),(240,720),(360,80),(360,720); ...
- Windows tests: tests/domain/services/test_coordinate_transformation_service.py::OSTCoordinateSystemParsingTests::test_pdf_coordinates_honour_rotation_and_flips_on_a_scaled_page; tests/domain/services/test_coordinate_transformation_service.py::CanonicalCoordinateValidationTests; tests/domain/services/test_coordinate_transformation_service.py::OSTCoordinateSystemEdgeTests::test_pdf_coordinates_default_to_a_letter_width_when_flipping (+7 more, see divergences.json)
- note: Together with GEO-08, the display semantic to follow is the Windows one: OST positions live in the unrotated page space and the view applies flip then CCW rotation about the page centre (zoom_handler.py:339-347). ...

#### 8. MSH-02: Page rotation and flip are never applied to 3D view meshes
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: GEO-03, LP-02, RPV-28
- Windows: `meshing/mesh_builder.py:71-88,143-147; infrastructure/visualization_provider.py:115-140; domain/services/coordinate_transformation_service.py:106-154`: In the 3D view only (_MeshGeneratorAdapter), when a page has rotation != 0, flip_x or flip_y, every mesh vertex is re-projected: x_ost = -world_x, then ost_to_pdf_coordinates with page sf = 1, width = width_pts*ratio, height = height_pts*ratio, result (-pdf_x/72, (output_height - pdf_y)/72); faces are reversed when exactly one of flip_x/flip_y is set. ...
- iOS: `Services/ProjectService.mm:156-206 (no page transform); Page.rotation/flip_x/flip_y are loaded (ProjectStore+Load.cpp:83-90) but unused for meshes; ...`: Meshes are always generated unrotated and unflipped, so pages with rotation or flip are placed and oriented differently from Windows.
- Windows tests: tests/integration/rendering/test_page_flip_mesh.py (all 4 tests); tests/presentation/visualization/meshing/test_mesh_factory.py::MeshFactoryPageFlipTests; tests/integration/rendering/test_page_flip_mesh.py::PageFlip3DMeshTests (+4 more, see divergences.json)
- note: Prerequisites: page size source (Windows page.width_pts/height_pts x scale ratio / 72; iOS Page.width/height are documented as inches, unverified) and ost_to_pdf_coordinates parity (iOS coord_transform.cpp differs structurally from Windows common/page_transform.hpp, which also throws for non-quarter ...

#### 9. QTY-07: perp_slope() has the dx and dy tests swapped, breaking arc centers for axis-aligned chord segments
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: CURVE-01
- Windows: `ost_visualizer/domain/services/uom_service.py:369-374; cpp_extensions/src/linear_geom/linear_geom.cpp:29-35`: perp_slope(dx,dy): dy==0 -> +inf; dx==0 -> 0.0; else -dx/dy. (1,0) -> inf, (0,1) -> 0.0, (0,0) -> inf.
- iOS: `Core/Domain/UomService.hpp:323-330`: dx==0 -> +inf; dy==0 -> 0.0; else -dx/dy. (1,0) -> 0.0, (0,1) -> inf. A horizontal or vertical p1->control or control->p2 segment picks the wrong center formula, so arc length is wrong. Verified: position [1,0,0,1,1,1] curve=0 gives 1.6186 (Windows 2.1962, headline test expects pi/sqrt(2)=2.2214 +-0.03); arc (135,45,270 deg) R=100 gives 236.72 vs 392.64.
- Windows tests: tests/domain/services/test_uom_service.py::UomServiceTests::test_axis_aligned_circular_curve_uses_its_arc_length; tests/domain/services/test_uom_service.py::LinearQuantity::test_curve_helpers_agree_with_closed_forms; tests/domain/services/test_uom_service.py::UomCurvedArcTests::test_arcs_with_axis_aligned_chord_segments_use_the_dedicated_center_formulas (+2 more, see divergences.json)
- note: (test id for helper test is UomServiceTests... under LinearQuantity: class UomLinearQuantityTests::test_curve_helpers_agree_with_closed_forms). Fixing perp_slope alone is not enough: the direction (QTY-08) and at-end (QTY-09) rules must also change. ...

#### 10. QTY-08: Arc sweep direction uses the legacy cross-product rule instead of 'the arc through the control point'
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: CURVE-02
- Windows: `ost_visualizer/domain/services/uom_service.py:498-515; cpp_extensions/src/linear_geom/linear_geom.cpp:36-46`: ccw_to_end = (end_angle-start_angle) mod 2pi; ccw_to_control = (control_angle-start_angle) mod 2pi; angle_diff = ccw_to_end if ccw_to_control <= ccw_to_end else ccw_to_end - 2pi. The arc always passes through the control point at any sweep. Owner-confirmed 2026-11-06 per the test docstring: start 0, control 250, end 251 deg at R=100 measures 438.1.
- iOS: `Core/Domain/UomService.hpp:451-461`: clockwise = (cross1+cross2) < 0, then end_angle is shifted. This picks the minor arc when the control point is near one end of a >180 degree arc. Verified: (0,250,251) R=100 gives 190.21 (Windows 437.74); (0,110,100) gives 174.51 (Windows 453.43); 22% of random 3-point arcs differ (1314/6000), e.g. [2.806,-0.005,3.249,-0.853,-4.437,9.953] 274.98 vs 0.96. ...
- Windows tests: tests/domain/services/test_uom_service.py::UomArcControlPointSideTests; tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests; tests/domain/services/test_uom_service.py::UomArcOrientationRegressionTests (+4 more, see divergences.json)
- note: CURVE-02 also covers the straight-chord rule (QTY-09).

#### 11. QTY-09: Control point coinciding with an end point must measure the straight chord; iOS has no such rule
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/services/uom_service.py:437-455,467-468; cpp_extensions/src/linear_geom/linear_geom.cpp:48-70`: _control_is_at_an_end: min(dist(control,p1), dist(control,p2)) <= 1e-9 * chord_length -> evenly spaced straight-chord points (u*p1+t*p2) with the computed segment count. Band is relative to the chord length, not to the coordinates.
- iOS: `Core/Domain/UomService.hpp:406-480`: No at-end check: the control==end case goes to the circle fit or quadratic fallback. Verified: [0,0,60,80,60,80] curve=0 gives 115.87 (Windows 100.0); [0,0,60,80,0,0] 115.87; [0,0,100,50,100,50] 115.90 (Windows 111.80); [0,0,60,80,1e-12,-1e-12] 144.22 (Windows 100.0).
- Windows tests: tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_a_control_point_equal_to_an_end_point_is_a_straight_chord; tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_a_coincident_control_is_a_chord_through_the_seven_value_and_closed_loop_edges; tests/domain/services/test_uom_service.py::UomArcSideAndStraightChordTests::test_a_control_point_within_a_tiny_relative_epsilon_of_an_end_is_still_that_end (+3 more, see divergences.json)

#### 12. EXP-02: DXF is written in iOS Y-up coordinates; Windows DXF is Z-up (plan in XY, height in Z)
- owner: export-formats | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: MSH-M01
- Windows: `exporters/dxf_exporter.py:30-38; cpp_extensions/src/dxf/dxf_writer.cpp:442-466`: Feature-edge endpoints are taken from the raw Z-up mesh (x3d, y3d, z) with no axis transform: DXF 10/20/30 = (-ost_x, ost_y, height).
- iOS: `Services/ExportService.mm:153-155; Core/Export/dxf_writer.cpp:108-129`: Mesh vertices are written as-is in iOS Y-up space: group 10/20/30 = (X, height, -ost_y*sr). Plan and elevation are swapped relative to Windows, so a CAD plan view shows an elevation.
- Windows tests: tests/presentation/visualization/exporters/test_dxf_exporter.py
- note: The DXF writer needs no change; the conversion belongs in the exportDXF caller. DXFWriter itself stays byte-identical; convert iOS Y-up (X,Y,Z) to DXF (X,-Z,Y) in the exportDXF caller via a Core function.

#### 13. EXP-01: Three.js export applies the Z-up to Y-up swap (x,z,-y) to meshes that are already Y-up, so the model lies on its side and the camera is wrong
- owner: export-formats | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/visualization/renderers/threejs/adapters/threejs_mesh_adapter.py:69-78 (camera), 173 and 175 (vertex and normal swap)`: Source meshes are Z-up (x3d=-ost_x/ratio, y3d=ost_y/ratio, z=height). Vertices and normals are emitted as [v0, v2, -v1]. Camera position is [cx, cz+size*1.5, -cy-size] and target is [cx, cz, -cy], from source-space bounds. size = max(extents, 1.0).
- iOS: `Core/Export/threejs_writer.cpp:61 (bounds), 71-86 (camera), 104 (vertices), 118-119 (normals)`: The same (x,z,-y) swap is applied to iOS meshes, which are Y-up: X=-ost_x*sr, Y=height, Z=-ost_y*sr (gen_linear.cpp:25-34). Height lands on -Z and horizontal Z lands on Y(up). Harness result for the 100x96x6 wall: exported y-range -3..3, z-range -96..0, camera target y=-30.
- Windows tests: tests/presentation/visualization/renderers/threejs/adapters/test_threejs_mesh_adapter.py::test_threejs_adapter_frames_camera_and_bounds_in_y_up_space; tests/presentation/visualization/renderers/threejs/adapters/test_threejs_mesh_adapter.py::test_threejs_adapter_exports_layer_condition_and_area_metadata
- note: The positions are tied to EXP-02: DXF needs a different mapping from the same iOS space.

#### 14. EXP-03: Export hole polygons are mirrored in Z: ExportService forgets to negate Y
- owner: export-formats | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: MSH-12, GEO-02, LP-06, RPV-M06
- Windows: `cpp_extensions/src/coord_transform/coord_transform.cpp:27-51 (transform_holes_to_3d); presentation/visualization/meshing/mesh_builder.py:39-57`: Holes get exactly the same transform as the outer polygon (-x/ratio, +y/ratio in Z-up space). The outer polygon and its holes are always in the same space.
- iOS: `Services/ExportService.mm:33-34 (holes use +position[j+1]) vs Core/Meshing/MeshFactory.cpp:161 and Services/ProjectService.mm:122-123 (both negate Y)`: The outer ring uses (-x*sr, -y*sr) but export holes use (-x*sr, +y*sr). For a slab with ost y in 0..100 and a hole at y 20..40, the hole lands at z=+20..+40 while the slab spans z -100..0, so the hole is outside the outer ring. Harness: slab bbox z -100..40, 20 triangles, instead of 32.
- Windows tests: tests/domain/services/test_takeoff_domain_service.py::test_area_holes_are_grouped_under_their_area_parent; tests/presentation/visualization/renderers/threejs/test_mesh_processor.py::test_area_holes_and_display_options_reach_their_collaborators; tests/domain/services/test_coordinate_transformation_service.py::OSTCoordinateSystemThreeDTests::test_unrotated_3d_maps_x_to_negative_inches_and_y_to_inches (+2 more, see divergences.json)
- note: The hole-map code is a verbatim duplicate of ProjectService.mm:105-130, which is correct, so export and render disagree inside iOS too. Move the hole-map helper into Core and share it. Extract the duplicated build_hole_map into Core and share it between ProjectService.mm and ExportService.mm.

#### 15. QTY-11: Sidebar totals are summed over the 2D page plus the 3D page set at once, regardless of the active view
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: LP-04, COND-QTYSCOPE-01
- Windows: `ost_visualizer/presentation/coordinators/sidebar_coordinator.py:231-248`: In the 3D view (view stack index 0) totals use the selected 3D page uids; in the 2D view only the active 2D page. With no page, quantities are {} (every condition shows zero). compute_quantities_for_pages feeds the takeoffs of exactly those pages to compute_page_quantities.
- iOS: `Controllers/ViewController/ViewController.mm:714-726`: Takeoffs are taken from page2D OR any page in selected3DPageUIDs, whatever _is2DMode is. In 2D mode with 3 pages in the 3D selection, totals include all of them; the same takeoff is never double counted but the page set is wrong.
- Windows tests: tests/integration/refresh/test_page_condition_quantities.py; tests/integration/quantities/test_page_deletion_sidebar.py; tests/presentation/coordinators/test_sidebar_coordinator.py::test_sidebar_quantities_use_selected_pages_in_3d_view (+3 more, see divergences.json)
- note: Needs a decision only if product wants the combined set on iPad. Windows is unambiguous, so default is fix. Verifier: iOS has mutually exclusive 2D/3D modes (_planView.hidden = !_is2DMode), so the union is a real divergence, not a platform adaptation.

#### 16. QTY-02: Attachment footprint and perimeter are never aggregated (hard-coded 0), so calc 12, 22 and 33 are wrong
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/services/condition_quantity_service.py:96-108,124-125`: For each child of a primary takeoff whose condition exists, is on the same page and has is_attachment: attachment_footprint += width*depth and attachment_perimeter += 2*(width+depth) of the CHILD'S condition. These feed calc 12 (area - footprint), 22 ((area - footprint)*thickness) and 33 (perimeter + hole perimeter + attachment perimeter).
- iOS: `Core/Domain/UomService.hpp:790-791`: Passes 0.0, 0.0 with a TODO comment, so calc 12, 22 and 33 ignore attachments. calc_raw_for_area already has the right formulas (lines 607-608, 637-642, 641-642).
- Windows tests: tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_attachment_children_subtract_footprint_and_add_perimeter_to_the_parent; tests/domain/services/test_condition_quantity_service.py::TakeoffLifecycleQuantityTests::test_attachment_aware_area_formula_uses_child_dimensions; tests/domain/services/test_condition_quantity_service.py::ChildKindAndRoundingBoundaryTests::test_only_attachment_children_contribute_a_footprint_to_the_parent

#### 17. QTY-03: Any child takeoff is treated as a hole polygon (no condition-type or same-page check)
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/services/condition_quantity_service.py:96-108`: Children are used only if their condition exists, child.page_uid == parent.page_uid, and the child condition is_area with a non-empty position (hole) or is_attachment (footprint). Linear/count children and foreign-page children are ignored.
- iOS: `Core/Domain/UomService.hpp:746-749,771-780`: holes_by_parent indexes every is_hole() takeoff by parent_uid regardless of page or condition type; every child with a non-empty position is passed as a hole polygon. A 2-point linear child contributes hole perimeter 2*len (calc_polygon_perimeter closes a 2-vertex polygon), so area calc 14/15/33 are inflated: Windows 40.0 vs iOS 54.142135624 for a SQ10 paren ...
- Windows tests: tests/domain/services/test_condition_quantity_service.py::TakeoffLifecycleQuantityTests::test_foreign_page_and_missing_condition_children_do_not_subtract_area; tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_a_child_on_a_different_page_is_ignored_whichever_page_uid_sorts_first; tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_a_child_that_is_neither_area_nor_attachment_does_not_affect_the_parent (+1 more, see divergences.json)
- note: The missing-condition child case cannot occur in iOS because the loader drops takeoffs with unknown conditions.

#### 18. MSH-14: Hole classification: iOS skips every takeoff with a parent and cuts holes regardless of type, parent or visibility
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: GEO-04, EXP-10, EXP-M03, LP-14
- Windows: `domain/services/takeoff_domain_service.py:63-86; meshing/mesh_builder.py:39-57,110-112; domain/services/project_data_service.py:197-216 (visible_only); ...`: A takeoff is a hole only if its condition is an area condition AND its parent_uid is an area takeoff in the processed set (selected pages, visible layers). Orphan holes (parent missing) and holes under a non-area parent stay ordinary area/other takeoffs and are meshed normally. A hole whose condition layer is hidden is not cut. ...
- iOS: `Services/ProjectService.mm:111-128 build_hole_map and :167-168 (if takeoff.is_hole() continue); Services/ExportService.mm:23-37,54-55`: Every takeoff with parent_uid not in {'', '0'} is skipped from meshing, whatever its condition type and whether the parent exists; hole polygons are cut into the parent even if the hole's condition layer is hidden (no visibility check in build_hole_map).
- Windows tests: tests/domain/services/test_takeoff_domain_service.py::TakeoffGroupingTests::test_area_holes_are_grouped_under_their_area_parent; ...::test_a_hole_under_a_non_area_parent_stays_a_top_level_takeoff; tests/domain/services/test_takeoff_domain_service.py::test_area_holes_are_grouped_under_their_area_parent (+4 more, see divergences.json)
- note: Also fix ExportService (MSH-12) in the same shared function. Windows rule: a hole is an AREA-condition takeoff whose parent is an AREA takeoff in the processed set. RPV-17 is the 2D PlanView half of the same rule.

#### 19. DBP-04: Bid.takeoff_increments loads 0.0 for NULL TakeoffIncrements, which disables snapping; Windows uses 1.0
- owner: database-persistence | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: SNP-01, LP-19, DBP-05, LOAD-01
- Windows: `ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:126 (float(bid_row.TakeoffIncrements or 1.0)); ...`: NULL (and 0) -> 1.0 inch snap increment; plan_view/view.py:3955-3962 disables snapping only when the value <= 0 after loading.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:35; Core/Domain/Bid.hpp:24-31; used by ViewController.mm:584,936`: column_double(9) gives 0.0 for NULL, so snap_increments_ost() returns 0 and no grid snapping happens.
- Windows tests: tests/infrastructure/mdb/components/test_hierarchy_reader.py:744-857 (test_full_hierarchy_maps_every_bid_field...); tests/infrastructure/mdb/components/test_hierarchy_reader.py:858 (test_minimal_schema_hierarchy_uses_documented_defaults); tests/infrastructure/mdb/components/test_hierarchy_reader.py:534-548 (+3 more, see divergences.json)
- note: Only the NULL case is unambiguous. Verifier: the plan-view path always uses hierarchy_reader (NULL or 0 -> 1.0). The cover-sheet reader keeps 0.0 but iOS has no cover sheet. LOAD-01 ScaleFactor sub-claim was dropped (Windows is inconsistent for sf1 == 0).

#### 20. QTY-13: Takeoff Curve NULL loads as 0 (curved) instead of -1 (straight)
- owner: database-persistence | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-02, LOAD-M01
- Windows: `ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:1131-1135`: curve = int(Curve) if the column is not None else -1. Missing or NULL Curve means straight (CURVE_DISABLED = -1).
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:256; Core/Persistence/OstDatabase.cpp:73-76`: to.curve = stmt.column_int(5), and sqlite3_column_int returns 0 for NULL. Import binds empty XML fields as NULL (ProjectStore_Internal.hpp:82), so any takeoff without a Curve value gets curve=0. calc_linear_length(pos, 0) with size>=6 then measures only the first 7 values as a curved segment (a multi-segment polyline loses its other segments). ...
- Windows tests: tests/infrastructure/mdb/components/test_bid_data_reader.py::TakeoffHydrationContractTests::test_access_reader_keeps_untyped_takeoff_columns_as_extras; tests/infrastructure/mdb/components/test_bid_data_reader.py:4204 (test_explicit_zero_and_null_markers_are_roots_without_area_or_curve_loss)
- note: Overlaps the persistence area. Use the verifier-corrected plan (the two-segment polyline); a single-segment position does not discriminate.

#### 21. RPV-02: canvas_delta_to_ost ignores flips and has the wrong sign for rotation 90/270
- owner: geometry | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: GEO-08
- Windows: `plan_view/components/drag_handler.py:235-247 (scene_to_ost_delta); zoom_handler.py:339-348`: The delta is mapped through the linear part of the inverse page transform (rotation by -rot, flip scale), then multiplied by scale_ratio/(72*view_scale). Flips are honoured.
- iOS: `Core/Geometry/coord_transform.cpp:38-59; used at DragController.mm:46-47 and ViewController.mm:905-908`: Rotation-only switch: 90 -> (-dy,-dx), 180 -> (-dx,dy), 270 -> (dy,dx) times units/pt. There are no flip parameters, so a flipped page drags mirrored. Signs for 90/270 are opposite to Windows.
- Windows tests: tests/presentation/components/plan_view/components/test_drag_handler.py::test_scene_to_ost_delta_scales_by_the_pdf_ratio_without_a_page_transform; ::test_page_rotation_mixes_the_axes_in_both_directions
- note: Expected values obtained by running Windows QTransform inverse (scratch t_inverse.py). Add flip parameters to canvas_delta_to_ost.

#### 22. RPV-03: Placement canvas->OST and OST->canvas ignore flips and use the stale rotation formulas
- owner: rendering-planview | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: LP-M01
- Windows: `plan_view/components/selection_manager.py:434-443 (_scene_pos_to_ost); plan_view/view.py:2570-2573 (_ost_to_scene_pos); zoom_handler.py:339-348`: Point mapping is the exact inverse/forward of the page transform (rotation and flips), so a point placed at a screen position round-trips through the overlay mapping.
- iOS: `PlanView/Components/PlanView+Placement.mm:34-76`: Hand-written 4-way rotation switch with no flip handling, and the old rotation convention (e.g. 180: ox=(W-x)*upp, oy=y*upp). The overlay is drawn with page.flip_x/flip_y but placement ignores them, so takeoffs placed on a flipped page are stored mirrored. It also applies grid rounding inside _canvasToOst.
- Windows tests: tests/integration/placement/test_linear_increment.py (preview vs commit across rotation and flip); tests/presentation/components/plan_view/ (drag/placement under page transform); tests/domain/services/test_coordinate_transformation_service.py::OSTCoordinateSystemParsingTests::test_pdf_coordinates_honour_rotation_and_flips_on_a_scaled_page
- note: Depends on RPV-01 and RPV-02 being fixed first. Depends on GEO-06 and RPV-02. Contradicts the claim that the iOS forward and inverse maps are consistent (true only for rotation without flips).

#### 23. COL-01: Negative ColorFill is drawn grey on iOS; Windows masks the low 24 bits
- owner: parsers-units | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: EXP-07, RPV-12
- Windows: `presentation/visualization/services/color_service.py:35-40 (int_to_hex) used by get_condition_color (:225-228) and _create_hierarchy_map (:111-112)`: r = c & 0xFF, g = (c>>8)&0xFF, b = (c>>16)&0xFF on Python ints, so negative values use their two's-complement low bytes: int_to_hex(-1) = '#ffffff', int_to_hex(-16776961) = '#ff0000', int_to_hex(-2147483633) = '#0f0000'. The Edit dialog swatch also masks (edit_condition_dialog.py:870-875).
- iOS: `Core/Domain/Condition.hpp:63-67; swatches at Controllers/ConditionsViewController.mm:339-344 and EditConditionViewController+TableView.mm:265-274`: color_fill < 0 returns 0.5/0.5/0.5 (grey) for 3D meshes, 2D overlays and swatches. The colour-picker prefill (+Pickers.mm:192-196) masks the bits, so the same condition shows grey in the list and a different colour in the picker.
- Windows tests: tests/presentation/visualization/services/test_color_service.py::test_hex_helpers_round_trip_bgr_integers; ::test_condition_color_uses_bgr_fill_and_defaults_missing_fill_to_red; tests/domain/entities/test_condition.py (+1 more, see divergences.json)
- note: Negative values are rare (system colours / alpha bits) but the rule is explicit in Windows. Also update the two UIColor swatch sites (ConditionsViewController.mm, EditConditionViewController+TableView.mm) to use the same helper.

#### 24. DBP-03: Negative Condition.DisplaySize is kept instead of defaulting to 100
- owner: database-persistence | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: MSH-M03, LOAD-01
- Windows: `ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:93-100 (_normalize_display_size), used at 921`: None -> 100.0; value <= 0 -> 100.0; otherwise float(value).
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:187-189; consumer Core/Meshing/MeshFactory.cpp:119`: Only == 0.0 becomes 100.0. A negative value is kept, and MeshFactory then scales count shapes by max(display_size,0.1)/100 = 0.001 while PlanView.mm:364,721 uses 100.
- Windows tests: tests/infrastructure/mdb/components/test_bid_data_reader.py:953-966 (ConditionDisplaySizeTests)
- note: Verified with the host build: DisplaySize=-5 loads as -5.

#### 25. GEO-05: Area slope: iOS applies a descending slope for opposite rise/run signs and uses the wrong rotation sense for its mirrored frame
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: MSH-03, MSH-04
- Windows: `ost_visualizer/presentation/visualization/core/geometry/area.py:40-55; tests/presentation/visualization/core/geometry/test_area.py`: has_slope = run != 0 (rise/run truthy). proj = (x-cx)*cos(-rot)+(y-cy)*sin(-rot) in the Windows 3D frame (X=-x, Y=y). z_top = z_offset + thickness + span*prog*abs(rise)/abs(run), where prog=(proj-min)/span (span>0). The slope always rises along +proj. ...
- iOS: `Core/Meshing/gen_area.cpp:22-33,47,52-79`: asc = (rise>0&&run>0)||(rise<0&&run<0); if descending y = base + thickness + slope_h - h_chg. So rise=-1,run=4 gives a mirrored top, unlike Windows. proj uses rot=-rotation in the iOS (x,z)=(-x,-y) frame, i.e. X*cos(r)-Z*sin(r), whereas Windows is X*cos(r)+Z*sin(r) after Z=-Y. Rotation != 0 therefore slopes in a mirrored direction. ...
- Windows tests: tests/presentation/visualization/core/geometry/test_area.py::AreaSlopeBehaviorTests::test_area_mesh_slope_direction_uses_rotation_not_rise_run_signs; tests/presentation/visualization/core/geometry/test_area.py::test_area_mesh_slope_direction_uses_rotation_not_rise_run_signs
- note: Verified by running both through harnesses. PORTING_LOG.md:1492-1497 ('Ascending/descending logic matches desktop') is wrong for the current Windows code. The linear-mesh slope (up/down by sign) is a different Windows code path and is left to the meshing mapper. ...

#### 26. RPV-04: Angle snap is 22.5 degrees; Windows default is 15 degrees (22.5 is not even a valid Windows option)
- owner: rendering-planview | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: SNP-02, GEO-13
- Windows: `plan_view/components/placement_mode.py:290-310; domain/entities/config.py:32-33; domain/aggregates/config_aggregate.py:30`: snapped = round(atan2(dy,dx)/radians(angle))*radians(angle), length preserved. The unpressed angle defaults to 15; the Shift-pressed angle defaults to 0 (disabled). Valid angles are {0,1,2,3,4,5,10,15,30,45,90}. An angle <=0 returns the target unchanged.
- iOS: `PlanView/Components/PlanView+Placement.mm:3, 12-21; DragController.mm:11-24`: kSnapAngleRad = M_PI/8 (22.5 degrees), duplicated in two files. A toggle button turns it on or off.
- Windows tests: tests/presentation/components/plan_view/components/test_placement_mode.py::test_default_mouse_snap_angles_are_15_unpressed_and_off_when_pressed; ::test_zero_mouse_snap_angle_disables_angle_snap; ::test_configured_pressed_mouse_snap_angle_uses_shift_state (+3 more, see divergences.json)
- note: The Windows Shift-modifier maps to the iOS toggle; that is platform. The angle constant and the extraction are the fix. The iOS on/off toggle maps to the Windows Shift modifier (pressed = 0 = off, unpressed = 15 degrees). Configurable angle list is a Windows-only feature.

#### 27. RPV-08: Whole-shape drag rounds every vertex independently and so distorts off-grid shapes
- owner: rendering-planview | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: SNP-04, GEO-11
- Windows: `plan_view/components/drag_handler.py:293-299`: handle -1: dx = snap(pos[0]+ost_dx) - pos[0] and dy likewise from the FIRST point, then the same (dx,dy) is added to every point. Shape size is preserved. Trailing values (index >= complete pairs) are untouched; move_only_first_pair applies to the first pair only.
- iOS: `Controllers/ViewController/Components/DragController.mm:113-119`: Each vertex is rounded to the grid separately: position[i] = snapOst(start[i]+dx). Off-grid vertices are quantised independently, changing width and height.
- Windows tests: test_drag_handler.py::test_body_drag_snaps_the_first_point_and_applies_the_same_delta_to_all; ::test_body_drag_snaps_with_each_axis_of_the_first_point; ::test_body_drag_translates_every_point_and_keeps_trailing_values (+2 more, see divergences.json)
- note: Port compute_new_position into Core so the Windows ComputeNewPositionTests can run on the host. Compare with a 1e-9 tolerance (Windows adds the snapped delta, 1 ulp from an absolute snap).

#### 28. RPV-07: Increment rounding uses C round() (half away from zero); Windows uses Python round() (half to even)
- owner: rendering-planview | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: SNP-03, GEO-12
- Windows: `plan_view/components/drag_handler.py:261-264; placement_mode.py:307 (angle rounding)`: snap_ost(v) = round(v/inc)*inc with Python banker's rounding: 0.5->0, 1.5->2, 2.5->2, 3.5->4. The same applies to the angle quotient.
- iOS: `PlanView+Placement.mm:6-10, 18; DragController.mm:5-9, 21`: round() rounds ties away from zero: 0.5->1, 2.5->3.
- Windows tests: tests/presentation/components/plan_view/components/test_drag_handler.py::test_snap_ost_rounds_to_the_increment_and_ignores_non_positive_increments; tests/presentation/components/plan_view/components/test_drag_handler.py:1405-1424
- note: Ties are rare but exact at grid midpoints with whole-inch scales.

#### 29. RPV-06: Angle snap followed by per-axis grid rounding instead of Windows length-only grid snap
- owner: rendering-planview | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `placement_mode.py:380-417`: _snap_angle_for_placement: angle snap first, then ONLY if the snap kind is GRID the length from the origin is rounded to the increment along the snapped direction (a result that rounds to zero collapses to the origin); for any other kind (line snap, NONE) no rounding happens. The angle is therefore preserved exactly.
- iOS: `PlanView+Placement.mm:291-295, 359-363; DragController.mm:44-90`: The target is rounded per axis, then angle-snapped, then rounded per axis again, so the final point is generally off the snapped ray.
- Windows tests: test_placement_mode.py::test_angle_snap_distance_increment_applies_only_to_grid_snap; ::test_grid_distance_snap_rounds_to_increment_and_collapses_short_drags
- note: Corner and handle drags (DragController) use grid, then angle, with no final rounding, and match Windows compute_new_position. This item is about placement only.

#### 30. RPV-10: Multi (group) drag applies the raw delta; Windows snaps the group delta per axis
- owner: rendering-planview | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: SNP-05
- Windows: `plan_view/components/input_handler.py:1730-1735, 1787-1835`: group_dx, group_dy = snap_ost(ost_dx), snap_ost(ost_dy), added to every point of every selected takeoff; children follow their selected area ancestor; the whole move is rejected if the children become invalid.
- iOS: `Controllers/ViewController/ViewController.mm:901-923`: The unsnapped delta is added to all takeoffs of the selected condition on the page: tk.position[i] = start[i] + ostDelta.
- Windows tests: tests/presentation/components/plan_view/components/test_input_handler.py::test_group_translation_moves_nested_children_by_the_parent_displacement; ::test_group_translation_is_rejected_as_a_whole_when_children_would_become_invalid; tests/presentation/components/plan_view/components/test_input_handler.py:11548-11551,11643-11690,6867-6906
- note: Child rules (holes follow their parent only when the parent is selected) are tracked in RPV-15.

#### 31. RPV-M03: Dragging an end handle of a curved linear (6 or 7 values) is processed as a polygon corner
- owner: rendering-planview | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `plan_view/components/drag_handler.py:332-372 (compute_new_position linear branch); input_handler.py:855-861 (corner_count is set only for area conditions)`: For linears corner_count is 0. End-handle drags snap the angle against the other end and keep the control point at the same perpendicular offset from the chord (pos[4],pos[5] recomputed from the new midpoint). No validity or winding rejection applies.
- iOS: `Controllers/ViewController/Components/DragController.mm:51, 70-111`: cornerCount = position.size()/2 for every shape, so a 6-value linear has cornerCount 3 and goes through the polygon branch: the angle snap origin is the previous point (the control point for handle 0), the control point stays where it was, and the move is reverted when polygon_is_valid fails or the 3-point winding flips.
- Windows tests: tests/presentation/components/plan_view/components/test_drag_handler.py::ComputeNewPositionTests
- note: Windows values from the real compute_new_position. Applies to existing curved takeoffs imported from Windows, even though iOS 2D draws them straight (RPV-22). Even if curved rendering stays out of scope, drag should not corrupt the control point.

#### 32. MSH-05: Curved wall segment count uses double arithmetic instead of Windows' integer truncation
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `cpp_extensions/src/linear_geom/linear_geom.cpp:140-162 calc_adaptive_segs`: base = clamp(int(max(chord, 0.8*(|c-p1|+|p2-c|))/5.0), 6, 64); then base += int(offset/5.0) (offset = distance from control to the chord line, t clamped to [0,1]); clamp 6..64. Both divisions truncate separately.
- iOS: `Core/Meshing/gen_curved.cpp:118-134 calc_adaptive_segments`: Keeps fractions: base = clamp(max/5.0, 6, 64) then base += offset/5.0 as doubles, then one final truncation. Differs by +1 whenever the two fractional parts sum to >= 1 (51 of 301 random inputs).
- Windows tests: tests/integration/native/test_geometry_boundaries.py::NativeLinearArcSideAndStraightChordTests::test_the_native_arc_matches_the_python_measure_on_a_grid
- note: The same segment count drives the measured curved length in the quantity code (Windows uom_service._calc_curve_segments); check the quantity port too.

#### 33. MSH-06: Curved wall arc side (sweep direction) rule differs; arcs over about 180 degrees take the wrong route
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `cpp_extensions/src/linear_geom/linear_geom.cpp:37-47 arc_sweep, used at :112 and :210`: Sweep is CCW to the end if the control point's angle lies inside the CCW arc from start to end, otherwise the CW arc: the arc always passes through the control point whatever its length.
- iOS: `Core/Meshing/gen_curved.cpp:79-94`: clockwise = (cross(P1-ctr, C-ctr) + cross(C-ctr, P2-ctr)) < 0, then a_e is wrapped to that direction. This is wrong for major arcs: R=100 circle, 80 test cases per sweep, wrong route in 0 cases up to 181 degrees, 32/80 at 250, 64/80 at 330.
- Windows tests: tests/integration/native/test_geometry_boundaries.py::NativeLinearArcSideAndStraightChordTests::test_the_ost_confirmed_arc_follows_the_251_degree_route; ...::test_major_arcs_with_the_control_point_near_either_end; ...::test_reversed_direction_and_swapped_end_points (+2 more, see divergences.json)
- note: Windows quantity code uses the same rule (uom_service._advanced_curve_points), so the curved-length quantity on iOS (UomService.hpp) probably has the same fault; flag to the quantity mapper.

#### 34. MSH-07: Control point equal to an end point should give an evenly spaced straight chord; iOS builds a non-uniform quadratic
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `cpp_extensions/src/linear_geom/linear_geom.cpp:48-58 control_is_at_an_end (nearest end distance <= 1e-9 * chord), used at :74 and :169; ...`: If the control is within 1e-9*chord of an end, points are p1 + t*(p2-p1) with uniform t. perp_slope is exact (dy == 0 gives vertical). For 7-value positions the control is rebuilt from midpoint + perpendicular offset only when the chord length is > 0, otherwise position[4..5] is kept.
- iOS: `Core/Meshing/gen_curved.cpp:34-116 (no end-point test; v1/v2 use |dz| < 1e-10); MeshFactory.cpp:42-57 (offset form uses len > 1e-10 and falls back to the midpoint)`: A control at an end falls into the quadratic Bezier with c = endpoint, which has the same silhouette but non-uniform vertex spacing (x(t) = 100*(2t - t^2)). Degenerate zero-length 7-value curves use the midpoint instead of the stored control.
- Windows tests: tests/integration/native/test_geometry_boundaries.py::NativeLinearArcSideAndStraightChordTests::test_a_control_point_equal_to_an_end_point_is_a_straight_chord; ...::test_a_control_point_within_a_tiny_relative_epsilon_of_an_end_is_still_that_end; ...::test_the_end_point_epsilon_is_relative_to_the_chord_length (+1 more, see divergences.json)
- note: Collinear controls (control inside the chord) already match (Bezier fallback, tests test_collinear_controls_keep_the_quadratic_fallback).

#### 35. MSH-08: Ellipse count shape ignores depth (iOS draws a circular cylinder of diameter = width)
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: EXP-M01
- Windows: `meshing/mesh_factory.py:203-206; core/mesh_generator.py:458-502; core/geometry/radial.py:104-125; core/geometry/takeoff_geometry.py:95-108`: Elliptical cylinder, 24 segments, 2 cap centres (50 vertices, 96 faces), x radius = width/2, y radius = depth/2 (depth = max(condition.depth or width, 1), scaled by display_size for count), rotated by -rotation.
- iOS: `Core/Meshing/MeshFactory.cpp:133-139`: Circle and Ellipse both call generate_cylinder_mesh(x, z, width, ...) with diameter = width; depth ignored.
- Windows tests: tests/integration/rendering/test_threejs_mesh_footprint.py::test_rotated_ellipse_uses_render_minimum_only_for_two_d_threejs; tests/presentation/visualization/meshing/test_mesh_factory.py; tests/integration/rendering/test_threejs_mesh_footprint.py
- note: Documented as partial in the iOS docs. EXP-M01 also covers MSH-09, MSH-10, MSH-11 (triangle, rhombus, hexagon, octagon exported as boxes).

#### 36. MSH-09: Triangle count shape is rendered as a box
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `meshing/mesh_factory.py:211-214; core/mesh_generator.py:548-587; core/geometry/radial.py:49-73; utils/mesh.py get_triangle_mesh_faces`: Isosceles triangle prism: corners [0,-d/2],[w/2,d/2],[-w/2,d/2] (Windows XY) rotated by -rotation, 6 vertices, 8 faces.
- iOS: `Core/Meshing/MeshFactory.cpp:140-142 (falls to generate_count_mesh box width x depth)`: 8-vertex box.
- Windows tests: tests/presentation/visualization/utils/test_mesh.py::PrismTopologyTableTests::test_triangle_and_rhombus_tables_close_their_prisms; tests/integration/rendering/test_page_flip_mesh.py::test_flip_transforms_every_point_mesh_vertex_not_only_its_anchor
- note: Windows POLYGON_SHAPES also lists TRIANGLE (3, pi/6) but the factory dispatches triangle before the polygon branch, so that entry is unused for meshes.

#### 37. MSH-10: Rhombus count shape is rendered as a box
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `meshing/mesh_factory.py:207-210; core/mesh_generator.py:589-624; core/geometry/radial.py:76-101`: Rhombus prism using width only: half diagonal hw = width/2, corners [0,-hw],[0.575*hw,0],[0,hw],[-0.575*hw,0] rotated by -rotation; 8 vertices, 12 faces (get_rhombus_mesh_faces).
- iOS: `Core/Meshing/MeshFactory.cpp:140-142`: Box width x depth.
- Windows tests: tests/presentation/visualization/utils/test_mesh.py::PrismTopologyTableTests::test_triangle_and_rhombus_tables_close_their_prisms

#### 38. MSH-11: Hexagon and octagon count shapes are rendered as boxes
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `meshing/mesh_factory.py:215-219; domain/entities/shape.py:12-16 (HEXAGON (6, 0), OCTAGON (8, pi/8)); core/mesh_generator.py:504-546; core/geometry/radial.py:104-125`: Regular polygon prism, circumradius width/2, angle offset 0 (hexagon) or pi/8 (octagon), rotated by -rotation, 2 cap centres: hexagon 14 vertices / 24 faces, octagon 18 / 32.
- iOS: `Core/Meshing/MeshFactory.cpp:140-142`: Box width x depth (8 vertices).
- Windows tests: tests/presentation/visualization/utils/test_mesh.py::PrismTopologyTableTests::test_radial_tables_close_cylinders_and_polygon_prisms
- note: Ids: SQUARE 0, CIRCLE 1, TRIANGLE 2, RECTANGLE 3, RHOMBUS 12, ELLIPSE 13, HEXAGON 14, OCTAGON 15 already match iOS Types.hpp.

#### 39. MSH-13: Non-square/circle count depth: a negative condition depth becomes 1.0 on Windows, width on iOS
- owner: meshing | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `core/geometry/takeoff_geometry.py:95-108 (depth = max(condition.depth if condition.depth else width, 1.0))`: Only 0/None falls back to width; a negative depth is truthy and is clamped to 1.0.
- iOS: `Core/Meshing/MeshFactory.cpp:113 (std::max(c.depth > 0 ? c.depth : width, 1.0))`: Any non-positive depth falls back to width.
- Windows tests: none found
- note: Windows quirk found by running Windows code; matters if imported data uses -1 as an unset sentinel.

#### 40. ELV-02: Metric elevations: compound m+cm without 'and'/'+' dropped, '+' glued form fails, decimal comma, 'centimeters', signs, word boundaries
- owner: parsers-units | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `position_parser.py:86-105 (_parse_metric_elevation), 108-129; elevation_text.py:10-18`: Accepted shapes: '[-]N[.|,]N m [and|+] [-]N[.|,]N cm' (the connector is OPTIONAL), 'N m|meter|meters', 'N cm|centimeter|centimeters', case-insensitive, decimal comma = point, unit must end at a word boundary ('5cmx' invalid). A leading minus on the metres part negates metres+cm together; a minus on cm alone subtracts. Result cm/2.54 inches.
- iOS: `ProjectStore.cpp:376-404`: Number scanner accepts digits and '.' only (no comma). Only the first number is used unless the text after the unit starts with 'and' or '+' separated by a space: "Wall @T 1m 20cm" -> 100 cm only (39.37 in; Windows 47.244 in), "1.5m 20cm" -> 150 cm, "3m+20cm" -> 0 (rest_low[1]=='+' fails the 'm' test at :384), "2,5 m" -> 0, "2 CENTIMETERS"/"2 centimeter" -> ...
- Windows tests: tests/infrastructure/parsers/test_position_parser.py::MetricElevationParsingTests; ::MetricSpellingBoundaryTests; ::MetricSignAppliesToTheWholeValueTests (+2 more, see divergences.json)

#### 41. ELV-03: Elevation marker detection differs (first '@' vs last ' @T'/' @B' / ' @ ' / ' @'; leading space; case)
- owner: parsers-units | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/services/elevation.py:25-59 (parse_elevation, used by position_parser.py:112)`: Marker must be preceded by a space; the LAST ' @T' or ' @B' wins; if none, the last ' @ ' or ' @' is a legacy bare marker (bottom). T/B are case-sensitive: ' @t' falls to the bare-marker rule and its value 't 5...' is not an elevation. Text with '@' not preceded by a space ('Wall@T 5' 0"', '@T 6"', 'a@b @T ...') is not a marker.
- iOS: `ProjectStore.cpp:296-314`: Uses name.find('@') (FIRST '@' anywhere), then optionally eats a 't'/'T'/'b'/'B', case-insensitive. 'Wall @T 5' 6" @B 3' 0"' -> (66, top) (Windows (36, bottom)); 'Wall@T 5' 0"' -> (60, top) (Windows none); '@T 6"' -> (6, top) (Windows none); 'Wall @t 5' 0"' -> (60, top) (Windows none); ...
- Windows tests: tests/infrastructure/parsers/test_position_parser.py::ElevationValueBoundaryTests::test_the_last_elevation_marker_wins_and_a_legacy_bare_marker_is_a_bottom; ::CompoundMetricConversionAndEmptyBaseNameTests::test_a_marker_without_the_leading_space_is_not_an_elevation_marker; ::test_an_elevation_marker_with_an_empty_base_name_still_has_an_elevation
- note: parseElevation in EditConditionHelpers.mm already implements the correct marker rules; extract_z_value should reuse that logic (moved into Core).

#### 42. ELV-04: Validity gate: iOS accepts elevation text the Windows grammar rejects (bare feet, mixed units, decimal inches, missing closing quote, lookalike prime characters)
- owner: parsers-units | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `domain/services/elevation_text.py:3-33 (is_elevation_text prefix patterns) + position_parser.py:108-129 (imperial-only or metric-only unit check)`: Imperial needs feet' [-] whole [n/d]" with the closing quote, OR feet' followed by a word boundary ('5'x' valid, '5'' and '5' ' invalid), OR inches with closing quote; decimals in inches are outside the grammar ('6.5"', '5' 6.5"'); text that carries both unit families ('5'cm', '5'm', '6"m', '5m'', '3 meters'') yields (0.0,False). ...
- iOS: `ProjectStore.cpp:263-289 (parse_number_at accepts '.'), 324-369`: Accepts: "Wall @T 5'" and "Wall @T 5' " -> 60 top; "5' 6" (no closing quote) -> 66; "5' 6.5\"" -> 66.5; "6.5\"" -> 6.5; "1.5\"" -> 1.5; "5.5'x" -> 66; "5' 1/2\"" -> 60.5; "5'cm", "5'm", "6\"m", "3 meters'" -> values; "5' 2 m" -> 62. ...
- Windows tests: tests/infrastructure/parsers/test_position_parser.py::ElevationTextValidityCharacterizationTests; ::MixedUnitAndCacheTests; ::ElevationValueBoundaryTests::test_decimal_inches_and_nested_fractions_are_outside_the_grammar (+2 more, see divergences.json)
- note: Windows is internally inconsistent here: elevation_text.is_strippable_elevation_text (used only by export/CSV name stripping) DOES accept a trailing bare "5'", while extract_z_value rejects it. The tests pin extract_z_value, so port that behaviour as-is.

#### 43. ELV-05: Detached minus ('- 5' 6"', '- 6"', '- 1 m') returns 0
- owner: parsers-units | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `elevation_text.py:3-18 (patterns allow '-' followed by whitespace); position_parser.py:63-82,88-105 (whitespace stripped from the sign group)`: A minus separated from the number by spaces still negates the whole value: "Wall @T - 5' 6\"" -> (-66,true); 'Wall @T - 6"' -> (-6,true); 'Wall @T - 1 m' -> (-100/2.54,true); "Wall @T - 5'x" -> (-60,true); 'Wall @B - 6"' -> (-6,false).
- iOS: `ProjectStore.cpp:263-289 (parse_number_at)`: parse_number_at skips spaces after '-' but then calls std::stod on the substring including the inner space ('- 5'), which throws and is caught, so the whole call returns {0.0,false}. All detached-minus forms give (0,false).
- Windows tests: tests/infrastructure/parsers/test_position_parser.py::DetachedMinusTests; ::ElevationValueBoundaryTests::test_zero_and_negative_values_keep_their_top_or_bottom_flag

#### 44. ELV-06: Only ' ' and tab are separators; Windows treats every Unicode whitespace (NBSP, narrow NBSP, thin/figure/em/ideographic space, VT, LF, NEL) as one
- owner: parsers-units | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `position_parser.py (re patterns use \s and str.split(); _without_whitespace)`: Any str.isspace() character separates feet/inches/fraction parts and surrounds the value; U+200B (zero width space) is NOT whitespace and invalidates the text. Runs of mixed whitespace count as one separator.
- iOS: `ProjectStore.cpp:257-261 (skip_spaces: ' ' and '\t' only)`: NBSP etc. are not skipped, so e.g. "Wall @T<NBSP>5' 0\"" -> (0,false) (Windows (60,true)) and every NBSP-separated mixed number fails (also fails earlier because of ELV-01).
- Windows tests: tests/infrastructure/parsers/test_position_parser.py::UnicodeWhitespaceBoundaryTests; ::UnicodeWhitespaceInElevationNameTests; ::FractionalInchesParsingTests::test_any_unicode_whitespace_separates_a_mixed_number_like_a_space
- note: Needs a UTF-8 aware whitespace test in Core (do not use isspace on bytes).

#### 45. FMT-01: inches_to_display rounds half away from zero; Windows round() is banker's rounding; non-finite input not handled
- owner: parsers-units | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: COND-DIM-01
- Windows: `ost_visualizer/domain/services/dimension_format_service.py:26-55`: eighths = round(abs(inches)*8) with Python round-half-to-even; non-finite input returns ''. 0.0625 -> '' ; 0.3125 -> '1/4"'; 0.5625 -> '1/2"'; 0.8125 -> '3/4"'; 1.0625 -> '1"'; 1.3125 -> '1 1/4"'; 12.0625 -> '1' 0"'; 24.5625 -> '2' 0 1/2"'. Exactly 1/16-odd multiples (a common drawing dimension, e.g. 5/16") hit the tie.
- iOS: `Controllers/EditConditionViewController/Components/EditConditionHelpers.mm:106-136 (round() at :111)`: C round(): 0.0625 -> '1/8"', 0.3125 -> '3/8"', 0.5625 -> '5/8"', 0.8125 -> '7/8"', 1.0625 -> '1 1/8"'. NaN/inf cast to int is undefined behaviour. 288 of the 576 +/-k/16 values up to 3 ft differ (checked with a Python replica of the iOS arithmetic). Because the edit dialog re-parses the displayed text on save, the rounded value is also what gets persisted.
- Windows tests: tests/domain/services/test_dimension_format_service.py::DimensionDisplayTableTests; ::DimensionFormattingTests; tests/domain/services/test_dimension_format_service.py::test_rounding_goes_to_the_nearest_eighth_and_can_carry_into_feet (+1 more, see divergences.json)
- note: The Windows tests never test an odd-sixteenth tie, so the banker's behaviour is unpinned but it is what the Windows code does now. Flagged 'fix' because behaviour is clear and reproducible; if the product owner considers the Windows tie result a bug, this becomes needs-decision. ...

#### 46. QTY-05: Metric UOM codes 11-16 not supported: conversion falls back to raw inches, no labels, no metric unit lists
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/services/uom_service.py:16-21,50-57,123-134,166-177,192-211`: Codes 11 mm, 12 mm², 13 m, 14 m², 15 mm³, 16 m³ with divisors raw/(1/25.4), raw/(1000/25.4), raw/(1/25.4^2), raw/(1e6/25.4^2), raw/(1/25.4^3), raw/(1e9/25.4^3), i.e. inches*25.4, *0.0254, *645.16, *0.00064516, *16387.064, *1.6387064e-5. is_metric_uom(code). ...
- iOS: `Core/Domain/UomService.hpp:22-35,56-83,135-237; Controllers/EditConditionViewController/Components/EditConditionViewController+Pickers.mm:256`: Enum stops at 9. convert_to_uom keeps divisor 1.0 for unknown codes, so a metric condition reports raw inches (1000 in -> 1000, Windows code 11 gives 25400); get_uom_label returns '' (no unit shown); get_valid_uoms_for_calc_type has no metric parameter, so a metric bid cannot pick m/mm and new conditions never get 13/14 defaults.
- Windows tests: tests/domain/services/test_uom_service.py::UomCodeTableTests; tests/domain/services/test_uom_service.py::UomConversionTests::test_every_unit_divides_inches_by_its_size_in_inches; tests/domain/services/test_uom_service.py::UomValidUnitTests::test_every_calculation_code_selects_its_unit_family_in_both_systems (+2 more, see divergences.json)

#### 47. IMP-01: Imported bid keeps the file's BidNo; Settings.NextBidNo is never consumed
- owner: import-ost-osp | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/components/import_operations.py:134-148 (_assign_next_bid_no); ost_visualizer/infrastructure/database/settings_cardinality.py:32-60`: Inside the import transaction: read Settings.NextBidNo (more than one Settings row raises GlobalSettingsCardinalityError). next = int(value) if value not in (None,'') else 1, and 0 becomes 1 (non-numeric raises). bid_row['BidNo'] = str(next). Settings.NextBidNo = next+1, inserting a Settings row if none exists. ...
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:118-140; Core/Persistence/ProjectStore/ProjectStore.cpp:501-517 (seeds NextBidNo=1)`: bid_row is inserted verbatim, so Bids.BidNo is whatever the XML says, and Settings.NextBidNo is never read or advanced.
- Windows tests: tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_initializes_missing_global_settings_once; tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_normalizes_zero_to_the_documented_default; tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_rejects_multiple_global_settings_rows (+2 more, see divergences.json)
- note: iOS create_schema always seeds one Settings row with NextBidNo=1, so the first import on a fresh DB should get BidNo 1.

#### 48. IMP-02: Parser rewrites a backslash before an attribute's closing quote into a slash
- owner: import-ost-osp | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: XML-02
- Windows: `ost_visualizer/infrastructure/mdb/importers/ost_importer.py:427-433 (plain ET.parse, no pre-processing)`: Attribute values are taken byte-exact as parsed by expat. Verified by running the Windows importer: JobName with a trailing backslash is returned unchanged.
- iOS: `Core/Import/OstXmlParser.cpp:34-65 (sanitize_ost_xml), called at :71`: Before parsing, every backslash that immediately precedes the closing quote of an attribute value is replaced with a forward slash. Verified with a host build of the iOS parser: a JobName value ending in a backslash is stored with a slash instead. ...
- Windows tests: tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_import_without_bid_element_fails_before_writer
- note: There is no Windows test for or against the sanitizer. The fix is to remove it. If it was added for a specific real-world file, that file needs to be identified first (flagged here rather than guessed). Remove the sanitizer. If a fixture relied on it, confirm with the user.

#### 49. IMP-03: Source UID value '0' is stored as 0 instead of NULL (pages without a folder vanish from the page selector)
- owner: import-ost-osp | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/importers/ost_importer.py:295-327 (_remap_uid_value: '0' becomes 'NULL' for any field name ending in UID; ...`: After the bid-internal remap, every '*UID' attribute whose value is '0' is stored as NULL (e.g. BidPageFolderUID, MasterPageUID, ParentUID, BidLayerUID, BidConditionFolderUID). Non-UID fields such as Sequence keep '0'. ...
- iOS: `Core/Persistence/ProjectStore/ProjectStore_Internal.hpp:76-88 (only empty and 'NULL' bind NULL); consumer Controllers/ViewController/Components/ViewController+PageSelecto ...`: '0' is bound as text and stored as integer 0. Load reads page.folder_uid as the string '0'. When the project has page folders, the selector shows only pages whose folder_uid equals a folder UID or is empty, so a page with BidPageFolderUID=0 matches neither loop and gets no button. This is from code reading only; I cannot run the app.
- Windows tests: tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_remap_row_maps_or_nulls_master_data_references_per_field; tests/infrastructure/mdb/importers/test_ost_importer.py::test_remap_prefixes_name_every_reference_field_family_per_field; tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_transform_remaps_internal_global_and_null_references
- note: Overlaps IMP-06 (the same table drives both). It is split out because it is independently testable and has a visible UI effect.

#### 50. IMP-04: BidPageSettings are not canonicalised (one selected area per page)
- owner: import-ost-osp | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/importers/ost_importer.py:402-404,422-425; ost_visualizer/infrastructure/database/page_area_selection.py:20-42`: After the remap, BidPageSettings rows are rewritten as: inactive rows (BidAreaSelected <= 0, non-numeric counts as 0) first in original order, then exactly one selected row per page group. The group key is the BidPageUID, with ''/'0'/'NULL'/absent forming one no-page group. ...
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:192-211`: BidPageSettings rows are inserted as they are, so duplicate selected areas per page survive.
- Windows tests: tests/integration/import_export/test_page_area_selection.py::test_import_canonicalizes_duplicate_page_area_selection_then_save_succeeds; tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_page_uid_zero_is_never_a_page_and_collapses_as_no_page_selection; tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_blank_and_zero_page_selections_leave_one_no_page_selection
- note: Depends on IMP-03/06 (page UID zero becomes NULL before canonicalisation).

#### 51. IMP-05: No typed value conversion (YESNO, numeric, blank handling)
- owner: import-ost-osp | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-24, IMP-M04
- Windows: `ost_visualizer/infrastructure/mdb/components/import_operations.py:542-574,576-583,585-629 (_insert_raw_row, _should_omit_autonumber_uid, _convert_access_value)`: Values are converted by destination column type. YESNO (85 columns in database_creator.py, e.g. BidLayers.Show, Bids.IsAccepted, BidTakeoffs.IsNegativeQuantity, BidConditions.Grid/Trim/Backout/RoundQuantity, BidPages.FlipX/FlipY): 'True','true','-1','1' become -1, anything else including '' and 'False' becomes 0. ...
- iOS: `Core/Persistence/ProjectStore/ProjectStore_Internal.hpp:76-88 (bind_row)`: Only '' and 'NULL' become NULL; every other value is bound as text and left to SQLite column affinity. 'True'/'true' in an INTEGER column stays text and reads back as 0 through sqlite3_column_int (e.g. Layer Show in ProjectStore+Load.cpp:115-116, takeoff negative at :259). 'False'/'' on a boolean column is stored as NULL or text, not 0. ...
- Windows tests: tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_value_conversion_follows_access_column_types; tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_raw_row_insert_filters_columns_and_omits_blank_autonumber; tests/infrastructure/mdb/components/test_import_operations.py:693 (test_access_import_value_conversion_follows_access_column_types) (+1 more, see divergences.json)
- note: The iOS DDL has lost the Access YESNO/DATETIME distinction, so a per-column type table (derivable from database_creator.py _TABLE_DDL) is needed. The datetime and binary parts are split out as IMP-20. ...

#### 52. DBP-07: Takeoff position is serialized with %.10g and no newline; Windows writes full float repr with a trailing newline
- owner: parsers-units | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: QTY-M02, GEO-M04, POS-02
- Windows: `ost_visualizer/infrastructure/mdb/components/serialization.py:18-28 (encode_position), called with preserve_indices for every index at takeoff_operations.py:256-262,508-5 ...`: str(float(v)) joined by ';' plus '\n' (shortest round-trip repr, always '.0' for integers): [10.0,20.0,30.0,40.5] -> b'10.0;20.0;30.0;40.5\n'; [0.1234567,100.0005] -> b'0.1234567;100.0005\n'; full precision preserved.
- iOS: `Core/Persistence/ProjectStore/ProjectStore_Internal.hpp:90-102 (used by ProjectStore+Takeoffs.cpp:41,91)`: snprintf('%.10g') joined by ';': '10;20;30;40.5'; 1234.567890123456 -> '1234.56789'; 10000.1234567 -> '10000.12346'. Drag positions lose digits beyond the 10th significant digit.
- Windows tests: tests/infrastructure/mdb/components/test_takeoff_operations.py:1490-1507,1792,1833; tests/infrastructure/mdb/components/test_serialization.py (test_preserved_indices_keep_full_float_precision_and_others_still_round, test_dimension_fractional_roundtrip_does_not_quantize_model_geometry); tests/infrastructure/mdb/components/test_takeoff_operations.py::test_insert_preserves_unrounded_position_precision (+6 more, see divergences.json)
- note: Numeric difference is sub-1e-5 units for typical coordinates, but the Windows contract is 'no quantization'. The Position column type difference (TEXT vs BLOB) is platform-adapted. The precision loss is the real defect; ...

#### 53. GEO-07: signed_area is un-centred; loses precision at large offsets
- owner: geometry | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: RPV-11
- Windows: `cpp_extensions/src/geom_utils/geom_utils.cpp:106-119`: Computes the 2x shoelace sum relative to the first vertex ((x_i-x_0)*(y_j-y_0) - (x_j-x_0)*(y_i-y_0)), so it is translation-invariant. n=pos.size()/2; returns 0 for n<2. The concave test polygon translated by 1e8 returns exactly 12 and -12 reversed.
- iOS: `Core/Geometry/geom_utils.cpp:61-74`: Plain un-centred sum of x_i*y_j - x_j*y_i. At an offset of 1e8 the same polygon returns 16 / -16 instead of 12 / -12 (verified in the harness), and a unit square at 1e8 returns 0.0 (Windows 2.0). It is used for the winding sign in DragController.mm:93,101.
- Windows tests: tests/presentation/components/plan_view/components/test_geometry_utils.py::TakeoffLifecyclePrecisionTests::test_native_winding_and_concave_rotation_pivot_preserve_translation
- note: For realistic page coordinates (about 1e4) the sign is almost always right; the wrong magnitude is still a pinned divergence. The mesh code has its own un-centred calc_signed_area (gen_area.cpp:10-20), the same as Windows area.py:7-15 (also un-centred, /2), so that one matches.

#### 54. QTY-14: Polygon area is taken about (0,0) instead of the first vertex (precision loss for far-from-origin polygons)
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/services/uom_service.py:232-243`: Shoelace sum computed relative to vertices[0], so translation does not cost precision.
- iOS: `Core/Domain/UomService.hpp:250-263`: Raw shoelace about the origin. Verified: 2x0.1 sliver at x,y=1e8 gives 0 (Windows 0.2); 1x1 square at 1e9 gives 0 (Windows 1.0); 2x10 at 1e9 gives 0 (expected 20). Real OST coordinates are page inches (rarely above 1e4), so practical error is far below 1e-6 sq in.
- Windows tests: tests/domain/services/test_uom_service.py::UomGeometryHelperTests::test_polygon_area_is_absolute_orientation_free_and_needs_three_vertices; tests/domain/services/test_uom_service.py::UomWaveThreeReworkTests::test_polygon_area_of_a_small_polygon_a_billion_inches_from_the_origin; tests/domain/services/test_uom_service.py::AreaSlopeBehaviorTests::test_area_quantity_retains_fractional_origin_precision (+1 more, see divergences.json)

#### 55. EXP-05: HTML export applies all negatives to all positives; Windows HTML subtracts negatives only from positives of the same condition type
- owner: export-formats | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `renderers/threejs/mesh_processor.py:75-76 (metadata carries condition_type); cpp_extensions/src/module_geometry.cpp:25,76-79 (negatives_by_type)`: For HTML (and the 3D view) each positive is differenced only with negatives of its own condition_type. OBJ/DXF metadata lacks condition_type so it defaults to 0, which makes OBJ/DXF all-vs-all. Verified by running: a negative linear (type 0) crossing a slab (area, type 1) leaves the slab whole in the Windows HTML (24 vertices, x -100..0) but splits it in the ...
- iOS: `Services/ExportService.mm:71-89 (shared by exportThreeJS at 171-197)`: One all-negatives vs all-positives pass for every format, so HTML cuts slabs with linear negatives.
- Windows tests: none found
- note: ProjectService.mm:151-213 (3D view) already does per-type matching, so the render path matches. Only the export is wrong for HTML.

#### 56. EXP-06: Export ignores grayscale and display-mode opacity; Windows exports apply them
- owner: export-formats | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: SET-07
- Windows: `infrastructure/visualization_provider.py:261-274 (get_export_options); presentation/visualization/services/color_service.py:55-60,71-79,230-296; ...`: Colour comes from color_service.get_color_mapping and get_color_for_takeoff using config.grayscale_enabled (default False) and the display mode (default 'Original'). Grayscale: int_to_hex(color_fill) gives r,g,b in 0..1, gray = 0.299r+0.587g+0.114b, gray_int = int(gray*255) truncated, then hex. OBJ Kd = hex_to_rgb (6 dp); DXF layer colour = that hex; ...
- iOS: `Services/ExportService.mm:74-76,148-151,183-184; Services/ProjectService.mm:187-200`: ExportService has no RenderConfig input. It always uses the raw condition RGB (Condition::get_rgb) and writes HTML opacity=1.0 (ThreeJSWriter::add_mesh default). The 3D view applies grayscale and alpha (float gray, no 8-bit truncation), but export never does.
- Windows tests: tests/presentation/visualization/services/test_color_service.py; tests/infrastructure/test_visualization_provider.py
- note: iOS RenderConfig modes are Solid/OST/Transparent; Windows uses Solid/Original/Transparent (name mismatch for the same mode). The 3D-view grayscale lacks the int() truncation, which is an issue for the render owner. The page_area_selections inactive colour has no iOS counterpart (windowsOnly). ...

#### 57. COL-02: Grayscale is not quantised to 8 bits (and uses float32) on iOS
- owner: rendering-planview | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: SET-06, RPV-30
- Windows: `color_service.py:55-59 (_as_grayscale), 71-77`: r,g,b = byte/255.0 (double); gray = 0.299*r + 0.587*g + 0.114*b; gray_int = int(gray*255) (truncation); output byte repeated 3 times, so white (255,255,255) becomes 254 (0.299+0.587+0.114 = 0.9999999999999999) and (255,0,0) becomes 76 (0.298039..).
- iOS: `Services/ProjectService.mm:197-200; PlanView/PlanView.mm:245-248; PlanView/Components/PlanView+Placement.mm:164-167 (3 duplicated copies)`: float gray = 0.299f*r + 0.587f*g + 0.114f*b used directly as r=g=b, no truncation to 1/255 steps: red -> 0.299 (Windows 76/255 = 0.29804), white -> 1.0 (Windows 254/255). Differences up to 1/255.
- Windows tests: tests/presentation/visualization/services/test_color_service.py::ColorMappingTests::test_grayscale_desaturates_colors_and_legend_but_keeps_opacity; tests/presentation/visualization/services/test_color_service.py:194-208; tests/presentation/visualization/services/test_color_service.py::test_grayscale_desaturates_colors_and_legend_but_keeps_opacity
- note: Windows applies grayscale to the hex colour and then re-reads it, so the quantised byte is what both 2D and 3D see. Three duplicated copies of the formula (ProjectService.mm, PlanView.mm, PlanView+Placement.mm); extract one Core gray_byte().

#### 58. COL-03: 3D alpha for Pattern::None in the pattern-driven mode is 0.5 on iOS, 1.0 on Windows
- owner: rendering-planview | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: SET-04, RPV-24
- Windows: `color_service.py:264-282 (get_color_for_takeoff, line 278: pattern_type = condition.pattern if condition.pattern else pt.SOLID) with pattern.py:24-33 (get_3d_opacity)`: In Original mode the 3D opacity comes from the pattern: SOLID(1) -> 1.0; a falsy pattern (NONE = 0) is treated as SOLID -> 1.0; every other pattern (2-8) and unknown codes -> 0.5. Solid mode -> 1.0 (colour-map opacity); Transparent mode -> 0.5.
- iOS: `Services/ProjectService.mm:187-192`: is_ost: alpha = (pattern == Pattern::Solid) ? 1.0 : 0.5, so Pattern::None (0) renders at 0.5.
- Windows tests: tests/presentation/visualization/services/test_color_service.py::ColorServiceOpacityTests::test_original_mode_3d_opacity_comes_from_the_pattern_not_the_color_map; ::test_non_original_modes_ignore_the_pattern_for_3d_opacity; tests/domain/entities/test_pattern.py (+2 more, see divergences.json)
- note: iOS names the pattern mode 'OST'; Windows calls it 'Original' (see COL-MODE-01). Windows color_service substitutes SOLID for a falsy pattern before get_3d_opacity, so 3D pattern 0 gives 1.0 (pinned by test_color_service). Only the 2D rule for pattern 0 is contradictory (see COL-04).

#### 59. QTY-12: Number formatting: metric UOMs need 2 decimals when |value| < 100; iOS always rounds to 0 decimals with locale formatting
- owner: conditions | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: QTY-FMT-01
- Windows: `ost_visualizer/application/utils/quantity_display.py:4-17`: format_quantity_number: value==0 -> zero_text ''; is_metric_uom(code) and abs(value)<100 -> f'{value:,.2f}'; else f'{value:,.0f}' (comma grouping, '.' decimal, Python round-half-even on the exact binary value). format_quantity_with_uom: '<number or 0> <label>' when the label is non-empty, else the bare number.
- iOS: `Controllers/ConditionsViewController.mm:46-57`: NSNumberFormatter decimal style with maximumFractionDigits=0 for every UOM (12.345 m -> '12', Windows '12.35'); grouping separators follow the device locale. ICU rounds the shortest decimal representation, so 12.345 at 2 digits would give 12.34, while Windows gives 12.35 (exact binary value is slightly above); this is an expectation, not run on a device.
- Windows tests: tests/application/utils/test_quantity_display.py::QuantityDisplayFormattingTests; tests/application/utils/test_quantity_display.py::QuantityDisplayLabelTests; tests/integration/quantities/test_condition_uom_parity.py::ConditionUomConsistencyTests::test_metric_sidebar_properties_and_summary_use_same_uoms (+1 more, see divergences.json)
- note: Depends on QTY-05 for the metric codes to exist. Python '{:,.0f}' and NSNumberFormatter both round half-even for the 0-decimal case. Move the number logic to a Core function; use snprintf and manual comma grouping so output is locale independent.

#### 60. QTY-15: Sidebar quantities are not refreshed when the 2D page changes or the 2D/3D mode switches
- owner: quantities | impact: wrong-numbers | disposition: fix | testable: objc-only | verdict: CONFIRMED | status: open
- aliases: LP-M02
- Windows: `ost_visualizer/presentation/coordinators/sidebar_coordinator.py:218-248; tests/integration/refresh/test_page_condition_quantities.py`: Active-page changes call update_conditions_quantities (handle_active_page_changed); after conditions rebuild the projection runs once and is authoritative. Mode (view stack) is an input of the totals.
- iOS: `Controllers/ViewController/Components/ViewController+PageSelector.mm:303-320; Controllers/ViewController/ViewController.mm:359-380`: _2DButtonTapped: and _switchToMode2D: never call _updateConditionQuantities. Totals refresh only on 3D page toggle, edits/undo, place, drag end and when the conditions sheet opens (ViewController+Conditions.mm:22). With the medium-detent sheet left open while the user changes the 2D page, the displayed totals are stale.
- Windows tests: tests/integration/refresh/test_page_condition_quantities.py::PageNavigationQuantityProjectionTests; tests/presentation/coordinators/test_sidebar_coordinator.py::test_sidebar_quantities_use_selected_pages_in_3d_view; tests/integration/refresh/test_page_condition_quantities.py
- note: Becomes mandatory once QTY-11 is fixed (totals depend on the mode). Mandatory once QTY-11 makes totals mode dependent.

#### 61. DLG-PREC-01: Rise/Run/Display Size shown with %g (6 significant digits) and re-saved from that text
- owner: conditions | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: COND-M02
- Windows: `edit_condition_dialog.py:864-866 (str(cond.rise) / str(abs(cond.rise)), repr is exact) and 1494-1503/1522-1542 (value compared with stored before dto.set)`: Text uses Python repr so float(text) == stored value; unchanged rise/run are not written. Display size uses str(int(display_size)).
- iOS: `EditConditionViewController.mm:12-14 (OSTPlainDisplay '%g'), 331-333, 697-701`: rise 3.14159265 -> "3.14159" -> saved as 3.14159; run 0.333333333333 -> 0.333333. Because _save writes all fields (DLG-SAVE-01) this happens without the user touching the field.
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py (Area rise/run abs and change-only DTO)

#### 62. RPV-05: Grid snap is unconditional; Windows only snaps when the grid point is within the pixel threshold, and only when enabled with increment > 0
- owner: rendering-planview | impact: wrong-numbers | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: SNP-07
- Windows: `placement_mode.py:763-817; view.py:449-458; domain/entities/config.py:34`: _grid_snap_from_cursor: requires snap_to_grid_enabled, threshold_px > 0 and increment > 0. The snapped OST point is accepted only if the viewport distance between cursor and snapped point <= threshold (default 8 px); otherwise the raw cursor OST point is used with kind NONE. Priority is takeoff snap, then PDF line snap, then grid, then raw.
- iOS: `PlanView/Components/PlanView+Placement.mm:34-60 (rounds both axes in every case), 283-295, 353-363, 393-403`: Every placement point is rounded to the increment whenever increment > 0; there is no threshold and no enable flag.
- Windows tests: test_placement_mode.py::test_grid_fallback_still_rounds_when_snap_index_is_empty; ::test_zero_grid_threshold_disables_grid_snap; ::test_non_positive_snap_increment_disables_grid_snap (+3 more, see divergences.json)
- note: Platform decision needed only for the threshold unit (use 8 screen points); the rule itself is clear.

#### 63. COL-04: 2D fill opacity in the pattern-driven mode: Windows 0.0 for None and line patterns, iOS 0.5
- owner: rendering-planview | impact: wrong-numbers | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: RPV-23, SET-05, RPV-M05
- Windows: `color_service.py:80-83,113-116 with domain/entities/pattern.py:15-21 (get_2d_opacity)`: 2D Original mode: NONE -> 0.0, SOLID -> 1.0, HORIZONTAL..DIAG_CROSSHATCH (2-7) -> 0.0 (fill clear, hatching drawn by the PDF renderer), TRANSPARENT -> 0.5, unknown -> 0.5. 2D Transparent mode -> 0.5, Solid -> 1.0.
- iOS: `PlanView/PlanView.mm:270-277; PlanView/Components/PlanView+Placement.mm:169-176`: alpha = Solid ? 1.0 : 0.5 for every pattern in OST mode; no hatch drawing exists in PlanView/TakeoffOverlayView (grep for pattern/hatch finds only this line).
- Windows tests: tests/domain/entities/test_pattern.py::PatternOpacityTests; tests/presentation/visualization/services/test_color_service.py::ColorMappingTests::test_original_mode_takes_opacity_from_the_2d_pattern; tests/presentation/visualization/services/test_color_service.py::test_original_mode_takes_opacity_from_the_2d_pattern (+7 more, see divergences.json)
- note: Needs a user decision: reproducing 0.0 without implementing hatch patterns would make line-pattern conditions invisible on iOS. The numeric rule itself is clear.

#### 64. DBP-37: ProjectData::page_scale_ratio uses sf1 == 0 where Windows uses sf1 > 0
- owner: database-persistence | impact: wrong-numbers | disposition: needs-decision | testable: core-cpp | verdict: AMBIGUOUS | status: open
- Windows: `ost_visualizer/application/services/page_visualization_metadata_service.py:20-22 (ratio = sf2/sf1 if sf1 > 0 else 1.0)`: Any non-positive sf1 gives ratio 1.0.
- iOS: `Core/Data/ProjectData.hpp:33-43`: Only sf1 == 0.0 gives 1.0; sf1 = -2, sf2 = 5 gives -2.5.
- Windows tests: tests/application/services/test_page_visualization_metadata_service.py
- note: Edge case only. Verifier: Windows contradicts itself for a negative sf1: page_visualization_metadata_service.py:20-22 uses sf1=sf1 or 1.0, ratio = sf2/sf1 if sf1>0 else 1.0, while the mesh path (visualization_service.py:159-160 and visualization_provider.py:121-123) uses only 'or 1.0' and computes s ...

#### 65. COND-METRIC-01: Metric bids unsupported: UOM codes 11-16, mm display, metric defaults
- owner: conditions | impact: wrong-numbers | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/services/uom_service.py:16-19,39-56,123-134,166-177,194-211; presentation/dialogs/edit_condition_dialog.py:90-143,219-232,1241-1252; ...`: Bids.MeasureBase=1 switches the dialog to mm (display_to_inches/inches_to_display with metric=True), metric UOM lists (m,mm / m2,mm2 / m3,mm3), metric style defaults (uom1 13/14, spacing/thickness 100/150 mm, width 300 mm), UOM labels and divisors (UOM_M = 1000/25.4 inches, etc.), and 2-decimal quantity text when |value| < 100.
- iOS: `Core/Domain/UomService.hpp:22-83,200-237; EditConditionHelpers.mm:106-306`: Only UOM codes 0-9. convert_to_uom falls back to divisor 1.0 for unknown codes, so a metric condition (UOM 13/14/16) shows raw inches as if they were meters/m2/m3, with an empty unit label. The dialog always uses feet-inches.
- Windows tests: tests/domain/services/test_uom_service.py; tests/domain/services/test_dimension_format_service.py; tests/application/utils/test_quantity_display.py
- note: Windows duplicate_conditions_to_bid also re-normalizes UOMs for the destination measure system (not an iOS feature).

#### 66. RPV-09: Area edge-handle drag is always free; Windows constrains it to slide along the neighbouring edges unless Shift is held
- owner: rendering-planview | impact: wrong-numbers | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: GEO-10, SNP-M01
- Windows: `drag_handler.py:300-318, 375-430 (_constrained_edge_resize)`: For corner_count >= 3 and no free mode, the dragged edge moves along its normal by the projection of the delta, and its two corners are re-intersected with the adjacent edge lines (_line_line_intersect; edges shorter than 1e-9 are ignored) and then grid-snapped. ...
- iOS: `DragController.mm:53-69`: Always the free variant, with the delta angle-snapped from (0,0) when angle snapping is on.
- Windows tests: test_drag_handler.py::test_constrained_edge_resize_slides_the_edge_along_its_neighbours; ::test_constrained_edge_resize_snaps_the_new_corners; ::test_triangles_use_constrained_edge_resize_unless_free (+7 more, see divergences.json)
- note: Shift maps to a platform control; the default behaviour must be constrained. Windows free mode is the Shift modifier; the natural iOS mapping is the angle-snap toggle (ON = constrained, OFF = free). ...

## (B) Data safety

45 items, global rank order. Items with disposition `needs-decision` also appear in section E; `cannot-reproduce` in section F.

#### 67. IMP-06: No UID remapping: PK collisions overwrite other projects' rows (INSERT OR REPLACE); original UIDs kept; no global UID floor
- owner: import-ost-osp | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-09, BRW-06, LP-07
- Windows: `ost_visualizer/infrastructure/mdb/importers/ost_importer.py:242-261,263-280,282-293,353-409 (_transform, _build_uid_map, _build_page_uid_map, _remap_data); ...`: Every imported row gets a fresh UID. The floor is the max of MAX(UID) over Bids, CdnTypes, JobStatuses, all BID_SECTIONS tables (incl. BidEmployees), BidPages, BidNamedViews, BidHotLinks and all PAGE_SECTIONS tables, and of every child FK column that references those UIDs (BID_RELATIONSHIPS). New CdnTypes then JobStatuses rows take floor+1, +2, ...; ...
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:118-211 (rows inserted with their source UIDs); ...`: All source UIDs are kept as primary keys and FKs. INSERT OR REPLACE means a second project that reuses any UID silently overwrites rows of the first. Host-verified: project A (Bid 1, page 20, condition 10) then project B (Bid 2, page 20, condition 10) leaves page 20 and condition 10 owned by Bid 2 and Bid 1 with 0 pages. ...
- Windows tests: tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_transform_remaps_internal_global_and_null_references; tests/infrastructure/mdb/importers/test_ost_importer.py::test_remap_prefixes_name_every_reference_field_family_per_field; tests/infrastructure/mdb/importers/test_ost_importer.py::test_uid_assignment_skips_rows_without_a_usable_uid_without_gaps (+10 more, see divergences.json)
- note: ImportService must return and post the new Bid UID (it currently posts the file's UID, ImportService.mm:98-104,319-325). The remap also fixes the row-order effect noted above. The iOS duplicate guard (IMP-19) becomes redundant once this lands. ...

#### 68. DBP-11: SQL errors are swallowed everywhere: statements keep going after a failure, the transaction commits, and every writer returns true
- owner: database-persistence | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: BRW-07, COND-M04
- Windows: `ost_visualizer/infrastructure/mdb/mdb_writer.py:63-105 (_connection: commit only if no error, rollback and re-raise otherwise; nested depth tracking)`: Any error rolls the whole operation back and the write method returns False (or raises under the strict policy).
- iOS: `Core/Persistence/OstDatabase.cpp:56-62 (step returns rc==SQLITE_ROW only), 98-114 (no busy_timeout), 125-134; e.g. ...`: Failures of INSERT/UPDATE/DELETE are invisible. Host repro (fail_inject.py): trigger raising ABORT on DELETE FROM BidPages, then permanently_delete_project -> rc=1, Bids row deleted, BidPages row still present, all earlier cascade deletes committed. duplicate_condition returns the new uid even if the INSERT failed; import_project ignores commit().
- Windows tests: tests/integration/mdb/test_ownership_and_atomicity.py:154 (test_later_chunk_delete_failure_rolls_back_page_condition_and_folder_batches); tests/infrastructure/mdb/components/test_project_operations.py:902; tests/infrastructure/mdb/components/test_takeoff_operations.py:1074,2344 (+4 more, see divergences.json)
- note: Root cause of several other silent-success behaviors (DBP-10, DBP-12..17). Root cause: Statement::step() returns rc == SQLITE_ROW only, so DONE and error are indistinguishable. Fix in OstDatabase then make every writer check results and roll back.

#### 69. IMP-08: Import is not atomic and swallows SQL failures; unknown attributes abort the rest of the table while reporting success
- owner: import-ost-osp | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-10
- Windows: `ost_visualizer/infrastructure/mdb/components/import_operations.py:33-82,384-424,549-558`: The whole import (master data, NextBidNo, bid row, all tables) runs in one connection scope; any exception rolls everything back and import returns False. _insert_raw_row first filters the row to columns that exist in the destination table, so unknown XML attributes are dropped silently. A row with no known columns is skipped.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:8-27,104,131-139,161-168,244-245; Core/Persistence/OstDatabase.cpp:56-62`: Statements are built from every XML attribute. If one is not a column (or the table does not exist), prepare fails. insert_rows then returns false at the first bad row, which aborts all remaining rows of that table, and the caller ignores the result. The Bids and BidConditions paths skip only that row. ...
- Windows tests: tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_raw_row_insert_filters_columns_and_omits_blank_autonumber; tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rolls_back_every_write_when_a_bid_row_fails; tests/infrastructure/mdb/components/test_import_operations.py::test_a_page_table_failure_rolls_back_every_earlier_table_and_is_logged (+3 more, see divergences.json)
- note: A row-level column filter needs the destination table's column list (PRAGMA table_info), mirroring _get_table_columns.

#### 70. DBP-12: permanently_delete_project cascade leaves orphan rows that Windows delete_bids removes
- owner: database-persistence | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: BRW-04
- Windows: `ost_visualizer/infrastructure/mdb/components/bid_operations.py:82-244 (_delete_bids_cascade_chunk)`: Annotation-link tables are cleaned by BidTakeoffFromUID and BidTakeoffToUID of the bid's takeoffs; every page-scoped table is cleaned both through the bid's pages and by BidUID; BidPercents via takeoffs, BidLaborCostCodes and BidTimeCardStates parents; ...
- iOS: `Core/Persistence/ProjectStore/ProjectStore.cpp:615-751 (exec_bid at 632-640)`: Row-level differential against the real Windows code (diff_delete.py), rows remaining on iOS but deleted by Windows: BidPercents linked only by BidPageUID, BidLaborCostCodeUID or BidTimeCardStateUID; BidDimensions/BidALines/BidArrows linked only by BidTakeoffFromUID, BidTakeoffToUID or BidUID; ...
- Windows tests: tests/infrastructure/mdb/components/test_bid_operations.py:64 (test_delete_bid_removes_bid_scoped_children_without_direct_links); tests/infrastructure/mdb/components/test_bid_operations.py:261 (test_delete_bid_removes_annotation_linked_only_by_to_takeoff); tests/infrastructure/mdb/components/test_bid_operations.py:3020,3513 (+5 more, see divergences.json)
- note: The iOS comment claims it matches the Windows cascade; it does not. Required deletions are exactly the (table, column) pairs Windows bid_operations.py:107-244 deletes; Windows itself leaves a few (BidDPCSubscribers.BidEmployeeUID, BidHotLinks.BidPageUID, BidNamedViews.BidPageUID).

#### 71. COND-DEL-01: Condition delete cascade is incomplete and does not null surviving takeoff self-references
- owner: conditions | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-15, QTY-M01
- Windows: `ost_visualizer/infrastructure/mdb/components/condition_operations.py:334-546; infrastructure/mdb/components/constants.py:73-81`: Inside one transaction, after verifying every condition belongs to the bid: (1) NULLs ParentUID, TypGroupTakeoffUID, TypPageTakeoffUID, TypGroupMarkerUID on every surviving takeoff that references a deleted takeoff; (2) deletes BidDimensions/BidALines/BidArrows rows whose BidTakeoffFromUID OR BidTakeoffToUID is a deleted takeoff; ...
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:170-261; ViewController+Conditions.mm:189-223`: Deletes annotation children only by BidTakeoffFromUID, BidPercents only by takeoff, BidTakeoffs, BidLaborActivity, ConditionSetStyles, BidConditions. Never touches the To-side annotation links, BidPercents via labor activity, BidTypGroupViews/AffectDPCTypGroupViews, BidTakeoffTotals, BidTypicalGroupTotals, BidConditionUser, and does not NULL ParentUID/Typ* s ...
- Windows tests: tests/infrastructure/mdb/components/test_condition_operations.py::ConditionDeleteCascadeContractTests::test_deleting_a_condition_removes_every_dependent_of_that_condition_in_that_bid_only; tests/infrastructure/mdb/components/test_condition_operations.py::ConditionOperationsPersistenceTests::test_delete_condition_removes_all_ancillary_condition_dependents; tests/infrastructure/mdb/components/test_condition_operations.py::ConditionOperationsPersistenceTests::test_condition_delete_clears_surviving_takeoff_self_references (+5 more, see divergences.json)
- note: The iOS schema (OstSchema.cpp) already has every table/column needed for the full cascade. Leftover rows from the incomplete cascade also feed COND-DUP-02. After deleting, surviving children keep a dangling ParentUID and iOS skips them as holes. Requires bid ownership check too.

#### 72. IMP-07: Bid row keeps source-DB UID fields (BidProjectUID etc.); Windows nulls them and sets the target project to NULL
- owner: import-ost-osp | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: BRW-M02
- Windows: `ost_visualizer/infrastructure/mdb/importers/ost_importer.py:25-33,295-327; ost_visualizer/infrastructure/mdb/components/import_operations.py:74-77`: BidProjectUID, ParentBidUID, OrigBidProjectUID, OrigParentBidUID, SourceBidUID, OCRUID and CoverSheetSelItemUID become NULL on every row. EstimatorUID, PrManagerUID, JobSiteManagerUID and EmployeeUID map through the employee map or become NULL. JobStatusUID maps through the status map or becomes NULL. ...
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:118-140 and ProjectStore+Folders.cpp:45-69 (load_hierarchy)`: The file's BidProjectUID is stored as is. The Windows exporter writes the real project UID (ost_exporter.py:658, _BID_ATTR_ORDER). load_hierarchy files the bid under whichever local BidProjects row has that UID. create_schema prefers UID 1 for the 'Deleted Bids' project (ProjectStore.cpp:25-26), so an OST exported from a project with UID 1 lands inside Delet ...
- Windows tests: tests/infrastructure/mdb/importers/test_ost_importer.py::test_remap_prefixes_name_every_reference_field_family_per_field; tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_transform_remaps_internal_global_and_null_references; tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_passes_resolved_maps_to_transform_and_assigns_project (+1 more, see divergences.json)
- note: iOS has no target-folder picker (Windows ImportHandler resolves one), so NULL is the correct iOS value. Windows' Deleted-Bids-target special case is Windows-only.

#### 73. IMP-09: No raw-graph validation: Windows rejects or repairs invalid projects before any write
- owner: import-ost-osp | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-M03
- Windows: `ost_visualizer/infrastructure/mdb/importers/ost_importer.py:158-240 (_validated_raw_data, _discard_stale_named_views); ...`: Order: (1) resolve_takeoff_graph: a malformed or duplicate takeoff UID, a malformed ParentUID or a ParentUID cycle is fatal; takeoffs whose parent is missing are removed together with all their descendants (warning logged). (2) clear_missing_selected_page_references sets BidSettings.BidPageSelectedUID to '0' when a numeric UID is not a page. ...
- iOS: `Services/ImportService.mm:52-96 and 272-317 (nothing between parse and import_project)`: No validation at all. Orphan takeoffs, dangling FKs, duplicate or malformed UIDs, parent cycles, 2 BidSettings rows and a missing Bid UID are all stored. INSERT OR REPLACE collapses duplicate UIDs silently.
- Windows tests: tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_import_skips_orphaned_takeoffs_and_cascading_children; tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_import_rejects_duplicate_takeoff_uid_before_remapping; tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_import_rejects_duplicate_page_uid_before_backend_remapping (+13 more, see divergences.json)
- note: The relationship table (BID_RELATIONSHIPS) is plain data and should be ported verbatim, together with RAW_BID_RELATIONSHIPS filtering (the import-reconciled Bids employee/job-status columns are excluded). Port BID_RELATIONSHIPS (about 144 entries) verbatim.

#### 74. BRW-01: Trash and permanent delete run immediately with no confirmation
- owner: project-browser | impact: data-safety | disposition: fix | testable: objc-only | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/handlers/project_write_handler.py:633-801 (_delete_bids)`: confirm() is called before any write. Permanent delete (bid already in Deleted Bids): title 'Delete Bids', message 'Permanently delete this bid and all its data?\nThis cannot be undone.' (n>1: 'Permanently delete {n} bids and all their data?\nThis cannot be undone.'). ...
- iOS: `Controllers/DatabaseViewController.mm:279-285,345-354,442-453`: Swipe-to-delete (commitEditingStyle) and the context-menu Delete call _deleteBid immediately, with no UIAlertController. In the Deleted Bids section this calls permanentlyDeleteProjectWithUID, which cascades through ~50 tables. Only folder deletion has a confirm alert.
- Windows tests: tests/presentation/handlers/test_project_write_handler.py::test_trashing_one_bid_confirms_and_moves_it_with_its_original_project; tests/presentation/handlers/test_project_write_handler.py::test_permanent_delete_confirms_with_the_bid_count_and_writes_per_database; tests/presentation/handlers/test_project_write_handler.py::test_permanent_delete_of_one_bid_uses_the_singular_text (+2 more, see divergences.json)

#### 75. BRW-02: Deleted Bids system folder is identified by NAME on iOS (UID '1' on Windows): renamable, name-merged, name-reserved
- owner: project-browser | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/entities/project_constants.py:1-8; infrastructure/mdb/components/project_operations.py:134-155,170-174; ...`: The system project is the row with UID '1', whatever its name. The tree disables Rename for UID 1, and the writer refuses to delete UID 1. Any other project may be created or renamed to 'Deleted Bids' (duplicate names are allowed). Nothing is merged or deleted by name.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp:83-92,122-123; ProjectStore.cpp:28-108,522; Controllers/DatabaseViewController.mm:272-277,549-590`: (a) Long-press -> Rename is offered on the Deleted Bids folder and rename_folder(uid '1', 'Foo') returns true. After the next create_schema (it runs on every loadHierarchy) a NEW 'Deleted Bids' row (UID 2) is created. The old trash folder, with its bids, becomes an ordinary folder 'Foo', so those bids are no longer trash. ...
- Windows tests: tests/infrastructure/mdb/components/test_project_operations.py::DeletedBidsProjectGuardTests; tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_full_hierarchy_maps_every_bid_field_and_orders_bids_and_projects
- note: Host-run on a build of the iOS persistence code. Windows has no writer-level rename guard; the Windows rule is UI-only (_can_rename_context), so iOS must enforce it in its own UI or store.

#### 76. DBP-14: delete_takeoff: no cleanup of 'To' annotation links or self references, and it deletes hole children instead of detaching them
- owner: database-persistence | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: UND-05
- Windows: `ost_visualizer/infrastructure/mdb/components/takeoff_operations.py:334-419 (_run_delete_takeoffs)`: All targets must belong to one bid. Deletes BidDimensions/BidALines/BidArrows rows whose BidTakeoffFromUID OR BidTakeoffToUID is in the set, BidPercents by BidTakeoffUID, then sets ParentUID, TypGroupTakeoffUID, TypPageTakeoffUID and TypGroupMarkerUID to NULL on surviving rows that reference a deleted takeoff, then deletes the takeoffs. ...
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Takeoffs.cpp:100-192`: If the takeoff is a root, ALL rows with ParentUID=uid are deleted too; annotation cleanup is only by BidTakeoffFromUID; TypGroupTakeoffUID/TypPageTakeoffUID/TypGroupMarkerUID are never cleared; returns true for unknown uids and for any bid. Differential (diff_delete.py): iOS keeps BidDimensions/BidALines/BidArrows rows linked only via BidTakeoffToUID; ...
- Windows tests: tests/infrastructure/mdb/components/test_takeoff_operations.py:183 (test_takeoff_delete_clears_all_surviving_self_references); tests/infrastructure/mdb/components/test_takeoff_operations.py:215 (test_delete_parent_takeoff_clears_child_parent_uid); tests/infrastructure/mdb/components/test_takeoff_operations.py:249 (test_delete_takeoff_removes_annotations_linked_by_either_endpoint) (+2 more, see divergences.json)
- note: Net effect for UI-driven deletes is the same; the writer contract differs and leaves dangling annotation links. UID allocation half is DBP-18.

#### 77. DBP-17: No existence/ownership validation in writers; unknown targets and wrong bids report success
- owner: database-persistence | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: UND-04, LP-M03
- Windows: `ost_visualizer/infrastructure/database/bid_owned_identity.py:127-235 (require_*), used by every writer, e.g. ...`: Targets must exist and belong to one authoritative bid; takeoff inserts must reference a condition, page, area and parent of that bid with the parent on the same page; failures return False/raise and write nothing.
- iOS: `ProjectStore+Takeoffs.cpp:7-212 (bid_uid parameter ignored), ProjectStore+Folders.cpp:114-134,172-206, ProjectStore+Conditions.cpp:170-261`: Host run (takeoff mode): set_layer_visibility('20','1',false) changed layer 1 (a template layer of no bid) and returned true; set_takeoff_negative/delete_takeoff for nonexistent uids return true; insert_takeoff with condition 777 / page 888 stored a row (uid 51); assign_bid_to_folder to folder 77 or to the Deleted Bids folder succeeds; ...
- Windows tests: tests/integration/mdb/test_ownership_and_atomicity.py:51,448,475,533; tests/infrastructure/database/test_bid_owned_identity.py; tests/infrastructure/mdb/components/test_project_operations.py:124,140,165 (+3 more, see divergences.json)
- note: Windows treats rename of an absent project as a silent success (pinned), so do not tighten that one. Windows does not check set_layer_visibility against the current bid; do not add stricter scoping than Windows. Renaming an absent folder stays a silent success (pinned).

#### 78. DBP-18: UID allocation ignores referencing columns (create_folder, insert_takeoff, duplicate_condition)
- owner: database-persistence | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: BRW-03, COND-DUP-02
- Windows: `ost_visualizer/infrastructure/mdb/components/identity_allocation.py:4-47 (_next_uid_preserving_references), used by project_operations.py:119, takeoff_operations.py:504, ...`: next = MAX(UID)+1, raised above the MAX of every child column that references the table (BID_RELATIONSHIPS), so a new row never adopts a dangling reference. Probe (win_alloc.py): BidTakeoffs max 5 with BidPercents.BidTakeoffUID=9 gives 10; BidConditions max 1 with BidLaborActivity.BidConditionUID=12 gives 13; create_project with Bids.BidProjectUID=8 gives 9.
- iOS: `ProjectStore+Folders.cpp:94-101, ProjectStore+Takeoffs.cpp:55-69, ProjectStore+Conditions.cpp:114-118`: MAX(UID)+1 of the target table only: 6, 2 and 8 respectively, so a new folder 'claims' orphan bids (BidProjectUID=8), a new takeoff or condition inherits dangling BidPercents/BidLaborActivity references.
- Windows tests: tests/infrastructure/mdb/components/test_project_operations.py:70; tests/infrastructure/mdb/components/test_condition_operations.py:231; tests/infrastructure/mdb/components/test_identity_allocation.py (+4 more, see divergences.json)
- note: Needs the BID_RELATIONSHIPS list from ost_visualizer/infrastructure/mdb/raw_bid_integrity.py. One shared allocator (MAX(UID) raised above MAX of every referencing column) applied at three call sites: create_folder (Folders.cpp), insert_takeoff (Takeoffs.cpp), duplicate_condition (Conditions.cpp). ...

#### 79. DBP-20: Page-folder tree builder loses nested folders (objects moved out of an unordered_map)
- owner: database-persistence | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: BRW-14, LP-09
- Windows: `ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:180-206,265 (+ remove_empty_folders)`: Folders are linked by reference so any depth works; folders with a missing or cross-bid parent become roots; empty branches are pruned; a parent cycle raises.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:296-344`: for each pair: all_folders[parent].subfolders[child] = std::move(pair.second) while iterating the same unordered_map. A child moved before its own children are attached leaves the moved-from object in all_folders, so grandchildren attach to a dead copy. ...
- Windows tests: tests/infrastructure/mdb/components/test_hierarchy_reader.py:337 (test_page_hierarchy_accepts_valid_multi_level_folder_chain); tests/infrastructure/mdb/components/test_hierarchy_reader.py:644 (test_folders_nest_by_parent_drop_empty_branches_and_keep_name_order); tests/infrastructure/mdb/components/test_hierarchy_reader.py:250,281,311 (+7 more, see divergences.json)
- note: Cycles: Windows raises; iOS silently drops all folders in the cycle (decision belongs with DBP-31). Result of std::unordered_map iteration order differs per STL (MSVC insertion order vs libc++ hash order): test with several UID layouts. Fix BRW-15, BRW-16, BRW-17 in the same function.

#### 80. GEO-09: polygon_is_valid accepts degenerate and self-touching polygons (strict-crossing only)
- owner: geometry | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: RPV-14
- Windows: `cpp_extensions/src/geom_utils/geom_utils.cpp:45-90`: Returns false if n<3; any non-finite coordinate; a vertex equal to the next one; a zero-turn 180-degree spike (cross==0 and dot<=0 at a vertex); zero or non-finite total area; any proper crossing; or any vertex lying exactly on a non-adjacent edge (T-junction / touching). Collinear edge-vertices, slivers and concave shapes are valid in either orientation. ...
- iOS: `Core/Geometry/geom_utils.cpp:23-43; callers DragController.mm:104, PlanView+Placement.mm:306`: Checks only n<3 and proper edge crossings. In the harness all 6 Windows-invalid cases return TRUE: collinear 3-point (0,0),(1,0),(2,0); duplicate vertex (0,0),(2,0),(2,0),(0,2); self-touch (0,0),(4,0),(4,4),(2,0),(0,4); self-touch (0,0),(4,0),(2,0),(2,4),(0,4); NaN vertex; Inf vertex. ...
- Windows tests: tests/integration/native/test_geometry_boundaries.py::NativeGeometryBoundaryTests::test_polygon_rejects_degenerate_and_self_touching_boundaries; tests/integration/native/test_geometry_boundaries.py::NativeGeometryBoundaryTests::test_polygon_preserves_small_slivers_concavity_and_collinear_edge_vertices; tests/integration/native/test_geometry_boundaries.py::test_polygon_rejects_degenerate_and_self_touching_boundaries (+1 more, see divergences.json)
- note: The Windows drag path evaluates validity on 2D scene coordinates (x*72*view_scale/ratio) and the placement path on raw OST points. iOS evaluates both on raw OST. The exact-zero collinearity tests are scale-sensitive only in rare rounding cases. ...

#### 81. UND-02: Undo/redo advances history even when the persistence write fails
- owner: settings-undo-camera | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/services/undo_redo_service.py:469-524 (_submit_history_transition: the entry moves stacks only on COMMITTED), :197-231 (push_local: action Fal ...`: If the undo or redo action returns False or raises, the entry stays on its source stack and is retried by the next undo or redo. A failed forward persistence pushes no history entry.
- iOS: `Controllers/ViewController/Components/UndoRedoController.mm:47-93; Controllers/ViewController/ViewController.mm:739-776 (_applyPosition, _removeTakeoffFromProject and _ad ...`: performUndo and performRedo always pop and push the opposite stack and call undoRedoDidModify. In-memory state and history advance although SQLite saveTakeoffPosition, deleteTakeoff or insertTakeoff may have failed. An insert failure leaves uid empty but still appends the takeoff to the project.
- Windows tests: tests/presentation/services/test_undo_redo_service.py:86-125,584-638
- note: Needs the delegate protocol to return success; the persistence helpers already return BOOL or nil.

#### 82. UND-03: Deleting a condition does not invalidate undo/redo history
- owner: settings-undo-camera | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/application/services/project_write_service.py:681-692 (delete_conditions publishes invalidates_undo=True); ...`: Condition delete clears the whole undo service. Condition create and update publish invalidates_undo=False and keep history.
- iOS: `Controllers/ViewController/Components/ViewController+Conditions.mm:189-223; Controllers/ViewController/ViewController.mm:1044 (the only clear())`: didRequestDeleteConditionUID removes the condition and its takeoffs but never touches _undoRedoController. Undo of an earlier Delete entry, or redo of a Place entry, re-inserts BidTakeoffs rows pointing at the deleted condition (the DDL has no REFERENCES), leaving orphan rows. ...
- Windows tests: tests/application/services/test_project_write_service.py:5583,6083 (delete publishes invalidates_undo True); tests/application/services/test_project_write_service.py:1062,5613 (create or update keeps False)
- note: The iOS alert already says 'This cannot be undone', but the stale history can still re-insert rows.

#### 83. COND-UPD-02: Update rewrites every column instead of only the changed ones; also writes ColorLine
- owner: conditions | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DLG-SAVE-01, COND-M06
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1406-1699,1714-1722; condition_operations.py:625-700; ...`: The dialog builds an UpdateConditionDto containing only fields whose value differs from the stored condition; an unchanged form writes nothing (not dirty -> no save; empty DTO -> success without a write). Unknown fields (is_top, z_value) are never persisted. ColorLine, Connect, ConnectTolerance and SnapToLinear are never written.
- iOS: `ProjectStore+Conditions.cpp:7-81; EditConditionViewController.mm:300-312; EditConditionViewController+Pickers.mm:205-217`: update_condition does a full-row UPDATE of ~43 columns from the in-memory struct on every Save, even when nothing changed (Save is always enabled), so NULL numeric columns become 0, DisplaySize 0 becomes 100, Backout 1 becomes -1, and the colour picker's Done also sets ColorLine = ColorFill and marks the form dirty even if the colour was untouched.
- Windows tests: tests/infrastructure/mdb/components/test_condition_operations.py::ConditionUpdateContractTests::test_every_updatable_field_writes_its_own_column; tests/infrastructure/mdb/components/test_condition_operations.py::ConditionUpdateContractTests::test_an_empty_change_set_succeeds_without_writing; tests/infrastructure/mdb/components/test_condition_operations.py::ConditionOperationsPersistenceTests::test_condition_update_without_persistable_changes_is_a_noop_success (+2 more, see divergences.json)
- note: Requires an update API that takes a field mask or a before/after diff. Needs a field-mask or before/after diff API and a real dirty flag (form state != saved state).

#### 84. COND-UPD-01: Cleared layer / type / folder / notes are written as '' instead of NULL
- owner: conditions | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-16
- Windows: `ost_visualizer/infrastructure/mdb/components/condition_operations.py:644-652`: CdnTypeUID, BidLayerUID, BidConditionFolderUID are written int(value) or NULL when empty/None; Notes are written encode_text_blob(text) or NULL when empty; Backout is -1 / 0.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:51,73-76`: Always bind_text, so an empty layer_uid / cdn_type_uid / condition_folder_uid / notes is stored as the empty string '' (not NULL), e.g. choosing '(none)' for Type writes CdnTypeUID=''.
- Windows tests: tests/infrastructure/mdb/components/test_condition_operations.py::ConditionOperationsPersistenceTests::test_condition_update_persists_mapped_fields_and_clears_empty_relationships; tests/infrastructure/mdb/components/test_condition_operations.py::ConditionUpdateContractTests::test_clearing_a_reference_or_the_notes_writes_null_and_backout_false_writes_zero; tests/infrastructure/mdb/components/test_condition_operations.py:620 (test_condition_update_ref_no_shifts_only_conflicting_peers) (+1 more, see divergences.json)
- note: Notes storage form (blob vs SQLite text) is a platform storage detail; the NULL-vs-empty rule is the parity requirement.

#### 85. COND-UPD-03: Save failures are not reported; in-memory state is updated regardless
- owner: conditions | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1764-1771; application/use_cases/project/update_condition_use_case.py:36-42`: A failed write returns 'Failed to save condition to database.', the dialog shows a 'Save Failed' warning and stays open with the form dirty; the condition list is rebuilt from the DB.
- iOS: `ProjectStore+Conditions.cpp:79-80; ViewController+Conditions.mm:140-144; EditConditionViewController.mm:306-311`: update_condition returns true after prepare succeeds regardless of the step result; the delegate stores the condition in memory first and ignores [ImportService updateCondition:]'s result, then the dialog dismisses. A failed write (locked DB, trigger, constraint) shows the new values until the next reload.
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py::test_edit_condition_ok_click_with_failed_save_stays_open_and_warns
- note: Same pattern for shift_condition_ref_nos (returns true after step) and duplicate failure (silent nil return).

#### 86. COND-DUP-01: Duplicated condition keeps the source GUID
- owner: conditions | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-13
- Windows: `ost_visualizer/infrastructure/mdb/components/condition_operations.py:30-32,102-127`: The copy gets a fresh GUID '{' + uuid4().upper() + '}' (regex ^\{[0-9A-F]{8}(-[0-9A-F]{4}){3}-[0-9A-F]{12}\}$) and RefNo = MAX(RefNo in bid)+1 (+index for batches); every other column is copied verbatim; the source row is required to belong to the bid or nothing is inserted.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:106-168`: CREATE TEMP TABLE ... SELECT * then UPDATE only UID and RefNo, so the copy has the same GUID as its source; source row is selected by UID only (not scoped to the bid).
- Windows tests: tests/infrastructure/mdb/components/test_condition_operations.py::ConditionDuplicateContractTests::test_duplicates_get_fresh_identities_and_keep_every_other_value; tests/infrastructure/mdb/components/test_condition_operations.py::ConditionDuplicateContractTests::test_a_null_binary_value_is_copied_as_null; tests/infrastructure/mdb/components/test_condition_operations.py::ConditionDuplicateContractTests::test_unknown_or_foreign_conditions_block_the_duplicate (+2 more, see divergences.json)
- note: The GUID generator is a platform detail (NSUUID passed in, or a Core RNG); the format is what must match. Also require the source condition to belong to the bid.

#### 87. RPV-15: Child backouts and attachments do not follow when an area body is dragged
- owner: rendering-planview | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: GEO-M01, UND-M01
- Windows: `plan_view/components/input_handler.py:1480-1510 (release), 1787-1835`: When an area is moved with the body handle (index -1), every descendant takeoff of that area (expanded via _expanded_takeoff_transform_uids) is translated by the parent's first-point displacement and committed with it, so holes stay inside their parent. A corner or edge drag does not move children (they are validated instead, drag_handler.py:1012-1035).
- iOS: `Controllers/ViewController/ViewController.mm:924-940, 971-976; DragController.mm:113-119`: Only the dragged takeoff's position is changed. Child takeoffs (holes, attachments) stay behind, ending outside their parent.
- Windows tests: test_input_handler.py::test_child_holes_only_follow_a_body_drag; ::test_area_release_keeps_the_last_valid_polygon_and_moves_child_holes; ::test_child_takeoffs_do_not_follow_the_body_drag_of_a_non_area_parent (+6 more, see divergences.json)
- note: Windows additionally validates hole containment on corner drags (windowsOnly). Parent and children must be committed and pushed as ONE undo entry (UND-07).

#### 88. RPV-16: 2D selection is not cleared on page switch, so Delete acts on an off-page takeoff
- owner: layers-pages | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `plan_view/view.py:5040-5075 (load_page calls clear(preserve_deferred_selection=not project_changed and not page_changed))`: Changing the page drops the selection; page_selection_reset is emitted once the new page is current.
- iOS: `Controllers/ViewController/Components/ViewController+PageSelector.mm:297-316; ViewController.mm:388-420, 786-793`: _2DButtonTapped reloads the plan view but leaves _selectedTakeoffUID, _selectedConditionUID and _activeHandleIndex set; _handleDeleteTakeoff then deletes the previous page's takeoff (and its children) invisibly. PlanView._lastTappedUID also stays set.
- Windows tests: tests/presentation/components/plan_view/test_view.py::test_known_sequence_page_switch_drops_stale_selection; ::test_page_switch_reports_the_cleared_selection_once_the_new_page_is_current
- note: An active placement session survives a page switch on Windows (preserve_place) and on iOS; that part matches.

#### 89. RPV-M01: Area edge-handle drag is not validated (self-touching or winding-reversed polygons are committed)
- owner: rendering-planview | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/presentation/components/plan_view/components/drag_handler.py:89-99, 481-506; input_handler.py:1434-1439`: For any area vertex drag (handle_index >= 0, which includes edge handles) the proposed position must pass polygon_is_valid and must not reverse winding versus the last valid position. Otherwise the previous valid geometry is kept, and on release the last valid position is what gets committed.
- iOS: `Controllers/ViewController/Components/DragController.mm:53-69 (edge branch), 91-107 (validation only in the corner branch)`: The edge branch writes both corners with no polygon_is_valid or winding check, so an edge dragged across the opposite edge is persisted as an inverted polygon. Only corner drags are validated.
- Windows tests: tests/presentation/components/plan_view/components/test_drag_handler.py::test_area_invalid_resize_keeps_previous_valid_geometry; tests/presentation/components/plan_view/components/test_drag_handler.py::test_area_resize_that_reverses_winding_keeps_previous_valid_geometry; tests/presentation/components/plan_view/components/test_drag_handler.py::test_polygon_edit_requires_three_valid_points_and_stable_winding
- note: Ran Windows _polygon_edit_geometry_valid with a harness. Shares the validity function with RPV-14; once polygon_is_valid is ported, the edge branch just needs the same check the corner branch already has.

#### 90. IMP-12: OSP image staging overwrites/collides: basename-only destination, no content check
- owner: import-ost-osp | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/importers/osp_importer.py:310-347,400-446`: Each referenced packaged image goes to <working>/<ost stem>/<name> if its member is directly under TempImages!.tmp, otherwise <ost stem>/Images/<sha1(member.casefold())[:16]>/<name>. If the destination exists with different content the file is written to <stem>-<sha256[:16|32|full]><suffix>; identical content is reused; ...
- iOS: `Services/ImportService.mm:172-218`: Every .pdf entry anywhere in the CAB is written to Documents/OST/<ost stem>/<basename> with atomically:YES, overwriting any existing file. Two members with the same basename in different folders overwrite each other (and both references are then rewritten to the same path). ...
- Windows tests: tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_does_not_overwrite_different_existing_image; tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_reuses_identical_existing_image; tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_resolves_most_specific_nested_member_without_overwrite (+3 more, see divergences.json)
- note: The iOS root (Documents/OST/<stem>) is the platform-adapted equivalent of get_default_working_dir()/<stem>; only the collision and staging logic is the divergence.

#### 91. IMP-13: CAB reader silently skips corrupt blocks, ignores checksums and treats PDF extraction failure as success
- owner: import-ost-osp | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `cpp_extensions/src/cab/cab_compressor.cpp:380-423,495-520; osp_importer.py:243-247`: FDICopy extracts through the Windows cabinet library. Any decode or corruption error makes extract_cab return false, the import fails with 'the OSP archive could not be extracted', and nothing is imported. The legacy test fixtures write CFDATA checksum 0 (meaning none).
- iOS: `Core/Import/CabExtractor.cpp:176-258 (blocks with missing 'CK' or an inflate error are skipped with continue; csum at offset 0 never read); ...`: A bad MSZIP block is dropped without error, so every later byte in that folder shifts. extract_cab_file only fails if the resulting buffer is shorter than the file's end; otherwise it returns shifted, wrong bytes. There is no check that produced bytes equal cbUncomp. CFDATA checksums are not verified. Stored-block truncation just breaks the loop. ...
- Windows tests: tests/integration/native/test_osp_cab.py; tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_extraction_failure_fails_import_and_cleans_temp_files; tests/infrastructure/mdb/importers/test_osp_importer.py::test_extraction_failure_message_names_the_cause_and_archive
- note: Windows FDI behavior on bad checksums is the documented library behavior (I could not run FDI here). The 'fail on corruption' rule is clear from the Windows tests (extract_cab false means the import fails). A host test needs a zlib on the build machine.

#### 92. IMP-10: CdnTypes/JobStatuses reconciliation differs (key normalisation, ambiguity errors, UID allocation, unmapped refs)
- owner: import-ost-osp | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-25
- Windows: `ost_visualizer/infrastructure/mdb/components/import_operations.py:150-230; ost_visualizer/infrastructure/database/master_data_identity.py:64-69,98-125; ...`: Identity key for CdnTypes.Name is text.strip().lower(); JobStatuses.Name is exact text. Incoming rows that collapse to the same key raise AmbiguousMasterDataIdentityError, and so do existing DB rows that share a key (the import is rolled back). An existing match maps old UID to the existing UID; otherwise a new UID = running floor+1 is inserted. ...
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:29-92,106-129,148-171`: Key is the exact Name (case- and whitespace-sensitive) for both tables. No ambiguity detection (later existing rows overwrite earlier ones in the lookup; a duplicate incoming name silently reuses the first row's new UID). New UID = MAX(UID of that table)+1 (not a global floor). ...
- Windows tests: tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_uses_condition_type_name_contract; tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rejects_ambiguous_condition_type_name_contract; tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_inserts_new_master_rows_above_the_uid_floor (+7 more, see divergences.json)
- note: Python lower() vs C++ tolower: use a Unicode-aware casefold for the Name key or restrict the test to ASCII and flag the non-ASCII case.

#### 93. IMP-11: Employees, AccessLevels, PayClasses and BidEmployees are not reconciled/imported as on Windows
- owner: import-ost-osp | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-23, XML-01, DBP-M04
- Windows: `ost_visualizer/infrastructure/mdb/components/import_operations.py:232-382; ost_visualizer/infrastructure/mdb/schema_contract.py:10,40-46; ost_importer.py:34-39,319-322`: BidEmployees is a bid section and PayClasses a global section, both parsed. AccessLevels reuse existing rows by Description (blank descriptions never match), PayClasses by Name, Employees by business key (EmployeeNo case-folded, else first|last|email trimmed and lower-cased). Employees get their PayClassUID/AccessLevelUID mapped (unmapped becomes NULL). ...
- iOS: `Core/Persistence/OstSchema.cpp:990-1005,1032-1037; Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:213-242`: BidEmployees (despite existing in the schema) and PayClasses are never parsed or stored. Employees and AccessLevels are inserted with INSERT OR IGNORE under their source UID. A UID collision silently keeps the local row, and the Bid's EstimatorUID (kept raw) then points at an unrelated employee. No PayClassUID/AccessLevelUID remap.
- Windows tests: tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_employees_reuse_business_key_and_remap_references; tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_pay_classes_and_access_levels_reuse_names_and_insert_rest; tests/infrastructure/mdb/components/test_import_operations.py::test_employee_identity_keys_use_number_first_then_trimmed_lowercase_names (+13 more, see divergences.json)
- note: The iOS app has no employee UI; this is persistence parity only, so the data survives edits and a later export. BID_SECTIONS needs BidEmployees between BidSettings and BidLayers; GLOBAL_SECTIONS needs PayClasses; ...

#### 94. DBP-35: Multi-row edits are not atomic: multi-takeoff drag, Show/Hide All layers and RefNo shift + update use one connection and transaction per row
- owner: database-persistence | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: COND-M03
- Windows: `ost_visualizer/infrastructure/mdb/components/takeoff_operations.py:245-270 (save_takeoff_positions in one transaction), layer_operations.py:259-352 (update_all_layers_sho ...`: All rows of one user action commit or roll back together.
- iOS: `Controllers/ViewController/ViewController.mm:945-1000 (one saveTakeoffPosition per takeoff), Controllers/ViewController/Components/ViewController+Layers.mm:83-120 (one se ...`: Every ImportService call opens its own sqlite connection and commits independently, so an interruption in the middle leaves a partial move, partial layer state or a shifted RefNo with no update.
- Windows tests: tests/infrastructure/mdb/components/test_takeoff_operations.py:1792,1833; tests/infrastructure/mdb/components/test_layer_operations.py; tests/infrastructure/mdb/components/test_condition_operations.py:620 (+2 more, see divergences.json)
- note: Depends on DBP-11. Add batch store functions; callers in ViewController.mm and ViewController+Layers.mm/+Conditions.mm then use them.

#### 95. DBP-M02: load_project and load_hierarchy swallow SQL errors and return an empty or partial model; Windows pins that read errors never become an empty family
- owner: database-persistence | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:102-150 (require_column, errors re-raised via _record_caught_read_error), 1079-1200`: A missing required column/table or a failing read raises; tests assert the error propagates and no empty takeoff or condition family is published.
- iOS: `Core/Persistence/OstDatabase.cpp:56-62,136-143; ProjectStore+Load.cpp:15-345; ProjectStore+Folders.cpp:20-74`: prepare failures leave an invalid Statement that every block skips, and step() returns false for both SQLITE_DONE and errors, so a failing or interrupted query yields silently fewer rows. Host repro: after DROP TABLE BidTakeoffs, load_project('10') returns the bid, pages and conditions with zero takeoffs and no error indication; ...
- Windows tests: tests/integration/persistence/test_takeoff_read_contract.py:13 (test_deleted_record_read_error_never_becomes_empty_takeoff_family); tests/integration/persistence/test_scoped_family_read_contract.py:46 (test_condition_folder_query_failure_cannot_publish_incomplete_family)
- note: Related to DBP-11 (write side) and DBP-31. The user sees an apparently empty plan after a transient SQLITE_BUSY or schema drift.

#### 96. DBP-08: parse_position accepts partially numeric tokens and mishandles out-of-range values
- owner: parsers-units | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: POS-01, GEO-M03
- Windows: `ost_visualizer/domain/utils/position.py:6-19`: Strip XML newline entities, split on ';', drop empty tokens, float(token) for each; ANY token that is not entirely a float gives []. '1e400' -> inf.
- iOS: `Core/Persistence/ProjectStore/ProjectStore.cpp:161-204`: std::stod ignores trailing garbage and accepts hex: '1.5abc;2' -> [1.5,2]; '1,5;2' -> [1,2]; '1.5 2.5;3' -> [1.5,3]; '0x10;2' -> [16,2]; '1&#13;;2' -> [1,2]; '1e400;1' -> [] (out_of_range). 13 of 35 probe cases agree with Windows on pinned inputs; the pinned ones (entities, empty tokens, 'bad') all match.
- Windows tests: tests/integration/persistence/test_position_contracts.py (test_position_parsing_preserves_previous_text_edge_cases); tests/infrastructure/mdb/components/test_serialization.py; tests/infrastructure/mdb/components/test_serialization.py::test_position_parser_reads_text_and_binary_storage_values (+4 more, see divergences.json)
- note: Malformed positions would otherwise produce phantom geometry instead of an empty position. Python-only float spellings (inf, nan, underscores, 1e999 -> inf) need not be reproduced except as decided in POS-M01.

#### 97. EXP-08: HTML payload is not escaped: JSON names and the title are inserted raw, and placeholders in user text expand
- owner: export-formats | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `renderers/threejs/threejs_renderer.py:282-296; tests/presentation/visualization/renderers/threejs/test_threejs_renderer.py:98-144,673-691`: Scene JSON comes from json.dumps(separators=(',',':')) with '<' replaced by <; the title and subtitle go through html.escape(quote=False); a single regex pass replaces {{TITLE}}/{{SUBTITLE}}/{{SCENE_DATA}}, so user text containing placeholders is never expanded.
- iOS: `Core/Export/threejs_writer.cpp:96 (name), 34-41 (sequential replace, title first), 177`: The condition name is concatenated into JSON unescaped, and the title is inserted into HTML unescaped. Harness: the name 'Wall "A"\x </script><script>alert(1)</script>_0' produced invalid scene JSON. The title 'A & <b>Bid</b> {{SCENE_DATA}}' got the whole scene JSON expanded inside <title>.
- Windows tests: tests/presentation/visualization/renderers/threejs/test_threejs_renderer.py::test_html_generation_escapes_title_and_script_terminators; tests/presentation/visualization/renderers/threejs/test_threejs_renderer.py::test_html_generation_does_not_expand_template_placeholders_in_user_text; tests/presentation/visualization/renderers/threejs/test_threejs_renderer.py::test_header_shows_escaped_title_and_subtitle
- note: Reachable because condition names and bid names are user text from imported .ost files. Windows keeps scripts parseable via <.

#### 98. EXP-09: Export filename: iOS uses the raw bid name only; Windows builds '<bid> - <pages>.<ext>' with sanitising and a 255-character limit
- owner: export-formats | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `infrastructure/visualization_provider.py:55-89; infrastructure/utils/filename.py:3-6; application/services/export_service.py:50-65,94-99`: sanitize_filename replaces [\x00-\x1f\\/:"*?<>|] with '_'. 1 page gives '<bid> - <page>.<ext>'; 2 pages '<bid> - A + B.<ext>'; 3 or more '<bid> - A + B + N more.<ext>' (N = count-2). Length is capped at 255 by truncating the bid to '...' form, else the page, else 'Export_<N>_pages.<ext>'. ...
- iOS: `Services/ExportService.mm:94-98,117-120,159-161,188-190`: The file name is bid_name + extension, with no sanitising, no page names and no length limit. A bid name containing '/' becomes a sub-path that does not exist, so the write fails and the user sees 'No visible meshes to export' (see EXP-21). An empty bid name gives 'export'.
- Windows tests: tests/infrastructure/test_visualization_provider.py::ExportFilenameTests::test_suggested_export_filename_replaces_windows_control_characters; tests/infrastructure/test_visualization_provider.py::ExportFilenameCompatibilityTests::test_export_filename_consolidation_preserves_html_and_mesh_policy; tests/infrastructure/test_visualization_provider.py::ExportFilenameCompatibilityTests::test_export_filename_consolidation_preserves_long_name_fallbacks (+1 more, see divergences.json)
- note: The same base name drives the .mtl pairing; keep the OBJ/MTL stem identical.

#### 99. BRW-05: delete_folder does not clear OrigBidProjectUID references to the deleted folder
- owner: project-browser | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-39
- Windows: `ost_visualizer/infrastructure/mdb/components/project_operations.py:183-200`: Deleting projects sets Bids.BidProjectUID=NULL and Bids.OrigBidProjectUID=NULL wherever they equal a deleted project, then deletes the project rows. A trashed bid whose original project was deleted therefore has no restore target.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp:146-168`: Only UPDATE Bids SET BidProjectUID=NULL WHERE BidProjectUID=?; OrigBidProjectUID is untouched. Host-run: bid 41 (trashed from folder 6) keeps OrigBidProjectUID=6 after delete_folder('6'). Combined with BRW-03, a later new folder with UID 6 would be treated as that bid's original project.
- Windows tests: tests/infrastructure/mdb/components/test_project_operations.py::test_delete_project_clears_deleted_bid_restore_reference; tests/infrastructure/mdb/components/test_project_operations.py::test_deleting_projects_detaches_their_bids_and_restore_references; tests/infrastructure/mdb/components/test_project_operations.py::test_deleting_several_projects_detaches_and_removes_every_one_of_them (+3 more, see divergences.json)
- note: Prerequisite for BRW-09.

#### 100. COND-STYLE-01: Style change wipes notes, pattern, shared options and every unsaved text edit
- owner: conditions | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: DLG-M01
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1197-1303`: Changing style keeps name, Cond. No., elevation, layer, type, notes, colour, pattern, spacing text and the typed spacing; display_name is carried among Count/Attachment/Area and round_quantity+round_to among Linear/Area (_shared_options); pattern is set to Transparent only on the very first population (previous_type is None, i.e. never on a style change); ...
- iOS: `EditConditionViewController.mm:346-424; EditConditionViewController+Pickers.mm:49-76`: _applyStyleDefaults sets pattern = Transparent, spacing = 4.0, notes.clear(), resets display_name/round_quantity/round_up/trim/etc. for the new type, and then _syncFieldCacheFromCondition rebuilds every text cache from the stored condition, discarding unsaved edits to name, Cond. No., elevation value and notes.
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py::test_shared_fields_carry_over_and_type_specific_fields_get_defaults; tests/presentation/dialogs/test_edit_condition_dialog.py::test_rounding_carries_over_between_linear_and_area_only; tests/presentation/dialogs/test_edit_condition_dialog.py::test_notes_survive_count_to_linear_and_back
- note: Windows carry rules: display_name carried when both types are in {Count, Attachment, Area}; round_quantity/round_to carried when both types are in {Linear, Area}.

#### 101. COND-REF-01: Condition number is coerced instead of validated
- owner: conditions | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: DLG-REF-01
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1449-1469`: int(text.strip()) must succeed and be >= 1, otherwise the save is blocked with 'Condition number must be a positive whole number.' and focus returns to the field. ' 8 ' is accepted as 8.
- iOS: `EditConditionViewController.mm:580-583`: refText.intValue: '', 'abc', '0', '-1', '1.5' and '12abc' become 1, 1, 1, 1, 1 and 12; values < 1 are silently set to 1 (which then shifts other conditions' numbers if 1 is taken).
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py::test_edit_condition_rejects_invalid_condition_numbers; tests/presentation/dialogs/test_edit_condition_dialog.py::test_edit_condition_accepts_valid_changed_condition_number
- note: The field uses a number pad, but empty and 0 are still reachable.

#### 102. COND-ELEV-01: Elevation text is reformatted when the field loses focus
- owner: conditions | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: DLG-ELEV-01
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:161-175,382,1415-1431`: The elevation field is a plain QLineEdit (only _DimensionLineEdit fields normalize on focus-out), so the typed text is kept verbatim and becomes the name suffix: base + ' @T ' / ' @B ' + typed text. z_value is parsed from it with display_to_inches.
- iOS: `EditConditionViewController.mm:805-827 (kTagElevVal is in the isDimensionField set)`: On blur a parseable elevation is rewritten to the canonical eighths format, e.g. "10'6" becomes "10' 6\"" and "10' 6.3\"" becomes "10' 6 1/4\"", so the saved name suffix and z differ from Windows.
- Windows tests: tests/domain/services/test_elevation.py; tests/presentation/dialogs/test_edit_condition_dialog.py::test_edit_condition_rejects_non_finite_numeric_values

#### 103. UND-06: Undo of a delete restores a reduced column set; Windows restores the saved extras
- owner: settings-undo-camera | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/handlers/plan_view_action_handler.py:3801-3836 (saved_takeoff_extras captured via get_takeoff_extras into InsertTakeoffSpec.raw_extras), infra ...`: On undo the row is re-inserted with the typed columns plus every other BidTakeoffs column captured before the delete, except bid-scoped FKs on cross-bid restores. GUID is regenerated.
- iOS: `Controllers/ViewController/Components/UndoRedoController.mm:95-131 (entry keeps ost_domain::Takeoff); Core/Domain/Takeoff.hpp; ProjectStore+Takeoffs.cpp:71-96`: The Takeoff model holds only uid, condition, page, area, position, rotation, curve, parent and negative flag. The restored row has only those 10 columns, so Quantity, Count, No, GridOffsetX, GridOffsetY, GridRotation, Font* and GUID are lost (NULL) after delete then undo.
- Windows tests: tests/presentation/handlers/test_plan_view_action_handler.py:3811,3849
- note: iOS keeps the original uid on restore, while Windows allocates a new uid and rebinds history. That uid-retention difference is acceptable because ordering is LIFO.

#### 104. UND-08: Delete cascades only direct children of a root takeoff; Windows deletes all transitive descendants
- owner: settings-undo-camera | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: RPV-M02
- Windows: `ost_visualizer/domain/services/takeoff_domain_service.py:6-19 (expand_takeoff_uids_with_descendants); presentation/handlers/plan_view_action_handler.py:3769-3779`: Deleting a takeoff deletes the set of all descendants through parent_uid links, at any depth and regardless of whether the selected takeoff is a root. History restores the whole set.
- iOS: `Controllers/ViewController/Components/UndoRedoController.mm:95-131; Core/Persistence/ProjectStore/Components/ProjectStore+Takeoffs.cpp:110-137`: deleteSelection collects the selected takeoff and, only if it is a root (parent empty or '0'), its direct children (parent_uid == selected). Deleting a non-root with its own children, or any grandchild level, leaves descendants orphaned. The DB delete cascade has the same one-level, root-only rule.
- Windows tests: tests/domain/services/test_takeoff_domain_service.py:159; tests/infrastructure/mdb/components/test_takeoff_operations.py:215; tests/domain/services/test_takeoff_domain_service.py::TakeoffDescendantExpansionTests::test_selection_expands_to_every_descendant_but_not_siblings_or_ancestors
- note: Windows DB delete nulls child ParentUID rather than cascading. The app layer expands the set, which iOS can mirror by computing the descendant set in Core. DB level (DBP-14) detaches children (ParentUID NULL); the app layer expands the delete set to all descendants. ...

#### 105. COL-PICK-01: Colour picker also overwrites ColorLine
- owner: conditions | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `edit_condition_dialog.py:326-329 and 1546-1548 (only color_fill is set); infrastructure/mdb/components/condition_operations.py:566`: Changing the colour changes ColorFill only; ColorLine is untouched (and read by nothing else except the OST exporter).
- iOS: `Controllers/EditConditionViewController/Components/EditConditionViewController+Pickers.mm:205-217 (line 214: _condition.color_line = _condition.color_fill)`: ColorLine is overwritten with the new fill colour and persisted by update_condition (ProjectStore+Conditions.cpp:39). Also converts UIColor floats with (int)(r*255.0) truncation (unverified on device).
- Windows tests: none found
- note: Rounding vs truncation for arbitrary UIColor floats is unverified without a device; exact k/255 inputs round-trip in both.

#### 106. COND-M05: update_condition is not scoped to the bid and does no layer or folder ownership check
- owner: conditions | impact: data-safety | disposition: fix | testable: core-cpp | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/infrastructure/mdb/components/condition_operations.py:655-692`: Requires the condition to belong to the bid, requires any new BidLayerUID/BidConditionFolderUID to belong to the same bid, and updates with WHERE [UID]=? AND [BidUID]=?; otherwise raises and returns False.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:7-81 (WHERE UID = ?); Services/ImportService.mm:478-480`: The signature has no bid argument and the UPDATE is keyed on UID only, so any UID is rewritten regardless of its bid and any layer/folder uid is accepted. Probe: update_condition on a condition of bid 2 returned true and renamed it.
- Windows tests: tests/infrastructure/mdb/components/test_condition_operations.py::ConditionUpdateContractTests::test_updating_a_condition_of_another_bid_or_into_another_bids_layer_is_refused; tests/infrastructure/mdb/components/test_condition_operations.py::ConditionOperationsPersistenceTests::test_condition_update_rejects_cross_bid_relationships_atomically
- note: UIDs are globally unique so the UI cannot normally hit this; it is a defensive rule Windows tests pin.

#### 107. IMP-M02: OSP image directory/file write failures are ignored on iOS; Windows aborts the whole import
- owner: import-ost-osp | impact: data-safety | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/infrastructure/mdb/importers/osp_importer.py:310-347 (mkdir/shutil.copy2 raise), 196-219 (import_osp returns False on any exception)`: Any failure while staging a packaged image (disk full, permissions) raises, the OST is not rewritten, nothing is written to the database and the import reports failure. Pinned by test_a_copy_failure_does_not_rewrite_the_embedded_ost (copy2 raising 'disk full' propagates and the OST text is unchanged).
- iOS: `Services/ImportService.mm:187-218`: createDirectoryAtPath is called with error:nil and its result ignored; [NSData writeToFile:atomically:] return value is ignored; an extraction that returns empty is skipped (ImportService.mm:204). The import proceeds, rewrites every ImagePath to the non-existent target and returns YES, so a full disk gives a project whose drawings are missing but is reported ...
- Windows tests: tests/infrastructure/mdb/importers/test_osp_importer.py::test_a_copy_failure_does_not_rewrite_the_embedded_ost; tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_extraction_failure_fails_import_and_cleans_temp_files
- note: Related to IMP-12/IMP-13 but a distinct trigger (I/O failure rather than corrupt CAB); the fix is to check and propagate each write result.

#### 108. GEO-M02: No hole and attachment containment validation when editing a parent area
- owner: rendering-planview | impact: data-safety | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/presentation/components/plan_view/components/drag_handler.py:485-492 (area vertex drag), 907-1035 (_attachment_position_valid, _takeoff_children_valid_for_ ...`: A parent area edit (vertex or edge drag, whole-shape or group move, rotate) is rejected when any hole child would leave the parent, a hole would intersect a sibling hole, or an attachment would not fit the area minus holes. The previous valid geometry is kept.
- iOS: `Controllers/ViewController/Components/DragController.mm:91-111 (only polygon_is_valid plus winding guard)`: Dragging a parent area vertex or edge accepts any simple polygon, even if it cuts through existing holes or leaves attachments outside. No containment or sibling-overlap check exists.
- Windows tests: tests/presentation/components/plan_view/components/test_geometry_utils.py; tests/integration/geometry/test_attachment_validation.py
- note: The mapper listed the Windows geometry_utils helpers only under windowsOnly. They are an edit-time validation rule that affects persisted geometry, so it belongs in the divergence list. Porting adds new Core code; the user decides, since 'no new features' applies. ...

#### 109. POS-M01: Non-finite position coordinates (inf/nan) are accepted on iOS; Windows rejects them and aborts the load
- owner: parsers-units | impact: data-safety | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/domain/entities/takeoff.py:84-96 (has_valid_contract uses _is_finite_number on every coordinate); ...`: parse_position('nan;2;3;4') returns [nan,2,3,4] because float() accepts it, but the Takeoff contract check then raises ValueError, so the whole bid load fails with the offending UID. Windows test: test_access_reader_rejects_duplicate_and_contract_violating_takeoffs (test_bid_data_reader.py:1171-1180) pins this for b'nan;2;3;4\n'.
- iOS: `Core/Persistence/ProjectStore/ProjectStore.cpp:195 (std::stod accepts inf/nan); ProjectStore+Load.cpp:256-267`: 'inf;1' parses to [inf,1] and 'nan;1' to [nan,1] (verified with the compiled iOS parse_position). The takeoff is kept when size >= 2, so NaN or inf coordinates flow into area, length and mesh geometry.
- Windows tests: none found
- note: Windows aborts the whole bid load. iOS needs a decision on whether to fail the load, drop the takeoff, or sanitise. Silently keeping non-finite geometry is clearly not the Windows rule. ...

#### 110. IMP-M01: iOS XML parser accepts malformed files that Windows rejects and keeps raw windows-1252 bytes as invalid UTF-8
- owner: import-ost-osp | impact: data-safety | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/infrastructure/mdb/importers/ost_importer.py:427-433 (plain ET.parse, expat)`: Strict XML 1.0 parsing. Ran it: rejects (ParseError) a file with a raw 0xE9 byte and no encoding declaration, duplicate attributes, '<' inside an attribute value, a second root element, an undefined entity (&foo;), a bare '&', an unquoted attribute value and a U+0001 control character. ...
- iOS: `Core/Import/OstXmlParser.cpp:71-81 (pugixml load_buffer with default flags); consumer Services/ImportService.mm:344-349,360-363`: Host harness: ACCEPTS all of those malformed inputs (duplicate attribute: last wins; '<' kept; second root ignored; '&foo;' and bare '&' kept literally; 0x01 kept). For a windows-1252 declaration it keeps the raw bytes (43 61 66 E9 20 96), which is invalid UTF-8; only ISO-8859-1 is converted (C3 A9). The invalid bytes reach SQLite via bind_text. ...
- Windows tests: tests/infrastructure/mdb/importers/test_ost_importer.py::test_import_ost_rejects_unreadable_or_malformed_files_before_the_writer
- note: Decision needed on the approach: pugixml cannot be made strict, so reproducing expat means a pre-validation pass (libxml2/NSXMLParser, or UTF-8 validation plus a windows-1252 transcode plus duplicate-attribute checks). ...

#### 111. DBP-31: load_project does no integrity validation and silently drops takeoffs; Windows refuses to load invalid data
- owner: database-persistence | impact: data-safety | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: BRW-20, LP-M04
- Windows: `ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:144-188,1112-1115,1195-1200; infrastructure/database/bid_owned_identity.py:105-124,238-294; ...`: Raises on malformed or duplicate UIDs (positive integers only), a takeoff referencing a missing condition, page or parent, a hole on a different page than its parent, ParentUID cycles, non-finite rotation/position, and folder cycles. Takeoffs with an empty position are kept (position []).
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:238-270, 296-344`: No checks. Takeoffs with a missing condition or fewer than 2 coordinates are dropped silently (host load: 5 rows inserted, 4 loaded); missing pages, parents, cycles and bad UIDs are tolerated; folder cycles silently remove the folders.
- Windows tests: tests/infrastructure/mdb/components/test_bid_data_reader.py:4204,4250 (test_a_non_finite_rotation_violates_the_takeoff_contract_with_its_uid); tests/infrastructure/mdb/components/test_hierarchy_reader.py:218,235,311,380,391; tests/infrastructure/mdb/components/test_bid_data_reader.py::test_bid_load_rejects_takeoff_with_missing_required_owner (+4 more, see divergences.json)
- note: A strict load that aborts a whole project on one bad row is a product decision on a phone.

## (C) Behavior

86 items, global rank order. Items with disposition `needs-decision` also appear in section E; `cannot-reproduce` in section F.

#### 112. LP-16: PlanView ignores page_index, rotation, flip and the real PDF page size
- owner: rendering-planview | impact: behavior | disposition: fix | testable: objc-only | verdict: CONFIRMED | status: open
- aliases: RPV-18, RPV-19
- Windows: `ost_visualizer/application/services/page_load_strategy_service.py:29-143; presentation/components/plan_view/view.py:5040-5140; ...`: Renders PDF page page_index (Index1-1, clamped to the page count) with the page rotation, applies flip_x/flip_y to background and takeoff items, and sizes the canvas from the actual PDF page size, falling back to the stored Width/Height*72 (612x792 default).
- iOS: `PlanView/Components/PDFCanvasView.mm:18-19,43; PlanView/PlanView.mm:126-188`: Always draws CGPDFDocumentGetPage(doc, 1) with rotation 0, never flips the background (takeoffs get flipped by ost_to_pdf_coordinates), and uses the stored Width/Height*72 as the canvas, aspect-fitting the PDF to it.
- Windows tests: tests/domain/entities/test_page.py; tests/integration/pages/test_adjust_images_apply.py; tests/presentation/components/plan_view/components/test_page_loader.py (+1 more, see divergences.json)
- note: CGPDF vs PDFium and Qt vs UIKit drawing are platform differences; the page_index, rotation, flip and size rules are not. Ships together with GEO-06, RPV-02, RPV-03; also RPV-M04 (count symbol orientation) follows from a view-level page transform. Windows uses the crop/visible box size; ...

#### 113. LP-08: Pages are ordered by Index1/Name instead of BidPages.Sequence/Name/UID; folder pages and subfolders are laid out differently
- owner: layers-pages | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: BRW-18, DBP-27
- Windows: `ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:711-713; domain/entities/bid.py:80-99; presentation/components/page_combo.py:174-205,230-245,486-487`: Pages are ordered by Sequence, then Name, then UID. Folders are ordered by Name then UID. Within a folder, subfolders come first, then the folder's own pages; loose pages come last. get_first_page_uid is the first page in that tree walk. Index1 is only the PDF page index.
- iOS: `Services/ProjectService.mm:49-54,60-63; Controllers/ViewController/Components/ViewController+PageSelector.mm:104-190`: BidPages.Sequence and SheetNo are not loaded. pageList is sorted by (Index1-1, name) using byte order, folderOrder by name only with std::sort (unstable), and a folder's own pages are shown before its subfolders. With several PDFs (each Index1 starting at 1) pages interleave. The default first page is lowest Index1.
- Windows tests: tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_page_info_carries_every_stored_field_in_sequence_name_uid_order; ::test_pages_with_equal_sequence_and_name_are_ordered_by_uid; ::test_folders_nest_by_parent_drop_empty_branches_and_keep_name_order (+5 more, see divergences.json)
- note: Access ORDER BY on Name is locale and case-insensitive while SQLite BINARY (the tests) is case-sensitive; Windows prod behavior for case ties is therefore implicit. Needs Sequence (and SheetNo) loaded in ProjectStore+Load.cpp; ...

#### 114. QTY-16: UOM labels differ ('SQ IN'/'ROOF' vs 'IN²'/'SQ') and the area UOM list lacks roofing squares (7)
- owner: quantities | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: UOM-LBL-01, COND-UOMPICK-01
- Windows: `ost_visualizer/domain/services/uom_service.py:39-56,113-118`: Labels: 4 'IN²', 7 'SQ'. Area UOM options in order: (5,'SF'), (6,'SY'), (7,'SQ'), (4,'IN²').
- iOS: `Core/Domain/UomService.hpp:68-69,74-75,182-189`: 4 -> 'SQ IN', 7 -> 'ROOF'. Area options: SF, SY, SQ IN only; UOM_SQUARE_ROOFING (7) is convertible but cannot be chosen in the picker.
- Windows tests: tests/domain/services/test_uom_service.py::UomCodeTableTests::test_labels_cover_every_uom_and_unknown_codes_have_no_label; tests/domain/services/test_uom_service.py::UomCodeTableTests::test_every_offered_calculation_maps_to_the_unit_family_its_label_measures; tests/integration/quantities/test_condition_uom_parity.py::ConditionUomConsistencyTests::test_imperial_roofing_uom_remains_available_and_quantity_is_not_rescaled (+3 more, see divergences.json)
- note: Windows code uses the UTF-8 superscript characters; use the same bytes. Use the same UTF-8 bytes as Windows for the superscript labels.

#### 115. QTY-17: Unsupported condition types are computed as areas on iOS but refused/zeroed on Windows
- owner: quantities | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/services/uom_service.py:685-698,726-729; condition_quantity_service.py:73-81; uom_service.py:180-189`: Only types 0-3 are supported. calculate_condition_quantities raises UnsupportedConditionTypeError ('Unsupported condition type 7'). compute_page_quantities logs a warning and reports (0.0,0.0,0.0) for that condition without affecting the others. get_quantity_options_for_type(unknown) returns the COUNT options.
- iOS: `Core/Domain/UomService.hpp:117-132,659-728,759-761`: Any other int falls into the area branch: geometry is only computed for Area, so most calc types give 0 but calc 19/23 return 1.0 per takeoff (verified for type 7). get_quantity_options_for_type(unknown) returns the AREA options.
- Windows tests: tests/domain/services/test_uom_service.py::UomConditionTypeSupportTests; tests/domain/services/test_condition_quantity_service.py::UnsupportedConditionTypePageTests; tests/domain/services/test_condition_quantity_service.py::UnsupportedConditionTypeBoundaryTests (+1 more, see divergences.json)
- note: Only reachable with corrupted data (Type outside 0-3).

#### 116. MSH-15: A positive mesh completely removed by negatives stays visible in the 3D view
- owner: meshing | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `cpp_extensions/src/module_geometry.cpp:73-94 (inner 'if diff_result.has_value() && diff_result->is_valid()' has no else: nullopt or invalid result drops the positive); ...`: When boolean_difference_batch returns nullopt (result empty) the positive mesh is not returned at all: the element disappears from the 3D view and Three.js export. On failure/exception boolean_difference_batch returns the positive unchanged, which is kept.
- iOS: `Services/ProjectService.mm:214-220 (if diff.has_value() replace, else keep finalMesh)`: nullopt keeps the full, unsubtracted positive mesh.
- Windows tests: none found
- note: For OBJ/DXF the Windows result is different, see MSH-16.

#### 117. MSH-20: Mesh order: Windows sorts pages and groups by type (area, linear, count, attachment); iOS uses database row order
- owner: meshing | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `application/dtos/mesh_geometry_dto.py:7-8; domain/services/takeoff_domain_service.py:46-86; meshing/mesh_builder.py:110-130; renderers/threejs/mesh_processor.py:39-47`: Meshes come out page by page in sorted page-uid order, then all area takeoffs, then linear, count, attachment (hole children removed). Negative meshes keep this order, which is also the boolean subtraction order and the vertex-snap input order.
- iOS: `Services/ProjectService.mm:156-206; Services/ExportService.mm:43-78; Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:242 (no ORDER BY)`: Meshes are produced in takeoff row order across all selected pages.
- Windows tests: tests/domain/services/test_takeoff_domain_service.py::TakeoffGroupingTests::test_takeoffs_group_by_condition_type_with_empty_buckets_and_unknowns_dropped
- note: Low numeric risk, but ordering and tie-breaks were asked to match.

#### 118. MSH-19: Drop-stub union result is not passed through remove_degenerate_faces
- owner: meshing | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `meshing/mesh_factory.py:74-89,122-140 (return boolean_union(mesh, stub, mesh.metadata)); core/boolean_operations.py:62-73; cpp_extensions/src/module_geometry.cpp:119-130`: If boolean_union returns None or raises, the factory returns None and the whole takeoff mesh is dropped; the union result passes remove_degenerate_faces before use.
- iOS: `Core/Meshing/MeshFactory.cpp:61-73,80-92`: If the union has no value, the plain wall without the stub is returned; no remove_degenerate_faces on the union output.
- Windows tests: tests/presentation/visualization/core/test_boolean_operations.py::test_boolean_union_returns_none_when_native_returns_none; ...::test_boolean_union_logs_and_returns_none_when_native_raises
- note: Part (a) of the original MSH-19, split by the verifier. Union failure handling is MSH-19b. Tests that go through MeshFactory with drop_run need Manifold (OST_TEST_BOOLEAN=ON) or a local boolean_union stub.

#### 119. EXP-14: Export with no pages selected exports every page; Windows does nothing
- owner: export-formats | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: MSH-18, LP-13
- Windows: `presentation/handlers/export_handler.py:129-136; presentation/controllers/menu_controller.py:695-701; application/services/export_service.py:150-153`: export_format returns when page_uids is empty; the menu action is disabled without selected pages that have takeoffs. collect_takeoffs_for_pages([]) is empty, which gives NO_DATA.
- iOS: `Services/ExportService.mm:26,44; Controllers/ViewController/Components/ViewController+Export.mm:5-8; Services/ProjectService.mm:73-79,137`: selectedPages.empty() means 'no filter', so all pages are exported. The 3D view with an empty selection renders nothing (ProjectService.mm:137), but toggling pages off can leave the selection empty and the Export button still works.
- Windows tests: tests/presentation/handlers/test_export_handler.py; tests/application/services/test_export_service.py::test_empty_or_hidden_only_collection_returns_no_data_without_export; tests/application/services/test_export_service.py
- note: Pure logic once generateVisibleMeshes is moved into Core.

#### 120. EXP-13: OBJ face winding is inside-out for cylinders (circle/ellipse count) and curved walls
- owner: meshing | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: MSH-M02
- Windows: `exporters/adapters/obj_writer.py:32-43 (faces written as (f0,f2,f1))`: Every box-like, cylinder and curved mesh comes out outward-facing (positive signed volume) after the vertex rotation and face swap. Verified: circle d=24 h=48 gives +21467.487; curved wall (0,200)-(100,200) via (50,230), curve=0, h=96, t=6 gives +70478.256; straight/sloped wall, rotated box, area flat and sloped, and attachment all positive.
- iOS: `Core/Meshing/gen_count.cpp (cylinder), Core/Meshing/gen_curved.cpp; Core/Export/obj_writer.cpp:138-143 (faces written as stored)`: Box meshes are outward-facing, but cylinder meshes (circle, and ellipse routed to the cylinder) and curved walls are inside-out. Harness signed volumes: circle -21467.487, curved wall -70478.256, ellipse -21467.487 (the ellipse geometry is also wrong; see the mesh area).
- Windows tests: tests/presentation/visualization/exporters/adapters/test_obj_writer.py::test_coordinate_winding_and_multi_mesh_indices_without_mutating_input; tests/presentation/visualization/utils/test_mesh.py::PrismTopologyTableTests (closed prisms only, sign not asserted)
- note: The root cause is in the meshing area. It is visible in OBJ viewers with backface culling, where the cylinders look empty or black. Windows itself writes the isosceles triangle prism inside-out (signed volume -6912 for w=24,d=12,h=48), so do not copy that quirk when triangle prisms are ported. ...

#### 121. EXP-11: OBJ/DXF grouping and naming: Windows has one group, material and layer per condition; iOS one group per takeoff and DXF layers named by raw condition name
- owner: export-formats | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `exporters/base_exporter.py:109-149; exporters/obj_exporter.py:18-84; exporters/dxf_exporter.py:12-39`: Groups are keyed by condition_uid in first-seen order over [non-area takeoffs..., area parent takeoffs...]. Material and layer name = sanitize('{cdn_type_name or Unknown}_{condition.name or Condition_<uid>}_{uid[:8]}'). ...
- iOS: `Services/ExportService.mm:109-113,143-156,182-184`: OBJ: one group and one material per positive mesh named sanitize(condition.name+'_'+index), in project.takeoffs order, with no '# name' comment and no empty groups. DXF: layer name = raw condition.name (unsanitised, 'Layer_<i>' if empty). Conditions with the same name merge into one layer (add_layer overwrites the colour; last colour wins). ...
- Windows tests: tests/presentation/visualization/exporters/test_obj_exporter.py::test_export_resets_indices_skips_backouts_and_writes_paired_material_file; tests/presentation/visualization/exporters/test_dxf_exporter.py::test_writer_is_reset_and_receives_only_positive_feature_edges
- note: OBJWriter would need a per-condition group API (mesh list per group) or ExportService must group first.

#### 122. EXP-12: Name sanitisation rules differ (OBJ group/material and DXF layer)
- owner: export-formats | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `exporters/base_exporter.py:172-198`: Replaces ' ' '/' '\\' ':' ';' '*' '?' '<' '>' '|' '.' '(' ')' '[' ']' '{' '}' with '_', and removes '"' "'" ','. All other characters are kept (non-ASCII, #, &, =, %, +, -, @). A leading digit gets the 'Mat_' prefix. An empty result becomes 'Unknown'.
- iOS: `Core/Export/obj_writer.cpp:84-102`: Whitelist: keeps ASCII alnum, '_' and '-'; ' ' becomes '_'; every other byte is dropped (all UTF-8 non-ASCII bytes vanish); no digit prefix; empty becomes 'mesh'. DXF layer names are not sanitised at all (ExportService.mm:144).
- Windows tests: tests/presentation/visualization/exporters/test_obj_exporter.py
- note: Apply the Windows sanitiser to DXF layer names too. DXF forbids <>/\":;?*|=` in layer names.

#### 123. EXP-16: MTL body and header/comment lines differ from Windows
- owner: export-formats | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `exporters/obj_exporter.py:35-54,64-66; tests/presentation/visualization/exporters/test_obj_exporter.py:44-65`: The OBJ header is '# Created by OST Visualizer' then 'mtllib <stem>.mtl' then a blank line. The MTL header is '# Created by OST Visualizer', '# Display Mode: <mode>', '# Materials: <n>' and a blank line. Per material: 'newmtl', '# <original name>', 'Kd %.6f %.6f %.6f', 'Ka 0.200000 ...', 'Ks 0.500000 ...', 'Ns 20.000000', 'Ni 1.000000', 'd 1.000000', 'illum ...
- iOS: `Core/Export/obj_writer.cpp:124-125,158-171`: OBJ header '# OBJ exported by OST Visualizer (iOS)'. MTL header '# MTL exported by OST Visualizer (iOS)' with no mode/count lines. Per material: 'Kd %.4f' (e.g. 0.5020 vs 0.501961), 'Ka 0.1000 0.1000 0.1000', 'Ks 0.0000 0.0000 0.0000', 'd 1.0000', 'illum 1'; no Ns/Ni, no '# name' line. The different Ks/illum change viewer shading.
- Windows tests: tests/presentation/visualization/exporters/test_obj_exporter.py::test_export_resets_indices_skips_backouts_and_writes_paired_material_file
- note: The display-mode label comes from config (Windows default 'Original'); iOS has no 'Original' mode name (RenderConfig 'OST').

#### 124. LP-10: Tapping a folder REPLACES the 3D selection; Windows folder check adds/removes only that folder's pages
- owner: layers-pages | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/components/page_combo.py:322-345`: Checking a folder (including a partially checked one) adds all pages beneath it (nested included) to the existing selection; unchecking removes only those pages; pages selected elsewhere are kept and an update is emitted only if the selection actually changed. A folder with no pages has no checkbox.
- iOS: `Services/ProjectService.mm:90-100; ViewController+PageSelector.mm:281-286`: select3DFolder clears the whole selection and inserts the folder's pages, so a second tap does nothing; there is no way to remove a folder's pages and pages selected elsewhere are lost. The old porting log records this as an intentional 'idempotent' choice.
- Windows tests: tests/presentation/components/test_page_combo.py::test_folder_checkbox_keeps_pages_checked_elsewhere; ::test_toggling_a_partial_folder_checks_all_of_its_pages; ::test_folder_checkbox_checks_and_unchecks_every_page_beneath_it (+1 more, see divergences.json)

#### 125. LP-11: Re-tapping the active 2D page clears the 2D page; Windows keeps it
- owner: layers-pages | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/components/page_combo.py:685-704 (SinglePageComboBox._on_tree_clicked/_select_uid); ...`: Clicking a page row only activates it when uid != active; the active page is never cleared by clicking it again.
- iOS: `Services/ProjectService.mm:82-88; ViewController+PageSelector.mm:298-316`: select2DPage toggles: selecting the already-active page sets selected2DPageUID to empty and loads a blank canvas (width 612x792).
- Windows tests: tests/presentation/components/test_page_combo.py::test_clicking_a_page_row_activates_it_without_toggling_its_check; ::test_unchecking_final_3d_page_preserves_active_2d_page

#### 126. LP-12: Initial page is the lowest Index1 page; Windows restores the saved BidPageSelectedUID, else the first page of the tree walk
- owner: layers-pages | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/coordinators/ui_event_coordinator.py:1136-1169,4940-4980; infrastructure/mdb/components/bid_data_reader.py:218-245`: Order of precedence: pending/restored UI selection, current active page, BidSettings.BidPageSelectedUID (if that page exists), else the first page in combo (tree-walk) order. Both the 3D selection and the 2D active page start as that one page. The saved page is written when the user picks a page, and a fallback first page is not saved until picked.
- iOS: `Services/ProjectService.mm:64-67`: BidSettings.BidPageSelectedUID is never read or written; both 3D and 2D selections start as pageList[0] (lowest Index1, then name).
- Windows tests: tests/integration/navigation/test_bid_switch_last_page.py
- note: Reading it is safe; the write-back rule (save on pick, not for the unpicked fallback) is optional and can be decided later.

#### 127. LP-15: The Image layer does not hide the page background on iOS
- owner: rendering-planview | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:670-680,755; domain/services/project_data_service.py:645-675; ...`: Page.layer_visible is the visibility of the layer named 'image' (name trimmed and lowercased). Toggling the Image layer, or Show All / Hide All, updates every page's layer_visible and the PDF/overlay is not loaded or drawn when it is false.
- iOS: `PlanView/PlanView.mm:126-165 (draws the PDF whenever image_path exists); ViewController+Layers.mm`: The Image layer appears in the list and its Show flag is persisted, but it has no effect on the page background.
- Windows tests: tests/domain/services/test_project_data_service.py::test_image_layer_visibility_updates_page_memory_immediately; ::test_show_all_layer_visibility_updates_conditions_and_pages_immediately; ::test_non_image_layer_visibility_does_not_update_page_memory_by_name_guess

#### 128. LP-18: The Comments layer is listed and bulk-toggled on iOS; Windows hides it from the layer list
- owner: layers-pages | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/application/services/project_read_service.py:43-53; domain/services/project_data_service.py:520-545`: get_merged_bid_layers and get_bid_layer_snapshot drop the layer whose name (trimmed, case-insensitive) is 'comments', so it is neither shown nor affected by Show All / Hide All.
- iOS: `Controllers/ViewController/Components/ViewController+Layers.mm:8-24,83-119`: Every bid layer is listed. Show All / Hide All write Show to every layer row including Comments.
- Windows tests: tests/application/services/test_project_read_service.py::test_merged_bid_layers_hide_comments_layer_from_project_ui; tests/domain/services/test_project_data_service.py::test_bid_layer_snapshot_fallback_filters_comments_layer

#### 129. LP-20: Page field loading: image_show_mode comes from the wrong column; flag, rotation parsing; Sequence/SheetNo not loaded
- owner: database-persistence | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-28, DBP-29, BOOL-01
- Windows: `ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:739-769 (sequence 745, rotation 751, flips 752-753, show 763)`: image_show_mode = BidPages.Show; rotation = int(Rotation or 0) % 360; flip_x/flip_y true only for -1 or 1 (-1, True); overlay_resized via _is_stored_flag_set (-1, 1, 'true'); Sequence and SheetNo are loaded (used for ordering and labels).
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:83,89-97`: image_show_mode is read from RasterDrawMethod (column 19) instead of Show; rotation is the raw int (450 stays 450, negative stays negative); flip and overlay flags are any non-zero value (FlipX=2 is true, text 'true' reads 0); Sequence, SheetNo, ZoomFac, CurrentX/Y, Invert and Bitonal are not loaded.
- Windows tests: tests/infrastructure/mdb/components/test_bid_data_reader.py; tests/infrastructure/mdb/components/test_hierarchy_reader.py::HierarchyRealSchemaTests::test_page_info_carries_every_stored_field_in_sequence_name_uid_order; tests/infrastructure/mdb/components/test_bid_data_reader.py:2456 (BidPages Show column read) (+1 more, see divergences.json)
- note: Only Sequence (LP-08) and, later, overlay features consume these today; rotation is normalised inside the transform functions.

#### 130. COND-FOLDER-01: Condition folders: flat, folderless conditions placed in the first folder, empty folders hidden, ParentUID ignored
- owner: conditions | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: QTY-20, DBP-32, DBP-30
- Windows: `ost_visualizer/presentation/components/conditions_sidebar.py:461-481,606-638; infrastructure/mdb/components/bid_data_reader.py:1010-1062`: Folders form a tree via ParentUID; conditions without a folder (or with an unknown folder uid) sit at the root next to the root folders; empty folders are still shown; type groups (default on) are built inside each folder or at the root; folders and type groups sort by name; ...
- iOS: `Controllers/ConditionsViewController.mm:133-222; Core/Domain/ConditionFolder.hpp:7-11; ProjectStore+Load.cpp:139-152`: Folders are flat (no ParentUID/nesting), sorted by name; folderless conditions are put in the alphabetically-first folder (fallback_folder_uid); folders without conditions produce no section; the folder name is shown only when there is more than one folder; ...
- Windows tests: tests/presentation/components/test_conditions_sidebar.py; tests/infrastructure/mdb/components/test_condition_folder_operations.py; tests/infrastructure/mdb/components/test_bid_data_reader.py (+2 more, see divergences.json)
- note: Nested display on iOS is a UI design choice (indent/section headers), but the grouping/placement rules must follow the Windows tree. Core rules are in scope (folderless at root, empty folders shown, ParentUID loaded, siblings sorted by name at every level, dangling type uid -> Unknown group); ...

#### 131. COND-LIST-01: Rows inside a section are unordered (Windows sorts by ref_no)
- owner: conditions | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: QTY-21
- Windows: `ost_visualizer/presentation/components/conditions_sidebar.py:234-239,687-710`: Tree sorting is enabled on column 'No.' ascending; condition items sort by ref_no (numeric), so within each folder/type group conditions appear in ascending ref_no (ties keep database read order).
- iOS: `Controllers/ConditionsViewController.mm:146-204`: Rows are appended while iterating std::unordered_map<string, Condition>, so order within a section is arbitrary and can change between launches.
- Windows tests: tests/presentation/components/test_conditions_sidebar.py::test_condition_sidebar_delete_replacement_selects_previous_logical_condition
- note: The logical-order tests (condition_selection_after_delete) rely on this ordering.

#### 132. COND-PLACE-01: Conditions on a hidden layer can still be edited and used for placing on iOS
- owner: conditions | impact: behavior | disposition: fix | testable: objc-only | verdict: CONFIRMED | status: open
- aliases: LP-17, RPV-21
- Windows: `ost_visualizer/presentation/coordinators/placement_coordinator.py:52-72,127-129; presentation/components/conditions_sidebar.py:310-335,712-725,858-870`: A condition whose layer is hidden (layer_visible False) is not placeable (Placement.enter returns False and exits placement), its sidebar row is greyed with no inline rename, and the Edit button / double-click edit are disabled for it.
- iOS: `Controllers/ViewController/Components/ViewController+Conditions.mm:45-65; Controllers/ConditionsViewController.mm:389-405`: didSelectConditionUID always enters PlaceTakeoff mode (no layer-visibility check), the row is not greyed, and the detail button opens the edit dialog for any condition.
- Windows tests: tests/presentation/coordinators/test_placement_coordinator.py::test_enter_rejects_unplaceable_requests_without_activating_plan_view; tests/presentation/components/test_conditions_sidebar.py::test_double_click_edit_dialog_follows_properties_permission_and_placeability; tests/integration/visibility/test_deferred_layers.py (+2 more, see divergences.json)
- note: Windows also suspends an active placement tool when its layer is hidden and restores it on Show.

#### 133. COND-LAYER-01: Layer choices, ordering and empty-layer handling differ
- owner: conditions | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: LP-21
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1086-1163,1475-1479; domain/entities/layer.py:89-93; application/services/project_read_service.py:43-53`: Layers offered = bid layers merged by case-insensitive name (template-preferring) sorted by sequence, minus only the Comments layer (so Image, Annotation, Default and user layers are all selectable; default sequence order Image, Annotation, Default, Comments). An empty layer text is saved as layer_uid None (BidLayerUID NULL); ...
- iOS: `EditConditionViewController.mm:82-106,584-586; EditConditionViewController+Pickers.mm:113-135; EditConditionViewController+TableView.mm:185-203`: Offers Default first then the rest sorted by name, excluding Image, Annotation and Comments; if the condition has no layer, Save silently assigns the first offered layer (Default) and the picker has no way to keep 'none'.
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py; tests/integration/conditions/test_nested_layer_editor.py; tests/application/services/test_project_read_service.py::test_merged_bid_layers_hide_comments_layer_from_project_ui
- note: Whether iOS should intentionally hide Image/Annotation layers is a product decision; Windows allows them.

#### 134. COND-NAV-01: Edit Previous/Next walks conditions in name order, not list order
- owner: conditions | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/handlers/condition_action_handler.py:1708; presentation/components/conditions_sidebar.py:350-364`: The navigation list is sidebar.collect_ordered_condition_uids(): the visible tree order (folders/type groups by name, conditions by ref_no ascending), so Previous/Next follow what the user sees.
- iOS: `Controllers/ViewController/Components/ViewController+Conditions.mm:82-90`: All condition uids sorted by std::string name compare across the whole project, ignoring folders, type groups and ref_no.
- Windows tests: tests/integration/conditions/test_editor_navigation.py::test_no_discard_cancel_and_navigation_boundaries; tests/integration/conditions/test_editor_navigation.py::test_mdb_save_previous_and_repeated_navigation_keep_sidebar_in_sync

#### 135. COND-VAL-01: Empty Display Size and other plain-number edge handling
- owner: conditions | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: DLG-DISPSIZE-01
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1497-1526,1538-1561`: Count/Attachment Display Size empty -> 100.0; non-numeric or non-finite -> 'Invalid Value: "x" is not a valid number for Display Size.'; outside 10..500 -> 'Validation Error: Display Size must be between 10% and 500%.' Rise/Run (Linear): float(text or '0'), non-finite rejected with 'Rise/Run must be a finite number.'
- iOS: `EditConditionViewController.mm:523-544,615-631,642-649`: Empty Display Size is parsed as 0.0 and then rejected by the range check; plain numbers go through NSScanner scanDouble with no finite check (whether 'nan'/'inf' are accepted by NSScanner could not be verified here, and 'nan' would slip past the range comparison).
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py::test_edit_condition_rejects_non_finite_numeric_values; tests/application/use_cases/project/test_update_condition_use_case.py::test_use_case_rejects_invalid_changes_before_writing; tests/application/use_cases/project/test_update_condition_use_case.py::test_use_case_accepts_boundaries_skips_empty_and_reports_writer_failure

#### 136. COND-TRIM-01: Trim and Curved Segment are not mutually exclusive on iOS
- owner: conditions | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1305-1331,899-901`: Checking Trim unchecks and disables Curved Segment; checking Curved unchecks Trim; a loaded condition with both set shows Curved off and persists it as false.
- iOS: `EditConditionViewController+TableView.mm:429-457`: The two switches are independent; both can be saved as true.
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py::test_trim_disables_curved_segment_control

#### 137. BRW-09: Restore from Deleted Bids ignores OrigBidProjectUID; iOS also allows moving trash bids to any folder
- owner: project-browser | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-22
- Windows: `ost_visualizer/presentation/handlers/project_write_handler.py:580-614; presentation/components/project_tree_view.py:167-186,1502-1525,284-285`: Restore groups the selected trash bids by Bids.OrigBidProjectUID and moves each group back to that project (no original -> orphan). Trash bids cannot be dragged anywhere else, and nothing can be dropped onto Deleted Bids; the only way out of the trash is Restore.
- iOS: `Controllers/DatabaseViewController.mm:371-457; ProjectStore+Folders.cpp:191-206`: Trash bids get 'Restore as Unassigned' (BidProjectUID=NULL; OrigBidProjectUID is never consulted), and also the 'Move to Folder' menu, which re-homes a trash bid into any folder.
- Windows tests: tests/presentation/handlers/test_project_write_handler.py::test_restore_bids_groups_by_original_project_and_uses_the_trash_marker; tests/presentation/handlers/test_project_write_handler.py::test_restore_bids_stops_on_a_flush_or_write_failure; tests/presentation/components/test_project_tree_view.py::test_drag_eligibility_excludes_deleted_bids_and_cross_database_selection (+2 more, see divergences.json)
- note: A dangling OrigBidProjectUID makes the Windows group fail (Restore Error, no change); iOS behaviour for it needs the user (rare once BRW-05 is fixed).

#### 138. BRW-13: Bids within a folder and among orphans are listed UID DESC instead of (BidProjectUID, BidNo, UID) ascending
- owner: project-browser | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-26
- Windows: `ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:72-81; presentation/components/project_tree_view.py:453-454,689`: The reader orders all bids by BidProjectUID, BidNo, UID (ascending, NULLs first). The tree then sorts by column 0 = bid_no numerically ascending (1, 2, 10, 33, 100) with ties kept in reader order (UID ascending). Orphans: ordered by BidProjectUID first (NULL, then 0, then dangling ids).
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp:46-48; Core/Domain/DatabaseHierarchy.hpp:11-16`: ORDER BY UID DESC for every bid; BidNo is not read. Host-run: folder 5 bids [21,20,12,11,10], orphans [32,31,30]. Windows would give folder 5: [20,10,11,12,21] (BidNo 1,5,5,5,9) and orphans [31 (NULL), 32 (0), 30 (99)].
- Windows tests: tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_bids_with_equal_project_and_number_are_ordered_by_uid; tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_full_hierarchy_maps_every_bid_field_and_orders_bids_and_projects; tests/presentation/components/test_project_tree_view.py::test_default_bid_order_is_numeric_and_keeps_status_uid_separate_from_sort_value (+2 more, see divergences.json)
- note: Needs BidNo in BidSummary. The displayed order is BidNo ascending (the tree re-sorts column 0 with a stable sort); reader order only breaks ties. Folder name collation is BRW-M01.

#### 139. BRW-15: Empty nested page folders are kept; Windows prunes empty branches at every level
- owner: database-persistence | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-M01
- Windows: `ost_visualizer/infrastructure/parsers/utils/parser.py (remove_empty_folders); hierarchy_reader.py:265`: Recursively removes any folder that has no pages and no non-empty subfolders.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:336-344`: Only roots are pruned (and only if empty overall). Empty subfolders stay in the tree, and the page selector prints a label for them. Host-run: EmptyChild(13) under Root stays.
- Windows tests: tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_folders_nest_by_parent_drop_empty_branches_and_keep_name_order; tests/infrastructure/mdb/components/test_hierarchy_reader.py:644 (test_folders_nest_by_parent_drop_empty_branches_and_keep_name_order, pins root-level pruning and name order)

#### 140. BRW-17: Page-folder children are held in unordered_map, so subfolder order is arbitrary (Windows: Name, UID)
- owner: database-persistence | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:167-176,182-206; presentation/components/page_combo.py:241-244`: Folders are read ORDER BY [Name],[UID] and dicts keep insertion order, so roots and subfolders appear in Name,UID order, subfolders before the folder's own pages.
- iOS: `Core/Domain/Folder.hpp:15; ProjectStore+Load.cpp:279-283 (no ORDER BY); ViewController+PageSelector.mm:209`: Subfolders are visited in hash order. Host-run: Root's subfolders printed Middle, EmptyChild, Alpha instead of Alpha, EmptyChild, Middle.
- Windows tests: tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_folders_nest_by_parent_drop_empty_branches_and_keep_name_order

#### 141. BRW-10: Trash stores a dangling BidProjectUID as OrigBidProjectUID
- owner: project-browser | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/services/project_data_service.py:1080-1088 (find_project_uid_for_bid); presentation/handlers/project_write_handler.py:640-645; ...`: A bid whose BidProjectUID is not a known project is an orphan: find_project_uid_for_bid returns None, so no original project is recorded. The writer also refuses a restore project that has no BidProjects row.
- iOS: `Core/Persistence/ProjectStore/ProjectStore.cpp:580-610`: Reads the raw Bids.BidProjectUID, even when it points at no existing project, and writes it to OrigBidProjectUID. Host-run: bid 1 with BidProjectUID 99 (no such project) -> (BidProjectUID 1, OrigBidProjectUID 99).
- Windows tests: tests/presentation/handlers/test_project_write_handler.py::test_unassigned_bids_share_one_trash_group_whatever_their_missing_project_is; tests/infrastructure/mdb/components/test_project_operations.py::test_move_bid_rejects_missing_restore_project_before_mutation

#### 142. BRW-12: assign_bid_to_folder does not check that the bid and the target project exist
- owner: project-browser | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/components/project_operations.py:15-81`: move_bids_to_project requires every bid UID to exist exactly once and the target project (and restore project) to exist; otherwise it returns False and nothing in the batch changes.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp:172-189`: Plain UPDATE Bids SET BidProjectUID=? WHERE UID=? and always returns true. Host-run: assign(bid 1, folder '999') -> true and BidProjectUID=999 (dangling; later claimable per BRW-03). assign(missing bid '77','1') -> true.
- Windows tests: tests/infrastructure/mdb/components/test_project_operations.py::test_move_bid_rejects_missing_project_owner_before_mutation; tests/infrastructure/mdb/components/test_project_operations.py::test_move_bids_rejects_missing_batch_member_before_mutation; tests/infrastructure/mdb/components/test_project_operations.py::test_a_missing_destination_project_blocks_the_move (+1 more, see divergences.json)
- note: Orphaning a missing bid is a success on Windows (test_deleting_or_orphaning_a_project_or_bid_that_is_absent_changes_nothing), so remove_bid_from_folder already matches.

#### 143. BRW-19: Folder names are not trimmed and whitespace-only names are accepted
- owner: project-browser | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/handlers/project_write_handler.py:255-284; presentation/components/project_tree_view.py:842-855`: The name is stripped (Python str.strip()) before writing; a blank or whitespace-only name is refused with no write (the tree restores the old label); an unchanged name is a no-op. A new folder is created as 'New Project' and renamed inline.
- iOS: `Controllers/DatabaseViewController.mm:155-165,191-197`: Only checks name.length > 0. ' ' creates or renames to a whitespace-only name; surrounding spaces/newlines are stored. Rename is always written.
- Windows tests: tests/presentation/handlers/test_project_write_handler.py::test_rename_project_normalizes_the_name_and_picks_the_database; tests/presentation/handlers/test_project_write_handler.py::test_rename_project_refuses_blank_names_and_unsaved_pending_changes; tests/presentation/components/test_project_tree_view.py::test_project_inline_rename_commits_changed_name_and_ignores_blank_or_unchanged
- note: Python strip() removes more characters than NSCharacterSet whitespaceAndNewlineCharacterSet (e.g. U+001C-U+001F); the helper should document its character set.

#### 144. RPV-13: Minimum placement length uses strict >=; Windows also accepts values within isclose of the minimum
- owner: rendering-planview | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: SNP-06
- Windows: `placement_mode.py:95-96, 1748-1813, 1815-1854`: _meets_placement_minimum(length, minimum) = length >= minimum or math.isclose(length, minimum) (rel_tol 1e-9). The minimum is the snap increment, or 1e-6 when the increment is 0. Applied to the chord length (linear) and to both |dx| and |dy| (area rectangle).
- iOS: `PlanView+Placement.mm:372-376, 412-416`: hypot(dx,dy) >= minLen and fabs(dx) >= minLen && fabs(dy) >= minLen, strict; a one-increment drag is silently discarded when float rounding lands just below the increment.
- Windows tests: test_placement_mode.py::test_minimum_fractional_linear_and_area_release_preserves_preview; ::test_subminimum_geometry_does_not_commit; tests/presentation/components/plan_view/components/test_input_handler.py:4590-4660
- note: Computed: for k in 0..39, 14 steps of k*0.1 -> (k+1)*0.1 are below 0.1.

#### 145. RPV-17: Every takeoff with a parent_uid is dropped as a hole, so attachments vanish from 2D, 3D and exports
- owner: rendering-planview | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `presentation/visualization/pdf/renderers/takeoff_renderer.py:617-697; domain/services/takeoff_domain_service.py:63-86; meshing/mesh_builder.py:91-170`: Only AREA children of AREA parents are holes (backouts). Children of other types, such as attachments (parent = area takeoff), are drawn as normal count items in 2D and meshed in 3D (processing order area, linear, count, attachment).
- iOS: `PlanView/PlanView.mm:211-217, 229-232; Services/ProjectService.mm:112, 167; Services/ExportService.mm:24, 54`: if (takeoff.is_hole()) continue; with is_hole() = parent_uid not in ('', '0'). Attachment takeoffs get no overlay, no hit area, no 3D mesh and no export geometry.
- Windows tests: tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py::test_only_real_area_parents_with_area_children_make_holes
- note: The 3D and export part overlaps the meshing mapper (ProjectService.mm:167, ExportService.mm:54). A hole condition check (parent is an area takeoff and child is area type) is the Windows rule.

#### 146. RPV-20: Polygon close detection uses the snapped tap point and a strict canvas distance; Windows uses the raw cursor and <= 12 viewport px
- owner: rendering-planview | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `placement_mode.py:1697-1730`: If len(points) >= 3 and the raw cursor viewport position is within 12 px (<=) of the first point, the polygon closes (if valid) without appending a point.
- iOS: `PlanView+Placement.mm:297-322`: Computes the tap OST after grid rounding and angle snapping from the last point, converts that back to canvas and tests distance < 12/zoom, so angle snapping can pull the point more than 12 pt away from the first point and prevent closing.
- Windows tests: tests/presentation/components/plan_view/components/test_placement_mode.py (area click placement tests)
- note: Closing an invalid polygon neither flashes nor appends, on both sides.

#### 147. RPV-25: Count shapes: hexagon, octagon, rhombus orientation and size, and ellipse rotation differ
- owner: rendering-planview | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: GEO-17
- Windows: `domain/entities/shape.py:12-16, 48-115; presentation/visualization/core/geometry/takeoff_geometry.py:58-82`: Ellipse/circle: a 32-gon with radii (w/2, d/2) rotated by the takeoff rotation. Triangle: apex (0,-d/2), base (+-w/2, +d/2). Rhombus: points (0,-w/2),(0.575*w/2,0),(0,w/2),(-0.575*w/2,0) using width only. Hexagon: 6 vertices from angle 0 at radius w/2 on both axes; octagon: 8 vertices from angle pi/8 at radius w/2 (depth ignored). ...
- iOS: `PlanView/PlanView.mm:508-623, 653-789; PlanView/Components/TakeoffOverlayView.mm:5-14`: Analytic ellipse with rotation ignored; rhombus uses ry (depth) vertically; hexagon and octagon via regularPolygonPoints(cx,cy,rx,ry,n) starting at -90 degrees with ry radius. Triangle, rectangle and square match.
- Windows tests: tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py::CountGeometryTests; tests/domain/entities/test_shape.py; tests/presentation/visualization/meshing/test_mesh_factory.py
- note: Values produced by running compute_count_vertices. Sizes and minimums are RPV-26.

#### 148. RPV-26: Count and linear size rules and minimums differ
- owner: rendering-planview | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `takeoff_geometry.py:8-9, 95-120; takeoff_renderer.py:540-615`: Point size in OST units: width = max(width or 1.0, 1.0); depth = width for Square/Circle else max(depth or width, 1.0); for COUNT conditions both are scaled by max(display_size, 0.1)/100 (attachments are not scaled). Pixels = ost/ratio*72*view_scale (view_scale = 3.0 for PDF and blank pages, render_quality.py:3). ...
- iOS: `PlanView/PlanView.mm:526-545, 363-368, 374-377, 448-451`: Width uses (width>0 ? width : 1.0) with no 1.0 floor; display_size <= 0 becomes 100%; the scale applies to every count/attachment; a 4 pt minimum is applied to each dimension independently; the linear minimum is 2.0 pt (kMinThicknessPts) in canvas points, i.e. 3 times thicker than 2.0 scene px at view_scale 3.
- Windows tests: tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py::test_count_takeoff_is_centered_and_has_a_minimum_rendered_size; ::test_linear_thickness_scales_with_view_scale_and_has_a_minimum
- note: Values obtained by running takeoff_geometry. The pixel-to-point mapping assumes the Windows plan scene runs at view_scale 3.0 for PDF pages; confirm with the plan-view owner before choosing iOS constants.

#### 149. RPV-31: Area holes are painted as white polygons and are not hit-testable
- owner: rendering-planview | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `takeoff_renderer.py:699-736, 617-697`: Holes are subtracted from the parent path (the PDF shows through) and each hole keeps an invisible, selectable item with its own uid, so tapping inside a hole selects the hole takeoff.
- iOS: `PlanView/PlanView.mm:297-317, 229-232, 396-415`: A white fill with alpha 1 is drawn over the parent, hiding the PDF inside the hole; the hit area of the parent is its outer polygon only, so tapping inside a hole selects the parent.
- Windows tests: tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py::test_area_solid_fill_excludes_backout_hole; ::test_area_linear_patterns_exclude_backout_hole
- note: Windows tests pin subtraction (test_area_solid_fill_excludes_backout_hole).

#### 150. UND-01: Undo history is unbounded on iOS; Windows keeps 50 entries and drops the oldest
- owner: settings-undo-camera | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/services/undo_redo_service.py:76 (max_size=50), :173-195 (_push_entry: pop(0) when len > max_size)`: The undo stack is capped at 50 entries and the oldest is dropped on overflow. A push also clears the redo stack.
- iOS: `Controllers/ViewController/Components/UndoRedoController.mm:14-17,34,43,123`: std::vector stacks with no cap, so every drag, place and delete since project load stays undoable and held in memory.
- Windows tests: tests/presentation/services/test_undo_redo_service.py:403-414
- note: UndoRedoController holds pure C++ state and could move to Core as a UndoHistory with the cap.

#### 151. UND-07: Multi-takeoff move pushes one undo entry per takeoff; Windows pushes one entry for the whole move
- owner: settings-undo-camera | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/handlers/plan_view_action_handler.py:2174-2251 (on_positions_flushed: one push_local with all changed takeoffs)`: All takeoffs and annotations moved by one gesture are undone and redone atomically as one history entry. AGENTS.md: mixed geometry and property replay is one application mutation.
- iOS: `Controllers/ViewController/ViewController.mm:945-969`: For a multi drag, didEndDragTakeoff loops over _multiDragStartPositions and calls pushMoveForTakeoff once per takeoff, producing N entries and N single-takeoff undos. Each also persists separately.
- Windows tests: tests/integration/history/test_small_drag.py:68-136; tests/integration/history/test_attachment_movement.py:15
- note: UndoRedoController needs a grouped Move entry holding a list of (uid, old, new).

#### 152. UND-09: 'Count as Negative Quantity' toggle is not undoable and applies to one takeoff
- owner: settings-undo-camera | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/handlers/plan_view_action_handler.py:2072-2116 (on_set_negative; _push_local_property_history)`: The negative flag is set for all selected takeoffs in one history entry, with old and new values captured, so it can be undone and redone.
- iOS: `Controllers/ViewController/ViewController.mm:515-548`: The menu handler calls ImportService setTakeoffNegative for the single pressed takeoff and mutates the project directly. No undo entry is pushed, and the BOOL result of the DB write is ignored.
- Windows tests: tests/presentation/handlers/test_plan_view_action_handler.py:7407,13434
- note: Requires a new UndoAction kind for property edits.

#### 153. UND-11: No-op drag detected with exact equality instead of a 1e-9 absolute tolerance
- owner: settings-undo-camera | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/components/plan_view/components/drag_handler.py:266-280 (_positions_meaningfully_different)`: A drag whose result equals the original within abs_tol=1e-9 per value (or has an equal length) is treated as unchanged: no flush, no history, no write. A length mismatch counts as different.
- iOS: `Controllers/ViewController/ViewController.mm:952,990`: if (pair.second != tk.position) and if (oldPos != newPos) use exact vector equality, so floating residue pushes a history entry and writes to SQLite for a visually unchanged drag.
- Windows tests: tests/presentation/components/plan_view/components/test_drag_handler.py:1427-1437
- note: Related to SNP-04: per-vertex re-quantization makes the residue more likely on iOS.

#### 154. SET-01: Default and fallback display mode: Solid on iOS, Original on Windows; mode vocabulary is OST vs Original
- owner: settings-undo-camera | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: COL-MODE-01, DBP-36
- Windows: `ost_visualizer/domain/entities/config.py:26 (DEFAULT_DISPLAY_MODE = Original), :55-58; domain/aggregates/config_aggregate.py:222-235,441-451`: Vocabulary is Solid, Original, Transparent. The default (also used when a stored or updated value is invalid) is Original. An invalid stored mode is replaced by Original and the correction is saved with a warning.
- iOS: `Core/Domain/RenderConfig.hpp:11-15; Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:270-272; Services/ImportService.mm:525-526; ...`: Vocabulary is Solid, OST, Transparent. The default and the invalid fallback are Solid. The label OST replaces Original.
- Windows tests: tests/domain/entities/test_config.py:33-36,293-323; tests/domain/aggregates/test_config_aggregate.py:433-473; tests/application/services/test_config_service.py:161-174 (+2 more, see divergences.json)
- note: Renaming the persisted value or label (OST vs Original) is cosmetic as long as it maps to the same behavior. The default is the substantive part. Windows keeps separate 2D/3D modes with a sync flag (default synced); ...

#### 155. SET-02: Render config save ignores write failures and never writes back corrected values
- owner: settings-undo-camera | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/aggregates/config_aggregate.py:397-428 (_save_config, update_options); :222-235 (_apply_config with save_corrections)`: A failed save raises OSError and the in-memory config is rolled back with no event published. A load that corrected an invalid stored value saves the correction once.
- iOS: `Core/Persistence/ProjectStore/ProjectStore_Internal.hpp:104-128 (set_pref ignores failed prepare or step); ProjectStore+Conditions.cpp:263-290; ...`: set_pref silently returns on failure and save_render_config returns true whenever the DB opens, so ImportService posts the changed notification even if nothing was written. load_render_config falls back in memory but never persists the corrected value.
- Windows tests: tests/application/services/test_config_service.py:265-290; tests/domain/aggregates/test_config_aggregate.py:638-704
- note: Storage differs by platform (SQLite vs JSON); only the success and failure semantics need to match.

#### 156. SET-03: No-op render-config updates re-save and re-notify, forcing a remesh
- owner: settings-undo-camera | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/application/services/config_service.py:14-24; domain/aggregates/config_aggregate.py:416-428`: update_options returns only the changed keys. When nothing changed, nothing is saved and no APP_CONFIG_UPDATED event is published.
- iOS: `Services/ImportService.mm:520-534; Controllers/DatabaseViewController.mm:609-627; Controllers/ViewController/ViewController.mm:1065-1074`: Every sheet choice, including re-selecting the current mode, calls saveRenderConfig, which writes and posts OSTRenderConfigChangedNotification, and the open ViewController regenerates all meshes and reloads the plan page.
- Windows tests: tests/application/services/test_config_service.py:125-133,149-159
- note: Needs a compare-before-save in ImportService or a Core settings service.

#### 157. CAM-01: Fit/Reset camera framing: distance, pitch and yaw differ from Camera.show_object
- owner: settings-undo-camera | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `cpp_extensions/src/renderer/renderer.cpp:303-334 (Camera::show_object, configure_clip_planes); presentation/components/mesh_view.py:817-826,1504-1538`: Z-up. size = max(bbox extent) (100 if <1). target = bbox centre. position = centre + (0, size, 1.5*size). Distance = 1.80278*size, elevation 56.31 deg, fixed azimuth on the +y (page-bottom) side, fov 45 deg. The bounds also include the 3D page plane (plan texture).
- iOS: `Rendering/Renderer.mm:455-481,536-551; Controllers/ViewController/Components/CameraController.mm:31-39`: Y-up. target = bbox centre. distance = max(2.5*maxExtent, 3.0). yaw 0.5 rad, pitch 0.45 rad (about 25.8 deg), eye = target + dist*(cos p sin yaw, sin p, cos p cos yaw). Because iOS maps z = -ost_y (MeshFactory.cpp:26), +z is the page-top side, while Windows looks from the +y page-bottom side. Orientation is derived from code, so confirm on device.
- Windows tests: tests/integration/native/test_renderer_boundaries.py:30-67; tests/presentation/components/test_mesh_view.py:1638
- note: Values taken from running the Windows native Camera. The near and far planes also differ (iOS: near=max(dist*0.005,0.01), far=max(dist+2R,4R); Windows: near=max(0.1,0.001*size), far=max(10000,20*size)), which is platform-adapted.

#### 158. DLG-RISE-01: Area rise/run are stored and shown signed on iOS; Windows applies abs()
- owner: conditions | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `edit_condition_dialog.py:864-866 (display abs), 1566-1593 (abs(float(...)) for Area/other types; Linear keeps the sign)`: For Area (and Attachment/Count fallthrough branch) rise and run are saved as abs(value) and shown as abs(value); Linear keeps the sign.
- iOS: `EditConditionViewController.mm:331-333, 664-676 (_parsePlainText for Rise/Run, no abs for Area)`: A negative Area rise/run is saved negative; calc_slope_factor then returns 1.0 (UomService.hpp:518-523 requires > 0) so Area quantities lose the slope.
- Windows tests: none found

#### 159. DLG-FINITE-01: Non-finite / overflowing numeric text is accepted on iOS
- owner: parsers-units | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: COND-DIM-02
- Windows: `dimension_format_service.py:92-98,100-165 (_finite_float -> None for nan/inf/overflow); edit_condition_dialog.py:1389-1403,1413-1424 (isfinite checks) and 1509,1544,1556 ...`: 'nan', 'inf', '-inf', 'Infinity', 1e400 and a 5000-digit run all return None / alert 'is not a valid dimension for <label>'.
- iOS: `Controllers/EditConditionViewController/Components/EditConditionHelpers.mm:208-306 (intValue feet saturates at INT_MAX; NSScanner fallback at :299-304); ...`: A 5000-digit feet value becomes 2147483647*12 inches and is accepted; per Apple's NSScanner documentation scanDouble returns HUGE_VAL on overflow, so '1e400' is accepted as +inf; whether the text 'inf'/'nan' is accepted by scanDouble is unverified without a device. No isfinite check anywhere in the dialog.
- Windows tests: tests/domain/services/test_dimension_format_service.py::DimensionFormattingTests::test_dimension_text_rejects_non_finite_values_before_formatting; ::DimensionParsingTableTests::test_overflowing_digit_runs_are_rejected_in_every_layout; ::test_unparseable_text_and_unsupported_fractions_are_rejected (+4 more, see divergences.json)
- note: Whether NSScanner accepts inf/nan is unverified; the overflow cases are certain per documentation. NSScanner acceptance of inf/nan could not be verified without a device; use strtod with full consumption plus isfinite.

#### 160. IMP-14: No OSP package structure validation: exactly one top-level .ost, duplicates, TempImages!.tmp contents
- owner: import-ost-osp | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/importers/osp_importer.py:154-189 (_inspect_package), 219-262`: All member names are normalised to backslash-separated parts. Duplicate or case-insensitive-conflicting members reject. Exactly one top-level member ending in .ost is required (zero, two or only nested ones reject). Any member under TempImages!.tmp (case-insensitive) must have at least 2 parts and a .pdf/.tif/.tiff extension, else reject. ...
- iOS: `Services/ImportService.mm:135-170`: Takes the first entry whose name ends in .ost anywhere (nested folders allowed, additional .ost files ignored). No duplicate check and no TempImages!.tmp rules. Anything with a non-empty list_cab result is accepted.
- Windows tests: tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_requires_exactly_one_top_level_ost_member; tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_rejects_case_insensitive_duplicate_members; tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_rejects_unsupported_members_under_temp_images_root (+3 more, see divergences.json)
- note: Move the package inspection (names to parts, rules) into a Core function that takes the list_cab names.

#### 161. IMP-16: OSP image reference resolution and path rewrite rules differ
- owner: import-ost-osp | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/importers/osp_importer.py:264-308,349-398 (_collect_image_references, _resolve_image_member, _rewrite_image_paths, _is_supported_image_p ...`: Only BidPage elements (anywhere under the root) and only ImagePath/OverlayImagePath values with extension .pdf/.tif/.tiff are considered. A value starting with TempImages!.tmp is looked up exactly (case-insensitive); any other value matches the packaged member with the deepest path-suffix match (flat member 'a.pdf' matches only if reference ends in it; ...
- iOS: `Services/ImportService.mm:187-218,234-270`: Every .pdf entry anywhere is extracted by basename and every non-empty ImagePath/OverlayImagePath of every child of BidPages is rewritten to <dest>/<basename of the original value> regardless of extension, of whether the file exists, or of whether it was in the package. Missing images lose their original path (pointing at a non-existent file); ...
- Windows tests: tests/infrastructure/mdb/importers/test_osp_importer.py::test_image_references_match_the_deepest_member_suffix; tests/infrastructure/mdb/importers/test_osp_importer.py::test_image_paths_are_rewritten_only_for_copied_references; tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_uses_same_flat_lookup_for_original_and_visualizer_paths (+6 more, see divergences.json)
- note: The Windows set of supported extensions is in domain/entities/file_extensions.py:8 (OSP_IMAGE_EXTENSIONS).

#### 162. COND-M07: Placement/mode handling after duplicating or deleting a condition
- owner: conditions | impact: behavior | disposition: fix | testable: objc-only | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/presentation/handlers/condition_action_handler.py:1203-1206,1262-1271; tests/integration/conditions/test_duplicate_placement.py`: After a duplicate, if the 2D takeoff view is active placement is entered for the last new condition (and the sidebar highlights it); outside 2D the current placement is kept and only the highlight changes. After a successful delete placement.force_exit() and ensure_select_mode() always run, whichever condition was deleted.
- iOS: `Controllers/ViewController/Components/ViewController+Conditions.mm:164-187,189-223`: Duplicate only reloads and selects the new row (no placement). Delete leaves place mode unless the deleted uid equals placeConditionUID (then switches to View); deleting any other condition keeps the current mode.
- Windows tests: tests/integration/conditions/test_duplicate_placement.py::test_duplicate_uses_condition_state_projected_before_active_placement; tests/integration/conditions/test_duplicate_placement.py::test_duplicate_outside_2d_view_keeps_placement_and_only_highlights
- note: Mapping Windows' select mode to the iOS EditMode enum is a platform detail; the rules themselves are unambiguous.

#### 163. RPV-M04: Count and attachment symbol orientation ignores page rotation and flips
- owner: rendering-planview | impact: behavior | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `presentation/visualization/pdf/renderers/takeoff_renderer.py:595-615; plan_view/components/zoom_handler.py:339-366`: The count shape is generated in the unrotated scene frame and then the page transform (rotation about the page centre, flips) is applied to the whole item, so on a 90/270 page a w x d rectangle appears d x w, a triangle apex follows the page rotation and flips mirror the shape.
- iOS: `PlanView/PlanView.mm:508-623 (_buildCountPath, _buildMultiSelectionBorder)`: Only the centre point goes through ost_to_pdf_coordinates; the shape is built axis-aligned in canvas space and rotated only by takeoff.rotation, so page rotation and flips never change its orientation or aspect.
- Windows tests: tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py::CountGeometryTests
- note: Becomes automatically correct if RPV-19 is implemented as a view-level transform on the whole canvas; if implemented as coordinate mapping, the shape generator must receive the page transform. Selection borders for count shapes have the same issue.

#### 164. RPV-M07: Pan gesture start point is the location at recognition, not at touch-down
- owner: rendering-planview | impact: behavior | disposition: fix | testable: objc-only | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `plan_view/components/placement_mode.py:1639-1700 (press position); plan_view/components/input_handler.py:838-850 (drag start at press)`: The first point of a linear or rectangle and the drag origin are the exact press position.
- iOS: `PlanView/Components/PlanView+Placement.mm:352-356, 392-396; PlanView/PlanView.mm:937-941`: The start point (and _dragStartCanvasPoint for edit drags) is taken from locationInView: when the UIPanGestureRecognizer reaches Began. UIKit begins a pan only after the touch has moved a recognition threshold, so the start is offset from touch-down by that travel, and edit drags lose that initial movement. ...
- Windows tests: none found
- note: Based on documented UIKit gesture behavior and the cited code; needs device validation. Platform detail rather than a Windows rule, but it changes stored coordinates.

#### 165. DBP-19: insert_takeoff stores empty strings for unassigned area/root parent (Windows NULL/0)
- owner: database-persistence | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/components/takeoff_operations.py:512-533`: BidAreaUID = NULL when unassigned, ParentUID = 0 for roots (or the parent uid), IsNegativeQuantity bool (Access -1), Curve and Rotation as given, Position blob.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Takeoffs.cpp:79-96`: Host run: BidAreaUID TEXT '' and ParentUID TEXT '' for a root (typeof text), IsNegativeQuantity 1/0. iOS reads them back as '0' so it works, but SQL such as ParentUID IS NULL or ParentUID = 0 no longer finds roots.
- Windows tests: tests/infrastructure/mdb/components/test_takeoff_operations.py:1490-1491
- note: Boolean encoding 1 vs Access -1 is DBP-34.

#### 166. EXP-21: Write failure and 'nothing to export' show the same message; Windows distinguishes them
- owner: export-formats | impact: behavior | disposition: fix | testable: objc-only | verdict: CONFIRMED | status: open
- Windows: `application/services/export_service.py:74-148; domain/entities (ExportErrorCode NO_DATA/WORKER_FAILED/UNEXPECTED); exporters/base_exporter.py:85`: An empty selection or hidden-only content gives NO_DATA ('No takeoffs found for any of the selected pages.'). A writer returning False gives WORKER_FAILED ('Export function returned False'); an exception gives UNEXPECTED with the message.
- iOS: `Controllers/ViewController/Components/ViewController+Export.mm:42-53; Services/ExportService.mm:122-123,163-164,192-193`: Every nil return, including ofstream open failure (for example a bad file name, see EXP-09), shows 'Nothing to Export - No visible meshes to export.'
- Windows tests: tests/application/services/test_export_service.py::test_export_failure_result_distinguishes_worker_rejection_and_exception; tests/presentation/visualization/exporters/test_base_exporter.py::test_base_exporter_propagates_explicit_writer_failure
- note: Platform UI messaging, but the conflation hides real failures.

#### 167. EXP-15: HTML export excludes hidden-layer conditions; Windows HTML includes them and lets the viewer toggle them
- owner: export-formats | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: MSH-17, LP-23
- Windows: `application/services/export_service.py:150-153; tests/application/services/test_export_service.py:383-399; renderers/threejs/adapters/threejs_mesh_adapter.py:103-138`: For the HTML strategy collect_takeoffs_for_pages(visible_only=False), so takeoffs of hidden layers are exported. The scene carries layers[{uid,name,visible,sequence}] with the initial visibility, and the viewer hides those meshes via its layer panel. OBJ/DXF/FBX use visible_only=True.
- iOS: `Services/ExportService.mm:52-53 (shared generateVisibleMeshes for HTML)`: Hidden-layer conditions are dropped before meshing, and the iOS viewer has no layer panel.
- Windows tests: tests/application/services/test_export_service.py::test_html_export_collects_hidden_takeoffs_and_passes_layer_metadata; tests/application/services/test_export_service.py::test_non_html_export_keeps_visible_only_collection; tests/domain/services/test_project_data_service.py::test_collect_takeoffs_for_pages_can_include_hidden_layer_takeoffs (+1 more, see divergences.json)
- note: Fidelity needs the layer panel (a new viewer feature) or an explicit decision to keep hiding. Do not guess.

#### 168. BRW-08: iOS deletes non-empty folders (unassigning their bids); Windows only deletes empty projects
- owner: project-browser | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: DBP-38
- Windows: `ost_visualizer/presentation/handlers/project_write_handler.py:931-992 (_delete_projects)`: Projects with bids are skipped. When any selected project has bids: warning 'Cannot Delete Project' / 'Some selected projects still have bids inside.\nOnly empty projects can be deleted.'. The Deleted Bids project is silently skipped. ...
- iOS: `Controllers/DatabaseViewController.mm:202-235; ProjectStore+Folders.cpp:136-170`: Offers 'Delete "<name>"?' with the message '{n} project(s) will become unassigned.' and then unassigns all bids (BidProjectUID=NULL) and deletes the folder.
- Windows tests: tests/presentation/handlers/test_project_write_handler.py::test_delete_projects_skips_the_trash_and_projects_with_bids; tests/presentation/handlers/test_project_write_handler.py::test_delete_projects_confirmation_text_counts_several_projects; tests/presentation/handlers/test_project_write_handler.py::test_delete_projects_stops_on_cancel_flush_failure_and_write_failure (+1 more, see divergences.json)
- note: The Windows writer itself does detach bids (BRW-05); the 'only empty' rule is a handler rule. Windows counts only project_info.bids, so bids that merely have OrigBidProjectUID = this project do not block.

#### 169. BRW-11: Trashing an orphan bid overwrites OrigBidProjectUID with NULL; Windows leaves the stored value alone
- owner: project-browser | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: DBP-21
- Windows: `ost_visualizer/infrastructure/mdb/components/project_operations.py:42-70; presentation/handlers/project_write_handler.py:732-779`: The handler passes orig_project_uid=None for an orphan, and the writer then does not touch OrigBidProjectUID at all, so a previously stored value survives.
- iOS: `Core/Persistence/ProjectStore/ProjectStore.cpp:597-610`: Always writes OrigBidProjectUID: NULL when the bid had no project (bind_null). Host-run: bid 80 (BidProjectUID NULL, OrigBidProjectUID 7) -> (1, NULL).
- Windows tests: tests/infrastructure/mdb/components/test_project_operations.py::test_move_without_a_restore_project_leaves_the_restore_column_alone; tests/presentation/handlers/test_project_write_handler.py::test_unassigned_bids_share_one_trash_group_whatever_their_missing_project_is; tests/infrastructure/mdb/components/test_project_operations.py:312 (test_move_without_a_restore_project_leaves_the_restore_column_alone) (+1 more, see divergences.json)
- note: Windows keeps a possibly stale restore reference (a later Restore would then go to that project). The writer tests pin the behavior, but it may be incidental; the product owner should decide whether to copy it or keep the iOS clearing.

#### 170. COND-EXTRA-01: iOS edits and persists Connect, Tolerance and Snap-to-Linear that Windows never reads or edits
- owner: conditions | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: DLG-EXTRA-01
- Windows: `ost_visualizer/domain/entities/condition.py:11-59; presentation/dialogs/edit_condition_dialog.py:657-797`: The Windows Condition has no connect, connect_tolerance, snap_to_linear, color_line or backout field. Count/Attachment advanced shows only Display Name; Linear shows Round/Drop-Run/Trim/Curved; Area shows Round/Grid/Tile/Gap/Display options. Only Backout is ever written (on type change, from TYPE_DEFAULTS).
- iOS: `EditConditionViewController.mm:209-261,376-421,619-622,658-661; EditConditionViewController+TableView.mm:429-457; Core/Domain/Condition.hpp:27,45-46,56`: Extra rows: 'Connect' + 'Tolerance' for Linear and Count/Attachment, 'Snap to Linear' for Area; the style defaults set connect=true, connect_tolerance=6.0, snap_to_linear=-1; Save writes them (and ColorLine/Backout). Nothing else in iOS reads these fields.
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py::test_count_attachment_advanced_properties_show_only_display_name; tests/presentation/dialogs/test_edit_condition_dialog.py::test_linear_advanced_properties_show_measurement_controls; tests/presentation/dialogs/test_edit_condition_dialog.py::test_area_advanced_properties_show_grid_and_display_controls
- note: Violates 'no new features' but removing existing UI is a product call. Docs (PORTING_MAP.md:39,64) describe these as desktop concepts and the dialog as narrower than desktop.

#### 171. LP-22: Hide All hides layer-less conditions in Windows memory; iOS keeps them visible
- owner: layers-pages | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: AMBIGUOUS | status: open
- aliases: EXP-23
- Windows: `ost_visualizer/domain/services/project_data_service.py:664-675`: update_all_layer_visibility sets Condition.layer_visible for EVERY condition (including ones with no layer or an unknown layer uid) and for every page; a reload recomputes visibility from layers, so such conditions are visible again.
- iOS: `Core/Data/ProjectData.hpp:44-52; ViewController+Layers.mm:102-119`: Visibility is computed per condition from its layer; a blank or unknown layer uid is always visible, so Hide All never hides them.
- Windows tests: tests/domain/services/test_project_data_service.py::test_show_all_layer_visibility_updates_conditions_and_pages_immediately; tests/domain/entities/test_layer.py; tests/domain/services/test_project_data_service.py
- note: Windows is self-inconsistent for layer-less conditions (session-only hide vs always visible on load); real OST conditions normally have a layer so impact is low. Verifier: Windows update_all_layer_visibility sets Condition.layer_visible for EVERY condition (including ones with no or an unknown layer ...

#### 172. QTY-19: Takeoffs with fewer than 2 position values are dropped at load, so count/attachment quantities can differ
- owner: database-persistence | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- aliases: POS-M02
- Windows: `ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:1116-1200 (position at 1124/1167); parse_position at domain/utils/position.py:5-18`: Every takeoff row is kept (empty or unparsable Position gives []). Count/attachment calc 23 still counts 1 for such a takeoff; linear/area give 0. A takeoff pointing at a missing condition aborts the load with an integrity error instead of being dropped.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:264-267`: push_back only when the condition exists and position.size()>=2. parse_position returns {} on any unparsable token, so such takeoffs vanish and do not count. Missing-condition takeoffs are skipped silently.
- Windows tests: tests/domain/services/test_condition_quantity_service.py::ChildKindAndRoundingBoundaryTests::test_a_takeoff_without_geometry_totals_zero
- note: Likely a deliberate defensive filter; only the product owner can say whether a position-less count should show. Overlaps the persistence area.

#### 173. QTY-22: Zero quantities are hidden on iOS but shown as '0 <label>' on Windows
- owner: conditions | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: COND-QTYFMT-01
- Windows: `ost_visualizer/application/utils/quantity_display.py:12-17; conditions_sidebar.py:745-762`: Every condition row gets text for all three slots: value 0 with a non-empty label shows '0 LF', '0 EA'; a missing quantity entry defaults to (0,0,0); an empty label shows ''.
- iOS: `Controllers/ConditionsViewController.mm:46-48,320-337`: formatQty returns nil for 0.0 and the subtitle joins only non-nil parts. Conditions without takeoffs on the page show no quantity at all.
- Windows tests: tests/application/utils/test_quantity_display.py::QuantityDisplayFormattingTests::test_quantity_display_facade_replacement_preserves_exact_formatting; tests/integration/quantities/test_page_deletion_sidebar.py::PageDeletionQuantitySidebarTests::test_conditions_spanning_deleted_and_remaining_pages_recalculate; tests/presentation/components/test_conditions_sidebar.py::ConditionsSidebarZeroQuantityTests::test_zero_remaining_quantity_keeps_the_condition_uom_visible (+2 more, see divergences.json)
- note: Platform layout (3 columns vs one subtitle line) is legitimately different.

#### 174. GEO-14: Area tessellation: earcut is fed a mirrored frame, so the triangle set and diagonals differ from Windows
- owner: meshing | impact: behavior | disposition: fix | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/visualization/core/geometry/area.py:86-112`: The 2D ring handed to earcut is the 3D (X,Y) = (-x, +y) footprint of the bottom vertices (holes appended with hole start indices). Bottom triangles use the earcut order as-is; the top ring is reversed. Verified faces for the square [0,0,4,0,4,4,0,4] thickness 1: bottom (1,0,3),(3,2,1); top (7,4,5),(5,6,7).
- iOS: `Core/Meshing/gen_area.cpp:118-130`: polygon is built from vertices in the iOS (x,z) = (-x,-y) frame, which is the Windows frame mirrored in Y. Earcut ear selection and diagonals depend on orientation, so the same polygon gets different triangles: bottom (2,3,0),(0,1,2) (the other diagonal) for the square; holes and L-shapes also differ. ...
- Windows tests: tests/integration/native/test_geometry_boundaries.py::test_valid_earcut_and_curve_inputs_are_accepted
- note: Vertex arrays are already identical under the (X,Y,Z)->(X,Z,-Y) mapping, so only the earcut input frame and the winding (GEO-15) differ. Geometry (area, volume) is the same; this only affects face lists and ordering in exports and boolean tie-breaks. Overlaps with the meshing mapper.

#### 175. UI-M01: Feet-inch and elevation text fields leave iOS Smart Punctuation on, so typed ' and " can become curly quotes that every parser rejects
- owner: conditions | impact: behavior | disposition: fix | testable: objc-only | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `domain/services/dimension_format_service.py:61-90 (regexes accept only ASCII ' and "); infrastructure/parsers/position_parser.py (ASCII ' and U+2032 only)`: Typed straight quotes always parse; curly quotes are never valid.
- iOS: `Controllers/EditConditionViewController/Components/EditConditionViewController+TableView.mm:149-160 (elevation field), 245-250 and 420-425 (dimension fields); ...`: Fields set autocorrectionType and autocapitalizationType only; no smartQuotesType or smartDashesType is set anywhere (grep). With UIKit default, Smart Punctuation (default setting on) can convert ' and " to U+2019 and U+201D, so '5' 6"' fails displayToInches and shows 'is not a valid elevation/dimension'. Unverified on device.
- Windows tests: none found
- note: Platform-layer input adaptation needed to keep the Windows grammar usable; not verified on a device (no Xcode here).

#### 176. BRW-16: Cyclic page-folder parents silently hide the folders and their pages (Windows refuses the load)
- owner: database-persistence | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:192-198 (require_acyclic_bid_owned_parent_graph)`: Raises RuntimeError 'BidPageFolders.UID=10 participates in a ParentUID cycle' and the hierarchy/bid load fails.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:308-344`: Folders in a cycle never qualify as roots, so they vanish from project.folders, while their pages keep folder_uid set. Host-run: cycle 18<->19, page 203 present in pages with folder_uid '18', but absent from every selector list (not under any folder, not loose).
- Windows tests: tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_page_hierarchy_rejects_folder_parent_cycles
- note: Corrupt-data edge case. Windows is clear that it fails closed; whether iOS should show an error screen is a product choice.

#### 177. BRW-M01: Folder (project) display order is locale-aware/case-insensitive on Windows but SQLite BINARY / std::string < on iOS
- owner: project-browser | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/presentation/components/project_tree_view.py:453-454 (sortByColumn(0, Ascending)) and :641-655 (_create_project_item builds a plain QTreeWidgetItem); ...`: Project nodes are plain QTreeWidgetItem, so the tree orders them with Qt's default item compare, which is locale-aware and case-insensitive at primary level (lowercase before uppercase on ties). Measured with PySide6 6.10.1 / Qt 6.10.1, locale en_US, offscreen, a plain QTreeWidget sorted ascending on column 0. ...
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp:29-31 (ORDER BY Name, SQLite BINARY, no UID tie-break); ...`: Binary byte order, so every uppercase name sorts before every lowercase name ('Zebra' before 'apple'). The mapper judged the folder ordering as 'matches (same as Qt)'. That was wrong for mixed-case, accented and punctuation names. The Windows tests only pin Alpha/Beta/Alpha, which hides the difference.
- Windows tests: tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_full_hierarchy_maps_every_bid_field_and_orders_bids_and_projects (reader order only; no Windows test pins the Qt collation)
- note: Evidence is an empirical Qt run, scripts in scratchpad/verify-project-browser/qsort.py and qcmp.py. The exact order is locale-dependent, so the user must decide how close to match. It also affects the page-selector folder order (BRW-17).

#### 178. CAM-04: Pinch zoom clamp is a fixed 1..500 that the fit distance can exceed; Windows clamps relative to the scene
- owner: settings-undo-camera | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `cpp_extensions/src/renderer/renderer.cpp:291-315 (Camera::zoom, configure_clip_planes)`: new_distance = clamp(distance - delta*distance*0.001, 2*near, 0.5*far), where near = max(0.1, 0.001*size) and far = max(10000, 20*size), so the limits scale with the scene. Native results from distance 100: zoom(+120) gives 88.0; zoom(-120) gives 112.0; zoom(+1e6) gives 0.2; zoom(-1e9) gives 5000.0.
- iOS: `Controllers/ViewController/Components/CameraController.mm:108-121; Rendering/Renderer.mm:474-477`: cameraDistance is clamped to [1.0, 500.0] on every pinch change, while fitCameraToMeshes sets distance = 2.5*maxExtent with no upper bound. For maxExtent > 200 the first pinch event, even with zero delta, snaps the camera inward to 500.
- Windows tests: tests/integration/native/test_renderer_boundaries.py
- note: iOS near and far planes are computed per frame, so the clamp must be redefined together with them; the unit scale of the iOS scene was not verified here. Pinch rate (delta*dist*0.5) is platform-adapted.

#### 179. COND-M08: Editor indexes the style label array with the raw condition type (crash on unsupported types)
- owner: conditions | impact: behavior | disposition: needs-decision | testable: objc-only | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:837,1200-1203; domain/services/condition_quantity_service.py:73-80`: The style combo falls back to its first entry (Linear) for a type not in {0..3}; quantities skip unsupported types with a warning and (0,0,0). The database can therefore carry other type values and the app keeps running.
- iOS: `Controllers/EditConditionViewController/Components/EditConditionViewController+TableView.mm:91-92; EditConditionViewController.mm:209-260`: styleLabels()[static_cast<int>(_condition.condition_type)] is unchecked, so Type outside 0..3 raises NSRangeException when the editor opens; _rebuildAdvRows treats any non-Count/Linear type as Area.
- Windows tests: none found
- note: Windows has no test for the dialog with an unsupported type, so the intended behavior is not pinned; the crash itself is clearly unintended.

#### 180. COND-STYLE-02: Style-change default spacing and shape retention differ
- owner: conditions | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: AMBIGUOUS | status: open
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:90-143,1274-1276,1442-1448,1598-1602`: On a style change the DTO sets spacing = TYPE_DEFAULTS spacing (Linear 4.0, Area 6.0, Count 4.0, Attachment 4.0) and it stays unless the spacing field was edited to a value different from the stored spacing (then the field wins). Shape is reset to Square only when the change is not within {Count, Attachment}; Count <-> Attachment keeps the shape. ...
- iOS: `EditConditionViewController.mm:350,393-420`: spacing = 4.0 for every type (Area should be 6.0); shape forced to Square for Count and Attachment even when switching between them.
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py::test_shared_fields_carry_over_and_type_specific_fields_get_defaults
- note: The 'unchanged spacing is replaced by the type default' rule is a Windows quirk (UI still shows the old text); it is deterministic and tested only indirectly, so flag it if the product intent is unclear. Verifier: Split finding. (a) Shape: CONFIRMED and clear. ...

#### 181. DBP-33: Reference field defaults, enforced relationships and UID-required metadata are not replicated
- owner: database-persistence | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/reference_schema_metadata.py:67-129 (61 defaults, e.g. BidTakeoffs.Count/GridOffsetX/GridOffsetY/GridRotation/IsNegativeQuantity/FontCol ...`: Rows created through Access get the defaults; deletes must respect the enforced relationships.
- iOS: `Core/Persistence/OstSchema.cpp:6-807`: No DEFAULT or REFERENCES clauses (PRAGMA foreign_key_list(BidTakeoffs) is empty). insert_takeoff therefore leaves Count, GridOffsetX/Y, GridRotation, FontColor, NameFontColor NULL (Windows: 0).
- Windows tests: tests/infrastructure/mdb/test_reference_schema_metadata.py; tests/integration/mdb/test_relationship_catalog.py; tests/fixtures/reference_mdb_schema.json
- note: Matters when iOS-created rows are exported back to OST (blocked by B-009).

#### 182. EXP-17: OBJ vertex order, triangulation diagonals and winding conventions differ from Windows for identical geometry
- owner: meshing | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `presentation/visualization/core/geometry (Z-up generators) with exporters/adapters/obj_writer.py:32-43`: Vertex order is the Z-up generator order. Wall [0,0,100,0], h96, t6 OBJ vertices: (-0,0,3),(0,0,-3),(-100,0,-3),(-100,0,3),(-0,96,3),(0,96,-3),(-100,96,-3),(-100,96,3); faces '1 3 2','1 4 3','5 6 7','5 7 8','1 8 4','1 5 8','2 5 1','2 6 5','3 6 2','3 7 6','4 7 3','4 8 7'. Column and circle start points also differ in direction.
- iOS: `Core/Meshing/gen_linear.cpp:20-40; Core/Meshing/mesh_helpers.hpp:36-52 (same face index list as Windows, but vertices relabelled by the sign of the perpendicular offset)`: Same positions, different order: wall vertices (-0,0,-3),(0,0,3),(-100,0,3),(-100,0,-3),... faces '1 2 3','1 3 4','5 7 6',... The circle winds the opposite way around the centre. The triangle sets differ even for the straight wall (triangulation diagonals), so no shape produced identical OBJ lines in a triangle-set comparison.
- Windows tests: tests/presentation/visualization/meshing/test_mesh_factory.py
- note: Byte parity requires reworking the iOS generators' vertex labelling to Windows space plus either the face swap or a changed writer. That is a meshing-area decision: accept geometric equivalence or demand identical file text. ...

#### 183. EXP-18: Mesh/takeoff ordering and page-selection order differ
- owner: export-formats | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `domain/services/project_data_service.py:197-216; domain/services/page_selection_service.py:64-76; domain/services/takeoff_domain_service.py:50-86; ...`: The selection is an ordered list (the user's order). Takeoffs are collected page by page in that order, each page in its own takeoff order. OBJ/DXF then put non-area takeoffs before area parents and group by condition (EXP-11). The HTML geometry order is area, then linear, then count, then attachment (type order [1,0,2,3]); within a type, input order.
- iOS: `Services/ExportService.mm:43-44; Services/ProjectService.mm:20,73-78; Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:241-266`: The page selection is std::set<std::string> (sorted by uid, user order lost). Takeoffs are traversed in global project.takeoffs load order (SQLite row order with no ORDER BY) and filtered by set membership. Meshes are exported in that order for every format, with no type ordering.
- Windows tests: tests/presentation/visualization/renderers/threejs/test_mesh_processor.py::test_meshes_are_emitted_area_then_linear_then_count_then_attachment; tests/domain/services/test_project_data_service.py::test_collecting_takeoffs_skips_pages_whose_takeoffs_are_all_hidden
- note: The iOS selection model is a set, so Windows' selection order cannot be reproduced without changing the UI model. That is a product decision; the type and area ordering alone can be fixed.

#### 184. EXP-20: Three.js scene payload structure, precision, normals and defaults differ from the Windows scene
- owner: export-formats | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `renderers/threejs/adapters/threejs_mesh_adapter.py:31-100,160-200; presentation/visualization/utils/mesh.py:115-200; renderers/threejs/mesh_processor.py:47-100; ...`: One geometry per mesh: vertices/normals/indices/color [r,g,b]/opacity/name/visible/takeoff_uid/page_uid/condition_uid/area_uid/layer_uid/cdn_type_uid/cdn_type_name/hover/hover_bottom/hover_top. Vertices are split per 60-degree crease group (a box gives 24 vertices, a cylinder 98, a wall 24) with per-group smooth normals. ...
- iOS: `Core/Export/threejs_writer.cpp:50-168; Services/ExportService.mm:178-184`: Per geometry only {name, vertices, normals, indices, color, opacity}. The vertices are the original mesh vertices (a box has 8), with area-weighted per-vertex normals and no crease splitting. Names are '<ConditionName>_<i>'. Floats are printed with %.4f (colour 0.5020). Opacity is always 1.0. The top level has only {camera, geometries}. ...
- Windows tests: tests/presentation/visualization/renderers/threejs/adapters/test_threejs_mesh_adapter.py::test_threejs_adapter_convert_mesh_splits_creased_vertices_and_defaults_metadata; tests/presentation/visualization/renderers/threejs/adapters/test_threejs_mesh_adapter.py::test_threejs_adapter_exports_layer_condition_and_area_metadata; tests/presentation/visualization/renderers/threejs/test_mesh_processor.py
- note: Only needed if the iOS viewer adopts the Windows scene schema or native shading. Clear sub-items: crease-based normals (needed by native shading), full-precision numbers, opacity, name semantics. Fields serving Windows-only viewer features (hover, layers, areas, plan) belong in windowsOnly.

#### 185. EXP-22: Edge cases: negative-only and fully-negated exports
- owner: export-formats | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: AMBIGUOUS | status: open
- Windows: `exporters/base_exporter.py:56-60,77-85,151-170; module_geometry.cpp:73-94; renderers/threejs/threejs_renderer.py:79-80`: (a) Takeoffs that exist but yield no positive mesh (for example only a negative takeoff): OBJ and DXF report success and write a file with empty groups and an MTL entry (DXF extents default 0..10); HTML returns False and writes nothing. ...
- iOS: `Services/ExportService.mm:81-89,104-106`: (a) Any empty mesh list returns nil, shown as 'Nothing to Export', and no file is written. (b) On nullopt the original positive is kept for all formats, which matches Windows OBJ/DXF but not Windows HTML.
- Windows tests: tests/presentation/visualization/exporters/test_base_exporter.py::test_base_exporter_returns_false_without_writing_when_nothing_is_exportable; tests/presentation/visualization/renderers/threejs/test_threejs_renderer.py::test_empty_mesh_result_returns_none_without_writing_html
- note: Windows is self-contradictory between formats and arguably buggy (a fully negated element still exported; success on an empty file). Do not guess; ask the user which behaviour iOS should have. Verifier: Windows is self-contradictory between formats. ...

#### 186. EXP-M02: HTML scene winding and normal sign: Windows sends inward-facing meshes and inward normals, and the viewer lights with the raw normal
- owner: export-formats | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `renderers/threejs/adapters/threejs_mesh_adapter.py:160-178; utils/mesh.py:115-200 (prepare_vertices_for_shading); ...`: Measured on real Windows HTML scenes: signed volume of wall -57600, column -13824, circle -21467.487, slab -57600, and mean(outward . normal) is negative for all of them, so normals point inward. When the export contains any negative takeoff, apply_boolean_operations flips every returned positive, including ones untouched because the negative has another con ...
- iOS: `Core/Export/threejs_writer.cpp:109-144`: Normals are area-weighted sums of the iOS face winding: boxes are outward (positive volume), cylinders and curved walls inward (EXP-13). The iOS Phong flatShading material does not use the attribute, so there is no visible effect today.
- Windows tests: tests/presentation/visualization/renderers/threejs/adapters/test_threejs_mesh_adapter.py::test_threejs_adapter_convert_mesh_splits_creased_vertices_and_defaults_metadata
- note: Only matters if the Windows shader or normal data is adopted (EXP-19 and EXP-20 decision). Windows' flip-when-negatives-exist behavior is itself inconsistent, so do not guess which sign convention to reproduce.

#### 187. GEO-15: Face winding sense is opposite to Windows for every area mesh (outward on iOS, inward on Windows)
- owner: meshing | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: AMBIGUOUS | status: open
- Windows: `ost_visualizer/presentation/visualization/core/geometry/area.py:69-135; cpp_extensions/src/boolean/boolean_ops.cpp:148-227 (fix_face_winding flips negative-volume meshes)`: Under the right-handed (-x, y, z) frame Windows area meshes have a NEGATIVE signed volume (4x4x1 square: -16; with hole: -15; L-shape -12; two holes -87). Windows linear (-200 for 10x2x10) and count (-48 for 4x4x3) meshes are inward as well. The boolean module re-orients meshes before operating.
- iOS: `Core/Meshing/gen_area.cpp:160-201`: Same face construction rule, but evaluated in the iOS Y-up (x,z) frame, where the wall rule is chosen with calc_signed_area (sign flipped relative to the Windows frame) and the triangulation orientation differs. Result: every area face normal is the exact opposite of the Windows face after mapping (X,Z,-Y), signed volume +16, +15, +12, +87 (outward). ...
- Windows tests: tests/integration/rendering/test_page_flip_mesh.py::test_flip_keeps_top_face_winding_direction_of_unflipped_mesh
- note: The negative-volume outcome on Windows looks like a side effect of negating X without reversing winding, not necessarily intent. Windows renderers and exporters evidently tolerate it. A project-wide decision belongs to the meshing mapper; flagged here because the area/earcut winding is in scope. ...

#### 188. IMP-15: Windows-filesystem member-name safety rules not applied (absolute/rooted, ':', device names, trailing dot/space)
- owner: import-ost-osp | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: AMBIGUOUS | status: open
- Windows: `ost_visualizer/infrastructure/mdb/importers/osp_importer.py:81-101 (_is_windows_device_name, _is_safe_cab_member_name), 126-133`: Rejects any member that is absolute, has a drive or root, contains '..' escape, has ':' in any part, has a part ending in '.' or space, or has a Windows reserved device name (CON, PRN, AUX, NUL, CONIN$, CONOUT$, COM1-9/LPT1-9 incl. superscripts) as the base name of any part, before extraction.
- iOS: `Services/ImportService.mm:146-218`: No such rules. iOS never writes member paths (only basenames), so there is no traversal risk, but archives Windows rejects are accepted.
- Windows tests: tests/infrastructure/mdb/importers/test_osp_importer.py::test_only_non_empty_relative_cab_member_names_are_safe; tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_rejects_unsafe_cab_member_paths_before_extraction; tests/infrastructure/mdb/importers/test_osp_importer.py::test_reserved_device_names_are_rejected_in_every_component (+2 more, see divergences.json)
- note: These rules exist to protect a Windows filesystem extraction. The decision for the user is whether an iOS build should reject the same archives (strict parity) or accept names that cannot hurt it. I did not guess. ...

#### 189. IMP-17: CAB compression types other than stored and MSZIP are unsupported
- owner: import-ost-osp | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: AMBIGUOUS | status: open
- Windows: `cpp_extensions/src/cab/cab_compressor.cpp:380-423,495-520 (FDICopy)`: The Windows cabinet library extracts every cabinet compression type it supports (stored, MSZIP, Quantum, LZX). Windows' own exporter writes MSZIP (FCI, line 484); the legacy OST app's CABs in the tests are stored.
- iOS: `Core/Import/CabExtractor.cpp:249-252`: Any other typeCompress breaks out of decompress_folder with no data, so the OST extraction reports 'Failed to extract OST from package' and PDFs are silently skipped (ImportService.mm:204-216).
- Windows tests: none found
- note: Whether real OSP files use LZX is unknown; implementing an LZX decoder would be new code, so I did not mark it fix. Needs an answer on whether LZX-compressed OSPs matter. Verifier: iOS CabExtractor.cpp:249-252 supports only stored and MSZIP; ...

#### 190. IMP-18: CAB member names are not decoded (legacy ANSI code page vs UTF-8 flag)
- owner: import-ost-osp | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: AMBIGUOUS | status: open
- Windows: `cpp_extensions/src/cab/cab_compressor.cpp:83-89 (decode_cab_member), 380-423`: Names are decoded as UTF-8 when the CFFILE attribute _A_NAME_IS_UTF (0x80) is set, otherwise with the system ANSI code page (CP_ACP) and re-encoded as UTF-8. Decode failure aborts the listing/extraction.
- iOS: `Core/Import/CabExtractor.cpp:144-157; Services/ImportService.mm:172-185,206-215`: Name bytes are used as is (backslash converted to slash), and the attribute flag is ignored. A legacy ANSI name containing a byte like 0x96 (en dash in cp1252) is invalid UTF-8, so [NSString stringWithUTF8String:] on the destination dir/file path returns nil. ...
- Windows tests: tests/integration/native/test_osp_cab.py::test_osp_imports_legacy_ansi_members_from_unicode_archive_path; tests/integration/native/test_osp_cab.py::test_native_cab_round_trip_preserves_utf8_paths_and_member_names
- note: CP_ACP is machine-dependent on Windows (the tests assume cp1252). The decision is which fixed code page iOS should assume for non-UTF-flagged names; cp1252 matches the Windows fixtures. ...

#### 191. IMP-19: iOS rejects re-import of an already-imported Bid; Windows always imports an independent copy
- owner: import-ost-osp | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/importers/ost_importer.py:84-99; import_operations.py:33-82`: There is no duplicate check. Every import allocates new UIDs and a new BidNo, so importing the same file twice yields two bids.
- iOS: `Services/ImportService.mm:68-83,289-304; Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:248-262`: If Bids.UID equals the file's Bid UID the import fails with 'This project is already imported'. This also blocks two different projects that happen to carry the same Bid UID, and it only guards the Bid row, not page/condition/takeoff UIDs (see IMP-06). For OSP it runs after the PDFs have already been written.
- Windows tests: tests/infrastructure/mdb/importers/test_ost_importer.py::test_import_ost_reports_exact_booleans_for_writer_results
- note: The guard is an iOS addition. Removing it is the Windows behavior, but it changes user-visible messaging, so I left the call to the user.

#### 192. IMP-20: DATETIME and binary column values stored as raw text
- owner: import-ost-osp | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/infrastructure/mdb/components/import_operations.py:594-615`: DATETIME columns (14, e.g. Bids.CreateDateTime, ModDateTime, BidDate, CopyTimeStamp): a value with exactly 6 whitespace-separated integers 'Y M D h m s' becomes a datetime; anything else (blank, ISO, 3 parts, month 13, non-numeric) becomes NULL. Memo/IMAGE columns (30, e.g. ...
- iOS: `Core/Persistence/ProjectStore/ProjectStore_Internal.hpp:76-88`: Everything is stored as TEXT, so '2026 7 19 0 39 2' stays that string and invalid dates are kept instead of NULL. Binary columns hold UTF-8 text. Load reads bid_date/copy_timestamp as strings, and Settings.Created is written by create_schema as 'YYYY-MM-DD HH:MM:SS', so the DB mixes two date formats.
- Windows tests: tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_value_conversion_follows_access_column_types; tests/infrastructure/mdb/components/test_import_operations.py::test_datetime_text_needs_exactly_six_parts_otherwise_it_reads_as_null; tests/infrastructure/mdb/components/test_import_operations.py::test_only_annotation_text_tables_use_the_latin1_name_encoding
- note: The iOS schema has no datetime type, so which text format to store (raw 6-part string vs an ISO string like Settings.Created) is a product call that also affects any future OST export.

#### 193. IMP-M03: CAB extraction re-inflates and holds the entire folder in memory for every member
- owner: import-ost-osp | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `cpp_extensions/src/cab/cab_compressor.cpp:495-520 (FDICopy extract_cab, single streaming pass to files)`: One pass over the cabinet extracts every member straight to disk; memory use does not grow with package size.
- iOS: `Core/Import/CabExtractor.cpp:166-258,273-318; Services/ImportService.mm:187-221`: extract_cab_file calls decompress_folder, which inflates the whole folder into a std::vector, and importOSPFile calls it once per PDF plus once for the OST. For an OSP with N PDFs in one folder the whole folder (all drawings) is decompressed N+1 times and held in RAM each time. Code reading only (not run); ...
- Windows tests: none found
- note: Not a Windows-parity rule; flagged because large OSPs (hundreds of MB of drawings) can exhaust iOS memory. Needs a decision on whether to stream per member or per folder.

#### 194. MSH-16: Export boolean rules differ per Windows format; iOS uses one rule for OBJ, DXF and Three.js
- owner: export-formats | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: AMBIGUOUS | status: open
- Windows: `exporters/base_exporter.py:151-172,200-210 (OBJ/DXF/FBX; mesh metadata has only IsNegativeQuantity/takeoff_uid, no condition_type); ...`: OBJ/DXF/FBX: condition_type defaults to 0 for every mesh, so every negative subtracts from every positive across types; a positive consumed entirely is absent from processed_mesh_cache and is therefore regenerated and exported unsubtracted; negatives are not written. ...
- iOS: `Services/ExportService.mm:17-92 (one function for all three formats): all negatives applied to all positives, consumed positive (nullopt) kept unsubtracted (:83-87)`: Matches the OBJ/DXF quirk, but Three.js export uses the same all-types rule and keeps consumed positives, which differs from Windows HTML (same-type only, consumed dropped).
- Windows tests: none found
- note: Windows is self-contradictory between exporters (the OBJ/DXF behaviour looks accidental because condition_type is missing from the export metadata), so I did not pick one. Verifier: Both Windows halves reproduced: with no condition_type key (OBJ/DXF metadata) the half-cut positive is reduced to |vol ...

#### 195. MSH-19b: Drop-stub union failure: Windows drops the whole takeoff mesh, iOS keeps the plain wall
- owner: meshing | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `meshing/mesh_factory.py:74-89,122-140 (return boolean_union(mesh, stub, mesh.metadata)); core/boolean_operations.py:62-73; cpp_extensions/src/module_geometry.cpp:119-130`: If boolean_union returns None or raises, the factory returns None and the whole takeoff mesh is dropped; the union result passes remove_degenerate_faces before use.
- iOS: `Core/Meshing/MeshFactory.cpp:61-73,80-92`: If the union has no value, the plain wall without the stub is returned; no remove_degenerate_faces on the union output.
- Windows tests: tests/presentation/visualization/core/test_boolean_operations.py::test_boolean_union_returns_none_when_native_returns_none; ...::test_boolean_union_logs_and_returns_none_when_native_raises
- note: Split from MSH-19 by the verifier: Windows returns None when boolean_union fails or raises (mesh_factory.py:88,139), which looks accidental. Product intent unclear.

#### 196. QTY-18: Parent token 'None' counts as a root takeoff in Windows quantities but as a hole in iOS (Windows is internally inconsistent)
- owner: quantities | impact: behavior | disposition: needs-decision | testable: core-cpp | verdict: AMBIGUOUS | status: open
- Windows: `ost_visualizer/domain/services/condition_quantity_service.py:62,88-91 vs ost_visualizer/domain/entities/takeoff.py:7,135-137`: compute_page_quantities treats parent_uid in ('', '0', 'None') as root, but Takeoff.is_hole (and the loader contract checks) treat only '' and '0' as root, so 'None' is a hole elsewhere. Quantity tests pin the 'None' = root behavior.
- iOS: `Core/Domain/Takeoff.hpp:22-25; Core/Domain/UomService.hpp:745-749,767`: is_hole() = non-empty and != '0', so a takeoff with parent 'None' is skipped as a hole and its quantity is lost; 'None' parents also become hole owners. Verified: Windows 198 vs iOS 197 for backout test; sentinel test Windows 212 vs iOS 204.
- Windows tests: tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_backouts_are_subtracted_only_from_their_parent_and_never_counted_as_primaries; tests/domain/services/test_condition_quantity_service.py::ConditionQuantityRootSentinelTests
- note: A literal 'None' string can only arrive from foreign data (the iOS import writes NULL/'0'). Decide whether quantity code should use the 3-token root set while Takeoff::is_hole keeps the 2-token set. ...

#### 197. RPV-22: Curved linear takeoffs: 2D draws a straight chord, placement never produces a curve, and there is no centre handle
- owner: rendering-planview | impact: behavior | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `takeoff_renderer.py:540-593; selection_manager.py:645-672; drag_handler.py:331-345, 363-398; placement_mode.py:1832-1850; plan_view_action_handler.py:2565-2585`: curve >= 0 and >= 6 values draws an arc band through the control point (proc_curved_pos, gen_curve_pts); selecting shows 3 handles (ends plus control); end drags keep the control point at the same perpendicular offset; placing a condition with is_curved_segment emits a 6-value position [x1,y1,x2,y2,mx,my] and the takeoff is created with Curve = 0.
- iOS: `PlanView/PlanView.mm:253-256 (positions truncated to 4 values); PlanView+Placement.mm:376-378; DragController.mm`: 2D shows a straight band; placement always emits 4 values with curve -1; no control handle. The 3D mesh does draw the curve.
- Windows tests: tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py::test_the_curve_flag_and_value_count_decide_between_arc_and_straight; ::test_a_curved_linear_is_an_arc_band_through_the_control_point
- note: Needs the Windows ost_linear_geom arc code (and the 3D curve code in gen_curved.cpp should be checked against it by the meshing mapper). It is a decision whether curved 2D rendering and creation are in the iOS scope.

## (D) UI / cosmetic

19 items, global rank order. Items with disposition `needs-decision` also appear in section E; `cannot-reproduce` in section F.

#### 198. QTY-23: Conditions list labels: iOS strips elevation suffixes from names and renames the unknown type group
- owner: conditions | impact: ui-cosmetic | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: COND-NAME-01, UI-NAME-01
- Windows: `ost_visualizer/presentation/components/conditions_sidebar.py:652-670,687-693; domain/services/elevation.py:121-131`: Sidebar shows condition.name verbatim. strip_elevation_suffix is used only for CSV/hover text and only strips explicit ' @T'/' @B' suffixes whose text is strippable. A cdn type uid with no entry in CdnTypes shows condition.cdn_type_name ('Unknown'); no uid shows '(unassigned)'.
- iOS: `Controllers/ConditionsViewController.mm:8-28,315-317,170-181`: stripElevation() removes ' @T', ' @B', ' @ ' and ' @' suffixes from the row title without the 'strippable text' check, so names like 'Wall @ Level 2' display as 'Wall'. An unknown cdn type uid shows '(unassigned)' instead of 'Unknown'.
- Windows tests: tests/domain/services/test_elevation.py; tests/domain/services/test_elevation_text.py; tests/presentation/components/test_conditions_sidebar.py::test_condition_sidebar_cdn_type_group_rows_are_bold
- note: May be an intentional space-saving choice for the phone layout.

#### 199. COND-SHAPE-01: Shape picker order differs (Diamond before Ellipse)
- owner: conditions | impact: ui-cosmetic | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- aliases: UI-SHAPE-ORDER-01
- Windows: `ost_visualizer/presentation/dialogs/edit_condition_dialog.py:68-77`: Order: Square, Circle, Triangle, Rectangle, Ellipse(13), Diamond(12), Hexagon, Octagon.
- iOS: `EditConditionHelpers.mm:21-29`: Order: Square, Circle, Triangle, Rectangle, Diamond(12), Ellipse(13), Hexagon, Octagon (ids 0,1,2,3,12,13,14,15).
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py

#### 200. BRW-21: Name fallbacks for empty folder/bid names differ ('Untitled Folder'/'Untitled Bid' vs 'Project {uid}'/'Bid {uid}')
- owner: project-browser | impact: ui-cosmetic | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/domain/entities/project_factory.py:20-28,44-52; presentation/components/project_tree_view.py:668`: An empty project name displays as f'Project {project_uid}'; an empty bid name as f'Bid {bid.uid}'.
- iOS: `Controllers/DatabaseViewController.mm:332,474-475`: Empty bid name -> 'Untitled Bid'; empty folder name -> 'Untitled Folder'.
- Windows tests: tests/domain/entities/test_project_factory.py::ProjectFactoryHierarchyTests::test_build_projects_keeps_file_order_names_paths_and_adds_one_unassigned_group; tests/domain/entities/test_project_factory.py::ProjectFactoryHierarchyTests::test_build_loaded_files_keeps_each_files_projects_and_orphans_separate

#### 201. CAM-03: Reset with no renderable content moves the camera on iOS; Windows is a no-op
- owner: settings-undo-camera | impact: ui-cosmetic | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/components/mesh_view.py:817-826 (reset_view returns when no bounds)`: reset_view with no scene or plan leaves the camera untouched (and the navigation action is disabled).
- iOS: `Controllers/ViewController/Components/CameraController.mm:31-39`: fitScene sets yaw=0.5, pitch=0.45 and, when there are no meshes, distance=12 while leaving the pan target where it was.
- Windows tests: tests/presentation/components/test_mesh_view.py:1648
- note: Cosmetic.

#### 202. COND-CDNPICK-01: Condition-type picker sorts case-sensitively; Windows sorts case-insensitively
- owner: conditions | impact: ui-cosmetic | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/dialogs/condition_types_dialog.py:152; presentation/components/conditions_sidebar.py:544-547`: Types listed sorted by name.lower() (stable).
- iOS: `Controllers/EditConditionViewController/EditConditionViewController.mm:107-112`: std::sort by raw name (uppercase before lowercase, non-stable for equal names).
- Windows tests: tests/presentation/dialogs/test_edit_condition_dialog.py
- note: The sidebar's type-GROUP order is case-sensitive on Windows too and iOS matches that.

#### 203. GEO-18: Curved linear takeoffs are drawn as straight chords in the 2D overlay
- owner: rendering-planview | impact: ui-cosmetic | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/visualization/core/geometry/takeoff_geometry.py:37-55,142-165`: For linear takeoffs with curve>=0 and >=6 values, the footprint is built from the curved centreline (proc_curved_pos, gen_curve_pts with 24 segments, inner/outer offsets by thickness) rather than a straight chord.
- iOS: `PlanView/PlanView.mm:250-256 (linearPos = first 4 values only)`: Curved linear takeoffs (curve>=0) are drawn and hit-tested as the straight chord p1-p2; the control point is ignored. Only the 3D mesh uses the curve (MeshFactory.cpp:37-75).
- Windows tests: tests/integration/native/test_geometry_boundaries.py::NativeLinearAxisAlignedArcTests
- note: Requires the curve generator, which belongs to the meshing mapper (Core/Meshing/gen_curved.cpp). Low severity because the data is untouched; it affects visual placement.

#### 204. LP-25: Page button label fallback for unnamed pages
- owner: layers-pages | impact: ui-cosmetic | disposition: fix | testable: objc-only | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/components/page_combo.py:19-29`: Label = [sequence][sheet no][name] joined by ' - ' (sequence/sheet shown only with the preferences, both off by default); if no part exists the label is the page uid.
- iOS: `ViewController+PageSelector.mm:134-148,178-189`: Label is always the page name; an empty name gives an empty, tiny button.
- Windows tests: tests/presentation/components/test_page_combo.py::test_page_label_missing_sequence_does_not_show_duplicate_one; ::test_page_label_index_uses_sequence_not_pdf_page_index

#### 205. RPV-27: Negative marker anchor is the vertex mean; Windows uses the area-weighted path centroid
- owner: rendering-planview | impact: ui-cosmetic | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `takeoff_renderer.py:390-437, 464-483`: Marker centre = polygon area centroid of the item path (MoveTo/LineTo points), falling back to the nearest interior anchor when the centroid is outside the shape, or the vertex mean for zero area.
- iOS: `PlanView/PlanView.mm:346-355`: Arithmetic mean of the transformed vertices (for count shapes, the position point).
- Windows tests: tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py::test_area_with_hole_uses_anchor_inside_visible_fill; ::test_the_marker_is_drawn_over_a_negative_takeoff
- note: Linear and symmetric count shapes agree.

#### 206. RPV-29: 3D lighting constants and two-sided normal flip differ
- owner: rendering-planview | impact: ui-cosmetic | disposition: fix | testable: objc-only | verdict: CONFIRMED | status: open
- Windows: `cpp_extensions/src/renderer/renderer.cpp:597-626, 1254-1263`: ambient 0.55; key 0.45 along (500,-500,500); fill 0.35 along (-500,500,500); rim 0.2 along (0,500,-500) (Z-up); max(dot,0) per light; no view-dependent normal flip; no face culling.
- iOS: `Rendering/Renderer.mm:553; Rendering/Shaders/src/fs_mesh.sc:9-25 and fs_oit_accum.sc:9-25`: ambient 0.20, key 0.60, fill 0.30, rim 0.20 (old desktop values), directions remapped to Y-up, and the normal is flipped towards the viewer when dot(N,V) < 0.
- Windows tests: none found
- note: The Windows rim and key directions were only checked for consistency with an (x,y,z)->(x,z,-y) mapping. Removing the normal flip is part of parity; confirm it is not needed on Metal.

#### 207. RPV-32: Handle and stroke visuals differ from Windows (and scale with zoom)
- owner: rendering-planview | impact: ui-cosmetic | disposition: fix | testable: objc-only | verdict: CONFIRMED | status: open
- Windows: `plan_view/components/handle_style.py:1-25; selection_manager.py:340-352; placement_mode.py:822-831`: White handles (alpha 224) with a black 1 px outline, ItemIgnoresTransformations (constant screen size). Takeoff outlines use a 2.0 cosmetic pen. Rect-drag mid handles use 2.5 half-size in the placement preview.
- iOS: `PlanView/Components/TakeoffOverlayView.mm:16-43, 75-133; PlanView/PlanView.mm:11, 553`: Blue handles with white outline (orange when active), drawn in canvas units so they grow and shrink with zoom; stroke 1.5 in canvas units; negative markers alone use 1/zoom sizing.
- Windows tests: tests/presentation/components/plan_view/components/test_handle_style.py
- note: Orange 'active handle' is an iOS-only touch affordance (tap then drag) and should stay.

#### 208. UI-MSG-01: Validation alert wording differs
- owner: conditions | impact: ui-cosmetic | disposition: fix | testable: objc-only | verdict: CONFIRMED | status: open
- Windows: `edit_condition_dialog.py:1389-1403,1413-1424,1433-1456,1506-1560`: '"x" is not a valid dimension for Height.'; '"x" is not a valid elevation.'; 'Condition number must be a positive whole number.'; '"x" is not a valid number for Display Size.'; 'Rise must be a number.' / 'Rise must be a finite number.'.
- iOS: `EditConditionViewController.mm:499-544,561-575`: '"x" is not a valid <Label>.' for every field including Rise/Run/Display Size; no message for ref no.
- Windows tests: none found
- note: Wording only; include with DLG-FINITE-01/DLG-REF-01 when those are fixed.

#### 209. UND-10: Selection after undo of a delete: Windows selects the restored takeoffs, iOS clears the selection
- owner: settings-undo-camera | impact: ui-cosmetic | disposition: fix | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/handlers/plan_view_action_handler.py:3870-3871 (_undo_delete selects restored uids when the selection owner is current)`: Undo of a delete selects the restored takeoffs; redo of the delete clears the selection.
- iOS: `Controllers/ViewController/Components/UndoRedoController.mm:56-60`: Undo of a Delete calls undoRedoClearSelection and leaves nothing selected. Redo of delete clears selection (matches).
- Windows tests: tests/presentation/handlers/test_plan_view_action_handler.py:3811
- note: Cosmetic.

#### 210. CAM-02: Camera refits on every empty-to-non-empty page selection; Windows preserves camera within a bid
- owner: settings-undo-camera | impact: ui-cosmetic | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: CONFIRMED | status: open
- Windows: `ost_visualizer/presentation/components/mesh_view.py:1199-1300 (same-bid refresh keeps the camera), :1433-1446`: The camera is fitted once per bid scene (first renderable content) and kept across same-bid scene updates, including after an empty selection. An explicit Reset is the only other refit. A per-bid saved camera is also restored when re-entering a bid (listed under windowsOnly).
- iOS: `Controllers/ViewController/Components/ViewController+PageSelector.mm:271-296; Controllers/ViewController/ViewController.mm:1049-1059`: fitCameraToMeshes runs on project load, and again on every transition from no selected 3D page to some selected 3D page, discarding the user's pose.
- Windows tests: tests/presentation/components/test_mesh_view.py:651,749,941
- note: iOS added the transition refit deliberately (PORTING_LOG 2026-03-24).

#### 211. DBP-34: Boolean columns are written as 1 by iOS but as bool (Access -1) by Windows
- owner: database-persistence | impact: ui-cosmetic | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `layer_operations.py:44,106 (Show -1/0), page_operations.py:479 (_access_bool), condition_operations.py:650-651 (Backout -1/0, others bool), takeoff_operations.py:532 (IsN ...`: Access YESNO true is stored as -1.
- iOS: `ProjectStore+Conditions.cpp:49-66 (DisplayName, DisplayDimension, RoundQuantity, Connect, Trim, IsCurvedSegment, Grid, DisplayGridWhileDrawing, DropRun written as 1/0; ...`: Mixed encoding inside one table (Backout -1, Grid 1). All iOS and Windows readers treat any nonzero as true, so only a future OST export would care.
- Windows tests: tests/infrastructure/mdb/components/test_layer_operations.py; tests/infrastructure/mdb/components/test_takeoff_operations.py:1490
- note: Low risk.

#### 212. DBP-M05: Per-page selected area (BidPageSettings.BidAreaSelected) is not loaded, so Windows' greyed-out inactive-area takeoffs have no iOS counterpart
- owner: database-persistence | impact: ui-cosmetic | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED (verifier-added) | status: open
- Windows: `ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:772-808 (_parse_page_area_selections_for_bid), presentation/visualization/services/color_service.py:211-22 ...`: Reads BidPageSettings rows with BidAreaSelected > 0 ordered by BidPageUID, BidAreaSelected DESC, UID DESC (first row per page wins) into page -> area uid (None when none). In 2D and 3D colouring, a takeoff whose area_uid differs from the page's selected area uses inactive_object_color.
- iOS: `Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp (no BidPageSettings/BidAreas query); Core/Data/ProjectData.hpp`: area_uid is loaded on each takeoff but the page selection is never read, so takeoffs of unselected areas keep their condition colour. The mapper listed this only under Windows-only features.
- Windows tests: tests/infrastructure/mdb/components/test_bid_data_reader.py (page area selection tests); tests/presentation/visualization (colour service tests)
- note: Affects colour only, never geometry or quantities. Included so the gap is tracked; ask the user whether it is in scope.

#### 213. EXP-19: Three.js viewer shading, colour management and lighting differ from the Windows viewer (explicitly pinned by Windows tests)
- owner: export-formats | impact: ui-cosmetic | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `renderers/threejs/templates/viewer.html:749-750,764-770,803-814,1985-1991,2330-2370; tests/presentation/visualization/renderers/threejs/test_threejs_renderer.py:638-671`: three@0.150.0. Colour management is disabled and the renderer uses LinearEncoding and NoToneMapping. Shading is a custom native-matching ShaderMaterial with ambient 0.55, key dir [500,500,500] at 0.45, fill [-500,500,-500] at 0.35, rim [0,-500,-500] at 0.2 (lighting*uColor, DoubleSide, polygonOffset). Weighted blended OIT for transparent meshes. ...
- iOS: `Core/Export/threejs_writer.cpp:170-250 (lines 192, 203-209, 221-224, 232-236)`: three@0.152.0. sRGB output with ACESFilmic tone mapping, exposure 0.25, AmbientLight 0.35, DirectionalLights 1.2/0.6/0.4 at (500,500,-500),(-500,500,500),(0,-500,-500), MeshPhongMaterial with flatShading and DoubleSide. Fixed background #1a1a1a, FOV 45, near 0.1, far 100000, no transparency pass.
- Windows tests: tests/presentation/visualization/renderers/threejs/test_threejs_renderer.py::test_viewer_shading_matches_the_native_renderer; tests/presentation/visualization/renderers/threejs/test_threejs_renderer.py::test_viewer_uses_bounds_aware_dynamic_camera_clipping
- note: The iOS viewer is a custom minimal template, not a port of the 3767-line viewer.html. Decide whether to adopt the Windows template (it bundles layer, plan, hover and other features, out of scope for 'no new features') or only the shading/colour constants.

#### 214. LP-24: Layer list ordering ties and merged template layers
- owner: layers-pages | impact: ui-cosmetic | disposition: needs-decision | testable: needs-extraction-from-objc | verdict: AMBIGUOUS | status: open
- Windows: `ost_visualizer/domain/entities/layer.py:89-93; infrastructure/mdb/components/bid_data_reader.py:575-615`: The sidebar lists bid layers plus locked template layers (IsTemplate and IsLocked), merged by lowercased name with the bid row winning, sorted by Sequence only (stable; ties keep template-first then database order).
- iOS: `ViewController+Layers.mm:12-24; ProjectStore+Load.cpp:103-121`: Lists only WHERE BidUID rows and sorts by (sequence, name). Locked template layers never appear, and ties sort alphabetically.
- Windows tests: tests/domain/entities/test_layer.py::MergeLayersForBidTests; tests/domain/entities/test_layer.py::LayerEntityContractTests::test_merge_prefers_the_later_sequence_for_equal_non_template_names
- note: No ORDER BY in the Windows layer query, so tie behavior is effectively undefined. Verifier: Windows lists bid layers plus locked template layers merged by lowercased name and sorted by Sequence only; ...

#### 215. MSH-21: Flat quantized face shading instead of Windows' 60-degree crease-angle smooth shading
- owner: rendering-planview | impact: ui-cosmetic | disposition: needs-decision | testable: core-cpp | verdict: CONFIRMED | status: open
- Windows: `presentation/visualization/utils/mesh.py:prepare_vertices_for_shading (crease_angle_degrees=60), _normalize_vector (normals oriented to +z)`: Vertices are split at creases above 60 degrees and normals are averaged inside a crease group, so cylinders and curved walls shade smoothly while hard edges stay sharp.
- iOS: `Rendering/MeshLoader.cpp:26-66 (flat face normals snapped to 1/2048; smooth vertex normals computed but unused); Renderer.mm:376-395; Rendering/Shaders/src/fs_mesh.sc`: Every triangle uses its own flat normal; lighting flips the normal toward the viewer (two-sided).
- Windows tests: tests/presentation/visualization/utils/test_mesh.py::ShadingPreparationTests
- note: The iOS log (2026-03-23, 'Quantized flat shading') records this as a deliberate rendering choice to avoid seam artifacts, so it is a decision, not a straight port. Rendering is a platform layer.

#### 216. EXP-24: DXF LINE entity order is the std::unordered_set iteration order of the STL in use
- owner: export-formats | impact: ui-cosmetic | disposition: cannot-reproduce | testable: core-cpp | verdict: CANNOT_REPRODUCE | status: open
- Windows: `cpp_extensions/src/mesh/feature_edges.cpp:109-175; exporters/dxf_exporter.py:32-38`: Edges come from an unordered_set in MSVC STL iteration order, so the LINE order in the DXF is MSVC-specific.
- iOS: `Core/Geometry/feature_edges.cpp:109-175; Services/ExportService.mm:153-155`: The same algorithm on libc++, whose unordered_set iteration order differs. LINE order (and which endpoint is first in a LINE) can differ from Windows.
- Windows tests: none found
- note: CAD semantics are order-independent. The DXFWriter itself is byte-identical for the same line list. Verifier: LINE entity order follows unordered_set iteration (feature_edges.cpp:109-175), which is MSVC STL specific and cannot be matched under libc++. ...

## (E) Needs your decision

44 items. Each stays `open` and is not touched in Phase 3 until answered. Full evidence is in the item block (sections A-D) and `divergences.json`.

### RPV-05 (rank 62, wrong-numbers, owner rendering-planview): Grid snap is unconditional; Windows only snaps when the grid point is within the pixel threshold, and only when enabled with increment > 0

**Question.** Windows snaps to the grid only when snapping is enabled, the increment is > 0 and the grid node lies within the pixel threshold (default 8 px) of the cursor; otherwise the raw cursor is used. iOS rounds every tap unconditionally. Touch input has no hover, so the gate is a product call. What should iOS do?

- (a) Port the proximity gate with an 8-point threshold.
- (b) Keep always-snap on the grid (only the rounding and angle/length rules are ported).

### COL-04 (rank 63, wrong-numbers, owner rendering-planview): 2D fill opacity in the pattern-driven mode: Windows 0.0 for None and line patterns, iOS 0.5

**Question.** In the pattern-driven display mode ('Original' on Windows, 'OST' on iOS) Windows uses 2D fill alpha 0.0 for pattern None and for line patterns 2-7 (hatch lines are drawn instead), 1.0 for Solid, 0.5 otherwise; iOS uses 0.5 for every non-Solid and has no hatch renderer. Pattern 0 itself is contradictory on Windows (2D opacity 0.0 but built with a solid brush; 3D opaque). How should iOS render 2D fills?

- (a) Implement hatch rendering first, then adopt the Windows alpha table.
- (b) Adopt the alpha table with outline-only fills (line-pattern conditions would look empty on iOS).
- (c) Keep the 0.5 approximation. For pattern 0 also choose: treated as Solid everywhere (alpha 1.0) or mirror Windows 2D output (alpha 0.0 with outline).

### DBP-37 (rank 64, wrong-numbers, owner database-persistence): ProjectData::page_scale_ratio uses sf1 == 0 where Windows uses sf1 > 0

**Question.** Windows is inconsistent for a negative ScaleFactor1: the page metadata service returns ratio 1.0 (sf1 > 0 guard) while the mesh path computes sf2/sf1 (negative). iOS follows the mesh path. Only reachable with invalid data. Which rule?

- (a) Metadata-service rule: any non-positive sf1 gives 1.0 (zero is first coalesced to 1.0 by the loader).
- (b) Keep the current iOS rule.

### COND-METRIC-01 (rank 65, wrong-numbers, owner conditions): Metric bids unsupported: UOM codes 11-16, mm display, metric defaults

**Question.** Windows switches to millimetre dialog text, metric UOM lists, metric defaults and metric quantity formatting when Bids.MeasureBase == 1. The core UOM table and normalisation (codes 11-16) are in QTY-05/QTY-06; this item is the dialog and presentation side. Should iOS support metric bids end to end?

- (a) Yes: mm dimension parse/display, metric defaults (uom1 13/14), metric 2-decimal quantity text.
- (b) Core UOM math only; the dialog stays feet-inches (metric bids show correct numbers but imperial text).

### GEO-M02 (rank 108, data-safety, owner rendering-planview): No hole and attachment containment validation when editing a parent area

**Question.** Windows rejects a parent-area edit (vertex/edge drag, move, rotate) when a hole child would leave the parent, a hole would intersect a sibling hole, or an attachment would not fit the area minus holes. iOS accepts any simple polygon. Port this containment validation (new Core code)?

- (a) Port path_is_inside / path_intersects_any / attachment_fits_area and call them from the drag computation.
- (b) Leave unvalidated (iOS cannot create holes or attachments; exposure is limited to editing imported parents with children).

### POS-M01 (rank 109, data-safety, owner parsers-units): Non-finite position coordinates (inf/nan) are accepted on iOS; Windows rejects them and aborts the load

**Question.** Windows parse_position accepts 'nan'/'inf' but the Takeoff contract check then raises and the whole bid load fails naming the UID; iOS keeps non-finite geometry. What should iOS do?

- (a) Fail the bid load with an error naming the takeoff UID (Windows).
- (b) Drop that takeoff and continue.
- (c) Sanitise to an empty position.

### IMP-M01 (rank 110, data-safety, owner import-ost-osp): iOS XML parser accepts malformed files that Windows rejects and keeps raw windows-1252 bytes as invalid UTF-8

**Question.** Windows parses .ost with expat (strict): it rejects raw non-UTF-8 bytes without a declaration, duplicate attributes, '<' in attribute values, a second root element, undefined entities, a bare '&' and control characters, and honours declared windows-1252. pugixml accepts all of these and keeps raw windows-1252 bytes as invalid UTF-8 (which can make a bid name nil in NSDictionary literals). How strict should iOS be?

- (a) Add a pre-validation pass (libxml2/NSXMLParser, or UTF-8 validation plus windows-1252 transcode plus duplicate-attribute checks) so malformed files are rejected like Windows.
- (b) Only transcode declared non-UTF-8 encodings to UTF-8 (fixes the nil-string crash) and keep pugixml's leniency.
- (c) Keep as is.

### DBP-31 (rank 111, data-safety, owner database-persistence): load_project does no integrity validation and silently drops takeoffs; Windows refuses to load invalid data

**Question.** Windows refuses to load a bid whose takeoffs reference a missing condition, page or parent, form a ParentUID cycle, sit on a different page than their parent, or have non-finite geometry; iOS loads tolerantly and silently drops takeoffs with an unknown condition or fewer than 2 coordinates. How should iOS treat invalid data?

- (a) Fail the open with an error alert (Windows).
- (b) Load tolerantly but surface a diagnostic.
- (c) Keep the silent skip.

### EXP-15 (rank 167, behavior, owner export-formats): HTML export excludes hidden-layer conditions; Windows HTML includes them and lets the viewer toggle them

**Question.** Windows HTML export includes takeoffs on hidden layers and ships a layer list so the viewer can toggle them; OBJ/DXF export visible layers only. iOS filters hidden layers for all three formats and its HTML viewer has no layer panel, so including them would show hidden elements permanently. What should iOS HTML export do?

- (a) Include hidden-layer meshes with layer metadata and add a layer toggle to the iOS viewer (new viewer feature).
- (b) Keep filtering hidden layers in HTML.

### BRW-08 (rank 168, behavior, owner project-browser): iOS deletes non-empty folders (unassigning their bids); Windows only deletes empty projects

**Question.** Windows only deletes empty projects (a project with bids shows 'Only empty projects can be deleted.'); iOS offers to delete a non-empty folder and unassigns its bids after an alert. Which rule?

- (a) Refuse to delete non-empty folders with the Windows message.
- (b) Keep the unassign-with-confirmation flow.

### BRW-11 (rank 169, behavior, owner project-browser): Trashing an orphan bid overwrites OrigBidProjectUID with NULL; Windows leaves the stored value alone

**Question.** Trashing an orphan bid: Windows leaves OrigBidProjectUID untouched (the writer only writes it when a restore project is given), keeping a possibly stale restore target; iOS writes NULL. Copy Windows?

- (a) Copy Windows: do not touch OrigBidProjectUID for an orphan.
- (b) Keep the iOS clearing (safer, contradicts a pinned Windows test).

### COND-EXTRA-01 (rank 170, behavior, owner conditions): iOS edits and persists Connect, Tolerance and Snap-to-Linear that Windows never reads or edits

**Question.** iOS edits and persists Connect, Tolerance (a required parsed field) and Snap to Linear; the Windows Condition entity and dialog never read or edit them. Removing existing UI is a product call. What should iOS do?

- (a) Remove the three controls and stop writing the columns (field set equals Windows).
- (b) Keep them.

### LP-22 (rank 171, behavior, owner layers-pages): Hide All hides layer-less conditions in Windows memory; iOS keeps them visible

**Question.** Windows update_all_layer_visibility hides EVERY condition in memory including conditions with no layer or an unknown layer uid, but a reload recomputes visibility from layers (layer-less conditions are visible again). iOS always shows layer-less conditions. Which should iOS follow?

- (a) Hide All also hides layer-less conditions (needs a per-condition visibility flag).
- (b) Keep the load rule (layer-less conditions always visible).

### QTY-19 (rank 172, behavior, owner database-persistence): Takeoffs with fewer than 2 position values are dropped at load, so count/attachment quantities can differ

**Question.** Windows keeps every takeoff row even when Position is empty or unparsable (position [], a Count with calc 23 still counts 1); iOS drops takeoffs with fewer than 2 position values at load. Should iOS keep such takeoffs?

- (a) Keep them with an empty position (counts match Windows; PlanView and mesh code must tolerate empty positions).
- (b) Keep the iOS defensive filter (a corrupt count takeoff is not counted).

### QTY-22 (rank 173, behavior, owner conditions): Zero quantities are hidden on iOS but shown as '0 <label>' on Windows

**Question.** Windows shows '0 <unit label>' for a zero quantity in every slot that has a label; iOS hides zero quantities in the one-line subtitle. Which should iOS show?

- (a) Show '0 LF' / '0 EA' exactly as Windows (add the zero text rule to the Core formatter).
- (b) Keep zeros hidden because the subtitle is one line (platform layout).

### BRW-16 (rank 176, behavior, owner database-persistence): Cyclic page-folder parents silently hide the folders and their pages (Windows refuses the load)

**Question.** Windows refuses to load a bid whose page folders form a ParentUID cycle; iOS silently drops the cyclic folders and leaves their pages unreachable in the selector. Which?

- (a) Fail the bid open with an error.
- (b) Surface the cycle members under a root so the pages stay reachable.

### BRW-M01 (rank 177, behavior, owner project-browser): Folder (project) display order is locale-aware/case-insensitive on Windows but SQLite BINARY / std::string < on iOS

**Question.** Windows orders project (folder) nodes with Qt's locale-aware, case-insensitive compare ('apple' before 'Zebra', '10 Main' before '9 Main'); iOS uses SQLite BINARY / std::string < (uppercase first). Exact Qt/OS-locale ordering cannot be reproduced in portable Core C++. How close should iOS be?

- (a) Case-insensitive ASCII compare with tie-breaks (fold, then lowercase first, then UID).
- (b) Use localizedStandardCompare/localizedCompare in the UI layer.
- (c) Keep binary order.

### CAM-04 (rank 178, behavior, owner settings-undo-camera): Pinch zoom clamp is a fixed 1..500 that the fit distance can exceed; Windows clamps relative to the scene

**Question.** iOS clamps the pinch distance to a fixed [1, 500] while the fit distance (2.5 x extent) can exceed 500, so the first pinch event snaps the camera inward; Windows clamps relative to the scene (2*near .. 0.5*far, near = max(0.1, 0.001*size), far = max(10000, 20*size)). Redefine the clamp together with iOS near/far planes?

- (a) Scene-relative clamp using the Windows limits expressed in iOS scene units.
- (b) Keep the fixed clamp but raise the upper bound above the fit distance.

### COND-M08 (rank 179, behavior, owner conditions): Editor indexes the style label array with the raw condition type (crash on unsupported types)

**Question.** EditConditionViewController indexes its style-label array with the raw condition type and raises NSRangeException for Type outside 0-3 (reachable from corrupted imports). Windows falls back to the first entry (Linear). Intended behaviour for an unsupported Type?

- (a) Show Linear like Windows (a Save would then rewrite Type).
- (b) Refuse to open the editor with a clear message.
- (c) Open read-only.

### COND-STYLE-02 (rank 180, behavior, owner conditions): Style-change default spacing and shape retention differ

**Question.** On a style change Windows is self-contradictory for spacing: the form field keeps its old text, but the saved DTO sets spacing to the TYPE_DEFAULTS value (Area 6.0, others 4.0) unless the field was edited. Shape reset to Square only when leaving the {Count, Attachment} family is clear. Which spacing result should iOS persist?

- (a) Persisted rule: type default (Area 6.0, others 4.0) unless the user edited the field.
- (b) Displayed rule: keep the existing spacing.

### DBP-33 (rank 181, behavior, owner database-persistence): Reference field defaults, enforced relationships and UID-required metadata are not replicated

**Question.** Windows applies 61 field defaults (for example BidTakeoffs Count/GridOffsetX/GridOffsetY/GridRotation/FontColor = 0), 83 enforced relationships and UID-required flags to rows it creates; iOS declares none (insert_takeoff leaves those columns NULL). Declaring foreign keys would break delete ordering and INSERT OR REPLACE. What should be added?

- (a) Add DEFAULT clauses only.
- (b) Add defaults and enforced foreign keys (needs delete-order and import changes).
- (c) Leave as is (matters only once OST export exists).

### EXP-17 (rank 182, behavior, owner meshing): OBJ vertex order, triangulation diagonals and winding conventions differ from Windows for identical geometry

**Question.** OBJ vertex order, bottom-face diagonals and winding differ from Windows for identical geometry (same vertex sets, positive volume for boxes). Byte parity would require relabelling the iOS generators to the Windows vertex order plus a face swap. Windows itself is inconsistent (triangle prism winds inward). Require identical OBJ text?

- (a) Geometric equivalence is enough (same vertex set, positive signed volume, same bounding box).
- (b) Demand byte-identical vertex and face lines (rework generators; decide the triangle/rhombus winding first).

### EXP-18 (rank 183, behavior, owner export-formats): Mesh/takeoff ordering and page-selection order differ

**Question.** Windows exports takeoffs page by page in the user's selection order (HTML: area, linear, count, attachment); iOS keeps the 3D page selection in a std::set (user order lost) and traverses takeoffs in database order. The type ordering is fixed by MSH-20. Should the user's page selection order be preserved in exports?

- (a) Yes: change the selection model to an ordered list.
- (b) No: keep sorted order and only apply the type ordering.

### EXP-20 (rank 184, behavior, owner export-formats): Three.js scene payload structure, precision, normals and defaults differ from the Windows scene

**Question.** The Windows scene payload carries crease-split vertices with smooth normals, full-precision floats, opacity, per-mesh metadata, bounds, conditions, layers and view options; iOS emits 8-vertex meshes at %.4f precision with only camera and geometries. Adopt the Windows schema?

- (a) Adopt the numeric parts only (full-precision numbers, opacity, crease-split normals).
- (b) Adopt the full schema including metadata used by Windows-only viewer features.
- (c) Keep the minimal payload.

### EXP-22 (rank 185, behavior, owner export-formats): Edge cases: negative-only and fully-negated exports

**Question.** Windows is inconsistent between formats for edge cases: a negative-only export writes an empty OBJ/DXF group successfully but HTML fails; a fully negated positive is exported intact in OBJ/DXF but dropped (render fails) in HTML. Which outcome should iOS have?

- (a) Copy the per-format Windows outcomes.
- (b) One unified rule: report 'nothing to export' for negative-only and drop fully negated positives everywhere.
- (c) Keep iOS (positive kept in every format; empty list shows 'Nothing to Export').

### EXP-M02 (rank 186, behavior, owner export-formats): HTML scene winding and normal sign: Windows sends inward-facing meshes and inward normals, and the viewer lights with the raw normal

**Question.** Windows HTML scenes carry inward-facing meshes with inward normals (the native-matching shader lights with the raw normal attribute); iOS meshes are outward and its Phong material ignores the normal attribute. Matters only if the Windows shader or normal data is adopted (EXP-19/EXP-20). Which sign convention?

- (a) Reproduce Windows (inward normals).
- (b) Keep outward normals.

### GEO-15 (rank 187, behavior, owner meshing): Face winding sense is opposite to Windows for every area mesh (outward on iOS, inward on Windows)

**Question.** Windows area meshes have negative signed volume (inward faces under its (-x,y,z) frame); iOS area meshes are outward (positive volume). Windows pins only that a flip keeps the unflipped winding, not inward vs outward. Should iOS reproduce Windows' face list exactly? (GEO-14 and GEO-15 must be fixed together: feeding earcut the Windows frame alone gives an inconsistent mesh.)

- (a) Keep outward-facing iOS meshes (positive volume, safe for Manifold and viewers); only fix the triangulation sets (GEO-14) with the wall rule flipped consistently.
- (b) Reproduce the Windows index order exactly (volumes -16, -15, -6 in the verifier cases) for byte-identical exports; lighting from winding-derived normals must then be re-checked.

### IMP-15 (rank 188, behavior, owner import-ost-osp): Windows-filesystem member-name safety rules not applied (absolute/rooted, ':', device names, trailing dot/space)

**Question.** Windows rejects OSP members that are unsafe on a Windows filesystem (':' in a part, reserved device names, trailing dot or space, absolute or rooted paths). iOS only writes basenames, so there is no traversal risk. Reject the same archives?

- (a) Strict parity: port _is_safe_cab_member_name and reject before extraction.
- (b) Accept names that cannot hurt iOS.

### IMP-17 (rank 189, behavior, owner import-ost-osp): CAB compression types other than stored and MSZIP are unsupported

**Question.** iOS CabExtractor supports stored and MSZIP only; Windows FDI also extracts LZX and Quantum. No Windows test has such a fixture and the Windows exporter writes MSZIP. Implement an LZX decoder (new code)?

- (a) Yes, add LZX (and Quantum) decoding.
- (b) No, keep stored + MSZIP and report a clear error.

### IMP-18 (rank 190, behavior, owner import-ost-osp): CAB member names are not decoded (legacy ANSI code page vs UTF-8 flag)

**Question.** Windows decodes CAB member names as UTF-8 when attribs & 0x80 is set, otherwise with the machine ANSI code page (CP_ACP, machine dependent; Windows tests assume cp1252). iOS ignores the flag, so a legacy cp1252 name is invalid UTF-8. Which fixed code page should iOS assume for non-UTF-flagged names?

- (a) cp1252 (matches the Windows fixtures).
- (b) Another fixed code page or a lossy fallback.

### IMP-19 (rank 191, behavior, owner import-ost-osp): iOS rejects re-import of an already-imported Bid; Windows always imports an independent copy

**Question.** iOS refuses to import a Bid whose UID already exists ('already imported'); Windows has no duplicate check and remaps every UID, so importing a file twice yields two bids. After IMP-06 (UID remap) the guard is redundant. Keep it?

- (a) Remove it (Windows behaviour; second import creates an independent copy with a new BidNo).
- (b) Keep it as a user-facing guard.

### IMP-20 (rank 192, behavior, owner import-ost-osp): DATETIME and binary column values stored as raw text

**Question.** Windows converts DATETIME columns from the 'Y M D h m s' text to a datetime (anything else becomes NULL); iOS stores the raw text and Settings.Created uses 'YYYY-MM-DD HH:MM:SS', so the database mixes formats. Which representation should iOS store?

- (a) Keep the raw 6-part string, but NULL for invalid values.
- (b) Convert to an ISO text like Settings.Created.
- (c) Leave as is (no visible effect today: bid_date and copy_timestamp are never used).

### IMP-M03 (rank 193, behavior, owner import-ost-osp): CAB extraction re-inflates and holds the entire folder in memory for every member

**Question.** CabExtractor re-inflates the whole folder for every extracted member (N PDFs means N+1 inflations held in RAM); Windows streams every member in one FDI pass. Large OSPs (hundreds of MB of drawings) can exhaust iOS memory. Change the extraction strategy?

- (a) Single-pass extractor that streams each member to disk.
- (b) Keep as is (results are byte-identical; only a resource limit).

### MSH-16 (rank 194, behavior, owner export-formats): Export boolean rules differ per Windows format; iOS uses one rule for OBJ, DXF and Three.js

**Question.** Windows applies different boolean rules per output: OBJ/DXF metadata has no condition_type so every negative cuts every positive and a fully consumed positive reappears un-subtracted; HTML and the 3D view subtract only same-condition-type negatives and drop a fully consumed positive. The OBJ/DXF behaviour looks accidental. Which rule should iOS use?

- (a) Copy Windows exactly per format (HTML/3D: same type only, consumed positive dropped; OBJ/DXF: all-vs-all, consumed positive exported intact).
- (b) Use the HTML/3D-view rule for every format.
- (c) Keep the current single iOS rule (all-vs-all, consumed positive kept).

### MSH-19b (rank 195, behavior, owner meshing): Drop-stub union failure: Windows drops the whole takeoff mesh, iOS keeps the plain wall

**Question.** When boolean_union of a wall and its drop stub fails or raises, Windows returns None and the whole takeoff mesh disappears (mesh_factory.py:88,139), which looks accidental; iOS keeps the plain wall. Copy Windows?

- (a) Drop the takeoff mesh (Windows).
- (b) Keep the plain wall (iOS).

### QTY-18 (rank 196, behavior, owner quantities): Parent token 'None' counts as a root takeoff in Windows quantities but as a hole in iOS (Windows is internally inconsistent)

**Question.** Windows is inconsistent about the literal parent token 'None': quantity code (condition_quantity_service.py:62,88-91) treats '', '0' and 'None' as roots, but Takeoff.is_hole (takeoff.py:7,135-137) treats only '' and '0' as roots, and the Windows loader would reject a 'None' parent anyway. iOS loads NULL/empty parents as '0', so 'None' cannot reach iOS from a file. Which rule should iOS compute_page_quantities follow?

- (a) Port the 3-token root set into compute_page_quantities only (matches the pinned Windows tests ConditionQuantityRootSentinelTests); Takeoff::is_hole keeps 2 tokens.
- (b) Leave iOS on the 2-token rule (no behaviour change for real data).

### RPV-22 (rank 197, behavior, owner rendering-planview): Curved linear takeoffs: 2D draws a straight chord, placement never produces a curve, and there is no centre handle

**Question.** Windows draws curved linears (curve >= 0, >= 6 values) as arc bands, shows a centre handle, and creates a 6-value position when a condition is a curved segment; iOS draws a straight chord, has no centre handle and always creates 4 values. Existing imported curved takeoffs are drawn straight (GEO-18); this item is creation and the handle. Is curved 2D editing in scope?

- (a) Yes: placement of the control point, centre handle, drag keeps control offset (RPV-M03).
- (b) No: only draw existing curves (GEO-18) and keep drag from corrupting the control point (RPV-M03).

### QTY-23 (rank 198, ui-cosmetic, owner conditions): Conditions list labels: iOS strips elevation suffixes from names and renames the unknown type group

**Question.** Windows shows condition.name verbatim in the sidebar (elevation stripping is used only for CSV export and hover text); iOS strips everything after the last ' @T', ' @B', ' @ ' or ' @'. Windows names a type group whose uid has no CdnTypes row 'Unknown'; iOS says '(unassigned)'. What should iOS do?

- (a) Show the full name and use 'Unknown' (strict Windows parity).
- (b) Keep stripping for the compact phone list but reuse Windows strip_elevation_suffix rules (explicit marker, strippable value, empty base returns the original), and use 'Unknown'.
- (c) Keep the current iOS behaviour.

### CAM-02 (rank 210, ui-cosmetic, owner settings-undo-camera): Camera refits on every empty-to-non-empty page selection; Windows preserves camera within a bid

**Question.** Windows fits the camera once per bid scene and keeps the user's pose across same-bid scene updates (including an emptied page selection); iOS refits on every transition from no selected 3D page to some selected page (added deliberately, PORTING_LOG 2026-03-24). Which?

- (a) Preserve the camera (Windows).
- (b) Keep the refit.

### DBP-34 (rank 211, ui-cosmetic, owner database-persistence): Boolean columns are written as 1 by iOS but as bool (Access -1) by Windows

**Question.** Windows stores true YESNO values as -1 (Access); iOS mixes 1 and -1 within BidConditions and writes 1 for IsNegativeQuantity. All readers treat any non-zero as true. Adopt -1 everywhere?

- (a) Yes, write -1 for every true flag.
- (b) No, keep the current mixed encoding until an OST export exists.

### DBP-M05 (rank 212, ui-cosmetic, owner database-persistence): Per-page selected area (BidPageSettings.BidAreaSelected) is not loaded, so Windows' greyed-out inactive-area takeoffs have no iOS counterpart

**Question.** Windows loads the per-page selected area (BidPageSettings.BidAreaSelected) and colours takeoffs of other areas with the inactive-object colour in 2D and 3D; iOS never reads it. Port page area selection (loader plus colour wiring)?

- (a) Yes (colour only, never geometry or quantities).
- (b) No (Areas are not surfaced on iOS).

### EXP-19 (rank 213, ui-cosmetic, owner export-formats): Three.js viewer shading, colour management and lighting differ from the Windows viewer (explicitly pinned by Windows tests)

**Question.** The Windows HTML viewer is a 3767-line template (three@0.150.0, linear colour, no tone mapping, native 3-light shader, layer panels, hover, section cuts, 2D plan tab). iOS writes a minimal viewer (three@0.152.0, ACES, Phong lights). Windows tests assert the absence of MeshPhongMaterial, DirectionalLight, AmbientLight and ACES. How much should iOS adopt?

- (a) Adopt only the shading and colour-management constants (NoToneMapping, LinearEncoding, ambient 0.55 / key 0.45 / fill 0.35 / rim 0.2 shader).
- (b) Adopt the whole Windows template (large, includes features outside 'no new features').
- (c) Keep the minimal viewer.

### LP-24 (rank 214, ui-cosmetic, owner layers-pages): Layer list ordering ties and merged template layers

**Question.** Windows lists bid layers plus locked template layers merged by lower-cased name, sorted by Sequence only (tie order undefined); iOS lists only the bid's own rows sorted by (sequence, name). Bids normally copy all four templates, so impact is rare. Merge template layers and replace the tie rule?

- (a) Port the merge_layers_for_bid rule (template merge, sequence sort, stable ties).
- (b) Keep as is.

### MSH-21 (rank 215, ui-cosmetic, owner rendering-planview): Flat quantized face shading instead of Windows' 60-degree crease-angle smooth shading

**Question.** Windows shades with 60-degree crease-angle smoothing (cylinders and curved walls look smooth, hard edges stay sharp); iOS deliberately uses flat quantized face normals (PORTING_LOG 2026-03-23) to avoid seam artefacts. Adopt the Windows shading?

- (a) Port prepare_vertices_for_shading (split at 60 degrees, averaged normals in a crease group) into MeshLoader.
- (b) Keep flat shading (rendering-layer choice, no numbers affected).

## (F) Cannot reproduce on iOS

- **EXP-24** (ui-cosmetic): DXF LINE entity order is the std::unordered_set iteration order of the STL in use. Evidence: Windows `cpp_extensions/src/mesh/feature_edges.cpp:109-175; exporters/dxf_exporter.py:32-38`; iOS `Core/Geometry/feature_edges.cpp:109-175; Services/ExportService.mm:153-155`. CAD semantics are order-independent. The DXFWriter itself is byte-identical for the same line list. Verifier: LINE entity order follows unordered_set iteration (feature_edges.cpp:109-175), which is MSVC STL specific and cannot be matched under libc++. ...
- Partial: exact Qt/locale ordering of folder names (BRW-M01) and the machine-dependent ANSI code page of legacy CAB member names (IMP-18) cannot be reproduced bit for bit; both are decisions in section E.

## (G) Per-area feature table

Columns: iOS feature, iOS location, Windows module, Windows tests, status, divergence ids (canonical ids from sections A-D).

### quantities

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| UOM code table, labels, metric flag | Core/Domain/UomService.hpp:22-83 | ost_visualizer/domain/services/uom_service.py:6-65 | tests/domain/services/test_uom_service.py::UomCodeTableTests | diverges | QTY-05, QTY-16 |
| Calc-type codes and per-condition-type quantity option lists | Core/Domain/UomService.hpp:37-51,85-133 | ost_visualizer/domain/services/uom_service.py:22-38,68-106,180-189 | tests/domain/services/test_uom_service.py::UomCodeTableTests::test_quantity_option_lists_are_the_dia ... | matches | QTY-17 |
| Valid UOMs per calc type (unit-family selection) | Core/Domain/UomService.hpp:135-196 | ost_visualizer/domain/services/uom_service.py:107-177 | tests/domain/services/test_uom_service.py::UomValidUnitTests::test_every_calculation_code_selects_it ... | diverges | QTY-05, QTY-16 |
| UOM conversion (convert_to_uom), imperial codes 0-9 | Core/Domain/UomService.hpp:200-237 | ost_visualizer/domain/services/uom_service.py:192-211,315-319 | tests/domain/services/test_uom_service.py::UomConversionTests::test_every_unit_divides_inches_by_its ... | matches | QTY-05 |
| Polygon geometry helpers (vertices, area, perimeter, bounding box) | Core/Domain/UomService.hpp:241-303 | ost_visualizer/domain/services/uom_service.py:225-243,585-612 | tests/domain/services/test_uom_service.py::UomGeometryHelperTests; ... | diverges | QTY-14 |
| Linear polyline length (stride-7 segments) | Core/Domain/UomService.hpp:305-321,509-516 | ost_visualizer/domain/services/uom_service.py:536-551 | tests/domain/services/test_uom_service.py::UomLinearQuantityTests::test_segment_lengths_are_summed_e ... | matches | - |
| Curved linear length (control point, segment count, arc fit, quadratic fallback) | Core/Domain/UomService.hpp:323-507 | ost_visualizer/domain/services/uom_service.py:369-533; cpp_extensions/src/linear_geom/linear_geom.cp ... | tests/domain/services/test_uom_service.py::UomServiceTests; tests/domain/services/test_uom_service.p ... | diverges | QTY-07, QTY-08, QTY-09 |
| Slope factor | Core/Domain/UomService.hpp:518-523,673 | ost_visualizer/domain/services/uom_service.py:676-682,731-735 | tests/domain/services/test_uom_service.py::UomGeometryHelperTests::test_slope_factor_is_the_hypotenu ... | diverges | QTY-10 |
| Raw quantity formulas per condition type (count/attachment, linear, area incl. ... | Core/Domain/UomService.hpp:527-650 | ost_visualizer/domain/services/uom_service.py:336-366,554-582,615-673 | tests/domain/services/test_uom_service.py::UomCountQuantityTests; ... | matches | - |
| Per-takeoff three-slot quantity calculation | Core/Domain/UomService.hpp:659-728 | ost_visualizer/domain/services/uom_service.py:701-778 | tests/domain/services/test_uom_service.py; tests/domain/services/test_condition_quantity_service.py: ... | diverges | QTY-04, QTY-10, QTY-17, QTY-05, QTY-06 |
| Page-level aggregation compute_page_quantities (group by condition, sum, negative sign, ho ... | Core/Domain/UomService.hpp:734-804 | ost_visualizer/domain/services/condition_quantity_service.py:50-139 | tests/domain/services/test_condition_quantity_service.py::TakeoffLifecycleQuantityTests; ... | diverges | QTY-01, QTY-02, QTY-03, QTY-18 |
| Condition/Takeoff/Types entities (quantity-relevant fields) | Core/Domain/Condition.hpp:8-74, Core/Domain/Takeoff.hpp:8-31, Core/Domain/Types. ... | ost_visualizer/domain/entities/condition.py; ost_visualizer/domain/entities/takeoff.py | tests/domain/services/test_takeoff_domain_service.py | matches | - |
| Load of quantity-relevant condition/takeoff columns | Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:154-270 | ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:880-1015,1110-1190 | tests/infrastructure/mdb/components/test_bid_data_reader.py::TakeoffHydrationContractTests | diverges | QTY-06, QTY-13, QTY-19 |
| Sidebar quantity display (formatQty, subtitle) | Controllers/ConditionsViewController.mm:46-57,320-337 | ost_visualizer/application/utils/quantity_display.py:4-17; ost_visualizer/presentation/components/co ... | tests/application/utils/test_quantity_display.py; tests/integration/quantities/test_condition_uom_pa ... | diverges | QTY-12, QTY-22 |
| Which takeoffs feed the sidebar totals and when they refresh | Controllers/ViewController/ViewController.mm:708-727; ... | ost_visualizer/presentation/coordinators/sidebar_coordinator.py:218-248; ... | tests/integration/refresh/test_page_condition_quantities.py; tests/integration/quantities/test_page_ ... | diverges | QTY-11, QTY-15 |
| Conditions list structure/order | Controllers/ConditionsViewController.mm:8-28,126-225,299-337 | ost_visualizer/presentation/components/conditions_sidebar.py:239,423-512,606-710 | - | diverges | COND-FOLDER-01, COND-LIST-01, QTY-23 |
| Quantity/UOM pickers in the condition editor | Controllers/EditConditionViewController/Components/EditConditionViewController+P ... | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:85-140,1230-1290,1332-1360,1607-1625 | tests/integration/quantities/test_condition_uom_parity.py | platform-adapted | QTY-05 |
| Quantity use in ProjectService / ExportService / PlanView | Services/ProjectService.mm, Services/ExportService.mm, PlanView/* | - | - | unverified | - |

### geometry

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| Segment intersection test (strict proper crossing only) | Core/Geometry/geom_utils.cpp:6-21 | cpp_extensions/src/geom_utils/geom_utils.cpp:7-44 | tests/integration/native/test_geometry_boundaries.py (indirect, via polygon_is_valid) | matches | - |
| Open polyline self-intersection check | Core/Geometry/geom_utils.cpp:45-59; used by PlanView/Components/PlanView+Placeme ... | cpp_extensions/src/geom_utils/geom_utils.cpp:91-105 | - | matches | - |
| Polygon validity check (self-intersection and degeneracy) | Core/Geometry/geom_utils.cpp:23-43; used by Controllers/ViewController/Component ... | cpp_extensions/src/geom_utils/geom_utils.cpp:45-90 | tests/integration/native/test_geometry_boundaries.py::test_polygon_rejects_degenerate_and_self_touch ... | diverges | GEO-09 |
| Signed area of a flat coordinate list (2x shoelace sum, used for winding sign) | Core/Geometry/geom_utils.cpp:61-74; used by DragController.mm:93,101 | cpp_extensions/src/geom_utils/geom_utils.cpp:106-119 | tests/presentation/components/plan_view/components/test_geometry_utils.py::TakeoffLifecyclePrecision ... | diverges | GEO-07 |
| Point-to-segment distance | PlanView/PlanView.mm:19-31 (static pointToSegmentDistSq, not in Core); ... | cpp_extensions/src/geom_utils/geom_utils.cpp:11-33; ost_visualizer/presentation/components/plan_view ... | - | platform-adapted | - |
| Point-in-polygon hit test (even-odd ray cast) | PlanView/PlanView.mm:33-45 and :797-815 | ost_visualizer/presentation/components/plan_view/components/selection_manager.py (Qt scene hit testi ... | - | platform-adapted | - |
| Earcut triangulation core (vendored mapbox earcut, with holes) | Core/Geometry/earcut.hpp; Core/Geometry/earcut_impl.cpp:7-11 | cpp_extensions/src/earcut/earcut.hpp; cpp_extensions/src/module_earcut.cpp | tests/integration/native/test_geometry_boundaries.py::test_valid_earcut_and_curve_inputs_are_accepte ... | matches | GEO-14 |
| Area mesh triangulation, hole index remap, side walls and slope | Core/Meshing/gen_area.cpp:22-204 | ost_visualizer/presentation/visualization/core/geometry/area.py:18-158; ... | tests/presentation/visualization/core/geometry/test_area.py::AreaSlopeBehaviorTests; ... | diverges | GEO-05, GEO-14, GEO-15 |
| OST to 3D world transform (X negation, scale ratio) | Core/Meshing/MeshFactory.cpp:20-169; Services/ProjectService.mm:103-130,156-181; ... | ost_visualizer/domain/services/coordinate_transformation_service.py:105-154; ... | tests/integration/rendering/test_page_flip_mesh.py; tests/domain/services/test_coordinate_transforma ... | diverges | MSH-01, MSH-14 |
| Page rotation and flip applied to 3D geometry | absent: Page.flip_x/flip_y/rotation (Core/Domain/Page.hpp:18-19,17) are never re ... | ost_visualizer/presentation/visualization/meshing/mesh_builder.py:71-88; ... | tests/integration/rendering/test_page_flip_mesh.py; tests/domain/services/test_coordinate_transforma ... | diverges | MSH-02 |
| Page scale ratio (sf2/sf1, sf1==0 gives 1.0) | Core/Data/ProjectData.hpp:33-43; Core/Persistence/ProjectStore/Components/Projec ... | ost_visualizer/domain/services/coordinate_transformation_service.py:35-44,82-86; ... | tests/domain/services/test_coordinate_transformation_service.py::OSTCoordinateSystemStateTests; ... | matches | MSH-01 |
| 2D plan scaling OST units <-> canvas points (rotation 0) | PlanView/PlanView.mm:208,258-268,365-375,448,528-532; ... | ost_visualizer/domain/services/coordinate_transformation_service.py:88-104,156-161; ... | tests/domain/services/test_coordinate_transformation_service.py::OSTCoordinateSystemUnitConversionTe ... | matches | GEO-06, RPV-02 |
| OST to PDF/page coordinates (rotation, flips) | Core/Geometry/coord_transform.cpp:8-36,61-137; used at PlanView/PlanView.mm:258- ... | cpp_extensions/src/coord_transform/coord_transform.cpp:52-85; cpp_extensions/src/common/page_transfo ... | tests/domain/services/test_coordinate_transformation_service.py::CanonicalCoordinateValidationTests; ... | diverges | GEO-06 |
| Canvas<->OST mapping for placement, drag and hit-test with page rotation | Core/Geometry/coord_transform.cpp:38-59 (canvas_delta_to_ost, iOS-only); ... | ost_visualizer/presentation/components/plan_view/components/drag_handler.py:236-258; ... | - | diverges | RPV-02 |
| Drag handle geometry (corner, edge, whole shape) with polygon validity and winding guard | Controllers/ViewController/Components/DragController.mm:31-122 | ost_visualizer/presentation/components/plan_view/components/drag_handler.py:89-98,263-265,286-424,48 ... | - | diverges | RPV-09, RPV-08, RPV-07, RPV-04 |
| 2D takeoff footprints (count shapes, linear, curved linear) in the plan view | PlanView/PlanView.mm:250-256,318-331,432-470,507-620; ... | ost_visualizer/presentation/visualization/core/geometry/takeoff_geometry.py:8-9,12-82,95-120; ... | tests/domain/entities/test_shape.py | diverges | RPV-25, GEO-18 |
| Shared vector and mesh data types | Core/Geometry/vec_types.hpp; Core/Geometry/mesh_data.hpp | cpp_extensions/src/common/vec_types.hpp; cpp_extensions/src/mesh/mesh_data.hpp | - | matches | - |
| Page entity geometry fields | Core/Domain/Page.hpp:7-39; Core/Persistence/ProjectStore/Components/ProjectStore ... | ost_visualizer/domain/entities/page.py:19-25,51-56; ost_visualizer/infrastructure/mdb/components/bid ... | - | matches | - |
| Renderer math (Vec3/Vec4/Mat4 lookAt/perspective, 1e-8 epsilons) | Rendering/MathUtils.hpp:1-195 (used by Rendering/Renderer.mm:539-556 and MeshLoa ... | cpp_extensions/src/renderer (GLM based) | - | platform-adapted | - |

### meshing

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| Straight linear wall mesh and MeshFactory linear defaults (height/thickness min 1, z_value ... | Core/Meshing/gen_linear.cpp:7-54; Core/Meshing/MeshFactory.cpp:20-94 | presentation/visualization/meshing/mesh_factory.py:48-89; presentation/visualization/core/mesh_gener ... | tests/presentation/visualization/utils/test_mesh.py::PrismTopologyTableTests::test_box_tables_close_ ... | matches | - |
| Linear slope (rise/run) for straight and curved walls | Core/Meshing/mesh_helpers.hpp:19-28; gen_linear.cpp:23-39; ... | linear_geom.cpp calc_linear_mesh_verts, proc_slope_params, gen_curved_wall_pts | - | matches | - |
| Straight drop stub geometry (generate_drop_stub_mesh) | Core/Meshing/gen_linear.cpp:56-127 | presentation/visualization/core/mesh_generator.py:149-233 | - | matches | - |
| Drop stub union with the wall (boolean_union in MeshFactory) | Core/Meshing/MeshFactory.cpp:61-73,80-92 | meshing/mesh_factory.py:74-89,122-140; core/boolean_operations.py:62-73 | tests/presentation/visualization/core/test_boolean_operations.py | diverges | MSH-19 |
| Curved linear wall mesh (arc fit / Bezier fallback, adaptive segments, end caps) | Core/Meshing/gen_curved.cpp:34-203; MeshFactory.cpp:37-75 | cpp_extensions/src/linear_geom/linear_geom.cpp (curve_pts_impl, gen_adv_curve_pts, calc_adaptive_seg ... | tests/integration/native/test_geometry_boundaries.py::NativeLinearArcSideAndStraightChordTests; ... | diverges | MSH-05, MSH-06, MSH-07 |
| Curved drop stub geometry | Core/Meshing/gen_curved.cpp:205-287 | core/mesh_generator.py:235-320 | - | matches | - |
| Area prism mesh with earcut triangulation | Core/Meshing/gen_area.cpp:35-204; Core/Geometry/earcut_impl.cpp; ... | core/geometry/area.py; core/mesh_generator.py:59-100 | tests/presentation/visualization/meshing/test_mesh_factory.py::MeshFactoryPageFlipTests; ... | matches | - |
| Area slope | Core/Meshing/gen_area.cpp:22-33,47-79 | core/geometry/area.py:40-61 | tests/presentation/visualization/core/geometry/test_area.py | diverges | GEO-05 |
| Count box mesh: square, rectangle and attachment conditions (display_size scaling for coun ... | Core/Meshing/gen_count.cpp:7-39; MeshFactory.cpp:95-143 | core/geometry/takeoff_geometry.py:85-108; core/geometry/radial.py:22-46 | tests/presentation/visualization/meshing/test_mesh_factory.py::ThreejsExportLayerTests::test_square_ ... | matches | MSH-13 |
| Count cylinder (circle shape, 24 segments) | Core/Meshing/gen_count.cpp:41-100; MeshFactory.cpp:133-139 | core/mesh_generator.py:415-456; core/geometry/radial.py:104-125 | tests/presentation/visualization/utils/test_mesh.py::PrismTopologyTableTests::test_radial_tables_clo ... | matches | - |
| Ellipse, triangle, rhombus, hexagon and octagon count meshes | Core/Meshing/MeshFactory.cpp:133-142 (ellipse routed to circular cylinder, all o ... | core/mesh_generator.py:458-624; core/geometry/radial.py:49-101 | tests/integration/rendering/test_threejs_mesh_footprint.py; tests/presentation/visualization/utils/t ... | diverges | MSH-08, MSH-11 |
| Axis and mirror convention (X negated, Z = -y, Y up) | Core/Meshing/MeshFactory.cpp:26-27,39-40,101,161; ProjectService.mm:122; ... | domain/services/coordinate_transformation_service.py:106-123; cpp_extensions/src/coord_transform/coo ... | - | platform-adapted | EXP-03 |
| Page scale applied to mesh coordinates | Services/ProjectService.mm:119,170; Services/ExportService.mm:30,57; ... | infrastructure/visualization_provider.py:114; meshing/mesh_builder.py:106 | tests/integration/rendering/test_page_flip_mesh.py::test_mesh_pipeline_preserves_model_scale_when_fl ... | diverges | MSH-01 |
| Page rotation / flip transform of 3D meshes | absent (Page.rotation/flip_x/flip_y loaded at ProjectStore+Load.cpp:83-90 but un ... | meshing/mesh_builder.py:71-88,143-147; infrastructure/visualization_provider.py:115-140 | tests/integration/rendering/test_page_flip_mesh.py; tests/presentation/visualization/meshing/test_me ... | diverges | MSH-02 |
| Manifold boolean ops (difference, difference_batch, union, intersection, vertex snapping 0 ... | Core/Geometry/boolean_ops.cpp (983 lines) | cpp_extensions/src/boolean/boolean_ops.cpp | tests/presentation/visualization/core/test_boolean_operations.py; ... | matches | - |
| mesh_repair (remove_degenerate_faces, merge_vertices, remove_unreferenced_vertices, repair ... | Core/Geometry/mesh_repair.cpp | cpp_extensions/src/mesh/mesh_repair.cpp | - | matches | - |
| Feature edges (extract_feature_edges, compute_dihedral_angle) and DXF use at 0.1 rad | Core/Geometry/feature_edges.cpp; Services/ExportService.mm:154; ... | cpp_extensions/src/mesh/feature_edges.cpp; exporters/dxf_exporter.py:32 | tests/integration/native/test_boolean_geometry.py (single triangle -> edges {(0,1),(0,2),(1,2)}) | matches | - |
| Per-takeoff mesh orchestration for the 3D view (negatives grouped by condition type, boole ... | Services/ProjectService.mm:103-236 | meshing/mesh_builder.py:91-191; core/boolean_operations.py:26-59 | tests/domain/services/test_takeoff_domain_service.py::TakeoffGroupingTests | diverges | MSH-14, MSH-15, MSH-20, MSH-02, MSH-01 |
| Per-takeoff mesh orchestration for OBJ/DXF/Three.js export | Services/ExportService.mm:17-92 (single generateVisibleMeshes shared by the thre ... | exporters/base_exporter.py:56-215; renderers/threejs/mesh_processor.py:20-110 | - | diverges | EXP-03, MSH-16, EXP-15, EXP-14 |
| MeshLoader: double to float, flat quantized face normals, area-weighted vertex normals (un ... | Rendering/MeshLoader.cpp:3-95; Rendering/Shaders/src/fs_mesh.sc | presentation/visualization/utils/mesh.py:prepare_vertices_for_shading,_compute_face_normals | tests/presentation/visualization/utils/test_mesh.py::ShadingPreparationTests | platform-adapted | MSH-21 |
| earcut wrapper (triangulate_polygon) | Core/Geometry/earcut_impl.cpp | cpp_extensions/src/module_earcut.cpp | tests/integration/native/test_geometry_boundaries.py | matches | - |

### parsers-units

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| OST XML section parsing (parse_ost_xml, section lists, singular()) | Core/Import/OstXmlParser.cpp:8-127; Core/Persistence/OstSchema.cpp:990-1044 | ost_visualizer/infrastructure/mdb/importers/ost_importer.py (_parse_ost_xml 430-482, _matches_sectio ... | tests/infrastructure/mdb/importers/test_ost_importer.py::test_section_children_match_the_singular_or ... | diverges | IMP-11, IMP-02 |
| Takeoff Position text parsing (parse_position, newline-entity stripping, ';' split) | Core/Persistence/ProjectStore/ProjectStore.cpp:123-204 (called from ProjectStore ... | ost_visualizer/domain/utils/position.py:5-17; ost_visualizer/domain/utils/text_cleanup.py | tests/infrastructure/mdb/components/test_serialization.py::test_position_parser_reads_text_and_binar ... | diverges | DBP-08 |
| Takeoff Position text serialisation (serialize_position) | Core/Persistence/ProjectStore/ProjectStore_Internal.hpp:90-102; ... | ost_visualizer/infrastructure/mdb/components/serialization.py (encode_position, preserve_indices); ... | tests/infrastructure/mdb/components/test_serialization.py::test_position_serializer_returns_bytes_fo ... | diverges | DBP-07 |
| Elevation name split/join (parseElevation, reassembleElevation) | Controllers/EditConditionViewController/Components/EditConditionHelpers.mm:47-99 | ost_visualizer/domain/services/elevation.py:25-67 (parse_elevation, reassemble_elevation) | tests/domain/services/test_elevation.py | matches | - |
| z_value / is_top extraction from condition name (extract_z_value) | Core/Persistence/ProjectStore/ProjectStore.cpp:206-407; ... | ost_visualizer/infrastructure/parsers/position_parser.py:31-129 (extract_z_value_from_name); ... | tests/infrastructure/parsers/test_position_parser.py (all classes); ... | diverges | ELV-01, ELV-06 |
| Feet-inch text to inches (displayToInches) used for every dimension field and the elevatio ... | Controllers/EditConditionViewController/Components/EditConditionHelpers.mm:138-3 ... | ost_visualizer/domain/services/dimension_format_service.py:77-165 (display_to_inches, imperial branc ... | tests/domain/services/test_dimension_format_service.py::DimensionParsingTableTests | diverges | DLG-FINITE-01 |
| Inches to feet-inch display text (inchesToDisplay) | Controllers/EditConditionViewController/Components/EditConditionHelpers.mm:101-1 ... | ost_visualizer/domain/services/dimension_format_service.py:26-55 (inches_to_display, imperial branch ... | tests/domain/services/test_dimension_format_service.py::DimensionDisplayTableTests | diverges | FMT-01 |
| Edit Condition dimension validation and save (name/elevation/ref no/dims/display size/rise ... | Controllers/EditConditionViewController/EditConditionViewController.mm:12-31,300 ... | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:166-178,389-410 (_parse_dimension_field ... | - | diverges | - |
| UOM labels, quantity option lists, valid-UOM lists | Core/Domain/UomService.hpp:56-196 | ost_visualizer/domain/services/uom_service.py:39-176 | tests/domain/services/test_uom_service.py (labels test ~L160-181, valid-uoms ~L258-325) | diverges | QTY-16 |
| Imperial UOM divisors (convert_to_uom) | Core/Domain/UomService.hpp:200-237 | ost_visualizer/domain/services/uom_service.py (_UOM_DIVISORS, convert_to_uom) | tests/domain/services/test_uom_service.py | matches | - |
| Quantity number formatting in conditions list (formatQty) | Controllers/ConditionsViewController.mm:46-57,314-332 | ost_visualizer/application/utils/quantity_display.py:4-17; ost_visualizer/presentation/components/co ... | tests/application/utils/test_quantity_display.py | diverges | QTY-12 |
| Condition BGR int to RGB floats (Condition::get_rgb) | Core/Domain/Condition.hpp:63-73; used by MeshFactory.cpp:16, PlanView.mm:243, Pl ... | ost_visualizer/presentation/visualization/services/color_service.py:35-52 (int_to_hex, hex_to_rgb) | tests/presentation/visualization/services/test_color_service.py::test_hex_helpers_round_trip_bgr_int ... | diverges | COL-01 |
| Grayscale conversion | Services/ProjectService.mm:197-200; PlanView/PlanView.mm:245-248; ... | ost_visualizer/presentation/visualization/services/color_service.py:55-59 (_as_grayscale) | tests/presentation/visualization/services/test_color_service.py::test_grayscale_desaturates_colors_a ... | diverges | COL-02 |
| Display-mode alpha (Solid/OST/Transparent) | Services/ProjectService.mm:187-192; PlanView/PlanView.mm:270-277; ... | ost_visualizer/presentation/visualization/services/color_service.py:62-68,80-83,264-298; ... | tests/domain/entities/test_pattern.py; tests/presentation/visualization/services/test_color_service. ... | diverges | COL-03, COL-04, SET-01 |
| Colour picker to ColorFill int | Controllers/EditConditionViewController/Components/EditConditionViewController+P ... | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:326-329 (_on_color_changed) | - | diverges | COL-PICK-01 |
| Snap increment unit conversion (Bid::snap_increments_ost) | Core/Domain/Bid.hpp:24-31 | ost_visualizer/presentation/components/plan_view/view.py:3955-3962 (set_snap_settings) | - | matches | DBP-04 |
| Style/pattern/shape picker labels and ids | Controllers/EditConditionViewController/Components/EditConditionHelpers.mm:10-43 | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:55-89; ... | - | diverges | COND-SHAPE-01 |
| DXF colour helpers (RGBColor::from_hex, to_aci) | Core/Export/dxf_writer.cpp:8-54 | cpp_extensions/src/dxf/dxf_writer.cpp:9-55 | - | matches | - |
| Deleted Bids constants | Core/Persistence/ProjectStore/ProjectStore.cpp:25-26 | ost_visualizer/domain/entities/project_constants.py | - | matches | - |
| Conditions list display name (stripElevation) | Controllers/ConditionsViewController.mm:8-28,315 | ost_visualizer/presentation/components/conditions_sidebar.py:690 (display_name = cond.name); ... | tests/domain/services/test_elevation.py | diverges | QTY-23 |

### import-ost-osp

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| OST XML section parsing (Bid row, BID_SECTIONS, BidPages + per-page PAGE_SECTIONS, tail se ... | Core/Import/OstXmlParser.cpp:8-32,67-127; Core/Persistence/OstSchema.cpp:990-104 ... | ost_visualizer/infrastructure/mdb/importers/ost_importer.py (_parse_ost_xml 427-482, _matches_sectio ... | tests/infrastructure/mdb/importers/test_ost_importer.py::test_parse_reads_each_section_family_from_i ... | diverges | IMP-11 |
| XML attribute-name mapping CopyTimestamp -> CopyTimeStamp | none needed; ProjectStore_Internal.hpp:55-74 inserts attribute names verbatim | ost_visualizer/infrastructure/mdb/importers/ost_importer.py (_database_row 73-77); ... | tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_import_restores_database_column_na ... | platform-adapted | - |
| Malformed or Bid-less OST rejection | Core/Import/OstXmlParser.cpp:74-85; Core/Import/OstXmlParser.hpp:20; ... | ost_visualizer/infrastructure/mdb/importers/ost_importer.py (import_ost 84-99, _parse_ost_xml 427-43 ... | tests/infrastructure/mdb/importers/test_ost_importer.py::test_import_ost_rejects_unreadable_or_malfo ... | matches | - |
| Backslash 'sanitizer' pre-pass on the raw XML | Core/Import/OstXmlParser.cpp:34-65,71 | ost_visualizer/infrastructure/mdb/importers/ost_importer.py (_parse_ost_xml uses plain ET.parse, no ... | - | diverges | IMP-02 |
| OST import orchestration (read file, parse, create schema, duplicate-Bid guard, import, no ... | Services/ImportService.mm:28-109 | ost_visualizer/infrastructure/mdb/importers/ost_importer.py (import_ost 84-99, _validated_raw_data 1 ... | tests/infrastructure/mdb/importers/test_ost_importer.py; tests/infrastructure/mdb/components/test_im ... | diverges | IMP-09, IMP-10, IMP-06, IMP-01, IMP-08, IMP-19 |
| Row insertion into SQLite (import_project) | Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:8-27,94-246; ... | ost_visualizer/infrastructure/mdb/components/import_operations.py (_write_to_db 384-424, _insert_raw ... | tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_writes_bid_tables_ ... | diverges | IMP-05, IMP-06, IMP-08 |
| Global table reconciliation: CdnTypes and JobStatuses by Name | Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:29-92,106-116,1 ... | ost_visualizer/infrastructure/mdb/components/import_operations.py (_resolve_cdn_types 150-188, _reso ... | tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_uses_condition_typ ... | diverges | IMP-10 |
| Global Employees / AccessLevels pass-through | Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:213-242 | ost_visualizer/infrastructure/mdb/components/import_operations.py (_resolve_access_levels 232-282, _ ... | tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_employees_reuse_bu ... | diverges | IMP-11 |
| Duplicate import guard (is_bid_imported) | Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:248-262; ... | - | - | diverges | IMP-19 |
| SQLite table and column coverage used by the import | Core/Persistence/OstSchema.cpp:6-947 | ost_visualizer/infrastructure/mdb/database_creator.py (_TABLE_DDL) | tests/fixtures/reference_mdb_schema.json | matches | IMP-05, IMP-20, IMP-08 |
| CAB header, folder and file-table parsing, list_cab | Core/Import/CabExtractor.cpp:49-164,260-271 | cpp_extensions/src/cab/cab_compressor.cpp (list_cab 521-545, fdi_notify 380-423); ... | tests/integration/native/test_osp_cab.py::test_osp_imports_legacy_ansi_members_from_unicode_archive_ ... | unverified | IMP-18 |
| CAB folder decompression (stored and MSZIP) | Core/Import/CabExtractor.cpp:166-258,273-318 | cpp_extensions/src/cab/cab_compressor.cpp (FDICopy via extract_cab 495-520) | tests/integration/native/test_osp_cab.py; tests/infrastructure/mdb/importers/test_osp_importer.py::O ... | diverges | IMP-13, IMP-17 |
| OSP import orchestration: pick .ost member, extract PDFs, rewrite ImagePath/OverlayImagePa ... | Services/ImportService.mm:111-331 | ost_visualizer/infrastructure/mdb/importers/osp_importer.py (import_osp 196-219, _extracted_ost 236- ... | tests/infrastructure/mdb/importers/test_osp_importer.py; tests/integration/native/test_osp_cab.py | diverges | IMP-12, IMP-14, IMP-15, IMP-16 |
| SceneDelegate URL import (cold start and openURLContexts) | SceneDelegate.mm:23-71 | ost_visualizer/application/dtos/file_import_args.py (parse_project_file_args 35-67); ... | - | platform-adapted | - |
| DatabaseViewController document-picker import | Controllers/DatabaseViewController.mm:102-135 | ost_visualizer/presentation/handlers/import_handler.py (_import_file 91-213) | - | platform-adapted | - |

### export-formats

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| OBJ text format: 'v %.6f' lines, 1-based 'f' indices with running global vertex offset, em ... | Core/Export/obj_writer.cpp:12-24,42-61,117-150 | ost_visualizer/presentation/visualization/exporters/adapters/obj_writer.py; ... | tests/presentation/visualization/exporters/adapters/test_obj_writer.py::test_coordinate_winding_and_ ... | matches | EXP-13, EXP-17 |
| OBJ group/material naming, per-condition grouping, MTL body and comment/header lines | Services/ExportService.mm:102-134; Core/Export/obj_writer.cpp:84-102,152-174 | exporters/base_exporter.py:109-149,172-198; exporters/obj_exporter.py:18-84 | tests/presentation/visualization/exporters/test_obj_exporter.py | diverges | EXP-11, EXP-12, EXP-16 |
| OBJ axis transform and face winding | Core/Export/obj_writer.cpp:132-143 (no transform and no face swap); ... | exporters/adapters/obj_writer.py:32-43 | tests/presentation/visualization/exporters/adapters/test_obj_writer.py | diverges | EXP-13, EXP-17 |
| DXF writer (native ost_dxf): sections, tables, layers, ACI/true colour, handles, %.6f grou ... | Core/Export/dxf_writer.cpp, Core/Export/dxf_writer.hpp | cpp_extensions/src/dxf/dxf_writer.cpp; cpp_extensions/src/dxf/dxf_writer.hpp | tests/presentation/visualization/exporters/test_dxf_exporter.py (mock-level only; ... | matches | - |
| DXF export flow: one layer per condition, feature edges at 0.1 rad, positive meshes only | Services/ExportService.mm:136-169 | exporters/dxf_exporter.py:12-39 | tests/presentation/visualization/exporters/test_dxf_exporter.py::test_writer_is_reset_and_receives_o ... | diverges | EXP-02, EXP-06, EXP-11, EXP-12 |
| Shared geometry used by export: feature-edge extraction, mesh repair, boolean ops, mesh_da ... | Core/Geometry/feature_edges.cpp, mesh_repair.cpp, boolean_ops.cpp, mesh_data.hpp | cpp_extensions/src/mesh/feature_edges.cpp; cpp_extensions/src/mesh/mesh_repair.cpp | - | matches | - |
| Negative (backout) subtraction in OBJ/DXF export: all negatives subtracted from all positi ... | Services/ExportService.mm:71-89 | exporters/base_exporter.py:151-170; cpp_extensions/src/module_geometry.cpp:15-106 | tests/presentation/visualization/exporters/test_base_exporter.py | matches | EXP-05, EXP-22 |
| Visibility rule for OBJ/DXF: hidden-layer conditions are skipped, hidden-layer takeoffs ne ... | Services/ExportService.mm:52-53; Core/Data/ProjectData.hpp:47-54 | domain/services/project_data_service.py:197-216; domain/entities/takeoff.py:127-131 | tests/application/services/test_export_service.py::test_non_html_export_keeps_visible_only_collectio ... | matches | EXP-15, LP-22 |
| Mesh collection for export (page filter, visibility, hole map, scale, boolean, ordering) | Services/ExportService.mm:17-92 | exporters/base_exporter.py:39-209; domain/services/takeoff_domain_service.py:50-86 | tests/domain/services/test_takeoff_domain_service.py::test_area_holes_are_grouped_under_their_area_p ... | diverges | EXP-03, MSH-01, EXP-05, MSH-14, EXP-14, EXP-18 |
| Three.js HTML writer (scene JSON, camera, template) | Core/Export/threejs_writer.cpp | renderers/threejs/threejs_renderer.py; renderers/threejs/mesh_processor.py | tests/presentation/visualization/renderers/threejs/adapters/test_threejs_mesh_adapter.py; ... | diverges | EXP-01, EXP-08, EXP-15, EXP-19, EXP-20 |
| Export file naming | Services/ExportService.mm:94-98,117-120,159-161,188-190 | infrastructure/visualization_provider.py:55-89; infrastructure/utils/filename.py | tests/infrastructure/test_visualization_provider.py::ExportFilenameTests; ... | diverges | EXP-09 |
| Colour derivation for export (ColorFill BGR int to RGB) | Core/Domain/Condition.hpp:57-66; Services/ExportService.mm:148-151 | presentation/visualization/services/color_service.py:35-62,230-279 | tests/presentation/visualization/services/test_color_service.py | diverges | EXP-06, COL-01 |
| Action sheet, temp-file writing and UIActivityViewController share flow | Controllers/ViewController/Components/ViewController+Export.mm:5-79; ... | presentation/handlers/export_handler.py (save dialog, deferred persistence flush, bid-changed guard) | tests/presentation/handlers/test_export_handler.py | platform-adapted | - |
| Empty-export / failure messaging | Controllers/ViewController/Components/ViewController+Export.mm:42-53; ... | application/services/export_service.py:74-148; presentation/visualization/exporters/base_exporter.py ... | tests/application/services/test_export_service.py; tests/presentation/visualization/exporters/test_b ... | diverges | EXP-21, EXP-22 |

### database-persistence

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| Table/column DDL: 64 desktop tables plus _AppMeta and _AppPreferences | Core/Persistence/OstSchema.cpp:6-807, ProjectStore.cpp:409-562 (create_schema) | ost_visualizer/infrastructure/mdb/database_creator.py (_TABLE_DDL 18-815); ... | tests/infrastructure/mdb/test_database_creator.py; tests/infrastructure/mdb/access_reference_ddl_gol ... | matches | - |
| Reference schema metadata: 61 FIELD_DEFAULTS, 83 enforced relationships, UID-required | Core/Persistence/OstSchema.cpp:6-807 (no DEFAULT/REFERENCES clauses) | ost_visualizer/infrastructure/mdb/reference_schema_metadata.py; ... | tests/infrastructure/mdb/test_reference_schema_metadata.py; tests/integration/mdb/test_relationship_ ... | diverges | DBP-33 |
| Index set (FK_INDEXES, Locking unique indexes) | Core/Persistence/OstSchema.cpp:809-952 | ost_visualizer/infrastructure/mdb/reference_schema_metadata.py:130-327 | tests/infrastructure/mdb/test_reference_schema_metadata.py | matches | - |
| Seed rows: Settings, 'Deleted Bids' project UID 1, 4 default layers, 26 SchemaRegistry ver ... | Core/Persistence/ProjectStore/ProjectStore.cpp:499-556 | ost_visualizer/infrastructure/mdb/database_creator.py:1109-1152; ... | tests/infrastructure/mdb/test_database_creator.py | matches | - |
| create_schema bootstrap on an existing file (CREATE IF NOT EXISTS, per-call seeding) | Core/Persistence/ProjectStore/ProjectStore.cpp:409-562 | ost_visualizer/infrastructure/mdb/database_creator.py (DatabaseCreator.create_database) | tests/infrastructure/mdb/test_database_creator.py | platform-adapted | - |
| 'Deleted Bids' system project resolved by name, with duplicate merging | Core/Persistence/ProjectStore/ProjectStore.cpp:25-121, Components/ProjectStore+F ... | ost_visualizer/domain/entities/project_constants.py (UID '1' constant); ... | tests/infrastructure/mdb/components/test_project_operations.py (test_the_system_project_is_refused_a ... | platform-adapted | - |
| OstDatabase RAII wrapper (open/WAL/foreign_keys, Statement, begin/commit/rollback) | Core/Persistence/OstDatabase.hpp/.cpp | ost_visualizer/infrastructure/mdb/mdb_writer.py:63-105 (_connection transaction/rollback); ... | tests/integration/mdb/test_ownership_and_atomicity.py; tests/infrastructure/mdb/components/test_proj ... | diverges | DBP-11 |
| import_project (.ost rows to SQLite) | Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:94-246; ... | ost_visualizer/infrastructure/mdb/components/import_operations.py; ... | tests/infrastructure/mdb/importers/test_ost_importer.py; tests/infrastructure/mdb/components/test_im ... | diverges | IMP-06, IMP-08, IMP-05, IMP-10, BRW-13 |
| OST XML section constants (BID/PAGE/TAIL/GLOBAL sections, singular()) | Core/Persistence/OstSchema.cpp:990-1044 | ost_visualizer/infrastructure/mdb/schema_contract.py:3-79 | tests/infrastructure/mdb/test_schema_contract.py | diverges | IMP-11 |
| is_bid_imported duplicate guard | Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp:248-262; ... | (none: Windows allocates a fresh Bid UID per import) | - | platform-adapted | IMP-06 |
| load_hierarchy (project folders and bids list) | Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp:20-74 | ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:21-146 | tests/infrastructure/mdb/components/test_hierarchy_reader.py (test_full_hierarchy_maps_every_bid_fie ... | diverges | LP-08 |
| load_project: bid row, pages, layers, CdnTypes | Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:15-136 | ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:473-503,670-770; ... | tests/infrastructure/mdb/components/test_bid_data_reader.py | diverges | LP-20, COND-FOLDER-01, DBP-04 |
| load_project: conditions | Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:154-236 | ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:810-1008 | tests/infrastructure/mdb/components/test_bid_data_reader.py (ConditionDisplaySizeTests) | diverges | DBP-03, QTY-06, COND-FOLDER-01, ELV-01 |
| load_project: takeoffs | Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:238-270 | ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:1079-1200 | tests/infrastructure/mdb/components/test_bid_data_reader.py (test_explicit_zero_and_null_markers_are ... | diverges | QTY-13, DBP-31 |
| load_project: page-folder tree (BidPageFolders) | Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:272-345 | ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:148-266; ... | tests/infrastructure/mdb/components/test_hierarchy_reader.py (test_page_hierarchy_accepts_valid_mult ... | diverges | DBP-20 |
| parse_position (position text to doubles) | Core/Persistence/ProjectStore/ProjectStore.cpp:161-204 | ost_visualizer/domain/utils/position.py:6-19; ost_visualizer/infrastructure/mdb/components/serializa ... | tests/integration/persistence/test_position_contracts.py; tests/infrastructure/mdb/components/test_s ... | diverges | DBP-08 |
| extract_z_value (condition name to elevation inches and top/bottom) | Core/Persistence/ProjectStore/ProjectStore.cpp:206-407 | ost_visualizer/infrastructure/parsers/position_parser.py:31-130; ... | tests/infrastructure/parsers/test_position_parser.py; tests/infrastructure/mdb/components/test_bid_d ... | diverges | ELV-01 |
| serialize_position (takeoff position text written to BidTakeoffs.Position) | Core/Persistence/ProjectStore/ProjectStore_Internal.hpp:90-102 | ost_visualizer/infrastructure/mdb/components/serialization.py:18-28 (encode_position); ... | tests/infrastructure/mdb/components/test_takeoff_operations.py:1501 (test_insert_preserves_unrounded ... | diverges | DBP-07 |
| Folder CRUD: create_folder, rename_folder, delete_folder (BidProjects) | Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp:76-170 | ost_visualizer/infrastructure/mdb/components/project_operations.py:113-208 | tests/infrastructure/mdb/components/test_project_operations.py | diverges | DBP-18, COND-DEL-01 |
| delete_project (soft delete to Deleted Bids, OrigBidProjectUID) | Core/Persistence/ProjectStore/ProjectStore.cpp:564-613 | ost_visualizer/infrastructure/mdb/components/project_operations.py:15-81; ... | tests/infrastructure/mdb/components/test_project_operations.py (test_move_assigns_the_project_and_th ... | diverges | BRW-11 |
| permanently_delete_project (hard delete of a bid in the trash) | Core/Persistence/ProjectStore/ProjectStore.cpp:615-751 | ost_visualizer/infrastructure/mdb/components/bid_operations.py:82-244 | tests/infrastructure/mdb/components/test_bid_operations.py:64,261,3020,3513 | diverges | DBP-12 |
| assign_bid_to_folder / remove_bid_from_folder (move or orphan bids) | Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp:172-206; ... | ost_visualizer/infrastructure/mdb/components/project_operations.py:15-111; ... | tests/infrastructure/mdb/components/test_project_operations.py | diverges | DBP-17, BRW-09 |
| set_layer_visibility (BidLayers.Show -1/0) | Core/Persistence/ProjectStore/Components/ProjectStore+Takeoffs.cpp:7-25 | ost_visualizer/infrastructure/mdb/components/layer_operations.py:43-55,259-352 | tests/infrastructure/mdb/components/test_layer_operations.py | matches | DBP-17, DBP-35 |
| save_takeoff_position | Core/Persistence/ProjectStore/Components/ProjectStore+Takeoffs.cpp:27-45 | ost_visualizer/infrastructure/mdb/components/takeoff_operations.py:245-270 | tests/infrastructure/mdb/components/test_takeoff_operations.py:1792,1833 | diverges | DBP-07, DBP-17, DBP-35 |
| insert_takeoff | Core/Persistence/ProjectStore/Components/ProjectStore+Takeoffs.cpp:47-98 | ost_visualizer/infrastructure/mdb/components/takeoff_operations.py:421-565 | tests/infrastructure/mdb/components/test_takeoff_operations.py:1490-1507 | diverges | DBP-18, DBP-19, DBP-17 |
| delete_takeoff (with hole children) | Core/Persistence/ProjectStore/Components/ProjectStore+Takeoffs.cpp:100-192 | ost_visualizer/infrastructure/mdb/components/takeoff_operations.py:334-419 | tests/infrastructure/mdb/components/test_takeoff_operations.py:183,215,249 | diverges | DBP-14 |
| set_takeoff_negative | Core/Persistence/ProjectStore/Components/ProjectStore+Takeoffs.cpp:194-212 | ost_visualizer/infrastructure/mdb/components/takeoff_operations.py:199-208 | tests/infrastructure/mdb/components/test_takeoff_operations.py | matches | DBP-17, DBP-34 |
| update_condition (persist edited condition) | Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:7-81 | ost_visualizer/infrastructure/mdb/components/condition_operations.py:625-700 | tests/infrastructure/mdb/components/test_condition_operations.py:620 | diverges | COND-UPD-01 |
| shift_condition_ref_nos (RefNo conflict shift) with caller gating | Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:83-104; ... | ost_visualizer/infrastructure/mdb/components/condition_operations.py:591-623 | tests/infrastructure/mdb/components/test_condition_operations.py:620 (test_condition_update_ref_no_s ... | matches | DBP-35 |
| duplicate_condition | Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:106-168 | ost_visualizer/infrastructure/mdb/components/condition_operations.py:80-133 | tests/infrastructure/mdb/components/test_condition_operations.py:231,1359; ... | diverges | COND-DUP-01 |
| delete_condition (condition cascade) | Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:170-261 | ost_visualizer/infrastructure/mdb/components/condition_operations.py:334-546 | tests/infrastructure/mdb/components/test_condition_operations.py:74,181,281 | diverges | DBP-12, COND-DUP-01, DBP-14 |
| RenderConfig persistence (color_mode, grayscale_enabled in _AppPreferences) | Core/Domain/RenderConfig.hpp; Core/Persistence/ProjectStore/Components/ProjectSt ... | ost_visualizer/domain/entities/config.py:18-26,56-58 (JSON app config: display_mode_3d/2d, grayscale ... | tests/domain/entities (config tests) | platform-adapted | SET-01 |
| ProjectData helpers (page_scale_ratio, is_condition_visible, project_name fallback) | Core/Data/ProjectData.hpp:19-53 | ost_visualizer/application/services/page_visualization_metadata_service.py:20-22; ... | tests/domain/entities/test_layer.py | diverges | DBP-08, DBP-34 |

### conditions

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| Condition entity: field set, defaults, type/shape/pattern ids | Core/Domain/Condition.hpp:8-56, Core/Domain/Types.hpp:5-31 | ost_visualizer/domain/entities/condition.py; ost_visualizer/domain/entities/shape.py | tests/domain/entities/test_condition.py | diverges | COND-EXTRA-01 |
| CdnType entity | Core/Domain/CdnType.hpp:7-11 | ost_visualizer/domain/entities/cdn_type.py | - | matches | - |
| ConditionFolder entity | Core/Domain/ConditionFolder.hpp:7-11, Core/Persistence/ProjectStore/Components/P ... | ost_visualizer/domain/entities/condition_folder.py; ost_visualizer/infrastructure/mdb/components/bid ... | tests/infrastructure/mdb/components/test_condition_folder_operations.py | diverges | COND-FOLDER-01 |
| Condition load from SQLite (BidConditions row -> Condition) | Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:154-236 | ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:810-1008 | tests/infrastructure/mdb/components/test_bid_data_reader.py; tests/domain/services/test_uom_service. ... | diverges | QTY-06, ELV-01 |
| Elevation z_value/is_top derived from condition name | Core/Persistence/ProjectStore/ProjectStore.cpp:206-405 (extract_z_value) | ost_visualizer/infrastructure/parsers/position_parser.py:109-130; ... | tests/infrastructure/parsers/test_position_parser.py; tests/domain/services/test_elevation.py | diverges | ELV-01 |
| Elevation name parse/reassemble (' @T v' / ' @B v' / ' @ v') | Controllers/EditConditionViewController/Components/EditConditionHelpers.mm:47-99 | ost_visualizer/domain/services/elevation.py:25-62 | tests/domain/services/test_elevation.py | matches | COND-ELEV-01 |
| update_condition persistence | Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:7-81 | ost_visualizer/infrastructure/mdb/components/condition_operations.py:548-700; ... | tests/infrastructure/mdb/components/test_condition_operations.py; ... | diverges | COND-UPD-01, COND-UPD-02, COND-UPD-03 |
| RefNo conflict shift when a condition takes an existing number | Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:83-104; ... | ost_visualizer/infrastructure/mdb/components/condition_operations.py:591-623 | tests/infrastructure/mdb/components/test_condition_operations.py::test_condition_update_ref_no_shift ... | matches | - |
| Duplicate condition | Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:106-168; ... | ost_visualizer/infrastructure/mdb/components/condition_operations.py:80-133; ... | tests/infrastructure/mdb/components/test_condition_operations.py::ConditionDuplicateContractTests | diverges | COND-DUP-01, DBP-18 |
| Delete condition (+ cascade, in-memory removal) | Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:170-261; ... | ost_visualizer/infrastructure/mdb/components/condition_operations.py:334-546; ... | tests/infrastructure/mdb/components/test_condition_operations.py::ConditionDeleteCascadeContractTest ... | diverges | COND-DEL-01 |
| Conditions list: sections, grouping by type, ordering | Controllers/ConditionsViewController.mm:126-225 | ost_visualizer/presentation/components/conditions_sidebar.py:423-510,606-710 | tests/presentation/components/test_conditions_sidebar.py::test_condition_sidebar_groups_unassigned_c ... | diverges | COND-LIST-01, COND-FOLDER-01 |
| Conditions list row text (ref no, name, qty1-3 with uom labels) | Controllers/ConditionsViewController.mm:8-57,299-362 | ost_visualizer/presentation/components/conditions_sidebar.py:687-766; ... | tests/presentation/components/test_conditions_sidebar.py::ConditionsSidebarZeroQuantityTests | diverges | QTY-22, QTY-23, QTY-11, COND-PLACE-01 |
| Select a condition in the list to start placing takeoffs | Controllers/ViewController/Components/ViewController+Conditions.mm:45-65 | ost_visualizer/presentation/coordinators/placement_coordinator.py:52-72,127-129; ... | tests/presentation/coordinators/test_placement_coordinator.py::test_enter_rejects_unplaceable_reques ... | diverges | COND-PLACE-01 |
| Long-press Duplicate / Delete action sheet with delete confirmation | Controllers/ConditionsViewController.mm:407-508 | ost_visualizer/presentation/components/conditions_sidebar.py:900-1180; ... | tests/presentation/handlers/test_condition_action_handler.py | platform-adapted | COND-DEL-01 |
| Edit dialog: dimension rows per condition type (Count/Attachment, Linear, Area) | Controllers/EditConditionViewController/EditConditionViewController.mm:173-207 | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:537-569,832-917 | tests/presentation/dialogs/test_edit_condition_dialog.py | matches | QTY-10 |
| Edit dialog: appearance (color, pattern, spacing only for line patterns, shape for count-l ... | EditConditionViewController+TableView.mm:53-63,259-310; ... | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:571-613,1178-1195 | tests/presentation/dialogs/test_edit_condition_dialog.py::test_shared_color_picker_preserves_ost_int ... | matches | COND-SHAPE-01, COND-UPD-02 |
| Edit dialog: style (type) change locked when takeoffs exist | EditConditionViewController.mm:39-45,72; EditConditionViewController+Pickers.mm: ... | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1373-1387; ... | tests/presentation/dialogs/test_edit_condition_dialog.py::test_edit_condition_dialog_initializes_sty ... | matches | - |
| Edit dialog: defaults applied on style change | EditConditionViewController.mm:346-424 | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:90-143,1197-1303,1442-1448 | tests/presentation/dialogs/test_edit_condition_dialog.py::test_shared_fields_carry_over_and_type_spe ... | diverges | COND-STYLE-01, COND-STYLE-02 |
| Edit dialog: Results (3 quantity slots, calc-type lists per type, UOM per calc type) | EditConditionViewController+Pickers.mm:221-303; Core/Domain/UomService.hpp:85-19 ... | ost_visualizer/domain/services/uom_service.py:68-189; ost_visualizer/presentation/dialogs/edit_condi ... | tests/domain/services/test_uom_service.py::UomValidUnitTests | diverges | QTY-16, COND-METRIC-01 |
| Edit dialog: validation and save flow | EditConditionViewController.mm:300-312,546-701 | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1406-1775; ... | tests/presentation/dialogs/test_edit_condition_dialog.py; tests/application/use_cases/project/test_u ... | diverges | COND-REF-01, COND-VAL-01, QTY-10, COND-ELEV-01, COND-LAYER-01, COND-UPD-02 |
| Edit dialog: advanced options per type | EditConditionViewController.mm:209-261; EditConditionViewController+TableView.mm ... | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:657-797 | tests/presentation/dialogs/test_edit_condition_dialog.py::test_count_attachment_advanced_properties_ ... | diverges | COND-EXTRA-01, COND-TRIM-01 |
| Edit dialog: layer and condition-type pickers | EditConditionViewController.mm:82-112; EditConditionViewController+Pickers.mm:78 ... | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:974-1163; ... | tests/presentation/dialogs/test_edit_condition_dialog.py::test_edit_condition_preserves_duplicate_na ... | diverges | COND-LAYER-01, COND-CDNPICK-01 |
| Edit dialog: Previous/Next with unsaved-changes prompt | EditConditionViewController.mm:263-293,711-801; ViewController+Conditions.mm:72- ... | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1777-1839; ... | tests/integration/conditions/test_editor_navigation.py | platform-adapted | COND-NAV-01 |
| Post-save refresh of meshes, plan overlays, list and quantities | ViewController+Conditions.mm:140-151 | ost_visualizer/application/condition_change_impact.py | tests/application/test_condition_change_impact.py | platform-adapted | - |
| Dimension text formatting and parsing (feet/inches/eighths) | EditConditionHelpers.mm:106-136 (inchesToDisplay), 156-306 (displayToInches) | ost_visualizer/domain/services/dimension_format_service.py | tests/domain/services/test_dimension_format_service.py | diverges | FMT-01, DLG-FINITE-01 |
| UOM labels, divisors, quantity unit conversion | Core/Domain/UomService.hpp:22-83,198-237 | ost_visualizer/domain/services/uom_service.py:6-64,194-211 | tests/domain/services/test_uom_service.py | diverges | QTY-16, COND-METRIC-01 |
| Render config load/save (colour mode, grayscale) housed in the conditions store file | Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:263-290 | ost_visualizer/infrastructure/mdb/components/settings_operations.py | tests/infrastructure/mdb/components/test_settings_operations.py | unverified | - |

### layers-pages

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| Layer entity + Show flag load | Core/Domain/Layer.hpp; Core/Persistence/ProjectStore/Components/ProjectStore+Loa ... | ost_visualizer/domain/entities/layer.py (Layer, BidLayer); ost_visualizer/infrastructure/mdb/compone ... | tests/domain/entities/test_layer.py; tests/infrastructure/mdb/components/test_bid_data_reader.py | matches | LP-24 |
| Condition visibility from layer (is_condition_visible) | Core/Data/ProjectData.hpp:44-52 | ost_visualizer/domain/entities/layer.py:is_layer_visible; ost_visualizer/domain/entities/takeoff.py: ... | tests/domain/entities/test_layer.py::LayerEntityContractTests (unknown layer visible, blank uid visi ... | matches | LP-22 |
| Layer visibility filter in 3D mesh build | Services/ProjectService.mm:165; Services/ExportService.mm:52 | ost_visualizer/domain/services/project_data_service.py:197-216 (collect_takeoffs_for_pages visible_o ... | tests/domain/services/test_project_data_service.py::test_layer_visibility_updates_condition_memory_i ... | matches | EXP-15 |
| Layer visibility filter in 2D overlays | PlanView/PlanView.mm:239 | ost_visualizer/presentation/components/plan_view/view.py (hidden_layer_uids) | tests/integration/visibility/test_deferred_layers.py | matches | - |
| Layers sheet (list, per-layer toggle, Show All / Hide All) | Controllers/ViewController/Components/ViewController+Layers.mm:8-169 | ost_visualizer/presentation/components/layers_sidebar.py; ost_visualizer/application/services/projec ... | tests/application/services/test_project_read_service.py::test_merged_bid_layers_hide_comments_layer_ ... | diverges | LP-18, LP-22, LP-24 |
| Layer Show persistence | Core/Persistence/ProjectStore/Components/ProjectStore+Takeoffs.cpp:7-25; ... | ost_visualizer/infrastructure/mdb/components/layer_operations.py:43-55,259-352 | tests/infrastructure/mdb/components/test_layer_operations.py | matches | - |
| Default (template) layer seeding on new database | Core/Persistence/ProjectStore/ProjectStore.cpp:525-541; ... | ost_visualizer/infrastructure/mdb/schema_contract.py:59-64 (DEFAULT_LAYER_ROWS); ... | tests/infrastructure/mdb/test_schema_contract.py | matches | - |
| Image layer controls page image visibility | (absent) PlanView/PlanView.mm:126-188 draws the PDF unconditionally | ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:670-770 (Page.layer_visible = image ... | tests/domain/services/test_project_data_service.py::test_image_layer_visibility_updates_page_memory_ ... | diverges | LP-15 |
| Page entity + BidPages load | Core/Domain/Page.hpp; Core/Persistence/ProjectStore/Components/ProjectStore+Load ... | ost_visualizer/domain/entities/page.py; ost_visualizer/domain/entities/page_info.py | tests/domain/entities/test_page.py; tests/infrastructure/mdb/components/test_bid_data_reader.py | diverges | LP-20 |
| Page ordering and default page order | Services/ProjectService.mm:49-54; Controllers/ViewController/Components/ViewCont ... | ost_visualizer/infrastructure/mdb/components/bid_data_reader.py:711-713; ... | tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_page_info_carries_every_stored_fi ... | diverges | LP-08 |
| Page folder tree build + ownership | Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:272-345; ... | ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:148-266; ... | tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_page_hierarchy_accepts_valid_mult ... | diverges | DBP-20 |
| 3D page selection (multi-select, toggle a page) | Services/ProjectService.mm:72-80; ViewController+PageSelector.mm:271-296 | ost_visualizer/presentation/components/page_combo.py:293-345; ost_visualizer/domain/services/page_se ... | tests/domain/services/test_page_selection_service.py; tests/presentation/components/test_page_combo. ... | matches | LP-10 |
| Folder selection | Services/ProjectService.mm:90-100; ViewController+PageSelector.mm:271-296,318-36 ... | ost_visualizer/presentation/components/page_combo.py:322-345 | tests/presentation/components/test_page_combo.py::test_folder_checkbox_keeps_pages_checked_elsewhere ... | diverges | LP-10 |
| 2D active page selection | Services/ProjectService.mm:82-88; ViewController+PageSelector.mm:298-316 | ost_visualizer/presentation/components/page_combo.py:398-399,414-421 | tests/presentation/components/test_page_combo.py::test_clicking_a_page_row_activates_it_without_togg ... | diverges | LP-11 |
| Initial page selection on bid open | Services/ProjectService.mm:64-67 | ost_visualizer/presentation/coordinators/ui_event_coordinator.py:1136-1169; ... | tests/integration/navigation/test_bid_switch_last_page.py | diverges | LP-12 |
| Two page bars (3D multi-select bar, 2D single bar) | ViewController+PageSelector.mm:41-269 | ost_visualizer/presentation/components/page_combo.py (PageComboBox, SinglePageComboBox) | tests/presentation/components/test_page_combo.py | platform-adapted | - |
| Page scale ratio (sf2/sf1) and defaults | Core/Data/ProjectData.hpp:33-43; ProjectStore+Load.cpp:77-82; ... | ost_visualizer/domain/services/coordinate_transformation_service.py:35-44,82-86; ... | tests/domain/services/test_coordinate_transformation_service.py; ... | matches | MSH-01 |
| Page rotation/flip/scale feeding 3D geometry | Services/ProjectService.mm:132-236; Core/Meshing/MeshFactory.cpp | ost_visualizer/presentation/visualization/meshing/mesh_builder.py:71-88,146-149; ... | tests/integration/rendering/test_page_flip_mesh.py | diverges | MSH-01, MSH-02 |
| ost_to_pdf_coordinates (OST units to page points) | Core/Geometry/coord_transform.cpp:8-137; PlanView/PlanView.mm:258-262; ... | cpp_extensions/src/coord_transform/coord_transform.cpp:52-85; cpp_extensions/src/common/page_transfo ... | tests/domain/services/test_coordinate_transformation_service.py | diverges | GEO-06 |
| PlanView page canvas and PDF background | PlanView/PlanView.mm:126-188; PlanView/Components/PDFCanvasView.mm:8-48 | ost_visualizer/application/services/page_load_strategy_service.py; ... | tests/domain/entities/test_page.py; tests/integration/pages/test_adjust_images_apply.py | diverges | LP-16 |
| Bid snap increments | Core/Domain/Bid.hpp:23-31; ProjectStore+Load.cpp:35 | ost_visualizer/presentation/components/plan_view/view.py:3955-3962; ... | tests/presentation/components/plan_view/test_view.py (set_snap_settings 5.0/0 -> 5.0, 50.8/1 -> 2.0, ... | diverges | DBP-04 |
| Per-page condition quantity totals | Controllers/ViewController/ViewController.mm:708-727; ... | ost_visualizer/presentation/coordinators/sidebar_coordinator.py:223-258; ... | tests/presentation/coordinators/test_sidebar_coordinator.py::test_sidebar_quantities_use_selected_pa ... | diverges | QTY-11, QTY-01 |
| Export page selection and hole grouping | Services/ExportService.mm:15-107; ViewController+Export.mm:62-80 | ost_visualizer/application/services/export_service.py:76-155; ost_visualizer/domain/services/takeoff ... | tests/application/services/test_export_service.py | diverges | EXP-03, EXP-14, EXP-15 |
| Page overlay metadata fields (path, offset, rotation, deskew, resized, rect) | Core/Domain/Page.hpp:24-35; ProjectStore+Load.cpp:87-97 | ost_visualizer/domain/entities/overlay.py; ost_visualizer/domain/entities/page.py:152-342 | tests/infrastructure/mdb/components/test_overlay_rect.py; tests/integration/geometry/test_overlay_st ... | unverified | - |
| Conditions on hidden layers (placement guard) | Controllers/ViewController/Components/ViewController+Conditions.mm:48-64; ... | ost_visualizer/presentation/coordinators/ui_event_coordinator.py:1492-1545,2973-2975; ... | tests/integration/visibility/test_deferred_layers.py; tests/presentation/coordinators/test_sidebar_c ... | diverges | COND-PLACE-01 |
| Condition layer picker (Edit Condition) | Controllers/EditConditionViewController/EditConditionViewController.mm:88-110 | ost_visualizer/presentation/dialogs/edit_condition_dialog.py:1110-1150; ... | tests/application/services/test_project_read_service.py::test_merged_bid_layers_hide_comments_layer_ ... | diverges | COND-LAYER-01 |
| Delete project removes BidLayers/BidPages/BidPageFolders rows | Core/Persistence/ProjectStore/ProjectStore.cpp:731-735 | ost_visualizer/application/use_cases/project/delete_bids_use_case.py; ... | - | unverified | - |

### rendering-planview

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| 2D page canvas and PDF background (PDFCanvasView) | PlanView/PlanView.mm:126-188; PlanView/Components/PDFCanvasView.mm:1-50 | ost_visualizer/presentation/components/plan_view/components/page_loader.py; ... | tests/presentation/components/plan_view/components/test_page_loader.py | diverges | LP-16 |
| OST<->canvas coordinate mapping (rotation, flips, deltas) | Core/Geometry/coord_transform.cpp:8-137; PlanView/PlanView.mm:258-268; ... | cpp_extensions/src/coord_transform/coord_transform.cpp; cpp_extensions/src/common/page_transform.hpp | tests/domain/services/test_coordinate_transformation_service.py; ... | diverges | GEO-06, RPV-02, RPV-03 |
| Linear takeoff overlay (straight band) | PlanView/PlanView.mm:436-482, 253-256 | presentation/visualization/pdf/renderers/takeoff_renderer.py:540-593; ... | tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py::test_linear_thickness_scale ... | diverges | RPV-27, COL-04 |
| Area takeoff overlay with holes | PlanView/PlanView.mm:484-506, 294-317 | takeoff_renderer.py:617-736; domain/services/takeoff_domain_service.py:63-86 | tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py::test_only_real_area_parents ... | diverges | RPV-17, RPV-32 |
| Count / attachment point overlay | PlanView/PlanView.mm:508-623; PlanView/Components/TakeoffOverlayView.mm:5-14 | takeoff_renderer.py:595-615; domain/entities/shape.py | test_takeoff_renderer.py::test_count_takeoff_is_centered_and_has_a_minimum_rendered_size; ... | diverges | RPV-26, RPV-27 |
| 2D fill colour, opacity, grayscale mapping | PlanView/PlanView.mm:242-248, 270-277; PlanView+Placement.mm:160-176; ... | presentation/visualization/services/color_service.py:35-300; domain/entities/pattern.py:15-34 | tests/presentation/visualization/services/test_color_service.py | diverges | COL-03, COL-01, RPV-31 |
| Negative-quantity marker | PlanView/PlanView.mm:346-355; TakeoffOverlayView.mm:135-154 | takeoff_renderer.py:390-437 | tests/presentation/visualization/pdf/renderers/test_takeoff_renderer.py::test_the_marker_is_drawn_ov ... | diverges | MSH-02 |
| Selection handles and multi-selection border (2D) | PlanView/PlanView.mm:625-789; TakeoffOverlayView.mm:16-43 | plan_view/components/selection_manager.py:340-372, 634-712, 916-953; ... | tests/presentation/components/plan_view/components/test_selection_manager.py; test_handle_style.py | matches | COL-04 |
| Takeoff hit-testing and tap cycling | PlanView/PlanView.mm:802-901 | selection_manager.py:153-276; input_handler.py:1575-1600 | tests/presentation/components/plan_view/components/test_selection_manager.py | platform-adapted | RPV-32 |
| Long-press context actions (negative toggle) | Controllers/ViewController/ViewController.mm:482-561; PlanView.mm:903-929 | plan_view/components/input_handler.py:3099-3230 | - | platform-adapted | - |
| Takeoff drag editing (body, corner, edge, group) | Controllers/ViewController/Components/DragController.mm:1-138; ... | plan_view/components/drag_handler.py:261-430; plan_view/components/input_handler.py:1455-1540, 1730- ... | tests/presentation/components/plan_view/components/test_drag_handler.py::ComputeNewPositionTests; ... | diverges | RPV-08, RPV-09, RPV-10, RPV-15 |
| Count placement (tap) | PlanView/Components/PlanView+Placement.mm:264-279 | plan_view/components/placement_mode.py:1639-1700; presentation/handlers/plan_view_action_handler.py: ... | tests/presentation/components/plan_view/components/test_placement_mode.py | diverges | RPV-05, RPV-07, COND-PLACE-01 |
| Linear placement (press-drag-release) | PlanView+Placement.mm:345-389 | placement_mode.py:1815-1854, 380-417 | test_placement_mode.py::test_minimum_fractional_linear_and_area_release_preserves_preview; ... | diverges | RPV-04, RPV-05, RPV-06, RPV-13, COND-PLACE-01, COL-04 |
| Area placement (rectangle drag and polygon taps, undo last point, cancel) | PlanView+Placement.mm:112-143, 281-342, 390-428 | placement_mode.py:1639-1813, 344-370 | test_placement_mode.py::test_area_takeoff_click_drag_rectangle_placement_is_unchanged; ... | diverges | RPV-04, RPV-13, GEO-09, RPV-20 |
| Grid and angle snapping | PlanView+Placement.mm:3-21, 57-58; DragController.mm:5-24 | placement_mode.py:290-310, 380-417, 763-817; drag_handler.py:261-264 | test_placement_mode.py::test_default_mouse_snap_angles_are_15_unpressed_and_off_when_pressed; ... | diverges | RPV-04, RPV-05, RPV-06, RPV-07 |
| Polygon validity and self-intersection checks | Core/Geometry/geom_utils.cpp:23-74; PlanView+Placement.mm:306, 326; ... | cpp_extensions/src/geom_utils/geom_utils.cpp:45-119; plan_view/components/geometry_utils.py:13-19 | tests/integration/native/test_geometry_boundaries.py; tests/presentation/components/plan_view/compon ... | diverges | GEO-09, GEO-07 |
| Undo/redo for move, place and delete | Controllers/ViewController/Components/UndoRedoController.mm; ... | application undo service (not compared in depth) | - | unverified | - |
| 2D selection state and 2D<->3D selection sync | ViewController.mm:692-706, 829-886; ViewController+PageSelector.mm:297-316 | plan_view/view.py:5040-5075 (clear on page change); mesh_view.py:454-548 | tests/presentation/components/plan_view/test_view.py::test_known_sequence_page_switch_drops_stale_se ... | diverges | RPV-16 |
| 3D mesh passes: opaque, weighted-blended OIT, transparency threshold | Rendering/Renderer.mm:526-639, 565-567; Rendering/Shaders/src/fs_oit_*.sc | cpp_extensions/src/renderer/renderer.cpp:464, 629-740, 2260-2420 | tests/integration/native/test_renderer_boundaries.py | platform-adapted | - |
| 3D shading and lighting | Renderer.mm:553; Rendering/Shaders/src/fs_mesh.sc:9-25; MeshLoader.cpp:26-66 | cpp_extensions/src/renderer/renderer.cpp:597-626, 1254-1263 | - | diverges | COL-02 |
| 3D mesh-ID picking | Renderer.mm:490-522, 715-737; ViewController.mm:449-464 | renderer.cpp:1914-1975 (render_pick_pass, pick_at); presentation/components/mesh_view.py:454-502 | tests/integration/native/test_renderer_boundaries.py | matches | - |
| 3D selection highlight (tint mask and yellow edges) | Renderer.mm:641-713; Rendering/Shaders/src/fs_selection_post.sc | renderer.cpp:759-772, 2000-2040, 2377-2410 | - | matches | - |
| 3D colour and opacity per mesh | Services/ProjectService.mm:183-205 | color_service.py:264-282; domain/entities/pattern.py:27-34 | tests/presentation/visualization/services/test_color_service.py::test_original_mode_3d_opacity_comes ... | diverges | RPV-25, COL-01, RPV-31 |
| 3D camera (orbit, pan, pinch, fit) | Controllers/ViewController/Components/CameraController.mm; ... | renderer.cpp:290-380 (Camera); mesh_view.py:320-433 | - | platform-adapted | - |
| Display mode config (Solid / OST / Transparent) and grayscale flag | Core/Domain/RenderConfig.hpp; ViewController.mm:1085-1093; ... | domain/entities/config.py:20-100 (Solid/Original/Transparent, separate 2D and 3D modes) | tests/presentation/test_config.py | platform-adapted | - |
| 3D page rotation/flip transform of meshes | Services/ProjectService.mm:156-206; Core/Meshing/MeshFactory.cpp | presentation/visualization/meshing/mesh_builder.py:71-88, 147 | tests/integration/rendering/test_page_flip_mesh.py | diverges | RPV-29 |

### project-browser

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| Hierarchy load: folders (BidProjects) with their bids, orphan detection | Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp:20-74 (load_hi ... | ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py (_parse_hierarchy); ... | tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_full_hierarchy_maps_every_bid_fie ... | matches | BRW-13, BRW-21 |
| Folder (project) ordering | ProjectStore+Folders.cpp:29-31 (ORDER BY Name) | hierarchy_reader.py:40-43 (ORDER BY [Name], [UID]); presentation/components/project_tree_view.py:453 ... | tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_full_hierarchy_maps_every_bid_fie ... | matches | - |
| Bid ordering inside a folder and among orphans | ProjectStore+Folders.cpp:46-48 (ORDER BY UID DESC); ... | hierarchy_reader.py:72-81 (order_by_existing Bids: BidProjectUID, BidNo, UID); ... | tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_bids_with_equal_project_and_numbe ... | diverges | BRW-13 |
| Create folder | DatabaseViewController.mm:139-167; ImportService.mm:403-407; ... | application/use_cases/project/create_project_use_case.py; infrastructure/mdb/components/project_oper ... | tests/infrastructure/mdb/components/test_project_operations.py::test_a_project_is_created_with_the_n ... | diverges | DBP-18, BRW-19, BRW-02 |
| Rename folder | DatabaseViewController.mm:169-200 (long-press header -> Rename); ... | application/use_cases/project/rename_project_use_case.py; presentation/handlers/project_write_handle ... | tests/presentation/handlers/test_project_write_handler.py::test_rename_project_normalizes_the_name_a ... | diverges | BRW-02, BRW-19 |
| Delete folder | DatabaseViewController.mm:202-235; ImportService.mm:416-419; ... | application/use_cases/project/delete_projects_use_case.py; infrastructure/mdb/components/project_ope ... | tests/presentation/handlers/test_project_write_handler.py::test_delete_projects_skips_the_trash_and_ ... | diverges | BRW-05, BRW-08 |
| Move bid to folder / remove from folder | DatabaseViewController.mm:371-457 (context menu); ImportService.mm:421-432; ... | application/use_cases/project/move_bids_use_case.py; infrastructure/mdb/components/project_operation ... | tests/application/use_cases/project/test_move_bids_use_case.py; ... | diverges | BRW-12, BRW-09 |
| Trash a bid (move to Deleted Bids) | DatabaseViewController.mm:279-285; ImportService.mm:377-387; ... | presentation/handlers/project_write_handler.py:720-801 (_delete_bids active branch); ... | tests/presentation/handlers/test_project_write_handler.py::test_trashing_one_bid_confirms_and_moves_ ... | diverges | BRW-01, BRW-10, BRW-11 |
| Permanent delete from Deleted Bids | DatabaseViewController.mm:279-285,345-354; ImportService.mm:389-399; ... | presentation/handlers/project_write_handler.py:646-719; infrastructure/mdb/components/bid_operations ... | tests/presentation/handlers/test_project_write_handler.py::test_permanent_delete_confirms_with_the_b ... | diverges | BRW-01, DBP-12 |
| Restore from Deleted Bids | DatabaseViewController.mm:423-440 ('Restore as Unassigned' -> removeBidFromFolde ... | presentation/handlers/project_write_handler.py:580-614 (restore_bids); ... | tests/presentation/handlers/test_project_write_handler.py::test_restore_bids_groups_by_original_proj ... | diverges | BRW-09 |
| Deleted Bids system folder identity and protection | ProjectStore+Folders.cpp:6-18,83-92,122-123,143-144; ... | domain/entities/project_constants.py (UID '1'); infrastructure/mdb/components/project_operations.py: ... | tests/infrastructure/mdb/components/test_project_operations.py::DeletedBidsProjectGuardTests | diverges | BRW-02 |
| Open a bid / load project into the viewer | DatabaseViewController.mm:358-367; Controllers/ViewController/ViewController.mm: ... | application/use_cases/project/load_bid_use_case.py; infrastructure/mdb/components/bid_data_reader.py ... | tests/application/use_cases/project/test_load_bid_use_case.py; ... | diverges | DBP-31 |
| Page-folder tree built at load time (Folder entity) | Core/Domain/Folder.hpp:9-38; ProjectStore+Load.cpp:272-345 | domain/entities/folder.py; infrastructure/mdb/components/hierarchy_reader.py:148-266 | tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_page_hierarchy_accepts_valid_mult ... | diverges | DBP-20, BRW-17 |
| Page / folder ordering in the page selector | Services/ProjectService.mm:45-63; Controllers/ViewController/Components/ViewCont ... | hierarchy_reader.py:226-228 (pages ORDER BY Sequence, Name, UID); ... | tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_page_info_carries_every_stored_fi ... | diverges | LP-08 |
| Domain structs for the browser (DatabaseFolder, BidSummary, FolderGroup, DatabaseHierarchy ... | Core/Domain/DatabaseFolder.hpp; Core/Domain/DatabaseHierarchy.hpp | domain/entities/hierarchy_data.py; domain/entities/project.py | tests/domain/entities/test_project_factory.py | matches | BRW-13 |
| Import .ost/.osp from document picker and Files 'open in' | DatabaseViewController.mm:100-135; SceneDelegate.mm:7-71; ... | application/use_cases/project/import_project_files_from_args_use_case.py; ... | tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_passes_resolved_ma ... | diverges | IMP-06 |
| Folder expand/collapse state and section layout | DatabaseViewController.mm:239-266,289-318,459-547 | presentation/components/project_tree_view.py:896-928,982-1016 | tests/presentation/components/test_project_tree_view.py::test_default_expansion_is_recorded_and_surv ... | platform-adapted | - |
| Visualization settings sheet (render config) in the browser nav bar | DatabaseViewController.mm:592-639; ImportService.mm:510-534 | - | - | unverified | - |
| App lifecycle (Main.m, AppDelegate, SceneDelegate) | Main.m; AppDelegate.m; SceneDelegate.mm:7-31 | ost_visualizer/main.py; application/app_controller.py | - | platform-adapted | - |

### settings-undo-camera

| iOS feature | iOS location | Windows module | Windows tests | status | ids |
|---|---|---|---|---|---|
| Render color mode model (Solid / OST / Transparent) and grayscale flag | Core/Domain/RenderConfig.hpp:9-26 | ost_visualizer/domain/entities/config.py; ost_visualizer/domain/aggregates/config_aggregate.py | tests/domain/entities/test_config.py; tests/domain/aggregates/test_config_aggregate.py | diverges | SET-01 |
| Render config persistence (SQLite _AppPreferences keys color_mode, grayscale_enabled) | Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:263-290; ... | ost_visualizer/domain/aggregates/config_aggregate.py (_load_config, update_options, _save_config); ... | tests/domain/aggregates/test_config_aggregate.py; tests/application/services/test_config_service.py | diverges | SET-02, SET-03 |
| Visualization Settings sheet (mode and grayscale) | Controllers/DatabaseViewController.mm:594-639 | ost_visualizer/presentation/dialogs/options/dialog.py; ost_visualizer/presentation/components/menu_b ... | tests/presentation/dialogs/options/test_dialog.py; tests/presentation/controllers/test_menu_controll ... | platform-adapted | - |
| Render config change notification triggers remesh and 2D refresh | Controllers/ViewController/ViewController.mm:249-253,1065-1074,1085-1093 | ost_visualizer/presentation/managers/app_config_presentation_manager.py; ... | tests/application/services/test_config_service.py | platform-adapted | SET-03 |
| 3D mesh alpha and grayscale by render mode | Services/ProjectService.mm:184-200 | ost_visualizer/presentation/visualization/services/color_service.py:55-85,247,264-282; ... | tests/presentation/visualization/services/test_color_service.py; ... | diverges | COL-03, COL-02 |
| 2D plan overlay alpha, grayscale and placement preview colour | PlanView/PlanView.mm:245-277; PlanView/Components/PlanView+Placement.mm:160-176 | ost_visualizer/presentation/visualization/services/color_service.py:80-85,284-299; ... | tests/domain/entities/test_pattern.py; tests/presentation/visualization/services/test_color_service. ... | diverges | COL-04 |
| Bid snap increment conversion (metric mm to OST inches) | Core/Domain/Bid.hpp:24-31 | ost_visualizer/presentation/components/plan_view/view.py:3955-3962 (set_snap_settings) | tests/domain/entities/test_bid.py | matches | - |
| Bid takeoff_increments / measure_base load | Core/Persistence/ProjectStore/Components/ProjectStore+Load.cpp:16-38 | ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:122-127 | tests/infrastructure/mdb/components/test_hierarchy_reader.py:534-548; ... | diverges | DBP-04 |
| Grid snapping of dragged and placed coordinates | Controllers/ViewController/Components/DragController.mm:5-9,60-119; ... | ost_visualizer/presentation/components/plan_view/components/drag_handler.py:261-299; ... | tests/presentation/components/plan_view/components/test_drag_handler.py:1405-1500; ... | diverges | RPV-07, RPV-05 |
| Angle snapping toggle and step | Controllers/ViewController/ViewController.mm:598-608; PlanView/PlanView.mm:76; ... | ost_visualizer/presentation/components/plan_view/components/placement_mode.py:290-309; ... | tests/presentation/components/plan_view/components/test_placement_mode.py:1048-1078 | diverges | RPV-04 |
| Minimum snapped placement length | PlanView/Components/PlanView+Placement.mm:374-375,414-415 | ost_visualizer/presentation/components/plan_view/components/placement_mode.py:95-96,1269-1276,1760-1 ... | tests/presentation/components/plan_view/components/test_input_handler.py:4590-4660 | diverges | RPV-13 |
| Undo/redo stacks for takeoff move, place and delete | Controllers/ViewController/Components/UndoRedoController.h/.mm:1-138; ... | ost_visualizer/presentation/services/undo_redo_service.py; ost_visualizer/presentation/handlers/plan ... | tests/presentation/services/test_undo_redo_service.py; tests/integration/history/test_small_drag.py | diverges | UND-01, UND-11 |
| Undo / Redo / Delete toolbar enablement | Controllers/ViewController/ViewController.mm:729-737 | ost_visualizer/presentation/services/undo_redo_service.py (can_undo, can_redo, change callback) | tests/presentation/services/test_undo_redo_service.py | platform-adapted | - |
| History reset on project load | Controllers/ViewController/ViewController.mm:1040-1046 | ost_visualizer/presentation/services/undo_redo_service.py:92-95,526-539 (set_active_bid, clear) | tests/presentation/services/test_undo_redo_service.py:423-431 | matches | UND-03 |
| Delete selected takeoff with child cascade (undoable) | Controllers/ViewController/Components/UndoRedoController.mm:95-131; ... | ost_visualizer/domain/services/takeoff_domain_service.py:6-19; ... | tests/domain/services/test_takeoff_domain_service.py:159; tests/infrastructure/mdb/components/test_t ... | diverges | DBP-14, UND-06, UND-08, UND-10 |
| Multi-takeoff (select-all-of-condition) drag with undo | Controllers/ViewController/ViewController.mm:901-923,943-969 | ost_visualizer/presentation/components/plan_view/components/input_handler.py:1730-1865; ... | tests/integration/history/test_small_drag.py:68-136; tests/presentation/components/plan_view/compone ... | diverges | RPV-10, UND-07 |
| Takeoff negative-quantity toggle (context menu) | Controllers/ViewController/ViewController.mm:500-561 | ost_visualizer/presentation/handlers/plan_view_action_handler.py:2072-2116 | tests/presentation/handlers/test_plan_view_action_handler.py:7407,13434 | diverges | UND-09 |
| Orbit, pan-mode and pinch-zoom gestures | Controllers/ViewController/Components/CameraController.mm:11-29,43-121 | ost_visualizer/presentation/components/mesh_view.py:320-433,803-856; ... | tests/presentation/components/test_mesh_view.py; tests/integration/native/test_renderer_boundaries.p ... | platform-adapted | CAM-04 |
| Fit-to-scene / Reset camera | Rendering/Renderer.mm:455-481; Controllers/ViewController/Components/CameraContr ... | cpp_extensions/src/renderer/renderer.cpp:303-334 (Camera::show_object); ... | tests/integration/native/test_renderer_boundaries.py:30-67; tests/presentation/components/test_mesh_ ... | diverges | CAM-01, CAM-03 |
| Floating mode button and EditMode (View/Select/Place; camera Orbit/Pan) | Controllers/ViewController/Components/FloatingModeButton.h/.mm; ... | ost_visualizer/presentation/modes/cursor.py; ost_visualizer/presentation/components/mesh_view.py:568 ... | - | platform-adapted | - |
| Area-placement undo-last-point and cancel buttons | Controllers/ViewController/ViewController.mm:610-659; ... | ost_visualizer/presentation/components/plan_view/components/input_handler.py:2531-2552 | - | platform-adapted | - |

## (H) Windows features with no iOS counterpart (list only)

From the Windows-only inventory (87 features). `[platform-only]` = tied to the Windows platform (not a candidate for iOS).

### ai-takeoff

- AI takeoff analysis (PDF linework, text and region finding): Read-side AI tools: sheet listing with scale hints, PDF text and vector segment extraction, closed-region finding with door-gap closing and dashed-outline handling, quant ...
- AI takeoff proposals (scale and slab elements): propose_scale, propose_element and polygon simplification that build slab conditions and takeoffs from detected regions, with high-impact assumption tracking (scale, thic ...
- AI changeset review, apply, undo and discard: Review dialog showing outlines, added area/volume and assumptions (Accept disabled until high-impact ones are settled). ...
- AI assumptions sidecar and audit log: Per-bid assumptions saved under ~/.ost_visualizer/ai_takeoff, a rebind prompt when the database moves, and an audit log of tool calls and decisions.
- AI takeoff MCP helper, local pipe bridge and MCP Setup tab [platform-only]: ostv-takeoff-mcp stdio helper exposing 15 tools, talking to the running app over a user-only named pipe with a rotating session key. ...
- Read-only MCP server for databases (ost-visualizer) [platform-only]: ostv-mcp stdio helper exposing about 40 bounded read tools and prompts (projects, bids, pages, conditions, takeoffs, summary, PDF text/vector summaries, scope-gap checks) ...

### annotation

- Annotation drawing tools and rendering: Place, select, resize, rotate and render line, arrow, dimension, rectangle, oval, polygon, cloud, ink, highlight and text annotations (plus callout, hotlink and named-vie ...
- Annotation persistence, text lifecycle and history: Insert, delete and save annotation positions, styles and text properties (Latin-1 BidTexts/BidCallOuts encoding rules, page-scale rescaling) with annotation-aware undo/re ...
- Annotation styles, fonts and creation defaults: Per-annotation color, line width, font name/size/bold/italic/underline and text alignment, with normalization rules. ...
- Hotlinks: Hotlink markers on a page that jump to a named view or page. Config.hotlink_target chooses whether the target opens in the Annotation window, the View window or Main.
- Named views: Named rectangular regions on a page (create via tool, pick via Select Named View dialog, rect-to-position normalization) used as navigation targets for hotlinks and detac ...
- Annotation window and View window (detached plan windows): Detached secondary plan windows showing a page or named view, kept in sync with Main (takeoffs, layers, annotations, remote changes). Also a detached 3D Mesh window. ...
- Takeoff condition labels on the 2D plan: Display Name and Display Dimension labels on 2D takeoffs, with area label and style label text, draggable and editable (font, color, position) and persisted. ...

### business

- Bid comparison by condition ref number: Compares an old/source bid with a new/target bid by condition RefNo and returns bounded aggregates by condition type with affected pages. ...
- Cover Sheet (bid properties): Edit bid name, number, job ID, bid date, notes, estimator, job status, measure base, takeoff increments, scale style and new-page defaults, and open Areas, Employees and ...
- Summary tab (condition quantity summary): Review quantities grouped by Area, Type or Page with unused conditions hidden, multi-area totals, and copy/delete rows. ...
- Project tree structure editing (new bid, duplicate, rename, copy/cut/paste bids): Create a new project or bid, duplicate a bid, rename bids and projects, copy/cut/paste bids across projects, and Restore. ...
- Job status grouping, change and bid lock: Group projects by job status, change a bid's status, and block editing when the status is marked Locked. iOS has no lock guard.
- Employees master data: Master > Employees dialog with list and detail editing (name, address, phones, email, pay class), also used as estimator picker.
- Payroll classes master data: Master > Payroll Classes editor for pay classes.
- Job statuses master data: Master > Job Statuses editor (ordering, locked flag, in-use protection).
- Condition types master data: Master > Condition Types editor (CdnTypes) with in-use delete blocking and picker mode. iOS can only pick an existing type.
- Bid areas (Areas dialog, area combo, page area): Hierarchical bid areas (new, delete, indent, outdent, move) selectable per page and used in Summary grouping. iOS never surfaces BidAreas.
- Metric units, dimension display and quantity formatting: Metric UOMs (mm, mm2, m, m2, mm3, m3), metric/imperial normalization, metric dimension and quantity display, and metric takeoff settings. ...
- Quantity rounding and attachment quantity aggregation: Applies round_quantity/round_up to calculated totals and aggregates attachment footprint and perimeter into parent area quantities. ...

### collaboration

- Microsoft SQL Server database backend: Find, connect to, create and initialize SQL Server databases (Windows or SQL auth, TLS options, schema-v1 validation and provisioning), browsed alongside other databases. ...
- Multi-user SQL collaboration engine: Per-database sessions with heartbeat and bid presence, expiring edit leases, optimistic row versions, Change Tracking feed with remote reconciliation, FIFO write queue, l ...
- SQL Server credentials in Windows Credential Manager and Windows authentication [platform-only]: SQL passwords stored in Windows Credential Manager, Windows-integrated authentication, and ODBC Driver 18 dependency.

### editing

- Cut, copy, paste and duplicate takeoffs and annotations: Plan-item clipboard across pages and bids (reference remapping, parent/hole relinking, one transaction), Ctrl+C/X/V/D, Intelligent Paste axis snap, and conditions carried ...
- Paste Backout: Paste copied backout (hole) takeoffs into a target parent area with a live preview and validity checks.
- Backout mode (place holes inside an area): Tools > Backout Mode: choose a parent area and draw backout areas inside it with containment validation. ...
- Rotate and flip selected takeoffs (single and group): Rotate Takeoff Left/Right, Flip Horizontal/Vertical and a rotate handle, with group rotation that moves area children, saved and undoable.
- Slope direction rotate handle: On-plan handle to set the slope direction of sloped area and linear takeoffs. iOS meshes slopes but has no plan handle.
- Set as Curved Segment and polygon control points: Context-menu toggle to curve a linear segment, and Add or Subtract Control Point on polygons and lines.
- Assign to Current Area and Select Objects in Current Area: Assign selected takeoffs to the page's current bid area, and select all objects belonging to the current area.
- Reassign takeoffs to another condition: Context-menu submenu that moves selected takeoffs to a different condition, with impact checks.
- Marquee (roping) selection and Select All: Rubber-band selection with touching or inclusive method (Options), Ctrl+A, and multi-item selection and move. ...
- Snap to PDF lines, takeoffs and right angles: Snap index for PDF line ends, midpoints, crossings and edges, existing takeoff geometry, and right angles, with per-snap enable and pixel thresholds plus separate pressed ...
- Cursor and pointer preferences [platform-only]: Full-window crosshairs with color and thickness, auto zoom level, and advanced mouse controls.
- Zoom tool, zoom percentage selector and view commands: Zoom-in/out/reset commands, zoom percentage combo (separate drafts for plan and 3D), previous/next page commands and a Zoom cursor tool.
- PDF text selection and copy: Select and copy text runs from the page PDF on the plan, with a PDF-text context menu.
- Complete undo/redo history: Edit > Undo/Redo covers takeoff add, delete, move, rotate and property changes, annotations, condition and folder edits, layer and area changes, group rotation, AI change ...
- Create conditions and condition folders: New condition, new condition folder, rename, delete and drag-drop into folders, group by condition type, placeable-state display, and impact checks. ...
- Condition copy, cut and paste (across bids): Copy and cut conditions in the Conditions sidebar and paste them into another bid or folder.
- Duplicate and reassign takeoff; Select Objects by condition: Right-click a condition to duplicate it and move only the displayed page's takeoffs to the copy in one transaction, or to select that condition's takeoffs on the page.
- Renumber Conditions: Project > Renumber Conditions rewrites condition reference numbers for a bid. iOS only shifts RefNos on duplicate.
- Layer management and default layers: Create, rename, delete (blocked when in use), reorder, check/uncheck all and edit default layers, including special Annotations, Comments and Image layers whose visibilit ...
- Edit actions from the 3D view context menu: Delete, Assign to Area, Reassign Condition, Count as Negative and Set Curved from the 3D view. iOS 3D long-press offers only Count as Negative Quantity.

### export

- OST export (.ost): Export the selected bid back to an OST file (raw-table serialization with Latin-1 rules). Needed for round trip to desktop. iOS has no writer.
- OSP export (.osp package): Package the bid plus its page images into a CAB-based OSP with safe member paths. iOS can only extract CAB files.
- PDF export with takeoff and annotation markup: Per-page PDF export drawing takeoffs and all annotation types as vector markup over the page, overlay or composite background (or blank), using PDFium and QPDF.
- PDF annotation captions (Bluebeam-compatible): Options > Export lets users choose which measurement captions appear on exported takeoff annotations in the PDF.
- FBX export: Mesh export to FBX through the shared exporter base.
- Summary CSV export: Export the Summary tab (grouped by Area, Type or Page) to CSV with natural/type/page/area sorting and optional condition-name elevation dropping.
- Full-featured HTML 3D export viewer: The Windows HTML export has a 2D plan tab with the PDF and SVG takeoff overlay, page combo, Visibility panel (by Layer, Condition or Area), hover tip with elevations/SF/C ...
- Elevation callouts in HTML and PDF exports: Visibility-aware callouts with condition name, top and bottom elevation (nearest 1/8 in) and cubic yards, each independently toggled, with color options for HTML and PDF.
- Drop condition elevation from names in OST/OSP and CSV exports: Export options that strip the elevation suffix from condition names.

### other

- Multi-database registry (Open Databases dialog): Register several databases (Access and SQL), check or uncheck them to load or unload, remove entries, and persist the list (file_state.json). ...
- Pan sidebar (mini-map): Docked thumbnail of the current page showing the visible area; click or drag to pan Main's plan view.
- Tower crane visualization ([crane] notes tag): Count conditions whose notes contain a [crane model=... mast= jib= counterjib= trolley= hook= hookheight=] tag show a GLB tower crane model (classic, liebherr) on the foo ...
- 3D section cuts (north-south and east-west): Two sliders clip the model with capped cut faces, outlines and pickable caps; labels in feet from the model edge.
- 3D feature-edge overlay toggle: Options and 3D checkbox to draw feature edges for the whole scene (boundary edges and creases over 15 degrees). iOS draws edges only as a selection highlight.
- Saved per-bid 3D camera state: Remembers and restores the 3D camera per bid across scene refreshes and remote updates.
- Separate 2D and 3D takeoff display modes with sync toggle: View > Takeoff: Transparent, Solid or Original for 2D and 3D independently, a Sync 2D/3D toggle, and grayscale. iOS has one color mode for both views.
- Count condition shape meshes (ellipse, triangle, rhombus, hexagon, octagon): Elliptical cylinder with width/depth and triangle, rhombus, hexagon and octagon prisms. iOS builds only box and circular cylinder.

### page-settings

- Set Scale dialog and page settings bar: Edit page scale (Architectural, Civil, Metric or custom), apply to all pages, and page-bar scale and area combos with rescaling of existing positions.
- Rename page and delete page: Edit > Rename Page and Delete Page, with deletion cascading to the page's takeoffs and annotations.
- Plan Organizer (add, duplicate, import and move pages and page folders): Cover Sheet tree to add pages and folders, duplicate pages, import PDF/TIFF files as pages (multi-page rows), drag to reorder and set per-page image, size, scale and show ...
- Adjust Images dialog and Invert/Bitonal toggles: Image menu and Ctrl+I: rotation (0/90/180/270), flip X/Y, invert and bitonal rendering of the page image, optionally for all pages, persisted per page. ...
- Rotate and flip page image commands: Rotate Image Left/Right and Flip Image Horizontal/Vertical, separate from rotating or flipping takeoffs.
- Overlay image (select, remove, move, resize, show): Attach a second image/PDF to a page, move and resize it via the overlay rect, remove it, and composite it on the plan. iOS loads the overlay fields but never draws them.
- Page show mode (Original, Overlay, Both): Show Original Image and Show Overlay Image toggles in the Image and context menus, persisted per page.
- Multi-page PDF page index and TIFF raster pages: Select which page of a multi-page PDF a sheet uses, and support TIF/TIFF raster page images (including in OSP). ...
- Page image plane in the 3D view: Page image, overlay or composite is shown as a textured plane under the 3D model with the page's mode, rotation and effects.
- Persisted page view state and selected page: Saves each page's zoom and view center and each bid's selected page in the database with debounced deferred persistence.
- Missing page file and render-progress notices: Overlay bar when a page image or PDF cannot be found or loaded, plus a page render loading bar and PDF render scale and prefetch logic.
- Page name display options (page index and sheet number): Options to show the PDF page index and the sheet number alongside sheet names in page lists.

### platform

- Microsoft Access .mdb databases (open, create, Compact/Repair) [platform-only]: Open and browse Access databases through pyodbc, create a new Access database, and Compact/Repair from the Open Databases dialog. ...
- Realtime Sync with On-Screen Takeoff and OST-active edit blocking [platform-only]: Monitors the Global\OSTMdbCommit Windows event from the OST desktop app (RealtimeService companion), reloads on commit, and blocks edits while OST is active.
- Licensing (activation, validation, free vs commercial gating) [platform-only]: License dialog, HWID v1 identity, signature verification, periodic validation, activation audit identity, and Feature gating (viewer vs licensed 3D, edit, import and expo ...
- Update check and update dialog [platform-only]: Checks the server for newer versions and shows changelog sections in an update dialog.
- About dialog [platform-only]: Help > About OST Visualizer with version and license info.
- Windows installer, file associations and build tooling [platform-only]: MSI installer, .ost/.osp registry associations, Nuitka/CMake build scripts and bundled MCP helper executables.
- Desktop menu bar, toolbars, sidebars, status bar and keyboard shortcuts [platform-only]: File/Edit/View/Tools/Image/Project/Master/Help menus, toggleable toolbars and docked sidebars, status bar with date, license and collaboration state, Ctrl-key shortcuts, ...
- Takeoff toolbar customization [platform-only]: Options > Takeoff Toolbar to hide or show individual navigation, cursor, annotation, zoom and Page Settings controls, with Restore Default Toolbar.
- Options dialog and persisted app configuration [platform-only]: Options tabs (Options, Takeoff Toolbar, Fonts & Colors, Export, MCP Setup), Apply/OK/Cancel, reset all settings, and persisted config.json and workspace state (window geo ...
- Windows light/dark theme following [platform-only]: Qt Fusion or Windows 11 style, palette and title-bar tracking of the Windows color scheme across Main and detached windows.

### Additional Windows-only items reported by the area mappers

- (quantities) Condition Summary tab (folder/page/area/type grouped totals, multi-area total rows, bold columns, area detail nodes) and summary CSV export
- (quantities) Partial quantity recompute by condition (only_condition_uids filter, zero prepopulation) used by sidebar partial updates and the summary
- (quantities) Per-takeoff cubic yards (compute_takeoff_cubic_yards) for elevation callouts
- (quantities) Net area in SF with hole subtraction and zero clamp (calculate_net_area_sf) for PDF annotation export
- (quantities) Stored quantity recomputation and UOM normalization on OST export / measurement-system change / cross-bid condition copy
- (quantities) In-plan area dimension label showing the three quantities ('%.2f LABEL') and condition-name labels with per-takeoff fonts (display_dimension / display_name)
- (quantities) Per-page takeoff counts with visible counts and deterministic ordering (summarize_takeoffs_by_page), used by MCP tools and bid comparison
- (quantities) Quantity consumers beyond the sidebar: bid comparison, MCP read service, AI takeoff read service
- (quantities) Takeoff domain helpers: group_takeoffs_by_type, group_area_takeoffs_with_holes, expand_takeoff_uids_with_descendants, is_takeoff_relevant_for_area_usage, reassign-geometry compatibility
- (geometry) Page scale recalibration: rescale takeoff and annotation positions between page scales (factor target/source ratio, SCALE_EPSILON 1e-9, annotation rotation-slot preservation)
- (geometry) 3D page image plane (native and Three.js plane transforms, page floor elevation, texture sizing)
- (geometry) Page transform helpers: quarter-turn normalization and validation, output_dimensions, dimensions_match tolerance 1e-9
- (geometry) Native 2D/3D batch transforms: transform_to_2d, transform_vertices_to_2d, transform_to_3d, transform_vertices_to_3d, transform_holes_to_3d (iOS has only inline ratio maths in PlanView and MeshFactory)
- (geometry) Hole and attachment containment validation geometry (path_is_inside, path_intersects_any, attachment_fits_area, parent-contains-holes checks while dragging)
- (geometry) Takeoff rotate/mirror helpers (polygon_centroid, rotate_position_coords, rotate_points_around, mirror_position_coords) and resize-cursor direction maths
- (geometry) Takeoff footprint bounds and mirrored point-takeoff rotation helpers
- (geometry) Windows pyd validation wrapper for earcut input (partial coordinate, out-of-range or unsorted hole indices, ring<3)
- (meshing) Tower crane placement for count conditions tagged [crane ...] (placement at footing centre/top, kept outside the boolean step and bounds)
- (meshing) Generator edge tables and mesh metadata (MeshData.edges, get_box/radial/rhombus/triangle/curved/area edge lists, metadata type/has_slope/segments); ...
- (meshing) calculate_mesh_bounds with default bounds (-1000,1000,-1000,1000,-10,10) for empty scenes
- (meshing) FBX export mesh feed (BaseExporter-based, same boolean rule as OBJ/DXF)
- (parsers-units) Metric measure-base handling: millimetre dimension display/parse (display_to_mm, mm_to_display with %g 6 significant digits, inches_to_display(metric=True)), metric UOM codes 11-16 with labels/choices ...
- (parsers-units) Elevation-name conversion and stripping helpers: convert_elevation_in_name, _format_metric_elevation, strip_elevation_suffix, strip_raw_condition_elevations, elevation_name_collisions, format_structur ...
- (parsers-units) OST attribute serialiser for export (serialize_value/serialize_row: datetime 'Y M D h m s', bytes decode, float/Decimal shapes, per-table text encodings) and the golden attribute-order helper
- (parsers-units) Page scale labels and presets (ARCH/CIVIL/METRIC scale tables, format_custom_scale)
- (parsers-units) Date/time handling: bid date display '%m/%d/%Y %I:%M:%S %p', cover-sheet _parse_bid_date, new-bid 'Y M D h m 0' stamps (iOS stores BidDate/CopyTimeStamp as opaque text and never parses or shows them)
- (parsers-units) Overlay rect storage parse/serialise/validate (parse_overlay_rect_storage, serialize_overlay_rect_storage, full_page_overlay_rect, ntpath identity helpers); iOS keeps OverlayRect as raw text
- (parsers-units) Annotation/page position and text storage encoding (encode_position with 3-decimal rounding and preserve_indices for non-takeoff tables, serialize_position_for_table, latin-1 annotation text, decode_t ...
- (parsers-units) Colour service extras: hex/rgba string parsing, as_hex_with_opacity, legend hierarchy_map grouping by (name, z, colour), inactive-area colour substitution, get_color_mapping
- (parsers-units) Quantity text in other surfaces: condition summary columns and 2D takeoff label text (f'{value:.2f} {uom}')
- (parsers-units) MDB reader numeric/text helpers (parse_float with comma-decimal tolerance and defaults, decode_value latin-1, remove_empty_folders, _is_stored_flag_set text handling); ...
- (import-ost-osp) SQL Server collaboration import: queued import payload with source size/mtime snapshot, import_ost_mutation / import_osp_mutation recorder results, SqlProjectWriter.import_ost_data identity graph
- (import-ost-osp) Import target selection (selected project folder, Deleted Bids special case that imports as orphan), file-state/workspace targets and post-import selection of the new project
- (import-ost-osp) Command-line / Windows Explorer file-association import with single-instance forwarding and rejected-argument reporting
- (import-ost-osp) Windows temp-directory extraction staging: extended-length (UNC) path prefixes, short OSTVTemp root on the same drive, 240-character path limit, junction/containment checks, temp tree cleanup, and the ...
- (import-ost-osp) Progress dialog, file dialog and message boxes around import; access-feature gating; deferred-persistence flush before import
- (import-ost-osp) Access/MDB-specific schema compatibility handling for import targets (UnsupportedMdbSchemaError, optional tables and columns missing)
- (import-ost-osp) OST/OSP export and CAB creation (round trip of imported data); out of the import scope but the counterpart of every import rule above
- (export-formats) FBX export
- (export-formats) PDF annotation export (Bluebeam/QPDF), page PDF extraction for HTML
- (export-formats) OST and OSP bid-file export (round trip)
- (export-formats) HTML viewer: layer / condition / area visibility panels, 'show by' groups, Show all / Hide all
- (export-formats) HTML viewer: 2D plan mode with embedded PDF pages and 2D takeoff overlays, page image planes, elevation callouts
- (export-formats) HTML viewer: hover tooltips with structural elevations, section cut with caps, edges toggle, weighted-blended transparency pass, light/dark theme, bounds-aware dynamic clipping
- (export-formats) Tower crane scene data for HTML (and crane meshing)
- (export-formats) Area selections: inactive-area object colour in OBJ/DXF/HTML (page_area_selections, inactive_object_color)
- (export-formats) Separate display_mode_2d / display_mode_3d configuration feeding exports
- (export-formats) Export dialog with page-aware title, deferred-persistence flush, bid/page-changed guards and licence gating (Feature.EXPORT)
- (database-persistence) Create a condition (insert_condition) and condition-folder CRUD (insert/rename/delete)
- (database-persistence) Batch/cross-bid condition duplication (duplicate_conditions, duplicate_conditions_to_bid with UOM normalization for the destination)
- (database-persistence) Renumber conditions (two-phase RefNo renumbering)
- (database-persistence) Layer management: insert, rename, delete, swap sequence, default (template) layers, update_all_layers_show
- (database-persistence) Bulk takeoff writes: batch insert with raw extras and cross-bid copy, positions, rotations, text properties, curve, reassign area/condition, bulk negative flag
- (database-persistence) Annotation families: read, insert, delete, save positions/styles/text properties
- (database-persistence) Page writes: scale (with content rescale), name, view state, show mode, overlay image/rect, invert, bitonal, image adjustments, page area selection, page delete cascade
- (database-persistence) Bid lifecycle beyond import/delete: create_bid, duplicate_bid, save cover sheet, bid areas, job statuses, employees, pay classes, condition types, selected page, job-status update, project create/rena ...
- (database-persistence) Reading bid areas, page area selections, cover sheet, selected page, named views, hot links, takeoff text styles, legend positions, page SheetNo/Sequence/Show/ZoomFac/Invert/Bitonal
- (database-persistence) Project/bid hierarchy extras: page_count, condition_count, job status and estimator names, restore_bids to the saved project, move_bids/orphan_bids use cases, bid clipboard copy/paste
- (database-persistence) Integrity guards and diagnostics: bid-owned identity checks, raw bid integrity validation, schema compatibility inspector (optional columns/tables), reference validation
- (database-persistence) New database creation with Access reference metadata, database maintenance (compact/repair/backup) and MDB connection manager
- (database-persistence) OST/OSP export (raw bid data read-back with Access-to-XML serialization)
- (database-persistence) SQL Server back-ends and descriptor/router layer (platform-specific, not applicable to the iOS SQLite store)
- (conditions) Create a new condition (insert) with type defaults, type/layer/folder ownership checks and next RefNo
- (conditions) Renumber all conditions in the current sidebar sort order
- (conditions) Condition folder management: create, rename, delete, nesting (ParentUID), drag-and-drop move, cut/copy/paste of conditions
- (conditions) Copy conditions to another bid (measure-system UOM normalization, owner references cleared)
- (conditions) Condition Types management dialog (create, rename, delete with usage check) and free-text type entry with create-on-the-fly / ambiguity warning
- (conditions) Layers dialog opened from the condition editor (create/rename/reorder/delete/show layers)
- (conditions) Inline rename in the sidebar with case-insensitive duplicate-name check
- (conditions) Bulk change of layer or type for many conditions from the context menu
- (conditions) Condition Summary tab (quantities by condition/type/folder with takeoff and page breakdown)
- (conditions) Duplicate-and-reassign selected takeoffs, and reassign takeoffs to another condition
- (conditions) Tower-crane notes tag parsing and save-time warnings on Count conditions
- (conditions) Sidebar structure options: group-by-type toggle, expand/collapse all, column sort, multi-selection, keyboard shortcuts, 'select objects' command
- (conditions) Multi-condition duplicate/delete/edit with 'Yes to all' delete confirmation
- (conditions) Edit dialog OK/Apply buttons, read-only/locked-bid mode and SQL edit-lease/queue integration
- (layers-pages) Layer CRUD: add, rename, delete (with usage confirmation), reorder (swap sequence), duplicate-name checks
- (layers-pages) Default (template) layers management and locked built-in layers
- (layers-pages) Layer-rename rendering-impact classifier (skips refresh when only ordinary names change)
- (layers-pages) Annotation layer visibility (annotations take their layer's Show; reserved annotation/image/comments names)
- (layers-pages) Page scale editing (scale combo, Set Scale dialog, rescale of existing takeoff positions)
- (layers-pages) Rename page
- (layers-pages) Page image adjustments (rotate, flip, invert, bitonal) with persistence
- (layers-pages) Page area selection and Areas (BidAreas CRUD, area combo, per-page BidPageSettings.BidAreaSelected, inactive-area colouring in 2D/3D)
- (layers-pages) Page overlay compositing and show mode (overlay image/rect/rotation/deskew, show modes 0/1/2, image sizing)
- (layers-pages) Page view-state and selected-page persistence (ZoomFac/CurrentX/CurrentY per page, BidSettings.BidPageSelectedUID write-back)
- (layers-pages) Delete pages
- (layers-pages) 3D page image plane (PDF page as a textured plane in the 3D scene, driven by the Image layer and page rotation/flip)
- (layers-pages) Page combo extras: prev/next page navigation, label options (page index/sheet number), takeoff-present indicator, 'Items Without Page' pseudo page
- (layers-pages) Condition summary by page/area
- (rendering-planview) Snap-to-takeoff and snap-to-PDF-line (endpoint, midpoint, perpendicular, intersection) with a spatial SnapIndex
- (rendering-planview) Snap to right angle (axis alignment with the first area point, default enabled, 8 px threshold, indicator line)
- (rendering-planview) Attachment placement and containment, backout (area hole) placement/validation, paste-backout, hole/parent containment checks during drags
- (rendering-planview) Polygon control-point insert/remove on area edges and vertices
- (rendering-planview) Rotation and slope-rotate handles, rotate/flip selected takeoffs, keyboard nudge, copy/paste/intelligent paste, rubber-band (roping) selection, zoom box, Shift free-mode
- (rendering-planview) Annotation tools and rendering (highlight, text, dimension, line, arrow, polygon, cloud, ink, box, hotlink, named views)
- (rendering-planview) Line-pattern hatch rendering, area grid lines and condition name/dimension labels in 2D
- (rendering-planview) Page overlay and composite images, raster images, invert/bitonal, PDF text selection, zoom re-render tiles, render_quality scaling
- (rendering-planview) Page area selection with inactive-object colour for takeoffs outside the selected area; area_uid assigned on creation; multi-condition placement (place_condition_uids)
- (rendering-planview) Separate synced/unsynced 2D and 3D display modes and show-edges / tower crane / grayscale preferences
- (rendering-planview) 3D page-plane texture, section cuts with cap meshes, instanced tower-crane models, all-edges toggle and per-mesh edge rendering
- (project-browser) Create a new empty bid (New > Project)
- (project-browser) Duplicate bid; copy, cut and paste bids (including paste into another folder)
- (project-browser) Drag-and-drop move of bids between folders
- (project-browser) Empty Deleted Bids Folder (permanently delete all trashed bids)
- (project-browser) Multi-select delete/trash/restore across several bids or folders, with mixed permanent-plus-trash handling
- (project-browser) Import into a chosen target folder (selected tree node), with the 'imported as orphaned because Deleted Bids cannot be a target' rule and an import result summary
- (project-browser) Multi-database project tree (open, close, new database; file roots)
- (project-browser) Group by Job Status, Change Job Status, and bid locking via job status (blocks trashing/moving the active locked bid)
- (project-browser) Bid columns (No., Name, Status, Bid Date, Job No., Estimator, Pages, Conditions, Notes, Copy From, Copy Timestamp) and column sorting
- (project-browser) Renumber Conditions from the project tree
- (project-browser) Condition folder insert/rename/delete and nested condition-folder hierarchy (BidConditionFolder.parent_uid, cycle check)
- (project-browser) Remembering the selected page per bid and the project-tree workspace state (selection, expanded nodes)
- (project-browser) Per-bid read of page and condition counts shown in the tree, and background bid load/prepare/apply with stale-version checks
- (settings-undo-camera) Separate 2D and 3D display modes plus the 'Sync 2D/3D display modes' flag (default synced; the 3D mode wins when sync is turned on)
- (settings-undo-camera) Global Edges toggle (show_edges) and tower crane toggle (show_tower_crane, default on) for 3D and HTML
- (settings-undo-camera) Options dialog and validated JSON config: fonts, colours, crosshair, toolbar prefs, elevation callouts, captions, hotlink and roping settings, inactive_object_color, auto-zoom level, AI takeoff flag
- (settings-undo-camera) Configurable snap settings: mouse snap angles (valid set 0,1,2,3,4,5,10,15,30,45,90), Shift modifier, snap-to-grid, PDF-line, takeoff and right-angle thresholds (default 8 px), snap index
- (settings-undo-camera) Keyboard arrow nudge of selected takeoffs by one snap increment
- (settings-undo-camera) Per-bid saved and restored 3D camera state (cache with eviction)
- (settings-undo-camera) Camera inertia, damping and velocity clamps; rotate and pan velocities
- (settings-undo-camera) Section cuts (north-south, east-west sliders with feet read-out)
- (settings-undo-camera) Zoom-percent combo and scene navigation controls; set_zoom_percent and wheel zoom
- (settings-undo-camera) 3D page-plane (plan texture) included in fit bounds
- (settings-undo-camera) Persist and restore the last selected page per bid (BidSettings.BidPageSelectedUID)
- (settings-undo-camera) Global Settings defaults and BidSettings reading (ScaleStyle, ScaleFactor1/2, PageWidth/Height, MeasureBase, TakeoffIncrements, NextBidNo)
- (settings-undo-camera) Undo history for rotate, paste, property edits (condition, area, curve), annotations, AI changesets; labelled entries; discard_entry; lifetime targets, suspension and rebind on restore; ...
- (settings-undo-camera) Inactive-area object colour (area selection)
- (settings-undo-camera) Desktop viewer cursors (zoom, rotate, move overlay)

## (I) Stale claims in the existing iOS docs

The docs are PORTING_STATUS.md, PORTING_MAP.md, PORTING_PLAN.md, PORTING_BLOCKERS.md, PORTING_STATUS.json and PORTING_LOG.md (repo root; there is no docs folder). The code is the truth. Claims reported by several areas are listed once.

| Doc | Claim | Reality (evidence) | Reported by |
|---|---|---|---|
| PORTING_STATUS.md:40 (also PORTING_STATUS.json 'Measurement and quantity calculations', PORTING_BLOCKERS.md:65 ... | Quantity calculations: 'Imperial formulas, holes, slope, curved length, grid, and negative aggregation exist in UomService.hpp.' Missing items are listed as metric UOMs, normalization, attachment aggr ... | Flat formulas, grid, holes and negative sums were verified to match Windows (5000 random cases, 0 diffs). 'Slope' is wrong for areas with negative rise/run (QTY-10). 'Curved length' is wrong in three ways (QTY-07/08/09). Hole aggregation is wrong for non-area children and foreign pages (QTY-03). Attachments with a parent are skipped entirely (QTY-01), a gap the docs do not mention. | quantities |
| PORTING_BLOCKERS.md:65 and PORTING_LOG.md:71 | Attachment quantity parity: 'iOS quantity formulas accept attachment footprint/perimeter, but compute_page_quantities passes TODO zero values.' | True for footprint/perimeter (UomService.hpp:790-791), but incomplete. compute_page_quantities also drops Attachment/non-area takeoffs that have a parent_uid (UomService.hpp:767), so an attachment's own count is 0 (QTY-01). | quantities |
| PORTING_BLOCKERS.md:64 and PORTING_MAP.md:62 | Metric UOM support missing: 'iOS UomService.hpp defines only imperial codes 0-9' and 'metric/imperial normalization are missing'. | Accurate but understated. Codes 11-16 silently convert with divisor 1.0 (raw inches shown with an empty label), the picker is not metric-aware, and labels 4/7 are wrong ('SQ IN'/'ROOF' vs 'IN²'/'SQ') with roofing missing from the area picker. Bid.measure_base is loaded but unused (QTY-05, QTY-06, QTY-16). | quantities |
| PORTING_MAP.md:63 | condition_quantity_service.py -> UomService.hpp / ConditionsViewController.mm: 'Page condition totals are displayed ... formula parity needs fixtures.' | Parity now checked by differential test: formulas match, aggregation and page-scope do not. Sidebar totals use the 2D page + 3D pages union regardless of mode and are not refreshed on 2D page change (QTY-11, QTY-15). Displayed numbers lack metric decimals and zero text (QTY-12, QTY-22). | quantities |
| PORTING_PLAN.md:46 and PORTING_STATUS.md:61 | Open task: 'Verify rounding, grid counts, holes, negative quantities, slope, and curved length calculations against desktop.' | Done in this pass. Grid counts, negative quantities and flat holes match. Rounding is not applied (QTY-04), area slope (QTY-10) and curved length (QTY-07/08/09) differ, and hole/attachment aggregation differs (QTY-01/02/03). | quantities |
| PORTING_BLOCKERS.md:26-27 (B-001) and PORTING_STATUS.json B-001 'needs_verification' | B-001 'Replaced': curved/sloped geometry parity needs fixture comparison only. | The iOS arc code predates Windows' arc-side fix (owner decision 2026-11-06, cited in test_uom_service.py UomArcControlPointSideTests docstring) and the straight-chord rule. Core/Domain/UomService.hpp and Core/Meshing/gen_curved.cpp:81-95 both still use the legacy cross-product direction rule. | quantities |
| PORTING_MAP.md:65 and PORTING_BLOCKERS.md:70 | 'Source-confirmed application of round_quantity/round_up in calculated totals was not found' and the plan item 'Apply or explicitly defer round_quantity / round_up'. | Confirmed (never applied), but it is not deferrable under the parity rule: Windows applies ceil(value/increment)*increment per takeoff before the sign (QTY-04). | quantities |
| PORTING_STATUS.md:45, PORTING_MAP.md:106, PORTING_BLOCKERS.md:72 | 'No iOS app test target, fixture suite ... was found.' | An untracked Tests/ host harness (CMakeLists.txt, Support/TestHarness.hpp, README.md) appeared in the iOS working tree during this pass (created by another step, not by this mapper). It has no quantity tests yet. UomService.hpp compiles standalone on MSVC and was the basis for the differential harness. | quantities |
| PORTING_MAP.md:50 (Geometry, Meshing, And Booleans table) | `cpp_extensions/src/common`, `mesh`, `geom_utils`, `coord_transform`, `earcut` -> `Core/Geometry/*`: Ported. Portable C++ geometry modules are present. | Only earcut, vec_types and mesh_data match Windows. geom_utils lacks point_to_segment_distance and its polygon_is_valid/signed_area are the old weaker versions (GEO-07, GEO-09). coord_transform.cpp is a stale API: Windows now has the 8-argument validated ost_to_pdf_coordinates plus transform_*_to_2d/3d and common/page_transform.hpp; ... | geometry |
| PORTING_LOG.md:192-218 (Phase 1: Portable C++ Core Copied) | geom_utils.cpp and coord_transform.cpp were copied as 'direct copy' from the desktop C++ extensions. | The Windows geom_utils (polygon_is_valid, signed_area, point_to_segment_distance) and coord_transform (ost_to_pdf_coordinates, page_transform.hpp) have since changed substantially; the iOS copies are a snapshot of an older revision. | geometry |
| PORTING_LOG.md:481-487 (Phase 4 coordinate transformation approach) | iOS passes sr=1/(sf2/sf1) to MeshFactory 'This matches the desktop pipeline: transform_to_3d(ost_x, ost_y, scale_ratio) = {-(ost_x/ratio), ost_y/ratio}'. | The current Windows 3D pipeline builds its mesh generator with a default scale-1 coordinate system (visualization_provider.py:114), so 3D geometry is raw OST units and the page ratio is never applied; test_page_flip_mesh.py:97-99 pins X in [-10,-4] for ratio 4 (GEO-01). The ratio-dividing transform_to_3d is only used by the per-page rotation/flip box, with ratio 1. | geometry |
| PORTING_LOG.md:1492-1497 (Phase 9: Area slope) | Per-vertex Y ... 'Ascending/descending logic matches desktop calc_area_mesh_verts'. | Windows area slope always rises along the rotated projection and ignores the sign of rise/run (test_area.py::test_area_mesh_slope_direction_uses_rotation_not_rise_run_signs); iOS flips descending slopes and uses the opposite rotation sense in its mirrored frame (GEO-05). | geometry |
| PORTING_LOG.md:1523-1526 (Phase 9: Hole grouping) | `build_hole_map` scans takeoffs for children (is_hole()==true) and groups by parent_uid, matching the desktop approach. | Windows group_area_takeoffs_with_holes only treats area-condition children whose parent is an area takeoff as holes; attachments, orphan holes and holes under non-area parents stay as normal takeoffs (GEO-04). The export copy also has a sign bug (GEO-02). | geometry |
| PORTING_LOG.md:1220-1226 (Phase 7: X coordinate negation 'corrects the mirroring difference') | X negation in MeshFactory (-x, with y) corrects the mirroring difference to desktop. | Partially superseded: iOS now also negates Y into Z (a Y-up proper-rotation adaptation, fine), but page rotation and flip transforms were never ported and Windows applies a (-x, +y) frame with face reversal for single flips (GEO-03). | geometry |
| PORTING_STATUS.md:22 and PORTING_MAP.md:51-55 (Mesh generation / geometry: area with holes and slope 'source-p ... | Area with holes, slope and curved meshes are source-present and need only fixture comparison. | Concrete numeric mismatches with Windows were found for area meshes: scale (GEO-01), rotation/flip (GEO-03), holes in export (GEO-02, GEO-04), slope direction (GEO-05), tessellation (GEO-14) and winding (GEO-15). A Windows-derived unit test suite would catch each of them. | geometry |
| PORTING_LOG.md (2026-03-23 Phase 4, 'Coordinate transformation approach') | iOS computes scale_ratio = sf2/sf1 and passes sr = 1/scale_ratio; 'This matches the desktop pipeline: transform_to_3d(ost_x, ost_y, scale_ratio)'. | The Windows mesh pipeline builds its coordinate system with CoordinateTransformerFactory.create() (scale factors 1.0), so meshes are in raw OST units and the page scale is never applied (tests/integration/rendering/test_page_flip_mesh.py::test_mesh_pipeline_preserves_model_scale_when_flipping). See MSH-01. | meshing |
| PORTING_LOG.md (2026-03-23 Phase 9, Area slope) | 'Ascending/descending logic matches desktop calc_area_mesh_verts'. | Windows area slope ignores the signs of rise/run and has its own rotation direction (core/geometry/area.py, pinned by test_area.py). iOS differs in both (MSH-03, MSH-04). | meshing |
| PORTING_LOG.md (2026-03-23 Phase 9, Curved linear mesh) | Arc fit, Bezier fallback and 'adaptive segmentation: 6-64 segments' are ported from the desktop; B-001 resolved. | The curve code is a reimplementation, not a port: segment count rounding, arc side for sweeps above about 180 degrees and the straight-chord rule differ from linear_geom.cpp (MSH-05, MSH-06, MSH-07). | meshing |
| PORTING_LOG.md (2026-03-23 Phase 9, Hole grouping) | build_hole_map groups children (is_hole() == true) by parent_uid into 'scaled/negated hole polygons' for both rendering and export. | Windows only treats area-condition takeoffs whose parent is an area takeoff in the set as holes (group_area_takeoffs_with_holes); and ExportService.mm:33-34 does not negate z for holes, so export holes are mirrored (MSH-12, MSH-14). | meshing |
| PORTING_STATUS.md line 22 (Mesh generation \| Partial) | 'Linear, curved linear, area with holes, count/attachment base meshes, slope, and drop stubs are source-present. Advanced count shapes are partial.' | Source-present but not equal to Windows: area slope direction, curved-wall segments/arc side/straight chord, page scale and page rotation/flip all differ. Ellipse (width/depth), triangle, rhombus, hexagon and octagon are not partial, they are absent and render as a cylinder or a box. | meshing |
| PORTING_STATUS.md line 23, PORTING_STATUS.json summary.boolean_ops, PORTING_PLAN.md (checked 'Manifold boolean ... | Boolean operations: Ported. | boolean_ops.cpp is identical to Windows, but the orchestration is not: consumed positives stay visible, export applies one rule to all formats, union failure/degenerate handling differs (MSH-15, MSH-16, MSH-19). boolean_ops.cpp:1 also keeps a stale 'TODO: remove guard when Manifold library is linked' comment although no guard exists. | meshing |
| PORTING_MAP.md line 50 (cpp_extensions/src/... coord_transform ... \| Core/Geometry/* \| Ported) | coord_transform and the other C++ geometry modules are portable and ported. | mesh/boolean/earcut/feature_edges/mesh_repair are byte-identical, but Core/Geometry/coord_transform.cpp has an older API (coord_scale, offsets, is_page_rotated, auto_rotate_180) than the Windows ost_to_pdf_coordinates, which now goes through common/page_transform.hpp (validated, quarter turns only). Outside the meshing scope, but the 3D page flip/rotate (MSH-02) depends on it. | meshing |
| PORTING_MAP.md line 54, PORTING_BLOCKERS.md B-001 | linear_geom: curved mesh code exists without GLM; remaining validation is only fixture comparison for curved/sloped cases. | The fixture-style comparison done here shows concrete mismatches for curved walls and sloped areas (MSH-03 to MSH-07). | meshing |
| PORTING_MAP.md line 53, PORTING_BLOCKERS.md 'Count shape mesh parity', PORTING_STATUS.md 'Count condition mesh ... | iOS only routes circle/ellipse to cylinder and all other count shapes to box; desktop has extra paths. | Accurate as written. The one correction is that ellipse is a circular cylinder of diameter = width (depth ignored), and square, rectangle, triangle, rhombus, hexagon and octagon all render as width x depth boxes. Shape ids match Windows. | meshing |
| PORTING_LOG.md (2026-03-23 Phase 3) | 'dimension defaults match Python source' for MeshFactory. | True except a negative condition depth on non-square shapes (Windows clamps to 1.0, iOS uses width, MSH-13). | meshing |
| PORTING_MAP.md (Import, Persistence, And Schema table, row ost_importer.py) | `ost_importer.py` -> `OstXmlParser.cpp` \| Ported \| 'Parser walks the same bid/page/bid-tail/global section model.' | The section model is not the same: BidEmployees and PayClasses are not parsed, the child-tag rule lacks the 'section minus s' alternative, CopyTimestamp is not remapped, and sanitize_ost_xml rewrites attribute values ending in a backslash (XML-01, XML-02). Confirmed by running the iOS parser and the Windows _parse_ost_xml on the same file. | parsers-units |
| PORTING_LOG.md (Phase 4 'position parser ported from infrastructure/parsers/position_parser.py'; ... | parse_position and extract_z_value were ported from Windows (semicolon-separated floats, XML entity cleanup, trailing zero trim; imperial feet/inches and metric m/cm -> inches). | The Windows source named does not define parse_position (it lives in domain/utils/position.py); the iOS functions are hand-written in Core/Persistence/ProjectStore/ProjectStore.cpp:161-407 and do not implement the Windows grammar: 145 of 342 test-derived names give different z_value/is_top (ELV-01..06) and position parsing accepts junk tokens (POS-01). Tools/export_ost.py no longer exists in the repo. | parsers-units |
| PORTING_STATUS.md (Project/edit persistence row) and PORTING_MAP.md (settings_reader/settings_operations row) | Render-config persistence lives in `ProjectStore+RenderConfig.cpp`. | No such file exists. Render-config load/save is in Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp (~lines 265-290, prefs table _AppPreferences). | parsers-units |
| PORTING_STATUS.md ('Color/material handling' row) and PORTING_LOG.md (Phase 3 'get_rgb() BGR int -> RGB float' ... | BGR-to-RGB conversion, render color modes and transparency handling are present. | Byte order is right, but negative ColorFill is grey instead of low-24-bit masked (COL-01), grayscale is not 8-bit quantised (COL-02), 3D alpha for pattern None and all 2D alpha rules differ (COL-03/COL-04), and the mode is named 'OST' with default Solid instead of 'Original' (COL-MODE-01). | parsers-units |
| PORTING_STATUS.md / PORTING_MAP.md (Quantity And Measurement rows) | Curved length and imperial formula paths in UomService.hpp are source-present; only metric UOMs, attachments and rounding are missing. | Curved-length arithmetic is present but wrong for many inputs (perp_slope swapped, arc sweep direction, missing straight-chord shortcut; CURVE-01/02), UOM labels for codes 4 and 7 differ and roofing is not selectable (UOM-LBL-01), and quantity text uses locale grouping (QTY-FMT-01). | parsers-units |
| PORTING_BLOCKERS.md (Metric UOM support gap) | Only metric UOM codes/normalisation are missing from iOS measurement support. | Also missing: the metric measure-base mode of the Edit Condition dialog (mm display/parse via display_to_mm/inches_to_display(metric=True)), metric defaults, and metric quantity formatting. Bid::snap_increments_ost is the only metric-aware piece (it matches Windows). | parsers-units |
| PORTING_MAP.md (Import, Persistence, And Schema table, ost_importer row) | `ost_importer.py` -> `Core/Import/OstXmlParser.cpp` is Ported: the parser walks the same bid/page/bid-tail/global section model. | Only _parse_ost_xml has a counterpart, and even it lacks BidEmployees and PayClasses and adds a backslash sanitizer. The rest of ost_importer.py and of the write path has no iOS code: raw-graph validation, UID remap, BidPageSettings canonicalisation, master-data reconciliation, BidNo allocation, typed conversion and atomic write (IMP-01 to IMP-11). | import-ost-osp |
| PORTING_STATUS.md (Source-Confirmed Systems, `.ost` import path); PORTING_STATUS.json source_confirmed_systems ... | `.ost` import path \| Ported \| 'Needs fixture validation'. | Not a fixture-validation gap. Behavior is structurally different from Windows: no remap (host-verified overwrite of another project's rows), no validation, non-atomic writes that report success after SQL failures. Status should be Partial. | import-ost-osp |
| PORTING_MAP.md (osp_importer row); PORTING_STATUS.md `.osp` import path | iOS lists CAB entries, extracts .ost, extracts PDFs, rewrites ImagePath/OverlayImagePath and imports to SQLite (Partial, needs validation against real OSP variants). | True as a description of the flow, but it omits that Windows' package validation, TempImages!.tmp resolution, collision-safe staging, tif/tiff handling, 'preserve unresolved paths' and fail-on-corrupt-CAB behavior are all absent (IMP-12 to IMP-18). Real-variant validation is not the only gap. | import-ost-osp |
| PORTING_STATUS.md (Project/edit persistence row) and PORTING_MAP.md (settings_reader row) | Evidence includes `ProjectStore+RenderConfig.cpp`. | That file does not exist. load_render_config/save_render_config live in Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp (line 263). The Components folder has only Conditions, Folders, Import, Load, Takeoffs. (Outside this area; noted for the doc owner.) | import-ost-osp |
| PORTING_STATUS.md Import error handling row; PORTING_BLOCKERS.md B-007 | Parser/import failures produce NSError and alerts for picker/URL imports; error handling is Partial and only needs validation of progress/cancellation. | The NSError paths fire only for unreadable file, empty parse, duplicate Bid UID or DB open failure. import_project swallows every SQL failure (unknown column, missing table, step errors), always commits and returns true, so partial imports are reported as success (IMP-08). | import-ost-osp |
| PORTING_LOG.md Phase 10 entry (lines ~1568-1641) | Lists ProjectStore.hpp/.cpp import_project, ImportService.h/.mm and ProjectListViewController at the repo root; '40+ tables'; 'UID collision check prevents duplicate imports'; ... | import_project is now Core/Persistence/ProjectStore/Components/ProjectStore+Import.cpp; ImportService is under Services/; ProjectListViewController and ProjectLoader no longer exist (DatabaseViewController replaced the first); the schema has 64 tables; the collision check only compares Bids.UID, while page/condition/takeoff UIDs are INSERT OR REPLACEd over existing rows (host-verified); ... | import-ost-osp |
| PORTING_LOG.md lines 147 and 1236 (Initial Analysis; Phase 7 scope cleanup) | 'cab' is Windows-only and was removed from scope (winevent, cab, HWID, monitoring). | OSP import is in scope and implemented: Core/Import/CabExtractor.cpp is a custom CAB reader with MSZIP via zlib (-lz is linked), so CAB handling exists on iOS (Partial; see IMP-12 to IMP-18). | import-ost-osp |
| PORTING_STATUS.md SQLite table coverage row; PORTING_MAP.md schema row | Static comparison found 64 desktop table names plus _AppMeta, but 'column/type semantics need fixture validation'. | Columns are no longer an open question: a script comparison of database_creator.py _TABLE_DDL against OstSchema.cpp found identical column-name sets for all 64 tables (case-insensitive). The open items are types (YESNO/DATETIME/IMAGE flattened to INTEGER/TEXT/BLOB, IMP-05, IMP-20) and unknown-attribute handling (IMP-08). | import-ost-osp |
| PORTING_STATUS.md:27 (also PORTING_MAP.md:93-95, PORTING_STATUS.json 'OBJ, DXF, and Three.js exports' system e ... | OBJ/DXF/Three.js export \| Ported | Only the DXF writer class is identical to Windows. Exported content diverges: Three.js axes wrong (EXP-01), DXF axes wrong (EXP-02), export hole Y not negated (EXP-03), page-scale division (EXP-04), HTML negatives not per type (EXP-05), colour pipeline missing (EXP-06), grouping/naming/MTL differ (EXP-11, EXP-12, EXP-16), no escaping (EXP-08), cylinder/curved winding inverted (EXP-13). 'Partial' would be accurate. ... | export-formats |
| PORTING_MAP.md:87 | Desktop Three.js renderer/export -> Core/Export/threejs_writer.cpp: Ported | Windows exports ALL takeoffs on selected pages (hidden layers included) into a 3767-line viewer with layer panels, plan mode, hover, section cut and native shading. iOS writes a minimal, differently shaded viewer from visible meshes only (EXP-15, EXP-19, EXP-20). It is not a port. | export-formats |
| PORTING_LOG.md:1367 | Three.js coordinate transform: (x, y, z) -> (x, z, -y) matching desktop | That mapping is right only for Windows' Z-up meshes. The iOS meshes are Y-up (see PORTING_LOG.md:361-366 and gen_linear.cpp), so applying it lays the model on its side (EXP-01). The correct iOS mapping is identity. | export-formats |
| PORTING_LOG.md:1364,1366,1370-1371 | Three.js writer 'Ported from desktop infrastructure/visualization/renderers/threejs/', three.js v0.152.0, 3-point lighting with MeshPhongMaterial | That path no longer exists; the code is under presentation/visualization/renderers/threejs. Windows pins three@0.150.0, no tone mapping, linear encoding, a native 3-light shader, and tests assert there is NO MeshPhongMaterial, DirectionalLight, AmbientLight or ACES (EXP-19). | export-formats |
| PORTING_LOG.md:1148 | OBJ writer 'Sanitizes group/material names (spaces -> underscores, special chars removed)' (presented as desktop-equivalent) | Windows replaces a fixed list of characters with '_', keeps everything else including non-ASCII, prefixes 'Mat_' to leading digits and names materials '{cdn_type}_{condition}_{uid8}' per condition. iOS drops non-ASCII and all other symbols and names per takeoff (EXP-11, EXP-12). | export-formats |
| PORTING_LOG.md:1216 | DXF export 'Creates per-condition layers with correct RGB colors' | Layers are keyed by raw condition name, unsanitised; conditions with the same name merge and the last colour wins. Colours ignore grayscale (EXP-06, EXP-11). | export-formats |
| PORTING_LOG.md:1526 | 'Both rendering and export paths pass holes to create_mesh_for_takeoff' (hole map shared) | The export hole map is a duplicate that omits the Y negation, so holes are mirrored in exported area meshes (EXP-03). Hole parent/child resolution also differs from Windows (EXP-10). | export-formats |
| PORTING_MAP.md:30 (also PORTING_BLOCKERS.md:17, PORTING_STATUS.json:83, PORTING_PLAN.md:10) | Desktop evidence path presentation/visualization/exporters/ost_exporter.py | This file does not exist. The OST exporter is ost_visualizer/infrastructure/mdb/exporters/ost_exporter.py (tests under tests/infrastructure/mdb/exporters/). OSP export is in presentation/visualization/exporters/osp_exporter.py. | export-formats |
| PORTING_MAP.md:94 / PORTING_STATUS.md:44 / PORTING_BLOCKERS.md:71 | DXF writer 'Ported'; export has a 'basic no visible meshes alert' | The DXF writer source is byte-identical to Windows (accurate). However the same alert is shown for any file-write failure (nil return), not only for empty exports, and the DXF/HTML/OBJ content gaps above are not mentioned (EXP-21). | export-formats |
| PORTING_STATUS.md:20 and PORTING_MAP.md:27 (also PORTING_STATUS.json 'Project/edit persistence operations') | Persistence is implemented in ProjectStore+RenderConfig.cpp (render-config writes). | There is no ProjectStore+RenderConfig.cpp. load_render_config/save_render_config live in Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:263-290 and use get_pref/set_pref from ProjectStore_Internal.hpp:104-128 on the _AppPreferences table. | database-persistence |
| PORTING_STATUS.json:187 (SQLite table coverage ios_paths) | iOS path is Core/Persistence/ProjectStore.cpp. | The file is Core/Persistence/ProjectStore/ProjectStore.cpp plus Components/ProjectStore+{Import,Load,Folders,Takeoffs,Conditions}.cpp and ProjectStore_Internal.hpp. | database-persistence |
| PORTING_STATUS.md SQLite table coverage row ('Exact semantic parity needs fixture tests'), PORTING_STATUS.json ... | SQLite persistence is Ported. | DDL, indexes and seed rows do match (64/64 tables, same columns and order, 26 SchemaRegistry rows, 4 default layers). The behavior layer is not ported: import does not remap UIDs and can overwrite another bid's rows (DBP-09), errors are swallowed (DBP-11), cascades are incomplete (DBP-12, DBP-14, DBP-15), nested page folders can vanish (DBP-20). Status should be Partial. | database-persistence |
| PORTING_LOG.md Phase 10 (lines 1573, 1604-1609) | '40+ CREATE TABLE IF NOT EXISTS DDL'; 'UID collision check prevents duplicate imports'. | The schema has 64 desktop tables plus _AppMeta and _AppPreferences (66). The only check is is_bid_imported on the raw Bids.UID; child-table UID collisions between different bids are written with INSERT OR REPLACE and overwrite the earlier bid's rows. | database-persistence |
| PORTING_LOG.md:1616-1617 (reference files ported: export_ost.py parse_position and extract_z_value) | parse_position and extract_z_value were ported from the Python source into ProjectStore.cpp. | parse_position matches only the well-formed inputs (DBP-08). extract_z_value is a different hand-written parser: 31 of 71 probe names differ from Windows extract_z_value_from_name, including cases pinned by tests/infrastructure/parsers/test_position_parser.py (DBP-01). | database-persistence |
| PORTING_BLOCKERS.md B-005 'Remaining validation' and PORTING_PLAN.md:27 ('Verify non-loaded SQLite tables are ... | Non-loaded imported tables only need confirmation that they survive edits. | They do not all survive import: BidEmployees and PayClasses rows are never parsed (OstSchema.cpp BID_SECTIONS/GLOBAL_SECTIONS, DBP-23), tables with an unknown attribute are dropped silently (DBP-10), and deletes leave orphan rows in non-loaded tables (DBP-12, DBP-14, DBP-15). | database-persistence |
| PORTING_PLAN.md:53 ('Validate condition duplicate/delete and takeoff delete cascades against desktop behavior' ... | Cascades still to be validated. | Now validated by a row-level differential against the real Windows writers: they differ (DBP-12, DBP-13, DBP-14, DBP-15). | database-persistence |
| PORTING_STATUS.md:28 and PORTING_BLOCKERS.md (Import error handling: 'Parser/import failures produce NSError a ... | Import failures are reported to the user. | Only parse failure, an open failure or an already-imported Bid UID produce an NSError. ProjectStore::import_project ignores every insert and commit failure and returns true (DBP-10, DBP-11), so a project can import as empty with a success message. | database-persistence |
| PORTING_MAP.md:25-26 (mdb_reader/mdb_writer rows) | iOS 'writes common layer visibility, takeoff position/insert/delete/negative, condition, folder, and render-config changes' and loads the visualization/editing subset. | Accurate but incomplete: it also soft-deletes and permanently deletes bids, duplicates conditions and shifts RefNo. The loaded subset omits Sequence, SheetNo, Show, ZoomFac, Invert, Bitonal for pages, takeoff text styles, areas, annotations and condition-folder parents (see windowsOnly). | database-persistence |
| PORTING_STATUS.md:20 and PORTING_MAP.md:27 | Persistence and settings are implemented in `ProjectStore+RenderConfig.cpp`. | No such file exists. Core/Persistence/ProjectStore/Components contains only ProjectStore+Conditions.cpp, +Folders.cpp, +Import.cpp, +Load.cpp and +Takeoffs.cpp. load_render_config and save_render_config live in ProjectStore+Conditions.cpp:263-290, which the header comment of ProjectStore.cpp also states. | conditions |
| PORTING_MAP.md:41 | `domain/entities/layer.py`, `cdn_type.py`, `condition_folder.py`, `folder.py` -> Core/Domain/*: Ported. | Not ported at parity. Folder.hpp keeps children in an unordered_map and the load-time tree builder loses nested subtrees, keeps empty nested folders and drops cyclic ones. ConditionFolder.hpp has only uid and name (no parent_uid). The project-browser entities (project.py, project_factory.py, hierarchy_data.py, loaded_file.py) have no row at all; ... | conditions, project-browser |
| PORTING_MAP.md:39 | Condition.hpp models `trim/connect` and other fields as the desktop entity subset. | connect, connect_tolerance, snap_to_linear, color_line and backout do not exist on the Windows Condition entity (domain/entities/condition.py) and Windows never reads or edits them. They are iOS-only fields, only touched by the edit dialog and ProjectStore (COND-EXTRA-01). Windows-only: layer_visible, folder_uid, cdn_type_name default 'Unknown'. | conditions |
| PORTING_MAP.md:64 | `edit_condition_dialog.py` -> EditConditionViewController: 'Desktop dialog has broader options and workflows.' | For the field set the iOS dialog is broader, not narrower: it adds Connect, Tolerance and Snap to Linear. Windows-only parts are create, the type/layer dialogs, tower-crane tag warnings, OK/Apply and multi-select; several of the shared behaviors (style-change carry-over, ref-number validation, trim/curved exclusion, Area abs rise/run) are not matched. | conditions |
| PORTING_PLAN.md:53 | Open item: 'Validate condition duplicate/delete and takeoff delete cascades against desktop behavior.' | The validation shows they differ: delete misses 6 cascade targets plus self-reference NULLing (COND-DEL-01), duplicate keeps the source GUID and allocates UIDs without reserving referenced ones (COND-DUP-01/02). The RefNo shift is the one part that already matches. | conditions |
| PORTING_LOG.md:1157 | Phase 7: 'Conditions sorted alphabetically by name.' | That described the old action-sheet toggle. The current conditions list sorts only sections (folder name, then type name); rows inside a section follow std::unordered_map order, and the edit navigation list sorts by name (COND-LIST-01, COND-NAV-01). Windows sorts rows by ref_no. | conditions |
| PORTING_MAP.md:63 and PORTING_STATUS.md:40 | Quantity gaps are limited to metric UOMs, attachment footprint/perimeter and rounding; 'Page condition totals are displayed' by ConditionsViewController.mm. | ConditionsViewController.mm only formats the numbers (hiding zeros, unlike Windows); the totals come from ViewController.mm:708-727 with a different page scope than Windows (COND-QTYSCOPE-01). Also not listed: no per-calc-type UOM normalization on load (COND-UOMNORM-01), Area slope uses signed rise/run (COND-AREAABS-01), UOM 7 and the IN²/SQ labels are wrong (COND-UOMPICK-01). | conditions |
| PORTING_STATUS.md:20 and PORTING_MAP.md:27 | Persistence is split across ProjectStore+Folders.cpp, ProjectStore+Conditions.cpp, ProjectStore+Takeoffs.cpp and ProjectStore+RenderConfig.cpp. | ProjectStore+RenderConfig.cpp does not exist. Render-config load/save are in ProjectStore+Conditions.cpp:263-280. ProjectStore+Folders.cpp is project/bid folder CRUD (BidProjects), not page folders. Page-folder building lives in ProjectStore+Load.cpp:272-345, and layer-visibility writes in ProjectStore+Takeoffs.cpp:7-25. | layers-pages |
| PORTING_MAP.md:41 | domain/entities/layer.py, cdn_type.py, condition_folder.py and folder.py are 'Ported' for layer visibility and page folders. | Only the Show flag and the blank/unknown-uid-is-visible rule are ported. The Windows layer semantics are missing: the Image layer driving page visibility, Comments exclusion, template/locked merge, name normalization. The page-folder builder loses 3-level nesting (LP-09). 'Ported' overstates it. | layers-pages |
| PORTING_MAP.md:14 and PORTING_STATUS.md:20 | Current-scope folder, condition, takeoff, layer and render-config operations exist. | The only layer write is UPDATE BidLayers SET Show (ProjectStore+Takeoffs.cpp:7-25). There is no layer insert/rename/delete/reorder, and no page write of any kind (rename, scale, rotation/flip, area, overlay, view state, selected page). | layers-pages |
| PORTING_STATUS.md:41 (Layers/pages row) and PORTING_MAP.md:38,75 | Layers/pages are Partial: fields are loaded and page selection/folders work; only overlay rendering and full page-settings parity are missing. | The documented gap list is too small. Core results already differ: page order uses Index1 not Sequence (LP-08), the folder tree breaks at depth 3 (LP-09), folder tap replaces the selection (LP-10), the Image layer is ignored (LP-15), PlanView ignores page_index/rotation/flip (LP-16), page quantities mix the 2D and 3D page sets (LP-04) and 3D geometry is scaled/untransformed unlike Windows (LP-01, LP-02). | layers-pages |
| PORTING_STATUS.md:37, PORTING_BLOCKERS.md:37, PORTING_PLAN.md:33 | PDFCanvasView draws page 1 only; page-index-aware drawing is needed 'if multi-page PDFs are required'. | The code claim is accurate (PDFCanvasView.mm:18-19). The condition is not: Windows always renders page_index = Index1-1, which iOS loads (Page.page_index) and never uses, so every project with a multi-page PDF shows the wrong background. Rotation, flip and the real PDF page size are also ignored (LP-16). | layers-pages |
| PORTING_LOG.md (2026-03-23 Phase 4, 'Coordinate transformation approach') | Dividing coordinates and dimensions by the page scale ratio (sr = 1/(sf2/sf1)) 'matches the desktop pipeline'. | Not the current Windows 3D pipeline: visualization_provider.generate_meshes forces ratio 1.0 for all pages, so 3D geometry stays in unscaled OST units (test_mesh_pipeline_preserves_model_scale_when_flipping). The ratio is only used for 2D PDF-point conversion. | layers-pages |
| PORTING_LOG.md (2026-03-23 Phase 6 and Phase 6b revision) | Page selector sorted by page_index then name; tapping a folder replaces the selection and is 'idempotent'; first page selected on load. | Describes iOS behavior, not Windows parity. Windows sorts by BidPages.Sequence, Name, UID; a folder check adds or removes only its pages; the initial page is BidSettings.BidPageSelectedUID or the first page of the tree walk. | layers-pages |
| PORTING_LOG.md (Phase 6b and Phase 7, layers) and PORTING_STATUS.json phase notes | The Layers UI toggles all layers and 'Show All / Hide All' match the desktop layer system. | The visible rule matches, but the Windows layer list excludes the Comments layer, the Image layer hides the page image, conditions on hidden layers cannot be placed, and Hide All also hides layer-less conditions in memory (LP-15, LP-17, LP-18, LP-22). | layers-pages |
| PORTING_MAP.md:37 and PORTING_STATUS.md Domain table | Bid.hpp models core bid metadata and snap increments (Partial). | snap_increments_ost() matches Windows' metric conversion, but TakeoffIncrements NULL/0 loads as 0.0 (no snapping) where Windows defaults to 1.0 (LP-19). | layers-pages |
| PORTING_STATUS.md:20 and PORTING_MAP.md:27 | Persistence / render-config writes live in ProjectStore+RenderConfig.cpp | That file does not exist. Render config load/save is in Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp (lines ~270-287), stored in _AppMeta prefs. | rendering-planview |
| PORTING_STATUS.md:25 and PORTING_MAP.md:73 | 2D takeoff view: overlays, holes, negative markers and patterns/opacities are represented | No hatch/line-pattern rendering exists; fill alpha is Solid ? 1.0 : 0.5 for every pattern (Windows: NONE and line patterns are 0 with hatch lines); every child takeoff (including attachments) is dropped as a hole (PlanView.mm:229); holes are white polygons rather than cut-outs; curved linears are drawn straight. See RPV-17, RPV-22, RPV-23, RPV-31. | rendering-planview |
| PORTING_STATUS.md:42, PORTING_MAP.md:56, PORTING_BLOCKERS.md:68, PORTING_STATUS.json 'Plan editing and snappin ... | iOS has grid and angle snapping; only the endpoint/midpoint/perpendicular snap index (and backout/paste-backout) are missing | The existing grid and angle snaps do not match Windows: 22.5 degree vs 15, no pixel threshold, per-axis rounding after angle snap, half-away-from-zero rounding (RPV-04..07). The Windows snap index also has INTERSECTION and PDF-line sources and right-angle snap, all absent. Drag and placement rules also diverge (RPV-08..10, 13, 15, 20). | rendering-planview |
| PORTING_MAP.md:50 ('geom_utils, coord_transform ... Ported') | Portable C++ geometry modules are present and ported | Core/Geometry/coord_transform.cpp and geom_utils.cpp are stale copies of the Phase 1 import. Windows has since added page_transform.hpp (new rotation/flip semantics, validation), a stricter polygon_is_valid, an origin-shifted signed_area and point_to_segment_distance (RPV-01, RPV-11, RPV-14). | rendering-planview |
| PORTING_STATUS.md:43 (Color/material handling) | BGR-to-RGB conversion, render color modes, transparency handling are present | Negative color_fill is mapped to grey (Condition.hpp:65-69) while Windows uses two's-complement bytes; 3D pattern 0 gets alpha 0.5 where Windows gives 1.0; 2D alpha table differs (RPV-12, RPV-23, RPV-24). iOS labels the mode 'OST' where Windows uses 'Original' (separate prefs, not shared). | rendering-planview |
| PORTING_LOG.md (2026-03-23 Phase 5c 'Three-point directional lighting') | Matches desktop opaque_fragment_shader: ambient 0.2, key 0.6, fill 0.3, rim 0.2 (renderer.cpp lines 301-326) | Current Windows renderer.cpp uses ambient 0.55, key 0.45, fill 0.35, rim 0.2 (lines 607-613, 640-646, 1254-1263), and no normal flip. The cited line range is obsolete. | rendering-planview |
| PORTING_STATUS.json:187-189 (source_confirmed_systems / SQLite table coverage) | ios_paths include Core/Persistence/ProjectStore.cpp | The store now lives under Core/Persistence/ProjectStore/ProjectStore.cpp and Components/*.cpp. | rendering-planview |
| PORTING_STATUS.md:37 and PORTING_BLOCKERS.md B-003 | PDF viewing draws page 1; rotation and scaling need device validation | Confirmed for page 1, but it is a hard divergence: page_index (Index1-1) is loaded yet ignored, stored dimensions are used instead of the actual PDF page size, and OST rotation/flips are applied only to the overlay (RPV-18, RPV-19). | rendering-planview |
| PORTING_STATUS.md:19 (Project browser row) | Project browser \| Ported \| DatabaseViewController.mm, ImportService.mm \| Project/folder list, import, delete, and folder actions are source-present. | Source-present but it diverges from Windows in ordering, delete/restore semantics, the Deleted Bids identity, UID allocation, the cascade, validation and error reporting (21 divergences in this report). The real implementation is in Core/Persistence/ProjectStore/Components/ProjectStore+Folders.cpp and ProjectStore.cpp (resolve_deleted_bids_project_uid, delete_project, permanently_delete_project). ... | project-browser |
| PORTING_STATUS.md:20 and PORTING_MAP.md:27 | Project/edit persistence evidence lists ProjectStore+RenderConfig.cpp. | That file does not exist. Render-config load/save lives in Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp (see the header comment in ProjectStore.cpp:11-12). | project-browser |
| PORTING_STATUS.md:18 and PORTING_MAP.md:24 | Static comparison found all 64 desktop table names in iOS plus _AppMeta. | create_schema also creates _AppPreferences (ProjectStore.cpp:491-497), which holds the render-config preferences; the trackers never list it. | project-browser |
| PORTING_LOG.md (Phase 10 entry, ~lines 1568-1648) | Creates ProjectListViewController.h/.mm (UITableView of projects, swipe-to-delete), ProjectStore.hpp/.cpp with import_project(), list_projects(), load_project(), delete_project(), and 'ViewController. ... | ProjectListViewController no longer exists; the browser is Controllers/DatabaseViewController.* (the root of a UINavigationController from SceneDelegate.mm, which pushes ViewController initWithBidUID:). list_projects() does not exist; it was replaced by load_hierarchy() with folder groups. ... | project-browser |
| PORTING_LOG.md (Phases 4, 6, 6b) | JSON data pipeline: Tools/export_ost.py, ProjectLoader.h/.mm, Resources/sample_project.json, project picker action sheet listing bundled/Documents JSON, folder button bar from JSON format v2/v3. | None of these exist any more (no Tools/ directory, no ProjectLoader.*, Resources/ is empty). Projects come only from the SQLite database via ImportService/ProjectStore, and the page selector is driven by ProjectService.mm and ViewController+PageSelector.mm. | project-browser |
| PORTING_LOG.md (Phase 10 'Key design decisions') | 'UID collision check prevents duplicate imports'. | This is a divergence rather than parity: iOS keeps the file's own UIDs and refuses a second bid with the same Bids.UID, and uses INSERT OR REPLACE for child rows, so colliding UIDs silently overwrite other bids' rows (BRW-06). Windows remaps every UID, so duplicate imports are allowed and independent. | project-browser |
| PORTING_PLAN.md:68 and PORTING_STATUS.md:20 | [x] Project browser and folder organization source paths; folder writes exist for 'current scope'; Priority 4 only lists condition duplicate/delete and takeoff delete cascade validation. | The plan has no item for project/bid delete cascade, trash restore, Deleted Bids identity, folder delete policy, UID allocation or bid/page ordering parity; these are the open items in this report. Bid permanent-delete cascade parity (BRW-04) is also not covered by the 'verify non-loaded tables preserved after edits/deletes' item in PORTING_PLAN.md:27. | project-browser |
| PORTING_STATUS.md:20 (Project/edit persistence row) and PORTING_MAP.md:27 | Render-config writes live in ProjectStore+RenderConfig.cpp. | No such file exists. load_render_config and save_render_config are in Core/Persistence/ProjectStore/Components/ProjectStore+Conditions.cpp:263-290, using get_pref and set_pref from ProjectStore_Internal.hpp:104-128 over the _AppPreferences table. | settings-undo-camera |
| PORTING_MAP.md:27 | Windows settings_reader.py and settings_operations.py map to RenderConfig.hpp, ProjectStore+RenderConfig.cpp and ImportService.mm (render config load/save). | Those Windows modules read and write the MDB Bids, global Settings and BidSettings tables (cover-sheet defaults, TakeoffIncrements, ScaleStyle, BidPageSelectedUID). The Windows counterpart of RenderConfig is domain/entities/config.py plus domain/aggregates/config_aggregate.py plus application/services/config_service.py, persisted by a JSON repository. | settings-undo-camera |
| PORTING_PLAN.md:52 (Priority 4), PORTING_MAP.md:78, PORTING_STATUS.md:42 | Expand undo/redo scope for condition edits, layer edits, folder actions or project actions to reach undo parity. | Windows UndoRedoService covers plan items (takeoff move, rotate, insert, delete, paste, property edits such as negative, curve, area and condition assignment, annotations, AI changesets). It has no undo for condition, layer or folder edits, and it clears history when a condition is deleted (project_write_service.py:690). ... | settings-undo-camera |
| PORTING_LOG.md:1558 | Camera improvements: two-finger pan via UIPanGestureRecognizer with min/maxNumberOfTouches=2. | CameraController.mm:17-22 installs one pan recognizer with maximumNumberOfTouches = 1. Translate-pan is a FAB mode toggle (panMode), not two-finger pan. | settings-undo-camera |
| PORTING_LOG.md:610-617 (Phase 6 bottom toolbar) | Toolbar actions are Reset, minus, plus, Fit and Project. | ViewController.mm:281-283 builds Reset, Undo, Redo, Delete, Conds, Layers, Export. Reset calls fitScene, and there are no zoom +/- or separate Fit buttons. | settings-undo-camera |
| PORTING_LOG.md:685-687 and :1175 | Settings/preferences screen and undo/redo are deferred, not done. | A Visualization Settings sheet exists (DatabaseViewController.mm:594-639) and undo/redo is implemented (UndoRedoController.*). The later 2026-05-15 entries never supersede these deferrals. | settings-undo-camera |

## Appendix: alias index (alias -> kept id)

BOOL-01 -> LP-20, BRW-03 -> DBP-18, BRW-04 -> DBP-12, BRW-06 -> IMP-06, BRW-07 -> DBP-11, BRW-14 -> DBP-20, BRW-18 -> LP-08, BRW-20 -> DBP-31, BRW-M02 -> IMP-07, COL-MODE-01 -> SET-01, COND-AREAABS-01 -> QTY-10, COND-DIM-01 -> FMT-01, COND-DIM-02 -> DLG-FINITE-01, COND-DUP-02 -> DBP-18, COND-M01 -> QTY-01, COND-M02 -> DLG-PREC-01, COND-M03 -> DBP-35, COND-M04 -> DBP-11, COND-M06 -> COND-UPD-02, COND-NAME-01 -> QTY-23, COND-QTYFMT-01 -> QTY-22, COND-QTYROUND-01 -> QTY-04, COND-QTYSCOPE-01 -> QTY-11, COND-UOMNORM-01 -> QTY-06, COND-UOMPICK-01 -> QTY-16, COND-ZVAL-01 -> ELV-01, CURVE-01 -> QTY-07, CURVE-02 -> QTY-08, DBP-01 -> ELV-01, DBP-02 -> QTY-13, DBP-05 -> DBP-04, DBP-06 -> QTY-06, DBP-09 -> IMP-06, DBP-10 -> IMP-08, DBP-13 -> COND-DUP-01, DBP-15 -> COND-DEL-01, DBP-16 -> COND-UPD-01, DBP-21 -> BRW-11, DBP-22 -> BRW-09, DBP-23 -> IMP-11, DBP-24 -> IMP-05, DBP-25 -> IMP-10, DBP-26 -> BRW-13, DBP-27 -> LP-08, DBP-28 -> LP-20, DBP-29 -> LP-20, DBP-30 -> COND-FOLDER-01, DBP-32 -> COND-FOLDER-01, DBP-36 -> SET-01, DBP-38 -> BRW-08, DBP-39 -> BRW-05, DBP-M01 -> BRW-15, DBP-M03 -> IMP-09, DBP-M04 -> IMP-11, DLG-DISPSIZE-01 -> COND-VAL-01, DLG-ELEV-01 -> COND-ELEV-01, DLG-EXTRA-01 -> COND-EXTRA-01, DLG-M01 -> COND-STYLE-01, DLG-REF-01 -> COND-REF-01, DLG-SAVE-01 -> COND-UPD-02, EXP-04 -> MSH-01, EXP-07 -> COL-01, EXP-10 -> MSH-14, EXP-23 -> LP-22, EXP-M01 -> MSH-08, EXP-M03 -> MSH-14, GEO-01 -> MSH-01, GEO-02 -> EXP-03, GEO-03 -> MSH-02, GEO-04 -> MSH-14, GEO-08 -> RPV-02, GEO-10 -> RPV-09, GEO-11 -> RPV-08, GEO-12 -> RPV-07, GEO-13 -> RPV-04, GEO-16 -> GEO-06, GEO-17 -> RPV-25, GEO-M01 -> RPV-15, GEO-M03 -> DBP-08, GEO-M04 -> DBP-07, IMP-M04 -> IMP-05, LOAD-01 -> DBP-04, LOAD-M01 -> QTY-13, LP-01 -> MSH-01, LP-02 -> MSH-02, LP-03 -> GEO-06, LP-04 -> QTY-11, LP-05 -> QTY-01, LP-06 -> EXP-03, LP-07 -> IMP-06, LP-09 -> DBP-20, LP-13 -> EXP-14, LP-14 -> MSH-14, LP-17 -> COND-PLACE-01, LP-19 -> DBP-04, LP-21 -> COND-LAYER-01, LP-23 -> EXP-15, LP-M01 -> RPV-03, LP-M02 -> QTY-15, LP-M03 -> DBP-17, LP-M04 -> DBP-31, MSH-03 -> GEO-05, MSH-04 -> GEO-05, MSH-12 -> EXP-03, MSH-17 -> EXP-15, MSH-18 -> EXP-14, MSH-M01 -> EXP-02, MSH-M02 -> EXP-13, MSH-M03 -> DBP-03, MSH-M04 -> ELV-01, POS-01 -> DBP-08, POS-02 -> DBP-07, POS-M02 -> QTY-19, QTY-20 -> COND-FOLDER-01, QTY-21 -> COND-LIST-01, QTY-FMT-01 -> QTY-12, QTY-M01 -> COND-DEL-01, QTY-M02 -> DBP-07, RPV-01 -> GEO-06, RPV-11 -> GEO-07, RPV-12 -> COL-01, RPV-14 -> GEO-09, RPV-18 -> LP-16, RPV-19 -> LP-16, RPV-21 -> COND-PLACE-01, RPV-23 -> COL-04, RPV-24 -> COL-03, RPV-28 -> MSH-02, RPV-30 -> COL-02, RPV-M02 -> UND-08, RPV-M05 -> COL-04, RPV-M06 -> EXP-03, SET-04 -> COL-03, SET-05 -> COL-04, SET-06 -> COL-02, SET-07 -> EXP-06, SNP-01 -> DBP-04, SNP-02 -> RPV-04, SNP-03 -> RPV-07, SNP-04 -> RPV-08, SNP-05 -> RPV-10, SNP-06 -> RPV-13, SNP-07 -> RPV-05, SNP-M01 -> RPV-09, UI-NAME-01 -> QTY-23, UI-SHAPE-ORDER-01 -> COND-SHAPE-01, UND-04 -> DBP-17, UND-05 -> DBP-14, UND-M01 -> RPV-15, UNITS-M01 -> QTY-06, UOM-LBL-01 -> QTY-16, XML-01 -> IMP-11, XML-02 -> IMP-02, LOAD-01 -> DBP-04 (and DBP-03)
