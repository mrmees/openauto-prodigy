# Audio and Equalizer Real-Time Safety Remediation — Design

Status: COMPLETED 2026-07-23
Date: 2026-07-23
Grounded against: `origin/main` at `1240f423a91c9a355f56e0fc9b33534157a37f8b`

## 1. Objective

Repair the bounded set of audio/equalizer safety defects selected by the
private remediation consolidation map without changing the product's routing,
focus, protocol, or user-interface architecture. The work has two ownership
boundaries:

1. the producer/PipeWire real-time boundary (`AudioRingBuffer` and
   `AudioService`); and
2. the Qt/equalizer real-time boundary (`EqualizerService` and
   `EqualizerEngine`).

This is one consolidated implementation tranche and one pull request. It is
not ten independent feature rounds.

## 2. Confirmed current-state defects

Revalidation against the grounded commit confirmed all ten premises:

- An overflowing ring-buffer writer and the PipeWire reader both update the
  SPA read cursor. Their non-CAS stores can regress or skip the cursor.
- Playback validates the dequeued `pw_buffer` only. An unmapped
  `datas[0].data` reaches the unconditional silence fill and crashes.
- Enum-typed equalizer invokables can index beyond the three stream states.
- User-preset rename accepts empty and duplicate target names.
- Adaptive buffer growth is unreachable (`bufferMs >= 500`, cap `100`) and a
  handle-only mutation could not affect a later stream even if reached.
- The PipeWire process callback allocates through `QString::toUtf8()` and
  performs blocking `fprintf` diagnostics.
- Equalizer coefficient double-buffer recycling permits a writer to mutate a
  buffer while the RT reader copies it after two rapid publications.
- PipeWire stream errors are discarded when a stream has no recovery hook.
- Playback buffer sizing accepts unbounded configuration and can overflow its
  arithmetic/power-of-two loop or terminate on allocation failure.
- Equalizer construction/config restoration arms the save timer, enabling a
  no-user-action rewrite of validator-pruned configuration.

The focused baseline tests are green but do not exercise these contracts.

## 3. Decisions

### 3.1 Ring overflow is drop-newest

`AudioRingBuffer` remains a lock-free single-producer/single-consumer queue.
The producer exclusively owns the write cursor; the consumer exclusively owns
the read cursor. When there is insufficient free space, `write()` copies only
the bytes that fit and returns that short count. The unwritten tail is dropped.

This is the standard safe SPSC ownership model. It deliberately replaces the
unsafe drop-oldest behavior. Existing callers already accept a short write:
local media reports it, while AA and the BT tap may discard excess input. No
mutex or allocation enters either audio callback.

`drain()` becomes reader-owned and is safe with a live writer, although the BT
tap keeps its stronger fully-quiesced transition ordering.

### 3.2 Playback buffers are structurally validated

The real callback continues to dequeue and requeue the actual PipeWire buffer.
The buffer-filling body is factored into a private static seam used by the
callback and its tests. It rejects a missing `spa_buffer`, zero data planes,
missing `datas`, null data, null chunk, invalid stride, or unusable capacity.
The callback requeues every successfully dequeued buffer even when rejected.

Capture behavior is unchanged except for matching structural guards if the
same unchecked container pointers are encountered while editing the shared
boundary.

### 3.3 Buffer sizing is static, bounded, and truthful

The configured target is a static creation-time buffer. Runtime resizing is
not introduced. The dead adaptive-growth timer, handle fields, and no-op
configuration accessor are removed.

Playback buffer targets are clamped to 500–5000 ms. The documented/default
value becomes 500 ms, matching the existing effective floor. Size arithmetic
uses a bounded 64-bit calculation before conversion and power-of-two rounding;
unsupported sample rates or impossible sizes fail stream creation cleanly.
Allocation failure is caught and reported as stream-creation failure.

The obsolete `audio.adaptive` default/schema entry is removed. An old key in a
user file remains harmless unknown configuration; it never controlled runtime
behavior in the current implementation.

### 3.4 RT diagnostics are published, not printed

The PipeWire process callback publishes only primitive atomic counters and
scaled integer snapshots. A Qt-owner-thread diagnostic timer consumes them and
uses category logging. The callback performs no string conversion, formatted
I/O, allocation, mutex acquisition, or Qt logging.

The dead growth timer may be replaced by this diagnostic timer; it is not used
to mutate stream sizing.

### 3.5 Stream errors are always attributable

On `PW_STREAM_STATE_ERROR`, the PipeWire callback queues a copied stream name
and error description to the application thread for warning output. Existing
per-stream recovery hooks are still dispatched against their receiver context
and keep their current auto-cancellation behavior. Diagnostics do not depend on
a recovery hook being installed.

### 3.6 Equalizer stream values are checked at every public boundary

