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

## Current Status

**Video, touch input, and fullscreen are working on real hardware.**

- Wireless AA connection working (BT discovery → WiFi AP → TCP)
- H.264 video decoding and display at 1280x720 @ 30fps
- Multi-touch input forwarding to phone via AA Input service channel
- OS-level fullscreen when AA is connected (hides labwc taskbar)
- Touch debug overlay available (green crosshair circles with coordinates)
- Auto-navigation to AA screen on connect, back to launcher on disconnect

**Active issues:**
- Touch may need calibration (coordinate mapping needs testing)
- Audio pipeline not yet implemented
- Settings UI not yet implemented

## Architecture Notes

- **Transport:** Wireless only — BT discovery → WiFi AP → TCP. No USB/libusb.
- **AA protocol:** ASIO threads for protocol, Qt main thread for UI. Bridged via `QMetaObject::invokeMethod(Qt::QueuedConnection)`.
- **Video:** FFmpeg software H.264 decode → `QVideoSink::setVideoFrame()` → QML `VideoOutput`. Data arrives with AnnexB start codes already present — no prepending needed.
- **Video data delivery:** SPS/PPS codec config arrives as `AV_MEDIA_INDICATION` (msg ID 0x0001, no timestamp). Regular frames arrive as `AV_MEDIA_WITH_TIMESTAMP_INDICATION` (msg ID 0x0000, 8-byte timestamp). Both must be forwarded to the decoder.
- **Touch input:** QML `MultiPointTouchArea` → `TouchHandler` QObject → `InputServiceChannel::sendInputEventIndication()`. Coordinates mapped from screen space to AA touch space (1024x600).
- **Fullscreen:** `Window.FullScreen` visibility mode when AA is connected. TopBar/BottomBar hidden with `Layout.preferredHeight: 0`.
- **Config:** QSettings INI format, backward-compatible with original OAP files
- **Services:** 10 AA channels — CONTROL(0), INPUT(1), SENSOR(2), VIDEO(3), MEDIA_AUDIO(4), SPEECH_AUDIO(5), SYSTEM_AUDIO(6), AV_INPUT(7), BLUETOOTH(8), WIFI(14)
- **BT discovery:** Conditional compile (`#ifdef HAS_BLUETOOTH`), Qt6::Bluetooth RFCOMM server

## Key Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, QML engine, context properties, auto-nav on connect |
| `src/core/aa/AndroidAutoService.cpp` | AA lifecycle, TCP transport, BT integration |
| `src/core/aa/BluetoothDiscoveryService.cpp` | BT RFCOMM server, WiFi credential handshake |
| `src/core/aa/AndroidAutoEntity.cpp` | Protocol handshake, control channel |
| `src/core/aa/ServiceFactory.cpp` | Creates all service instances, wires TouchHandler to input channel |
| `src/core/aa/VideoService.cpp` | Marshals H.264 data to Qt thread (both timestamped and non-timestamped) |
| `src/core/aa/VideoDecoder.cpp` | FFmpeg decode → QVideoFrame |
| `src/core/aa/TouchHandler.hpp` | QObject bridge: QML touch events → AA InputEventIndication protobuf |
| `src/core/Configuration.cpp` | INI config with 6 enums, 13 colors, wireless settings |
| `qml/main.qml` | Root window, fullscreen toggle, TopBar/BottomBar visibility |
| `qml/applications/android_auto/AndroidAutoMenu.qml` | VideoOutput, MultiPointTouchArea, touch debug overlay |

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
