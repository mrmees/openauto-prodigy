---
status: diagnosed
trigger: "3-finger gesture overlay doesn't work at all on the Pi"
created: 2026-03-01T00:00:00Z
updated: 2026-03-01T00:00:00Z
---

## Current Focus

hypothesis: gestureDetected signal is never connected to GestureOverlay.show()
test: grep for all connections to gestureDetected
expecting: zero connections found
next_action: return diagnosis

## Symptoms

expected: 3-finger tap on touchscreen shows GestureOverlay (volume/brightness/home controls)
actual: nothing happens — overlay never appears
errors: none
reproduction: tap 3 fingers on Pi touchscreen at any time
started: "has not worked for a long time"

## Eliminated

- hypothesis: GestureOverlay z-order or visibility issue
  evidence: z=999, visible toggles correctly in show()/dismiss(), QML structure is correct in Shell.qml
  timestamp: 2026-03-01

- hypothesis: GestureOverlay.qml has broken QML
  evidence: QML is well-formed — show() sets visible=true, dismiss() sets visible=false, timer works
  timestamp: 2026-03-01

## Evidence

- timestamp: 2026-03-01
  checked: EvdevTouchReader.cpp line 252-253
  found: "When not grabbed, discard events" — `if (!grabbed_.load()) continue;` discards ALL events when ungrabbed, including the ones needed for gesture detection
  implication: When AA is not connected, grabbed_=false, so checkGesture() is never called. But even during AA, gesture detection DOES run (line 358-365).

- timestamp: 2026-03-01
  checked: grep for "gestureDetected" across entire src/
  found: Signal is declared (header line 72), emitted (cpp line 331), but NEVER connected to anything
  implication: Even when 3-finger gesture IS detected during AA (when grabbed), the signal goes nowhere. Nobody calls GestureOverlay.show().

- timestamp: 2026-03-01
  checked: AndroidAutoPlugin.cpp lines 112-127
  found: sidebarVolumeSet and sidebarHome signals are connected, but gestureDetected is NOT connected
  implication: This was an oversight — sidebar signals were wired up but gesture signal was forgotten.

- timestamp: 2026-03-01
  checked: main.cpp
  found: No gestureDetected connection anywhere in main.cpp either
  implication: The signal has NEVER been wired to anything in the app lifecycle.

## Resolution

root_cause: EvdevTouchReader::gestureDetected() signal is emitted but never connected to GestureOverlay.show(). Two independent issues: (1) the signal is not wired to any slot/lambda that would trigger the QML overlay, and (2) when AA is NOT connected (grabbed_=false), all evdev events are discarded before gesture detection runs, so 3-finger tap on the home screen is completely invisible to the app.
fix: empty
verification: empty
files_changed: []
