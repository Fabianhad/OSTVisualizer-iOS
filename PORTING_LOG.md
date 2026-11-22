# OST Visualizer — iOS Porting Log

> Chronological record of porting decisions, changes, and progress.

> **Reading note (2026-11-19, updated 2026-11-21):** entries dated before 2026-11-15 are historical and may be superseded. Several of them describe files, tables, Windows behaviour or "Ported" status that were later found to be wrong or have since changed (`PARITY_MAP.md` section I lists the stale claims). The entries from 2026-11-15 on are listed newest first and record what was true on that day; where a later entry changes a statement it says so (the "nothing was built" and "no GitHub run exists" statements of 2026-11-15 to 2026-11-19 were changed by the CI runs of 2026-11-20; the code of job 5 was seen by run 44856186199 of 2026-11-21 UTC, see "Verified on GitHub" in the job 5 entry). Job 5 (2026-11-20, fourth entry below) re-baselined to Windows `b9902e6`: statements in older entries about the Windows behaviour at `6d4786f` (the "possible Windows defects", the deliberate deviations, the open condition-number question) are superseded by it. The current truth is `PORTING_STATUS.md` (with `PORTING_STATUS.json`), `PORTING_MAP.md` and `PARITY_MAP.md`; the remaining work is in `PORTING_PLAN.md`. Job 6 (2026-11-21, the third entry below) re-baselined to Windows `49a2d59`: statements in older entries that job 6 changed (COND-M08 "show Linear" and "unverified on Windows", `5'6` accepted as 60 inches by the dialog, `duplicate_condition` and the Windows allocator "unguarded") are superseded by it; the code of job 6 was seen by run 44953904172 of 2026-11-21 UTC (see "Verified on GitHub" in the job 6 entry); the last CI-verified code state is `e151655`. Job 7 (2026-11-21, the second entry below, documents only) recorded the owner's five decisions of that day and the device-check table, which the owner returned blank (no merge into main): statements in older entries that left the quantity reset of Windows, the title of the Duplicate alert, two duplicates at 2147483646, the stray quote of the `5'6` rule or the stale citations in four comment files open ("owner decision pending", "for the owner", "wrong-number risk") are superseded by it. Job 8 (2026-11-21, step 0, the first entry below, documents only) recorded that the owner's device checks DEV-01 to DEV-11 all PASSED on the owner's iPhone with the IPA of run 44953904172: statements in older entries that said the device-check table was blank, that all eleven checks stay open, that no item is verified on device, that 172 fixed and 27 fixed-ui-unverified are the counts, or that the code has never run on a device are superseded by it (176 fixed, 23 fixed-ui-unverified).

---

## 2026-11-21 - Job 8 step 0: device checks DEV-01 to DEV-11 all PASS, items verified on device

The owner's message of job 8: the owner tested on their iPhone with the IPA of GitHub run 44953904172 (head commit `e151655`) and recorded the device checks DEV-01 to DEV-11 as all PASS (11 of 11; the owner gave no per-check notes). The earlier statement that the table came back with all eleven results blank (job 7, 2026-11-21, commit `80fb867`) was true when written and stays as history. The owner's rule (substance): mark the items covered only by these checks as 'verified on device' in the docs (COND-ELEV-01 needs DEV-04 and DEV-10, DLG-FINITE-01 needs DEV-05 and DEV-11; leave the other fixed-ui-unverified items as they are and carry them into the next device pack). This entry is documents only: no code, test, fixture, code comment or behaviour of the app was changed and the Windows repository was not touched (read only). `PORTING_BLOCKERS.md`, `PARITY_MAP.md` ("Job 8 outcome"), `PARITY_DIVERGENCES.json` (`header.deviceChecksJob8`, `statusBeforeDeviceCheck` and `deviceVerified` on the records), `PORTING_STATUS.md` / `.json`, `PORTING_PLAN.md`, `PORTING_MAP.md`, `BUILD.md` and `Config/Tests/README.md` carry the same facts.

### Device checks (DEV-01 to DEV-11)
- The owner tested on their iPhone with the IPA of GitHub run 44953904172 (head commit `e151655`) and recorded the device checks DEV-01 to DEV-11 as all PASS (11 of 11; the owner gave no per-check notes). The earlier statement that the table came back with all eleven results blank (job 7, 2026-11-21, commit `80fb867`) was true when written and stays as history. The owner gave no per-check notes.
- DEV-01: the device bug 1 of the two device bugs: transparent meshes, the OIT path fix (no item id): **PASS** (no item id; verified on device, fix `aef35d8` (transparent meshes)).
- DEV-02: the device bug 2: Delete selection (no item id): **PASS** (no item id; verified on device, fix `34e14c9` (Delete selection)).
- DEV-03: COND-STYLE-02: style change in a metric and in an imperial bid: **PASS** (COND-STYLE-02 is verified on device and fixed).
- DEV-04: COND-ELEV-01: the elevation field (placeholder, refusal of a unit-less text; job 5): **PASS** (with DEV-10, COND-ELEV-01 is verified on device and fixed).
- DEV-05: DLG-FINITE-01: the condition-number alert (job 5): **PASS** (with DEV-11, DLG-FINITE-01 is verified on device and fixed).
- DEV-06: LP-24 and LP-22: Hide All / Show All with shared layers: **PASS** (the status of LP-24 (fixed) and LP-22 (no-longer-divergent) is unchanged).
- DEV-07: COND-PLACE-01: Image-layer placement (the Image-layer half only): **PASS** (covers the Image-layer half of COND-PLACE-01 only, the item stays fixed-ui-unverified).
- DEV-08: EXP-12 and EXP-16: export names: **PASS** (the status of EXP-12 (fixed) and EXP-16 (no-longer-divergent) is unchanged).
- DEV-09: COND-M08: the unknown-type condition edit (job 6): **PASS** (COND-M08 is verified on device and fixed).
- DEV-10: COND-ELEV-01: the "5'6" refusal (job 6): **PASS** (with DEV-04, COND-ELEV-01 is verified on device and fixed; the observation of note l was observed on device, PASS).
- DEV-11: DLG-FINITE-01: the duplicate overflow alert (job 6): **PASS** (with DEV-05, DLG-FINITE-01 is verified on device and fixed).

### Verified on device
- Verified on device (every check that covers the item is PASS): COND-ELEV-01 (DEV-04 and DEV-10), DLG-FINITE-01 (DEV-05 and DEV-11), COND-M08 (DEV-09) and COND-STYLE-02 (DEV-03); they move from fixed-ui-unverified to fixed (`statusBeforeDeviceCheck` keeps the earlier value in `PARITY_DIVERGENCES.json`). Covered by a passed check, status unchanged: LP-24 and LP-22 (DEV-06; LP-24 fixed, LP-22 no-longer-divergent) and EXP-12 and EXP-16 (DEV-08; EXP-12 fixed, EXP-16 no-longer-divergent). The two device bugs of 2026-11-20 (no item id) are verified on device 2026-11-21 (PASS): DEV-01 (transparent meshes, fix `aef35d8`) and DEV-02 (Delete selection, fix `34e14c9`). COND-PLACE-01 stays fixed-ui-unverified: DEV-07 PASSED for the Image-layer half only; the other half (hide a layer while placing: the mode goes to View, the FAB place button is off, the row is greyed; show it again: placement resumes; hide, pick another tool, show: nothing resumes) is the non-DEV row of the device-check table, was not among the eleven checks and is carried into the next device pack.
- The Windows-look point (an opacity of 0.5 drawn almost opaque on Windows) is a separate owner decision and stays as it is.

### Status after the job
- Counts: 176 fixed, 23 fixed-ui-unverified, 16 wont-fix, 0 blocked-needs-decision, 1 cannot-reproduce, 4 no-longer-divergent (= 220; 199 items ported, unchanged; dispositions unchanged: fix 199, wont-fix 16, no-longer-divergent 4, cannot-reproduce 1); tests 1760 (1769 with Manifold) unchanged. Four transitions, fixed-ui-unverified -> fixed: COND-ELEV-01, COND-STYLE-02, DLG-FINITE-01, COND-M08 (`statusBeforeDeviceCheck` keeps the earlier value; `deviceVerified` records the date, the checks, the device and the build). The outcome of the 44 decisions of 2026-11-16 is now 26 fixed, 4 fixed-ui-unverified, 13 wont-fix, 1 no-longer-divergent.
- Items verified on device through the DEV checks: COND-ELEV-01, DLG-FINITE-01, COND-M08 and COND-STYLE-02 (the four that were fixed-ui-unverified), LP-24, LP-22, EXP-12 and EXP-16 (already fixed or no-longer-divergent, status unchanged) and the two device bugs of 2026-11-20 (no item id).

### Carried into the next device pack
- Carried into the next device pack: the 23 items that stay fixed-ui-unverified, namely GEO-06, LP-16, RPV-17, RPV-31, RPV-32, RPV-M07, RPV-16, UND-10, UND-03, UND-07, UND-09, QTY-15, COND-PLACE-01, COND-UPD-03, COND-LAYER-01, COND-TRIM-01, UI-M01, BRW-02, BRW-08, BRW-16, DBP-31, POS-M01, EXP-21 (COND-PLACE-01 is among them because DEV-07 covered only its Image-layer half); the next device pack is `DEVICE_CHECKS.md` at the repository root (to be created in job 8 on the new branch `ios-feature-m1`).

### Merge and CI
- The merge of `ios-parity-sync` into `main` was requested by the owner (job 8) and is recorded after it happens; this documents-only commit carries `[skip ci]` and the last CI-verified code state stays `e151655` (run 44953904172). The last full local host runs (1760 / 1769 tests, MSVC and clang) were made on the code of `e151655`, which has not changed.

### Owner notes
- Owner notes are unchanged (32 entries: 4 resolved, 2 informational, 26 open); DEV-10 passing also covers the observation of note l (a stored name such as `Footing @T 5'6` opens with `5'6` in the field and any other edit of it is refused until the elevation is fixed or cleared): observed on device, PASS; note l stays open (the Windows behaviour is the owner's call).

### Tests and CI
- No code changed, so the test counts (1760 tests, 1769 with Manifold) are unchanged. CI: no GitHub run exists for the documents-only commit of job 8 (`[skip ci]`); the last CI-verified code state is `e151655` (run 44953904172, success).

### Not verified / still open
- The 23 fixed-ui-unverified items (GEO-06, LP-16, RPV-17, RPV-31, RPV-32, RPV-M07, RPV-16, UND-10, UND-03, UND-07, UND-09, QTY-15, COND-PLACE-01, COND-UPD-03, COND-LAYER-01, COND-TRIM-01, UI-M01, BRW-02, BRW-08, BRW-16, DBP-31, POS-M01, EXP-21) have not been checked on a device; the other half of COND-PLACE-01 is among them. Everything else that DEV-01 to DEV-11 do not cover (open-in and the document picker, PDF drawing, iPad popovers, memory and frame time, large projects; how the files exported in DEV-08 reached the desktop viewer or text editor was not reported) is not verified either.
- The 26 open owner-note entries (unchanged); of the owner notes only b (LP-24), c and n ask the owner something.
- The merge of `ios-parity-sync` into `main` is recorded after it happens.

### Documents
- Updated in this job: `PORTING_BLOCKERS.md`, `PARITY_MAP.md`, `PARITY_DIVERGENCES.json`, `PORTING_STATUS.md` and `.json`, this log, `PORTING_PLAN.md`, `PORTING_MAP.md`, `BUILD.md` and `Config/Tests/README.md`. The older entries of this log were left as history; the job 7 entry carries history markers where it said the checks were blank or open, and the job 6 entry carries an 'Updated 2026-11-21 (job 8)' marker on its DEV-01 to DEV-11 line.

---

## 2026-11-21 - Job 7: owner decisions recorded, device checks blank, no merge

The owner's instruction: "Record my decisions and the device-check results, then merge ios-parity-sync into main if everything passed. Windows repo read-only. No force-push. Never change app behavior in this job." The five decisions below were recorded, the owner's device-check table (the header said "tested on my iPhone with the IPA from run 44953904172; fill in PASS or FAIL with a note") was returned with all eleven results blank, so no outcome is known and none is recorded, and the merge rule of the owner (any check FAIL or blank: do not merge, report which ones and the items they cover, and stop) stopped the job before any merge. This entry is documents only: no code, test, fixture, code comment or behaviour of the app was changed, no status changed and the Windows repository was not touched (read only). `PORTING_BLOCKERS.md` (section "Decisions of the owner and the device-check table, 2026-11-21 (job 7)"), `PARITY_MAP.md` ("Job 7 outcome"), `PARITY_DIVERGENCES.json` (`header.decisionsJob7`) and `PORTING_STATUS.md` / `.json` carry the same facts.

### Decisions of the owner (5)
- **Overflow alert title (DLG-FINITE-01, the iOS DBP-11 wording).** Decided by the owner: the overflow alert of Duplicate keeps the title "Duplicate Error" (Windows titles it "Duplicate Conditions"; the message text is identical to Windows); no change.
- **Quantity reset (COND-M08, owner note k, FYI-20).** Decided by the owner: the quantity reset is NOT ported; recorded as a possible Windows defect: silent data loss (Windows replaces a stored quantity that the option list of the type does not offer by "(none)" on every save of that condition; for an unknown type the option list is always the Count list, so it hits unknown types too).
- **Two duplicates at 2147483646 (DLG-FINITE-01).** Decided by the owner: two duplicates at 2147483646 are pinned in Core (`refuse_new_condition_numbers(2147483646, 2)` gives X = 2147483648, plural) and by sequential store calls, and are not reachable through the UI (Duplicate copies one condition per request).
- **Stray quote in the elevation text (COND-ELEV-01, possible Windows defect (1)).** Decided by the owner: a single stray ASCII `"` anywhere in the elevation text switches the closing-inch-mark rule off: recorded as a possible Windows defect and ported as is (confirmed by the owner).
- **Stale citations in four comment files.** Decided by the owner: the stale Windows line-number citations in four iOS comment files are left as they are (`Core/Domain/CameraFraming.hpp` :6 cites `mesh_view.py`; `Config/Tests/Settings/test_scene_rendering.cpp` :16 cites `mesh_view.py`; `Config/Tests/Persistence/test_load_fields.cpp` :86 and :88 cite `plan_view/view.py` and `test_view.py`; `Config/Tests/Settings/test_takeoff_selection.cpp` :14 cites `test_view.py`); the old -> new map of these Windows files in `PARITY_MAP.md` (section "Re-baseline 2026-11-21") stays the reference; no code comment, test or behaviour was touched.
- The records that said "pending", "for the owner", "owner decision" or "wrong-number risk" about these topics keep their text as history and carry a marker "Superseded / updated 2026-11-21 (job 7)" (the COND-M08 and DLG-FINITE-01 records, owner note k, the DBP-11 note, the possible-Windows-defect lists of job 6 in `PORTING_BLOCKERS.md`, `PARITY_MAP.md`, `PARITY_DIVERGENCES.json` and `PORTING_STATUS.md` / `.json`).

### Device checks (DEV-01 to DEV-11)
- History (superseded by the entry "2026-11-21 - Job 8 step 0" above): 2026-11-21 (job 7): the owner's device-check table was returned with all eleven results blank (DEV-01 to DEV-11); nothing was recorded as passed or failed at that time; all eleven stayed open; no status changed; the merge into main was not done.
- Coverage of the eleven checks (from the device-check table): DEV-01 the device bug 1 of the two device bugs (transparent meshes, the OIT path fix; no item id); DEV-02 the device bug 2 (Delete selection; no item id); DEV-03 COND-STYLE-02; DEV-04 COND-ELEV-01 (placeholder and the refusal of a unit-less text, job 5); DEV-05 DLG-FINITE-01 (the condition-number alert, job 5); DEV-06 LP-24 and LP-22; DEV-07 COND-PLACE-01; DEV-08 EXP-12 and EXP-16; DEV-09 COND-M08; DEV-10 COND-ELEV-01 (the "5'6" refusal, job 6); DEV-11 DLG-FINITE-01 (the duplicate overflow alert, job 6).
- Rule of the owner: a fixed-ui-unverified item becomes "verified on device" only when every check that covers it is recorded PASS (COND-ELEV-01 needs both DEV-04 and DEV-10, DLG-FINITE-01 needs both DEV-05 and DEV-11); the rest stays as it is. The other fixed-ui-unverified items (GEO-06, LP-16, `RPV-*`, `UND-*`, QTY-15, COND-UPD-03, COND-LAYER-01, COND-TRIM-01, UI-M01, `BRW-*`, DBP-31, POS-M01, EXP-21 and the like) are covered by rows of the device-check table that are not among DEV-01 to DEV-11 and are untouched by this. Consequence in job 7 (history, replaced in job 8): no item was recorded as "verified on device"; the statuses stayed 172 fixed, 27 fixed-ui-unverified, 16 wont-fix, 4 no-longer-divergent, 1 cannot-reproduce (= 220); and the merge rule of the owner (any check blank or failed: do not merge) applied.

