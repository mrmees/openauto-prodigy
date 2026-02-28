# OpenAuto Prodigy — Claude Code Context

## What This Is

Clean-room open-source rebuild of OpenAuto Pro (BlueWave Studio, defunct). Raspberry Pi-based **wireless-only** Android Auto head unit app using Qt 6 + QML, with a plugin-based architecture supporting BT audio, phone calls, and extensible third-party plugins.

## Hands-Off: `open-android-auto` Submodule

The proto definitions at `libs/open-androidauto/proto/` are a git submodule pointing to `https://github.com/mrmees/open-android-auto`. **Do not modify files in this submodule.** That repo is managed separately as a community resource. If proto changes are needed, note them — don't make them here.

## Workflow

This project follows a structured workflow. See `AGENTS.md` for the full loop.

- **Vision & principles:** `docs/project-vision.md`
- **Current priorities:** `docs/roadmap-current.md`
- **Session log:** `docs/session-handoffs.md`
- **Idea parking lot:** `docs/wishlist.md`
- **Documentation index:** `docs/INDEX.md`

## Design Philosophy

See `docs/project-vision.md` for design principles and `docs/design-philosophy.md` for detailed rationale.

## Current Status

See `docs/roadmap-current.md` for current status and priorities.

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
- `tests/` — Unit tests (47 tests covering config, plugins, theme, audio, events, notifications, device registry, protocol, codecs, video)
- `web-config/` — Flask web config panel (Python, HTML/CSS/JS)
- `docs/` — Design decisions, development guide, wireless setup, plans
  - `docs/aa-protocol/` — AA protocol reference, phone-side debug notes, video resolution docs
  - `docs/OpenAutoPro_archive_information/` — Archived info from the original OpenAuto Pro
- `install.sh` — Interactive installer for RPi OS Trixie

## Build & Test

```bash
mkdir build && cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure  # from build dir (47 tests)
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

**Launch / Restart:**
```bash
ssh matt@192.168.1.152 '~/openauto-prodigy/restart.sh'
# Or with SIGKILL for stuck processes:
ssh matt@192.168.1.152 '~/openauto-prodigy/restart.sh --force-kill'
```

**Note:** If cross-compiled binaries have issues (missing libs, ABI mismatch), fall back to Option B.

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
- **Video pipeline:** FFmpeg decode (hw or sw, auto-selected) on worker thread → `QVideoFrame` (via VideoFramePool or DmaBufVideoBuffer on Qt 6.8) → `QVideoSink::setVideoFrame()` → QML `VideoOutput`. Multi-codec (H.264/H.265/VP9/AV1) with per-codec decoder config.
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
| `src/core/aa/VideoDecoder.cpp` | FFmpeg multi-codec decode worker with hw/sw fallback |
| `src/core/aa/VideoFramePool.hpp` | QVideoFrame allocation pool with format caching |
| `src/core/aa/DmaBufVideoBuffer.cpp` | Qt 6.8 zero-copy DRM_PRIME video buffer |
| `src/ui/CodecCapabilityModel.cpp` | QAbstractListModel for codec/decoder selection UI |
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

### Qt / Build

- Don't use `loadFromModule()` — not available in Qt 6.4
- `QColor` needs `Qt6::Gui` link, not `Qt6::Core`
- Q_OBJECT in header-only classes needs a .cpp file listed in CMakeLists.txt for MOC
- BT code is `#ifdef HAS_BLUETOOTH` guarded — builds without qt6-connectivity-dev
- **QTimer needs `#include <QTimer>`** — forward declaration alone causes "not declared in scope" errors
- **QTimer only works on threads with a Qt event loop** — starting a QTimer from a Boost.ASIO thread silently does nothing. Use `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)` to marshal to the main thread.
- **QVideoFrame is ref-counted** — do NOT reuse frame buffers. Allocate fresh frames each decode.
- **QDBusArgument `>>` operator** cannot extract QVariantMap directly — use manual `beginMap()/endMap()` with `QDBusVariant`

### AA Protocol

