# API and Core Asynchronous Lifecycle Remediation — Implementation Plan

Date: 2026-07-22
Status: ACTIVE
Design: `docs/plans/2026-07-22-api-core-async-lifecycle-remediation-design.md`
Base: `origin/main` at `42f6aa4344fec17f275122cdf76ced8a6fb3b369`

## Execution Rules

- Execute one bounded task and one commit at a time; nobody pushes mid-wave.
- `Tier: opus` tasks use `gpt-5.6-terra` for this plan.
- Protocol, cross-thread cancellation, clock/process orchestration, and
  cross-repository security decisions remain `Tier: main`.
- Preserve every root hard constraint, including the additive-only External API
  proto and hands-off AA submodule proto.
- Keep the companion repository on its separately reviewed branch and commit
  history. Proto changes must remain byte-identical across repositories.
- New feature ideas go to the wishlist; they do not expand this wave.

## Task 1: Activate the consolidated wave

**Tier:** main

**Files:**

- Add: `docs/plans/2026-07-22-api-core-async-lifecycle-remediation-design.md`
- Add: `docs/plans/2026-07-22-api-core-async-lifecycle-remediation-plan.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`

**Acceptance criteria:**

- The design and plan are ACTIVE and grounded on merged PR #28.
- The roadmap names one consolidated wave and its companion dependency.
- Public docs contain no private audit identifiers, evidence, or backlinks.

**Test command:** `git diff --check && python3 scripts/check-doc-links.py`

**Out of scope:** Behavior, tests, private-ledger lifecycle changes, or
publication.

## Task 2: Upgrade and version the pairing credential contract

**Tier:** main

**Files:**

- Modify: `proto/api/api.proto`
- Modify: `src/core/api/ApiAuth.hpp`
- Modify: `src/core/api/ApiAuth.cpp`
- Modify: `src/core/api/PairingManager.hpp`
- Modify: `src/core/api/PairingManager.cpp`
- Modify: `src/core/api/ApiSession.hpp`
- Modify: `src/core/api/ApiSession.cpp`
- Modify: `src/core/api/ApiServer.hpp`
- Modify: `src/core/api/ApiServer.cpp`
- Modify: `qml/applications/settings/ApiSettings.qml`
- Modify: `tests/test_api_proto_roundtrip.cpp`
- Modify: `tests/test_api_auth.cpp`
- Modify: `tests/test_api_pairing.cpp`
- Modify: `tests/test_api_session.cpp`
- Modify: `tests/test_api_server.cpp`
- Modify: `tests/test_api_loopback.cpp`
- Modify: `docs/architecture.md`

**Acceptance criteria:**

- Pairing generates exactly 24 canonical Base32 characters through a
  deterministic test seam and displays six groups of four.
- QR uses `code=`, never `pin=`, and the challenge declares `BASE32_120` at
  additive field 7.
- Capabilities advertise secure pairing at additive field 13.
- New credentials persist generation 2; absent/legacy generation is rejected
  with the additive typed upgrade code at `AuthReject` field 6.
- Store loading parses transactionally; load failure preserves prior state,
  blocks save/pairing, and cannot truncate the file.
- Existing known-client proof, constant-time comparison, pairing-window,
  terminal-frame, listener, and peer-admission behavior remains unchanged.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_api_proto_roundtrip test_api_auth test_api_pairing test_api_session test_api_server test_api_loopback openauto-prodigy -j$(nproc)
