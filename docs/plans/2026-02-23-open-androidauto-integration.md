# open-androidauto Integration Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace aasdk with open-androidauto as the sole AA protocol library, eliminating Boost.ASIO and moving to a fully Qt-native protocol stack.

**Architecture:** All AA protocol work runs on a dedicated QThread (not the UI thread). open-androidauto's library handles transport, TLS, session FSM, message framing, and control channel. Prodigy implements all 8 service channel handlers (Video, Audio×3, Input, Sensor, Bluetooth, WiFi, AVInput) as `IChannelHandler`/`IAVChannelHandler` subclasses in `src/core/aa/`. Channel handlers communicate with the UI/audio layer via Qt signals/slots with `Qt::QueuedConnection`.

**Tech Stack:** Qt 6 (Core, Network, Multimedia), OpenSSL, Protobuf, FFmpeg (libavcodec/libavutil), PipeWire

**Branch:** `feature/open-androidauto-integration` off `main`

**Execution:** Subagent-driven in current session. One commit per task. PR to main when Phase 6 validation passes on Pi.

**Codex Review Gates:** Use Codex (via MCP) for code review at these checkpoints:
- **After Phase 1 (Task 4):** Review SessionConfig extensions, ServiceDiscoveryBuilder, and channel/message ID constants. Verify proto usage and config builder correctness.
- **After Phase 2 (Task 9):** Review all non-AV channel handlers and integration test. Check handler pattern consistency, thread safety, signal/slot usage.
- **After Phase 3 (Task 12):** Review AV handlers (audio + video + mic). Focus on media data flow, ACK handling, video focus logic, and IAVChannelHandler usage.
- **After Phase 4 (Task 16):** Review AndroidAutoOrchestrator, TouchHandler refactor, and ProtocolLogger port. Critical review — this is the orchestration layer that replaces ASIO.
- **After Phase 5 (Task 19):** Final review — verify no aasdk remnants, clean Boost removal, Qt logging migration. Check CMakeLists.txt for stale references.

---

## Phase 0: Branch Setup

### Task 0: Create Feature Branch

**Step 1: Create and switch to feature branch**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git checkout main
git checkout -b feature/open-androidauto-integration
```

**Step 2: Verify clean starting point**

```bash
git log --oneline -3
git status
```

Expected: On `feature/open-androidauto-integration`, clean working tree (or only untracked files).

---

## Phase 1: Foundation — Session Boot + Service Discovery

### Task 1: Extend SessionConfig for Service Discovery Content

Currently `SessionConfig` only has head unit identity fields. The session needs to know what channels/capabilities to advertise. Extend it with channel configuration data that Prodigy populates.

**Files:**
- Modify: `libs/open-androidauto/include/oaa/Session/SessionConfig.hpp`
- Test: `libs/open-androidauto/tests/test_session_config.cpp` (new)
- Modify: `libs/open-androidauto/tests/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
// test_session_config.cpp
#include <QTest>
#include <oaa/Session/SessionConfig.hpp>

class TestSessionConfig : public QObject {
    Q_OBJECT
private slots:
    void testDefaultsHaveNoChannels() {
        oaa::SessionConfig config;
        QVERIFY(config.channels.isEmpty());
    }

    void testAddVideoChannel() {
        oaa::SessionConfig config;
        oaa::ChannelConfig ch;
        ch.channelId = 1; // Video
        ch.descriptor = QByteArray("fake-descriptor");
        config.channels.append(ch);
        QCOMPARE(config.channels.size(), 1);
        QCOMPARE(config.channels[0].channelId, static_cast<uint8_t>(1));
    }
};

QTEST_MAIN(TestSessionConfig)
#include "test_session_config.moc"
```

**Step 2: Run test to verify it fails**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make test_session_config && ./libs/open-androidauto/tests/test_session_config`
Expected: FAIL — `ChannelConfig` not defined

**Step 3: Implement SessionConfig extensions**

Add to `SessionConfig.hpp`:

```cpp
#include <QList>
#include <QByteArray>

struct ChannelConfig {
    uint8_t channelId = 0;
    QByteArray descriptor;  // Pre-serialized ChannelDescriptorData protobuf
};

struct SessionConfig {
    // ... existing fields ...

    // Channel capabilities — each entry is a pre-serialized ChannelDescriptor
    // Prodigy builds these from its config; library just inserts them into
    // ServiceDiscoveryResponse.
    QList<ChannelConfig> channels;
};
```

**Step 4: Update AASession::buildServiceDiscoveryResponse to use config.channels**

Modify `libs/open-androidauto/src/Session/AASession.cpp:296-320`:

Replace the current loop that only sets `channel_id` with one that deserializes the full `ChannelDescriptorData`:

```cpp
QByteArray AASession::buildServiceDiscoveryResponse() const {
    proto::messages::ServiceDiscoveryResponse resp;

    resp.set_head_unit_name(config_.headUnitName.toStdString());
    resp.set_car_model(config_.carModel.toStdString());
    resp.set_car_year(config_.carYear.toStdString());
    resp.set_car_serial(config_.carSerial.toStdString());
    resp.set_left_hand_drive_vehicle(config_.leftHandDrive);
    resp.set_headunit_manufacturer(config_.manufacturer.toStdString());
    resp.set_headunit_model(config_.model.toStdString());
    resp.set_sw_build(config_.swBuild.toStdString());
    resp.set_sw_version(config_.swVersion.toStdString());
    resp.set_can_play_native_media_during_vr(config_.canPlayNativeMediaDuringVr);

    // Insert pre-built channel descriptors from config
    for (const auto& ch : config_.channels) {
        auto* desc = resp.add_channels();
        desc->ParseFromArray(ch.descriptor.constData(), ch.descriptor.size());
    }

    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    return data;
}
```

**Step 5: Run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc) && ctest --output-on-failure -R test_session`
Expected: All session tests PASS

**Step 6: Commit**

```bash
git add libs/open-androidauto/include/oaa/Session/SessionConfig.hpp \
        libs/open-androidauto/src/Session/AASession.cpp \
        libs/open-androidauto/tests/test_session_config.cpp \
        libs/open-androidauto/tests/CMakeLists.txt
