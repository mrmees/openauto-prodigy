# Protocol Plumbing — Wire Up Existing Proto Handlers

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Bridge the gap between proto messages the library already parses and the app-level services that should act on them. Audio focus, voice sessions, sensor accuracy, and navigation events.

**Architecture:** No new channels or handlers. All work is wiring existing signals to existing services, trimming false advertisements, and publishing events to the plugin system via `IEventBus`.

**Tech Stack:** C++17, Qt 6, protobuf (proto3), CMake, QTest

---

### Task 1: Trim Sensor Channel to Only Advertised-and-Supported Types

The ServiceDiscoveryBuilder advertises 6 sensor types but the handler only pushes data for 3. Location, compass, accelerometer, and gyroscope are promised but never delivered. Parking brake is implemented but not advertised.

**Files:**
- Modify: `src/core/aa/ServiceDiscoveryBuilder.cpp:300-312`
- Test: `tests/test_sensor_channel_handler.cpp`

**Step 1: Write a test that verifies only supported sensors are advertised**

Add to `tests/test_sensor_channel_handler.cpp`:

```cpp
void testOnlyAdvertisesSupportedSensors() {
    // The handler supports: NIGHT_DATA, DRIVING_STATUS, PARKING_BRAKE
    // Verify these are the only ones we should advertise
    oaa::hu::SensorChannelHandler handler;
    QSignalSpy sendSpy(&handler, &oaa::hu::SensorChannelHandler::sendRequested);
    handler.onChannelOpened();

    // Request parking brake — should get OK response + initial data
    oaa::proto::messages::SensorStartRequestMessage req;
    req.set_sensor_type(oaa::proto::enums::SensorType::PARKING_BRAKE);
    QByteArray payload(req.ByteSizeLong(), '\0');
    req.SerializeToArray(payload.data(), payload.size());

    handler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);

    // Should have 2 sends: start response + initial parking brake data
    QCOMPARE(sendSpy.count(), 2);
}
```

**Step 2: Run test to verify it passes (parking brake already works)**

Run: `cd build && cmake --build . -j$(nproc) && ctest -R test_sensor --output-on-failure`

Expected: PASS (handler already supports parking brake)

**Step 3: Trim ServiceDiscoveryBuilder sensor list**

In `src/core/aa/ServiceDiscoveryBuilder.cpp`, replace lines 306-311:

```cpp
// Before (lies about capabilities):
addSensor(oaa::proto::enums::SensorType::NIGHT_DATA);
addSensor(oaa::proto::enums::SensorType::DRIVING_STATUS);
addSensor(oaa::proto::enums::SensorType::LOCATION);
addSensor(oaa::proto::enums::SensorType::COMPASS);
addSensor(oaa::proto::enums::SensorType::ACCEL);
addSensor(oaa::proto::enums::SensorType::GYRO);

// After (only what we can actually deliver):
addSensor(oaa::proto::enums::SensorType::NIGHT_DATA);
addSensor(oaa::proto::enums::SensorType::DRIVING_STATUS);
addSensor(oaa::proto::enums::SensorType::PARKING_BRAKE);
```

**Step 4: Build and run tests**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`

Expected: All tests pass.

**Step 5: Commit**

```bash
git add src/core/aa/ServiceDiscoveryBuilder.cpp tests/test_sensor_channel_handler.cpp
git commit -m "fix: only advertise sensors we can actually populate

Removes LOCATION, COMPASS, ACCEL, GYRO from service discovery
(handler had no data for these). Adds PARKING_BRAKE which was
implemented but not advertised."
```

---

### Task 2: Bridge AA Audio Focus → AudioService

The phone sends `AudioFocusRequest` on the control channel. Currently `AASession` auto-grants and responds but never tells `AudioService`. This means AA audio streams play at full volume regardless of focus state, and BT audio from other sources is never ducked.

**Files:**
- Modify: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `libs/open-androidauto/src/Session/AASession.cpp:60-90`
- Modify: `libs/open-androidauto/include/oaa/Session/AASession.hpp`
- Test: `tests/test_aa_orchestrator.cpp`

**Step 1: Add `audioFocusChanged` signal to AASession**

The audio focus logic currently lives in an inline lambda in the AASession constructor. Refactor it to:
1. Keep the auto-response behavior (phone still gets granted)
2. Add a signal that tells the app what focus type was requested

In `libs/open-androidauto/include/oaa/Session/AASession.hpp`, add to signals:

```cpp
signals:
    // existing signals...

    /// Emitted when phone requests audio focus change.
    /// streamType: 0=release, 1=media(GAIN), 2=speech(GAIN_TRANSIENT), 3=nav(GAIN_NAVI)
    void audioFocusChanged(int focusType);
