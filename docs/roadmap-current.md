# Current Roadmap

Governance: capture user-facing capability ideas in `docs/wishlist.md`, concrete
technical findings in `docs/engineering-backlog.md`, and unconfirmed hardware
observations in `docs/validation-current.md`. Only promoted work should appear
in this roadmap. Wishlist entries must state current-stack fit, required or
unavailable hardware, and whether promotion needs normal design, a targeted
spike, or research-first feasibility work.

> **Parity program status (updated 2026-07-21):** the 2026-07-05 design sprint
> (`docs/archive/plans/2026-07-05-fable-work-program-design.md`, phases A–F)
> shipped External API v1, HTML/JS web widgets, HFP call audio,
> multi-dashboards + overlays, theme upload, and the two-stage local media
> player. The EQ parity audit is complete. No promoted Phase F work remains:
> the proposed `0x8012` margin-push experiment was closed after the gold-traced
> protocol definition established that the message is a theming-token response,
> and the key-event map remains an unpromoted wishlist idea. The completed Phase
> F light-plan record is archived.

## Now

- Android Auto GAL 5.0–6.0 production compatibility — **ACTIVE**. Restore the
  released open-android-auto `v1.5` `dist` boundary, move GAL selection out of
  the CLUSTER laboratory into durable session-wide configuration, then advance
  through separately hardware-accepted 5.0, 5.1, and 6.0 checkpoints. Each
  accepted version becomes a supported production choice and the shipped
  default advances to the highest accepted tuple, ending at 6.0. The work adds
  ackless audio and single-codec display policy at 5.0, typed MediaOptions and
  energy-forecast tolerance at 5.1, and typed modern video options at 6.0.
  H.265 remains the accepted production codec from repeated live validation;
  the default order is restored to H.265 then H.264 after GAL 5.0 accidentally
  made the legacy H.264-first list decisive. H.264 GAL 6.0 fallback behavior is
  proven on `2bc574e`. The remaining hardware gate is one H.265 matrix with
  independent operator checkpoints, three reconnects, a second-phone smoke,
  and lower-GAL smokes—not a new codec A/B or comparative resource threshold.
  Requested-version authority and explicit lower-version fallbacks remain.
  GAL 6.1, semantic unresolved options, EV UI, outgoing media stats, overlays,
  AUXILIARY, and third-display work are excluded. Design and plan:
  `docs/plans/2026-07-26-aa-gal-5-0-to-6-0-production-compatibility-design.md`
  and
  `docs/plans/2026-07-26-aa-gal-5-0-to-6-0-production-compatibility-plan.md`.

## Done (recent)

- Android Auto GAL 4.3 display compatibility — **COMPLETE 2026-07-26**
  (Pi/Pixel live-validated). Prodigy retains GAL 1.7 as its default and adds a
  default-off 4.3 laboratory profile. A requested 4.3 session accepts a MATCH
  response reporting 4.3 or higher, while the requested tuple remains the sole
  local feature-policy input. Every modern field-11 declaration carries field-1
  companion insets that preserve the legacy margins and AA's usable-aspect
  chrome choice. Live MAIN composition matched the 1.7 Navbar boundary;
  CLUSTER native false/true/restored-false kept identical crop geometry while
  the phone-rendered maneuver banner was present/absent/present. The final
  repository/ARM gates passed, and the bounded Fable review found no blocker or
  major; its three minor research findings are deferred in the engineering
  backlog. ADB/logcat unavailability remains an evidence-source limitation, not
  an unresolved defect. Design and plan:
  `docs/archive/plans/2026-07-26-aa-gal-4-3-display-compatibility-design.md` and
  `docs/archive/plans/2026-07-26-aa-gal-4-3-display-compatibility-plan.md`.

- Android Auto AUXILIARY turn-card role-swap experiment — **COMPLETE
  2026-07-25** (Pi/Pixel live-validated). AA 17.3 accepted MAIN ID 0 plus an
  AUXILIARY ID 1 on the existing secondary channels 12/13. AUXILIARY/UNKNOWN
  opened but produced no decodable frame. AUXILIARY/TURN_CARD was idle without
  a route, including while YouTube Music played, and rendered a compact
  maneuver card when a Maps route became active. The existing session-bit-16
  toggle made no content difference. The temporary descriptor and Pi binary
  were restored to CLUSTER after capture. Maps 26.30.05 identifies a separate
  AUXILIARY/NAVIGATION selector, but its missing local enum and AA 17.3
  provenance are tracked in open-android-auto issue #14 before another phase.
  Plan:
  `docs/archive/plans/2026-07-25-aa-auxiliary-role-swap-experiment.md`.