- SPS/PPS arrives as `AV_MEDIA_INDICATION` (no timestamp) — must forward to decoder
- H.264 data from the protocol already has AnnexB start codes — do NOT prepend additional ones (may be outdated — verify with open-androidauto)
- WiFi SSID/password in config must match hostapd.conf exactly
- **`touch_screen_config` must be video resolution (1280x720), not display resolution (1024x600)** — the phone interprets touch coordinates relative to `touch_screen_config`. Mismatch causes touch misalignment.
- **Phone sends `config_index=3` in `AVChannelSetupRequest`** — despite our video config list only having indices 0-1. This is the phone's internal reference, not an index into our list.
- **FFmpeg `thread_count` must be 1 for real-time AA decode** — `thread_count=2` causes multi-threaded H.264 decoders to buffer frames internally, resulting in permanent EAGAIN on phones that send small P-frames.
- **Some phones output `AV_PIX_FMT_YUVJ420P` (fmt=12) instead of `AV_PIX_FMT_YUV420P` (fmt=0)** — JPEG full-range vs limited-range YUV420. Must accept both formats or frames will be silently discarded (black screen). Moto G Play 2024 outputs YUVJ420P; Samsung S25 Ultra outputs YUV420P.
- **AA `VideoConfig.margin_width/height` actually works** — the phone renders UI in a centered sub-region with black bar margins. Use for non-standard screen ratios and sidebar layouts. Margins are locked at session start. See `docs/aa-protocol/`.

### PipeWire / Audio

- **PipeWire playback: always output full periods** — set `d.chunk->size = maxSize` and silence-fill any gap. PipeWire's graph timing is fixed by quantum/rate; variable `chunk->size` values cause tempo wobble. Reporting only `bytesRead` was the cause of "skippy" audio.
- **`SPA_DICT_INIT_ARRAY` inline syntax** causes "taking address of temporary array" — use named `spa_dict_item` arrays with `SPA_DICT_INIT` instead.

### Pi / System

- **labwc `mouseEmulation="yes"`** destroys multi-touch — must be `"no"` in `~/.config/labwc/rc.xml`
- **Qt evdevtouch plugin** causes duplicate events — use direct evdev with EVIOCGRAB instead
- **WAYLAND_DISPLAY** on Pi is `wayland-0` (not `wayland-1`)
- **DFRobot USB Multi Touch** (vendor 3343:5710): 10 points, MT Type B, 0-4095 range
- **EVIOCGRAB must be toggled with AA connection state** — grab on AA connect (route touch to AA), ungrab on disconnect (return touch to Wayland/libinput). Permanent grab steals touch from the launcher UI.
- **Sidebar QML MouseArea vs EVIOCGRAB** — during AA, EVIOCGRAB steals all touch from Qt. Sidebar touch actions are handled via evdev hit zones in `EvdevTouchReader`, not QML `MouseArea`. The QML controls are visual only on Pi.
- **TCP keepalive alone won't detect dead connections on a local WiFi AP** — when the Pi IS the AP, there's no router to send RST. Must actively poll `tcp_info` via `getsockopt(IPPROTO_TCP, TCP_INFO)` and check `tcpi_backoff >= 3` (~16s detection). `tcpi_retransmits` resets between polls.
- **`<netinet/tcp.h>` and `<linux/tcp.h>` conflict** — only include `<netinet/tcp.h>` for `tcp_info`.
- **Boost.ASIO sockets don't set SOCK_CLOEXEC** — forked processes inherit the TCP acceptor FD, preventing port rebind. Must `fcntl(fd, F_SETFD, FD_CLOEXEC)` after socket open.
- **SO_REUSEADDR must be set before bind** — the Boost.ASIO 2-arg acceptor constructor does open+bind+listen in one shot, too late for socket options. Use separate open/set_option/bind/listen.
- Boost.Log truncates multiline — use `ShortDebugString()` for protobuf
- `pkill` on names >15 chars silently matches nothing — use `pkill -f`

## Hardware (Pi Target)

| Component | Details |
|-----------|---------|
| Board | Raspberry Pi 4 |
| Display | 1024x600 @ 60Hz (LTM LONTIUM, HDMI) |
| Touch | DFRobot USB Multi Touch (10-point, MT Type B, 0-4095 range) |
| WiFi | Built-in (used as AP for phone connection) |
| BT | Built-in (RFCOMM for AA discovery, A2DP sink, HFP) |
| IP | 192.168.1.152 (LAN), 10.0.0.1 (wlan0 AP) |
| OS | RPi OS Trixie, labwc compositor |
