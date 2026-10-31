# Contributing to Halo CE: Arena Evolved

Arena Evolved is built on [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce), a community build of [OpenCE](https://github.com/OpenCommunityEdition/OpenCE). Fixes that belong in OpenCE or ChupathingyCE themselves are best sent there; we merge their releases regularly.

## Pull requests

- **Open them against `PeterRichardsonGolf/Halo-CE-Arena-Evolved`'s `main`.**
- One change per pull request, with how you tested it (which platforms you built and ran).
- The game protocol stays OpenCE's: don't change how games are joined or what's sent on the wire. New networked features go on ChupathingyCE's own channel (Delta, `docs/delta.md`), falling back silently when the other side is OpenCE.
- The 32-bit builds must stay byte-identical unless the change is meant for them: `tools/port_neutrality_check.py` checks it.
- Commit messages, pull requests and issues are public.

## Using AI tools

You can use AI tools when you contribute. You're responsible for what you submit: understand every line, test it, and be able to explain it in review. Say in the pull request if a large part of it was generated. Pull requests that are mostly generated and untested will be closed.

## Versions

- **`0.MINOR.0`** is a major release, with a theme.
- **`0.MINOR.PATCH`** is a regular release, with features and fixes.
- **A letter after it** is a hotfix of that release: crash, security or network fixes only, no features. Letters start at `a`, in order: 0.7.5, then 0.7.5a, 0.7.5b.
- **1.0.0** comes once the game is feature-complete and stable on every platform.

Test builds are the `nightly` release and `-testing` tags, outside the numbering. The network version is separate from the release version: a hotfix changes it only when cross-play needs it.

## Bugs

Use [the bug report form](https://github.com/PeterRichardsonGolf/Halo-CE-Arena-Evolved/issues/new?template=bug.yml). A bug that OpenCE has too is best reported to [OpenCE](https://github.com/OpenCommunityEdition/OpenCE/issues).

## Game files

Arena Evolved never provides game files. Don't commit, attach or link maps, disc images or other copyrighted game data.
