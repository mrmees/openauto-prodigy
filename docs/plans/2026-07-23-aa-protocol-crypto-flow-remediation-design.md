# Android Auto Protocol Crypto and Flow-Control Remediation — Design

Date: 2026-07-23
Status: ACTIVE
Base: `origin/main` at `796b5c29f51e1b070af36ed1f756b65c6d9e861c`

## Outcome

The Android Auto protocol library fails closed at its OpenSSL and fragmented-
message boundaries, uses its configured liveness deadline, keeps the phone's
audio transmit window replenished, and preserves navigation guidance while the
phone is rerouting. The changes remain inside the existing wireless-AA protocol
contract: no protobuf, transport, codec, public application API, QML, or audio-
routing schema changes are required.

## Current-State Revalidation

| Area | Attempted refutation | Result | Remediation boundary |
|---|---|---|---|
| TLS initialization | Checked embedded material validity, current handshake failure reporting, cleanup, and every OpenSSL constructor/load return | Accepted. The shipped material is valid, but allocation, parse, install, key-match, BIO, and `SSL_new` failures remain unchecked | Make initialization transactional, return failure with a bounded diagnostic, and leave the object deinitialized |
| Encrypted runtime I/O | Checked the new fatal-handshake path, active-session watchdog, outer AA framing, and upstream aasdk per-frame encryption | Accepted for fatal read/write handling. The claimed need to carry partial TLS records across AA frames is not established: each encrypted AA frame is a complete cryptor call in both implementations | Check every established-session SSL/BIO operation. A non-complete encrypted AA frame fails the session immediately; do not invent cross-frame TLS buffering |
| Fragmented-message size | Checked parser buffering, serializer limits, assembler reset, protobuf limits, and concurrent channels | Accepted. FIRST declares a total size that the parser discards; the assembler has neither a per-message nor aggregate reservation limit and never verifies LAST reaches the declaration exactly | Carry the declared total into a bounded assembler, validate continuation flags and exact completion, and fail the malformed session |
| Audio receive permits | Compared the advertised window, current ACK cadence, video behavior, the verified ACK schema, and both local OpenAuto references | Accepted. Advertising ten permits but returning all ten only after exhaustion forces a round-trip stall at every window boundary | Keep the ten-frame headroom but return one permit for every accepted audio frame |
| Session liveness | Searched every `pingTimeout` use and traced Active entry, ping send, pong, disconnect, close, and restart | Accepted. `pingTimeout` is dead; a hard-coded miss count makes the actual deadline four intervals | Use one single-shot configured pong deadline, reset only while Active, and stop it at every terminal boundary |
| Navigation rerouting | Compared the handler with the generated enum contract and downstream clearing behavior | Accepted. Only ACTIVE is retained; REROUTING currently publishes inactive and clears guidance | Treat ACTIVE and REROUTING as navigation-present while keeping INACTIVE and UNAVAILABLE false |

## Design

### 1. Make the OpenSSL boundary transactional and explicit

`Cryptor::init()` returns success/failure and checks every allocation, PEM
parse, certificate/key install, private-key match, BIO creation, and `SSL_new`
operation. An overload that accepts explicit PEM bytes drives the same path so
tests can prove invalid material fails without mutating the production embedded
certificate or key. Any failure drains a bounded OpenSSL diagnostic, calls
`deinit()`, and leaves all owned pointers null.

Established-session encryption and decryption return a typed result plus output
instead of encoding failure as an empty byte array. Each SSL operation clears
the thread error queue immediately before the call and classifies the result
immediately afterward, as required by OpenSSL. Decryption drains the application
bytes from the structurally complete TLS record sequence carried by one
encrypted AA frame. A fatal, closed, or incomplete record is an error at this
boundary; no empty payload is
forwarded to the frame assembler. Encryption likewise rejects partial/fatal
writes before any frame from that logical message enters the transport queue.

Messenger emits one bounded `tlsFailed` notification per lifecycle. A session
in post-handshake negotiation, Active, or graceful shutdown transitions to the
appended `DisconnectReason::TlsError`; the existing handshake path retains
`HandshakeError`. Stop/restart clears both latches and all crypto state.

### 2. Enforce declared and aggregate fragmented-message bounds

`FrameHeader` carries the FIRST frame's unsigned 32-bit declared plaintext
message size after parsing; serialization remains wire-identical. The assembler
stores one structured partial-message record per channel: payload, declared
size, message type, and encryption type.

The default limits are 16 MiB for one assembled message and 32 MiB of declared
in-flight fragmented messages across all channels. They are deliberately above
expected 1080p compressed frames and protobuf metadata while remaining bounded
on a 4 GB Pi. A configurable constructor seam allows small deterministic tests
without allocating production-sized buffers.

