# Android Auto APK Decompilation Analysis

> **Source:** Android Auto v16.1.660414-release (`com.google.android.projection.gearhead`)
> **Date:** 2026-02-23
> **Method:** jadx 1.5.1 decompilation of base APK (34MB)
> **Classes:** 20,761 decompiled
> **Obfuscation:** ProGuard/R8 — all internal classes in `defpackage/` with 2-4 character names
> **Architecture:** Entire AA protocol is Java/Dalvik (no native .so for protocol handling)
> **Legal basis:** DMCA §1201(f) interoperability exemption, EU Software Directive Art. 6

This document contains protocol constants, message structures, enum definitions, and architectural details extracted from decompilation of the Android Auto phone app. This is the **phone side** of the AA protocol — the counterpart to head unit firmware analysis. Combined with our head unit firmware analyses (Alpine, Kenwood, Sony), this provides complete visibility into both sides of the protocol.

**No Google proprietary code is reproduced here.** Only protocol constants, enum values, message type IDs, and structural information necessary for interoperability are documented.

---

## Key Discoveries

### 1. Video Codecs Beyond H.264
The phone supports **VP9, AV1, and H.265** in addition to H.264 Baseline Profile. This means newer head units can negotiate better video codecs.

### 2. Complete ByeBye Reason Codes
aasdk only knew about QUIT(1). The full enum has 8 values including DEVICE_SWITCH, POWER_DOWN, USER_PROFILE_SWITCH, and WIRELESS_PROJECTION_DISABLED.

### 3. Video Focus States Fully Mapped
aasdk had two "UNK" video focus values. All 4 are now known, including the critical `VIDEO_FOCUS_PROJECTED_NO_INPUT_FOCUS` state.

### 4. 21 Service Types (vs aasdk's ~10)
The complete service type enum reveals CAR_CONTROL, CAR_LOCAL_MEDIA, BUFFERED_MEDIA_SINK, and WIFI_DISCOVERY that weren't previously known.

### 5. Integrated Overlay Protocol
Six new media channel message types (0x800D-0x8012) support Google Maps overlay rendering directly into the video stream — a newer capability not in aasdk.

### 6. 26 Vehicle Sensor Types
Complete sensor catalog including EV-specific sensors (toll card, vehicle energy model, trailer data, raw EV trip settings).

### 7. Complete Status/Error Taxonomy
76+ disconnect detail codes and 30+ status codes providing exact error classification for debugging.

---

## Service Type Enum (Complete)

This is the master list of all AA service types. aasdk implements types 1-9 and 17. Types 10-16 and 18-21 are production services seen in Kenwood/Sony firmware but undocumented until now.

| Value | Name | aasdk? | Description |
|-------|------|--------|-------------|
| 0 | UNKNOWN | - | Invalid/unset |
| 1 | CONTROL | Yes | Version exchange, SSL, service discovery, focus, ping, shutdown |
| 2 | VIDEO_SINK | Yes | H.264/VP9/AV1/H.265 video from phone to HU display |
| 3 | AUDIO_SINK_GUIDANCE | Yes | Navigation prompts (16kHz mono) |
| 4 | AUDIO_SINK_SYSTEM | Yes | System/notification sounds (16kHz mono) |
| 5 | AUDIO_SINK_MEDIA | Yes | Music/media (48kHz stereo) |
| 6 | AUDIO_SOURCE | Yes | Microphone input from HU to phone |
| 7 | SENSOR_SOURCE | Yes | Vehicle sensors (GPS, speed, night mode, etc.) |
| 8 | INPUT_SOURCE | Yes | Touch, keys, rotary input |
| 9 | BLUETOOTH | Yes | BT pairing and HFP management |
| 10 | NAVIGATION_STATUS | No | Turn-by-turn nav data for HU's native display |
| 11 | MEDIA_PLAYBACK_STATUS | No | Now-playing metadata for HU's native display |
| 12 | MEDIA_BROWSER | No | Media library browsing (artists, albums, playlists) |
| 13 | PHONE_STATUS | No | Call state, contact info for HU display |
| 14 | NOTIFICATION | No | Phone notifications displayed on HU |
| 15 | RADIO | No | AM/FM/HD Radio/DAB tuner control |
| 16 | VENDOR_EXTENSION | No | Vendor-specific passthrough |
| 17 | WIFI_PROJECTION | Yes | WiFi credential exchange for wireless AA |
| 18 | WIFI_DISCOVERY | No | WiFi network discovery (newer wireless AA) |
| 19 | CAR_CONTROL | No | Vehicle control integration |
| 20 | CAR_LOCAL_MEDIA | No | Control of HU's local media playback |
| 21 | BUFFERED_MEDIA_SINK | No | Buffered/offline media content |

---

## Control Channel (Service Type 1) — Message IDs

