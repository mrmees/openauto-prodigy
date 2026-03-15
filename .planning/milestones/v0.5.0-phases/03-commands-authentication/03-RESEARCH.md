# Phase 3: Commands & Authentication - Research

**Researched:** 2026-03-06
**Domain:** Android Auto protocol -- outbound HU commands, BT auth exchange, input haptic notifications
**Confidence:** HIGH

## Summary

Phase 3 adds the first HU-initiated outbound commands to the protocol stack: media playback control, voice session activation, BT authentication exchange, and input haptic feedback handling. The research reveals one critical correction to CONTEXT.md assumptions and resolves all three "needs research" message IDs.

**Critical finding:** VoiceSessionRequest is a **Control channel** message (channel 0, msgId 0x0011), not an InputChannelHandler message as CONTEXT.md suggests. The ControlChannel already receives this message and emits `voiceSessionRequested(payload)`. The outbound `sendVoiceSessionRequest()` method should be added to ControlChannel (following the same pattern as `sendShutdownRequest()`, `sendCallAvailability()`, etc.), with the orchestrator providing the public API.

**Primary recommendation:** Implement all four features in a single plan. Each modifies one handler + orchestrator wiring. The outbound command pattern (build protobuf, serialize, emit `sendRequested`) is already established in BluetoothChannelHandler and InputChannelHandler.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions
- MediaPlaybackCommand added to MediaStatusChannelHandler as `sendPlaybackCommand(PlaybackCommandType)` + `togglePlayback()` convenience
- Track current playback state internally (handler already receives PlaybackStatus)
- VoiceSessionRequest uses push-to-talk model: START on press, STOP on release
- BT AUTH_DATA (0x8003) case added to BluetoothChannelHandler -- log raw hex, no proto schema
- BT AUTH_RESULT = 0x8004 provisional -- handle with logging
- Graceful BT auth fallback -- log warnings, don't break existing pairing
- InputBindingNotification parsed with HapticFeedbackType enum in InputChannelHandler
- Emit Qt signal for haptic feedback (no physical output)
- API-only -- no UI trigger wiring in this phase
- Follows Phase 2 signal pattern: handlers emit Qt signals, orchestrator connects and logs

### Claude's Discretion
- Exact signal signatures for new signals
- Log message formatting within established patterns
- Whether to add a `sendMediaCommand()` helper in orchestrator or keep it handler-only
- Test coverage approach (unit tests for command serialization, integration test for Pi verification)
- Temporary test trigger mechanism for Pi verification (CLI flag, debug key, etc.)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| MED-01 | HU can send MediaPlaybackCommand (PAUSE/RESUME) to phone | MediaPlaybackCommand proto verified (Silver). MsgId 0x8002 on MediaStatus channel (ch 10). Outbound pattern exists in BluetoothChannelHandler |
| MED-02 | HU sends VoiceSessionRequest START/STOP | VoiceSessionRequest proto verified (Silver). **Channel 0 (Control), msgId 0x0011** -- NOT input channel. ControlChannel already receives this; add `sendVoiceSessionRequest()` method |
| BT-01 | HU exchanges BluetoothAuthenticationData (0x8003) and handles BluetoothAuthenticationResult (0x8004) | AUTH_DATA 0x8003 defined in MessageIds.hpp. AUTH_RESULT 0x8004 needs adding. No proto schema -- log raw hex. Not observed in S25 captures |
| INP-01 | HU receives InputBindingNotification haptic feedback from phone | InputBindingNotification proto exists. HapticFeedbackType enum has 5 values (SELECT through DRAG_END). MsgId needs provisional assignment (0x8004 on input channel) |

</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| open-androidauto | in-tree | Protocol handlers, ControlChannel, session management | Project's own protocol library |
| protobuf | system | Message serialization/deserialization | AA protocol wire format |
| Qt 6 (Core, Network) | 6.4/6.8 | Signals/slots, QByteArray, QObject | Project framework |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Qt6::Test | system | Unit test framework | Handler command serialization tests |

