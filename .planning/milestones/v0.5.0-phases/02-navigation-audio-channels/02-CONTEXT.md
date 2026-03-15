# Phase 2: Navigation & Audio Channels - Context

**Gathered:** 2026-03-06
**Status:** Ready for planning

<domain>
## Phase Boundary

Prodigy correctly receives and processes navigation guidance (NavigationTurnEvent, NavigationFocusIndication) and per-channel audio state (AudioFocusState, AudioStreamType) from the phone. This is protocol compliance — handle inbound messages that are currently ignored. No new UI rendering, no changes to audio playback behavior.

</domain>

<decisions>
## Implementation Decisions

### NavigationTurnEvent (0x8004)
- Parse and expose all fields: road_name, maneuver_type, turn_direction, turn_icon, distance_meters, distance_unit
- Emit Qt signals with full data so future UI can render turn cards
- Turn icon PNG bytes passed through as QByteArray — no decoding in the handler, future UI layer handles that

### NavigationNotification Enhancement
- Enhance existing parsing beyond current first-step extraction
- Extract lane guidance data (NavigationLaneDirection with shape + is_recommended)
- Extract multiple steps (multi-step lookahead), not just steps(0)
- Extract ETA/destination details from destinations list

### Navigation Focus Negotiation
- Handle NavigationFocusRequest: always auto-grant with NavigationFocusResponse (Prodigy has no native nav to compete)
- Track nav focus state internally in NavigationChannelHandler
- Handle NavigationFocusIndication: parse, update tracked focus state, log opaque focus_data payload
- All focus handling lives in NavigationChannelHandler (self-contained, not orchestrator)

### AudioFocusState (0x8021) Per-Channel
- Parse the protobuf on each audio channel (4/5/6)
- Log at debug level
- Track per-channel focus state as member variable on AudioChannelHandler
- Emit Qt signal on state change

### AudioStreamType (0x8022) Per-Channel
- Parse the enum on each audio channel
- Log at debug level
- Track per-channel stream type as member variable on AudioChannelHandler
- Emit Qt signal on type received

### Signal Architecture
- Qt signals only — handlers emit, orchestrator connects and logs
- No IEventBus events or Q_PROPERTYs in this phase
- Consistent pattern: both nav and audio use handler signals -> orchestrator log connections
- Future work wires signals to UI/AudioService when there's a consumer

### Code Layer
- Handler changes live in open-androidauto library (NavigationChannelHandler, AudioChannelHandler)
- Prodigy orchestrator connects signals and logs — no app-layer parsing
- Follows existing pattern established in Phase 1

### Claude's Discretion
- Exact signal signatures (parameter types, names)
- Log message formatting within established patterns
- Whether to add new MessageId constants or reuse existing ones
- Test coverage approach for new handler code

</decisions>

<specifics>
## Specific Ideas

- NavigationTurnEvent is the primary ~1Hz guidance message — this is what a future turn-card UI would consume
- NavigationNotification enhancement gives richer data for future lane-guidance display
- Per-channel audio state tracking prepares for future AudioService integration (ducking, volume) without changing behavior now
- "Protocol compliance" theme: handle what the phone sends, don't change how Prodigy behaves yet

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `NavigationChannelHandler` (libs/open-androidauto): already parses NavState, NavStep (Notification), NavDistance. Needs TurnEvent + focus handling added
- `AudioChannelHandler` (libs/open-androidauto): handles AV setup/start/stop/media. Needs AudioFocusState + AudioStreamType cases in onMessage switch
- `AndroidAutoOrchestrator`: already wires navHandler_ and all three audio handlers. Signal connections go here
- Proto definitions at v1.0: NavigationTurnEventMessage.pb.h, AudioFocusStateMessage.pb.h, AudioStreamTypeMessage.pb.h all available

### Established Patterns
- Handler `onMessage()` switch on messageId -> parse protobuf -> emit Qt signal
- `QByteArray::fromRawData()` for zero-copy payload views
- `ShortDebugString()` for protobuf logging (Boost.Log truncation workaround)
- Unknown messages logged with hex prefix + payload in default case

### Integration Points
- `NavigationChannelHandler::onMessage()` — add case for NAV_TURN_EVENT (0x8004), NAV_FOCUS_REQUEST, NAV_FOCUS_INDICATION
- `AudioChannelHandler::onMessage()` — add cases for 0x8021 (AudioFocusState) and 0x8022 (AudioStreamType)
- `AndroidAutoOrchestrator.cpp` — connect new signals from handlers
- `NavigationChannelHandler.hpp` — add new signals, sendRequested for focus response
- `AudioChannelHandler.hpp` — add new signals and state members

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-navigation-audio-channels*
*Context gathered: 2026-03-06*
