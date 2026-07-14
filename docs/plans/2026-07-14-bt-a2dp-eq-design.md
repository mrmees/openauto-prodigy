# BT A2DP Through the Equalizer (+ EQ Hygiene Riders) — Design

Status: ACTIVE
**Date:** 2026-07-14 · **Grounded against:** `dev` at `7b5d9dd`.
**Origin:** EQ parity audit 2026-07-14 (session-handoffs entry; wishlist § "From
EQ parity audit (2026-07-14)"). Promoted by Matthew same day: BT A2DP → EQ,
persistence fix, Phone→System relabel. Web EQ editor stays parked. Milestone
tag + dev→main PR follow this work.

## 1. Problem

BT A2DP music routes BlueZ → PipeWire natively (`bluez_input.*` node
auto-linked to the hardware sink); it never passes through the app. Three
consequences, all confirmed live on the Pi (2026-07-14):

1. **No EQ** — the Media curve governs AA media + local media only.
2. **No master volume** — volume is applied per app stream
   (`SPA_PROP_channelVolumes` in `AudioService::setMasterVolume`), so the HU
   volume control does not affect BT playback at all (phone-side only).
3. **No ducking/focus** — nav/speech prompts do not duck BT music
   (`applyDucking` only reaches AudioService streams).

Original OpenAuto Pro applied a sink-level 15-band LADSPA EQ (and Pulse sink
volume), which governed all three for every source.

Adjacent latent defect found during the audit: `EqualizerEngine` is stateful
per instance (per-channel biquad chains, interpolation state, soft limiters),
but the single Media engine instance is **shared** by two concurrent
consumers — AA media (`AndroidAutoOrchestrator.cpp:339`) and the local media
player (`PlaybackEngine.cpp:131` via `MediaPlayerPlugin.cpp:49`). Simultaneous
playback corrupts filter state (and races). This design makes BT a third
Media-curve consumer, so the fix is load-bearing, not optional.

## 2. Decisions (Matthew, 2026-07-14)

- **Approach A** — app-side loopback tap (below). Filter-chain sink rejected
  (second EQ implementation to keep matched, no volume/ducking fix); app-owned
  global sink rejected (AA latency risk, double-EQ, blast radius).
- **BT follows the Media curve.** No fourth EQ tab, no new curve config. The
  curve compensates for cabin/speakers, not the source.
- Riders confirmed: engine-instance fan-out fix; persist unsaved gains +
  bypass; `Phone` → `System` relabel with config-key migration.

## 3. Architecture

```
phone A2DP ──> bluez_input.* node
                  │  (WirePlumber rule: target.object = "openauto-bt-eq-in")
                  ▼
   app capture stream "openauto-bt-eq-in"  (S16, 48 kHz, stereo — PipeWire resamples)
                  │  RT memcpy (capture process callback)
                  ▼
            AudioRingBuffer
                  │  existing playback process path:
                  ▼  ring read → EQ hook → focus gain → output
   app playback stream "BT Audio"  (role Music, priority = AA media's 50)
                  │
                  ▼
            hardware sink
```

- **WirePlumber rule** shipped by both installers to
  `/etc/wireplumber/wireplumber.conf.d/50-openauto-bt-eq.conf`: match
  `node.name ~ "bluez_input.*"` → `update-props { target.object =
  "openauto-bt-eq-in" }`. (Pi runs PipeWire 1.4 / WirePlumber 0.5 — SPA-JSON
  conf.d fragment, not Lua.)
- **Fallback is the safety property:** if the target node does not exist (app
  down, feature disabled) PipeWire falls back to the default sink — BT audio
  plays direct, un-EQ'd, exactly today's behavior. Never set
  `node.dont-fallback`. Rule file missing ⇒ also today's behavior.
- **Tap streams are created once at startup and stay open.** Idle PipeWire
  streams suspend for free. On-demand creation (e.g. on A2DP transport
  D-Bus events) is rejected: it races WirePlumber's link decision for the
  freshly appeared bluez node, and a lost race sticks the node to the
  hardware sink for the session.
- Both process callbacks run on AudioService's single `pw_thread_loop`, so
  the existing SPSC `AudioRingBuffer` semantics hold (same thread; no new
  synchronization). New RT code is one bounded memcpy; overrun drops,
  underrun silence-fills — existing behavior.

## 4. Components

### 4.1 AudioService: second capture slot

`openCaptureStream` currently enforces a single capture (`capture_` member,
used by the AA mic path). Refactor to per-handle capture state (small list or
two named slots) so the BT intake capture coexists with the AA mic capture.
The AA mic path's behavior must not change (same callback, same
single-instance semantics for its slot).