| Msg ID | Name | Direction | Proto Class | Notes |
|--------|------|-----------|-------------|-------|
| 1 | VERSION_REQUEST | Car → Phone | raw bytes | 2 shorts: major, minor |
| 2 | VERSION_RESPONSE | Phone → Car | raw bytes | 4 shorts: msg_type, major, minor, status |
| 3 | SSL_HANDSHAKE | Both | raw bytes | TLS 1.2 data via memory BIOs |
| 4 | AUTH_COMPLETE | Car → Phone | protobuf | Status enum (success/failure) |
| 6 | SERVICE_DISCOVERY_RESPONSE | Car → Phone | protobuf | HU capability manifest |
| 7 | CHANNEL_OPEN_REQUEST | Phone → Car | protobuf | service_type + channel_id |
| 11 | PING_REQUEST | Car → Phone | protobuf | long timestamp |
| 12 | PING_RESPONSE | Phone → Car | protobuf | long timestamp (echo) |
| 14 | NAVIGATION_FOCUS_NOTIFICATION | Car → Phone | protobuf | NAV_FOCUS_NATIVE(1) or NAV_FOCUS_PROJECTED(2) |
| 15 | BYE_BYE_REQUEST | Both | protobuf | ByeByeReason enum |
| 16 | BYE_BYE_RESPONSE | Both | protobuf | Empty ack, triggers disconnect |
| 19 | AUDIO_FOCUS_NOTIFICATION | Car → Phone | protobuf | AudioFocusState enum + unsolicited flag |
| 24 | CALL_AVAILABILITY_STATUS | Car → Phone | protobuf | boolean: is_call_available |
| 26 | SERVICE_DISCOVERY_UPDATE | Car → Phone | protobuf | Updated ServiceDescriptor for runtime changes |
| 255 | (error) | - | - | Unexpected message → disconnect |
| 65535 | (error) | - | - | Framing error → disconnect |

### Protocol Version Negotiation

```
Phone supports: v1.6 (minimum) through v1.7 (preferred)
Fallback cap: v6.0
If car requests > v1.7: negotiated version falls to v6.0
If no compatible version: STATUS_NO_COMPATIBLE_VERSION
```

---

## Media/Video Channel — Message IDs

These IDs are shared across VIDEO_SINK and AUDIO_SINK channels (service types 2-5):

| Msg ID | Hex | Name | Direction | Notes |
|--------|-----|------|-----------|-------|
| 0 | 0x0000 | DATA | Phone → Car | Video/audio data frame with timestamp |
| 1 | 0x0001 | CODEC_CONFIG | Phone → Car | Codec init data (H.264 SPS/PPS, etc.) |
| 32768 | 0x8000 | SETUP | Phone → Car | Media setup/negotiation |
| 32769 | 0x8001 | START | Phone → Car | Begin streaming |
| 32770 | 0x8002 | STOP | Phone → Car | Stop streaming |
| 32771 | 0x8003 | CONFIG | Both | Media configuration exchange |
| 32772 | 0x8004 | ACK | Car → Phone | Flow control acknowledgment |
| 32773 | 0x8005 | MICROPHONE_REQUEST | Car → Phone | Open/close mic |
| 32774 | 0x8006 | MICROPHONE_RESPONSE | Phone → Car | Mic open/close ack |
| 32775 | 0x8007 | VIDEO_FOCUS_REQUEST | Car → Phone | VideoFocusType + VideoFocusReason |
| 32776 | 0x8008 | VIDEO_FOCUS_NOTIFICATION | Phone → Car | VideoFocusType + boolean |
| 32777 | 0x8009 | UPDATE_UI_CONFIG_REQUEST | Car → Phone | Request UI reconfiguration |
| 32778 | 0x800A | UPDATE_UI_CONFIG_REPLY | Phone → Car | UI config response |
| 32779 | 0x800B | AUDIO_UNDERFLOW_NOTIFICATION | Car → Phone | Buffer underrun |
| 32780 | 0x800C | ACTION_TAKEN_NOTIFICATION | Phone → Car | User action on phone |
| 32781 | 0x800D | OVERLAY_PARAMETERS | Phone → Car | Google Maps overlay setup |
| 32782 | 0x800E | OVERLAY_START | Phone → Car | Begin overlay rendering |
| 32783 | 0x800F | OVERLAY_STOP | Phone → Car | End overlay rendering |
| 32784 | 0x8010 | OVERLAY_SESSION_DATA | Phone → Car | Overlay frame data |
| 32785 | 0x8011 | UPDATE_HU_UI_CONFIG_REQUEST | Car → Phone | HU requests UI config update |
| 32786 | 0x8012 | UPDATE_HU_UI_CONFIG_RESPONSE | Phone → Car | UI config update response |
| 32787 | 0x8013 | MEDIA_STATS | Both | Media quality statistics |
| 32788 | 0x8014 | MEDIA_OPTIONS | Both | Media stream options |

