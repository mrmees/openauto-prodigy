# Reverse Engineering Notes — OpenAuto Pro v16.1

This document captures everything discovered through reverse engineering the OpenAuto Pro v16.1 binary image. It's intended as a reference for anyone continuing analysis, whether with Ghidra, binary patching, or rebuilding from source.

## Source Material

### Disk Image
- **File:** `bluewavestudio-openauto-pro-release-16.1.img` (7.3 GB)
- **Format:** MBR partition table, 2 partitions
  - Partition 1: FAT32 boot (offset 4,194,304 / sector 8192)
  - Partition 2: ext4 root (offset 276,824,064 / sector 540,672)
- **OS:** Raspbian/Raspberry Pi OS (armhf)
- **Total files:** ~160,773

### Key Binaries (extracted from ext4 root)
| File | Size | Type | Notes |
|------|------|------|-------|
| `autoapp` | 28,296,188 bytes | ARM 32-bit ELF, stripped | Main application |
| `controller_service` | 457 KB | ARM 32-bit ELF, stripped | Companion service (watchdog-managed) |
| `libaasdk.so` | 884 KB | ARM shared library | Android Auto SDK |
| `libaasdk_proto.so` | 1.3 MB | ARM shared library | AA protobuf definitions |

**BuildID (autoapp):** `13030d9465b45b7b39c0154fd86783b1c20f90e1`

### Build Environment (from strings in binary)
- **Build path:** `/home/pi/workdir/openauto/`
- **Compiler:** GCC (ARM, likely from Raspbian toolchain)
- **Qt version:** 5.11
- **Boost version:** 1.67
- **Protobuf:** libprotobuf 17 (proto2 syntax)
- **OpenSSL:** 1.1.x
- **Build date:** ~February 2024 (v16.1 release)

---

## Architecture

### Namespace Structure
The binary uses namespace `f1x::openauto::autoapp::` with these sub-namespaces:

| Namespace | Purpose | Key Classes |
|-----------|---------|-------------|
| `androidauto` | Android Auto connection/services | `AndroidAutoEntity`, `SensorService`, `VideoService`, `AudioService`, `InputService`, `BluetoothService`, `WifiService` |
| `autobox` | Wireless AA dongle (Autobox) | Connection management, USB/WiFi transport |
| `bluetooth` | BT telephony & media | A2DP, HFP, phonebook (via BlueZ/KF5BluezQt) |
| `mirroring` | Screen mirroring | ADB-based mirroring service |
| `obd` | OBD-II diagnostics | Gauge formulas, ELM327 interface |
| `audio` | Audio pipeline | LADSPA equalizer, PulseAudio, FM radio (RTL-SDR), music library |
| `ui` | QML controllers | C++ backend for each QML view |
| `api` | Plugin API | Protobuf over Unix socket, external app integration |
| `system` | System services | Gesture sensor, temp sensor, day/night mode, WiFi config |

### Shared Library Dependencies (54 total)
Key dependencies linked by `autoapp`:
```
libQt5Quick, libQt5Qml, libQt5Core, libQt5Gui, libQt5Widgets
libQt5Multimedia, libQt5DBus, libQt5X11Extras, libQt5Network
libboost_filesystem, libboost_system, libboost_log, libboost_thread, libboost_regex
libaasdk.so, libaasdk_proto.so
libprotobuf.so.17
libssl.so.1.1, libcrypto.so.1.1
libusb-1.0
libpulse, libasound
libKF5BluezQt.so.6
libbcm_host (Broadcom VideoCore GPU)
libgps, libi2c, librtlsdr, libxdo
```

### Service Architecture
The application follows a service-oriented pattern:
- `ServiceFactory::create()` — instantiates all AA services and registers their channel descriptors
- Each service implements `fillFeatures()` to declare its capabilities to Android Auto
- Services handle channels: video, audio (media/speech/system), input, sensor, bluetooth, wifi
- The WiFi service is central to the Android Auto 12.7+ compatibility bug (see [android-auto-fix-research.md](android-auto-fix-research.md))

---

## Recovered Assets

### QML UI (162 files)
**Extraction method:** Custom Python script (`tools/extract_qrc.py`) that searches the binary's `.rodata` section for `import QtQuick` patterns and extracts contiguous UTF-8 text blocks bounded by null bytes. SVGs extracted by finding `<svg` to `</svg>` blocks.

