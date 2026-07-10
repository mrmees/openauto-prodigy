# Android Auto Protocol Cross-Reference

> **Sources:** Sony XAV-AX100 firmware (GAL Protocol, debug symbols) + Android Auto APK v16.1 (phone-side, decompiled)
> **Date:** 2026-02-23
> **Purpose:** Definitive protocol reference for OpenAuto Prodigy, cross-referencing both sides of the AA connection

This document combines findings from two complementary analyses:

1. **Sony XAV-AX100 firmware** — Head unit side. Google's AAP Receiver Library (GAL Protocol) with debug symbols in `libspandroidauto.so` and unobfuscated protobuf message names in `accessory_server`. Built on Sunplus SPHE8388, Linux 3.4.5, OpenSSL 1.0.2h.

2. **Android Auto APK v16.1** — Phone side. Decompiled with jadx, heavily obfuscated (ProGuard/R8), but enum names and protobuf field structures survive. Pure Java/Dalvik implementation.

Together these provide **both sides of every protocol exchange** — the head unit's C++ implementation with real type names, and the phone's Java implementation with exact enum values and wire format details.

---

## Service Type Cross-Reference

The Sony firmware implements services 1-16 (via protobuf types in `accessory_server`). The APK defines 21 service types. Combined:

| ID | Name | Sony HU | APK Phone | aasdk | Protobuf Messages (from Sony symbols) |
|----|------|---------|-----------|-------|---------------------------------------|
| 1 | CONTROL | Yes | Yes | Yes | ServiceDiscovery*, ChannelOpen*, Ping*, ByeBye*, AuthResponse |
| 2 | VIDEO_SINK | Yes | Yes | Yes | VideoConfiguration, VideoFocus* |
| 3 | AUDIO_SINK_GUIDANCE | Yes | Yes | Yes | AudioConfiguration, AudioFocus*, MicrophoneRequest/Response |
| 4 | AUDIO_SINK_SYSTEM | Yes | Yes | Yes | (shares audio message types with 3,5) |
| 5 | AUDIO_SINK_MEDIA | Yes | Yes | Yes | (shares audio message types with 3,4) |
| 6 | AUDIO_SOURCE | Yes | Yes | Yes | AudioSourceCallbacks |
| 7 | SENSOR_SOURCE | Yes | Yes | Yes | SensorSourceService, SensorBatch, SensorRequest/Response/Error |
| 8 | INPUT_SOURCE | Yes | Yes | Yes | InputSourceService, InputReport, InputFeedback, TouchEvent, KeyEvent |
| 9 | BLUETOOTH | Yes | Yes | Yes | BluetoothService, BluetoothPairingRequest/Response, BluetoothAuthenticationData |
| 10 | NAVIGATION_STATUS | Yes | Yes | No | NavigationStatusService, NavigationStatus, NavigationNextTurnEvent/DistanceEvent, NavFocus* |
| 11 | MEDIA_PLAYBACK_STATUS | Yes | Yes | No | MediaPlaybackStatusService, MediaPlaybackStatus, MediaPlaybackMetadata |
| 12 | MEDIA_BROWSER | Yes | Yes | No | MediaBrowserService, MediaBrowserInput, MediaGetNode, MediaList/Song/Root/SourceNode |
| 13 | PHONE_STATUS | Yes | Yes | No | PhoneStatusService, PhoneStatus, PhoneStatus_Call, PhoneStatusInput |
| 14 | NOTIFICATION | Yes | Yes | No | GenericNotificationService, GenericNotificationMessage, Subscribe/Unsubscribe/Ack |
| 15 | RADIO | Yes | Yes | No | RadioService, RadioProperties, RadioStationInfo, RadioStateNotification, HD Radio*, StationPreset*, Traffic* |
| 16 | VENDOR_EXTENSION | Yes | Yes | No | VendorExtensionService |
| 17 | WIFI_PROJECTION | — | Yes | Yes | (WiFi credential exchange) |
| 18 | WIFI_DISCOVERY | — | Yes | No | (newer wireless AA discovery) |
| 19 | CAR_CONTROL | — | Yes | No | (vehicle control integration) |
| 20 | CAR_LOCAL_MEDIA | — | Yes | No | (HU local media playback control) |
| 21 | BUFFERED_MEDIA_SINK | — | Yes | No | (buffered/offline media) |

**Key insight:** The Sony firmware (2018) implements services 1-16. The APK (2026) adds 17-21. Services 17-21 are newer additions not present in the GAL SDK that shipped with the SPHE8388.

