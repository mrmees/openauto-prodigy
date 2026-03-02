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

### Active

<!-- Current scope. Building toward these. -->

## Current Milestone: v0.4.3 Interface Polish & Settings Reorganization

**Goal:** Redesign the entire UI with an automotive-minimal aesthetic and reorganize settings into logical, domain-specific categories.

**Target features:**
- Settings reorganization into 6 top-level categories: Android Auto, Display, Audio (w/ EQ), Connectivity, Companion, System/About
- Automotive-minimal visual redesign across entire app (settings, launcher, nav strip, top bar, modals)
- Consistent iconography with minimal whitespace around icons for glanceability
- UX touch improvements (BT device management, modal dismiss, button feedback, read-only field clarity)
- EQ accessible both in Audio settings and via nav strip shortcut

**Deferred to future milestones:**
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
- Multi-display / resolution support — 1024x600 target only for v1.0
- Custom icon sets / full theme engine — color palettes + wallpapers are sufficient for v1.0

## Context

OpenAuto Pro (BlueWave Studio) was a commercial Pi-based AA head unit that went defunct. This project is a clean-room rebuild — no OAP code, no aasdk dependency. The protocol library (`open-android-auto`) is maintained as a separate community resource.

v0.4 shipped logging and theming. v0.4.1 shipped 10-band graphic EQ with per-stream profiles. v0.4.2 shipped service hardening — WiFi AP, Bluetooth SDP, systemd ordering, and clean shutdown all work reliably without manual intervention. Codebase is ~20K+ lines C++ across 170+ files, ~3K QML, installer, web panel, 57 tests. v0.4.3 focuses on full UI refresh with automotive-minimal styling and settings reorganization.

## Constraints

- **Hardware:** Raspberry Pi 4, 1024x600 DFRobot touchscreen, built-in WiFi/BT
- **Stack:** Qt 6.8 + QML, C++17, CMake, PipeWire, BlueZ D-Bus — locked
- **Dev platform:** Qt 6.4 on Ubuntu VM (no Bluetooth) + Qt 6.8 on Pi (full features)
- **Solo maintainer:** Process must stay lightweight; no heavy CI/review gates
- **No GPU on dev VM:** GPU-dependent work (DMA-BUF, hw decode) tested on Pi only
- **Submodule:** `libs/open-androidauto/proto/` is read-only — managed separately

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

---
*Last updated: 2026-03-02 after v0.4.3 milestone start*
