# Phase F — Light Plans (commodity items)

Status: COMPLETED 2026-07-21

**Date:** 2026-07-05 · **Grounded against:** `fable-design-sprint` at `db2e7eb`.
**Closure:** F1 and F2 shipped; F3's wire premise was disproven by the later
gold protocol trace; F4 was never promoted and remains wishlist-only.

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

## F3. `0x8012` wire-verification premise — CLOSED 2026-07-21

The proposed experiment assumed that wire ID `0x8012` was an HU-initiated
runtime margins/content-insets/day-night request. That interpretation came from
the older silver-confidence `oaa/av/UiConfigMessages.proto` definition.

The current gold-traced video definitions, production handler, and handler
tests establish a different contract:

- The phone sends Material You theming tokens in `UiConfigRequest` on `0x8011`.
- The HU returns `UpdateHuUiConfigResponse` on `0x8012`, with an
  accepted/rejected/error status.
- The shipped handler already sends the accepted response and tests the payload.

Therefore an HU margin payload on `0x8012` would test a disproven schema, not a
candidate feature mechanism. No throwaway send path or phone matrix is needed,
and none should be built from that premise.

Live Navbar viewport reconfiguration remains a separate unpromoted idea in
`docs/wishlist.md`. If promoted later, its design must start from the
gold-traced runtime update-config messages and keep service-discovery content
dimensions, rendering, and touch mapping coherent.

---

## F4. Key-event navigation map — NOT PROMOTED

The original sketch proposed mapping non-touch evdev devices (keyboard/HID or
kernel `gpio-keys`) to native actions or AA keycodes according to projection
focus. It was never a roadmap commitment. The idea remains in
`docs/wishlist.md` and requires its own promotion/design cycle before any
implementation work.
