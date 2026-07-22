# Phase F — Light Plans (commodity items)

Status: COMPLETED 2026-07-21

**Date:** 2026-07-05 · **Grounded against:** `fable-design-sprint` at `db2e7eb`.
**Nature: LIGHT plans** (sprint program §5.F — commodity work, "light plans suffice"). Unlike the deep plans in this directory, these are scoped outlines: an executor with judgment can work from them directly; a low-power executor should first expand one into full `writing-plans` format (read `README-executor-handbook.md` §2 either way). Roadmap outcomes quoted are canonical (`docs/roadmap-current.md`).

> **Closure note (2026-07-21):** F1 shipped 2026-07-10. The F2 audit
> completed 2026-07-14 and its promoted follow-up shipped 2026-07-15. F3's
> proposed `0x8012` experiment was closed without implementation after the
> gold-traced protocol contract disproved its premise: `0x8012` is the HU
> response to the phone's `0x8011` theming-token request, not an HU margin
> push. F4 was never promoted and remains wishlist-only. This archive retains
> the original task outlines below as historical planning context.

---

## F1. Local media player plugin

**Outcome (roadmap):** media player plugin (Qt Multimedia) integrated with `MediaStatusService` and the now-playing UI.

**Grounding:** plugin pattern = `src/plugins/bt_audio/` (static plugin, provider integration); `IMediaStatusProvider` is the arbitration seam (API design §8.1 — source string, playbackState int, title/artist/album). `MediaStatusService` merges sources — read how BtAudioPlugin feeds it before adding a third source.

**Key design question to settle first (30 min, blocks the rest):** audio path. `QMediaPlayer`'s ffmpeg backend outputs straight to PipeWire with its own stream — it will NOT be an `AudioService` stream, so master volume/ducking/EQ won't apply (same class of limitation as HFP SCO, design 2026-07-05-hfp-call-audio §3). Options: (a) accept it v1 (consistent with the SCO precedent — recommended), (b) decode via QAudioDecoder and push PCM through `AudioService::createStream` (full integration, real work). Record the choice in the plan expansion.

**Task outline:**
1. `MediaPlayerPlugin` skeleton (`src/plugins/media_player/`, id `org.openauto.media-player`) — register in `main.cpp` beside the other static plugins; QML view `qml/applications/media_player/`.
2. `QMediaPlayer` + file browser (USB mounts + `~/Music`; `QMediaMetaData` for tags).
3. Feed `MediaStatusService` as source `"MediaPlayer"` (title/artist/album/playbackState) — the now-playing widget and the API `media` stream then work for free (verify the source-string mapping table in `ApiSerializers` gains the new source if API v1 has landed).
4. Playback controls via ActionRegistry (`media.play/pause/next/previous` are RESERVED prefixes in the API — check the arch doc §9.1 reserved list before naming).
5. Tests: metadata→provider mapping; playback-state transitions with a fixture file.

**Verify:** full ctest + cross-build; on Pi: play a local file, now-playing widget updates, BT audio still arbitrates correctly when both sources active.

---

## F2. Equalizer completion — parity audit only — COMPLETED 2026-07-14

> **Audit executed 2026-07-14** (session-handoffs entry same date): on-HU + YAML
> legs hold, web advanced-EQ leg absent; gaps + quirks filed to `docs/wishlist.md`
> § "From EQ parity audit (2026-07-14)". No code changed. Note: the verify line
> below ("audible preset change during BT playback") is unsatisfiable as written —
> BT A2DP routes BlueZ→PipeWire natively and never passes through an EQ engine
> (one of the filed findings).

**Outcome (roadmap):** EQ plugin with YAML settings, on-HU basic changes/profile swapping, web backend for advanced setup.

**Grounding (scout 2026-07-05, program §8.2):** the EQ is **functional, not incomplete** — `EqualizerService` runs 3 engines with presets + persistence; `AudioService.cpp:154-158` applies EQ on the RT thread; `EqualizerPlugin` exists (`main.cpp:373`). Known quirk: the Phone EQ engine is attached to the "AA System" stream (`AndroidAutoOrchestrator.cpp:343`) — mislabeled, pre-existing, fix only if trivial.

