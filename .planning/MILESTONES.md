# Milestones

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

