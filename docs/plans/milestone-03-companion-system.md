# Milestone 3: Companion & System (Feb 22-23, 2026)

## What Was Built

### Companion App (Android, Kotlin)

- **Phone-to-Pi data bridge:** Lightweight Android companion app pushes time, GPS, battery level, and internet proxy status to the Pi head unit over TCP (port 9876, newline-delimited JSON, 5-second interval).
- **SOCKS5 proxy server:** CONNECT-only proxy (RFC 1928/1929) on port 1080 bound to WiFi interface, routes Pi traffic out the phone's cellular connection via explicit `Network.bindSocket()`. Username/password auth required. Egress ACLs block RFC1918, link-local, loopback, and multicast to prevent abuse as a network scanner.
- **Challenge-response authentication:** Pi sends random 32-byte nonce, phone responds with HMAC-SHA256(shared_secret, nonce). Session key issued on success. All subsequent status messages carry per-message HMAC. Sequence numbers with sliding window of 10 prevent replay.
- **QR/PIN pairing:** Pi generates 6-digit PIN, phone enters it. Shared secret derived via HKDF-SHA256(PIN + device_id + timestamp). Stored in Android Keystore (phone) and `~/.openauto/companion.key` (Pi, mode 600).
- **Auto-start on WiFi SSID detection:** `ConnectivityManager.NetworkCallback` watches for the Pi's AP SSID. Foreground service starts automatically on match, stops on WiFi disconnect.
- **Multi-vehicle support:** Companion app supports pairing with up to 20 head units. Vehicle list stored as JSON array in SharedPreferences. SSID-based matching determines which vehicle is connected. Per-vehicle SOCKS5 enable/disable and credentials.
- **Jetpack Compose UI with 3 screens:**
  - VehicleListScreen: home screen showing all paired vehicles with connection indicators
  - PairingScreen: collects SSID, friendly name, and 6-digit PIN
  - StatusScreen: per-vehicle status with sharing toggles and unpair action
- **Legacy prefs migration:** On first launch after multi-vehicle update, single-vehicle SharedPreferences automatically migrated to vehicle list format. Idempotent.

### Pi-Side CompanionListener (C++)

- **CompanionListenerService:** TCP server on port 9876. Single connection at a time (rejects additional clients). Validates auth, parses JSON, dispatches to clock/GPS/battery/proxy subsystems. Exposes state as Q_PROPERTYs for QML bindings.
- **Clock management:** Calls `timedatectl set-time` via D-Bus (polkit-authorized). Only adjusts when delta exceeds 30 seconds. Backward jump protection: rejects >5-minute backward jumps unless 3 consecutive messages agree. Disables systemd-timesyncd during companion-driven sync.
- **GPS state tracking:** Exposes lat/lon/speed/accuracy/bearing as Q_PROPERTYs. `age_ms` field from phone allows Pi to assess staleness (stale at 30s, cleared at 60s).
- **IPC endpoint:** `companion_status` command exposed via Unix socket IPC for web config panel.
- **Polkit rule:** Ships `/etc/polkit-1/rules.d/50-openauto-time.rules` to allow non-root time setting.

### Pi Installer Multi-Vehicle Support

- **UUID-suffixed default SSID:** `install.sh` generates 4-char hex UUID and offers "OpenAutoProdigy-XXXX" as default. User can override with custom SSID. Prevents conflicts between multiple vehicles.

### Settings UI Scaling (UiMetrics)

- **Centralized design token system:** `UiMetrics.qml` singleton computes a clamped scale factor from the shortest screen edge vs 600px baseline: `scale = clamp(0.9, min(width, height) / 600, 1.35)`.
- **14 design tokens:** rowH, touchMin, marginPage, marginRow, spacing, gap, sectionGap, fontTitle, fontBody, fontSmall, fontTiny, fontHeading, headerH, sectionH, backBtnSize, iconSize, iconSmall, radius, gridGap.
- **Replaced all hardcoded pixels** in 7 control files (SettingsComboBox, SettingsToggle, SettingsSpinBox, SettingsTextField, SettingsSlider, SettingsPageHeader, SectionHeader) and 7 settings pages (Audio, Video, Display, Connection, System, About, SettingsMenu).
- **Width handling:** Fixed-width controls converted to proportional sizing (ComboBox: `width * 0.35`, SpinBox: `width * 0.3`, exit dialog: `width * 0.4`).

### Touch-Friendly Settings Redesign

