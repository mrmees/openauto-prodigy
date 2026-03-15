# Phase 1: Proto Foundation & Observability - Context

**Gathered:** 2026-03-04
**Status:** Ready for planning

<domain>
## Phase Boundary

Update open-android-auto submodule from current (bdbb3f3) to v1.0 tag (1cd8919) and add debug logging for all channel messages that Prodigy currently ignores silently. No new feature handling — just make the invisible visible.

</domain>

<decisions>
## Implementation Decisions

### Submodule Update
- Update `libs/open-androidauto/proto` from bdbb3f3 to v1.0 tag
- Review v1.0 changelog/diff FIRST before updating — understand what changed before fixing anything
- v1.0 delta is ~20 commits: mostly docs, validation tests, schema corrections — expected to be additive
- Fix any compilation breaks from proto renames or new fields
- Submodule is read-only — no changes to proto files in this repo

### Unhandled Message Logging
- Purpose: discover what we don't know — "here's a message we're not handling" breadcrumbs, not explanations
- Log format should invite exploration: message ID, channel, raw payload — enough to go look it up, not a decoded breakdown
- Use existing `qCDebug(lcAA)` pattern with Qt logging categories
- Default to debug level (silent unless user enables verbose AA logging)
- Follow existing logging conventions — no new logging categories needed for this phase

### Regression Safety
- All 47 existing tests must pass after submodule update
- Phone connection test on Pi confirms no protocol regression (video, touch, audio)
- Both automated and manual verification required

### Claude's Discretion
- Whether to add a catch-all handler vs per-channel default cases
- Exact wording of log lines (within the "invite exploration" constraint)

</decisions>

<specifics>
## Specific Ideas

- Logging is about knowing what we don't know — the messages should make you want to go dig, not tell you everything upfront
- "Here's something we're ignoring, go explore" over "here's what this message means"

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `qCDebug(lcAA)` / `qCWarning(lcAA)` — established AA logging pattern used throughout `src/core/aa/`
- `oaa::hu::StubChannelHandler` — existing stub handler in open-androidauto, may serve as catch-all template
- `oaa::ProtocolLogger` — already captures protocol traffic to file (jsonl/tsv), separate from this debug logging

### Established Patterns
- Channel handlers instantiated in `AndroidAutoOrchestrator` (video, audio x3, input, sensor, bt, wifi, av_input, nav, media_status, phone_status)
- Qt logging categories with `QLoggingCategory` — verbose control via CLI/config/web panel
- All channel handler types come from `oaa::hu::Handlers/` in the open-androidauto library

### Integration Points
- `AndroidAutoOrchestrator.hpp` — where channel handlers are declared and wired to AASession
- `AndroidAutoOrchestrator.cpp` — session setup, handler registration
- `libs/open-androidauto/proto/` — submodule pointer update
- `CMakeLists.txt` — may need adjustments if v1.0 adds new source files

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-proto-foundation-observability*
*Context gathered: 2026-03-04*
