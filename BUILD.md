# Building OST Visualizer (iOS) in GitHub Actions

The workflow `.github/workflows/build.yml` ("Build IPA") builds the app on a macOS runner (`macos-15`, Xcode 16.4) with standard tools (XcodeGen + xcodebuild) and produces a **fake-signed** IPA. No Apple account, certificate, provisioning profile or secret is involved. The Nyxian build (`Config/Project.plist`, `Config/Entitlements.plist`) is kept; only `LDEMinimumVersion` (now 16.0) and `-Os` in `LDECompilerFlags` changed in `Config/Project.plist`, and the Nyxian build was not re-run.

Build settings that hold everywhere: **minimum iOS 16.0** and **`-Os`** (`GCC_OPTIMIZATION_LEVEL = s`) for C, C++ and Objective-C in every configuration.

What the job does, in order (the step names are the ones in the Actions log):

1. Checkout.
2. Select Xcode and show the toolchain (`/Applications/Xcode_16.4.app`).
3. Install XcodeGen and ldid (`brew install xcodegen ldid`).
4. Show cmake version.
5. Unit tests (host build): `cmake -S Config/Tests -B build/tests -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build/tests --parallel`, `ctest --test-dir build/tests --output-on-failure --no-tests=error`. A failing test stops the job before any library or app build.
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

- Every push to any branch starts a run (a newer push to the same ref cancels the run in progress). The job has a 90 minute timeout.
- Or: GitHub, **Actions** tab, **Build IPA**, **Run workflow**. Or from a terminal: `gh workflow run build.yml --ref main` (the workflow file must exist on the default branch for manual runs), then `gh run watch`.
- The first run (and the first run after a pin change) compiles bgfx, which is the slow part. Later runs restore the libraries from the Actions cache (keyed by the three SHAs and `BGFX_RECIPE_VERSION`).

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

The tests build the platform-independent `Core/` (and `Rendering/MeshLoader.cpp`) with CMake (C++17) and are the same command as the CI step. Run them from the repository root.

macOS or Linux:

```sh
cmake -S Config/Tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure --no-tests=error
```

Windows (MSVC), from an "x64 Native Tools Command Prompt for VS" (or after running `vcvars64.bat`):

```bat
cmake -S Config\Tests -B build\tests -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build\tests --parallel
ctest --test-dir build\tests --output-on-failure --no-tests=error
```

With a non-MSVC compiler (AppleClang on the CI runner, GCC, Clang) the tests are compiled with `-Os`, like the app, on top of the Debug flags (`-g`, no `NDEBUG`). MSVC builds are left alone (Debug stays `/Od`).

Windows downloads zlib automatically (needs git and network). Add `-DOST_TEST_BOOLEAN=ON` to the first command to build the real Manifold boolean operations (downloads Manifold v3.0.0); CI does not set it. There are 12 test areas (ctest entries) and, on the last local run (2026-11-19, MSVC), 1598 tests, all passing, plus 1606 with `-DOST_TEST_BOOLEAN=ON`; before the parity job there were 0 iOS behaviour tests. The per-area counts, how to add a test, the oracle files and the FMA caveat are in `Config/Tests/README.md`.

## Build on a Mac by hand

`brew install xcodegen`, put the four bgfx libraries in `Libs/bgfx/bgfx/.build/ios-arm64/bin/` (take them from a workflow run: the cache, or run the bgfx steps yourself), `xcodegen generate`, then open `OSTVisualizer.xcodeproj` or run the same `xcodebuild` command as the workflow. The generated project is not committed.

## Things to know