- Android Auto runtime CLUSTER lab — **COMPLETE 2026-07-25** (Pi/Pixel
  live-validated). The default-off experiment now accepts process-lifetime
  480p/720p resolution, DPI, centered content geometry, and the AA 17.3
  experimental turn-data toggle through Debug Settings or `aa.cluster.*`
  External API action dispatch. Valid active edits stage one snapshot and
  renegotiate through an AA-only reconnect; descriptor, decoded-frame
  validation, and the aspect-aware widget crop activate together. No YAML edit
  or Prodigy restart is required. MAIN descriptors/decoding/focus/touch and the
  single-CLUSTER scope remain unchanged. A 24-capture live matrix with
  navigation active, navigation stopped, and YouTube Music playing showed
  Google Maps in every CLUSTER profile: navigation only added route UI, while
  the turn-data toggle and media playback selected no alternate content.
  Issue #10 subsequently confirmed the toggle's `session_configuration` value
  16 has no AA 17.3 consumer. Historical capture notes reported GAL 1.1 while
  current source requests 1.7; the active GAL 4.3 compatibility plan begins by
  capturing the deployed request to resolve that discrepancy. Resolution,
  crop geometry, and DPI visibly changed framing/scale and negotiated the
  expected carrier.
  Plan:
  `docs/archive/plans/2026-07-25-aa-runtime-cluster-lab-plan.md`.

- Android Auto CLUSTER square viewport — **COMPLETE 2026-07-25**
  (Pi/Pixel live-validated). Prodigy retains the protocol-required 800×480
  H.264 carrier while requesting a centered 300×300 phone-rendered CLUSTER
  region through total margins of 500×180. One fixed 3×3 dashboard widget
  clips and uniformly upsizes that region without a second decoder, frame
  rewrite, shader, stretch, or projected input. The Pixel opened CLUSTER
  channels 12/13, delivered an exact 800×480 decoded carrier, and reached
  Rendering without a geometry mismatch; Matthew accepted the visible basic
  implementation. MAIN behavior, default-off policy, and the experimental
  scope remain unchanged. Design and plan:
  `docs/archive/plans/2026-07-25-aa-cluster-square-viewport-design.md` and
  `docs/archive/plans/2026-07-25-aa-cluster-square-viewport-plan.md`. The live
  Pi/Pixel result corresponds to the accepted implementation through
  `7ccdfc4`; subsequent pre-publication CLUSTER-only lifecycle hardening is
  covered by the repository build, test, and ARM cross-build gates but remains
  pending the next routine hardware regression and is not described as a
  separate hardware-validation pass. Established legacy-channel behavior is
  unchanged.

- Android Auto projected CLUSTER dashboard widget feasibility spike —
  **COMPLETE 2026-07-24** (Pi/Pixel positive result). Behind a default-off
  startup flag, Prodigy advertises one fixed H.264 800×480 CLUSTER display on
  independent video/input channels and renders the phone-produced stream in a
  fixed 2×2 dashboard widget. The Pixel 8 accepted both descriptors, opened
  channels 12/13, streamed and decoded an independent Maps surface, kept it
  flowing on the native dashboard after MAIN exited, and returned to healthy
  MAIN projection afterward. The flag-off path retained the legacy MAIN-only
  descriptor and reconnect behavior. The bench did not have an active Maps
  route during capture, so route-specific maneuver presentation remains a
  follow-up rather than an inferred result. Generalized multi-display,
  alternate resolutions, second-monitor routing, and production enablement
  remain unpromoted. Design and plan:
  `docs/archive/plans/2026-07-24-aa-projected-cluster-widget-design.md` and
  `docs/archive/plans/2026-07-24-aa-projected-cluster-widget-plan.md`.

- Android Auto Assistant microphone transport — **COMPLETE 2026-07-24**
  (Pi/Pixel live-validated). Prodigy now captures the configured Pi microphone
  as bounded 16 kHz mono PCM, applies configured gain, obeys the phone's
  AVInput transmit window, and quiesces capture before teardown. Live Assistant
  recognition completed across repeated clean capture open/close cycles while
  wireless H.265 projection remained healthy. Design and plan:
  `docs/archive/plans/2026-07-24-aa-assistant-microphone-transport-design.md`
  and
  `docs/archive/plans/2026-07-24-aa-assistant-microphone-transport-plan.md`.

