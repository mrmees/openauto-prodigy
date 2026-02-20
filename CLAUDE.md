# OpenAuto Prodigy — Claude Code Context

## What This Is

Clean-room open-source rebuild of OpenAuto Pro (BlueWave Studio, defunct). Raspberry Pi-based **wireless-only** Android Auto head unit app using Qt 6 + QML, with a plugin-based architecture supporting BT audio, phone calls, and extensible third-party plugins.

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
- `libs/aasdk/` — Android Auto SDK (git submodule, SonOfGib's fork)
- `tests/` — Unit tests (8 tests: configuration, yaml_config, theme_controller, theme_service, audio_service, plugin_discovery, plugin_manager, plugin_model)
- `web-config/` — Flask web config panel (Python, HTML/CSS/JS)
- `docs/` — Design decisions, development guide, wireless setup, plans
- `install.sh` — Interactive installer for RPi OS Trixie

## Build & Test

```bash
mkdir build && cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure  # from build dir (8 tests)
```

**Dependencies:** See `docs/development.md` for full package list. Bluetooth requires `qt6-connectivity-dev` (Pi only, optional on dev).

## Dual-Platform Build

Code must compile on both:
- **Qt 6.4** (Ubuntu 24.04 VM — `claude-dev`) — dev builds (no Bluetooth)
- **Qt 6.8** (RPi OS Trixie) — target platform (full Bluetooth support)

Key incompatibilities are documented in `docs/development.md` (QML loading, resource paths, type flattening).

## Pi Deployment

```bash
# Copy changed source files to Pi, then:
ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy/build && cmake --build . -j3"
# Launch:
ssh matt@192.168.1.149 'cd /home/matt/openauto-prodigy && nohup env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 ./build/src/openauto-prodigy > /tmp/oap.log 2>&1 &'
```

**IMPORTANT:** Do NOT scp the x86 binary from the dev VM to the Pi. Copy source files and build natively on Pi (ARM64).

## Current Status

**v0.2.0 — Plugin architecture milestone complete.**

Working features:
- Wireless AA connection (BT discovery → WiFi AP → TCP)
- H.264 video decoding and display at 1280x720 @ 60fps
- Multi-touch input via direct evdev reader (bypasses Qt/Wayland)
- Plugin system with lifecycle management (Discover → Load → Init → Activate ↔ Deactivate → Shutdown)
- YAML configuration with deep merge and hot-reload
- Theme system (day/night mode, YAML-based color definitions)
- Audio pipeline via PipeWire (AA media/nav/phone streams)
- Bluetooth audio plugin (A2DP sink monitoring, AVRCP metadata + controls via BlueZ D-Bus)
- Phone plugin (HFP via BlueZ D-Bus, dialer, incoming call overlay)
- Web config panel (Flask, Unix socket IPC, settings/theme/plugin management)
- 3-finger gesture overlay (volume, brightness, home, day/night toggle)
- Interactive install script for RPi OS Trixie

**Known limitations / TODO:**
- D-Bus signal connection warnings for BT audio and phone plugins (non-fatal, but plugins don't receive live state updates)
- HFP call audio routing not yet wired through PipeWire
- Phone contacts not yet synced (PBAP profile)
- Touch device path hardcoded to `/dev/input/event4` — should auto-discover by VID/PID (3343:5710)
- `wlan0` IP (10.0.0.1) doesn't survive reboot — needs permanent config
- Dynamic plugin loading (.so) untested — only static plugins currently
- Launcher QML has no background when theme file is missing — bundled fallback added but UI is still bare

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
- **Touch input:** `EvdevTouchReader` (QThread) reads `/dev/input/event4` with `EVIOCGRAB` → MT Type B slot tracking → `TouchHandler` → AA `InputEventIndication` protobuf.
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
| `src/core/services/AudioService.cpp` | PipeWire stream management |
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
