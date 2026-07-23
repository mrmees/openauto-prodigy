# API and Core Asynchronous Lifecycle Remediation — Design

Date: 2026-07-22
Status: ACTIVE
Grounded on: `42f6aa4344fec17f275122cdf76ced8a6fb3b369`
Companion baseline: `openauto-companion` `origin/main` at
`1a7c985761992501547e8547e94ca4f3d838b93d`

## Goal

Close the revalidated External API and core asynchronous-lifecycle defects as
one bounded wave while preserving the existing public architecture. The wave
also coordinates a security upgrade in `openauto-companion`: new pairing uses
a high-entropy QR/manual code, legacy six-digit-derived credentials are retired,
and both implementations move together without a compatibility downgrade.

The wave consolidates twelve product roots. It does not turn them into one
large code change: each ownership boundary remains an independently testable
commit and both repositories retain separate review and publication gates.

## Revalidated Current State

| Area | Current behavior | Required contract |
|---|---|---|
| Pairing proof | A captured nonce, salt, and HMAC can test the complete six-digit PIN space offline. | New pairing secrets have at least 112 bits of random strength and are explicitly versioned on the wire and at rest. |
| Pairing/store | PIN generation excludes one upper-bound value; store parsing mutates live state and load failure is ignored. | Secure code generation has a deterministic seam; failed loads preserve prior data and prohibit destructive saves/pairing. |
| Handshake timer | One timer budget covers both ClientHello and the response to the issued challenge. | Each inbound handshake stage receives the configured deadline. |
| API teardown | Session cleanup and registered actions can outlive the services they dereference during application-child teardown. | The server stops and unregisters actions while dependencies are still alive. |
| Clock sync | `waitForFinished()` blocks the Qt main thread for up to multiple seconds. | Clock/timezone commands execute asynchronously, preserve ordering and NTP restoration, and recover from failures/timeouts. |
| Call timestamp | An active call's epoch start is captured before a wall-clock step and remains stale. | A successful clock step recomputes the active-call epoch and emits one coalesced phone status. |
| Connectivity | Identical reports re-emit route changes and rebuild the proxy path. | Exact duplicate route tuples are no-ops; password or other tuple changes still propagate. |
| Event bus | A queued callback owns a copied function and still executes after unsubscribe. | Unsubscribe suppresses queued delivery and, off the owner thread, does not return while that callback is executing. |
| System daemon client | Only selected socket errors retry through unowned one-shot timers. | One owned retry timer handles every terminal error/disconnect without duplicate attempts. |
| Plugin lifecycle | Non-`std::exception` throws escape initialize/shutdown. | All plugin exceptions are contained; reverse shutdown continues. |
| IPC framing | Each `readyRead()` chunk is parsed as one JSON request. | Newline frames survive partial and coalesced reads with a bounded per-client buffer. |

## Design Decisions

### 1. Secure pairing is a coordinated credential-generation upgrade

- Generate 24 characters from the RFC 4648 Base32 alphabet `A-Z2-7` with the
  operating system random generator: 120 random bits. The canonical value is
  uppercase and ungrouped; displays group it as six four-character blocks.
- QR remains the primary path. Manual entry remains available, accepts case and
  visual separators, and normalizes to the canonical value before derivation.
- Preserve the existing proof shape:
  `secret = SHA256(code_utf8 || salt)` and
  `proof = HMAC-SHA256(secret, challenge_nonce)`. The security change is the
  non-enumerable random input, not a compensating client-side conversion.
- Do not implement a PAKE from scratch and do not claim that a slower password
  KDF alone prevents offline guessing. RFC 9106 defines a memory-hard password
  KDF; it raises guess cost but does not make a six-digit transcript safe.
- Extend `PairingChallenge` additively at field 7 with a
  `PairingSecretFormat`; fields 3 through 6 remain reserved. The server emits
  `BASE32_120`; the new companion rejects absent, legacy, and unknown formats.
- Add an External API capability flag at `Capabilities` field 13. The challenge
  field is the pre-auth discriminator; the capability confirms the resulting
  session contract without reusing a reserved number.
