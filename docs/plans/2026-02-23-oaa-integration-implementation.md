# open-androidauto Integration Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace aasdk with open-androidauto throughout the OpenAuto Prodigy app, eliminating the Boost.ASIO dependency from the AA pipeline.

**Architecture:** Channel handlers implement `oaa::IChannelHandler` (or `IAVChannelHandler`) and register with `oaa::AASession`. AASession runs on a dedicated QThread. Service discovery descriptors are contributed by each handler. AndroidAutoService becomes a thin QObject orchestrating QTcpServer + AASession + BT discovery.

**Tech Stack:** Qt 6 (6.4 dev / 6.8 Pi), C++17, CMake, protobuf (proto2), PipeWire (audio), OpenSSL (TLS)

**Design doc:** `docs/plans/2026-02-23-oaa-integration-design.md`

**APK decompilation reference:** `~/claude/personal/openautopro/openauto-pro-community/docs/android-auto-apk-decompilation.md` — AA v16.1 APK analysis revealing expanded enums, new message IDs, and complete service type catalog.

**Channel ID reference (from aasdk ChannelId enum):**

| Name | ID | Handler Type |
|------|-----|-------------|
| CONTROL | 0 | Built-in (ControlChannel) |
| INPUT | 1 | IChannelHandler |
| SENSOR | 2 | IChannelHandler |
| VIDEO | 3 | IAVChannelHandler |
| MEDIA_AUDIO | 4 | IAVChannelHandler |
| SPEECH_AUDIO | 5 | IAVChannelHandler |
| SYSTEM_AUDIO | 6 | IAVChannelHandler |
| AV_INPUT | 7 | IAVChannelHandler |
| BLUETOOTH | 8 | IChannelHandler |
| WIFI | 14 | IChannelHandler |

**Message ID enums (from proto files):**
- Control: `ControlMessageIdsEnum.proto` (0x0001–0x0013, **+0x0018, 0x001A from APK**)
- AV: `AVChannelMessageIdsEnum.proto` (0x0000–0x8008, **+0x8009–0x8014 from APK**)
- Input: `InputChannelMessageIdsEnum.proto` (0x8001–0x8003)
- Sensor: `SensorChannelMessageIdsEnum.proto` (0x8001–0x8003)
- Bluetooth: `BluetoothChannelMessageIdsEnum.proto` (0x8001–0x8003)
- WiFi: `WifiChannelMessageIdsEnum.proto` (0x8001–0x8002)

---

## Phase 0: Protocol Compatibility Hardening

APK decompilation of Android Auto v16.1 revealed that many of our proto enums are incomplete — missing values that real phones may send or expect. Since these are proto2 enums, an unrecognized value causes parse failure and field-level data loss. We expand all narrow enums **now**, before building channel handlers that depend on them.

**Guiding principle:** Don't restructure proto wire format (field numbers, message structure). Our current protos produce bytes that phones accept. Only expand enum value sets and add new message ID constants.

### Task 1: Expand VideoFocus Enums

**Source:** APK `VideoFocusType` (4 values) and `VideoFocusReason` (5 values)

**Files:**
- Modify: `libs/open-androidauto/proto/VideoFocusModeEnum.proto`
- Modify: `libs/open-androidauto/proto/VideoFocusReasonEnum.proto`

**Step 1: Update VideoFocusModeEnum.proto**

Current has NONE(0), FOCUSED(1), UNFOCUSED(2). The APK uses a different naming scheme (PROJECTED/NATIVE/etc.) but maps to the same wire values. Keep our naming convention but add the missing value:

```proto
enum Enum
{
    NONE = 0;
    FOCUSED = 1;           // APK: VIDEO_FOCUS_PROJECTED
    UNFOCUSED = 2;         // APK: VIDEO_FOCUS_NATIVE
    FOCUSED_TRANSIENT = 3; // APK: VIDEO_FOCUS_NATIVE_TRANSIENT (phone still renders)
    FOCUSED_NO_INPUT = 4;  // APK: VIDEO_FOCUS_PROJECTED_NO_INPUT_FOCUS (AA video, HU touch)
}
```

Wait — the APK mapping is: PROJECTED=1, NATIVE=2, NATIVE_TRANSIENT=3, PROJECTED_NO_INPUT_FOCUS=4. Our existing code maps FOCUSED→PROJECTED and UNFOCUSED→NATIVE. Value 3 in APK is "NATIVE_TRANSIENT" which means HU has temporary focus but phone keeps rendering (think reverse camera). Value 4 is AA video displayed but HU handles touch. Let's use names that match the semantic meaning:

```proto
enum Enum
{
    NONE = 0;
    FOCUSED = 1;               // AA projected, AA handles input
    UNFOCUSED = 2;             // HU native UI, AA paused
    UNFOCUSED_TRANSIENT = 3;   // HU native temporarily (reverse cam), phone keeps rendering
    FOCUSED_NO_INPUT = 4;      // AA video displayed, HU handles touch (split-screen/PIP)
}
```

**Step 2: Update VideoFocusReasonEnum.proto**

```proto
enum Enum
{
    NONE = 0;               // APK: UNKNOWN
    PHONE_SCREEN_OFF = 1;   // Phone screen turned off
    LAUNCH_NATIVE = 2;      // User switched to HU native UI
    LAST_MODE = 3;          // Restore previous focus state
    USER_SELECTION = 4;     // User explicitly switched views
}
```

**Step 3: Build and test**

