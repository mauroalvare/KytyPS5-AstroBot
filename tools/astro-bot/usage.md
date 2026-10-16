# Astro Bot experimental test kit

This kit is an overlay for Windows x64. It includes profiles, launch scripts and a
per-process Vulkan layer. It includes no emulator executable, game, save, shader
cache or controller/audio recording. Supply your own game files.

The default **Stable** mode keeps the game's native/dynamic rendering resolution,
uses the existing non-RT renderer, disables GI and requests 60 Hz virtual vblank.
It is a conservative baseline, not a guarantee of crash-free or constant 60 FPS.
The frame layer preserves aspect ratio in fullscreen. Windows HDR being enabled
does not make this SDR renderer native HDR.

## Install the release overlay

Extract the overlay ZIP to a dedicated folder. Open Windows PowerShell there:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Setup-AstroBot.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Start-AstroBot.ps1 -GamePath 'D:\Games\AstroBot'
```

The example folder is illustrative. It must contain `eboot.bin` for PPSA21564
01.007.000 with SHA256
`3b5100797fe83663e18a650f82f901d066acf1029ae80cfb1be638fe0839debd`.
Other versions are rejected. A successful launch remembers the folder only in
`.local/settings.json`; later launches can use `Play-AstroBot.cmd`.

Setup downloads the exact [Jetsku U59 int19.1 release](https://github.com/Jetsku/KytyPS5/releases/tag/u59-windows-20261013-int19.1)
and checks its ZIP SHA256 before extracting into `runtime/`.
This is the existing upstream PGO executable, **not a rebuilt PGO binary from this
fork**. `provenance.json` pins the URL, ZIP and executable hashes and the exact
[source base](https://github.com/Jetsku/KytyPS5/tree/28bc20f146a276479acf42d2e405fb46a4491b8e).
No global Vulkan layer registration, drivers or Windows settings are changed.

To reuse a previously downloaded matching archive:

```powershell
.\Setup-AstroBot.ps1 -ArchivePath 'D:\Downloads\KytyPS5-U59-Windows-x64.zip'
```

## Preview without starting anything

```powershell
.\Start-AstroBot.ps1 -GamePath 'D:\Games\AstroBot' -Mode 1440p -PrepareOnly
```

`-PrepareOnly` checks the files and prints JSON. It does not change process
environment, save configuration, create directories/logs or launch the emulator.

## Optional experiments

```powershell
.\Start-AstroBot.ps1 -Mode 1440p
.\Start-AstroBot.ps1 -Mode 90
.\Start-AstroBot.ps1 -Mode 120
.\Start-AstroBot.ps1 -Mode Ultrawide
.\Start-AstroBot.ps1 -Mode DeferredGI
.\Start-AstroBot.ps1 -Mode Stable -Windowed
.\Start-AstroBot.ps1 -Diagnostics
```

1440p requests fixed 2560x1440 rendering. Ultrawide requests 3440x1440, but its
projection/HUD correctness still needs testing. DeferredGI retains GI with the
non-tiled renderer and can be expensive. The 90/120 profiles also change the guest
time conversion and use matching emulator vblank values; they do not guarantee
those FPS, correct game speed or synchronized audio. Performance varies by scene.
Resolution and lighting changes can compile additional pipelines and cause pauses.
No optimized shader replacements are enabled by this kit.

## Controls and troubleshooting

Click inside the game window before using keyboard input. `J` is PlayStation
Cross, `L` Circle, `K` Square, `I` Triangle, `Enter` Options. `WASD` controls the
left stick and `F/H/T/G` the right stick. Use these if a connected controller does
not advance the title screen.

USB DualSense buttons, sticks, gyro and adaptive triggers can work while fine
haptics remain absent; support for one feature does not prove the others work.
Try native routing first, then the optional conventional rumble fallback below.
It does not reproduce the original fine audio-driven haptics.

If setup or launch reports a SHA/version mismatch, use the exact pinned upstream
archive and the required PPSA21564 01.007.000 game version. Do not disable the hash
guards or use a different game executable with these profiles. Setup can reuse a
matching ZIP via `-ArchivePath`.

New areas and effects can compile shaders/pipelines, causing first-visit pauses.
Heavy scenes can still run near 20 FPS even when a simpler sky view is fast;
fixed 1440p and additional GI can cost more. Return to `-Mode Stable` to compare
with the native/dynamic baseline. Memory-protection settings retain the verified
preset; unrepeatable memory-tuning gains were not promoted into defaults.

Use `-Diagnostics` for presentation/audio routing details and inspect
`.local/logs/<run-id>/startup.txt`, `errors.txt`, `presentation.txt` and `frames.csv`,
plus `.local/last-run.json`. Logs remain local and may contain private paths.
Review them before posting. If startup ends before all mods are confirmed, use
the patch status and error log rather than assuming the requested mode applied.

## Optional conventional vibration fallback

Use native DualSense routing first. If inputs/gyro/triggers work but USB fine
haptics are absent, the separate fallback can be prepared explicitly:

```powershell
.\Setup-AstroBot.ps1 -IncludeRumbleFallback
.\Start-AstroBot.ps1 -Vibration Compatible
```

`Patch-RumbleFallback.ps1` preserves the original executable and checks its complete
SHA256 plus instruction bytes before changing exactly six bytes in a separate
copy. It selects the release's existing PCM peak-envelope conventional rumble
path for vibration ports and preserves the speaker route. Its source implementation
is in this fork's `src/libs/dualSenseHaptics.cpp`. This is not native audio-driven
fine haptics; physical behavior still needs testing on each controller/system.

## Use a source-built emulator

Build the emulator using the repository's normal clang-cl/Qt/SDK requirements and
keep its matching runtime DLLs next to that executable. In a release overlay:

```powershell
.\Start-AstroBot.ps1 -GamePath 'D:\Games\AstroBot' -SourceExecutable 'D:\Build\kyty_emulator.exe' -Vibration Compatible
```

A source build uses the `KYTY_DUALSENSE_HAPTICS=rumble` flag instead of the six-byte
PGO patch. The launcher records it as a user-supplied build and does not claim it
matches the pinned PGO release. `-Display` is available only for source builds with
`KYTY_DISPLAY` support; use an exact SDL monitor name or its one-based list index.
No hardcoded monitor-ID binary patch is included.

From a source checkout, build just the layer without downloading the upstream
runtime, using an x64 Visual Studio developer shell with clang-cl, Ninja,
CMake 3.22+ and Vulkan headers available:

```powershell
.\tools\astro-bot\Setup-AstroBot.ps1 -SkipRuntime -VulkanHeaders 'D:\SDKs\Vulkan-Headers'
```

The layer requires Vulkan headers, not SDL or the Vulkan loader for CPU builds.
GPU tests are disabled during setup. A source checkout can also use normal setup
to download the pinned upstream runtime.

## Local saves and diagnostics

Every installed kit uses its own `runtime/_SaveData/`. Setup never imports saves
from another installation and never extracts user data from the release archive.
Back up saves before manually transferring your own files between installations.
`runtime/_PipelineCache/` and other runtime caches remain local.
Configuration, startup/error logs and `last-run.json` are in `.local/`. Patch
verification is pending until every enabled mod appears in the runtime's success
log; a requested resolution/FPS value alone is not proof that it applied.
Diagnostic logs can contain local paths. Review them before sharing; no files are
uploaded automatically. All local state and runtime files are gitignored.

## Build an overlay ZIP

From a source checkout, after compiling/installing the frame layer:

```powershell
.\tools\astro-bot\Package-AstroBot.ps1 -OutputDirectory 'D:\Build\astro-overlay' -LayerInstallDirectory 'D:\Build\layer-install' -ArchivePath 'D:\Build\astro-overlay.zip' -SourceCommit (git rev-parse HEAD)
```

The output directory must be empty or new. The packager uses an explicit whitelist
and adds file hashes in `overlay-info.json`. The optional `-SourceCommit` accepts
exactly 40 hexadecimal characters and records the fork source revision; omit it
only when a source revision is unavailable. It excludes emulator executables,
game data, configuration, logs, saves, caches, tests and source-build directories.
The root LICENSE is included as `LICENSE.txt`. Setup reuses the upstream release's
runtime and accompanying notices after the user downloads it separately.
