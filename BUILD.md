# Building OST Visualizer (iOS) in GitHub Actions

The workflow `.github/workflows/build.yml` ("Build IPA") builds the app on a macOS runner (`macos-15`, Xcode 16.4) with standard tools (XcodeGen + xcodebuild) and produces a **fake-signed** IPA. No Apple account, certificate, provisioning profile or secret is involved. The Nyxian build (`Config/Project.plist`, `Config/Entitlements.plist`) is kept; only `LDEMinimumVersion` (now 16.0) and `-Os` in `LDECompilerFlags` changed in `Config/Project.plist`, and the Nyxian build was not re-run.

Build settings that hold everywhere: **minimum iOS 16.0** and **`-Os`** (`GCC_OPTIMIZATION_LEVEL = s`) for C, C++ and Objective-C in every configuration.

Status: the workflow has gone through every step, up to the IPA upload, three times on GitHub (2026-11-20 UTC, runs 44654559214, 44661853681 and 44678523124, the last on `275cf66`, the code before job 5; see "What the first CI runs showed"). The IPA has not been installed or launched anywhere: install, launch and every runtime behaviour are not checked by CI and no device check is recorded (see "What is still not checked").

What the job does, in order (the step names are the ones in the Actions log):

1. Checkout.
2. Select Xcode and show the toolchain (`/Applications/Xcode_16.4.app`).
3. Install XcodeGen and ldid (`brew install xcodegen ldid`).
4. Show cmake version.
5. Unit tests (host build): `cmake -S Config/Tests -B build/tests -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build/tests --parallel "$(sysctl -n hw.ncpu)"`, `ctest --test-dir build/tests --output-on-failure --no-tests=error`. A failing test stops the job before any library or app build. The build gets a job count on purpose: a bare `--parallel` is an unlimited `make -j` with the Makefile generator, which started about 60 and then about 110 compiler processes at once on the runner (documented as 3 cores and about 7 GB RAM, not printed in the job log) and made this step take about 55 minutes instead of a few (first full run, 2026-11-20).
6. Verify `libmanifold.a` is arm64.
7. Validate plists (`plutil -lint` on `Config/Info.plist` and `Config/Entitlements.fakesign.plist`).
8. bgfx, bx and bimg libraries: restore from the Actions cache or, on a cache miss, check out the three pinned sources, build `ios-arm64-release`, copy the four libraries to `Libs/bgfx/bgfx/.build/ios-arm64/bin`; then "Verify bgfx libraries" (each is arm64 only).
9. Verify the minimum iOS (minos) of the five prebuilt libraries (`libmanifold.a` and the four bgfx libraries): none may need more than iOS 16.0 (see "Things to know").
10. Save the bgfx libraries to the cache (only after steps 8 and 9 passed).
11. Generate Xcode project: `xcodegen generate --spec project.yml`, then `xcodebuild -list`.
12. Verify optimisation level and deployment target: reads `GCC_OPTIMIZATION_LEVEL` and `IPHONEOS_DEPLOYMENT_TARGET` back from the generated project for Debug and Release; must be `s` and 16.0.
13. Build app (Release, iphoneos, unsigned): `xcodebuild ... CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY=""`, log in `build/xcodebuild.log`.
14. Locate and check the app bundle: exactly one `.app`; executable `OST Visualizer`, bundle id `com.fabian.ostvisualizer` and `MinimumOSVersion` 16.0 (`EXPECTED_MIN_OS`) as expected; the executable is arm64 only; no `*.sc` shader source in the bundle.
15. Fake-sign with ldid: `Config/Entitlements.fakesign.plist` converted by `plutil`, memory-entitlement check, `ldid -S`.
16. Verify entitlements: `ldid -e` is printed and checked again.
17. Package IPA: `Payload/OST Visualizer.app` zipped into `OST-Visualizer.ipa`; the executable must be in the listing.
18. Upload IPA (artifact `OST-Visualizer-ipa`, 30 days). If a step failed, the `xcodebuild` log is uploaded as `xcodebuild-log` (7 days).

## Run it

