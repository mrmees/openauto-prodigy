# Phase 2: Settings Restructure - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Reorganize settings from 6 categories to 9 (AA, Display, Audio, Bluetooth, Theme, Companion, System, Information, Debug). Move, remove, or demote controls per SET-01 through SET-10. Eliminate duplicates between AA and Connection pages. No new settings capabilities — this is a reorganization of existing controls.

</domain>

<decisions>
## Implementation Decisions

### Category mapping — where controls land

**AA page (slim):**
- Resolution picker, DPI slider, auto-connect toggle only
- FPS removed from UI entirely (SET-08) — YAML-only, 30fps default
- Codec/decoder selection moves to Debug
- Protocol capture moves to Debug

**Display page:**
- Screen info (size/PPI/resolution) as an info bar at the top — contextual reference, not a section
- UI scale stepper
- Brightness slider (stays here)
- Navbar settings (position, show during AA)

**Audio page:**
- No changes — master volume, output device, mic gain, input device, EQ access

**Bluetooth page:**
- Name stays "Bluetooth" (not Connections)
- BT device name, pairing toggle, paired devices list
- WiFi AP settings removed (moved to Debug as read-only)
- All AA duplicate settings removed (auto-connect, TCP port, protocol capture)

**Theme page (NEW — promoted from Display):**
- Theme picker — all themes in one flat list, no grouping by source (bundled/user/companion)
- Wallpaper picker
- Force dark mode toggle (duplicated here AND on System — same config path, auto-synced)
- Swipe-to-delete for non-default themes (replaces double-tap confirmation)

**Companion page:**
- No changes — enable toggle, connection status, pairing code, GPS/battery/proxy info

**System page:**
- Left-hand drive toggle
- Clock 24h toggle (moved from Display)
- Force dark mode toggle (duplicate of Theme toggle — same config path)
- Night mode configuration (source picker, time settings, GPIO pin/active-high) — moved from Display
- Software version
- Close App button

**Information page (NEW — split from System):**
- All identity fields read-only: HU name, manufacturer, model, car model, car year
- Hardware profile (read-only)
- Touch device (read-only)
- No software version (that's on System)

**Debug page (NEW — last in list, visible):**
- Codec/decoder selection (full section — per-codec enable/disable, hw/sw toggle, decoder name)
- Protocol capture toggle, format, include media frames, capture path
- TCP port (read-only info)
- WiFi AP channel/band (read-only info, YAML-only to change)
- AA protocol test buttons (Play/Pause, Prev, Next, Search, Assist, Voice)
- No verbose logging toggle — stays YAML/CLI only

### Removals
- FPS selector removed from UI (YAML-only)
- About OpenAutoProdigy section removed entirely (SET-09)
- ConnectivitySettings.qml — unused file, delete
- All duplicate settings between AASettings and ConnectionSettings eliminated

### Claude's Discretion
- Section ordering within each page
- Info bar visual treatment on Display page
- Swipe-to-delete implementation details
- How to handle the transition from old ConnectionSettings to new Bluetooth page
- Debug page visual styling (whether to differentiate it from regular settings pages)

</decisions>

<specifics>
## Specific Ideas

- Screen info on Display should be an info bar at the top, not a settings section — provides context while adjusting scale/brightness
- Force dark mode toggle intentionally duplicated on Theme AND System pages — users look for it in both places, same config path keeps them synced
- WiFi AP on Debug should clearly indicate "configured at install time, change via YAML" — not just silently read-only
- Theme deletion: swipe-to-delete, all themes deletable except the default theme

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `SettingsToggle`: config-path-driven on/off switch with restart badge — used extensively
- `SettingsSlider`: debounced value slider with config persistence
- `SettingsListItem`: clickable row with icon/label/chevron — used for category navigation
- `ReadOnlyField`: icon + label + value display — perfect for Information page
- `SectionHeader`: uppercase divider — used for grouping within pages
- `FullScreenPicker`: modal bottom-sheet selector — used for theme/wallpaper/device pickers
- `SegmentedButton`: multi-option toggle — used for codec format, WiFi band
- `InfoBanner`: status message component — could be adapted for Display screen info bar

### Established Patterns
- Config-driven controls: most settings bind via `configPath` string, auto-load/save
- StackView navigation: SettingsMenu manages push/pop with back handling
- Section grouping via SectionHeader with conditional visibility
- Restart badges for settings that need app restart

### Integration Points
- `SettingsMenu.qml` routes to category pages — needs 3 new routes (Theme, Information, Debug)
- `ApplicationController` provides back navigation + title management
- `CodecCapabilityModel` drives codec UI — moves from AASettings to DebugSettings
- `PairedDevicesModel` stays on Bluetooth page
- EQ sub-page navigation from Audio stays unchanged

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-settings-restructure*
*Context gathered: 2026-03-10*
