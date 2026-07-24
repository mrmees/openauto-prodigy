# Wishlist

This file contains unpromoted, user-facing capabilities that the project may
choose to build. Entries are not approved scope. Bugs, maintenance findings,
release tooling, and documentation debt belong in
[engineering-backlog.md](engineering-backlog.md); observations awaiting a
milestone check belong in [validation-current.md](validation-current.md).

Every entry carries three promotion-time qualifiers:

- **Stack** — whether the capability fits the current Prodigy/Companion stack
  or requires a substantial new subsystem or external component. “Current”
  still means implementation and design work are required.
- **Hardware** — whether the existing Pi/phone bench is sufficient, additional
  commodity hardware is required, or vehicle-specific hardware and wiring must
  be supplied. Hardware availability must be reconfirmed before promotion.
- **Investigation** — **Normal design** for an established implementation path,
  **Targeted spike** for one bounded unknown, or **Research first** when
  feasibility must be established before feature planning.

These labels describe the repository and bench as of 2026-07-24. Re-evaluate
all three when promoting an item; they are not estimates or approval.

## Projection and Audio

- **Android Auto assistant microphone transport** — capture the configured
  microphone when the phone opens the AVInput channel and return audio with the
  configured gain. This is protocol-critical work and requires its own
  main-tier design plus phone bench validation.
  **Stack:** Current Prodigy; the AVInput descriptor/handler and PipeWire
  capture service exist, but their production lifecycle is not connected.
  **Hardware:** A compatible microphone is required; the existing configuration
  already assumes one, but Pi capture must be bench-verified.
  **Investigation:** Targeted spike, not extensive feasibility research.

- **Keep AA system sounds audible during media playback** — define an
  intentional policy for touch clicks and other short System-channel sounds.
  Current priority ordering can mute System audio beneath AA media; the former
  five-second full-duck premise no longer matches the milestone implementation.
  **Stack:** Current Prodigy audio-focus and per-stream PipeWire stack.
  **Hardware:** Existing Pi/phone/audio bench.
  **Investigation:** Normal design.

- **Per-AA-channel volume controls** — provide separate user levels for media,
  calls/voice, and notifications/guidance while retaining master volume and
  focus-relative base-volume behavior.
  **Stack:** Current Prodigy audio, EQ, configuration, and QML stack.
  **Hardware:** Existing Pi/phone/audio bench.
  **Investigation:** Normal design with explicit focus and persistence rules.

- **Expose Prodigy local media inside Android Auto** — advertise the recovered
  CarLocalMedia service so USB/local playback metadata and controls can appear
  inside AA, with phone requests routed back to the canonical media actions.
  **Stack:** Extension within current Prodigy and its protocol library; the
  recovered schemas exist, but no production service descriptor or handler is
  wired.
  **Hardware:** Existing Pi/phone bench; removable media is optional test input.
  **Investigation:** Research first. Advertise only this service behind an
  experimental toggle and capture whether current Pixel/Moto phones activate
  service type 20 before designing the feature.

- **Native cluster-lite second display** — render Prodigy-owned turn data,
  calls, now-playing information, and later vehicle gauges in a separate QML
  window assigned to a configured Qt/DRM screen; do not require a second AA
  video stream.
  **Stack:** Current Prodigy providers and Qt/QML stack, extended with explicit
  multi-window/screen ownership.
  **Hardware:** A second display/output is required for completion; desktop
  simulation can cover only part of the work.
  **Investigation:** Targeted spike for Pi multi-screen/DRM placement; feature
  feasibility itself is established.

- **Long-term: AA/native blended dashboard** — reserve a stable native region
  for gauges, climate, camera, or media while Android Auto owns the remaining
  viewport, with authoritative touch and focus boundaries.
  **Stack:** Substantial extension of current Prodigy display, AA service
  discovery, input, and dashboard ownership.
  **Hardware:** An ultrawide or otherwise representative display is required
  for final validation; a simulated resolution may support early exploration.
  **Investigation:** Research first. Confirm current-phone behavior for native
  element/inset/blended-UI fields before committing to a layout architecture.

