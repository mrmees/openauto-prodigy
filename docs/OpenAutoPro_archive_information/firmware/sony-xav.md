# Sony XAV-AX100 Firmware Analysis

> **Source:** Sony XAV-AX100 firmware v1.02.09 from `XAV-AX100_v10209.zip`
> **Also examined:** XAV-9500ES v3.02.00 (.eup, encrypted RAUC), XAV-AX5500 v2.00 (.FIR, encrypted)
> **Date:** October 2018 (AX100 firmware), April 2025 (9500ES/AX5500)
> **Platform:** Sunplus SPHE8388 (Gemini), ARM Cortex-A9, Linux 3.4.5 + eCos RTOS
> **SoC:** Sunplus Technology (now Realtek subsidiary)
> **Firmware format:** `.eup` — ZIP container with Manifest + encrypted RAUC bundle (newer models), or direct ZIP with `CUST_PACK.BIN` (AX100)

**No proprietary code was decompiled or disassembled.** This analysis was performed entirely through firmware decryption using keys derived from GPL-licensed U-Boot source code published by Sony, followed by `strings` analysis and filesystem extraction of the decrypted images.

**Key finding:** The XAV-AX100 firmware uses AES-128-CBC encryption with a key embedded in the (GPL-licensed) U-Boot ISP update script. The key derivation is documented in Sony's own open-source U-Boot code. Once decrypted, the firmware reveals a complete Android Auto implementation using Google's AAP Receiver Library (GAL Protocol) with protobuf-based services, OpenSSL 1.0.2h, and Qt 5.3 on DirectFB.

---

## Firmware Generations

Sony's car audio lineup spans two distinct firmware generations:

### Older Generation (XAV-AX100, AX5000 family)
- **SoC:** Sunplus SPHE8388 ("Gemini")
- **CPU:** ARM Cortex-A9, 256MB RAM
- **OS:** Linux 3.4.5 + eCos RTOS (dual-boot)
- **Display:** DirectFB (no Wayland/X11)
- **Framework:** Qt 5.3, custom "siba" plugin architecture
- **Storage:** SPI NAND flash, SquashFS partitions
- **Firmware:** AES-128-CBC encrypted, **key derivable from GPL source**
- **AA:** Google AAP Receiver Library (GAL Protocol)