### Proto Definitions Available
| Proto File | Package | Confidence | Status |
|------------|---------|------------|--------|
| `MediaPlaybackCommandMessage.proto` | `oaa.proto.messages` | Silver | Ready to use |
| `VoiceSessionRequestMessage.proto` | `oaa.proto.messages` | Silver | Ready to use |
| `InputBindingNotificationMessage.proto` | `oaa.proto.messages` | Silver | Ready to use |
| `HapticFeedbackTypeEnum.proto` | `oaa.proto.enums` | Unverified | Ready to use (enum values structurally sound) |

## Architecture Patterns

### Message ID Resolution (CRITICAL FINDINGS)

Research resolved all "needs research" message IDs from CONTEXT.md:

| Feature | Channel | Message ID | Direction | Evidence |
|---------|---------|------------|-----------|----------|
| MediaPlaybackCommand | 10 (MediaStatus) | 0x8002 | HU -> Phone | media.md docs (Silver), proto channel docs |
| VoiceSessionRequest | **0 (Control)** | **0x0011** | HU -> Phone | Wire capture, ControlChannel.cpp, message_map.py |
| InputBindingNotification | 1 (Input) | **0x8004** (provisional) | Phone -> HU | Not in captures; assign next available after BINDING_RESPONSE (0x8003) |
| BT AUTH_DATA | 8 (Bluetooth) | 0x8003 | Phone -> HU | Already in MessageIds.hpp |
| BT AUTH_RESULT | 8 (Bluetooth) | 0x8004 | Phone -> HU | Provisional -- add to BluetoothMessageId namespace |

### VoiceSessionRequest Channel Correction

**CONTEXT.md says:** "Add `sendVoiceSessionRequest(VoiceSessionType)` to `InputChannelHandler`"

**Protocol reality:** VoiceSessionRequest is a Control channel message (channel 0, msgId 0x0011). Wire captures confirm: `{"channel_id":0, "message_id":17, "message_name":"VOICE_SESSION_REQUEST"}`. The ControlChannel already has:
- Inbound handling (`case MSG_VOICE_SESSION_REQUEST: emit voiceSessionRequested(payload)`)
- The `voiceSessionRequested(const QByteArray& payload)` signal

**Correct implementation:** Add `sendVoiceSessionRequest(int sessionType)` to `ControlChannel` (following the pattern of `sendShutdownRequest()`, `sendCallAvailability()`, etc.). Expose via `session_->controlChannel()->sendVoiceSessionRequest()` from the orchestrator.

### Outbound Command Pattern (Established)

The outbound send pattern is already used in two places:

```cpp
// Pattern: BluetoothChannelHandler::handlePairingRequest()
oaa::proto::messages::BluetoothPairingResponse resp;
resp.set_already_paired(true);
resp.set_status(oaa::proto::enums::BluetoothPairingStatus::OK);
QByteArray data(resp.ByteSizeLong(), '\0');
resp.SerializeToArray(data.data(), data.size());
emit sendRequested(channelId(), oaa::BluetoothMessageId::PAIRING_RESPONSE, data);
```

```cpp
// Pattern: InputChannelHandler::sendTouchIndication()
oaa::proto::messages::InputEventIndication indication;
// ... populate fields ...
QByteArray data(indication.ByteSizeLong(), '\0');
indication.SerializeToArray(data.data(), data.size());
emit sendRequested(channelId(), oaa::InputMessageId::INPUT_EVENT_INDICATION, data);
```

```cpp
// Pattern: ControlChannel::sendCallAvailability()
oaa::proto::messages::CallAvailabilityStatus msg;
msg.set_call_available(available);
QByteArray payload(msg.ByteSizeLong(), '\0');
msg.SerializeToArray(payload.data(), payload.size());
emit sendRequested(0, MSG_CALL_AVAILABILITY, payload);
```

### Handler Signal Pattern (Established from Phases 1-2)

```cpp
// Header: signal declaration
signals:
    void playbackStateChanged(int state, const QString& appName);

// Source: emit after parsing
emit playbackStateChanged(state, sourceApp);

// Orchestrator: connect + log
connect(&mediaStatusHandler_, &oaa::hu::MediaStatusChannelHandler::playbackStateChanged,
        this, [this](int state, const QString& appName) {
    eventBus_->publish("aa.media.state", QVariantMap{...});
});
```

### Files to Modify

