---
status: diagnosed
trigger: "No '[Bluetooth] auth data received' log during BT pairing. Channel opens, pairing works, but auth data handler never fires."
created: 2026-03-06T12:00:00Z
updated: 2026-03-06T12:01:00Z
---

## Current Focus

hypothesis: CONFIRMED - phone never sends AUTH_DATA (0x8003) on the BT channel. This is expected behavior.
test: checked protocol capture baseline + code signal chain
expecting: n/a - root cause found
next_action: return diagnosis

## Symptoms

expected: "[Bluetooth] auth data received" log message appears during BT pairing
actual: No such log message appears. BT channel opens, pairing works.
errors: none (silent failure)
reproduction: pair phone via Bluetooth
started: after 03-02 implementation

## Eliminated

- hypothesis: signal connection is missing in orchestrator
  evidence: connection exists at AndroidAutoOrchestrator.cpp:494, properly connects btHandler_.authDataReceived
  timestamp: 2026-03-06T12:00:30Z

- hypothesis: signal is never emitted by BluetoothChannelHandler
  evidence: BluetoothChannelHandler.cpp:42 does emit authDataReceived(data) when messageId == AUTH_DATA (0x8003). Code is correct.
  timestamp: 2026-03-06T12:00:40Z

- hypothesis: eventBus_ is null so the connect block is skipped
  evidence: even if eventBus_ were null, the BluetoothChannelHandler itself has qDebug logging at line 40-41 that would fire regardless of the orchestrator connection. The handler-level log doesn't appear either.
  timestamp: 2026-03-06T12:00:50Z

## Evidence

- timestamp: 2026-03-06T12:00:20Z
  checked: BluetoothChannelHandler.cpp onMessage() switch
  found: AUTH_DATA case (0x8003) correctly emits authDataReceived signal with qDebug log
  implication: handler code is correct, issue is upstream (no message arrives)

- timestamp: 2026-03-06T12:00:30Z
  checked: AndroidAutoOrchestrator.cpp lines 493-497
  found: connect(&btHandler_, &BluetoothChannelHandler::authDataReceived, ...) exists inside if(eventBus_) block
  implication: connection exists but is guarded by eventBus_ being non-null. Secondary issue (see below).

- timestamp: 2026-03-06T12:00:40Z
  checked: Protocol capture baseline (2026-02-28-s25-cleanbuild.normalized.json)
  found: Only BT channel messages are PAIRING_REQUEST (0x8001) and PAIRING_RESPONSE (0x8002). Zero AUTH_DATA (0x8003) messages in entire capture.
  implication: The S25 Ultra phone NEVER sends AUTH_DATA during normal BT pairing flow

- timestamp: 2026-03-06T12:00:50Z
  checked: GAL protocol reference + proto definitions
  found: BluetoothAuthenticationData exists in protocol spec alongside BluetoothPairingRequest. AUTH_DATA is a real message type but appears to be part of a separate BT auth flow (possibly related to numeric comparison or certificate-based pairing) that the current already_paired=true response bypasses.
  implication: By responding with already_paired=true + status=OK, the phone skips the authentication data exchange entirely

- timestamp: 2026-03-06T12:00:55Z
  checked: MessageIds.hpp BluetoothMessageId namespace
  found: AUTH_DATA=0x8003, AUTH_RESULT=0x8004 defined. AUTH_RESULT comment says "Provisional -- no proto schema, raw payload only"
  implication: These are speculative message IDs based on protocol research, never observed in practice

## Resolution

root_cause: The phone never sends AUTH_DATA (0x8003) on the Bluetooth channel. The handler code and signal connections are all correct, but the phone skips the authentication data exchange because the HU responds to PAIRING_REQUEST with already_paired=true + status=OK. The AUTH_DATA/AUTH_RESULT messages appear to be part of an alternative BT auth flow (possibly numeric comparison or certificate-based) that is bypassed when the HU claims the device is already paired. This is normal, expected behavior -- not a bug.
fix:
verification:
files_changed: []
