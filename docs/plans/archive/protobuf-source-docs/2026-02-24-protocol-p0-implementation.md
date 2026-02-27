# Protocol P0 — Fix Proven Gaps Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Fix all enum/message gaps in existing working channels — things that happen during real AA sessions and affect behavior today.

**Architecture:** Modify existing proto files, C++ message ID constants, and handler code. No new handlers, no new channels. All changes are in `libs/open-androidauto/`.

**Tech Stack:** protobuf (proto2), C++17, Qt 6 (QTest), CMake

---

### Task 1: Expand VideoFocusMode Enum

The phone sends focus values 1-4 (PROJECTED, NATIVE, NATIVE_TRANSIENT, PROJECTED_NO_INPUT_FOCUS) but our enum only has NONE(0), FOCUSED(1), UNFOCUSED(2). The cross-reference confirms values 1-4 from both Sony firmware and APK (`vee.java`).

**Files:**
- Modify: `libs/open-androidauto/proto/VideoFocusModeEnum.proto`
- Modify: `libs/open-androidauto/proto/VideoFocusReasonEnum.proto`
- Test: `tests/test_video_channel_handler.cpp`

**Step 1: Write failing test for new focus modes**

Add to `tests/test_video_channel_handler.cpp`:

```cpp
void testVideoFocusProjectedMode() {
    oaa::hu::VideoChannelHandler handler;
    QSignalSpy focusSpy(&handler, &oaa::hu::VideoChannelHandler::videoFocusChanged);
    handler.onChannelOpened();

    oaa::proto::messages::VideoFocusIndication indication;
    indication.set_focus_mode(oaa::proto::enums::VideoFocusMode::PROJECTED);
    indication.set_unrequested(false);
    QByteArray payload(indication.ByteSizeLong(), '\0');
    indication.SerializeToArray(payload.data(), payload.size());

    handler.onMessage(oaa::AVMessageId::VIDEO_FOCUS_INDICATION, payload);

    QCOMPARE(focusSpy.count(), 1);
    QCOMPARE(focusSpy[0][0].toInt(), 1); // PROJECTED = 1
}

void testVideoFocusNativeMode() {
    oaa::hu::VideoChannelHandler handler;
    QSignalSpy focusSpy(&handler, &oaa::hu::VideoChannelHandler::videoFocusChanged);
    handler.onChannelOpened();

    oaa::proto::messages::VideoFocusIndication indication;
    indication.set_focus_mode(oaa::proto::enums::VideoFocusMode::NATIVE);
    indication.set_unrequested(true);
    QByteArray payload(indication.ByteSizeLong(), '\0');
    indication.SerializeToArray(payload.data(), payload.size());

    handler.onMessage(oaa::AVMessageId::VIDEO_FOCUS_INDICATION, payload);

    QCOMPARE(focusSpy.count(), 1);
    QCOMPARE(focusSpy[0][0].toInt(), 2); // NATIVE = 2
    QCOMPARE(focusSpy[0][1].toBool(), true); // unrequested
}
```

**Step 2: Run tests to verify they fail**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) 2>&1 | tail -20`
Expected: Compile error — `VideoFocusMode::PROJECTED` and `VideoFocusMode::NATIVE` not defined.

**Step 3: Update VideoFocusModeEnum.proto**

Replace the enum in `libs/open-androidauto/proto/VideoFocusModeEnum.proto` (lines 25-30):

```protobuf
    enum Enum
    {
        NONE = 0;
        PROJECTED = 1;
        NATIVE = 2;
        NATIVE_TRANSIENT = 3;
        PROJECTED_NO_INPUT_FOCUS = 4;
    }
```

Note: This removes `FOCUSED(1)` and `UNFOCUSED(2)`. `PROJECTED` replaces `FOCUSED` (same wire value 1). `NONE(0)` replaces `UNFOCUSED` semantically.

**Step 4: Update VideoFocusReasonEnum.proto**

Replace the enum in `libs/open-androidauto/proto/VideoFocusReasonEnum.proto` (lines 25-29):

```protobuf
    enum Enum
    {
        UNKNOWN = 0;
        PHONE_SCREEN_OFF = 1;
        LAUNCH_NATIVE = 2;
        LAST_MODE = 3;
        USER_SELECTION = 4;
    }