| File | Changes |
|------|---------|
| `libs/open-androidauto/include/oaa/Channel/MessageIds.hpp` | Add `PLAYBACK_COMMAND = 0x8002` to MediaStatusMessageId, `AUTH_RESULT = 0x8004` to BluetoothMessageId, `BINDING_NOTIFICATION = 0x8004` to InputMessageId |
| `libs/open-androidauto/include/oaa/HU/Handlers/MediaStatusChannelHandler.hpp` | Add `sendPlaybackCommand()`, `togglePlayback()`, `currentPlaybackState_` member |
| `libs/open-androidauto/src/HU/Handlers/MediaStatusChannelHandler.cpp` | Implement outbound command, track state from PlaybackStatus |
| `libs/open-androidauto/include/oaa/Channel/ControlChannel.hpp` | Add `sendVoiceSessionRequest(int sessionType)` |
| `libs/open-androidauto/src/Channel/ControlChannel.cpp` | Implement `sendVoiceSessionRequest()` |
| `libs/open-androidauto/include/oaa/HU/Handlers/BluetoothChannelHandler.hpp` | Add signals for authData/authResult |
| `libs/open-androidauto/src/HU/Handlers/BluetoothChannelHandler.cpp` | Add AUTH_DATA + AUTH_RESULT cases |
| `libs/open-androidauto/include/oaa/HU/Handlers/InputChannelHandler.hpp` | Add hapticFeedbackRequested signal |
| `libs/open-androidauto/src/HU/Handlers/InputChannelHandler.cpp` | Add BINDING_NOTIFICATION case, parse InputBindingNotification |
| `src/core/aa/AndroidAutoOrchestrator.hpp` | Add public API methods for media command + voice session |
| `src/core/aa/AndroidAutoOrchestrator.cpp` | Connect new signals, add public methods, wire to ControlChannel |

### Anti-Patterns to Avoid
- **Putting VoiceSessionRequest on InputChannelHandler** -- protocol puts it on Control channel. Despite CONTEXT.md guidance, the wire protocol is authoritative
- **Using channel-specific message IDs for Control channel messages** -- Control channel uses low message IDs (0x0001-0x001A), not the 0x8000+ range used by feature channels

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Protobuf serialization | Manual byte packing | `msg.SerializeToArray()` | Protobuf handles field encoding, varint compression |
| Playback state tracking | Custom state machine | Simple member variable updated from PlaybackStatus signal | State already arrives via handler -- just cache it |
| BT auth crypto | Any crypto implementation | Raw hex logging only | BT pairing works externally via BlueZ; auth exchange is for protocol compliance logging |

## Common Pitfalls

### Pitfall 1: VoiceSessionRequest on Wrong Channel
**What goes wrong:** Sending VoiceSessionRequest on Input or PhoneStatus channel -- phone ignores it
**Why it happens:** Channel-map.md lists VoiceSessionRequest under "Phone Status" channel; CONTEXT.md suggests InputChannelHandler
**How to avoid:** Wire capture confirms channel 0 (Control), msgId 0x0011. Use `ControlChannel::sendVoiceSessionRequest()`
**Warning signs:** Voice button press produces no response from phone

### Pitfall 2: VoiceSessionType Value 0
**What goes wrong:** Sending `VOICE_SESSION_START = 0` instead of `= 1`
**Why it happens:** Default protobuf int value is 0
**How to avoid:** Proto explicitly notes "Value 0 is invalid (throws null in APK)". Use enum values START=1, STOP=2
**Warning signs:** Phone crashes or ignores voice request

### Pitfall 3: Missing Playback State Tracking for togglePlayback()
**What goes wrong:** `togglePlayback()` sends wrong command because state not tracked
**Why it happens:** Handler receives PlaybackStatus but doesn't cache the state value
**How to avoid:** Add `int currentPlaybackState_ = 0` member, update in `handlePlaybackStatus()`, check in `togglePlayback()`
**Warning signs:** Toggle always sends PAUSE or always sends RESUME

### Pitfall 4: AUTH_DATA Without Proto Schema
**What goes wrong:** Attempting to parse AUTH_DATA payload as a known protobuf message
**Why it happens:** No proto file exists for BluetoothAuthenticationData
**How to avoid:** Log raw hex payload only. Do not parse. CONTEXT.md explicitly says "no proto schema exists"
**Warning signs:** Parse failure warnings in logs

