# Android Auto Session and Transport Lifecycle Remediation — Implementation Plan

Date: 2026-07-22
Status: ACTIVE
Design: `docs/plans/2026-07-22-aa-session-transport-lifecycle-remediation-design.md`
Base: `origin/main` at `97072775ef1441d1db857db413abed9e30d00d82`

## Execution Rules

- Execute one bounded task and one commit at a time; nobody pushes mid-wave.
- Protocol state, teardown ordering, TLS classification, and channel routing
  are judgment-heavy and remain `Tier: main`.
- `Tier: opus` uses `gpt-5.6-terra` if dispatched.
- Never edit `libs/prodigy-oaa-protocol/proto/`; preserve wireless-only AA,
  HFP HF-role, no-ofono, frozen API/numeric rails, and wishlist-then-promote.
- Public commits and docs contain no private audit identifiers, evidence, or
  backlinks. Private lifecycle state changes only in the ignored ledger.

## Task 1: Activate and pin the consolidated wave

**Tier:** main

**Files:**

- Add: `docs/plans/2026-07-22-aa-session-transport-lifecycle-remediation-design.md`
- Add: `docs/plans/2026-07-22-aa-session-transport-lifecycle-remediation-plan.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`

**Acceptance criteria:**

- Design and plan are ACTIVE, grounded on merged PR #29, and contain the
  accepted/refuted revalidation matrix.
- The roadmap names one bounded AA session/transport lifecycle wave.
- Every task has a tier, exact files, testable criteria, a test command, and an
  explicit out-of-scope line.
- Public documentation exposes no private audit metadata.

**Test command:** `git diff --check && python3 scripts/check-doc-links.py`

**Out of scope:** Behavior, tests, private-ledger lifecycle changes, or
publication.

## Task 2: Make session and messenger teardown/restart deterministic

**Tier:** main

**Files:**

- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/CircularBuffer.hpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/FrameParser.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/FrameParser.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/FrameAssembler.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/FrameAssembler.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/Messenger.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/Messenger.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/AASession.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Session/AASession.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_messenger.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_fsm.cpp`
- Modify: `tests/test_aa_orchestrator.cpp`
- Modify: `libs/prodigy-oaa-protocol/README.md`

**Acceptance criteria:**

- `AASession` destruction performs no transport write and no external-handler
  callback in Active, negotiating, or shutting-down states.
- Explicit finalization closes all registered handlers exactly once while they
  are alive, detaches old send paths, clears registrations, and is idempotent.
- Normal disconnect stops delivery before notification; restart reconnects the
  registered handlers and begins a fresh cycle.
- Synchronous replacement destroys the prior session only after finalization,
  and no persistent handler can send through the old messenger or before the
  replacement version exchange.
- Repeated Messenger start delivers one message/error. Stop/start discards
  partial raw frames, fragmented messages, TLS state, and queued sends.
- Existing graceful shutdown and session FSM success behavior remains intact.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_messenger test_session_fsm test_aa_orchestrator openauto-prodigy -j$(nproc)
ctest --output-on-failure -R '^test_(messenger|session_fsm|aa_orchestrator)$'
```

**Out of scope:** Protocol message formats, channel payload behavior, general
QObject ownership, or transport implementation replacement.

## Task 3: Type TLS failure and unify channel-open response routing

**Tier:** main

**Files:**

- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/Cryptor.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/Cryptor.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Messenger/Messenger.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Messenger/Messenger.cpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/SessionState.hpp`
- Modify: `libs/prodigy-oaa-protocol/include/oaa/Session/AASession.hpp`
- Modify: `libs/prodigy-oaa-protocol/src/Session/AASession.cpp`
- Modify: `libs/prodigy-oaa-protocol/src/Channel/ControlChannel.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_cryptor.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_messenger.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_session_fsm.cpp`
- Modify: `libs/prodigy-oaa-protocol/tests/test_control_channel.cpp`
- Modify: `libs/prodigy-oaa-protocol/README.md`

**Acceptance criteria:**

- Successful TLS remains green; WANT-I/O remains pending; deterministic
  malformed input returns fatal with a nonempty diagnostic.
- Messenger emits one fatal signal after sending any generated alert bytes.
- A session in TLS handshake reports `HandshakeError` immediately and cannot
  later emit the generic timeout result for the same failure.
- Accepted and rejected channel-open responses use the requested nonzero
  channel, message ID `0x0008`, and Control message type in both dispatch paths.
- Channel handlers open/close and existing status values remain unchanged.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_cryptor test_messenger test_session_fsm test_control_channel openauto-prodigy -j$(nproc)
ctest --output-on-failure -R '^test_(cryptor|messenger|session_fsm|control_channel)$'
```