- Replace the QR query key `pin` with `code`. New companion builds reject the
  old key instead of silently assigning it new semantics.
- Add `AuthReject.code` at field 6, after its reserved range, with a typed
  credential-upgrade result. Human-readable reasons remain informational.
- Persist a credential generation in both stores. Missing generation means
  legacy. Prodigy refuses legacy known-client authentication; Companion's
  storage migration retires legacy records and enters the existing pairing
  flow. This intentionally requires one coordinated re-pair and does not leave
  an insecure fallback enabled.

The 120-bit choice follows NIST SP 800-63B's current 112-bit minimum security
strength for random values while keeping manual fallback possible. QR scanning
absorbs the usability cost in the normal path.

### 2. Store failure is fail-closed and non-destructive

- Parse into a temporary client list and replace live state only after the
  entire document validates.
- Record whether the store loaded successfully. A failed load prohibits save
  and opening a pairing window, so a later pairing cannot overwrite the file.
- A missing file is a successful empty-store load. Existing permissions,
  credential lookup, and atomic pairing persistence behavior remain intact.

### 3. Handshake deadlines apply per inbound stage

- Start the configured timer while waiting for `ClientHello`.
- After successfully sending `AuthRequired` or `PairingChallenge`, restart the
  same timer for the corresponding response stage.
- Stop it on `Ready` or teardown. No new retry, transport, or backpressure
  semantics are introduced.

### 4. API lifetime ends before its providers

- Track the action registry with guarded lifetime semantics and unregister the
  two pairing actions during explicit shutdown/destruction.
- Call `ApiServer::stop()` immediately after `app.exec()` returns, before plugin
  shutdown and application-child destruction. Session-owned actions and
  notifications are released while their registries still exist.
- `stop()` remains idempotent and restartable for tests.

### 5. Clock work is an asynchronous ordered transaction

- Replace blocking `QProcess` waits with one asynchronous runner and a bounded
  command timeout.
- A clock step remains ordered as `set-ntp false`, `set-time`, then
  `set-ntp true`. Failure or timeout of an earlier command still advances to
  the NTP restore command. Duplicate time-step requests do not build an
  unbounded queue.
- Timezone changes use the same runner but remain independent single commands.
- Emit `clockAdjusted` only after a successful `set-time`. The API phone
  publisher then recomputes `started_at_unix_ms` from the corrected wall clock
  and the authoritative call duration, and schedules one normal coalesced
  status update. Timezone-only changes do not alter epoch timestamps.

### 6. Duplicate asynchronous state is suppressed at its owner

- `ApiInboundState` retains the canonical active route tuple: active, peer
  host, port, and password. An exact duplicate emits neither observable state
  nor route mutation. A password-only change still reaches the route owner.
- `EventBus` queues shared subscription state rather than a detached callback
  copy. Delivery checks active state immediately before invocation. Off-thread
  unsubscribe waits for an already-running callback; owner-thread/self
  unsubscribe never deadlocks.
- `SystemServiceClient` owns one single-shot retry timer, cancels it on connect,
  and schedules it for every terminal socket error or disconnect. Ambiguous
  failed sockets are aborted before retry; duplicate signals cannot create
  duplicate timers.

### 7. Plugin and IPC boundaries fail safely

- Catch unknown exceptions from plugin initialize and shutdown just as metadata
  probing already does. A failed initialize stays disabled; shutdown continues
  in reverse order.
- Keep one newline-framing buffer per IPC socket. Process all complete frames in
  order, retain partial tails, remove buffers on disconnect, and reject an
  oversized frame/buffer at 1 MiB before closing that client.

## Cross-Repository Integration

`openauto-companion` receives the same additive protobuf definition, code
normalization, QR/manual UI, credential-generation persistence, and migration
tests. Its security branch starts from `origin/main`. The already live-validated
airplane-mode recovery commit on `dev` is carried as an explicit first commit so
the coordinated APK cannot regress that deployed behavior; no other `dev`
history is imported.

