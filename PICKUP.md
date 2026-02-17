# OpenAuto Prodigy — Complete Project State

Everything you need to pick this project back up from any machine on the network.

---

## TL;DR

Clean-room open-source rebuild of OpenAuto Pro (defunct BlueWave Studio). Wireless-only Android Auto head unit for Raspberry Pi. Qt 6 + QML frontend, SonOfGib's aasdk for AA protocol, FFmpeg for video decode.

**Phase 1 is ~75% done.** Video, touch input, and fullscreen are working on real hardware. Audio pipeline and settings UI remain.

---

## Repositories

| Repo | Location | Purpose |
|------|----------|---------|
| **openauto-prodigy** | `/home/matt/claude/personal/openautopro/openauto-prodigy/` | Implementation (this repo) |
| **openauto-pro-community** | `/home/matt/claude/personal/openautopro/openauto-pro-community/` | Recovered specs, QML, protos, RE notes |
| **GitHub** | `https://github.com/mrmees/openauto-prodigy` (PRIVATE) | Remote origin |

The community repo has the design doc, phase 1 plan, RE notes, and ABI analysis. It's the "why" — this repo is the "what."

---

## Raspberry Pi (Target Hardware)

| Detail | Value |
|--------|-------|
| **Hostname** | `prodigy.local` |
| **IP** | `192.168.1.149` |
| **SSH** | `matt@192.168.1.149` — key auth configured |
| **OS** | RPi OS Trixie (Debian 13, 64-bit) |
| **Qt** | 6.8.2 |
| **Hardware** | Raspberry Pi 4 |
| **Display** | DFRobot 7" 1024x600 touchscreen |
| **Display server** | labwc (Wayland) |
| **sudo** | Works over SSH, full root access |
| **Build path** | `~/openauto-prodigy/` (cloned from GitHub) |
| **Phone tested** | Samsung Galaxy S24 Ultra (SM-S938U) |

### SSH Access

```bash
ssh matt@192.168.1.149
```

Key auth is set up — no password needed.

**Note:** The `claude-dev` VM (192.168.189.x) is on VMware NAT and can reach this IP via the host's routing. mDNS (`prodigy.local`) may not resolve from the VM.

### Build on Pi

```bash
cd ~/openauto-prodigy
git pull --recurse-submodules
cd build
cmake --build . -j3    # -j3 recommended on Pi 4 (leaves headroom)
```

### Run on Pi

```bash
# From a local terminal or SSH with DISPLAY forwarding:
export XDG_RUNTIME_DIR=/run/user/1000
export WAYLAND_DISPLAY=wayland-0
export QT_QPA_PLATFORM=wayland
./src/openauto-prodigy

# Or via SSH (backgrounded):
ssh matt@192.168.1.149 'DISPLAY=:0 XDG_RUNTIME_DIR=/run/user/1000 nohup /home/matt/openauto-prodigy/build/src/openauto-prodigy </dev/null >/tmp/oap.log 2>&1 & sleep 1; pgrep -f openauto-prodigy && echo running'

# Kill running instance:
ssh matt@192.168.1.149 'pkill -f openauto-prodigy'
```

### Pi Dependencies (all should already be installed)

```bash
sudo apt install cmake qt6-base-dev qt6-declarative-dev qt6-wayland \
  qt6-connectivity-dev qt6-multimedia-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window qml6-module-qtqml-workerscript \
  libboost-system-dev libboost-log-dev \
  libprotobuf-dev protobuf-compiler libssl-dev \
  libavcodec-dev libavutil-dev \
  hostapd dnsmasq
```

---

## Dev Build (claude-dev VM)

| Detail | Value |
|--------|-------|
| **Machine** | VMware VM on MINIMEES (Windows host) |
| **IP** | 192.168.189.129 |
| **OS** | Ubuntu 24.04 |
| **Qt** | 6.4.2 |
| **Bluetooth** | Not available (no qt6-connectivity-dev) |
| **Purpose** | Fast code iteration, unit tests |

### Build on VM