- **Long-term: live navbar viewport changes during projection** — renegotiate
  the AA content region and update touch/navbar mappings through one
  authoritative runtime path when navbar visibility or edge changes. Wire ID
  0x8012 is not the update mechanism.
  **Stack:** Substantial extension of the current AA display/input path.
  **Hardware:** Existing Pi touchscreen and phone bench.
  **Investigation:** Research first because a supported live renegotiation
  mechanism has not been established.

- **Long-term: true projected multi-display Android Auto** — advertise and own
  separate MAIN/CLUSTER video channels, decoders, renderers, focus state,
  configuration, and input associations, initially for navigation-focused
  secondary content and additional resolutions.
  **Stack:** Substantial multi-instance refactor of the current singleton AA
  video path, using protocol definitions already present in the stack.
  **Hardware:** A second physical display/output is required for completion.
  **Investigation:** Research first. Validate phone activation and per-display
  lifecycle with an isolated descriptor/capture spike before implementation.

## Phone, Companion, and Connectivity

- **Native active-call controls outside projection** — expose at least hangup
  for an Active native HFP call when the user is outside the Phone view and
  Android Auto is not presenting the call UI. Mute and DTMF can be considered
  during design.
  **Stack:** Current Prodigy telephony provider, ActionRegistry, overlays, and
  QML stack.
  **Hardware:** Existing Pi/phone HFP bench.
  **Investigation:** Normal design.

- **Long-term: companion notifications on the head unit** — add an additive
  Companion/API notification contract and a distraction-aware widget or
  overlay presentation through NotificationService.
  **Stack:** Current Prodigy plus Companion, with additive frozen API work.
  **Hardware:** Existing Pi/Android phone bench.
  **Investigation:** Normal design, including Android permission and
  distraction-policy decisions.

## Customization and Content

- **Visible two-minute pairing action** — replace the persistent-looking
  pairable switch with an “Allow New Pairings for 2 Minutes” action and
  authoritative open/closed feedback. Keep the finite BlueZ timeout.
  **Stack:** Current Prodigy Bluetooth and QML stack.
  **Hardware:** Existing Pi/phone Bluetooth bench.
  **Investigation:** Normal design.

- **Browser theme and wallpaper upload** — add preview, progress, and drag/drop
  to themes.html using the shipped theme-install endpoint.
  **Stack:** Current web-config and ThemeService stack.
  **Hardware:** Existing Pi/browser bench.
  **Investigation:** Normal design.

- **Long-term: web-config advanced EQ editor** — expose EqualizerService state
  and preset CRUD through explicit IPC, then provide per-stream band controls.
  Decide during design whether EQ also gains a frozen-additive External API
  surface.
  **Stack:** Current Prodigy web-config, IPC, EQ, and configuration stack.
  **Hardware:** Existing Pi/audio/browser bench.
  **Investigation:** Normal design.

- **Long-term: seed example web widgets on fresh installs** — offer sample
  content after the widget catalog is broad enough to be useful, without
  silently overwriting user packages.
  **Stack:** Current widget packaging and installer stack.
  **Hardware:** Existing Pi/browser bench.
  **Investigation:** Normal design after more widget types exist.

- **Backup and restore user content** — preserve widgets, themes, and config
  across reflashes through an explicit, version-aware workflow.
  **Stack:** Current Prodigy packaging, configuration, and web-config stack.
  **Hardware:** No new device is required; removable or network storage can be
  an optional transfer target.
  **Investigation:** Normal design with schema/version and secret-handling
  policy.

- **Long-term: persistent, isolated web-widget storage** — give widgets durable
  per-widget profiles/origins instead of the current ephemeral shared browser
  storage.
  **Stack:** Current Qt WebEngine widget host, extended with per-package profile
  and origin ownership.
  **Hardware:** Existing Pi/browser bench.
  **Investigation:** Targeted spike for Qt WebEngine profile lifecycle and
  migration behavior.

- **Long-term: WebAppHost** — add manifest-driven fullscreen streaming
  applications with persistent per-app profiles, bounded process lifetime,
  login input strategy, and explicit audio-focus policy. The design seed remains
  in [the archived web-surface strategy](archive/plans/2026-07-07-web-surface-strategy-design.md).
  **Stack:** Substantial extension of the current Qt WebEngine/plugin stack; no
  separate phone app is inherently required.
  **Hardware:** Existing Pi/display bench for the host, but individual services
  may impose codec, DRM, or input requirements.
  **Investigation:** Research first for process isolation, service compatibility,
  DRM/codec limits, login input, and audio routing.

