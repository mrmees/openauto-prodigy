# Android Auto Protocol Reference — Empirical Findings

This document compiles empirical findings from protocol probing on a real Android 14 phone
(Moto G Play 2024 / Samsung S25 Ultra, AA app 13.x) connecting to OpenAuto Prodigy on
Raspberry Pi 4.

Generated from captures in `Testing/captures/` on 2026-02-23.

---

## Session Startup Sequence

### Pre-TCP: Bluetooth Discovery

Before TCP, the phone and HU negotiate via RFCOMM (see [Bluetooth Discovery](#bluetooth-discovery) below).
The BT handshake provides the phone with WiFi AP credentials. The phone then connects to the
HU's WiFi AP and initiates a TCP connection. Total BT→WiFi→TCP time: ~8-20s.

### TCP Session Handshake

Complete handshake from TCP connect to first video frame, observed from baseline capture:

| Time (s) | Direction | Message | Notes |
|-----------|-----------|---------|-------|
| 0.000 | HU→Phone | VERSION_REQUEST | Raw binary, v1.7 |
| 0.063 | Phone→HU | VERSION_RESPONSE | Phone responds v1.7 (STATUS_SUCCESS) |
| 0.063 | Phone→HU | SSL_HANDSHAKE | TLS Client Hello |
| 0.064 | HU→Phone | SSL_HANDSHAKE | TLS Server Hello + Cert |
| 0.108 | Phone→HU | SSL_HANDSHAKE | TLS Client Key Exchange + Finished |
| 0.109 | HU→Phone | AUTH_COMPLETE | TLS established |
| 0.156 | Phone→HU | SERVICE_DISCOVERY_REQUEST | 881 bytes (v1.7) |
| 0.157 | HU→Phone | SERVICE_DISCOVERY_RESPONSE | HU capabilities |
| 0.21-0.28 | Both | CHANNEL_OPEN × 9 | See channel open order below |
| 0.27-0.28 | Both | AV_SETUP × 4 | Video + 3 audio channels |
| 0.28-0.34 | Both | SENSOR_START × 4-7 | Depends on advertised sensors |
| 0.31 | Phone→HU | VIDEO_FOCUS_REQUEST | Requesting projection |
| 0.31 | HU→Phone | VIDEO_FOCUS_INDICATION | Granting projection |
| ~0.5 | Phone→HU | First video frame | H.264 IDR frame |

**Total handshake: ~500ms from TCP to first video frame.**

Note: Channel 0 (CONTROL) is implicit — it carries the handshake messages above and is never
explicitly opened via CHANNEL_OPEN. The 9 explicit opens are for channels 1-8 and 14.

### Shutdown Sequence

**HU-initiated shutdown** (app exit/restart):
1. HU sends `SHUTDOWN_REQUEST` with `reason=QUIT` on CONTROL channel
2. Waits 300ms for phone's `SHUTDOWN_RESPONSE`
3. Stops entity, closes TCP, shuts down BT discovery
4. Code: `AndroidAutoService.cpp:117-123`, `AndroidAutoEntity.cpp:74-92`

**Phone-initiated shutdown** (user exits AA on phone):
1. Phone sends `SHUTDOWN_REQUEST(reason)` on CONTROL channel
2. HU sends `SHUTDOWN_RESPONSE` (empty)
3. HU calls disconnect handler → deactivates AA plugin, shows launcher
4. Code: `AndroidAutoEntity.cpp:289-312`

**Disconnect without shutdown** (connection loss, WiFi drop):
- Connection watchdog detects via `tcp_info` polling (`tcpi_backoff >= 3`, ~16s)
- No graceful shutdown exchange — entity is forcefully stopped

### Channel Open Order (v1.7)

1. INPUT (ch1)
2. SENSOR (ch2)
3. BLUETOOTH (ch8)
4. WIFI (ch14)
5. AV_INPUT (ch7) — microphone
6. MEDIA_AUDIO (ch4)
7. VIDEO (ch3)
8. SPEECH_AUDIO (ch5)
9. SYSTEM_AUDIO (ch6)

Note: At v1.1, VIDEO opened first. v1.7 prioritizes control/input channels.

---

## Channel Architecture

### Channel Map

| ID | Name | Direction | Purpose | Message Types |
|----|------|-----------|---------|---------------|
| 0 | CONTROL | Both | Protocol control, handshake, shutdown | VERSION_REQ/RSP, SSL, AUTH, SERVICE_DISCOVERY, CHANNEL_OPEN, PING, SHUTDOWN, NAV_FOCUS, VOICE_SESSION, AUDIO_FOCUS |
| 1 | INPUT | Phone→HU (config), HU→Phone (events) | Touch + keycodes | BINDING_REQ/RSP, INPUT_EVENT_INDICATION |
| 2 | SENSOR | Both | Environmental/vehicle sensors | SENSOR_START_REQ/RSP, SENSOR_EVENT_INDICATION |
| 3 | VIDEO | Phone→HU (frames), HU→Phone (control) | H.264 video projection | AV_SETUP_REQ/RSP, AV_START/STOP, AV_MEDIA, VIDEO_FOCUS |
| 4 | MEDIA_AUDIO | Phone→HU | Music/podcast playback | AV_SETUP_REQ/RSP, AV_START/STOP, AV_MEDIA, AV_MEDIA_ACK |
| 5 | SPEECH_AUDIO | Phone→HU | Navigation voice + TTS | AV_SETUP_REQ/RSP, AV_START/STOP, AV_MEDIA, AV_MEDIA_ACK |
| 6 | SYSTEM_AUDIO | Phone→HU | UI sounds, alerts | AV_SETUP_REQ/RSP, AV_START/STOP, AV_MEDIA, AV_MEDIA_ACK |
| 7 | AV_INPUT | HU→Phone | Microphone capture | AV_INPUT_OPEN_REQ/RSP, AV_MEDIA |
| 8 | BLUETOOTH | Both | HFP pairing method | BT_PAIRING_REQ/RSP, BT_AUTH_DATA |
| 9-13 | Unknown | — | Reserved/production SDK | Not mapped in aasdk |
| 14 | WIFI | Both | In-session WiFi credentials | WIFI_CREDENTIALS_REQ/RSP |
| 255 | NONE | — | Sentinel | — |

### CONTROL Channel Message IDs

| ID | Name | Direction |
|----|------|-----------|
| 0x0001 | VERSION_REQUEST | HU→Phone |
| 0x0002 | VERSION_RESPONSE | Phone→HU |
| 0x0003 | SSL_HANDSHAKE | Both |
| 0x0004 | AUTH_COMPLETE | HU→Phone |
| 0x0005 | SERVICE_DISCOVERY_REQUEST | Phone→HU |
| 0x0006 | SERVICE_DISCOVERY_RESPONSE | HU→Phone |
| 0x0007 | CHANNEL_OPEN_REQUEST | Phone→HU |
| 0x0008 | CHANNEL_OPEN_RESPONSE | HU→Phone |
| 0x000b | PING_REQUEST | Both |
| 0x000c | PING_RESPONSE | Both |
| 0x000d | NAVIGATION_FOCUS_REQUEST | Phone→HU |
| 0x000e | NAVIGATION_FOCUS_RESPONSE | HU→Phone |
| 0x000f | SHUTDOWN_REQUEST | Both |
| 0x0010 | SHUTDOWN_RESPONSE | Both |
| 0x0011 | VOICE_SESSION_REQUEST | Phone→HU |
| 0x0012 | AUDIO_FOCUS_REQUEST | Phone→HU |
| 0x0013 | AUDIO_FOCUS_RESPONSE | HU→Phone |
| 0x0017 | Unknown | Phone→HU (observed in baseline) |

### AV Channel Message IDs (shared by VIDEO, MEDIA_AUDIO, SPEECH_AUDIO, SYSTEM_AUDIO, AV_INPUT)

| ID | Name | Direction |
|----|------|-----------|
| 0x0000 | AV_MEDIA_WITH_TIMESTAMP | Phone→HU |
| 0x0001 | AV_MEDIA_INDICATION | Phone→HU |
| 0x8000 | AV_SETUP_REQUEST | Phone→HU |
| 0x8001 | AV_START_INDICATION | Phone→HU |
| 0x8002 | AV_STOP_INDICATION | Phone→HU |
| 0x8003 | AV_SETUP_RESPONSE | HU→Phone |
| 0x8004 | AV_MEDIA_ACK | HU→Phone |
| 0x8005 | AV_INPUT_OPEN_REQUEST | Phone→HU |
| 0x8006 | AV_INPUT_OPEN_RESPONSE | HU→Phone |
| 0x8007 | VIDEO_FOCUS_REQUEST | Phone→HU |
| 0x8008 | VIDEO_FOCUS_INDICATION | HU→Phone |

---

## Bluetooth Discovery

### Overview

Wireless AA uses Bluetooth for initial discovery and WiFi credential exchange, then switches
to WiFi for the actual data connection. The sequence is: BT RFCOMM → WiFi credentials →
phone joins AP → TCP connection → SSL → AA protocol.

### RFCOMM Handshake

HU registers an SDP service with UUID `4de17a00-52cb-11e6-bdf4-0800200c9a66` (Android Auto
Wireless) and listens on an RFCOMM channel. Phone discovers HU via BT and connects.

**Message framing:** `[2-byte length (BE)][2-byte msgId (BE)][protobuf payload]`

| Step | MsgId | Direction | Proto Type | Content |
|------|-------|-----------|------------|---------|
| 1 | 1 | HU→Phone | WifiStartRequest | HU's WiFi AP IP address + TCP listener port |
| 2 | 2 | Phone→HU | WifiInfoRequest | Phone requests WiFi credentials (empty) |
| 3 | 3 | HU→Phone | WifiInfoResponse | SSID, password, BSSID (wlan0 MAC), WPA2_PERSONAL |
| 4 | 6 | Phone→HU | WifiStartResponse | Phone acknowledges (empty) |
| 5 | 7 | Phone→HU | WifiConnectionStatus | Phone reports WiFi join success/failure |

**Critical details:**
- BSSID (AP's MAC) is **required** — phone uses it to identify the correct WiFi AP
- IP discovery order: wlan0 → ap0 → first non-loopback IPv4
- Security mode: hardcoded WPA2_PERSONAL, access point type: DYNAMIC
- Code: `BluetoothDiscoveryService.cpp:125-287`

### Distinction: BT Discovery vs In-Session WiFi Channel

The RFCOMM handshake above happens **before TCP** — it bootstraps the WiFi connection.
Channel 14 (WIFI) operates **during the active AA session** and handles WiFi credential
updates (e.g., if the phone needs to re-authenticate to the AP). These are separate flows:

| Flow | When | Transport | Purpose |
|------|------|-----------|---------|
| BT RFCOMM | Before TCP | Bluetooth | Initial WiFi AP credentials |
| Ch14 WIFI | During session | TCP (AA protocol) | Credential refresh / status |

---

## Input Protocol

### Touch Events

Touch events are sent as `InputEventIndication` protobuf messages on channel 1. The format
mirrors Android's `MotionEvent`.

**Action codes:**

| Action | Value | Meaning |
|--------|-------|---------|
| DOWN | 0 | First finger touches screen |
| UP | 1 | Last finger lifted |
| MOVE | 2 | Any active finger moved |
| POINTER_DOWN | 5 | Additional finger down (multi-touch) |
| POINTER_UP | 6 | One finger lifted, others remain |

**Pointer format:** Each message includes ALL active pointers, not just the changed one.

```
TouchEvent {
  touch_action: <action code>
  action_index: <array index of changed pointer>
  pointer[]: { x, y, pointer_id }
}
```

- `action_index` = array index into pointer list, NOT the pointer ID
- `pointer_id` = stable per-finger identifier (slot index from evdev MT Type B)
- For UP events, the lifted finger must be included at its last position

**Coordinate space:**

Touch coordinates are sent in **content space** — the visible AA rendering area after
margin/sidebar crops, NOT the full video frame resolution and NOT the physical display
resolution.

- Evdev raw range (0-4095) → normalized (0.0-1.0) → content dimensions
- `touch_screen_config` in ServiceDiscoveryResponse must match content dimensions
- If sidebar is active, content width/height are reduced by the sidebar margin
- See `docs/aa-display-rendering.md` for detailed mapping math

Code: `EvdevTouchReader.cpp:176-194` (mapping), `TouchHandler.hpp:30-103` (protobuf)

### Keycodes

Supported keycodes advertised in Input service config:

| Keycode | Value | Purpose |
|---------|-------|---------|
| HOME | 3 | Return to AA home screen |
| BACK | 4 | Navigate back |
| MICROPHONE | 84 | Trigger Google Assistant |

Code: `ServiceFactory.cpp:293-296`

### Input Binding

Phone sends `BINDING_REQUEST` after input channel opens, containing touch configuration
(touchpad vs touchscreen mode, supported actions). HU responds with `BINDING_RESPONSE`.
Observed payload: `0a 03 03 04 54` — touchscreen mode with multi-touch support.

---

## Version Negotiation (Probe 1)

**Finding:** Phone always responds v1.7 regardless of what HU requests.

| HU Request | Phone Response | Phone Stores |
|------------|---------------|-------------|
| v1.1 | v1.7 | headUnitProtocolVersion=1.1 |
| v1.7 | v1.7 | headUnitProtocolVersion=1.7 |

- SERVICE_DISCOVERY_REQUEST grows from 841→881 bytes at v1.7 (+40 bytes)
- Channel open order changes at v1.7 (control channels prioritized)
- **Recommendation:** Always request v1.7

---

## Service Discovery

### ServiceDiscoveryResponse Fields

| Field | # | Type | Status | Notes |
|-------|---|------|--------|-------|
| channels | 1 | repeated ChannelDescriptor | Active | Service advertisements |
| head_unit_name | 2 | string | Deprecated | Phone reads headunit_info instead |
| car_model | 3 | string | Deprecated | |
| car_year | 4 | string | Deprecated | |
| car_serial | 5 | string | Deprecated | |
| left_hand_drive | 6 | bool | Active | |
| headunit_manufacturer | 7 | string | Deprecated | |
| headunit_model | 8 | string | Deprecated | |
| sw_build | 9 | string | Deprecated | |
| sw_version | 10 | string | Deprecated | |
| can_play_native_media_during_vr | 11 | bool | Deprecated | |
| hide_clock | 12 | bool | **DEAD** | No effect on modern AA (Probe 5) |
| session_configuration | 13 | int32 | Unknown | Purpose unclear |
| display_name | 14 | string | Active | |
| probe_for_support | 15 | bool | Active | |
| connection_configuration | 16 | ConnectionConfig | Active | Ping timeouts etc |
| headunit_info | 17 | HeadUnitInfo | Active | Modern identity fields |

### HeadUnitInfo Fields (all strings)

| Field | # | Description |
|-------|---|-------------|
| make | 1 | Vehicle manufacturer |
| model | 2 | Vehicle model |
| year | 3 | Vehicle year |
| vehicle_id | 4 | Vehicle serial/ID |
| head_unit_make | 5 | HU manufacturer |
| head_unit_model | 6 | HU model |
| head_unit_software_build | 7 | Git hash / build ID |
| head_unit_software_version | 8 | Version string |

**Missing:** No theme_version or palette_version field in any known proto definition (fields
1-17 in SDR, fields 1-8 in HeadUnitInfo). Phone logs imply HU palette version is read as 0;
exact field source is still unknown. May be an undocumented field 18+ or set at runtime.

### CarInfoInternal (Phone's Internal View)

The phone maintains a `CarInfoInternal` object per paired HU, logged during BT setup:

```
CarInfoInternal[
  manufacturer=OpenAuto Project,
  model=Universal,
  headUnitProtocolVersion=1.7,
  modelYear=2026,
  vehicleId=e29d78b9...,
  bluetoothAllowed=true,
  hideProjectedClock=false,    // NOT affected by hide_clock field
  driverPosition=0,
  headUnitMake=OpenAuto Project,
  headUnitModel=Raspberry Pi 4,
  headUnitSoftwareBuild=b6a5995,
  headUnitSoftwareVersion=0.3.0,
  canPlayNativeMediaDuringVr=false,
  hidePhoneSignal=false,
  hideBatteryLevel=false
]
```

### CarDisplayUiFeatures

Phone computes display UI features from the video config:

```
CarDisplayUiFeatures{
  resizeType=0,
  hasClock=false,              // HU doesn't have native clock
  hasBatteryLevel=false,       // HU doesn't show battery
  hasPhoneSignal=false,        // HU doesn't show signal
  hasNativeUiAffordance=false,
  hasClusterTurnCard=false
}
```

When `hasClock=false`, phone renders its own status bar with clock, battery, signal.
This is controlled by the video config's UiConfig, NOT by the deprecated `hide_clock` field.

### carModuleFeaturesCache

Phone logs a feature set including:
```
HERO_THEMING, COOLWALK, INDEPENDENT_NIGHT_MODE, NATIVE_APPS,
MULTI_DISPLAY, HERO_CAR_CONTROLS, HERO_CAR_LOCAL_MEDIA,
ENHANCED_NAVIGATION_METADATA, ASSISTANT_Z, MULTI_REGION,
PREFLIGHT, CONTENT_WINDOW_INSETS, GH_DRIVEN_RESIZING
```

These are phone-side feature flags, not HU-advertised. Useful for understanding what the
phone thinks it supports.

---

## Video Pipeline

### Resolution Support (Probe 7)

| Resolution | Enum | Dimensions | Phone Layout | Status |
|------------|------|-----------|-------------|--------|
| 480p | `_480p` | 800×480 | `sw750dp` | Tested, working |
| 720p | `_720p` | 1280×720 | Not tested | Expected to work (default) |
| 1080p | `_1080p` | 1920×1080 | `sw2194dp xlrg` | Tested, working |

**Key findings:**
- 1080p triggers "xlrg" layout classification (vs "normal" at 480p)
- Phone dynamically adjusts UI layout density for resolution
- Display dimensions include margin calculations (e.g., 1920×984 with sidebar margins)
- 480p fallback config is always advertised alongside preferred resolution
- 1080p software decode on Pi 4 is CPU-intensive — 720p is the production sweet spot

### Video Setup Exchange

- Phone sends `AV_SETUP_REQUEST` with `config_index=3` — phone's internal reference, NOT our config list index
- HU responds with `max_unacked=10` and `configs(0)` pointing to primary resolution
- Video focus grant triggers first IDR frame within ~200ms

### Margin Negotiation

`VideoConfig.margin_width` / `margin_height` creates a centered AA content region with
black bar margins. Used for sidebar layout on non-standard aspect ratios. Margins are
locked at session start (part of ServiceDiscoveryResponse). See `docs/aa-display-rendering.md`.

---

## Audio Pipeline

### Audio Configuration

| Stream | Channel | Sample Rate | Channels | Bit Depth | Direction |
|--------|---------|-------------|----------|-----------|-----------|
| Media | 4 | 48kHz | Stereo | 16-bit | Phone→HU |
| Speech/Nav | 5 | 48kHz | Mono | 16-bit | Phone→HU |
| System | 6 | 16kHz | Mono | 16-bit | Phone→HU |
| Microphone | 7 | 16kHz | Mono | 16-bit | HU→Phone |

### 48kHz Speech Audio (Probe 2)

- Frame size doubled: 2058→4106 bytes (exactly 2x for 48kHz vs 16kHz mono)
- Phone logs misleading: "Failed to find 48kHz guidance config, fallback to 16KHz" — but ACTUALLY captures at 48kHz
- Phone confirms: `Start capturing system audio with sampling rate: 48000`
- TTS latency: `audioPipelineLatency: [196, 147.38] ms` — reasonable
- MAX_UNACK: 1 (single outstanding frame, flow-controlled)

### ALARM Audio Type

`AudioType::ALARM` (4) exists in the proto but cannot be tested without knowing its
channel ID (likely in 9-13 range, undocumented in aasdk). Wrong channel would break session.

---

## Sensor Channel

### Supported Sensors

| Sensor | Enum Value | Requested by Phone | Reporting Mode | Data Source |
|--------|-----------|-------------------|----------------|-------------|
| LOCATION | 1 (0x01) | Yes (twice) | 0x00 + 0x01 | No GPS — placeholder |
| COMPASS | 2 (0x02) | Yes (Probe 8) | 0x03 (IMU) | No data — placeholder |
| NIGHT_DATA | 10 (0x0a) | Yes | 0x00 (standard) | ThemeService day/night |
| DRIVING_STATUS | 13 (0x0d) | Yes | 0x00 (standard) | Always UNRESTRICTED |
| ACCEL | 19 (0x13) | Yes (Probe 8) | 0x03 (IMU) | No data — placeholder |
| GYRO | 20 (0x14) | Advertised, NOT requested | — | — |

### Sensor Request Payload Format

```
08 <type> 10 <mode>
```
- `type`: SensorType enum value
- `mode`: Reporting mode/interval
  - `00` = standard (NIGHT_DATA, DRIVING_STATUS)
  - `01` = high frequency (LOCATION first request)
  - `03` = IMU mode (COMPASS, ACCEL)

### Night Mode (Probe 3)

Night mode sensor push works correctly:
```
SENSOR_EVENT_INDICATION: 6a 02 08 00   → night=false (day mode)
```
Phone processes:
```
CAR.SENSOR: isNightModeEnabled DAY_NIGHT_MODE_VAL_CAR returning car day night sensor night=false
CAR.SYS: setting night mode false
CAR.WM: Updating video configuration. isNightMode: false
```

Night mode and palette theming are related but separate paths — night mode controls
light/dark theme switching, palette version controls Material You dynamic color application.

---

## Theming / Palette (Probe 4) — UNRESOLVED

Phone logs:
```
GH.CarThemeVersionLD: Updating theming version to: 0
GH.ThemingManager: Not updating theme, palette version doesn't match.
  Current version: 2, HU version: 0
```

Phone logs imply HU palette version is read as 0 from an undiscovered field. No
`theme_version` or `palette_version` field exists in any known proto definition (checked
aasdk, GAL docs, aa-proxy-rs). Likely an undocumented extension to ServiceDiscoveryResponse
(field 18+), HeadUnitInfo (field 9+), or set at runtime via UpdateUiConfigRequest.

Impact: Without palette v2, Material You dynamic colors from HU are not applied.
Phone falls back to default Google Maps theming. Cosmetic, not functional.

---

## Protocol Wire Format

### Message Size by Type

| Message | Typical Size | Notes |
|---------|-------------|-------|
| VERSION_REQUEST | 6 bytes | Raw binary |
| VERSION_RESPONSE | 6 bytes | Raw binary |
| SSL_HANDSHAKE | 200-800 bytes | TLS records |
| SERVICE_DISCOVERY_REQUEST | 881 bytes (v1.7) | Grows with version |
| SERVICE_DISCOVERY_RESPONSE | ~600-800 bytes | Depends on services |
| CHANNEL_OPEN_REQUEST | ~4 bytes | Minimal |
| CHANNEL_OPEN_RESPONSE | ~4 bytes | Minimal |
| SENSOR_START_REQUEST | 6 bytes | Type + mode |
| SENSOR_START_RESPONSE | 4 bytes | Status only |
| SENSOR_EVENT_INDICATION | 6 bytes | Sensor-specific payload |
| AV_SETUP_REQUEST | 4 bytes | Config index |
| AV_SETUP_RESPONSE | 8 bytes | Status + config |
| Video frames | 1000-16000+ bytes | H.264, varies |
| Audio frames | 2058-4106 bytes | PCM, rate-dependent |

---

## Probe Results Summary

| # | Probe | Change | Outcome | Capture |
|---|-------|--------|---------|---------|
| 1 | Version v1.7 | AASDK_MINOR=7 | Phone negotiates v1.7, +40B in SDR, channel order changes. **Kept.** | `probe-1-version-bump/` |
| 2 | 48kHz Speech | Speech sample rate 16→48kHz | Frame size doubles, phone captures 48kHz despite "fallback" log. **Kept.** | `probe-2-48khz-speech/` |
| 3 | Night Mode | Existing sensor push | Sensor works, phone applies day/night. Palette v2 mismatch discovered. | `probe-3-night-mode/` |
| 4 | Palette v2 | Find theme_version field | **BLOCKED** — field not in any known proto. Needs raw proto capture from production HU. | `probe-4-palette-v2/` |
| 5 | hide_clock | SDR field 12 = true | **DEAD** — no effect on modern AA. Use CarDisplayUiFeatures instead. | `probe-5-8-hideclock-sensors/` |
| 6 | ALARM Audio | Find ALARM channel ID | **SKIPPED** — channel IDs 9-13 unmapped, wrong channel breaks session. | `probe-6-alarm-audio/` |
| 7 | 1080p Video | Config change to 1080p | Works, phone renders xlrg layout at 1920x984. Pi 4 CPU-intensive. | `probe-7-1080p/` |
| 8 | Extra Sensors | +COMPASS, +ACCEL, +GYRO | Phone requests COMPASS + ACCEL, ignores GYRO. IMU mode suffix 0x03. **Kept.** | `probe-5-8-hideclock-sensors/` |

---

## Permanent Code Changes

Changes from protocol exploration (now on main):

| Change | File | Validated By |
|--------|------|-------------|
| Protocol version v1.7 | `libs/aasdk/include/aasdk/Version.hpp` | Probe 1 |
| 48kHz speech audio | `src/core/aa/ServiceFactory.cpp` | Probe 2 |
| Extra sensors (COMPASS, ACCEL, GYRO) | `src/core/aa/ServiceFactory.cpp` | Probe 8 |
| ProtocolLogger | `src/core/aa/ProtocolLogger.{hpp,cpp}` | All captures |
| Universal CHANNEL_OPEN recognition | `src/core/aa/ProtocolLogger.cpp` | Baseline |

---

## Open Questions

1. **Palette version field number** — where does the phone read theme_version from? Needs raw protobuf unknown-field capture from production HU.
2. **GYRO sensor** — advertised but never requested. May require accelerometer data first, or only used during active navigation (dead reckoning).
3. **CarDisplayUiFeatures** — how to set `hasClock=true`. Likely via UiConfig in VideoConfig, not the deprecated hide_clock field.
4. **Channels 9-13** — service-to-channel mapping for production SDK services. Needs passive channel-ID discovery from traces.
5. **Session configuration** — purpose of field 13 (int32) in ServiceDiscoveryResponse.
6. **Live night mode toggle** — need to test night=true push during active session and verify real-time theme switch.
7. **ALARM audio channel** — which channel ID does it use? Cannot test without channel map.
8. **UpdateUiConfigRequest** — runtime UI reconfiguration (post-session-start), untested.
9. **1080p sustained performance** — short capture only. Need thermal/CPU profiling under load. Consider adaptive resolution policy.
10. **48kHz system audio** — would bumping channel 6 to 48kHz also work? Low priority.
11. **ACCEL/COMPASS data streaming** — phone requests these sensors but we send no data. Implement with realistic timing and conformance checks.
12. **Control message 0x0017** — observed in baseline capture, unmapped. Purpose unknown.