```

**Step 5: Fix VideoChannelHandler references to FOCUSED → PROJECTED**

In `libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp`:
- Line 90: Change `VideoFocusMode::FOCUSED` → `VideoFocusMode::PROJECTED`
- Line 178: Change `VideoFocusMode::FOCUSED` → `VideoFocusMode::PROJECTED`
- Line 179: Change `VideoFocusMode::UNFOCUSED` → `VideoFocusMode::NONE`

In `tests/test_video_channel_handler.cpp`:
- Any existing reference to `VideoFocusMode::FOCUSED` → `VideoFocusMode::PROJECTED`

**Step 6: Build and run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R test_video_channel_handler --output-on-failure`
Expected: All video handler tests PASS.

**Step 7: Commit**

```bash
git add libs/open-androidauto/proto/VideoFocusModeEnum.proto libs/open-androidauto/proto/VideoFocusReasonEnum.proto libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp tests/test_video_channel_handler.cpp
git commit -m "feat(oaa): expand VideoFocusMode to full protocol spec (PROJECTED/NATIVE/NATIVE_TRANSIENT)"
```

---

### Task 2: Expand ShutdownReason Enum

Currently only NONE(0) and QUIT(1). The phone handles different shutdown reasons differently — POWER_DOWN(7) may trigger cleaner disconnects than USER_SELECTION(1).

**Files:**
- Modify: `libs/open-androidauto/proto/ShutdownReasonEnum.proto`
- Modify: `libs/open-androidauto/src/Session/AASession.cpp` (use proper reason on shutdown)
- Test: (verify build + existing shutdown tests still pass)

**Step 1: Update ShutdownReasonEnum.proto**

Replace the enum (lines 25-29):

```protobuf
    enum Enum
    {
        NONE = 0;
        USER_SELECTION = 1;
        DEVICE_SWITCH = 2;
        NOT_SUPPORTED = 3;
        NOT_CURRENTLY_SUPPORTED = 4;
        PROBE_SUPPORTED = 5;
        WIRELESS_PROJECTION_DISABLED = 6;
        POWER_DOWN = 7;
        USER_PROFILE_SWITCH = 8;
    }
```

Note: `QUIT` is renamed to `USER_SELECTION` (same wire value 1). This matches the APK's `ByeByeReason` enum (`vwe.java`).

**Step 2: Fix AASession shutdown reason**

In `libs/open-androidauto/src/Session/AASession.cpp` line 124:
- Change `sendShutdownRequest(1); // QUIT` to `sendShutdownRequest(1); // USER_SELECTION`

No functional change — same wire value. Just corrects the comment.

**Step 3: Fix any references to ShutdownReason::QUIT**

Search for `ShutdownReason::QUIT` in the codebase. If found in the app code (`src/`), change to `ShutdownReason::USER_SELECTION`.

**Step 4: Build and run all tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass. Rename from QUIT to USER_SELECTION is wire-compatible.

**Step 5: Commit**

```bash
git add libs/open-androidauto/proto/ShutdownReasonEnum.proto libs/open-androidauto/src/Session/AASession.cpp
git commit -m "feat(oaa): expand ShutdownReason enum to full ByeBye protocol spec"
```

---

### Task 3: Expand StatusCode Enum

Currently only OK(0) and FAIL(1). The full enum has 33 values from APK (`vyw.java`). Enables proper error responses in ChannelOpenResponse, AuthComplete, etc.

**Files:**
- Modify: `libs/open-androidauto/proto/StatusEnum.proto`

**Step 1: Update StatusEnum.proto**

Replace the enum (lines 24-31):

```protobuf
message Status
{
    enum Enum
    {
        OK = 0;
        UNSOLICITED_MESSAGE = 1;
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
        CERT_NOT_YET_VALID = -23;
        CERT_EXPIRED = -24;
        PING_TIMEOUT = -25;
        CAR_PROPERTY_SET_FAILED = -26;
        CAR_PROPERTY_LISTENER_FAILED = -27;
        COMMAND_NOT_SUPPORTED = -250;
        FRAMING_ERROR = -251;
        UNEXPECTED_MESSAGE = -253;
        BUSY = -254;
        OUT_OF_MEMORY = -255;
    }
}
```

Note: `FAIL(1)` is renamed to `UNSOLICITED_MESSAGE(1)` to match the APK. Proto2 allows negative enum values.

**Step 2: Fix references to Status::FAIL**

