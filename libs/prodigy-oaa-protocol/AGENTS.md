# libs/prodigy-oaa-protocol/ — Protocol Library

- **`proto/` is a hands-off community submodule** ([open-android-auto](https://github.com/mrmees/open-android-auto)). If a proto change is needed, note it for that repo — never edit it here.
- Proto C++ generation happens from `proto/oaa/*.proto` at build time.
- Protocol *behavior* gotchas (touch semantics, video quirks, socket handling) live in `src/core/aa/AGENTS.md` — read that before changing handler logic here.