- Installer and deployment lifecycle remediation — **COMPLETE 2026-07-24**
  (Pi-live-validated). Source installs now bind to their complete checkout and
  own child-process and rebuild lifetimes. Both install modes share validated
  touch, AP-radio, network, preflight, and unit contracts. Prebuilt upgrades
  stage and recover managed payloads while preserving service activity and
  enablement, and resolution validation cleans up only its own verified Xvfb
  child. Live validation covered Pi touch/WiFi capability parsing, temporary
  success and rollback transactions, real Wayland/PipeWire socket conditions,
  one responsive production process, byte-identical configuration, and
  unchanged hostapd/Bluetooth lifetimes. Design and plan:
  `docs/archive/plans/2026-07-24-installer-deployment-lifecycle-remediation-design.md`
  and
  `docs/archive/plans/2026-07-24-installer-deployment-lifecycle-remediation-plan.md`.

- Media lifecycle and persistence remediation — **COMPLETE 2026-07-24**
  (Pi-live-validated). Exact saved tracks alone own saved positions, explicit
  user transport actions durably take ownership, and shuffle, repeat, and seek
  mutations persist at their boundary. Media scanning now quiesces before
  eject and shutdown with generation-safe cancellation, while valid-fixture
  errors and scanner timeouts fail deterministically. Live validation covered
  exact and fallback restores, late-source retry, user takeover, mode restart,
  cold-scan restart, active-scan safe eject and physical remount, local audio
  and shared media state, one responsive process, byte-identical configuration
  restoration, and unchanged hostapd/Bluetooth lifetimes. Design and plan:
  `docs/archive/plans/2026-07-24-media-lifecycle-persistence-remediation-design.md`
  and
  `docs/archive/plans/2026-07-24-media-lifecycle-persistence-remediation-plan.md`.

- UI input and widget-grid lifecycle remediation — **COMPLETE 2026-07-24**
  (Pi-live-validated). Navbar evdev claims now follow rendered 20/60/20 QML
  geometry with explicit touch-slot and popup-generation ownership. Widget
  remaps keep retained placements and reserved pages reachable, apply pending
  remaps before edits, and persist page/dimension baselines across restarts.
  Plugin deactivation is ordered ahead of replacement activation while its QML
  view and context retire safely after the current dispatch; one owner also
  replaces screen-DPI connections. Two review passes returned three findings
  each; all six were confirmed and fixed, with none dismissed. The deployed
  shell and dashboard were live-validated before wireless H.265 re-entry; one
  responsive process remained, hostapd and Bluetooth were unchanged with zero
  restarts, and the exact original configuration was restored. Design and plan:
  `docs/archive/plans/2026-07-24-ui-input-widget-grid-lifecycle-remediation-design.md`
  and
  `docs/archive/plans/2026-07-24-ui-input-widget-grid-lifecycle-remediation-plan.md`.

- Audio and equalizer real-time safety remediation — **COMPLETE 2026-07-23**
  (Pi-live-validated). The audio ring and PipeWire ingestion paths now have
  single-owner cursors, bounded validated buffers, and non-RT diagnostics.
  Equalizer inputs and restoration are guarded, while coefficient publication
  uses a bounded atomic snapshot instead of recyclable shared storage. The
  deployed application retained responsive IPC, wireless H.265 projection,
  active AA audio graphs, and the Bluetooth EQ graph without changing hostapd
  or Bluetooth service lifetimes; the original Pi configuration was restored
  byte-for-byte. Design and plan:
  `docs/archive/plans/2026-07-23-audio-eq-rt-safety-remediation-design.md` and
  `docs/archive/plans/2026-07-23-audio-eq-rt-safety-remediation-plan.md`.

- Bluetooth, HFP, and AVRCP state remediation — **COMPLETE 2026-07-23**
  (Pi-live-validated). Bluetooth state now comes from asynchronous, coalesced
  BlueZ snapshots; the pairing agent implements explicit decision and display
  semantics; AVRCP discovery and player loss are lifecycle-safe; and telephony
  accepts call and transport evidence only from one selected audio gateway.
  Deployment immediately adopted the already-connected Pixel, retained both
  phone players, drove Moto playback through the A2DP EQ path with advancing
  millisecond progress, and kept the selected Pixel gateway stable. One
  responsive process remained live while hostapd and Bluetooth retained their
  PIDs and restart counts. Design and plan:
  `docs/archive/plans/2026-07-23-bt-hfp-avrcp-state-remediation-design.md`
  and
  `docs/archive/plans/2026-07-23-bt-hfp-avrcp-state-remediation-plan.md`.

