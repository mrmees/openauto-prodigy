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
- **Qt 6.4** (WSL2 Ubuntu 24.04) — dev builds (no Bluetooth)
- **Qt 6.8** (RPi OS Trixie) — target platform (full Bluetooth support)

Key incompatibilities are documented in `docs/development.md` (QML loading, resource paths, type flattening).

## Current Status

**Wireless-only pivot complete.** USB code removed, Bluetooth discovery service added, TCP listener on configurable port (default 5288).

**Next:** Test on Pi with hostapd WiFi AP + Bluetooth pairing. See `docs/wireless-setup.md`.

Video pipeline code is written but untested on live video (same state as before the wireless pivot).

## Architecture Notes

- **Transport:** Wireless only — BT discovery → WiFi AP → TCP. No USB/libusb.
- **AA protocol:** ASIO threads for protocol, Qt main thread for UI. Bridged via `QMetaObject::invokeMethod(Qt::QueuedConnection)`.
- **Video:** FFmpeg software H.264 decode → `QVideoSink::setVideoFrame()` → QML `VideoOutput`
- **Config:** QSettings INI format, backward-compatible with original OAP files
- **Services:** 10 AA channels — CONTROL(0), INPUT(1), SENSOR(2), VIDEO(3), MEDIA_AUDIO(4), SPEECH_AUDIO(5), SYSTEM_AUDIO(6), AV_INPUT(7), BLUETOOTH(8), WIFI(14)
- **BT discovery:** Conditional compile (`#ifdef HAS_BLUETOOTH`), Qt6::Bluetooth RFCOMM server

## Key Files

| File | Purpose |
|------|---------|
| `src/core/aa/AndroidAutoService.cpp` | AA lifecycle, TCP transport, BT integration |
| `src/core/aa/BluetoothDiscoveryService.cpp` | BT RFCOMM server, WiFi credential handshake |
| `src/core/aa/AndroidAutoEntity.cpp` | Protocol handshake, control channel |
| `src/core/aa/ServiceFactory.cpp` | Creates all 8 service instances |
| `src/core/aa/VideoService.cpp` | Marshals H.264 data to Qt thread |
| `src/core/aa/VideoDecoder.cpp` | FFmpeg decode → QVideoFrame |
| `src/core/Configuration.cpp` | INI config with 6 enums, 13 colors, wireless settings |

## Gotchas

- Don't use `loadFromModule()` — not available in Qt 6.4
- `QColor` needs `Qt6::Gui` link, not `Qt6::Core`
- Boost.Log truncates multiline — use `ShortDebugString()` for protobuf
- SonOfGib aasdk is proto2 — no `device_model` field in ServiceDiscoveryRequest
- BT code is `#ifdef HAS_BLUETOOTH` guarded — builds without qt6-connectivity-dev on WSL2
- WiFi SSID/password in config must match hostapd.conf exactly