Search the codebase for `Status::FAIL`. Replace with `Status::UNSOLICITED_MESSAGE` or a more specific error code as appropriate. Key locations:
- `ControlChannel.cpp` line 161: `sendChannelOpenResponse` → Use `Status::INVALID_CHANNEL` instead of `Status::FAIL`
- `AuthCompleteIndicationMessage` → Use `Status::AUTHENTICATION_FAILURE` instead of `Status::FAIL`
- Sensor, BT, WiFi handler error paths → Use specific codes where obvious

**Step 3: Build and run all tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass. Grep for any remaining `Status::FAIL` references that need updating.

**Step 4: Commit**

```bash
git add libs/open-androidauto/proto/StatusEnum.proto
git add -u  # any files with FAIL→specific code changes
git commit -m "feat(oaa): expand StatusCode enum to full protocol spec (33 error codes)"
```

---

### Task 4: Add AV Message ID Constants (0x8009-0x8015)

Add the newer AV message IDs to the C++ constants and proto enum. Handlers will log them instead of emitting `unknownMessage`.

**Files:**
- Modify: `libs/open-androidauto/include/oaa/Channel/MessageIds.hpp`
- Modify: `libs/open-androidauto/proto/AVChannelMessageIdsEnum.proto`
- Modify: `libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp`
- Modify: `libs/open-androidauto/src/HU/Handlers/AudioChannelHandler.cpp` (same pattern)
- Test: `tests/test_video_channel_handler.cpp`

**Step 1: Write failing test for unknown AV messages**

Add to `tests/test_video_channel_handler.cpp`:

```cpp
void testNewerAVMessagesLoggedNotUnknown() {
    oaa::hu::VideoChannelHandler handler;
    QSignalSpy unknownSpy(&handler, &oaa::IChannelHandler::unknownMessage);
    handler.onChannelOpened();

    // 0x800C (ACTION_TAKEN) should be logged, not emitted as unknownMessage
    handler.onMessage(0x800C, QByteArray());
    QCOMPARE(unknownSpy.count(), 0);

    // 0x8014 (MEDIA_STATS) should also be logged, not unknown
    handler.onMessage(0x8014, QByteArray());
    QCOMPARE(unknownSpy.count(), 0);
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — currently 0x800C hits the `default:` case and emits `unknownMessage`.

**Step 3: Add constants to MessageIds.hpp**

Add after line 18 in `libs/open-androidauto/include/oaa/Channel/MessageIds.hpp`:

```cpp
    // Newer AV messages (APK v16.1+) — logged but not fully handled yet
    constexpr uint16_t VIDEO_FOCUS_NOTIFICATION       = 0x8009;
    constexpr uint16_t UPDATE_UI_CONFIG_REQUEST        = 0x800A;
    constexpr uint16_t UPDATE_UI_CONFIG_REPLY          = 0x800B;
    constexpr uint16_t AUDIO_UNDERFLOW                 = 0x800C;
    constexpr uint16_t ACTION_TAKEN                    = 0x800D;
    constexpr uint16_t OVERLAY_PARAMETERS              = 0x800E;
    constexpr uint16_t OVERLAY_START                   = 0x800F;
    constexpr uint16_t OVERLAY_STOP                    = 0x8010;
    constexpr uint16_t OVERLAY_SESSION_UPDATE          = 0x8011;
    constexpr uint16_t UPDATE_HU_UI_CONFIG_REQUEST     = 0x8012;
    constexpr uint16_t UPDATE_HU_UI_CONFIG_RESPONSE    = 0x8013;
    constexpr uint16_t MEDIA_STATS                     = 0x8014;
    constexpr uint16_t MEDIA_OPTIONS                   = 0x8015;
```

**Step 4: Add same values to AVChannelMessageIdsEnum.proto**

Add after line 37 in `libs/open-androidauto/proto/AVChannelMessageIdsEnum.proto`:

```protobuf
        VIDEO_FOCUS_NOTIFICATION = 0x8009;
        UPDATE_UI_CONFIG_REQUEST = 0x800A;
        UPDATE_UI_CONFIG_REPLY = 0x800B;
        AUDIO_UNDERFLOW = 0x800C;
        ACTION_TAKEN = 0x800D;
        OVERLAY_PARAMETERS = 0x800E;
        OVERLAY_START = 0x800F;
        OVERLAY_STOP = 0x8010;
        OVERLAY_SESSION_UPDATE = 0x8011;
        UPDATE_HU_UI_CONFIG_REQUEST = 0x8012;
        UPDATE_HU_UI_CONFIG_RESPONSE = 0x8013;
        MEDIA_STATS = 0x8014;
        MEDIA_OPTIONS = 0x8015;
