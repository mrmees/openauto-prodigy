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

- **Keep AA system sounds audible during media playback** — define an
  intentional policy for touch clicks and other short System-channel sounds.
  Current priority ordering can mute System audio beneath AA media; the former
  five-second full-duck premise no longer matches the milestone implementation.

- **Per-AA-channel volume controls** — provide separate user levels for media,
  calls/voice, and notifications/guidance while retaining master volume and
  focus-relative base-volume behavior.

- **Long-term: live navbar viewport changes during projection** — renegotiate
  the AA content region and update touch/navbar mappings through one
  authoritative runtime path when navbar visibility or edge changes. Wire ID
  0x8012 is not the update mechanism.

## Phone, Companion, and Connectivity

- **Native active-call controls outside projection** — expose at least hangup
  for an Active native HFP call when the user is outside the Phone view and
  Android Auto is not presenting the call UI. Mute and DTMF can be considered
  during design.

- **Long-term: companion notifications on the head unit** — add an additive
  companion/API notification contract and a distraction-aware widget or
  overlay presentation through NotificationService.

## Customization and Content

- **Visible two-minute pairing action** — replace the persistent-looking
  pairable switch with an “Allow New Pairings for 2 Minutes” action and
  authoritative open/closed feedback. Keep the finite BlueZ timeout.

- **Browser theme and wallpaper upload** — add preview, progress, and drag/drop
  to themes.html using the shipped theme-install endpoint.

- **Long-term: web-config advanced EQ editor** — expose EqualizerService state
  and preset CRUD through explicit IPC, then provide per-stream band controls.
  Decide during design whether EQ also gains a frozen-additive External API
  surface.

- **Long-term: seed example web widgets on fresh installs** — offer sample
  content after the widget catalog is broad enough to be useful, without
  silently overwriting user packages.

- **Backup and restore user content** — preserve widgets, themes, and config
  across reflashes through an explicit, version-aware workflow.

- **Long-term: persistent, isolated web-widget storage** — give widgets durable
  per-widget profiles/origins instead of the current ephemeral shared browser
  storage.

- **Long-term: WebAppHost** — add manifest-driven fullscreen streaming
  applications with persistent per-app profiles, bounded process lifetime,
  login input strategy, and explicit audio-focus policy. The design seed remains
  in [the archived web-surface strategy](archive/plans/2026-07-07-web-surface-strategy-design.md).

## Extensions and Vehicle Hardware

- **Long-term: OBD-II vehicle telemetry** — expose selected diagnostics and
  gauges through a bounded hardware plugin and dashboard contributions without
  coupling the shell to a specific adapter.

- **Backup-camera integration** — provide a low-latency camera surface with an
  explicit activation source, safe projection/media interaction, and a clear
  unavailable state.

- **Long-term: FM radio** — integrate RTL-SDR tuning and RDS station/track
  metadata as a first-class media source.

- **Key-event and steering-wheel navigation map** — map keyboard, HID, or GPIO
  buttons to focus movement, back, media controls, and projection focus through
  ActionRegistry.

- **Long-term: vehicle GPIO and power integration** — support the Miata's
  ignition sense, power latch, amplifier switching, dimmer, and MCP23017 needs
  through reusable plugin/hardware capabilities rather than one-off shell
  behavior. Vehicle wiring remains outside this repository.

- **Long-term: additional resolutions and multi-display Android Auto** —
  explore hardware beyond the current 1024x600 single-display contract,
  including shell layout and scaling, projection viewports, touch mapping, and
  AA multi-display capabilities.
