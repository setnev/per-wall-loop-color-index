# Windows community release

The `wall-loop-v0.1.0` release packages the Windows x64 build tested by the project
owner. It adds Per Wall Loop Color Index to Snapmaker Orca 2.4.1. It is a community
preview distributed by this project; it is unsigned.

Extract the complete portable ZIP to a writable folder and run
`snapmaker-orca.exe`. Keep the DLLs, `mesa`, and `resources` beside the executable.
The feature is in Advanced mode under **Multimaterial → Filament for Features**.
See [usage and limits](PerWallLoopColorIndex.md).

The application still reports the upstream 2.4.1 version. The included
`RELEASE-INFO.json` identifies this feature release and its source commit.
Download community updates from this repository's Releases page; upstream
downloads do not include this feature.

## Source and licenses

The release includes `LICENSE.txt`, third-party notices, a source reference, and
SHA-256 checksums. Source archives and the release tag are published beside the
binary at https://github.com/setnev/per-wall-loop-color-index/releases.
The upstream AGPL-3.0 license and source attributions are retained; dependency
notices retain their own license texts.

## Building from source

Use a fresh checkout and out-of-source build directories. Install Git, Visual
Studio 2022 Build Tools with Desktop development with C++, a Windows SDK, Perl,
and CMake 3.x. The packaged build used MSVC 19.44, SDK 10.0.26100.0, Strawberry
Perl 5.42.3.1, and CMake 3.28.4. Put CMake before Strawberry Perl's `c/bin` on PATH.

From a VS 2022 x64 developer command prompt at the checkout root:

```bat
cmake -S deps -B deps/build -G "Visual Studio 17 2022" -A x64 -DDESTDIR="%CD%/deps/build/OrcaSlicer_dep" -DCMAKE_BUILD_TYPE=Release -DDEP_DEBUG=OFF
cmake --build deps/build --config Release --target deps --parallel 2
cmake -S . -B build_release -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DBBL_RELEASE_TO_PUBLIC=1 -DORCA_TOOLS=OFF -DBUILD_TESTS=ON -DCMAKE_PREFIX_PATH="%CD%/deps/build/OrcaSlicer_dep/usr/local" -DOPENSSL_ROOT_DIR="%CD%/deps/build/OrcaSlicer_dep/usr/local" -DCMAKE_INSTALL_PREFIX="%CD%/build_release/Snapmaker_Orca" -DWIN10SDK_PATH="%WindowsSdkDir%Include/%WindowsSDKVersion%"
cmake --build build_release --config Release --target ALL_BUILD --parallel 2
call scripts\run_gettext.bat
cmake --install build_release --config Release
```

Dependencies require network access during their first build. Their download
locations and patches are in `deps/`; vendored sources are in `deps_src/`.
The upstream `build_release_vs2022.bat` is another supported build entry point.

The first preview uses a Release-optimized slicing engine and a GUI built with
optimization disabled to fit the development machine's memory. To reproduce that
GUI setting, set the Release Optimization property on `libslic3r_gui` to Disabled
in the generated Visual Studio solution. An ordinary Release build may keep the
default optimization. Dependencies that follow a moving Git branch can differ
on subsequent builds; this is not a claim of bit-for-bit reproducibility.

Run the feature tests:

```bat
build_release\tests\libslic3r\Release\libslic3r_tests.exe [WallLoopFilaments]
build_release\tests\fff_print\Release\fff_print_tests.exe [WallLoopFilaments]
```

The first release passed 160 assertions in 14 test cases and a launcher `--help`
smoke check. The project owner confirmed separate wall colors and materials in
Snapmaker Orca. Physical printer validation details were not supplied.

## Packaging

Once the runtime has been installed, the tracked packager includes the license,
usage notes, dependency notices, and a manifest of every packaged file:

```bat
python scripts/package_wall_loop_release.py --runtime build_release/Snapmaker_Orca --output build_release/releases --tag wall-loop-v0.1.0
```

The first preview's runtime was assembled from the completed application binaries,
runtime DLLs, resources, the bundled Mesa fallback, and the MSVC redistributable
DLLs. This bypassed an install rule requesting an unused MQTT library variant.
It was then packaged using the same script. Generated compiler intermediates,
private settings, credentials, and developer tools are excluded from the release.
