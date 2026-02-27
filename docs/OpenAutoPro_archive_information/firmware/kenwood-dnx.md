# Kenwood DNX / DDX Series Firmware Analysis

> **Source:** Kenwood firmware S_V1_5_0009_8000 (SOC + Boot) from `S_V1_5_0009_8000.zip`
> **Date:** October 2025 (firmware build: September 2025)
> **Platform:** Telechips TCC8971 (quad-core Cortex-A7), codename "azalea"
> **OS:** Automotive Linux (Yocto-based), kernel from `zImage.dtb`
> **Qt:** 5.3, **GStreamer:** 1.4.x, **ALSA** audio
> **AAP SDK:** Google `libreceiver-lib.so` (5.3MB, newer than Alpine's embedded version)
> **SSL:** OpenSSL (full, not MatrixSSL)
> **Protobuf:** Google protobuf (full runtime, not lite)
> **Rootfs build:** 2025-09-09, SVN revision 1885M

This document contains protocol constants, configuration structures, and architectural details extracted from string analysis of the Kenwood head unit firmware. Unlike the Alpine iLX-W650BT (bare-metal RTOS), the Kenwood runs a full Linux userspace with separate shared libraries for each subsystem.

**No proprietary code was decompiled or disassembled.** All information was extracted via `strings`, `binwalk`, and `dtc` analysis of publicly available firmware update files.

---

## Firmware Structure

The `.nfu` (NFU = NAVI Firmware Update) archive contains:

| File | Size | Description |
|------|------|-------------|
| `19common_fsbios_bootimg.img` | 12MB | BIOS boot image |
| `approm_20dnx_ab_vga_hdr.ext4` | 490MB | Application ROM (ext4) — all JK apps and AA libraries |
| `automotive-linux-image-20dnx.ext4` | 330MB | Root filesystem (ext4) — Linux OS, Google libreceiver-lib |
| `qboot_dnx_8000_vga_nonsec.bin` | 72MB | Quick boot binary |
| `recovery.img` | 13MB | Android boot image (recovery) |
| `fbios_dual.bin` | 512KB | FBIOS |
| `sbios_dual.bin` | 5MB | SBIOS |
| `zImage.dtb` | 73KB | Flattened device tree |
| `Boot_1.5.0009.8000.nfu` | 4.6MB | ARM boot loader |

The SOC header identifies the platform as `BOOTLOADER/JKCE/NAVI17/LD#####`, built 2025/09/09.

---

## Platform Architecture

### Hardware

- **SoC:** Telechips TCC8971 (`telechips,tcc8971` / `telechips,tcc897x`)
- **CPU:** 4× ARM Cortex-A7
- **GPU:** EGL 1.4 (software/integrated)
- **Device tree model:** `JVCKenwood azalea`

### Software Stack

- **OS:** Automotive Linux (Yocto), root at `automotive-linux-image-20dnx.ext4`
- **Application ROM:** Mounted from `approm_20dnx_ab_vga_hdr.ext4`
- **Qt 5.3** — QML 2.3 / QtQuick.Controls 1.2
- **GStreamer 1.4** — audio/video pipelines
- **ALSA** — audio HAL
- **libconfig** — configuration files (`.cfg`)
- **RealVNC** — screen sharing / remote display
- **Garmin** — navigation app (`/Garmin/`)
- **mDNS** — service discovery daemon

### Key Binaries

| Binary | Role |
|--------|------|
| `Infotainment` | Main application (2.4MB) |
| `System` | System service manager |
| `GuiApp` | GUI application (QML-based) |
| `NaviApp` | Navigation app interface |
| `JKAudioService` | Audio routing service |
| `JKBtService` | Bluetooth manager |
| `JKWiFiService` | WiFi manager |
| `JKRadioService` | FM/AM/HD Radio |
| `JKDrService` | Dead reckoning service |
| `JKGenService` | General service daemon |
| `JKMountService` | USB/storage mount service |
| `JKAbsAppMngServ` | Application manager |
| `JKMiracastServ` | Miracast/screen mirroring |
| `bsa_server` | Broadcom BSA Bluetooth stack |
| `gpsd` | GPS daemon |
| `mdnsd` | mDNS daemon |
| `RealVNC` | VNC server |

---

## Android Auto Libraries

The AA implementation is split across multiple shared libraries:

| Library | Size | Role |
|---------|------|------|
| `libreceiver-lib.so` (rootfs) | 5.3MB | Google's official AAP receiver SDK |
| `libJKAndroidAuto.so` | 702KB | JK AA proxy — state machines, sensors, transport |
| `libJKAndroidAuto_2_2_1.so` | 726KB | Newer version (HUIG 2.2.1 support) |
| `libInfoFuncAap.so` | 245KB | Application-level AA controller — IPC, GPS, UI |
| `libInfoFuncAap_2_2_1.so` | 242KB | Newer version |
| `libJKBtAap.so` | 73KB | Bluetooth SPP bridge for AA |
| `libMCS_JKCAOA.so` | 68KB | USB AOAP transport layer (libusb) |
| `libvncaaphidsdk.so` | 1.4MB | RealVNC-based USB HID SDK for touch/key input |

### Version Coexistence

The firmware ships **two versions** of key libraries (`_2_2_1` suffix = HUIG v2.2.1 compliance). The 2.2.1 versions add:
- `reportRpmData()` — engine RPM sensor
- `reportDrivingStatusData()` — driving status reporting
- Separate `AAPConnectionManager` class (vs `AAPConnectionReceiver` in base)

---

## Google libreceiver-lib.so — Protocol Reference

### Source File Layout

```
build/gen/protos.pb.{h,cc}          # Generated protobuf code
../linux/sink/build/gen/protos.pb.h  # Linux sink variant
src/
├── AudioSource.cpp                  # Microphone input → phone
├── BluetoothEndpoint.cpp            # BT pairing management
├── Controller.cpp                   # Session lifecycle / message router
├── GalVerificationVendorExtensionCallbackRouter.cpp  # GAL test hooks
├── InputSource.cpp                  # Touch/key input → phone
├── MediaBrowserEndpoint.cpp         # Media library browsing
├── MediaPlaybackStatusEndpoint.cpp  # Now-playing metadata
├── MediaSinkBase.cpp                # Audio/video output base
├── MessageRouter.cpp                # Channel-based message routing
├── NavigationStatusEndpoint.cpp     # Turn-by-turn navigation
├── NotificationEndpoint.cpp         # Phone notifications (NEW)
├── PhoneStatusEndpoint.cpp          # Phone call status
├── RadioEndpoint.cpp                # Radio tuner control (NEW)
├── SensorSource.cpp                 # Vehicle sensor data → phone
└── SslWrapper.cpp                   # TLS encrypt/decrypt
```

**New vs Alpine:** `NotificationEndpoint.cpp`, `RadioEndpoint.cpp`, and `GalVerificationVendorExtensionCallbackRouter.cpp` are not present in the Alpine SDK.

### Callback Interfaces

```cpp
IControllerCallbacks
IAudioSinkCallbacks
IAudioSourceCallbacks
IVideoSinkCallbacks
IInputSourceCallbacks
IPhoneStatusCallbacks
IMediaBrowserCallbacks
IMediaPlaybackStatusCallbacks
INotificationEndpointCallbacks      // NEW
INavigationStatusCallbacks
IVendorExtensionCallbacks
IGalVerificationVendorExtensionCallbacks  // NEW
IRadioCallbacks                     // NEW
IBluetoothCallbacks
```

### Core Internal Classes

```
Controller           — central session manager, message dispatcher
GalReceiver          — top-level AAP receiver entry point
SslWrapper           — TLS layer (OpenSSL-based, unlike Alpine's MatrixSSL)
AudioSource          — microphone/audio input endpoint
InputSource          — touch/key/rotary input sender
MediaSinkBase        — audio/video output base class
MessageRouter        — routes messages to endpoints by channel ID
VideoSink            — video decoder/renderer
Frame, IoBuffer, Channel, WorkItem — internal transport primitives
```

### Package Identifiers

```
com.google.android.gms.apitest.car                             # CTS test app
com.google.android.projection.gearhead.pctsverifier             # PCTS verifier
com.google.android.projection.localsink                         # Local sink (dev)
com.google.android.projection.protocol.GalVerificationVendorExtension  # GAL test extension
```

---

## Protobuf Message Types

### Channel/Session Control

| Message | Direction | Description |
|---------|-----------|-------------|
| `ServiceDiscoveryRequest` | Phone → HU | Capabilities query |
| `ServiceDiscoveryResponse` | HU → Phone | Capabilities response |
| `ChannelOpenRequest` | Phone → HU | Open a service channel |
| `ChannelOpenResponse` | HU → Phone | Channel open result |
| `ChannelCloseNotification` | Either | Close a channel |
| `PingRequest` / `PingResponse` | Either | Keep-alive |
| `ByeByeRequest` / `ByeByeResponse` | Either | Graceful disconnect |
| `AuthResponse` | — | Authentication result |
| `Setup` | — | Session setup |
| `Start` | — | Start session (fields: `session_id`, `configuration_index`) |
| `Config` | — | Configuration (fields: `status`, `configuration_indices`) |
| `Ack` | — | Acknowledgment |

### Audio

| Message | Description |
|---------|-------------|
| `AudioFocusNotification` | HU → Phone: audio focus state (field: `focus_state`) |
| `AudioFocusRequestNotification` | Phone → HU: request audio focus |
| `AudioConfiguration` | Audio stream configuration |
| `MicrophoneRequest` / `MicrophoneResponse` | Microphone open/close (fields: `status`, `session_id`) |

### Video

| Message | Description |
|---------|-------------|
| `VideoConfiguration` | Video stream config (fields: `frame_rate`, `codec_resolution`) |
| `VideoFocusNotification` | HU → Phone: video focus state (field: `focus`) |
| `VideoFocusRequestNotification` | Phone → HU: request video focus (field: `disp_channel_id`) |

### Input

| Message | Description |
|---------|-------------|
| `InputReport` | HU → Phone: input events (fields: `timestamp`, `touch_event`, `touchpad_event`, `disp_channel_id`, `key_event`, `absolute_event`, `relative_event`) |
| `InputFeedback` | Phone → HU: haptic/visual feedback |
| `KeyEvent` | Keyboard event |
| `KeyEvent.Key` | Individual key descriptor |
| `KeyBindingRequest` / `KeyBindingResponse` | Key binding negotiation |
| `TouchEvent` | Touch event container (fields: `action`, `action_index`, `pointer_data`) |
| `TouchEvent.Pointer` | Individual touch pointer |
| `AbsoluteEvent` | Absolute position input (touchpad/knob) |
| `RelativeEvent` | Relative movement input (trackball) |

### Navigation

| Message | Description |
|---------|-------------|
| `NavFocusNotification` / `NavFocusRequestNotification` | Navigation focus (field: `focus_type`) |
| `NavigationStatus` | Navigation state (field: `status`) |
| `NavigationStatusStart` / `NavigationStatusStop` | Start/stop nav updates |
| `NavigationNextTurnEvent` | Turn instruction (next maneuver) |
| `NavigationNextTurnDistanceEvent` | Distance to next turn (fields: `display_distance_e3`, `display_distance_unit`) |

### Media

| Message | Description |
|---------|-------------|
| `MediaPlaybackStatus` | Playback state |
| `MediaPlaybackMetadata` | Track metadata (title, artist, album, art) |
| `MediaBrowserInput` | Media browsing commands |
| `MediaGetNode` | Request media tree node |
| `MediaRootNode` | Root of media tree |
| `MediaList` / `MediaListNode` | List of media items |
| `MediaSong` / `MediaSongNode` | Individual song/track |
| `MediaSource` / `MediaSourceNode` | Audio source (fields: `name`, `path`, `album_art`) |

### Phone

| Message | Description |
|---------|-------------|
| `PhoneStatus` | Call state + signal (fields: `calls`, `signal_strength`) |
| `PhoneStatus.Call` | Individual call info |
| `PhoneStatusInput` | Phone UI commands (accept/reject/etc.) |

### Sensors

| Message | Description |
|---------|-------------|
| `SensorBatch` | Batch sensor data (field: `driving_status_data`) |
| `SensorRequest` / `SensorResponse` | Sensor capability query |
| `SensorError` | Sensor error (fields: `sensor_type`, `sensor_error_type`) |
| `LocationData` | GPS location (field: `timestamp`) |
| `GpsSatelliteData` / `GpsSatellite` | Satellite info |
| `SpeedData` | Vehicle speed |
| `RpmData` | Engine RPM |
| `OdometerData` | Odometer |
| `FuelData` | Fuel level/range |
| `GearData` | Transmission gear |
| `DoorData` | Door open/closed |
| `HvacData` | Climate control |
| `LightData` | Headlights / turn signals |
| `NightModeData` | Day/night mode |
| `ParkingBrakeData` | Parking brake |
| `PassengerData` | Passenger detection |
| `DrivingStatusData` | Driving status flags |
| `CompassData` | Compass bearing (fields: `bearing_e6`, `pitch_e6`, `roll_e6`) |
| `AccelerometerData` | Accelerometer |
| `GyroscopeData` | Gyroscope |
| `DeadReckoningData` | Dead reckoning position |
| `TirePressureData` | Tire pressure |
| `EnvironmentData` | Temperature, etc. |

### Bluetooth

| Message | Description |
|---------|-------------|
| `BluetoothPairingRequest` / `BluetoothPairingResponse` | BT pairing flow |
| `BluetoothAuthenticationData` | BT auth data |
| `BluetoothService` | BT service descriptor in ServiceDiscovery |

### Radio (NEW — not in Alpine)

| Message | Description |
|---------|-------------|
| `RadioStateNotification` | Current tuner state |
| `RadioStationInfo` / `RadioStationInfoNotification` | Station details |
| `RadioStationMetaData` | Station metadata (RDS/RBDS) |
| `RadioSourceRequest` / `RadioSourceResponse` | Select radio source |
| `ActiveRadioNotification` | Active radio changed |
| `ScanStationsRequest` / `ScanStationsResponse` | Station scan |
| `SeekStationRequest` / `SeekStationResponse` | Seek up/down |
| `StepChannelRequest` / `StepChannelResponse` | Step frequency |
| `TuneToStationRequest` / `TuneToStationResponse` | Direct tune |
| `MuteRadioRequest` / `MuteRadioResponse` | Mute radio |
| `CancelRadioOperationsRequest` / `CancelRadioOperationsResponse` | Cancel pending ops |
| `ConfigureChannelSpacingRequest` / `ConfigureChannelSpacingResponse` | Set frequency step |
| `GetProgramListRequest` / `GetProgramListResponse` | Program list |
| `GetTrafficUpdateRequest` / `GetTrafficUpdateResponse` | Traffic info |
| `StationPreset` / `StationPresetList` / `StationPresetsNotification` | Preset management |
| `HdRadioArtistExperience` / `HdRadioComment` / `HdRadioCommercial` | HD Radio data |
| `HdRadioPsdData` / `HdRadioSisData` / `HdRadioStationInfo` | HD Radio info |
| `RdsData` | RDS/RBDS data |
| `TrafficIncident` | Traffic incident details |
| `RadioProperties` | Radio capabilities |

### Notifications (NEW — not in Alpine)

| Message | Description |
|---------|-------------|
| `GenericNotificationMessage` | Phone notification content |
| `GenericNotificationAck` | Notification acknowledgment |
| `GenericNotificationSubscribe` / `GenericNotificationUnsubscribe` | Subscribe to notifications |

### Voice

| Message | Description |
|---------|-------------|
| `VoiceSessionNotification` | Voice session state (field: `status`) |

### Instrument Cluster (NEW — not in Alpine)

| Message | Description |
|---------|-------------|
| `InstrumentClusterInput` | Instrument cluster button press |

### GAL Verification (Test/Certification)

| Message | Description |
|---------|-------------|
| `GalVerificationAudioFocus` | Test audio focus |
| `GalVerificationVideoFocus` | Test video focus |
| `GalVerificationBugReportRequest/Response` | Bug report capture |
| `GalVerificationDisplayInformationRequest/Response` | Display info query |
| `GalVerificationInjectInput` | Inject test input events |
| `GalVerificationMediaSinkStatus` | Media sink health check |
| `GalVerificationScreenCaptureRequest/Response` | Screen capture |
| `GalVerificationSetSensor` | Override sensor data |

### Diagnostics

| Message | Description |
|---------|-------------|
| `GoogleDiagnosticsBugReportRequest/Response` | Google diagnostics |
| `DiagnosticsData` | Diagnostic telemetry |

---

## Enum Types (Complete List)

All enum types confirmed via `CHECK failed: X_IsValid(value)` assertions in `libreceiver-lib.so`:

| Enum | Description |
|------|-------------|
| `AudioFocusRequestType` | Audio focus request kind |
| `AudioFocusStateType` | Audio focus grant state |
| `AudioStreamType` | Audio stream identifier |
| `BluetoothPairingMethod` | BT pairing method |
| `ByeByeReason` | Disconnect reason |
| `Config_Status` | Configuration status |
| `DriverPosition` | LEFT / RIGHT / CENTER |
| `FeedbackEvent` | Input feedback type |
| `Gear` | Transmission gear position |
| `HdAcquisionState` | HD Radio signal acquisition |
| `HeadLightState` | Headlight on/off/auto |
| `InstrumentClusterInput_InstrumentClusterAction` | Cluster button action |
| `ItuRegion` | ITU radio frequency region |
| `MediaCodecType` | Audio/video codec (H264, AAC, etc.) |
| `MediaList_Type` | Media browser list type |
| `MediaPlaybackStatus_State` | Playing/Paused/Stopped/etc. |
| `MessageStatus` | Protocol message status |
| `NavFocusType` | Navigation focus type |
| `NavigationNextTurnDistanceEvent_DistanceUnits` | Meters/km/miles/yards/feet |
| `NavigationNextTurnEvent_NextTurnEnum` | Turn type (left/right/U-turn/merge/etc.) |
| `NavigationNextTurnEvent_TurnSide` | Turn side (left/right) |
| `NavigationStatus_NavigationStatusEnum` | Active/inactive navigation |
| `NavigationStatusService_InstrumentClusterType` | Cluster display type |
| `PhoneStatus_State` | Call state (ringing/active/held) |
| `PointerAction` | Touch pointer action (DOWN/UP/MOVE) |
| `RadioType` | AM/FM/HD/DAB/SiriusXM |
| `RdsType` | RDS data type |
| `SensorErrorType` | Sensor error classification |
| `SensorType` | Sensor identifier |
| `TouchScreenType` | Touch screen type descriptor |
| `TrafficServiceType` | Traffic data provider |
| `TurnIndicatorState` | Blinker left/right/hazard |
| `VideoCodecResolutionType` | 480p/720p/1080p |
| `VideoFocusMode` | Video focus mode |
| `VideoFocusReason` | Why video focus changed |
| `VideoFrameRateType` | 30fps/60fps |
| `VoiceSessionStatus` | Voice session state |

### Enums NOT in Alpine (new in this SDK version)

- `FeedbackEvent`, `Gear`, `HdAcquisionState`, `HeadLightState`
- `InstrumentClusterInput_InstrumentClusterAction`
- `ItuRegion`, `MediaCodecType`, `MediaList_Type`
- `NavigationNextTurnDistanceEvent_DistanceUnits`
- `NavigationNextTurnEvent_NextTurnEnum`, `NavigationNextTurnEvent_TurnSide`
- `NavigationStatusService_InstrumentClusterType`
- `RadioType`, `RdsType`, `TouchScreenType`
- `TrafficServiceType`, `TurnIndicatorState`
- `VideoFocusReason`, `VoiceSessionStatus`

---

## ServiceDiscovery Configuration

### From `AndroidAutoConfig.cfg` (libconfig format)

The config files are in libconfig format (not XML like the Alpine SDK). Each model variant has its own config with display-specific parameters.

#### Head Unit Info (Default Config)

```
head_unit_info = {
    // SSL credentials (Google CA + JVC Kenwood device cert + private key)
    creds_root     = "<Google Automotive Link CA, expires 2044-06-05>"
    creds_client   = "<JVC Kenwood device cert, issuer: Google Automotive Link>"
    creds_private  = "<RSA 2048-bit private key>"

    // Service Discovery identity
    make                        = "KENWOOD HOME"      // displayed as "Return to KENWOOD HOME"
    model                       = "After Market"       // required for aftermarket units
    year                        = "2018"
    vehicle_id                  = ""                   // fallback: SystemInformation serial
    driver_position             = 1                    // 0=LEFT, 1=RIGHT, 2=CENTER
    head_unit_make              = "JVC KENWOOD"
    head_unit_model             = ""                   // fallback: model name from SystemInfo
    head_unit_software_build    = "multi"              // required when N/A
    head_unit_software_version  = ""                   // fallback: version from VersionInfo

    // Display parameters
    pixel_aspect_ratio          = 11500                // actual_ratio × 10^4
    screen_density              = 170                  // ppi
    screen_dp_width             = 753                  // dp
    screen_dp_height            = 452                  // dp
    screen_viewingdistance      = 950                  // mm
    vertical_physical_size      = 79680                // physical screen height (µm?)
    screen_density_480p         = 150                  // density override for 480p mode

    // Microphone
    aap_mic_volume_adjust_index = 0                    // 0=none, 1-53 = -20dB to +32dB

    // Hardware
    usb_port_restriction        = 0                    // 0=all ports, 1=USB1 only
    isGyroSensorXYZSupported    = 0                    // 0=Z only, 1=XYZ

    // Supported keycodes
    isKeycodeMediaSupport           = 1
    isKeycodeTelSupport             = 1
    isKeycodeCallSupport            = 0
    isKeycodeEndcallSupport         = 0
    isKeycodeNavigationSupport      = 1
    isKeycodeHomeSupport            = 0
    isKeycodeSearchSupport          = 1
    isKeycodeBackSupport            = 0
    isKeycodeMediaPlayPauseSupport  = 1
    isKeycodeMediaPlaySupport       = 1
    isKeycodeMediaPauseSupport      = 1
    isKeycodeMediaPreviousSupport   = 1
    isKeycodeMediaNextSupport       = 1
}
```

#### Model-Specific Display Variants

| Field | Default | KWD_6_8_HD | KWD_7_WVGA | JVC_6_8_WVGA |
|-------|---------|------------|------------|--------------|
| `make` | KENWOOD HOME | KENWOOD HOME | KENWOOD HOME | JVC HOME |
| `year` | 2018 | 2020 | 2020 | 2020 |
| `driver_position` | 1 (RIGHT) | 0 (LEFT) | 0 (LEFT) | 0 (LEFT) |
| `head_unit_make` | JVC KENWOOD | JVCKENWOOD Corporation | JVCKENWOOD Corporation | JVCKENWOOD Corporation |
| `screen_density` | 170 | **256** | 170 | 170 |
| `screen_dp_width` | 753 | **800** | 753 | 753 |
| `screen_dp_height` | 452 | **450** | 452 | 452 |
| SSL cert serial | 0x86 / OU=02 | 0xCE / OU=03 | 0xCE / OU=03 | 0xCE / OU=03 |

The Default config is Japanese market (RIGHT driver position). The `KWD_6_8_HD` variant is clearly the high-resolution 800×450dp at 256 DPI model.

---

## JK AA Proxy Architecture

### Connection State Machine (`libJKAndroidAuto.so`)

```
┌─────────────────┐
│   Disconnected   │
└────────┬────────┘
         │ recvRequestStartWorkingAAP()
         ▼
┌─────────────────────────────┐
│  ConnectingWaitRequestStart  │
└────────┬────────────────────┘
         │ recvRequestStart()
    ┌────┴────┐
    ▼         ▼
┌────────┐  ┌──────────────────────┐
│  USB   │  │  Wireless (WAAP)     │
│  AOAP  │  │                      │
├────────┤  ├──────────────────────┤
│Spp     │  │WifiApRequested       │
│Requested│ │  → WifiSocketRequested│
│  → Aap │  │                      │
│Requested│ │                      │
└───┬────┘  └──────────┬───────────┘
    │                  │
    └───────┬──────────┘
            ▼
    ┌───────────────┐
    │   Connected    │
    └───────────────┘
```

**State classes:**
- `AAPConnectionStateDisconnected`
- `AAPConnectionStateConnectingWaitRequestStart`
- `AAPConnectionStateConnectingSppRequested` — SPP Bluetooth
- `AAPConnectionStateConnectingAapRequested` — USB AOAP
- `AAPConnectionStateConnectingWifiApRequested` — WiFi AP mode
- `AAPConnectionStateConnectingWifiSocketRequested` — WiFi TCP socket
- `AAPConnectionStateConnected`

### Negotiation State Machine

```
Unconfirmed → VersionRequest → ConfirmStart → ConfirmedSuccess
                                            → ConfirmedFail
```

### Wireless Connection Flow

The `WAAPConnectionManager` class handles wireless AA:

1. `setwifiSSID()` — configure AP SSID
2. `generatePassword()` — generate WiFi password
3. `WifiDiscoveryProvider` sends `JksWifiVersionRequest` / `JksWifiVersionResponse` / `JksWifiInfoResponse`
4. `WifiSocketCreator` creates TCP server socket on phone-facing WiFi
5. `WifiTransport` handles data transfer

### AOAP (USB) Transport (`libMCS_JKCAOA.so`)

The AOAP layer uses libusb directly:

1. `CAOAListener_Linux::StartListener()` — scan USB for phones
2. Detect Google VID (0x18D1), check for PIDs 0x2D00/0x2D01 (already accessory)
3. If not accessory: `SetupAccessory()` with manufacturer/model/description/version/URI/serial
4. `CAOALayer_Linux` — bulk transfer IN/OUT endpoints for data

### VNC AAPHID SDK (`libvncaaphidsdk.so`)

The RealVNC AAPHID SDK provides an alternative input path via USB HID descriptors:

- Presents virtual HID devices to the phone (multi-touch digitizer, keyboard)
- Sends raw USB HID reports instead of AAP protobuf `InputReport` messages
- Used for wired AA where USB HID is faster than protobuf

**Key API:**
```cpp
VNCAAPHIDSDKInitialize()                    // Entry point (AAPHID_SDK_1_0)
Connection::setRotation(uint8)              // Screen rotation
Connection::setCurrentSize(uint16, uint16)  // Display dimensions
Connection::createTouchHID()                // Register touch digitizer
Connection::createKeyHID()                  // Register keyboard
Connection::deviceSendTouchEvent(vector<VNCTouchDescriptor>)
Connection::deviceSendMappedKeyEvent(uint8, int)
Connection::deviceSendDeviceKeyEvent(uint32, uint8)
```

---

## Sensor Data Integration

### GPS Processing (`libInfoFuncAap.so`)

The controller parses NMEA sentences directly:

| NMEA Sentence | Data |
|---------------|------|
| `GPGGA` | Fix quality, position, altitude |
| `GPGSA` | Satellite selection, DOP |
| `GPGSV` | Satellites in view, azimuth, elevation, SNR |
| `GPRMC` | Recommended minimum — position, speed, course |
| `PUBX` | u-blox proprietary extensions |

These are converted to `MBA_SYNC_GPS_INFO` and then to AAP `LocationData` / `GpsSatelliteData` protobuf messages.

### Vehicle Sensor Reporting (`SensorInfoGenerator`)

```cpp
// Location
reportLocationData(timestamp, latitude, has_lat, longitude, has_lon,
                   altitude, has_alt, speed, has_speed, bearing)

// Vehicle dynamics
reportSpeedData(speed, has_speed, is_raw, is_km, accuracy)
reportRpmData(rpm)                                    // v2.2.1 only
reportCompassData(bearing)
reportCompassData3D(has_bearing, bearing, has_pitch, pitch, has_roll, roll)
reportGyroscopeData(has_x, x, has_y, y, has_z, z)
reportAccelerometerData(...)

// Vehicle state
reportGearData(gear)
reportNightModeData(is_night)
reportParkingBrakeData(engaged)
reportDrivingStatusData(status)                       // v2.2.1 only
reportFuelData(has_level, level, has_range, range, is_ev, has_battery)
reportDoorData(hood, trunk, driver, passenger, name_str, has_pos, position)
reportHvacData(has_target, target_temp, has_current, current_temp)
reportLightData(has_headlight, headlight_state, has_turn, turn_state,
                has_hazard, hazard_on)
reportOdometerData(odometer, has_trip, trip)
reportTirePressureData(...)
```

### Conversion Functions

```cpp
convert_locationInfo_latitude(sign, degrees, minutes, seconds, fraction)
convert_locationInfo_longitude(sign, degrees, minutes, seconds, fraction)
convert_locationInfo_altitude(raw_altitude)
convert_locationInfo_speed(raw_speed)
convert_locationInfo_bearing(raw_bearing)
convert_sensorInfo_speed(speed_double)
convert_sensorInfo_NightMode(mode_byte)
convert_sensorInfo_GearData(is_reverse, is_park)
convert_sensorInfo_RotationSpeed(axis, rate_dps)
convert_sensorInfo_Acceleration_y(accel_g)
convert_sensorInfo_CompassData_bearing(degrees)
convert_sensorInfo_CompassData_pitch(radians)
```

---

## Audio Architecture

### GStreamer Pipeline (`libJKAndroidAuto.so`)

Audio decoding uses GStreamer with these elements:

```
appsrc → AAC decoder → appsink → ALSA output
```

**Caps string:** `audio/mpeg,mpegversion=4,stream-format=%s,rate=%5d,channels=%1d`

**Audio stream classes:**
- `AapMediaStream` — main music audio (with volume control)
- `AapGuidanceStream` — navigation guidance (PCM player)
- `AapAudioInputStream` — microphone capture (ALSA input → phone)
- `AapAudioOutputStream` — phone → speaker (GStreamer AAC decode + ALSA)

### Audio Focus Management

`AAPFocusManager` coordinates:
- `onRequestAudioFocus` — phone requests audio focus
- `onRequestVideoFocus` — phone requests video focus
- `onRequestNavigationFocus` — phone requests nav focus
- `setMixVolume(int, int, int, uint8)` — ducking/mixing levels
- `setIntAtt(bool)` — interrupt attenuation

---

## Bluetooth Integration (`libJKBtAap.so`)

### IPC Messages

**Requests (App → BT manager):**
- `GetCallStatus` — is there an active call?
- `GetAlreadyPaired` — is this device already paired?
- `GetConnectStatus` — SPP connection state
- `GetBluetoothStatus` — BT hardware on/off
- `GetSupportParingMethods` — supported pairing methods
- `GetBluetoothDeviceAddress` — local BT MAC
- `StartAap` — initiate SPP connection for AA
- `EndAap` — terminate SPP connection

**Notifications (BT manager → App):**
- `CallStatus` — call state changed
- `ActiveState` — BT active state
- `AlreadyPaired` — paired status result
- `ConnectStatus` — SPP connection result
- `PairingPasskey` — passkey for display
- `BluetoothStatus` — hardware state
- `SupportParingMethods` — method list
- `BluetoothDeviceAddress` — MAC address

---

## QML UI (AA-Related Screens)

### Layout Dimensions

- **Full-screen AA:** 800×480 pixels
- **Split-screen AA:** 400×358 at x=400, y=61 (right half, minus header/footer)
- **Dialog overlays:** centered, various sizes

### Key Screens

| QML File | Purpose |
|----------|---------|
| `EntAapCpSourceLayout.qml` | Full-screen AA video surface |
| `EntAapCpSplitSourceLayout.qml` | Split-screen AA right panel |
| `EntAapCpSplitSurfaceLayout.qml` | Split-screen video surface (400×358) |
| `EntAapCpSplitSubSurfaceLayout.qml` | Split-screen media controls |
| `EntAapCpSplitAudioMediaLayout.qml` | Split-screen audio metadata |
| `EntAapCpDummyScreenLayout.qml` | Black placeholder during transitions |
| `EntAapCpTempLayout.qml` | Temporary transition layout |
| `EntAapCpAapConnectingDialogLayout.qml` | "Connecting..." progress dialog |
| `EntAapCpAapConnectErrorToastLayout.qml` | Error toast (maps TID_4849–4855) |
| `EntAapCpAapConnectGuideDialogLayout.qml` | Connect instructions |
| `EntAapCpAapConnectRequestToastLayout.qml` | "Please connect" prompt |
| `EntAapCpAapBatteryStatusToastLayout.qml` | Phone battery warning |
| `EntAapCpAapDeviceListDialogLayout.qml` | Wireless device picker |
| `EntAapCpAttentionDialogLayout.qml` | Attention/warning dialog |
| `EntAapCpNaviGuidanceDialogLayout.qml` | Native navi guidance confirmation |
| `EntAapCpConnectionNoticeLayout.qml` | AA vs CarPlay selection |
| `EntAapCpWirelessCpConnectConfirmDialogLayout.qml` | Wireless CP confirm |
| `EntAapCpWirelessCpLocationSensorErrorDialogLayout.qml` | GPS/sensor error |
| `EntAapCpOobBtPairingErrorToastLayout.qml` | OOB BT pairing errors |
| `EntAapCp2ndCpConnectionNoticeDialogLayout.qml` | Second-screen CP notice |

---

## Comparison: Kenwood vs Alpine

| Aspect | Alpine iLX-W650BT | Kenwood DNX/DDX |
|--------|-------------------|-----------------|
| **Platform** | Toshiba TMM9200, bare-metal RTOS | Telechips TCC8971, Linux |
| **SDK Build** | `tamul_aap_1.4.1` (version labeled) | Newer (unlabeled, more features) |
| **SSL** | MatrixSSL 3.9.5 | OpenSSL (full) |
| **Protobuf** | Lite runtime | Full runtime (descriptor support) |
| **Config format** | XML attribute paths | libconfig (`.cfg` files) |
| **Radio endpoint** | Not present | Full radio tuner integration |
| **Notification endpoint** | Not present | Generic notification support |
| **Instrument cluster** | Not present | Cluster input/display support |
| **HD Radio** | Not present | Full HD Radio data structures |
| **GAL verification** | Basic | Extended (screen capture, bug report) |
| **Media browser** | Basic | Full media tree (nodes, songs, sources) |
| **Vehicle sensors** | Basic (location, speed, compass) | Comprehensive (fuel, doors, HVAC, tires, etc.) |
| **Audio pipeline** | Custom (MISO framework) | GStreamer 1.4 + ALSA |
| **USB input** | Direct | RealVNC AAPHID SDK (virtual HID) |
| **Split-screen** | Not observed | Full split-screen UI |

### New Protobuf Messages (Kenwood only)

Messages found in Kenwood `libreceiver-lib.so` but NOT in Alpine's firmware:

1. **Radio:** All `Radio*`, `StationPreset*`, `HdRadio*`, `RdsData`, `TrafficIncident`
2. **Notifications:** `GenericNotification*`
3. **Instrument Cluster:** `InstrumentClusterInput`
4. **Media Browser:** `MediaGetNode`, `MediaList*`, `MediaSong*`, `MediaSource*`, `MediaRootNode`
5. **Diagnostics:** `GoogleDiagnostics*`, `DiagnosticsData`
6. **Vehicle Sensors:** `TirePressureData`, `EnvironmentData`, `DeadReckoningData`, `FuelData`, `DoorData`, `HvacData`, `PassengerData`
7. **Navigation Detail:** `NavigationNextTurnEvent`, `NavigationNextTurnDistanceEvent`
8. **Voice:** `VoiceSessionNotification`

### New Enum Types (Kenwood only)

`FeedbackEvent`, `Gear`, `HdAcquisionState`, `HeadLightState`, `InstrumentClusterInput_InstrumentClusterAction`, `ItuRegion`, `MediaCodecType`, `MediaList_Type`, `NavigationNextTurnDistanceEvent_DistanceUnits`, `NavigationNextTurnEvent_NextTurnEnum`, `NavigationNextTurnEvent_TurnSide`, `NavigationStatusService_InstrumentClusterType`, `RadioType`, `RdsType`, `TouchScreenType`, `TrafficServiceType`, `TurnIndicatorState`, `VideoFocusReason`, `VoiceSessionStatus`

---

## Implications for OpenAuto Prodigy

### What we're doing right

1. **TCP transport** — Kenwood uses the same Google `libreceiver-lib.so` TCP path for wireless AA
2. **Protobuf messages** — our aasdk implementation covers all the core messages
3. **Audio focus management** — Kenwood's `AAPFocusManager` matches our approach
4. **Touch coordinate mapping** — same evdev → AA coordinate space mapping

### What we should consider adding

1. **Split-screen support** — Kenwood implements 400×358 split-screen AA alongside native content. Worth considering for our 1024×600 display.
2. **VoiceSessionNotification** — voice session state tracking we may be missing
3. **NavigationNextTurn events** — rich turn-by-turn data (turn type, distance, side) for potential instrument cluster or HUD display
4. **MediaBrowser** — full media tree browsing (not just now-playing)
5. **GenericNotification** — phone notification forwarding
6. **GearData / ParkingBrakeData** — if we ever add OBD-II support
7. **Video focus reasons** — more granular video focus management
8. **Multiple display channels** — `disp_channel_id` in `InputReport` and `VideoFocusRequestNotification` suggests multi-display support in the protocol

### What's not relevant to us

1. **AOAP/USB transport** — we're wireless-only
2. **VNC AAPHID SDK** — USB HID input path, not applicable to wireless
3. **HD Radio integration** — no radio hardware
4. **Instrument cluster** — no secondary display (yet)
5. **Most vehicle sensors** — no CAN bus / OBD-II connection (yet)
