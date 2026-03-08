# OpenAuto Prodigy

## What This Is

Open-source wireless Android Auto head unit for Raspberry Pi 4, replacing the defunct OpenAuto Pro. Qt 6 + QML application with plugin architecture, PipeWire audio, and direct BlueZ D-Bus integration. Targets car-mounted Pi with touchscreen as a daily-driver head unit.

## Core Value

A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## Requirements

### Validated

<!-- Shipped and confirmed valuable. -->

- ✓ Wireless AA connection flow — BT discovery → WiFi AP → TCP → video projection — existing
- ✓ Multi-codec video decode (H.264/H.265/VP9/AV1) with hw/sw fallback — existing
- ✓ Touch input via direct evdev with multi-touch + 3-finger gesture overlay — existing
- ✓ Audio playback via PipeWire (media, navigation, phone streams) — existing
- ✓ Plugin system with IPlugin interface, lifecycle hooks, QML views — existing
- ✓ BT Audio plugin (A2DP sink + AVRCP controls via BlueZ D-Bus) — existing
- ✓ Phone plugin (HFP dialer + incoming call overlay via BlueZ D-Bus) — existing
- ✓ BluetoothManager with pairing dialog, auto-connect retry, device model — existing
- ✓ Settings UI (audio, video, display, connection, system sections) — existing
- ✓ YAML config with deep merge, typed accessors — existing
- ✓ Day/night theme via ThemeService — existing
- ✓ Categorized logging with quiet defaults, verbose via CLI/config/web panel — v0.4
- ✓ Multi-theme color palettes with user override paths — v0.4
- ✓ Wallpaper system with per-theme defaults and user override — v0.4
- ✓ 3-tier brightness control (sysfs > ddcutil > software overlay) — v0.4
- ✓ 10-band graphic EQ with per-stream profiles, bundled/user presets, touch UI — v0.4.1
- ✓ Interactive installer for RPi OS Trixie (hardware detection, hostapd, labwc, services) — existing
- ✓ Prebuilt release distribution workflow — existing
- ✓ Web config panel (Flask + Unix socket IPC) — existing
- ✓ Protocol capture logging (jsonl/tsv) — existing
- ✓ Cross-compilation toolchain (Pi 4 aarch64) — existing
- ✓ 47 unit tests covering core systems — existing
- ✓ Reliable WiFi AP: rfkill self-heal, hostapd recovery, independent of app lifecycle — v0.4.2
- ✓ systemd service hardening: correct ordering, auto-restart with rate limiting, sd_notify — v0.4.2
- ✓ SDP self-healing: socket permissions, BlueZ --compat override, app-side retry with logging — v0.4.2
- ✓ PipeWire graceful degradation and clean stream teardown on shutdown — v0.4.2
- ✓ Installer health check: PipeWire, BlueZ, hostapd, labwc, group memberships — v0.4.2
- ✓ Automotive-minimal control restyling with press feedback, Material Symbols icons, theme-aware dividers — v0.4.3
- ✓ Settings restructured into 6-category tile grid (AA, Display, Audio, Bluetooth, Companion, System) — v0.4.3
- ✓ EQ dual-access: NavStrip shortcut + Audio settings, deep navigation with shared state — v0.4.3
- ✓ Shell polish: NavStrip/TopBar/launcher restyled, modal dismiss, BT forget pill button — v0.4.3
- ✓ Day/night theme color animation on all major surfaces — v0.4.3
- ✓ UI scales correctly across screen resolutions (800x480 through ultrawide) — v0.4.4
- ✓ UiMetrics unclamped dual-axis scaling drives all sizing — zero hardcoded pixel values in QML — v0.4.4
- ✓ Layout adapts to different aspect ratios without clipping or wasted space — v0.4.4
- ✓ Zone-based evdev touch routing with priority dispatch and finger stickiness — v0.4.5
- ✓ 3-control navbar (volume/clock-home/brightness) with tap/short-hold/long-hold gestures — v0.4.5
- ✓ Navbar edge positioning (top/bottom/left/right) with LHD/RHD awareness — v0.4.5
- ✓ Navbar-aware AA viewport margins with correct touch routing split — v0.4.5
- ✓ GestureOverlay evdev touch passthrough fix (per-control hit zones under EVIOCGRAB) — v0.4.5
- ✓ Single navigation system — TopBar, NavStrip, sidebar overlay removed — v0.4.5
- ✓ Proto submodule updated to v1.0 verified definitions (223 proto files) — v0.5.0
- ✓ NavigationTurnEvent parsing with road name, maneuver, distance, icon, lane guidance — v0.5.0
- ✓ VoiceSessionRequest START/STOP for voice button activation — v0.5.0
- ✓ BT channel auth data exchange (0x8003/0x8004) — v0.5.0
- ✓ InputBindingNotification haptic feedback parsing — v0.5.0
- ✓ Unhandled message debug logging with hex payload for full protocol observability — v0.5.0
- ✓ Protocol library renamed to prodigy-oaa-protocol — v0.5.0
- ✓ 64 unit tests covering core systems — v0.5.0