---

## Complete Protobuf Message Catalog

### Source Key
- **Sony** = Unobfuscated type name from `accessory_server` debug symbols
- **APK** = Obfuscated class extending `zyt` (GeneratedMessage), deobfuscated via string analysis
- **Both** = Confirmed in both sources

### Control Channel Messages

| Sony Type Name | APK Class | Wire ID | Direction | Fields (from APK) |
|---------------|-----------|---------|-----------|-------------------|
| (raw bytes) | — | 1 | Car→Phone | VERSION_REQUEST: 2 shorts (major, minor) |
| (raw bytes) | — | 2 | Phone→Car | VERSION_RESPONSE: 4 shorts (type, major, minor, status) |
| (raw bytes) | — | 3 | Both | SSL_HANDSHAKE: TLS 1.2 data |
| AuthResponse | `vvv` | 4 | Car→Phone | status: StatusCode enum |
| ServiceDiscoveryResponse | `wby` | 6 | Car→Phone | See detailed structure below |
| ChannelOpenRequest | `vwx` | 7 | Phone→Car | service_type(int), channel_id(int) |
| ChannelOpenResponse | — | 8 | Car→Phone | status: StatusCode enum |
| PingRequest | `waj` | 11 | Car→Phone | timestamp(long) |
| PingResponse | `wak` | 12 | Phone→Car | timestamp(long) |
| NavFocusNotification | `vyz` | 14 | Car→Phone | focus_type: NavFocusType enum |
| ByeByeRequest | `vwf` | 15 | Both | reason: ByeByeReason enum |
| ByeByeResponse | `vwg` | 16 | Both | (empty ack) |
| AudioFocusNotification | `vvq` | 19 | Car→Phone | state: AudioFocusState, unsolicited(bool) |
| — | `vwh` | 24 | Car→Phone | CALL_AVAILABILITY: is_available(bool) |
| — | `wbz` | 26 | Car→Phone | SERVICE_UPDATE: updated ServiceDescriptor |
| ChannelCloseNotification | — | — | Either | (Sony only — not seen in APK dispatch) |

### Media/Video Channel Messages

| Sony Type Name | APK Class | Wire ID | Direction | Purpose |
|---------------|-----------|---------|-----------|---------|
| (data frame) | — | 0x0000 | Phone→Car | AV data with timestamp |
| (codec config) | — | 0x0001 | Phone→Car | H.264 SPS/PPS, codec init |
| Setup | — | 0x8000 | Phone→Car | Media setup |
| Start | — | 0x8001 | Phone→Car | Begin streaming |
| Stop | — | 0x8002 | Phone→Car | Stop streaming |
| Config | `vxb` | 0x8003 | Both | Media configuration (codec, max_unacked) |
| Ack | `vvk` | 0x8004 | Car→Phone | Flow control ACK (session_id) |
| MicrophoneRequest | — | 0x8005 | Car→Phone | Open/close microphone |
| MicrophoneResponse | — | 0x8006 | Phone→Car | Mic ack |
| VideoFocusRequestNotification | `wdd` | 0x8007 | Car→Phone | focus_type(VideoFocusType), reason(VideoFocusReason) |
| VideoFocusNotification | `wdb` | 0x8008 | Phone→Car | focus_type(VideoFocusType), bool |
| — | — | 0x8009 | Car→Phone | UPDATE_UI_CONFIG_REQUEST (newer AA) |
| — | — | 0x800A | Phone→Car | UPDATE_UI_CONFIG_REPLY (newer AA) |
| — | — | 0x800B | Car→Phone | AUDIO_UNDERFLOW (newer AA) |
| — | — | 0x800C | Phone→Car | ACTION_TAKEN (newer AA) |
| — | — | 0x800D | Phone→Car | OVERLAY_PARAMETERS (newer AA) |
| — | — | 0x800E | Phone→Car | OVERLAY_START (newer AA) |
| — | — | 0x800F | Phone→Car | OVERLAY_STOP (newer AA) |
| — | — | 0x8010 | Phone→Car | OVERLAY_SESSION_DATA (newer AA) |
| — | — | 0x8011 | Car→Phone | UPDATE_HU_UI_CONFIG_REQ (newer AA) |
| — | — | 0x8012 | Phone→Car | UPDATE_HU_UI_CONFIG_RESP (newer AA) |
| — | — | 0x8013 | Both | MEDIA_STATS (newer AA) |
| — | — | 0x8014 | Both | MEDIA_OPTIONS (newer AA) |