### Merge
- Merge preconditions, as facts (no merge was done): the working tree was clean; the head `e151655` of the green GitHub run 44953904172 holds all the code (`e151655` itself is a docs commit; the latest commit that changed code is `fc3a18e`) and no code changed after it (`736b221` and the commit of this job are documents only, `[skip ci]`); `main` (`4a1302d`) is an ancestor of `ios-parity-sync` (28 commits ahead of `main` at `736b221`), so a merge commit would be possible; the last full local host runs (1760 / 1769 tests, MSVC and clang) were made on the code of `e151655`, which has not changed. Not met in job 7 (the eleven device-check results were blank then); met in job 8 (see the entry above). So `main` is untouched at `4a1302d`, the branch `ios-parity-sync` is not merged and no pull request was opened.
- Items covered by the eleven blank checks: COND-STYLE-02 (DEV-03), COND-ELEV-01 (DEV-04 and DEV-10), DLG-FINITE-01 (DEV-05 and DEV-11), LP-24 and LP-22 (DEV-06), COND-PLACE-01 (DEV-07), EXP-12 and EXP-16 (DEV-08), COND-M08 (DEV-09), and the two device-bug fixes of 2026-11-20 that have no item id (DEV-01, DEV-02).

### Owner notes
- Owner note k (the quantity reset; FYI-20 in `PARITY_DIVERGENCES.json`) is closed: decided by the owner on 2026-11-21 (job 7), not ported, recorded as a possible Windows defect (silent data loss). The counting rule now reads: 32 entries = 15 owner notes (a to o) + 10 notes for information + 7 iOS-internal leftovers; 4 are resolved (a, e, g, k), 2 are information only (j and o) and 26 are open (the owner notes b, c, d, f, h, i, l, m and n, the 10 notes for information and the 7 leftovers); in the terms of `PARITY_DIVERGENCES.json` the entries that are still current number 28 (26 open and 2 informational), the 10 other records (7 closed, XCI-01 stale-resolved, XCI-09 and XCI-10 informational) are history, 38 = 28 + 10. The figures 3 resolved / 27 open (and 29 current) are those at the end of job 6.

### Status after the job
- Unchanged in job 7 (history, replaced in job 8: 176 fixed, 23 fixed-ui-unverified, see the entry above): 220 records: 172 fixed, 27 fixed-ui-unverified, 16 wont-fix, 0 blocked-needs-decision, 1 cannot-reproduce, 4 no-longer-divergent (199 ported); dispositions fix 199, wont-fix 16, no-longer-divergent 4, cannot-reproduce 1. No transition in job 7: nothing was recorded as "verified on device".

### Tests and CI
- No code changed, so the test counts (1760 tests, 1769 with Manifold) and the last full local runs (MSVC and clang, on the code of `e151655`) are unchanged. CI: no GitHub run exists for the documents-only commit of job 7 (`[skip ci]`); the last CI-verified code state is `e151655` (run 44953904172, success).

### Not verified / still open
- History, job 7 (superseded by the entry "2026-11-21 - Job 8 step 0" above): all eleven device checks DEV-01 to DEV-11 were open: the owner's table was blank, so nothing was known to have run on a device or a simulator.
- The 26 open entries (the owner notes b, c, d, f, h, i, l, m and n, the 10 notes for information and the 7 leftovers); of the owner notes only b (LP-24), c and n (add the 105 optional datetime fixture rows?) ask the owner something.
- History, job 7: the merge of `ios-parity-sync` into `main` (waited for the device checks; `main` stayed at `4a1302d`, no pull request existed); in job 8 the merge was requested by the owner and is recorded after it happens.
- The possible Windows defects listed for the Windows side (including the quantity reset and the stray quote), and the stale citations of the four comment files (left on purpose).

### Documents
- Updated in this job: `PORTING_BLOCKERS.md`, `PARITY_MAP.md`, `PARITY_DIVERGENCES.json`, `PORTING_STATUS.md` and `.json`, this log, `PORTING_PLAN.md`, `PORTING_MAP.md` and `BUILD.md`; `Config/Tests/README.md` was checked and needed no change (it restates none of the affected facts). The older entries of this log were left as history; the job 6 entry carries superseded notes.

---

## 2026-11-21 - Job 6: re-baseline to Windows 49a2d59, iOS aligned with three Windows fixes

The owner's instruction: "Follow the latest Windows fixes." Branch `ios-parity-sync`, never `main`, no force-push, the Windows repository read-only, "Failing test first for each code change; beyond those, build and host-test fixes only." Step 0 re-baselined against the changed Windows repository ("Treat the current Windows code as the truth"), step 1 verified three Windows fixes and changed iOS only where it differed (each change test first, mutation-checked by its fixer, read by five independent reviewers, and mutation-checked again on the final tree by three independent testers), step 2 updated the documents. The changes were uncommitted in the working tree when this entry was written; they were committed afterwards (five commits: `8773287`, `8ef1251`, `bb58e0f`, `fc3a18e`, `e151655`) and pushed together (`eae0894..e151655`), and GitHub run 44953904172 on `e151655` succeeded (see "Verified on GitHub" below); the last CI-verified code state is `e151655`. The Windows repository was only read. The iOS HEAD at the start of the job was `eae0894` (the docs-only commit, `[skip ci]`, that recorded run 44856186199).

### Step 0: Windows b9902e6 -> bf6b8fc -> 49a2d59
- Windows HEAD is `49a2d59` ("1.3.3 - 2026-11-21", 2026-11-21 14:27 -0500), working tree clean. The owner's text called the third change "a new commit with the three items"; when the job started it was only an uncommitted working tree on `bf6b8fc` (the plans cite it so) and it was committed while the job ran: `edit_condition_dialog.py` is byte-identical to the working tree the plans used (sha256 starts with `dcd286916008adec`) and `elevation_text.py` is unchanged since `b9902e6` (`1839c0c2284b8229`). The documents cite it as `49a2d59`; `bf6b8fc` holds only items without an iOS counterpart and the datetime rule move.
- `bf6b8fc` (2026-11-20 14:36 -0500, "fix: cover sheet save, unreachable PDF shares, folder edits after Reload, compact 3D controls"): 39 files, 5933 insertions, 2223 deletions (19 production files, 16 test files, 2 documents, 2 ledgers): the cover sheet save, unreachable PDF shares (new `application/utils/path_probe.py`), folder edits after Reload (SQL collaboration), compact 3D controls, tower-crane docstring and dead-import removals, and the shared datetime and decimal text rules (new `domain/services/ost_text_values.py`, used by the serializer, the queued-save payloads and the Access importer). **No iOS counterpart** for the cover sheet, SQL collaboration, the path probe, the 3D controls (section cuts, edges toggle, control strip height) and the tower crane (searched in `Controllers`, `Core`, `Services`, `PlanView` and `Rendering`; the only hits are the `CoverSheetSelItemType` / `CoverSheetSelItemUID` reference columns that the import keeps, comments that say ODBC and the UNC handling of CAB member names, none of them a counterpart). Two outcome-level counterparts behave like Windows and need no change: an unreachable PDF is an absent PDF on iOS (`fileExistsAtPath:` in `PlanView.mm:211` and `CGPDFDocumentCreateWithURL` in `PDFCanvasView.mm:22` never throw, and no message bar exists) and the Cover Sheet bid date is stored as opaque text and never parsed.
- `49a2d59`: 20 files, 1634 insertions, 277 deletions: the three behaviour changes below (`edit_condition_dialog.py`; `condition_operations.py` with `update_condition_dto.py` (`MIN_ACCESS_LONG`), `project_write_service.py` and the two handlers for the messages), their five test files, `AGENTS.md` (:559-578) and `CHANGELOG.md` (:71-79), the two ledgers and the version bump 1.3.2 -> 1.3.3 (`README.md`, `installer/ostvisualizer.json`, `application_info.py`).
- **IMP-20 (1), the shared datetime rule (a six-part `Y M D h m s` text is kept, NULL when invalid): already matching.** `parse_ost_datetime` is `ost_visualizer/domain/services/ost_text_values.py:15-22` (it catches AttributeError, ValueError, TypeError and OverflowError) and the importer calls it at `import_operations.py:597` (the b9902e6 line `:608` is gone). The Windows function of `49a2d59` (Python 3.11.4, the version of the fixture header) gives the stored answer on all 3448 rows of `Config/Tests/Fixtures/import_datetime_oracle.txt` (0 differences, re-run in job 6); the plan also compared a Python transcription of the iOS `is_valid_datetime` (read from the C++, not the C++ run) with Windows on all 3448 rows, on the inputs of the new Windows tests and on 152 further edge inputs (0 differences; the C++ itself runs the 3448 rows in `ImportColumnTypes.Imp20_TheConversionMatchesTheWindowsFunctionOnEveryOracleValue`). 105 extra oracle rows were offered and not added to the fixture (the owner: "beyond those, build and host-test fixes only"). No iOS change except comment citations.
- The items of job 5 are untouched by `bf6b8fc` and `49a2d59` except the three items below: of the 38 non-document, non-ledger files of `b9902e6` 8 differ later (`update_condition_dto.py`, `condition_operations.py`, `import_operations.py`, `position_parser.py`, `edit_condition_dialog.py` and three test files) and 30 are byte-identical (compared file by file in job 6).
- Line map (Windows `b9902e6` -> `49a2d59`, each line re-read at `49a2d59`): `edit_condition_dialog.py` placeholder :386-389 -> :389-392, `_type_default_thickness` :1236-1239 -> :1250-1253, the elevation validation :1423-1447 -> :1437-1461 (the new refusal is :1444-1445), type change :1458-1463 -> :1474-1479, condition number :1464-1492 -> :1480-1508, thickness :1559 and :1589 -> :1575 and :1605, spacing :1624 -> :1640; `condition_operations.py` NULL RefNo guard :612 -> :626 (function :595-648 -> :609-662), MAX probe and overflow text :623-641 -> :637-655, `_allocate_condition_identities` :41-58 -> :42-66; `import_operations.py` :608 -> gone; `position_parser.py` :23-33 -> :21-31; `update_condition_dto.py` :5-9 -> :5-10 (`MAX_CONDITION_NUMBER` stays at :5, `MIN_ACCESS_LONG` is new at :6, `ConditionNumberOverflowError` :8 -> :9 and its docstring :9 -> :10).

