# OpenAuto Prodigy — Claude Code Context

## What This Is

Clean-room open-source rebuild of OpenAuto Pro (BlueWave Studio, defunct). Raspberry Pi-based **wireless-only** Android Auto head unit app using Qt 6 + QML.

## Repository Layout

- `src/` — C++ source (core AA protocol + Qt UI controllers)
- `qml/` — QML UI files
- `libs/aasdk/` — Android Auto SDK (git submodule, SonOfGib's fork)
- `tests/` — Unit tests (currently 2: configuration, theme_controller)
- `docs/` — Design decisions, development guide, wireless setup

## Build & Test

```bash
mkdir build && cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure  # from build dir
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

**Video, touch input, and fullscreen are working on real hardware.**

- Wireless AA connection working (BT discovery → WiFi AP → TCP)
- H.264 video decoding and display at 1280x720 @ 60fps (negotiated)
- Actual throughput: ~50-60fps decode, 4.5-8.5ms per frame on Pi 4
- Multi-touch input via direct evdev reader (bypasses Qt/Wayland entirely)
- Touch latency: 0.2-0.4ms (evdev → AA protocol send)
- Proper aspect ratio with letterboxing (PreserveAspectFit, black bars)
- Debug touch overlay (green crosshair circles with AA coordinates)
- OS-level fullscreen when AA is connected (hides labwc taskbar)
- Auto-navigation to AA screen on connect, back to launcher on disconnect

**Active issues / TODO:**
- Audio pipeline not yet implemented
- Settings UI not yet implemented
- Touch device path hardcoded to `/dev/input/event4` (should be configurable)
- `wlan0` IP (10.0.0.1) doesn't survive reboot — needs permanent config
- Debug touch overlay is always on (should be togglable from settings)

## Architecture Notes

- **Transport:** Wireless only — BT discovery → WiFi AP → TCP. No USB/libusb.
- **AA protocol:** ASIO threads for protocol, Qt main thread for UI. Bridged via `QMetaObject::invokeMethod(Qt::QueuedConnection)`.
- **Video pipeline:** FFmpeg software H.264 decode on dedicated worker thread → fresh `QVideoFrame` per decode → `QVideoSink::setVideoFrame()` via QueuedConnection → QML `VideoOutput` (PreserveAspectFit).
- **Video data delivery:** SPS/PPS codec config arrives as `AV_MEDIA_INDICATION` (msg ID 0x0001, no timestamp). Regular frames arrive as `AV_MEDIA_WITH_TIMESTAMP_INDICATION` (msg ID 0x0000, 8-byte timestamp). Both must be forwarded to the decoder.
- **Touch input:** `EvdevTouchReader` (QThread) reads `/dev/input/event4` directly with `EVIOCGRAB` → tracks MT Type B slot state → `TouchHandler::sendTouchIndication()` → ASIO strand → `InputServiceChannel::sendInputEventIndication()`. No QML touch handling.
- **Touch coordinate mapping:** Evdev (0-4095) → AA coordinates (1280x720, matching video resolution). Letterbox-aware: computes video area within physical display (1024x600) and maps touches only to the video region.
- **Aspect ratio:** Video is 1280x720 (16:9), display is 1024x600 (~17:10). `PreserveAspectFit` renders at 1024x576 with 12px black bars top/bottom.
- **Fullscreen:** `Window.FullScreen` visibility mode when AA is connected. TopBar/BottomBar hidden with `Layout.preferredHeight: 0`.
- **Config:** QSettings INI format, backward-compatible with original OAP files
- **Services:** 10 AA channels — CONTROL(0), INPUT(1), SENSOR(2), VIDEO(3), MEDIA_AUDIO(4), SPEECH_AUDIO(5), SYSTEM_AUDIO(6), AV_INPUT(7), BLUETOOTH(8), WIFI(14)
- **BT discovery:** Conditional compile (`#ifdef HAS_BLUETOOTH`), Qt6::Bluetooth RFCOMM server
- **Performance instrumentation:** `PerfStats.hpp` utility, 5-second rolling averages logged for video decode and touch pipelines.

## Key Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, QML engine, context properties, EvdevTouchReader startup, auto-nav on connect |
| `src/core/aa/AndroidAutoService.cpp` | AA lifecycle, TCP transport, BT integration |
| `src/core/aa/BluetoothDiscoveryService.cpp` | BT RFCOMM server, WiFi credential handshake |
| `src/core/aa/AndroidAutoEntity.cpp` | Protocol handshake, control channel |
| `src/core/aa/ServiceFactory.cpp` | Creates all service instances, wires TouchHandler to input channel |
| `src/core/aa/VideoService.cpp` | Marshals H.264 data to decode worker thread |
| `src/core/aa/VideoDecoder.cpp` | FFmpeg decode worker thread → QVideoFrame |
| `src/core/aa/EvdevTouchReader.cpp` | Direct Linux evdev multi-touch reader (MT Type B) |
| `src/core/aa/EvdevTouchReader.hpp` | Slot tracking, letterbox-aware coordinate mapping |
| `src/core/aa/TouchHandler.hpp` | Bridge: touch events → AA InputEventIndication protobuf, debug overlay properties |
| `src/core/aa/PerfStats.hpp` | Lightweight timing metrics (rolling min/max/avg) |
| `src/core/Configuration.cpp` | INI config with 6 enums, 13 colors, wireless settings |
| `qml/main.qml` | Root window, fullscreen toggle, TopBar/BottomBar visibility |
| `qml/applications/android_auto/AndroidAutoMenu.qml` | VideoOutput (PreserveAspectFit), debug touch overlay, status display |

## AA Protocol: Touch Events

Based on Android MotionEvent. Critical details learned the hard way:

- **Actions:** DOWN(0), UP(1), MOVE(2), POINTER_DOWN(5), POINTER_UP(6)
- **ALL active pointers** must be included in every message (not just the changed finger)
- **`action_index`** is the ARRAY INDEX into the pointers list, not the pointer ID
- **`pointer_id`** is a stable per-finger identifier (we use slot index)
- **Touch coordinates must be in VIDEO RESOLUTION space** (1280x720), NOT the `touch_screen_config` dimensions (1024x600). The phone maps touch coords to the video it's rendering.
- **For UP events:** include the lifted finger in the pointer array at its last position
- **Reference implementation:** harryjph/android-auto-headunit (confirmed protocol format)

## AA Protocol: Video Resolutions

AA supports fixed resolutions only (from `VideoResolutionEnum.proto`):
- `_480p` (800x480)
- `_720p` (1280x720) — current default
- `_1080p` (1920x1080)
- `_720p_p`, `_1080pp` — portrait variants

No custom resolutions (e.g. 1024x600) available.

## Gotchas

- Don't use `loadFromModule()` — not available in Qt 6.4
- `QColor` needs `Qt6::Gui` link, not `Qt6::Core`
- Boost.Log truncates multiline — use `ShortDebugString()` for protobuf
- SonOfGib aasdk is proto2 — no `device_model` field in ServiceDiscoveryRequest
- BT code is `#ifdef HAS_BLUETOOTH` guarded — builds without qt6-connectivity-dev
- WiFi SSID/password in config must match hostapd.conf exactly
- SPS/PPS arrives as `AV_MEDIA_INDICATION` (no timestamp) — must forward to decoder, not discard
- H.264 data from aasdk already has AnnexB start codes — do NOT prepend additional ones
- Q_OBJECT in header-only classes needs a .cpp file listed in CMakeLists.txt for MOC
- `pkill` on names >15 chars silently matches nothing — use `pkill -f` for long binary names
- **QVideoFrame is ref-counted** — do NOT reuse frame buffers (double-buffer scheme). Qt's render thread holds read mappings; allocate fresh frames each decode to avoid "Cannot map in ReadOnly mode" crashes.
- **labwc `mouseEmulation="yes"`** destroys multi-touch — converts to single mouse pointer. Must be `"no"` in `~/.config/labwc/rc.xml` on the Pi.
- **Qt evdevtouch plugin** (`QT_QPA_GENERIC_PLUGINS`) causes duplicate events even with `grab=1`. Don't use it — use direct evdev reading with EVIOCGRAB instead.
- **WAYLAND_DISPLAY** on Pi is `wayland-0` (not `wayland-1`). Check `ls /run/user/1000/wayland-*` if unsure.
- **DFRobot USB Multi Touch** (vendor 3343:5710): 10 touch points, MT Type B, axis range 0-4095 for X and Y.

## Hardware (Pi Target)

| Component | Details |
|-----------|---------|
| Board | Raspberry Pi 4 |
| Display | 1024x600 @ 60Hz (LTM LONTIUM, HDMI) |
| Touch | DFRobot USB Multi Touch (10-point, MT Type B, 0-4095 range) |
| WiFi | Built-in (used as AP for phone connection) |
| BT | Built-in (RFCOMM for AA discovery) |
| IP | 192.168.1.149 (LAN), 10.0.0.1 (wlan0 AP) |
| OS | RPi OS Trixie, labwc compositor |