```bash
cd ~/claude/personal/openautopro/openauto-prodigy
mkdir -p build && cd build
cmake ..
make -j$(nproc)
ctest --output-on-failure   # run tests
```

The `#ifdef HAS_BLUETOOTH` guards let it compile without qt6-connectivity-dev.

---

## Phase 1 Task Status

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Repository & build system | **DONE** | CMake, submodule, CI-ready |
| 2 | Configuration system | **DONE** | INI config, 6 enums, 13 colors x2, wireless settings |
| 3 | Theme engine | **DONE** | Day/night toggle, QML context property |
| 4 | QML UI shell | **DONE** | Launcher, home, AA, settings screens |
| 5 | AA service layer | **DONE** | TCP transport, handshake, 8 service channels |
| 6 | Video pipeline | **WORKING** | FFmpeg H.264 → QVideoSink, 1280x720 @ 30fps on Pi 4 |
| 7 | Touch input | **WORKING** | Multi-touch via InputServiceChannel, debug overlay |
| 8 | Fullscreen | **WORKING** | OS-level Window.FullScreen when AA connected |
| 9 | Audio pipeline | **PENDING** | 3 channels (media, speech, system) via PipeWire |
| 10 | Settings UI | **PENDING** | QML settings screens |
| 11 | Integration & polish | **IN PROGRESS** | Touch calibration, error handling, systemd service |

### Wireless Connection (Verified Working)

The full wireless flow works end-to-end:
1. **Bluetooth Discovery** — Pi runs RFCOMM server with AA Wireless UUID
2. **WiFi Credential Exchange** — Phone sends WifiInfoRequest, Pi responds with SSID/password
3. **WiFi AP** — hostapd on wlan0 at 5GHz (10.0.0.1/24)
4. **TCP Connection** — Phone connects to Pi on port 5288
5. **AA Protocol** — Handshake, service discovery, video/input channels open

### Key Discoveries (from first hardware test)

- **SPS/PPS delivery:** Android Auto sends H.264 SPS/PPS codec config as `AV_MEDIA_INDICATION` (message ID 0x0001, no timestamp), NOT as `AV_MEDIA_WITH_TIMESTAMP_INDICATION`. Both message types must be forwarded to the video decoder. Without SPS/PPS, the decoder fails with `non-existing PPS 0 referenced`.
- **AnnexB start codes:** aasdk already delivers NAL units WITH AnnexB start codes (`00 00 00 01`). The VideoDecoder was incorrectly prepending additional start codes — removed.
- **Video resolution:** Phone sends 1280x720 video. Software decode handles this easily on Pi 4 (~12% CPU per core).
- **Touch coordinate space:** AA touch config is 1024x600 (matching the display). QML maps screen coordinates to this space.

---

## Architecture (What's Built)

### C++ Core

| File | What It Does |
|------|-------------|
| `src/main.cpp` | Entry point, QML engine, exposes C++ objects to QML, auto-nav on AA connect/disconnect |
| `src/core/Configuration.hpp/cpp` | INI config parser, all settings, backward-compatible with OAP |
| `src/core/aa/AndroidAutoService.hpp/cpp` | AA lifecycle manager, TCP transport, BT integration |
| `src/core/aa/BluetoothDiscoveryService.hpp/cpp` | BT RFCOMM server, WiFi credential handshake protocol |
| `src/core/aa/AndroidAutoEntity.hpp/cpp` | Protocol entity, control channel, version/SSL/auth/discovery handshake |
| `src/core/aa/ServiceFactory.hpp/cpp` | Creates all AA service instances, wires TouchHandler to input channel |
| `src/core/aa/VideoService.hpp/cpp` | Receives H.264 NALUs (both timestamped and non-timestamped), marshals to Qt main thread |
| `src/core/aa/VideoDecoder.hpp/cpp` | FFmpeg H.264 software decode → QVideoFrame (YUV420P) |
| `src/core/aa/TouchHandler.hpp/cpp` | QObject bridge: QML touch events → AA InputEventIndication protobuf via InputServiceChannel |
| `src/ui/ThemeController.hpp/cpp` | Day/night theme colors, exposed as QML context property |
| `src/ui/ApplicationController.hpp/cpp` | Navigation stack, app type enum |

