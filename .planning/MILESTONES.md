# Milestones

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