### What changed in iOS and what already matched
| Item | Outcome | iOS change or pin |
|---|---|---|
| COND-ELEV-01 (the elevation dialog rule, "the 5'6 fix"; no new item id) | changed | Windows `edit_condition_dialog.py:72` (`_TEXT_AFTER_FOOT_MARK`), `:1437-1461`, refusal `:1444-1445`: after the shared parser accepts the text, the dialog refuses it when there is no `"` anywhere in it, a letter or digit follows a foot mark (' or U+2032) with or without a space, and the rest is not a lone x or X. iOS: new `ost_domain::elevation_text_needs_closing_inch_mark` (`ConditionFormRules.cpp:69`), called by `parse_elevation_z` (`:62`); same alert ("Invalid Value", `"<text>" is not a valid elevation. <hint>`); refused `5'6`, `5'6x`, `10'6`, `5'abc`, `-2'3`, `5<U+2032>6` (imperial and metric); accepted `5'x`, `5'X` (60), `-4'x` (-48), `5' 6"`, `5'6"` (66), `10'6 1/2"` (126.5); the loader and the Core parser are unchanged (a stored "Footing @T 5'6" still reads 60, top); no Objective-C change |
| COND-M08 (a stored condition type outside 0-3; decided, REPLACES "show Linear") | changed, status fixed -> fixed-ui-unverified | Windows `edit_condition_dialog.py` `_STYLE_ITEMS` :64-69, `_KNOWN_STYLE_TYPES` :70, `_load_condition` :839-847, `_sync_unknown_style_item` :970-978 and six tests (`EditConditionUnknownTypeTests`, `test_edit_condition_dialog.py:1432`): no crash, a disabled "Unknown (N)" entry selected in the style choice (after Linear / Area / Count / Attachment, removed when another condition loads), Area-like layout, saving without choosing a type keeps the stored type (a notes-only edit writes only the notes), choosing a real type saves that type with its defaults, with takeoffs the style choice stays disabled. iOS: Core `unknown_style_type`, `condition_style_choices`, `condition_style_label`, `editor_layout` (`ConditionChoices`), `condition_form_for_edit` (`ConditionStyleChange`); `editor_condition_type` and `styleLabels()` removed; Objective-C: `_unknownStyleType` set in `_normalizeLoadedCondition` (both load points), rows and parsing branch on `editor_layout`, the style sheet adds a disabled `UIAlertAction` "Unknown (N)", the Style row label comes from Core; tested with the types 7 and -1 in Core and on a SQLite row (the form of the loaded condition, a notes-only edit, the change set, the stored row; the screen itself never ran): only the notes are written |
| DLG-FINITE-01 (the allocator guard for NEW condition numbers; no new item id) | changed | Windows `condition_operations.py` `_allocate_condition_identities` :42-66 (guard and message :56-62): refuses before any write when the highest number of the bid plus the count of new conditions would pass 2147483647 (`Cannot add N condition(s): the new condition numbers would reach X, past 2147483647, the highest condition number.`, X = highest + N); `renumber_conditions` keeps temporaries above the highest number when they fit, else below both the lowest number and 1, refuses only below -2147483648. iOS: the only allocator is `ost_store::duplicate_condition` (it computed MAX(RefNo) + 1 with `column_int`: at 2147483647 the copy wrapped to -2147483648); Core `condition_numbers_overflow_message`, `refuse_new_condition_numbers`, `condition_duplicate_failed_issue`; the store reads MAX(RefNo) as 64 bit inside its transaction after the ownership check (`ProjectStore+Conditions.cpp:467-494`), refuses before any write (no row, no UID consumed) and returns the text through a new optional `std::string *error`; `ImportService duplicateCondition:inBid:error:` and `ViewController+Conditions.mm` show it in the existing "Duplicate Error" alert (title unchanged: iOS DBP-11 wording; Windows titles it "Duplicate Conditions"; decided by the owner 2026-11-21, job 7: the title stays); highest 2147483646 + 1 stores 2147483647, 2147483647 + 1 is refused with X = 2147483648, 2147483646 + 2 is refused (unreachable through the iOS UI, which duplicates one condition per request; pinned in Core and by sequential store calls, decided by the owner 2026-11-21, job 7); every row unchanged after a refusal |
| IMP-20 (1) | already matched | the shared datetime rule gives the stored answer on all 3448 fixture rows; comment citations updated, no test change |

Other iOS paths that allocate or shift condition numbers (the owner asked for the list): `update_condition` -> `shift_conflicting_ref_nos` and its in-memory twins in `ConditionChanges.hpp` and `ProjectData.hpp` (the user types the number; guarded in job 5), `parse_condition_number` (user input, capped at 2147483647), `import_insert_raw_row` (copies the stored RefNo of a `.ost` / `.osp`, allocates nothing), `shift_condition_ref_nos` (no production caller, no guard, creates no new number: left unchanged). iOS has no New Condition, no paste or copy to another bid, no Duplicate-and-Reassign, no multi-condition duplicate and no Renumber Conditions, so the temporaries rule has no iOS counterpart. The code comments call the allocator guard COND-ALLOC-01: a label, not a parity item id.

### Possible Windows defects found while porting (port as is; the lists are in `PORTING_STATUS.md`, "Possible Windows defects", "New in job 6")
- Foot-mark rule (7): one ASCII `"` anywhere switches the rule off (`5'x"`, `5'6x"`, `5'abc"`, `5'x1/2"` save as 60, `10'6.3"` as 120, `6"5'7` as 60); the class is ASCII only (`5'<e acute>`, `5'_`, U+0663, U+FF16, U+017F, U+212A, CJK are accepted as 60); U+201D and U+2033 do not count as an inch mark; the lone-x exemption needs the x to be last and is decided per foot mark; the `\s*` is unreachable for a first foot mark; pre-existing: only ValueError is caught (a feet run beyond the float range is an uncaught OverflowError in the Qt slot, iOS refuses it with the normal message); consequence for existing data: a stored name such as "Footing @T 5'6" opens with `5'6` in the field and every dirty save of that condition is refused until the elevation is fixed (Windows validates only a dirty form, `_apply_changes` :1760-1765; iOS the same). **Updated 2026-11-21 (job 7):** the first of the seven (the stray `"`) is confirmed by the owner as a possible Windows defect, ported as is.
- COND-M08 (5): not ported, decided by the owner 2026-11-21 (job 7): Windows resets a stored quantity that the option list of the type does not offer to "(none)" on every save (:897-898, :984-990, :1658-1664; for an unknown type the list is the Count list, so a type-7 condition with calc 11 / uom 5 saves `{'calc_type1': 0, 'uom1': -1, 'notes': ...}` on Windows while iOS writes only the notes; a possible Windows defect, classified by the owner on 2026-11-21 (job 7) as silent data loss, it also hits unknown types; job 6 called it a "wrong-number risk"; executed headless); Windows keeps the disabled "Unknown (N)" item after a Save (ported as is); picking a real style carries nothing and writes the Spacing (generic rule); a notes-only edit of an Area-like condition with a negative stored rise or run writes abs() (`:1608`, `:1620`; the `.mm` half is not host-testable); the tower-crane note warnings are not ported.
- Allocator (4): PWD-1 (low) `renumber_conditions` raises the overflow error inside a `try` whose `except Exception` re-raises only when `_record_caught_mutation_error` is true (`condition_operations.py:802-808`; on the Access path that is a pyodbc.Error inside a transaction only, `infrastructure/mdb/mdb_writer.py:216-219`, while the SQL-Server writer returns true in every case, `infrastructure/sql/writer.py:1199-1203`, and so re-raises: that path was not checked further), so on the Access path the handler shows the generic "Failed to renumber conditions" and the new text is never shown (no iOS counterpart); PWD-2 only the Access path carries `user_message` (the SQL-Server paths were not checked); PWD-3 the docstring `update_condition_dto.py:10` still says "A renumber would shift ..."; PWD-4 a plain failed Duplicate without a message shows nothing on Windows, iOS keeps its alert.
- The job-5 owner note a is overtaken for `5'6` (Windows fixed it); the 18 records of job 5 keep their classification (the files behind them are byte-identical at `49a2d59` except `import_operations.py`, `condition_operations.py` and `edit_condition_dialog.py`).

### Decisions superseded
- COND-M08: the decision of 2026-11-16 (option a, an unsupported stored type shows Linear like Windows; a Save rewrites the type) and the job-5 note "the Windows result for the thickness is unverified" are replaced by the decision above (the owner's instruction: follow the Windows fix). Removed: `editor_condition_type`, `styleLabels()` and the tests `CondM08_UnsupportedType.TheEditorShowsLinear`, `CondM08_UnsupportedType.ASaveRewritesTheTypeLikeAStyleChange` and `CondM08_UnsupportedType.ASaveRewritesTheTypeAndKeepsTheSpacingInAMetricBid`.
- Job-5 owner notes: a (Windows accepts `5'6` as 60 inches) resolved for `5'6` (`5'x` stays accepted), e (COND-M08) resolved, g (`duplicate_condition` at the limit, "Windows is unguarded too") resolved for `duplicate_condition` (`shift_condition_ref_nos` stays unguarded and unused).
- Old statements corrected: "Windows accepts `5'6` and `10'6`", "COND-M08 is unverified on Windows", "`duplicate_condition` still computes `COALESCE(MAX(RefNo),0)+1` and reads it with `column_int` ... Windows is unguarded too".

### Status after the job
- 220 records: 172 fixed, 27 fixed-ui-unverified, 16 wont-fix, 0 blocked-needs-decision, 1 cannot-reproduce, 4 no-longer-divergent; 0 open (199 ported). Before: 173 / 26 / 16 / 0 / 1 / 4. Move: COND-M08 fixed -> fixed-ui-unverified (new Objective-C wiring that never ran; Core host-tested). COND-ELEV-01 and DLG-FINITE-01 stay fixed-ui-unverified. Dispositions unchanged (199 fix, 16 wont-fix, 4 no-longer-divergent, 1 cannot-reproduce). No new item ids. The outcome of the 44 decisions of 2026-11-16 is now 24 fixed, 6 fixed-ui-unverified, 13 wont-fix, 1 no-longer-divergent.

### Tests
- Host tests: 1720 -> 1760 (1729 -> 1769 with `-DOST_TEST_BOOLEAN=ON`): +40 (+40). Per area (before -> now): Conditions 211 -> 251; unchanged Browser 67, Export 108 (114 with Manifold), Formatting 74, Geometry 173, Harness 10, Import 307, Meshing 107 (110 with Manifold), Parsers 120, Persistence 308, Quantities 134, Settings 101. 1760 = 1750 behaviour tests + 10 harness self-tests; counted from the `TEST(` macros (1770 including the 10 Manifold-only tests; with Manifold one Meshing test is skipped). All pass on MSVC and on clang with FMA, with and without Manifold: first in the runs of the combined tree before the independent review, and again in the final full runs of the finished tree (see "Final full host runs" below; the numbers are the same).
- New: `Conditions/test_condition_elevation_foot_mark.cpp` (suite `CondElev02_ClosingInchMark`, 14 tests, one 189-row table computed with the real Windows regex and `elevation_text_inches`); `CondAlloc01_NewNumbers` (3) and `CondAlloc01_DuplicateFailed` (1) in `test_condition_form.cpp`, `CondAlloc01_DuplicateLimit` (10) in `test_condition_duplicate.cpp`; `CondM08_UnknownStyle` (15: 5 in `test_condition_choices.cpp`, 8 in `test_style_change.cpp`, 2 in `test_condition_update.cpp` on a real SQLite row where the Type column stays the integer 7 / -1); `snapshot_all` in `ConditionsTestSupport.hpp` (every row of every table, used to prove that a refusal changes nothing).
- Removed: the three `CondM08_UnsupportedType.*` tests. Changed, name kept: `CondElev01_Validation.AnAcceptedElevationReloadsToTheValueTheDialogSaved` (row `5'6` removed, 44 -> 40 checked rows) and `CondElev01_Elevation.TheTextGivesZAndNanIsRefused` (`10'6` is refused now, `10'6"` gives 126.0); comment-only edits in `test_condition_elevation.cpp` (the dialog accepts 90 of the 261 names, was 93) and `Parsers/test_elevation_text_value.cpp` (the shared parser still gives `5'6` = 60). No test was renamed.
- Red runs (test first): elevation rule `-Filter CondElev02` against the unchanged code: 12 tests, 4 passed (pins) and 8 failed (every failing check was a "refused?" check, no "accepted?" check failed), then the stubbed free function 1 test 0 passed, then 13 of 13; allocator: 5 tests 0 passed against the unchanged store (a duplicate at 2147483647 returned "21" and stored RefNo -2147483648), then 14 tests 8 passed 6 failed against a signature-only stub; COND-M08: Conditions with the old behaviour behind the new API, 253 tests 242 passed 11 failed. Green: Conditions 251 of 251 on MSVC and clang, Persistence 308 of 308.
- Oracle data: the foot-mark table from the real Windows regex and `elevation_text_inches`; the COND-M08 field sets from the real Windows dialog run headless (types 7 and -1, imperial and metric, plain and rich rows); the allocator expectations from the real Windows function run in memory with a stub cursor. `check_objc` (hand-written stubs, not the real SDK): 27 of 27 files, 0 errors, 4 stub warnings (unchanged); the compile against the real SDK came afterwards on CI (see "Verified on GitHub" below). The line-ending check is clean and the Windows repository status stayed empty.
- Final full host runs on the finished tree (after the review fix-ups, the citation edits and the mutation gap closures below): MSVC 1760 of 1760; MSVC with Manifold (`-DOST_TEST_BOOLEAN=ON`) 1769 of 1769; zig clang (FMA, `-ffp-contract=on`) 1760 of 1760; clang with Manifold 1769 of 1769. The per-area counts are the ones above, unchanged (Conditions 251). The Objective-C syntax check against the hand-written stubs: files=27 ok=27 failed=0 errors=0 warnings=4 (the unchanged baseline). The line-ending check printed OK 0 and the Windows repository status was empty at `49a2d59`. The earlier runs of the combined tree before the independent review gave the same numbers and stay true as the record of that moment (between them the fix-up pass ran only Conditions, 251 of 251 on MSVC and clang, and Parsers 120 and Formatting 74 on MSVC).

### Independent review and mutation checks
- Five independent read-only reviewers: the elevation rule (Core and tests), the allocator (Core, store, tests), COND-M08 Core, COND-M08 Objective-C wiring (read as compiler and crash hunter), requirements coverage and tree hygiene. 0 behaviour defects. Findings, all fixed as tests or comments: two overflow rows of the new elevation table that could not detect a lost overflow refusal (the rule refused them first), stale Windows line citations (`b9902e6` numbers next to `49a2d59` ones), comments that cited scratch files, an over-claiming COND-M08 characterisation test (`LinearOnlyFieldsThatEqualTheStoredOnesAreNotWritten` asserts a field set Windows does not produce in an unreachable state: kept as an iOS characterisation), the SQLite notes-only test lacking the metric row. Findings not changed: `shift_condition_ref_nos` is public and unguarded with no production caller (info); the iPad popover risk and the grey detail text of a reused Style cell (pre-existing, device only); the quantity reset of Windows (owner decision; decided by the owner 2026-11-21, job 7: not ported, a possible Windows defect, silent data loss).
- Reviewer re-derivations: a line-by-line Python translation of the C++ foot-mark loop against the verbatim Windows regex on 14,900,788 strings of length 0 to 5 plus 3,000,000 random strings of length 3 to 14 (0 differences; the compiled C++ did not run on that corpus) and on all 189 table rows (0 mismatches; the host test runs them through the compiled C++); the character tables over all 1,114,112 code points; a Python model of `refuse_new_condition_numbers` against the real Windows function over 15 values of the highest number (11 listed entries, one of them the range 2147483644 to 2147483648) and 4 counts (0 differences in fits, X and stored numbers); the Windows dialog run headless for the types 7, -1, 4, 99, INT_MAX and INT_MIN (the pick sets equal the C++ expected sets except the iOS-only Connect field).
- Mutation checks by the three fixers (temporary edits of the real tree restored byte-exact with a sha256 check, or scratch copies): elevation rule 26 mutants of `ConditionFormRules.cpp` (M01 to M26), 25 killed and 1 equivalent (the rule fed the unstripped text: identical on 579,195 short texts over a 14-character alphabet; the mutant "class without A-Z" survived the first 13 tests and a 14th test closed it); allocator 9 mutants run and killed (`<=` against `<`, singular and plural swapped, guard dropped, `column_int64` against `column_int`, first row instead of MAX, column check dropped, error not cleared, error not set, guard before the ownership check) and 1 equivalent by reasoning, not run (`bind_int64` against `bind_int`: the stored number never passes INT_MAX once the guard passed); COND-M08 14 mutants of Core, all killed in the end (two survived the first tests and were killed after `backout = true` was stored in one test and `EditingTheAreaControlsOfAnUnknownTypeSavesLikeAnArea` was written). 49 mutants run, 48 killed, 1 equivalent. The fix-up pass after the review added 4 mutants in a scratch copy (restored and verified): removing both overflow guards of the elevation parse (`feet_digits_to_inches` refusing infinity and `parse_elevation_z` refusing a non-finite value) is killed by 3 tests, among them the reworked rows of `CondElev02_ClosingInchMark.ForeignMarksAndMarksWithoutReadableFeetStayRefused`; each guard alone is hidden by the other (the first alone survives, 251 of 251 green; the second alone is killed only by `CondElev01_Validation.AnInfiniteElevationIsRefused`), so the rows pin the combined refusal; a metric-only error in `condition_changes_for_save` is killed by the new metric iteration of `CondM08_UnknownStyle.ANotesOnlySaveWritesOnlyTheNotesColumn` and by 3 Core tests (with the old imperial-only shape the SQLite test would not have seen it): 3 killed, 1 survived because of the redundancy. The independent mutation re-check of the final tree and the final full host runs, made after the review's fix-up pass, are recorded in the next two bullets and under "Tests" above (the fix-up pass itself ran Conditions on MSVC and clang, 251 of 251, and Parsers 120 of 120 and Formatting 74 of 74 on MSVC; it added no test).
- Independent mutation re-check of the final tree, made after the review's fix-up pass: three independent testers mutated the final production code in scratch copies (byte-write mutate and restore; the Conditions area, filtered or full; nothing in the repository was touched): 100 mutants including 2 controls. The fixers' own lists above (26 + 9 + 14 = 49 mutants) are not part of the 100. Elevation rule: 41 mutants, 37 killed, 4 equivalent (R20 the rule fed the unstripped text, N23 the order of the two refusals in the same `||` chain, N32 the exemption's `return false` instead of `continue`, N28 a control that splits the expression into identical branches). Condition-number guard: 30 mutants, 24 killed, 3 judged equivalent (N10 `count == 1` -> `count < 2`, which differs only for a count of zero: judged equivalent by the tester and closed below by a line for a count of zero; N16 the refusal commits instead of rolling back, the transaction is empty at that point; N19 the TypeNull check removed, a NULL reads as 0) and 3 survived (N14 the refusal raised only after the INSERT has run, a real gap; N27 a control that weakens `snapshot_all` and tampers with another table on refusal, which survives by design: it proves that the all-tables snapshot is load-bearing; N29 the error branch of a failing MAX read ignored, not testable). COND-M08: 29 mutants, 27 killed, 1 equivalent (M13 `condition_form_for_edit` not normalising Trim and Curved for an unknown type: the Area-like save path never reads them) and 1 survived (M20 a stored type far outside 0..3 (4, 99, INT_MAX, INT_MIN) treated as a type change on the save path, a real gap). As run: 88 killed, 8 equivalent or judged equivalent and 4 survived, of 100.
- The three real gaps (N14, N10, M20) were missed by the fixers' lists, so they are one more finding of the independent review. They were closed by assertions inside existing tests: no test was added and the totals did not change. N14: a third block "a failing INSERT at the limit" in `CondAlloc01_DuplicateLimit.AnUnknownOrForeignConditionAtTheLimitIsTheGenericFailure` (at the limit, with an INSERT that would fail, the caller still gets the overflow text and the data is unchanged). N10: a count-of-zero line (plural, "Cannot add 0 conditions: ...") in `CondAlloc01_NewNumbers.TheMessageIsTheWindowsTextWithSingularAndPlural`. M20: the stored types 4, 99, INT_MAX and INT_MIN added to the loop of `CondM08_UnknownStyle.ANotesOnlyEditOfAConditionWithManyStoredValuesWritesOnlyTheNotes`. Each closure was confirmed in a fresh scratch copy: the unmutated Conditions area 251 of 251; the mutant N14 fails 1 of the 14 `CondAlloc01` tests, N10 fails 1 of the 14, M20 fails 1 of the 251; the scratch copy was verified identical to the tree afterwards. After the closures 91 of the 100 mutants are killed, 7 are equivalent (R20, N23, N32, N28, N16, N19, M13) and 2 are not killable (N27, the control that survives by design, and N29, not testable). The Objective-C(++) wiring cannot be mutation-tested on the host: it has never run on a device and compiled only against hand-written stubs on the authoring machine (CI run 44953904172 compiled and linked it against the real SDK afterwards: see "Verified on GitHub").

### Owner notes (details in `PORTING_STATUS.md`, "Still open")
- Resolved: a (for `5'6`), e (COND-M08), g (for `duplicate_condition`). New: the quantity reset of Windows (not ported, owner decision; **Superseded / updated 2026-11-21 (job 7):** CLOSED, decided by the owner: not ported, a possible Windows defect, silent data loss, it also hits unknown types; job 6 called it a "wrong-number risk on save"), stored names such as "Footing @T 5'6" now fail every dirty save until the elevation is fixed (Windows parity), the iPad popover risk of the Edit Condition pickers (pre-existing, device only), the 105 optional datetime fixture rows (offered, not added), and the Windows tree moved while the job ran (the plans were written against the uncommitted working tree on `bf6b8fc` and re-verified against `49a2d59`). Note j (Windows bookkeeping): `AGENTS.md:559-578` and `CHANGELOG.md:71-79` of `49a2d59` state the intended behaviour for the three items; the iOS documents may cite them.

### What the user must still check on a device (DEV-09 to DEV-11, continuing DEV-01 to DEV-08)
- DEV-01 to DEV-08 (the device-bug fixes of 2026-11-20 and job 5): open, never run on a device. DEV-09 to DEV-11 are new and open: **Updated 2026-11-21 (job 7; history): the owner's table of DEV-01 to DEV-11 came back blank; all eleven stayed open (entry "2026-11-21 - Job 7" above).** **Updated 2026-11-21 (job 8): DEV-01 to DEV-11 are all PASS (the owner, the owner's iPhone, the IPA of run 44953904172; entry "2026-11-21 - Job 8 step 0" above).**
- DEV-09 (COND-M08): a bid whose condition has a stored Type outside 0-3 (for example 7, and -1; how the file reaches the device was not checked): Edit Condition opens without a crash; the Style row reads "Unknown (7)" ("Unknown (-1)"); the rows are Area-like (Dimensions Thickness / Rise / Run; Appearance without Shape; the Advanced rows of an Area); tap Style: Linear, Area, Count, Attachment and a greyed, untappable "Unknown (N)" after Attachment (run it on an iPhone: the iPad has no popover anchor); a notes-only Save writes only the notes (compare the Type column and every other column of the row on a desktop copy); picking a real style saves that type with its defaults; Previous / Next from a condition that still has an unknown type (a second one, if the first was changed above) to a known one removes the entry and back to an unknown one shows it; with takeoffs the Style row is greyed and the type is kept; Cancel right after opening does not ask "Unsaved Changes".
- DEV-10 (COND-ELEV-01): `5'6`, `5'6x`, `10'6`, `5'abc`, `-2'3` are refused on Save with `"<text>" is not a valid elevation.` plus the hint of the bid (imperial and metric); `5'x`, `5'X`, `-4'x`, `5' 6"`, `5'6"`, `10'6 1/2"` are accepted; a condition whose stored name ends in a feet-digit suffix such as "Footing @T 5'6" opens with `5'6` in the field, reads 60 in the 3D view (top) and any other edit of it is refused on Save until the elevation is fixed (Windows parity).
- DEV-11 (DLG-FINITE-01, duplicate overflow alert): in a bid with at least two conditions, set the Cond. No. of one condition (A) to 2147483647 and Save (accepted) and note the number of conditions; long-press any condition of that bid > Duplicate: the "Duplicate Error" alert reads `Cannot add 1 condition: the new condition numbers would reach 2147483648, past 2147483647, the highest condition number.` in full; tap OK: nothing was added, no new condition is selected, mode and placement are unchanged, and reopening the bid shows the same rows; set it to 2147483646 and Save, Duplicate: the copy appears with Cond. No. 2147483647 (not a negative number), Duplicate again: the same alert, no second copy; in another bid Duplicate works without an alert.

### Verified on GitHub (run 44953904172, success)
- The five commits of this job on branch `ios-parity-sync` (never `main`, no force-push), pushed together as `eae0894..e151655`: `8773287` fix(conditions): the Edit Condition dialog refuses a foot mark followed by a digit or letter (the elevation rule, with its 14 + 2 tests); `8ef1251` fix(conditions): refuse a new condition number past 2147483647 when duplicating (the allocator guard); `bb58e0f` fix(conditions): COND-M08, a stored condition type outside 0-3 is kept (disabled "Unknown (N)" entry); `fc3a18e` docs(comments): cite the Windows `49a2d59` line numbers and the moved datetime rule (comment-only); `e151655` docs: job 6, re-baseline to Windows `49a2d59`, COND-M08 decided, the "5'6" rule, the allocator guard (the ten documents). The intermediate trees were verified on their own before the push (exported with `git archive` into scratch copies): `8773287` Conditions 225 of 225 and Parsers 120 of 120; `8ef1251` Conditions 239 of 239 and Persistence 308 of 308; the final tree is the one of the full runs (Conditions 251).
- GitHub Actions "Build IPA" (job `build-ipa`), run 44953904172: event push of branch `ios-parity-sync`, head commit `e151655`, created 2026-11-21 21:53:57 UTC, finished 22:00:42 UTC (6 min 45 s), conclusion success. It passed on the first run; no fix was needed. The last run before it was 44856186199 on `df456a2` (entry "Job 5" below).
- Steps: host unit tests (ctest) 12 of 12 areas passed (the CI build uses the Manifold stub, the configuration of the local 1760-test runs; the Manifold-only tests of the 1769-test runs were local only; the CI step took 87.5 s of test time), libmanifold arm64 check, plist lint, bgfx libraries restored from the cache (the bgfx build steps were skipped), minos check, XcodeGen with the `-Os` and 16.0 readback, `xcodebuild` Release iphoneos unsigned BUILD SUCCEEDED against the real iPhoneOS 18.5 SDK, app bundle check, `ldid` fake-sign, entitlements check, IPA packaging and upload, all green.
- Warnings: the first-party warnings are the same set as in run 44856186199 (9 distinct: 6 `-Wundeclared-selector` (`PlanView.mm:180` and `:186`, `ViewController.mm:311-313`, `EditConditionViewController+TableView.mm:158`), 1 `-Wprotocol` (`EditConditionViewController.mm:62`), 2 `-Wobjc-protocol-method-implementation` (`EditConditionViewController+TableView.mm:44` and `:76`)). Compared by file and message nothing was added and nothing removed; only the three warnings of `EditConditionViewController+TableView.mm` moved, from `:154`, `:43` and `:75` to `:158`, `:44` and `:76`, because this job edited that file, so the Objective-C(++) wiring of this job added no first-party warning. The same 88 distinct third-party warnings (`sqlite3.c` and others), the `Info.plist` notices, the `ld: warning: ignoring duplicate libraries: '-lc++'` notice and the Node.js 20 deprecation annotation; 88 compile units.
- Artifact `OST-Visualizer-ipa`, downloaded and parsed locally with the Python Mach-O and code-signature parser (nothing from it was run): `OST-Visualizer.ipa` 1,970,032 bytes (sha256 starts `2fdd7e2d86a655bb`), bundle id `com.fabian.ostvisualizer`, executable `OST Visualizer` 5,140,192 bytes (sha256 starts `0bbc32e84127220b`; the executable of run 44856186199 had 5,120,016 bytes, sha256 starts `ada7d0fa602ee3c1`), `MinimumOSVersion` 16.0, thin arm64 only, SDK iphoneos18.5, Xcode 1640, ad-hoc `ldid` signature with 1239 code page hashes in each of the two CodeDirectories (SHA-1 and SHA-256), all recomputed and matching, entitlements only `get-task-allowed`, no memory entitlement, no `*.sc` shader sources: all checks passed. The executable contains the new code of this job: the strings "Unknown (" and "the new condition numbers would reach" occur once each (0 in the executable of run 44856186199 and in all earlier ones); the other new texts ("Cannot add ...", ", the highest condition number.", "Failed to duplicate the condition.") share identical literals that existed before, so only those two strings prove the new code by their presence.
- What the run shows: the code of this job compiles and links against the real iPhoneOS SDK and the host tests pass 12 of 12 areas on AppleClang arm64. What it does not show: that the new Objective-C(++) code is correct at runtime (it only shows that it compiles and links with the Core that the host tests cover; the `EditConditionViewController` (+Pickers, +TableView, `_Internal.h`), `EditConditionHelpers`, `ImportService` and `ViewController+Conditions` wiring has never run), anything on a device or a simulator, and the Manifold-only tests (run locally only, on MSVC and clang, all pass). The docs-only commit that records this run carries `[skip ci]` and has no run of its own: the last CI-verified code state is `e151655`.

### Not verified / still open
- The Objective-C(++) wiring of job 6 (`EditConditionViewController` (+Pickers, +TableView, `_Internal.h`), `EditConditionHelpers`, `ImportService`, `ViewController+Conditions`) was syntax-checked against hand-written stubs (27 of 27 files, 0 errors, 4 stub warnings) and compiles and links against the real SDK (run 44953904172, section above), but it has never run (no device, no simulator). Not seen: a disabled `UIAlertAction` greyed and untappable in an action sheet on iOS 16 and later; the current style is not marked inside the sheet (a UI idiom difference from the Windows combo box); row counts against built rows at runtime; cell reuse colours; the long Duplicate text on a phone; the `abs()` of a negative stored rise or run on a notes-only edit of an Area-like layout (the `.mm` parse with `absolute:YES`).
- iPad (pre-existing, device only): the pickers of the Edit Condition screen are action sheets without a popover anchor and the project targets iPad: tapping Style on an iPad would throw (read, not run; run DEV-09 on an iPhone).
- How a type-7 condition is treated by the Conditions list, quantities, 2D and 3D was only read (a reviewer audited every use of the condition type; no crash site found). A Type stored above INT_MAX reads truncated by `sqlite3_column_int` (out of range, not tested).
- Windows facts: the Windows tests cited were read, not run (the real-Access tests need Access); values come from Windows functions run in memory read-only and from a headless run of the real dialog by a reviewer; whether the SQL-Server paths guard the allocator was not checked; older iOS files may hold -2147483648 (a wrapped duplicate) or 2147483648 (an update shift) and the guard does not repair them (Windows has no repair either); the New Condition, paste, copy-to-bid and Renumber Conditions paths have no iOS counterpart and were not run.
- The Manifold-guarded tests ran locally (1769) and not on CI (stub; also not in run 44953904172).

### Documents
- Updated in this job: `PORTING_STATUS.md` and `.json`, this log, `PORTING_BLOCKERS.md`, `PORTING_PLAN.md`, `PORTING_MAP.md`, `PARITY_MAP.md`, `PARITY_DIVERGENCES.json`, `Config/Tests/README.md` and `BUILD.md`. The older entries of this log were left as history; the entry of 2026-11-20 carries a superseded note.

---

## 2026-11-20 - Job 5: re-baseline to Windows b9902e6, iOS aligned with the fixed Windows behaviour

The owner's instruction: "Follow the fixed Windows behavior and answer the last open decisions." Step 0 re-baselined the documents against the changed Windows repository, step 1 verified each Windows fix and changed iOS only where it differed (each change test first, independently reviewed, mutation-checked in scratch copies), step 2 kept COND-FOLDER-01, IMP-20 (2) and (3), EXP-17, EXP-13 and everything already wont-fix, step 3 updated the documents. The changes were uncommitted in the working tree when this entry was written; they were committed afterwards (four commits: `d07abe1`, `31dd319`, `37fb424`, `df456a2`) and pushed, and GitHub run 44856186199 on `df456a2` succeeded (see "Verified on GitHub" below). The Windows repository was only read.

**Superseded / updated 2026-11-21 (job 6):** this entry records what was true at the end of job 5 (Windows `b9902e6`, 220 records with 173 fixed and 26 fixed-ui-unverified, 1720 tests). Job 6 changed these statements: COND-M08 is decided and changed (a stored unsupported type keeps its type and the editor shows a disabled "Unknown (N)" entry; the Windows result is no longer unverified; the three `CondM08_UnsupportedType.*` tests were removed (all three are named in the job 6 entry above; one of them is named below, in the list of renames); status fixed -> fixed-ui-unverified, so the counts are now 172 / 27); the owner notes a (`5'6`), e (COND-M08) and g (`duplicate_condition` at the limit) are resolved; the elevation dialog now refuses `5'6` and `10'6` like Windows `49a2d59`; `duplicate_condition` is guarded. The Windows `file:line` of this entry use the `b9902e6` numbering: the lines of COND-STYLE-02, COND-ELEV-01, DLG-FINITE-01, COND-UPD-02 and IMP-20 (1) moved and are re-cited in the job 6 entry; the lines in the unchanged Windows files still hold.

### Step 0: Windows b9902e6 against 6d4786f
- Windows HEAD is `b9902e6` (2026-11-19 22:23 -0500, "fix: iOS-port defects in condition editing, layer visibility, meshes and import"), exactly one commit after `6d4786f`, the reference of the entries below; working tree clean. `git diff 6d4786f b9902e6`: 42 files, 6109 insertions, 2376 deletions: 18 production `.py` files, 20 test files (17 modified, 3 new), `AGENTS.md` (+42 lines; the bullets at 517-558 state the intended behaviour), `CHANGELOG.md` (+30 lines) and the two quality ledgers (`tests/quality_inventory.json` 600 -> 603 modules, +66 methods; `tests/quality_review.json` bookkeeping only).
- The production files and the iOS items they touch: `update_condition_dto.py`, `update_condition_use_case.py` and `condition_operations.py` (condition number cap, NULL RefNo guard: DLG-FINITE-01, COND-UPD-02); `domain/entities/layer.py`, `project_data_service.py`, `layer_operations.py` and `ui_event_coordinator.py` (shared template rows on Hide All / Show All: LP-24, LP-22); `elevation_text.py` and `position_parser.py` (one elevation parser: COND-ELEV-01); `edit_condition_dialog.py` (COND-STYLE-02, COND-ELEV-01, DLG-FINITE-01); `hierarchy_reader.py` (DBP-05); `import_operations.py` (IMP-20 (1)); `osp_importer.py` (IMP-12); `annotation_operations.py` (IMP-20 (2b), log only); `boolean_operations.py` (EXP-13, EXP-M02); `base_exporter.py` (EXP-12); `obj_exporter.py` (EXP-16); `takeoff_renderer.py` (RPV-27 (b)).
- Behaviour changes outside the items the owner listed: the condition-number cap 2147483647 with exact message strings, the Image-layer pages rule of Hide All, the hint of the "nan" elevation message and the metric placeholder, and the annotation latin-1 warning (log only). `position_parser.py` now delegates to `elevation_text.py`; its decision table is unchanged for every input.
- References that stay at `6d4786f` (history, not bumped): the statements about the tree at `6d4786f` in the older entries of this log, and the code comments "Windows (reference 6d4786f)" in `TakeoffSelection.hpp`, `test_takeoff_selection.cpp`, `test_scene_rendering.cpp`, `test_camera_framing.cpp`, `test_raw_integrity.cpp` and `Fixtures/import_validation_oracle.txt`: the Windows files behind them are not among the 42 changed files, so they stay valid (inferred from the file list). The oracle headers keep `dceb47d` as the origin of the data and now say that it was re-verified against `b9902e6` (see Tests).
- Line numbers: `b9902e6` changed 18 production files; pre-existing line numbers moved in 17 of them (all except `import_operations.py`, where one line changed in place). The Windows `file:line` of this entry were re-read at `b9902e6`; those of the older entries keep the `6d4786f` numbering unless they say otherwise.

### The 18 "possible Windows defects" after b9902e6
- Fixed on Windows in `b9902e6` (12): DBP-05 (`hierarchy_reader.py:126-130`), EXP-12 (`base_exporter.py:15`, `:200`), EXP-16 (`obj_exporter.py:9-10`, `:50`, `:74`), IMP-20 (1) (`import_operations.py:608`), IMP-12 (`osp_importer.py:428-430`), COND-UPD-02 (`condition_operations.py:612`), COND-STYLE-02 (a) and (b) (`edit_condition_dialog.py:1236-1239`, `:1458-1463`, `:1559`, `:1589`, `:1624`), COND-ELEV-01 (`elevation_text.py:131-144`), LP-24 (`layer.py:89-94`, `layer_operations.py:259-371`, `project_data_service.py:665-700`, `ui_event_coordinator.py:6661-6754`), RPV-27 (b) (`takeoff_renderer.py:385-416`), LP-22 (`project_data_service.py:675-700`).
- Changed differently on Windows, decided on iOS (2): EXP-13 and EXP-M02 (`boolean_operations.py:37-111`, helpers :37-67 and `apply_boolean_operations` :70-111: a bounding-box overlap rule, not a `boolean_applied` flag; uncut positives keep the generator winding, cut ones are still reversed; the per-shape winding split of EXP-17 remains). No longer a defect (1): COND-PLACE-01 note 2 (a consequence of the LP-22 fix). Unchanged on Windows and kept (2): COND-FOLDER-01 and EXP-17. Log message only (1): IMP-20 (2b) (`annotation_operations.py:178-186`: one warning naming the encoding and the character, the text is still not saved). 12 + 2 + 1 + 2 + 1 = 18; the 18 records live in 16 items. Still possible Windows defects: EXP-13 / EXP-M02 and EXP-17 (windings of one OBJ), COND-FOLDER-01, IMP-20 (2b), and the new one below.
- New, not among the 18: Windows accepts the elevation `5'6` and `5'x` as 60 inches and drops the rest silently, reads `10'6` as 120 inches and refuses `5' 1/2"` while accepting `1/2"` (`elevation_text.py:7`, `:131-144`). iOS ports it as is (parity).
- Old statements corrected: "Windows flips EVERY positive when the export holds a negative" and "one Windows OBJ mixes both windings" (the b9902e6 rule is above), "Windows accepts a condition number up to 64 bits" (it caps at 2147483647), "Windows would raise on a NULL RefNo", "Windows disagrees with itself on a stored increment of 0", "Windows keeps control characters", "Windows writes the name raw in the OBJ comment", "iOS treats the Image layer like any other layer".

### What changed in iOS and what already matched
| Item | Outcome | iOS change or pin |
|---|---|---|
| COND-STYLE-02 | changed | `condition_to_save` saves Spacing and the Linear / Area thickness as the fields show them, also when equal to the stored value; only the Count / Attachment thickness (100 mm and 0 metric, 4.0 and 0.0 imperial) and the backout come from the table; new `condition_changes_for_save` marks Spacing, Thickness and Backout on every type change (literal write parity, observable only for a stored NULL column). The 2026-11-16 decision (a) and the 2026-11-19 metric deviation are replaced. COND-M08 (a stored unsupported type) is unverified on Windows |
| COND-ELEV-01 | changed | new `elevation_text_inches` (Value / NotAnElevation / Raises); `extract_z_value_from_name` delegates to it; `parse_elevation_z` refuses a bare number, mm, mixed units, `5'` alone and a zero denominator with Windows' message and hint; metric placeholder `e.g. 3.2 m` |
| DLG-FINITE-01 (condition number) | changed | cap 2147483647 with Windows' range message; `update_condition` probes `MAX(RefNo)` and rolls the whole save back on an overflow, returning Windows' text through an optional `std::string *error`; Obj-C alert "Save Failed". Status fixed -> fixed-ui-unverified |
| LP-24 (a) | changed | `is_shared_template_layer`, `plan_bulk_layer_visibility` (NoOwnLayers / AlreadyThere / Write); `ost_store::set_layers_visibility` writes only rows of the bid; memory, sheet and reload agree; `layers_to_change_for_bulk_visibility` removed |
| COND-PLACE-01 note 1 | changed | Image-layer exemption on the single toggle only (like Windows); `is_image_layer_uid`; `ost_data::placement_tap_accepted(ProjectData, uid)` guards the placement sink, the floating button and a new optional delegate query `planView:canPlaceWithCondition:` asked at the start of a tap or pan |
| EXP-12 | changed | `sanitize_material_name` writes one `_` per U+0000-U+001F and U+007F; the oracle table has no control character (0 of 569 rows) and is unchanged |
| COND-UPD-02, LP-22, DBP-05, EXP-16, IMP-12, IMP-20 (1), RPV-27 (b), COND-PLACE-01 note 2 | already matched | pins added (`CondUpd02_NullRefNo.*`, `Lp22_HideAllMemory.*`, `Lp24_HideAllAgreement.*`, `Dbp05_TakeoffIncrementsZero.AStoredZeroTurnsSnappingOff`, `ObjWriter.Exp16_*`, `Imp12_NonConflicting.*`, `ImportColumnTypes.Imp20_*`, `MarkerAnchor.FollowsTheDisplayedShapeOnEveryRotationAndFlip`, `CondPlace01_LayerlessCondition.*`); stale comments corrected; the 176 X rows of `Fixtures/import_datetime_oracle.txt` became N after the current Windows function was run read-only on all 3448 rows |
| Winding (EXP-13, EXP-M02, GEO-15, EXP-17, MSH-21) | no code change | iOS keeps every mesh outward-facing (decided); nothing in iOS consumes or inverts the old flip (no culling, two-sided lighting, writers write faces as stored); the bounding-box pre-check is not ported; pinned by `BooleanRule.Winding_ThePassNeverReversesTheFacesOfAPositive`, `ExportRule.Winding_EveryMeshThatLeavesTheViewAndBothExportsIsOutwardFacing` (Manifold twin `ExportRuleManifold.Winding_...WithManifold`) and `ObjWinding.BooleanDifferenceResultsAreOutwardFacing` |

Decisions of 2026-11-19 overtaken: 8 of the 21 keep records (DBP-04, EXP-12, EXP-16, LP-24, IMP-20 (1), COND-UPD-02, IMP-12, RPV-27 (b); six because Windows now behaves like iOS, EXP-12 and LP-24 because iOS changed), EXP-13 keeps its decision on a changed Windows rule, 12 are unaffected; the 2 deliberate deviations are gone; LP-22 (2026-11-16) is overtaken. The counts 25 / 21 / 4 of 2026-11-19 stay true as such.

### Status after the job
- 220 records: 173 fixed, 26 fixed-ui-unverified, 16 wont-fix, 0 blocked-needs-decision, 1 cannot-reproduce, 4 no-longer-divergent; 0 open (199 ported). Before: 172 / 25 / 21 / 0 / 1 / 1. Moves: EXP-12 and LP-24 wont-fix -> fixed; EXP-16, DBP-04 and LP-22 wont-fix -> no-longer-divergent; DLG-FINITE-01 fixed -> fixed-ui-unverified. Dispositions 199 fix, 16 wont-fix, 4 no-longer-divergent, 1 cannot-reproduce.

### Tests
- Host tests: 1632 -> 1720 (1640 -> 1729 with `-DOST_TEST_BOOLEAN=ON`): +88 (+89). Per area (before -> now): Conditions 174 -> 211, Persistence 278 -> 308, Export 97 -> 108 (114 with Manifold), Parsers 114 -> 120, Import 304 -> 307, Geometry 172 -> 173; unchanged Browser 67, Formatting 74, Harness 10, Meshing 107 (110 with Manifold), Quantities 134, Settings 101. 1720 = 1710 behaviour tests + 10 harness self-tests. All pass on MSVC and on clang, with and without Manifold. The 1632 before the job include the 33 tests of the two device-bug fixes above (Settings +27, Meshing +6) and the first CI fix (Geometry +1); the older entries give 1598 / 1599.
- New test files: `Conditions/test_condition_elevation_dialog.cpp`, `Parsers/test_elevation_text_value.cpp`, `Persistence/test_layer_visibility.cpp`, `Persistence/test_placement_image_layer.cpp` (the placement tests sit in Persistence although the rules live in `ConditionChoices` and `ProjectData`).
- Removed: `ExportText.SanitizeMaterialNameDropsControlCharactersUnlikeWindows` (replaced by `ExportText.Exp12_*`, 4 tests; `ObjWriter.Exp12_*` 2 and `ExportGroups.Exp12_*` 2 are new). Renamed (old -> new): `CondStyle02_Spacing.AStyleChangePersistsTheTypeDefaultSpacingUnlessEdited` -> `AStyleChangeKeepsTheSpacingTheFieldShows`; `AMetricBidPersistsTheMetricTypeDefaultsForEveryStyleChange` -> `AMetricBidSavesTheMetricThicknessAndTheSpacingItShowsForEveryStyleChange`; `AnImperialBidKeepsTheWindowsTableForEveryStyleChange` -> `AnImperialBidSavesTheWindowsThicknessTableAndTheSpacingItShowsForEveryStyleChange`; `CondM08_UnsupportedType.ASaveRewritesTheTypeWithTheMetricDefaultsInAMetricBid` -> `ASaveRewritesTheTypeAndKeepsTheSpacingInAMetricBid`; `CondRef01_ConditionNumber.ANumberBeyondTheStorableRangeIsRefused` -> `ANumberAboveTheHighestConditionNumberIsRefusedWithTheRangeMessage`; `Dbp05_TakeoffIncrementsZero.ZeroKeepsSnappingDisabledUntilTheOwnerDecides` -> `AStoredZeroTurnsSnappingOff`; `Dbp35_SetLayersVisibility.LockedTemplateLayersAreAccepted` -> `SharedTemplateRowsAreSkipped`; `ObjWinding.BooleanResultsStayOutwardFacingWithANegativeInTheExport` -> `BooleanDifferenceResultsAreOutwardFacing` (it only tested `boolean_difference_batch`); `ImportColumnTypes.Imp20_AnIntegerBeyondTheCIntRangeReadsAsNullInsteadOfFailingTheImport` -> `Imp20_AnIntegerBeyondTheCIntRangeReadsAsNull`; `Imp20_AnOverflowingDateReadsAsNullAndTheImportGoesOn` -> `Imp20_AnOutOfRangeDateStoresNullAndTheImportGoesOn`. Removed code: `layers_to_change_for_bulk_visibility`, and the overload `ost_domain::placement_tap_accepted(bool)` (replaced by `ost_data::placement_tap_accepted(ProjectData, uid)`).
- Oracle data re-verified against `b9902e6`: all 569 sanitiser rows of `ExportTextOracleData.hpp` equal the current Windows function (none has a control character) and both Export oracle files regenerate byte-identically from `b9902e6`; `Fixtures/import_datetime_oracle.txt` was regenerated by replacing the 176 X rows by N (header updated, the oracle test simplified).
- `check_objc` (hand-written stubs, not the real SDK): 27 of 27 files, 0 errors, 4 stub warnings (unchanged); the compile against the real SDK came afterwards on CI (see "Verified on GitHub" below). The line-ending check is clean and the Windows repository status stayed empty.

### Independent review and mutation checks
- Four independent reviewers (export, import and geometry pins; the whole tree against `b9902e6`; conditions; layers and placement). 0 blockers, 2 majors, both found and fixed: (1) the winding decision was pinned only at pass level, so a face reversal inside `ExportMeshSet` survived every Export test; `ExportRule.Winding_EveryMeshThatLeavesTheViewAndBothExportsIsOutwardFacing` (and a Manifold twin) now runs the view and both export entry points on three scenes; (2) the floating mode button dead-ended in Select when Place was refused for a hidden condition; the refusal now calls `_leavePlacement`.
- The reviewers reported 22 minors (counted per report, several overlap): stale Windows citations and one stale test comment in code (renumbered / reworded); COND-STYLE-02 was equal by value but not by write (`condition_changes_for_save`); the preview of a hidden armed condition (the delegate query); a tap-guard decision that lived only in Obj-C with an identity predicate in Core (`ost_data::placement_tap_accepted`); test gaps (`CondUpd02_NullRefNo.ANullReferenceNumberIsDifferentFromTheNewNumberZero`, `CondStyle02_FieldWins.ACountOrAttachmentSavesTheTableThicknessWhateverTheFormHolds`, `Dbp35_SetLayersVisibility.ALockedRowThatIsNoTemplateRowIsIneligibleAndRefusesTheBatch`); the oracle header and the RPV-27 page-B comment; a new test file with CRLF (now LF); the documents (this step). Not changed: the single toggle of another bid's template row (owner note b).
- Mutants in the review reports (the first pass, by the independent reviewers, in scratch copies restored by file copy: the restore did not refresh timestamps, so those verdicts were re-checked by the clean re-check in the next bullet): 40 recorded, 35 killed and 5 survived at first. The survivors led to the tests named above (the face reversal in `ExportMeshSet`, the NULL RefNo guard, the Count / Attachment table thickness, the eligibility filter) and one is an equivalent mutant (the `UPDATE` keeps `AND [BidUID] = ?`). After the fix-ups, scratch-copy mutants were killed by the new tests: the three winding mutants (face reversal in `run_boolean_pass`, in a replaced and in an unchanged positive of `BooleanPass.cpp`), the NULL RefNo guard, both Count / Attachment variants, the eligibility filter and four mutants of `placement_tap_accepted`. A mutation re-check of the final tree was run afterwards (next bullet).
- Mutation checks of the final tree: run by workflow agents in scratch copies only (nothing in the repository was touched). The first pass was made by the independent reviewers (scratch copies, restored by file copy: the restore did not refresh timestamps, so those verdicts were re-checked). The clean re-check (byte-write mutate and restore in fresh scratch copies) ran 174 mutants over the three chains. Conditions chain: 64 mutants, 63 killed, 1 equivalent, 0 survived. Layers chain: 50 mutants, 42 killed, 4 equivalent, 4 survived (three redundant-guard gaps in `plan_bulk_layer_visibility`: Comments recognised by the listed name, Comments recognised by the live name, and a listed row without a live layer; and one pre-existing line of `set_layers_visibility`, the template filter `&&` with only one flag column present, which is not part of the alignment and stays unpinned). Export, import and geometry chain: 60 mutants, 53 killed, 4 equivalent, 3 survived (the winding of the boolean-failure fallback, and the page-contract guards of `canvas_marker_anchor`: the scale factors and the output dimensions). Closing action after the re-check: assertions were added to existing tests (no new test names, the counts are unchanged): `BooleanRule.AFailingBooleanOperationReturnsEveryPositiveUnchangedAndLogsAnError` (faces and signed volume), `FootprintOnCanvas.InvalidPageContractThrows` (the marker guards) and `Lp24_BulkPlan.OnlySharedLayersFailAndWriteNothing` (Comments by either name, a listed row without a live layer). Each of the six mutants (the boolean-failure fallback reversing faces, the two marker guards removed, the two Comments checks removed, a listed row without a live layer counted as own) was re-run against the final tests and is detected: five by a failing test, the last one by an aborting debug assertion (that mutant dereferences an end iterator). Not added (accepted): the optional hardening "(90, flip_x only) combination in the marker test" and the pre-existing template-filter line. Equivalent mutants (9 of the 174): `< 0x20` against `<= 0x20` (space is handled by its own case), the `# Display Mode:` MTL header written raw (this equals Windows; the extra `single_line` of iOS there is unobservable with the fixed labels), the size shortcut of `files_have_same_content`, a reversal inside `from_manifold` (undone by `fix_face_winding`), a NULL MAX treated as overflow in the overflow probe (cannot occur: the probe only runs when a row exists), the own-uid filter and the UPDATE's `AND [BidUID] = ?` of `set_layers_visibility` (redundant guards), the layer-less branch of `is_condition_visible` (a layer stored under an empty uid cannot exist) and `<= 0` against `< 0` in `snap_increments_ost`.

### Owner notes (nothing changed for them; details in `PORTING_STATUS.md`, "Still open")
- a: the Windows elevation quirks above (`5'6`, `5'x`, `10'6`); b: a single toggle of ANOTHER bid's locked template row writes the database on both sides (`layer_operations.py:43-55`) but leaves the iOS memory and sheet stale until reload (needs a decision: refuse it, or update the merged list); c: Hide All with only shared layers is silent on iOS (like the Windows sidebar path; a one-line option `_presentLayerVisibilityFailure` for `NoOwnLayers` exists); d: Windows residuals not copied (`layers_dialog.py:615-618`, a layer missing from the merged list, the pages with no Image layer); e: COND-M08 thickness unverified on Windows; f: Windows' create-new-condition flow (`condition_action_handler.py:593-611`) still persists imperial defaults in a metric bid, iOS has no creation path; g: `duplicate_condition` and `shift_condition_ref_nos` at 2147483647; h: the EXP-13 bounding-box pre-check is not ported and `OBJWriter` also routes the `# Display Mode:` header through `single_line`; i: the new Objective-C wiring was syntax-checked against stubs, compiles against the real SDK on CI (run 44856186199) and has never run; j: Windows `AGENTS.md:517-558` and `CHANGELOG.md` now state the intended behaviour. Notes a, b and c ask something of the owner (b is the one open question, LP-24); d to j are information. With the 10 older notes for information and the 7 code-hygiene notes the list holds 27 entries: 26 open and 1 informational (j).

### What the user must still check on a device (DEV-03 to DEV-08, continuing DEV-01 and DEV-02)
- DEV-01 and DEV-02 are the checks of the two device-bug fixes of 2026-11-20 (the entry "Two device bugs" below); their wording is also in `PORTING_BLOCKERS.md` and in the device-check tables of `PORTING_STATUS.md` and `.json`. DEV-01: the 3D view in Transparent mode, and in Original mode with a condition whose pattern is Transparent or a hatch, shows the meshes half see-through (an opaque mesh in front hides one behind it; Solid mode looks as before; mid and dark grey meshes are visible on the dark background; after a page is added to the 3D selection the meshes are still drawn). DEV-02: tap a takeoff of a condition that has several takeoffs and press Delete in View, Select and Place mode: only that takeoff goes, nothing is selected or outlined afterwards; Undo brings it back selected, Redo clears; the Conditions list never loses a row.
- DEV-03: Edit Condition, style change in a metric and an imperial bid (Spacing keeps what the field shows, Linear / Area thickness is the field, Count / Attachment save the table thickness). DEV-04: the elevation field (placeholder `e.g. 3.2 m`; `3.2 m` and `320 cm` accepted; `3200`, `5'` and `10' 6" 2m` refused with the hint). DEV-05: condition number 2147483648 refused, and a renumber past 2147483647 shows the "Save Failed" alert and changes nothing. DEV-06: the Layers sheet after Hide All / Show All (shared rows untouched, the sheet equals a reload, a bid with only shared layers: nothing happens). DEV-07: the Image layer with the single toggle while a condition on it is armed (not suspended, taps and drags do nothing, the floating button leaves Select -> View; Hide All still suspends, Show All restores). DEV-08: OBJ and DXF export of a condition name with a line break (`_` and no injected lines; needs a desktop viewer).

### Verified on GitHub (run 44856186199, success)
- GitHub Actions "Build IPA" (job `build-ipa`), run 44856186199: event push of branch `ios-parity-sync`, head commit `df456a2` (the four commits of this job: `d07abe1`, `31dd319`, `37fb424`, `df456a2`; dated 2026-11-20 21:29 -0500), created 2026-11-21 02:29:37 UTC, finished 02:32:50 UTC (3 min 13 s), conclusion success. It passed on the first run; no fix was needed. The last run before it was 44678523124 on `275cf66` (entry "Two device bugs" below).
- Steps: host unit tests (ctest) 12 of 12 areas passed (the CI build uses the Manifold stub, so the 10 Manifold-only tests of the local count 1729 are not part of it; the step took 2 min 10 s), libmanifold arm64 check, plist lint, bgfx libraries restored from the cache (the bgfx build steps were skipped), minos check, XcodeGen with the `-Os` and 16.0 readback, `xcodebuild` Release iphoneos unsigned BUILD SUCCEEDED against the real iPhoneOS 18.5 SDK (35 s), app bundle check, `ldid` fake-sign, entitlements check, IPA packaging and upload, all green.
- Warnings: the first-party warnings are identical to run 44678523124 (9 distinct: 6 `-Wundeclared-selector` (`PlanView.mm:180` and `:186`, `ViewController.mm:311-313`, `EditConditionViewController+TableView.mm:154`), 1 `-Wprotocol` (`EditConditionViewController.mm:62`), 2 `-Wobjc-protocol-method-implementation` (`EditConditionViewController+TableView.mm:43` and `:75`)), compared as a set with file, line, column and message: nothing added, nothing removed, so the Objective-C(++) wiring of this job added no first-party warning. The same 88 distinct third-party warnings (`sqlite3.c` and others), the two `Info.plist` notices, the `ld: ignoring duplicate libraries '-lc++'` notice and the Node.js 20 deprecation annotation; 88 compile units.
- Artifact `OST-Visualizer-ipa`, downloaded and parsed locally with the Python Mach-O and code-signature parser (nothing from it was run): `OST-Visualizer.ipa` 1,969,963 bytes (sha256 starts `fcacaece7b825c63`), bundle id `com.fabian.ostvisualizer`, executable `OST Visualizer` 5,120,016 bytes (sha256 starts `ada7d0fa602ee3c1`), `MinimumOSVersion` 16.0, arm64 only, SDK iphoneos18.5, ad-hoc `ldid` signature with 1235 code page hashes in each of the two CodeDirectories (SHA-1 and SHA-256), all recomputed and matching, entitlements only `get-task-allowed`, no memory entitlement, no `*.sc` shader sources: all checks passed. The executable differs from that of run 44678523124 and contains the new code of this job: the strings "Condition number must be between 1 and 2147483647.", "e.g. 3.2 m", the two elevation hints and the overflow text "cannot be used: the conditions numbered" occur once each (0 in the earlier executable), the selector `canPlaceWithCondition` three times (0 before).
- What the run shows: the code of this job compiles and links against the real iPhoneOS SDK and the host tests pass 12 of 12 areas on AppleClang arm64. What it does not show: that the new Objective-C(++) code is correct at runtime (it only shows that it compiles and links with the Core that the host tests cover), anything on a device or a simulator, and the 10 Manifold-only tests (run locally only, on MSVC and clang, all pass). The docs-only commit that records this run carries `[skip ci]` and has no run of its own: the last CI-verified code state is `df456a2`.

### Not verified / still open
- The code of this job was seen by GitHub run 44856186199 on `df456a2` (section above). The new Objective-C(++) wiring (`ImportService`, `EditConditionViewController` (+TableView), `ViewController` (+Conditions, +Layers), `PlanView` (+Placement)) was syntax-checked against the stubs and compiles and links against the real SDK (that run), but it has never run (no device, no simulator); the decisions behind it are host-tested in Core. Inferred, not run: a refused placement press is consumed by the place recognizer, so the page does not scroll under it.
- The Windows result of COND-M08 is unknown; the Windows tests cited were read, not run. The Manifold-guarded tests ran locally (1729) and not on CI (stub; also not in run 44856186199). The mutation checks of the final tree are recorded in full (see "Independent review and mutation checks" above): 7 of the 174 mutants survived, six of them were closed by assertions in existing tests and one pre-existing line of `set_layers_visibility` (the template filter) stays unpinned (accepted).

### Documents
- Updated in this job: `PORTING_STATUS.md` and `.json`, this log, `PORTING_BLOCKERS.md`, `PORTING_PLAN.md`, `PORTING_MAP.md`, `PARITY_MAP.md`, `PARITY_DIVERGENCES.json`, `Config/Tests/README.md` and `BUILD.md`. The older entries of this log were left as history; the entry of 2026-11-19 carries a superseded note.

---

## 2026-11-20 - Two device bugs: transparent meshes invisible, Delete in the 2D view

Found on a phone with the green IPA of run 44661853681. Root causes below are from reading the code and running the Core on the host; what only a GPU or the UIKit layer can show is marked inferred.

### Bug 1: the meshes are not visible when the visualization mode is Transparent, or Original with the transparent pattern
- Only meshes with an opacity below 0.99 vanish (Solid mode is fine), so only the transparent passes are involved. The sync changed the default mode from Solid to Original (`RenderConfig::DEFAULT_MODE`, Windows `Config.DEFAULT_DISPLAY_MODE`): every mesh of a condition whose pattern is not Solid or None has opacity 0.5 and takes the weighted-blended OIT passes from the first launch. Before, every mesh was opaque and those passes never ran (no device run of them is on record).
- Cause (inferred, not observed on a device): the OIT framebuffers had a depth texture of their own (`_oitDepthTex`) that nothing cleared (the OIT views clear colour only) and nothing wrote (no `WRITE_Z`), while both passes use `DEPTH_TEST_LESS`. Against undefined contents (typically zeros) every fragment is rejected, the accumulation stays 0 and `fs_oit_composite` discards. Windows shares one depth texture between `accum_fbo` and `opaque_fbo` (renderer.cpp 1803, 1815). The OIT code, the shaders and the framebuffer code are byte-identical to 42b46c3, so this was latent and the new default mode exposed it.
- Second defect: the composite blended its premultiplied output with `SRC_ALPHA`, applying the coverage twice; a mesh that did draw came out dim and mid or dark grey was nearly invisible on the 0x353535 background.
- Third, unit related (found while ruling out suspect 1, MSH-01): the far plane came from the radius of the LAST fit (scene radius 15 and distance 12 at start: far 60), so a raw-unit scene that appears without a refit lay beyond it (the nearest vertices 288 units, the farthest 10123 units from the initial camera).
- Ruled out with numbers: Core produces the scene (27 to 256 meshes per bid in an analysis over pages of ratio 1 to 240 with rotation and flip, real Manifold v3.0.0 on the host dropped none); the fit pose, near/far, the zoom clamp, winding, culling (no pass sets a cull flag), page rotation and flip, the lighting constants and the boolean pass. A page rotation that is not a multiple of 90 makes `generate_view_meshes` throw and the view stays empty silently; it is now logged.
- Fix: `Rendering/Renderer.mm` lets the OIT framebuffers test against the depth of the opaque scene (`_sceneDepthTex`), composites with `ONE / INV_SRC_ALPHA`, draws a transparent mesh with plain alpha blending when the OIT framebuffers are not usable, and takes the clip planes from Core; `Core/Domain/SceneRendering.hpp` holds `mesh_pass_for_alpha`, `camera_eye` and `render_clip_planes` (the far plane reaches the farthest corner of the live bounds; unchanged at a fit pose); `MathUtils.hpp` uses the Core threshold; `ProjectService.mm` logs the exception.
- Tests: `Settings/test_scene_rendering.cpp` (11) and `Meshing/test_scene_in_view.cpp` (6: a house bid on four pages of different scale, rotation and flip through `generate_view_meshes`, the bounds as the renderer computes them, the fit pose, the frustum). Red first against the old rules placed in Core: the far plane did not reach the scene and a transparent mesh had no pass when the OIT framebuffers were unusable; the guard tests (the scene exists, finite bounds, inside the frustum at the fit pose, planes unchanged at the fit pose, the mode never changes which meshes exist) pass on the old code too, so the unit change MSH-01 is not what hides a FITTED scene. The shared depth and the composite blend are GPU state: compiled by the CI, not host-testable.

### Bug 2: Delete in the 2D view selected the other takeoffs of the same condition, or "deleted the condition", depending on the mode circle
- Cause (code read, the .mm cannot run here): after a Delete `ViewController` cleared the selected takeoff but left the highlighted condition set. `PlanView` draws a group border on every takeoff of the highlighted condition while no takeoff is selected, so the siblings looked selected. In Place mode Delete also dropped the place condition and the floating button's Place entry while the placement tool stayed live. The logic dates from the initial commit 1f4390f, it is not a regression of the sync.
- "Deletes the condition": no code path from the Delete button or the mode circle deletes a condition. The paths that can: the Conditions sheet (long press on a row, Delete, confirmation alert, `ImportService deleteCondition:` -> `ost_store::delete_condition`) and the bid and folder deletes of the browser (`delete_project`, `permanently_delete_project`, which remove the conditions of the bid with it). Not reproduced; the owner is asked what exactly was seen.
- Fix: `Core/Domain/TakeoffSelection.hpp` (`select_takeoff`, `clear_selection`, `clear_selection_for_page_switch`, `keep_selection_on_page`, `select_after_history`, `delete_enabled`, `takeoffs_to_delete`) with the Windows rules: Delete removes the selected takeoff and its parent_uid descendants and takes no mode and no condition; afterwards nothing is selected and no condition is highlighted (a running placement keeps its tool); the undo selects the restored takeoff while its page is on screen, the redo clears. `ViewController.mm` and `UndoRedoController.mm` call them; the Delete button follows every selection change; a takeoff picked in the 3D view on another page is dropped when the 2D view opens, so Delete cannot act on a takeoff the user does not see.
- Tests: `Settings/test_takeoff_selection.cpp` (14, the exact failing cases of the report in every mode) and `test_takeoff_delete_safety.cpp` (2: the app's own decisions and the store's delete and insert against a real database: no condition row, no other takeoff and nothing of another bid changes, the undo restores every column but GUID). Red first against the old rule placed in Core: 4 of the 14 failed (the highlight stayed after Delete and Redo, the place offer was dropped in every mode, the undo did not highlight). That red run is against the old rule in Core, not against the .mm.

### Review
Three independent reviewers read both fixes and ran mutations in private directories; no blocker. Fixed because of them: the undo no longer offers Place for the restored takeoff's condition (it would have opened a COND-PLACE-01 route), the tested Delete decision is the one `UndoRedoController` calls, the safety test compares every column, several Windows citations, seven mutants that survived (eye direction, the eight corners, the far-plane margin, the placement keeping its condition, the handle reset, the undo's place offer). All seven are caught now.

### Verified on GitHub
GitHub run 44678523124: event push of branch `ios-parity-sync`, head commit `275cf66`, created 2026-11-20 05:04:01 UTC, finished 05:08:48 UTC, conclusion success. It built the code of `34e14c9` and `aef35d8` (Delete in the 2D view and the transparent meshes: `ViewController.mm`, `UndoRedoController.mm`, `Renderer.mm`, `ProjectService.mm`, `MathUtils.hpp`, `TakeoffSelection.hpp`, `SceneRendering.hpp`) and the docs commit `275cf66`: the host unit tests passed, 12 of 12 areas (ctest; the CI build uses the Manifold stub), `xcodebuild` ended with BUILD SUCCEEDED against the real iPhoneOS SDK with the same first-party warnings as the earlier runs and no new one (the three `-Wundeclared-selector` warnings of `ViewController.mm` moved from `:310-312` to `:311-313` because `34e14c9` added a line above them), and the IPA artifact `OST-Visualizer-ipa` was downloaded and parsed locally (bundle id, minos 16.0, arm64, ad-hoc `ldid` signature with all page hashes recomputed, entitlements only `get-task-allowed`; the executable differs from the earlier runs and contains the device-bug fixes). Verified by GitHub run 44678523124 (success): the code up to `275cf66`. At that time no run had seen the code of job 5 (run 44856186199 has since, see the entry "Job 5" above, "Verified on GitHub"); nothing ran on a device at that time (DEV-01 and DEV-02 then stayed open; both PASS since job 8 on the IPA of run 44953904172, which contains those fixes). The text of this entry above is what was known when it was written; the run verifies the host tests and the compile, not the behaviour on a device.

### Open for the owner (not changed; see PORTING_BLOCKERS.md "Open for the owner after the two device bugs")
The transparent look (Windows draws an opacity of 0.5 almost opaque), the camera refit rule (CAM-02, CAM-03), the portrait crop of the fit, the iOS group state of a condition picked in the list, undo and redo clearing without the Windows page check, the depth range of an unfitted scene.

---

## 2026-11-20 - First full CI runs: the app builds and packages on GitHub, two fix(build) commits

### Runs (GitHub Actions "Build IPA", job `build-ipa`, macos-15-arm64, Xcode 16.4 build 16F6, iPhoneOS 18.5 SDK, Apple clang 17.0.0; times UTC)
- Three runs on the pushed branch `ios-parity-sync`, after the run of 2026-11-16 that had stopped at the host tests (Meshing, FMA).
- 44642251350 on `292c406` (the 7-commit push), 00:00: failed after 1 h 5 min. Host tests: 11 of 12 areas passed, Meshing among them (the 2026-11-16 failure is gone, which fits the `strict_fp.hpp` fix of 2026-11-17); Geometry failed `MarkerAnchor.CountTriangleAndCurvedBand` (x 77.4244 against 72.5756). The host-test step took about 64 minutes (second fix below); xcodegen, xcodebuild, ldid and packaging did not run.
- 44654559214 on `5dc7b2b`, 01:34: success, 57 min 47 s, the first run through every step (host-test step 55 min 20 s; bgfx cache miss, bgfx built in 25 s and saved; xcodebuild 61 s; artifact `OST-Visualizer-ipa`).
- 44661853681 on `787a57d`, 02:35: success, 4 min 8 s (host-test step 168 s with all 12 areas passing, 64 s of it ctest; bgfx cache hit, build skipped, verify steps run; xcodebuild 50 s). The app executable is byte-identical to the previous run's (sha256 `3dce8c45db3ba6b35a9bbb7e0bdba88a59ffb9d1c00468c8963f9a77c023bcdd`).
- A third successful run, 44678523124 on `275cf66` (the later push of the two device-bug fixes), succeeded; it is recorded in the entry "Two device bugs" above ("Verified on GitHub"). A fourth successful run, 44856186199 on `df456a2` (the push of job 5, 2026-11-21 UTC), succeeded; it is recorded in the entry "Job 5" above ("Verified on GitHub"). A fifth successful run, 44953904172 on `e151655` (the push of job 6, 2026-11-21 UTC), succeeded; it is recorded in the entry "Job 6" above ("Verified on GitHub").

### The two fix(build) commits
- `5dc7b2b` marker anchor test accepts either libm-decided tie side (tests and docs only, no app code). Cause, inferred and not observed on arm64: on a mirror-symmetric curved band the two best grid centres of the marker-anchor search are mirror images; in the doubles their squared distances tie bit for bit on the x86-64 builds (MSVC, clang), so the first one wins (Windows `min()`, the C++ strict `<`), and a last-bit change in the `atan2`/`cos`/`sin` arc points breaks the tie; the arm64 run took the mirror one. Evidence: no fused multiply-add is emitted for `takeoff_footprint.cpp` and `Meshing/gen_curved.cpp` (aarch64-macos build with zig clang 22: 0 fused instructions with `strict_fp.hpp`, 20 and 31 without), and one ulp on one of the 53 libm results of the test case flips the answer in 23 of 106 cases, to the CI value. Change: the curved-band assertion accepts the Windows x or its mirror (150 - x); the new libm-free test `MarkerAnchor.AnExactTieKeepsTheFirstCandidateOfTheWindowsOrder` pins the tie rule (`<=` instead of `<` makes it fail). The suite is now 1599 tests (1607 with `-DOST_TEST_BOOLEAN=ON`; Geometry 172), all passing on MSVC and on the clang+FMA build. An independent review found and fixed a real defect in the first version of the assertion (a `std::min` of two temporaries bound to a reference, vacuous under clang `-Os`). Recorded in `PORTING_BLOCKERS.md` ("Not reproducible on iOS"), `Config/Tests/README.md` and `BUILD.md`.
- `787a57d` bound the host-test build to the runner's core count: `cmake --build build/tests --parallel` (bare, an unlimited `make -j` with the Makefile generator) became `--parallel "$(sysctl -n hw.ncpu)"`. The log of run 44654559214 shows about 60 and then about 110 compiler processes started within one second on a runner GitHub documents as 3 cores and about 7 GB, and `test_Harness` linked only 28 minutes later: 55 min 20 s for the step instead of 168 s. Same sources, flags and tests; the app executable is byte-identical before and after.

### What the runs showed (both successful runs, identical unless noted)
- Host tests on AppleClang 17 arm64 (Debug, `-Os`): `100% tests passed out of 12`, the 12 areas. The default CI run uses the Manifold stub, so the 9 tests that need real Manifold are not part of it (1 other test only runs without it; the suite has 8 more tests with the option on) and real Manifold has not run on AppleClang through the host tests.
- Prebuilt libraries: `libmanifold.a` is thin arm64, iOS, minos 16.0 (15 objects); `libbgfxRelease.a`, `libbxRelease.a`, `libbimgRelease.a` and `libbimg_decodeRelease.a` (22, 22, 24 and 1 objects) are arm64, iOS, minos 16.0; the minos check passed. Both plists pass `plutil -lint`. XcodeGen generated the project (one target); `GCC_OPTIMIZATION_LEVEL = s` and `IPHONEOS_DEPLOYMENT_TARGET = 16.0` read back for Debug and Release.
- `xcodebuild` Release, iphoneos, unsigned: BUILD SUCCEEDED, 88 translation units in 4 clang response files (all with `-Os`, none with another `-O` level), one link. First compile of every Objective-C and Objective-C++ file against the real SDK: no error. Warnings in first-party code (none is an error): 6 `-Wundeclared-selector` (`PlanView.mm` x2, `ViewController.mm` x3, `EditConditionViewController+TableView.mm` x1; the methods are defined in category or class files and only not declared where they are used), 1 `-Wprotocol` and 2 `-Wobjc-protocol-method-implementation` around `EditConditionViewController` (the class extension adopts `UITableViewDataSource`, the `+TableView` category implements it), `ld: warning: ignoring duplicate libraries: '-lc++'`, and two `Config/Info.plist` warnings (no open-in-place key; all orientations unless full screen is required). `Config/Info.plist` was not edited in response to the CI runs (the file lists the open-in-place and orientation keys as deliberately absent) and adding keys would change app behaviour: the owner's decision. 88 more warnings are in third-party code (`bgfx.h`, Manifold `linalg.h`, `sqlite3.c`).
- Bundle and IPA: executable `OST Visualizer`, bundle id `com.fabian.ostvisualizer`, MinimumOSVersion 16.0, arm64 only, no `*.sc`; ldid fake-signed from `Config/Entitlements.fakesign.plist` (`get-task-allowed` only; no key matches `memory|jetsam|extended-virtual-addressing`); IPA of 1,963,627 bytes uploaded as artifact `OST-Visualizer-ipa` (kept 30 days).
- Local inspection of the downloaded artifact of both runs (a Python parser, nothing executed): zip integrity, one app, Info.plist keys expanded from the build variables, thin arm64 Mach-O iOS minos 16.0, ad hoc signature (SHA-1 and SHA-256 code directories, all 1233 page hashes recomputed and equal, entitlements `get-task-allowed` only, no CMS signature). `-Os` cannot be read from a binary: it rests on the CI-logged build settings and the 4 response files.
- Workflow: `concurrency` per ref with `cancel-in-progress` (a newer push cancels a run in progress), `on: push` and `workflow_dispatch`, `contents: read`, 90 min timeout; every run warns that Node.js 20 is deprecated (checkout@v4, cache@v4 and upload-artifact@v4 are forced to Node.js 24; a warning, not a failure).

### Not verified / still open
- Never run: install and launch of the fake-signed IPA (only structure and hashes were checked, not that the owner's installer accepts the signature; iRAM+ memory entitlements are applied by the owner afterwards); anything at runtime (crashes, Metal/bgfx rendering and shaders on a GPU, picking, frame time and memory, large projects); the 25 fixed-ui-unverified device checks; `.ost`/`.osp` open-in and the document picker; real `.ost`/`.osp` project files (the repo holds only synthetic oracles and two CAB files).
- Only Release was compiled (for Debug only the settings were read back); no iOS Simulator slice was built. Real Manifold is not exercised on AppleClang by the host tests (stub) nor on a device.
- The libm cause of the marker-anchor tie is inferred, not observed on arm64. On a device the marker of an exactly mirror-symmetric curved band may sit one grid cell (4.85 points at the test scale) on the other side of its axis than on Windows; the owner may want to look at it.
- The runner size (3 cores, about 7 GB) is documented by GitHub for this standard runner size, not measured: the job log does not print it.
- Test counts: the entries of 2026-11-19 and older give 1598 (1606 with Manifold), the snapshot before the first CI fix; the suite has 1599 (1607) since. Not changed by this entry: the decisions, the parity statuses (172 fixed, 25 fixed-ui-unverified, 21 wont-fix, 1 cannot-reproduce, 1 no-longer-divergent) and the still undecided items (condition-number range of DLG-FINITE-01, the two COND-PLACE-01 notes).

### Documents
- The CI results were recorded in `PORTING_BLOCKERS.md` (B-010, B-011 and "Not reproducible on iOS"), `BUILD.md`, `Config/Tests/README.md`, `PORTING_MAP.md` and `PORTING_PLAN.md`; this entry is new and the older entries were left as history, with a superseded note on the 2026-11-19 entry.

---

## 2026-11-19 - Open blockers closed by user decisions, four code changes, possible Windows defects

> Superseded in part on 2026-11-20 (entry above): the statements below, and in the older entries, that no GitHub run exists for the current code, that the `.mm` files were never compiled against the real SDK, that nothing was committed and that the suite has 1598 tests (1606 with Manifold) are as of the date of each entry. The CI runs of 2026-11-20 compiled and packaged the app, the work is committed on `ios-parity-sync` (HEAD `787a57d` on 2026-11-20 when that was written; `275cf66` before job 5; `df456a2` with the code of job 5, run 44856186199; `e151655` with the code of job 6, run 44953904172), and the first CI fix made the suite 1599 (1607).
>
> Superseded / updated 2026-11-20 (job 5, the entry "2026-11-20 - Job 5" above): Windows `b9902e6` fixed 12 of the 18 "possible Windows defects" named below and the owner asked to follow the fixed Windows behaviour. Overtaken: 8 of the 21 keep decisions (DBP-04, EXP-12, EXP-16, LP-24, IMP-20 (1), COND-UPD-02, IMP-12, RPV-27 (b)), the two deliberate deviations (COND-STYLE-02, COND-ELEV-01), the open condition-number question of DLG-FINITE-01 (Windows caps at 2147483647) and the counts 172 / 25 / 21 / 1 / 1 (now 173 / 26 / 16 / 1 / 4) and 1598 / 1599 tests (now 1720). The text of this entry is left as it was.

### Decisions recorded (25)
- The user decided every item that was `blocked-needs-decision` (8) and every candidate found while fixing (16 entries). **21 decisions keep the iOS behaviour with no code change:** DBP-04 (with DBP-05, SNP-01, LP-19, LOAD-01), BRW-09 (DBP-22), EXP-12, EXP-13 (MSH-M02), EXP-16, LP-24, RPV-29, IMP-20 (three sub-rules), COND-UPD-02, COND-FOLDER-01, DBP-33, EXP-21, EXP-09, RPV-26, RPV-27 (a and b), RPV-16, UND-02, GEO-M02, IMP-12. **4 are code changes** (below). Counting rule: IMP-20 is three records (its three sub-rules), DBP-04 and DBP-05 are one record, RPV-27 (a) and (b) are one record; counted apart the keep decisions are 22.
- IMP-20, all three sub-rules: a DATETIME integer outside the C int range stores NULL and the import goes on; `BidTexts.Name` and `BidCallOuts.Name` are stored as UTF-8 text; memo and IMAGE text columns stay TEXT. The decided DATETIME rule itself (raw 6-part string, NULL when invalid) is fixed and tested.
- The 8 items that were `blocked-needs-decision` (DBP-04, BRW-09, EXP-12, EXP-13, EXP-16, IMP-20, LP-24, RPV-29) changed to `wont-fix`; the candidates keep their status and the question inside each is marked decided. DLG-FINITE-01 is decided only for its Rise/Run half.
- The DBP-33 decision text was corrected: `NameFontColor` is a Windows default (`reference_schema_metadata.py:113`), so the explicitly defaulted columns are Count, GridOffsetX/Y, GridRotation, FontColor and NameFontColor. `IsNegativeQuantity` (also a Windows default, `:111`) was checked against the code: it is a typed column written on every insert with the takeoff's own flag, so the record is consistent and the decision was not widened.
- The decisions are recorded in `PORTING_BLOCKERS.md`, `PARITY_MAP.md` and as `decisionUpdate` records in `PARITY_DIVERGENCES.json`; the status documents (`PORTING_STATUS.md`, `PORTING_STATUS.json`, `PORTING_PLAN.md`, `PORTING_MAP.md`) and this log were synced with them.

### Code changes (4), with host tests
- COND-STYLE-02 (deliberate deviation from the literal Windows behaviour): on a style change in a metric bid the metric type defaults `style_dimension_defaults(type, metric)` are persisted, not the imperial table. +6 tests (one added by the reviewer); the test that pinned the literal Windows behaviour was renamed and updated; 4 of 163 Conditions tests were red before the change. Review: production code correct, one test gap closed by the reviewer. Status `fixed-ui-unverified` (was `fixed`).
- COND-ELEV-01 (deliberate deviation): after a Save `is_top` and `z_value` are derived from the condition name with `extract_z_value_from_name`, so memory equals a reload. +8 tests, including a round trip through the real store (244 of 261 conditions differed on the old code; six of the first seven new tests were red). Review: confirmed fixed. Status `fixed-ui-unverified` (was `fixed`).
- DLG-FINITE-01 (port, Rise/Run half only): a whitespace-only Rise or Run is refused with "Rise must be a number." / "Run must be a number.". +2 tests (the new test was red before the change). Status stays `fixed`. The condition-number half is not decided and stays open (iOS refuses 2147483648 and up, Windows accepts any whole number the column holds).
- UND-09 (port): "Count as Negative Quantity" changes only the pressed takeoff, in one undoable entry (`Core/Domain/NegativeChange.hpp`, `ViewController.mm`, `UndoRedoController`). +6 tests in Settings. Status stays `fixed-ui-unverified`.
- Test first and review: the red run before the change is recorded for all four code changes (UND-09: against the old rule placed in Core, 5 of the 6 new tests failed; DLG-FINITE-01: 1 of 5 tests failed, on all 60 rows). All four were independently reviewed, each with a mutation check on a copy restored byte-identically: COND-STYLE-02 (production code correct, one test gap closed by the reviewer), COND-ELEV-01, DLG-FINITE-01 and UND-09 (no defect found).

### Possible Windows defects (researched read-only)
- For every kept item that looks like a Windows defect the Windows file:line was researched without touching the Windows repository: the real Windows functions were re-executed headless where possible and an adversarial pass merged the findings. 18 records, 14 for items decided on 2026-11-19, 3 for earlier decisions (EXP-17, EXP-M02, LP-22) and 1 for an open note (COND-PLACE-01 note 2); 28 questions were judged "kept, not a defect". All of them, with the file:line, are in `PORTING_STATUS.md` ("Possible Windows defects") for the user to fix on the Windows side. The most serious ones are EXP-12 and EXP-16 (a line break in a condition name injects statements into OBJ, MTL and DXF files; unsafe, high confidence) and COND-STYLE-02 and COND-ELEV-01 (wrong numbers).
- Every cited file existed and every line range lay inside its file (checked by script); seven citations were spot-checked again by hand while these documents were written. The Windows status snapshots taken during the steps were identical to the one taken at the start; HEAD `6d4786f`, working tree clean.

### Status and counts after this job
- 220 records: 172 fixed, 25 fixed-ui-unverified, 21 wont-fix, 0 blocked-needs-decision, 1 cannot-reproduce, 1 no-longer-divergent; 0 open. On 2026-11-18 it was 174 / 23 / 13 / 8 / 1 / 1.

### Verification (fresh runs, 2026-11-19)
- Host unit tests: 1598 of 1598 pass on MSVC and on clang 22 (zig toolchain, x86-64 with FMA3, `-Os`, `-ffp-contract=on`, Debug flags like CI); 1606 of 1606 with `-DOST_TEST_BOOLEAN=ON` on both. Before this job: 1576 and 1584. Added: 22 (Conditions +16, Settings +6). Per area: Browser 67, Conditions 174, Export 97 (102 with the boolean option), Formatting 74, Geometry 171, Harness 10, Import 304, Meshing 101 (104 with the boolean option), Parsers 114, Persistence 278, Quantities 134, Settings 74. iOS behaviour tests: 1588 (1598 minus the 10 harness self-tests).
- Objective-C syntax check: 27 of 27 platform files, 0 errors, 4 warnings (baseline protocol warnings in `EditConditionViewController`). This is not a build against the real SDK: the stubs are hand-written, the target is macOS, and nothing about ARC runtime behaviour, layout or UI is covered.
- Nothing was committed; the user commits.

### Not verified / still open
- Still open and not decided by the user (20 entries): the condition number above the int range (the other half of DLG-FINITE-01), the two COND-PLACE-01 notes (Image-layer exemption on hide; layer-less conditions on Hide All), 10 notes for information and 7 code-hygiene notes; see `PORTING_STATUS.md`, section "Still open".
- Not verified: no run of the GitHub workflow exists for the current code (the only run, 2026-11-16, stopped at the host unit tests), the `.mm` files have never been compiled against the real SDK, and nothing was run on a device. The three `.mm` changes of this job (`EditConditionViewController.mm`, `ViewController.mm`, `UndoRedoController`) were only syntax-checked against the hand-written stubs, and the UI part of the 25 fixed-ui-unverified ids, among them COND-STYLE-02, COND-ELEV-01 and UND-09, has never been seen running.

---

## 2026-11-18 - Objective-C wiring wave, final review, status documents synced

> Superseded in part on 2026-11-19 (entry above): the final item statuses, the 8 blocked items, the test counts and the 23 fixed-ui-unverified ids named in this entry are as of 2026-11-18.

### Objective-C wiring wave (2026-11-17 to 2026-11-18)
- Goal: make the Core results reachable from the app. No `.m`/`.mm` file can be compiled on the authoring machine, so a syntax checker was built first: `clang -fsyntax-only` (ARC, C++17, the include paths and defines of the build) over every platform `.m`/`.mm` against hand-written stub Foundation, UIKit, QuartzCore, CoreGraphics and UniformTypeIdentifiers headers. It found 4 compile errors that already existed at HEAD (Core-call signature drift in `DragController.mm`, `ViewController.mm` and `PlanView.mm`); they were fixed in the wave. The checker and the stubs are tools outside the repo.
- Six clusters, each a fixer and an independent reviewer (plus rework): U1 services (`ProjectService`, `ExportService`, export UI), U2a placement and drag numerics, U2b plan view visuals (page transform, holes, handles, minimum sizes), U3 `ViewController` (page switching, undo, camera, renderer settings), U4 conditions UI, U5 import and browser. The wave was interrupted and resumed twice (a usage limit, then a stop by the user to leave clean sections); after each stop the tree was checked: no half-edited file, line-ending check clean, Windows repo untouched.
- Result of the final audit: 108 wiring entries for 57 item ids, 108 done, 0 open. COND-PLACE-01 (hiding a layer suspends an active placement and showing it restores it, and the Conditions list follows the layer state) was the last open hand-off and was finished and reviewed at the end, with 9 new host tests.
- Final item statuses on 2026-11-18 (220 records; changed on 2026-11-19, see the entry above): 174 fixed, 23 fixed-ui-unverified, 13 wont-fix, 8 blocked-needs-decision (DBP-04, BRW-09, EXP-12, EXP-13, EXP-16, IMP-20, LP-24, RPV-29), 1 cannot-reproduce (EXP-24), 1 no-longer-divergent (MSH-19); 0 open. The 8 blocked items are Windows behaviours that look accidental, inconsistent or unsafe; they were not ported (iOS behaviour kept) and were listed with the Windows file:line, together with 16 candidates; the user decided all of them on 2026-11-19 (see the entry above).

### Verification at the end of the job (fresh runs, 2026-11-18)
- Host unit tests on 2026-11-18 (superseded by the counts of 2026-11-19 in the entry above): 1576 of 1576 pass on MSVC and on clang 22 (zig toolchain, x86-64 with FMA3, `-Os`, `-ffp-contract=on`, Debug flags like CI); 1584 of 1584 with `-DOST_TEST_BOOLEAN=ON` on both. Per area: Browser 67, Conditions 158, Export 97 (102 with the boolean option), Formatting 74, Geometry 171, Harness 10, Import 304, Meshing 101 (104), Parsers 114, Persistence 278, Quantities 134, Settings 68. iOS behaviour tests: 0 before this job, 1566 now (1576 minus the 10 harness self-tests).
- Objective-C syntax check: 27 of 27 platform files, 0 errors, 4 warnings (baseline protocol warnings in `EditConditionViewController`). This is not a build against the real SDK: the stubs are hand-written, the target is macOS, and nothing about ARC runtime behaviour, layout or UI is covered.
- Windows repo: working tree clean at `6d4786f` (2026-11-17; the commit holds the same 18 paths that were uncommitted at the 2026-11-16 re-baseline), read-only throughout.

### Documents
- `PORTING_STATUS.md` and `PORTING_STATUS.json` rewritten to the final state (phases, final statuses by area and impact, what was verified and how, what is not verified, the needs-my-decision list (replaced on 2026-11-19 by the decisions and the possible Windows defects), features not implemented). `PORTING_PLAN.md` rewritten as the remaining work (first full CI run, real-SDK compile, device checks, decisions, features not implemented). This entry and the reading note replace the stale parts of the 2026-11-15 entry.

### Not verified / still open
- No run of the GitHub workflow exists for the current code. The only run (2026-11-16) stopped at the host unit tests; everything after them (bgfx build, xcodegen, xcodebuild, ldid fake-signing, IPA) has never run. The `.mm` files have never been compiled against the real SDK and nothing was run on a device; the UI part of the 23 fixed-ui-unverified ids of that day (25 since 2026-11-19; listed in `PORTING_STATUS.md`) has never been seen.
- Comments in `Config/Tests/CMakeLists.txt` and `project.yml` still refer to a top-level `Tests/` folder; the harness is at `Config/Tests/` (the CMake source root path itself is correct).
- Nothing of this job is committed; the user commits.

---

## 2026-11-17 - Core completion

- The Core half of Phase 3 was finished on the re-baselined list of 220 records, in test-first steps (parsers and formatting, geometry and placement, quantities, meshing and export rules, persistence and import, conditions, browser, settings and undo), each with an independent reviewer. New Core modules took over logic that lived in `.mm` files (page transform and page view, placement and drag rules, takeoff footprint and hole grouping, condition choices, form rules and style change, dimension and number formatting, export text and file names, export mesh sets, OSP package rules, store integrity checks, browser rules, undo history, camera framing).
- End of the Core wave: the host tests passed on MSVC (with and without the boolean option) and on the clang build with `-ffp-contract=on` (the count at that moment is not recorded here; the final counts are in the 2026-11-18 entry); 56 items were marked deferred-wiring (Core done, `.mm` wiring pending), with a list of 108 wiring entries for 57 ids.
- The CI failure of 2026-11-16 (Meshing, `zero_len_7`) was fixed in Core: the meshing code, the earcut instantiation and some geometry code are compiled without fused multiply-add contraction (`Core/Geometry/strict_fp.hpp`, `#pragma clang fp contract(off)`, plus `ost_fp::hypot`). Verified on the clang+FMA host build only, not on GitHub.
- The Objective-C wave started the same day (see the 2026-11-18 entry).

---

## 2026-11-16 - Decisions, re-baseline, first CI run

- All 44 `needs-decision` items were answered by the user (33 "port", 11 "keep"; GEO-15 and EXP-17 chose the option that keeps outward-facing meshes and ended as wont-fix, so wont-fix is 13). Build decisions: minimum iOS 16.0 everywhere (`project.yml`, `Config/Info.plist`, `Config/Project.plist`, `build.yml`), `-Os` (`GCC_OPTIMIZATION_LEVEL: s`) in every configuration and in the Nyxian flags, `Info.plist` additions kept (empty `UILaunchScreen`, iPhone and iPad). The sources lost their pre-iOS-16 availability checks. The CI workflow gained a minos check of the five prebuilt libraries and a readback of `-Os` and the 16.0 target from the generated project.
- The Windows tree had changed since the Phase 1 map (uncommitted at the time; committed as `6d4786f` on 2026-11-17): one export boolean rule for the 3D view and all exports, writers that read only the post-boolean mesh, an export without a surviving mesh fails before writing, and a drop-stub union failure keeps the plain wall and logs a warning. The map was re-baselined against it: 4 new records (RB-01 to RB-04), 220 in total. `PARITY_DIVERGENCES.json` was added at the repo root as the machine-readable twin (header, items, decisions, test plans); its top level is `{header, items}`.
- First GitHub Actions run (00:48 UTC, macos-15-arm64, Xcode 16.4, Apple clang 17.0.0): checkout, Xcode selection, install of xcodegen 2.46.0 and ldid 2.1.5_2 and the configure and build of the host tests worked; the unit tests failed (Meshing 75 of 76, `CurvedMesh.CentreLineMatchesTheWindowsMeshOnTheOracleCases`, `zero_len_7` sample 3: a result that differs between MSVC and AppleClang). The job stopped there. The cause and fix are in the 2026-11-17 entry.

---

## 2026-11-15 - Build/Test Infrastructure, Parity Map, Documentation Rewrite

### Phase 0: build and test infrastructure (new files; committed and pushed the same day: `a950f9a`, `c73b6d0`)
- `project.yml` (XcodeGen) reproduces the Nyxian build of `Config/Project.plist` with standard Xcode settings: arm64 device only, deployment target 14.8 at that time (raised to 16.0 on 2026-11-16 by user decision), bundle id `com.fabian.ostvisualizer`, product name `OST Visualizer`, the same `-I`, `-D`, `-ObjC` and library flags. The Xcode project is generated and not committed (`.gitignore` gained `*.xcodeproj/`, `DerivedData/`, `/build/`, `/deps/`).
- `.github/workflows/build.yml` ("Build IPA", macos-15, Xcode 16.4): host unit tests, `libmanifold.a` arm64 check, plist lint, bgfx/bx/bimg built from pinned commits with an Actions cache, `xcodegen generate`, unsigned Release `xcodebuild`, bundle checks, fake-sign with `ldid`, entitlement verification, IPA upload. No Apple account, certificate or secret is used.
- Fake-signing: `Config/Entitlements.fakesign.plist` holds only `get-task-allowed`. The job fails if any entitlement key matches `memory|jetsam|extended-virtual-addressing`, because memory entitlements are applied after install with iRAM+ (bundle id and executable name are kept stable for that). `Config/Info.plist` (copied document types and scene manifest) and `BUILD.md` complete the set. The Nyxian files `Config/Project.plist` and `Config/Entitlements.plist` were not changed.
- Host test harness `Config/Tests/` (CMake, `Support/TestHarness.hpp`, `TestMain.cpp`, `BooleanOpsStub.cpp`, `Harness/test_harness_selftest.cpp`): builds all of `Core/` plus `Rendering/MeshLoader.cpp` (with `boolean_ops.cpp` stubbed unless `OST_TEST_BOOLEAN` is on) into one library and one test executable per area folder. Built and run with MSVC on Windows: the 10-test harness self-test passes. The AppleClang run is configured in CI and has not happened. There are no iOS behaviour tests yet and `Config/Tests/Fixtures` holds only `.gitkeep`.
- Why the tests live under `Config/`: Nyxian compiles every `.c`, `.cpp`, `.m`, `.mm` and `.swift` file under the project folder except the top-level `Config/` and `Resources/` folders (Nyxian's public source, `NXPhaseEngine.m`; the installed version was not inspected). The harness has its own `main()` and a boolean-operations stub, so outside `Config/` an on-device Nyxian build would fail with duplicate symbols. The CI build is unaffected because `project.yml` lists the app folders explicitly.

### Phase 3 (partial, stopped early): Core parity fixes on branch ios-parity-sync (commit `42ad10d`; completed on 2026-11-16 to 2026-11-18, see the later entries)
- Applied test-first in the host-testable Core (`Config/Tests/<Area>` ported from the Windows tests, Windows code as oracle): quantities (UomService.hpp), geometry/page transform, persistence error propagation (DBP-11), elevation/position parsing, meshing (curved walls, count shapes, area slope, 3D scale/rotation), import pipeline. 591 host tests pass under MSVC (iOS behaviour tests before: 0).
- NOT finished: independent review had completed only for quantities, geometry and DBP-11; the parsers, meshing and import steps were not reviewed or finished (import was mid-step); export, remaining persistence, browser, conditions and settings steps were not started; none of the Objective-C wiring (`.mm`) or extraction was done, so most fixes are not yet reachable from the app. Nothing here was built or run on iOS.
- The status docs (`PORTING_STATUS.*`, `PORTING_BLOCKERS.md`, `PORTING_MAP.md`, `PORTING_PLAN.md`) and `PARITY_MAP.md` still describe the pre-fix baseline (every item `open`); they were not updated for the fixes above (superseded: rewritten on 2026-11-18).

### Phase 1: parity map (`PARITY_MAP.md`)
- Method: 12 area mappers and 12 independent verifiers compared iOS with the Windows app (`dceb47d`) at iOS `4a1302d`. The real iOS C++ sources were compiled on the host with MSVC (with the vendored sqlite3 and pugixml) and compared with the Windows Python modules and native extensions; Obj-C/UIKit code was compared by reading and re-implementing the arithmetic. No iOS source file was edited.
- Result: 369 reported items merged into 216 divergences (153 merged as aliases, 1 refuted, 1 synthetic split MSH-19/MSH-19b): 171 `fix`, 44 `needs-decision`, 1 `cannot-reproduce`. By impact: 66 wrong numbers, 45 data safety, 86 behaviour, 19 UI. Of the 171 fix items, 83 are testable on the host as is, 76 need their logic moved from `.mm` into `Core/` first, 12 are UIKit/manual only.
- Largest findings: meshes are divided by the page scale ratio (MSH-01) and page rotation/flip is never applied in 3D (MSH-02); attachments on areas count 0 (QTY-01); `round_quantity` is never applied (QTY-04); curved length is wrong (QTY-07 to QTY-09); DXF and Three.js axes are wrong (EXP-01, EXP-02); import can overwrite other projects' rows (IMP-06); SQL failures are swallowed (DBP-11).
- Consequence for this log: earlier entries that say a module "matches desktop" or is "Ported" (for example the 2026-03-23 coordinate approach, area slope, hole grouping and Three.js entries) were not borne out by the comparison.

### Phase 2: documentation rewrite
- `PORTING_MAP.md` rewritten from the code: every iOS and Windows path verified to exist, one status per row, parity ids per row. `PORTING_PLAN.md` rewritten as the remaining work: Phase 3 fixes by impact and owner, the 44 decisions, features not implemented, validation that needs a Mac, CI or a device. This reading note was added to the log; older entries were not changed. `PORTING_STATUS.md`, `PORTING_STATUS.json` and `PORTING_BLOCKERS.md` were rewritten in the same pass (see those files).
- Claims removed as wrong: `ProjectStore+RenderConfig.cpp` (never existed; render config is in `ProjectStore+Conditions.cpp`), the Windows paths `infrastructure/mdb/ost_schema.py`, `domain/ost_schema.py` and `presentation/visualization/exporters/ost_exporter.py`, "no CI or test target", and the "Ported" status of rows that diverge.

### Not verified / still open at the end of 2026-11-15 (superseded by the 2026-11-18 entry)
- Nothing was built or run on iOS: no Xcode, macOS, device, XcodeGen or ldid was available. The CI workflow had not run yet (its first run, on 2026-11-16, stopped at the host unit tests); the Obj-C/Obj-C++ sources had not been compiled by Xcode.
- Phase 3 had only partly started (see above). The machine-readable parity data with the test-plan expected values was still outside the repositories; it was added as `PARITY_DIVERGENCES.json` at the repo root on 2026-11-16.
- Comments in `Config/Tests/CMakeLists.txt` and `project.yml` still refer to a `Tests/` folder; the harness is at `Config/Tests/` (the CMake source root path itself is correct).

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
