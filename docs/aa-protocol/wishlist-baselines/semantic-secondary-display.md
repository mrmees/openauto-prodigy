# Native Semantic Secondary Display

## Baseline conclusion

A Prodigy-owned cluster-lite screen does not need another AA video channel.
Prodigy can render a second QML window from semantic navigation, media, and
phone-status messages already sent from the phone to the HU. This is distinct
from projected CLUSTER/AUXILIARY support and can ship independently.

Useful inputs are:

- navigation state, maneuver/road text, localized distance, ETA/destination,
  and lane/lookahead data where available;
- media source, playback state, position, title, artist, album, and artwork;
- phone/call status exposed by the existing phone-status handler; and
- later, Prodigy-native vehicle gauges from the sensor-provider layer.

AA controls the semantic content; Prodigy controls layout, styling, update
rate, privacy, stale-state handling, and which physical Qt/DRM screen owns the
window.

## Current Prodigy state

**Code-confirmed — Prodigy:** the orchestrator already registers navigation,
media-status, and phone-status handlers and publishes provider/event-bus state.
The main missing subsystem is explicit multi-window/screen ownership, followed
by a field-coverage and stale-state audit of those providers.

The native cluster should consume provider snapshots, not bind directly to
protocol-handler objects. That keeps it usable with future non-AA sources and
keeps channel reconnect details out of QML.

## Important limitations

- Semantic turn guidance is not a full map surface.
- Modern rich navigation messages are semantic-first; do not assume a live
  maneuver bitmap is always sent just because older image fields exist.
- Media control still uses the canonical AA input channel key events.
- Phone-derived state must clear or mark stale on session loss.
- A native semantic window has no AA `CarDisplayId`, video focus, decoder, or
  per-display input channel.

## Targeted spike

Run the existing USB screen beside the Pi HDMI output. Bind a read-only second
window to recorded provider fixtures, then to a live AA session. Verify screen
selection across restart/hotplug, update cadence, stale clearing, navigation
start/stop/reroute, media app changes, incoming/active/ended call, and display
loss. This spike is mostly Qt/DRM work; no new AA capability advertisement is
required.
