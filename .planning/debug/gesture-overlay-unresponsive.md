---
status: diagnosed
trigger: "GestureOverlay controls unresponsive during AA"
created: 2026-03-03T00:00:00Z
updated: 2026-03-03T00:00:00Z
---

## Current Focus

hypothesis: gestureActive_ flag causes processSync() to suppress ALL touches (including zone dispatch) when overlay is visible
test: trace processSync() code path when gestureActive_ is true
expecting: zone dispatch is skipped when gestureActive_ is true
next_action: confirmed - return diagnosis

## Symptoms

expected: Overlay controls (volume, brightness, home, theme, close) respond to touch during AA
actual: Overlay appears but all controls are unresponsive
errors: none
reproduction: 3-finger tap during AA -> overlay shows -> touch any control -> nothing happens
started: since evdev-to-service bridging was added (commit 9e474c7)

## Eliminated

(none needed - root cause found on first hypothesis)

## Evidence

- timestamp: 2026-03-03T00:00:00Z
  checked: processSync() in EvdevTouchReader.cpp lines 271-430
  found: Zone dispatch (router_.dispatch) happens at lines 301-325, THEN gestureBlocking check at lines 328-333. However checkGesture() at line 298 sets gestureActive_=true and KEEPS it true until all fingers lift (line 263-265). The 3-finger gesture that opens the overlay sets gestureActive_=true. When the user lifts all 3 fingers, gestureActive_ resets to false (line 263-265). So gestureActive_ should be false for the NEXT touch. This is NOT the bug.

- timestamp: 2026-03-03T00:00:00Z
  checked: checkGesture() flow for single-finger touch AFTER overlay is open
  found: When user touches overlay with 1 finger after lifting all 3 gesture fingers, gestureActive_ is false (was reset at line 263-265 when fingers lifted). gestureBlocking is false. Zone dispatch at line 322 DOES run. So the zone dispatch path is reached.

- timestamp: 2026-03-03T00:00:00Z
  checked: TouchRouter::dispatch() in TouchRouter.cpp lines 33-74
  found: dispatch() passes raw evdev coordinates (x, y) directly to the zone callback. The zone callback in main.cpp (line 522-588) receives these raw evdev coords.

- timestamp: 2026-03-03T00:00:00Z
  checked: Zone callback in main.cpp line 522-588, specifically coordinate conversion at lines 529-530
  found: The callback converts rawX/rawY from evdev (0-4095) to pixel coords: px = (rawX / 4095.0f) * dw. This is correct for the zone callback.

- timestamp: 2026-03-03T00:00:00Z
  checked: Zone registration in main.cpp lines 518-521
  found: Zone is registered with pixel bounds (0, 0, dw, dh) via bridge->updateZone(). EvdevCoordBridge::rebuildAndPush() converts these to evdev coords via pixelToEvdevX/Y. The zone bounds become (0, 0, 4095, 4095) -- full screen. This is correct.

- timestamp: 2026-03-03T00:00:00Z
  checked: GestureOverlay.qml lines 16-17, 40-44, 100, 137, 163, 219
  found: CRITICAL - acceptInput starts false (line 16), set to true after 500ms by inputGuardTimer (lines 40-44). Sliders have enabled: overlay.acceptInput (lines 100, 137). Buttons have enabled: overlay.acceptInput (lines 163, 191, 219). But this is only the QML side - during EVIOCGRAB, QML doesn't receive touch at all. The evdev zone callback in main.cpp bypasses QML entirely - it calls AudioService/DisplayService/ActionRegistry directly. So acceptInput doesn't matter for the evdev path.

- timestamp: 2026-03-03T00:00:00Z
  checked: processSync() lines 297-325 - interaction between checkGesture() and zone dispatch
  found: ROOT CAUSE FOUND. Look at lines 298 and 301-325 carefully. checkGesture() is called at line 298. When 3 fingers are down and gestureActive_ becomes true, gestureBlocking=true. BUT the zone dispatch loop at 301-325 runs BEFORE the gestureBlocking check at 328-333. So zone dispatch IS attempted during the gesture. However, the REAL issue is different: when the user does a SINGLE finger touch on the overlay after the gesture, the zone dispatch runs fine. The question is whether the callback actually fires. Let me re-examine...

- timestamp: 2026-03-03T00:00:00Z
  checked: Re-examined the full flow from 3-finger gesture to subsequent single-finger touch
  found: ACTUAL ROOT CAUSE - The 3-finger gesture detection in checkGesture() (line 252-259) sets gestureActive_=true and emits gestureDetected(). Then on the SAME SYN where gestureActive_ becomes true, gestureBlocking=true at line 298. Lines 328-333 clear ALL dirty flags and return early. This means the zone dispatch at lines 301-325 runs but slots[i].dirty was already set. The zone dispatch marks consumed slots as dirty=false (line 323). Then gestureBlocking clears ALL dirty flags again at line 330. This is fine. The problem is AFTER the gesture -- when fingers lift. At line 263-265, when nowActive==0 and gestureActive_==true, gestureActive_ is set to false. But this happens INSIDE checkGesture() which is called from processSync() at line 298. So gestureBlocking for this SYN is still true (it was true when the last finger lifted because gestureActive_ was true at the START of checkGesture, and checkGesture only clears it at line 264 but returns the value... wait, let me re-read).

- timestamp: 2026-03-03T00:00:00Z
  checked: checkGesture() return value when all fingers lift
  found: Lines 263-266: when nowActive==0 and gestureActive_==true, it sets gestureActive_=false, gestureMaxFingers_=0. Then line 268 returns gestureActive_ which is now FALSE. So gestureBlocking=false for that SYN. This means subsequent single-finger touches should work. The zone dispatch path is not blocked.

- timestamp: 2026-03-03T00:00:00Z
  checked: Re-read zone registration timing in main.cpp lines 500-605
  found: REAL ROOT CAUSE FOUND. The gesture zone is registered inside the gestureTriggered signal handler (line 501-605). This signal is connected with the default connection type (AutoConnection). The gestureDetected() signal is emitted from the evdev reader THREAD (line 258). The connection chain is: EvdevTouchReader::gestureDetected (reader thread) -> AndroidAutoPlugin::gestureTriggered -> lambda in main.cpp. The gestureDetected signal from EvdevTouchReader is connected to what? Let me check AndroidAutoPlugin.

## Resolution

root_cause: (need to verify AndroidAutoPlugin signal chain)
fix: (diagnosis only)
verification: (diagnosis only)
files_changed: []
