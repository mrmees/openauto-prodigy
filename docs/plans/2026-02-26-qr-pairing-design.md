# QR Code Pairing — Design

## Goal

Replace manual PIN entry with a scannable QR code displayed on the head unit. The companion app scans the QR to get PIN + connection info in one step. PIN text remains as a fallback.

## QR Payload

Custom URI scheme for deep-linking:

```
openauto://pair?pin=482917&host=10.0.0.1&port=9876
```

- `pin` — 6-digit pairing PIN (same as today)
- `host` — Pi's WiFi AP IP (from `wlan0`, default `10.0.0.1`)
- `port` — companion listener port (from config, default `9876`)

## QR Library

[qrcodegen](https://github.com/nayuki/QR-Code-generator) — single C++ header file, MIT license, zero dependencies. Vendored into `libs/qrcodegen/`. Generates a boolean matrix from a string.

## Rendering

SVG string built in C++. Each dark module becomes a `<rect>` element. SVG scales cleanly at any resolution and QML `Image` renders it natively. Exposed to QML as a `data:image/svg+xml;base64,...` URI via Q_PROPERTY.

## Components

### CompanionListenerService (C++)

- New Q_PROPERTY: `QString qrCodeDataUri` (READ, NOTIFY)
- New private method: `QString generateQrSvg(const QString& payload)` — qrcodegen → SVG → base64
- `generatePairingPin()` updated to also build the URI payload and set `qrCodeDataUri`
- Host IP sourced from existing config or hardcoded `10.0.0.1` (WiFi AP)
- Port sourced from existing `companion.port` config

### CompanionSettings.qml

- QR code `Image` displayed above the PIN text when `qrCodeDataUri` is non-empty
- Same "Generate Pairing PIN" button triggers both QR + PIN
- PIN text kept below QR as fallback with hint: "Or enter this PIN manually"

### Vendored Library

- `libs/qrcodegen/qrcodegen.hpp` — single header, added to include path in CMakeLists.txt

## What Doesn't Change

- PIN generation (6-digit random, SHA256 derivation)
- HMAC challenge-response authentication
- `~/.openauto/companion.key` storage
- Companion TCP listener
- Web config panel
