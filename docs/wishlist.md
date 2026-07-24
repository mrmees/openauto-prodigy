# Wishlist

This file tracks actionable outcomes that have not been promoted into
docs/roadmap-current.md. Completed work, superseded ideas, theoretical review
notes, and investigation history belong in archived plans, reviews, and session
handoffs. Promotion requires a fresh design against the current tree; a bullet
here is context, not an approved implementation plan.

## Reliability and Correctness

- **Configuration-surface integrity** — make every exposed setting round-trip
  through its canonical owner or remove it. The current gaps are web-config
  fields that IpcServer ignores (video resolution, brightness, and night mode),
  the Debug page sending the video.codecs sequence through scalar accessors,
  and installer-written display dimensions that runtime does not consume.
  Preserve the shipped generic ConfigService/YamlConfig traversal; add typed
  sequence handling, capability-aware codec advertising, visible write
  failures, and schema coverage instead of reviving the superseded 2026-02-21
  config-contract plan.

- **Bluetooth startup and pairing resilience** — pace and retry failed initial
  BlueZ GetManagedObjects snapshots, refresh the pairing prompt name when the
  snapshot fills the device cache, and present PairableTimeout=120 as a
  momentary “Allow New Pairings for 2 Minutes” action with authoritative
  PropertiesChanged feedback. Keep the finite window: an app crash must not
  leave the head unit indefinitely pairable.

- **Boot and service observability** — eliminate the /var/run/sdp ownership
  race with creation-aware or sufficiently bounded service ordering, and retain
  capped persistent user-service logs so PipeWire/WirePlumber failures survive
  a reboot without unbounded SD-card growth.

- **Audio-graph recovery after daemon disruption** — reconnect and
  re-enumerate AudioService after PipeWire restarts, recreate BT EQ tap streams,
  detect/recover a missing BlueZ HFP profile after external stack restarts, and
  adopt pre-existing live bluez_input nodes when the tap comes up. Source and
  prebuilt installers already encode Bluetooth-before-audio restart order; this
  item is runtime recovery, not another installer-order change.

- **Android Auto reconnect connection ownership** — disconnect or replace the
  mediaStatusHandler and navigation-handler EventBus connections created for
  each new AA session so publications do not multiply across reconnects.

- **NavigationTurnLabel invalid UTF-8 log flood** — sanitize or safely handle
  the phone-provided bytes at the parse boundary without editing the hands-off
  community proto subtree here. Preserve useful diagnostics without one malformed
  label drowning the journal.

- **ApiServer failed-start retry hygiene** — make start failure clean up its
  TCP/WebSocket listeners and publisher set, or require a full stop before
  retry, so a second start cannot leak servers or duplicate publishers.

- **Master-volume persistence retry cap** — bound retries after a volume change
  when config storage remains unwritable, then emit one final actionable
  warning instead of writing every two seconds until shutdown.

- **Backward clock-step agreement** — compare consecutive phone reports against
  a drift-adjusted target rather than identical millisecond timestamps so the
  three-report safety gate can accept a legitimate large backward correction.

- **Local-media edge cases** — treat legitimate sub-500 ms tracks as playable;
  reconcile Filesystem MountPoints property changes; expose an authoritative
  removable/canEject model role; define nested-root precedence and record-level
  canonical deduplication; and make hot-plug drive-property lookup asynchronous.

- **Web-config hardening** — design authentication once across every route,
  replace English-substring IPC status mapping with structured
  transport-versus-application errors, and add wallpaper-path agreement
  coverage that exercises a real file and directory rejection.

## Product and UX Candidates

- **Android Auto assistant microphone transport** — connect
  AVInputChannelHandler micCaptureRequested to a configured AudioService capture
  stream and feed the protocol channel with mic gain applied. This is
  protocol-critical work and needs its own main-tier promotion plus Pixel
  end-to-end bench validation.

- **Native call UI ownership** — suppress the native incoming-call popup while
  Android Auto owns the call UI; outside projection, keep answer/reject touch
  ownership in the shell and expose a persistent Active-call affordance with at
  least hangup. The current popup can render above AA while raw evdev touch
  still routes underneath it.

- **Cross-source audio policy** — evaluate AA focus loss/gain messages when
  local playback starts or stops, send AVRCP Pause when Bluetooth music loses
  music focus, and partial-duck or mix short AA System sounds instead of
  silencing media for the entire channel-open window. Add per-role user volumes
  for AA media, calls/voice, and notifications/guidance, including coverage
  where AudioStreamHandle baseVolume is not 1.0.

- **Live navbar viewport reconfiguration during AA** — provide one
  authoritative update path for shell viewport, protocol margins/content
  configuration, touch dimensions, and evdev navbar zones. Do not design this
  around wire ID 0x8012; current traces identify that as the head unit's theme
  token response, not viewport renegotiation.

