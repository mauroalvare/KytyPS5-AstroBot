# Contributing to the Astro Bot preview

Follow the [original project's current contribution rules](https://github.com/KytyPS5/KytyPS5#contributing).
This fork does not override them.

- Search existing reports before opening another.
- Name the affected game, exact build, concrete problem and reproduction steps.
- Keep each fix focused. Upstream automatically closes general/refactoring PRs that do
  not address a concrete game bug or launch problem, and large unrelated bundles.
- Attach complete logs for compatibility reports, following the Game Emulation Status
  Report template. Review logs for personal information before sharing publicly.
- Reproduce a preview-only issue here. Retest with a clean upstream build before
  reporting it as an upstream regression.
- Use the existing clang-format hook: `python -m pre_commit install --install-hooks`.
  It is pinned to clang-format 22.1.3 and formats staged C++ files in `src`.
- Validate shared source on Windows and Linux. A platform-specific change needs to
  build on that platform; do not report unrun checks as passing.
- Keep experiments opt-in. State known limits, measured results and untested behavior.
- Preserve the original license, dependency notices and author attribution.
- Do not commit games, firmware, save data, translated/extracted game shaders, pipeline
  caches, local config, private paths or credentials.

The preview combines separate experiments for testing; that is not an upstream-ready
single PR. Propose independently reviewed fixes after each has sufficient evidence.
See [credits](docs/astro-bot/CREDITS.md) and [testing](docs/astro-bot/TESTING.md).

## AI assistance and human responsibility

The inherited upstream AI-use policy permits development assistance but requires
contributors to understand, review and test submitted code. Upstream repository
communication must come from the human contributor. AI-assisted PRs must disclose
what assistance was used and the human review/testing performed.

This preview's local code, tools and documentation were prepared with AI assistance.
Its CPU checks and assistant code reviews are automated evidence, not human source
review. The maintainer's gameplay observations are identified separately. Full human
source review has not been recorded, and no upstream PR or issue comment has been
submitted by this preview. Complete that review before proposing a change upstream.
Announcements in this checkout are drafts for the human maintainer to review and post.