Messages 0x8009-0x8014 exist only in the newer APK — the Sony firmware predates these additions.

### Sensor Data Messages (from Sony symbols)

Each sensor type has a corresponding protobuf data message:

| Sensor Type | Data Message (Sony) | APK Enum Value |
|-------------|-------------------|----------------|
| SENSOR_LOCATION | LocationData | 1 |
| SENSOR_COMPASS | CompassData | 2 |
| SENSOR_SPEED | SpeedData | 3 |
| SENSOR_RPM | RpmData | 4 |
| SENSOR_ODOMETER | OdometerData | 5 |
| SENSOR_FUEL | FuelData | 6 |
| SENSOR_PARKING_BRAKE | ParkingBrakeData | 7 |
| SENSOR_GEAR | GearData | 8 |
| SENSOR_OBDII_DIAGNOSTIC_CODE | DiagnosticsData | 9 |
| SENSOR_NIGHT_MODE | NightModeData | 10 |
| SENSOR_ENVIRONMENT_DATA | EnvironmentData | 11 |
| SENSOR_HVAC_DATA | HvacData | 12 |
| SENSOR_DRIVING_STATUS_DATA | DrivingStatusData | 13 |
| SENSOR_DEAD_RECKONING_DATA | DeadReckoningData | 14 |
| SENSOR_PASSENGER_DATA | PassengerData | 15 |
| SENSOR_DOOR_DATA | DoorData | 16 |
| SENSOR_LIGHT_DATA | LightData | 17 |
| SENSOR_TIRE_PRESSURE_DATA | TirePressureData | 18 |
| SENSOR_ACCELEROMETER_DATA | AccelerometerData | 19 |
| SENSOR_GYROSCOPE_DATA | GyroscopeData | 20 |
| SENSOR_GPS_SATELLITE_DATA | GpsSatelliteData | 21 |
| SENSOR_TOLL_CARD | — | 22 (APK only) |
| SENSOR_VEHICLE_ENERGY_MODEL_DATA | — | 23 (APK only) |
| SENSOR_TRAILER_DATA | — | 24 (APK only) |
| SENSOR_RAW_VEHICLE_ENERGY_MODEL | — | 25 (APK only) |
| SENSOR_RAW_EV_TRIP_SETTINGS | — | 26 (APK only) |

Sony implements sensors 1-21. The APK adds 22-26 (EV-specific sensors added after the Sony firmware shipped).

### Input Messages (from Sony symbols)

| Sony Type Name | Contents |
|---------------|----------|
| InputReport | Container for touch/key/absolute/relative events |
| TouchEvent | Touch interaction container |
| TouchEvent_Pointer | Individual touch point: action, x, y, pointer_id |
| KeyEvent | Key interaction container |
| KeyEvent_Key | Individual key: keycode, is_down |
| AbsoluteEvent | Absolute axis container |
| AbsoluteEvent_Abs | Absolute value: axis_type, value |
| RelativeEvent | Relative axis container |
| RelativeEvent_Rel | Relative delta: axis_type, delta |
| InputFeedback | Haptic feedback from phone |
| InputSourceService | Service descriptor with TouchScreen and TouchPad configs |

### Navigation Messages (from Sony symbols)

| Sony Type Name | Purpose |
|---------------|---------|
| NavigationStatusService | Service config with ImageOptions |
| NavigationStatusService_ImageOptions | Icon size, color depth, DPI |
| NavigationStatus | Current nav state (native/projected) |
| NavigationStatusStart | Begin nav status updates |
| NavigationStatusStop | End nav status updates |
| NavigationNextTurnEvent | Turn type, turn side, road name, icon |
| NavigationNextTurnDistanceEvent | Distance to turn, distance units |
| NavFocusNotification | Nav focus state (Car→Phone) |
| NavFocusRequestNotification | Nav focus request (Phone→Car) |

### Phone Status Messages (from Sony symbols)

| Sony Type Name | Purpose |
|---------------|---------|
| PhoneStatusService | Service descriptor |
| PhoneStatus | Overall phone state |
| PhoneStatus_Call | Individual call: state, type, number, name, contact, thumbnail |
| PhoneStatusInput | Phone control commands from HU |

### Media Browser Messages (from Sony symbols)