**New vs aasdk:** Messages 0x8009-0x8014 are all new. The integrated overlay messages (0x800D-0x8010) are particularly interesting — they support rendering Google Maps overlays directly into the video stream, which is a newer AA feature.

---

## Protocol Enums

### Media Codec Types

| Value | Name | Notes |
|-------|------|-------|
| 1 | MEDIA_CODEC_AUDIO_PCM | Raw PCM audio |
| 2 | MEDIA_CODEC_AUDIO_AAC_LC | AAC-LC in raw container |
| 3 | MEDIA_CODEC_VIDEO_H264_BP | H.264 Baseline Profile (standard) |
| 4 | MEDIA_CODEC_AUDIO_AAC_LC_ADTS | AAC-LC with ADTS framing |
| 5 | **MEDIA_CODEC_VIDEO_VP9** | VP9 video (new) |
| 6 | **MEDIA_CODEC_VIDEO_AV1** | AV1 video (new) |
| 7 | **MEDIA_CODEC_VIDEO_H265** | H.265/HEVC video (new) |

**Implication for Prodigy:** We currently only support H.264 BP. VP9/AV1/H.265 support would reduce bandwidth and improve quality, but requires hardware decode support. The Pi 4 has H.265 decode but not VP9 or AV1.

### Audio Focus States

| Value | Name |
|-------|------|
| 0 | AUDIO_FOCUS_STATE_INVALID |
| 1 | AUDIO_FOCUS_STATE_GAIN |
| 2 | AUDIO_FOCUS_STATE_GAIN_TRANSIENT |
| 3 | AUDIO_FOCUS_STATE_LOSS |
| 4 | AUDIO_FOCUS_STATE_LOSS_TRANSIENT_CAN_DUCK |
| 5 | AUDIO_FOCUS_STATE_LOSS_TRANSIENT |
| 6 | AUDIO_FOCUS_STATE_GAIN_MEDIA_ONLY |
| 7 | AUDIO_FOCUS_STATE_GAIN_TRANSIENT_GUIDANCE_ONLY |

### Audio Focus Request Types

| Value | Name |
|-------|------|
| 1 | AUDIO_FOCUS_GAIN |
| 2 | AUDIO_FOCUS_GAIN_TRANSIENT |
| 3 | AUDIO_FOCUS_GAIN_TRANSIENT_MAY_DUCK |
| 4 | AUDIO_FOCUS_RELEASE |

### Video Focus Types

Previously partially unknown in aasdk. Now fully mapped:

| Value | Name | Description |
|-------|------|-------------|
| 1 | VIDEO_FOCUS_PROJECTED | AA has focus — show projected screen |
| 2 | VIDEO_FOCUS_NATIVE | HU has focus — show native/OEM screen |
| 3 | VIDEO_FOCUS_NATIVE_TRANSIENT | HU has temporary focus (e.g. reverse camera) |
| 4 | **VIDEO_FOCUS_PROJECTED_NO_INPUT_FOCUS** | AA video displayed but touch goes to HU |

**Value 4 is new and important.** This allows showing AA video as a background/secondary display while the HU handles touch input — useful for split-screen setups or displaying AA nav in a cluster display.

### Video Focus Reasons

| Value | Name | Description |
|-------|------|-------------|
| 0 | UNKNOWN | Default/unset |
| 1 | PHONE_SCREEN_OFF | Phone screen turned off |
| 2 | LAUNCH_NATIVE | User switched to HU native UI |
| 3 | LAST_MODE | Restore previous focus state |
| 4 | USER_SELECTION | User explicitly switched views |

### ByeBye Reasons (Complete)

aasdk only knew QUIT(1). The full set:

| Value | Name | Description |
|-------|------|-------------|
| 1 | USER_SELECTION | User chose to disconnect |
| 2 | DEVICE_SWITCH | Switching to different phone |
| 3 | NOT_SUPPORTED | Feature not supported |
| 4 | NOT_CURRENTLY_SUPPORTED | Feature temporarily unavailable |
| 5 | PROBE_SUPPORTED | Probe connection (capability check) |
| 6 | WIRELESS_PROJECTION_DISABLED | WiFi AA disabled |
| 7 | POWER_DOWN | HU shutting down |
| 8 | USER_PROFILE_SWITCH | Switching user profiles |

### Navigation Focus Types

| Value | Name |
|-------|------|
| 1 | NAV_FOCUS_NATIVE |
| 2 | NAV_FOCUS_PROJECTED |

### Bluetooth Pairing Methods

aasdk had values 1 and 3 as "UNK". Now fully mapped:

