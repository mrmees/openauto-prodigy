# Android Auto APK v16.1 — Proto Reference

> Extracted from decompiled Google Android Auto APK (v16.1). ProGuard-obfuscated class names preserved for traceability. Field numbers and types are reliable; message/field names are inferred from context.

## Service Types (from `qlf.java`)

| ID | Service Type | APK Handler | Status in open-androidauto |
|----|-------------|-------------|---------------------------|
| 1 | Sensor | `hzy.java` (indirect) | **Implemented** |
| 2 | Video (output) | `ias.java` | **Implemented** |
| 3 | Input (touch/keys) | `iaa.java` | **Implemented** |
| 4 | Audio (media) | `hzt.java` (indirect) | **Implemented** |
| 5 | Audio (speech/nav) | `hzt.java` (indirect) | **Implemented** |
| 6 | Audio (phone call) | `hzt.java` (indirect) | **Implemented** |
| 7 | WiFi Discovery | — | **Implemented** |
| 8 | Bluetooth | `hzh.java` | **Implemented** |
| 9 | AV Input (mic) | `hzp.java` | **Implemented** |
| 10 | Navigation | `hzy.java` | Not implemented |
| 11 | Media Playback | `hzt.java` | Not implemented |
| 12 | Media Browser | — (no dedicated handler) | Not implemented |
| 13 | Phone Status | `iae.java` | Not implemented |
| 14 | Notification | — (no dedicated handler) | Not implemented |
| 15 | Radio | `iaq.java` | Not implemented |
| 16 | Vendor Extension | `iba.java` | Not implemented |
| 17 | (reserved) | — | N/A |
| 18 | WiFi Projection | — | Not implemented |
| 19 | Car Control | `hxp.java` | Not implemented |
| 20 | Car Local Media | `hxu.java` | Not implemented |
| 21 | Buffered Media Sink | `ibh.java` | Not implemented |

## Control Channel Messages (from `vee.W()`)

| ID | Message | Direction | Notes |
|----|---------|-----------|-------|
| 0x0001 | VERSION_REQUEST | HU→Phone | Raw binary, 4 bytes |
| 0x0002 | VERSION_RESPONSE | Phone→HU | Raw binary, 6 bytes |
| 0x0003 | SSL_HANDSHAKE | Bidirectional | TLS data passthrough |
| 0x0004 | AUTH_COMPLETE | HU→Phone | Protobuf: status enum |
| 0x0005 | SERVICE_DISCOVERY_REQUEST | Phone→HU | |
| 0x0006 | SERVICE_DISCOVERY_RESPONSE | HU→Phone | |
| 0x0007 | CHANNEL_OPEN_REQUEST | Phone→HU | Arrives on target channel |
| 0x0008 | CHANNEL_OPEN_RESPONSE | HU→Phone | |
| 0x0009 | CHANNEL_CLOSE_NOTIFICATION | Either | Not yet observed |
| 0x000a | (unknown) | — | Present in map, no handler |
| 0x000b | PING_REQUEST | Either | |
| 0x000c | PING_RESPONSE | Either | |
| 0x000d | NAV_FOCUS_REQUEST | Phone→HU | |
| 0x000e | NAV_FOCUS_RESPONSE | HU→Phone | |
| 0x000f | SHUTDOWN_REQUEST | Either | |
| 0x0010 | SHUTDOWN_RESPONSE | Either | |
| 0x0011 | VOICE_SESSION_REQUEST | Phone→HU | |
| 0x0012 | AUDIO_FOCUS_REQUEST | Phone→HU | |
| 0x0013 | AUDIO_FOCUS_RESPONSE | HU→Phone | |
| 0x0018 | CALL_AVAILABILITY_STATUS | HU→Phone | Commercial HUs send this |
| 0x001a | SERVICE_DISCOVERY_UPDATE | Phone→HU | Service list update |
| 0x00ff | (special) | — | Present in APK map |
| 0x0fff | (special) | — | Present in APK map |
| 0xDEAD | (diagnostic?) | — | Present in APK map |
| 0xFFFF | (sentinel?) | — | Present in APK map |

