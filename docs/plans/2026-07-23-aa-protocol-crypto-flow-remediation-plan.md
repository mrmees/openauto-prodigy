# Android Auto Protocol Crypto and Flow-Control Remediation — Implementation Plan

Date: 2026-07-23
Status: ACTIVE
Design: `docs/plans/2026-07-23-aa-protocol-crypto-flow-remediation-design.md`
Base: `origin/main` at `796b5c29f51e1b070af36ed1f756b65c6d9e861c`

## Execution Rules

- Execute one bounded task and one commit at a time; nobody pushes mid-wave.
- Every implementation task is `Tier: main`: OpenSSL classification, protocol
  framing, session state, ACK permits, and navigation semantics are
  judgment-heavy protocol work.
- Never edit `libs/prodigy-oaa-protocol/proto/`; preserve wireless-only AA,
  HFP HF-role, no-ofono, frozen API/numeric rails, and wishlist-then-promote.
- Public commits and docs contain no private audit identifiers, evidence, or
  backlinks. Private lifecycle state changes only in the ignored ledger.
- Tests are added in the same task as the behavior they pin.

## Task 1: Activate and pin the protocol-correctness wave

**Tier:** main

**Files:**

- Add: `docs/plans/2026-07-23-aa-protocol-crypto-flow-remediation-design.md`
- Add: `docs/plans/2026-07-23-aa-protocol-crypto-flow-remediation-plan.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`

**Acceptance criteria:**

- Design and plan are ACTIVE and grounded on merged PR #30.
- The six current-code concerns have explicit attempted-refutation results.
- Every task names exact files, testable acceptance, a command, a tier, and an
  out-of-scope boundary.
- The Pi matrix distinguishes required, optional, and unnecessary operations.
- No private identifier, evidence, or ledger path appears in tracked content.

**Test command:** `git diff --check && python3 scripts/check-doc-links.py`

**Out of scope:** Runtime behavior, tests, ledger closure, deployment, or
publication.

## Task 2: Make OpenSSL initialization and runtime I/O fail closed

**Tier:** main

**Files:**

- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/Cryptor.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/Cryptor.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/Messenger.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/Messenger.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/SessionState.hpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/AASession.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Session/AASession.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_cryptor.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_messenger.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_fsm.cpp`
- Modify: `libs/prodigy-oaa-protocol/README.md`

**Acceptance criteria:**

- Every OpenSSL allocation, PEM parse, certificate/key install and match, BIO
  creation/write/read, `SSL_new`, `SSL_read`, and `SSL_write` result used by the
  library is checked at the call boundary.
- Invalid or mismatched explicit PEM input returns failure, reports a bounded
  diagnostic, leaks nothing, and leaves the cryptor safely reusable.
- Valid embedded credentials still complete a client/server handshake and
  support repeated encrypted messages.
- Fatal or incomplete established-session TLS input produces no application
  payload and emits one failure; a failed encrypted send queues no frame.
- Post-handshake TLS failure closes exactly once with appended `TlsError`;
  initial handshake failure remains `HandshakeError`.
- Stop/restart clears all TLS failure latches and state.

**Test command:**
`cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(cryptor|messenger|session_fsm)' --output-on-failure`

**Out of scope:** TLS version/cipher policy, credential identity, peer
verification, framing limits, ACK cadence, ping timing, or navigation state.

## Task 3: Bound and validate fragmented-message assembly

**Tier:** main

**Files:**

- Modify: `libs/prodigy-oaa-protocol/include/oaa/Version.hpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/FrameHeader.hpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/FrameParser.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/FrameParser.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/FrameAssembler.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/FrameAssembler.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/Messenger.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/Messenger.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/SessionState.hpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/AASession.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Session/AASession.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_frame_parser.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_frame_assembler.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_messenger.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_fsm.cpp`
- Modify: `libs/prodigy-oaa-protocol/README.md`

**Acceptance criteria:**

- FIRST's declared plaintext total reaches the assembler without wire-format
  change and valid serialized multi-frame messages still round-trip.
- One message is capped at 16 MiB and aggregate declared in-flight assembly at
  32 MiB; tests use injectable smaller limits.