ctest --output-on-failure -R '^test_api_(proto_roundtrip|auth|pairing|session|server|loopback)$'
```

**Out of scope:** TLS, PAKE implementation, legacy pairing fallback, AA proto,
or non-pairing API features.

## Task 3: Bound each handshake stage and close API ownership explicitly

**Tier:** opus

**Files:**

- Modify: `src/core/api/ApiSession.cpp`
- Modify: `src/core/api/ApiServer.hpp`
- Modify: `src/core/api/ApiServer.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_api_session.cpp`
- Modify: `tests/test_api_server.cpp`
- Modify: `tests/test_api_request_handlers.cpp`

**Acceptance criteria:**

- The deadline restarts only after a successful transition from ClientHello to
  AuthPending or PairingPending.
- Each stage times out independently; Ready and teardown cancel the timer.
- Pairing actions are unregistered idempotently and cannot call a destroyed
  server.
- `main.cpp` stops the API before plugin/provider and app-child teardown.
- Session-owned actions/notifications close exactly once.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_api_session test_api_server test_api_request_handlers openauto-prodigy -j$(nproc)
ctest --output-on-failure -R '^test_api_(session|server|request_handlers)$'
```

**Out of scope:** Pairing cryptography, transport framing, publisher content, or
general QObject teardown changes.

## Task 4: Make clock synchronization nonblocking and repair call epochs

**Tier:** main

**Files:**

- Modify: `src/core/services/ClockSyncService.hpp`
- Modify: `src/core/services/ClockSyncService.cpp`
- Modify: `src/core/api/ApiPublishers.hpp`
- Modify: `src/core/api/ApiPublishers.cpp`
- Modify: `src/core/api/ApiServer.hpp`
- Modify: `src/core/api/ApiServer.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_clock_sync.cpp`
- Modify: `tests/test_api_publishers.cpp`
- Modify: `tests/test_api_server.cpp`
- Modify: `docs/architecture.md`

**Acceptance criteria:**

- Time and timezone report handlers return without waiting for a child process.
- The three-command clock transaction stays ordered; failure and timeout still
  restore NTP; duplicates cannot grow an unbounded queue.
- `clockAdjusted` emits only for successful `set-time`.
- An active call recomputes `started_at_unix_ms` from the corrected clock and
  current duration and emits once through normal publisher coalescing.
- Idle calls, timezone-only changes, backward-jump guard, drift threshold, and
  serialization remain unchanged.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_clock_sync test_api_publishers test_api_server openauto-prodigy -j$(nproc)
ctest --output-on-failure -R '^test_(clock_sync|api_publishers|api_server)$'
```

**Out of scope:** NTP policy redesign, HFP state-machine changes, deliberate live
clock stepping, or protobuf timestamp semantics.

## Task 5: Suppress duplicate connectivity and make EventBus unsubscribe final

**Tier:** main

**Files:**

- Modify: `src/core/api/ApiInboundState.hpp`
- Modify: `src/core/api/ApiInboundState.cpp`
- Modify: `src/core/services/EventBus.hpp`
- Modify: `src/core/services/EventBus.cpp`
- Modify: `tests/test_api_request_handlers.cpp`
- Modify: `tests/test_event_bus.cpp`
- Modify: `docs/reference/plugin-api.md`

**Acceptance criteria:**

- Exact duplicate active and inactive connectivity tuples emit nothing.
- A host, port, active-state, or password change emits one route update;
  observable property notification occurs only when observable state changes.
- A callback queued before unsubscribe does not execute afterward.
- Off-thread unsubscribe waits for a currently executing callback, while
  owner-thread and self-unsubscribe do not deadlock.
- Publish/subscribe remain thread-safe and callbacks still execute on the bus
  owner thread.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_api_request_handlers test_event_bus -j$(nproc)
ctest --output-on-failure -R '^test_(api_request_handlers|event_bus)$'
```

**Out of scope:** Proxy-routing policy, EventBus topic redesign, or plugin ABI
changes.

## Task 6: Make system-daemon reconnect single-owner and complete

**Tier:** opus

**Files:**

- Modify: `src/core/services/SystemServiceClient.hpp`
- Modify: `src/core/services/SystemServiceClient.cpp`
- Add: `tests/test_system_service_client.cpp`
- Modify: `tests/CMakeLists.txt`

**Acceptance criteria:**

- One owned single-shot timer schedules retries for every terminal error and
  disconnect and stops on connection.