A FIRST frame is rejected if its declaration is zero, no larger than its first
fragment, over the per-message cap, or over the aggregate reservation budget.
MIDDLE/LAST flags must match the FIRST frame. Appends may not reach or exceed the
declared total before LAST, and LAST must finish at exactly the declared total.
Duplicate FIRST replaces and releases the old reservation before the new one is
validated. Reset and every rejection release payload and reservation state.

An assembly failure is surfaced once through Messenger and terminates the
session with the appended `DisconnectReason::ProtocolError`; malformed input is
never silently converted into an empty or partial application message.

### 3. Correct flow-control and state semantics at their owners

Audio setup continues advertising `max_unacked = 10`, preserving pipeline
headroom. Every accepted media frame immediately emits an ACK with
`ack_count = 1`, matching the verified permit semantics and the existing video
handler. The unused counter and duplicated numeric literal are removed.

`AASession` owns a single-shot pong-deadline timer in addition to the periodic
ping timer. Entering Active sends the first ping immediately and starts the
periodic cadence. A ping arms the deadline only when none is outstanding; a
pong while Active clears it only when its echoed timestamp matches a ping sent
in the current deadline window. Disconnect, finalization, shutdown, or restart
stops both timers and clears the outstanding timestamps. Expiry produces exactly one `PingTimeout` disconnect at
`SessionConfig::pingTimeout`, independent of `pingInterval`.

Navigation state handling uses generated enum names. ACTIVE and REROUTING map
to observable active navigation; UNAVAILABLE and INACTIVE map to false. Signals
remain change-gated, so ACTIVE-to-REROUTING does not duplicate notification or
clear the downstream guidance snapshot.

## Failure and Notification Semantics

- Initialization failure: no active TLS object, one handshake failure, no
  transport write from the failed attempt.
- Runtime TLS failure: one `tlsFailed`, one `TlsError` disconnect, then the
  existing session finalization path.
- Assembly violation: one protocol error for the offending frame, one
  `ProtocolError` disconnect, all partial state released.
- Ping expiry: one `PingTimeout` disconnect; late pongs cannot revive it.
- Audio and navigation signals retain existing channels and payload schemas.

## Out of Scope

- Any edit under `libs/prodigy-oaa-protocol/proto/` or `proto/api/`.
- TLS versions, cipher selection, embedded credential identity, certificate
  verification policy, or Android Auto authentication schema.
- USB transport, wireless discovery, TCP socket policy, service discovery, or
  channel-open routing.
- Video ACK policy, decoder behavior, audio buffering, PipeWire routing, EQ,
  HFP, ofono, Bluetooth AVRCP, QML, logging configuration, External API, or the
  companion app.
- Touch, input, video, or night-state findings assigned to the next wave.

## Verification and Live Acceptance Matrix

| Check | Level | Acceptance |
|---|---|---|
| Invalid/valid TLS material and lifecycle | Required local | Invalid PEM and mismatched material fail transactionally with diagnostics; valid client/server handshake and restart remain green |
| Runtime TLS read/write failure | Required local | Fatal encrypted I/O emits once, forwards no payload/frame, and closes with `TlsError` |
| Fragment assembly limits | Required local | Exact declared completion succeeds; zero/short/oversize/mismatch/aggregate cases fail and release all state |
| Audio ACK cadence | Required local | Setup advertises ten; every accepted frame returns exactly one permit without waiting for exhaustion |
| Ping deadline | Required local | Different interval/timeout combinations expire by timeout, matching pongs clear it, stale/mismatched pongs do not, and terminal states cancel it |
| Navigation rerouting | Required local | ACTIVE→REROUTING remains active with no duplicate false edge; INACTIVE/UNAVAILABLE clear once |
| Focused and repository gates | Required local | Focused tests, full build, explicit app target, full CTest, docs links, and `git diff --check` pass; every review finding is adjudicated |
| aarch64 application | Required before deploy | `./cross-build.sh` succeeds from the reviewed commit |
| Pi deployment health | Required live | Snapshot prior binary; deploy/restart only the app; one process owns responsive IPC; hostapd and Bluetooth retain PID/restart state |
| Wireless AA regression | Required live | Pixel reconnects, TLS/auth/service channels complete, H.265 reaches first frame, and no TLS/protocol/ping failure appears |
| AA audio | Required live when phone supplies media | Sustained media playback remains audible without periodic permit-window stalls or new channel errors |
| Rerouting transition | Optional live | A safe stationary route/replay shows guidance remains present through REROUTING |
| Fault injection, second phone, service restarts | Not required | No malformed live TLS injection, Bluetooth restart, re-pairing, HFP call, or unrelated service operation |

Matthew's standing Pi authorization covers the scoped binary deployment and
application restart. No Bluetooth/hostapd restart or unrelated checkout
mutation is planned.