git commit -m "feat(oaa): extend SessionConfig with channel descriptors for service discovery"
```

---

### Task 2: Build the Service Discovery Config Builder (Prodigy side)

Port all the `fillFeatures()` logic from the current aasdk service stubs into a standalone config builder that produces `SessionConfig` with pre-serialized channel descriptors.

**Files:**
- Create: `src/core/aa/ServiceDiscoveryBuilder.hpp`
- Create: `src/core/aa/ServiceDiscoveryBuilder.cpp`
- Test: `tests/test_service_discovery_builder.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `src/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
// tests/test_service_discovery_builder.cpp
#include <QTest>
#include "core/aa/ServiceDiscoveryBuilder.hpp"

// Use oaa proto headers (not aasdk_proto)
#include "ChannelDescriptorData.pb.h"
#include "ServiceDiscoveryResponseMessage.pb.h"

class TestServiceDiscoveryBuilder : public QObject {
    Q_OBJECT
private slots:
    void testDefaultBuildProducesAllChannels() {
        oap::aa::ServiceDiscoveryBuilder builder;
        // Minimal config — use defaults
        oaa::SessionConfig config = builder.build();

        // Should have 9 channels: video, media, speech, system, input,
        // sensor, bluetooth, wifi, avinput
        QCOMPARE(config.channels.size(), 9);
    }

    void testVideoChannelDescriptor() {
        oap::aa::ServiceDiscoveryBuilder builder;
        auto config = builder.build();

        // Find video channel (id=1)
        bool found = false;
        for (const auto& ch : config.channels) {
            if (ch.channelId == 1) {
                found = true;
                proto::data::ChannelDescriptor desc;
                QVERIFY(desc.ParseFromArray(ch.descriptor.constData(),
                                            ch.descriptor.size()));
                QCOMPARE(desc.channel_id(), 1u);
                QVERIFY(desc.has_av_channel());
                break;
            }
        }
        QVERIFY(found);
    }

    void testSensorChannelHasExpectedTypes() {
        oap::aa::ServiceDiscoveryBuilder builder;
        auto config = builder.build();

        for (const auto& ch : config.channels) {
            if (ch.channelId == 6) { // Sensor
                proto::data::ChannelDescriptor desc;
                desc.ParseFromArray(ch.descriptor.constData(),
                                    ch.descriptor.size());
                QVERIFY(desc.has_sensor_channel());
                // Night, driving, location, compass, accel, gyro = 6 sensors
                QCOMPARE(desc.sensor_channel().sensors_size(), 6);
                return;
            }
        }
        QFAIL("Sensor channel not found");
    }
};

QTEST_MAIN(TestServiceDiscoveryBuilder)
#include "test_service_discovery_builder.moc"
```

**Step 2: Run test to verify it fails**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make test_service_discovery_builder 2>&1 | head -20`
Expected: FAIL — ServiceDiscoveryBuilder.hpp not found

**Step 3: Implement ServiceDiscoveryBuilder**

This class replaces all the `fillFeatures()` methods scattered across the current service stubs. It reads from `YamlConfig` and produces a fully-populated `SessionConfig`.

```cpp
// src/core/aa/ServiceDiscoveryBuilder.hpp
#pragma once

#include <oaa/Session/SessionConfig.hpp>

namespace oap { class YamlConfig; }

namespace oap {
namespace aa {

class ServiceDiscoveryBuilder {
public:
    explicit ServiceDiscoveryBuilder(oap::YamlConfig* yamlConfig = nullptr,
                                     const QString& btMacAddress = "00:00:00:00:00:00");

    oaa::SessionConfig build() const;

private:
    QByteArray buildVideoDescriptor() const;
    QByteArray buildMediaAudioDescriptor() const;
    QByteArray buildSpeechAudioDescriptor() const;
    QByteArray buildSystemAudioDescriptor() const;
    QByteArray buildInputDescriptor() const;
    QByteArray buildSensorDescriptor() const;
    QByteArray buildBluetoothDescriptor() const;
    QByteArray buildWifiDescriptor() const;
    QByteArray buildAVInputDescriptor() const;

    oap::YamlConfig* yamlConfig_ = nullptr;
    QString btMacAddress_;
};

} // namespace aa
} // namespace oap
```

Each `build*Descriptor()` method serializes a `ChannelDescriptorData` protobuf and returns it as `QByteArray`. The logic is ported directly from the current `fillFeatures()` methods in `ServiceFactory.cpp:119-805`.

Key details to preserve from current implementation:
- **Video** (channel 1): Resolution from `yamlConfig->videoResolution()`, 30fps, margin support for sidebar
- **Media audio** (channel 3): 48kHz, 16-bit, 2ch stereo
- **Speech audio** (channel 4): 48kHz, 16-bit, 1ch mono (upgraded from 16kHz per probe findings)
- **System audio** (channel 5): 16kHz, 16-bit, 1ch mono
- **Input** (channel 2): Touch screen config matching content dimensions (sidebar-aware), keycodes HOME/BACK/MICROPHONE
- **Sensor** (channel 6): NIGHT_DATA, DRIVING_STATUS, LOCATION, COMPASS, ACCEL, GYRO
- **Bluetooth** (channel 7): Adapter MAC, HFP pairing method
- **WiFi** (channel 8): SSID from config
- **AVInput** (channel 10): 16kHz, 16-bit, 1ch (microphone)

**Step 4: Run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc) && ctest --output-on-failure -R test_service_discovery`
Expected: PASS

**Step 5: Validate against golden capture**

Write a test that builds a ServiceDiscoveryResponse using the builder and compares the channel set against the baseline protocol capture at `Testing/captures/baseline/pi-protocol.log`.

**Step 6: Commit**

```bash
git add src/core/aa/ServiceDiscoveryBuilder.hpp src/core/aa/ServiceDiscoveryBuilder.cpp \
        tests/test_service_discovery_builder.cpp tests/CMakeLists.txt src/CMakeLists.txt
git commit -m "feat: add ServiceDiscoveryBuilder — ports fillFeatures() to oaa config"
```

---

### Task 3: Add ChannelId Constants to open-androidauto

The library needs channel ID constants matching the AA protocol. Currently aasdk defines these in `aasdk/Messenger/ChannelId.hpp`.

**Files:**
- Create: `libs/open-androidauto/include/oaa/Channel/ChannelId.hpp`
- Modify: `libs/open-androidauto/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
// test in test_session_config.cpp or new test_channel_id.cpp
void testChannelIdConstants() {
    QCOMPARE(oaa::ChannelId::Control, static_cast<uint8_t>(0));
    QCOMPARE(oaa::ChannelId::Video, static_cast<uint8_t>(1));
    QCOMPARE(oaa::ChannelId::Input, static_cast<uint8_t>(2));
    QCOMPARE(oaa::ChannelId::Sensor, static_cast<uint8_t>(6));
}
```

