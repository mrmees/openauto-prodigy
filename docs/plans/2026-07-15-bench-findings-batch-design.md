# Bench-Findings Batch: ExecStopPost Removal + Input-Device & Master-Volume Persistence — Design

Status: ACTIVE
**Date:** 2026-07-15 · **Grounded against:** `dev` at `69fc78b`.
**Codex spec review (gpt-5.6-sol, 2026-07-15):** round 1 verdict REWORK —
2 P1 / 4 P2 / 1 P3, ALL verified against the tree and accepted (zero
dismissals): AA-assistant mic row impossible (no AVInput→capture wiring
exists — acceptance narrowed, mic transport is separate protocol-critical
work); master-volume shutdown flush required (debounce alone loses saves);
settings-slider is a second racing volume-persist path (removed);
emit-under-mutex deadlock edge on the no-PipeWire path; picker live-sync
claim narrowed; settings-tree + roadmap added to scope; manual
`audio.input_device` YAML stays an ignored unknown key. Incorporated below.
**Origin:** BT A2DP EQ bench findings (wishlist § "From BT A2DP EQ bench
(2026-07-15)"). Promoted by Matthew 2026-07-15: items 1–3 as one batch;
pairing-window UI stays wishlisted (stretch, not promoted). ExecStopPost
decision: **remove entirely** (Matthew, 2026-07-15).

## Scope

Six fixes; one cycle (SDD → codex gate → bench spot-checks). Items 1–3 are the
original bench-findings promotion; items 4–6 are Codex post-merge findings on
PR #20, verified against the tree and folded in by Matthew 2026-07-15.

1. Remove the unit's `ExecStopPost` `bluetoothctl disconnect` hook.
2. Make input-device (mic) selection persist and survive restart.
3. Make master-volume changes persist to disk (debounced).
4. **Stop the A2DP retarget rule from hijacking HFP SCO call audio**
   (post-merge P1 — live regression on the deployed Pi; execute FIRST).
5. Validate the loaded plugin binary's API version/ID against its manifest
   before `initialize()` (post-merge P2 hardening).
6. Fix stale roadmap references left by the BT-EQ archival (post-merge P3).

**Out of scope** (wishlisted, not promoted): pairing-window UI fix +
`pairableChanged` cache bug; AVRCP-pause-on-focus-loss; AA System-channel duck
softening; epoch-quiesced ring transitions; web EQ editor (PARKED). The
wishlisted "tap sweep of pre-existing live `bluez_input.*` nodes" stays out of
scope **unless** item 4 lands on the app-side-retarget fallback, which absorbs
it naturally (see item 4).

---

## Item 1 — Remove ExecStopPost `bluetoothctl disconnect`

### Problem

`install.sh:1723` ships, inside the generated `openauto-prodigy.service`:

```
ExecStopPost=-/bin/sh -c '[ "$SERVICE_RESULT" = "success" ] && timeout 5 /usr/bin/bluetoothctl disconnect || true'
```

Every clean stop/restart — i.e. every deploy — deliberately kicks the phone
off BT. Root-caused at the 2026-07-15 bench as the primary mechanism behind
"funky BT during automated SSH ops" (session-handoffs 2026-07-15 bench-COMPLETE
entry).

### Why removal is safe (Chesterton's fence)

The hook's original rationale (commit `843f347`, 2026-03-02: "disconnect BT so
the phone doesn't stay connected to a dead audio sink") is obsolete. The BT
A2DP EQ feature (shipped `ALPHA-26-07-15-01`) added a WirePlumber fallback:
when the app stops, the `bluez_input.*` stream relinks direct-to-sink and
audio keeps playing — bench-proven through every app stop/error on
2026-07-14/15. The sink is no longer "dead" on app stop. On real shutdowns the
BT controller goes down with the OS and phones detect supervision-timeout link
loss exactly as when walking out of range; an explicit disconnect buys nothing.

### Change

- `install.sh`: delete the `bluetoothctl disconnect` `ExecStopPost` line
  (line 1723). The adjacent `wf-panel-restore` `ExecStopPost` line stays.
- Live Pi unit (deploy step, not an install.sh re-run):
  `sudo sed -i '/bluetoothctl disconnect/d' /etc/systemd/system/openauto-prodigy.service && sudo systemctl daemon-reload`
- Docs: wishlist entry flips on ship; session-handoffs entry. No other doc
  describes the disconnect-on-clean-stop behavior (`docs/how-to/testing-reconnect.md`
  mentions a *manual* `bluetoothctl disconnect` — unrelated, unchanged).
- Codex-verified (spec review round 1): the source installer's service heredoc
  is the ONLY generator of the hook — re-running `install.sh` after the edit
  cannot resurrect it, and `install-prebuilt.sh` (release installs) already
  generates a unit with no disconnect hook. Nuance: on real shutdowns the
  explicit disconnect shortened the link by the remaining OS-shutdown
  interval; no behavior depends on that.

### Acceptance / bench row

Clean `systemctl restart openauto-prodigy` while the phone is connected and
streaming → phone **stays connected** (btmon/`bluetoothctl info` shows no
disconnect), music continues via fallback, and relinks into the EQ tap on the
next transport cycle. Known limitation (separate wishlist item, unchanged): the
surviving stream stays direct/un-EQ'd until that transport cycle.

Side effect to note in docs: deploys stop kicking the phone — bench ergonomics
change for every future session.

---

## Item 2 — Input-device selection persistence

### Root cause (three stacked faults — differs from the wishlist's summary)

The wishlist said "`input_device` is never written to config.yaml"; true, but
the mechanism matters:

1. **Silent schema rejection.** The QML *does* write and save the key
   (`qml/applications/settings/AudioSettings.qml:82-83`:
   `ConfigService.setValue("audio.input_device", nodeName)` + `save()`), but
   `YamlConfig::setValueByPath` (`src/core/YamlConfig.cpp:1283`) validates the
   dotted path against the **defaults schema**, which contains
   `audio.output_device` (:58) and `audio.microphone.device` (:63) but **no
   `audio.input_device`** → the write returns false and nothing persists.
2. **Split key at startup.** `src/main.cpp:320` loads the mic from
   `yamlConfig->microphoneDevice()` = `audio.microphone.device` — a key the UI
   never writes. (Output device, by contrast, loads from the same key the UI
   writes — which is why only input is broken.)
3. **Combo repopulates from the dead key.** `AudioSettings.qml:87-93` reads
   `audio.input_device` back → always empty → "auto".

Also affected: the web-config path `src/core/services/IpcServer.cpp:391`
persists to the same dead `audio.input_device` key and silently fails
identically (the live `setInputDevice` call at :382 works).

### Fix — consolidate on the canonical schema key `audio.microphone.device`

- `AudioSettings.qml` input picker: on selection write
  `audio.microphone.device` (keep the existing `ConfigService.save()` and the
  live `AudioService.setInputDevice()` call); populate the combo from the
  **live** `AudioService.inputDevice()` value (Q_INVOKABLE,
  `AudioService.hpp` `inputDevice()`) at construction, and resync selection on
  `AudioInputDeviceModel` count changes (device enumeration resets the model,
  `AudioDeviceModel.cpp:60`). Scope note: `setInputDevice` has no change
  signal / Q_PROPERTY, so an already-open picker does NOT track IPC-set
  changes — accepted; no new signal added (YAGNI).
- `IpcServer.cpp:391`: persist to `audio.microphone.device`. The JSON field
  name `input_device` in the IPC API is **unchanged** (get at :370 already
  reads the live service value; `web-config/templates/audio.html` binds the
  JSON field only — unaffected).
- `docs/reference/settings-tree.md:72`: currently advertises the dead
  `audio.input_device` key and "restart required" — update to
  `audio.microphone.device`, applies to newly created capture streams.
- No YamlConfig schema changes, no new keys, startup path untouched.
- Migration: none required — the rejected UI/IPC writes could never create
  `audio.input_device` in app-generated configs. A manually authored
  `audio.input_device` is preserved by the unknown-key merge
  (`YamlMerge.hpp:18`) but ignored; optional cleanup NOT included.

**Alternative rejected:** adding `audio.input_device` to the defaults schema —
leaves two keys meaning the same thing and `microphone.device` as a dead
vestige read only at startup.

### Behavior notes

- `AudioService::setInputDevice` only affects capture streams created
  afterwards (`PW_KEY_TARGET_OBJECT` set at stream creation, in
  `openCaptureStreamWithOptions`).
- **The AA assistant mic is NOT unblocked by this fix** (Codex spec review
  P1, verified): the protocol handler emits `micCaptureRequested` but NO
  production code consumes it, and the orchestrator owns no capture handle —
  the only in-tree capture consumer is `BtAudioTap`. AA microphone transport
  is separate protocol-critical work (`Tier: main`, own promotion cycle);
  this batch only makes the device *preference* real. The wishlist premise
  "unblocks AA-assistant mic config" was necessary-but-not-sufficient.
- If the persisted device is absent at boot, the combo shows no selection and
  routing targets a missing node (PipeWire falls back). Same pre-existing
  behavior as output-device; not worsened, not fixed here.

### Acceptance / bench rows

- Unit-level: `set_audio_config` IPC with `input_device` → config.yaml gains
  `audio: microphone: device:`; `setValueByPath("audio.microphone.device", …)`
  returns true. (Exact tests at plan time.)
- Bench: select mic on HU → key lands in config.yaml → app restart → combo
  shows the selection and `AudioService.inputDevice()` matches. Acceptance
  ends at YAML round-trip + service restoration + picker restoration — no AA
  assistant row (impossible until AVInput capture wiring exists).

---

## Item 3 — Master-volume persistence

### Root cause (corrected by spec review round 1)

All runtime volume changes funnel through `AudioService::setMasterVolume`
(`AudioService.cpp:488`) → `masterVolumeChanged` — gesture overlay
(`GestureOverlayController.cpp:112`), mute toggle (`main.cpp:1006-1010`), IPC
(`IpcServer.cpp:384`), settings slider. Startup loads from config
(`main.cpp:321`, `YamlConfig::masterVolume` default 80). Persistence is
**inconsistent, not absent**: the settings-screen slider independently
persists via `configPath: "audio.master_volume"` (`AudioSettings.qml:24`;
`SettingsSlider` writes + saves after 300 ms and on destruction) while the
gesture/navbar/mute/IPC paths never flush (bench: gesture 0→59 while disk
stayed 89; restart reloaded 89). Any centralized fix must also REMOVE the
slider's independent write path or there are two racing writers.

### Fix — single centralized persist path, debounced, with shutdown flush

- Connect `AudioService::masterVolumeChanged` → dirty flag + single-shot
  **2000 ms** debounce `QTimer` (mirrors `EqualizerService`'s
  `kSaveDebounceMs`, `EqualizerService.cpp:11`) →
  `yamlConfig->setMasterVolume(audioService->masterVolume())` + save.
  Observe `YamlConfig::save()`'s boolean and retry transient failure like the
  EQ pattern (`EqualizerService.cpp:473`); note `ConfigService::save()`
  swallows the result — persist via a path that surfaces it. The save is
  durable (tmp-write + fsync + rename + parent-dir fsync, `YamlConfig::save`).
- **Shutdown flush (spec review P1):** a pending debounce gets no event-loop
  turn once `app.exec()` returns — connect `aboutToQuit` → stop timer +
  synchronous save-if-dirty, mirroring the EQ flush (`main.cpp:333` →
  `EqualizerService::saveNow`). Hard crash/power loss inside the 2 s window
  is the accepted debounce tradeoff (documented, not defended).
- Timer is main-thread, context-bound (receiver-context connection), lifetime
  ends before `ConfigService`/`YamlConfig`. `setMasterVolume` is documented
  thread-safe API — the queued/context-bound connection keeps a future
  worker-thread caller from touching the timer cross-thread.
- **Emit-under-lock rider (spec review P2):** the no-PipeWire branch of
  `setMasterVolume` emits `masterVolumeChanged` while holding `mutex_`
  (`AudioService.cpp:489-494`); QML handlers read `masterVolume()` (same
  non-recursive mutex) from the signal → deadlock on the dev path. Hoist the
  emit out of the locked scope, and emit in both branches only when the
  clamped value actually changed.
- Place the connection **after** the initial load at `main.cpp:321` (natural
  ordering) so the boot-time apply doesn't trigger a redundant save.
- **Single writer:** remove the settings slider's
  `configPath: "audio.master_volume"` (initialize + sync it from live
  `AudioService.masterVolume`; keep its `onMoved` → `setMasterVolume`).
  Remove the direct write at `IpcServer.cpp:393`, and make the IPC handler's
  immediate `save()` fire only when a device field was actually persisted —
  otherwise a master-only IPC request saves the stale in-memory volume before
  the debounce lands.

### Mute semantics (decided)

The mute toggle sets volume 0, so muting then power-cycling boots at 0 —
silent until the user raises volume. Accepted as correct restore-last-state
behavior (presented 2026-07-15; the boot-muted bench row already proved
boot-at-0 is safe: genuine streaming at vol 0, smooth fade-in on raise).

### Acceptance / bench row

- `main.cpp` wiring is NOT covered by ctest (ctest never compiles `main.cpp` —
  standing trap) → verification is app-target build + bench rows. The
  AudioService emit-under-lock rider IS unit-testable.
- Bench: change volume via gesture → wait >2 s → config.yaml reflects it →
  restart app → volume restored (no stale reload). Rapid volume drag produces
  one write, not a write per step (journal/inotify spot-check).
- Bench (spec review P1): change volume, clean-restart **within 2 s** →
  new value survives (the aboutToQuit flush row).

---

## Item 4 — A2DP retarget rule hijacks HFP SCO call audio (post-merge P1)

### Root cause (verified 2026-07-15)

`config/50-openauto-bt-eq.conf:14` matches `node.name = "~bluez_input.*"`
alone and retargets matches to the tap (`target.object = "openauto-bt-eq-in"`).
But HFP SCO far-end voice is ALSO a `bluez_input.<MAC>` node
(`api.bluez5.profile = headset-audio-gateway`) — documented in the shipped HFP
design (`docs/archive/plans/2026-07-05-hfp-call-audio-design.md:32`), which
relies on WirePlumber's default auto-link to the sink ("the routing problem is
already solved by the platform"). The conf's comment claiming the only other
bluez node is `bluez_midi.server` forgot SCO.

Consequence chain: during a call the SCO downlink node is created → rule
retargets it into the tap → `BtAudioTap` **drops** all captured samples while
its A2DP gate is closed (`BtAudioTap.cpp:88`, gate follows A2DP
`MediaTransport1.State == "active"`, which is idle during a call) → **far-end
call audio silently discarded**. If A2DP activity lingers, call audio instead
plays through the Media EQ/focus path as "BT Audio" — also wrong.

**The deployed Pi (`ALPHA-26-07-15-01` + this conf) likely has silent phone
calls whenever the app is running.** The 7/7 bench had no HFP-call row with
the rule installed. Interim mitigation if calls are needed before the fix:
remove `50-openauto-bt-eq.conf` on the Pi + restart wireplumber (BT music
reverts to un-EQ'd).

### Fix — two-stage, empirically gated (bench lesson: verify what monitor
rules can actually see)

- **Stage A (preferred, one-line conf fix):** discriminate A2DP positively in
  the monitor rule — candidate `api.bluez5.profile = "a2dp-source"` (or
  negative-match `headset-audio-gateway`). MUST be verified live first:
  `media.class` was bench-proven absent at monitor-rule evaluation
  (2026-07-15, commit `9077e17`); `api.bluez5.profile` may or may not be
  available at that point. Verification: SSH + btmon/pw-dump while Matthew
  places a call; confirm (a) the property is visible to the rule, (b) SCO
  routes direct-to-sink, (c) A2DP still retargets to the tap.
- **Stage B (fallback if no property discriminates at rule-eval time):** drop
  the WirePlumber rule and move selection into the app — on A2DP transport
  activation, positively identify the A2DP `bluez_input.*` node in the
  registry (props ARE readable there — `ScoNodeMonitor.cpp:72` proves it) and
  retarget it via metadata. This path also absorbs the wishlisted
  "sweep pre-existing live nodes" item (app restart during live streaming
  would relink into the tap immediately).

### Acceptance / bench rows

- Incoming AND outgoing HFP call with the tap active: far-end voice audible,
  routed direct (NOT through tap/EQ), mic uplink works, music duck/resume
  around the call intact.
- BT music still tap-routed/EQ'd after the change (audible preset swap).
- App-not-running fallback unchanged (no `node.dont-fallback`).

---

## Item 5 — Plugin ABI gate trusts manifest, not the loaded binary (post-merge P2)

### Root cause (verified 2026-07-15)

`PluginDiscovery.cpp:50` enforces `manifest.apiVersion == HOST_API_VERSION`
from `plugin.yaml` only. The dynamic-load path (`PluginManager.cpp:57-75`)
then loads the `.so` and calls `initialize(context)` (:88) without ever
comparing `plugin->apiVersion()` (`IPlugin.hpp:24`) or `plugin->id()` against
the manifest. A stale v1 binary beside a v2 manifest initializes against the
shifted `IHostContext` vtable — the exact crash the 2026-07-14
HOST_API_VERSION=2 bump (exact-match acceptance) was added to prevent.

Field exposure today is nil — all five in-tree plugins register statically
(`main.cpp:414-446`); the dynamic path loads nothing in a stock install. This
is cheap hardening of the supported third-party surface, not a live bug.

### Fix

In `PluginManager`'s dynamic-load path, after `PluginLoader::load` and before
registration/`initialize()`: reject (unload + `pluginFailed`) when
`plugin->apiVersion() != HOST_API_VERSION` or `plugin->id() != manifest.id`.
Test: manifest/binary-mismatch case at the PluginManager level (mock or
fixture plugin lying about its version/ID).

---

## Item 6 — Stale roadmap references after BT-EQ archival (post-merge P3)

`docs/roadmap-current.md` still lists the shipped BT A2DP EQ work under "Now"
(:58) and cites `docs/plans/...` paths that moved to `docs/archive/plans/`
(at least the bt-a2dp-eq design at :59 and the hfp-mic-9876 pair at :70; sweep
the whole file for `docs/plans/` refs whose targets moved). Docs-only fix:
move shipped work to "Done", repoint refs at `docs/archive/plans/`, and add
THIS batch under "Now" (AGENTS.md requires roadmap updates when priorities
change — spec review P2).

---

## Shared constraints & execution notes

- Build in `~/builds/openauto-prodigy` (ext4); never in-repo on /mnt/e. Build
  the `openauto-prodigy` app target explicitly before claiming green or gating.
- Codex gate pre-push (`bash scripts/codex-review.sh`); adjudicate every
  finding; no pushes without go; no tags unless Matthew declares.
- Deploy restart order `bluetooth` → `pipewire wireplumber` → `openauto-prodigy`.
  Until item 1 lands on the Pi, expect deploys to kick the phone.
- QML changes ship inside the binary — item 2 needs cross-build + binary rsync,
  not a Pi-side `git pull`.
- Execution order: item 4 first (live call-audio regression), then 1–3, 5, 6
  in any order.
- Bench rows needing Matthew present: item 4 call rows (any phone). The
  item 2 AA-assistant mic row was REMOVED (impossible — see item 2); item 2
  verifies over SSH + one restart.
- Opportunistic bench cleanup while in there: remove the stale one-way S25
  bond from the HU.
- Wishlist-then-promote: anything new found mid-execution goes to
  `docs/wishlist.md`, not into scope.
- Line numbers in this doc are accurate at the grounded revision but will
  drift — the implementation plan pairs them with symbol names.