### Pitfall 5: Input Channel 0x8004 Collision with Bluetooth 0x8004
**What goes wrong:** Confusion between InputMessageId::BINDING_NOTIFICATION (0x8004) and BluetoothMessageId::AUTH_RESULT (0x8004)
**Why it happens:** Same numeric value on different channels
**How to avoid:** Each namespace is channel-scoped. The switch/case in each handler's `onMessage()` only sees messages for its own channel
**Warning signs:** None -- this is a code review confusion, not a runtime error

## Code Examples

### MediaPlaybackCommand (outbound, MediaStatusChannelHandler)
```cpp
// Source: MediaPlaybackCommandMessage.proto + established outbound pattern
void MediaStatusChannelHandler::sendPlaybackCommand(int command)
{
    oaa::proto::messages::MediaPlaybackCommand msg;
    msg.set_command(static_cast<oaa::proto::messages::PlaybackCommandType>(command));
    QByteArray data(msg.ByteSizeLong(), '\0');
    msg.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::MediaStatusMessageId::PLAYBACK_COMMAND, data);
}

void MediaStatusChannelHandler::togglePlayback()
{
    if (currentPlaybackState_ == Playing)
        sendPlaybackCommand(oaa::proto::messages::PLAYBACK_COMMAND_PAUSE);
    else
        sendPlaybackCommand(oaa::proto::messages::PLAYBACK_COMMAND_RESUME);
}
```

### VoiceSessionRequest (outbound, ControlChannel)
```cpp
// Source: VoiceSessionRequestMessage.proto + ControlChannel pattern
void ControlChannel::sendVoiceSessionRequest(int sessionType)
{
    oaa::proto::messages::VoiceSessionRequest msg;
    msg.set_session_type(static_cast<oaa::proto::messages::VoiceSessionType>(sessionType));
    QByteArray payload(msg.ByteSizeLong(), '\0');
    msg.SerializeToArray(payload.data(), payload.size());
    emit sendRequested(0, MSG_VOICE_SESSION_REQUEST, payload);
}
```

### InputBindingNotification (inbound, InputChannelHandler)
```cpp
// Source: InputBindingNotificationMessage.proto + HapticFeedbackTypeEnum.proto
void InputChannelHandler::handleBindingNotification(const QByteArray& payload)
{
    oaa::proto::messages::InputBindingNotification msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[InputChannel] failed to parse InputBindingNotification";
        return;
    }
    int feedbackType = msg.feedback_type();
    qDebug() << "[InputChannel] haptic feedback requested:" << feedbackType;
    emit hapticFeedbackRequested(feedbackType);
}
```

