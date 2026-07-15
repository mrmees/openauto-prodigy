# Bench-Findings Batch: ExecStopPost Removal + Input-Device & Master-Volume Persistence — Design

Status: ACTIVE
**Date:** 2026-07-15 · **Grounded against:** `dev` at `69fc78b`.
**Origin:** BT A2DP EQ bench findings (wishlist § "From BT A2DP EQ bench
(2026-07-15)"). Promoted by Matthew 2026-07-15: items 1–3 as one batch;
pairing-window UI stays wishlisted (stretch, not promoted). ExecStopPost
decision: **remove entirely** (Matthew, 2026-07-15).

## Scope

Three small, independent fixes; one cycle (SDD → codex gate → bench spot-checks).

1. Remove the unit's `ExecStopPost` `bluetoothctl disconnect` hook.
2. Make input-device (mic) selection persist and survive restart.
3. Make master-volume changes persist to disk (debounced).

**Out of scope** (wishlisted, not promoted): pairing-window UI fix +
`pairableChanged` cache bug; AVRCP-pause-on-focus-loss; tap sweep of
pre-existing live `bluez_input.*` nodes; AA System-channel duck softening;
epoch-quiesced ring transitions; web EQ editor (PARKED).

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
  live `AudioService.setInputDevice()` call); repopulate the combo from the
  **live** `AudioService.inputDevice()` value (Q_INVOKABLE,
  `AudioService.hpp:156`), falling back to the config key — live truth also
  reflects IPC-set changes mid-session.
- `IpcServer.cpp:391`: persist to `audio.microphone.device`. The JSON field
  name `input_device` in the IPC API is **unchanged** (get at :370 already
  reads the live service value).
- No YamlConfig schema changes, no new keys, startup path untouched.

**Alternative rejected:** adding `audio.input_device` to the defaults schema —
leaves two keys meaning the same thing and `microphone.device` as a dead
vestige read only at startup.

### Behavior notes

- `AudioService::setInputDevice` only affects capture streams created
  afterwards (`PW_KEY_TARGET_OBJECT` set at stream creation,
  `AudioService.cpp:687-688`). AA assistant capture is created per invocation,
  so a new selection applies from the next assistant use — no restart needed.
- If the persisted device is absent at boot, the combo shows no selection and
  routing targets a missing node (PipeWire falls back). Same pre-existing
  behavior as output-device; not worsened, not fixed here.

### Acceptance / bench rows

- Unit-level: `set_audio_config` IPC with `input_device` → config.yaml gains
  `audio: microphone: device:`; `setValueByPath("audio.microphone.device", …)`
  returns true. (Exact tests at plan time.)
- Bench (needs Matthew + Pixel): select mic on HU → key lands in config.yaml →
  app restart → combo shows the selection and `AudioService.inputDevice()`
  matches → **AA assistant end-to-end mic round-trip** — this path has NEVER
  been testable before this fix.

---

## Item 3 — Master-volume persistence

### Root cause

All runtime volume changes funnel through `AudioService::setMasterVolume`
(`AudioService.cpp:488`) → `masterVolumeChanged` — gesture overlay
(`GestureOverlayController.cpp:112`), mute toggle (`main.cpp:1006-1010`), IPC
(`IpcServer.cpp:384`). Startup loads from config (`main.cpp:321`,
`YamlConfig::masterVolume` default 80), but nothing ever flushes runtime
changes back to disk (bench: runtime 0→59 while disk stayed 89; restart
reloaded 89).

### Fix — debounced persist-on-change in the main.cpp wiring

- Connect `AudioService::masterVolumeChanged` → single-shot **2000 ms**
  debounce `QTimer` (mirrors `EqualizerService`'s `kSaveDebounceMs` pattern,
  `EqualizerService.cpp:11`) → `yamlConfig->setMasterVolume(audioService->masterVolume())`
  + `configService->save()`. `YamlConfig::save` already has the durable
  tmp-write + fsync + rename + parent-dir-fsync treatment from the shipped EQ
  persistence rider (`YamlConfig.cpp:257`).
- Place the connection **after** the initial load at `main.cpp:321` (natural
  ordering) so the boot-time apply doesn't trigger a redundant save.
- Remove the now-redundant direct write at `IpcServer.cpp:393`
  (`config_->setMasterVolume(...)`) so there is a **single** persist path; the
  IPC handler's immediate `save()` for device keys stays.

### Mute semantics (decided)

The mute toggle sets volume 0, so muting then power-cycling boots at 0 —
silent until the user raises volume. Accepted as correct restore-last-state
behavior (presented 2026-07-15; the boot-muted bench row already proved
boot-at-0 is safe: genuine streaming at vol 0, smooth fade-in on raise).

### Acceptance / bench row

- `main.cpp` wiring is NOT covered by ctest (ctest never compiles `main.cpp` —
  standing trap) → verification is app-target build + bench row.
- Bench: change volume via gesture → wait >2 s → config.yaml reflects it →
  restart app → volume restored (no stale reload). Rapid volume drag produces
  one write, not a write per step (journal/inotify spot-check).

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
- Opportunistic bench cleanup while in there: remove the stale one-way S25
  bond from the HU.
- Wishlist-then-promote: anything new found mid-execution goes to
  `docs/wishlist.md`, not into scope.