```bash
cd libs/open-androidauto/build && cmake --build . -j$(nproc) && ctest --output-on-failure
```

**Step 4: Commit**

```bash
git add libs/open-androidauto/proto/VideoFocus*.proto
git commit -m "proto: expand VideoFocusMode and VideoFocusReason with APK-confirmed values"
```

---

### Task 2: Expand ShutdownReason (ByeBye) Enum

**Source:** APK `ByeByeReason` — 8 values vs our 2 (NONE, QUIT)

**Files:**
- Modify: `libs/open-androidauto/proto/ShutdownReasonEnum.proto`

**Step 1: Update enum**

```proto
enum Enum
{
    NONE = 0;
    QUIT = 1;                          // APK: USER_SELECTION
    DEVICE_SWITCH = 2;                 // Switching to different phone
    NOT_SUPPORTED = 3;                 // Feature not supported
    NOT_CURRENTLY_SUPPORTED = 4;       // Feature temporarily unavailable
    PROBE_SUPPORTED = 5;               // Probe connection (capability check)
    WIRELESS_PROJECTION_DISABLED = 6;  // WiFi AA disabled
    POWER_DOWN = 7;                    // HU shutting down
    USER_PROFILE_SWITCH = 8;           // Switching user profiles
}
```

**Step 2: Build, test, commit**

```bash
cd libs/open-androidauto/build && cmake --build . -j$(nproc) && ctest --output-on-failure
git add libs/open-androidauto/proto/ShutdownReasonEnum.proto
git commit -m "proto: expand ShutdownReason with all 8 APK-confirmed ByeBye reasons"
```

---

### Task 3: Expand Status Enum

**Source:** APK `StatusCode` — 30+ values vs our 2 (OK, FAIL)

**Files:**
- Modify: `libs/open-androidauto/proto/StatusEnum.proto`

**Step 1: Update enum**

```proto
enum Enum
{
    OK = 0;                                        // APK: STATUS_SUCCESS
    FAIL = 1;                                      // APK: STATUS_UNSOLICITED_MESSAGE
    NO_COMPATIBLE_VERSION = -1;
    CERTIFICATE_ERROR = -2;
    AUTHENTICATION_FAILURE = -3;
    INVALID_SERVICE = -4;
    INVALID_CHANNEL = -5;
    INVALID_PRIORITY = -6;
    INTERNAL_ERROR = -7;
    MEDIA_CONFIG_MISMATCH = -8;
    INVALID_SENSOR = -9;
    BLUETOOTH_PAIRING_DELAYED = -10;
    BLUETOOTH_UNAVAILABLE = -11;
    BLUETOOTH_INVALID_ADDRESS = -12;
    BLUETOOTH_INVALID_PAIRING_METHOD = -13;
    BLUETOOTH_INVALID_AUTH_DATA = -14;
    BLUETOOTH_AUTH_DATA_MISMATCH = -15;
    BLUETOOTH_HFP_ANOTHER_CONNECTION = -16;
    BLUETOOTH_HFP_CONNECTION_FAILURE = -17;
    KEYCODE_NOT_BOUND = -18;
    RADIO_INVALID_STATION = -19;
    INVALID_INPUT = -20;
    RADIO_STATION_PRESETS_NOT_SUPPORTED = -21;
    RADIO_COMM_ERROR = -22;
    AUTHENTICATION_FAILURE_CERT_NOT_YET_VALID = -23;
    AUTHENTICATION_FAILURE_CERT_EXPIRED = -24;
    PING_TIMEOUT = -25;
    CAR_PROPERTY_SET_REQUEST_FAILED = -26;
    CAR_PROPERTY_LISTENER_REGISTRATION_FAILED = -27;
    COMMAND_NOT_SUPPORTED = -250;
    FRAMING_ERROR = -251;
    UNEXPECTED_MESSAGE = -253;
    BUSY = -254;
    OUT_OF_MEMORY = -255;
}
```

**Note:** proto2 supports negative enum values. Verify protobuf compiler accepts this; if not, use `sint32` field type and constants instead.

**Step 2: Build, test, commit**

```bash
cd libs/open-androidauto/build && cmake --build . -j$(nproc) && ctest --output-on-failure
git add libs/open-androidauto/proto/StatusEnum.proto
git commit -m "proto: expand Status enum with all 30+ APK-confirmed status codes"
```

---

### Task 4: Fix AudioFocusType Naming + Expand BluetoothPairingMethod

**Source:** APK `AudioFocusRequestType` confirms value 3 is `GAIN_TRANSIENT_MAY_DUCK` (not `GAIN_NAVI`). APK `BluetoothPairingMethod` has proper names for all values.

**Files:**
- Modify: `libs/open-androidauto/proto/AudioFocusTypeEnum.proto`
- Modify: `libs/open-androidauto/proto/BluetoothPairingMethodEnum.proto`

**Step 1: Fix AudioFocusTypeEnum.proto**

```proto
enum Enum
{
    NONE = 0;
    GAIN = 1;
    GAIN_TRANSIENT = 2;
    GAIN_TRANSIENT_MAY_DUCK = 3;   // Was: GAIN_NAVI (misleading name from aasdk)
    RELEASE = 4;
}
```

**Step 2: Fix BluetoothPairingMethodEnum.proto**

```proto
enum Enum
{
    NONE = 0;
    OOB = 1;                    // Was: UNK_1 — Out-of-Band pairing
    NUMERIC_COMPARISON = 2;     // Was: A2DP (misleading name from aasdk)
    PASSKEY_ENTRY = 3;          // Was: UNK_3
    PIN = 4;                    // Was: HFP (misleading name from aasdk)
}
```

