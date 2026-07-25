# Android Auto Assistant Microphone Transport — Design

Date: 2026-07-24
Status: COMPLETED 2026-07-24
Grounded against: `dev` at `975b3ef7c9e354d54e06cd90830068dbb30ec377`

## Goal

When Android Auto opens Prodigy's AVInput service for Assistant or voice
search, capture the configured Pi microphone as 16 kHz, mono, signed 16-bit
PCM; apply the configured microphone gain; and deliver bounded, low-latency
audio to the phone until Android Auto closes the input or the session ends.

This promotes the first qualified item from `docs/wishlist.md`. It fits the
current Prodigy stack and the required microphone is attached to the Pi bench.

## Current-State Findings

The required pieces exist, but they do not form a safe production path:

- `ServiceDiscoveryBuilder` already advertises AVInput channel 7 as 16 kHz,
  mono, 16-bit PCM, and `AndroidAutoOrchestrator` registers the persistent
  `AVInputChannelHandler` with each session.
- `AVInputChannelHandler` acknowledges phone open/close requests and can frame
  timestamped PCM, but nothing consumes `micCaptureRequested`. It reports
  success before capture exists, accepts data without enforcing the phone's
  `max_unacked` window, and currently ignores acknowledgements.
- `AudioService` can create the required PipeWire capture stream with its
  callback installed before connection. That callback runs on PipeWire's
  real-time thread; calling a Qt signal, `Messenger`, `AASession`, or
  `QTcpSocket` from it would violate the AA thread-affinity contract.
- `audio.microphone.device` already selects the PipeWire input globally, and
  `audio.microphone.gain` is exposed in settings, but the gain has no production
  consumer.
- Session teardown finalizes persistent channel handlers before destroying the
  transport, but owns no microphone handle and therefore cannot yet quiesce a
  capture callback before the handler loses its wire connection.

Two independent references resolve the principal protocol ambiguity without a
proto change. The recovered `AVInputOpenRequest` includes `max_unacked`, and
the established aasdk/OpenAuto AVInput implementation receives
`AV_MEDIA_ACK_INDICATION` from the phone on this service, defers the open
response until capture start succeeds, uses response value `0` for success and
`1` for failure, and sends timestamped microphone frames in microseconds. The
generic recovered ACK documentation describes the ordinary phone-to-HU sink
direction; it does not invalidate the explicit AVInput source-channel receive
path.

## Design

### 1. Keep protocol state and capture policy separate

`AVInputChannelHandler` remains the owner of AVInput request/response state,
the session value, and transmit permits. A synchronous capture-controller
callback supplied by the application handles `open=true` and `open=false`.
The handler invokes it before replying, so an immediate PipeWire open failure
returns response value `1`; success and idempotent close return `0`.

The callback is intentionally synchronous. `AASession`, the handler,
`AndroidAutoOrchestrator`, and `AudioService` lifecycle calls all run on their
shared Qt event-loop thread, and `openCaptureStreamWithOptions()` reports
immediate creation/connect failure without blocking on audio delivery. A
later PipeWire stream error is a runtime failure rather than a second open
response: Prodigy closes the local capture, disables AVInput sending, records
the error, and allows the phone's next open request to retry. It does not
invent an unsolicited response message.

A replacement `open=true` starts a fresh capture generation: any prior handle
is closed first, the PCM bridge is purged, and the permit count is reset. A
close while already inactive succeeds without side effects. Channel close and
session teardown use the same idempotent stop path.

### 2. Enforce the phone's AVInput transmit window

For each successful open, the handler resets outstanding sends and adopts a
positive `max_unacked` capped at 24, the recovered protocol's established
upper bound; a missing or non-positive value falls back to one.
`sendMicData()` runs only on the Qt owner thread, returns whether it accepted a
frame, and refuses to emit after close or once outstanding frames reach the
negotiated window.

`AV_MEDIA_ACK_INDICATION` is parsed rather than ignored. Repeated receive
timestamps take precedence as the acknowledged-frame count; otherwise a
positive `ack_count` is used. Acknowledgements for another session, malformed
messages, zero counts, and counts beyond the outstanding total cannot create
permits. When a valid acknowledgement changes a full window into an available
one, the handler notifies the application-side bridge to resume draining.

The existing session value remains zero for compatibility with the established
implementation. No protobuf definition, field, numeric, or wire message ID
changes.

### 3. Cross the real-time boundary through a bounded PCM bridge

A new application-side `AVInputCaptureBridge` owns a fixed-capacity SPSC byte
ring and a Qt-owner drain timer. Its PipeWire producer entry point only checks
atomics, copies bytes into already allocated storage, and increments primitive
drop counters. It performs no allocation, logging, locking, Qt invocation, or
protocol/socket work.

