# Current Roadmap

Governance: capture new ideas in `docs/wishlist.md`; only promoted items should appear in this roadmap.

> **Parity program status (updated 2026-07-07):** the 2026-07-05 design sprint (`docs/superpowers/specs/2026-07-05-fable-work-program-design.md`, phases A–F) has been designed **and executed**: External API v1, HTML/JS web widgets, HFP call audio, multi-dashboards + overlay framework, and the theme-upload endpoint are all shipped (see Done). What remains from the program: the Phase F light-plan items (`docs/superpowers/plans/2026-07-05-phase-f-light-plans.md` — media player, EQ parity audit, 0x8012 experiment, key-event nav) and the HFP live checks (L3/L4/L5, need Matthew + phone).

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
- HTML/JS web-widget runtime (sprint Phase C2) — `prodigy://` scheme + WebWidgetHost (locked-down WebEngine) + `prodigy` JS shim riding the External API WebSocket; packaged widgets under `~/.openauto/webwidgets/`. Pi-verified end-to-end incl. full touchscreen checklist; shim-hardening batch + authoring guide (`docs/web-widget-authoring.md`) landed 2026-07-07. COMPLETE.
- Theme/wallpaper upload endpoint — `POST /api/theme/install` (web-config multipart → temp-file handoff → IPC `install_theme` → `importCompanionTheme`), real success/failure in the response (fixes the legacy 9876 ack-lie). Deployed to Pi 2026-07-07; HTTP contract handed to the companion maintainer. COMPLETE (legacy 9876 retirement now gates only on the companion shipping its client).
- Cross-build fast mode — app-only default + `--full` flag; Pi deploy builds dropped ~20 min → ~4 min. COMPLETE.

## Now

- Local media player plugin. *(promoted from Later 2026-07-07 — next Fable arc)*
  - Rationale: local file playback with metadata/cover art is table stakes for a head unit; prodigy currently only plays BT audio. Biggest remaining daily-driver parity gap. Light plan exists (Phase F1); needs the full design → plan → execute arc.
  - Outcome: media player plugin (Qt Multimedia) integrated with MediaStatusService and the now-playing UI.

- Audio equalizer — parity audit only. *(scoped down 2026-07-05: the substrate scout found the EQ already functional — EqualizerService runs 3 engines with presets + persistence, applied on the PipeWire RT thread)*
  - Remaining: audit web-config advanced-profile parity against the original outcome statement (on-HU preset swapping vs web-based advanced EQ setup / profile creation). Audit protocol in Phase F2 light plan.

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
  - Remaining (this repo): **retire `CompanionListenerService` + port 9876** once the companion ships its API v1 + HTTP theme client. At retirement: dedup the camelCase→hyphen conversion + fix-or-delete the legacy RNG hygiene items (see wishlist).
  - Companion-parity follow-up idea (wishlist, not promoted): phone notifications displayed on the head unit — blockers dissolved (NotificationService + overlay framework both exist), but it waits on the companion's API v1 migration.
- Key-event navigation map (steering-wheel / hardware buttons) — sketch in Phase F4 light plan.
- CI automation for builds and tests.
- Multi-display / resolution support beyond 1024x600.
- Community contribution workflow (issue templates, PR guidelines).

## Deferred

- USB Android Auto support — explicitly out of scope.
- CarPlay or non-AA projection protocols.
- Hardware support beyond Pi 4.
- Cloud services, accounts, or telemetry.
