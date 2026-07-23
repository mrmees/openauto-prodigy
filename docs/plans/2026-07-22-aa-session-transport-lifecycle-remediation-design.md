# Android Auto Session and Transport Lifecycle Remediation — Design

Date: 2026-07-22
Status: ACTIVE
Base: `origin/main` at `97072775ef1441d1db857db413abed9e30d00d82`

## Outcome

Android Auto session replacement, shutdown, protocol restart, TLS failure, and
Bluetooth discovery startup become deterministic without changing the wireless
AA protocol schema or any media/input behavior. Persistent channel handlers are
closed exactly once while they are alive, a destroyed session cannot send or
call external objects, restarted messengers begin with empty framing and crypto
state, channel-open responses use the requested service channel, fatal TLS
handshakes fail immediately, and transient RFCOMM listener startup failures
recover without restarting the application.

## Current-State Revalidation

| Area | Attempted refutation | Result | Remediation boundary |
|---|---|---|---|
| Session destructor | Checked graceful acknowledgement, deferred deletion, synchronous replacement, and QObject parent cleanup | Accepted. Timeout and mid-handshake teardown can reach external raw handlers after their owners are destroyed; an Active destructor can also initiate a write | The destructor performs internal cleanup only. The orchestrator explicitly finalizes a session while handlers are alive |
| Active replacement | Checked the existing sensor-only reset and all persistent handler registrations | Accepted. Other handlers and old send connections can retain wire state into the replacement session | One idempotent session finalizer closes every registered handler, detaches their send path, and clears registrations |
| RFCOMM listener startup | Checked the existing SDP retry and orchestrator error path | Accepted. Listener failure returns before SDP registration and has no retry owner | Add one bounded listener retry timer; SDP starts only after a real listener port exists |
| TLS handshake result | Checked OpenSSL return handling and session timeout behavior | Accepted. WANT-I/O and fatal results collapse to the same boolean | Return a typed result, drain the OpenSSL error queue for diagnostics, and disconnect immediately on fatal failure |
| Channel-open response routing | Compared both `AASession` delivery paths with the service-channel behavior in aasdk | Accepted as duplicated-path drift. Normal phones use the already-correct nonzero-channel path; the control helper ignores its target | Route both paths through one helper that emits the response on the target service channel |
| Messenger restart | Checked connection ownership, parser/assembler partial state, crypto state, and the restart allowed by `AASession` | Accepted. Repeated starts duplicate connections and stopped sessions retain partial state | Make start/stop idempotent and reset parser, assembler, crypto, queue, and signal ownership at the boundary |

## Design

### 1. Separate graceful shutdown from terminal ownership finalization

`AASession::stop()` remains the phone-visible graceful operation. A new
idempotent `finalize()` operation is the terminal local teardown used by the
orchestrator. It performs no network write and emits no disconnect reason. It
stops timers and messenger delivery, closes registered channels once, detaches
handler sends, and clears the raw registration map.

The destructor never calls `stop()`, never writes through the transport, and
never dereferences external handlers. It only stops owned timers and messenger
internals. `AndroidAutoOrchestrator::teardownSession()` disconnects session
signals first, calls `finalize()` synchronously while its value-member handlers
are alive, then preserves the existing deferred-versus-synchronous deletion
choice.

Normal transitions to `Disconnected` also stop messenger delivery and close
channels before publishing the state transition. The close operation is
cycle-idempotent. Restarting the same `AASession` reconnects registered handler
send paths and begins a fresh close cycle. The orchestrator consumes the typed
`disconnected` signal as the teardown trigger instead of independently
tearing down from both `stateChanged(Disconnected)` and `disconnected`.

### 2. Give Messenger a complete restart boundary

`Messenger::start()` is idempotent. `stop()` disconnects its pipeline and
clears all per-connection state:

- `FrameParser` buffer and state;
- `FrameAssembler` partial messages and message types;
- TLS state and handshake-failure latch;
- pending sends and the sending flag.

Small reset methods remain owned by the parser and assembler; no protocol
payload or framing rule changes. Tests split both a raw frame and a fragmented
message across stop/start to prove that no old tail can complete in the next
connection, and repeated `start()` delivers one message and one transport
error.

### 3. Surface fatal TLS results without changing the wire protocol