Only numeric values 0–2 are valid `StreamId` inputs. Public enum-facing
operations check before indexing. Invalid mutations become no-ops; getters
return neutral values; `saveUserPreset` returns an empty result; and
`acquireEngine` returns null. Internal accessors assert their valid precondition.
The frozen numeric order and `Phone = System` alias do not change.

### 3.7 Preset rename preserves a unique, non-empty namespace

Rename rejects whitespace-only/empty targets, bundled preset names, and names
owned by another user preset. Renaming a preset to its current exact name is a
successful no-op. Existing case-sensitive name semantics remain unchanged.

### 3.8 Loading is save-free

Construction and `loadFromConfig()` run under an explicit restore guard.
Operations used to seed defaults or apply restored presets do not arm the save
timer or mark the service dirty. Only a later user mutation becomes writable;
`saveNow()` flushes pending dirty state and is a no-op after a clean restore.
The validator continues to reject malformed gain arrays without mutating the
raw YAML merely because the application booted.

### 3.9 Coefficients use an atomic snapshot mailbox

Non-RT mutations are serialized with a control-side mutex. They calculate a
complete coefficient set off the RT thread, then publish each primitive through
lock-free atomics bracketed by an even/odd generation sequence.

The RT callback makes one bounded snapshot attempt. It accepts the snapshot
only when the generation is unchanged and even; otherwise it defers the update
to the next process callback. It never spins, locks, allocates, logs, or touches
storage being written non-atomically. Existing interpolation, bypass crossfade,
filter state, limiter behavior, and per-consumer engine ownership remain.

The target platforms must provide always-lock-free atomics for the published
primitive types; this is enforced at compile time.

## 4. Acceptance contracts

- Concurrent ring writes/reads never share cursor ownership, never report more
  than capacity, and preserve the exact sequence of accepted bytes.
- Overflow returns a short write and increments diagnostics without replaying
  consumed audio.
- Malformed/unmapped playback buffers are rejected without dereference and are
  requeued by the real callback path.
- Playback always fills a valid requested period, bounded by `d.maxsize`.
- Static buffer configuration cannot overflow, loop indefinitely, or request an
  unbounded allocation; documented defaults match runtime behavior.
- The PipeWire process callback contains no allocation, mutex, or logging path.
- Every stream error produces an application-thread diagnostic, with existing
  BT recovery behavior unchanged.
- Every public equalizer operation is safe for invalid stream values.
- Preset names remain non-empty and unique across bundled/user namespaces.
- Loading valid or malformed EQ configuration does not schedule a write.
- Concurrent coefficient publication and processing produces finite bounded
  output and never exposes a torn coefficient set.
- Existing focus, EQ response, bypass, presets, persistence after user changes,
  AA audio, local media, BT A2DP tap, and capture behavior remain green.

## 5. Dynamic verification

Focused tests exercise the real ring methods, the factored real playback-buffer
body, stream-state callback, sizing helper, public equalizer methods, load/save
timer behavior, and concurrent engine mutation/processing. Concurrency tests run
repeatedly before the full suite.

The final gate is:

1. focused target builds and tests;
2. repeated ring/engine stress tests;
3. full local build;
4. explicit `openauto-prodigy` target build;
5. full `ctest --output-on-failure`;
6. `git diff --check`;
7. repository review gate with every finding adjudicated;
8. aarch64 cross-build; and
9. binary-only Pi deployment and live smoke checks.

## 6. Pi/live acceptance matrix

### Required after deployment

- Preserve a rollback copy of the binary and the exact configuration.
- Deploy only the cross-built application binary and restart only the
  application service.
- Exactly one application process owns responsive IPC and reports the new
  version.
- Bluetooth and hostapd retain their PIDs and restart counts.
- Normal connected-phone audio playback proceeds without new PipeWire stream
  errors, crashes, or restart loops.
- Wireless AA reconnects normally when the phone is available; otherwise the
  unavailable phone-dependent row is reported honestly.

### Optional evidence

- Change EQ gains/presets repeatedly during active playback and listen for
  discontinuity or corruption.
- Exercise both BT A2DP and AA/local playback sources.
- Observe a deliberate short-write/overrun diagnostic under synthetic load.

### Not required

- Bluetooth daemon restart, re-pairing, HFP call testing, AA protocol capture,
  routing-rule changes, or malformed live PipeWire buffer injection.

Matthew's standing authorization covers application deployment/restart and Pi
operations for this wave. Avoid unrelated Pi checkout changes and unnecessary
service disruption.

## 7. Out of scope

- Android Auto protocol, protobuf, channel, transport, video, or input changes.
- `proto/api/`, External API semantics, or JS-shim capabilities.
- Bluetooth discovery/AVRCP state, HFP, ofono, or PipeWire routing rules.
- QML or visual changes.
- Logging configuration registration/persistence.
- A new adaptive/live-resizing audio feature.
- Wishlist items such as local-media flush-on-seek.

All project rails in `AGENTS.md` and `src/AGENTS.md` remain binding.