| Value | Name |
|-------|------|
| -1 | BLUETOOTH_PAIRING_UNAVAILABLE |
| 1 | BLUETOOTH_PAIRING_OOB |
| 2 | BLUETOOTH_PAIRING_NUMERIC_COMPARISON |
| 3 | BLUETOOTH_PAIRING_PASSKEY_ENTRY |
| 4 | BLUETOOTH_PAIRING_PIN |

### Display Types

| Value | Name | Description |
|-------|------|-------------|
| 0 | DISPLAY_TYPE_MAIN | Primary HU display |
| 1 | DISPLAY_TYPE_CLUSTER | Instrument cluster display |
| 2 | DISPLAY_TYPE_AUXILIARY | Secondary/rear display |

### Driver Position

| Value | Name |
|-------|------|
| 0 | LEFT |
| 1 | RIGHT |
| 2 | CENTER |
| 3 | UNKNOWN |

### Gear Types

| Value | Name |
|-------|------|
| -1 | UNKNOWN_GEAR |
| 0 | NEUTRAL |
| 1-6 | GEAR_1 through GEAR_6 |
| 10 | DRIVE |
| 11 | PARK |
| 12 | REVERSE |

### Video Resize Action

| Value | Name |
|-------|------|
| 0 | ACTION_UNKNOWN |
| 1 | RESIZE_TO_SMALLER |
| 2 | RESIZE_TO_LARGER |

### Haptic Feedback Types

| Value | Name |
|-------|------|
| 1 | SELECT |
| 2 | FOCUS_CHANGE |
| 3 | DRAG_SELECT |
| 4 | DRAG_START |
| 5 | DRAG_END |

### Car Local Media Playback Actions

| Value | Name |
|-------|------|
| 0 | PLAY |
| 1 | PAUSE |
| 2 | PREVIOUS |
| 3 | NEXT |
| 4 | STOP |

---

## Sensor Types (Complete — 26 Types)

| Value | Name | aasdk? | Description |
|-------|------|--------|-------------|
| 1 | SENSOR_LOCATION | Yes | GPS lat/lon/altitude/bearing |
| 2 | SENSOR_COMPASS | Yes | Compass heading |
| 3 | SENSOR_SPEED | Yes | Vehicle speed |
| 4 | SENSOR_RPM | No | Engine RPM |
| 5 | SENSOR_ODOMETER | No | Odometer reading |
| 6 | SENSOR_FUEL | No | Fuel level/range |
| 7 | SENSOR_PARKING_BRAKE | No | Parking brake state |
| 8 | SENSOR_GEAR | No | Current gear |
| 9 | SENSOR_OBDII_DIAGNOSTIC_CODE | No | OBD-II DTCs |
| 10 | SENSOR_NIGHT_MODE | Yes | Day/night display mode |
| 11 | SENSOR_ENVIRONMENT_DATA | No | Outside temperature, pressure |
| 12 | SENSOR_HVAC_DATA | No | Climate control state |
| 13 | SENSOR_DRIVING_STATUS_DATA | Yes | Driving/parked/moving (mandatory) |
| 14 | SENSOR_DEAD_RECKONING_DATA | No | Wheel ticks + gyro for tunnel nav |
| 15 | SENSOR_PASSENGER_DATA | No | Passenger occupancy |
| 16 | SENSOR_DOOR_DATA | No | Door open/close state |
| 17 | SENSOR_LIGHT_DATA | No | Headlight/turn signal state |
| 18 | SENSOR_TIRE_PRESSURE_DATA | No | TPMS data |
| 19 | SENSOR_ACCELEROMETER_DATA | No | 3-axis accelerometer |
| 20 | SENSOR_GYROSCOPE_DATA | No | 3-axis gyroscope |
| 21 | SENSOR_GPS_SATELLITE_DATA | No | GPS satellite info (PRN, SNR, etc.) |
| 22 | SENSOR_TOLL_CARD | No | Toll transponder data |
| 23 | SENSOR_VEHICLE_ENERGY_MODEL_DATA | No | EV energy consumption model |
| 24 | SENSOR_TRAILER_DATA | No | Trailer connection state |
| 25 | SENSOR_RAW_VEHICLE_ENERGY_MODEL | No | Raw EV energy telemetry |
| 26 | SENSOR_RAW_EV_TRIP_SETTINGS | No | EV trip/range settings |

---

## Status Codes (Complete)

### Protocol Status (`vyw`)