### 4.2 BT tap glue (new, small)

A `BtAudioTap` in `src/plugins/bt_audio/`, owned by `BtAudioPlugin` (the
BT-audio domain home; it reaches AudioService/EqualizerService via
IHostContext like MediaPlayerPlugin does), that:

- opens the capture stream `openauto-bt-eq-in` (S16/48k/2) and the playback
  stream `"BT Audio"` (role Music, priority 50, `targetDevice = "auto"`),
- wires capture callback → ring buffer → playback handle,
- acquires a Media-curve engine (`acquireEngine(StreamId::Media, 48000, 2)`)
  and attaches it to the playback handle (`handle->eqEngine`), same pattern as
  `PlaybackEngine.cpp:131`,
- releases both on shutdown.

Factor the non-PipeWire logic (ring plumbing decisions, lifecycle) so it is
unit-testable; the PipeWire graph behavior itself is a bench item.

### 4.3 EqualizerService: engine fan-out (rider)

- New concrete-class API: `EqualizerEngine* acquireEngine(StreamId, float
  sampleRate, int channels)` and `void releaseEngine(EqualizerEngine*)`.
  Each call returns a **dedicated instance** initialized with the stream's
  current gains + bypass. `setGain` / `applyPreset` / `setBypassed` fan out to
  every live instance of that StreamId.
- Existing consumers migrate: AA media / speech / system
  (AndroidAutoOrchestrator) and the local player (MediaPlayerPlugin) switch
  from `engineForStream` to acquire/release. `engineForStream` is then
  removed (all callers are in-tree; it is NOT part of `IEqualizerService`,
  so no interface/vtable change and no `HOST_API_VERSION` bump).
- The per-stream `StreamState.engine` member becomes the fan-out list; the
  service's own gains/preset bookkeeping is unchanged.

### 4.4 Persistence rider

- `writeToConfig` additionally writes, per stream: `gains: [10 floats]` and
  `bypassed: bool` (always written; source of truth for the unsaved case).
- `loadFromConfig` order per stream: named preset (bundled or user) wins if
  set; else apply raw `gains`; `bypassed` always restored. Missing keys ⇒
  current defaults (Flat/Voice/Voice, bypass off).
- YamlConfig gains the matching accessors beside `eqStreamPreset` /
  `eqUserPresets`.

### 4.5 Relabel rider: Phone → System

- `StreamId::Phone` → `StreamId::System` (value stays 2 — append-only
  discipline; source-level rename, in-tree consumers recompile).
- Config stream key `"phone"` → `"system"`: loader reads `system` first,
  falls back to `phone`; writer emits `system` only (stale `phone` block
  dropped on first save).
- UI: `EqSettings.qml` segment `["Media", "Nav", "Phone"]` →
  `["Media", "Nav", "System"]`. `Q_PROPERTY phonePreset` → `systemPreset`
  (+ signal) — QML-internal, no external surface reads it.
- The engine attachment at `AndroidAutoOrchestrator.cpp:343` (AA System
  stream ← formerly-"Phone" engine) becomes honest instead of mislabeled.
  Real call audio (HFP SCO) remains outside AudioService by design
  (2026-07-05 HFP decision) — out of scope here.

## 5. Behavior changes (user-visible, document in handoff + docs)

