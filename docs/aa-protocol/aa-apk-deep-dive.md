# Android Auto APK Deep Dive — Complete Protocol Knowledge

*Extracted from AA v16.1.660414-release via APK indexer + graph walker*
*1,946 proto classes, 180K cross-references, 3,391 enum mappings analyzed*

## Table of Contents

- [Channel Types](#channel-types) — all 22 channels the phone knows about
- [Video Codecs](#video-codecs) — H.264, H.265, VP9, AV1
- [Sensor Types](#sensor-types) — all 26 sensor types
- [Audio Focus States](#audio-focus-states) — 8 states for audio arbitration
- [Key Codes](#key-codes) — Android keycodes including car-specific extensions
- [Navigation Maneuvers](#navigation-maneuvers) — turn-by-turn instruction types
- [Phone Call States](#phone-call-states) — telephony state machine
- [Disconnect Reasons](#disconnect-reasons) — why the phone drops the connection
- [Session Errors](#session-errors) — what the phone validates about our config
- [WiFi BT RFCOMM Protocol](#wifi-bt-rfcomm-protocol) — wireless setup message types
- [Wireless Setup Flow](#wireless-setup-flow) — the full connection state machine
- [Notification Types](#notification-types) — all notification categories
- [Fuel & EV Types](#fuel--ev-types) — vehicle energy types
- [HVAC Properties](#hvac-properties) — car climate control IDs
- [What We're Missing](#what-were-missing) — gaps in our implementation

---

## Channel Types

The phone recognizes 22 channel types (enum `qlf`). These are *logical* types — the wire channel IDs (0-14) are assigned at runtime via ServiceDiscoveryResponse.

| ID | Name | Status | Notes |
|----|------|--------|-------|
| 0 | UNKNOWN | — | |
| 1 | CONTROL | **Handled** | Session management, auth, ping, shutdown |
| 2 | VIDEO_SINK | **Handled** | H.264 video from phone → HU display |
| 3 | AUDIO_SINK_GUIDANCE | **Handled** | Navigation voice prompts |
| 4 | AUDIO_SINK_SYSTEM | **Handled** | System sounds |
| 5 | AUDIO_SINK_MEDIA | **Handled** | Music/media audio |
| 6 | AUDIO_SOURCE | **Handled** | Microphone (HU → phone) |
| 7 | SENSOR_SOURCE | **Handled** | GPS, speed, night mode, etc. |
| 8 | INPUT_SOURCE | **Handled** | Touch, buttons, rotary |
| 9 | BLUETOOTH | **Handled** | BT pairing coordination |
| 10 | NAVIGATION_STATUS | **Handled** | Turn-by-turn nav data |
| 11 | MEDIA_PLAYBACK_STATUS | **Handled** | Now playing metadata |
| 12 | MEDIA_BROWSER | Not handled | Browse phone's music library |
| 13 | PHONE_STATUS | **Handled** | Call state, caller info |
| 14 | NOTIFICATION | Not handled | Toast, SMS, IM, calendar, etc. |
| 15 | RADIO | Deprecated | Was for HD Radio integration |
| 16 | VENDOR_EXTENSION | Not handled | OEM-specific extensions |
| 17 | WIFI_PROJECTION | **Handled** | WiFi credential exchange |
| 18 | WIFI_DISCOVERY | Not handled | WiFi Direct discovery |
| 19 | CAR_CONTROL | Not handled | HVAC, vehicle controls |
| 20 | CAR_LOCAL_MEDIA | Not handled | Local media on HU |
| 21 | BUFFERED_MEDIA_SINK | Not handled | Buffered/offline media |

### Priority for Implementation

1. **NOTIFICATION (14)** — Low-hanging fruit. Receive SMS/IM/calendar notifications.
2. **MEDIA_BROWSER (12)** — Browse and play from phone's music library.
3. **CAR_CONTROL (19)** — HVAC controls if connected to vehicle CAN bus.
4. **VENDOR_EXTENSION (16)** — OEM customization hooks.
5. **WIFI_DISCOVERY (18)** — WiFi Direct (P2P) as alternative to AP mode.

---

## Video Codecs

Enum `vyn` — codecs supported in the AA protocol:

| Value | Name | Notes |
|-------|------|-------|
| 1 | MEDIA_CODEC_AUDIO_PCM | 16-bit PCM audio |
| 2 | MEDIA_CODEC_AUDIO_AAC_LC | AAC-LC audio |
| 3 | MEDIA_CODEC_VIDEO_H264_BP | H.264 Baseline Profile — **what we use** |
| 4 | MEDIA_CODEC_AUDIO_AAC_LC_ADTS | AAC-LC with ADTS framing |
| 5 | MEDIA_CODEC_VIDEO_VP9 | VP9 video |
| 6 | MEDIA_CODEC_VIDEO_AV1 | AV1 video |
| 7 | MEDIA_CODEC_VIDEO_H265 | H.265/HEVC video |

**Key insight:** The phone supports H.265, VP9, and AV1 in addition to H.264. H.265 gives 2x compression at same quality — could enable 1080p on Pi 4 if we add HEVC decode. AV1 is the newest but Pi 4 lacks hardware decode.

---

## Sensor Types

Enum `wbv` — all 26 sensor types the phone may request:

| Value | Name | We Handle? | Notes |
|-------|------|-----------|-------|
| 1 | SENSOR_LOCATION | Yes (GPS) | GPS coordinates |
| 2 | SENSOR_COMPASS | No | Heading/bearing |
| 3 | SENSOR_SPEED | No | Vehicle speed |
| 4 | SENSOR_RPM | No | Engine RPM |
| 5 | SENSOR_ODOMETER | No | Total distance |
| 6 | SENSOR_FUEL | No | Fuel level % |
| 7 | SENSOR_PARKING_BRAKE | Yes | Parking brake on/off |
| 8 | SENSOR_GEAR | No | P/R/N/D/1-9 |
| 9 | SENSOR_OBDII_DIAGNOSTIC_CODE | No | DTC codes |
| 10 | SENSOR_NIGHT_MODE | Yes | Day/night mode |
| 11 | SENSOR_ENVIRONMENT_DATA | No | Ambient temp, pressure |
| 12 | SENSOR_HVAC_DATA | No | Climate control state |
| 13 | SENSOR_DRIVING_STATUS_DATA | Yes | Moving/parked/restricted |
| 14 | SENSOR_DEAD_RECKONING_DATA | No | Wheel ticks for inertial nav |
| 15 | SENSOR_PASSENGER_DATA | No | Seat occupancy |
| 16 | SENSOR_DOOR_DATA | No | Door open/close state |
| 17 | SENSOR_LIGHT_DATA | No | Headlight state |
| 18 | SENSOR_TIRE_PRESSURE_DATA | No | TPMS data |
| 19 | SENSOR_ACCELEROMETER_DATA | No | 3-axis accel |
| 20 | SENSOR_GYROSCOPE_DATA | No | 3-axis gyro |
| 21 | SENSOR_GPS_SATELLITE_DATA | No | Satellite count/SNR |
| 22 | SENSOR_TOLL_CARD | No | Electronic toll collection |
| 23 | SENSOR_VEHICLE_ENERGY_MODEL_DATA | No | EV energy model |
| 24 | SENSOR_TRAILER_DATA | No | Trailer connection state |
| 25 | SENSOR_RAW_VEHICLE_ENERGY_MODEL | No | Raw EV data |
| 26 | SENSOR_RAW_EV_TRIP_SETTINGS | No | EV trip planning params |

Sensors 22-26 are new (not in our SensorTypeEnum.proto). All sensor requests should be accepted with OK — the phone gracefully handles missing data.

---

## Audio Focus States

Enum `vvu` — audio focus arbitration between HU and phone:

| Value | Name | Description |
|-------|------|-------------|
| 0 | AUDIO_FOCUS_STATE_INVALID | Error/unknown state |
| 1 | AUDIO_FOCUS_STATE_GAIN | Phone gets full audio control |
| 2 | AUDIO_FOCUS_STATE_GAIN_TRANSIENT | Phone gets temporary audio (e.g., nav prompt) |
| 3 | AUDIO_FOCUS_STATE_LOSS | Phone lost audio — HU is playing |
| 4 | AUDIO_FOCUS_STATE_LOSS_TRANSIENT_CAN_DUCK | Phone should lower volume (e.g., nav over music) |
| 5 | AUDIO_FOCUS_STATE_LOSS_TRANSIENT | Phone should pause |
| 6 | AUDIO_FOCUS_STATE_GAIN_MEDIA_ONLY | Phone gets media audio only (no nav/phone) |
| 7 | AUDIO_FOCUS_STATE_GAIN_TRANSIENT_GUIDANCE_ONLY | Phone gets nav audio only |

---

## Key Codes

Enum `vyh` — 251 key codes. Notable car-specific extensions:

| Value | Name | Notes |
|-------|------|-------|
| 3 | KEYCODE_HOME | Return to launcher |
| 4 | KEYCODE_BACK | Back navigation |
| 5 | KEYCODE_CALL | Accept/place call |
| 6 | KEYCODE_ENDCALL | End call |
| 24 | KEYCODE_VOLUME_UP | Volume up |
| 25 | KEYCODE_VOLUME_DOWN | Volume down |
| 79 | KEYCODE_HEADSETHOOK | Headset button (play/pause/answer) |
| 84 | KEYCODE_SEARCH | Voice search |
| 85 | KEYCODE_MEDIA_PLAY_PAUSE | Toggle play/pause |
| 86 | KEYCODE_MEDIA_STOP | Stop playback |
| 87 | KEYCODE_MEDIA_NEXT | Next track |
| 88 | KEYCODE_MEDIA_PREVIOUS | Previous track |
| 164 | KEYCODE_VOLUME_MUTE | Mute toggle |
| 219 | KEYCODE_ASSIST | Google Assistant |
| 231 | KEYCODE_VOICE_ASSIST | Voice assistant (alternative) |
| 260 | KEYCODE_NAVIGATE_PREVIOUS | Nav: previous item |
| 261 | KEYCODE_NAVIGATE_NEXT | Nav: next item |
| 262 | KEYCODE_NAVIGATE_IN | Nav: drill in |
| 263 | KEYCODE_NAVIGATE_OUT | Nav: drill out |
| **65535** | **KEYCODE_SENTINEL** | End marker |
| **65536** | **KEYCODE_ROTARY_CONTROLLER** | Rotary dial input (car-specific) |
| **65537** | **KEYCODE_MEDIA** | Media button (car-specific) |
| **65543** | **KEYCODE_TERTIARY_BUTTON** | Third steering wheel button |
| **65544** | **KEYCODE_TURN_CARD** | Show turn-by-turn card |

The car-specific codes (65536+) are AA protocol extensions not in standard Android.

---

## Navigation Maneuvers

Enum `vzh` — turn-by-turn instruction types:

| Value | Name |
|-------|------|
| 0 | UNKNOWN |
| 1 | STRAIGHT |
| 2 | SLIGHT_LEFT |
| 3 | SLIGHT_RIGHT |
| 4 | NORMAL_LEFT |
| 5 | NORMAL_RIGHT |
| 6 | SHARP_LEFT |
| 7 | SHARP_RIGHT |
| 8 | U_TURN_LEFT |
| 9 | U_TURN_RIGHT |

---

## Phone Call States

Enum `wae` — telephony state machine:

| Value | Name | Description |
|-------|------|-------------|
| 0 | UNKNOWN | |
| 1 | IN_CALL | Active call in progress |
| 2 | ON_HOLD | Call on hold |
| 3 | INACTIVE | No active call |
| 4 | INCOMING | Ringing incoming call |
| 5 | CONFERENCED | In a conference call |
| 6 | MUTED | Call muted |

---

## Disconnect Reasons

Enum `xkh` — why the phone disconnects from the HU:

| Value | Name | Actionable? |
|-------|------|-------------|
| 0 | UNKNOWN_CODE | |
| 1 | PROTOCOL_INCOMPATIBLE_VERSION | Check version negotiation |
| 2 | PROTOCOL_WRONG_CONFIGURATION | Fix ServiceDiscoveryResponse |
| 3 | PROTOCOL_IO_ERROR | Check TCP connection |
| 4 | PROTOCOL_BYEBYE_REQUESTED_BY_CAR | We sent shutdown |
| 5 | PROTOCOL_BYEBYE_REQUESTED_BY_USER | User disconnected on phone |
| 6 | PROTOCOL_WRONG_MESSAGE | We sent an invalid message |
| 7 | PROTOCOL_AUTH_FAILED | SSL/TLS handshake failed |
| 8 | PROTOCOL_AUTH_FAILED_BY_CAR | Our cert rejected |
| 9 | TIMEOUT | General timeout |
| 12 | CAR_NOT_RESPONDING | We stopped sending pings |
| 13 | PROTOCOL_AUTH_FAILED_BY_CAR_CERT_NOT_YET_VALID | Clock skew (cert not yet valid) |
| 14 | PROTOCOL_AUTH_FAILED_BY_CAR_CERT_EXPIRED | Certificate expired |
| 24 | AUDIO_ERROR | Audio config invalid |
| 26 | PROJECTION_PROCESS_CRASH_LOOP | Phone-side AA app crashing |

---

## Session Errors

Enum `xki` — validation errors the phone checks before/during session:

| Value | Name | What It Means |
|-------|------|---------------|
| 1 | NO_SENSORS | ServiceDiscoveryResponse has no sensor channel |
| 6 | BAD_MIC_AUDIO_CONFIG | Microphone audio config invalid |
| 7 | BAD_AUDIO_CONFIG | Audio output config invalid |
| 8 | MISSING_AUDIO_CONFIG | No audio config in service discovery |
| 9 | NAV_NO_IMAGE_OPTIONS | Navigation channel missing image options |
| 12 | BAD_VIDEO | Video config invalid |
| 13 | MISSING_VIDEO | No video channel in service discovery |
| 17 | NO_VIDEO_CONFIG | Video channel has no config |
| 18 | BAD_CODEC_RESOLUTION | Codec doesn't support the resolution |
| 19 | BAD_DISPLAY_RESOLUTION | Invalid display resolution |
| 20 | BAD_FPS | Invalid frame rate |
| 21 | NO_DENSITY | No DPI specified |
| 22 | BAD_DENSITY | Invalid DPI value |
| 26 | NO_INPUT | No input channel |
| 48 | VIDEO_ACK_TIMEOUT | We're not ACK'ing video frames fast enough |
| 57 | PING_TIMEOUT | We missed pings |
| 58 | MULTIPLE_DISPLAY_CONFIGS | Multiple display configs (only one allowed?) |
| 75 | AUTH_FAILED_OBSOLETE_SSL | SSL version too old |

---

## WiFi BT RFCOMM Protocol

Enum `wdn` — messages exchanged over BT RFCOMM during wireless setup:

| ID | Name | Direction | Notes |
|----|------|-----------|-------|
| 1 | MESSAGE_WIFI_REQUEST_START_BT | Phone → HU | Start wireless setup |
| 2 | MESSAGE_WIFI_REQUEST_INFO_BT | Phone → HU | Request WiFi credentials |
| 3 | MESSAGE_WIFI_RESPONSE_INFO_BT | HU → Phone | Send WiFi credentials |
| 4 | MESSAGE_WIFI_VERSION_REQUEST_BT | HU → Phone | Version negotiation |
| 5 | MESSAGE_WIFI_VERSION_RESPONSE_BT | Phone → HU | Version response |
| 6 | MESSAGE_WIFI_CONNECT_STATUS_BT | Phone → HU | WiFi connection progress |
| 7 | MESSAGE_WIFI_RESPONSE_START_BT | HU → Phone | Acknowledge start |
| 8 | MESSAGE_WIFI_PING_REQUEST_BT | Either | Keep-alive ping |
| 9 | MESSAGE_WIFI_PING_RESPONSE_BT | Either | Ping response |
| 10 | MESSAGE_WIFI_CONNECTION_REJECTION_BT | HU → Phone | Reject connection |
| 11 | MESSAGE_WIFI_SETUP_INFO_BT | HU → Phone | Extended setup data |

### New: WPP over TCP

The phone supports running the WiFi Projection Protocol over TCP (not just BT RFCOMM). Evidence:
- `WIRELESS_WIFI_PROJECTION_PROTOCOL_HU_SUPPORTS_WPP_OVER_TCP`
- `WIRELESS_WIFI_PROJECTION_PROTOCOL_TCP_*` event family
- This enables "reconnection without BT" — phone connects directly to HU's TCP port

---

## Wireless Setup Flow

Key events from `xik` reveal the complete wireless connection state machine:

```
1. WIRELESS_SETUP_START
2. WIRELESS_CONNECTING_RFCOMM → WIRELESS_CONNECTED_RFCOMM
3. WIRELESS_RFCOMM_VERSION_CHECK_COMPLETE
4. WIRELESS_RFCOMM_INFO_REQUEST_SENT → WIRELESS_RFCOMM_INFO_RESPONSE_RECEIVED
5. WIRELESS_CONNECTING_WIFI → WIRELESS_CONNECTED_WIFI
6. WIRELESS_WIFI_SOCKET_CONNECTING → WIRELESS_WIFI_SOCKET_CONNECTED
7. SSL_STARTED → SSL_SUCCESS
8. WIRELESS_PROJECTING_STAGE_REACHED
```

### Credential Caching

The phone caches WiFi credentials per HU:
- `WIRELESS_WIFI_CACHED_CREDENTIALS_USED` — phone uses saved creds (skips BT RFCOMM)
- `WIRELESS_WIFI_CACHED_CREDENTIALS_INVALID` — cached creds don't work
- `WIRELESS_WIFI_CACHED_CREDENTIALS_FAILED` — connection with cached creds failed

This means **after first connection**, the phone may connect directly via WiFi without going through BT RFCOMM again. The HU should accept TCP connections from previously-paired phones.

### WPA3 Support

The phone checks for WPA3:
- `WIRELESS_WIFI_MD_WPA3_SECURITY_SUPPORTED`
- `WIRELESS_WIFI_USING_WPA3_TRANSITION_SECURITY`
- `WIRELESS_WIFI_USING_WPA3_SECURITY`
- `WIRELESS_WIFI_SCAN_RESULTS_TARGET_NETWORK_SUPPORTS_WPA3_WPA2`

### 5GHz Requirement

- `WIRELESS_WIFI_NO_5GHZ_SUPPORT` — phone/HU doesn't support 5GHz
- 5GHz is strongly preferred; 2.4GHz may cause latency issues

### Connection Rejection Reasons

- `WIRELESS_WIFI_PROJECTION_PROTOCOL_CONNECTION_REJECTION_REASON_INVALID_SETUP_TOKEN`
- `WIRELESS_WIFI_PROJECTION_PROTOCOL_WPP_OVER_TCP_REJECTED_MOBILE_DEVICE_ID_NOT_FOUND`

---

## Notification Types

Enum `xks` — all notification categories the phone can send:

| Value | Name |
|-------|------|
| 1 | TOAST |
| 2 | MEDIA |
| 3 | IM_NOTIFICATION |
| 4 | NAV_NOTIFICATION_HERO |
| 5 | SMS_NOTIFICATION |
| 6 | CALL |
| 7 | RECENT_CALL |
| 8 | GMM_SUGGESTION |
| 9-22 | Various Google Now cards |
| 23 | NAV_NOTIFICATION_NORMAL |
| 24 | SDK_NOTIFICATION |
| 25 | NAV_SUGGESTION |
| 26 | BATTERY_LOW |
| 27 | NOW_ROUTINE |

---

## Fuel & EV Types

### Fuel Types (enum `vxo`)

| Value | Name |
|-------|------|
| 0 | FUEL_TYPE_UNKNOWN |
| 1 | FUEL_TYPE_UNLEADED |
| 2 | FUEL_TYPE_LEADED |
| 3-4 | FUEL_TYPE_DIESEL_1/2 |
| 5 | FUEL_TYPE_BIODIESEL |
| 6 | FUEL_TYPE_E85 |
| 7 | FUEL_TYPE_LPG |
| 8 | FUEL_TYPE_CNG |
| 9 | FUEL_TYPE_LNG |
| 10 | FUEL_TYPE_ELECTRIC |
| 11 | FUEL_TYPE_HYDROGEN |
| 12 | FUEL_TYPE_OTHER |

### EV Connector Types (enum `vxl`)

| Value | Name |
|-------|------|
| 1 | J1772 |
| 2 | MENNEKES (Type 2) |
| 3 | CHAdeMO |
| 4 | CCS Combo 1 |
| 5 | CCS Combo 2 |
| 6-8 | Tesla (Roadster, HPWC, Supercharger) |
| 9 | GB/T |

---

## HVAC Properties

Enum `vuz` — Android VHAL property IDs for climate control. These use Android's Vehicle HAL numbering:

| Value | Name |
|-------|------|
| 289408269 | HVAC_STEERING_WHEEL_HEAT |
| 289408270 | HVAC_TEMPERATURE_DISPLAY_UNITS |
| 320865540 | HVAC_DEFROSTER |
| 354419973 | HVAC_AC_ON |
| 354419974 | HVAC_MAX_AC_ON |
| 354419975 | HVAC_MAX_DEFROST_ON |
| 354419976 | HVAC_RECIRC_ON |
| 354419977 | HVAC_DUAL_ON |
| 354419978 | HVAC_AUTO_ON |
| 354419984 | HVAC_POWER_ON |
| 356517120 | HVAC_FAN_SPEED |
| 356517121 | HVAC_FAN_DIRECTION |
| 356517131 | HVAC_SEAT_TEMPERATURE |
| 358614275 | HVAC_TEMPERATURE_SET |

---

## Channel Error Codes

Enum `nop` — errors reported during channel operation:

### Protocol Errors (100-302)
| Value | Name |
|-------|------|
| 100 | INVALID_MESSAGE_RECEIVED |
| 102 | CAR_DID_NOT_SEND_ALL_ACKS |
| 103 | CALLBACK_QUEUE_OVERFLOW |
| 104 | RECEIVED_ACK_WITH_WRONG_SESSION_ID |
| 200 | CHANNEL_CLOSE_REQUESTED |
| 201 | CHANNEL_CLOSED_DUE_TO_PROTOCOL_ERROR |
| 300 | TRIED_TO_SEND_CODEC_CONFIG_WHILE_NOT_STARTED |
| 301 | TRIED_TO_SEND_DATA_WHILE_NOT_STARTED |

### Audio Errors (1000-2206)
Extensive audio state machine with capture start/stop, bottom half management, focus changes. Key ones:
- `1008` AUDIO_STREAMING_REQUEST_CANCELLED_DUE_TO_LONG_SILENCE — phone stops sending mic audio after silence
- `1100` AUDIO_HANDLE_FOCUS_CHANGE_COMMAND — audio focus changed
- `1101` AUDIO_FOCUS_CHANGE_COMMAND_TIMED_OUT — we didn't respond to focus change fast enough

---

## Connection States

Enum `xje` — full connection state machine:

| Value | Name |
|-------|------|
| 1 | WAIT_FOR_CAR_CONNECTION |
| 2 | CHECK_COUNTRY_WHITELIST |
| 3 | CHECK_PHONE_BLACKLIST |
| 4 | AUTHORIZE_CAR_CONNECTION |
| 9 | CAR_MOVING |
| 12 | SETUP_DONE |
| 13 | CHECK_PERMISSIONS |
| 15 | LOCK_SCREEN |
| 16 | GH_WAIT_FOR_CAR_CONNECTION |
| 30 | GH_SETUP_DONE |

---

## What We're Missing

### Critical Gaps (affect user experience)

1. **Video ACK timeout (error 48)** — phone expects timely ACKs for video frames. If we're slow decoding, phone may disconnect.

2. **Sensor types 22-26** — not in our enum. Should be added even if we return no data. Phone may error if it gets an unknown type.

3. **Audio focus state 6-7** — GAIN_MEDIA_ONLY and GAIN_TRANSIENT_GUIDANCE_ONLY. We need to handle these for proper audio mixing.

4. **WPP over TCP reconnection** — phone can reconnect via TCP without BT RFCOMM after first connection. We should accept these.

5. **Connection rejection message (wdn=10)** — we have no way to reject a connection attempt.

### Nice-to-Have Features

1. **Notification channel** — receive SMS, IM, calendar alerts on HU display
2. **Media browser** — browse phone's music library from HU
3. **H.265 video** — 2x compression vs H.264, could enable 1080p on Pi
4. **Navigation maneuvers** — display turn-by-turn on HU's own UI (instrument cluster)
5. **Car control / HVAC** — if connected to vehicle CAN bus
6. **Rotary controller input** — for knob-based head units
7. **WPA3** — stronger WiFi security

### Known Phone Behaviors

- Phone caches WiFi credentials per HU (BSSID+SSID based)
- Phone verifies 5GHz support before connection
- Phone checks for "dongle" device type via BT metadata
- Phone has a "denylist" for problematic HUs
- Phone supports dual-STA WiFi (connect to HU AP while staying on internet)
- Phone tracks BT battery level and may reject suspicious devices
- SSL certificate validity dates matter — clock skew causes AUTH_FAILED