```

In `libs/open-androidauto/src/Session/AASession.cpp`, modify the audio focus lambda (lines 64-90) to also emit the signal:

```cpp
connect(controlChannel_, &ControlChannel::audioFocusRequested,
        this, [this](const QByteArray& payload) {
            proto::messages::AudioFocusRequest req;
            if (!req.ParseFromArray(payload.constData(), payload.size()))
                return;
            auto type = req.audio_focus_type();
            proto::enums::AudioFocusState::Enum state;
            switch (type) {
            case proto::enums::AudioFocusType::GAIN:
                state = proto::enums::AudioFocusState::GAIN; break;
            case proto::enums::AudioFocusType::GAIN_TRANSIENT:
                state = proto::enums::AudioFocusState::GAIN_TRANSIENT; break;
            case proto::enums::AudioFocusType::GAIN_NAVI:
                state = proto::enums::AudioFocusState::GAIN_TRANSIENT_GUIDANCE_ONLY; break;
            case proto::enums::AudioFocusType::RELEASE:
                state = proto::enums::AudioFocusState::LOSS; break;
            default:
                state = proto::enums::AudioFocusState::NONE; break;
            }
            qDebug() << "[AASession] Audio focus request type:" << (int)type
                     << "→ state:" << (int)state;
            proto::messages::AudioFocusResponse resp;
            resp.set_audio_focus_state(state);
            QByteArray data(resp.ByteSizeLong(), '\0');
            resp.SerializeToArray(data.data(), data.size());
            controlChannel_->sendAudioFocusResponse(data);

            // Notify app layer
            emit audioFocusChanged(static_cast<int>(type));
        });
```

**Step 2: Write failing test for audio focus bridge**

Add to `tests/test_aa_orchestrator.cpp`:

```cpp
void testAudioFocusGainRequestsDucking() {
    // Verify that when AASession emits audioFocusChanged(GAIN=1),
    // the orchestrator calls requestAudioFocus on the media stream
    // This test will initially fail because the connection doesn't exist yet
}
```

Note: The exact test structure depends on what mocking is available. If `AudioService` is mockable through `IAudioService`, create a mock. Otherwise test via signal spy on the orchestrator.

**Step 3: Connect audioFocusChanged in AndroidAutoOrchestrator**

In `src/core/aa/AndroidAutoOrchestrator.cpp`, after session creation (around line 220 where handlers are registered), add:

```cpp
// Bridge AA audio focus to PipeWire stream management
connect(session_, &oaa::AASession::audioFocusChanged,
        this, [this](int focusType) {
    if (!audioService_) return;

    switch (focusType) {
    case 1: // GAIN — media playback
        if (mediaStream_)
            audioService_->requestAudioFocus(mediaStream_, oap::AudioFocusType::Gain);
        break;
    case 2: // GAIN_TRANSIENT — voice/speech
        if (speechStream_)
            audioService_->requestAudioFocus(speechStream_, oap::AudioFocusType::GainTransient);
        break;
    case 3: // GAIN_NAVI — navigation prompt
        if (speechStream_)
            audioService_->requestAudioFocus(speechStream_, oap::AudioFocusType::GainTransientMayDuck);
        break;
    case 4: // RELEASE
        if (mediaStream_)  audioService_->releaseAudioFocus(mediaStream_);
        if (speechStream_) audioService_->releaseAudioFocus(speechStream_);
        break;
    }
}, Qt::QueuedConnection);
```

In `src/core/aa/AndroidAutoOrchestrator.hpp`, add include if needed:

```cpp
#include "core/services/IAudioService.hpp"  // for AudioFocusType
```

**Step 4: Build and run tests**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`

Expected: All tests pass. Ducking behavior now active during AA sessions.

**Step 5: Commit**