| Sony Type Name | Purpose |
|---------------|---------|
| MediaBrowserService | Service descriptor |
| MediaBrowserInput | Browse commands from HU |
| MediaPlaybackStatusService | Now-playing service descriptor |
| MediaPlaybackStatus | Playback state (playing/paused/stopped/etc.) |
| MediaPlaybackMetadata | Track title, artist, album, duration, art |
| MediaSourceService | Media source descriptor |
| MediaSource | Individual source (e.g. Spotify, local) |
| MediaSourceNode | Source tree node |
| MediaRootNode | Root of media tree |
| MediaListNode | List/folder node |
| MediaSongNode | Individual track node |
| MediaGetNode | Request for specific node |
| MediaList | List container |

### Radio Messages (from Sony symbols)

| Sony Type Name | Purpose |
|---------------|---------|
| RadioService | Service descriptor |
| RadioProperties | Frequency range, type, capabilities |
| RadioStationInfo | Station frequency, name, type |
| RadioStationMetaData | RDS/RBDS text, artist, title |
| RadioSourceRequest/Response | Tune to source |
| RadioStateNotification | Current radio state |
| ActiveRadioNotification | Active radio info |
| SelectActiveRadioRequest | Switch radio source |
| MuteRadioRequest/Response | Mute/unmute |
| StepChannelRequest/Response | Channel up/down |
| ConfigureChannelSpacingRequest/Response | Set frequency step |
| ScanStationsRequest/Response | Station scan |
| SeekStationRequest/Response | Seek next/prev |
| TuneToStationRequest/Response | Direct frequency tune |
| StationPresetsNotification | Preset list update |
| RadioStationInfoNotification | Station info update |
| StationPreset / StationPresetList | Preset storage |
| HdRadioStationInfo | HD Radio station data |
| HdRadioPsdData | HD Radio Program Service Data |
| HdRadioSisData | HD Radio Station Information Service |
| HdRadioComment | HD Radio comment text |
| HdRadioCommercial | HD Radio commercial data |
| HdRadioArtistExperience | HD Radio artist content |
| TrafficIncident | Traffic event data |
| GetTrafficUpdateRequest/Response | Traffic query |
| GetProgramListRequest/Response | Program listing |

### Notification Messages (from Sony symbols)

| Sony Type Name | Purpose |
|---------------|---------|
| GenericNotificationService | Service descriptor |
| GenericNotificationMessage | Notification content |
| GenericNotificationSubscribe | Subscribe to notification category |
| GenericNotificationUnsubscribe | Unsubscribe |
| GenericNotificationAck | Acknowledge notification |

### GAL Verification Messages (from Sony symbols)

These are Google's certification/testing protocol messages — used during GAL (Google Automotive Link) device certification:

| Sony Type Name | Purpose |
|---------------|---------|
| GalVerificationSetSensor | Inject sensor data for testing |
| GalVerificationAudioFocus | Test audio focus handling |
| GalVerificationVideoFocus | Test video focus handling |
| GalVerificationInjectInput | Inject touch/key events |
| GalVerificationMediaSinkStatus | Test media sink state |
| GalVerificationBugReportRequest/Response | Request/receive bug report |
| GalVerificationDisplayInformationRequest/Response | Query display info |
| GalVerificationScreenCaptureRequest/Response | Capture screen for testing |
| GoogleDiagnosticsBugReportRequest/Response | Google diagnostics integration |

---

## ServiceDescriptor Field Map

Cross-referencing Sony's service builder functions with APK protobuf field numbers:

```protobuf
message ServiceDescriptor {
  optional int32 presence_bits = 1;
  optional ServiceType service_type = 2;        // qlf enum
  optional MediaSinkConfig media_sink = 3;       // Sony: buildVideoSinkService, buildMediaAudioSinkService, etc.
  optional InputSourceConfig input_source = 4;   // Sony: buildInputSourceService (TouchScreen, TouchPad)
  optional SensorSourceConfig sensor_source = 5; // Sony: buildSensorSourceService
  optional AudioSourceConfig audio_source = 6;   // Sony: buildAudioSourceService (microphone)
  optional BluetoothConfig bluetooth = 7;        // Sony: AndroidAutoBluetoothConfig (address, pairing_modes)
  optional NavigationStatusConfig nav_status = 8;    // Sony: NavigationStatusService + ImageOptions
  optional MediaPlaybackConfig media_playback = 9;   // Sony: MediaPlaybackStatusService
  optional PhoneStatusConfig phone_status = 10;      // Sony: PhoneStatusService
  optional NotificationConfig notification = 11;     // Sony: GenericNotificationService
  optional MediaBrowserConfig media_browser = 12;    // Sony: MediaBrowserService
  optional RadioConfig radio = 13;                   // Sony: RadioService + RadioProperties
  optional VendorExtensionConfig vendor_ext = 14;    // Sony: VendorExtensionService
  optional WifiProjectionConfig wifi = 15;           // APK only (2018 Sony predates wireless AA)
  optional CarControlConfig car_control = 16;        // APK only (newer)
  optional CarLocalMediaConfig car_local_media = 17; // APK only (newer)
  optional BufferedMediaSinkConfig buffered = 18;    // APK only (newer)
}
```