| Value | Name |
|-------|------|
| 1 | STATUS_UNSOLICITED_MESSAGE |
| 0 | STATUS_SUCCESS |
| -1 | STATUS_NO_COMPATIBLE_VERSION |
| -2 | STATUS_CERTIFICATE_ERROR |
| -3 | STATUS_AUTHENTICATION_FAILURE |
| -4 | STATUS_INVALID_SERVICE |
| -5 | STATUS_INVALID_CHANNEL |
| -6 | STATUS_INVALID_PRIORITY |
| -7 | STATUS_INTERNAL_ERROR |
| -8 | STATUS_MEDIA_CONFIG_MISMATCH |
| -9 | STATUS_INVALID_SENSOR |
| -10 | STATUS_BLUETOOTH_PAIRING_DELAYED |
| -11 | STATUS_BLUETOOTH_UNAVAILABLE |
| -12 | STATUS_BLUETOOTH_INVALID_ADDRESS |
| -13 | STATUS_BLUETOOTH_INVALID_PAIRING_METHOD |
| -14 | STATUS_BLUETOOTH_INVALID_AUTH_DATA |
| -15 | STATUS_BLUETOOTH_AUTH_DATA_MISMATCH |
| -16 | STATUS_BLUETOOTH_HFP_ANOTHER_CONNECTION |
| -17 | STATUS_BLUETOOTH_HFP_CONNECTION_FAILURE |
| -18 | STATUS_KEYCODE_NOT_BOUND |
| -19 | STATUS_RADIO_INVALID_STATION |
| -20 | STATUS_INVALID_INPUT |
| -21 | STATUS_RADIO_STATION_PRESETS_NOT_SUPPORTED |
| -22 | STATUS_RADIO_COMM_ERROR |
| -23 | STATUS_AUTHENTICATION_FAILURE_CERT_NOT_YET_VALID |
| -24 | STATUS_AUTHENTICATION_FAILURE_CERT_EXPIRED |
| -25 | STATUS_PING_TIMEOUT |
| -26 | STATUS_CAR_PROPERTY_SET_REQUEST_FAILED |
| -27 | STATUS_CAR_PROPERTY_LISTENER_REGISTRATION_FAILED |
| -250 | STATUS_COMMAND_NOT_SUPPORTED |
| -251 | STATUS_FRAMING_ERROR |
| -253 | STATUS_UNEXPECTED_MESSAGE |
| -254 | STATUS_BUSY |
| -255 | STATUS_OUT_OF_MEMORY |

### Disconnect Reasons (`xkh`)

| Value | Name |
|-------|------|
| 0 | UNKNOWN_CODE |
| 1 | PROTOCOL_INCOMPATIBLE_VERSION |
| 2 | PROTOCOL_WRONG_CONFIGURATION |
| 3 | PROTOCOL_IO_ERROR |
| 4 | PROTOCOL_BYEBYE_REQUESTED_BY_CAR |
| 5 | PROTOCOL_BYEBYE_REQUESTED_BY_USER |
| 6 | PROTOCOL_WRONG_MESSAGE |
| 7 | PROTOCOL_AUTH_FAILED |
| 8 | PROTOCOL_AUTH_FAILED_BY_CAR |
| 9 | TIMEOUT |
| 10 | NO_LAUNCHER |
| 11 | COMPOSITION |
| 12 | CAR_NOT_RESPONDING |
| 13 | PROTOCOL_AUTH_FAILED_BY_CAR_CERT_NOT_YET_VALID |
| 14 | PROTOCOL_AUTH_FAILED_BY_CAR_CERT_EXPIRED |
| 15 | CONNECTION_ERROR |
| 16 | USB_ACCESSORY_ERROR |
| 17 | CAR_SERVICE_INIT_ERROR |
| 18 | CAR_SERVICE_CONNECTION_ERROR |
| 19 | USB_CHARGE_ONLY |
| 20 | PREFLIGHT_FAILED |
| 21 | SOCKET_VPN_CONNECTION_ERROR |
| 22 | NO_MANAGE_USB_PERMISSION_ERROR |
| 23 | FRX_ERROR |
| 24 | AUDIO_ERROR |
| 25 | NO_NEARBY_DEVICES_PERMISSION_ERROR |
| 26 | PROJECTION_PROCESS_CRASH_LOOP |
| 1000 | BLUETOOTH_FAILURE |
| 1001 | SESSION_OBSOLETE_DUE_TO_NEW_CONNECTION |

### Disconnect Detail Codes (`xki`)

<details>
<summary>Click to expand (76 values)</summary>