**Step 2: Implement**

```cpp
// libs/open-androidauto/include/oaa/Channel/ChannelId.hpp
#pragma once
#include <cstdint>

namespace oaa {
namespace ChannelId {
    constexpr uint8_t Control      = 0;
    constexpr uint8_t Video        = 1;
    constexpr uint8_t Input        = 2;
    constexpr uint8_t MediaAudio   = 3;
    constexpr uint8_t SpeechAudio  = 4;
    constexpr uint8_t SystemAudio  = 5;
    constexpr uint8_t Sensor       = 6;
    constexpr uint8_t Bluetooth    = 7;
    constexpr uint8_t WiFi         = 8;
    constexpr uint8_t AVInput      = 10;
} // namespace ChannelId
} // namespace oaa
```

**Step 3: Run tests, commit**

```bash
git add libs/open-androidauto/include/oaa/Channel/ChannelId.hpp \
        libs/open-androidauto/CMakeLists.txt
git commit -m "feat(oaa): add ChannelId constants"
```

---

### Task 4: Add Message ID Constants for Service Channels

Each service channel has its own message ID set. Create per-channel message ID headers in the library. These are needed by the Prodigy-side handlers.

**Files:**
- Create: `libs/open-androidauto/include/oaa/Channel/MessageIds.hpp`

**Step 1: Define message IDs**

Based on the proto enums and aasdk source, the key message IDs per channel are:

```cpp
// libs/open-androidauto/include/oaa/Channel/MessageIds.hpp
#pragma once
#include <cstdint>

namespace oaa {

// AV channel messages (Video, Audio, AVInput)
namespace AVMessageId {
    constexpr uint16_t AV_MEDIA_WITH_TIMESTAMP = 0x0000;
    constexpr uint16_t AV_MEDIA_INDICATION     = 0x0001;
    constexpr uint16_t SETUP_REQUEST           = 0x8000;
    constexpr uint16_t SETUP_RESPONSE          = 0x8001;
    constexpr uint16_t START_INDICATION        = 0x8002;
    constexpr uint16_t STOP_INDICATION         = 0x8003;
    constexpr uint16_t ACK_INDICATION          = 0x8004;
    constexpr uint16_t INPUT_OPEN_REQUEST      = 0x8005;
    constexpr uint16_t INPUT_OPEN_RESPONSE     = 0x8006;
    constexpr uint16_t VIDEO_FOCUS_REQUEST     = 0x8007;
    constexpr uint16_t VIDEO_FOCUS_INDICATION  = 0x8008;
}

// Input channel messages
namespace InputMessageId {
    constexpr uint16_t INPUT_EVENT_INDICATION  = 0x8001;
    constexpr uint16_t BINDING_REQUEST         = 0x8002;
    constexpr uint16_t BINDING_RESPONSE        = 0x8003;
}

// Sensor channel messages
namespace SensorMessageId {
    constexpr uint16_t SENSOR_START_REQUEST    = 0x8001;
    constexpr uint16_t SENSOR_START_RESPONSE   = 0x8002;
    constexpr uint16_t SENSOR_EVENT_INDICATION = 0x8003;
}

// Bluetooth channel messages
namespace BluetoothMessageId {
    constexpr uint16_t PAIRING_REQUEST         = 0x8001;
    constexpr uint16_t PAIRING_RESPONSE        = 0x8002;
}

// WiFi channel messages
namespace WiFiMessageId {
    constexpr uint16_t SECURITY_REQUEST        = 0x8001;
    constexpr uint16_t SECURITY_RESPONSE       = 0x8002;
}

} // namespace oaa
```

**Step 2: Commit**

```bash
git add libs/open-androidauto/include/oaa/Channel/MessageIds.hpp
git commit -m "feat(oaa): add per-channel message ID constants"
```

---

## Phase 2: Non-AV Channel Handlers (Prodigy Side)

### Task 5: SensorChannelHandler

Simplest real channel. Receives sensor start requests from phone, pushes night mode/driving status. Validates the full handler pattern.

**Files:**
- Create: `src/core/aa/handlers/SensorChannelHandler.hpp`
- Create: `src/core/aa/handlers/SensorChannelHandler.cpp`
- Test: `tests/test_sensor_channel_handler.cpp`
- Modify: `tests/CMakeLists.txt`, `src/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
// tests/test_sensor_channel_handler.cpp
#include <QTest>
#include <QSignalSpy>
#include "core/aa/handlers/SensorChannelHandler.hpp"
#include <oaa/Channel/ChannelId.hpp>
#include "SensorStartRequestMessage.pb.h"
#include "SensorTypeEnum.pb.h"

class TestSensorChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oap::aa::SensorChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::Sensor);
    }

    void testSensorStartRequestEmitsSend() {
        oap::aa::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        // Simulate a sensor start request for NIGHT_DATA
        proto::messages::SensorStartRequestMessage req;
        req.set_sensor_type(static_cast<int32_t>(
            proto::enums::SensorType::NIGHT_DATA));
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onChannelOpened();
        handler.onMessage(0x8001, payload); // SENSOR_START_REQUEST

        // Should send: SensorStartResponse + SensorEventIndication (initial night mode)
        QVERIFY(sendSpy.count() >= 1);
    }

    void testNightModeUpdate() {
        oap::aa::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();
        handler.pushNightMode(true);

        QCOMPARE(sendSpy.count(), 1);
        // Verify it sent on sensor channel with SENSOR_EVENT_INDICATION
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Sensor);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(), static_cast<uint16_t>(0x8003));
    }
};

QTEST_MAIN(TestSensorChannelHandler)
#include "test_sensor_channel_handler.moc"
```

**Step 2: Run test to verify it fails**

Expected: FAIL — SensorChannelHandler.hpp not found

**Step 3: Implement SensorChannelHandler**

```cpp
// src/core/aa/handlers/SensorChannelHandler.hpp
#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>

namespace oap {
namespace aa {

class SensorChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit SensorChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::Sensor; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload) override;

    // Push sensor data — called from external providers (e.g. NightModeProvider)
    void pushNightMode(bool isNight);
    void pushDrivingStatus(int status);

private:
    void handleSensorStartRequest(const QByteArray& payload);
    void sendSensorEvent(const QByteArray& serializedIndication);

    bool channelOpen_ = false;
};

} // namespace aa
} // namespace oap
```