- **Minimum iOS is 16.0, everywhere:** `project.yml` (`deploymentTarget` and `IPHONEOS_DEPLOYMENT_TARGET`), `Config/Info.plist` (`MinimumOSVersion`), `Config/Project.plist` (`LDEMinimumVersion`) and the workflow (`EXPECTED_MIN_OS`). bgfx's build scripts hard-code `-mios-version-min=16.0` and `libmanifold.a` is built for 16.0 too, so the libraries and the app agree. The sources contain no availability checks or fallbacks (`@available`, `API_AVAILABLE`, `systemVersion` comparisons; searched again on 2026-11-18), so every API they use must exist on iOS 16.0: that has not been checked against the SDK, only by Xcode when it compiles them.
- **Minos of the prebuilt libraries is checked in CI.** The step "Verify the minimum iOS (minos) of the prebuilt libraries" (after the bgfx libraries are restored or built, before they are cached) parses every ar member of `libmanifold.a`, `libbgfxRelease.a`, `libbxRelease.a`, `libbimgRelease.a` and `libbimg_decodeRelease.a` as a Mach-O object (thin or fat), reads `LC_BUILD_VERSION` (`minos`; `LC_VERSION_MIN_IPHONEOS` for old objects) and fails the job if any object has a minos greater than `EXPECTED_MIN_OS` (16.0), was not built for iOS (platform 2), has no version command, or if an archive contains no object. When bumping bgfx or rebuilding `libmanifold.a`, keep their deployment target at 16.0 or lower. The same script, run on 2026-11-18 on the copies in the working tree (`libmanifold.a`: 15 objects; the four bgfx libraries, which are not tracked in git: 22, 22, 24 and 1 objects), reports platform 2, minos 16.0 for every object. CI builds the bgfx libraries again from the pinned sources, so this is not a result for the CI libraries.
- **Size optimisation.** `project.yml` sets `GCC_OPTIMIZATION_LEVEL: s` (`-Os`) for C, C++ and Objective-C in the base settings and in the Debug and Release configurations, and `Config/Project.plist` passes `-Os` to the Nyxian build. The step "Verify optimisation level and deployment target" reads both values back from the generated Xcode project for both configurations. There are no Swift sources, so no Swift optimisation level is set. The host tests use `-Os` too (non-MSVC compilers).
- The app is compiled with the iPhoneOS 18.5 SDK (Xcode 16.4, Apple clang 17; seen in the log of the run of 2026-11-16 UTC). The Nyxian build links against the SDK on the device (26.4). On iOS 26 an app linked against an older SDK typically keeps the older system look, so the UI chrome can differ from the Nyxian build (untested).
- **Nyxian and the tests.** Nyxian compiles every `.c`, `.cpp`, `.m`, `.mm` and `.swift` file under the project folder except the top-level `Config/` and `Resources/` folders (Nyxian's public source, `NXPhaseEngine.m`; the installed version was not checked). The host unit tests contain their own `main()` and a boolean-operations stub, so they live in `Config/Tests/` (not in a top-level `Tests/`), where Nyxian does not look. Do not move them out of `Config/`, or an on-device Nyxian build fails with duplicate symbols. The CI build is not affected: `project.yml` lists the app folders explicitly.

## What a first CI run should show

Only the part up to the unit tests has ever run on GitHub: the run of 2026-11-16 UTC (image `macos-15-arm64`, Xcode 16.4, iPhoneOS 18.5 SDK, Apple clang 17.0.0, XcodeGen 2.46.0, ldid 2.1.5) passed the toolchain steps, then failed in "Unit tests (host build)": the Meshing area, one test, a floating-point difference between MSVC and AppleClang (see `Config/Tests/README.md`, "FMA and arm64"). The code has changed a lot since, so nothing below is a result; it is what each step should print if all is well.

| Step | Expected |
|---|---|
| Select Xcode | `Xcode 16.4`, iphoneos SDK `18.5`, Apple clang 17 |
| Install XcodeGen and ldid | versions printed, `ldid` found |
| Unit tests (host build) | ctest: `100% tests passed, 0 tests failed out of 12`; each area prints `TESTS: n PASSED: n FAILED: 0` with the counts in `Config/Tests/README.md` (1598 in total) |
| Verify libmanifold.a is arm64 | `arm64` |
| Validate plists | `Config/Info.plist: OK`, `Config/Entitlements.fakesign.plist: OK` |
| Build bgfx (cache miss only) | `ls` shows the four `.a` files in `deps/bgfx/.build/ios-arm64/bin` |
| Verify bgfx libraries | four lines `...a: arm64` |
| Verify the minimum iOS (minos) | per library `platform 2 (2 = iOS) minos 16.0: N objects ok` |
| Generate Xcode project | `OSTVisualizer.xcodeproj` exists; `xcodebuild -list` shows the `OSTVisualizer` target and scheme |
| Verify optimisation level and deployment target | `Debug: GCC_OPTIMIZATION_LEVEL=s IPHONEOS_DEPLOYMENT_TARGET=16.0` and the same for Release |
| Build app | `** BUILD SUCCEEDED **` (see the warning below) |
| Locate and check the app bundle | `executable: OST Visualizer`, `bundle id:  com.fabian.ostvisualizer`, `min iOS:    16.0` |
| Verify entitlements | an XML plist whose only key is `get-task-allowed` |
| Package IPA | listing starts with `Payload/OST Visualizer.app/...`; `ls -l build/OST-Visualizer.ipa` |
| Upload IPA | artifact `OST-Visualizer-ipa` |

**Build app is the step most likely to fail on the first run.** No Objective-C or Objective-C++ file of the current code has been compiled by Xcode: they were only syntax-checked with clang against hand-written stub versions of the Foundation, UIKit, QuartzCore and CoreGraphics headers (27 files, 0 errors, 4 baseline warnings). That check finds typos, wrong Core call signatures and missing includes. It does not check against the real Apple SDK (wrong or missing real signatures, nullability, protocol conformance, ARC ownership, iOS 16 availability), and it does not run anything. The link step has never run either (the Core, the five prebuilt libraries and the system libraries `-lz` and `-lc++`).

## Troubleshooting by step

| Step | What failed | What to check |
|---|---|---|
| Select Xcode | `$XCODE_APP is not installed on this runner image` | The runner image no longer has Xcode 16.4. Change `XCODE_APP` and `runs-on` together and bump `BGFX_RECIPE_VERSION` (the bgfx makefiles hard-code clang paths verified only with 16.4). |
| Install XcodeGen and ldid | `brew` error or `command -v ldid` fails | Network or Homebrew problem on the runner; re-run. |
| Unit tests (host build) | A test or an area fails | Run the same three commands locally (`Config/Tests/README.md`, `--filter`). A failing port is an iOS Core bug or a Windows ambiguity: never edit an expected value or a tolerance to pass; see `PORTING_BLOCKERS.md`. A failure in Meshing or Geometry that does not reproduce on MSVC is probably fused multiply-add on arm64 in a file that does not include `Core/Geometry/strict_fp.hpp`. `no tests were found` means the CMake configure found no `test_*.cpp`. |
| Verify libmanifold.a is arm64 | wrong architecture list | `Libs/Manifold/lib/libmanifold.a` must contain arm64 only. |
| Validate plists | `plutil -lint` error | XML syntax in `Config/Info.plist` or `Config/Entitlements.fakesign.plist` (a stray character in a comment is enough). |
| Build bgfx libraries | `make` fails | The generated makefiles expect Xcode 16.4 at `/Applications/Xcode.app`, `SDKROOT` exported in the same step, and bx and bimg next to bgfx. Check the three SHAs. |
| Verify bgfx libraries | not arm64 only / file missing | The build did not produce `libbgfxRelease`, `libbxRelease`, `libbimgRelease`, `libbimg_decodeRelease` for ios-arm64. A cache entry must never hold anything else (bump `BGFX_RECIPE_VERSION` to drop it). |
| Verify the minos of the prebuilt libraries | `FAIL` line, `no Mach-O objects`, error text | Some object needs a newer iOS than 16.0, was not built for iOS, or has no version command. Rebuild with a deployment target of 16.0 or lower. |
| Generate Xcode project | `xcodegen` error | `project.yml` syntax or a source path that does not exist; `minimumXcodeGenVersion` is 2.40.0. |
| Verify optimisation level and deployment target | `must be s` / `must be 16.0` | `project.yml`: base settings and the Debug and Release entries. |
| Build app | compile error | Download the `xcodebuild-log` artifact (7 days). Expect the first real compile of the Objective-C++ files here: signatures of the Core functions they call (they changed a lot), missing `#include`, protocol methods, ARC ownership. Link errors: missing static libraries in `Libs/bgfx/bgfx/.build/ios-arm64/bin` (cache) or duplicate symbols (a file with a `main()` or the boolean stub reached the app target: the host tests must stay in `Config/Tests`). |
| Locate and check the app bundle | `expected exactly one .app`, identity or arm64 error, `*.sc` found | `PRODUCT_NAME`, `PRODUCT_BUNDLE_IDENTIFIER`, `Config/Info.plist` `MinimumOSVersion`, `ARCHS`; a `.sc` file means `project.yml` started copying resources. |
| Fake-sign with ldid | `contains a memory entitlement` | Remove the key from `Config/Entitlements.fakesign.plist`; iRAM+ adds memory entitlements after installation. |
| Verify entitlements | `no entitlements embedded` or a memory key in the binary | `ldid -S` did not embed the plist, or the entitlements file changed. |
| Package IPA | `executable missing from the IPA` | The `.app` copy failed; look at `build/ipa-listing.txt` in the log. |

## What cannot be checked outside GitHub or macOS

- That `project.yml` generates a project that Xcode compiles and links, and that `Release` produces an app: no Xcode, XcodeGen or macOS was available while the code was written. The unit tests and the minos check above are the only parts that ran, on Windows.
- That the IPA installs and starts, and every UI behaviour (the 25 `fixed-ui-unverified` items are listed in `PORTING_BLOCKERS.md`): needs a device and a fake-sign installer.
- That `ldid` accepts `Config/Entitlements.fakesign.plist` on the runner. The tool was installed in the run of 2026-11-16 UTC; it was not run.
- The real arm64 AppleClang result of the host tests (only approximated by a clang build with fused multiply-add on x86-64).
- Nyxian: the changes to `Config/Project.plist` (16.0, `-Os`) were not run in Nyxian, and the Nyxian source scan rule above comes from Nyxian's public source.
