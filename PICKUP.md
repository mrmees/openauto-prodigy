# OpenAuto Prodigy — Complete Project State

Everything you need to pick this project back up from any machine on the network.

---

## TL;DR

Clean-room open-source rebuild of OpenAuto Pro (defunct BlueWave Studio). Wireless-only Android Auto head unit for Raspberry Pi. Qt 6 + QML frontend, SonOfGib's aasdk for AA protocol, FFmpeg for video decode.

**Phase 1 is ~60% done.** Core protocol, config, theme, UI shell, and video pipeline are written. Audio, settings UI, and integration testing remain. The wireless pivot (BT discovery + WiFi AP + TCP) is code-complete but untested on real hardware.

---

## Repositories

| Repo | Location | Purpose |
|------|----------|---------|
| **openauto-prodigy** | `E:/claude/personal/openautopro/openauto-prodigy/` | Implementation (this repo) |
| **openauto-pro-community** | `E:/claude/personal/openautopro/openauto-pro-community/` | Recovered specs, QML, protos, RE notes |
| **GitHub** | `https://github.com/mrmees/openauto-prodigy` (PRIVATE) | Remote origin |

The community repo has the design doc, phase 1 plan, RE notes, and ABI analysis. It's the "why" — this repo is the "what."

---

## Raspberry Pi (Target Hardware)

| Detail | Value |
|--------|-------|
| **Hostname** | `prodigy.local` |
| **IP** | `192.168.1.149` |
| **SSH** | `matt@prodigy.local` — key auth configured |
| **OS** | RPi OS Trixie (Debian 13, 64-bit) |
| **Qt** | 6.8.2 |
| **Hardware** | Raspberry Pi 4 |
| **Display** | DFRobot 7" 1024x600 |
| **Display server** | labwc (Wayland) |
| **sudo** | Works over SSH, full root access |
| **Build path** | `~/openauto-prodigy/` (cloned from GitHub) |

### SSH Access

```bash
ssh matt@prodigy.local
# or
ssh matt@192.168.1.149
```

Key auth is set up — no password needed (assuming your SSH key is deployed).

### Build on Pi

```bash
cd ~/openauto-prodigy
git pull --recurse-submodules
cd build
cmake ..
make -j$(nproc)
```

### Run on Pi

```bash
export XDG_RUNTIME_DIR=/run/user/1000
export WAYLAND_DISPLAY=wayland-0
export QT_QPA_PLATFORM=wayland
./src/openauto-prodigy
```

### Pi Dependencies (all should already be installed)

```bash
sudo apt install cmake qt6-base-dev qt6-declarative-dev qt6-wayland \
  qt6-connectivity-dev qt6-multimedia-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript \
  libboost-system-dev libboost-log-dev libusb-1.0-0-dev \
  libprotobuf-dev protobuf-compiler libssl-dev \
  libavcodec-dev libavutil-dev \
  hostapd dnsmasq
```

---

## Dev Build (WSL2)

| Detail | Value |
|--------|-------|
| **Distro** | Ubuntu 24.04 (NOT docker-desktop — use `wsl -d Ubuntu`) |
| **Qt** | 6.4.2 |
| **Bluetooth** | Not available (no qt6-connectivity-dev) |
| **Purpose** | Fast code iteration, unit tests |

### WSL2 Gotcha

Default WSL distro may be docker-desktop. Always specify:
```bash
wsl -d Ubuntu
```

### Build in WSL2

Same as Pi, minus Bluetooth. The `#ifdef HAS_BLUETOOTH` guards let it compile without qt6-connectivity-dev.

```bash
cd ~/openauto-prodigy   # or wherever you clone it
mkdir -p build && cd build
cmake ..
make -j$(nproc)
ctest --output-on-failure   # run tests
```

### Can't sudo in WSL2 from Claude's shell

Password prompt blocks. Install deps manually or use a terminal.

---

## Phase 1 Task Status

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Repository & build system | **DONE** | CMake, submodule, CI-ready |
| 2 | Configuration system | **DONE** | INI config, 6 enums, 13 colors x2, wireless settings |
| 3 | Theme engine | **DONE** | Day/night toggle, QML context property |
| 4 | QML UI shell | **DONE** | Launcher, home, AA, settings screens |
| 5 | AA service layer | **DONE** | TCP transport, handshake, 8 service channels |
| 6 | Video pipeline | **CODE COMPLETE** | FFmpeg H.264 → QVideoSink — untested on live video |
| 7 | Audio pipeline | **PENDING** | 3 channels (media, speech, system) via PipeWire |
| 8 | Settings UI | **PENDING** | QML settings screens |
| 9 | Integration testing | **PENDING** | End-to-end with real phone |
| 10 | Polish | **PENDING** | Error handling, UX, systemd service |