Implementation in `.cpp`:
- `onMessage()` dispatches by message ID using constants from `MessageIds.hpp`
- `handleSensorStartRequest()` parses `SensorStartRequestMessage`, sends `SensorStartResponseMessage` (OK), then sends initial data for that sensor type
- `pushNightMode()` / `pushDrivingStatus()` build `SensorEventIndication` and emit via `sendRequested` signal
- All proto serialization uses open-androidauto's proto headers (same .proto files, different build target)

**Step 4: Run tests**

Run: `ctest --output-on-failure -R test_sensor_channel`
Expected: PASS

**Step 5: Commit**

```bash
git add src/core/aa/handlers/SensorChannelHandler.hpp \
        src/core/aa/handlers/SensorChannelHandler.cpp \
        tests/test_sensor_channel_handler.cpp \
        tests/CMakeLists.txt src/CMakeLists.txt
git commit -m "feat: add SensorChannelHandler using oaa IChannelHandler"
```

---

### Task 6: InputChannelHandler

Handles binding requests from phone. Touch events are sent proactively (not in response to messages). This handler also replaces the current `TouchHandler`'s dependency on aasdk.

**Files:**
- Create: `src/core/aa/handlers/InputChannelHandler.hpp`
- Create: `src/core/aa/handlers/InputChannelHandler.cpp`
- Test: `tests/test_input_channel_handler.cpp`
- Modify: `src/core/aa/TouchHandler.hpp` — remove aasdk dependency, use new handler

**Step 1: Write the failing test**

```cpp
// tests/test_input_channel_handler.cpp
#include <QTest>
#include <QSignalSpy>
#include "core/aa/handlers/InputChannelHandler.hpp"
#include <oaa/Channel/ChannelId.hpp>

class TestInputChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oap::aa::InputChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::Input);
    }

    void testSendTouchEvent() {
        oap::aa::InputChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        // Send a single-finger DOWN at (640, 360)
        oap::aa::InputChannelHandler::Pointer pt{640, 360, 0};
        handler.sendTouchIndication(1, &pt, 0, 0); // action=DOWN

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Input);
    }

    void testBindingRequestResponds() {
        oap::aa::InputChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        // Simulate a BindingRequest
        proto::messages::BindingRequest req;
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(0x8002, payload); // BINDING_REQUEST

        QCOMPARE(sendSpy.count(), 1); // Should respond with BindingResponse
    }
};

QTEST_MAIN(TestInputChannelHandler)
#include "test_input_channel_handler.moc"
```

**Step 2: Implement InputChannelHandler**

The handler owns the touch-sending logic that currently lives in `TouchHandler`. `TouchHandler` will be refactored to call `InputChannelHandler::sendTouchIndication()` instead of using aasdk's `InputServiceChannel` directly.

```cpp
// src/core/aa/handlers/InputChannelHandler.hpp
#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include <vector>

namespace oap {
namespace aa {

class InputChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    struct Pointer { int x; int y; int id; };

    explicit InputChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::Input; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload) override;

    // Called from EvdevTouchReader thread — thread-safe via sendRequested signal
    void sendTouchIndication(int count, const Pointer* pointers,
                             int actionIndex, int action);

private:
    void handleBindingRequest(const QByteArray& payload);
    bool channelOpen_ = false;
};

} // namespace aa
} // namespace oap
```

**Step 3: Run tests, commit**

```bash
git commit -m "feat: add InputChannelHandler — replaces aasdk InputServiceChannel"
```

---

### Task 7: BluetoothChannelHandler

Minimal handler — receives pairing requests, logs them.

**Files:**
- Create: `src/core/aa/handlers/BluetoothChannelHandler.hpp`
- Create: `src/core/aa/handlers/BluetoothChannelHandler.cpp`
- Test: `tests/test_bluetooth_channel_handler.cpp`

**Step 1: Write test, Step 2: Implement**

Same pattern as Tasks 5-6. Key behavior:
- `channelId()` returns `oaa::ChannelId::Bluetooth` (7)
- `onMessage(0x8001, ...)` parses `BluetoothPairingRequest`, logs it
- `fillFeatures` equivalent is already in ServiceDiscoveryBuilder

**Step 3: Commit**

```bash
git commit -m "feat: add BluetoothChannelHandler"
```

---

### Task 8: WiFiChannelHandler

Handles WiFi security requests — sends back SSID/password from config.

**Files:**
- Create: `src/core/aa/handlers/WiFiChannelHandler.hpp`
- Create: `src/core/aa/handlers/WiFiChannelHandler.cpp`
- Test: `tests/test_wifi_channel_handler.cpp`

**Step 1: Write test, Step 2: Implement**

Key behavior:
- `channelId()` returns `oaa::ChannelId::WiFi` (8)
- `onMessage(0x8001, ...)` → send `WifiSecurityResponse` with SSID/key from config
- Constructor takes SSID + password strings

**Step 3: Commit**

```bash
git commit -m "feat: add WiFiChannelHandler"
```

---

### Task 9: Integration Test — Non-AV Channels End-to-End

Write an integration test that creates an `AASession` with `ReplayTransport`, registers all 4 non-AV handlers + control channel, and replays a golden capture through service discovery + channel opens.

**Files:**
- Create: `tests/test_oaa_integration.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the test**

```cpp
// tests/test_oaa_integration.cpp
#include <QTest>
#include <QSignalSpy>
#include <oaa/Session/AASession.hpp>
#include <oaa/Transport/ReplayTransport.hpp>
#include "core/aa/handlers/SensorChannelHandler.hpp"
#include "core/aa/handlers/InputChannelHandler.hpp"
#include "core/aa/handlers/BluetoothChannelHandler.hpp"
#include "core/aa/handlers/WiFiChannelHandler.hpp"
#include "core/aa/ServiceDiscoveryBuilder.hpp"

class TestOAAIntegration : public QObject {
    Q_OBJECT
private slots:
    void testSessionBootWithHandlers() {
        oaa::ReplayTransport transport;
        oap::aa::ServiceDiscoveryBuilder builder;
        oaa::SessionConfig config = builder.build();

        oaa::AASession session(&transport, config);

        // Register non-AV handlers
        oap::aa::SensorChannelHandler sensorHandler;
        oap::aa::InputChannelHandler inputHandler;
        oap::aa::BluetoothChannelHandler btHandler;
        oap::aa::WiFiChannelHandler wifiHandler("TestSSID", "TestPass");

        session.registerChannel(oaa::ChannelId::Sensor, &sensorHandler);
        session.registerChannel(oaa::ChannelId::Input, &inputHandler);
        session.registerChannel(oaa::ChannelId::Bluetooth, &btHandler);
        session.registerChannel(oaa::ChannelId::WiFi, &wifiHandler);

        QSignalSpy stateSpy(&session, &oaa::AASession::stateChanged);

        // Simulate connection
        transport.simulateConnect();
        session.start();

        // Should advance to VersionExchange
        QVERIFY(stateSpy.count() >= 1);
    }
};