- Every push to any branch starts a run (a newer push to the same ref cancels the run in progress). The job has a 90 minute timeout. Measured on 2026-11-20 UTC: 4 min 8 s with a warm bgfx cache (run 44661853681; run 44678523124, also warm, was created at 05:04:01 and finished at 05:08:48 UTC); the cold-cache run took 57 min 47 s (run 44654559214), of which the host-test step was 55 min 20 s (still unbounded then, see step 5); bgfx itself was built in 25 s.
- Or: GitHub, **Actions** tab, **Build IPA**, **Run workflow**. Or from a terminal: `gh workflow run build.yml --ref main` (the workflow file must exist on the default branch for manual runs), then `gh run watch`.
- The first run (and the first run after a pin change) compiles bgfx, which took 25 s on the runner (cache miss of 2026-11-20 UTC, run 44654559214; that run was slow for another reason, see above). Later runs restore the libraries from the Actions cache (keyed by the three SHAs and `BGFX_RECIPE_VERSION`).

## Download and install

1. Open the finished run and download the artifact **OST-Visualizer-ipa** (a zip that contains `OST-Visualizer.ipa`). Or: `gh run download <run-id> -n OST-Visualizer-ipa`. Artifacts are kept 30 days.
2. The IPA is fake-signed, so stock iOS refuses to install or launch it. Use an installer that accepts fake-signed IPAs, for example TrollStore, or re-sign it with your own tool. These are examples, not requirements.

## Entitlements

`Config/Entitlements.fakesign.plist` is the only place to edit them. It is deliberately minimal: its only entitlement is `get-task-allowed` (true), and it explains itself in a comment. The job converts it with `plutil`, signs with `ldid -S`, prints the result (`ldid -e`) in the log, and **fails** (before signing and again on the signed binary) if any entitlement key matches `memory`, `jetsam` or `extended-virtual-addressing` (for example `com.apple.developer.kernel.increased-memory-limit` or `com.apple.developer.kernel.extended-virtual-addressing`). The pattern is `MEMORY_ENTITLEMENT_REGEX` at the top of `build.yml`.

To add an entitlement, add a `<key>` and its value inside the `<dict>` of `Config/Entitlements.fakesign.plist`, for example:

```xml
<key>get-task-allowed</key>
<true/>
<key>your.entitlement.key</key>
<true/>
```

Commit and push. The next run embeds it, and the "Verify entitlements" step prints the final list. Only add entitlements your fake-signing installer honours (some installers reject IPAs with entitlements they do not know). Never add memory entitlements here: iRAM+ is the only place they are applied.

Memory entitlements are **not** part of the build. You add them after installing, with iRAM+. For that to keep working after a reinstall, the bundle id (`com.fabian.ostvisualizer`) and the executable name (`OST Visualizer`, inside `OST Visualizer.app`) are kept the same in every build; the job fails if they change.

## Bump the bgfx pin

bgfx, bx and bimg are checked out at the commits in the `env:` block at the top of `build.yml` (`BGFX_SHA`, `BX_SHA`, `BIMG_SHA`). They are the commits the headers committed under `Libs/bgfx/` were taken from. To move to newer ones:

1. Change the three SHAs.
2. Replace `Libs/bgfx/bgfx/include`, `Libs/bgfx/bx/include` and `Libs/bgfx/bimg/include` with the `include/` folders of the same commits. The app compiles against the committed headers and links the freshly built libraries, so they must match.
3. If the bgfx shader binary version changed (the committed `Rendering/Shaders/generated/*.h` start with the bytes `FSH`/`VSH` and then `0x0b`, i.e. version 11), regenerate them with the `shaderc` of the same bgfx commit.

Bump `BGFX_RECIPE_VERSION` to throw away the cached libraries without changing the SHAs. Always bump it when you change `runs-on` or `XCODE_APP` too: they are not part of the cache key (it holds the runner OS and architecture, the three SHAs and `BGFX_RECIPE_VERSION`). The libraries are built with Xcode 16.4 on `macos-15`; the generated bgfx makefiles hard-code clang flags that were only verified with that toolchain.

## Unit tests locally

