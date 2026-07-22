# Current Roadmap

Governance: capture new ideas in `docs/wishlist.md`; only promoted items should appear in this roadmap.

> **Parity program status (updated 2026-07-21):** the 2026-07-05 design sprint
> (`docs/archive/plans/2026-07-05-fable-work-program-design.md`, phases A–F)
> shipped External API v1, HTML/JS web widgets, HFP call audio,
> multi-dashboards + overlays, theme upload, and the two-stage local media
> player. The EQ parity audit is complete. No promoted Phase F work remains:
> the proposed `0x8012` margin-push experiment was closed after the gold-traced
> protocol definition established that the message is a theming-token response,
> and the key-event map remains an unpromoted wishlist idea. The completed Phase
> F light-plan record is archived.

## Done (recent)

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
  the proxy-blackhole (third incident; wishlist entry updated). Spotify login
  round not run (optional; the WebAppHost arc will cover login UX). Spec:
  `docs/archive/plans/2026-07-07-web-surface-strategy-design.md`. COMPLETE.
- Audio equalizer parity audit (Phase F2) — verdict 2026-07-14: on-HU and YAML
  legs of the original outcome statement hold (on-HU UI exceeds "basic changes":
  per-stream 10-band sliders, bypass, preset picker, user-preset save/delete;
  QML→C++ save path verified empirically). The **web advanced-EQ leg is entirely
  absent** (no web-config routes, no IPC commands, no API surface) — precise gaps
  + three coverage/labeling quirks filed to `docs/wishlist.md` § "From EQ parity
  audit (2026-07-14)". Building the web editor is a wishlist-promote decision.
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

## Now

- Android Auto initial night-state delivery — **ACTIVE 2026-07-22**. Retain
  provider state before the sensor channel/subscription becomes active, seed it
  explicitly before session start, and send that authoritative snapshot when
  the phone subscribes to NIGHT_DATA. Scope is limited to the in-tree sensor
  handler, orchestrator seeding, decoded regression coverage, and current
  behavior documentation. Design and plan:
  `docs/plans/2026-07-22-aa-night-initial-state-remediation-design.md` and
  `docs/plans/2026-07-22-aa-night-initial-state-remediation-plan.md`.

## Later

- Reduce unnecessary logging / enable debug logging options. MOSTLY RESOLVED 2026-07-02:
  - BtManager paired-device spam now logs only when the count changes (info level, `BluetoothManager.cpp`). NavStrip QML color warnings died with NavStrip itself (deleted in the v0.4.5 Navbar rework; all 43 ThemeService QML references audited as resolving). Verbose infrastructure already exists (`lcBT` defaults to info threshold, `--verbose` flag).
  - Remaining: a general startup-log audit on the Pi to catch any other noise sources.
- Plugin system expansion (OBD-II, backup camera, GPIO control).
- Theme engine and user-facing theme selection.
  - Delta narrowed 2026-07-07: companion theme/wallpaper upload (`/api/theme/install`) and the web-config themes page both exist; remaining is an audit of in-car theme selection UX.
- Companion API integration — **COMPLETE 2026-07-14**. The Android companion's
  API v1 reporting, pairing, connectivity, and theme-transfer paths passed the
  live cutover before the head unit retired `CompanionListenerService` and its
  legacy listener. Reporting-session expiry and reconnect self-heal were also
  bench-validated. The separate phone-notification idea remains unpromoted; its
  remaining scope is an additive notification contract and display policy, not
  an API migration prerequisite.
- Streaming web apps (WebAppHost) — fullscreen Spotify/YouTube/parked-video surface
  riding the slice-1 Widevine wiring. Scoped (Decision 3 of the web-surface spec);
  wishlist entry has the open questions. Not yet promoted.
- CI automation for builds and tests.
- Multi-display / resolution support beyond 1024x600.
- Community contribution workflow (issue templates, PR guidelines).

## Deferred

- USB Android Auto support — explicitly out of scope.
- CarPlay or non-AA projection protocols.
- Hardware support beyond Pi 4.
- Cloud services, accounts, or telemetry.
