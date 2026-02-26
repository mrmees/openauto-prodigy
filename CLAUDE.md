# OpenAuto Prodigy — Claude Code Context

## What This Is

Clean-room open-source rebuild of OpenAuto Pro (BlueWave Studio, defunct). Raspberry Pi-based **wireless-only** Android Auto head unit app using Qt 6 + QML, with a plugin-based architecture supporting BT audio, phone calls, and extensible third-party plugins.

## Design Philosophy

This project exists because BlueWave Studio stopped developing OpenAuto Pro, the source was never open, and time moved on — dependencies broke, the Pi ecosystem evolved, and users were left stranded. Prodigy is a **fresh start for everyone**, built on current software, with no legacy baggage.

### Core Principles

1. **Open source, GPL v3.** No closed-source dependencies, no proprietary blobs, no "contact us for licensing." Anyone can build, modify, and redistribute.

2. **Raspberry Pi 4 is the reference hardware.** Every feature must work on a Pi 4 with a basic touchscreen. If it doesn't run well on a Pi 4, it doesn't ship. Other SBCs and x86 are welcome but Pi 4 is the floor.

3. **Current software stack.** RPi OS Trixie (Debian 13) is the base OS. Qt 6, PipeWire, labwc (Wayland), modern C++17. No backporting to ancient distros.

4. **Android 12+ minimum, Android 14+ primary target.** Android 12 is the floor because that's when wireless AA became mandatory on new devices. We optimize for Android 14+ and don't bend over backwards for older quirks. AA protocol version: request v1.7 (current negotiated maximum).

5. **Wireless only.** USB AA adds complexity (AOAP, libusb, USB permissions) for a use case that's increasingly irrelevant. Wireless is the future, and it's what the Pi's built-in WiFi/BT supports natively.

6. **Plugin architecture for extensibility.** Core AA functionality is a plugin. BT audio is a plugin. Phone is a plugin. OBD-II, dashcam, backup camera — all future plugins. The shell and plugin system are the product; AA is just the killer app.

7. **Installable by normal humans.** One script, minimal prerequisites, clear error messages. If someone can flash an SD card and SSH in, they should be able to install Prodigy.

## Repository Layout

- `src/` — C++ source (plugin system, core services, AA protocol)
  - `src/core/` — Configuration, services (Audio, Theme, IPC, Config), AA protocol
  - `src/core/plugin/` — Plugin infrastructure (IPlugin, HostContext, PluginManager, Discovery, Loader)
  - `src/plugins/` — Static plugins (android_auto, bt_audio, phone)
  - `src/ui/` — Qt/QML controllers (ApplicationController, PluginModel, PluginRuntimeContext)
- `qml/` — QML UI files
  - `qml/components/` — Shell, TopBar, BottomBar, Clock, Wallpaper, GestureOverlay
  - `qml/controls/` — Reusable controls (Tile, Icon, NormalText, SpecialText)
  - `qml/applications/` — Plugin views (launcher, android_auto, settings, home, bt_audio, phone)
