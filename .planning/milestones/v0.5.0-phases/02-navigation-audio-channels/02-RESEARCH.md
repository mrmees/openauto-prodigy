# Phase 2: Navigation & Audio Channels - Research

**Researched:** 2026-03-06
**Domain:** AA protocol handler extension (NavigationTurnEvent, NavigationFocus, AudioFocusState, AudioStreamType)
**Confidence:** HIGH

## Summary

This phase extends two existing open-androidauto handlers (NavigationChannelHandler and AudioChannelHandler) to process inbound messages that are currently hitting `default:` / `unknown` cases. All proto definitions already exist and compile. All handler classes already have the right base class, signal infrastructure, and `onMessage()` switch structure. The work is purely additive: new switch cases, protobuf parsing, state tracking, and Qt signal emission.

The navigation focus negotiation (NavigationFocusRequest/Response) already works -- it flows through the Control Channel and AASession auto-grants. The remaining NAV-02 work is handling NavigationFocusIndication, whose message ID and channel routing are currently unknown (marked "--" in protocol docs). This needs investigation during implementation via protocol capture logs or will appear as an unknown message in existing logging.

**Primary recommendation:** Add switch cases for known message IDs (0x8004 on nav channel, 0x8021/0x8022 on audio channels), parse protos, emit signals, wire orchestrator log connections. Nav focus indication handling depends on identifying its message ID from protocol captures.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- NavigationTurnEvent (0x8004): Parse all fields (road_name, maneuver_type, turn_direction, turn_icon, distance_meters, distance_unit), emit Qt signals, pass turn icon PNG as QByteArray
- NavigationNotification Enhancement: Extract lane guidance, multi-step lookahead, ETA/destination details
- Navigation Focus: Auto-grant NavigationFocusRequest, track state in NavigationChannelHandler, handle NavigationFocusIndication with opaque focus_data logging
- AudioFocusState (0x8021): Parse per audio channel (4/5/6), debug log, track as member, emit signal on change
- AudioStreamType (0x8022): Parse per audio channel, debug log, track as member, emit signal
- Signal Architecture: Qt signals only -- no IEventBus events or Q_PROPERTYs this phase
- Code Layer: Handler changes in open-androidauto library; orchestrator connects and logs

### Claude's Discretion
- Exact signal signatures (parameter types, names)
- Log message formatting within established patterns
- Whether to add new MessageId constants or reuse existing ones
- Test coverage approach for new handler code

### Deferred Ideas (OUT OF SCOPE)
None
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| NAV-01 | HU parses NavigationTurnEvent (0x8004) and exposes turn type, road name, distance, and icon data | Proto `NavigationTurnEvent` exists with all fields. Message ID 0x8004 confirmed on nav channel. Handler already includes the pb.h but has no switch case. |
| NAV-02 | HU handles NavigationFocusIndication from phone to complete focus negotiation flow | NavigationFocusRequest/Response already handled on Control Channel (0x000d/0x000e). NavigationFocusIndication proto exists but message ID unknown -- may appear on nav channel or control channel. |
| AUD-01 | HU receives and processes AudioFocusState (0x8021) per-channel focus indicator | Proto `AudioFocusState` is a simple `bool has_focus` message. Message ID 0x8021 confirmed. Currently hits default case in AudioChannelHandler. |
| AUD-02 | HU receives and processes AudioStreamType (0x8022) per-channel stream type | Proto `AudioStreamType` wraps `AudioStreamType.Enum` (MEDIA=1, GUIDANCE=2). Message ID 0x8022 confirmed. Currently hits default case. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| protobuf | system (libprotobuf) | Proto message parsing | Already in use, all pb.h files generated |
| Qt 6 Core | 6.4+ (VM) / 6.8 (Pi) | QObject signals, QByteArray | Already the project foundation |

### Supporting
No new libraries needed. All work uses existing protobuf + Qt signal infrastructure.

## Architecture Patterns