Also add the APK's `-1` value if proto2 supports it:
```proto
    UNAVAILABLE = -1;           // APK: BLUETOOTH_PAIRING_UNAVAILABLE
```

**Step 3: Build, test, commit**

```bash
cd libs/open-androidauto/build && cmake --build . -j$(nproc) && ctest --output-on-failure
git add libs/open-androidauto/proto/AudioFocusTypeEnum.proto libs/open-androidauto/proto/BluetoothPairingMethodEnum.proto
git commit -m "proto: fix AudioFocusType/BluetoothPairingMethod naming from APK decompilation"
```

---

### Task 5: Expand SensorType Enum

**Source:** APK has 26 sensor types vs our 21. Missing: TOLL_CARD(22), VEHICLE_ENERGY_MODEL(23), TRAILER(24), RAW_VEHICLE_ENERGY_MODEL(25), RAW_EV_TRIP_SETTINGS(26).

**Files:**
- Modify: `libs/open-androidauto/proto/SensorTypeEnum.proto`

**Step 1: Add missing values**

```proto
    // Existing values 0-21 unchanged...
    TOLL_CARD = 22;
    VEHICLE_ENERGY_MODEL = 23;
    TRAILER = 24;
    RAW_VEHICLE_ENERGY_MODEL = 25;
    RAW_EV_TRIP_SETTINGS = 26;
```

**Step 2: Build, test, commit**

```bash
cd libs/open-androidauto/build && cmake --build . -j$(nproc) && ctest --output-on-failure
git add libs/open-androidauto/proto/SensorTypeEnum.proto
git commit -m "proto: add 5 EV/trailer sensor types from APK decompilation"
```

---

### Task 6: Add New Message IDs to Enums

**Source:** APK shows control messages 24 (CALL_AVAILABILITY_STATUS) and 26 (SERVICE_DISCOVERY_UPDATE), plus AV messages 0x8009-0x8014.

**Files:**
- Modify: `libs/open-androidauto/proto/ControlMessageIdsEnum.proto`
- Modify: `libs/open-androidauto/proto/AVChannelMessageIdsEnum.proto`

**Step 1: Add to ControlMessageIdsEnum.proto**

```proto
    // After AUDIO_FOCUS_RESPONSE = 0x0013:
    CALL_AVAILABILITY_STATUS = 0x0018;     // Car → Phone: boolean is_call_available
    SERVICE_DISCOVERY_UPDATE = 0x001A;     // Car → Phone: updated ServiceDescriptor
```

**Note:** APK message IDs 24 and 26 are decimal. 24 = 0x0018, 26 = 0x001A.

**Step 2: Add to AVChannelMessageIdsEnum.proto**

```proto
    // After VIDEO_FOCUS_INDICATION = 0x8008:
    UPDATE_UI_CONFIG_REQUEST = 0x8009;
    UPDATE_UI_CONFIG_REPLY = 0x800A;
    AUDIO_UNDERFLOW_NOTIFICATION = 0x800B;
    ACTION_TAKEN_NOTIFICATION = 0x800C;
    OVERLAY_PARAMETERS = 0x800D;
    OVERLAY_START = 0x800E;
    OVERLAY_STOP = 0x800F;
    OVERLAY_SESSION_DATA = 0x8010;
    UPDATE_HU_UI_CONFIG_REQUEST = 0x8011;
    UPDATE_HU_UI_CONFIG_RESPONSE = 0x8012;
    MEDIA_STATS = 0x8013;
    MEDIA_OPTIONS = 0x8014;
```

**Step 3: Build, test, commit**

```bash
cd libs/open-androidauto/build && cmake --build . -j$(nproc) && ctest --output-on-failure
git add libs/open-androidauto/proto/ControlMessageIdsEnum.proto libs/open-androidauto/proto/AVChannelMessageIdsEnum.proto
git commit -m "proto: add new control and AV message IDs from APK decompilation"
```

---

### Task 7: Add MediaCodecType and ServiceType Enums (New Files)

**Source:** APK reveals full video codec catalog and 21 service types.

**Files:**
- Create: `libs/open-androidauto/proto/MediaCodecTypeEnum.proto`
- Create: `libs/open-androidauto/proto/ServiceTypeEnum.proto`
- Modify: `libs/open-androidauto/CMakeLists.txt` (add to proto compilation list)

**Step 1: Create MediaCodecTypeEnum.proto**

```proto
syntax="proto2";
package oaa.proto.enums;

message MediaCodecType
{
    enum Enum
    {
        NONE = 0;
        AUDIO_PCM = 1;
        AUDIO_AAC_LC = 2;
        VIDEO_H264_BP = 3;
        AUDIO_AAC_LC_ADTS = 4;
        VIDEO_VP9 = 5;
        VIDEO_AV1 = 6;
        VIDEO_H265 = 7;
    }
}
```

**Step 2: Create ServiceTypeEnum.proto**