QTEST_MAIN(TestOAAIntegration)
#include "test_oaa_integration.moc"
```

**Step 2: Run tests, commit**

```bash
git commit -m "test: add oaa integration test — session boot with non-AV handlers"
```

---

## Phase 3: AV Channel Handlers

### Task 10: AudioChannelHandler (generic — Media, Speech, System)

Generic AV handler for downstream audio. Replaces the current `AudioServiceStub` template in `ServiceFactory.cpp:79-203`.

**Files:**
- Create: `src/core/aa/handlers/AudioChannelHandler.hpp`
- Create: `src/core/aa/handlers/AudioChannelHandler.cpp`
- Test: `tests/test_audio_channel_handler.cpp`

**Step 1: Write the failing test**

```cpp
// tests/test_audio_channel_handler.cpp
#include <QTest>
#include <QSignalSpy>
#include "core/aa/handlers/AudioChannelHandler.hpp"
#include <oaa/Channel/ChannelId.hpp>
#include "AVChannelSetupRequestMessage.pb.h"

class TestAudioChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testMediaChannelId() {
        oap::aa::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QCOMPARE(handler.channelId(), oaa::ChannelId::MediaAudio);
    }

    void testAVSetupRequestResponds() {
        oap::aa::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        // Simulate AVChannelSetupRequest
        proto::messages::AVChannelSetupRequest req;
        req.set_config_index(0);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(0x8000, payload); // SETUP_REQUEST

        QVERIFY(sendSpy.count() >= 1); // AVChannelSetupResponse
    }

    void testMediaDataEmitsSignal() {
        oap::aa::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy dataSpy(&handler, &oap::aa::AudioChannelHandler::audioDataReceived);

        handler.onChannelOpened();

        // Simulate AV_MEDIA_WITH_TIMESTAMP (handled by AASession, calls onMediaData)
        QByteArray pcmData(960, '\x42'); // 960 bytes of PCM
        handler.onMediaData(pcmData, 1234567890);

        QCOMPARE(dataSpy.count(), 1);
    }
};

QTEST_MAIN(TestAudioChannelHandler)
#include "test_audio_channel_handler.moc"
```

**Step 2: Implement AudioChannelHandler**

```cpp
// src/core/aa/handlers/AudioChannelHandler.hpp
#pragma once

#include <oaa/Channel/IAVChannelHandler.hpp>
#include <oaa/Channel/MessageIds.hpp>

namespace oap {
namespace aa {

class AudioChannelHandler : public oaa::IAVChannelHandler {
    Q_OBJECT
public:
    explicit AudioChannelHandler(uint8_t channelId, QObject* parent = nullptr);

    uint8_t channelId() const override { return channelId_; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload) override;

    // IAVChannelHandler
    void onMediaData(const QByteArray& data, uint64_t timestamp) override;
    bool canAcceptMedia() const override { return channelOpen_; }

signals:
    // Prodigy connects this to AudioService::writeAudio
    void audioDataReceived(const QByteArray& data, uint64_t timestamp);

private:
    void handleSetupRequest(const QByteArray& payload);
    void handleStartIndication(const QByteArray& payload);
    void handleStopIndication(const QByteArray& payload);
    void sendAck();

    uint8_t channelId_;
    int32_t session_ = -1;
    bool channelOpen_ = false;
};

} // namespace aa
} // namespace oap
```

Key implementation details:
- `onMediaData()` writes PCM to AudioService and sends ACK back via `sendRequested`
- ACK uses `AVMediaAckIndication` protobuf with session ID and value=1
- Three instances created (Media ch3, Speech ch4, System ch5), each wired to a different PipeWire stream

**Step 3: Run tests, commit**

```bash
git commit -m "feat: add AudioChannelHandler — generic AV handler for media/speech/system"
```

---

### Task 11: VideoChannelHandler

The most complex handler. Receives H.264 frames, manages video focus, forwards to VideoDecoder.

**Files:**
- Create: `src/core/aa/handlers/VideoChannelHandler.hpp`
- Create: `src/core/aa/handlers/VideoChannelHandler.cpp`
- Test: `tests/test_video_channel_handler.cpp`

**Step 1: Write the failing test**

```cpp
#include <QTest>
#include <QSignalSpy>
#include "core/aa/handlers/VideoChannelHandler.hpp"

class TestVideoChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oap::aa::VideoChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::Video);
    }

    void testMediaDataEmitsVideoFrame() {
        oap::aa::VideoChannelHandler handler;
        QSignalSpy spy(&handler, &oap::aa::VideoChannelHandler::videoFrameData);

        handler.onChannelOpened();

        QByteArray h264Data(1024, '\x00');
        handler.onMediaData(h264Data, 1234567890);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toByteArray().size(), 1024);
    }

    void testVideoFocusRequestEmitsResponse() {
        oap::aa::VideoChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        // Simulate VideoFocusRequest from phone
        proto::messages::VideoFocusRequest req;
        req.set_mode(proto::enums::VideoFocusMode::FOCUSED);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::VIDEO_FOCUS_REQUEST, payload);

        // Should emit videoFocusChanged signal
        // (response is sent via VideoFocusIndication, not a direct response)
    }
};
```

**Step 2: Implement VideoChannelHandler**

Port logic from current `VideoService.hpp/cpp`:
- `onMediaData()` emits `videoFrameData(QByteArray, qint64)` signal → connected to VideoDecoder
- `onMessage()` handles: SETUP_REQUEST, START_INDICATION, STOP_INDICATION, VIDEO_FOCUS_REQUEST
- `setVideoFocus()` sends `VideoFocusIndication` to phone
- `setFocusSuppressed()` controls whether phone focus requests are honored

**Step 3: Run tests, commit**

```bash
git commit -m "feat: add VideoChannelHandler — H.264 video + focus management"
```

---

### Task 12: AVInputChannelHandler (Microphone)

Upstream audio — captures mic data and sends to phone.

**Files:**
- Create: `src/core/aa/handlers/AVInputChannelHandler.hpp`
- Create: `src/core/aa/handlers/AVInputChannelHandler.cpp`
- Test: `tests/test_avinput_channel_handler.cpp`

**Step 1: Write test, implement**

Port from `AVInputServiceStub` in ServiceFactory.cpp:639-783. Key behavior:
- `onMessage(INPUT_OPEN_REQUEST)` → open/close PipeWire capture stream
- Capture callback sends `AV_MEDIA_WITH_TIMESTAMP` upstream via `sendRequested`
- Handles `AV_MEDIA_ACK` from phone

**Step 2: Commit**

```bash
git commit -m "feat: add AVInputChannelHandler — microphone upstream to phone"
```

---

## Phase 4: Orchestration Rewrite

### Task 13: Create AndroidAutoOrchestrator (Qt-native replacement for AndroidAutoService)

Replace the ASIO-based `AndroidAutoService` + `AndroidAutoEntity` + `ServiceFactory` with a single Qt-native orchestrator that uses `AASession` from open-androidauto.

**Files:**
- Create: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Create: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `src/core/aa/AndroidAutoService.hpp` — keep the QObject shell, delegate to orchestrator
- Test: `tests/test_aa_orchestrator.cpp`

**Step 1: Design the class**

```cpp
// src/core/aa/AndroidAutoOrchestrator.hpp
#pragma once

