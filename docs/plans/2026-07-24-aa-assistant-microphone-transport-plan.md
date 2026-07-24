# Android Auto Assistant Microphone Transport — Implementation Plan

Date: 2026-07-24
Status: ACTIVE
Design: `docs/plans/2026-07-24-aa-assistant-microphone-transport-design.md`
Base: `dev` at `975b3ef7c9e354d54e06cd90830068dbb30ec377`

## Execution Rules

- Execute in task order, one bounded task and one commit at a time. Do not push
  mid-plan.
- Every implementation task is `Tier: main`: this feature crosses AA protocol
  flow control, a PipeWire real-time callback, Qt socket affinity, and session
  teardown.
- Read root `AGENTS.md`, `src/AGENTS.md`, `src/core/aa/AGENTS.md`, and
  `libs/prodigy-oaa-protocol/AGENTS.md` before changing their owned files.
- Never edit `libs/prodigy-oaa-protocol/proto/`; preserve wireless-only AA,
  frozen API/numerics, HFP HF role, no-ofono, and External API rails.
- Tests ship with the behavior they pin. A worker owns its focused red/green
  loop and synthesized result; raw build logs stay out of the handoff.

## Task 1: Activate the promoted capability

**Tier:** main

**Files:**

- Add: `docs/plans/2026-07-24-aa-assistant-microphone-transport-design.md`
- Add: `docs/plans/2026-07-24-aa-assistant-microphone-transport-plan.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/wishlist.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`

**Acceptance criteria:**

- Both documents are ACTIVE and grounded on the same `dev` commit.
- Roadmap Now names the bounded microphone outcome and links both documents.
- The promoted item is removed from the unapproved wishlist; no other wishlist
  scope changes.
- Current-state research records the deferred 0/1 response semantics, AVInput
  acknowledgement evidence, real-time thread boundary, gain owner, and
  capture-first teardown order.
- Every implementation task names exact files, a test command, testable
  acceptance, a tier, and an out-of-scope boundary.

**Test command:**
`git diff --check && python3 scripts/check-doc-links.py`

**Out of scope:** Runtime behavior, production tests, deployment, publication,
or any other wishlist promotion.

## Task 2: Make the AVInput handler an honest windowed sender

**Tier:** main

**Files:**

- Modify: `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/AVInputChannelHandler.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/HU/Handlers/AVInputChannelHandler.cpp`
- Modify: `tests/test_avinput_channel_handler.cpp`

**Acceptance criteria:**

- A synchronous application capture-controller decision occurs before every
  open response. Open success returns value 0 and enables sending; immediate
  failure returns value 1 and enables nothing; close is idempotent and returns
  value 0. If no capture controller is registered, an open request fails closed
  with value 1; the handler never falls back to its former false success.
- A replacement open and every close/channel-close reset outstanding permits
  and capture state without duplicating stop side effects.
- Missing or non-positive `max_unacked` permits one outstanding frame. A
  positive value is enforced exactly; `sendMicData()` returns false instead of
  emitting beyond it.
- Valid same-session ACK timestamps/counts release at most the outstanding
  total and signal a newly available window. Malformed, zero, stale-session,
  and over-count ACKs cannot create or underflow permits.
- Timestamp framing remains eight-byte big-endian microseconds followed by
  unmodified PCM. Existing session value zero and all proto files remain
  unchanged.

**Test command:**
`cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R test_avinput_channel_handler --output-on-failure`

**Out of scope:** PipeWire capture, gain, buffering, other AV handlers,
protobuf edits, service discovery, or application teardown.

## Task 3: Add the bounded real-time-to-Qt PCM bridge

**Tier:** main

**Files:**

- Add: `src/core/aa/AVInputCaptureBridge.hpp`
- Add: `src/core/aa/AVInputCaptureBridge.cpp`
- Modify: `src/CMakeLists.txt`
- Add: `tests/test_avinput_capture_bridge.cpp`
- Modify: `tests/CMakeLists.txt`

**Acceptance criteria:**

- Construction preallocates a 4 KiB SPSC ring. The producer entry point only
  reads/writes atomics and copies into that storage: no allocation, lock, log,
  QObject call, queued invocation, or protocol call occurs there.
- The Qt-owner consumer creates only complete 640-byte/20 ms S16LE frames,
  applies the capture-generation gain with signed 16-bit saturation, and uses
  a monotonic microsecond timestamp.
- Gain is exact in the documented 0.5–4.0 range; invalid/non-finite values are
  normalized deterministically. Positive, negative, unity, fractional, and
  clipping cases are unit tested.