```

**Step 5: Add log-only cases to VideoChannelHandler.cpp**

In `libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp`, add before the `default:` case (line 58):

```cpp
    // Newer AV messages — log and acknowledge, don't emit unknownMessage
    case oaa::AVMessageId::VIDEO_FOCUS_NOTIFICATION:
    case oaa::AVMessageId::UPDATE_UI_CONFIG_REQUEST:
    case oaa::AVMessageId::UPDATE_UI_CONFIG_REPLY:
    case oaa::AVMessageId::AUDIO_UNDERFLOW:
    case oaa::AVMessageId::ACTION_TAKEN:
    case oaa::AVMessageId::OVERLAY_PARAMETERS:
    case oaa::AVMessageId::OVERLAY_START:
    case oaa::AVMessageId::OVERLAY_STOP:
    case oaa::AVMessageId::OVERLAY_SESSION_UPDATE:
    case oaa::AVMessageId::UPDATE_HU_UI_CONFIG_REQUEST:
    case oaa::AVMessageId::UPDATE_HU_UI_CONFIG_RESPONSE:
    case oaa::AVMessageId::MEDIA_STATS:
    case oaa::AVMessageId::MEDIA_OPTIONS:
        qDebug() << "[VideoChannel] received newer AV message:" << Qt::hex << messageId
                 << "size:" << payload.size() << "(not yet handled)";
        break;
```

**Step 6: Same pattern for AudioChannelHandler.cpp**

Add identical log-only cases to the AudioChannelHandler switch. (Read the file first to confirm the switch structure.)

**Step 7: Build and run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass, including the new `testNewerAVMessagesLoggedNotUnknown`.

**Step 8: Commit**

```bash
git add libs/open-androidauto/include/oaa/Channel/MessageIds.hpp libs/open-androidauto/proto/AVChannelMessageIdsEnum.proto libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp libs/open-androidauto/src/HU/Handlers/AudioChannelHandler.cpp tests/test_video_channel_handler.cpp
git commit -m "feat(oaa): add AV message IDs 0x8009-0x8015, log instead of unknownMessage"
```

---

### Task 5: Add Control Channel Message IDs and CALL_AVAILABILITY

Add missing control channel message IDs. Implement `sendCallAvailability(bool)` since commercial HUs all send this.

**Files:**
- Modify: `libs/open-androidauto/src/Channel/ControlChannel.cpp`
- Modify: `libs/open-androidauto/include/oaa/Channel/ControlChannel.hpp`
- Modify: `libs/open-androidauto/proto/ControlMessageIdsEnum.proto`
- Create: `libs/open-androidauto/proto/CallAvailabilityMessage.proto`
- Test: `tests/test_control_channel.cpp` (if exists, otherwise add inline test)

**Step 1: Create CallAvailabilityMessage.proto**

Create `libs/open-androidauto/proto/CallAvailabilityMessage.proto`:

```protobuf
syntax = "proto2";
package oaa.proto.messages;

message CallAvailabilityStatus {
    optional bool call_available = 1;
}
```

**Step 2: Add message IDs to ControlMessageIdsEnum.proto**

Add after line 44 (after AUDIO_FOCUS_RESPONSE):

```protobuf
        CHANNEL_CLOSE_NOTIFICATION = 0x0009;
        NAV_FOCUS_NOTIFICATION = 0x000e;
        CALL_AVAILABILITY_STATUS = 0x0018;
        SERVICE_DISCOVERY_UPDATE = 0x001a;
```

**Step 3: Add constants and handler cases to ControlChannel.cpp**

Add after line 33 (after MSG_AUDIO_FOCUS_REQUEST):

```cpp
constexpr uint16_t MSG_CHANNEL_CLOSE              = 0x0009;
constexpr uint16_t MSG_CALL_AVAILABILITY           = 0x0018;
constexpr uint16_t MSG_SERVICE_DISCOVERY_UPDATE     = 0x001a;
```

Add cases in the switch before `default:` (line 136):

```cpp
    case MSG_CHANNEL_CLOSE:
        qDebug() << "[ControlChannel] channel close notification received";
        break;

    case MSG_CALL_AVAILABILITY:
        qDebug() << "[ControlChannel] call availability status received (unexpected direction)";
        break;

    case MSG_SERVICE_DISCOVERY_UPDATE:
        qDebug() << "[ControlChannel] service discovery update received (unexpected direction)";
        break;