`Cryptor::doHandshake()` returns `Complete`, `WantIo`, or `Failed`. Only
OpenSSL retry states map to `WantIo`; all other results capture a bounded
diagnostic from `SSL_get_error()` and the OpenSSL error queue. Messenger sends
any generated alert bytes, then emits one `handshakeFailed` signal for a fatal
result. `AASession` accepts that signal only in `TLSHandshake`, cancels the
deadline, and reports the appended `DisconnectReason::HandshakeError`.

Successful encryption and the existing handshake framing remain unchanged.
Malformed TLS input is exercised deterministically without a live phone.

### 4. Use one channel-open response contract

`ControlChannel::sendChannelOpenResponse(targetChannelId, accepted)` honors the
target argument. Both the channel-zero compatibility dispatch and the normal
service-channel dispatch call this helper. The response retains message ID
`0x0008`, `MessageType::Control`, encryption policy, and status values; only
the erroneous channel-zero routing is removed. The hands-off protocol submodule
is not modified.

### 5. Retry RFCOMM listener startup before SDP registration

`BluetoothDiscoveryService` owns a listener retry timer separate from its SDP
retry timer. A failed listen schedules one later attempt, emits the terminal
error only after the existing bounded retry budget, and never attempts SDP
with port zero. Success cancels listener retry and enters the existing SDP
registration flow exactly once. `stop()` cancels both retry owners.

A narrow virtual listener/SDP seam lets the production state machine run in a
unit test without a BlueZ daemon or adapter. Production still uses
`QBluetoothServer` and the legacy BlueZ SDP path.

## Acceptance Criteria

- Active, negotiating, shutting-down, already-disconnected, and repeated
  finalization paths perform no destructor-initiated network write or external
  callback.
- Every registered handler receives one close per started session cycle; old
  handlers cannot send through an old messenger or transmit before a
  replacement session begins version exchange.
- Repeated Messenger start is single-wired, and stop/start discards partial
  parser, assembler, crypto, and send state.
- Malformed TLS input produces one immediate handshake failure with a useful
  diagnostic and does not wait for the generic session timer.
- Accepted and rejected channel-open responses are emitted on the requested
  service channel through both dispatch paths.
- RFCOMM listen failure retries, eventual success uses the returned nonzero
  port for SDP, stop cancels retries, and exhaustion emits one terminal error.
- Existing version exchange, successful TLS, service discovery, shutdown,
  sender filtering, media, input, focus, H.265 projection, A2DP, and wireless
  discovery behavior remains unchanged.

## Out of Scope

- Any file under `libs/prodigy-oaa-protocol/proto/`, AA protobuf semantics,
  USB transport, wireless bootstrap message formats, video flow control, input
  mapping, or protocol capture format.
- Bluetooth daemon restart, pairing policy, HFP/ofono, PipeWire routing, A2DP
  media behavior, logging configuration, External API, companion app, QML, or
  frozen numerics.
- General QObject ownership redesign, SDP session lifetime redesign, TLS
  certificate replacement, crypto hardening beyond handshake result reporting,
  or unbounded service supervision.

## Verification Matrix

### Required locally

- Focused protocol library tests for session FSM, messenger, cryptor, and
  control-channel routing.
- Focused application tests for orchestrator replacement/destruction and
  Bluetooth discovery retry/stop behavior.
- Full build, explicit `openauto-prodigy` target, full CTest,
  `git diff --check`, and documentation-link validation.
- Repository review gate with every result adjudicated, followed by an aarch64
  application cross-build.

### Required live after deployment

- Take a rollback snapshot without modifying the Pi checkout or its unrelated
  dirty QML/submodule state.
- Record application, hostapd, and Bluetooth PIDs; deploy only the reviewed
  binary and restart only the application.
- Complete wireless AA connection, project H.265, then exercise a deliberate
  app restart or replacement connection while AA is active and reconnect.
- Confirm one application process, responsive IPC, clean session negotiation,
  normal projection/input/audio, working RFCOMM discovery, and unchanged
  hostapd/Bluetooth PIDs.

### Optional live checks

- Repeat reconnect, background/foreground focus, and a second phone/player.
- Capture read-only journal evidence for graceful acknowledgement versus local
  timeout finalization.

### Not required

- Bluetooth daemon or hostapd restart, re-pairing, malformed live TLS injection,
  HFP call testing, protocol-schema capture, or unrelated QML inspection.