## AV Channel Messages (from `vee.S()`)

| ID | Message | Notes |
|----|---------|-------|
| 0x0000 | AV_MEDIA_WITH_TIMESTAMP | First 8B = uint64 BE timestamp |
| 0x0001 | AV_MEDIA_INDICATION | No timestamp (SPS/PPS) |
| 0x8000 | SETUP_REQUEST | Phone→HU |
| 0x8001 | START_INDICATION | Phone→HU |
| 0x8002 | STOP_INDICATION | Phone→HU |
| 0x8003 | SETUP_RESPONSE | HU→Phone |
| 0x8004 | ACK_INDICATION | HU→Phone |
| 0x8005 | AV_INPUT_OPEN_REQUEST | Phone→HU |
| 0x8006 | AV_INPUT_OPEN_RESPONSE | HU→Phone |
| 0x8007 | VIDEO_FOCUS_REQUEST | Phone→HU |
| 0x8008 | VIDEO_FOCUS_INDICATION | HU→Phone |
| 0x8009 | VIDEO_FOCUS_NOTIFICATION | Direction unknown |
| 0x800A | UPDATE_UI_CONFIG_REQUEST | Direction unknown |
| 0x800B | UPDATE_UI_CONFIG_REPLY | Direction unknown |
| 0x800C | AUDIO_UNDERFLOW | Direction unknown |
| 0x800D | ACTION_TAKEN | Direction unknown |
| 0x800E | OVERLAY_PARAMETERS | Direction unknown |
| 0x800F | OVERLAY_START | Direction unknown |
| 0x8010 | OVERLAY_STOP | Direction unknown |
| 0x8011 | OVERLAY_SESSION_UPDATE | Direction unknown |
| 0x8012 | UPDATE_HU_UI_CONFIG_REQUEST | Direction unknown |
| 0x8013 | UPDATE_HU_UI_CONFIG_RESPONSE | Direction unknown |
| 0x8014 | MEDIA_STATS | Direction unknown |
| 0x8015 | MEDIA_OPTIONS | Direction unknown |

## Key Enums

### StatusCode (from `vyw.java`)

```
OK = 0
UNSOLICITED_MESSAGE = 1
NO_COMPATIBLE_VERSION = -1
CERTIFICATE_ERROR = -2
AUTHENTICATION_FAILURE = -3
INVALID_SERVICE = -4
INVALID_CHANNEL = -5
INVALID_PRIORITY = -6
INTERNAL_ERROR = -7
MEDIA_CONFIG_MISMATCH = -8
INVALID_SENSOR = -9
BLUETOOTH_PAIRING_DELAYED = -10
BLUETOOTH_UNAVAILABLE = -11
BLUETOOTH_INVALID_ADDRESS = -12
BLUETOOTH_INVALID_PAIRING_METHOD = -13
BLUETOOTH_INVALID_AUTH_DATA = -14
BLUETOOTH_AUTH_DATA_MISMATCH = -15
BLUETOOTH_HFP_ANOTHER_CONNECTION = -16
BLUETOOTH_HFP_CONNECTION_FAILURE = -17
KEYCODE_NOT_BOUND = -18
RADIO_INVALID_STATION = -19
INVALID_INPUT = -20
RADIO_STATION_PRESETS_NOT_SUPPORTED = -21
RADIO_COMM_ERROR = -22
CERT_NOT_YET_VALID = -23
CERT_EXPIRED = -24
PING_TIMEOUT = -25
CAR_PROPERTY_SET_FAILED = -26
CAR_PROPERTY_LISTENER_FAILED = -27
COMMAND_NOT_SUPPORTED = -250
FRAMING_ERROR = -251
UNEXPECTED_MESSAGE = -253
BUSY = -254
OUT_OF_MEMORY = -255
```