```

**Step 4: Add sendCallAvailability to ControlChannel.hpp**

Add to public methods (after line 28):

```cpp
    void sendCallAvailability(bool available);
```

**Step 5: Implement sendCallAvailability in ControlChannel.cpp**

Add after `sendNavigationFocusResponse` (after line 204):

```cpp
void ControlChannel::sendCallAvailability(bool available) {
    proto::messages::CallAvailabilityStatus msg;
    msg.set_call_available(available);
    QByteArray payload(msg.ByteSizeLong(), '\0');
    msg.SerializeToArray(payload.data(), payload.size());
    emit sendRequested(0, 0x0018, payload);
}
```

Add include at top: `#include "CallAvailabilityMessage.pb.h"`

**Step 6: Build and run all tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass.

**Step 7: Commit**

```bash
git add libs/open-androidauto/proto/CallAvailabilityMessage.proto libs/open-androidauto/proto/ControlMessageIdsEnum.proto libs/open-androidauto/src/Channel/ControlChannel.cpp libs/open-androidauto/include/oaa/Channel/ControlChannel.hpp
git commit -m "feat(oaa): add control channel message IDs and sendCallAvailability"
```

---

### Task 6: Add Parking Brake Sensor Push

Sony firmware enables parking brake as one of 3 mandatory sensors. Without it, AA may restrict features while "driving."

**Files:**
- Modify: `libs/open-androidauto/include/oaa/HU/Handlers/SensorChannelHandler.hpp`
- Modify: `libs/open-androidauto/src/HU/Handlers/SensorChannelHandler.cpp`
- Test: `tests/test_sensor_channel_handler.cpp`

**Step 1: Write failing test**

Add to `tests/test_sensor_channel_handler.cpp`:

```cpp
void testParkingBrakeUpdate() {
    oaa::hu::SensorChannelHandler handler;
    handler.onChannelOpened();

    // Subscribe to PARKING_BRAKE first
    oaa::proto::messages::SensorStartRequestMessage req;
    req.set_sensor_type(oaa::proto::enums::SensorType::PARKING_BRAKE);
    req.set_refresh_interval(1000);
    QByteArray payload(req.ByteSizeLong(), '\0');
    req.SerializeToArray(payload.data(), payload.size());
    handler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);

    QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
    handler.pushParkingBrake(true);

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Sensor);
    QCOMPARE(sendSpy[0][1].value<uint16_t>(),
             static_cast<uint16_t>(oaa::SensorMessageId::SENSOR_EVENT_INDICATION));
}

void testParkingBrakeNotSentWithoutSubscription() {
    oaa::hu::SensorChannelHandler handler;
    QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

    handler.onChannelOpened();
    handler.pushParkingBrake(true);
    QCOMPARE(sendSpy.count(), 0);
}
```

**Step 2: Run test to verify it fails**

Expected: Compile error — `pushParkingBrake` not declared.

**Step 3: Add declaration to SensorChannelHandler.hpp**

Add after line 22 (after `pushDrivingStatus`):

```cpp
    void pushParkingBrake(bool engaged);
```

**Step 4: Implement pushParkingBrake in SensorChannelHandler.cpp**

Add after `pushDrivingStatus` (after line 105), and add `#include "ParkingBrakeData.pb.h"` at top:

```cpp
void SensorChannelHandler::pushParkingBrake(bool engaged)
{
    if (!channelOpen_ || !activeSensors_.contains(
            static_cast<int>(oaa::proto::enums::SensorType::PARKING_BRAKE)))
        return;

    oaa::proto::messages::SensorEventIndication indication;
    auto* brake = indication.add_parking_brake();
    brake->set_parking_brake(engaged);

    QByteArray data(indication.ByteSizeLong(), '\0');
    indication.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::SensorMessageId::SENSOR_EVENT_INDICATION, data);
}
```

**Step 5: Add initial data for parking brake in handleSensorStartRequest**

In `SensorChannelHandler.cpp`, add after line 73 (the driving status initial push):

```cpp
    } else if (sensorType == oaa::proto::enums::SensorType::PARKING_BRAKE) {
        pushParkingBrake(true); // Default: parked (safe default)
```

