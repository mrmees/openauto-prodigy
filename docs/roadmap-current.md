# Current Roadmap

Governance: capture new ideas in `docs/wishlist.md`; only promoted items should appear in this roadmap.

> **Design sprint 2026-07-05:** deep design docs + implementation plans for the parity items below are being produced ahead of the roadmap's execution order (see `docs/superpowers/specs/2026-07-05-fable-work-program-design.md`). This changes design attention only — the "Now" items keep their delivery priority; HFP execution is in fact unblocked by the sprint's Phase D. Do not read the sprint doc and this roadmap as conflicting.

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

## Now

- General HFP call audio handling.
  - Rationale: typical head units maintain the HFP AG profile across both Android Auto and the base hardware — e.g., if AA crashes, call audio is not lost.
  - Outcome: phone calls work with audio through the head unit speakers/mic across a closed AA stream.

- Audio equalizer.
  - Rationale: users expect the ability to swap between equalizer presets and define custom EQ profiles for their vehicle and speaker setup.
  - Outcome: audio equalizer plugin with YAML settings file, on-head-unit component for basic changes / profile swapping, web settings backend for advanced EQ setup and profile creation.

## Later

- Dynamic AA video reconfiguration for sidebar changes.
  - Rationale: toggling the sidebar on/off or changing its position (left/right/top/bottom) requires recalculating the AA video content area (margin_width/margin_height in VideoConfig). Currently this isn't handled dynamically — the video config is locked at session start.
  - Discovery: `UiConfigMessages.proto` (added to open-android-auto 2026-02-28) defines `UpdateHuUiConfigRequest` (0x8012) — an HU-initiated push of margins, content insets, and day/night theme over the video AV channel. This may be the proper mechanism for runtime sidebar/margin changes and night mode, replacing the sensor-based workaround. Needs wire verification.
  - Outcome: sidebar show/hide and position changes trigger AA video renegotiation or margin recalculation within the active session, so the phone renders correctly for the new layout without reconnecting.
- Reduce unnecessary logging / enable debug logging options. MOSTLY RESOLVED 2026-07-02:
  - BtManager paired-device spam now logs only when the count changes (info level, `BluetoothManager.cpp`). NavStrip QML color warnings died with NavStrip itself (deleted in the v0.4.5 Navbar rework; all 43 ThemeService QML references audited as resolving). Verbose infrastructure already exists (`lcBT` defaults to info threshold, `--verbose` flag).
  - Remaining: a general startup-log audit on the Pi to catch any other noise sources.
- Plugin system expansion (OBD-II, backup camera, GPIO control).
- Theme engine and user-facing theme selection.
- HTML/JS extensibility — spike, then runtime. (Parity with paid alternatives.)
  - Rationale: HTML/JS widgets/apps as a primary path for developing new features going forward; paid alternatives validate the model (embedded web views + JS bridge). Spike first: Qt WebEngine memory/perf on Pi 4 go/no-go. Packaging gate already verified: `qml6-module-qtwebengine` 6.8.2+dfsg-4 exists in Debian Trixie arm64 (checked 2026-07-02), matching our Qt 6.8.2 — the spike is purely a runtime memory/perf question.
  - Outcome: WebEngine-based widgets/apps/overlays with a `prodigy` JS object (theme tokens, input events, API access), gated on spike results.
- External API (TCP + WebSocket, protobuf).
  - Rationale: an external integration surface (status streams, action dispatch, notifications) is the paid alternatives' biggest moat and the backbone the JS bridge talks to. Prodigy-private original schema — no wire compatibility with any paid alternative (their code is unlicensed).
  - Outcome: API v1 exposing media/nav/projection/phone status streams, action dispatch/register, and notification/toast display.
- Companion app migration to API v1.
  - The companion app **already exists and is well established** — Android (Kotlin/Gradle) app at `personal/openautopro/openauto-companion` (sibling repo, own AGENTS.md/docs). The 2026-07-05 arch decision "REPLACE" means *replace the wire protocol*, not the app: it migrates from the legacy port-9876 JSON/HMAC protocol (`CompanionListenerService`) to API v1 (`proto/api/`, WebSocket + PIN pairing, inbound reports per `companion.proto`). After migration, `CompanionListenerService` and port 9876 retire on the head-unit side.
  - Gap review 2026-07-05 (Codex inventory of legacy 9876 features, decisions by Matthew): **theme/wallpaper transfer moves to a new web-config HTTP upload/install endpoint** (head-unit work item — blobs don't belong under the API's 256 KiB frame cap); **API v1.1 additive batch approved** (SystemStatus display dims, TimeReport.timezone_id, ServerHello.server_id) as a post-Task-16 follow-up — SHIPPED 2026-07-06 on develop (with the API hardening docket + atomic config save; see session handoff); SOCKS5 auth is password-only by convention (app-side, no proto change). Companion runs dual-stack (legacy 9876 for theme transfer only) until the web-config endpoint ships.
  - Outcome: companion app speaks API v1 (GPS/battery/connectivity/time reports, pairing); theme transfer via web-config HTTP; legacy companion listener deleted.
- Multi-dashboard support + general overlay framework.
  - Rationale: the v0.6 widget-grid home screen already gives users a composable widget surface (WidgetGridModel/WidgetPickerModel). Remaining delta vs paid alternatives: multiple named dashboards, widget size options (2 widths × 3 heights), web-view widgets, and configurable overlays (position/size/visibility via actions/API, split-screen).
  - Outcome: multiple config-driven dashboards with sized widgets (native + web), generalized overlay system alongside the current purpose-built overlays.
- Local media player plugin.
  - Rationale: local file playback with metadata/cover art is table stakes for a head unit; prodigy currently only plays BT audio.
  - Outcome: media player plugin (Qt Multimedia) integrated with MediaStatusService and the now-playing UI.
- CI automation for builds and tests.
- Multi-display / resolution support beyond 1024x600.
- Community contribution workflow (issue templates, PR guidelines).

## Deferred

- USB Android Auto support — explicitly out of scope.
- CarPlay or non-AA projection protocols.
- Hardware support beyond Pi 4.
- Cloud services, accounts, or telemetry.
