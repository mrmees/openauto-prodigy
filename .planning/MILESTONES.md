# Milestones

## v0.6.5 Widget Refinement (Shipped: 2026-03-21)

**Delivered:** Visual refinement of all widgets via live preview tool iteration, date widget split from clock, custom battery gauge, Material Symbols font update, and hardware verification on Pi.

**Phases completed:** 3 phases (22-24), interactive review session

**Key accomplishments:**
- Date widget split from clock with column-driven breakpoints and US/intl date order config
- Clock simplified to digital + analog (minimal removed), bold font, no AM/PM, accent-colored analog hour hand, theme-reactive Canvas
- Custom Canvas battery gauge replacing icon-based display — vertical/horizontal orientation, accent fill, red at ≤20%
- All widgets reworked for automotive-sized scaling (icons, text, controls grow with layout)
- Material Symbols Outlined font updated to latest (4283 glyphs), all icon codepoints audited and corrected
- AAStatusWidget removed (redundant with AA focus toggle)
- Now Playing reworked: anchored button regions, scrolling titles, progressive breakpoints, automotive-sized controls
- Companion Status: captive_portal/public_off icons, satellite_alt GPS, wifi_tethering on/off, bounding box detail view
- Widget preview dev tool built for iterative QML development with mock services and display presets

**Stats:** 18 files changed (+1,066/-556) | Timeline: ~14 hours (2026-03-20 → 2026-03-21)

---

## v0.6.4 Widget Work (Shipped: 2026-03-17)

**Delivered:** Extended the widget contract with per-instance configuration and shipped 6 new widgets — theme cycle, battery, companion status, AA focus toggle, clock styles, and weather with live Open-Meteo integration.

**Phases completed:** 3 phases (19-21), 6 plans

**Key accomplishments:**
- Per-instance widget configuration: schema validation, gear icon config sheet in edit mode, YAML persistence surviving restarts and grid remap
- Theme cycle widget (1x1 tap-to-advance) and phone battery widget (companion data)
- Companion status widget (1x1 indicator → expanded GPS/battery/proxy detail at larger sizes)
- AA focus toggle widget (VideoFocusRequest NATIVE/PROJECTED with projection state reflection)
- Clock widget with 3 visual styles (digital/analog/minimal) selectable per instance via config sheet
- WeatherService: Open-Meteo HTTP fetch, per-location cache, subscriber-aware refresh timers, GPS via CompanionService
- WeatherWidget: responsive 3-breakpoint layout (1x1 temp → 2x1 icon+temp → 3x3+ extended with feels-like, humidity, wind, location)

**Stats:** 12 feat/fix commits, 50 files changed (+7,322/-136), 88 unit tests (14 new weather tests) | Timeline: ~5 hours (2026-03-16 → 2026-03-17)

**Known gaps:**
- WX-03 (weather alerts): Deferred — Open-Meteo alert coverage insufficient
- WX-05 (API key config): N/A — Open-Meteo requires no API key

---

## v0.6.3 Proxy Routing Fix (Shipped: 2026-03-16)

**Delivered:** Fixed system service privilege model, IPC security, transparent proxy routing, and status reporting so `set_proxy_route` works end-to-end on real hardware with the companion SOCKS bridge.

**Phases completed:** 4 phases, 9 plans, 16 tasks

**Key accomplishments:**
- Root daemon with restricted IPC socket (0660, openauto group, SO_PEERCRED peer credential validation)
- Idempotent iptables routing with flush/recreate model, owner-based + destination-based self-exemption via dedicated redsocks user
- Truthful status reporting — ACTIVE requires verified local pipeline health (TCP connect + iptables check), FAILED/DEGRADED with prioritized error codes
- Startup self-heal cleans all stale proxy state before accepting IPC connections
- End-to-end hardware validation: transparent proxy proven on Pi with real companion SOCKS bridge (origin IP changed from Pi to phone cellular)
- Both installers upgraded with consistent migration path (group creation, stale rule cleanup, service template rendering)
- IPv6-mapped address fix for Qt dual-stack peerAddress() breaking redsocks config

**Stats:** 66 commits, 50 files changed (+7,318/-310) | Timeline: 1 day (2026-03-15 → 2026-03-16)

**Bugs found during hardware validation:**
- eth0 in skip_interfaces default bypassed all iptables REDIRECT (default route uses eth0)
- redsocks config written 0600 but redsocks user needs read access (fixed to 0640 + chown)
- Qt peerAddress() returns `::ffff:10.0.0.31` — colons broke redsocks config parser

---

## v0.6.2 Theme Expression & Wallpaper Scaling (Shipped: 2026-03-16)

**Phases completed:** 5 phases, 6 plans, 0 tasks

**Key accomplishments:**
- (none recorded)

---

## v0.6 Widget Framework & Layout Refinement (Shipped: 2026-03-15)

**Phases completed:** 5 phases, 10 plans, 0 tasks

**Key accomplishments:**
- (none recorded)

---