The tests build the platform-independent `Core/` (and `Rendering/MeshLoader.cpp`) with CMake (C++17) and are the same commands as the CI step, except that CI gives `--parallel` a job count (see the note below the macOS and Linux commands). Run them from the repository root.

macOS or Linux:

```sh
cmake -S Config/Tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure --no-tests=error
```

On a machine with little memory give `--parallel` a number (`--parallel 4`): with the Makefile generator a bare `--parallel` is an unlimited `make -j`, and about 110 compiler processes at once are a lot for a 7 GB machine (on the CI runner, documented as 3 cores and about 7 GB, a bare `--parallel` made the host-test step take 55 minutes instead of 168 s) (the CI step passes `"$(sysctl -n hw.ncpu)"`).

Windows (MSVC), from an "x64 Native Tools Command Prompt for VS" (or after running `vcvars64.bat`):

```bat
cmake -S Config\Tests -B build\tests -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build\tests --parallel
ctest --test-dir build\tests --output-on-failure --no-tests=error
```

With a non-MSVC compiler (AppleClang on the CI runner, GCC, Clang) the tests are compiled with `-Os`, like the app, on top of the Debug flags (`-g`, no `NDEBUG`). MSVC builds are left alone (Debug stays `/Od`).

Windows downloads zlib automatically (needs git and network). Add `-DOST_TEST_BOOLEAN=ON` to the first command to build the real Manifold boolean operations (downloads Manifold v3.0.0); CI does not set it. There are 12 test areas (ctest entries) and, on the last local runs (job 5, 2026-11-20, MSVC and clang with FMA), 1720 tests, all passing, plus 1729 with `-DOST_TEST_BOOLEAN=ON`; history: 1598 (1606 with booleans) on 2026-11-19, 1599 (1607) after the first CI fix of 2026-11-20 (`5dc7b2b`), 1632 (1640) after the two device-bug fixes, then job 5 (+88, +89 with booleans); before the parity job there were 0 iOS behaviour tests. The per-area counts, how to add a test, the oracle files and the FMA caveat are in `Config/Tests/README.md`.

## Build on a Mac by hand

`brew install xcodegen`, put the four bgfx libraries in `Libs/bgfx/bgfx/.build/ios-arm64/bin/` (take them from a workflow run: the cache, or run the bgfx steps yourself), `xcodegen generate`, then open `OSTVisualizer.xcodeproj` or run the same `xcodebuild` command as the workflow. The generated project is not committed.

## Things to know