```proto
syntax="proto2";
package oaa.proto.enums;

message ServiceType
{
    enum Enum
    {
        UNKNOWN = 0;
        CONTROL = 1;
        VIDEO_SINK = 2;
        AUDIO_SINK_GUIDANCE = 3;
        AUDIO_SINK_SYSTEM = 4;
        AUDIO_SINK_MEDIA = 5;
        AUDIO_SOURCE = 6;
        SENSOR_SOURCE = 7;
        INPUT_SOURCE = 8;
        BLUETOOTH = 9;
        NAVIGATION_STATUS = 10;
        MEDIA_PLAYBACK_STATUS = 11;
        MEDIA_BROWSER = 12;
        PHONE_STATUS = 13;
        NOTIFICATION = 14;
        RADIO = 15;
        VENDOR_EXTENSION = 16;
        WIFI_PROJECTION = 17;
        WIFI_DISCOVERY = 18;
        CAR_CONTROL = 19;
        CAR_LOCAL_MEDIA = 20;
        BUFFERED_MEDIA_SINK = 21;
    }
}
```

**Step 3: Add to CMakeLists.txt, build, test, commit**

```bash
cd libs/open-androidauto/build && cmake --build . -j$(nproc) && ctest --output-on-failure
git add libs/open-androidauto/proto/MediaCodecTypeEnum.proto libs/open-androidauto/proto/ServiceTypeEnum.proto libs/open-androidauto/CMakeLists.txt
git commit -m "proto: add MediaCodecType and ServiceType enums from APK decompilation"
```

---

## Phase 1: Close Library Parity Gaps

Before touching the app, fix gaps in `libs/open-androidauto/` identified during design review.

### Task 8: Channel Descriptor API

Add a method to `IChannelHandler` so each handler can contribute its service discovery descriptor.

**Files:**
- Modify: `libs/open-androidauto/include/oaa/Channel/IChannelHandler.hpp`
- Modify: `libs/open-androidauto/src/Channel/IChannelHandler.cpp`
- Modify: `libs/open-androidauto/src/Session/AASession.cpp` (buildServiceDiscoveryResponse)
- Test: `libs/open-androidauto/tests/test_session_fsm.cpp`

**Step 1: Add `fillChannelDescriptor` virtual method to IChannelHandler**

```cpp
// In IChannelHandler.hpp, add:
#include "ChannelDescriptorData.pb.h"

// New virtual method (default empty — control channel doesn't need one):
virtual void fillChannelDescriptor(oaa::proto::data::ChannelDescriptor& descriptor) const {
    descriptor.set_channel_id(channelId());
}
```

**Step 2: Update AASession::buildServiceDiscoveryResponse to collect descriptors**

Replace the current minimal loop in `AASession.cpp:311-316` with:

```cpp
for (auto it = channels_.cbegin(); it != channels_.cend(); ++it) {
    auto* desc = resp.add_channels();
    it.value()->fillChannelDescriptor(*desc);
}
```

**Step 3: Add rich discovery fields from current AndroidAutoEntity**

Update `buildServiceDiscoveryResponse()` to include all the fields currently in `AndroidAutoEntity::onServiceDiscoveryRequest()`:
- `headunit_info` block (make, model, year, vehicle_id, sw_build, sw_version)
- `connection_configuration` with `ping_configuration`
- `display_name`
- Legacy fields (head_unit_name, car_model, car_year, etc.)
- `can_play_native_media_during_vr`

Add corresponding fields to `SessionConfig`.

**Step 4: Write test for descriptor collection**

```cpp
void testServiceDiscoveryIncludesChannelDescriptors() {
    // Register a mock handler that fills descriptor with AV channel data
    // Verify buildServiceDiscoveryResponse output includes it
}
```

**Step 5: Build and run tests**

```bash
cd libs/open-androidauto/build && cmake --build . -j$(nproc) && ctest --output-on-failure
```

**Step 6: Commit**

```bash
git add libs/open-androidauto/
git commit -m "feat(oaa): add channel descriptor API and rich service discovery response"
```

---

### Task 9: Wire Control-Plane Signals Through AASession

Currently `ControlChannel` emits `audioFocusRequested`, `navigationFocusRequested`, `voiceSessionRequested` but `AASession` doesn't handle them.

**Files:**
- Modify: `libs/open-androidauto/include/oaa/Session/AASession.hpp`
- Modify: `libs/open-androidauto/src/Session/AASession.cpp`
- Test: `libs/open-androidauto/tests/test_session_fsm.cpp`

**Step 1: Add signals to AASession**

```cpp
// In AASession.hpp signals section:
void audioFocusRequested(const QByteArray& payload);
void navigationFocusRequested(const QByteArray& payload);
void voiceSessionRequested(const QByteArray& payload);
```

**Step 2: Connect ControlChannel signals in AASession constructor**

```cpp
connect(controlChannel_, &ControlChannel::audioFocusRequested,
        this, &AASession::audioFocusRequested);
connect(controlChannel_, &ControlChannel::navigationFocusRequested,
        this, &AASession::navigationFocusRequested);
connect(controlChannel_, &ControlChannel::voiceSessionRequested,
        this, &AASession::voiceSessionRequested);
```

**Step 3: Add convenience send methods**

```cpp
// Public methods for app-level use:
void sendAudioFocusResponse(const QByteArray& payload);
void sendNavigationFocusResponse(const QByteArray& payload);
```

These delegate to `controlChannel_->sendAudioFocusResponse()` etc.

**Step 4: Build and test**

```bash
cd libs/open-androidauto/build && cmake --build . -j$(nproc) && ctest --output-on-failure
```

**Step 5: Commit**

```bash
git add libs/open-androidauto/
git commit -m "feat(oaa): wire audio/nav/voice focus signals through AASession"
```

---

### Task 10: Add Native Socket Descriptor to Transport

The connection watchdog needs the raw FD for `tcp_info` polling.