**Out of scope:** TLS certificate/key replacement, cipher policy, protobuf
changes, service discovery, media encryption policy, or protocol capture.

## Task 4: Recover transient RFCOMM listener startup failure

**Tier:** opus

**Files:**

- Modify: `src/core/aa/BluetoothDiscoveryService.hpp`
- Modify: `src/core/aa/BluetoothDiscoveryService.cpp`
- Modify: `tests/test_bt_discovery_service.cpp`
- Modify: `docs/architecture.md`
- Modify: `docs/how-to/testing-reconnect.md`

**Acceptance criteria:**

- One failed listen schedules one retry owner and does not attempt SDP.
- Eventual listener success cancels retry and registers SDP once with the
  returned nonzero RFCOMM port.
- Retry exhaustion emits one terminal error; `stop()` cancels pending listener
  and SDP retries; start after stop begins a fresh retry budget.
- The real production path still uses `QBluetoothServer`, legacy SDP, the same
  retry interval/budget, and wireless-only discovery.
- Tests drive the real state machine without BlueZ or a Bluetooth adapter.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_bt_discovery_service openauto-prodigy -j$(nproc)
ctest --output-on-failure -R '^test_bt_discovery_service$'
```

**Out of scope:** SDP record shape/session lifetime, Bluetooth daemon control,
pairing policy, A2DP, HFP, WiFi credential format, or orchestrator redesign.

## Task 5: Complete local, review, and cross-build gates

**Tier:** main

**Files:** No planned files. Confirmed review results receive bounded fix
commits with focused verification and behavior documentation when applicable.

**Acceptance criteria:**

- Focused protocol/orchestrator/discovery tests pass.
- Full build, explicit app target, and full CTest pass.
- Documentation links and `git diff --check` pass.
- `bash scripts/codex-review.sh origin/main` completes and every result is
  confirmed/fixed or dismissed with a recorded reason; one rerun follows any
  substantial review fix.
- `./cross-build.sh` produces the aarch64 application binary.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git diff --check
python3 scripts/check-doc-links.py
bash scripts/codex-review.sh origin/main
./cross-build.sh
```

**Out of scope:** Publishing, Pi mutation, or unrelated review cleanup.

## Task 6: Deploy, live-validate, close, and publish the wave

**Tier:** main

**Files:**

- Modify privately: `docs/private/audit-2026-07-20-remediation-ledger.json`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Move: active design and plan to `docs/archive/plans/` with status
  `COMPLETED 2026-07-22`

**Acceptance criteria:**

- A rollback snapshot is retained and unrelated Pi dirty state is unchanged.
- Reviewed aarch64 binary is deployed; only the app is restarted.
- Wireless discovery, a fresh H.265 session, active-session app restart or
  replacement, clean renegotiation, projection/input/audio, one process, and
  responsive IPC pass; hostapd and Bluetooth PIDs are unchanged.
- Private overlays close only after required evidence, unresolved counts are
  recomputed by immutable array position, and no mapped result is silently
  dropped.
- Exactly one handoff is appended; design/plan are archived in the closure
  commit; the branch is pushed and a draft PR is opened.

**Test command:** Follow the design's live matrix, then run
`git diff --check && python3 scripts/check-doc-links.py && git status --short`.

**Out of scope:** Bluetooth/hostapd restart, re-pairing, HFP calls, malformed
live TLS injection, companion deployment, release tag, or merge.