**Directory structure:**
```
original/qml/
├── root.qml, loader.qml, background.qml     # App bootstrap
├── MainScreen.qml, MainScreenDelegate.qml    # Main UI entry
├── ApplicationBase.qml, ApplicationMenu.qml  # App framework
├── applications/
│   ├── android_auto/     (12 files)  # AA connection UI, projection views
│   ├── autobox/          (10 files)  # Wireless AA dongle UI
│   ├── dashboards/       (8 files)   # OBD gauge dashboards
│   ├── equalizer/        (4 files)   # Audio equalizer
│   ├── home/             (3 files)   # Home screen
│   ├── launcher/         (4 files)   # App launcher grid
│   ├── music/            (11 files)  # Music player
│   ├── settings/         (17 files)  # All settings screens
│   └── telephone/        (12 files)  # BT telephony, dialer, contacts
├── components/           (22 files)  # Gauges, bars, camera views, clock
├── controls/             (41 files)  # Buttons, sliders, popups, keyboards
├── widgets/              (11 files)  # Home screen widgets
└── tools/                (3 files)   # ClockTools.js, MathTools.js, Runes.js
```

**Notes:**
- QML files reference C++ backend objects exposed via `qmlRegisterType` (e.g., `applicationManager`, `systemManager`, `audioManager`)
- Some QML files could not be auto-named and are stored as `unknown_NNN_0xOFFSET.qml`
- The QML import paths reveal the intended module structure (`import Applications.AndroidAuto 1.0`, etc.)

### SVG Icons (88 files)
Extracted alongside QML. Named icons include: `ico_androidauto`, `ico_bluetooth`, `ico_music`, `ico_settings`, `ico_navigation`, `ico_phone`, `ico_obd`, etc. Unnamed icons stored as `svg_NNN_0xOFFSET.svg`.

### Protobuf API Definition (34 messages)
**Extraction method:** Custom Python script (`tools/extract_proto.py`). Protobuf's generated C++ code embeds a serialized `FileDescriptorProto` at offset **0x5c1378** in the `autoapp` binary (6,700 bytes). Decoded using `google.protobuf.descriptor_pb2`.

**Package:** `io.bluewavestudio.openautopro.api`

**Key message groups:**
- **Handshake:** `HelloRequest`, `HelloResponse`, `Version`
- **Status:** `ProjectionStatus`, `MediaStatus`, `MediaMetadata`, `NavigationStatus`
- **UI Extensions:** `RegisterStatusIcon*`, `RegisterNotificationChannel*`, `ShowNotification`
- **OBD:** `ObdInjectGaugeFormulaValue`, `ObdConnectionStatus`, `SubscribeObdGaugeChange*`
- **Audio:** `RegisterAudioFocusReceiver*`, `AudioFocusChangeRequest/Response`, `AudioFocusAction`, `AudioFocusMediaKey`
- **Phone:** `PhoneConnectionStatus`, `PhoneVoiceCallStatus`

This API is used by external plugins to interact with OAP over a Unix domain socket.

### Scripts (7 files)
- `openauto` — Main launcher script (sets env, starts autoapp)
- `controller_service_watchdog.sh` — Restarts controller_service if it crashes
- `bwscursor` — Python cursor management
- `bwsrtc` — Python RTC (real-time clock) utility
- `bwshotspot` — Python WiFi hotspot configuration
- `openautopro.splash.service` — Systemd splash screen service
- `rpi4pci.sh` — Raspberry Pi 4 PCIe configuration

---

## Binary Analysis Details

### String-Based Discovery
The binary is stripped but retains significant information through:
1. **C++ template instantiations** — Mangled names reveal class hierarchies, method signatures, and type relationships even without a symbol table
2. **Qt meta-object strings** — QML type registrations, signal/slot names
3. **Boost.Log channel names** — Reveal the logging structure and module names
4. **Error messages and exceptions** — Reveal control flow and error handling paths
5. **Build paths** — `/home/pi/workdir/openauto/` reveals source tree structure

### Key Offsets in autoapp
| Offset | Content | Notes |
|--------|---------|-------|
| 0x5c1378 | Serialized FileDescriptorProto | Api.proto, 6700 bytes |
| Various .rodata | QML source text | Null-terminated UTF-8 blocks |
| Various .rodata | SVG content | `<svg>` to `</svg>` blocks |