### ByeByeReason / ShutdownReason (from `vwe.java`)

```
NONE = 0
USER_SELECTION = 1
DEVICE_SWITCH = 2
NOT_SUPPORTED = 3
NOT_CURRENTLY_SUPPORTED = 4
PROBE_SUPPORTED = 5
WIRELESS_PROJECTION_DISABLED = 6
POWER_DOWN = 7
USER_PROFILE_SWITCH = 8
```

### VideoFocusMode

```
NONE = 0
PROJECTED = 1
NATIVE = 2
NATIVE_TRANSIENT = 3
PROJECTED_NO_INPUT_FOCUS = 4
```

### VideoFocusReason

```
UNKNOWN = 0
PHONE_SCREEN_OFF = 1
LAUNCH_NATIVE = 2
LAST_MODE = 3
USER_SELECTION = 4
```

### CallState (from Phone Status service, `iae.java`)

```
IDLE = 0
INCOMING = 1
OUTGOING = 2
ACTIVE = 3
ON_HOLD = 4
CALL_WAITING = 5
DISCONNECTED = 6
```

### AccessPointType (from WiFi)

Note: APK enum values differ from our proto by 1.

| APK value | APK meaning | Our proto value |
|-----------|------------|-----------------|
| 1 | STATIC | 0 |
| 2 | DYNAMIC | 1 |

## P1 Service Protos (Not Yet Implemented)

### Service 10: Navigation (handler `hzy.java`)

Wire IDs: 0x8001-0x8008 (shared AV range)

Key messages (inferred):
- NavigationState: route status, ETA, distance remaining
- NavigationStep: upcoming maneuver info (direction, distance, road name)
- Maneuver: turn type, icon ID

### Service 11: Media Playback (handler `hzt.java`)

Wire IDs: 0x8001-0x8003

Key messages:
- MediaPlaybackStatus: playback state (playing/paused/stopped), position, duration
- MediaPlaybackMetadata: title, artist, album, art URL
- MediaPlaybackInput: play/pause/skip/seek commands

### Service 13: Phone Status (handler `iae.java`)

Wire IDs: 0x8001-0x8002

Key messages:
- PhoneStatus: list of active calls
- PhoneStatus_Call: number, display_name, call_state (CallState enum)
- PhoneStatusInput: user actions (answer, reject, hangup)

### Service 15: Radio (handler `iaq.java`)

Wire IDs: 0x801A-0x8022 (unique range, doesn't overlap AV)

Key messages:
- RadioStationList: presets and current station
- RadioTuneRequest: frequency, band
- RadioInfo: station name, metadata (RDS/HD Radio)

### Service 16: Vendor Extension (handler `iba.java`)

Raw byte passthrough — no protobuf. OEM-specific.

### Service 19: Car Control (handler `hxp.java`)

Wire IDs: 0x8001-0x8007

Complex property system — key/value pairs for HVAC, seat controls, vehicle settings. Mirrors Android VHAL (Vehicle Hardware Abstraction Layer).

### Service 20: Car Local Media (handler `hxu.java`)

Wire IDs: 0x8001-0x8003

Local media playback state. Similar to Service 11 but for head unit's own media.

### Service 21: Buffered Media Sink (handler `ibh.java`)

Stub in APK — appears unused.

## Evidence Gaps

These services need wire captures before implementation:

1. **Navigation (10):** No captures of nav data flowing. Need active navigation session logged.
2. **Media Playback (11):** Need media metadata messages captured during music playback.
3. **Phone Status (13):** Need call state messages captured during incoming/active call.
4. **Radio (15):** Requires FM/AM radio hardware — unlikely to trigger without it.
5. **Car Control (19):** Requires VHAL integration — Pi doesn't have vehicle bus.
6. **Overlay messages (0x800E-0x8011):** Completely unknown direction and content. Need capture.