## Extensions and Vehicle Hardware

- **Long-term: vehicle sensor-provider bridge and telemetry** — normalize
  external GPS, OBD-II, CAN, and GPIO sources behind bounded providers, expose
  selected gauges through dashboards/API, and advertise only real supported
  location, speed, gear, fuel/range, odometer, motion, and driving-state data to
  Android Auto.
  **Stack:** Extension within current Prodigy plugins, providers, API, and AA
  sensor channel; Companion can consume the public provider/API surface.
  **Hardware:** At least one external GNSS, OBD-II, CAN, or GPIO adapter is
  required; current availability must be inventoried before promotion.
  **Investigation:** Targeted spike per adapter and sensor type. The overall
  path is established, but data quality, permissions, rates, and vehicle mapping
  require bench evidence.

- **Backup-camera integration** — provide a low-latency camera surface with an
  explicit activation source, safe projection/media interaction, and a clear
  unavailable state.
  **Stack:** Extension within current Prodigy media/display/plugin ownership.
  **Hardware:** A compatible camera/capture path and reverse-activation source
  are required and are not assumed to be on the current bench.
  **Investigation:** Targeted spike for the camera/capture hardware path before
  UI planning.

- **Long-term: native FM radio** — integrate RTL-SDR tuning and RDS station/
  track metadata as a first-class Prodigy media source.
  **Stack:** New radio/media plugin within the current Prodigy service and audio
  architecture.
  **Hardware:** RTL-SDR tuner and suitable antenna are required and not assumed
  to be available.
  **Investigation:** Targeted spike for tuner support, RF quality, PipeWire
  routing, and RDS reliability.

- **Long-term: expose broadcast radio inside Android Auto** — advertise the AA
  terrestrial-radio service so a native Prodigy tuner can be controlled through
  the phone-rendered AA interface, including station and RDS metadata.
  **Stack:** Extension within the current AA protocol stack, dependent on a
  completed native radio source.
  **Hardware:** The native-radio tuner/antenna hardware is required.
  **Investigation:** Research first. Isolate radio service discovery and capture
  current-phone activation before planning full control semantics.

- **Key-event, rotary, and steering-wheel navigation map** — map keyboard, HID,
  rotary encoder, GPIO, or CAN buttons to native focus/back/media actions and AA
  D-pad, rotary, media, call, Assistant, and navigation events through
  ActionRegistry and the canonical AA input path.
  **Stack:** Current Prodigy ActionRegistry and AA input stack, extended with
  configurable relative/rotary input handling.
  **Hardware:** Keyboard/HID can cover bench behavior; vehicle completion needs
  the chosen encoder, GPIO, or CAN interface.
  **Investigation:** Targeted spike for AA rotary advertisement/event behavior;
  native action mapping is established.

- **Long-term: vehicle GPIO and power integration** — support the Miata's
  ignition sense, power latch, amplifier switching, dimmer, and MCP23017 needs
  through reusable plugin/hardware capabilities rather than one-off shell
  behavior. Vehicle wiring remains outside this repository.
  **Stack:** Extension within current Prodigy hardware/plugin providers plus
  external power-control circuitry.
  **Hardware:** Vehicle-specific wiring, MCP23017 and/or related interfaces, and
  safe power-latch hardware are required.
  **Investigation:** Research first for electrical safety, shutdown guarantees,
  fault behavior, and vehicle-specific signal validation.

- **Long-term: vehicle climate and control integration** — present bounded
  HVAC, heated-seat, defrost, door, lock, or mirror state/control through native
  UI and, only if proven viable, the recovered AA car-control surface. Begin
  read-only; writes require explicit per-vehicle allowlists and fail-safe rules.
  **Stack:** Substantial Prodigy provider/UI and AA protocol extension; no
  generic Companion-only implementation can supply missing vehicle controls.
  **Hardware:** A vehicle-specific CAN/VHAL-equivalent adapter and known signal
  definitions are required and are not present by assumption.
  **Investigation:** Research first. Establish phone activation, adapter access,
  legal/safety boundaries, and read/write semantics before feature planning.