**Step 6: Build and run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R test_sensor_channel_handler --output-on-failure`
Expected: All sensor tests pass.

**Step 7: Commit**

```bash
git add libs/open-androidauto/include/oaa/HU/Handlers/SensorChannelHandler.hpp libs/open-androidauto/src/HU/Handlers/SensorChannelHandler.cpp tests/test_sensor_channel_handler.cpp
git commit -m "feat(oaa): add pushParkingBrake sensor (mandatory per Sony firmware spec)"
```

---

### Task 7: Add Missing Tire Sensor Field to SensorEventIndication

The `SensorEventIndicationMessage.proto` is missing field 18 (tire data) despite `TIRE = 18` existing in the enum. Field numbers jump from 17 (light) to 19 (accel).

**Files:**
- Modify: `libs/open-androidauto/proto/SensorEventIndicationMessage.proto`
- Check: `libs/open-androidauto/proto/` for a TireData.proto (may need to create)

**Step 1: Check if TireData.proto exists**

Look for a tire pressure proto file. If it doesn't exist, create `libs/open-androidauto/proto/TirePressureData.proto`:

```protobuf
syntax="proto2";
package oaa.proto.data;

message TirePressure
{
    optional int32 front_left = 1;
    optional int32 front_right = 2;
    optional int32 rear_left = 3;
    optional int32 rear_right = 4;
}
```

**Step 2: Add field 18 to SensorEventIndicationMessage.proto**

Add import and field between light (17) and accel (19):

```protobuf
import "TirePressureData.proto";
```

And add between lines 61 and 62:

```protobuf
    repeated data.TirePressure tire_pressure = 18;
```

**Step 3: Build to verify proto compiles**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean build.

**Step 4: Commit**

```bash
git add libs/open-androidauto/proto/TirePressureData.proto libs/open-androidauto/proto/SensorEventIndicationMessage.proto
git commit -m "fix(oaa): add missing tire pressure field 18 to SensorEventIndication"
```

---

### Task 8: Add MediaCodec Enum Values (H.265, VP9, AV1)

Modern phones support newer video codecs. Pi 4 has H.265 HW decode. Adding enum values doesn't change behavior — just enables future negotiation.

**Files:**
- Modify: Find the video codec enum proto file (likely in VideoConfigData.proto or a separate codec enum)

**Step 1: Find and update the codec enum**

Search proto files for codec definitions. If a codec enum exists, add:

```protobuf
    VIDEO_VP9 = 5;
    VIDEO_AV1 = 6;
    VIDEO_H265 = 7;
```

If there's a separate `AudioCodecEnum.proto`, ensure PCM(1), AAC_LC(2), AAC_LC_ADTS(4) are present.

**Step 2: Build**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean build.

**Step 3: Commit**

```bash
git add libs/open-androidauto/proto/*Codec* libs/open-androidauto/proto/*Video*
git commit -m "feat(oaa): add H.265, VP9, AV1 codec enum values for future negotiation"
```

---

### Task 9: Save APK Deep Dive Reference

Save the proto definitions extracted from the APK as a reference document for future P1/P2 work. This is the evidence archive that gates future service implementation.

**Files:**
- Create: `docs/apk-proto-reference.md`

**Step 1: Write reference document**

Compile findings from the two APK deep dive agents into a structured reference covering:
- All 21 service types with APK handler classes
- Proto definitions for services 10-21 (field numbers, types, names)
- Complete enum cross-references (StatusCode, ByeByeReason, CallState, MediaPlaybackState, etc.)
- Source file mapping (APK class → proto message name)

**Step 2: Commit**

```bash
git add docs/apk-proto-reference.md
git commit -m "docs: add APK v16.1 proto reference for future P1/P2 service implementation"
```

---

### Task 10: Full Build + Test Verification

Final verification that all changes work together.

**Step 1: Clean build**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake .. && cmake --build . -j$(nproc)
```

**Step 2: Run all tests**

```bash
ctest --output-on-failure
```

Expected: All tests pass (43+ tests).

**Step 3: Check for stale references**

```bash
grep -r "VideoFocusMode::FOCUSED\|VideoFocusMode::UNFOCUSED\|Status::FAIL\|ShutdownReason::QUIT" libs/ src/ tests/ --include="*.cpp" --include="*.hpp"
```

Expected: No matches (all renamed to new enum values).

**Step 4: Cross-compile for Pi**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
docker run --rm -u "$(id -u):$(id -g)" -v "$(pwd):/src:rw" -w /src/build-pi openauto-cross-pi4 cmake --build . -j$(nproc)
```

Expected: Clean cross-compile.
