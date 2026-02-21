# OpenAuto Prodigy

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Open-source replacement for OpenAuto Pro — a Raspberry Pi-based Android Auto head unit application.

**Status: v0.3.0 — Wireless AA with audio, settings, and plugin architecture**

## What Is This?

[OpenAuto Pro](https://bluewavestudio.io/) was a commercial Android Auto head unit app for Raspberry Pi by BlueWave Studio. The company went defunct, leaving users with no updates and no path forward. OpenAuto Prodigy is a clean-room rebuild using recovered QML layouts and protobuf definitions as specifications — not copying code.

## Features

- **Wireless Android Auto** — BT discovery → WiFi AP → TCP (no USB)
- **H.264 video** at 1280x720 @ 30fps (FFmpeg software decode)
- **Multi-touch input** via direct evdev (auto-detected, bypasses Qt/Wayland)
- **Audio pipeline** — PipeWire with lock-free ring buffers for AA media/nav/phone streams
- **Audio device selection** — hot-plug USB audio detection, output/input device selection in Settings
- **Plugin architecture** — lifecycle-managed plugins with shared services via IHostContext
- **Bluetooth audio** — A2DP sink + AVRCP controls via BlueZ D-Bus
- **Phone** — HFP dialer + incoming call overlay via BlueZ D-Bus
- **Settings UI** — Audio, Display, Connection, Video, System, About pages
- **Theme system** — day/night mode with YAML-based color definitions
- **YAML configuration** — deep merge, hot-reload, web config panel (Flask)
- **3-finger gesture overlay** — volume, brightness, home, day/night toggle
- **Connection watchdog** — TCP_INFO polling detects dead peers on local WiFi AP
- **Graceful AA shutdown** — sends ShutdownRequest to phone before exit/restart

## Architecture

```
src/
├── main.cpp                            # Entry point, QML engine, plugin registration
├── core/
│   ├── YamlConfig.hpp/cpp              # YAML config with deep merge
│   ├── InputDeviceScanner.hpp/cpp      # Touch device auto-detection
│   ├── plugin/                         # Plugin infrastructure (IPlugin, PluginManager, Discovery)
│   ├── services/
│   │   ├── AudioService.hpp/cpp        # PipeWire stream management + ring buffer bridge
│   │   ├── PipeWireDeviceRegistry.hpp/cpp  # Hot-plug device enumeration
│   │   ├── ThemeService.hpp/cpp        # Day/night theme colors
│   │   ├── ConfigService.hpp/cpp       # QML-accessible config read/write
│   │   └── IpcServer.hpp/cpp           # Unix socket IPC for web config
│   └── aa/
│       ├── AndroidAutoService.hpp/cpp  # AA lifecycle, TCP transport, BT integration
│       ├── AndroidAutoEntity.hpp/cpp   # Protocol entity, control channel, handshake
│       ├── VideoDecoder.hpp/cpp        # FFmpeg H.264 → QVideoFrame
│       ├── EvdevTouchReader.hpp/cpp    # Direct evdev multi-touch + 3-finger gesture
│       └── TouchHandler.hpp/cpp        # Touch → AA protobuf bridge
├── plugins/
│   ├── android_auto/                   # AA plugin (video, touch, audio channels)
│   ├── bt_audio/                       # Bluetooth A2DP + AVRCP via BlueZ D-Bus
│   └── phone/                          # HFP dialer + incoming call overlay
└── ui/
    ├── ApplicationController.hpp/cpp   # Navigation, restart, quit
    ├── PluginModel.hpp/cpp             # QAbstractListModel for nav strip
    └── AudioDeviceModel.hpp/cpp        # Device selection for QML ComboBoxes
qml/
├── components/                         # Shell, TopBar, BottomBar, NavStrip, GestureOverlay
├── controls/                           # Tile, Icon, SettingsSlider, SettingsToggle, etc.
└── applications/                       # Plugin views (launcher, android_auto, settings, etc.)
libs/
└── aasdk/                              # SonOfGib's aasdk fork (git submodule)
tests/                                  # 17 unit tests
web-config/                             # Flask web config panel
```

## Target Platform

- **Hardware:** Raspberry Pi 4 (primary), Pi 5 (secondary)
- **OS:** RPi OS Trixie (Debian 13, 64-bit)
- **Display:** DFRobot 7" 1024x600 touchscreen, Qt 6.8 + Wayland (labwc)
- **AA Protocol:** [SonOfGib's aasdk fork](https://github.com/nicka-2) (git submodule)

## Building

See [docs/development.md](docs/development.md) for full setup instructions including dependencies and platform-specific notes.

### Quick Start (Raspberry Pi OS Trixie)

```bash
# Install dependencies
sudo apt install cmake qt6-base-dev qt6-declarative-dev qt6-wayland \
  qt6-connectivity-dev qt6-multimedia-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript \
  libboost-system-dev libboost-log-dev \
  libprotobuf-dev protobuf-compiler libssl-dev \
  libavcodec-dev libavutil-dev \
  libpipewire-0.3-dev \
  hostapd

# Build
git clone --recurse-submodules https://github.com/mrmees/openauto-prodigy.git
cd openauto-prodigy
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run (Wayland)
export XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 QT_QPA_PLATFORM=wayland
./src/openauto-prodigy
```

### Tests

```bash
cd build && ctest --output-on-failure  # 17 tests
```

## Key Design Decisions

- **FFmpeg software decode** over V4L2 hardware (kernel 6.x regression, see [docs/design-decisions.md](docs/design-decisions.md))
- **QVideoSink + VideoOutput** for rendering (canonical Qt 6 approach)
- **YAML configuration** with deep merge and hot-reload via ConfigService
- **Wireless-only** — USB AOAP never reliably opened service channels; wireless works first try
- **5GHz WiFi AP** — mandatory for Pi 4/5 due to BT/WiFi coexistence on shared CYW43455 antenna
- **PipeWire C API** with lock-free SPSC ring buffers bridging ASIO threads to PipeWire RT thread
- **Direct evdev** for touch input — Qt/Wayland touch stack causes duplicate events and breaks multi-touch

## Documentation

See [docs/INDEX.md](docs/INDEX.md) for a map of all project documentation.

## Related Resources

- [openauto-pro-community](https://github.com/mrmees/openauto-pro-community) — Recovered specs, QML, protos, icons, and research
- [aasdk (SonOfGib)](https://github.com/nicka-2) — Android Auto protocol SDK
- [uglyoldbob/android-auto](https://github.com/uglyoldbob/android-auto) — Rust AA implementation (useful protocol reference)
- [f1xpl/openauto](https://github.com/nicka-2/openauto) — Original open-source AA implementation

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.