- **Material Dark style foundation:** Switched from Basic/default to Material Dark as application-wide Qt Quick Controls style. `Material.theme` wired to ThemeService day/night mode.
- **UiMetrics baseline bump for car touchscreen:** Row height 48px -> 64px, touch minimum 44px -> 56px, body font 15px -> 20px, small font 13px -> 16px, all spacing increased proportionally. Existing UiMetrics references throughout QML automatically picked up larger values.
- **New touch controls:**
  - `FullScreenPicker`: Modal bottom-sheet dialog with 64px-tall tappable list items. Replaces ComboBox for multi-option selections (audio devices, resolution, day/night source, WiFi channel).
  - `SegmentedButton`: Inline 2-3 option toggle for small option sets (FPS 30/60, WiFi band, sidebar position, orientation).
  - `ReadOnlyField`: Display-only field with "Edit via web panel" hint. Replaces TextField for settings requiring keyboard input.
  - `InfoBanner`: Animated "Restart required" notification banner.
- **HeadUnit style overrides:** Custom style layer on top of Material Dark for car-scale sizing (48px slider handle, oversized switch, 64px minimum button height, 64px ItemDelegate).
- **Page-by-page migration:** All six settings pages rewritten with touch-friendly controls. SpinBox replaced with Tumbler or Slider. TextField replaced with ReadOnlyField or removed entirely (WiFi SSID/password/interface moved to web panel only).
- **Retired controls:** SettingsComboBox, SettingsSpinBox, SettingsTextField removed after full migration.

## Key Design Decisions

### Companion App Architecture

- **TCP + JSON over WiFi AP, not USB or Bluetooth:** Phone is already connected to Pi's WiFi AP for Android Auto. Using the same network link for companion data avoids additional hardware dependencies. TCP provides reliable delivery. JSON chosen for simplicity and debuggability.
- **SOCKS5 CONNECT-only, not VPN:** Full transparent internet via VpnService was evaluated and deferred. SOCKS5 CONNECT is ~200-300 lines of code, works for HTTP/HTTPS (the main use case), and doesn't require root or VPN permissions. UDP traffic (NTP, raw DNS) doesn't traverse SOCKS5, but time sync is handled by direct push and DNS via `socks5h://` (proxy resolves).
- **Phone pushes to Pi, not Pi pulls from phone:** Pi has no way to discover the phone's IP on its own AP. The phone knows the Pi's fixed IP (10.0.0.1). Push model also means the phone controls its own battery usage by adjusting push interval.
- **Per-message HMAC with session key:** Challenge-response proves identity at connection time. Session key provides ongoing message integrity. Sequence numbers prevent replay. HMAC on cleartext JSON is acceptable for v1 since the WiFi AP uses WPA2 for link-layer encryption.
- **Single connection at a time:** Pi rejects additional companion connections. Simplifies state management. If the phone reconnects (WiFi flap), old connection times out and new one is accepted.

### Multi-Vehicle Design

- **SSID as vehicle identifier, not UUID or serial number:** The WiFi SSID is the natural unique identifier — it's what the phone sees, it's configured at install time, and it's human-readable. UUID-suffixed defaults prevent accidental collisions between vehicles.
- **All multi-vehicle logic on the phone side:** The Pi doesn't know or care about multi-vehicle. Each Pi is its own world. The phone stores a list of vehicles keyed by SSID and matches against the current WiFi network. This means zero Pi-side protocol changes.
- **Connection generation counter for race conditions:** Each WiFi connection start increments a generation. Async callbacks from stale generations are ignored, preventing flapping when the phone briefly sees multiple networks.
- **Soft cap of 20 vehicles:** Prevents UI/performance edge cases while being more than enough for any real use case.

### Settings UI Strategy

- **Token system before touch redesign:** UiMetrics was built first as a pure refactor (replacing magic numbers with tokens at identical values), then baseline values were bumped in a separate step. This made the touch redesign a non-destructive operation — if the larger sizes didn't work, only the baseline values needed reverting.
- **Material Dark as foundation, HeadUnit as override layer:** Using Qt's built-in Material style provides a complete, tested set of touch-friendly controls. The thin HeadUnit custom style layer only overrides controls that need car-scale sizing. This means buttons, sliders, switches, and other standard controls get Material's touch feedback, ripple effects, and accessibility for free.
- **TextField removal instead of on-screen keyboard:** Adding an on-screen keyboard to Qt/QML on a Pi was deemed too complex and fragile. Instead, keyboard-dependent settings (WiFi SSID, password, head unit name, etc.) were moved to the web config panel. The touchscreen shows current values as ReadOnlyField with "Edit via web panel" hints. This keeps the touch UI simple and pushes complex text input to a proper keyboard (laptop/phone browser).
- **FullScreenPicker over ComboBox:** Qt's ComboBox dropdown is tiny and hard to tap on a 7" screen. The FullScreenPicker opens a modal bottom sheet filling ~60% of screen height with 64px-tall tappable rows. Single tap selects and closes.
- **SegmentedButton for 2-3 option choices:** For FPS (30/60), WiFi band (2.4/5 GHz), sidebar position (Left/Right), and orientation — inline segmented buttons are faster than opening a picker.
- **Rollback tag (`v0.3.0-pre-touch-redesign`):** Created before the Material style switch as a safety net. The touch redesign touches nearly every QML file; having a known-good checkpoint was essential.