```bash
git add libs/open-androidauto/include/oaa/Session/AASession.hpp \
        libs/open-androidauto/src/Session/AASession.cpp \
        src/core/aa/AndroidAutoOrchestrator.hpp \
        src/core/aa/AndroidAutoOrchestrator.cpp \
        tests/test_aa_orchestrator.cpp
git commit -m "feat: bridge AA audio focus requests to PipeWire stream ducking

When the phone requests audio focus (GAIN for music, GAIN_TRANSIENT
for voice, GAIN_NAVI for navigation), the appropriate PipeWire stream
now claims focus through AudioService. Other streams are ducked or
muted based on priority. Previously all streams played at full volume
simultaneously."
```

---

### Task 3: Wire Voice Session Response

The phone sends `VoiceSessionRequest` (0x0011) when the user taps the mic icon or says "Hey Google". The `ControlChannel` emits `voiceSessionRequested` but nothing connects to it — the phone gets no response.

**Files:**
- Modify: `libs/open-androidauto/src/Session/AASession.cpp`
- Modify: `libs/open-androidauto/include/oaa/Channel/ControlChannel.hpp`
- Modify: `libs/open-androidauto/src/Channel/ControlChannel.cpp`
- Test: `tests/test_control_channel.cpp`

**Step 1: Add voice session response method to ControlChannel**

Check if `ControlChannel` already has a `sendVoiceSessionResponse()` method. If not, add one following the pattern of `sendAudioFocusResponse()`:

In `libs/open-androidauto/include/oaa/Channel/ControlChannel.hpp`:

```cpp
public:
    // existing methods...
    void sendVoiceSessionResponse(const QByteArray& payload);
```

In `libs/open-androidauto/src/Channel/ControlChannel.cpp`, add the message ID constant if missing:

```cpp
constexpr uint16_t MSG_VOICE_SESSION_RESPONSE    = 0x0012; // verify — may share ID with audio focus
```

