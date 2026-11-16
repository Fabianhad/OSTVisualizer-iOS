# Building OST Visualizer (iOS) in GitHub Actions

The workflow `.github/workflows/build.yml` ("Build IPA") builds the app on a macOS runner with standard tools (XcodeGen + xcodebuild) and produces a **fake-signed** IPA. No Apple account, certificate, provisioning profile or secret is involved. The Nyxian build (`Config/Project.plist`, `Config/Entitlements.plist`) is untouched and keeps working.

What the job does, in order: checkout, unit tests (host build of `Core/`), check `libmanifold.a` is arm64, restore or build the bgfx libraries from pinned sources, `xcodegen generate` (from `project.yml`), `xcodebuild` (iphoneos, Release, signing off), fake-sign with `ldid`, verify the entitlements, zip `Payload/OST Visualizer.app` into `OST-Visualizer.ipa`, upload it.

## Run it

- Every push to any branch starts a run.
- Or: GitHub, **Actions** tab, **Build IPA**, **Run workflow**. Or from a terminal: `gh workflow run build.yml --ref main` (the workflow file must exist on the default branch for manual runs), then `gh run watch`.
- The first run (and the first run after a pin change) compiles bgfx, which is the slow part. Later runs restore the libraries from the Actions cache (keyed by the three SHAs and `BGFX_RECIPE_VERSION`).

## Download and install

1. Open the finished run and download the artifact **OST-Visualizer-ipa** (a zip that contains `OST-Visualizer.ipa`). Or: `gh run download <run-id> -n OST-Visualizer-ipa`. Artifacts are kept 30 days.
2. The IPA is fake-signed, so stock iOS refuses to install or launch it. Use an installer that accepts fake-signed IPAs, for example TrollStore, or re-sign it with your own tool. These are examples, not requirements.

## Entitlements

`Config/Entitlements.fakesign.plist` is the only place to edit them. It is deliberately minimal (only `get-task-allowed`) and explains itself in a comment. The job converts it with `plutil`, signs with `ldid -S`, prints the result (`ldid -e`) in the log, and **fails** (before signing and again on the signed binary) if any entitlement key matches `memory`, `jetsam` or `extended-virtual-addressing` (for example `com.apple.developer.kernel.increased-memory-limit` or `com.apple.developer.kernel.extended-virtual-addressing`). The pattern is `MEMORY_ENTITLEMENT_REGEX` at the top of `build.yml`.

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
3. If `BGFX_SHADER_BIN_VERSION` changed (it is 11 now), regenerate `Rendering/Shaders/generated/*.h` with the `shaderc` of the same bgfx commit.

Bump `BGFX_RECIPE_VERSION` to throw away the cached libraries without changing the SHAs. Always bump it when you change `runs-on` or `XCODE_APP` too: they are not part of the cache key (it holds the runner OS and architecture, the three SHAs and `BGFX_RECIPE_VERSION`). The libraries are built with Xcode 16.4 on `macos-15`; the generated bgfx makefiles hard-code clang flags that were only verified with that toolchain.

## Unit tests locally

The tests build the platform-independent `Core/` with CMake (C++17). Run them from the repository root.

macOS or Linux:

```sh
cmake -S Config/Tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```

Windows (MSVC), from an "x64 Native Tools Command Prompt for VS" (or after running `vcvars64.bat`):

```bat
cmake -S Config\Tests -B build\tests -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build\tests --parallel
ctest --test-dir build\tests --output-on-failure
```

Windows downloads zlib automatically (needs git and network). Add `-DOST_TEST_BOOLEAN=ON` to the first command to build the real Manifold boolean operations (downloads Manifold v3.0.0). More in `Config/Tests/README.md`.

## Build on a Mac by hand

`brew install xcodegen`, put the four bgfx libraries in `Libs/bgfx/bgfx/.build/ios-arm64/bin/` (take them from a workflow run: the cache, or run the bgfx steps yourself), `xcodegen generate`, then open `OSTVisualizer.xcodeproj` or run the same `xcodebuild` command as the workflow. The generated project is not committed.

## Things to know

- Minimum iOS is 14.8 (as in `Config/Project.plist`), but bgfx's build scripts hard-code `-mios-version-min=16.0` and `libmanifold.a` is also built for 16.0. The linker prints "built for newer iOS version" warnings. Running on iOS below 16 is untested.
- The app is compiled with the iPhoneOS 18.5 SDK (Xcode 16.4). The Nyxian build links against the SDK on the device (26.4). On iOS 26 an app linked against an older SDK typically keeps the older system look, so the UI chrome can differ from the Nyxian build (untested).
- **Nyxian and the tests.** Nyxian compiles every `.c`, `.cpp`, `.m`, `.mm` and `.swift` file under the project folder except the top-level `Config/` and `Resources/` folders (Nyxian's public source, `NXPhaseEngine.m`; the installed version was not checked). The host unit tests contain their own `main()` and a boolean-operations stub, so they live in `Config/Tests/`, where Nyxian does not look. Do not move them out of `Config/`, or an on-device Nyxian build fails with duplicate symbols. The CI build is not affected: `project.yml` lists the app folders explicitly.
