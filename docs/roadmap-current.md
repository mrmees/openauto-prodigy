# Current Roadmap

Governance: capture new ideas in `docs/wishlist.md`; only promoted items should appear in this roadmap.

> **Parity program status (updated 2026-07-14):** the 2026-07-05 design sprint (`docs/archive/plans/2026-07-05-fable-work-program-design.md`, phases A–F) has been designed **and executed**: External API v1, HTML/JS web widgets, HFP call audio (bench-complete 2026-07-13), multi-dashboards + overlay framework, and the theme-upload endpoint are all shipped; the media player shipped through stage 2; the EQ parity audit closed 2026-07-14 (see Done). What remains from the program: the two experimental Phase F light-plan items (`docs/plans/2026-07-05-phase-f-light-plans.md` — 0x8012 experiment, key-event nav notes).

## Done (recent)

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
- HFP call audio (sprint Phase D2) — TelephonyClient (BlueZ D-Bus) + ScoNodeMonitor (PipeWire) feeding a normative call state machine in PhoneStateService; SCO routes via WirePlumber natively. Executed on live hardware 2026-07-05. COMPLETE (interop live checks L3/L4/L5 — DTMF via IVR, RejectSCO comparison, Samsung/Moto — pending ~15 min bench time with Matthew + phone; checklist self-contained in the D2 design doc §11).
- External API v1 + v1.1 (sprint Phase B) — TCP 9810 / WebSocket 9811 protobuf server: pairing (PIN challenge), status publishers (media/nav/projection/phone/system), action dispatch, notifications, companion ingest (GPS/battery/connectivity/time); v1.1 additive fields + hardening docket + atomic YAML config save. Live on Pi. COMPLETE.
- Multi-dashboards + overlay framework (sprint Phase E) — named dashboards (YAML v4 + v3 migration, DashboardManager, switcher pills, widget size presets) and OverlayService (z-bands, action auto-registration, OverlayHost; PairingDialog migrated as proof). Pi-verified. COMPLETE.
- HTML/JS web-widget runtime (sprint Phase C2) — `prodigy://` scheme + WebWidgetHost (locked-down WebEngine) + `prodigy` JS shim riding the External API WebSocket; packaged widgets under `~/.openauto/webwidgets/`. Pi-verified end-to-end incl. full touchscreen checklist; shim-hardening batch + authoring guide (`docs/reference/web-widget-authoring.md`) landed 2026-07-07. COMPLETE.
- Theme/wallpaper upload endpoint — `POST /api/theme/install` (web-config multipart → temp-file handoff → IPC `install_theme` → `importCompanionTheme`), real success/failure in the response (fixes the legacy 9876 ack-lie). Deployed to Pi 2026-07-07; HTTP contract handed to the companion maintainer. COMPLETE (legacy 9876 retirement now gates only on the companion shipping its client).
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

## Now

- BT A2DP through the equalizer + EQ hygiene — **PROMOTED 2026-07-14** from the EQ
  parity audit findings (design: `docs/plans/2026-07-14-bt-a2dp-eq-design.md`,
  approach approved by Matthew same day).
  - Outcome: BT music obeys the Media EQ curve, HU master volume, and
    ducking (app-side loopback tap via WirePlumber retarget, direct-to-sink
    fallback). Riders: per-consumer EQ engine instances (fixes the shared
    Media-engine defect), unsaved gains/bypass persist across restart,
    Phone→System relabel with config migration.
  - Web EQ editor stays parked in the wishlist (on-HU UI already covers
    profile creation). Milestone tag + dev→main PR follow this work
    (Matthew, 2026-07-14).

- HFP mic fix + live checks + 9876 retirement stage-1 — **bench COMPLETE (2026-07-13)**; design + plan in `docs/plans/2026-07-11-hfp-mic-9876-retirement-{design,plan}.md`, all RESULT rows in `docs/plans/2026-07-11-hfp-bench-runbook.md`.
  - Bench verdicts: **mSBC is the shipped codec** (patched `libspa-0.2-bluetooth 1.4.2-1+rpt3+prodigy1` installed + held on the Pi; LC3-SWB encode bug confirmed with a clean A/B; CVSD drop-in = repo fallback only); L3 DTMF ✓, L4 RejectSCO default stays `false`, L5 Samsung mostly ✓ (answer/reject-during-AA wishlisted) / Moto no-service partial, L6 volume/echo ✓; §7 cutover fully validated with 9876 dead — including the time row, whose bench FAIL turned out to be a false-positive diagnosis (wiring existed; investigation found + fixed real bugs instead: timedatectl local-time parse, missing set-timezone polkit rule, untested duplicated logic → tested `ClockSyncService`; re-validated live with induced drift 2026-07-13). **B2 teardown planning is unblocked.**
  - Also shipped 2026-07-13: installers wire the patched-deb install + apt hold; scannable QR for API pairing (`prodigy://pair?...`, head-unit side) — companion-side scanner runs from `personal/openautopro/companion-qr-pairing-prompt.md`.
  - Remaining: dev→main PR (gates: §7 time row ✓ done; QR pairing end-to-end scan at the bench with the updated companion app — Matthew); upstream PipeWire issue (draft at `personal/openautopro/pipewire-lc3-swb-issue-draft.md`, Matthew approves before posting); then B2 teardown planning per design §B2.