### ServiceDiscoveryResponse Field Map

```protobuf
message ServiceDiscoveryResponse {
  optional int32 presence_bits = 1;
  repeated ServiceDescriptor services = 2;
  optional string head_unit_make = 3;           // Sony config: <VehicleMake value="SONY"/>
  optional string head_unit_model = 4;          // Sony config: <VehicleModel value="XAV-AX100"/>
  optional string head_unit_year = 5;           // Sony config: <VehicleYear value="2016"/>
  optional string head_unit_vehicle_id = 6;     // Sony: setVehicleId()
  optional DriverPosition driver_position = 7;  // Sony: setDriverPosition(int)
  optional string head_unit_build = 8;          // Sony config: <HeadUnitSwBuild/>
  optional string head_unit_serial = 9;         // (device-specific)
  optional string head_unit_sw_version = 10;    // Sony config: <HeadUnitSwVersion value="MCU:1.02.09, SOC:1.02.09"/>
  optional string head_unit_sw_build = 11;      // Sony config: <HeadUnitSwBuild/>
  optional bool unknown_12 = 12;
  // field 13 absent
  optional int32 unknown_14 = 14;
  optional string unknown_15 = 15;
  optional bool unknown_16 = 16;
  optional Unknown17 unknown_17 = 17;
}
```

---

## Configuration Cross-Reference

### Sony `androidauto_config.xml` → ServiceDiscoveryResponse Mapping

| XML Path | ServiceDiscoveryResponse Field | Value |
|----------|-------------------------------|-------|
| `CarInfo/VehicleMake` | `head_unit_make` (field 3) | "SONY" |
| `CarInfo/VehicleModel` | `head_unit_model` (field 4) | "XAV-AX100" |
| `CarInfo/VehicleYear` | `head_unit_year` (field 5) | "2016" |
| `CarInfo/HeadUnitMake` | (within service config) | "SONY" |
| `CarInfo/HeadUnitModel` | (within service config) | "XAV-AX100" |
| `CarInfo/HeadUnitSwVersion` | `head_unit_sw_version` (field 10) | "MCU:1.02.09, SOC:1.02.09" |
| `CarInfo/HeadUnitSwBuild` | `head_unit_sw_build` (field 11) | "" |

### Display Configuration → Video ServiceDescriptor

| XML Path | ServiceDescriptor.MediaSinkConfig Field | Sony Value |
|----------|----------------------------------------|------------|
| `Display/DisplayWidth` | physical_width_mm | 143 |
| `Display/DisplayHeight` | physical_height_mm | 77 |
| `Display/Density` | density_dpi | 170 |
| `Display/Resolution480P` | resolution(480p, fps=30) | true |
| `Display/Resolution720P` | resolution(720p, fps=30) | false |

### Input Configuration → Input ServiceDescriptor

| XML Path | InputSourceService Field | Sony Value |
|----------|------------------------|------------|
| `Input/TouchScreen/ScreenType` | touch_screen_type (1=cap, 2=res) | 2 (resistive) |
| `Input/TouchScreen/ScreenWidth` | touch_screen_width | 800 |
| `Input/TouchScreen/ScreenHeight` | touch_screen_height | 480 |
| `Input/Key/KEYCODE_*` | supported_keycodes[] | HOME, BACK, SEARCH, CALL, ENDCALL, MEDIA_*, DPAD_*, MEDIA, NAVIGATION, TEL, ENTER |

### Sensor Configuration → Sensor ServiceDescriptor