#include <QObject>
#include <QThread>
#include <QTcpServer>
#include <QTimer>
#include <memory>

#include <oaa/Session/AASession.hpp>
#include <oaa/Transport/TCPTransport.hpp>

#include "handlers/VideoChannelHandler.hpp"
#include "handlers/AudioChannelHandler.hpp"
#include "handlers/InputChannelHandler.hpp"
#include "handlers/SensorChannelHandler.hpp"
#include "handlers/BluetoothChannelHandler.hpp"
#include "handlers/WiFiChannelHandler.hpp"
#include "handlers/AVInputChannelHandler.hpp"
#include "ServiceDiscoveryBuilder.hpp"
#include "VideoDecoder.hpp"
#include "NightModeProvider.hpp"

namespace oap {

class YamlConfig;
class IAudioService;

namespace aa {

class AndroidAutoOrchestrator : public QObject {
    Q_OBJECT
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    enum ConnectionState {
        Disconnected = 0,
        WaitingForDevice,
        Connecting,
        Connected,
        Backgrounded
    };
    Q_ENUM(ConnectionState)

    explicit AndroidAutoOrchestrator(YamlConfig* yamlConfig,
                                     IAudioService* audioService,
                                     QObject* parent = nullptr);
    ~AndroidAutoOrchestrator() override;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    void requestVideoFocus();
    void requestExitToCar();

    VideoDecoder* videoDecoder() { return &videoDecoder_; }
    InputChannelHandler* inputHandler() { return &inputHandler_; }

    int connectionState() const { return state_; }
    QString statusMessage() const { return statusMessage_; }

signals:
    void connectionStateChanged();
    void statusMessageChanged();

private:
    void onNewConnection();
    void onSessionStateChanged(oaa::SessionState state);
    void onSessionDisconnected(oaa::DisconnectReason reason);

    void setState(ConnectionState state, const QString& message);
    void startConnectionWatchdog();
    void teardownSession();

    YamlConfig* yamlConfig_;
    IAudioService* audioService_;

    // TCP listener (Qt-native, replaces ASIO acceptor)
    QTcpServer tcpServer_;

    // Protocol thread — AASession + Messenger + Transport live here
    QThread protocolThread_;

    // Session (created per-connection, lives on protocolThread_)
    oaa::AASession* session_ = nullptr;
    oaa::TCPTransport* transport_ = nullptr;

    // Channel handlers (live on protocolThread_)
    VideoChannelHandler videoHandler_;
    AudioChannelHandler mediaAudioHandler_{oaa::ChannelId::MediaAudio};
    AudioChannelHandler speechAudioHandler_{oaa::ChannelId::SpeechAudio};
    AudioChannelHandler systemAudioHandler_{oaa::ChannelId::SystemAudio};
    InputChannelHandler inputHandler_;
    SensorChannelHandler sensorHandler_;
    BluetoothChannelHandler btHandler_;
    WiFiChannelHandler wifiHandler_;
    AVInputChannelHandler avInputHandler_;

    // Shared resources (live on UI thread)
    VideoDecoder videoDecoder_;
    NightModeProvider nightProvider_;
    QTimer watchdogTimer_;

    ConnectionState state_ = Disconnected;
    QString statusMessage_;

#ifdef HAS_BLUETOOTH
    class BluetoothDiscoveryService* btDiscovery_ = nullptr;
#endif
};

} // namespace aa
} // namespace oap
```

**Step 2: Implement the lifecycle**

The flow replaces what's in `AndroidAutoService.cpp:56-470`:

1. `start()`:
   - Start `protocolThread_`
   - Move all channel handlers to `protocolThread_`
   - Start `QTcpServer` listening on port 5277
   - Start BT discovery service (if HAS_BLUETOOTH)
   - Set state: WaitingForDevice

2. `onNewConnection()`:
   - Accept `QTcpSocket` from `QTcpServer::nextPendingConnection()`
   - Set TCP keepalive options (same as current: idle=5s, interval=3s, cnt=3)
   - Set `FD_CLOEXEC` on socket FD (gotcha from CLAUDE.md)
   - Create `TCPTransport` wrapping the socket
   - Build `SessionConfig` via `ServiceDiscoveryBuilder`
   - Create `AASession(transport, config)` on protocol thread
   - Register all channel handlers
   - Connect signals:
     - `videoHandler_.videoFrameData` → `videoDecoder_.decodeFrame` (QueuedConnection)
     - `mediaAudioHandler_.audioDataReceived` → AudioService write (QueuedConnection)
     - `sensorHandler_` ← `nightProvider_.nightModeChanged` (QueuedConnection)
   - Call `session_->start()`
   - Set state: Connecting

3. `onSessionStateChanged()`:
   - `Active` → set state: Connected, start watchdog
   - `Disconnected` → teardown

4. `teardownSession()`:
   - Stop watchdog
   - `session_->deleteLater()`
   - `transport_->deleteLater()`
   - Set state: WaitingForDevice (ready for next connection)

5. `stop()`:
   - Send shutdown if active
   - Stop TCP listener
   - Quit + wait protocolThread_
   - Set state: Disconnected

**Step 3: Implement connection watchdog**

Port the TCP_INFO polling from current `AndroidAutoService.cpp:442-470`. Since we now use `QTcpSocket` instead of ASIO sockets, we access the native FD via `QTcpSocket::socketDescriptor()` for `getsockopt(IPPROTO_TCP, TCP_INFO)`.

**Step 4: Write test**

```cpp
void testStartListens() {
    oap::aa::AndroidAutoOrchestrator orch(nullptr, nullptr);
    orch.start();
    QCOMPARE(orch.connectionState(),
             static_cast<int>(oap::aa::AndroidAutoOrchestrator::WaitingForDevice));
    orch.stop();
}
```

**Step 5: Commit**

```bash
git commit -m "feat: add AndroidAutoOrchestrator — Qt-native AA session lifecycle"
```

---

### Task 14: Refactor TouchHandler to Use InputChannelHandler

Remove all aasdk dependencies from `TouchHandler`. Instead of calling aasdk's `InputServiceChannel::sendInputEventIndication()`, call `InputChannelHandler::sendTouchIndication()`.

**Files:**
- Modify: `src/core/aa/TouchHandler.hpp`
- Modify: `src/core/aa/EvdevTouchReader.cpp` (if it references aasdk)

**Step 1: Update TouchHandler**

Remove:
- `#include <aasdk/Channel/Input/InputServiceChannel.hpp>`
- `#include <aasdk/Channel/Promise.hpp>`
- `#include <aasdk_proto/...>` (all proto includes)
- `boost::asio::io_service::strand* strand_`
- `std::shared_ptr<aasdk::channel::input::InputServiceChannel> channel_`
- `setChannel()` method

