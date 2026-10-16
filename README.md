# KytyPS5 Astro Bot preview

![Status: work in progress](https://img.shields.io/badge/status-work_in_progress-orange)
![Astro Bot: in game](https://img.shields.io/badge/Astro_Bot-in_game-yellow)
[![License: GPL-2.0](https://img.shields.io/badge/license-GPL--2.0-blue)](LICENSE)

An unofficial experimental fork for **Astro Bot (PPSA21564, v01.007.000)**, based on
[KytyPS5](https://github.com/KytyPS5/KytyPS5) and
[Jetsku's U59 int19.1 work](https://github.com/Jetsku/KytyPS5/tree/28bc20f146a276479acf42d2e405fb46a4491b8e).
It reaches controllable gameplay on the tested Windows PC. **Stable 60 FPS, 120 FPS,
full game completion, native HDR and full DualSense haptics are not established.**

This is an AI-assisted preview; [scope, automated checks and human gameplay reports](docs/astro-bot/CREDITS.md#assistance-disclosure) are documented separately. Full human source review remains pending before upstream submission.

The current report is encouraging but incomplete: busy views can fall to approximately
20 FPS with slow motion, while looking at the sky or simpler scenery performs much
better. First-time shader compilation can also cause pauses. This is a testing preview,
not a claim that Astro Bot is fully playable.

## Try the preview

1. Download the **Windows testing kit** from [Releases](https://github.com/mauroalvare/KytyPS5-AstroBot/releases).
2. Extract it into a new writable directory. Do not extract it over your usual emulator installation.
3. Run `Setup-AstroBot.ps1` as explained in the [English setup and usage guide](tools/astro-bot/usage.md).
4. Start with the **Stable** profile and a legally obtained, decrypted game directory containing `eboot.bin`.
5. Report the exact kit version, profile, hardware and result using the fork's [Issues](https://github.com/mauroalvare/KytyPS5-AstroBot/issues).

The kit contains tools, version-locked patch descriptions and a presentation layer.
Its setup downloads the checksum-pinned **original Jetsku U59 PGO release**; it is not
advertised as a fresh PGO build of this fork. An optional, guarded compatibility patch
for conventional rumble is documented in the kit. No game, firmware, save, shader
cache or extracted game shader is included.

## What is here

- Source changes for selecting the startup monitor without moving a visible window first.
- An opt-in PCM-to-conventional-rumble fallback for a DualSense whose USB audio endpoint
  accepts haptic samples without producing felt vibration. Native fine haptics remain
  the source default. This fallback is approximate and still needs wider physical testing.
- A Windows, per-process Vulkan presentation layer that preserves aspect ratio and adds
  black bars. It does not change global Vulkan registration or implement HDR.
- Astro Bot profiles for the non-RT renderer, experimental 1440p/ultrawide, 90/120 FPS
  targets and deferred GI. Targets do not guarantee achieved FPS or correct game speed.
- Reproducible setup, launch and packaging tools, CPU regression checks, and testing notes.

Optional optimized game shaders are **not shipped or enabled**. Static instruction
reductions alone were not treated as evidence of a performance improvement.

## Documentation

- [Setup, controls, profiles and troubleshooting](tools/astro-bot/usage.md)
- [Build from source and run checks](docs/astro-bot/BUILDING.md)
- [What has actually been tested](docs/astro-bot/TESTING.md)
- [Roadmap and known limitations](docs/astro-bot/ROADMAP.md)
- [Credits and exact provenance](docs/astro-bot/CREDITS.md)
- [Contributing and upstream rules](CONTRIBUTING.md)
- [Ready-to-share English announcement](docs/astro-bot/ANNOUNCEMENT.md)
- [Preserved U59 README](docs/astro-bot/BASE-README.md)

This fork is not affiliated with Sony Interactive Entertainment or PlayStation.
It follows the upstream policy of distributing no games or copyrighted system software.
Use only game files you have obtained legally. The original license and dependency
notices are preserved.
