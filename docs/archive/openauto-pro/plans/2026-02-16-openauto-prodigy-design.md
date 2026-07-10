# OpenAuto Prodigy — Design Document

**Date:** 2026-02-16
**Author:** Matthew Mees + Claude
**Status:** Approved

## Mission

Build a complete, open-source replacement for OpenAuto Pro — the Raspberry Pi Android Auto head unit application abandoned by BlueWave Studio. The goal is full feature parity with OAP v16.1, built on a modern stack, for the community of stranded OAP users.

This is a clean-room rebuild. The recovered QML files, protobuf API, icons, and manual serve as a detailed specification. We write fresh code that does the same things better.

## Project Identity

- **Name:** OpenAuto Prodigy (OAP)
- **License:** GPLv3
- **Repository:** `github.com/mrmees/openauto-prodigy` (to be created)
- **Tagline:** "The student that surpassed the master."

## Tech Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Language | C++17 | Modern, broad compiler support on Pi |
| UI Framework | Qt 6.8 (Qt Quick / QML) | Ships in RPi OS Trixie apt, hardware-accelerated, actively maintained |
| AA Protocol | SonOfGib's aasdk fork (submodule) | Working AA protocol with WiFi fix, GPLv3 |
| Build System | CMake | Standard for Qt 6 projects |
| Target OS | Raspberry Pi OS Trixie (Debian 13, 64-bit) | Current default, Qt 6.8.2 in apt, Linux 6.12 LTS |
| Target Hardware | Raspberry Pi 4 (primary), Pi 5 (secondary) | Most common OAP hardware |
| Audio | PipeWire | Modern, replaces PulseAudio, default on Trixie |
| Bluetooth | BlueZ 5 via D-Bus | Standard Linux BT stack |
| Config Format | INI files | Backward-compatible with OAP's `openauto_system.ini` |

### Why These Choices

**Qt 6 over Qt 5:** OAP used Qt 5.11, which is ancient and unsupported. Qt 6.8 has better GPU support on Pi 4, actively maintained QML, and Trixie ships it natively. The QML API is largely backward-compatible — differences are well-documented for porting.

**Trixie over Bookworm:** Trixie is the current RPi OS default (released Oct 2025). Building for Bookworm means starting outdated. Trixie ships Qt 6.8.2, Linux 6.12 LTS, and a full Qt 6 / KDE Frameworks 6 desktop stack.

**64-bit only:** Pi 4 runs 64-bit natively with significant performance gains. Trixie defaults to 64-bit. No reason to support 32-bit for new development.

**Clean-room rebuild over forking:** The recovered QML files have unnamed components, hardcoded values, and BlueWave-specific hacks. Fresh QML with clean component architecture, responsive layouts, and proper separation of concerns will be more maintainable. The recovered files are the spec, not the code.

**Independent over building on opencardev/dash:** While opencardev/openauto and openDsh/dash are excellent projects, we want full architectural control. They serve as reference implementations, not foundations. Our differentiator is the OAP-style UI, plugin API compatibility, and the specific feature set OAP users expect.

## Architecture

### Repository Structure

```
openauto-prodigy/
├── libs/
│   └── aasdk/              # SonOfGib's aasdk (git submodule)
├── src/
│   ├── core/               # Core library
│   │   ├── aa/             # Android Auto service layer
│   │   ├── audio/          # Audio manager, music player, equalizer
│   │   ├── bt/             # Bluetooth manager (A2DP, HFP, PBAP)
│   │   ├── mirror/         # Screen mirroring (ADB-based)
│   │   ├── obd/            # OBD-II gauges, ELM327
│   │   ├── api/            # Plugin API server (TCP + protobuf)
│   │   ├── system/         # GPIO, sensors, day/night, hotspot
│   │   └── hw/             # Camera (V4L2)
│   ├── ui/                 # QML context objects, theme engine
│   └── main.cpp            # Application entry point
├── qml/                    # All QML UI files
├── resources/              # SVG icons, default wallpapers, splash
├── proto/                  # Protobuf definitions (plugin API)
├── config/                 # Default INI configs
├── systemd/                # Service files, watchdog, splash
├── cmake/                  # CMake modules, toolchain files
├── tests/                  # Unit and integration tests
├── docs/                   # Design docs, user guide
├── original/               # Recovered OAP assets (reference only)
│   ├── qml/                # 162 recovered QML files
│   ├── images/             # 88 SVG icons
│   ├── scripts/            # 7 utility scripts
│   └── proto/              # Recovered Api.proto
└── CMakeLists.txt
```

### Core Modules

| Module | Responsibility | Key Dependencies |
|--------|---------------|------------------|
| `aa::Service` | Android Auto wired + wireless connection, video/audio/input | aasdk, libusb, OpenSSL |
| `audio::Manager` | PipeWire mixing, volume control, equalizer | PipeWire, LADSPA |
| `audio::MusicPlayer` | Local music playback, library scanning, metadata | Qt Multimedia, taglib |
| `bt::Manager` | BT pairing, A2DP streaming, HFP calls, PBAP phonebook | BlueZ 5 D-Bus |
| `mirror::Service` | ADB-based screen mirroring from Android devices | scrcpy / custom ADB |
| `obd::Service` | OBD-II gauge data, ELM327 interface, custom formulas | Serial / BT OBD |
| `api::Server` | Plugin API — TCP socket, protobuf messages, subscriptions | protobuf |
| `system::Manager` | GPIO, I2C sensors, day/night mode, WiFi hotspot | gpiod, I2C |
| `hw::Camera` | Rear camera view, V4L2 capture, GPIO trigger | V4L2, Qt Multimedia |
| `ui::Manager` | QML context properties, theme engine, app lifecycle | Qt Quick |

