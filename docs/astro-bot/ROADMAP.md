# Work in progress

Current status: **In game on one Windows PC; performance and compatibility remain experimental.**

## Next measurements

- Reproduce the approximately 20 FPS busy-view slowdown versus sky/sparse views.
- Distinguish GPU shader/rendering cost from CPU submission, draw-call and memory costs.
- Improve sustained frame times and 1% lows before treating a higher FPS target as useful.
- Confirm fixed 1440p render attachments, image quality and correct HUD/cutscene behavior.
- Re-run GPU validation of the current aspect-preserving presentation layer.
- Physically test compatible vibration and its interaction with native vibration calls.
- Verify 90/120 targets for distinct frames, real-time game speed and audio synchronization.
- Verify ultrawide projection, field of view, HUD and cutscenes; a wider image alone is not support.
- Measure deferred GI, tessellation and occlusion separately before recommending them.
- Test new areas, level transitions, save/load and longer sessions for crashes.
- Validate source builds on Windows and Linux before proposing changes upstream.

## Explicit limits

- Shader optimization candidates are not shipped or enabled.
- Native HDR is not implemented in the current presentation path.
- USB fine haptics, compatible rumble, monitor placement and FPS profiles need broader testing.
- No whole-game stability or completion result is available.
- A source build without the U59 training profile is not assumed to match its PGO performance.

Small, reproducible reports and focused fixes are welcome. Keep unmeasured experiments
opt-in and retain the stable/dynamic-resolution control for comparisons.
