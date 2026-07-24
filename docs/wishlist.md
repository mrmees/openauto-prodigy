# Wishlist

This file contains unpromoted, user-facing capabilities that the project may
choose to build. Entries are not approved scope. Bugs, maintenance findings,
release tooling, and documentation debt belong in
[engineering-backlog.md](engineering-backlog.md); observations awaiting a
milestone check belong in [validation-current.md](validation-current.md).

## Projection and Audio

- **Android Auto assistant microphone transport** — capture the configured
  microphone when the phone opens the AVInput channel and return audio with the
  configured gain. This is protocol-critical work and requires its own
  main-tier design plus phone bench validation.

- **Coordinate local playback with the phone's AA media session** — notify the
  phone of focus loss/gain when local playback takes or releases music focus so
  both sources do not continue independently.

- **Pause a Bluetooth source when it loses music focus** — send AVRCP Pause
  instead of merely making an A2DP phone inaudible while it continues playing.

- **Mix short AA system sounds over media** — define an intentional policy for
  touch clicks and other short System-channel sounds. Current priority ordering
  can mute System audio beneath AA media; the former five-second full-duck
  premise no longer matches the milestone implementation.

- **Per-AA-channel volume controls** — provide separate user levels for media,
  calls/voice, and notifications/guidance while retaining master volume and
  focus-relative base-volume behavior.

- **Live navbar viewport changes during projection** — renegotiate the AA
  content region and update touch/navbar mappings through one authoritative
  runtime path when navbar visibility or edge changes. Wire ID 0x8012 is not
  the update mechanism.

## Phone, Companion, and Connectivity

- **Persistent active-call controls** — expose at least hangup whenever a
  native HFP call is Active and the user is outside the Phone view. Mute and
  DTMF can be considered during design.

- **Battery charging indicator** — render the already-available companion
  charging state in BatteryWidget at dashboard scale.

- **Companion notifications on the head unit** — add an additive
  companion/API notification contract and a distraction-aware display policy
  through NotificationService and the overlay framework.

- **Proxy-route result feedback to API clients** — let a companion distinguish
  “SOCKS reported active” from “the head unit successfully applied the route”
  using a frozen-additive status or event.

- **Per-connection WiFi password rotation** — generate and deliver a fresh AP
  password during the Bluetooth handoff without a hostapd reload race that
  strands the connecting phone.

## Customization and Content

- **Visible two-minute pairing action** — replace the persistent-looking
  pairable switch with an “Allow New Pairings for 2 Minutes” action and
  authoritative open/closed feedback. Keep the finite BlueZ timeout.

- **Dashboard edit-mode latch** — let edit mode survive dashboard switches
  without reintroducing stale selection ownership.

- **Curated widget size bounds** — define useful per-widget minimum, maximum,
  and fixed-size constraints so the picker offers meaningful one-tap sizes.

- **Browser theme and wallpaper upload** — add preview, progress, and drag/drop
  to themes.html using the shipped theme-install endpoint.

- **Web-config advanced EQ editor** — expose EqualizerService state and preset
  CRUD through explicit IPC, then provide per-stream band controls. Decide
  during design whether EQ also gains a frozen-additive External API surface.

- **Seed example web widgets on fresh installs** — offer sample content without
  silently overwriting user packages.

- **Backup and restore user content** — preserve widgets, themes, and config
  across reflashes through an explicit, version-aware workflow.

- **Persistent, isolated web-widget storage** — decide whether widgets receive
  durable per-widget profiles/origins instead of the current ephemeral shared
  browser storage.

- **WebAppHost** — add manifest-driven fullscreen streaming applications with
  persistent per-app profiles, bounded process lifetime, login input strategy,
  and explicit audio-focus policy. The design seed remains in
  [the archived web-surface strategy](archive/plans/2026-07-07-web-surface-strategy-design.md).

## Extensions and Vehicle Hardware

- **OBD-II vehicle telemetry** — expose selected diagnostics and gauges through
  a bounded hardware plugin and dashboard contributions without coupling the
  shell to a specific adapter.

- **Backup-camera integration** — provide a low-latency camera surface with an
  explicit activation source, safe projection/media interaction, and a clear
  unavailable state.

- **FM radio** — integrate RTL-SDR tuning and RDS station/track metadata as a
  first-class media source.

- **Key-event and steering-wheel navigation map** — map keyboard, HID, or GPIO
  buttons to focus movement, back, media controls, and projection focus through
  ActionRegistry.

- **Miata GPIO/ignition/amp-control plugin** — package ignition sense, power
  latch, amplifier switching, dimmer, and MCP23017 behavior behind plugin and
  ActionRegistry boundaries. Vehicle wiring remains outside this repository.

- **In-car theme selection** — let the user preview and activate installed
  themes from the head unit; uploading and activating a theme remain separate
  actions.

- **Additional resolutions and multi-display layouts** — support hardware
  beyond the current 1024x600 single-display contract through explicit layout,
  scaling, projection-viewport, and touch-mapping rules.