- Android Auto input, video, and night remediation — **COMPLETE 2026-07-23**
  (Pi/Pixel live-validated). Decoder reset commands now form ordered stream
  boundaries; evdev touch ownership and mapping updates stay reader-safe; one
  application-lifetime service drives shell and AA night state; and active
  projection rejects replacement clients while listener ports and video-config
  counts share validated owners. Live validation decoded forced H.264 and
  H.265 hardware sessions, repeated H.265 after a same-process reconnect,
  rejected an extra active TCP client, and delivered matching forced day/night
  state without changing hostapd or Bluetooth service lifetimes. Design and
  plan:
  `docs/archive/plans/2026-07-23-aa-input-video-night-remediation-design.md`
  and
  `docs/archive/plans/2026-07-23-aa-input-video-night-remediation-plan.md`.

- Android Auto protocol crypto and flow-control remediation — **COMPLETE
  2026-07-23** (Pi/Pixel live-validated). OpenSSL initialization and runtime
  I/O now fail closed with explicit diagnostics; encrypted frames require
  structurally complete TLS records. Fragmented messages retain and enforce
  their declared totals under per-message and aggregate bounds. Audio receive
  permits replenish per accepted frame, liveness uses correlated pongs and the
  configured deadline, and rerouting preserves active guidance. Deployment
  retained one responsive application process, reopened every AA service
  channel, and resumed H.265 hardware projection without restarting hostapd or
  Bluetooth. Design and plan:
  `docs/archive/plans/2026-07-23-aa-protocol-crypto-flow-remediation-design.md`
  and
  `docs/archive/plans/2026-07-23-aa-protocol-crypto-flow-remediation-plan.md`.

- Android Auto session and transport lifecycle remediation — **COMPLETE
  2026-07-22** (Pi/Pixel live-validated). Terminal session finalization now
  closes persistent handlers while they are alive and prevents stale or
  reentrant traffic from crossing replacement boundaries. Messenger restart
  resets framing, assembly, TLS, and send state; fatal TLS failures surface
  immediately; channel-open responses use one validated service-channel path;
  and transient RFCOMM listener startup failures retry within a bounded budget.
  Deployment retained one responsive application process, automatically
  reconnected the Pixel, and resumed H.265 projection without restarting
  hostapd or Bluetooth. Design and plan:
  `docs/archive/plans/2026-07-22-aa-session-transport-lifecycle-remediation-design.md`
  and
  `docs/archive/plans/2026-07-22-aa-session-transport-lifecycle-remediation-plan.md`.

- API and core asynchronous lifecycle remediation — **COMPLETE 2026-07-22**
  (Pi/Pixel live-validated). Prodigy and Companion now pair with an explicitly
  versioned 120-bit Base32 code; legacy credentials retire without an insecure
  fallback, malformed stores fail closed, and paired credentials persist
  atomically. The same wave bounds handshake and IPC work, closes API/EventBus/
  plugin teardown hazards, makes clock work asynchronous, repairs reconnect
  state, and suppresses duplicate route mutation. Coordinated deployment proved
  QR pairing, saved-credential reconnect, live reports and SOCKS state, split
  and coalesced IPC, wireless H.265 projection, and unchanged Bluetooth/hostapd
  lifetimes. Design and plan:
  `docs/archive/plans/2026-07-22-api-core-async-lifecycle-remediation-design.md`
  and
  `docs/archive/plans/2026-07-22-api-core-async-lifecycle-remediation-plan.md`.

- Configuration startup contract remediation — **COMPLETE 2026-07-22**
  (Pi-live-validated). Logging configuration now has typed schema and sequence
  boundaries shared by startup, settings, and strict IPC handling; selective
  categories and verbose mode apply live and persist across restart. Known
  map-shape conflicts enter the existing corrupt-config fallback, and the
  first configured brightness assignment reaches its backend even when it is
  the default value. Live validation proved both logging modes across restart,
  restored the original configuration byte-for-byte, and retained responsive
  IPC, wireless H.265 projection, A2DP, and unchanged Bluetooth/hostapd
  lifetimes. Design and plan:
  `docs/archive/plans/2026-07-22-config-startup-contract-remediation-design.md`
  and
  `docs/archive/plans/2026-07-22-config-startup-contract-remediation-plan.md`.

