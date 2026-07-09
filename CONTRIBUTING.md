# Contributing to OpenAuto Prodigy

Thanks for your interest — contributions, bug reports, and testing on real hardware are all welcome.

## Filing issues

Use the issue templates (bug report / feature request). For bugs, logs from the Pi help enormously:

```bash
journalctl -u openauto-prodigy.service -n 200
```

## Development setup

See [docs/development.md](docs/development.md) for the platform setup (Qt 6.8, system packages, cross-compiling for the Pi).

**Read [AGENTS.md](AGENTS.md) before contributing** — humans and AI agents alike. It holds the repo's hard constraints (frozen API contract, submodule boundaries, protocol rules), the build/test/deploy commands, and the workflow conventions. Subsystem-specific rules live in nested `AGENTS.md` files next to the code.

## Branch flow

- Work lands on `dev`; PRs go to `main`.
- One logical change per PR; fill in the PR template.

## Before opening a PR

- Local build passes, including the app target (`cmake --build . --target openauto-prodigy`).
- `ctest --output-on-failure` is green.
- For Pi-affecting changes: cross-build (`./cross-build.sh`) and, if you have hardware, verify on a Pi.

## Style

- Match the surrounding code — naming, comment density, idiom.
- No new production dependencies without justification in the PR.
- The proto submodule (`libs/prodigy-oaa-protocol/proto/`) is hands-off — protocol definition changes go to [open-android-auto](https://github.com/mrmees/open-android-auto).

## Where to talk

GitHub issues are the venue for questions, ideas, and discussion.