The Qt consumer emits 20 ms packets: 640 bytes at 16 kHz, mono, S16LE. It
applies gain sample-by-sample with signed 16-bit saturation, obtains a
microsecond monotonic timestamp at send time, and calls the handler on the Qt
thread. Normal PipeWire batches remain FIFO and drain one frame per timer tick;
a 4 KiB ring bounds queued capture to approximately 128 ms. If the producer
overruns that ring or the protocol window refuses a packet, the consumer
discards stale queued PCM before the next send opportunity; live voice must
recover at the present rather than replay delayed speech. Drop diagnostics are
aggregated and logged only by the Qt consumer.

The bridge snapshots gain when a capture generation starts. Non-finite or
out-of-settings-range input falls back/clamps to the documented 0.5–4.0 range;
valid configured values are otherwise exact. Gain processing is outside the
real-time callback and has deterministic unit coverage for positive/negative
samples and saturation.

### 4. Give capture teardown first ownership

`AndroidAutoOrchestrator` owns the PipeWire capture handle and bridge. On a
successful open it starts a new bridge generation, then creates an
autoconnecting `AA Assistant Microphone` capture stream with the existing
configured input device and an immutable pre-connect callback. If stream
creation fails, it purges the bridge and reports failure to the handler.

Every stop boundary closes the PipeWire handle first. `AudioService` disables
and quiesces the real-time callback synchronously; only then does the
orchestrator stop/purge the bridge and reset handler permit state. Session
teardown performs this sequence before `AASession::finalize()` disconnects the
persistent handler from the messenger. Generation checks make a queued runtime
error or stale drain from an older capture a no-op after replacement.

The concrete `AudioService` options path is required in production because it
installs the callback before PipeWire connects. A non-concrete test double does
not silently use the race-prone legacy set-after-open path; it reports open
failure unless a focused test exercises the bridge independently.

## Failure Semantics

- Immediate PipeWire create/connect failure: no published capture handle, no
  active bridge, open response value `1`, and no PCM frames.
- Runtime PipeWire error: capture-first local teardown, handler sending
  disabled, one bounded diagnostic, and retry on a later phone open request.
- Invalid or missing transmit window: one outstanding frame maximum.
- Missing/malformed/stale acknowledgement: no new permit and no counter
  underflow; new live PCM is dropped rather than buffered without bound.
- Phone close, channel close, disconnect, reconnect, application stop, or
  destructor: capture callback quiesced, queued PCM purged, permits reset, and
  no frame crosses into the next session/generation.

## Out of Scope

- Any edit under `libs/prodigy-oaa-protocol/proto/` or `proto/api/`.
- HFP/SCO call uplink, phone-call routing policy, Bluetooth roles, ofono, or
  changes to AA media/speech/system playback and focus/ducking.
- Acoustic echo cancellation or active noise cancellation. The phone's `ec`
  and `anc` request flags remain observable diagnostics, not implemented DSP.
- A general PipeWire reconnect framework, generic capture API redesign,
  microphone calibration UI, automatic gain control, noise suppression, or
  equalization.
- Companion-app microphone/GPS work, dashboard widgets, GPIO/backend work,
  USB Android Auto, or any other wishlist capability.

## Verification and Live Acceptance Matrix

| Check | Level | Acceptance |
|---|---|---|
| AVInput response and permits | Required local | Capture controller precedes response; values 0/1 are correct; missing/invalid windows fall back to one; ACKs replenish only valid outstanding permits |
| PCM bridge | Required local | Producer path is bounded and non-Qt; 20 ms frames, gain, saturation, overflow purge, window refusal, stop, and generation replacement are deterministic |
| Orchestrator lifecycle | Required local | Successful open owns one capture; immediate failure owns none; close/error/teardown are capture-first and idempotent; reconnect cannot reuse stale PCM |
| Repository gates | Required local | Focused tests, full build, explicit app target, full CTest, docs links, and diff checks pass |
| Review gates | Required before push | Repository Codex review is adjudicated, then Fable performs the final pre-push review and every finding is adjudicated |
| aarch64 application | Required before deploy | `./cross-build.sh` succeeds from the reviewed commit |
| Assistant voice | Required live | Pixel opens AVInput, Prodigy returns success, spoken queries are recognized, and Assistant responds through the existing speech path |
| Repeated lifecycle | Required live | At least five Assistant open/close cycles plus disconnect during an active capture leave no capture node, crash, stale audio, or failed reconnect |
| Device and gain | Required live | The configured Pi microphone is the capture source; changing gain produces the expected relative PCM level without clipping/wraparound |
| Pi health | Required live | One responsive application process remains; hostapd and Bluetooth retain their PIDs and restart counts; wireless H.265 projection still works |
| AEC/ANC and other phones | Not required | No DSP claim, second-phone matrix, HFP call, service restart, re-pairing, or unrelated hardware operation |

Matthew's standing Pi authorization covers the scoped binary deployment and
application restart after the local and review gates. The implementation does
not restart Bluetooth or hostapd and preserves unrelated Pi checkout state.
