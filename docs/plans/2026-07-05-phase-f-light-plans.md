# Phase F — Light Plans (commodity items)

Status: ACTIVE (0x8012 wire experiment remains; key-event nav is an unpromoted sketch)

**Date:** 2026-07-05 · **Grounded against:** `fable-design-sprint` at `db2e7eb`.
**Nature: LIGHT plans** (sprint program §5.F — commodity work, "light plans suffice"). Unlike the deep plans in this directory, these are scoped outlines: an executor with judgment can work from them directly; a low-power executor should first expand one into full `writing-plans` format (read `README-executor-handbook.md` §2 either way). Roadmap outcomes quoted are canonical (`docs/roadmap-current.md`).

---

## F1. Local media player plugin — COMPLETED 2026-07-10

The two-stage implementation and Pi bench matrix are complete. The shipped
plugin includes AudioService-routed playback, folder and scanned-library views,
USB automount/eject, shared media status, and the now-playing surface. Design
and stage plans are archived under `docs/archive/plans/`; follow-up ideas remain
in `docs/wishlist.md` until promoted.

---

## F2. Equalizer parity audit — COMPLETED 2026-07-14

The audit confirmed the on-head-unit and YAML surfaces. The subsequently
promoted persistence, System-stream labeling, and BT A2DP routing work shipped
2026-07-15. A web-config EQ editor remains an unpromoted wishlist item.

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