Replace with:
- `InputChannelHandler* handler_` pointer
- `setHandler(InputChannelHandler*)` method
- `sendTouchIndication()` calls `handler_->sendTouchIndication()` directly

The touch data format stays the same (Pointer struct with x, y, id). The dispatch to the protocol thread happens via `sendRequested` signal in `InputChannelHandler`, not via ASIO strand dispatch.

**Step 2: Verify perf metrics still work**

The perf logging in `TouchHandler` currently runs on the ASIO strand. Move it to run inline in `sendTouchIndication()` — it's lightweight (just timing + periodic log).

**Step 3: Commit**

```bash
git commit -m "refactor: remove aasdk from TouchHandler, use InputChannelHandler"
```

---

### Task 15: Wire Orchestrator into main.cpp

Replace the current `AndroidAutoService` instantiation with `AndroidAutoOrchestrator` in the AA plugin.

**Files:**
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.hpp` (if needed)
- Modify: `src/main.cpp` (if AA service is created there)

**Step 1: Swap the service**

The `AndroidAutoPlugin` currently creates an `AndroidAutoService`. Change it to create `AndroidAutoOrchestrator` instead. The QML interface is the same (`connectionState`, `statusMessage`, `start()`, `stop()`, `videoDecoder()`, `touchHandler()`) so the plugin view doesn't change.

**Step 2: Build and verify compilation**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && make -j$(nproc)
```

**Step 3: Commit**

```bash
git commit -m "feat: wire AndroidAutoOrchestrator into AndroidAutoPlugin"
```

---

### Task 16: Port ProtocolLogger to open-androidauto

Move the logging hooks from aasdk's Messenger to open-androidauto's Messenger.

**Files:**
- Create: `libs/open-androidauto/include/oaa/Messenger/ProtocolLogger.hpp`
- Create: `libs/open-androidauto/src/Messenger/ProtocolLogger.cpp`
- Modify: `libs/open-androidauto/CMakeLists.txt`
- Test: `libs/open-androidauto/tests/test_protocol_logger.cpp`

**Step 1: Implement**

Connect to `Messenger::messageReceived` and intercept `sendMessage` calls. Log direction, channel, message ID, size, and timestamp in TSV format (same as current ProtocolLogger output).

**Step 2: Commit**

```bash
git commit -m "feat(oaa): add ProtocolLogger — hooks into Messenger for protocol capture"
```

---

## Phase 5: Cleanup — Remove aasdk

### Task 17: Remove aasdk References from src/

**Files to modify/delete:**
- Delete: `src/core/aa/AndroidAutoService.hpp` and `.cpp`
- Delete: `src/core/aa/AndroidAutoEntity.hpp` and `.cpp`
- Delete: `src/core/aa/ServiceFactory.hpp` and `.cpp`
- Delete: `src/core/aa/VideoService.hpp` and `.cpp`
- Delete: `src/core/aa/IService.hpp`
- Modify: `src/CMakeLists.txt` — remove all aasdk references
- Modify: `src/core/aa/ProtocolLogger.hpp` — remove aasdk includes (if not already migrated)

**Step 1: Remove old files**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git rm src/core/aa/AndroidAutoService.hpp src/core/aa/AndroidAutoService.cpp
git rm src/core/aa/AndroidAutoEntity.hpp src/core/aa/AndroidAutoEntity.cpp
git rm src/core/aa/ServiceFactory.hpp src/core/aa/ServiceFactory.cpp
git rm src/core/aa/VideoService.hpp src/core/aa/VideoService.cpp
git rm src/core/aa/IService.hpp
```

**Step 2: Update CMakeLists.txt**

In `src/CMakeLists.txt`:
- Remove `aasdk` and `aasdk_proto` from `target_link_libraries`
- Add `open-androidauto` to `target_link_libraries`
- Remove `${CMAKE_SOURCE_DIR}/libs/aasdk/include` from `target_include_directories`
- Remove `${CMAKE_BINARY_DIR}/libs/aasdk` from `target_include_directories`
- Add `${CMAKE_SOURCE_DIR}/libs/open-androidauto/include` and `${CMAKE_BINARY_DIR}/libs/open-androidauto` to `target_include_directories`

**Step 3: Verify no aasdk includes remain**

```bash
grep -r '#include.*aasdk' src/
```

Expected: no output

**Step 4: Build**

```bash
cmake .. && make -j$(nproc)
```

**Step 5: Commit**

```bash
git commit -m "refactor: remove all aasdk service stubs and entity"
```

---

### Task 18: Remove aasdk Submodule

**Files:**
- Modify: `CMakeLists.txt` (top-level) — remove `add_subdirectory(libs/aasdk)`
- Modify: `.gitmodules` — remove aasdk entry
- Delete: `libs/aasdk/`

**Step 1: Remove submodule**

```bash
git submodule deinit libs/aasdk
git rm libs/aasdk
```

**Step 2: Remove Boost.ASIO from build**

In top-level `CMakeLists.txt`:
- Remove `find_package(Boost REQUIRED COMPONENTS ...)` if Boost is no longer needed
- Or keep just `Boost::log` if still used for logging (check if any remaining code uses BOOST_LOG_TRIVIAL)

**Step 3: Clean up top-level CMakeLists.txt**

Replace:
```cmake
add_subdirectory(libs/aasdk)
```
With:
```cmake
add_subdirectory(libs/open-androidauto)
```

**Step 4: Full rebuild + test**

```bash
rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure
```

**Step 5: Commit**

```bash
git commit -m "feat!: remove aasdk submodule, switch to open-androidauto

