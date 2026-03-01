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
- ✓ Interactive installer for RPi OS Trixie (hardware detection, hostapd, labwc, services) — existing
- ✓ Prebuilt release distribution workflow — existing
- ✓ Web config panel (Flask + Unix socket IPC) — existing
- ✓ Protocol capture logging (jsonl/tsv) — existing
- ✓ Cross-compilation toolchain (Pi 4 aarch64) — existing
- ✓ 47 unit tests covering core systems — existing

### Active

<!-- Current scope. Building toward these. -->

- [ ] HFP call audio persists across AA connection state (calls don't drop if AA disconnects)
- [ ] Audio equalizer with presets and custom profiles (head unit UI + web config backend)
- [ ] Dynamic sidebar/video reconfiguration during active AA session
- [ ] Logging cleanup — quiet default output, debug toggle
- [ ] Theme selection — user-pickable color palettes + wallpaper selection
- [ ] Release-quality stability — daily driver reliable without manual intervention

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

The project has been developed organically over multiple sessions with significant progress — the core AA experience works end-to-end. What's needed now is structured milestone planning to close the remaining gaps and reach a tagged v1.0 release that others can install and use.

Existing codebase is substantial (~15K+ lines C++, ~3K QML, installer, web panel, 47 tests). Most AA protocol work is done. Remaining work is primarily feature completion (HFP audio, EQ, sidebar), polish (themes, logging), and release hardening.

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
| Color palette + wallpaper themes (not full engine) | Right scope for v1.0 — full themes are post-release | — Pending |

---
*Last updated: 2026-03-01 after initialization*
