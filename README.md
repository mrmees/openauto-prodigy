# OpenAuto Prodigy

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Open-source replacement for OpenAuto Pro — a Raspberry Pi-based Android Auto head unit application.

**Status: Phase 1 — Core AA Functionality (In Progress, Wireless-Only)**

## What Is This?

[OpenAuto Pro](https://bluewavestudio.io/) was a commercial Android Auto head unit app for Raspberry Pi by BlueWave Studio. The company went defunct, leaving users with no updates and no path forward. OpenAuto Prodigy is a clean-room rebuild using recovered QML layouts and protobuf definitions as specifications — not copying code.

## Phase 1 Progress

| # | Task | Status |
|---|------|--------|
| 1 | Repository & build system | Done |
| 2 | Configuration system | Done |
| 3 | Theme engine | Done |
| 4 | QML UI shell | Done |
| 5 | AA service layer (TCP, handshake, 8 channels) | Done |
| 6 | Video pipeline (FFmpeg H.264 → QVideoSink) | Code complete, untested |
| 7 | Audio pipeline | Pending |
| 8 | Wireless AA (BT discovery + WiFi AP + TCP) | Code complete, untested |
| 9 | Settings UI | Pending |
| 10 | Integration & polish | Pending |

### Wireless-Only Architecture
USB transport was abandoned after service channels consistently failed to open despite successful handshakes. The project now uses wireless-only AA: Bluetooth discovery → WiFi credential exchange → TCP on port 5288. See [docs/wireless-setup.md](docs/wireless-setup.md) for Pi setup and [PICKUP.md](PICKUP.md) for full project state.

## Architecture

```
src/
├── main.cpp                          # Entry point, QML engine setup
├── core/
│   ├── Configuration.hpp/cpp         # INI config (backward-compatible with OAP)
│   └── aa/
│       ├── AndroidAutoService.hpp/cpp  # AA lifecycle, TCP transport, BT integration
│       ├── AndroidAutoEntity.hpp/cpp   # Protocol entity, control channel, handshake
│       ├── ServiceFactory.hpp/cpp      # Creates all 8 service instances
│       ├── VideoService.hpp/cpp        # H.264 data → Qt main thread
│       ├── VideoDecoder.hpp/cpp        # FFmpeg H.264 → QVideoFrame (YUV420P)
│       └── IService.hpp               # Service interface
├── ui/
│   ├── ThemeController.hpp/cpp       # Day/night theme, QML context property
│   ├── ApplicationController.hpp/cpp # Navigation stack
│   └── ApplicationTypes.hpp          # App type enum
qml/
├── main.qml                          # Root window
├── applications/
│   ├── android_auto/AndroidAutoMenu.qml  # VideoOutput + status overlay
│   ├── home/HomeMenu.qml
│   ├── launcher/LauncherMenu.qml
│   └── settings/SettingsMenu.qml
├── components/                       # TopBar, BottomBar, Clock, Wallpaper
└── controls/                         # Icon, Tile, NormalText, SpecialText
libs/
└── aasdk/                            # SonOfGib's aasdk fork (git submodule)
tests/
├── test_configuration.cpp            # 2 tests passing
└── test_theme_controller.cpp
```

## Target Platform

- **Hardware:** Raspberry Pi 4 (primary), Pi 5 (secondary)
- **OS:** RPi OS Trixie (Debian 13, 64-bit)
- **Display:** Qt 6.8 + Wayland (labwc)
- **AA Protocol:** [SonOfGib's aasdk fork](https://github.com/nicka-2) (git submodule)

## Building

See [docs/development.md](docs/development.md) for full setup instructions including dependencies and platform-specific notes.

### Quick Start (Raspberry Pi OS Trixie)

```bash
# Install dependencies
sudo apt install cmake qt6-base-dev qt6-declarative-dev qt6-wayland \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript \
  libboost-system-dev libboost-log-dev libusb-1.0-0-dev \
  libprotobuf-dev protobuf-compiler libssl-dev \
  libavcodec-dev libavutil-dev qt6-multimedia-dev

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
cd build && ctest --output-on-failure
```

## Key Design Decisions

- **FFmpeg software decode** over V4L2 hardware (kernel 6.x regression, see [docs/design-decisions.md](docs/design-decisions.md))
- **QVideoSink + VideoOutput** for rendering (canonical Qt 6 approach)
- **INI config format** backward-compatible with original OAP config files
- **Plugin API** will be 100% backward-compatible with BlueWave's published API (TCP + protobuf)

## Related Resources

- [openauto-pro-community](https://github.com/mrmees/openauto-pro-community) — Recovered specs, QML, protos, icons, and research
- [aasdk (SonOfGib)](https://github.com/nicka-2) — Android Auto protocol SDK
- [f1xpl/openauto](https://github.com/nicka-2/openauto) — Original open-source AA implementation

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.