**Task outline (audit, ~half a day):**
1. Diff the roadmap outcome against reality: on-HU preset swap UI (exists in EqualizerPlugin? exercise it), custom profile creation via web-config (exists in `web-config/`? grep `equalizer` routes), YAML persistence round-trip.
2. Produce a gap list in `docs/session-handoffs.md`; only build what's actually missing (expected: web-config advanced editor is the likely gap).
3. If nothing is missing: mark the roadmap item done and move on — do not invent scope.

**Verify:** ctest (EQ tests exist); on Pi: audible preset change during BT playback; web-config profile edit persists across restart.

---

## F3. `UpdateHuUiConfigRequest` (0x8012) wire-verification experiment

**Outcome (roadmap discovery item):** determine whether 0x8012 (from `UiConfigMessages.proto`, open-android-auto 2026-02-28) actually lets the HU push margins/content-insets/day-night at runtime — replacing the sensor-based night-mode workaround and unlocking runtime sidebar resize. **This is an experiment protocol, not a feature plan — needs Pi + phone.**

**Protocol:**
1. Baseline capture: enable `connection.protocol_capture` (exists in YamlConfig: `enabled`, `format: jsonl`, `path`) and record a normal session start — confirm margins are in the initial `VideoConfig` (src/core/aa/AGENTS.md gotcha: margins locked at session start).
2. Implement a throwaway send path: a debug action (`aa.debug.sendUiConfig`, behind a config flag, NOT for merge) that serializes `UpdateHuUiConfigRequest` with changed `margin_width/height` and/or day-night flag, sent on the video AV channel, message id 0x8012. Reference the proto in `libs/prodigy-oaa-protocol` READ-ONLY.
3. Matrix per phone (Pixel 8, S25 Ultra, Moto G Play 2024): (a) margin change mid-session — does the phone re-render into the new sub-region (observe letterboxing + touch alignment)? (b) day/night push — does AA theme flip without the sensor channel? (c) does the phone ACK/NAK or silently ignore (capture the reply message id)?
4. Record per-phone results in `docs/aa-protocol/` (new note file) + handoff; the go/no-go verdict decides whether a real feature plan (runtime sidebar + native night-mode) gets written.
5. Delete the throwaway send path or keep it behind the debug flag — either way, note it.

**Verify:** capture files show the 0x8012 frames on the wire; touch coordinates still map correctly after any margin change (the src/core/aa/AGENTS.md touch gotchas apply in full).

---

## F4. Key-event navigation map (steering-wheel / hardware buttons)

**Outcome:** hardware inputs (GPIO buttons, HID media keys, resistive-ladder steering controls via ADC) drive both the native UI and AA.

**Notes (design sketch — no roadmap commitment yet; promote via wishlist governance if Matthew wants it built):**
- The seam already exists: `aa.sendButton` action takes an int keycode payload (API design §9.1 cites it); native navigation would be new actions (`navbar.*` pattern).
- Shape: an `InputMapPlugin` or core `KeyEventRouter` reading evdev devices that are NOT the touchscreen (the `InputDeviceScanner` INPUT_PROP_DIRECT filter already separates them), mapping scancode→action id via a YAML table (`input_map:` config namespace).
- Routing rule mirrors touch: when AA is projecting and focused → forward mapped AA keycodes via `aa.sendButton`; otherwise dispatch native actions. `IProjectionStatusProvider` is the arbitration input (same pattern as `CallAudioPolicy`).
- GPIO buttons on the Pi: prefer `gpio-keys` device-tree overlay (kernel turns GPIOs into a standard evdev keyboard — no custom GPIO code, same evdev path as HID).
- Open questions for the eventual plan: long-press/chord handling, dashboard-switch bindings (`app.dashboard.next` is action-ready), config UI vs YAML-only.

**Verify (when built):** evdev fixture test for the mapper; on Pi, a USB keyboard's media keys as the stand-in for steering-wheel hardware.
