# libs/prodigy-oaa-protocol/ — Protocol Library

- **`proto/` is a hands-off community submodule** ([open-android-auto](https://github.com/mrmees/open-android-auto)). If a proto change is needed, note it for that repo — never edit it here.
- Proto C++ generation happens from `proto/oaa/*.proto` at build time.
- The library is an in-tree static target with no separately installed ABI.
  Source-level signal or API changes update every repository call site in the
  same change; there is no independent library ABI version to bump.
- Protocol *behavior* gotchas (touch semantics, video quirks, socket handling) live in `src/core/aa/AGENTS.md` — read that before changing handler logic here.
