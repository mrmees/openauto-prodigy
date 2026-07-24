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

- **Long-term: native cluster-lite second display** — render Prodigy-owned turn data,
  calls, now-playing information, and later vehicle gauges in a separate QML
  window assigned to a configured Qt/DRM screen; do not require a second AA
  video stream.
  **Stack:** Current Prodigy providers and Qt/QML stack, extended with explicit
  multi-window/screen ownership.
  **Hardware:** Available without new purchases: retain the current USB display
  and attach an existing monitor to the Pi's native HDMI output.
  **Investigation:** Targeted spike for Pi multi-screen/DRM placement; feature
  feasibility itself is established.

- **Long-term: AA/native blended dashboard** — keep the normal Prodigy
  dashboard visible while embedding the MAIN Android Auto surface in a smaller
  QML region; AA decides how to arrange its own content within that advertised
  region while Prodigy owns the surrounding gauges, climate, camera, or media.
  This is not a secondary-display video channel.
  **Stack:** Substantial extension of current Prodigy display, AA service
  discovery, input, and dashboard ownership. The recovered blended-UI field is
  not yet in the hands-off in-tree protocol definitions and must arrive through
  an upstream protocol-library update.
  **Hardware:** Existing Pi touchscreen and phone bench; no ultrawide or second
  display is required for feasibility or implementation.
  **Investigation:** Research first. Trace the newer AA APK's asymmetric insets,
  native-element rectangles/types, corner radii, and runtime resize behavior,
  then test a bounded embedded MAIN viewport on current phones.

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
  **Hardware:** Available without new purchases: an existing HDMI monitor can
  run beside the current USB display.
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
  Companion phone location plus external GPS, OBD-II, CAN, and device-backend
  sources behind bounded providers, expose selected gauges through dashboards/
  API, and advertise only real supported location, speed, gear, fuel/range,
  odometer, motion, and driving-state data to Android Auto.
  **Stack:** Extension within current Prodigy providers, frozen-additive API,
  AA sensor channel, and Companion reporting. Companion GPS requires a new
  additive report; it is not present merely because the phone already knows its
  location.
  **Hardware:** OBD-II and CAN access are available. Companion GPS needs no new
  hardware; a PL2303GL-based GNSS receiver is an optional independent source.
  **Investigation:** Research first for the Companion-GPS-to-AA loop and whether
  location-with-speed or separate OBD/CAN speed changes Google Maps behavior;
  use targeted spikes for each physical adapter and sensor type.

- **Backup-camera integration** — provide a low-latency camera surface with an
  explicit activation action, safe projection/media interaction, and a clear
  unavailable state. GPIO, automation, or third-party software should all
  dispatch the same canonical action rather than own the camera UI directly.
  **Stack:** Extension within current Prodigy media/display/plugin ownership;
  activation goes through ActionRegistry and is callable through the External
  API.
  **Hardware:** USB webcams are available for prototyping. A production camera
  and any reverse-signal device remain installation choices, not API
  prerequisites.
  **Investigation:** Targeted spike for the camera/capture hardware path before
  UI planning.

- **Long-term: FM radio backend and optional native UI** — integrate RTL-SDR
  tuning, audio, and RDS station/track metadata behind a canonical radio
  backend. Prefer Android Auto as the primary interface if its recovered radio
  service works well enough; build a native Prodigy widget only if needed.
  **Stack:** New radio backend within the current Prodigy service, media, and
  audio architecture; a native widget is not a prerequisite.
  **Hardware:** An RTL-SDR Blog V4 is available; antenna suitability and Linux
  driver/tuning setup must be verified when promoted.
  **Investigation:** Targeted spike for tuner support, RF quality, PipeWire
  routing, and RDS reliability.

- **Long-term: expose broadcast radio inside Android Auto** — advertise the AA
  terrestrial-radio service so the Prodigy radio backend can be controlled
  through the phone-rendered AA interface, including station and RDS metadata.
  A successful AA surface may eliminate the need for a native FM widget.
  **Stack:** Extension within the current AA protocol stack plus a headless
  tuner/backend; it does not depend on completing a native radio UI.
  **Hardware:** No hardware is required for an initial simulated activation
  probe. The available RTL-SDR Blog V4 and a suitable antenna support real
  tuning validation later.
  **Investigation:** Research first. Isolate radio service discovery and capture
  current-phone activation before planning the backend or full control
  semantics.

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

- **Long-term: external device and vehicle-I/O integration** — keep pins,
  polarity, debounce, relays, timing, retries, and board/vehicle configuration
  in a separate user-selected backend service. Prodigy owns semantic action
  mapping, availability, UI/AA presentation, and safety policy rather than
  attempting to support every GPIO, CAN, serial, MQTT, or automation stack.
  **Stack:** The current External API already lets a backend register client-
  owned actions, receive ActionInvokedEvent, and dispatch Prodigy actions. An
  additive provider/report contract is still needed for state, acknowledgements,
  and backend errors.
  **Hardware:** Backend-defined. Existing GPIO, relay, OBD-II, and CAN equipment
  is sufficient for experiments; Prodigy requires no canonical board.
  **Investigation:** Research first for the semantic capability/state contract,
  disconnect behavior, authorization, electrical safety, and shutdown
  guarantees.

- **Long-term: experimental AA vehicle-control bridge** — discover what the
  recovered AA car-control surface can present, then map supported semantic
  controls such as fan, defrost, seat heat, or auxiliary switches to user-
  selected external-backend actions. Missing or unmapped controls report
  unavailable; AA never receives arbitrary GPIO or bus access.
  **Stack:** Substantial AA protocol/provider extension over the current
  External API action path. Reliable two-way controls also need the additive
  backend state/acknowledgement contract described above.
  **Hardware:** No hardware is required for a simulated feasibility probe. Real
  actuation uses whatever GPIO, relay, CAN, or other device the external backend
  defines.
  **Investigation:** Research first. Establish phone activation and UI/control
  behavior with a simulated backend before defining mappings, authorization,
  fail-safe behavior, or writes to real devices.