- Bluetooth AVRCP duration/position time-unit remediation — **COMPLETE
  2026-07-22** (Pi-live-validated). BlueZ `MediaPlayer1` millisecond values
  now remain unchanged through initial adoption, later property delivery,
  shared media state, and External API serialization. Duration-only Track
  updates, invalidation/stale-player clearing, and startup progress catch-up
  use the same bounded contract. Live validation reproduced the original
  1000x error, then proved exact Duration and Position propagation after
  deployment; Matthew confirmed correct time/progress UI and audible Moto A2DP
  playback through the EQ tap. Design and plan:
  `docs/archive/plans/2026-07-22-bt-avrcp-time-units-remediation-design.md`
  and
  `docs/archive/plans/2026-07-22-bt-avrcp-time-units-remediation-plan.md`.

- Android Auto initial night-state delivery — **COMPLETE 2026-07-22**
  (Pi-live-validated). The sensor handler now retains authoritative night state
  before channel open or subscription, sends that snapshot when the phone
  subscribes, and preserves the latest state across reconnects. Provider
  startup is explicitly seeded, including invalid-to-valid GPIO recovery.
  Forced day and night sessions decoded the first outgoing NIGHT_DATA value
  correctly and immediately selected the matching phone presentation. Design
  and plan:
  `docs/archive/plans/2026-07-22-aa-night-initial-state-remediation-design.md`
  and
  `docs/archive/plans/2026-07-22-aa-night-initial-state-remediation-plan.md`.

- Operations deployment reliability remediation — **COMPLETE 2026-07-22**
  (Pi-live-validated). Source and prebuilt installs now share the canonical
  BlueZ SDP compatibility setup; application and hostapd lifetimes are
  optional and independently recoverable; both install modes provide a
  readiness-aware, systemd-owned restart path; and the application enforces
  one IPC owner before acquiring hardware resources. Normal and forced Pi
  restarts each left one application process and responsive IPC while hostapd
  and BlueZ retained their PIDs; wireless Android Auto reconnected and
  projected successfully. Design and plan:
  `docs/archive/plans/2026-07-22-ops-deploy-p1-remediation-design.md` and
  `docs/archive/plans/2026-07-22-ops-deploy-p1-remediation-plan.md`.

- Memory and teardown safety stabilization — **COMPLETE 2026-07-21**
  (Pi-bench-validated). Generation- and lifetime-safe software video buffers
  prevent stale-size recycling and pool-destruction returns; WeatherData cache
  cleanup preserves the object being returned and both network completions use
  guarded targets; ScoNodeMonitor now stops through a direct AudioService
  pre-teardown edge, suppresses stale queued state, and rolls back failed
  registry-listener setup. Five clean Pi restart cycles and an active-mSBC-SCO
  restart plus subsequent call passed. Design and plan:
  `docs/archive/plans/2026-07-20-memory-teardown-safety-design.md` and
  `docs/archive/plans/2026-07-20-memory-teardown-safety-plan.md`.

