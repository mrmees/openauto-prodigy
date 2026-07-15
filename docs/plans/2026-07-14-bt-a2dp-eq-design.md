# BT A2DP Through the Equalizer (+ EQ Hygiene Riders) — Design

Status: ACTIVE
**Date:** 2026-07-14 · **Grounded against:** `dev` at `7b5d9dd`.
**Origin:** EQ parity audit 2026-07-14 (session-handoffs entry; wishlist § "From
EQ parity audit (2026-07-14)"). Promoted by Matthew same day: BT A2DP → EQ,
persistence fix, Phone→System relabel. Web EQ editor stays parked. Milestone
tag + dev→main PR follow this work.
**Codex spec review (gpt-5.6-sol, 2026-07-14):** round 1 verdict REWORK —
6 P1 / 8 P2 / 1 P3; all accepted and incorporated below except the
deprecated-QML-alias half of one P2 (dismissed: no external consumers exist at
alpha; in-tree QML migrates in the same commit; the `Phone = System` enum
alias IS kept for plugin source compat).

## 1. Problem

BT A2DP music routes BlueZ → PipeWire natively (`bluez_input.*` node
auto-linked to the hardware sink); it never passes through the app. Three
consequences, all confirmed live on the Pi (2026-07-14):

1. **No EQ** — the Media curve governs AA media + local media only.
2. **No master volume** — volume is applied per app stream
   (`SPA_PROP_channelVolumes` in `AudioService::setMasterVolume`), so the HU
   volume control does not affect BT playback at all (phone-side only).
3. **No focus arbitration** — speech/nav prompts do not affect BT music
   (`applyDucking` only reaches AudioService streams).

Original OpenAuto Pro applied a sink-level 15-band LADSPA EQ (and Pulse sink
volume), which governed all three for every source.

Adjacent latent defects that this work fixes because they become load-bearing:

- `EqualizerEngine` is stateful per instance (per-channel biquad chains,
  interpolation state, soft limiters), but the single Media engine instance is
  **shared** by two concurrent consumers — AA media
  (`AndroidAutoOrchestrator.cpp:339`) and the local media player
  (`PlaybackEngine.cpp:131` via `MediaPlayerPlugin.cpp:49`). Simultaneous
  playback corrupts filter state. BT would be a third consumer.
- `AudioService::createStream` never applies the current master volume to a
  newly created stream (`setMasterVolume` only iterates existing streams) — a
  stream created after boot-time volume restore plays at PipeWire's default
  volume until the user next touches volume. Affects AA streams today;
  would affect the BT tap worse (created at startup).
- EQ persistence never reaches disk: `EqualizerService::writeToConfig`
  mutates the in-memory `YamlConfig` only, and `setBypassed` doesn't even arm
  the save debounce. In a vehicle, power-cut is the normal shutdown — the
  whole persistence path must be made durable, not just extended.

## 2. Decisions (Matthew, 2026-07-14)

- **Approach A** — app-side loopback tap (below). Filter-chain sink rejected
  (second EQ implementation to keep matched, no volume/focus fix); app-owned
  global sink rejected (AA latency risk, double-EQ, blast radius).
- **BT follows the Media curve.** No fourth EQ tab, no new curve config. The
  curve compensates for cabin/speakers, not the source.
- Riders confirmed: engine-instance fan-out fix; persist unsaved gains +
  bypass (now: durably, to disk); `Phone` → `System` relabel with config-key
  migration.

## 3. Architecture

```
phone A2DP ──> bluez_input.* node   (media.class Stream/Output/Audio)
                  │  WirePlumber rule: target.object = "openauto-bt-eq-in"
                  │  (fallback to default sink when the target is ABSENT)
                  ▼
   app capture stream "openauto-bt-eq-in"
     · dedicated NON-AUTOCONNECT capture mode: no PW_STREAM_FLAG_AUTOCONNECT,
       inputDevice_ ignored — the ONLY way audio reaches it is a peer
       (the bluez stream) targeting it by name. Never mic-linked.
     · S16, 48 kHz, stereo requested; PipeWire converts/resamples A2DP source
       formats (44.1 kHz typical)
                  │  RT memcpy (capture process callback)
                  ▼
            AudioRingBuffer
                  │  existing playback process path:
                  ▼  ring read → EQ hook → focus gain → output
   app playback stream "BT Audio"
     · role Music, adaptive rate matching DISABLED (see §3.3)
     · INACTIVE until an A2DP transport is acquired (see §3.2)
                  │
                  ▼
            hardware sink
```

### 3.1 Bring-up / teardown ordering (the no-silent-failure contract)

WirePlumber's fallback protects us only while the capture target is *absent*.
A live capture node with a broken downstream would silently eat BT audio.
Therefore, strictly ordered:

- **Bring-up:** acquire EQ engine → create playback stream (inactive) →
  wire ring + callbacks → **only then** create the capture node (publishing
  the target). Any failure before the capture exists leaves BT audio on the
  direct path.
- **Teardown / error:** destroy the capture node **first** (WirePlumber
  relinks the bluez stream to the default sink), then playback, then release
  the engine. Any playback/EQ error at runtime triggers the same
  capture-first teardown. Stream error surfacing uses
  `pw_stream_events.state_changed` (new in AudioService — currently no
  state-change handling exists).

### 3.2 Stream lifecycle and idle economics

The **capture** node is permanent (created at startup per §3.1) — it must
pre-exist any `bluez_input` appearance, because a lost WirePlumber linking
race sticks the bluez node to the hardware sink for the session. A
non-autoconnected capture with no peer is truly idle.

The **playback** stream is NOT permanently active: our playback callback
always emits full periods (silence-filling underruns), which keeps the graph
running, the sink awake, and CPU busy. So: playback stream exists but
`pw_stream_set_active(false)`; `BtAudioPlugin`'s existing A2DP transport
D-Bus watch toggles it — transport acquired → reset ring, activate;
transport released → deactivate, reset ring. Toggling *activity* does not
create/destroy the capture node, so the linking race stays closed. Bench
measures idle CPU and sink suspend state (§7).

### 3.3 Clock domains

The playback callback's ring-fill PI controller + `pw_stream_set_rate`
exists for AA network audio (independent phone clock). The BT tap's producer
and consumer live in the same PipeWire graph — the controller would fight
PipeWire's own resampler. `createStream` gains a per-stream option to
disable adaptive rate matching; the BT tap sets it. (Bench watches long-run
ring fill / drops / pitch anyway; if evidence says otherwise, re-enable.)

### 3.4 WirePlumber rule (exact contract)

Shipped by both installers to
`/etc/wireplumber/wireplumber.conf.d/50-openauto-bt-eq.conf` (WirePlumber
0.5 SPA-JSON fragment — appends to, never overrides, `monitor.bluez.rules`):

```
monitor.bluez.rules = [
  {
    matches = [
      {
        node.name = "~bluez_input.*"
        media.class = "Stream/Output/Audio"
      }
    ]
    actions = {
      update-props = {
        target.object = "openauto-bt-eq-in"
      }
    }
  }
]
```

The `media.class` gate matters: targeting properties only act on stream
nodes, and the rule must never catch a device-node form. **Plan-time gate:**
capture `pw-dump` of the actual `bluez_input` node on the Pi during A2DP
playback and confirm `node.name` / `media.class` / absence of
`node.dont-fallback` before freezing the fragment. Never set
`node.dont-fallback` — fallback IS the failure mode.

### 3.5 Focus policy (corrected)

`applyDucking` semantics today: the focus holder plays at base volume;
with GAIN focus held, **all other streams mute to 0.0** (speech focus ducks
others to 0.2). Streams do not mix, and priority only orders focus-holder
selection. The original spec's "sources mix" was wrong.

Policy: the BT tap **participates in the existing focus system** — it
requests GAIN focus when its transport activates (§3.2) and releases on
transport release. Consequences, intentional: starting BT playback silences
AA media (and vice versa — last claimant wins, matching user intent of
"I just started playing this"); speech/nav prompts duck BT to the same 0.2
factor as other music. Exact focus API wiring is read at plan time from the
`applyDucking`/focus-request call sites.

## 4. Components

### 4.1 AudioService: capture refactor (second slot + safety)

`openCaptureStream` currently has a single `capture_` slot (AA mic) with a
`callbackActive` flag that does NOT make the `std::function` handoff safe
(RT thread can observe active, main thread clears the callback before
invocation). The refactor that adds the second slot fixes this pattern:

- Per-handle capture state: own listener, own callback, own userdata.
- Callback installed immutably **before** `pw_stream_connect`; never
  swapped afterward.
- Close path quiesces the stream under `pw_thread_loop_lock` (disconnect +
  destroy) before any callback storage is touched.
- New **non-autoconnect targeted-capture mode** (for the BT intake): omits
  `PW_STREAM_FLAG_AUTOCONNECT`, ignores `inputDevice_`, exposes input ports
  only. The AA mic slot keeps today's autoconnect+target semantics
  unchanged.

### 4.2 AudioService: volume-at-creation fix

`createStream` applies the current cubic master volume to the new stream
before returning (under the existing PipeWire lock, same order as
`setMasterVolume` to preserve the documented ABBA-deadlock avoidance).
Fixes the latent AA gap and the BT tap's boot case, including boot with
persisted volume 0.

### 4.3 BT tap glue (new, small)

A `BtAudioTap` in `src/plugins/bt_audio/`, owned by `BtAudioPlugin` (the
BT-audio domain home; it reaches AudioService/EqualizerService via
IHostContext like MediaPlayerPlugin does), that:

- implements §3.1 ordering, §3.2 activity toggling (driven by the plugin's
  existing TransportAdded/TransportRemoved D-Bus signals), and focus
  requests per §3.5,
- acquires a Media-curve engine (`acquireEngine(StreamId::Media, 48000, 2)`)
  and attaches it per the §4.4 ordering contract,
- releases everything capture-first on shutdown.

Factor the lifecycle/state machine (bring-up steps, error → teardown,
activity transitions) into a plain testable class; PipeWire graph behavior
itself is a bench item.

### 4.4 EqualizerService: engine fan-out (rider)

- New concrete-class API: `EqualizerEngine* acquireEngine(StreamId, float
  sampleRate, int channels)` / `void releaseEngine(EqualizerEngine*)`. Each
  call returns a dedicated instance initialized from the stream's current
  gains **and bypass**. `setGain` / `applyPreset` / `setBypassed` fan out to
  every live instance of that StreamId. Acquire/release/fan-out all run on
  the Qt owner thread (document it; assert if cheap).
- **RT ordering contract** (prevents use-after-free; also fixes the existing
  attach-after-connect pattern): the engine pointer is attached to a stream
  handle **before** `pw_stream_connect`, or via an AudioService setter that
  runs under `pw_thread_loop_lock`; a stream is fully destroyed/quiesced
  **before** its engine is released. Existing consumers
  (AndroidAutoOrchestrator, PlaybackEngine) migrate to acquire/release and
  to this ordering; `engineForStream` is then removed (all callers in-tree;
  it is not part of `IEqualizerService` — no interface change, no
  `HOST_API_VERSION` bump).
- `StreamState` gains an authoritative `bool bypassed` (today the embedded
  engine is the only bypass source of truth, and the fan-out list can be
  empty). New engines initialize from it; `isBypassed` reads it;
  `setBypassed` updates it before fan-out **and arms the save debounce**
  (today it doesn't).

### 4.5 Persistence rider — durable this time

- `EqualizerService` gains a disk-flush dependency (via `IConfigService` or
  a `YamlConfig::save` call — planner picks the one matching how ThemeService
  et al. persist): the 2 s debounce and `saveNow()` (aboutToQuit) both write
  the file, not just the in-memory tree. Power-cut durability = last change
  older than the debounce is on disk.
- Written per stream, always: preset name (as today), `gains: [10 floats]`,
  `bypassed: bool`.
- Load order per stream: named preset (bundled or user) wins if set; else
  raw `gains`; `bypassed` always restored. Missing keys ⇒ current defaults
  (Flat/Voice/Voice, bypass off).
- **Validation:** a gains entry must be exactly 10 finite numbers; each
  value clamps to ±12 dB; NaN/inf/non-scalar/short/long arrays reject the
  array (fall back to the preset-or-default path). No NaN may ever reach
  coefficient generation.

### 4.6 Relabel rider: Phone → System

- `enum class StreamId { Media, Navigation, System, Phone = System }` —
  value stays 2; the deprecated `Phone` alias keeps external plugin source
  compiling (`StreamId` is declared in the public `IEqualizerService.hpp`).
  In-tree code migrates to `System`.
- Config stream key migration happens on the **raw user YAML before the
  defaults merge** (once defaults contain `system`, a post-merge
  "read system, fall back to phone" can never trigger — the injected
  default `system` always exists, and `eqStreamPreset` can't distinguish
  absence). Rule: user file has `phone` and no `system` → copy the block to
  `system`, delete `phone`; both present → keep `system`, delete `phone`.
  Serialized output is tested, not just in-memory reads.
- UI: `EqSettings.qml` segment `["Media", "Nav", "Phone"]` →
  `["Media", "Nav", "System"]`; `phonePreset` Q_PROPERTY + signal renamed to
  `systemPreset` and QML consumers updated in the same commit. Declared
  source break for any external QML reader of the global context property —
  accepted (alpha, no external consumers; noted in handoff).
- The engine attachment at `AndroidAutoOrchestrator.cpp:343` (AA System
  stream ← formerly-"Phone" engine) becomes honest instead of mislabeled.
  Real call audio (HFP SCO) remains outside AudioService by design
  (2026-07-05 HFP decision) — out of scope here.

## 5. Behavior changes (user-visible, document in handoff + docs)

1. BT music obeys the Media EQ curve (preset swaps audible mid-playback).
2. HU master volume now controls BT playback level (and volume-at-creation
   fixes the latent AA boot-volume gap).
3. BT music joins focus arbitration: speech/nav prompts duck it; starting BT
   playback silences AA media and vice versa (last claimant wins — no more
   two-sources-mixing).
4. EQ slider positions and bypass survive restart — including power-cut —
   without saving a preset.
5. The third EQ tab reads "System" (AA system sounds), not "Phone".

## 6. Error handling

- Rule missing / app down / capture absent → WirePlumber fallback: BT direct
  to sink (audible, un-EQ'd).
- Partial bring-up or runtime stream error → capture-first teardown restores
  the direct path (§3.1). Failure-injection tests cover every partial step.
- Ring overrun drops oldest; underrun silence-fills — existing semantics.
- Malformed persisted gains → rejected per §4.5 validation, defaults apply.
- Config with both `phone` and `system` blocks → migration keeps `system`,
  drops `phone` (§4.6), covered by serialization tests.

## 7. Testing

**Unit (ctest):**
- Engine fan-out: distinct instances; gain/preset/bypass propagate to all
  instances of the StreamId; per-instance biquad state; release stops
  propagation; acquire-after-set inherits current gains + bypass.
- Persistence: disk round-trip through a real temp YAML file (write →
  destroy objects → fresh YamlConfig → reload); preset-name-wins ordering;
  bypass persists; debounce and saveNow both flush; malformed-gains matrix
  (NaN, inf, short, long, non-scalar).
- Migration: raw-YAML `phone`→`system` (only-phone, both, neither);
  serialized output asserted.
- Volume-at-creation: set master volume (incl. 0) → createStream → stream
  has the volume applied.
- Tap lifecycle state machine: bring-up ordering, failure at each step →
  capture-first teardown, transport activate/deactivate transitions, focus
  request/release pairing.
- Capture refactor: two concurrent slots; concurrent close/process stress on
  the callback handoff.
- Existing suites stay green: `test_equalizer_service`,
  `test_equalizer_engine`, `test_audio_service`, plus the app target build
  (ctest never compiles main.cpp).

**Bench (Pi + phone, one sitting):**
1. `pw-dump` the `bluez_input` node during playback: confirm
   `media.class = "Stream/Output/Audio"`, node.name pattern, no
   `node.dont-fallback` (plan-time gate, re-verified at bench).
2. `pw-link -l`: bluez → `openauto-bt-eq-in` while streaming; **assert no
   link from any microphone/source into `openauto-bt-eq-in`** (P1-1 guard).
3. Audible preset swap during BT playback (the F2 verify line, now
   satisfiable).
4. HU master volume changes BT level; boot muted → BT stays muted.
5. Focus rows: speech prompt ducks BT; starting BT silences AA media and
   vice versa.
6. Failure rows: stop `openauto-prodigy.service` mid-playback → music
   continues direct (fallback); kill only the playback path (failure
   injection hook or SIGSTOP variant at the bench's discretion) → capture
   teardown restores direct audio.
7. Idle economics: no A2DP transport → playback stream inactive, measure
   idle CPU vs today, sink allowed to suspend.
8. Formats: 44.1 kHz and 48 kHz A2DP sources; long-run (≥10 min) ring
   fill/drops/pitch stability with adaptive rate disabled.
9. AA regression: AA media still EQ'd; assistant/voice (AA mic capture slot)
   unaffected by the capture refactor.
10. Fresh-install row: installer places the WirePlumber conf; restart order
    `bluetooth` → `pipewire wireplumber` → app respected.

## 8. Executor guidance

**Invariants:**
- Never set `node.dont-fallback` on the bluez retarget — fallback IS the
  failure mode.
- The BT capture is non-autoconnect and never targets `inputDevice_` — mic
  audio must be unreachable from the tap by construction.
- Capture node exists only while the downstream (ring, playback, engine) is
  healthy — §3.1 ordering is not optional.
- Playback path: always fill and publish the full **requested** period
  (`wantBytes`, bounded by `d.maxsize`), silence-filling gaps — the existing
  callback honors `buf->requested` deliberately (resampler contract); do not
  change it to raw maxsize, and do not fork the callback for BT.
- `IEqualizerService` (pure interface) must not change beyond the additive
  `StreamId` alias — new methods go on the concrete `EqualizerService` only.
  No `HOST_API_VERSION` bump is required (no vtable/layout change).
- StreamId values: append/alias only, never renumber (Media=0, Nav=1,
  System=2, Phone=System).
- Engine attach/release obeys the §4.4 RT ordering contract everywhere,
  including migrated existing consumers.
- Wireless-only AA constraints and proto freezes are untouched by this work.

**Pitfalls:**
- The capture-slot refactor touches the AA mic path — bench row 9 covers it;
  don't skip the concurrent close/process stress test.
- WirePlumber 0.5 conf fragments are SPA-JSON under `wireplumber.conf.d/`
  (not Lua); rules APPEND to `monitor.bluez.rules`. Verify live with
  `wpctl status` + `pw-dump` after restart.
- A2DP transport D-Bus events toggle stream *activity* only — never create
  or destroy the capture node from them (that reopens the linking race).
- Boot-with-volume-0 is a real case (car powered off mid-mute) — test it.
- `git mv` doesn't stage edits made after the move (repo gotcha).
- Docker cross image is cached — this work adds no new build deps, so no
  image rebuild expected.

**Definition of done:** suite green + app target builds; cross-build clean;
bench rows 1–10 recorded in session-handoffs; docs updated (architecture.md
audio section, config schema reference, wishlist ledger closes the promoted
items + annotates the parked web editor); Codex gate run and adjudicated
before push; milestone tag + dev→main PR follow on Matthew's go (already
declared).