- Local media player plugin — **stage 2 (library + USB automount) code-complete + gated (2026-07-10)**; Pi deploy + stage-2 bench rows pending Matthew. Stage 1 shipped + benched 2026-07-09.
  - Stage 1 shipped (develop @ `2aeb411`, 18 commits): MediaPlayerPlugin (folder browse + now-playing bar), PlaybackEngine (QMediaPlayer PCM tap → AudioService, spike-gated GO), PlayQueue (shuffle/repeat, TDD), FolderModel, MediaArtProvider, 3-source playing-wins arbitration, NowPlayingWidget (art/progress/source badge/transport), API v1 LOCAL_MEDIA source + position fields. Suite 114/114 green; cross-build + Pi deploy verified healthy (service active, plugin registered, NRestarts=0).
  - Remaining stage 1: 12-row bench checklist with Matthew (touch/audible rows — see session-handoffs 2026-07-08 deploy record), then review + push.
  - Next: **stage 2 (library + USB automount) planning.**

## Later

- Dynamic AA video reconfiguration for sidebar changes.
  - Rationale: toggling the sidebar on/off or changing its position (left/right/top/bottom) requires recalculating the AA video content area (margin_width/margin_height in VideoConfig). Currently this isn't handled dynamically — the video config is locked at session start.
  - Discovery: `UiConfigMessages.proto` (added to open-android-auto 2026-02-28) defines `UpdateHuUiConfigRequest` (0x8012) — an HU-initiated push of margins, content insets, and day/night theme over the video AV channel. This may be the proper mechanism for runtime sidebar/margin changes and night mode, replacing the sensor-based workaround. Needs wire verification — the experiment protocol is written (Phase F3 light plan).
  - Outcome: sidebar show/hide and position changes trigger AA video renegotiation or margin recalculation within the active session, so the phone renders correctly for the new layout without reconnecting.
- Reduce unnecessary logging / enable debug logging options. MOSTLY RESOLVED 2026-07-02:
  - BtManager paired-device spam now logs only when the count changes (info level, `BluetoothManager.cpp`). NavStrip QML color warnings died with NavStrip itself (deleted in the v0.4.5 Navbar rework; all 43 ThemeService QML references audited as resolving). Verbose infrastructure already exists (`lcBT` defaults to info threshold, `--verbose` flag).
  - Remaining: a general startup-log audit on the Pi to catch any other noise sources.
- Plugin system expansion (OBD-II, backup camera, GPIO control).
- Theme engine and user-facing theme selection.
  - Delta narrowed 2026-07-07: companion theme/wallpaper upload (`/api/theme/install`) and the web-config themes page both exist; remaining is an audit of in-car theme selection UX.
- Companion app work — head-unit side DONE; ball is in the companion's court.
  - The companion (Android app, sibling repo `personal/openautopro/openauto-companion`) migrates from the legacy port-9876 JSON/HMAC protocol to API v1 (WebSocket + PIN pairing, `companion.proto` reports) + the new theme-upload HTTP endpoint. All head-unit prerequisites have shipped: API v1 + v1.1 additive batch (2026-07-06) and the theme-upload endpoint (2026-07-07, contract handed off).
  - **DONE (2026-07-14):** `CompanionListenerService` + port 9876 retired (B2 teardown, `docs/archive/plans/2026-07-14-b2-teardown-design.md`). Reporting-session liveness expiry shipped with it — a silently-vanished phone drops `connected` within ~35 s.
  - Companion-parity follow-up idea (wishlist, not promoted): phone notifications displayed on the head unit — blockers dissolved (NotificationService + overlay framework both exist), but it waits on the companion's API v1 migration.
- Streaming web apps (WebAppHost) — fullscreen Spotify/YouTube/parked-video surface
  riding the slice-1 Widevine wiring. Scoped (Decision 3 of the web-surface spec);
  wishlist entry has the open questions. Queued behind the media player arc.
- Key-event navigation map (steering-wheel / hardware buttons) — sketch in Phase F4 light plan.
- CI automation for builds and tests.
- Multi-display / resolution support beyond 1024x600.
- Community contribution workflow (issue templates, PR guidelines).

## Deferred

- USB Android Auto support — explicitly out of scope.
- CarPlay or non-AA projection protocols.
- Hardware support beyond Pi 4.
- Cloud services, accounts, or telemetry.