The repositories publish separate draft pull requests. Neither is merge-safe
alone for new pairing: deployment is coordinated, and the live matrix includes
the one-time credential retirement and re-pair.

## Acceptance Criteria

- A captured new pairing transcript is not enumerable over a six-digit space;
  the generated code contains 120 random bits and the server advertises the
  exact format.
- QR and manual entry normalize to the same canonical code and derive the same
  32-byte secret in C++ and Kotlin.
- Reserved protobuf numbers remain untouched; both repositories' API proto
  files are identical and existing messages/numerics retain their semantics.
- Missing/legacy credential generations cannot authenticate or create new
  credentials. The new Companion build reaches an explicit re-pair path.
- Store parse failure preserves prior in-memory data and cannot overwrite the
  source file through pairing.
- ClientHello, known-client response, and pairing response stages each receive
  the configured handshake deadline.
- API shutdown releases sessions and actions before provider/registry teardown.
- Clock commands never block the Qt main thread; order, timeout, NTP restore,
  and active-call timestamp correction are deterministic under tests.
- Duplicate connectivity reports, queued-unsubscribed events, and duplicate
  system-service retries produce no repeated side effect.
- Nonstandard plugin exceptions are contained and IPC handles split/coalesced
  newline frames with bounded memory.
- Existing API reporting, proxy password changes, plugin success paths, IPC
  commands, Android Auto, A2DP, HFP roles, and wireless transport remain
  unchanged.

## Out of Scope

- TLS transport, a new PAKE implementation, cloud/account pairing, backward
  support for creating or authenticating six-digit credentials, or a general
  credential-management UI.
- New External API mutations, protobuf renames/renumbering, JavaScript-shim
  capability, or changes under `libs/prodigy-oaa-protocol/proto/`.
- Android Auto protocol/transport, HFP/ofono/PipeWire routing, Bluetooth pairing,
  logging configuration, dashboards, themes, or unrelated QML.
- General process supervision, plugin ABI redesign, or replacing the web-config
  IPC protocol.
- Companion SOCKS routing features beyond retaining the already validated
  airplane-mode recovery behavior.

## Verification and Live Matrix

### Required locally

- Focused Prodigy API auth/pairing/session/server/publisher, clock, inbound
  state, EventBus, SystemServiceClient, plugin-manager, and IPC tests.
- Prodigy full build, explicit `openauto-prodigy` target, full CTest,
  documentation links, and `git diff --check`.
- Companion focused pairing/proto/storage/runtime tests plus
  `./gradlew :app:testDebugUnitTest :app:assembleDebug` and `git diff --check`.
- Byte-for-byte proto parity between the two repositories.
- Repository review gates in both repositories with every finding adjudicated.
- Prodigy aarch64 cross-build before deployment.

### Required live after coordinated deployment

- Snapshot the affected Prodigy credential/config files and preserve unrelated
  dirty Pi checkout state.
- Install the reviewed Companion APK and deploy/restart only the Prodigy app;
  do not restart Bluetooth or hostapd.
- Confirm the old credential is rejected/retired, the head unit displays a
  grouped 24-character code, QR scan reaches `READY`, and a subsequent saved
  credential reconnect reaches `READY` without manual entry.
- Confirm time/GPS/battery/connectivity reporting resumes, normal SOCKS route
  state is stable under repeated identical reports, and active Android Auto
  transport is not interrupted by Companion reconnect.
- Confirm one Prodigy application process, responsive IPC, normal A2DP/AA
  operation, and unchanged Bluetooth/hostapd PIDs.
- Exercise real IPC with a split request and two coalesced requests.

### Optional live checks

- Manual entry of the grouped pairing code.
- A controlled system-service disconnect/reconnect.
- A second phone/vehicle record.

### Not required

- Bluetooth daemon restart, Bluetooth re-pairing, HFP call testing, deliberate
  wall-clock stepping, AA protocol capture, or unrelated QML inspection.

Matthew has pre-approved scoped Pi deployment/restarts and draft-PR publication
for a completed wave. Destructive credential/config changes still require a
rollback snapshot and exact target verification before mutation.