- `libs/open-androidauto/` — AA protocol library (Transport, Messenger, Session, Channel, HU handlers)
- `libs/aasdk/` — Android Auto SDK (git submodule, SonOfGib's fork)
- `tests/` — Unit tests (17 tests covering config, plugins, theme, audio, events, notifications, device registry)
- `web-config/` — Flask web config panel (Python, HTML/CSS/JS)
- `docs/` — Design decisions, development guide, wireless setup, plans
- `install.sh` — Interactive installer for RPi OS Trixie

## Build & Test

```bash
mkdir build && cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure  # from build dir (17 tests)
```

**Dependencies:** See `docs/development.md` for full package list. Bluetooth requires `qt6-connectivity-dev` (Pi only, optional on dev).

## Dual-Platform Build

Code must compile on both:
- **Qt 6.4** (Ubuntu 24.04 VM — `claude-dev`) — dev builds (no Bluetooth)
- **Qt 6.8** (RPi OS Trixie) — target platform (full Bluetooth support)

Key incompatibilities are documented in `docs/development.md` (QML loading, resource paths, type flattening).

## Cross-Compilation (Pi 4)

A cross-compiler toolchain is set up on claude-dev for building aarch64 binaries locally:

- **Toolchain file:** `toolchain-pi4.cmake`
- **Sysroot:** `~/pi-sysroot` (rsync'd from the Pi — re-sync if Pi libraries are updated)
- **Build dir:** `build-pi/` (separate from the x86 `build/` dir)

```bash
mkdir build-pi && cd build-pi
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-pi4.cmake ..
make -j$(nproc)
```

When asked to "build for the Pi" or "cross-compile", use the toolchain file and `build-pi/` directory. Rsync the resulting binary to the Pi instead of source files.

## Pi Deployment

**Option A — Cross-compiled binary** (faster builds):
```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
```

**Option B — Source copy + native build** (guaranteed compatibility):
```bash
# Copy changed source files to Pi, then:
ssh matt@192.168.1.152 "cd /home/matt/openauto-prodigy/build && cmake --build . -j3"
```

**Launch:**
```bash
ssh matt@192.168.1.152 'cd /home/matt/openauto-prodigy && nohup env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 ./build/src/openauto-prodigy > /tmp/oap.log 2>&1 &'
```

**Note:** If cross-compiled binaries have issues (missing libs, ABI mismatch), fall back to Option B.

## Current Status

**v0.3.0 — Audio pipeline and settings complete.**

Working features:
- Wireless AA connection (BT discovery → WiFi AP → TCP)
- H.264 video decoding and display at 1280x720 @ 30fps on Pi 4
- Multi-touch input via direct evdev reader (auto-detected by INPUT_PROP_DIRECT, bypasses Qt/Wayland)
- Plugin system with lifecycle management (Discover → Load → Init → Activate ↔ Deactivate → Shutdown)
- YAML configuration with deep merge, hot-reload, and ConfigService for QML
- Theme system (day/night mode, YAML-based color definitions)
- Audio pipeline via PipeWire (AA media/nav/phone streams with lock-free ring buffers)
- PipeWire device registry with hot-plug detection (USB audio adapters appear in QML dropdowns)
- Audio device selection (output + input) via Settings UI (requires restart to apply)
- Graceful AA shutdown (sends ShutdownRequest to phone before app exit/restart)
- App restart from Settings with PID-based wait and FD_CLOEXEC for clean port rebind
- Bluetooth audio plugin (A2DP sink monitoring, AVRCP metadata + controls via BlueZ D-Bus)
- Phone plugin (HFP via BlueZ D-Bus, dialer, incoming call overlay)
- Settings pages: Audio, Display, Connection, Video, System, About
- Web config panel (Flask, Unix socket IPC, settings/theme/plugin management)
- 3-finger gesture overlay (volume, brightness, home, day/night toggle)
- Connection watchdog using TCP_INFO polling (detects dead peers on local WiFi AP)
- Interactive install script for RPi OS Trixie
- Configurable sidebar during AA (volume, home) using protocol-level margin negotiation

**Known limitations / TODO:**
- D-Bus signal connection warnings for BT audio and phone plugins (non-fatal, but plugins don't receive live state updates)
- HFP call audio routing not yet wired through PipeWire
- Phone contacts not yet synced (PBAP profile)
- `wlan0` IP (10.0.0.1) doesn't survive reboot — needs permanent config
- Dynamic plugin loading (.so) untested — only static plugins currently
- Audio device switching requires app restart (live PipeWire stream re-routing didn't work reliably)
- Phone doesn't cleanly reconnect after app restart — user must manually cycle BT/WiFi
- "Default" device label shows first registry device, not PipeWire's actual default sink
- Sidebar touch zones use evdev hit detection — QML MouseAreas are visual fallback only (EVIOCGRAB blocks Qt input on Pi)
- Sidebar config changes require app restart (margins locked at AA session start)
- Sidebar volume slider in QML won't respond to evdev-driven volume changes (no live binding back from C++)

## Architecture

### Plugin System

Plugins implement `IPlugin` interface with lifecycle hooks:
- `initialize(IHostContext*)` — one-time setup, access to shared services
- `onActivated(QQmlContext*)` — expose QML bindings, start plugin-specific work
- `onDeactivated()` — cleanup before context destruction
- `shutdown()` — final teardown

Three static plugins compiled into binary:
- **AndroidAutoPlugin** (`org.openauto.android-auto`) — fullscreen AA with video/touch/audio
- **BtAudioPlugin** (`org.openauto.bt-audio`) — A2DP sink + AVRCP controls via BlueZ D-Bus
- **PhonePlugin** (`org.openauto.phone`) — HFP dialer + incoming call overlay via BlueZ D-Bus

Dynamic plugins loaded from `~/.openauto/plugins/` (each needs `plugin.yaml` manifest + `.so`).

### Core Services (via IHostContext)

- **ConfigService** — YAML config read/write
- **ThemeService** — Day/night theme colors, Q_PROPERTY bindings for QML
- **AudioService** — PipeWire stream creation, volume, audio focus
- **IpcServer** — QLocalServer Unix socket for web config panel communication

### AA Protocol

- **Transport:** Wireless only — BT discovery → WiFi AP → TCP. No USB/libusb.
- **Protocol threading:** ASIO threads for protocol, Qt main thread for UI. Bridged via `QMetaObject::invokeMethod(Qt::QueuedConnection)`.
- **Video pipeline:** FFmpeg software H.264 decode on worker thread → `QVideoFrame` → `QVideoSink::setVideoFrame()` → QML `VideoOutput`.
- **Touch input:** `EvdevTouchReader` (QThread) reads auto-detected touch device (INPUT_PROP_DIRECT scan) with `EVIOCGRAB` → MT Type B slot tracking → `TouchHandler` → AA `InputEventIndication` protobuf.
- **Touch coordinate mapping:** Evdev (0-4095) → AA coordinates (1280x720). Letterbox-aware for 1024x600 display.
- **Audio:** 3 PipeWire streams (media, navigation, phone) with audio focus management.
- **3-finger gesture:** EvdevTouchReader detects 3 simultaneous touches within 200ms, suppresses AA forwarding, emits signal for overlay.

### BlueZ D-Bus Integration

Both BtAudioPlugin and PhonePlugin use BlueZ D-Bus directly (no ofono):
- `org.bluez.MediaTransport1` — A2DP connection state
- `org.bluez.MediaPlayer1` — AVRCP metadata + playback commands
- `org.bluez.Device1` — HFP device detection (UUID 0000111e/0000111f)
- `org.freedesktop.DBus.ObjectManager.GetManagedObjects` — scan for existing connections
- `org.freedesktop.DBus.Properties.PropertiesChanged` — live state updates

**D-Bus deserialization gotcha:** `QDBusArgument::operator>>` cannot extract `QVariantMap` directly. Must use manual `beginMap()/endMap()` with `QDBusVariant` for property values.

### Web Config Panel

Flask server (`web-config/server.py`) communicates with Qt app via Unix domain socket (`/tmp/openauto-prodigy.sock`). JSON request/response protocol. Routes: dashboard, settings, themes, plugins.

## Key Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, plugin registration, QML engine, IPC server |
| `src/core/plugin/IPlugin.hpp` | Plugin interface — lifecycle, metadata, QML component |
| `src/core/plugin/PluginManager.cpp` | Plugin lifecycle orchestration |
| `src/core/plugin/HostContext.cpp` | Shared service access for plugins |
| `src/ui/PluginRuntimeContext.cpp` | Scoped QML context per plugin activation |
| `src/ui/PluginModel.cpp` | QAbstractListModel for QML nav strip |
| `src/core/YamlConfig.cpp` | YAML config with deep merge |
| `src/core/services/ThemeService.cpp` | Day/night theme, color Q_PROPERTYs |
| `src/core/services/AudioService.cpp` | PipeWire stream management, ring buffer bridge |
| `src/core/services/PipeWireDeviceRegistry.cpp` | Hot-plug device enumeration via pw_registry |
| `src/ui/AudioDeviceModel.cpp` | QAbstractListModel for device selection ComboBoxes |
| `src/core/InputDeviceScanner.cpp` | Touch device auto-detection via INPUT_PROP_DIRECT |
| `src/core/services/IpcServer.cpp` | Unix socket IPC for web config |
| `src/plugins/android_auto/AndroidAutoPlugin.cpp` | AA plugin (wraps all AA protocol code) |
| `src/plugins/bt_audio/BtAudioPlugin.cpp` | BT audio with BlueZ D-Bus A2DP/AVRCP |
| `src/plugins/phone/PhonePlugin.cpp` | Phone with BlueZ D-Bus HFP |
| `src/core/aa/AndroidAutoService.cpp` | AA lifecycle, TCP transport, BT integration |
| `src/core/aa/VideoDecoder.cpp` | FFmpeg H.264 decode worker → QVideoFrame |
| `src/core/aa/EvdevTouchReader.cpp` | Direct evdev multi-touch + 3-finger gesture |
| `src/core/aa/TouchHandler.hpp` | Touch → AA protobuf bridge |
| `qml/components/Shell.qml` | App shell (status bar + content area + nav strip) |
| `qml/components/GestureOverlay.qml` | 3-finger tap quick controls overlay |
| `web-config/server.py` | Flask web config server |
| `install.sh` | Interactive RPi OS Trixie installer |

## AA Protocol: Touch Events

Based on Android MotionEvent. Critical details learned the hard way:

- **Actions:** DOWN(0), UP(1), MOVE(2), POINTER_DOWN(5), POINTER_UP(6)
- **ALL active pointers** must be included in every message (not just the changed finger)
- **`action_index`** is the ARRAY INDEX into the pointers list, not the pointer ID
- **`pointer_id`** is a stable per-finger identifier (we use slot index)
- **Touch coordinates must be in VIDEO RESOLUTION space** (1280x720), NOT the `touch_screen_config` dimensions (1024x600)
- **For UP events:** include the lifted finger in the pointer array at its last position

## AA Protocol: Video Resolutions

AA supports fixed resolutions only:
- `_480p` (800x480), `_720p` (1280x720) — current default, `_1080p` (1920x1080)
- Portrait variants: `_720p_p`, `_1080pp`

## Gotchas

- Don't use `loadFromModule()` — not available in Qt 6.4
- `QColor` needs `Qt6::Gui` link, not `Qt6::Core`
- Boost.Log truncates multiline — use `ShortDebugString()` for protobuf
- SonOfGib aasdk is proto2 — no `device_model` field in ServiceDiscoveryRequest
- BT code is `#ifdef HAS_BLUETOOTH` guarded — builds without qt6-connectivity-dev
- WiFi SSID/password in config must match hostapd.conf exactly
- SPS/PPS arrives as `AV_MEDIA_INDICATION` (no timestamp) — must forward to decoder
- H.264 data from aasdk already has AnnexB start codes — do NOT prepend additional ones
- Q_OBJECT in header-only classes needs a .cpp file listed in CMakeLists.txt for MOC
- `pkill` on names >15 chars silently matches nothing — use `pkill -f`
- **QVideoFrame is ref-counted** — do NOT reuse frame buffers. Allocate fresh frames each decode.
- **labwc `mouseEmulation="yes"`** destroys multi-touch — must be `"no"` in `~/.config/labwc/rc.xml`
- **Qt evdevtouch plugin** causes duplicate events — use direct evdev with EVIOCGRAB instead
- **WAYLAND_DISPLAY** on Pi is `wayland-0` (not `wayland-1`)
- **DFRobot USB Multi Touch** (vendor 3343:5710): 10 points, MT Type B, 0-4095 range
- **QDBusArgument `>>` operator** cannot extract QVariantMap directly — use manual `beginMap()/endMap()` with `QDBusVariant`
- **QTimer needs `#include <QTimer>`** — forward declaration alone causes "not declared in scope" errors
- **QTimer only works on threads with a Qt event loop** — starting a QTimer from a Boost.ASIO thread silently does nothing. Use `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)` to marshal to the main thread.
- **TCP keepalive alone won't detect dead connections on a local WiFi AP** — when the Pi IS the AP, there's no router to send RST. Must actively poll `tcp_info` via `getsockopt(IPPROTO_TCP, TCP_INFO)` and check `tcpi_backoff >= 3` (~16s detection).
- **`tcpi_retransmits` resets between polls** — use `tcpi_backoff` instead for reliable dead peer detection.
- **`<netinet/tcp.h>` and `<linux/tcp.h>` conflict** — only include `<netinet/tcp.h>` for `tcp_info`.
- **EVIOCGRAB must be toggled with AA connection state** — grab on AA connect (route touch to AA), ungrab on disconnect (return touch to Wayland/libinput). Permanent grab steals touch from the launcher UI.
- **PipeWire underrun handling: silence-fill short reads** — always report `maxSize` to PipeWire with silence padding for any underrun gap. Short reads (reporting only actual bytes) cause worse artifacts (clicks/pops at period boundaries) than silence padding. An earlier attempt at zero-filling ALL reads caused choppy audio, but that was a different issue — filling silence only during genuine underruns is correct.
- **Boost.ASIO sockets don't set SOCK_CLOEXEC** — forked processes (e.g. QProcess::startDetached for restart) inherit the TCP acceptor FD, preventing port rebind. Must `fcntl(fd, F_SETFD, FD_CLOEXEC)` after socket open.
- **SO_REUSEADDR must be set before bind** — the Boost.ASIO 2-arg acceptor constructor does open+bind+listen in one shot, too late for socket options. Use separate open/set_option/bind/listen.
- **`SPA_DICT_INIT_ARRAY` inline syntax** causes "taking address of temporary array" — use named `spa_dict_item` arrays with `SPA_DICT_INIT` instead.
- **AA `VideoConfig.margin_width/height` actually works** — the phone renders UI in a centered sub-region with black bar margins. Use this for non-standard screen ratios and sidebar layouts. Margins are locked at session start (set during `ServiceDiscoveryResponse`). See `docs/aa-video-resolution.md`.
- **Sidebar QML MouseArea vs EVIOCGRAB** — during AA, EVIOCGRAB steals all touch from Qt. Sidebar touch actions are handled via evdev hit zones in `EvdevTouchReader`, not QML `MouseArea`. The QML controls are visual only on Pi.
- **`touch_screen_config` must be video resolution (1280x720), not display resolution (1024x600)** — the phone interprets touch coordinates relative to `touch_screen_config`. Since we send coordinates in video resolution space, this field must match. Mismatch causes touch misalignment.
- **Phone sends `config_index=3` in `AVChannelSetupRequest`** — despite our video config list only having indices 0-1. This is the phone's internal reference, not an index into our list. We always respond with `add_configs(0)` (720p primary).
- **FFmpeg `thread_count` must be 1 for real-time AA decode** — `thread_count=2` causes multi-threaded H.264 decoders to buffer frames internally before producing output, resulting in permanent EAGAIN on phones that send small P-frames. Single-threaded decode produces frames immediately.
- **Some phones output `AV_PIX_FMT_YUVJ420P` (fmt=12) instead of `AV_PIX_FMT_YUV420P` (fmt=0)** — JPEG full-range vs limited-range YUV420. Identical pixel layout, different color range flag. Must accept both formats in the decoder or frames will be silently discarded (black screen). Moto G Play 2024 outputs YUVJ420P; Samsung S25 Ultra outputs YUV420P.

## Hardware (Pi Target)

| Component | Details |
|-----------|---------|
| Board | Raspberry Pi 4 |
| Display | 1024x600 @ 60Hz (LTM LONTIUM, HDMI) |
| Touch | DFRobot USB Multi Touch (10-point, MT Type B, 0-4095 range) |
| WiFi | Built-in (used as AP for phone connection) |
| BT | Built-in (RFCOMM for AA discovery, A2DP sink, HFP) |
| IP | 192.168.1.149 (LAN), 10.0.0.1 (wlan0 AP) |
| OS | RPi OS Trixie, labwc compositor |