- **Minimum iOS is 16.0, everywhere:** `project.yml` (`deploymentTarget` and `IPHONEOS_DEPLOYMENT_TARGET`), `Config/Info.plist` (`MinimumOSVersion`), `Config/Project.plist` (`LDEMinimumVersion`) and the workflow (`EXPECTED_MIN_OS`). bgfx's build scripts hard-code `-mios-version-min=16.0` and `libmanifold.a` is built for 16.0 too, so the libraries and the app agree. The sources contain no availability checks or fallbacks (`@available`, `API_AVAILABLE`, `systemVersion` comparisons; searched again on 2026-11-18), so every API they use must exist on iOS 16.0: Xcode 16.4 compiled all 88 translation units of the app against the iPhoneOS 18.5 SDK with deployment target 16.0 and `-Wunguarded-availability` on, and the build log of 2026-11-20 UTC has no availability warning; nothing was run on an iOS 16 device.
- **Minos of the prebuilt libraries is checked in CI.** The step "Verify the minimum iOS (minos) of the prebuilt libraries" (after the bgfx libraries are restored or built, before they are cached) parses every ar member of `libmanifold.a`, `libbgfxRelease.a`, `libbxRelease.a`, `libbimgRelease.a` and `libbimg_decodeRelease.a` as a Mach-O object (thin or fat), reads `LC_BUILD_VERSION` (`minos`; `LC_VERSION_MIN_IPHONEOS` for old objects) and fails the job if any object has a minos greater than `EXPECTED_MIN_OS` (16.0), was not built for iOS (platform 2), has no version command, or if an archive contains no object. When bumping bgfx or rebuilding `libmanifold.a`, keep their deployment target at 16.0 or lower. The same script, run on 2026-11-18 on the copies in the working tree (`libmanifold.a`: 15 objects; the four bgfx libraries, which are not tracked in git: 22, 22, 24 and 1 objects), reports platform 2, minos 16.0 for every object. The step has also run on GitHub (2026-11-20 UTC) and passed in the first two successful runs with the same object counts and platform 2, minos 16.0 for every object, on the bgfx libraries the workflow built from the pinned sources (run 44654559214) and on their cached copies (run 44661853681). The step passed in run 44678523124 as well (the bgfx libraries again came from the cache).
- **Size optimisation.** `project.yml` sets `GCC_OPTIMIZATION_LEVEL: s` (`-Os`) for C, C++ and Objective-C in the base settings and in the Debug and Release configurations, and `Config/Project.plist` passes `-Os` to the Nyxian build. The step "Verify optimisation level and deployment target" reads both values back from the generated Xcode project for both configurations. There are no Swift sources, so no Swift optimisation level is set. The host tests use `-Os` too (non-MSVC compilers). In the CI runs of 2026-11-20 UTC the settings read back as `s` and 16.0 for Debug and Release and all 88 compile units of the Release build use `-Os`; the optimisation level cannot be read from the finished binary.
- The app is compiled with the iPhoneOS 18.5 SDK (Xcode 16.4, Apple clang 17; seen in the logs of the runs of 2026-11-16 and 2026-11-20 UTC; the executable in the downloaded IPA, inspected locally, reports platform iOS, minos 16.0, sdk 18.5). The Nyxian build links against the SDK on the device (26.4). On iOS 26 an app linked against an older SDK typically keeps the older system look, so the UI chrome can differ from the Nyxian build (untested).
- **Nyxian and the tests.** Nyxian compiles every `.c`, `.cpp`, `.m`, `.mm` and `.swift` file under the project folder except the top-level `Config/` and `Resources/` folders (Nyxian's public source, `NXPhaseEngine.m`; the installed version was not checked). The host unit tests contain their own `main()` and a boolean-operations stub, so they live in `Config/Tests/` (not in a top-level `Tests/`), where Nyxian does not look. Do not move them out of `Config/`, or an on-device Nyxian build fails with duplicate symbols. The CI build is not affected: `project.yml` lists the app folders explicitly.

## What the first CI runs showed

Five runs of the workflow are recorded here (branch `ios-parity-sync`, runner image `macos-15-arm64`, Xcode 16.4 build 16F6, iPhoneOS 18.5 SDK, Apple clang 17.0.0; dates UTC). GitHub documents 3 cores and about 7 GB RAM for this standard runner size; the job log does not print them, so that is documented, not measured. Between the first two successful runs the runner image version (20261104.0514.1, then 20261118.0550.1), the Homebrew XcodeGen (2.46.0, then 2.47.0) and cmake (4.4.5, then 4.5.0) changed; Xcode and the compiler did not.

| Run | Commit | Result |
|---|---|---|
| 43782525569, 2026-11-16 | `42b46c3` | Failed after 1 min in "Unit tests (host build)": Meshing `CurvedMesh.CentreLineMatchesTheWindowsMeshOnTheOracleCases`, fused multiply-add on arm64. Fixed by `Core/Geometry/strict_fp.hpp` (`a9f1e52`). |
| 44642251350, 2026-11-20 | `292c406` | Failed after 1 h 5 min in the host tests: 11 of 12 areas passed, Geometry `MarkerAnchor.CountTriangleAndCurvedBand` failed (x 77.4244 against 72.5756: a tie decided by the last bit of a libm result, inferred and not observed on arm64; see `Config/Tests/README.md`, "FMA and arm64"). The step took about 64 minutes because of an unbounded `make -j`; XcodeGen, `xcodebuild`, ldid and packaging did not run. |
| 44654559214, 2026-11-20 | `5dc7b2b` | **Success**, 57 min 47 s: the first run through every step. Host-test step 55 min 20 s (still unbounded), bgfx cache miss (bgfx built in 25 s and saved to the cache), `xcodebuild` 61 s. |
| 44661853681, 2026-11-20 | `787a57d` | **Success**, 4 min 8 s: host-test step 168 s (all 12 areas passed, 64 s of ctest), bgfx cache hit, `xcodebuild` 50 s. Its app executable is byte-identical to the previous run's (sha256 `3dce8c45db3ba6b35a9bbb7e0bdba88a59ffb9d1c00468c8963f9a77c023bcdd`). |
| 44678523124, 2026-11-20 | `275cf66` | **Success** (created 05:04:01, finished 05:08:48 UTC): the first run on the code of the two device-bug fixes (`34e14c9`, `aef35d8`) and their docs commit `275cf66`; host tests 12 of 12 areas passed, bgfx from the cache, `xcodebuild` BUILD SUCCEEDED, IPA artifact uploaded; its app executable differs from the first two runs' (see below). |

Two commits followed the failed run of 2026-11-20: `5dc7b2b` (the marker-anchor assertion accepts the Windows point or its mirror image and a new libm-free test pins the tie rule; tests and docs only, no app code) and `787a57d` (the host-test build gets a job count, see step 5; same sources, flags and tests, and the app executable is byte-identical before and after).

Run 44678523124 (event push of branch `ios-parity-sync`, head commit `275cf66`), created 2026-11-20 05:04:01 UTC, finished 05:08:48 UTC, conclusion success. It built the code of `34e14c9` and `aef35d8` (Delete in the 2D view and the transparent meshes: `ViewController.mm`, `UndoRedoController.mm`, `Renderer.mm`, `ProjectService.mm`, `MathUtils.hpp`, `TakeoffSelection.hpp`, `SceneRendering.hpp`) and the docs commit `275cf66`: the host unit tests passed, 12 of 12 areas (ctest; the CI build uses the Manifold stub), `xcodebuild` ended with BUILD SUCCEEDED against the real iPhoneOS SDK with the same first-party warnings as the earlier runs and no new one (the three `-Wundeclared-selector` warnings of `ViewController.mm` moved from `:310-312` to `:311-313` because `34e14c9` added a line above them), and the IPA artifact `OST-Visualizer-ipa` was downloaded and parsed locally (bundle id, minos 16.0, arm64, ad-hoc `ldid` signature with all page hashes recomputed, entitlements only `get-task-allowed`; the executable differs from the earlier runs and contains the device-bug fixes). Verified by GitHub run 44678523124 (success): the code up to `275cf66`. No run has seen the code of job 5 (to be verified on GitHub); nothing ran on a device (DEV-01 and DEV-02 stay open).

No run has seen the code of job 5 (2026-11-20, the re-baseline to Windows `b9902e6`, uncommitted when this was written; to be verified on GitHub): the host tests there are 1720 (1729 with Manifold), and the Objective-C(++) files it changed (`ImportService`, `EditConditionViewController`, `ViewController`, `PlanView`) have not been compiled against the real SDK.

What the steps printed in the first two successful runs (identical unless noted; run 44678523124 passed the same steps):

| Step | What the runs showed (the first two successful runs, identical unless noted) |
|---|---|
| Select Xcode | `Xcode 16.4`, iphoneos SDK `18.5`, Apple clang 17 |
| Install XcodeGen and ldid | `ldid` 2.1.5_2 found; XcodeGen 2.46.0 in run 44654559214 and 2.47.0 in run 44661853681 (`brew` installs the current version, it is not pinned) |
| Unit tests (host build) | ctest `100% tests passed out of 12` on AppleClang 17 arm64 (Debug): Browser, Conditions, Export, Formatting, Geometry, Harness, Import, Meshing, Parsers, Persistence, Quantities, Settings. The step took 168 s (64 s of ctest) in run 44661853681 with the bounded job count (55 min 20 s in run 44654559214, bare `--parallel`). A passing area prints no `TESTS: n PASSED: n` line (only a failing one does, because of `--output-on-failure`), so the CI log does not show the test count (the suite had 1599 tests in those two runs and 1632 at `275cf66`, the code of run 44678523124; 1720 since job 5, which no run has seen yet). The step uses the Manifold stub (no `-DOST_TEST_BOOLEAN`), so the tests that need real Manifold are not part of it |
| Verify libmanifold.a is arm64 | `arm64` |
| Validate plists | `Config/Info.plist: OK`, `Config/Entitlements.fakesign.plist: OK` |
| Build bgfx (cache miss only) | Run 44654559214 (cache miss): bgfx, bx and bimg built from the pinned SHAs in 25 s, `ls` shows the four `.a` files in `deps/bgfx/.build/ios-arm64/bin`, then saved to the cache. Run 44661853681 (cache hit, about 9 MB): the build was skipped |
| Verify bgfx libraries | four lines `...a: arm64` |
| Verify the minimum iOS (minos) | `platform 2 (2 = iOS) minos 16.0: N objects ok` for every library: `libmanifold.a` 15 objects, `libbgfxRelease.a` 22, `libbxRelease.a` 22, `libbimgRelease.a` 24, `libbimg_decodeRelease.a` 1 (the bgfx ones built by the workflow in run 44654559214, restored from the cache in run 44661853681) |
| Generate Xcode project | `OSTVisualizer.xcodeproj` exists; `xcodebuild -list` shows the `OSTVisualizer` target and scheme |
| Verify optimisation level and deployment target | `Debug: GCC_OPTIMIZATION_LEVEL=s IPHONEOS_DEPLOYMENT_TARGET=16.0` and the same for Release |
| Build app | `** BUILD SUCCEEDED **`: `xcodebuild` took 61 s (run 44654559214) and 50 s (run 44661853681); 88 translation units, every one with `-Os`; details and warnings below |
| Locate and check the app bundle | `executable: OST Visualizer`, `bundle id:  com.fabian.ostvisualizer`, `min iOS:    16.0` |
| Verify entitlements | an XML plist whose only key is `get-task-allowed` |
| Package IPA | listing: `Payload/OST Visualizer.app/` with `OST Visualizer` (5,115,376 bytes), `Info.plist` and `PkgInfo`; `build/OST-Visualizer.ipa` is 1,963,627 bytes |
| Upload IPA | artifact `OST-Visualizer-ipa` |

**Build app** was expected to be the step most likely to fail, and it passed. Xcode 16.4 compiled the 88 translation units of the app target, every one with `-Os` (read from the 4 clang response files in the log), and linked them with one `clang++` call against `Libs/Manifold/lib` and the bgfx library folder. This is the first compile of every Objective-C and Objective-C++ file against the real iPhoneOS 18.5 SDK, and it had no error. It goes beyond the earlier syntax check against hand-written stub headers (27 files, 0 errors), which could not see real signatures, nullability, protocol conformance, ARC ownership or availability. The log has no availability warning although `-Wunguarded-availability` is on in the compile lines. The same step passed in run 44678523124 on the code up to `275cf66`, which includes the device-bug fixes in `Renderer.mm`, `ProjectService.mm`, `ViewController.mm` and `UndoRedoController.mm`.

Warnings in first-party code (none is an error): six `-Wundeclared-selector` (`PlanView.mm:180` and `:186`, `ViewController.mm:310-312` (`:311-313` in run 44678523124), `EditConditionViewController+TableView.mm:154`; the six methods are defined in category or class files and are only not declared where they are referenced); one `-Wprotocol` at `EditConditionViewController.mm:62` and two `-Wobjc-protocol-method-implementation` in `EditConditionViewController+TableView.mm` (the class extension adopts `UITableViewDataSource`, the category implements it, and no duplicate implementation exists); `ld: ignoring duplicate libraries '-lc++'`; and two Info.plist warnings that come from `Config/Info.plist`: it declares neither `LSSupportsOpeningDocumentsInPlace` nor `UISupportsDocumentBrowser`, and not all interface orientations are supported (the iPad multitasking warning). Adding keys would change app behaviour, so that is the owner's decision. 88 more warnings are in third-party code (mostly `sqlite3.c` `-Wshorten-64-to-32`, plus `-Wdocumentation` in `bgfx.h` and Manifold `linalg.h`). The first two successful runs also carry the annotation that Node.js 20 is deprecated and that `actions/checkout@v4`, `actions/cache/restore@v4`, `actions/cache/save@v4` (named in run 44654559214 only, the other run skips the save) and `actions/upload-artifact@v4` are forced to run on Node.js 24: a warning, not a failure, but the action versions may need a bump later. Run 44678523124 printed the same first-party warnings and no new one (the three `-Wundeclared-selector` warnings of `ViewController.mm` moved to `:311-313`).

The downloaded IPA of the first two successful runs was inspected locally with a Python parser that is not in the repository; nothing was executed. Zip integrity is fine and there is one `Payload/OST Visualizer.app`. Its `Info.plist` has bundle id `com.fabian.ostvisualizer`, `MinimumOSVersion` 16.0, `DTSDKName` iphoneos18.5, `UIDeviceFamily` [1, 2] and every key of `Config/Info.plist` with the build variables expanded. The executable is a thin arm64 Mach-O (platform iOS, minos 16.0, sdk 18.5, `cryptid` 0). The ad hoc signature (SHA-1 and SHA-256 code directories) has all 1233 page hashes equal to the file, an entitlements blob whose hash matches its special slot, no CMS signature (as `ldid` makes it), and only `get-task-allowed`; no entitlement key matches `memory|jetsam|extended-virtual-addressing`. `-Os` cannot be read from a binary: it rests on the settings read back in CI and on the compile lines. The IPA of run 44678523124 was downloaded and parsed locally the same way (bundle id, minos 16.0, arm64, ad-hoc `ldid` signature with all page hashes recomputed, entitlements only `get-task-allowed`); its executable differs from the earlier runs' and contains the device-bug fixes. What these runs do not show is listed under "What is still not checked".

## Troubleshooting by step

| Step | What failed | What to check |
|---|---|---|
| Select Xcode | `$XCODE_APP is not installed on this runner image` | The runner image no longer has Xcode 16.4. Change `XCODE_APP` and `runs-on` together and bump `BGFX_RECIPE_VERSION` (the bgfx makefiles hard-code clang paths verified only with 16.4). |
| Install XcodeGen and ldid | `brew` error or `command -v ldid` fails | Network or Homebrew problem on the runner; re-run. |
| Unit tests (host build) | A test or an area fails | Run the same three commands locally (`Config/Tests/README.md`, `--filter`). A failing port is an iOS Core bug or a Windows ambiguity: never edit an expected value or a tolerance to pass; see `PORTING_BLOCKERS.md`. A failure in Meshing or Geometry that does not reproduce on MSVC is probably fused multiply-add on arm64 in a file that does not include `Core/Geometry/strict_fp.hpp`; if that is excluded, suspect a result decided by the last bit of a libm function (`atan2`, `sin`, `cos`): one such tie is recorded in `PORTING_BLOCKERS.md`, "Not reproducible on iOS". `no tests were found` means the CMake configure found no `test_*.cpp`. The step takes about 3 minutes (168 s on 2026-11-20 UTC); tens of minutes suggest that the build job count is unbounded again (see step 5). |
| Verify libmanifold.a is arm64 | wrong architecture list | `Libs/Manifold/lib/libmanifold.a` must contain arm64 only. |
| Validate plists | `plutil -lint` error | XML syntax in `Config/Info.plist` or `Config/Entitlements.fakesign.plist` (a stray character in a comment is enough). |
| Build bgfx libraries | `make` fails | The generated makefiles expect Xcode 16.4 at `/Applications/Xcode.app`, `SDKROOT` exported in the same step, and bx and bimg next to bgfx. Check the three SHAs. |
| Verify bgfx libraries | not arm64 only / file missing | The build did not produce `libbgfxRelease`, `libbxRelease`, `libbimgRelease`, `libbimg_decodeRelease` for ios-arm64. A cache entry must never hold anything else (bump `BGFX_RECIPE_VERSION` to drop it). |
| Verify the minos of the prebuilt libraries | `FAIL` line, `no Mach-O objects`, error text | Some object needs a newer iOS than 16.0, was not built for iOS, or has no version command. Rebuild with a deployment target of 16.0 or lower. |
| Generate Xcode project | `xcodegen` error | `project.yml` syntax or a source path that does not exist; `minimumXcodeGenVersion` is 2.40.0. |
| Verify optimisation level and deployment target | `must be s` / `must be 16.0` | `project.yml`: base settings and the Debug and Release entries. |
| Build app | compile error | Download the `xcodebuild-log` artifact (7 days). The first real compile of every Objective-C and Objective-C++ file (2026-11-20 UTC) had no error, so a new error comes from a later change or from a changed runner image or Xcode: signatures of the Core functions they call, missing `#include`, protocol methods, ARC ownership. Warnings do not fail the step; the ones seen so far are listed under "What the first CI runs showed". Link errors: missing static libraries in `Libs/bgfx/bgfx/.build/ios-arm64/bin` (cache) or duplicate symbols (a file with a `main()` or the boolean stub reached the app target: the host tests must stay in `Config/Tests`). |
| Locate and check the app bundle | `expected exactly one .app`, identity or arm64 error, `*.sc` found | `PRODUCT_NAME`, `PRODUCT_BUNDLE_IDENTIFIER`, `Config/Info.plist` `MinimumOSVersion`, `ARCHS`; a `.sc` file means `project.yml` started copying resources. |
| Fake-sign with ldid | `contains a memory entitlement` | Remove the key from `Config/Entitlements.fakesign.plist`; iRAM+ adds memory entitlements after installation. |
| Verify entitlements | `no entitlements embedded` or a memory key in the binary | `ldid -S` did not embed the plist, or the entitlements file changed. |
| Package IPA | `executable missing from the IPA` | The `.app` copy failed; look at `build/ipa-listing.txt` in the log. |

## What is still not checked

Closed by the runs of 2026-11-20 UTC: `project.yml` generates a project that Xcode 16.4 compiles and links, `Release` (iphoneos, unsigned) produces an arm64 app, and `ldid` signs it on the runner (see "What the first CI runs showed"). Still open, because CI cannot check it or no run did:

- The `Debug` configuration and an iOS Simulator slice: only the Debug settings were read back (`GCC_OPTIMIZATION_LEVEL = s`, deployment target 16.0); only `Release` for `iphoneos` was compiled, and no Simulator slice was built.
- That the IPA installs and starts, and every UI behaviour (the 26 `fixed-ui-unverified` items, 25 until job 5, are listed in `PORTING_BLOCKERS.md`; the device checks too: DEV-01 and DEV-02 of the two device-bug fixes of 2026-11-20 and DEV-03 to DEV-08 of job 5, all eight in `PORTING_BLOCKERS.md`): needs a device and a fake-sign installer (afterwards iRAM+ applies the memory entitlements, which the owner does). CI built, signed and packaged the IPA, but nothing was installed or run: crashes, Metal/bgfx rendering and shaders on a GPU, picking, frame time and memory, large projects, `.ost` and `.osp` open-in and the document picker are all unchecked.
- Whether the owner's installer accepts the fake signature. `ldid` 2.1.5 signed the executable on the runner and `ldid -e` printed the same entitlements (only `get-task-allowed`); a local parser checked the signature structure and every page hash of the downloaded IPA, but nothing was installed.
- Real Manifold on AppleClang, and Manifold on a device. The host tests did run on AppleClang 17 arm64 in CI (all 12 areas passed in runs 44654559214, 44661853681 and 44678523124), but with the Manifold stub, so the `-DOST_TEST_BOOLEAN=ON` configuration (1729 tests instead of 1720 with the code of job 5; 1640 and 1632 at `275cf66`; 1607 and 1599 at the time of the first two runs) ran only on MSVC and on a clang build with fused multiply-add on x86-64. The libm cause of the marker-anchor tie is inferred, not observed on arm64; on a device the marker of an exactly mirror-symmetric curved band may sit one grid cell (4.85 points at the test scale) on the other side of its axis than on Windows.
- Nyxian: the changes to `Config/Project.plist` (16.0, `-Os`) were not run in Nyxian, and the Nyxian source scan rule above comes from Nyxian's public source.
- Representative real `.ost` and `.osp` project files: the repository holds only synthetic oracles and two CAB files, and no run touches a real project.