- Ring overflow, protocol-window refusal, stop, and generation replacement
  purge stale PCM so later sends resume with current audio. Drop diagnostics
  are aggregated outside the producer callback.
- Start/stop and window-available edges are idempotent, and no stale timer or
  prior-generation data invokes a sender after stop/restart.

**Test command:**
`cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(audio_ring_buffer|avinput_capture_bridge)' --output-on-failure`

**Out of scope:** AudioService internals, AA response/ACK parsing, generic ring
changes, DSP/AEC/ANC, UI, or microphone hardware access.

## Task 4: Own Assistant capture in the AA session lifecycle

**Tier:** main

**Files:**

- Modify: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `tests/test_aa_orchestrator.cpp`
- Modify: `docs/reference/config-schema.md`

**Acceptance criteria:**

- AVInput open starts exactly one bridge generation and one autoconnecting
  `AA Assistant Microphone` PipeWire capture at 16000 Hz, mono, 16-bit, using
  the configured input device and snapshotted configured gain.
- The immutable pre-connect capture callback only feeds the bridge. The bridge
  alone calls `AVInputChannelHandler::sendMicData()` from the orchestrator's Qt
  thread, and handler window-available edges resume its drain.
- An immediate capture failure returns false to the handler and leaves no
  handle/active bridge. A queued runtime error is generation-checked, logs once,
  and runs the same local stop path without an unsolicited wire response.
- Replacement open, phone close, channel close, disconnect, stop, and
  destructor close/quiesce the PipeWire capture first, then purge the bridge
  and reset handler state before session finalization or transport deletion.
- A reconnect cannot send PCM or process an error from the prior session. The
  legacy post-open callback API is not used as a production fallback.
- Orchestrator tests cover success/failure and ordering through explicit test
  seams without requiring a PipeWire daemon; existing AA playback, focus,
  service discovery, and reconnect tests remain green.
- `docs/reference/config-schema.md` documents the enforced 0.5–4.0 microphone
  gain range and identifies Assistant AVInput capture as its production
  consumer, matching the existing settings slider and bridge normalization.

**Test command:**
`cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(avinput_channel_handler|avinput_capture_bridge|aa_orchestrator|service_discovery_builder|audio_service)' --output-on-failure`

**Out of scope:** Changes to `IAudioService`, generic PipeWire reconnection,
microphone selection UI, HFP/SCO, audio focus/ducking, Companion, QML, or other
projection channels.

## Task 5: Verify, review, deploy, validate, and close

**Tier:** main

**Files:**

- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Move after completion:
  `docs/plans/2026-07-24-aa-assistant-microphone-transport-design.md` to
  `docs/archive/plans/2026-07-24-aa-assistant-microphone-transport-design.md`
- Move after completion:
  `docs/plans/2026-07-24-aa-assistant-microphone-transport-plan.md` to
  `docs/archive/plans/2026-07-24-aa-assistant-microphone-transport-plan.md`

**Acceptance criteria:**

- Focused tests, full local build, explicit `openauto-prodigy` target, full
  CTest, documentation-link validation, and diff checks pass.
- `bash scripts/codex-review.sh <base-ref>` completes and every finding is
  confirmed/fixed or dismissed with a recorded reason; substantial fixes get
  the required one rerun. After that repository review and main-session
  adjudication, Claude model `fable` performs the final pre-push review and
  every finding is likewise adjudicated.
- `./cross-build.sh` succeeds at the reviewed commit. A rollback snapshot
  precedes deployment, and only the binary/application service are changed.
- Live validation passes the design matrix: a Pixel recognizes spoken
  Assistant queries, five open/close cycles and an active-capture disconnect
  clean up, configured source/gain behave, wireless H.265 returns, one app
  process remains responsive, and hostapd/Bluetooth PIDs and restart counts do
  not change.
- One closure handoff records commands/results plus both review adjudications.
  The design/plan become `COMPLETED 2026-07-24`, move to the archive, roadmap
  Now clears, INDEX points to the archive, and the branch remains unpushed
  until Matthew's go-ahead.

**Test command:**
`cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && cmake --build . --target openauto-prodigy -j$(nproc) && ctest --output-on-failure && cd /mnt/e/claude/personal/openautopro/openauto-prodigy && python3 scripts/check-doc-links.py && git diff --check <base-ref>`

**Out of scope:** Release/milestone tagging, ready-for-review promotion,
Bluetooth/hostapd restart, HFP calls, second-phone coverage, unrelated Pi
checkout cleanup, or any new feature/finding outside this plan.