### Qt Compatibility

- **Material style properties restricted to Qt 6.4 common set:** `roundedScale` and `containerStyle` are Qt 6.5+ only and were avoided. Core Material properties (theme, accent, primary, foreground, background, elevation) and Dense/Normal variant are available in Qt 6.4.
- **Tumbler over SpinBox:** Qt's Tumbler (flick-to-scroll wheel picker) is available since Qt 5.7 — no compatibility concern. Used for time pickers (HH:MM), GPIO pin selection, and DPI.

## Lessons Learned

### Android Development

- **Dual-network routing (`Network.bindSocket()`):** The key to making SOCKS5 work — phone must explicitly bind outbound sockets to the cellular network. Without this, traffic would try to exit via WiFi (which goes to the Pi, not the internet). Needs early testing on target phones (Samsung S25 was the primary test device).
- **Android 14 FGS restrictions:** Must declare `foregroundServiceType="location"` in manifest and start with `FOREGROUND_SERVICE_TYPE_LOCATION`. Location permission must be granted before foreground service start, not after.
- **WiFi SSID access requires location permission:** Android gates SSID visibility behind `ACCESS_FINE_LOCATION`. Additionally, Android 13+ requires `NEARBY_WIFI_DEVICES`. The SSID may transiently report as `<unknown ssid>` — must be handled gracefully.
- **Connection generation counter prevents WiFi flapping bugs:** Without it, rapid WiFi connect/disconnect cycles could start multiple CompanionService instances or connect to the wrong vehicle.
- **SharedPreferences JSON migration must be idempotent:** If the app crashes during migration, it must not corrupt data or re-migrate on next launch.

### Settings UI

- **Scale factor clamping (0.9-1.35) prevents extreme results:** Without clamping, a 480p display would make everything tiny and a 1080p display would make everything comically large. The clamp keeps the UI usable across the realistic range of car touchscreens.
- **Non-settings UI must be verified after Material style switch:** Shell, TopBar, BottomBar, and nav strip use custom components that should be unaffected, but launcher tiles and exit dialog use some Qt Controls and need visual verification.
- **One page per commit during migration:** The touch redesign modifies all six settings pages. Migrating one at a time with individual commits makes it easy to bisect if something breaks.
- **Proportional widths over fixed widths:** `Layout.preferredWidth: 160` breaks on different screen sizes. `parent.width * 0.35` adapts naturally.

### Protocol & Security

- **SOCKS5 cleartext auth acceptable on local AP:** The Pi's WiFi AP uses WPA2, providing link-layer encryption. Adding TLS to the SOCKS5 proxy would add complexity for minimal security benefit in this deployment model.
- **Connection limits and rate limiting on SOCKS5:** Max 20 concurrent connections, 3 auth failures trigger 30-second lockout, 120-second idle timeout. Prevents abuse if someone connects to the AP without pairing.
- **timedatectl polkit rule must be in the installer:** Non-interactive D-Bus calls to `org.freedesktop.timedate1.SetTime` require polkit authorization. The rule must be deployed during install, not at runtime.

## Source Documents

- `2026-02-22-companion-app-design.md` — Companion app architecture, data protocol, SOCKS5 proxy design, pairing flow, Android permissions, risk register
- `2026-02-22-companion-app-implementation.md` — Pi-side CompanionListenerService implementation (C++ skeleton, auth, status handling, clock management, IPC), Android app implementation phases
- `2026-02-22-multi-vehicle-companion-design.md` — Multi-vehicle storage model, SSID matching, WifiMonitor redesign, UI navigation flow, install.sh UUID default
- `2026-02-22-multi-vehicle-companion-plan.md` — Multi-vehicle implementation tasks (Vehicle data model, CompanionPrefs, WifiMonitor, PairingScreen, VehicleListScreen, StatusScreen, MainActivity navigation, install.sh, E2E test)
- `2026-02-22-settings-ui-scaling-design.md` — UiMetrics singleton design, scale factor computation, design token definitions, proportional width strategy
- `2026-02-22-settings-ui-scaling.md` — UiMetrics implementation plan (singleton creation, CMake registration, 7 control files, 7 settings pages, Pi deployment/test)
- `2026-02-22-touch-friendly-settings-redesign.md` — Material Dark style, HeadUnit fallback, UiMetrics baseline bump, FullScreenPicker/SegmentedButton/ReadOnlyField/InfoBanner design, page-by-page control mapping, retired controls, implementation phases
- `2026-02-22-touch-friendly-settings-implementation.md` — Touch redesign implementation (15 tasks across 3 phases: foundation, page migration, cleanup)