- Zero, too-small, oversized, aggregate-overbudget, flag-mismatched,
  overrun, underrun-LAST, duplicate-FIRST, reset, and interleaved-channel cases
  have deterministic release and notification behavior.
- Only LAST at the exact declared total emits an application message.
- One assembly violation produces no partial message and closes exactly once
  with appended `ProtocolError`.

**Test command:**
`cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(frame_header|frame_parser|frame_assembler|messenger|session_fsm)' --output-on-failure`

**Out of scope:** Protobuf definitions, outbound fragmentation size, TCP read
buffering, media decode, TLS policy, or application-level payload limits.

## Task 4: Correct AA permits, liveness deadline, and rerouting state

**Tier:** main

**Files:**

- Modify: `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/AudioChannelHandler.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/HU/Handlers/AudioChannelHandler.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/AASession.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Session/AASession.cpp`
- Modify: `libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp`
- Modify: `tests/test_audio_channel_handler.cpp`
- Modify: `tests/test_navigation_channel_handler.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_config.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_fsm.cpp`
- Modify: `libs/prodigy-oaa-protocol/README.md`

**Acceptance criteria:**

- Audio setup advertises ten permits and each accepted media frame immediately
  returns `ack_count = 1`; no hard-coded duplicate threshold or counter remains.
- `pingInterval` controls send cadence and `pingTimeout` independently controls
  the single-shot pong deadline across custom configurations.
- Active sends a ping before arming the deadline; a pong clears the outstanding
  deadline only when it echoes a timestamp issued in that window, the next ping
  rearms it, and close/finalize/restart cancel both and clear timestamp state.
  Expiry emits one `PingTimeout` and cannot be revived by a late pong.
- ACTIVE→REROUTING remains active with no duplicate signal or downstream clear.
  INACTIVE and UNAVAILABLE transition inactive exactly once.
- Existing audio payload delivery, setup/start/stop, video ACK, navigation
  events, focus, and session success behavior remain unchanged.

**Test command:**
`cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest -R 'test_(audio_channel_handler|navigation_channel_handler|navigation_data_bridge|session_config|session_fsm|video_channel_handler)' --output-on-failure`

**Out of scope:** Audio buffers, PipeWire/EQ, video permits, nav UI/QML, touch,
night state, HFP, AVRCP, or new navigation features.

## Task 5: Verify, review, deploy, and close the wave

**Tier:** main

**Files:**

- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Move after completion:
  `docs/plans/2026-07-23-aa-protocol-crypto-flow-remediation-design.md` to
  `docs/archive/plans/2026-07-23-aa-protocol-crypto-flow-remediation-design.md`
- Move after completion:
  `docs/plans/2026-07-23-aa-protocol-crypto-flow-remediation-plan.md` to
  `docs/archive/plans/2026-07-23-aa-protocol-crypto-flow-remediation-plan.md`
- Private only: update the ignored remediation ledger overlay

**Acceptance criteria:**

- Focused tests, full build, explicit `openauto-prodigy` target, full CTest,
  docs links, and `git diff --check` pass.
- `bash scripts/codex-review.sh origin/main` completes and every finding is
  confirmed/fixed or dismissed with a recorded reason; a substantial fix gets
  the required single rerun.
- `./cross-build.sh` succeeds at the reviewed commit.
- A rollback snapshot precedes deployment. Only the reviewed binary and app
  service are touched; the Pi's unrelated dirty checkout state is preserved.
- The required live matrix passes: one responsive process, unchanged
  hostapd/Bluetooth PIDs and restart counts, Pixel wireless TLS/session/channel
  reconnect, H.265 first frame, and AA audio when available.
- The six private overlays close only after the required local/review/live
  evidence passes; counts are recomputed by immutable array position.
- Exactly one handoff is appended; design/plan become `COMPLETED 2026-07-23`
  and move to the archive in the closure commit.
- The clean branch is pushed and a draft PR is opened under Matthew's standing
  publication approval.

**Test command:**
`cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && cmake --build . --target openauto-prodigy -j$(nproc) && ctest --output-on-failure && cd /mnt/e/claude/personal/openautopro/openauto-prodigy && python3 scripts/check-doc-links.py && git diff --check origin/main`

**Out of scope:** Any unplanned remediation, companion-app work, service restart
beyond the application, re-pairing, release tag, or ready-for-review promotion.