**Files:**
- Modify: `libs/open-androidauto/include/oaa/Transport/ITransport.hpp`
- Modify: `libs/open-androidauto/include/oaa/Transport/TCPTransport.hpp`
- Modify: `libs/open-androidauto/src/Transport/TCPTransport.cpp`
- Test: `libs/open-androidauto/tests/test_tcp_transport.cpp`

**Step 1: Add virtual method to ITransport**

```cpp
virtual qintptr nativeSocketDescriptor() const { return -1; }
```

**Step 2: Implement in TCPTransport**

```cpp
qintptr TCPTransport::nativeSocketDescriptor() const {
    return socket_ ? socket_->socketDescriptor() : -1;
}
```

**Step 3: Test**

```cpp
void testNativeSocketDescriptor() {
    // Connected transport should return valid FD (> 0)
    // Disconnected should return -1
}
```

**Step 4: Build and test**

```bash
cd libs/open-androidauto/build && cmake --build . -j$(nproc) && ctest --output-on-failure
```

**Step 5: Commit**

```bash
git add libs/open-androidauto/
git commit -m "feat(oaa): expose native socket descriptor for connection watchdog"
```

---

### Task 11: Add ChannelId Constants

Create a header with the channel ID constants so app code doesn't need magic numbers.

**Files:**
- Create: `libs/open-androidauto/include/oaa/Channel/ChannelId.hpp`
- Modify: `libs/open-androidauto/CMakeLists.txt`

**Step 1: Create header**

```cpp
#pragma once
#include <cstdint>

namespace oaa {
namespace ChannelId {
    constexpr uint8_t CONTROL       = 0;
    constexpr uint8_t INPUT         = 1;
    constexpr uint8_t SENSOR        = 2;
    constexpr uint8_t VIDEO         = 3;
    constexpr uint8_t MEDIA_AUDIO   = 4;
    constexpr uint8_t SPEECH_AUDIO  = 5;
    constexpr uint8_t SYSTEM_AUDIO  = 6;
    constexpr uint8_t AV_INPUT      = 7;
    constexpr uint8_t BLUETOOTH     = 8;
    constexpr uint8_t WIFI          = 14;
} // namespace ChannelId
} // namespace oaa
```

**Step 2: Add to CMakeLists.txt and commit**

```bash
git add libs/open-androidauto/
git commit -m "feat(oaa): add ChannelId constants header"
```

---

## Phase 2: Create Feature Branch and Scaffold

### Task 12: Create Feature Branch

**Step 1: Branch from main**

```bash
git checkout -b feature/oaa-integration
```

**Step 2: Verify clean build**

```bash
mkdir -p build && cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure
```

---

## Phase 3: Non-AV Channel Handlers

These are the simplest channels — no media data, just request/response message patterns.

### Task 13: SensorChannelHandler

Replaces `SensorServiceStub` from `ServiceFactory.cpp:336-495`. Handles night mode, driving status, GPS, compass, accelerometer, gyroscope.

**Files:**
- Create: `src/core/aa/SensorChannelHandler.hpp`
- Create: `src/core/aa/SensorChannelHandler.cpp`

**Step 1: Create SensorChannelHandler**

Implements `oaa::IChannelHandler`. Key methods:
- `channelId()` → returns `oaa::ChannelId::SENSOR` (2)
- `fillChannelDescriptor()` → adds NIGHT_DATA, DRIVING_STATUS, LOCATION, COMPASS, ACCEL, GYRO sensors
- `onMessage()` → switch on messageId:
  - `0x8001` (SENSOR_START_REQUEST) → parse `SensorStartRequestMessage`, send `SensorStartResponseMessage`, then send initial sensor data
  - Unknown → emit `unknownMessage`
- `onChannelOpened()` → log, connect to NightModeProvider if present
- `onChannelClosed()` → disconnect NightModeProvider

Sends via `emit sendRequested(channelId(), messageId, serializedPayload)`.

**Step 2: Port sensor data sending logic**

Port `sendInitialSensorData()` and `sendNightModeUpdate()` from the old stub. Use `oaa::proto::` namespace instead of `aasdk::proto::`.

**Step 3: Build**

```bash
cd build && cmake --build . -j$(nproc)
```

**Step 4: Commit**

```bash
git add src/core/aa/SensorChannelHandler.*
git commit -m "feat(aa): add SensorChannelHandler using oaa::IChannelHandler"
```

---

### Task 14: InputChannelHandler

Replaces `InputServiceStub` from `ServiceFactory.cpp:223-331`. Handles touch input binding and provides a send interface for `TouchHandler`.

**Files:**
- Create: `src/core/aa/InputChannelHandler.hpp`
- Create: `src/core/aa/InputChannelHandler.cpp`

**Step 1: Create InputChannelHandler**

Implements `oaa::IChannelHandler`. Key methods:
- `channelId()` → returns `oaa::ChannelId::INPUT` (1)
- `fillChannelDescriptor()` → sets `touch_screen_config` (width/height with sidebar margin logic), `supported_keycodes` (HOME=3, BACK=4, MIC=84)
- `onMessage()`:
  - `0x8002` (BINDING_REQUEST) → send BindingResponse with OK
  - Unknown → emit `unknownMessage`
- `sendInputEvent(const QByteArray& serializedIndication)` → public method for TouchHandler to call; emits `sendRequested(1, 0x8001, payload)`

**Step 2: Build and commit**