| Value | Name |
|-------|------|
| 0 | UNKNOWN_DETAIL |
| 1 | NO_SENSORS |
| 2 | SENSORS_FAIL |
| 3 | NO_ACCESSORY_PERMISSION |
| 4 | NO_ACCESSORY_FD |
| 5 | SOCKET_FAIL |
| 6 | BAD_MIC_AUDIO_CONFIG |
| 7 | BAD_AUDIO_CONFIG |
| 8 | MISSING_AUDIO_CONFIG |
| 9 | NAV_NO_IMAGE_OPTIONS |
| 10 | NAV_BAD_SIZE |
| 11 | NAV_BAD_COLOR |
| 12 | BAD_VIDEO |
| 13 | MISSING_VIDEO |
| 14 | DISPLAY_REMOVAL_TIMEOUT |
| 15 | NO_AUDIO_CAPTURE |
| 16 | MISSING_LAUNCHER |
| 17 | NO_VIDEO_CONFIG |
| 18 | BAD_CODEC_RESOLUTION |
| 19 | BAD_DISPLAY_RESOLUTION |
| 20 | BAD_FPS |
| 21 | NO_DENSITY |
| 22 | BAD_DENSITY |
| 23 | NO_SENSORS2 |
| 24 | NO_AUDIO_MIC |
| 25 | NO_DISPLAY |
| 26 | NO_INPUT |
| 27 | COMPOSITION_RENDER_ERROR |
| 28 | COMPOSITION_IDLE_RENDER_ERROR |
| 29 | COMPOSITION_SCREENSHOT_ERROR |
| 30 | COMPOSITION_WINDOW_INIT_ERROR |
| 31 | COMPOSITION_INIT_FAIL |
| 32 | VENDOR_START_FAIL |
| 33 | VIDEO_ENCODING_INIT_FAIL |
| 34 | BYEBYE_BY_CAR |
| 35 | BYEBYE_BY_USER |
| 36 | UNEXPECTED_BYEBYE_RESPONSE |
| 37 | BYEBYE_TIMEOUT |
| 38 | INVALID_ACK |
| 39 | INVALID_ACK_CONFIG |
| 40 | NO_VIDEO_CONFIGS |
| 41 | EARLY_VIDEO_FOCUS |
| 42 | ERROR_STARTING_SERVICES |
| 43 | AUTH_FAILED |
| 44 | AUTH_FAILED_BY_CAR |
| 45 | FRAMING_ERROR |
| 46 | UNEXPECTED_MESSAGE |
| 47 | BAD_VERSION |
| 48 | VIDEO_ACK_TIMEOUT |
| 49 | AUDIO_ACK_TIMEOUT |
| 50 | WRITER_IO_ERROR |
| 51 | WRITER_UNKNOWN_EXCEPTION |
| 52 | READER_CLOSE |
| 53 | READER_INIT_FAIL |
| 54 | READER_IO_ERROR |
| 55 | AUTH_FAILED_BY_CAR_CERT_NOT_YET_VALID |
| 56 | AUTH_FAILED_BY_CAR_CERT_EXPIRED |
| 57 | PING_TIMEOUT |
| 58 | MULTIPLE_DISPLAY_CONFIGS |
| 59 | WIFI_NETWORK_UNAVAILABLE |
| 60 | WIFI_NETWORK_DISCONNECTED |
| 61 | EMPTY_USB_ACCESSORY_LIST |
| 62 | SPURIOUS_USB_ACCESSORY_EVENT |
| 63 | INVALID_ACCESSORY |
| 64 | CONNECTION_TRANSFER_ABORTED |
| 65 | DISPLAY_ID_INVALID |
| 66 | BAD_PRIMARY_DISPLAY |
| 67 | NO_ACTIVITY_LAYOUT_CONFIG |
| 68 | DISPLAY_CONFLICT |
| 69 | TOO_MANY_INPUTS |
| 70 | OUT_OF_MEMORY |
| 71 | INVALID_UI_CONFIG |
| 72 | NO_AUDIO_PLAYBACK_SERVICE |
| 73 | NO_ACTIVITY_FOUND |
| 74 | TOO_MANY_CRASHES |
| 75 | AUTH_FAILED_OBSOLETE_SSL |

</details>

---

## Protobuf Message Structures

### ServiceDiscoveryResponse (Control Message 6)

```protobuf
message ServiceDiscoveryResponse {
  optional int32 presence_bits = 1;
  repeated ServiceDescriptor services = 2;
  optional string head_unit_make = 3;
  optional string head_unit_model = 4;
  optional string head_unit_year = 5;
  optional string head_unit_vehicle_id = 6;
  optional DriverPosition driver_position = 7;
  optional string head_unit_build = 8;
  optional string head_unit_serial = 9;
  optional string head_unit_sw_version = 10;
  optional string head_unit_sw_build = 11;
  optional bool unknown_bool_12 = 12;
  // field 13 not present
  optional int32 unknown_int_14 = 14;
  optional string unknown_string_15 = 15;
  optional bool unknown_bool_16 = 16;
  optional Unknown17 unknown_17 = 17;
}
```

### ServiceDescriptor (within ServiceDiscoveryResponse)

Each descriptor identifies a service type and populates exactly one config sub-message:

