# Phase 3: Commands & Authentication - Context

**Gathered:** 2026-03-06
**Status:** Ready for planning

<domain>
## Phase Boundary

Wire HU-initiated commands (media playback control, voice session activation) to the phone, complete the BT authentication data exchange on the Bluetooth channel, and handle InputBindingNotification haptic feedback requests. This is the first phase with **outbound** HU-to-phone commands (Phases 1-2 were receive-only). No new UI triggers -- commands are exposed as handler APIs for future UI wiring.

</domain>

<decisions>
## Implementation Decisions

### Media Playback Commands (MED-01)
- Add `sendPlaybackCommand(PlaybackCommandType)` to `MediaStatusChannelHandler` -- reuse existing channel, no new handler
- Track current playback state internally (handler already receives PlaybackStatus)
- Expose convenience `togglePlayback()` that sends PAUSE if playing, RESUME if paused
- API-only -- no UI trigger wiring in this phase. Future work connects navbar gestures, companion app, etc.
- **Message ID for outbound MediaPlaybackCommand needs research** -- no constant exists in MessageIds.hpp yet. Researcher must determine correct wire ID from captures/APK analysis

### Voice Session Request (MED-02)
- Add `sendVoiceSessionRequest(VoiceSessionType)` to `InputChannelHandler` -- voice activation is logically an input event
- Push-to-talk model: START on press, STOP on release. API exposes both, callers decide timing
- API-only -- no UI trigger in this phase. Future work wires to navbar long-hold or evdev KEY_VOICECOMMAND
- **Pi verification required** -- must confirm Google Assistant actually activates on the S25 via a test trigger
- **Message ID needs research** -- VoiceSessionRequest wire ID not yet defined in MessageIds.hpp

### BT Auth Exchange (BT-01)
- Add case for `AUTH_DATA` (0x8003) in `BluetoothChannelHandler::onMessage()`
- Log raw payload hex (no proto schema exists in submodule for this message)
- If phone expects a response, send minimal acknowledgment
- Add provisional `AUTH_RESULT = 0x8004` to `BluetoothMessageId` namespace -- researcher verifies from captures
- Handle inbound 0x8004 with logging
- **Graceful fallback** -- if auth exchange fails or unexpected data arrives, log warning and continue. Don't break existing BlueZ D-Bus pairing that already works. Auth is additive compliance, not critical path
- No crypto implementation -- real BT pairing works externally

### InputBinding Haptics (INP-01)
- Parse `InputBindingNotification` with `HapticFeedbackType` enum in `InputChannelHandler`
- Log the feedback type at debug level
- Emit Qt signal for future consumers (visual flash, audio click, or actual haptics if hardware added)
- No physical output -- Pi has no haptic motor
- **Message ID needs research** -- BINDING_NOTIFICATION not in MessageIds.hpp. Researcher determines correct ID from captures/APK

### Signal Architecture
- Follows Phase 2 pattern: handlers emit Qt signals, orchestrator connects and logs
- New outbound pattern: handlers expose `send*()` methods that build protobuf and emit `sendRequested` signal
- This outbound pattern already exists in `BluetoothChannelHandler::handlePairingRequest()` and `InputChannelHandler::sendTouchIndication()`

### Claude's Discretion
- Exact signal signatures for new signals
- Log message formatting within established patterns
- Whether to add a `sendMediaCommand()` helper in orchestrator or keep it handler-only
- Test coverage approach (unit tests for command serialization, integration test for Pi verification)
- Temporary test trigger mechanism for Pi verification (CLI flag, debug key, etc.)

</decisions>

<specifics>
## Specific Ideas

- This is the first phase with HU-initiated outbound commands -- establishes the pattern for all future command sending
- Media toggle is the killer convenience -- "just pause/play" without knowing current state
- Voice push-to-talk matches how real head units work (steering wheel button hold)
- BT auth is best-effort -- existing pairing works fine, this just silences protocol warnings
- Three of four features need message ID research before implementation can start

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `MediaStatusChannelHandler` (libs/open-androidauto): already parses PlaybackStatus + Metadata inbound, has `playbackStateChanged` signal. Add outbound command + state tracking
- `InputChannelHandler` (libs/open-androidauto): already sends InputEventIndication via `sendTouchIndication()`. Outbound pattern exists -- add voice session request
- `BluetoothChannelHandler` (libs/open-androidauto): handles PAIRING_REQUEST, sends PAIRING_RESPONSE. Add AUTH_DATA case
- `sendRequested` signal on IChannelHandler base: used by all handlers for outbound messages
- Proto definitions available: `MediaPlaybackCommandMessage.proto` (PAUSE/RESUME/UNKNOWN), `VoiceSessionRequestMessage.proto` (START/STOP), `InputBindingNotificationMessage.proto` (HapticFeedbackType enum), `HapticFeedbackTypeEnum.proto` (SELECT/FOCUS_CHANGE/DRAG_*)

### Established Patterns
- Handler `onMessage()` switch on messageId -> parse protobuf -> emit Qt signal (Phases 1-2)
- Outbound: build protobuf -> serialize to QByteArray -> emit sendRequested(channelId, messageId, data)
- `QByteArray::fromRawData()` for zero-copy payload views on inbound
- Unknown messages logged with hex prefix + payload in default case
- Qt logging categories via `qCDebug(lcAA)` for AA protocol messages

### Integration Points
- `MediaStatusChannelHandler.hpp/.cpp` -- add sendPlaybackCommand(), togglePlayback(), state tracking
- `InputChannelHandler.hpp/.cpp` -- add sendVoiceSessionRequest(), handleBindingNotification()
- `BluetoothChannelHandler.hpp/.cpp` -- add AUTH_DATA + AUTH_RESULT cases
- `MessageIds.hpp` -- add new message ID constants (after research)
- `AndroidAutoOrchestrator.cpp` -- connect new signals from handlers

</code_context>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 03-commands-authentication*
*Context gathered: 2026-03-06*