| XML Sensor | Enabled | APK Enum | Sony Data Message |
|-----------|---------|----------|-------------------|
| SENSOR_LOCATION | false | 1 | LocationData |
| SENSOR_COMPASS | false | 2 | CompassData |
| SENSOR_SPEED | false | 3 | SpeedData |
| SENSOR_RPM | false | 4 | RpmData |
| SENSOR_ODOMETER | false | 5 | OdometerData |
| SENSOR_FUEL | false | 6 | FuelData |
| SENSOR_PARKING_BRAKE | **true** | 7 | ParkingBrakeData |
| SENSOR_GEAR | false | 8 | GearData |
| SENSOR_OBDII_DIAGNOSTIC_CODE | false | 9 | DiagnosticsData |
| SENSOR_NIGHT_MODE | **true** | 10 | NightModeData |
| SENSOR_ENVIRONMENT_DATA | false | 11 | EnvironmentData |
| SENSOR_HVAC_DATA | false | 12 | HvacData |
| SENSOR_DRIVING_STATUS_DATA | **true** | 13 | DrivingStatusData |
| SENSOR_DEAD_RECKONING_DATA | false | 14 | DeadReckoningData |
| SENSOR_PASSENGER_DATA | false | 15 | PassengerData |
| SENSOR_DOOR_DATA | false | 16 | DoorData |
| SENSOR_LIGHT_DATA | false | 17 | LightData |
| SENSOR_TIRE_PRESSURE_DATA | false | 18 | TirePressureData |
| SENSOR_ACCELEROMETER_DATA | false | 19 | AccelerometerData |
| SENSOR_GYROSCOPE_DATA | false | 20 | GyroscopeData |
| SENSOR_GPS_SATELLITE_DATA | false | 21 | GpsSatelliteData |

Sony enables only 3 sensors: parking brake, night mode, driving status (mandatory). No GPS — the XAV-AX100 has no built-in GPS receiver.

---

## Enum Cross-Reference

Both sources agree on all enum values. The APK provides exact integer codes; the Sony firmware confirms the type names.

### Audio Focus (confirmed both sides)

| Value | APK Name | Sony Usage |
|-------|----------|------------|
| 0 | AUDIO_FOCUS_STATE_INVALID | — |
| 1 | AUDIO_FOCUS_STATE_GAIN | `sendAudioFocus(1, ...)` |
| 2 | AUDIO_FOCUS_STATE_GAIN_TRANSIENT | `sendAudioFocus(2, ...)` |
| 3 | AUDIO_FOCUS_STATE_LOSS | `sendAudioFocus(3, ...)` |
| 4 | AUDIO_FOCUS_STATE_LOSS_TRANSIENT_CAN_DUCK | `sendAudioFocus(4, ...)` |
| 5 | AUDIO_FOCUS_STATE_LOSS_TRANSIENT | `sendAudioFocus(5, ...)` |
| 6 | AUDIO_FOCUS_STATE_GAIN_MEDIA_ONLY | `sendAudioFocus(6, ...)` |
| 7 | AUDIO_FOCUS_STATE_GAIN_TRANSIENT_GUIDANCE_ONLY | `sendAudioFocus(7, ...)` |

### Video Focus (confirmed both sides)

| Value | APK Name | Sony Function |
|-------|----------|---------------|
| 1 | VIDEO_FOCUS_PROJECTED | `sendVideoFocus(1, ...)` / `videoFocusRequest(1, ...)` |
| 2 | VIDEO_FOCUS_NATIVE | `sendVideoFocus(2, ...)` / `videoFocusRequest(2, ...)` |
| 3 | VIDEO_FOCUS_NATIVE_TRANSIENT | `sendVideoFocus(3, ...)` |
| 4 | VIDEO_FOCUS_PROJECTED_NO_INPUT_FOCUS | `sendVideoFocus(4, ...)` |

Sony's `videoFocusRequest(int, int)` signature confirms **two parameters**: focus_type and reason, matching the APK's VideoFocusRequest protobuf (field 2 = type, field 3 = reason).

### Video Focus Reasons (APK only — Sony predates these)

| Value | Name |
|-------|------|
| 0 | UNKNOWN |
| 1 | PHONE_SCREEN_OFF |
| 2 | LAUNCH_NATIVE |
| 3 | LAST_MODE |
| 4 | USER_SELECTION |

### Nav Focus (confirmed both sides)

| Value | APK Name | Sony Function |
|-------|----------|---------------|
| 1 | NAV_FOCUS_NATIVE | `sendNavigationFocus(1)` / `navFocusRequest(1)` |
| 2 | NAV_FOCUS_PROJECTED | `sendNavigationFocus(2)` / `navFocusRequest(2)` |

### ByeBye Reasons (APK provides full enum, Sony confirms usage)