```protobuf
message ServiceDescriptor {
  optional int32 presence_bits = 1;
  optional ServiceType service_type = 2;
  optional MediaSinkConfig media_sink = 3;
  optional InputSourceConfig input_source = 4;
  optional SensorSourceConfig sensor_source = 5;
  optional AudioSourceConfig audio_source = 6;
  optional BluetoothConfig bluetooth = 7;
  optional NavigationStatusConfig navigation_status = 8;
  optional MediaPlaybackStatusConfig media_playback_status = 9;
  optional PhoneStatusConfig phone_status = 10;
  optional NotificationConfig notification = 11;
  optional MediaBrowserConfig media_browser = 12;
  optional RadioConfig radio = 13;
  optional VendorExtensionConfig vendor_extension = 14;
  optional WifiProjectionConfig wifi_projection = 15;
  optional CarControlConfig car_control = 16;
  optional CarLocalMediaConfig car_local_media = 17;
  optional BufferedMediaSinkConfig buffered_media_sink = 18;
}
```

### VideoFocusRequest (Media Message 0x8007)

```protobuf
message VideoFocusRequest {
  optional VideoFocusType focus_type = 2 [default = VIDEO_FOCUS_PROJECTED];
  optional VideoFocusReason reason = 3;
}
```

### VideoFocusNotification (Media Message 0x8008)

```protobuf
message VideoFocusNotification {
  optional VideoFocusType focus_type = 1 [default = VIDEO_FOCUS_PROJECTED];
  optional bool unknown_bool = 2;
}
```

---

## AA-Specific Key Codes (65535+)

Standard Android keycodes (0-271) are all supported. These are the AA-specific extensions for head unit hardware buttons:

| Value | Name | Description |
|-------|------|-------------|
| 65535 | KEYCODE_SENTINEL | End marker / invalid |
| 65536 | KEYCODE_ROTARY_CONTROLLER | Rotary encoder knob |
| 65537 | KEYCODE_MEDIA | Media/source button |
| 65538 | KEYCODE_NAVIGATION | Navigation button |
| 65539 | KEYCODE_RADIO | Radio/tuner button |
| 65540 | KEYCODE_TEL | Phone/call button |
| 65541 | KEYCODE_PRIMARY_BUTTON | Primary custom HU button |
| 65542 | KEYCODE_SECONDARY_BUTTON | Secondary custom HU button |
| 65543 | KEYCODE_TERTIARY_BUTTON | Tertiary custom HU button |
| 65544 | KEYCODE_TURN_CARD | Dismiss turn card |

---

## SSL/TLS Architecture

From the control channel implementation:
- Protocol: **TLSv1.2**
- Phone acts as TLS **server** (`setUseClientMode(false)`)
- Mutual authentication required (`setNeedClientAuth(true)`)
- Root CA: Self-signed "Google Automotive Link" certificate (2014-06-06 to 2044-06-05)
  - Subject: `C=US, ST=California, L=Mountain View, O=Google Automotive Link`
- Private key derivation: 7 rounds of hashing (device identity + cert bytes → SHA → 48-byte material → 32-byte AES key + 16-byte IV)
- The phone-side cert is encrypted with an AES key derived from device identity

---

## GMS Car API Layer

Above the GAL protocol, the unobfuscated `com.google.android.gms.car` package provides:

| Class | Purpose |
|-------|---------|
| `CarGalMessage` | Wire message wrapper: channelId, messageType, data, timestamp, flags |
| `CarSensorEvent` | Sensor event: type, timestamp, float[], byte[] — GPS uses lat/lon as int×1e-7 |
| `CarPhoneStatus` | Phone status with CarCall[] (state, type, number, name, contact, thumbnail) |
| `AudioFocusInfo` | Audio focus with AudioAttributes, gain type, package name |
| `senderprotocol.Channel` | Channel abstraction — 2-byte type prefix + protobuf body |
| `navigation.NavigationState` | Nav state: steps[], destinations[], destination details |
| `navigation.NavigationStep` | Nav step: maneuver, roadName, lanes, cueTexts, turnImage |
| `navigation.Maneuver` | Turn maneuver type enum |

**Video focus simplification:** At the GMS IPC boundary, the 4-state VideoFocusType enum is reduced to a simple boolean (`ICarVideoFocusListener.e(boolean hasVideoFocus)`). The richer state is internal to the AA protocol.

---

## Implications for OpenAuto Prodigy

### Immediate Wins

1. **Fix ByeBye reason codes** — We can now send proper disconnect reasons (POWER_DOWN for shutdown, USER_SELECTION for user-initiated disconnect) instead of a generic QUIT.

2. **Implement video focus properly** — With all 4 states mapped, we can handle:
   - PROJECTED (normal AA display)
   - NATIVE (user switched to our launcher)
   - NATIVE_TRANSIENT (reverse camera, settings overlay)
   - PROJECTED_NO_INPUT_FOCUS (AA visible but touch handled by HU — useful for split-screen/PIP)