**IMPORTANT:** Verify the correct message ID for voice session response. It may NOT be 0x0012 (that's audio focus request). Check `libs/open-androidauto/src/Messenger/ProtocolLogger.cpp` and proto definitions for the actual voice session response ID. The APK analysis should have this.

Add the send method:

```cpp
void ControlChannel::sendVoiceSessionResponse(const QByteArray& payload) {
    emit sendRequested(channelId(), MSG_VOICE_SESSION_RESPONSE, payload);
}
```

**Step 2: Connect voiceSessionRequested in AASession**

In `libs/open-androidauto/src/Session/AASession.cpp`, after the audio focus connection (around line 90):

```cpp
// Auto-respond to voice session requests
connect(controlChannel_, &ControlChannel::voiceSessionRequested,
        this, [this](const QByteArray& payload) {
            Q_UNUSED(payload)
            qDebug() << "[AASession] Voice session requested, granting";
            // Voice session response is just an acknowledgment
            // The phone handles the actual voice UI
            // TODO: verify response format from APK analysis
        });
```

**Note:** This task requires verifying the exact response protobuf format. The phone may just need an empty ack, or it may need a specific proto message. Check `VoiceSessionRequestMessage.proto` — it only has `optional int32 session_type = 1`. The response format needs APK verification before implementing the send. For now, log and investigate during testing.

**Step 3: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`

Expected: All tests pass.

**Step 4: Test on Pi**

Deploy to Pi, connect phone, trigger "Hey Google" or tap mic icon. Check logs for the voice session debug message. Note whether the assistant activates — it may already work since the mic channel streams audio continuously.

**Step 5: Commit**

```bash
git add libs/open-androidauto/src/Session/AASession.cpp \
        libs/open-androidauto/include/oaa/Channel/ControlChannel.hpp \
        libs/open-androidauto/src/Channel/ControlChannel.cpp
git commit -m "feat: handle voice session requests from phone

Log and acknowledge VoiceSessionRequest (0x0011) instead of silently
dropping it. The mic channel already streams audio; this ensures the
phone knows the HU is ready for voice input."
```

---

### Task 4: Publish Navigation Events to IEventBus

The `NavigationChannelHandler` parses turn-by-turn data and emits Qt signals, but nothing in the app connects to them. Publish these events to `IEventBus` so any plugin can subscribe (turn signals, HUD, interior lighting, etc.).

**Files:**
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Test: `tests/test_aa_orchestrator.cpp`

**Step 1: Write failing test for nav event publishing**

Add to `tests/test_aa_orchestrator.cpp`:

```cpp
void testNavStepPublishesToEventBus() {
    // Create a mock event bus, verify that when the nav handler
    // emits navigationStepChanged, an event appears on the bus
    // with topic "aa.nav.step"
}
```

**Step 2: Define event bus topics**

Nav events use these topic strings (convention: `aa.nav.*`):

| Topic | Payload (QVariantMap) |
|-------|----------------------|
| `aa.nav.state` | `{"active": bool}` |
| `aa.nav.step` | `{"instruction": QString, "destination": QString, "maneuverType": int}` |
| `aa.nav.distance` | `{"distance": QString, "unit": int}` |

**Step 3: Wire nav handler signals to event bus in AndroidAutoOrchestrator**

In `src/core/aa/AndroidAutoOrchestrator.cpp`, after handler registration (around line 220), add:

```cpp
// Publish navigation events to plugin event bus
if (eventBus_) {
    connect(&navHandler_, &oaa::hu::NavigationChannelHandler::navigationStateChanged,
            this, [this](bool active) {
        eventBus_->publish("aa.nav.state", QVariantMap{{"active", active}});
    });

    connect(&navHandler_, &oaa::hu::NavigationChannelHandler::navigationStepChanged,
            this, [this](const QString& instruction, const QString& destination, int maneuverType) {
        eventBus_->publish("aa.nav.step", QVariantMap{
            {"instruction", instruction},
            {"destination", destination},
            {"maneuverType", maneuverType}
        });
    });

    connect(&navHandler_, &oaa::hu::NavigationChannelHandler::navigationDistanceChanged,
            this, [this](const QString& distance, int unit) {
        eventBus_->publish("aa.nav.distance", QVariantMap{
            {"distance", distance},
            {"unit", unit}
        });
    });
}
```

**Step 4: Pass IEventBus to AndroidAutoOrchestrator**

The orchestrator currently takes `config, audioService, yamlConfig` in its constructor. Add `IEventBus*`:

In `src/core/aa/AndroidAutoOrchestrator.hpp`:

```cpp
class AndroidAutoOrchestrator : public QObject {
public:
    explicit AndroidAutoOrchestrator(const oap::Configuration& config,
                                      oap::IAudioService* audioService,
                                      oap::YamlConfig* yamlConfig,
                                      oap::IEventBus* eventBus,
                                      QObject* parent = nullptr);
// ...
private:
    oap::IEventBus* eventBus_ = nullptr;
```

Update the constructor in `AndroidAutoOrchestrator.cpp` to store `eventBus_`.

Update the call site in `AndroidAutoPlugin::initialize()`:

```cpp
aaService_ = new oap::aa::AndroidAutoOrchestrator(
    config_, audioService, yamlConfig_, context->eventBus(), this);
```

**Step 5: Build and run tests**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`

Expected: All tests pass. Nav events now published to event bus.

**Step 6: Commit**

```bash
git add src/core/aa/AndroidAutoOrchestrator.hpp \
        src/core/aa/AndroidAutoOrchestrator.cpp \
        src/plugins/android_auto/AndroidAutoPlugin.cpp \
        tests/test_aa_orchestrator.cpp
git commit -m "feat: publish navigation events to plugin event bus

Navigation state, step, and distance events from the AA phone are
now published to IEventBus as aa.nav.state, aa.nav.step, and
aa.nav.distance. Plugins can subscribe to these for turn signals,
HUD display, interior lighting, or other responses to nav data."
```

---

### Task 5: Publish Phone Status Events to IEventBus

Same pattern as nav — `PhoneStatusChannelHandler` already parses call state and emits signals. Publish to event bus for plugin consumption.

**Files:**
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Test: `tests/test_aa_orchestrator.cpp`

**Step 1: Wire phone status signals to event bus**

In `src/core/aa/AndroidAutoOrchestrator.cpp`, alongside the nav event wiring:

```cpp
// Publish phone status events to plugin event bus
if (eventBus_) {
    connect(&phoneStatusHandler_, &oaa::hu::PhoneStatusChannelHandler::callStateChanged,
            this, [this](int callState, const QString& number,
                          const QString& displayName, const QByteArray& contactPhoto) {
        eventBus_->publish("aa.phone.call", QVariantMap{
            {"callState", callState},
            {"number", number},
            {"displayName", displayName},
            {"hasPhoto", !contactPhoto.isEmpty()}
        });
        // Note: contact photo bytes not included in QVariant — too large for event bus.
        // Plugins needing photos should connect directly to the handler signal.
    });

    connect(&phoneStatusHandler_, &oaa::hu::PhoneStatusChannelHandler::callsIdle,
            this, [this]() {
        eventBus_->publish("aa.phone.idle");
    });
}
```

**Step 2: Also publish media status events**

While we're here, wire up media status too:

```cpp
connect(&mediaStatusHandler_, &oaa::hu::MediaStatusChannelHandler::playbackStateChanged,
        this, [this](int state, const QString& appName) {
    eventBus_->publish("aa.media.state", QVariantMap{
        {"state", state},
        {"appName", appName}
    });
});

connect(&mediaStatusHandler_, &oaa::hu::MediaStatusChannelHandler::metadataChanged,
        this, [this](const QString& title, const QString& artist, const QString& album,
                      const QByteArray& /*albumArt*/) {
    eventBus_->publish("aa.media.metadata", QVariantMap{
        {"title", title},
        {"artist", artist},
        {"album", album}
    });
    // Note: album art bytes not included — too large for event bus.
    // Plugins needing art should connect directly to the handler signal.
});
```

**Step 3: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`

**Step 4: Commit**

```bash
git add src/core/aa/AndroidAutoOrchestrator.cpp tests/test_aa_orchestrator.cpp
git commit -m "feat: publish phone status and media events to plugin event bus

Phone call state (aa.phone.call, aa.phone.idle) and media playback
(aa.media.state, aa.media.metadata) events now available to plugins
via IEventBus. Enables plugin-based call handling UI and media
display outside the AA fullscreen view."
```

---

### Task 6: Deploy and Integration Test on Pi

All changes are plumbing — they need real-device testing to verify behavior.

**Step 1: Cross-compile and deploy**

```bash
cd build-pi && cmake --build . -j$(nproc)
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
```

**Step 2: Test sensor trim**

Connect phone, start AA session. Check logs:
- Should NOT see `SensorStartRequest` for LOCATION, COMPASS, ACCEL, or GYRO
- SHOULD see requests for NIGHT_DATA, DRIVING_STATUS, PARKING_BRAKE
- Verify phone doesn't complain about missing sensors

```bash
ssh matt@192.168.1.152 "grep -i sensor /tmp/oap.log"
```

**Step 3: Test audio focus ducking**

1. Start music playback on phone (via AA)
2. Trigger navigation prompt (start a route)
3. Check logs for `Audio focus request type: 3 → state: 7` (GAIN_NAVI)
4. Verify music volume ducks during nav prompt
5. After prompt, verify music resumes full volume

```bash
ssh matt@192.168.1.152 "grep -i 'audio focus' /tmp/oap.log"
```

**Step 4: Test voice session**

1. Tap mic icon in AA UI
2. Check logs for `Voice session requested`
3. Try saying "Hey Google" — does assistant activate?
4. Note behavior for follow-up work

```bash
ssh matt@192.168.1.152 "grep -i 'voice session' /tmp/oap.log"
```

**Step 5: Test nav events**

1. Start navigation to a destination
2. Check logs for nav event bus publishes
3. Verify instruction text, maneuver types, distance updates appear

```bash
ssh matt@192.168.1.152 "grep -i 'NavChannel\|aa.nav' /tmp/oap.log"
```

**Step 6: Commit any fixes discovered during testing**

---

## Event Bus Topic Reference

For plugin developers, the complete list of AA events published to `IEventBus`:

| Topic | Payload | When |
|-------|---------|------|
| `aa.nav.state` | `{active: bool}` | Navigation starts/ends |
| `aa.nav.step` | `{instruction: str, destination: str, maneuverType: int}` | Turn instruction update |
| `aa.nav.distance` | `{distance: str, unit: int}` | Distance to next maneuver |
| `aa.phone.call` | `{callState: int, number: str, displayName: str, hasPhoto: bool}` | Call state change |
| `aa.phone.idle` | (empty) | No active calls |
| `aa.media.state` | `{state: int, appName: str}` | Playback state change |
| `aa.media.metadata` | `{title: str, artist: str, album: str}` | Track metadata update |

## Future Work (Not In This Plan)

- **Background BT service** — separate design effort for persistent HFP/SDP/AA discovery
- **Sensor plugin registry** — dynamic sensor providers (OBD-II → speed/RPM, GPS module → location)
- **Audio focus denial** — currently always grants; may need to deny when HU has priority audio
- **Phone call audio routing** — HFP SCO → PipeWire (blocked on BT service design)
- **Voice session response format** — needs APK verification for proper response proto