### Data Flow

```
Phone ──USB/WiFi──> aasdk ──> aa::Service ──> ui::Manager ──> QML UI
                                   │                             ▲
                                   v                             │
                             audio::Manager <── bt::Manager ─────┘
                                   │
                             api::Server <──── External Plugins (TCP)
```

### Plugin API

100% backward-compatible with BlueWave's published API:
- **Protocol:** TCP/IP (not Unix socket)
- **Message format:** 32-bit size (LE) + 32-bit message ID (LE) + 32-bit flags (LE) + protobuf payload
- **API version:** 1.1 (matching OAP)
- **Reference:** `github.com/bluewave-studio/openauto-pro-api` (proto file + 16 Python examples)

Existing OAP plugins and integrations should work without modification.

### Configuration

Backward-compatible INI files:
- `openauto_system.ini` — all system settings (day/night colors, sensor config, hotspot, etc.)
- `openauto_applications.ini` — external app launcher entries (up to 8 apps)

Users migrating from OAP can drop in their existing config files.

## Feature Phases

### Phase 1 — Skeleton & Android Auto

The core reason people use OAP. This phase delivers a bootable head unit that does the primary job.

- Qt 6 Quick application boots on Pi 4, displays QML UI
- Main screen with navigation (top bar, bottom bar, content area)
- Wired Android Auto (USB) via aasdk
- Wireless Android Auto (WiFi) — SonOfGib's fix baked in from day one
- Audio output through PipeWire
- Day/night theme engine (manual toggle)
- Config loading from INI files
- Settings framework (AA settings: audio, video, BT adapter, system)

### Phase 2 — Audio & Bluetooth

The second most-used features in a car head unit.

- Bluetooth pairing and device management via BlueZ D-Bus
- A2DP music streaming
- HFP hands-free calling
- PBAP phonebook and call history access
- Local music player (library scan, playback, metadata, cover art)
- Equalizer (PipeWire filter chain or LADSPA)
- Volume and brightness controls in top bar

### Phase 3 — System Integration

Everything that makes it feel like a complete head unit, not just an AA box.

- Plugin API server (TCP + protobuf, full BlueWave API compatibility)
- External app launcher (8 apps, INI config, background/foreground management)
- Rear camera (V4L2, GPIO trigger, guide lines, orientation)
- Day/night auto-switching (TSL2561 I2C sensor, GPIO, clock-based)
- DS18B20 temperature sensor (1-Wire)
- WiFi hotspot management
- Keyboard controls and input mapping (iDrive, IBus, MMI 2G)

### Phase 4 — Extended Features

Lower-priority features and community requests.

- Screen mirroring (ADB-based, Android devices)
- OBD-II gauges and dashboards (ELM327, custom formulas)
- FM radio (RTL-SDR) — evaluate if worth including in 2026
- Wallpaper settings (stretch/fit/crop, opacity)
- Custom splash screen
- Community-requested features

Each phase is independently testable on real hardware. Phase 1 is the critical milestone — if Android Auto works with the familiar UI, people will adopt it even without the rest.

## Development Workflow

- **Primary dev machine:** MINIMEES (AMD Ryzen 7 9700X, Windows 10, WSL2 available)
- **Cross-compilation:** CMake toolchain for x86_64 -> aarch64, build on MINIMEES, deploy to Pi
- **Desktop testing:** Qt 6 QML runs on x86 with dummy backends for UI development
- **Hardware testing:** Deploy to Pi 4 over network (scp/rsync) for integration testing
- **CI:** GitHub Actions — build check on push, cross-compile validation
- **Test hardware:** Raspberry Pi 4 with DFRobot 7" 1024x600 capacitive touchscreen (HDMI + USB)

## Distribution

- `.deb` package (apt installable) for users with existing RPi OS Trixie
- Pre-built disk image for fresh installs (flash and go)
- Systemd service for auto-start, watchdog, splash screen

## Reference Materials

### Recovered from OAP v16.1
- 162 QML files (complete UI specification) — `original/qml/`
- 34-message protobuf API definition — `original/proto/Api.proto`
- 88 SVG icons — `original/images/`
- 7 utility scripts — `original/scripts/`
- User manual (43 pages) — `oap_manual_1.pdf`

### Upstream / Reference Projects
| Project | URL | Use |
|---------|-----|-----|
| f1xpl/aasdk | `github.com/f1xpl/aasdk` | Original AA SDK (frozen 2018, reference) |
| f1xpl/openauto | `github.com/f1xpl/openauto` | Original app (frozen 2018, reference) |
| SonOfGib/aasdk | `github.com/SonOfGib/aasdk` | AA SDK with WiFi fix (our submodule) |
| SonOfGib/openauto | `github.com/SonOfGib/openauto` | App with WiFi handler (reference) |
| opencardev/openauto | `github.com/opencardev/openauto` | Actively maintained openauto fork (reference) |
| openDsh/dash | `github.com/openDsh/dash` | Full infotainment UI on openauto (reference) |
| bluewave-studio/openauto-pro-api | `github.com/bluewave-studio/openauto-pro-api` | Official plugin API (proto + examples) |

### Reverse Engineering
- `docs/reverse-engineering-notes.md` — binary analysis, offsets, namespace structure
- `docs/android-auto-fix-research.md` — AA 12.7+ bug root cause and fix
- `docs/abi-compatibility-analysis.md` — library compatibility analysis
- Ghidra project with libaasdk.so loaded — WirelessServiceChannel analysis in progress