3. **Send VideoFocusReason** — When requesting focus changes, include the reason (USER_SELECTION, LAUNCH_NATIVE, etc.) for better phone-side behavior.

4. **BT pairing methods fully mapped** — OOB(1), NUMERIC_COMPARISON(2), PASSKEY_ENTRY(3), PIN(4). Can now properly negotiate all pairing types.

5. **Better error reporting** — The 30+ status codes give us precise error categories for debugging connection issues.

### Medium-Term

6. **H.265 video support** — The Pi 4 has hardware H.265 decode. Advertising H.265 in our ServiceDiscoveryResponse could improve video quality/bandwidth.

7. **Navigation status service (type 10)** — Show turn-by-turn in our native UI when not in AA projection.

8. **Media playback status (type 11)** — Show now-playing in our launcher.

9. **Phone status service (type 13)** — Show incoming calls in our native UI.

10. **Notification service (type 14)** — Display phone notifications.

### Long-Term

11. **Integrated overlay support** — The 0x800D-0x8010 messages suggest AA can render overlays (like Google Maps) directly into the video stream. This is a significant UX improvement for newer AA versions.

12. **Multi-display support** — DISPLAY_TYPE_CLUSTER and DISPLAY_TYPE_AUXILIARY confirm multi-display is a first-class AA feature. We could support an instrument cluster display via a second screen/output.

13. **Full vehicle sensor integration** — With 26 sensor types mapped, we could feed real OBD-II data to the phone via ELM327 adapter, enabling fuel-aware routing, gear display, etc.

### Known Unknowns Resolved

| Previously Unknown | Now Known |
|-------------------|-----------|
| ByeBye reasons beyond QUIT(1) | 8 reasons: USER_SELECTION through USER_PROFILE_SWITCH |
| VideoFocusType UNK_1, UNK_2 | All 4 values: PROJECTED, NATIVE, NATIVE_TRANSIENT, PROJECTED_NO_INPUT_FOCUS |
| VideoFocusReason values | 5 values: UNKNOWN through USER_SELECTION |
| BluetoothPairingMethod 1, 3 | OOB(1), PASSKEY_ENTRY(3) |
| Channels 9-13 service types | Full service type enum with 21 types |
| Video codec negotiation | H.264, VP9, AV1, H.265 all supported |
| Disconnect error taxonomy | 76 detail codes for precise debugging |

---

## Obfuscation Map (Key Classes)

For future reference, mapping obfuscated class names to their real identities:

| Obfuscated | Real Name | Category |
|------------|-----------|----------|
| `qmy` | ControlChannelHandler | Protocol handler |
| `qoh` | BaseChannelHandler | Protocol handler |
| `qnx` | AudioEndpointHandler | Protocol handler |
| `qor` | SensorEndpointHandler | Protocol handler |
| `qly` | BluetoothEndpointHandler | Protocol handler |
| `wby` | ServiceDiscoveryResponse | Protobuf message |
| `wbw` | ServiceDescriptor | Protobuf message |
| `wbz` | ServiceDiscoveryUpdate | Protobuf message |
| `vwx` | ChannelOpenRequest | Protobuf message |
| `vwf` | ByeByeRequest | Protobuf message |
| `vwg` | ByeByeResponse | Protobuf message |
| `vvq` | AudioFocusNotification | Protobuf message |
| `vvv` | AuthComplete | Protobuf message |
| `waj` | PingRequest | Protobuf message |
| `wak` | PingResponse | Protobuf message |
| `wdd` | VideoFocusRequest | Protobuf message |
| `wdb` | VideoFocusNotification | Protobuf message |
| `vwh` | CallAvailabilityStatus | Protobuf message |
| `vyw` | StatusCode | Enum |
| `vvu` | AudioFocusState | Enum |
| `vvt` | AudioFocusRequestType | Enum |
| `vwe` | ByeByeReason | Enum |
| `wbv` | SensorType | Enum |
| `vyn` | MediaCodecType | Enum |
| `wda` | VideoFocusType | Enum |
| `wdc` | VideoFocusReason | Enum |
| `vzb` | NavFocusType | Enum |
| `vvz` | BluetoothPairingMethod | Enum |
| `qlf` | ServiceType | Enum |
| `vxg` | DisplayType | Enum |
| `vxi` | DriverPosition | Enum |
| `vyh` | KeyCode | Enum |
| `xkh` | DisconnectReason | Enum |
| `xki` | DisconnectDetail | Enum |
| `vee` | MediaMessageType | Enum |
| `qpr` | ChannelPriority | Enum |
| `zyt` | GeneratedMessage (protobuf base) | Framework |
| `zyx` | EnumLite (protobuf interface) | Framework |