- Bench-findings batch — **COMPLETE 2026-07-15** (bench-validated, all rows
  PASS; merged to main via PR #21). Items 1-3 promoted by Matthew from the
  BT-EQ bench findings; items 4-6 from the Codex post-merge review of PR #20.
  Design: `docs/archive/plans/2026-07-15-bench-findings-batch-design.md`
  (twice Codex-reviewed), plan:
  `docs/archive/plans/2026-07-15-bench-findings-batch-plan.md`.
  Delivered: HFP-SCO no longer hijacked into the EQ tap (probe-verified
  `a2dp-source` positive match), deploys no longer kick the phone off BT,
  input-device + master-volume persistence, plugin ABI runtime validation
  (binary vs manifest, unload-on-reject), doc-reference hygiene.

- Wireless BT AA flow — RFCOMM server + SDP registration + TCP listener. Phone discovers, pairs, and connects without manual scripts. COMPLETE.
- Touch device auto-discovery — INPUT_PROP_DIRECT scan, axis ranges read from device at open time. COMPLETE.
- Persistent network configuration — hostapd + systemd-networkd (built-in DHCP, no dnsmasq). COMPLETE.
- Settings UI buildout — scrollable ListView, section headers, plugin settings integration. COMPLETE.
- Background system service hardening — bt.close() hang fix, layered shutdown timeouts. COMPLETE.
- Proto repo migration — standalone [open-android-auto](https://github.com/mrmees/open-android-auto) community repo. COMPLETE.
- Video ACK delta fix (PR #5) — prevents RxVid overflow on phone. COMPLETE.
- General Bluetooth cleanup — BluetoothManager with D-Bus Adapter1/Agent1, PairedDevicesModel, auto-connect retry, PairingDialog overlay, HSP/HFP profile registration, polkit rules. COMPLETE.
- Obsolete aasdk reference removal — cleaned source and active docs of stale aasdk references. COMPLETE.
- Install script overhaul — interactive installer validated on fresh Trixie: hardware detection (touch, WiFi, audio), BlueZ --compat, rfkill unblock, country code auto-detection, labwc multi-touch config, systemd services, launch option. COMPLETE.
- Prebuilt distribution workflow — `install-prebuilt.sh` + `tools/package-prebuilt-release.sh` for shipping Pi-ready release tarballs without source builds, with release naming conventions and installer mode selection (`source` vs GitHub `prebuilt`). COMPLETE.
- Protocol capture dumps for protobuf validation — configurable session-attached AA capture logging (`jsonl`/`tsv`) with media-frame filtering for low-noise regression inputs. COMPLETE.
- Protocol capture controls in Settings + web panel — `connection.protocol_capture.*` now editable from in-car Connection settings and `web-config` settings page (no manual YAML edits required). COMPLETE.
- Platform/plugin architecture refactor (v0.6) — formalized runtime boundaries between core platform, shell/dashboard, and feature plugins. Typed dashboard contributions (`DashboardContributionKind`), narrow provider interfaces (`IProjectionStatusProvider`, `INavigationProvider`, `IMediaStatusProvider`, `ICallStateProvider`), core-owned services (`PhoneStateService`, `MediaStatusService`, `AndroidAutoRuntimeBridge`, `GestureOverlayController`), legacy `Configuration` class deleted, root-context globals replaced with provider-backed properties. Pi hardware verified. COMPLETE.
- Settings touch input fix (v0.6) — replaced QML TapHandler overlays (which stole all touch from child controls) with `SettingsInputBoundary`, a C++ QQuickItem using `childMouseEventFilter()` for subtree-wide long-press-back detection without interfering with Sliders, Toggles, or other controls. Removed per-row/per-control back-hold plumbing from SettingsRow, SettingsHoldArea, SettingsSlider. Pi hardware verified. COMPLETE.
- AA connection validation on fresh install — full AA session (BT discovery → WiFi → TCP → video) verified on clean Trixie install. COMPLETE.
- HFP call audio and companion cutover — TelephonyClient (BlueZ D-Bus) +
  ScoNodeMonitor (PipeWire) feed the PhoneStateService call-state machine, with
  SCO routed natively through WirePlumber. The live interop matrix completed
  2026-07-13 with mSBC selected as the shipped codec, and the legacy companion
  listener/port was retired by the B2 teardown on 2026-07-14. External API
  reporting-session liveness and QR pairing are implemented and test-covered.
  COMPLETE.
- External API v1 + v1.1 (sprint Phase B) — TCP 9810 / WebSocket 9811 protobuf server: pairing (PIN challenge), status publishers (media/nav/projection/phone/system), action dispatch, notifications, companion ingest (GPS/battery/connectivity/time); v1.1 additive fields + hardening docket + atomic YAML config save. Live on Pi. COMPLETE.
- Multi-dashboards + overlay framework (sprint Phase E) — named dashboards (YAML v4 + v3 migration, DashboardManager, switcher pills, widget size presets) and OverlayService (z-bands, action auto-registration, OverlayHost; PairingDialog migrated as proof). Pi-verified. COMPLETE.
- HTML/JS web-widget runtime (sprint Phase C2) — `prodigy://` scheme + WebWidgetHost (locked-down WebEngine) + `prodigy` JS shim riding the External API WebSocket; packaged widgets under `~/.openauto/webwidgets/`. Pi-verified end-to-end incl. full touchscreen checklist; shim-hardening batch + authoring guide (`docs/reference/web-widget-authoring.md`) landed 2026-07-07. COMPLETE.
- Theme/wallpaper upload endpoint — `POST /api/theme/install` (web-config
  multipart → temp-file handoff → IPC `install_theme` →
  `importCompanionTheme`), with real success/failure in the response. Deployed
  to Pi 2026-07-07; the legacy companion listener was retired 2026-07-14.
  COMPLETE.
- Cross-build fast mode — app-only default + `--full` flag; Pi deploy builds dropped ~20 min → ~4 min. COMPLETE.
- Widevine enablement (web-surface strategy Slice 1) — CDM auto-wiring in main.cpp
  (`--widevine-path` from `/opt/WidevineCdm`, env-override respected), best-effort
  `libwidevinecdm0` in install.sh, `tools/eme-probe` verification harness. Verified
  on Pi 2026-07-07: CDM loaded YES (journal: `Widevine CDM wired:
  "/opt/WidevineCdm/gmp-widevinecdm/latest/libwidevinecdm.so"`); EME key-system
  access WIDEVINE SUPPORTED (mp4) and (webm) on-device; codecs h264=probably,
  aac=probably, vp9=probably; qrc secureContext=true; widget-runtime regression
  clean (renderer alive, zero webwidget errors). Desktop Chromium deliberately
  left installed (user's browser). Real-service bench check PASSED same evening
  (Matthew, on-device): Shaka demo "Angel One" Widevine asset played with video +
  audio — full DRM chain (license request → CDM decrypt → decode → render) proven
  against a real license server. Bench notes: the head-unit app must be stopped
  for browser bench tools (it owns the touchscreen), and stopping it reproduced
  the proxy-blackhole (third incident; later reporting-session liveness work
  closed the remaining route-ownership gap). Spotify login
  round not run (optional; the WebAppHost arc will cover login UX). Spec:
  `docs/archive/plans/2026-07-07-web-surface-strategy-design.md`. COMPLETE.
- Audio equalizer parity audit (Phase F2) — verdict 2026-07-14: on-HU and YAML
  legs of the original outcome statement hold (on-HU UI exceeds "basic changes":
  per-stream 10-band sliders, bypass, preset picker, user-preset save/delete;
  QML→C++ save path verified empirically). The **web advanced-EQ leg is entirely
  absent** (no web-config routes, no IPC commands, no API surface) — precise gaps
  and related coverage/labeling findings were triaged; the remaining
  `docs/wishlist.md` § "Long-term: web-config advanced EQ editor" outcome
  requires a fresh promotion decision.
  COMPLETE (audit; no code changed).
- BT A2DP through the equalizer + EQ hygiene — **SHIPPED 2026-07-15
  (ALPHA-26-07-15-01)**. Promoted 2026-07-14 from the EQ parity audit
  findings (design: `docs/archive/plans/2026-07-14-bt-a2dp-eq-design.md`,
  plan: `docs/archive/plans/2026-07-14-bt-a2dp-eq-plan.md`).
  - Outcome: BT music obeys the Media EQ curve, HU master volume, and
    ducking (app-side loopback tap via WirePlumber retarget, direct-to-sink
    fallback). Riders: per-consumer EQ engine instances (fixes the shared
    Media-engine defect), unsaved gains/bypass persist across restart,
    Phone→System relabel with config migration.
  - Web EQ editor stays parked in the wishlist (on-HU UI already covers
    profile creation). COMPLETE.

- Local media player plugin — **SHIPPED AND BENCH-COMPLETE 2026-07-10
  (ALPHA-26-07-10-01)**. The static plugin provides folder browsing, scanned
  Artists/Albums/Tracks library views, incremental metadata/art caching,
  udisks2 USB automount/eject/recovery, persisted paused-state restore,
  three-source playing-wins arbitration, the shared now-playing widget, and
  External API `LOCAL_MEDIA` status. Design:
  `docs/archive/plans/2026-07-08-media-player-design.md`; stage plans:
  `docs/archive/plans/2026-07-08-media-player-stage1.md` and
  `docs/archive/plans/2026-07-09-media-player-stage2-plan.md`.

## Later

No work is currently promoted into this horizon. Unpromoted user capabilities
live in `docs/wishlist.md`; technical leads live in
`docs/engineering-backlog.md` and require fresh research before promotion.

## Deferred

- USB Android Auto support — explicitly out of scope.
- CarPlay or non-AA projection protocols.
- Hardware support beyond Pi 4.
- Cloud services, accounts, or telemetry.