```bash
git add src/core/aa/InputChannelHandler.*
git commit -m "feat(aa): add InputChannelHandler using oaa::IChannelHandler"
```

---

### Task 15: BluetoothChannelHandler

Replaces `BluetoothServiceStub` from `ServiceFactory.cpp:500-557`. Simple — just handles pairing requests.

**Files:**
- Create: `src/core/aa/BluetoothChannelHandler.hpp`
- Create: `src/core/aa/BluetoothChannelHandler.cpp`

**Step 1: Create BluetoothChannelHandler**

- `channelId()` → `oaa::ChannelId::BLUETOOTH` (8)
- `fillChannelDescriptor()` → sets adapter_address, supported_pairing_methods (HFP)
- `onMessage()`:
  - `0x8001` (PAIRING_REQUEST) → log only (current behavior)
  - Unknown → emit `unknownMessage`

**Step 2: Build and commit**

```bash
git add src/core/aa/BluetoothChannelHandler.*
git commit -m "feat(aa): add BluetoothChannelHandler using oaa::IChannelHandler"
```

---

### Task 16: WifiChannelHandler

Replaces `WIFIServiceStub` from `ServiceFactory.cpp:559-634`. Sends WiFi credentials when phone requests them.

**Files:**
- Create: `src/core/aa/WifiChannelHandler.hpp`
- Create: `src/core/aa/WifiChannelHandler.cpp`

**Step 1: Create WifiChannelHandler**

- `channelId()` → `oaa::ChannelId::WIFI` (14)
- `fillChannelDescriptor()` → sets ssid from config
- `onMessage()`:
  - `0x8001` (CREDENTIALS_REQUEST) → send WifiSecurityResponse with SSID, key, security_mode, access_point_type

**Step 2: Build and commit**

```bash
git add src/core/aa/WifiChannelHandler.*
git commit -m "feat(aa): add WifiChannelHandler using oaa::IChannelHandler"
```

---

## Phase 4: AV Channel Handlers

These channels have a setup phase (SETUP_REQUEST → SETUP_RESPONSE, START/STOP) plus media data flow.

### Task 17: AudioChannelHandler (Template)

Replaces the templated `AudioServiceStub` from `ServiceFactory.cpp:79-218`. Handles media, speech, and system audio via template parameters.

**Files:**
- Create: `src/core/aa/AudioChannelHandler.hpp`
- Create: `src/core/aa/AudioChannelHandler.cpp`

**Step 1: Create AudioChannelHandler**

Implements `oaa::IAVChannelHandler`. Template parameters: channel ID, audio type enum, sample rate, channel count.