### Wireless Pivot (crosses tasks 5 & 8)

We pivoted from USB to wireless-only after USB connections consistently completed the handshake but never opened service channels. The wireless architecture is:

1. **Bluetooth Discovery** — Pi runs RFCOMM server with AA Wireless UUID
2. **WiFi Credential Exchange** — Phone sends WifiInfoRequest, Pi responds with SSID/password via protobuf
3. **WiFi AP** — hostapd + dnsmasq on wlan0 (10.0.0.1/24)
4. **TCP Connection** — Phone connects to Pi on port 5288 for AA protocol

This is **code-complete but never tested on hardware.** First real test will tell us if service channels open over TCP (they never did over USB).

---

## Architecture (What's Built)

### C++ Core

| File | What It Does |
|------|-------------|
| `src/main.cpp` | Entry point, QML engine setup |
| `src/core/Configuration.hpp/cpp` | INI config parser, all settings, backward-compatible with OAP |
| `src/core/aa/AndroidAutoService.hpp/cpp` | AA lifecycle manager, TCP transport, BT integration |
| `src/core/aa/BluetoothDiscoveryService.hpp/cpp` | BT RFCOMM server, WiFi credential handshake protocol |
| `src/core/aa/AndroidAutoEntity.hpp/cpp` | Protocol entity, control channel, version/SSL/auth/discovery handshake |
| `src/core/aa/ServiceFactory.hpp/cpp` | Creates all 8 AA service instances |
| `src/core/aa/VideoService.hpp/cpp` | Receives H.264 NALUs, marshals to Qt main thread |
| `src/core/aa/VideoDecoder.hpp/cpp` | FFmpeg H.264 software decode → QVideoFrame (YUV420P) |
| `src/ui/ThemeController.hpp/cpp` | Day/night theme colors, exposed as QML context property |
| `src/ui/ApplicationController.hpp/cpp` | Navigation stack, app type enum |

### QML UI

| Screen | File |
|--------|------|
| Root window | `qml/main.qml` |
| Home | `qml/applications/home/HomeMenu.qml` |
| Launcher | `qml/applications/launcher/LauncherMenu.qml` |
| Android Auto | `qml/applications/android_auto/AndroidAutoMenu.qml` |
| Settings | `qml/applications/settings/SettingsMenu.qml` |
| Components | `qml/components/` — TopBar, BottomBar, Clock, Wallpaper |
| Controls | `qml/controls/` — Icon, Tile, NormalText, SpecialText |

### Protocol Stack

```
Phone ←→ [Bluetooth RFCOMM] ←→ BluetoothDiscoveryService (WiFi creds)
Phone ←→ [WiFi AP / TCP:5288] ←→ AndroidAutoService → AndroidAutoEntity
                                    ↓
                              ServiceFactory creates:
                                VideoService (channel 0)
                                MediaAudioService (channel 1)
                                SpeechAudioService (channel 2)
                                SystemAudioService (channel 3)
                                InputService (channel 4)
                                SensorService (channel 5)
                                BluetoothService (channel 6)
                                WifiService (channel 7)
```

### Threading Model

- **ASIO thread** — AA protocol (aasdk), TCP transport
- **Qt main thread** — QML UI, video frames
- **Bridge** — `QMetaObject::invokeMethod(Qt::QueuedConnection)` to marshal between threads

---

## Wireless Setup (on Pi)

Full details in `docs/wireless-setup.md`. Summary:

### hostapd config (`/etc/hostapd/hostapd.conf`)

```ini
interface=wlan0
driver=nl80211
ssid=OpenAutoProdigy
hw_mode=g
channel=6
wpa=2
wpa_passphrase=prodigy1234
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
```

### dnsmasq config (`/etc/dnsmasq.d/openauto.conf`)

```ini
interface=wlan0
dhcp-range=10.0.0.10,10.0.0.50,255.255.255.0,24h
```

### Static IP on wlan0

10.0.0.1/24 — configured via dhcpcd or NetworkManager (see wireless-setup.md for options).

