# Building and checking the preview

## Get the source

```powershell
git clone --recurse-submodules --branch astro-bot-experimental https://github.com/mauroalvare/KytyPS5-AstroBot.git
cd KytyPS5-AstroBot
python -m pip install pre-commit
python -m pre_commit install --install-hooks
```

The source branch starts at Jetsku U59 commit `28bc20f146a276479acf42d2e405fb46a4491b8e`.
Its submodules stay pinned. Do not replace them with unrelated latest versions.
The original license, upstream workflow and formatting configuration are preserved.

## Windows emulator build

Use Git, CMake 3.22.1 or newer, Ninja, Python 3, glslang on PATH, and clang-cl in an x64
Visual Studio developer environment. MSVC `cl.exe` is not supported. Qt 6 with Concurrent,
Network and Widgets is needed for the GUI launcher; match the installed MSVC x64 Qt build.

```powershell
git submodule update --init --recursive
cmake -S . -B _Build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build _Build/windows --target launcher --parallel 2
cmake --install _Build/windows --prefix _Build/windows/install
cmake --build _Build/windows --target kyty_tests --parallel 2
ctest --test-dir _Build/windows --output-on-failure
```

Replace the Qt placeholder with your installation. Configuration downloads some pinned
build dependencies. See the [preserved base guide](BASE-README.md) for Linux/macOS and
PGO details. Use the source launcher's `-SourceExecutable` option to test a binary you
built instead of pretending it is the downloaded PGO release.

A regular Release build does not recreate Jetsku's profile-guided build. Reproducing PGO
requires the matching compiler, training profile and source-path arrangement described
by the base. Do not claim identical performance without measuring it.

## Presentation tool only

This is a Windows tool; it does not make the emulator's existing Linux/macOS paths unsupported.
Follow `tools/astro-bot/frame-layer/README.md` for portable CMake options and CPU checks.
Vulkan headers are required. GPU smoke tests are optional and should run only when the
PC is available; CPU checks do not open a window or execute game/GPU code.

The kit's `Package-AstroBot.ps1` takes the built layer installation and creates a clean
ZIP overlay. It must include scripts, profiles, source/license references and the layer
only, excluding game files, saves, caches, private logs and emulator binaries. See the
[usage guide](../../tools/astro-bot/usage.md) for exact packaging/setup parameters.

## Before an upstream contribution

Use the pinned clang-format 22.1.3 pre-commit hook. Keep one concrete Astro Bot bug and
its regression evidence per PR. Shared code must be validated for Windows and Linux;
platform-only changes must build on the affected platform. A Windows translation-unit
compile is useful evidence but is not a complete cross-platform build.

The upstream `build.yml` remains unchanged and manual on this base. The separate
`astro-tools.yml` checks the tool overlay. CI results are evidence, not gameplay tests.
Do not open a broad upstream PR bundling monitor, haptics, profiles and presentation.