### Existing Handler Pattern (follow exactly)
```
NavigationChannelHandler::onMessage()
  switch (messageId) {
    case KNOWN_ID:
      handleXxx(data);   // private method
      break;
    default:
      log unknown;
  }

handleXxx(payload):
  Proto msg;
  if (!msg.ParseFromArray(...)) { qWarning; return; }
  // extract fields
  qInfo/qDebug << "[Channel] ..."
  emit signalName(args);
```

### Recommended Signal Signatures

**NavigationChannelHandler new signals:**
```cpp
// NAV-01: Turn event (~1Hz during active navigation)
void navigationTurnEvent(const QString& roadName, int maneuverType,
                         int turnDirection, const QByteArray& turnIcon,
                         int distanceMeters, int distanceUnit);

// NAV-02: Focus state change
void navigationFocusChanged(bool hasFocus);
```

**AudioChannelHandler new signals:**
```cpp
// AUD-01: Focus state per channel
void audioFocusStateChanged(bool hasFocus);

// AUD-02: Stream type per channel
void audioStreamTypeChanged(int streamType);
```

### MessageIds.hpp Additions

Add to `NavigationMessageId` namespace:
```cpp
constexpr uint16_t NAV_TURN_EVENT     = 0x8004;
// NAV_FOCUS_REQUEST/RESPONSE are on Control Channel (0x000d/0x000e), not here
```

Add to `AVMessageId` namespace:
```cpp
constexpr uint16_t AUDIO_FOCUS_STATE  = 0x8021;
constexpr uint16_t AUDIO_STREAM_TYPE  = 0x8022;
```

### Orchestrator Wiring Pattern
```cpp
// In onNewConnection(), after existing signal connections:
connect(&navHandler_, &oaa::hu::NavigationChannelHandler::navigationTurnEvent,
        this, [](const QString& road, int maneuver, int dir,
                 const QByteArray& icon, int dist, int unit) {
    qCDebug(lcAA) << "[Nav] turn:" << road << "maneuver:" << maneuver
                   << "dir:" << dir << "dist:" << dist << unit
                   << "icon:" << icon.size() << "bytes";
});

connect(&mediaAudioHandler_, &oaa::hu::AudioChannelHandler::audioFocusStateChanged,
        this, [](bool hasFocus) {
    qCDebug(lcAA) << "[Audio:Media] focus:" << hasFocus;
});
// ... repeat for speech and system handlers
```

### Anti-Patterns to Avoid
- **Parsing protos in the orchestrator:** All parsing lives in handlers. Orchestrator only connects signals and logs.
- **Adding IEventBus events now:** CONTEXT.md explicitly says "Qt signals only" this phase.
- **Modifying Control Channel for nav focus:** NavigationFocusRequest/Response already work. Only NavigationFocusIndication needs handling, and it may arrive on the nav channel.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Proto parsing | Manual byte parsing | Generated .pb.h ParseFromArray() | Protos already compiled, tested |
| Signal dispatch | Custom callback system | Qt signals/slots | Already the project pattern |
| Message ID routing | Custom dispatch table | switch statement in onMessage() | Established pattern |

## Common Pitfalls

### Pitfall 1: NavigationFocusIndication Message ID Unknown
**What goes wrong:** We don't know what message ID NavigationFocusIndication arrives as, or whether it comes on the Navigation channel or Control channel.
**Why it happens:** Protocol docs show "--" for the message ID. The proto exists but the channel/ID mapping was never captured.
**How to avoid:** Check existing protocol capture logs for unknown messages on the nav channel. The existing "unknown msgId" logging in NavigationChannelHandler will show it. If it's on the control channel, check AASession's unhandled message logging. May need a live capture session to identify.
**Warning signs:** If no NavigationFocusIndication is ever seen, it may only be sent when native navigation competes with projected (which Prodigy never does).

