# AstroFrame presentation layer

This Windows x64 Vulkan layer preserves the aspect ratio of the emulator's final
full-surface `vkCmdBlitImage` transfer. It clears the unused area to black and
orders that clear before the centered blit. It does not change the game's internal
resolution, frame-rate limit, lighting, HDR support or window mode.

The layer is enabled only for a process that explicitly loads
`VK_LAYER_ASTRO_frame`. It does not install a global driver or registry entry.

## Build and CPU tests

Use an x64 Visual Studio developer shell with clang-cl, Ninja and CMake 3.22 or
newer. Only Vulkan headers are needed for the default build; neither a Vulkan
loader nor SDL is linked into the layer or CPU tests.

```powershell
git submodule update --init --depth 1 3rdparty/Vulkan-Headers
cmake -S tools/astro-bot/frame-layer -B _Build/astro-frame -G Ninja `
  -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
cmake --build _Build/astro-frame --parallel 1
ctest --test-dir _Build/astro-frame --output-on-failure -L cpu
cmake --install _Build/astro-frame --prefix _Build/astro-frame-install
```

Header discovery also accepts a Vulkan SDK through `VULKAN_SDK`, or
`-DASTRO_VULKAN_HEADERS=<Vulkan-Headers checkout or include directory>`.
`ASTRO_FRAME_BUILD_CPU_TESTS` defaults to `ON`. The CPU tests use mocked dispatch
functions and synthetic module headers; they never open a window or submit GPU
work. No game shader, save, executable or cache is included in these fixtures.

## Optional GPU smoke test

`ASTRO_FRAME_BUILD_GPU_TESTS=ON` additionally requires SDL3 and a Vulkan loader.
Set `SDL3_DIR` if the SDL3 CMake package is not in the normal search path. This
option defaults to `OFF`, and the hosted workflow runs CPU tests only.

The optional test creates hidden presentation windows, checks every RGB pixel
against expected bars and content, and is labeled `gpu` in CTest. Running it needs
a local Vulkan GPU. Vulkan validation and synchronization checks can be requested
by launching `frame_smoke.exe --validation` with the layer environment configured
and the Khronos validation layer installed.

## Process configuration

| Variable | Behavior |
| --- | --- |
| `ASTRO_FRAME_ASPECT=16:9` | Fits the final transfer into a 16:9 rectangle. Otherwise the source image's ratio is used. |
| `ASTRO_FRAME_INFO=<path>` | Optional presentation and shader-substitution log. |
| `ASTRO_FRAME_LOG=<path>` | Optional CPU presentation-interval CSV, not a GPU performance benchmark. |
| `ASTRO_FRAME_RENDER_DIAG=1` | Optional render-area diagnostics. Disabled by default. |
| `ASTRO_FRAME_SHADER_DIR=<directory>` | Optional exact-input SHA256 shader substitutions. Disabled when unset. |

Shader substitutions require `<input-sha256>.spv` and an adjacent
`<input-sha256>.sha256` containing the output payload's SHA256. These integrity
guards do not establish semantic correctness or performance. No replacement
shaders or optimization candidates are distributed here. Module-creation failure
falls back to the original module, except for device loss; later pipeline or
runtime failures are outside that fallback.

The corrected layer has CPU regression coverage. GPU validation of an earlier
revision does not validate a newly compiled revision. No frame-rate improvement
is inferred from these tests.