- `channelId()` → template param
- `fillChannelDescriptor()` → AV channel config with AUDIO stream type, audio type, sample rate, bit depth, channel count
- `onMessage()`:
  - `0x8000` (SETUP_REQUEST) → send AVChannelSetupResponse with OK, max_unacked=1, configs(0)
  - `0x8001` (START_INDICATION) → store session, start audio stream
  - `0x8002` (STOP_INDICATION) → stop audio stream
  - `0x8004` (AV_MEDIA_ACK_INDICATION) → ignore (we're receiver)
- `onMediaData()` → write PCM data to PipeWire stream, send AVMediaAckIndication
- `canAcceptMedia()` → return true (no backpressure for now)

**Step 2: Create type aliases**

```cpp
using MediaAudioHandler = AudioChannelHandler<oaa::ChannelId::MEDIA_AUDIO,
    oaa::proto::enums::AudioType::MEDIA, 48000, 2>;
using SpeechAudioHandler = AudioChannelHandler<oaa::ChannelId::SPEECH_AUDIO,
    oaa::proto::enums::AudioType::SPEECH, 48000, 1>;
using SystemAudioHandler = AudioChannelHandler<oaa::ChannelId::SYSTEM_AUDIO,
    oaa::proto::enums::AudioType::SYSTEM, 16000, 1>;
```

**Step 3: Build and commit**

```bash
git add src/core/aa/AudioChannelHandler.*
git commit -m "feat(aa): add AudioChannelHandler template using oaa::IAVChannelHandler"
```

---

### Task 18: AVInputChannelHandler (Microphone)

Replaces `AVInputServiceStub` from `ServiceFactory.cpp:639-783`. Sends captured audio from PipeWire to the phone.

**Files:**
- Create: `src/core/aa/AVInputChannelHandler.hpp`
- Create: `src/core/aa/AVInputChannelHandler.cpp`

**Step 1: Create AVInputChannelHandler**

Implements `oaa::IAVChannelHandler`:
- `channelId()` → `oaa::ChannelId::AV_INPUT` (7)
- `fillChannelDescriptor()` → AV input config (AUDIO, 16kHz, 16-bit, mono)
- `onMessage()`:
  - `0x8000` (SETUP_REQUEST) → send setup response
  - `0x8005` (AV_INPUT_OPEN_REQUEST) → open/close PipeWire capture, send AVInputOpenResponse
  - `0x8004` (AV_MEDIA_ACK_INDICATION) → phone ACKed our mic data
- `onMediaData()` → not used (we're the sender, not receiver)
- `sendMicData()` → serialize with timestamp, emit `sendRequested`

Port PipeWire capture callback from old stub — copies data, queues for send.

**Step 2: Build and commit**

```bash
git add src/core/aa/AVInputChannelHandler.*
git commit -m "feat(aa): add AVInputChannelHandler for microphone capture"
```

---

### Task 19: VideoChannelHandler

Replaces `VideoService.hpp/cpp`. Most complex handler — H.264 video reception, video focus management, decode pipeline integration.

**Files:**
- Create: `src/core/aa/VideoChannelHandler.hpp`
- Create: `src/core/aa/VideoChannelHandler.cpp`
- Delete: `src/core/aa/VideoService.hpp` (after migration)
- Delete: `src/core/aa/VideoService.cpp` (after migration)

**Step 1: Create VideoChannelHandler**

Implements `oaa::IAVChannelHandler`:
- `channelId()` → `oaa::ChannelId::VIDEO` (3)
- `fillChannelDescriptor()` → video configs (480p, 720p, 1080p at 30fps) with margin calculation from YamlConfig
- `onMessage()`:
  - `0x8000` (SETUP_REQUEST) → send setup response with configs(0)
  - `0x8001` (START_INDICATION) → store session
  - `0x8002` (STOP_INDICATION) → log
  - `0x8007` (VIDEO_FOCUS_REQUEST) → handle focus logic (suppression, exit-to-car), send VideoFocusIndication
- `onMediaData()` → forward H.264 data to VideoDecoder, send ACK
- `canAcceptMedia()` → true (future: check decoder queue depth)

Port video focus management (setVideoFocus, setFocusSuppressed) and the videoFocusChanged signal.

**Step 2: Build and commit**

```bash
git add src/core/aa/VideoChannelHandler.*
git commit -m "feat(aa): add VideoChannelHandler using oaa::IAVChannelHandler"
```

---

## Phase 5: Adapt TouchHandler

### Task 20: Port TouchHandler from aasdk to oaa

**Files:**
- Modify: `src/core/aa/TouchHandler.hpp`

**Step 1: Replace aasdk dependencies**

Remove:
- `#include <aasdk/Channel/Input/InputServiceChannel.hpp>`
- `#include <aasdk/Channel/Promise.hpp>`
- `#include <aasdk_proto/InputEventIndicationMessage.pb.h>` etc.
- `std::shared_ptr<aasdk::channel::input::InputServiceChannel> channel_`
- `boost::asio::io_service::strand* strand_`

Add:
- `#include "InputChannelHandler.hpp"` (or just a forward decl + pointer)
- Use `oaa::proto::messages::InputEventIndication` etc.

Replace `setChannel(...)` with `setInputHandler(InputChannelHandler* handler)`.

Replace `sendTouchIndication()` body:
- Build `InputEventIndication` protobuf using `oaa::proto::` namespace
- Serialize to QByteArray
- Call `handler_->sendInputEvent(serialized)`

No more strand dispatch — InputChannelHandler's `sendRequested` signal handles thread safety.

**Step 2: Build and commit**

```bash
git add src/core/aa/TouchHandler.hpp
git commit -m "refactor(aa): port TouchHandler from aasdk to oaa"
```

---

## Phase 6: Rewrite AndroidAutoService

This is the big one — replacing the core orchestration.

### Task 21: Rewrite AndroidAutoService

**Files:**
- Rewrite: `src/core/aa/AndroidAutoService.hpp`
- Rewrite: `src/core/aa/AndroidAutoService.cpp`
- Delete: `src/core/aa/AndroidAutoEntity.hpp`
- Delete: `src/core/aa/AndroidAutoEntity.cpp`
- Delete: `src/core/aa/ServiceFactory.hpp`
- Delete: `src/core/aa/ServiceFactory.cpp`
- Delete: `src/core/aa/IService.hpp`

**Step 1: Replace header**

Remove all aasdk and Boost.ASIO includes. Remove IAndroidAutoEntityEventHandler. New structure:

```cpp
#include <QObject>
#include <QTcpServer>
#include <QThread>
#include <oaa/Session/AASession.hpp>
#include <oaa/Transport/TCPTransport.hpp>

class AndroidAutoService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int connectionState ...)
    Q_PROPERTY(QString statusMessage ...)
public:
    explicit AndroidAutoService(...);
    ~AndroidAutoService();
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    // ...
private:
    void onNewConnection();
    void onSessionStateChanged(oaa::SessionState state);
    void onSessionDisconnected(oaa::DisconnectReason reason);
    void onAudioFocusRequested(const QByteArray& payload);
    void onNavigationFocusRequested(const QByteArray& payload);
    void createSession(QTcpSocket* socket);
    void destroySession();

    QTcpServer* tcpServer_ = nullptr;
    QThread* sessionThread_ = nullptr;
    oaa::AASession* session_ = nullptr;
    oaa::TCPTransport* transport_ = nullptr;
    // Channel handlers (owned by session thread)
    VideoChannelHandler* videoHandler_ = nullptr;
    // ... other handlers ...
};
```

**Step 2: Implement TCP listener with QTcpServer**

```cpp
void AndroidAutoService::start() {
    tcpServer_ = new QTcpServer(this);
    connect(tcpServer_, &QTcpServer::newConnection, this, &AndroidAutoService::onNewConnection);
    // Listen on configured port (default 5277)
    tcpServer_->listen(QHostAddress::Any, config_->tcpPort());
    setState(WaitingForDevice);
}
```

**Step 3: Implement session creation**

On `onNewConnection()`:
1. Take socket from `tcpServer_->nextPendingConnection()`
2. Create `QThread` for protocol work
3. Create `oaa::TCPTransport` wrapping the socket
4. Create `oaa::AASession` with transport + config
5. Create all channel handlers, register with session
6. Move transport, session, and all handlers to the QThread
7. Connect session signals (stateChanged, disconnected, audioFocus, navFocus)
8. Start session thread and call `session->start()`

**Step 4: Handle session state changes**

Map `oaa::SessionState` to the existing `ConnectionState` Q_ENUM:
- `Connecting/VersionExchange/TLSHandshake` → `Connecting`
- `ServiceDiscovery/Active` → `Connected`
- `Disconnected` → `Disconnected` (restart listener)

**Step 5: Handle audio/navigation focus**

Port the logic from `AndroidAutoEntity::onAudioFocusRequest()` and `onNavigationFocusRequest()`:
- Audio focus: grant what phone requests, send response via `session_->sendAudioFocusResponse()`
- Navigation focus: detect exit-to-car (type != 1), emit signal for plugin to handle

**Step 6: Port connection watchdog**

Use `transport_->nativeSocketDescriptor()` for `tcp_info` polling. Same `QTimer` + `getsockopt` logic as before.

**Step 7: Port SO_REUSEADDR and FD_CLOEXEC**

`QTcpServer` handles SO_REUSEADDR automatically. For FD_CLOEXEC, set it on the server socket after `listen()`.

**Step 8: Build and verify compilation**

This won't be testable until CMake is updated (next task), but verify the file compiles.

**Step 9: Commit**

```bash
git add src/core/aa/
git commit -m "feat(aa): rewrite AndroidAutoService with oaa::AASession and QTcpServer"
```

---

## Phase 7: CMake and Cleanup

### Task 22: Update CMakeLists.txt

**Files:**
- Modify: `CMakeLists.txt` (top-level)
- Modify: `src/CMakeLists.txt`

**Step 1: Remove aasdk from top-level CMakeLists.txt**

Remove `add_subdirectory(libs/aasdk)` and the CXX_FLAGS save/restore around it.

**Step 2: Update src/CMakeLists.txt**

- Remove `aasdk` and `aasdk_proto` from `target_link_libraries`
- Remove aasdk include directories from `target_include_directories`
- Add `open-androidauto` to `target_link_libraries` (if not already)
- Add new source files (all *ChannelHandler.cpp)
- Remove deleted source files (AndroidAutoEntity.cpp, ServiceFactory.cpp, IService.hpp, VideoService.cpp)

**Step 3: Full build**

```bash
cd build && cmake .. && cmake --build . -j$(nproc)
```

Fix any compilation errors from missed namespace changes, missing includes, etc. This is where the bulk of the integration debugging happens.

**Step 4: Commit**

```bash
git add CMakeLists.txt src/CMakeLists.txt
git commit -m "build: replace aasdk with open-androidauto in CMake"
```

---

### Task 23: Remove aasdk Submodule

Only do this after everything compiles and tests pass.

**Files:**
- Remove: `libs/aasdk/` (git submodule)
- Modify: `.gitmodules`

**Step 1: Remove submodule**

```bash
git submodule deinit libs/aasdk
git rm libs/aasdk
rm -rf .git/modules/libs/aasdk
```

**Step 2: Remove Boost.ASIO dependency if unused**

Check if anything else in the project uses Boost. If not, remove from CMake.

**Step 3: Commit**

```bash
git add -A
git commit -m "chore: remove aasdk submodule, replaced by open-androidauto"
```

---

## Phase 8: Testing and Validation

### Task 24: Dev VM Build Verification

```bash
cd build && cmake .. && cmake --build . -j$(nproc)
ctest --output-on-failure  # app-level tests
cd libs/open-androidauto/build && ctest --output-on-failure  # library tests
```

All tests must pass on Qt 6.4 (Ubuntu 24.04).

### Task 25: Pi Build and Smoke Test

**Step 1: Rsync source to Pi**

```bash
rsync -avz --exclude='build/' --exclude='.git/' \
  /home/matt/claude/personal/openautopro/openauto-prodigy/ \
  matt@192.168.1.149:/home/matt/openauto-prodigy/
```

**Step 2: Build on Pi**

```bash
ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy/build && cmake .. && cmake --build . -j3"
```

**Step 3: Launch and test with phone**

```bash
ssh matt@192.168.1.149 'cd /home/matt/openauto-prodigy && \
  nohup env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 \
  ./build/src/openauto-prodigy > /tmp/oap.log 2>&1 &'
```

Verify:
- [ ] App launches without crash
- [ ] BT discovery works
- [ ] Phone connects via WiFi
- [ ] Video displays
- [ ] Touch works
- [ ] Audio plays (media, navigation, system)
- [ ] Night mode sensor works
- [ ] Graceful shutdown works
- [ ] Reconnection works

**Step 4: Check logs for errors**

```bash
ssh matt@192.168.1.149 "cat /tmp/oap.log | grep -i 'error\|warning\|crash'"
```

---

## Summary

| Phase | Tasks | Description |
|-------|-------|-------------|
| 0 | 1–7 | **Protocol compatibility hardening** — expand enums, add message IDs from APK decompilation |
| 1 | 8–11 | Library parity gaps (descriptor API, control signals, native FD, channel IDs) |
| 2 | 12 | Create feature branch |
| 3 | 13–16 | Non-AV channel handlers (sensor, input, BT, WiFi) |
| 4 | 17–19 | AV channel handlers (audio, mic, video) |
| 5 | 20 | Port TouchHandler |
| 6 | 21 | Rewrite AndroidAutoService |
| 7 | 22–23 | CMake update, remove aasdk submodule |
| 8 | 24–25 | Build verification and Pi smoke test |
