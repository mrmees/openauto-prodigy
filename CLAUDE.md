# OpenAuto Prodigy — Claude Code Context

## What This Is

Clean-room open-source rebuild of OpenAuto Pro (BlueWave Studio, defunct). Raspberry Pi-based Android Auto head unit app using Qt 6 + QML.

## Repository Layout

- `src/` — C++ source (core AA protocol + Qt UI controllers)
- `qml/` — QML UI files
- `libs/aasdk/` — Android Auto SDK (git submodule, SonOfGib's fork)
- `tests/` — Unit tests (currently 2: configuration, theme_controller)
- `docs/` — Design decisions, development guide, debugging notes

## Build & Test

```bash
mkdir build && cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure  # from build dir
```

**Dependencies:** See `docs/development.md` for full package list.

## Dual-Platform Build

Code must compile on both:
- **Qt 6.4** (WSL2 Ubuntu 24.04) — dev builds
- **Qt 6.8** (RPi OS Trixie) — target platform

Key incompatibilities are documented in `docs/development.md` (QML loading, resource paths, type flattening).

## Current Status

**Phase 1, Task 6 (video pipeline):** Code complete, builds clean. Blocked on a protocol-level issue where the phone completes the handshake but never opens service channels. See `docs/debugging-notes.md` for full details.

Tasks 7-10 (audio, wireless, settings, integration) are pending.

## Architecture Notes

- **AA protocol:** ASIO threads for protocol, Qt main thread for UI. Bridged via `QMetaObject::invokeMethod(Qt::QueuedConnection)`.
- **Video:** FFmpeg software H.264 decode → `QVideoSink::setVideoFrame()` → QML `VideoOutput`
- **Config:** QSettings INI format, backward-compatible with original OAP files
- **Services:** 8 AA channels (video, 3 audio, input, sensor, bluetooth, wifi)

## Key Files

| File | Purpose |
|------|---------|
| `src/core/aa/AndroidAutoService.cpp` | AA lifecycle, USB/TCP transport |
| `src/core/aa/AndroidAutoEntity.cpp` | Protocol handshake, control channel |
| `src/core/aa/ServiceFactory.cpp` | Creates all 8 service instances |
| `src/core/aa/VideoService.cpp` | Marshals H.264 data to Qt thread |
| `src/core/aa/VideoDecoder.cpp` | FFmpeg decode → QVideoFrame |
| `src/core/Configuration.cpp` | INI config with 6 enums, 13 colors |

## Gotchas

- Don't use `loadFromModule()` — not available in Qt 6.4
- `QColor` needs `Qt6::Gui` link, not `Qt6::Core`
- Boost.Log truncates multiline — use `ShortDebugString()` for protobuf
- SonOfGib aasdk is proto2 — no `device_model` field in ServiceDiscoveryRequest
- Phone state degrades after rapid USB reconnects — may need AA cache clear