| Value | Name | Description |
|-------|------|-------------|
| 1 | USER_SELECTION | User chose to disconnect |
| 2 | DEVICE_SWITCH | Switching to different phone |
| 3 | NOT_SUPPORTED | Feature not supported |
| 4 | NOT_CURRENTLY_SUPPORTED | Feature temporarily unavailable |
| 5 | PROBE_SUPPORTED | Probe connection |
| 6 | WIRELESS_PROJECTION_DISABLED | WiFi AA disabled |
| 7 | POWER_DOWN | HU shutting down |
| 8 | USER_PROFILE_SWITCH | Switching profiles |

### Bluetooth Pairing (confirmed both sides)

| Value | APK Name | Sony Function |
|-------|----------|---------------|
| -1 | UNAVAILABLE | — |
| 1 | OOB | `setPairingModes(1)` |
| 2 | NUMERIC_COMPARISON | `setPairingModes(2)` |
| 3 | PASSKEY_ENTRY | `setPairingModes(3)` |
| 4 | PIN | `setPairingModes(4)` |

Sony's `AndroidAutoBluetoothConfig::setPairingModes(int)` and `setAddress(const char*)` match the APK's BluetoothConfig protobuf structure.

### Media Codecs

| Value | APK Name | Sony Support |
|-------|----------|-------------|
| 1 | AUDIO_PCM | Yes (primary audio codec) |
| 2 | AUDIO_AAC_LC | Yes |
| 3 | VIDEO_H264_BP | Yes (only video codec in 2018 firmware) |
| 4 | AUDIO_AAC_LC_ADTS | Yes |
| 5 | VIDEO_VP9 | No (APK 2026 only) |
| 6 | VIDEO_AV1 | No (APK 2026 only) |
| 7 | VIDEO_H265 | No (APK 2026 only) |

VP9, AV1, and H.265 are newer additions. The Sony firmware only supports H.264 BP.

### Status Codes (APK provides full enum)

| Value | Name | Usage Context |
|-------|------|---------------|
| 0 | SUCCESS | Normal operation |
| 1 | UNSOLICITED_MESSAGE | Unexpected message received |
| -1 | NO_COMPATIBLE_VERSION | Version mismatch |
| -2 | CERTIFICATE_ERROR | SSL cert problem |
| -3 | AUTHENTICATION_FAILURE | Auth failed |
| -4 to -9 | INVALID_* | Bad service/channel/priority/sensor/input |
| -8 | MEDIA_CONFIG_MISMATCH | Audio/video config disagreement |
| -10 to -17 | BLUETOOTH_* | BT pairing/HFP errors |
| -18 | KEYCODE_NOT_BOUND | Key not mapped |
| -19 to -22 | RADIO_* | Radio tuner errors |
| -23, -24 | CERT_NOT_YET_VALID, CERT_EXPIRED | Certificate date errors |
| -25 | PING_TIMEOUT | Keepalive failed |
| -26, -27 | CAR_PROPERTY_* | Vehicle property errors |
| -250 | COMMAND_NOT_SUPPORTED | Unsupported command |
| -251 | FRAMING_ERROR | Wire format error |
| -253 | UNEXPECTED_MESSAGE | Wrong message for state |
| -254 | BUSY | Processing, try later |
| -255 | OUT_OF_MEMORY | Resource exhaustion |

---

## Architecture Cross-Reference

### Sony HU Architecture (C++)

```
┌──────────────────────────┐
│    siba Application      │  Qt 5.3 + DirectFB
│  libandroidauto.so       │  D-Bus signals for focus/audio
│  (plugin)                │
├──────────────────────────┤
│  libspandroidauto.so     │  Sony wrapper SDK
│  SPAndroidAutoImpl       │  sendAudioFocus(), sendVideoFocus()
│  SPAndroidAutoNotifier   │  audioFocusRequest(), videoFocusRequest()
├──────────────────────────┤
│  libaccessory_client.so  │  Android Binder IPC
│  ChannelImpl::read/write │  Shared memory transport
├──────────────────────────┤
│  accessory_server        │  Main daemon (4.8MB)
│  AAPReceiverLibrary/     │  Google GAL Protocol
│  GALProtocol/protos.pb   │  Protobuf messages
│  OpenSSL 1.0.2h          │  TLS 1.2
├──────────────────────────┤
│  USB / Bluetooth         │  AOAP + RFCOMM
└──────────────────────────┘
```

### Phone Architecture (Java, from APK)