### Pitfall 2: AVMessageId 0x8004 Collision with Nav 0x8004
**What goes wrong:** Both `AVMessageId::ACK_INDICATION` and nav `NAV_TURN_EVENT` are 0x8004, causing confusion.
**Why it happens:** Message IDs are channel-scoped, not global. 0x8004 means different things on AV channels vs the Navigation channel.
**How to avoid:** This is already handled correctly -- each handler has its own switch. Just don't add `NAV_TURN_EVENT` to the `AVMessageId` namespace.

### Pitfall 3: AudioFocusState Proto vs Enum Naming Collision
**What goes wrong:** There's an `AudioFocusState` message (simple bool wrapper, msg 0x8021) AND an `AudioFocusState` enum (GAIN/LOSS/etc). They're different things.
**Why it happens:** Both exist in the proto definitions with the same name but different packages/purposes.
**How to avoid:** The message is `oaa::proto::messages::AudioFocusState` (has_focus bool). The enum is `oaa::proto::enums::AudioFocusState::Enum`. Use fully qualified names in includes and code.

### Pitfall 4: NavigationNotification Enhancement Scope
**What goes wrong:** CONTEXT.md asks for enhanced NavigationNotification parsing (lane guidance, multi-step, ETA) but this is not in the formal requirements (NAV-01/NAV-02).
**Why it happens:** It was discussed as part of the phase but the requirements only cover TurnEvent and FocusIndication.
**How to avoid:** Include the enhancement as a task but note it's a bonus beyond the four formal requirements. The existing `handleNavStep()` already parses `steps(0)` -- extending it to iterate all steps and extract lanes is incremental.

### Pitfall 5: ProtocolLogger Missing Names
**What goes wrong:** New message IDs (0x8021, 0x8022 on audio channels; 0x8004 on nav channel) don't have human-readable names in ProtocolLogger.
**Why it happens:** ProtocolLogger has a name mapping function that returns strings for known message IDs. New IDs need entries.
**How to avoid:** Add entries to the ProtocolLogger name mapping for the new message IDs. Nav channel 0x8004 already has "NAVIGATION_TURN_EVENT" in the logger (line 310 of ProtocolLogger.cpp).

## Code Examples

### NavigationTurnEvent Handling (NAV-01)
```cpp
// In NavigationChannelHandler.cpp
#include "oaa/navigation/NavigationTurnEventMessage.pb.h"  // already included

void NavigationChannelHandler::handleTurnEvent(const QByteArray& payload)
{
    oaa::proto::messages::NavigationTurnEvent msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[NavChannel] failed to parse NavigationTurnEvent";
        return;
    }

    QString roadName = msg.has_road_name()
        ? QString::fromStdString(msg.road_name()) : QString();
    int maneuver = msg.has_maneuver_type() ? msg.maneuver_type() : 0;
    int direction = msg.has_turn_direction() ? msg.turn_direction() : 0;
    QByteArray icon = msg.has_turn_icon()
        ? QByteArray(msg.turn_icon().data(), msg.turn_icon().size())
        : QByteArray();
    int distance = msg.has_distance_meters() ? msg.distance_meters() : -1;
    int unit = msg.has_distance_unit() ? msg.distance_unit() : 0;

    qInfo() << "[NavChannel] turn:" << roadName
            << "maneuver:" << maneuver << "dir:" << direction
            << "dist:" << distance << "unit:" << unit
            << "icon:" << icon.size() << "bytes";

    emit navigationTurnEvent(roadName, maneuver, direction, icon, distance, unit);
}
```

### AudioFocusState Handling (AUD-01)
```cpp
// In AudioChannelHandler.cpp
#include "oaa/audio/AudioFocusStateMessage.pb.h"

void AudioChannelHandler::handleAudioFocusState(const QByteArray& payload)
{
    oaa::proto::messages::AudioFocusState msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[AudioChannel" << channelId_ << "] failed to parse AudioFocusState";
        return;
    }

    bool hasFocus = msg.has_focus();
    qDebug() << "[AudioChannel" << channelId_ << "] focus state:" << hasFocus;

    if (hasFocus_ != hasFocus) {
        hasFocus_ = hasFocus;
        emit audioFocusStateChanged(hasFocus);
    }
}
```