- **Dashboard editing polish** — add an explicit edit-mode latch that survives
  dashboard switches and curate per-widget minimum/maximum grid sizes so
  fixed-size widgets regain a one-tap preset.

- **Battery charging indicator** — render the already-delivered
  CompanionState.phoneCharging state in BatteryWidget, for example with a bolt
  glyph that remains legible at dashboard scale.

- **Browser theme and wallpaper upload UI** — add preview, progress, and
  drag/drop to themes.html using the shipped POST /api/theme/install contract.

- **Web-config advanced EQ editor** — expose EqualizerService state and preset
  CRUD through explicit IPC commands, then add per-stream band controls. Decide
  during design whether remote EQ stays web/IPC-only or also gains an additive
  External API capability.

- **Installer seeding and user-content recovery** — optionally seed example web
  widgets on a fresh install and provide a deliberate backup/restore path for
  user widgets, themes, and config across reflashes.

- **FM radio** — integrate RTL-SDR tuning and RDS station/track metadata as a
  first-class media source.

- **Companion notifications** — add an additive companion/API notification
  contract and a distraction-aware display policy using NotificationService
  and the overlay framework.

- **Key-event and steering-wheel navigation map** — map keyboard, HID, or GPIO
  buttons to focus movement, back, media controls, and projection focus through
  ActionRegistry.

- **Per-connection WiFi password rotation** — generate and deliver a fresh AP
  password during the Bluetooth handoff, coordinating hostapd reload timing so
  credential rotation cannot strand the connecting phone.

- **WebAppHost** — host manifest-driven fullscreen streaming apps as launcher
  entries with persistent per-app profiles, bounded process lifetime, login
  input strategy, and explicit audio-focus policy. The design seed remains in
  docs/archive/plans/2026-07-07-web-surface-strategy-design.md.

- **Miata GPIO/ignition/amp-control plugin** — package ignition sense, power
  latch, amplifier switching, dimmer, and MCP23017 behavior behind the plugin
  and ActionRegistry boundaries; keep vehicle-specific wiring outside this
  public repository.

- **API proxy-route status feedback** — add a frozen-additive status/event so a
  companion can distinguish “reported SOCKS5 active” from “route applied by the
  head unit.”

## Developer, Release, and Documentation

- **Web-widget platform hardening** — finish the residual shim contract:
  reconnect-aware readiness, acknowledged subscribe, server-side unsubscribe,
  resilient malformed-URL handling, and connection-level error delivery.
  Tighten package entry validation, latch live widget URLs, document/fix the
  startup-frozen package catalog before dynamic installs, pin the JS proto
  generation toolchain, decide persistent per-widget storage/origin isolation,
  and replace full per-page delegate instantiation with a filtered model.
  Already-shipped prototype-key validation, closed-socket rejection,
  crash-reload context refresh, and the author limitations guide are not part
  of this item.

- **External API reference** — distill the shipped v1 contract into
  docs/reference/external-api.md: transports, pairing, request/response and
  subscription flows, capability flags, errors, and compatibility rules.

- **Release engineering hardening** — validate packager version/target path
  components, require annotated tag ↔ HEAD ↔ embedded binary agreement before
  publishing, add a version-aware patched-libspa upgrade path, and guard
  cross-build.sh against orphan containers. Ext4 build storage and ccache are
  optional performance follow-ups, not release correctness requirements.

- **Secret and documentation checks** — run a repository/history secret scan
  and extend scripts/check-doc-links.py to validate backticked Markdown paths in
  live documentation as well as explicit links.

## Long-Term Architecture

- **Three-tier community architecture** — separate language-neutral protocol
  definitions, reusable protocol implementations, and the Qt/QML application.
  De-Qt the current protocol implementation behind a deliberately chosen
  non-Qt ownership/event/transport abstraction, then split it into a public
  repository that openauto-prodigy consumes alongside open-android-auto.

## Current-Release Validation Checks

These are observations to re-test against ALPHA-26-07-24-01 before promoting
implementation. Remove a check when current hardware validation passes; turn it
into a scoped wishlist outcome only when it still reproduces.

- **Clean boot timing** — measure power-on to application readiness and confirm
  systemd-networkd-wait-online no longer delays graphical/application startup
  now that current installers disable that obsolete wait path.

- **Bluetooth advertising after disconnect** — disconnect one paired phone and
  confirm a second new phone can discover the head unit without restarting the
  app.

- **Startup D-Bus marshalling warnings** — capture a clean boot journal and
  check whether the prior read-only QDBusArgument and unregistered MediaPlayer1
  Track type warnings still occur.
