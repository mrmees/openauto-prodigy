# Android Auto Protocol Reference — Empirical Findings

This document compiles empirical findings from protocol probing on a real Android 14 phone
(Pixel 7 Pro, Google Maps, AA app 13.x) connecting to OpenAuto Prodigy on Raspberry Pi 4.

Generated from captures in `Testing/captures/` on 2026-02-23.

---

## Session Startup Sequence

Complete handshake from TCP connect to first video frame, observed from baseline capture:

| Time (s) | Direction | Message | Notes |
|-----------|-----------|---------|-------|
| 0.000 | HU→Phone | VERSION_REQUEST | Raw binary, v1.7 (after version bump) |
| 0.063 | Phone→HU | VERSION_RESPONSE | Phone responds v1.7 (STATUS_SUCCESS) |
| 0.063 | Phone→HU | SSL_HANDSHAKE | TLS Client Hello |
| 0.064 | HU→Phone | SSL_HANDSHAKE | TLS Server Hello + Cert |
| 0.108 | Phone→HU | SSL_HANDSHAKE | TLS Client Key Exchange + Finished |
| 0.109 | HU→Phone | AUTH_COMPLETE | TLS established |
| 0.156 | Phone→HU | SERVICE_DISCOVERY_REQUEST | 881 bytes (v1.7) |
| 0.157 | HU→Phone | SERVICE_DISCOVERY_RESPONSE | HU capabilities |
| 0.21-0.28 | Both | CHANNEL_OPEN × 10 | See channel open order below |
| 0.27-0.28 | Both | AV_SETUP × 4 | Video + 3 audio channels |
| 0.28-0.34 | Both | SENSOR_START × 4-7 | Depends on advertised sensors |
| 0.31 | Phone→HU | VIDEO_FOCUS_REQUEST | Requesting projection |
| 0.31 | HU→Phone | VIDEO_FOCUS_INDICATION | Granting projection |
| ~0.5 | Phone→HU | First video frame | H.264 IDR frame |

**Total handshake: ~500ms from TCP to first video frame.**

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

Note: At v1.1, VIDEO opened first. v1.7 prioritizes control channels.

---

## Version Negotiation (Probe 1)

**Finding:** Phone always responds v1.7 regardless of what HU requests.

| HU Request | Phone Response | Phone Stores |
|------------|---------------|-------------|
| v1.1 | v1.7 | headUnitProtocolVersion=1.1 |
| v1.7 | v1.7 | headUnitProtocolVersion=1.7 |

- SERVICE_DISCOVERY_REQUEST grows from 841→881 bytes at v1.7 (+40 bytes)
- Channel open order changes at v1.7
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

**Missing:** No theme_version or palette_version field in any known proto definition.
Phone expects palette v2 but reads default 0 from an undiscovered field.

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
| 720p | `_720p` | 1280×720 | Not tested | Expected to work |
| 1080p | `_1080p` | 1920×1080 | `sw2194dp xlrg` | Tested, working |

**Key findings:**
- 1080p triggers "xlrg" layout classification (vs "normal" at 480p)
- Phone dynamically adjusts UI layout density for resolution
- Display dimensions include margin calculations (e.g., 1920×984 with sidebar margins)
- 480p fallback config is always advertised alongside preferred resolution

### Video Setup Exchange

- Phone sends `AV_SETUP_REQUEST` with `config_index=3` — phone's internal reference, NOT our config list index
- HU responds with `max_unacked=10` and `configs(0)` pointing to primary resolution
- Video focus grant triggers first IDR frame within ~200ms

### Margin Negotiation

`VideoConfig.margin_width` / `margin_height` creates a centered AA content region with
black bar margins. Used for sidebar layout on non-standard aspect ratios. Margins are
locked at session start (part of ServiceDiscoveryResponse).

---

## Audio Pipeline

### Audio Configuration