BREAKING: Boost.ASIO is no longer a dependency.
All AA protocol communication now uses open-androidauto (Qt-native)."
```

---

### Task 19: Replace Boost.Log with qDebug/qWarning

If Boost is no longer needed after aasdk removal, replace all `BOOST_LOG_TRIVIAL` calls with Qt logging.

**Files:**
- All files in `src/` that use `BOOST_LOG_TRIVIAL`

**Step 1: Find all uses**

```bash
grep -rn 'BOOST_LOG_TRIVIAL' src/
```

**Step 2: Replace each with Qt equivalent**

- `BOOST_LOG_TRIVIAL(info)` → `qInfo()`
- `BOOST_LOG_TRIVIAL(warning)` → `qWarning()`
- `BOOST_LOG_TRIVIAL(error)` → `qCritical()`
- `BOOST_LOG_TRIVIAL(debug)` → `qDebug()`

Note: Qt logging uses `<<` operator same as Boost.Log, so the stream syntax is identical.

**Step 3: Remove Boost::log from CMake**

**Step 4: Full rebuild + test**

**Step 5: Commit**

```bash
git commit -m "refactor: replace Boost.Log with Qt logging, remove Boost dependency"
```

---

## Phase 6: Pi Deployment + Validation

### Available Test Infrastructure

The dev VM has direct access to both the Pi and the test phone:

- **Pi (SSH):** `ssh matt@192.168.1.149` — direct from VM, works through VMware NAT. Use for build, deploy, launch, log tailing, process management.
- **Phone (ADB):** `platform-tools/adb` in the project root — USB-connected to dev VM. Use for logcat capture, BT/WiFi state checks, screen state.
- **Capture script:** `Testing/capture.sh` — orchestrates simultaneous Pi log + phone logcat + protocol capture. Use for golden capture comparison.
- **Reconnect script:** `Testing/reconnect.sh` — automated BT disconnect → reconnect cycle for testing reconnection.

**Key gotchas for remote testing:**
- Use `ssh -f` (not `nohup ... &` inside SSH) for launching background processes on Pi — SSH hangs waiting for child FDs otherwise.
- Use `pgrep -f openauto-prodigy` (not `pgrep openauto-prodigy`) — process names >15 chars silently fail.
- Pi log at `/tmp/oap.log` — tail via `ssh matt@192.168.1.149 'tail -50 /tmp/oap.log'`.
- Phone logcat: `platform-tools/adb logcat -d | grep -i androidauto` for AA-specific logs.
- Never rsync `libs/aasdk/` or `build/` to Pi — x86 binaries overwrite ARM64 builds.

### Task 20: Deploy and Test on Pi

**Step 1: Copy source to Pi**

```bash
rsync -avz --exclude='build/' --exclude='.git/' --exclude='libs/aasdk/' \
  /home/matt/claude/personal/openautopro/openauto-prodigy/ \
  matt@192.168.1.149:/home/matt/openauto-prodigy/
```

**Step 2: Build on Pi**

```bash
ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy && rm -rf build && mkdir build && cd build && cmake .. && make -j3"
```

**Step 3: Run and verify**

```bash
ssh -f matt@192.168.1.149 'cd /home/matt/openauto-prodigy && nohup env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 ./build/src/openauto-prodigy > /tmp/oap.log 2>&1 &'
```

**Step 4: Automated verification via SSH + ADB**

Check Pi logs for expected protocol sequence:
```bash
# Verify version handshake
ssh matt@192.168.1.149 'grep -c "Version response: 1 . 7 MATCH" /tmp/oap.log'

# Verify all 9 channels opened
ssh matt@192.168.1.149 'grep -c "Opening channel" /tmp/oap.log'

# Verify video started
ssh matt@192.168.1.149 'grep "AV setup\|AV start" /tmp/oap.log'
```

Check phone side via ADB logcat:
```bash
# Verify phone sees the head unit
platform-tools/adb logcat -d | grep -i 'androidauto\|AAGateway\|projection' | tail -20
```

**Step 5: Acceptance gates**

1. App launches, shows launcher UI
2. Phone connects via BT → WiFi AP → TCP
3. Version handshake succeeds (v1.7) — verify in Pi log
4. Service discovery completes — verify 9 channel opens in Pi log
5. Video displays at 720p/30fps — verify via ADB logcat (phone reports resolution)
6. Touch works (tap, swipe, multi-touch) — manual test, verify in Pi log
7. Audio plays (music, nav prompts) — manual test
8. Night mode toggles correctly — verify sensor event in Pi log
9. Graceful shutdown works (exit app → phone gets shutdown request) — verify in both logs
10. Reconnect works — run `Testing/reconnect.sh` and verify new session in Pi log

**Step 6: Run capture and compare with baseline**

```bash
cd Testing && bash capture.sh
# Then compare channel open sequence:
diff <(grep 'CHANNEL_OPEN' captures/baseline/pi-protocol.log) \
     <(grep 'CHANNEL_OPEN' captures/latest/pi-protocol.log)
```

**Step 7: Commit final state**

```bash
git commit -m "docs: update CLAUDE.md for open-androidauto migration complete"
```

---

## Risk Summary

| Risk | Mitigation |
|------|-----------|
| Threading — signal/slot across QThread boundaries | All handlers emit via `sendRequested` (auto-queued). No direct cross-thread calls. |
| Shutdown race — handler destroyed while message in flight | Deterministic stop: stop transport → drain messenger → unregister handlers → deleteLater |
| Service discovery mismatch | Golden capture comparison test (Task 2 Step 5) |
| Video frame timing regression | Same FFmpeg decoder, same QVideoFrame path — only the data source changes |
| Touch latency regression | Measure in Task 14, compare against PerfStats baseline (~1ms total) |
| Boost removal breaks unrelated code | grep for ALL Boost usage before removing, handle log migration in Task 19 |