### App config (`~/.config/openauto_system.ini`)

```ini
[Wireless]
enabled=true
wifi_ssid=OpenAutoProdigy
wifi_password=prodigy1234
tcp_port=5288
```

**SSID and password must match between hostapd.conf and openauto_system.ini exactly.**

### BT Handshake Message IDs

| ID | Message | Direction |
|----|---------|-----------|
| 1 | WifiInfoRequest | Phone → Pi |
| 2 | WifiSecurityRequest | Phone → Pi |
| 3 | WifiSecurityResponse | Pi → Phone |
| 7 | WifiInfoResponse | Pi → Phone |

---

## Qt 6.4 vs 6.8 Compatibility (READ THIS)

Code must compile on both. Known landmines:

| Issue | Wrong | Right |
|-------|-------|-------|
| QML loading | `engine.loadFromModule()` | `engine.load(QUrl(...))` — loadFromModule doesn't exist before 6.5 |
| Resource paths | `qrc:/qt/qml/OpenAutoProdigy/` | `qrc:/OpenAutoProdigy/` — we use the 6.4 format |
| QML subdirectory types | Bare filename | Set `QT_QML_SOURCE_TYPENAME` in CMake to flatten type names |
| QColor linking | `Qt6::Core` | `Qt6::Gui` — QColor lives there |
| QSettings forward-declare | `class QSettings` in namespace | `#include <QSettings>` — forward-declare creates wrong type |
| QtQuick.Controls runtime | Missing package | Install `qml6-module-qtqml-workerscript` |

---

## Key Dependencies

| Dependency | Purpose |
|------------|---------|
| **aasdk** (git submodule) | SonOfGib's fork — AA protocol implementation |
| **Qt 6** (Core, Gui, Quick, QuickControls2, Multimedia, Network, [Bluetooth]) | UI framework + video output |
| **Boost** (system, log, log_setup) | Logging, ASIO networking |
| **FFmpeg** (libavcodec, libavutil) | H.264 video decode |
| **protobuf** | AA protocol serialization |
| **OpenSSL** | AA SSL/TLS handshake |
| **hostapd + dnsmasq** | WiFi access point (Pi only) |

---

## Known Issues & Gotchas

- **USB never worked fully** — Handshake completes but service channels never open. That's why we went wireless-only.
- **Video pipeline untested** — Code written, compiles, but no live video has ever flowed through it.
- **Proto2 aasdk** — SonOfGib's fork uses proto2. No `device_model` in ServiceDiscoveryRequest.
- **Boost.Log truncation** — Use `ShortDebugString()` for protobuf logging, not `DebugString()`.
- **BT conditional compile** — `#ifdef HAS_BLUETOOTH` guards. WSL2 builds without Bluetooth support.
- **WSL2 default distro** — May be docker-desktop, always use `wsl -d Ubuntu`.

---

## Unit Tests

2 tests, both passing:
- `test_configuration` — INI config read/write, enum parsing, color handling
- `test_theme_controller` — Day/night toggle, color property changes

Run from build dir: `ctest --output-on-failure`

---

## Reference Materials (in openauto-pro-community repo)

| Resource | Location |
|----------|----------|
| Design document | `docs/plans/2026-02-16-openauto-prodigy-design.md` |
| Phase 1 plan (10 tasks) | `docs/plans/2026-02-16-phase1-implementation.md` |
| Reverse engineering notes | `docs/reverse-engineering-notes.md` |
| AA compatibility research | `docs/android-auto-fix-research.md` |
| ABI analysis | `docs/abi-compatibility-analysis.md` |
| Recovered QML (162 files) | `original/qml/` |
| Recovered proto (34 messages) | `original/proto/Api.proto` |
| SonOfGib BT reference | `upstream/sonofgib-openauto/src/btservice/AndroidBluetoothServer.cpp` |

---

## What's Next

1. **Test wireless connection on Pi** — Does BT discovery work? Do WiFi creds exchange? Does TCP connect? Do service channels open?
2. **Audio pipeline (Task 7)** — 3 channels: media, speech, system. PipeWire output.
3. **Settings UI (Task 8)** — QML screens for all config options.
4. **Integration testing (Task 9)** — End-to-end with real phone, real video, real audio.
5. **Polish (Task 10)** — Error handling, systemd service, first-run experience.

---

## License

GPLv3. See LICENSE file.
