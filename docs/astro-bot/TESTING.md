# Testing evidence

These results describe limited scenes on one PC, not full game compatibility.

## Test machine and game

- Windows x64, Ryzen 7 9700X, GeForce RTX 5070 Ti 16 GB, 32 GB RAM.
- Astro Bot PPSA21564 v01.007.000, exact supported executable hash recorded in the profiles.
- DualSense over USB; gyro and adaptive triggers reported working.

The game path, logs containing local paths, saves and translated shader caches are not
published. Testers should review their own logs before attaching them publicly.

## Observed in game

- The U59 PGO base with the non-RT profile reaches controllable gameplay.
- A user run of the experimental 1440p profile logged all three expected modifications
  and ended with exit code 0. This proves the loader reported applying the profile;
  it is not an independent measurement of every render attachment's resolution.
- Busy views can drop to approximately **20 FPS**, with the game visibly slowing down.
  Looking at the sky or sparse scenery performs much better. This latest report is
  observational, without a matching new PresentMon capture.
- Gyro and adaptive triggers work on the tested DualSense. Native fine vibration has
  not been confirmed. The compatible rumble route still needs physical confirmation.

## Earlier PresentMon captures

Eight captures used the same stationary starting view in Sky Garden, before the new
fixed-resolution profiles. They are not traversal benchmarks.

| Case | Average FPS | 1% low FPS | Worst interval |
| --- | ---: | ---: | ---: |
| First visit | 33.89 | 3.77 | 916.18 ms |
| Warm control | 42.84 | 24.58 | 58.82 ms |
| Combined memory experiment, first capture | 46.32 | 28.58 | 48.53 ms |
| Same experiment, repeat | 43.11 | 25.83 | 71.51 ms |

The apparent initial gain did not repeat. No sustained win is claimed for that memory
experiment, and it was not promoted into the kit default. First-visit logs recorded
32 new pipelines and 6860.5 ms of aggregate draw wait. Warm GPU-active time was around
21 ms per frame; a 120 FPS budget is only 8.33 ms. Raising a target cannot remove that work.

1% low here is `1000 / mean(slowest ceil(1% of frame intervals))`. All nine saved CSVs
(eight gameplay captures and one title capture) were checked to contain a single
presentation chain. Raw private captures are not bundled.

## Offline checks completed before sharing

- 64 CPU-model control-flow/dimension checks for guest profiles, including foreground
  and background behavior and rate-field consistency.
- 12 native CPU arithmetic checks for the original timing leaf at 60/90/120.
- 9 simulated x64 checks for the guarded legacy PGO rumble path.
- 30 presentation-layer CPU checks plus 16 later regressions.
- 10 shader interface/validation unit tests; no game shader payload is distributed.
- 24 Windows PowerShell 5.1 lab checks and 7 frame-analysis tests.
- 553 unique cached SPIR-V modules validated before and after the existing host shrink
  transform. The cache itself is not distributed; this was not a new speed improvement.
- Monitor and haptics source translation units compile with clang-cl 19.1.5.

The public portable layer independently passed 3 CPU suites (49 synthetic checks).
The public kit passed 30 standalone CPU/file checks and 33 with real, verified offline
archive extraction and the guarded rumble copy; no executable was started.

Some investigation checks need a private game dump and are therefore evidence from the
local investigation, not all self-contained tests in this public checkout. Public kit
and presentation tests are documented separately in the build guide.

The earlier presentation-layer revision passed four GPU pixel/readback and Vulkan
synchronization tests. The current revision has newer guards and CPU checks and still
needs those GPU tests repeated. No linked full source build, Linux runtime, complete
playthrough, stable 60 FPS, correct 90/120 game speed or native HDR is claimed.

## Repeating a useful performance test

1. Record OS, CPU, GPU/driver, RAM/VRAM, game version/hash, exact commit/kit and profile.
2. Keep the same save, camera, route and graphics configuration.
3. Separate first-visit compilation/loading from a repeated warm visit.
4. Capture both a busy view and a sparse/sky view; record CPU and GPU timing as available.
5. Compare a control and one change at a time, then repeat the control.
6. Report average, 1% low, p99 interval, longest pause, crashes and visible game speed.
7. Check audio, HUD, cutscenes, resolution, aspect ratio and controller feedback as well.

Use the fork's issue tracker for preview results. For an upstream report, first reproduce
on an unmodified upstream build and follow its complete-log/status-report template.