### AudioStreamType Handling (AUD-02)
```cpp
// In AudioChannelHandler.cpp
#include "oaa/audio/AudioStreamTypeMessage.pb.h"

void AudioChannelHandler::handleAudioStreamType(const QByteArray& payload)
{
    oaa::proto::messages::AudioStreamType msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[AudioChannel" << channelId_ << "] failed to parse AudioStreamType";
        return;
    }

    int type = msg.stream_type();
    qDebug() << "[AudioChannel" << channelId_ << "] stream type:" << type;

    streamType_ = type;
    emit audioStreamTypeChanged(type);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Ignore unknown nav messages | Parse TurnEvent (0x8004) | This phase | Enables future turn-card UI |
| Ignore audio focus/type on channels | Track per-channel state | This phase | Enables future ducking/routing |
| Nav focus: just echo payload back | Same + track indication state | This phase | Completes focus handshake |

## Key Technical Details

### Message ID to Handler Mapping

| Channel | MsgID | Proto Message | Handler |
|---------|-------|---------------|---------|
| Navigation (9) | 0x8003 | NavigationState | NavigationChannelHandler (existing) |
| Navigation (9) | 0x8004 | NavigationTurnEvent | NavigationChannelHandler (NEW) |
| Navigation (9) | 0x8006 | NavigationNotification | NavigationChannelHandler (ENHANCE) |
| Navigation (9) | 0x8007 | NavigationNextTurnDistanceEvent | NavigationChannelHandler (existing) |
| Control (0) | 0x000d | NavigationFocusRequest | ControlChannel (existing, auto-grants) |
| Control (0) | 0x000e | NavigationFocusResponse | ControlChannel (existing) |
| Navigation (9) | ??? | NavigationFocusIndication | NavigationChannelHandler (NEW -- ID TBD) |
| MediaAudio (4) | 0x8021 | AudioFocusState | AudioChannelHandler (NEW) |
| SpeechAudio (5) | 0x8021 | AudioFocusState | AudioChannelHandler (NEW) |
| SystemAudio (6) | 0x8021 | AudioFocusState | AudioChannelHandler (NEW) |
| MediaAudio (4) | 0x8022 | AudioStreamType | AudioChannelHandler (NEW) |
| SpeechAudio (5) | 0x8022 | AudioStreamType | AudioChannelHandler (NEW) |
| SystemAudio (6) | 0x8022 | AudioStreamType | AudioChannelHandler (NEW) |

### Proto Field Summary

**NavigationTurnEvent** (silver confidence):
- `road_name` (string, optional)
- `maneuver_type` (ManeuverType.Enum -- 50 values, optional)
- `turn_direction` (TurnSide.Enum -- LEFT/RIGHT/UNKNOWN, optional)
- `turn_icon` (bytes -- PNG image data, optional)
- `distance_meters` (int32, optional)
- `distance_unit` (int32, optional)

**AudioFocusState** (silver confidence):
- `has_focus` (bool, optional) -- simple binary indicator

**AudioStreamType** (silver/bronze confidence):
- `stream_type` (AudioStreamType.Enum -- NONE=0, MEDIA=1, GUIDANCE=2)
- Note: enum values are bronze confidence (inferred, not wire-verified)

**NavigationFocusIndication** (silver confidence):
- `focus_data` (bytes, required) -- opaque payload, contents unknown

### Files to Modify

**open-androidauto library:**
1. `include/oaa/Channel/MessageIds.hpp` -- add NAV_TURN_EVENT, AUDIO_FOCUS_STATE, AUDIO_STREAM_TYPE constants
2. `include/oaa/HU/Handlers/NavigationChannelHandler.hpp` -- add signals, private handlers, state members
3. `src/HU/Handlers/NavigationChannelHandler.cpp` -- add switch cases, handler implementations
4. `include/oaa/HU/Handlers/AudioChannelHandler.hpp` -- add signals, state members
5. `src/HU/Handlers/AudioChannelHandler.cpp` -- add switch cases, handler implementations
6. `src/Messenger/ProtocolLogger.cpp` -- add name mappings for 0x8021, 0x8022 on audio channels

**Prodigy app:**
7. `src/core/aa/AndroidAutoOrchestrator.cpp` -- connect new signals for logging

## Open Questions

1. **NavigationFocusIndication message ID**
   - What we know: Proto exists, it's sent by the phone, carries opaque `focus_data` bytes
   - What's unclear: Which channel it arrives on and what message ID it uses
   - Recommendation: Check protocol capture logs for unknown messages on nav channel during navigation start/stop. If not found, handle as a known-unknown -- add a comment and log any unrecognized nav channel messages that might be it

2. **AudioStreamType enum values**
   - What we know: MEDIA=1 and GUIDANCE=2 per APK analysis (bronze confidence)
   - What's unclear: Whether additional values exist beyond 0/1/2
   - Recommendation: Log the raw int value so wire captures can validate. Don't switch on enum values -- just pass through as int

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test (QTest) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd build && ctest -R "test_audio_channel" --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| NAV-01 | NavigationTurnEvent parsed and signal emitted | unit | `cd build && ctest -R test_navigation_channel --output-on-failure` | No -- Wave 0 |
| NAV-02 | NavigationFocusIndication parsed and logged | unit | `cd build && ctest -R test_navigation_channel --output-on-failure` | No -- Wave 0 |
| AUD-01 | AudioFocusState parsed per-channel, signal emitted on change | unit | `cd build && ctest -R test_audio_channel --output-on-failure` | Exists (extend) |
| AUD-02 | AudioStreamType parsed per-channel, signal emitted | unit | `cd build && ctest -R test_audio_channel --output-on-failure` | Exists (extend) |

### Sampling Rate
- **Per task commit:** `cd build && ctest -R "test_audio_channel|test_navigation_channel" --output-on-failure`
- **Per wave merge:** `cd build && ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_navigation_channel_handler.cpp` -- covers NAV-01, NAV-02 (new file needed; no nav handler tests exist)
- [ ] Extend `tests/test_audio_channel_handler.cpp` -- covers AUD-01, AUD-02 (file exists, add new test cases)

## Sources

### Primary (HIGH confidence)
- Proto files in `libs/open-androidauto/proto/oaa/navigation/` and `libs/open-androidauto/proto/oaa/audio/` -- all field definitions, message structures
- Existing handler source: `NavigationChannelHandler.cpp`, `AudioChannelHandler.cpp` -- current patterns
- `ControlChannel.cpp` lines 128-130 -- nav focus already auto-granted at session level
- `ProtocolLogger.cpp` line 310 -- 0x8004 already named "NAVIGATION_TURN_EVENT"
- `proto/docs/channels/nav.md` -- channel-level protocol documentation

### Secondary (MEDIUM confidence)
- Proto audit YAML files -- cross-version APK class mappings
- `proto/analysis/tools/proto_stream_validator/message_map.py` -- message ID to proto mapping

### Tertiary (LOW confidence)
- AudioStreamType enum values (MEDIA=1, GUIDANCE=2) -- bronze confidence, APK-inferred only
- NavigationFocusIndication message ID -- unknown, marked "--" in all docs

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies, all protos already compile
- Architecture: HIGH -- follows exact patterns from existing handlers
- Pitfalls: HIGH -- well-understood domain, main risk is unknown FocusIndication msgID
- Proto schemas: MEDIUM -- silver confidence on most fields, bronze on AudioStreamType enum

**Research date:** 2026-03-06
**Valid until:** 2026-04-06 (stable domain, proto definitions won't change)
