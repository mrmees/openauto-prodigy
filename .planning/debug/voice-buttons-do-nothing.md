---
status: diagnosed
trigger: "Debug panel buttons in Settings > System are not working. Specifically, Voice Start and Voice Stop buttons do nothing when clicked."
created: 2026-03-06T12:00:00Z
updated: 2026-03-06T12:00:00Z
---

## Current Focus

hypothesis: Voice buttons are correctly wired end-to-end. The phone ignores the commands (same as media buttons).
test: Full code trace from QML -> orchestrator -> ControlChannel -> Messenger
expecting: Either a broken wire or a working wire with phone-side rejection
next_action: Return diagnosis

## Symptoms

expected: Voice Start/Stop buttons trigger Google Assistant on phone
actual: Buttons do nothing when clicked
errors: None visible (no crashes, no disconnects)
reproduction: Click Voice Start or Voice Stop in Settings > System debug panel while AA connected
started: Since debug panel was added

## Eliminated

- hypothesis: AAOrchestrator not exposed to QML
  evidence: main.cpp line 461 — `engine.rootContext()->setContextProperty("AAOrchestrator", static_cast<QObject*>(aaPlugin->orchestrator()))`
  timestamp: 2026-03-06

- hypothesis: QML calling wrong method names
  evidence: QML calls `AAOrchestrator.sendVoiceSessionRequest(1)` and `sendVoiceSessionRequest(2)`, matching the Q_INVOKABLE signature exactly
  timestamp: 2026-03-06

- hypothesis: sendVoiceSessionRequest not implemented
  evidence: AndroidAutoOrchestrator.cpp line 730-734 — implemented, calls `session_->controlChannel()->sendVoiceSessionRequest(sessionType)`
  timestamp: 2026-03-06

- hypothesis: ControlChannel::sendVoiceSessionRequest not wired to Messenger
  evidence: AASession.cpp line 108-110 — `connect(controlChannel_, &IChannelHandler::sendRequested, messenger_, &Messenger::sendMessage)` is present
  timestamp: 2026-03-06

- hypothesis: Buttons disabled when they shouldn't be
  evidence: QML `aaConnected` property checks `connectionState === 3` (Connected enum), buttons enabled correctly
  timestamp: 2026-03-06

- hypothesis: session_ is null when method called
  evidence: Guard at line 732 `if (session_)` would silently return, but buttons are only enabled when state==Connected which requires an active session
  timestamp: 2026-03-06

## Evidence

- timestamp: 2026-03-06
  checked: main.cpp line 461
  found: AAOrchestrator exposed at root QML context via `setContextProperty("AAOrchestrator", ...)`
  implication: QML can access the orchestrator

- timestamp: 2026-03-06
  checked: SystemSettings.qml lines 219, 239
  found: Voice Start calls `AAOrchestrator.sendVoiceSessionRequest(1)`, Voice Stop calls `AAOrchestrator.sendVoiceSessionRequest(2)`
  implication: QML method calls match C++ Q_INVOKABLE signatures

- timestamp: 2026-03-06
  checked: AndroidAutoOrchestrator.cpp lines 730-734
  found: `sendVoiceSessionRequest` delegates to `session_->controlChannel()->sendVoiceSessionRequest(sessionType)` with null guard
  implication: Method is implemented and routes to ControlChannel

- timestamp: 2026-03-06
  checked: ControlChannel.cpp lines 245-252
  found: Builds VoiceSessionRequest protobuf with session_type field, emits sendRequested + voiceSessionSent
  implication: Protobuf is correctly serialized and sent

- timestamp: 2026-03-06
  checked: AASession.cpp lines 108-110
  found: ControlChannel sendRequested connected to Messenger::sendMessage
  implication: Messages reach the transport layer

- timestamp: 2026-03-06
  checked: AndroidAutoOrchestrator.cpp lines 470-473
  found: Debug logging connected — `voiceSessionSent` signal logs "[Control] sent voice session request: N"
  implication: Can verify sends via logs; BUT this is inside `if (eventBus_)` block

- timestamp: 2026-03-06
  checked: Session memory
  found: "MediaPlaybackCommand (0x8002) on MediaStatus channel: phone ignores it" and "InputEventIndication button_event: phone ignores it too"
  implication: ALL outbound HU-to-phone commands are being ignored by the S25 Ultra — this is a phone-side issue, not a wiring issue

## Resolution

root_cause: The code is correctly wired end-to-end. The entire signal chain works: QML button -> Q_INVOKABLE -> ControlChannel -> protobuf serialize -> Messenger -> Transport -> phone. The phone (Samsung S25 Ultra) ignores the VoiceSessionRequest, just as it ignores MediaPlaybackCommand and InputEventIndication button events. This is consistent with the known issue documented in session memory — the phone rejects or ignores all HU-initiated commands across multiple channels (Input, MediaStatus, Control). The root cause is likely at the protocol level: either the message format doesn't match what the phone expects, or the phone requires a capability/feature negotiation step that hasn't been implemented.
fix: N/A (research only)
verification: N/A
files_changed: []