### BT AUTH_DATA (inbound, BluetoothChannelHandler)
```cpp
// No proto schema -- raw hex logging
case oaa::BluetoothMessageId::AUTH_DATA:
    qDebug() << "[BluetoothChannel] AUTH_DATA received"
             << "len:" << data.size()
             << "hex:" << data.left(64).toHex(' ');
    emit authDataReceived(data);
    break;
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| VoiceSessionRequest on PhoneStatus channel | Control channel (0), msgId 0x0011 | Verified via wire captures | CONTEXT.md assumption was wrong |
| aasdk manual message framing | open-androidauto IChannelHandler + sendRequested signal | Milestone 4 (Feb 2026) | Clean handler pattern already established |
| No outbound commands | Phase 3 establishes outbound command pattern | This phase | First HU-initiated commands beyond touch/sensor |

## Open Questions

1. **InputBindingNotification message ID**
   - What we know: Not in current MessageIds.hpp, not seen in wire captures (S25 Ultra)
   - What's unclear: Exact message ID. 0x8004 is the next available on input channel after BINDING_RESPONSE (0x8003)
   - Recommendation: Assign 0x8004 provisionally, same pattern as NAV_FOCUS_INDICATION. If wrong, the phone just won't send it and we'll see it in the "unknown message" default handler

2. **BT AUTH_RESULT message ID**
   - What we know: CONTEXT.md says 0x8004. AUTH_DATA is 0x8003. Not observed in S25 captures
   - What's unclear: Whether the phone actually sends this during pairing
   - Recommendation: Add 0x8004 provisionally. Log-only handler. Best-effort compliance

3. **VoiceSessionRequest bidirectionality**
   - What we know: Wire capture shows Phone -> HU direction (channel 0, msgId 0x0011, payload `0801` and `0802`). The ControlChannel already handles inbound
   - What's unclear: Whether the same message ID is used for HU -> Phone direction, or if there's a separate response message
   - Recommendation: Use same msgId 0x0011 for outbound (this is how other HU implementations work based on firmware analysis). Verify on Pi with actual phone

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt6::Test (QTest) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && ctest -R test_name --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| MED-01 | sendPlaybackCommand serializes correct protobuf, togglePlayback sends PAUSE when playing / RESUME when paused | unit | `ctest -R test_media_status_channel_handler --output-on-failure` | No -- Wave 0 |
| MED-02 | sendVoiceSessionRequest serializes VoiceSessionRequest with correct session_type on control channel | unit | `ctest -R test_control_channel --output-on-failure` | No -- Wave 0 (test_oaa_integration exists but doesn't cover voice) |
| BT-01 | AUTH_DATA and AUTH_RESULT cases emit signals with raw payload | unit | `ctest -R test_bluetooth_channel_handler --output-on-failure` | Yes -- extend existing |
| INP-01 | BINDING_NOTIFICATION case parses InputBindingNotification and emits hapticFeedbackRequested | unit | `ctest -R test_input_channel_handler --output-on-failure` | Yes -- extend existing |

### Sampling Rate
- **Per task commit:** `cd build && ctest -R "test_(media_status|bluetooth|input)_channel_handler|test_control" --output-on-failure`
- **Per wave merge:** `cd build && ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_media_status_channel_handler.cpp` -- covers MED-01 (new file, follows existing handler test pattern)
- [ ] Extend `tests/test_bluetooth_channel_handler.cpp` -- covers BT-01 (AUTH_DATA + AUTH_RESULT cases)
- [ ] Extend `tests/test_input_channel_handler.cpp` -- covers INP-01 (BINDING_NOTIFICATION case)
- [ ] Voice session test -- MED-02 can be tested via ControlChannel send method (verify serialization in existing test infrastructure or new test)

## Sources

### Primary (HIGH confidence)
- Wire capture: `libs/open-androidauto/proto/analysis/captures/non_media/2026-02-28-s25-cleanbuild.jsonl` -- VoiceSessionRequest channel/msgId verified
- `libs/open-androidauto/proto/analysis/tools/proto_stream_validator/message_map.py` -- authoritative (ch, msgId) to proto mapping
- `libs/open-androidauto/src/Channel/ControlChannel.cpp` -- existing VoiceSessionRequest handling
- `libs/open-androidauto/proto/docs/channels/media.md` -- MediaPlaybackCommand msgId 0x8002 (Silver confidence)
- `libs/open-androidauto/proto/docs/channels/phone.md` -- VoiceSessionRequest proto definition (Silver confidence)
- Proto files: `MediaPlaybackCommandMessage.proto`, `VoiceSessionRequestMessage.proto`, `InputBindingNotificationMessage.proto`, `HapticFeedbackTypeEnum.proto` -- all verified

### Secondary (MEDIUM confidence)
- `libs/open-androidauto/proto/oaa/bluetooth/BluetoothChannelMessageIdsEnum.proto` -- AUTH_DATA = 0x8003 defined
- `libs/open-androidauto/proto/oaa/input/InputChannelMessageIdsEnum.proto` -- no BINDING_NOTIFICATION entry (provisional assignment needed)

### Tertiary (LOW confidence)
- InputBindingNotification msgId 0x8004 -- provisional assignment, not observed in wire captures
- BT AUTH_RESULT 0x8004 -- not observed in wire captures, CONTEXT.md suggested value

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all proto definitions exist, patterns established in codebase
- Architecture: HIGH -- outbound command pattern proven, channel assignments verified via wire captures
- Pitfalls: HIGH -- VoiceSessionRequest channel correction is the major finding, backed by capture evidence

**Research date:** 2026-03-06
**Valid until:** 2026-04-06 (stable -- protocol definitions unlikely to change)