## v0.6 Architecture Formalization (Shipped: 2026-03-14)

**Delivered:** Formalized platform/plugin/widget architecture with typed contracts, provider interfaces, core-owned services, and C++ settings touch input boundary. Removed legacy Configuration class, replaced root-context globals with provider-backed properties, extracted platform policy from plugins.

**Phases completed:** Tracked outside GSD (docs/session-handoffs.md, docs/plans/)

**Key accomplishments:**
- Typed dashboard contributions (DashboardContributionKind: Widget vs LiveSurfaceWidget)
- Narrow provider interfaces (IProjectionStatusProvider, INavigationProvider, IMediaStatusProvider, ICallStateProvider)
- Core-owned services: PhoneStateService, MediaStatusService, AndroidAutoRuntimeBridge, GestureOverlayController
- Legacy Configuration class deleted, INI loader removed, YAML-to-INI sync shim removed
- Root-context globals replaced with provider-backed QML properties
- SettingsInputBoundary (C++ QQuickItem childMouseEventFilter) for subtree-wide long-press-back
- Removed per-row/per-control back-hold overlay plumbing from settings
- 82 unit tests, Pi hardware verified

**Stats:** 16 commits on dev/0.6, squash-merged to main | ~35K+ LOC | Timeline: 2 days (2026-03-13 → 2026-03-14)

---

## v0.5.3 Widget Grid & Content Widgets (Shipped: 2026-03-13)

**Delivered:** Replaced fixed 3-pane home screen with Android-style freeform widget grid — drag-to-reposition, drag-to-resize, multi-page swipe, and two new content widgets (AA navigation turn-by-turn, unified now playing).

**Phases completed:** 04-08 (5 phases, 13 plans)

**Key accomplishments:**
- Cell-based WidgetGridModel with occupancy tracking, collision detection, and YAML schema v3 persistence (auto-migrates from pane-based config)
- Grid renderer in HomeMenu.qml with Repeater-based positioning — widgets snap to integer grid cells derived from display size
- NavigationDataBridge + ManeuverIconProvider wiring AA turn-by-turn data to QML widget with phone PNG icons
- MediaDataBridge with AA > BT source priority, unified NowPlayingWidget replacing BT-only version
- Full edit mode: long-press entry, drag-to-reposition with snap-back, drag-to-resize with ghost rectangle, FAB add/X remove, 10s inactivity timeout, AA auto-exit
- SwipeView multi-page home screen with PageIndicator, lazy Loader instantiation, add/delete page FABs, empty page auto-cleanup

**Stats:** 55 commits, 69 files changed (+7,813/-704), ~28.9K LOC | Timeline: 1 day (2026-03-12 → 2026-03-13)

**Git range:** `feat(04-01)` → `docs(08-02)`

**What's next:** TBD — next milestone planning

---

## v0.5.2 Widget System & UI Polish (Shipped: 2026-03-11)

**Phases completed:** 3 phases, 8 plans