1. BT music obeys the Media EQ curve (preset swaps audible mid-playback).
2. HU master volume now controls BT playback level.
3. Nav/speech prompts duck BT music (same policy as AA media).
4. EQ slider positions and bypass survive restart even without saving a
   preset ("Custom" state is durable).
5. The third EQ tab reads "System" (AA system sounds), not "Phone".

## 6. Error handling

- App down / rule missing / feature regression → PipeWire target fallback:
  BT direct to sink (audible, un-EQ'd). No silent-failure mode.
- Ring overrun (sink stalled) drops oldest; underrun silence-fills — existing
  `AudioRingBuffer` + playback-callback semantics.
- Two music sources at once (second phone streams while first projects):
  streams mix; both duck under speech. Pre-existing condition, acceptable.
- Config migration: absent `system` + absent `phone` ⇒ defaults; malformed
  gains array entries clamp to ±12 dB (engine already clamps).

## 7. Testing

**Unit (ctest):**
- Engine fan-out: `acquireEngine` returns distinct instances; `setGain` /
  `applyPreset` / `setBypassed` propagate to all instances of the StreamId;
  biquad state is per-instance (process on one, other unaffected);
  release stops propagation.
- Persistence: gains + bypass round-trip; preset-name-wins ordering;
  `phone` → `system` key migration (old key read once, new key written).
- Tap lifecycle glue (non-PipeWire parts) as factored.
- Existing suites stay green: `test_equalizer_service`, `test_equalizer_engine`,
  `test_audio_service`, plus app target build (ctest never compiles main.cpp).

**Bench (Pi + phone, one sitting):**
1. `pw-link -l` shows `bluez_input.* → openauto-bt-eq-in` while streaming.
2. Audible preset swap during BT playback (the F2 verify line, now satisfiable).
3. HU master volume changes BT playback level.
4. Stop `openauto-prodigy.service` mid-playback → music continues direct
   (fallback row); restart → tap resumes on next track/transport.
5. AA reconnect regression: AA media still EQ'd; speech ducking intact.
6. Fresh-install row: installer places the WirePlumber conf; `systemctl
   restart wireplumber` ordering note (`bluetooth` → `pipewire wireplumber` →
   app) respected.

## 8. Executor guidance

**Invariants:**
- Never set `node.dont-fallback` on the bluez retarget — fallback IS the
  failure mode.
- Playback path: always output full periods (`d.chunk->size = maxSize`,
  silence-fill) — src/AGENTS.md PipeWire gotcha; the BT playback stream uses
  the existing callback, do not fork it.
- `IEqualizerService` (pure interface) must not change — new methods go on
  the concrete `EqualizerService` only. Any IHostContext-adjacent vtable
  change would require a `HOST_API_VERSION` bump (B2 lesson); this design
  requires none.
- StreamId values: append/rename only, never renumber (Media=0, Nav=1,
  System=2).
- Wireless-only AA constraints and proto freezes are untouched by this work.

**Pitfalls:**
- The capture-slot refactor touches the AA mic path — regression-test AA
  voice/assistant capture on the bench (row 5 covers reconnect; mic is
  exercised by assistant use).
- WirePlumber 0.5 conf fragments are SPA-JSON under
  `wireplumber.conf.d/` — not Lua (that's 0.4). Verify on-Pi with
  `wpctl status` after restart.
- A2DP nodes appear/disappear per transport; the tap streams outlive them by
  design. Do not add D-Bus-event-driven stream lifecycle.
- `git mv` doesn't stage edits made after the move (repo gotcha) — relevant
  if any file renames happen during execution.
- Docker cross image is cached — this work adds no new build deps, so no
  image rebuild expected.

**Definition of done:** suite green + app target builds; cross-build clean;
bench rows 1–6 recorded in session-handoffs; docs updated (architecture.md
audio section, config schema reference, wishlist ledger closes the two
promoted items + annotates the parked web editor); Codex gate run and
adjudicated before push; milestone tag + dev→main PR follow on Matthew's
go (already declared).
