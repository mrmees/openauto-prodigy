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
- ✓ DPI-based UiMetrics from EDID physical screen size with 7" default — v0.5.1
- ✓ User-facing scale stepper (±0.1) in Display settings — v0.5.1
- ✓ Clock readability: 75% control height, DemiBold, no AM/PM, 24h toggle — v0.5.1
- ✓ Full 34-role M3 color palette with semantic roles across all UI — v0.5.1
- ✓ Companion theme import (M3 palette + wallpaper over TCP) — v0.5.1
- ✓ AA rendering fix (PreserveAspectFit, no cropping with navbar) — v0.5.1
- ✓ SDP hidden_ui_elements (clock) when navbar visible during AA — v0.5.1
- ✓ M3 button components (ElevatedButton, FilledButton, OutlinedButton) with depth effects — v0.5.1
- ✓ Surface container hierarchy (Lowest/Low/Container/High/Highest) — v0.5.1
- ✓ 71 unit tests covering core systems — v0.5.1
- ✓ Widget-based home screen with 3-pane layout, launcher dock, and 3 built-in widgets (Clock, AA Status, Now Playing) — v0.5.2
- ✓ Settings reorganized into 9 focused categories with power-user settings demoted to YAML-only — v0.5.2
- ✓ SettingsRow alternating-row styling across all settings pages with touch-friendly sizing — v0.5.2
- ✓ Font sizes scaled ~1.4x for automotive readability (3mm+ cap height at arm's length) — v0.5.2
- ✓ Long-press-to-go-back gesture with expanding ripple indicator in settings — v0.5.2
- ✓ Cell-based freeform grid widget layout with drag-to-reposition, drag-to-resize, occupancy tracking — v0.5.3
- ✓ Multi-page home screen with SwipeView swipe navigation, PageIndicator, lazy instantiation — v0.5.3
- ✓ AA Navigation turn-by-turn widget (maneuver icon via QQuickImageProvider, road name, distance) — v0.5.3
- ✓ Unified Now Playing widget — AA media when connected, BT A2DP fallback, source indicator — v0.5.3
- ✓ Clock and AA Status widgets revised for variable grid sizing with pixel breakpoints — v0.5.3
- ✓ Edit mode (long-press entry, FAB add, X remove, 10s timeout, AA auto-exit) — v0.5.3
- ✓ Grid density auto-derived from display size via UiMetrics — v0.5.3
- ✓ Widget layout persistence in YAML schema v3 (grid positions, sizes, pages, opacity) — v0.5.3
- ✓ 77 unit tests covering core systems — v0.5.3
- ✓ WidgetDescriptor with category, description, icon metadata, min/max/default size constraints — v0.6.1
- ✓ DPI-based grid sizing (diagonal-proportional cellSide formula, auto-snap threshold) — v0.6.1
- ✓ Launcher dock replaced by singleton launcher widgets on reserved page — v0.6.1
- ✓ Grid spacing refined: auto-snap recovers gutter waste, page dots moved to navbar — v0.6.1
- ✓ Formalized widget contract: span-based breakpoints, context injection, ActionRegistry egress — v0.6.1
- ✓ All 6 widgets rewritten to formalized contract (no pixel breakpoints, no root globals) — v0.6.1
- ✓ Widget developer guide + plugin API reference updated + 15 architecture decision records — v0.6.1
- ✓ 85 unit tests covering core systems (all passing) — v0.6.1
- ✓ Wallpaper cover-fit scaling with texture memory cap and transition stability — v0.6.2
- ✓ Companion reconnect hardening (always-replace, socket cleanup, inactivity timeout) — v0.6.2
- ✓ Theme persistence (setTheme persists to config, wallpaper toggle gate) — v0.6.2
- ✓ M3 color audit — solid primaryContainer fills, surface tint properties, warning tokens, night comfort guardrail — v0.6.2
- ✓ 9 companion-created M3 themes promoted to bundled defaults, Prodigy as first-install identity — v0.6.2
- ✓ State matrix document as single source of truth for control token assignments — v0.6.2
- ✓ FullScreenPicker GPU performance fix (removed per-delegate shadows) — v0.6.2
- ✓ 87 unit tests covering core systems (all passing) — v0.6.2
- ✓ System service privilege model (root daemon, 0660 socket, openauto group, SO_PEERCRED auth) — v0.6.3
- ✓ Transparent proxy routing on real hardware (iptables, redsocks, owner+destination self-exemption) — v0.6.3
- ✓ Proxy state reporting is truthful (ACTIVE requires verified pipeline, FAILED/DEGRADED with error codes) — v0.6.3
- ✓ Proxy rule application is idempotent (flush/recreate model, startup self-heal) — v0.6.3
- ✓ End-to-end hardware validation of companion SOCKS proxy traversal — v0.6.3
- ✓ 140 Python + 87 C++ tests covering core systems (all passing) — v0.6.3
- ✓ Per-widget instance configuration with schema validation, gear icon config sheet, YAML persistence — v0.6.4
- ✓ Theme cycle widget (1x1, tap advances to next theme with name display) — v0.6.4
- ✓ Phone battery status widget (1x1, companion data with charging indicator) — v0.6.4
- ✓ Companion status widget (1x1 connected/disconnected, expanded GPS/battery/proxy at 2x1+) — v0.6.4
- ✓ AA focus toggle widget (VideoFocusRequest NATIVE/PROJECTED with state reflection) — v0.6.4
- ✓ Clock style variants (digital, analog, minimal — per-instance selection via config sheet) — v0.6.4
- ✓ Weather widget with Open-Meteo API, companion GPS, subscriber-aware caching, responsive breakpoints — v0.6.4
- ✓ 88 unit tests covering core systems (all passing) — v0.6.4
- ✓ Standalone date widget with column-driven breakpoints and US/intl config — v0.6.5
- ✓ Clock simplified to digital + analog (minimal removed), bold font, no AM/PM, analog accent hour hand — v0.6.5
- ✓ Custom battery gauge widget (Canvas, vertical/horizontal orientation, accent fill, red at ≤20%) — v0.6.5
- ✓ All widgets scale with layout size, Material Symbols font updated (4283 glyphs), all codepoints corrected — v0.6.5
- ✓ AAStatusWidget removed (redundant with AA focus toggle) — v0.6.5
- ✓ Now Playing reworked with automotive-sized controls, scrolling titles, progressive breakpoints — v0.6.5
- ✓ Per-widget long-press selection model replacing global edit mode (lift animation, accent border, auto-deselect) — v0.6.6
- ✓ Navbar gear/trash transformation during widget selection with tap-only enforcement and widget name display — v0.6.6
- ✓ 4-edge resize handles with atomic resizeWidgetFromEdge API, ghost preview, min/max clamping, collision detection — v0.6.6
- ✓ Bottom-sheet widget picker with categorized list, auto-placement, and "No Widget" entry suppression — v0.6.6
- ✓ Long-press empty space popup menu for Add Widget / Add Page with grid bounds check and gesture arbitration — v0.6.6
- ✓ Empty page auto-delete on widget removal via navbar trash with shifted-index race prevention — v0.6.6
- ✓ All FABs, badge buttons (X delete, gear config, corner resize), and global edit mode removed — v0.6.6
- ✓ Overlay lifecycle cleanup for no-selection states (AA fullscreen, plugin change, settings navigation, page change) — v0.6.6

### Active

<!-- Current scope. Building toward these. -->

(No active milestone — next milestone TBD)

### Backlog

<!-- Acknowledged but not in current milestone. -->

- HFP call audio persists across AA connection state (calls don't drop if AA disconnects)
- First-run experience guides user through phone pairing and WiFi verification
- Web config panel EQ interface
- Boot speed optimization (reliable first, fast later)
- BT Audio / Phone plugin rework — evaluate value vs stripping in favor of pure AA

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

v0.4 shipped logging and theming. v0.4.1 shipped 10-band graphic EQ with per-stream profiles. v0.4.2 shipped service hardening — WiFi AP, Bluetooth SDP, systemd ordering, and clean shutdown all work reliably without manual intervention. v0.4.3 shipped full UI refresh — automotive-minimal styling, 6-category settings, EQ dual-access, shell polish. v0.4.4 shipped resolution independence — unclamped dual-axis UiMetrics, full QML tokenization (zero hardcoded pixels), container-derived grid layouts, runtime auto-detection, and --geometry validation tooling. v0.4.5 shipped navbar rework — zone-based evdev touch routing, 3-control navbar with multi-gesture actions and edge positioning, navbar-aware AA viewport margins, gesture overlay touch fix, and dead UI cleanup (TopBar, NavStrip, sidebar removed). v0.5.0 shipped protocol compliance — proto submodule v1.0, navigation turn events, voice session commands, BT auth exchange, haptic feedback, retracted dead code cleanup after v1.2 proto verification, library renamed to prodigy-oaa-protocol. v0.5.1 shipped DPI sizing & UI polish — EDID-based DPI scaling, scale stepper, clock readability, full 34-role M3 color system, companion theme import, AA rendering fix, navbar status bar cleanup, M3 button components with visual depth effects. v0.5.2 shipped widget system & UI polish — 3-pane home screen with launcher dock and built-in widgets, settings reorganized into 9 categories with touch normalization and automotive-readable font sizes. v0.5.3 shipped widget grid & content widgets — Android-style freeform grid with drag/resize/multi-page, AA navigation turn-by-turn widget, unified now playing widget with AA/BT source priority. v0.6 shipped architecture formalization — typed provider interfaces, core-owned services, legacy Configuration removal, SettingsInputBoundary for settings touch input, 82 unit tests. v0.6.1 shipped widget framework refinement — WidgetDescriptor with size constraints and categories, DPI-based grid sizing with auto-snap, launcher dock replaced by singleton widgets, formalized widget contract (span-based breakpoints, context injection, ActionRegistry egress), all 6 widgets rewritten, developer guide and 15 ADRs, 85 unit tests all passing. v0.6.2 shipped theme expression & wallpaper scaling — wallpaper memory/render hardening, companion reconnect fix, theme persistence, M3 color audit with night comfort guardrail, 9 companion-created themes promoted to bundled defaults with Prodigy as first-install identity, FullScreenPicker GPU fix. v0.6.3 shipped proxy routing fix — root daemon with restricted IPC socket, idempotent iptables routing with self-exemption via dedicated redsocks user, truthful ACTIVE/FAILED/DEGRADED status reporting, startup self-heal, end-to-end hardware validation proving companion SOCKS bridge traversal. v0.6.4 shipped widget work — per-instance widget configuration with schema-driven config sheets, 6 new widgets (theme cycle, battery, companion status, AA focus toggle, clock styles, weather), WeatherService with Open-Meteo integration and subscriber-aware caching. v0.6.5 shipped widget refinement — date widget split from clock, custom Canvas battery gauge, all widgets reworked for automotive-sized scaling, Material Symbols font updated with corrected codepoints, AAStatusWidget removed, Now Playing reworked with scrolling titles and progressive breakpoints. v0.6.6 shipped homescreen layout & widget settings rework — replaced global edit mode with Android-style per-widget long-press selection, navbar gear/trash transformation with tap-only enforcement, 4-edge resize handles with atomic API, bottom-sheet widget picker with categorized list, long-press empty space popup menu, all FABs and badge buttons removed, overlay lifecycle cleanup for no-selection states. Codebase is ~71K LOC C++/QML + ~2K LOC Python system service, 88 unit tests.

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

| WidgetGridModel replaces WidgetPlacementModel (not extension) | Pane model can't represent grid coords | ✓ Good |
| Content widgets before edit mode | Delivers user value earlier | ✓ Good |
| Ghost rectangle for resize preview | Animating width/height is janky on Pi | ✓ Good |
| Drag overlay MouseArea at z:25 | Consistent drag across interactive/static widgets | ✓ Good |
| PageIndicator dots disabled in edit mode | Page-scoped editing makes mid-edit page switch confusing | ✓ Good |
| No maximum page limit | User decision — uncapped is fine for solo use | ✓ Good |
| YAML schema v3 with page field | v2 auto-migrates with page=0 default | ✓ Good |

| Architecture first, launcher second | Widget-based launcher (v0.7) consumes the architecture (v0.6) — avoids rework | ✓ Good |
| BT Audio / Phone deferred | Redundant when AA connected; decide fate after architecture is solid | — Pending |
| DPI-based cellSide formula (diagPx / (9.0 + bias * 0.8)) | Resolution-independent grid sizing; DPI cascade is scaffolding for future mm-based path | ✓ Good |
| Two-pass auto-snap threshold (0.5) | Recovers gutter waste without iterative packing; cascade guard prevents runaway | ✓ Good |
| Singleton launcher widgets replace dock | Reclaims vertical space, unified widget model, no separate LauncherDock code | ✓ Good |
| WidgetContextFactory as dedicated class | Keeps WidgetGridModel pure data; factory owns IHostContext + cell geometry | ✓ Good |
| Context injection via Loader.onLoaded + Binding | Typed, documented, NOTIFY-capable — not context properties | ✓ Good |
| Span-based breakpoints (not pixel thresholds) | Resolution-independent widget layouts; colSpan >= N not width > 300 | ✓ Good |
| ActionRegistry for widget command egress | Widgets dispatch via named actions, not raw PluginModel/ApplicationController | ✓ Good |
| promoteToBase on every mutation including opacity | Remap derives from base; all user edits must be preserved | ✓ Good |

| Night comfort guardrail (HSL sat clamp 0.55) | Simpler than HCT chroma, Qt-native, companion handles proper HCT | ✓ Good |
| Solid primaryContainer fills (not opacity overlays) | True M3 token pairing — onPrimaryContainer needs solid background | ✓ Good |
| Companion-created themes as bundled defaults | M3 color math from companion app, not hand-crafted YAML | ✓ Good |
| Prodigy theme as default ID | App identity theme, loads on fresh install | ✓ Good |
| FullScreenPicker no delegate shadows | Pi 4 GPU freezes with 9+ MultiEffect layers | ✓ Good |
| State matrix doc as color truth | Single source for control→token assignments, prevents audit drift | ✓ Good |
| Root daemon (not capabilities-only) | iptables + service restarts + rfkill too broad for capabilities without splitting | ✓ Good |
| Dedicated redsocks user for owner-based exemption | Root exemption punches giant hole in redirect policy | ✓ Good |
| Flush/recreate iptables model (not check-and-reuse) | Deterministic state, parameters may change, stale rules must not persist | ✓ Good |
| TCP connect as listener truth gate (not PID) | PID can exist without listener — only TCP connect proves redsocks is ready | ✓ Good |
| Local failure = immediate FAILED (no threshold) | Thresholds make sense for noisy remote probes, not local listener checks | ✓ Good |
| IPv6-mapped address stripping in IPC validator | Qt dual-stack peerAddress() returns ::ffff: prefix that breaks redsocks config | ✓ Good |
| skip_interfaces ["lo"] only (not ["lo", "eth0"]) | eth0 is default route — exempting it bypasses all REDIRECT rules | ✓ Good |
| Schema validation drops unknown keys silently | Defaults fill gaps via effectiveConfig merge — strict rejection would break forward compat | ✓ Good |
| Config schema exposed as QVariantList (not model) | Avoids dead-weight C++ QAbstractListModel for a one-shot config sheet UI | ✓ Good |
| Subscriber list per location stores individual intervals | Enables effective interval computation (shortest among active subscribers) | ✓ Good |
| Single 1-minute timer tick for weather refresh | Avoids per-location QTimers; tick checks per-location intervals | ✓ Good |
| Open-Meteo (no API key) over OpenWeatherMap | Free, no signup, sufficient for current conditions — alerts deferred | ✓ Good |
| Clock styles via Loader Component switching | Shared root-level time properties, clean separation of style rendering | ✓ Good |
| Per-widget selection (not global edit mode) | Android-style individual widget interaction, no all-widgets-edit simultaneously | ✓ Good |
| selectedInstanceId in QML (not C++) | Avoids split-brain state; C++ only gets thin remap gate via setWidgetSelected(bool) | ✓ Good |
| selectionTapInterceptor at z:15 covering ALL delegates | Prevents launcher widget navigation escape; interceptor owns drag gesture for Pi touch | ✓ Good |
| widgetInteractionMode on NavbarController (not re-registration) | controlRole() mode-aware dispatch; ActionRegistry naturally dispatches gear/trash roles | ✓ Good |
| Tap-only enforcement for gear/trash (pressedInWidgetMode flag) | Automotive touchscreen in bumpy cars produces >200ms presses — force Tap regardless of duration | ✓ Good |
| resizeWidgetFromEdge atomic API | Sequential move+resize has invalid intermediate state; atomic validation prevents it | ✓ Good |
| Popup menu width from text implicitWidth + chrome | Scales with DPI/font size instead of hardcoded pixel cap | ✓ Good |
| widgetDeselectedFromCpp signal for C++ → QML deselect bridge | 7 navigation actions call setWidgetSelected(false) in C++; signal syncs QML selectedInstanceId | ✓ Good |
| _skipPageCleanup flag for PGM-04 race prevention | removePage shifts indices; deferred cleanup in deselectWidget would iterate wrong pages | ✓ Good |

---
*Last updated: 2026-03-22 after v0.6.6 milestone*