**Key accomplishments:**
- Widget-based home screen with 3-pane layout (60/40 landscape, stacked portrait), launcher dock, and glass card WidgetHost with per-pane adjustable opacity
- Three built-in widgets (Clock, AA Status, Now Playing) with widget picker and long-press context menu
- Settings reorganized into 9 focused categories (AA, Display, Audio, Bluetooth, Theme, Companion, System, Information, Debug)
- SettingsRow component with alternating-row styling applied across all 9 settings pages for consistent touch-friendly layout
- Font sizes increased ~1.4x for automotive readability (3mm+ cap height at arm's length on 1024x600)
- Long-press-to-go-back gesture with expanding ripple indicator in settings navigation

**Stats:** 155 commits, 98 files changed (+11,041/-1,833), ~26.6K LOC | Timeline: 2 days (2026-03-10 → 2026-03-11)

---

## v0.5.0 Protocol Compliance (Shipped: 2026-03-08)

**Phases completed:** 4 phases, 8 plans, 0 tasks

**Key accomplishments:**
- Updated open-android-auto proto submodule to v1.0 (223 proto files changed) with unhandled message debug logging for full protocol observability
- NavigationTurnEvent parsing with road name, maneuver type, distance, icon, multi-step lane guidance, and NavigationFocusIndication state tracking
- First HU-to-phone outbound commands: VoiceSessionRequest (START/STOP) and media playback toggle with cached state
- BT channel auth exchange (BluetoothAuthenticationData/Result) and InputBindingNotification haptic feedback parsing
- Cleaned up 4 retracted requirements after v1.2 proto verification (NAV-02, AUD-01, AUD-02, MED-01) — all dead code removed
- Renamed protocol library from open-androidauto to prodigy-oaa-protocol with full build system and documentation update

**Stats:** 31 commits, 125 files changed (+2,576/-274), ~34.7K LOC | Timeline: 3 days (2026-03-04 → 2026-03-07)

---

## v0.4.5 Navbar Rework (Shipped: 2026-03-05)

**Phases completed:** 4 phases, 11 plans | 51 commits, 75 files changed (+4,848/-3,197), ~23.1K LOC

**Key accomplishments:**
- Zone-based evdev touch routing: TouchRouter + EvdevCoordBridge replace hardcoded sidebar dispatch with priority-based zone registry and finger stickiness
- 3-control navbar with tap/short-hold/long-hold gestures, edge positioning (top/bottom/left/right), LHD/RHD driver-side awareness
- Navbar-aware AA viewport margins: phone renders in sub-region with space reserved for navbar, touch correctly split between navbar and AA
- GestureOverlay evdev zone fix: overlay controls respond to touch under EVIOCGRAB with per-control hit zones at priority 210
- Content-space touch coordinate mapping: single computeContentDimensions() source of truth eliminates duplicated margin/touch logic
- Dead UI cleanup: TopBar, NavStrip, Clock, BottomBar, sidebar overlay all removed — one navigation system remains

### Known Gaps
- EDGE-02 (partial): Navbar renders per-edge at startup but content area margin reflow on live edge change requires restart

---

## v0.4.4 Scalable UI (Shipped: 2026-03-03)

**Phases completed:** 5 phases, 10 plans | 183 commits, 163 files, ~21.9K LOC

**Key accomplishments:**
- YAML config overrides for scale, fontScale, and individual tokens — user always wins over auto-derived values
- Unclamped dual-axis UiMetrics: min(scaleH, scaleV) for layout, geometric mean for fonts, pixel floors for legibility
- Full QML tokenization — zero hardcoded pixel values across all 20+ QML files (AUDIT-10 verified by grep)
- Container-derived grid layouts: LauncherMenu GridView with dynamic columns, settings grid, EQ min-height guard
- Runtime auto-detection: DisplayInfo bridge from window dimensions, --geometry CLI flag for validation
- Layout-derived sidebar touch zones matching QML at any resolution, dead display config removed

---

## v0.4.3 Interface Polish & Settings Reorganization (Shipped: 2026-03-03)

**Phases completed:** 4 phases, 8 plans, 18 tasks | 74 commits, 84 files changed (+8,946/-1,740)

**Key accomplishments:**
- Automotive-minimal control restyling: press feedback (scale+opacity), Material Symbols icons, divider colors across all interactive controls
- Settings restructured into 6-category tile grid (AA, Display, Audio, Bluetooth, Companion, System) with slide+fade StackView navigation
- EQ dual-access: NavStrip shortcut + Audio settings path, deep navigation with shared EqualizerService state
- Shell polish: NavStrip/TopBar/launcher restyled, modal dismiss-on-outside-tap, BT forget pill button
- Day/night theme color animation: smooth 300ms ColorAnimation on all 4 major surfaces
- Tech debt closure: highlightFontColor Q_PROPERTY, divider color hacks fixed, dead tileSubtitle removed

---

## v0.4 Logging & Theming (Shipped: 2026-03-01)

**Phases completed:** 2 phases, 9 plans, 0 tasks

**Key accomplishments:**
- Categorized logging infrastructure: 6 subsystem categories, quiet by default, verbose on demand via CLI/config/web panel
- All raw qDebug/qInfo/qWarning calls migrated to categorized macros — zero untagged log calls remain
- Multi-theme support: ThemeService discovers/switches color palettes from bundled + user paths
- DisplayService with 3-tier brightness: sysfs backlight > ddcutil > software dimming overlay
- Wallpaper system: per-theme defaults, user override, settings picker, config persistence
- Gesture overlay brightness control + wallpaper display in shell

---

## v0.4.1 Audio Equalizer (Shipped: 2026-03-02)

**Phases completed:** 3 phases, 6 plans, 31min total execution

**Key accomplishments:**
- RT-safe 10-band biquad DSP engine with lock-free coefficient interpolation and bypass crossfade
- EqualizerService with per-stream profiles (media/nav/phone), 8 bundled presets, user preset save/load
- YAML config persistence with 2-second debounce and guaranteed shutdown save
- AudioService pipeline integration — EQ processing wired into all 3 PipeWire streams
- Touch-friendly QML UI: 10 vertical band sliders, preset picker, swipe-to-delete, bypass toggle
- Promoted EQ to standalone nav strip plugin (user feedback during verification)

---

## v0.4.2 Service Hardening (Shipped: 2026-03-02)

**Phases completed:** 3 phases, 18 commits, 13 files changed (+1136/-144)

**Key accomplishments:**
- Pre-flight script: rfkill unblock, SDP socket self-heal, systemd-networkd readiness check
- systemd service hardening: correct After/Wants ordering, Restart=on-failure with rate limiting, BindsTo=hostapd
- sd_notify integration: READY=1 on startup, STOPPING=1 on shutdown, WATCHDOG=1 heartbeat
- SDP retry with exponential backoff and clear logging
- WiFi AP independent of app lifecycle (PartOf= instead of ExecStopPost)
- Installer diagnostics: PipeWire, BlueZ, hostapd, labwc, group membership verification

---

