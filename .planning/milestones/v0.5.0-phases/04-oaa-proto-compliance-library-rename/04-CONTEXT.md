# Phase 4: OAA Proto Compliance & Library Rename - Context

**Gathered:** 2026-03-07
**Status:** Ready for planning

<domain>
## Phase Boundary

Clean up after the proto v1.2 migration: remove dead code from retracted protos, verify WiFi field semantics, rename the in-tree protocol library from `open-androidauto` to `prodigy-oaa-protocol`, update docs to reflect v1.2 reality, and get all tests green with coverage for new v1.2 code paths. No new protocol features — this is housekeeping and alignment.

</domain>

<decisions>
## Implementation Decisions

### Dead Code Cleanup
- Full removal — if it's not called, it's gone. No stubs, no "RETRACTED" comments
- Remove all dead signals, slots, orchestrator connections, and code paths that only existed for retracted protos
- Specifically remove: AudioFocusState handler stubs, AudioStreamType handler stubs, NavigationFocusIndication handler stub, any lingering MediaPlaybackCommand references
- Remove MessageIds.hpp constants for retracted messages — proto docs in open-android-auto are the reference
- Verify and fix WiFi ssid/bssid semantics (v1.2 renamed field 1 to `bssid` — researcher confirms what the phone actually expects and code gets corrected)
- Update ROADMAP.md and REQUIREMENTS.md to reflect v1.2 reality (retracted features no longer listed as shipped capabilities)

### Library Rename
- Rename `libs/open-androidauto/` to `libs/prodigy-oaa-protocol/`
- Rename CMake target from `open-androidauto` to `prodigy-oaa-protocol`
- Keep C++ namespace as `oaa::` — it's short, established, and still accurate
- Proto submodule stays nested: `libs/prodigy-oaa-protocol/proto/` (moves with the rename)
- Update all CMakeLists.txt references, include paths, and documentation

### Retracted Handler Strategy
- Remove entirely — delete handler cases, signals, state tracking for retracted protos
- Generic unhandled-message logger from Phase 1 catches any surprise traffic on those IDs
- Keep debug panel buttons for Assist(219) and Voice(231) keycodes — these are the v1.2-correct approach to media/voice control and need Pi verification (separate phase)

### Test Alignment
- All tests must be green (100% pass rate) at phase completion
- Fix the failing navbar controller test
- Remove tests that reference retracted APIs/protos
- Add new tests covering v1.2 code paths (keycode-based commands, renamed ACK fields, bssid semantics)
- Quality over quantity — meaningful coverage, not padding numbers

### Claude's Discretion
- Exact ordering of cleanup vs rename operations (whichever minimizes merge conflicts)
- Whether to batch all dead code removal into one plan or split by subsystem
- Test implementation details (mock strategies, assertion patterns)

</decisions>

<specifics>
## Specific Ideas

- "prodigy-oaa-protocol" chosen because it's descriptive: Prodigy's protocol layer, built on oaa (open-android-auto) heritage
- Debug buttons stay because we haven't confirmed keycodes 219/231 actually trigger phone responses — that verification is a separate phase
- WiFi ssid/bssid is a correctness issue, not just naming — the phone may expect a MAC address (BSSID) in field 1, not a network name (SSID)

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- Phase 1 unhandled-message logger: catches any message IDs not explicitly handled — serves as safety net after removing retracted handlers
- `oaa_add_test()` CMake helper: streamlined test addition pattern from build optimization work
- Existing test suite (64 tests) covers all handler types — provides patterns for new v1.2 tests

### Established Patterns
- Handler `onMessage()` switch on messageId -> parse protobuf -> emit Qt signal (Phases 1-3)
- Outbound: build protobuf -> serialize -> emit sendRequested (Phase 3)
- `QByteArray::fromRawData()` for zero-copy payload views
- Qt logging categories via `qCDebug(lcAA)` for AA protocol messages

### Integration Points
- `libs/open-androidauto/` -> `libs/prodigy-oaa-protocol/` — directory rename affects git submodule path, CMakeLists.txt, all documentation
- `AudioChannelHandler.cpp` — remove retracted AudioFocusState/AudioStreamType stubs
- `NavigationChannelHandler.cpp` — remove retracted NavigationFocusIndication stub
- `AndroidAutoOrchestrator.cpp` — remove dead signal connections
- `MessageIds.hpp` — remove retracted message ID constants
- `ServiceDiscoveryBuilder.cpp` — verify bssid field usage
- `.planning/ROADMAP.md` and `.planning/REQUIREMENTS.md` — update to reflect v1.2 reality
- `CLAUDE.md` — update library references after rename

</code_context>

<deferred>
## Deferred Ideas

- Verify keycode-based media/voice control on Pi (keycodes 219/231 → phone response) — separate phase
- Functional validation of v1.2 protocol behavior end-to-end — separate testing phase

</deferred>

---

*Phase: 04-oaa-proto-compliance-library-rename*
*Context gathered: 2026-03-07*