### Newer Generation (XAV-9500ES, AX8500, AX6000, AX4000)
- **Firmware format:** ZIP containing Manifest + `.raucb` (RAUC bundle)
- **Encryption:** AES-256-CBC with RSA-OAEP key wrapping
- **OS:** Linux-based (RAUC OTA framework suggests Yocto/OE)
- **Status:** Fully encrypted, key not recoverable from public sources
- **GPL source:** Available from [oss.sony.net](https://oss.sony.net/Products/Linux/Audio/category02.html) but contains only kernel/userspace, not proprietary AA code

---

## XAV-AX100 Firmware Structure

### Container Format (`CUST_PACK.BIN`)

| Offset | Size | Content |
|--------|------|---------|
| 0x000 | 0x400 | GEMINI header (magic, version, build info, embedded CUST_UPDT.BIN path) |
| 0x400 | 0x20 | ISP image header (`Gemini_ISP_image` + null padding) |
| 0x420 | 0x800 | AES-encrypted init script (U-Boot shell script) |
| 0x4000 | ~87.9MB | AES-encrypted partition images (uboot2, ecos, kernel, rootfs, spsdk, spapp, ...) |
| 0x57f6800 | 0x10400 | AES-encrypted main update script (partition layout, write commands, MD5 checksums) |

### GEMINI Header (0x000–0x0FF)

```
Offset  Size  Field                     Value
------  ----  -----                     -----
0x00    6     Magic                     "GEMINI"
0x07    1     Flags                     0x70
0x0C    4     Size hint                 0x0005 8070 (LE)
0x40    16    Version                   "1.02.09.00"
0x80    24    Build identifier          "20.1.0.2.1.1.0.0"
0xC0    16    Embedded filename         "CUST_UPDT.BIN"
```

### Partition Layout (from decrypted update script)

| Partition | Size | Content |
|-----------|------|---------|
| uboot2 | 1 MB | U-Boot second stage bootloader |
| env | 512 KB | U-Boot environment variables |
| env_redund | 512 KB | Redundant U-Boot environment |
| ecos | 8 MB | eCos RTOS image (video/audio DSP, display controller) |
| kernel | 6 MB | Linux 3.4.5 uImage (ARM, uncompressed) |
| rootfs | 5.6 MB | Root filesystem (SquashFS, zlib, 471 inodes) |
| **spsdk** | **52.9 MB** | **Sunplus SDK + AA/CarPlay libraries (SquashFS)** |
| **spapp** | **52 MB** | **Application code + siba plugins (SquashFS)** |
| nvm | 50 MB | Non-volatile storage (settings, calibration) |
| pq | 2 MB | Picture quality settings |
| logo | 2 MB | Boot/splash logo |
| tcon | 384 KB | Display timing controller configuration |
| iop_car | 2 MB | IOP (I/O Processor) firmware for car interface |
| runtime_cfg | 1 MB | Runtime configuration |
| vi | 384 KB | Video input configuration |
| isp_logo | 5 MB | ISP update progress logo |
| vendordata | 256 KB | Vendor-specific data |
| pat_logo | 13 MB | Pattern/test logo |
| version_info | 512 KB | Firmware version string |
| vd_restore | 256 KB | Vendor data restore backup |

---

## Encryption / Decryption

### Init Script Key Derivation (from GPL U-Boot source)

The encryption scheme is documented in Sony's GPL-released U-Boot source code (`common/cmd_ispsp.c`):

1. Load the first 0x20 bytes of the ISP image header (always `"Gemini_ISP_image"` + 16 null bytes)
2. Compute MD5 hash: `MD5("Gemini_ISP_image\x00...")` = `c54b03ae2e35fa06ea2346e8c08075d4`
3. Use this as AES-128-CBC key with zero IV to decrypt the init script

### Partition Data Key (from decrypted update script)

The main update script sets a different AES key for partition data:

```
mw.l ${isp_key_addr0} 0x13279d67
mw.l ${isp_key_addr1} 0xcec46ab4
mw.l ${isp_key_addr2} 0xd4d6acdf
mw.l ${isp_key_addr3} 0x4594a891
```

Key (little-endian): `679d2713 b46ac4ce dfacd6d4 91a89445`

Each partition is encrypted with AES-128-CBC, zero IV, in 1MB blocks (each block independently re-initialized). The update script also includes MD5 checksums for each partition after write-back verification.

### Newer Model Encryption (XAV-9500ES)

The newer `.eup` format uses a significantly stronger scheme:

```
encryption {
  enc "A256CBC"
  alg "RSA-OAEP"
  cek "FRU0LSmtCA7jPXAFYIHz8D1p3tkgUhjnfczGr/u5JAH..."
  iv "gN45jb0ViIoyJZYiB+pEvw=="
}
signature {
  alg "SHA256"
  kid "testKey"
  sig "o561GVY6qqZL28AiMIOLhCi1URAVsb5gmSGAhcEsuJy..."
}
```

- AES-256-CBC (vs AES-128 in older models)
- RSA-OAEP wrapped content encryption key (not derivable from public data)
- SHA-256 signature (key ID "testKey" — possibly a development oversight)
- RAUC bundle format for OTA updates

---

## Android Auto Implementation

### Architecture

Sony's AA implementation is split across two partitions:

**spsdk (SDK layer):**
- `accessory_server` — Main daemon (4.8MB ARM ELF), handles USB accessory detection, AA/CarPlay protocol, iAP2
- `libspandroidauto.so` — AA shared library (with debug symbols!)
- `libaccessory_client.so` — IPC client library for app-level AA control
- `libspcarplay.so` — CarPlay shared library
- `etc/accessory/androidauto_config.xml` — AA configuration (vehicle info, display, sensors, input)

**spapp (Application layer):**
- `siba/plugins/libandroidauto.so` — AA plugin for the siba application framework
- `siba/plugins/libcarplay.so` — CarPlay plugin
- `siba/lib/libcpaa.so` — CarPlay/AA shared abstraction layer
- `siba/bin/appman` — Application manager
- `siba/bin/systemservice` — System service daemon

### Build Environment (from embedded paths)

```
/home/hs.deng/8388_09_28/linux/sdk/services/AccessoryService/AndroidAuto/src/
├── AAPReceiverLibrary/
│   └── BluetoothEndpoint.cpp
├── GALProtocol/
│   └── protos.pb.cpp
├── AndroidAutoManager.cpp
└── ...
```

This confirms Sony's implementation is based on **Google's AAP Receiver Library** with the **GAL (Google Automotive Link) Protocol**. The `protos.pb.cpp` file indicates protobuf-generated code from Google's `.proto` definitions.

### AA Services (from protobuf type information)

The `accessory_server` binary contains protobuf message types for all standard AA services:

**Control Channel:**
- `ServiceDiscoveryRequest` / `ServiceDiscoveryResponse`
- `ChannelOpenRequest` / `ChannelOpenResponse`
- `ChannelCloseNotification`
- `StepChannelRequest` / `StepChannelResponse`
- `ConfigureChannelSpacingRequest` / `ConfigureChannelSpacingResponse`
- `PingRequest` / `PingResponse`
- `ByeByeRequest` / `ByeByeResponse`
- `AuthResponse`

**Video:**
- `VideoConfiguration`
- `VideoSinkCallbacks` / `IVideoSinkCallbacks`
- `VideoFocusNotification`
- `VideoFocusRequestNotification`
- `VideoSessionStartWorkItem` / `VideoSessionStopWorkItem`
- `VideoSessionDataAvailableWorkItem`
- `VideoSessionCodecConfigWorkItem`

**Audio (3 sink channels):**
- `AudioConfiguration`
- `AudioSinkCallbacks` / `IAudioSinkCallbacks`
- `AudioSourceCallbacks` / `IAudioSourceCallbacks`
- `AudioFocusNotification`
- `AudioFocusRequestNotification`
- `AudioSessionStartWorkItem` / `AudioSessionStopWorkItem`
- `AudioSessionDataAvailableWorkItem`
- `MicrophoneRequest` / `MicrophoneResponse`
- Service builders: `buildMediaAudioSinkService`, `buildNavAudioSinkService`, `buildSystemAudioSinkService`

**Input:**
- `InputSourceService` (with `_TouchScreen` and `_TouchPad` variants)
- `TouchEvent` / `TouchEvent_Pointer`
- `KeyEvent` / `KeyEvent_Key`
- `InputReport` / `InputFeedback`
- `AbsoluteEvent` / `RelativeEvent`

**Sensors:**
- `SensorSourceService` (with `_Sensor` variant)
- `SensorBatch` / `SensorRequest` / `SensorResponse` / `SensorError`
- Data types: `LocationData`, `CompassData`, `SpeedData`, `RpmData`, `OdometerData`, `FuelData`, `ParkingBrakeData`, `GearData`, `NightModeData`, `EnvironmentData`, `HvacData`, `DrivingStatusData`, `DeadReckoningData`, `PassengerData`, `DoorData`, `LightData`, `TirePressureData`, `AccelerometerData`, `GyroscopeData`, `GpsSatelliteData`

**Phone:**
- `PhoneStatusService`
- `PhoneStatus` / `PhoneStatus_Call` / `PhoneStatusInput`

**Navigation:**
- `NavigationStatusService` (with `_ImageOptions`)
- `NavigationStatus` / `NavigationStatusStart` / `NavigationStatusStop`
- `NavigationNextTurnEvent` / `NavigationNextTurnDistanceEvent`
- `NavFocusNotification` / `NavFocusRequestNotification`

**Media:**
- `MediaSourceService` / `MediaSinkService`
- `MediaBrowserService`
- `MediaPlaybackStatusService`
- `MediaPlaybackStatus` / `MediaPlaybackMetadata`
- `MediaBrowserInput`
- `MediaGetNode` / `MediaListNode` / `MediaRootNode` / `MediaSongNode` / `MediaSourceNode`

**Bluetooth:**
- `BluetoothService` / `BluetoothEndpoint`
- `BluetoothPairingRequest` / `BluetoothPairingResponse`
- `BluetoothAuthenticationData`
- `BluetoothCallbacksImpl`
- `AndroidAutoBluetoothConfig` (with `setPairingModes`, `setAddress`)

**Voice:**
- `VoiceSessionNotification`

**Vendor Extension:**
- `VendorExtensionService`

**Diagnostics:**
- `GoogleDiagnosticsBugReportRequest` / `GoogleDiagnosticsBugReportResponse`
- `DiagnosticsData`

**Radio (head unit feature, not AA protocol):**
- `RadioService` / `RadioProperties` / `RadioSourceRequest/Response`
- HD Radio support: `HdRadioPsdData`, `HdRadioSisData`, `HdRadioComment`, `HdRadioCommercial`, `HdRadioArtistExperience`, `HdRadioStationInfo`
- `StationPreset` / `StationPresetList`
- `TrafficIncident`

**Generic Notifications:**
- `GenericNotificationService`
- `GenericNotificationMessage`
- `GenericNotificationSubscribe` / `GenericNotificationUnsubscribe`
- `GenericNotificationAck`

**Instrument Cluster:**
- `InstrumentClusterInput`

### AA Configuration

From `androidauto_config.xml` (production version in spapp):

```xml
<CarInfo>
    <VehicleMake value="SONY"/>
    <VehicleModel value="XAV-AX100"/>
    <VehicleYear value="2016"/>
    <HeadUnitMake value="SONY"/>
    <HeadUnitModel value="XAV-AX100"/>
    <HeadUnitSwVersion value="MCU:1.02.09, SOC:1.02.09"/>
</CarInfo>
<Display>
    <DisplayWidth value="143"/>        <!-- mm -->
    <DisplayHeight value="77"/>        <!-- mm -->
    <Density value="170"/>             <!-- DPI -->
    <Resolution480P value="true">
        <fps value="30"/>
    </Resolution480P>
    <Resolution720P value="false"/>
</Display>
<Input>
    <TouchScreen value="true">
        <ScreenType value="2"/>        <!-- 1=capacitive, 2=resistive -->
        <ScreenWidth value="800"/>
        <ScreenHeight value="480"/>
    </TouchScreen>
</Input>
<Sensor>
    <SENSOR_PARKING_BRAKE value="true"/>
    <SENSOR_NIGHT_MODE value="true"/>
    <SENSOR_DRIVING_STATUS_DATA value="true"/>
    <!-- All navigation sensors disabled (no GPS) -->
</Sensor>
```

Key observations:
- Reports as "Rolls-Royce Phantom 2016" in the SDK default config (placeholder)
- Production config correctly identifies as Sony XAV-AX100
- 800x480 resolution at 30fps, resistive touchscreen
- Only parking brake, night mode, and driving status sensors enabled
- No GPS/navigation sensor support (no built-in GPS)
- Probe timeout: 16 seconds (vs 22 in SDK default)

### State Machine

The AA state machine is managed by `AAStateMachineWorkItem` with these states and events:

- **States:** `AARunningState`, `AAStopState`, `AAProbeState`
- **Events:** `AASMEvent`, `AASMExternalEvent`
- **Transport:** `AATransportImpl`
- **Probe:** `AAProbeState`, `AAProbeTimer` (USB device detection)
- **Focus events:** `AAAudioFocusEvent`, `AAVideoFocusEvent`, `AANavFocusEvent`
- **Key events:** `AAKeyEvent`, `AAKeyEventLongPress`
- **Sensor events:** `AASensorEvent`
- **Touch events:** `AATouchEvent`
- **Report events:** `AAReportEvent`, `AAReportProcess`, `AAReportAudioFocusRequest`, `AAReportVideoFocusRequest`, `AAReportNavFocusRequest`, `AAReportBluetoothPairingRequest`, `AAReportRunningStatusChange`, `AAReportAudioSessionNotification`, `AAReportVoiceSessionNotification`

### IPC Architecture

The system uses Android's Binder IPC (ported to embedded Linux):

- `accessory.ChannelManager` — Binder service for channel management
- `accessory.ChannelManagerCallback` — Callback interface
- `accessory.ServiceShell` — Service shell for remote function calls
- `android.auto.accessory.manager` — AA manager service name

This is a full Android Binder framework running on embedded Linux, complete with `ProcessState`, `IPCThreadState`, `BnInterface`, `BpInterface`, and `RefBase`.

---

## Software Stack

| Component | Version | Notes |
|-----------|---------|-------|
| Linux kernel | 3.4.5 | ARM, uncompressed uImage |
| eCos RTOS | — | Dual-boot with Linux for real-time audio/video |
| OpenSSL | 1.0.2h (May 2016) | Used for AA TLS handshake |
| Qt | 5.3 | Widgets + Multimedia, DirectFB backend |
| Protocol Buffers | Lite runtime | Google's protobuf for AA messages |
| DirectFB | 1.6.3 | Display framework (no Wayland/X11) |
| GStreamer | — | Not detected; custom audio/video pipeline |
| U-Boot | 2014.04 | With Sunplus ISP extensions |
| Android Binder | — | Ported from Android for IPC |
| iAP2 | — | Apple iAP2 protocol stack for CarPlay |

### Application Framework ("siba")

Sony's application layer uses a custom Qt-based plugin framework called "siba":

| Binary | Role |
|--------|------|
| `appman` | Application manager — launches and manages plugins |
| `appdaemon` | Application daemon |
| `systemservice` | System service (hardware abstraction) |
| `mcuservice` | MCU communication service |
| `btservice` | Bluetooth service |
| `launcher.sh` | Launch script |

**Plugins (all `.so` libraries):**
- `libandroidauto.so` — Android Auto
- `libcarplay.so` — Apple CarPlay
- `libbtphone.so` — Bluetooth phone
- `libbtaudio.so` — Bluetooth audio (A2DP)
- `libtuner.so` — FM/AM radio
- `libmplayer.so` — Media player
- `libcamera.so` — Rear camera
- `libsettings.so` — System settings
- `libhome.so` — Home screen
- `libaudset.so` — Audio settings / equalizer
- `libcalibrator.so` — Touch calibration
- `libextradevice.so` — External device support
- `libmcuupdate.so` — MCU firmware update
- `libdemo.so` — Demo mode
- `libtestmode.so` — Factory test mode
- `libinitial.so` — Initial setup
- `libmonitoroff.so` — Screen off
- `libsourceoff.so` — Source off / standby
- `libshallowsleep.so` — Power management

---

## Comparison: Sony vs Alpine vs Kenwood vs Pioneer

| Aspect | Sony XAV-AX100 | Alpine iLX-W650BT | Kenwood DNX/DDX | Pioneer DMH-WT8600NEX |
|--------|---------------|-------------------|-----------------|----------------------|
| **Firmware format** | ZIP + AES-encrypted partitions | Single `.FIR` file | `.nfu` archive + ext4 | Encrypted `.avh` container |
| **Extractable?** | **Yes** (key in GPL U-Boot) | Yes (strings) | Yes (ext4 mount + strings) | **No** (fully encrypted) |
| **SoC** | Sunplus SPHE8388 | Toshiba TMM9200 | Telechips TCC8971 | NXP i.MX (likely) |
| **OS** | Linux 3.4.5 + eCos | Bare-metal RTOS | Linux | Android-based |
| **AA SDK** | Google AAP Receiver Library (GAL) | Google "tamul" v1.4.1 | Google `libreceiver-lib.so` | Unknown (encrypted) |
| **SSL library** | OpenSSL 1.0.2h | MatrixSSL 3.9.5 | OpenSSL | Unknown |
| **Protobuf** | Lite runtime | Lite runtime | Full runtime | Unknown |
| **IPC** | Android Binder (ported) | N/A (monolithic) | libconfig (`.cfg`) | Unknown |
| **Display** | DirectFB + Qt 5.3 | Custom (RTOS) | Qt + VNC | Unknown |
| **Encryption** | AES-128-CBC (key in GPL source) | None | None | AES + RSA-2048 (strong) |
| **Security** | Weak (key derivable) | Minimal | Minimal | **Strong** |

### Implications for OpenAuto Prodigy

1. **Complete service catalog:** Sony's implementation provides the most complete list of AA services we've seen, including generic notifications, instrument cluster, vendor extensions, and HD Radio integration — services not visible in the Alpine or Kenwood analyses.

2. **GAL Protocol confirmation:** Sony explicitly uses Google's GAL (Google Automotive Link) protocol with protobuf, confirming this is the standard AA receiver implementation used by OEMs.

3. **Binder IPC pattern:** Sony's use of Android Binder on embedded Linux shows that Google's reference implementation expects a Binder-like IPC mechanism for inter-process communication between the AA protocol handler and the application UI.

4. **Configuration format:** The `androidauto_config.xml` format provides a clear template for what vehicle/display/sensor/input information needs to be provided to the AA stack during initialization.

5. **State machine clarity:** The explicit state machine (Probe → Running → Stop) with typed events (audio focus, video focus, nav focus, touch, sensor, key) confirms the event-driven architecture used by production AA implementations.

6. **Dual RTOS architecture:** The eCos + Linux dual-boot pattern (where eCos handles real-time audio/video DSP while Linux runs the application layer) is a common embedded automotive pattern — relevant for understanding timing constraints.

---

## XAV-9500ES / Newer Generation Notes

The newer Sony models (XAV-9500ES, AX8500, etc.) use a fundamentally different architecture:

- **RAUC OTA framework** — suggests Yocto/OpenEmbedded build system
- **RSA-OAEP key wrapping** — no way to derive the AES key from public data
- **Signature key ID "testKey"** — possibly a development build that shipped to production, or a deliberate naming choice
- **GPL source available** — contains Linux kernel + standard GPL userspace, but NOT the proprietary AA implementation
- **Firmware version 3.02.00** — shared across all current models (XAV-AX8500, 9500ES, AX6000, AX4000)

The XAV-AX5500 and other older MCU-based models use a different format (`.FIR` with `KRSELECO` header) that is also fully encrypted with no key recovery path.

---

## Community Resources

- [superdavex/Sony-XAV-AX100](https://github.com/superdavex/Sony-XAV-AX100) — Community RE project for the AX100
- [Sony OSS Portal — Car Audio](https://oss.sony.net/Products/Linux/Audio/category02.html) — GPL source code for all Sony car audio models
- [Sony XAV-AX100 GPL source](https://prodgpl.blob.core.windows.net/download/Audio/XAV-AX100/) — Individual component tarballs (kernel, U-Boot, Qt, etc.)

---

## References

- Sony XAV-AX100 firmware v1.02.09 from `hav.update.sony.net`
- Sony XAV-9500ES firmware v3.02.00 from `hav.update.sony.net`
- Sony XAV-AX5500 firmware v2.00 from `hav.update.sony.net`
- Sunplus SPHE8388 U-Boot source (GPL) from `prodgpl.blob.core.windows.net`
- Sunplus Gemini kernel source (GPL) from `prodgpl.blob.core.windows.net`