### QML UI

| Screen | File |
|--------|------|
| Root window | `qml/main.qml` — fullscreen toggle, TopBar/BottomBar visibility |
| Home | `qml/applications/home/HomeMenu.qml` |
| Launcher | `qml/applications/launcher/LauncherMenu.qml` |
| Android Auto | `qml/applications/android_auto/AndroidAutoMenu.qml` — VideoOutput, MultiPointTouchArea, touch debug overlay |
| Settings | `qml/applications/settings/SettingsMenu.qml` |
| Components | `qml/components/` — TopBar, BottomBar, Clock, Wallpaper |
| Controls | `qml/controls/` — Icon, Tile, NormalText, SpecialText |

### Protocol Stack

```
Phone ←→ [Bluetooth RFCOMM] ←→ BluetoothDiscoveryService (WiFi creds)
Phone ←→ [WiFi AP / TCP:5288] ←→ AndroidAutoService → AndroidAutoEntity
                                    ↓
                              ServiceFactory creates:
                                VideoService     → VideoDecoder → QVideoSink
                                InputService     ← TouchHandler ← QML MultiPointTouchArea
                                MediaAudioService (stub)
                                SpeechAudioService (stub)
                                SystemAudioService (stub)
                                SensorService (stub)
                                BluetoothService (stub)
                                WifiService (stub)
```

### Threading Model

- **ASIO thread pool (4 threads)** — AA protocol (aasdk), TCP transport
- **Qt main thread** — QML UI, video decode, video frame display
- **Bridge** — `QMetaObject::invokeMethod(Qt::QueuedConnection)` to marshal ASIO → Qt; `strand_.dispatch()` for Qt → ASIO (touch events)

---

## Wireless Setup (on Pi)

Full details in `docs/wireless-setup.md`. Summary:

- **hostapd:** 5GHz (channel 36), WPA2-PSK, SSID `OpenAutoProdigy`
- **dnsmasq:** DHCP on wlan0, range 10.0.0.10-50
- **Static IP:** wlan0 = 10.0.0.1/24
- **App config:** `~/.config/openauto_system.ini` — SSID/password must match hostapd exactly
- **5GHz is mandatory** — Pi 4/5 CYW43455 shares one antenna for WiFi + BT, 2.4GHz causes coexistence interference

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

- **USB never worked fully** — Handshake completes but service channels never open. Wireless works first try.
- **SPS/PPS in wrong handler** — Must forward `AV_MEDIA_INDICATION` data to decoder (not just `AV_MEDIA_WITH_TIMESTAMP_INDICATION`). This was the root cause of "no video" — now fixed.
- **AnnexB start codes already present** — aasdk delivers H.264 data WITH start codes. Do NOT prepend additional ones.
- **Touch calibration** — May need adjustment. Debug overlay shows mapped coordinates for diagnosis.
- **Proto2 aasdk** — SonOfGib's fork uses proto2. No `device_model` in ServiceDiscoveryRequest.
- **Boost.Log truncation** — Use `ShortDebugString()` for protobuf logging, not `DebugString()`.
- **BT conditional compile** — `#ifdef HAS_BLUETOOTH` guards. VM builds without Bluetooth support.
- **Q_OBJECT MOC** — Header-only QObjects need a .cpp listed in CMakeLists.txt for MOC generation.
- **pkill long names** — `pkill openauto-prodigy` silently fails (>15 chars). Use `pkill -f openauto-prodigy`.

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

---

## What's Next

1. **Touch calibration** — Test with debug overlay, fix any coordinate mapping offset
2. **Audio pipeline** — 3 channels: media, speech, system. PipeWire output.
3. **Settings UI** — QML screens for all config options.
4. **Integration polish** — Error handling, reconnect logic, systemd service, first-run experience.

---

## License

GPLv3. See LICENSE file.
