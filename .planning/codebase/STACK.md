# Technology Stack

**Analysis Date:** 2025-03-01

## Languages

**Primary:**
- C++ 17 — Core application, plugin system, AA protocol, audio/video pipelines
- QML — Qt Quick UI (components, screens, plugin views)
- Python 3 — Web config server (Flask), system service (dbus-next)
- Bash — Installation and deployment scripting

**Secondary:**
- YAML — Configuration files (device config, themes)
- Protocol Buffers — Android Auto message definitions (in submodule)
- HTML/CSS/JavaScript — Web config panel templates
- INI — Legacy configuration format (openauto_system.ini)

## Runtime

**Environment:**
- Qt 6 — Core framework for UI, networking, IPC, multimedia
  - Qt 6.4 (Ubuntu 24.04 dev VM — `claude-dev`)
  - Qt 6.8 (RPi OS Trixie target — full Bluetooth support)
- GCC/G++ aarch64-linux-gnu — Cross-compilation toolchain for Pi 4
- Python 3.x — Web config and system service

**Package Manager:**
- CMake 3.22+ — Build system, dependency management, Qt integration
- pip — Python dependencies (Flask, dbus-next, PyYAML)
- apt — Linux packages (Pi OS Trixie, Ubuntu dev)

**Build Tool Chain:**
- protoc (Protocol Buffers compiler) — AA protocol message generation
- Qt MOC (Meta-Object Compiler) — Qt signal/slot reflection
- Qt RCC (Resource Compiler) — QML embedding, images
- Qt syncqt — Cross-compilation QML type discovery

## Frameworks

**Core:**
- Qt 6 (Core, Gui, Quick, QuickControls2, Multimedia, Network, DBus)
  - QML engine for UI rendering
  - QVideoSink/QVideoFrame for video playback
  - QLocalServer/QLocalSocket for IPC
  - QDBusConnection for BlueZ D-Bus integration
  - QAudioEngine (deprecated) — replaced by PipeWire

**Protocol & Messaging:**
- Protocol Buffers 3 — Android Auto protocol definitions
- OpenSSL (libssl) — TLS encryption for AA transport

**Testing:**
- Qt Test Framework — Unit tests (Qt6::Test)
- CMake test runner (CTest) — 47 tests covering config, plugins, theme, audio, events, etc.

**Build/Dev:**
- CMake — Project configuration and build orchestration
- pkg-config — System library discovery (FFmpeg, PipeWire, yaml-cpp)
- Docker — Cross-compilation environment for Pi

## Key Dependencies

**Critical:**
- libpipewire-0.3 — Audio subsystem integration, device enumeration, RT audio I/O
  - Replaces PulseAudio on modern Trixie installations
  - SPA (Simple Plugin API) headers for ring buffer manipulation
- libavcodec / libavutil — Multi-codec video decode (H.264, H.265, VP9, AV1)
  - FFmpeg 5/6 compatibility, hardware decode auto-selection
  - Worker thread decode with frame pooling
- yaml-cpp — YAML config file parsing and generation
  - Deep merge support for theme/device configuration
- libssl / libcrypto — OpenSSL for AA protocol TLS frames
- libprotobuf — Protocol Buffer message serialization

**Infrastructure:**
- Qt6 Bluetooth (optional, Pi only) — qt6-connectivity-dev for BlueZ adapter detection
  - Not available on dev VM (no Bluetooth hardware)
  - Guarded by `#ifdef HAS_BLUETOOTH` compiler flag
- Qt6 DBus — BlueZ service integration (A2DP, AVRCP, HFP)
  - Direct D-Bus method calls to org.bluez.* interfaces
- zlib (via Qt) — Compression for protocol frames
- ICU (via Qt) — Unicode text handling

**Web Config:**
- Flask 3.0+ — Lightweight Python web server
  - Routes: dashboard, settings, themes, audio, plugins
  - JSON request/response to Qt app via Unix socket

**System Service:**
- dbus-next 0.2.3+ — Async Python D-Bus client library
  - BlueZ ProfileManager1 registration
  - Systemd unit control via D-Bus
- PyYAML 6.0+ — Config parsing in system service

## Configuration

**Environment:**
- `.openauto/config.yaml` — Primary device config (WiFi, Bluetooth, device name, ports)
  - Location: `$HOME/.openauto/config.yaml`
  - YAML structure with deep merge override capability
- `.openauto/themes/default/theme.yaml` — Day/night color themes
- `openauto_system.ini` — Legacy INI format (fallback compatibility)
  - Located at `src/config/openauto_system.ini` in source tree

**Build:**
- `CMakeLists.txt` — Main project configuration
- `toolchain-pi4.cmake` — Cross-compilation settings (sysroot, target flags)
- `docker/Dockerfile.cross-pi4` — Cross-compiler container definition
- `src/CMakeLists.txt` — Openauto-core static library + executable configuration
- `libs/open-androidauto/CMakeLists.txt` — AA protocol library build
- `tests/CMakeLists.txt` — Test executable registration

**Runtime Flags:**
- `OAP_SKIP_QML_PLUGIN_IMPORT` — Disable static QML plugin imports (recommended for distro Qt)
- `BUILD_TESTING` — Enable/disable test build (default: ON)
- `CMAKE_TOOLCHAIN_FILE` — Path to cross-compilation toolchain
- `HAS_BLUETOOTH` — Conditional Bluetooth support (set by Qt find_package)

**Environment Variables:**
- `OAP_IPC_SOCKET` — Unix socket path for web config IPC (default: `/tmp/openauto-prodigy.sock`)
- `OAP_GITHUB_API_URL` — GitHub API endpoint for release asset lookup (installer only)
- `WAYLAND_DISPLAY` — Pi set to `wayland-0` by labwc compositor
- `Qt*_PATH` — Qt module paths (set by toolchain file for cross-compilation)

## Platform Requirements

**Development:**
- Ubuntu 24.04 LTS (Noble) or equivalent
- Qt 6.4+ with full dev packages (Core, Gui, Quick, QuickControls2, Multimedia, Network, DBus, Test)
- CMake 3.22+
- GCC/G++ 12+
- Python 3.10+ (for web config, system service)
- FFmpeg dev libraries (libavcodec, libavutil)
- PipeWire dev libraries
- yaml-cpp dev libraries
- OpenSSL dev libraries
- Protocol Buffers compiler + dev libraries

**Production (Pi):**
- Raspberry Pi 4 (3GB+ RAM recommended)
- RPi OS Trixie (Debian 13) — tested and supported
- Qt 6.8 (from RPi OS repo — no backport needed)
- PipeWire (default audio on Trixie, replaces PulseAudio)
- BlueZ 5.68+ — Bluetooth stack
- labwc 0.7+ — Wayland compositor
- systemd — Service management
- hostapd — WiFi AP mode
- iw — Wireless device configuration
- ca-certificates — HTTPS validation for GitHub API
- Python 3.11+ (system service daemon)
- Flask + dependencies via pip (web config)

**Optional:**
- Docker — Cross-compilation of Pi binaries on dev VM
- Ghidra — Binary reverse engineering (reference only, not part of build)
- ccache — Accelerate rebuilds (optional, enabled in installer)

---

*Stack analysis: 2025-03-01*