### Relationship to Upstream
OpenAuto Pro is built on f1xpl/openauto (GPLv3, frozen 2018) + f1xpl/aasdk (GPLv3, frozen 2018). BlueWave made extensive additions:
- Complete QML UI (upstream openauto uses X11/EGL directly)
- Bluetooth telephony stack
- OBD-II diagnostics
- Screen mirroring
- Music player with library
- FM radio (RTL-SDR)
- Plugin API (protobuf)
- Autobox wireless dongle support
- Equalizer (LADSPA/PulseAudio)
- System services (gestures, temp, day/night)

The core AA protocol handling (aasdk) is relatively unchanged from upstream, with the exception that BlueWave compiled it as a shared library rather than static.

---

## Android Auto Compatibility Issue

**Full details:** [android-auto-fix-research.md](android-auto-fix-research.md)

**Summary:** Android Auto 12.7+ (Sept 2024) sends a WiFi credentials request on the WiFi service channel. OAP's `WifiService` is a stub that doesn't handle this message. AA interprets the silence as failure and refuses to send video.

**SonOfGib's fix:** Implemented proper `WIFIServiceChannel` in aasdk and `WifiService` handler in openauto. Confirmed working with AA 12.8–13.1.

**Fix approach options for OAP:**
1. Binary patch — disable WiFi channel in `ServiceFactory` (simplest, loses wireless AA)
2. Library swap — rebuild `libaasdk.so` with fixes (protocol fix only, no service handler)
3. LD_PRELOAD hook — intercept at runtime (hacky but non-destructive)
4. Full rebuild — use recovered QML/proto + fixed aasdk/openauto (long-term solution)

---

## Upstream Forks (cloned locally)

| Repo | Branch | Location | Purpose |
|------|--------|----------|---------|
| f1xpl/openauto | master | `upstream/openauto/` | Original open source (2018, reference) |
| f1xpl/aasdk | master | `upstream/aasdk/` | Original AA SDK (2018, reference) |
| SonOfGib/openauto | sonofgib-newdev | `upstream/sonofgib-openauto/` | Fixed openauto with WiFi handler |
| SonOfGib/aasdk | sonofgib-newdev | `upstream/sonofgib-aasdk/` | Fixed aasdk with WiFi channel |

**Key fix commits:**
- aasdk: [SonOfGib/aasdk@284c366](https://github.com/SonOfGib/aasdk/commit/284c36661c870bf0a557431c559490608e8cb898)
- openauto: [SonOfGib/openauto@e3aa777](https://github.com/SonOfGib/openauto/commit/e3aa777467e14e15a011e3262c521b2c388de9af)

---

## Tools

### extract_qrc.py
Extracts embedded Qt resources (QML, SVG) from the binary.
- Scans `.rodata` for `import QtQuick` patterns
- Extracts contiguous UTF-8 text blocks until null byte
- Auto-names files based on QML content (component IDs, imports)
- Also extracts SVGs by `<svg>` pattern matching

### extract_proto.py
Recovers protobuf schema from embedded FileDescriptorProto.
- Searches for `"Api.proto"` string in binary
- Scans backward to find serialized descriptor start
- Uses `google.protobuf.descriptor_pb2` to decode
- Outputs reconstructed `.proto` file with all fields, types, enums

---

## Next Steps for Analysis

### Priority: Ghidra Decompilation
1. Load `autoapp` as ARM 32-bit ELF in Ghidra
2. Focus areas:
   - `ServiceFactory::create()` — where services are registered (patch point for WiFi fix)
   - `WifiService::fillFeatures()` — WiFi channel descriptor registration
   - `AndroidAutoEntity` — AA connection lifecycle, handshake, version negotiation
   - `ChannelFactory` / channel handler dispatch — how incoming AA messages are routed
3. Cross-reference with SonOfGib's source code to identify equivalent functions

### Priority: Library Compatibility Check
1. Check if SonOfGib's rebuilt `libaasdk.so` can be loaded by OAP's `autoapp`
2. Key compatibility factors:
   - Same Boost 1.67 ABI (Boost.Asio, Boost.Log)
   - Same protobuf 17 ABI
   - Same OpenSSL 1.1 ABI
   - Symbol versioning / SONAME matches
3. If compatible, this is the fastest path to a working fix

### Future: Full Rebuild
1. Use SonOfGib's openauto as the application framework
2. Integrate recovered QML UI
3. Re-implement BlueWave's C++ backend classes (informed by binary analysis)
4. Implement recovered Api.proto for plugin compatibility