### Active

<!-- Current scope. Building toward these. -->

- [ ] HFP call audio persists across AA connection state (calls don't drop if AA disconnects)
- [ ] First-run experience guides user through phone pairing and WiFi verification
- [ ] Web config panel EQ interface
- [ ] Boot speed optimization (reliable first, fast later)

### Out of Scope

<!-- Explicit boundaries. Includes reasoning to prevent re-adding. -->

- USB Android Auto — wireless-only by design
- CarPlay or non-AA protocols — single protocol focus for quality
- Hardware beyond Pi 4 — target hardware only for v1.0
- Cloud services, accounts, telemetry — privacy-first, offline-only
- Companion app — separate repo, separate timeline
- Per-connection WiFi rotation — post-v1.0 security enhancement
- Three-tier architecture refactor (de-Qt protocol lib) — post-v1.0 community architecture
- CI automation — desirable but not gating v1.0
- Community plugin examples (OBD, camera, GPIO) — plugin system exists, examples are post-v1.0
- User-facing scale options (S/M/L) — foundation shipped in v0.4.4, user controls in future milestone
- Custom icon sets / full theme engine — color palettes + wallpapers are sufficient for v1.0

## Context

OpenAuto Pro (BlueWave Studio) was a commercial Pi-based AA head unit that went defunct. This project is a clean-room rebuild — no OAP code, no aasdk dependency. The protocol library (`open-android-auto`) is maintained as a separate community resource.

v0.4 shipped logging and theming. v0.4.1 shipped 10-band graphic EQ with per-stream profiles. v0.4.2 shipped service hardening — WiFi AP, Bluetooth SDP, systemd ordering, and clean shutdown all work reliably without manual intervention. v0.4.3 shipped full UI refresh — automotive-minimal styling, 6-category settings, EQ dual-access, shell polish. v0.4.4 shipped resolution independence — unclamped dual-axis UiMetrics, full QML tokenization (zero hardcoded pixels), container-derived grid layouts, runtime auto-detection, and --geometry validation tooling. v0.4.5 shipped navbar rework — zone-based evdev touch routing, 3-control navbar with multi-gesture actions and edge positioning, navbar-aware AA viewport margins, gesture overlay touch fix, and dead UI cleanup (TopBar, NavStrip, sidebar removed). v0.5.0 shipped protocol compliance — proto submodule v1.0, navigation turn events, voice session commands, BT auth exchange, haptic feedback, retracted dead code cleanup after v1.2 proto verification, library renamed to prodigy-oaa-protocol. Codebase is ~34.7K lines C++/QML, 64 unit tests.

## Constraints

- **Hardware:** Raspberry Pi 4, multiple touchscreens (800x480 Pi official, 1024x600 DFRobot, larger/ultrawide), built-in WiFi/BT
- **Stack:** Qt 6.8 + QML, C++17, CMake, PipeWire, BlueZ D-Bus — locked
- **Dev platform:** Qt 6.4 on Ubuntu VM (no Bluetooth) + Qt 6.8 on Pi (full features)
- **Solo maintainer:** Process must stay lightweight; no heavy CI/review gates
- **No GPU on dev VM:** GPU-dependent work (DMA-BUF, hw decode) tested on Pi only
- **Submodule:** `libs/prodigy-oaa-protocol/proto/` is read-only — managed separately

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Wireless-only (no USB AA) | Simplifies transport, matches modern phone support | ✓ Good |
| Direct BlueZ D-Bus (no ofono) | Ofono is unreliable on modern Trixie; direct D-Bus is cleaner | ✓ Good |
| PipeWire over PulseAudio | Default on Trixie, lower latency, better device management | ✓ Good |
| Static plugins compiled in | Simpler build, faster startup, easier to maintain solo | ✓ Good |
| YAML config over INI | Deep merge, structured data, theme/device cascading | ✓ Good |
| Direct evdev touch (not Qt plugin) | Avoids labwc/Qt touch conflicts, enables EVIOCGRAB for AA | ✓ Good |
| Companion app separate repo/timeline | Head unit v1.0 shouldn't be blocked by phone app | — Pending |
| Color palette + wallpaper themes (not full engine) | Right scope for v1.0 — full themes are post-release | ✓ Good |
| Qt logging categories per subsystem | Clean default output, granular verbose control | ✓ Good |
| 3-tier brightness (sysfs > ddcutil > software) | Works on all hardware, DFRobot has no sysfs backlight | ✓ Good |
| Wallpaper override independent of theme | User can mix theme colors with any wallpaper | ✓ Good |
| Config defaults gate pattern | setValueByPath silently fails without initDefaults() entry | ⚠️ Revisit |
| Scale+opacity press feedback (not color overlay) | More tactile, works on any theme, no color math | ✓ Good |
| Settings 6-tile grid (not expandable list) | Glanceable at car distance, matches mental model | ✓ Good |
| EQ as NavStrip go-to shortcut (not plugin icon) | Shortcut navigates into settings, avoids plugin state conflicts | ✓ Good |
| Descope tile subtitles | Too small at 1024x600 for automotive use — deferred | ✓ Good |
| Descope WiFi AP from settings | Set-once at install via hostapd, no runtime UI needed | ✓ Good |
| Dual-axis scale: min for layout, geomean for fonts | Overflow safety + balanced readability on non-square screens | ✓ Good |
| Unclamped UiMetrics (no 0.9-1.35 range) | Needed for 800x480 to actually look smaller, ultrawide to spread | ✓ Good |
| fontTiny via _tok() fallback | Overridable through config but auto-derives if unset | ✓ Good |
| Container-derived grids (not fixed column count) | LauncherMenu adapts column count to width; settings keeps 3x2 | ✓ Good |
| --geometry CLI flag for resolution testing | Xvfb/VNC modes enable headless validation without real hardware | ✓ Good |
| Remove display.width/height/orientation config | Auto-detection is sole source; dead config caused confusion | ✓ Good |
| Layout-derived sidebar touch zones | C++ zone boundaries mirror QML math for consistency at any resolution | ✓ Good |
| Zone-based touch routing (TouchRouter) | Priority dispatch with finger stickiness replaces hardcoded sidebar hit-tests | ✓ Good |
| 3-control navbar (not plugin icons) | Volume/clock-home/brightness covers all needed actions; plugins via launcher tiles | ✓ Good |
| Tap/short-hold/long-hold gestures (no double-tap) | Avoids 300ms tap delay; short-hold-release is discoverable and reliable | ✓ Good |
| EvdevCoordBridge shared between AA + navbar + overlay | Single pixel-to-evdev bridge prevents coordinate divergence | ✓ Good |
| Per-control evdev zones at priority 210 | Higher than consume zone (200), ensures slider drags work under EVIOCGRAB | ✓ Good |
| computeContentDimensions() static method | Single source of truth for content-space dimensions used by touch + margins | ✓ Good |
| Proto exclusion pattern for reserved names | CMake EXCLUDE filters avoid protobuf descriptor field conflicts | ✓ Good |
| [AA:unhandled] prefix for library-level logging | Distinct from lcAA category, easy grep filtering for unregistered channels | ✓ Good |
| Voice session on ControlChannel (ch0, 0x0011) | Per wire capture, not InputChannel as initially assumed | ✓ Good |
| BT auth signals emit raw QByteArray | No proto schema exists for BT auth data — raw bytes are correct | ✓ Good |
| Retracted requirements after v1.2 proto verification | NAV-02, AUD-01, AUD-02, MED-01 were misidentified messages — removed code | ✓ Good |
| Library rename to prodigy-oaa-protocol | Distinguishes Prodigy's protocol lib from upstream open-android-auto | ✓ Good |
| WiFi BSSID sends MAC not SSID | QNetworkInterface for wlan0 MAC — correct SDP semantics | ✓ Good |

---
*Last updated: 2026-03-08 after v0.5.0 milestone completed*
