# Credits and provenance

## Base

- [KytyPS5/KytyPS5](https://github.com/KytyPS5/KytyPS5), based on
  [InoriRus/Kyty](https://github.com/InoriRus/Kyty).
- [Jetsku/KytyPS5](https://github.com/Jetsku/KytyPS5): U59 renderer, integration and PGO work.
- Exact source base: `28bc20f146a276479acf42d2e405fb46a4491b8e`, tag
  `u59-windows-20261013-int19.1`.
- Original U59 PGO executable SHA-256:
  `74e34635d3b8b426b20b9c2e0172aee87f76fede0c11001da959f1b8ecabab34`.

## Earlier fixes used during investigation

The initial Astro Bot startup and level-loading fixes came from existing contributors:

- LukaszRozmej: [upstream PR #1365](https://github.com/KytyPS5/KytyPS5/pull/1365), safe reads
  of unmapped guest memory during SRT materialization.
- Stepz97: [upstream PR #1470](https://github.com/KytyPS5/KytyPS5/pull/1470), Windows file
  read retry through a host buffer for protected guest pages.

U59 already addresses these failure classes. Those implementations were not blindly
cherry-picked again, and this fork does not claim authorship of them. All inherited
history, licenses and notices remain intact.

## Local contributions

Startup display selection, the opt-in haptics fallback, the external presentation tool,
Astro-specific version-locked profiles, launch/setup scripts, CPU checks and the test
report were assembled for this preview. They are separate from the U59 base and from
upstream fixes. Review each focused commit for its scope.

The testing kit deliberately retains an existing PGO executable for performance.
Source builds of this fork and the legacy PGO testing kit are distinct products: a
fresh build without the same training profile is not assumed to perform identically.
The optional binary rumble edit is reproducible, guarded and described by its script;
it is not represented as a source rebuild.

See the root `LICENSE` and inherited `LICENSES/` for license texts. The game and all
user-generated data are outside this repository and its release assets.

## Assistance disclosure

An AI coding assistant helped investigate failures, prepare the local changes, scripts,
documentation and automated checks. The human maintainer tested gameplay and reported
performance/controller behavior; those observations are listed in TESTING.md. Automated
reviews and compilation are not presented as a complete human source review. That review
and broader platform/runtime testing remain pending before any upstream submission.