- Duplicate error/disconnect signals never create parallel attempts.
- Ambiguous failed socket state is aborted before retry.
- A real local test server proves initial absence recovery and reconnect after
  disconnect using injected socket path and interval.
- Existing request/response and route-state behavior remains unchanged.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_system_service_client openauto-prodigy -j$(nproc)
ctest --output-on-failure -R '^test_system_service_client$'
```

**Out of scope:** System daemon protocol or service supervision changes.

## Task 7: Contain plugin exceptions and frame IPC streams

**Tier:** opus

**Files:**

- Modify: `src/core/plugin/PluginManager.cpp`
- Modify: `src/core/services/IpcServer.hpp`
- Modify: `src/core/services/IpcServer.cpp`
- Modify: `tests/test_plugin_manager.cpp`
- Modify: `tests/test_ipc_audio_config.cpp`
- Modify: `tests/test_ipc_install_theme.cpp`
- Modify: `tests/test_ipc_logging.cpp`
- Modify: `tests/test_ipc_single_instance.cpp`

**Acceptance criteria:**

- Unknown initialize exceptions disable only the throwing plugin.
- Unknown shutdown exceptions are contained and later plugins still shut down
  in reverse order.
- Split and coalesced newline requests receive one ordered response per frame.
- Per-client tails survive reads, are removed on disconnect, and cannot exceed
  1 MiB.
- Every existing IPC command and ownership rule remains unchanged.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_plugin_manager test_ipc_audio_config test_ipc_install_theme test_ipc_logging test_ipc_single_instance -j$(nproc)
ctest --output-on-failure -R '^test_(plugin_manager|ipc_(audio_config|install_theme|logging|single_instance))$'
```

**Out of scope:** Plugin ABI/versioning or replacing newline-delimited IPC.

## Task 8: Complete synchronized local gates and repository review

**Tier:** main

**Files:** No planned changes. Confirmed review findings receive bounded fix
commits with focused verification.

**Acceptance criteria:**

- Companion's matching plan is complete through its local gate.
- API proto files are byte-identical across repositories.
- `cmake --build . -j$(nproc)`, explicit `openauto-prodigy`, and full CTest pass.
- `git diff --check` and documentation links pass.
- `bash scripts/codex-review.sh origin/main` completes and every finding is
  fixed or dismissed with a recorded reason.

**Out of scope:** Unrelated cleanup found during review.

## Task 9: Cross-build and run the coordinated live matrix

**Tier:** main

**Files:** No planned tracked changes.

**Acceptance criteria:**

- `./cross-build.sh` succeeds from the reviewed Prodigy range.
- The reviewed Companion debug APK is available.
- Exact rollback snapshots precede credential/config mutation.
- The required live matrix in the design passes, including one-time secure
  re-pair, saved reconnect, reports, IPC framing, process/IPC health, AA/A2DP,
  and unchanged Bluetooth/hostapd PIDs.
- Unrelated dirty Pi files are neither pulled, reset, cleaned, nor overwritten.

**Out of scope:** Optional/not-required matrix rows unless needed to diagnose a
required failure.

## Task 10: Close, archive, reconcile, and publish both drafts

**Tier:** sonnet

**Files:**

- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Move: both active documents to `docs/archive/plans/`
- Private only: remediation ledger overlay

**Acceptance criteria:**

- Exactly one Prodigy handoff records implementation, review adjudication,
  local/cross/live evidence, and companion coordination.
- Both documents are marked `COMPLETED 2026-07-22` and archived in the same
  closure commit.
- Private lifecycle changes use immutable array positions and do not create a
  public backlink.
- Counts are recomputed after the ledger update.
- Completed branches are pushed and separate draft PRs are opened against each
  repository's `main`, using Matthew's standing publication approval.

**Test command:**

```bash
git diff --check
python3 scripts/check-doc-links.py
```

**Out of scope:** Merge, release tags, or release publication.