```
┌──────────────────────────┐
│  Android Auto UI          │  Activities, Views, Fragments
│  com.google.android       │
│  .projection.gearhead     │
├──────────────────────────┤
│  GMS Car Service API      │  CarGalMessage, CarSensorEvent
│  ICarVideoFocusListener   │  boolean focus (simplified)
│  ICarWindowManager        │  Display management
├──────────────────────────┤
│  GAL Protocol Handler     │
│  qmy (control channel)    │  Message dispatch
│  qnx (audio endpoints)    │  Audio stream management
│  qor (sensor endpoint)    │  Sensor data processing
│  qly (BT endpoint)        │  Pairing flow
├──────────────────────────┤
│  zyt protobuf messages    │  GeneratedMessage subclasses
│  TLS 1.2 (phone=server)  │  Mutual auth, Google Automotive Link CA
├──────────────────────────┤
│  USB AOAP / WiFi TCP      │  Transport layer
└──────────────────────────┘
```

### Protocol Version (confirmed both sides)

- Phone supports: v1.6 (minimum) → v1.7 (preferred), fallback cap v6.0
- Sony firmware: Newer than tamul v1.4.1 (Alpine), likely v1.5 or v1.6 era
- aasdk: v1.6 era

---

## Implementation Priority for OpenAuto Prodigy

Based on the cross-referenced data, prioritized by impact:

### P0 — Fix What's Broken

| Item | Current State | Fix | Source |
|------|-------------|-----|--------|
| ByeBye reasons | Send only QUIT(1) | Send POWER_DOWN(7) on shutdown, USER_SELECTION(1) on disconnect | APK enum |
| Video focus | Binary on/off | Implement all 4 states + reason codes | Both |
| BT pairing UNK values | 1=UNK, 3=UNK | 1=OOB, 3=PASSKEY_ENTRY | APK enum |

### P1 — Easy Wins

| Item | Effort | Benefit | Source |
|------|--------|---------|--------|
| H.265 video | Medium | Better quality, less bandwidth (Pi 4 has HW decode) | APK codec enum |
| Night mode sensor | Trivial | AA switches to dark mode automatically | Sony config |
| Parking brake sensor | Trivial | Unlock full AA features while parked | Sony config |
| Proper status codes | Low | Better error messages for debugging | APK status enum |

### P2 — New Services

| Service | Protobuf Names (Sony) | Value |
|---------|----------------------|-------|
| Navigation status (10) | NavigationNextTurnEvent, NavigationNextTurnDistanceEvent | Turn-by-turn in native UI |
| Media playback (11) | MediaPlaybackStatus, MediaPlaybackMetadata | Now-playing in launcher |
| Phone status (13) | PhoneStatus, PhoneStatus_Call | Incoming call display |
| Notifications (14) | GenericNotificationMessage | Phone notifications on HU |

### P3 — Advanced

| Item | Notes |
|------|-------|
| Media browser (12) | Full media tree navigation in native UI |
| Radio integration (15) | Control HU tuner from AA (requires tuner hardware) |
| Instrument cluster display (DISPLAY_TYPE_CLUSTER) | Secondary display output |
| Integrated overlays (0x800D-0x8010) | Google Maps overlay rendering |
| VP9/AV1 video | Requires newer SBC decode (not Pi 4) |

---

## File Locations

### Sony Firmware
- Extracted filesystem: `sony-fw/xavax100/spsdk_fs/` and `spapp_fs/`
- AA library (with debug symbols): `spsdk_fs/lib/libspandroidauto.so` (242KB, **not stripped**)
- AA plugin: `spapp_fs/siba/plugins/libandroidauto.so` (358KB)
- Main daemon: `spsdk_fs/bin/accessory_server` (4.8MB, stripped)
- AA config (production): `spapp_fs/siba/etc/androidauto_config.xml`
- AA config (SDK template): `spsdk_fs/etc/accessory/androidauto_config.xml`
- Static library: `spsdk_fs/lib/static/libspandroidauto.a` (346KB)

### Android Auto APK
- Decompiled source: `firmware/android-auto-apk/decompiled/sources/`
- Control channel: `defpackage/qmy.java`
- Base handler: `defpackage/qoh.java`
- Service types: `defpackage/qlf.java`
- All enums: `defpackage/v*.java`, `defpackage/w*.java`, `defpackage/x*.java`

### Analysis Documents
- Sony firmware analysis: `openauto-pro-community/docs/sony-xav-firmware-analysis.md`
- APK decompilation: `openauto-pro-community/docs/android-auto-apk-decompilation.md`
- This cross-reference: `openauto-pro-community/docs/android-auto-protocol-cross-reference.md`
- Protocol quick reference (skill): `reference/android-auto-protocol.md`