| Stream | Channel | Sample Rate | Channels | Status |
|--------|---------|-------------|----------|--------|
| Media | 4 | 48kHz | Stereo | Working |
| Speech/Nav | 5 | 48kHz | Mono | **Bumped from 16kHz (Probe 2)** |
| System | 6 | 16kHz | Mono | Working |
| Microphone | 7 | 16kHz | Mono (input) | Working |

### 48kHz Speech Audio (Probe 2)

- Frame size doubled: 2058→4106 bytes (exactly 2x for 48kHz vs 16kHz mono)
- Phone logs misleading: "Failed to find 48kHz guidance config, fallback to 16KHz" — but ACTUALLY captures at 48kHz
- Phone confirms: `Start capturing system audio with sampling rate: 48000`
- No regressions — TTS latency unchanged

### ALARM Audio Type

`AudioType::ALARM` (4) exists in the proto but cannot be tested without knowing its
channel ID (likely in 9-13 range, undocumented in aasdk).

---

## Sensor Channel

### Supported Sensors

| Sensor | Enum | Requested | Data Source |
|--------|------|-----------|------------|
| NIGHT_DATA | 10 (0x0a) | Yes | ThemeService day/night |
| DRIVING_STATUS | 13 (0x0d) | Yes | Always UNRESTRICTED |
| LOCATION | 1 (0x01) | Yes (twice) | No GPS — placeholder |
| COMPASS | 2 (0x02) | **Yes (Probe 8)** | No data — placeholder |
| ACCEL | 19 (0x13) | **Yes (Probe 8)** | No data — placeholder |
| GYRO | 20 (0x14) | Advertised but NOT requested | — |

### Sensor Request Payload Format

```
08 <type> 10 <mode>
```
- `type`: SensorType enum value
- `mode`: Appears to be reporting mode/interval
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

**TODO:** Test live night=true toggle during active session (requires user present).

---

## Theming / Palette (Probe 4) — UNRESOLVED

Phone logs:
```
GH.CarThemeVersionLD: Updating theming version to: 0
GH.ThemingManager: Not updating theme, palette version doesn't match.
  Current version: 2, HU version: 0
```

**The phone expects palette version 2 from the HU.** The field doesn't exist in any known
proto definition (checked aasdk, GAL docs, aa-proxy-rs). Likely an undocumented extension
to ServiceDiscoveryResponse or HeadUnitInfo.

Impact: Without palette v2, Material You dynamic colors from HU are not applied.
Phone falls back to default Google Maps theming. Cosmetic, not functional.

---

## Protocol Wire Format Observations

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

## Permanent Code Changes

Changes kept on `feature/protocol-logger` branch:

1. **Protocol version v1.7** — `libs/aasdk/include/aasdk/Version.hpp` AASDK_MINOR=7
2. **48kHz speech audio** — `src/core/aa/ServiceFactory.cpp` SpeechAudioServiceStub 48000Hz
3. **Extra sensors** — COMPASS, ACCEL, GYRO advertised in sensor channel
4. **ProtocolLogger** — TSV logging with channel/message name resolution
5. **Universal CHANNEL_OPEN recognition** — fixed in ProtocolLogger for all channels

---

## Open Questions

1. **Palette version field number** — where does the phone read theme_version from?
2. **GYRO sensor** — advertised but never requested. Trigger condition unknown.
3. **CarDisplayUiFeatures** — how to set hasClock=true and what UiConfig fields control it
4. **Channels 9-13** — service-to-channel mapping for production SDK services
5. **Session configuration** — purpose of field 13 (int32) in ServiceDiscoveryResponse
6. **Live night mode** — need to test night=true push during active session
7. **ALARM audio channel** — which channel ID does it use?
8. **UpdateUiConfigRequest** — runtime UI reconfiguration, untested
9. **1080p performance** — sustained decode performance on Pi 4 (capture was short)
10. **48kHz system audio** — would bumping channel 6 to 48kHz also work?
