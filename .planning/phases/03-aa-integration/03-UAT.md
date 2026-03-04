---
status: resolved
phase: 03-aa-integration
source: [03-01-SUMMARY.md, 03-02-SUMMARY.md]
started: 2026-03-04T04:00:00Z
updated: 2026-03-04T04:20:00Z
---

## Current Test

[testing complete]

## Tests

### 1. AA Video with Navbar Visible
expected: Connect phone to AA. The navbar should be visible on screen (default edge: bottom). The AA video should fill the remaining screen area with black letterbox bars if aspect ratio doesn't match. The video should NOT overlap the navbar.
result: issue
reported: "It's not visible in aa"
severity: major

### 2. Navbar Touch During AA
expected: While AA is active, tap navbar controls (home button, volume slider, etc.). Navbar should respond to touches — taps should NOT pass through to the AA session. The phone screen should not register phantom touches when you tap the navbar.
result: issue
reported: "Navbar responds to touches even though it's not visible. Can long hold and pops come up, even though I can't see the bar."
severity: major

### 3. AA Touch Passthrough
expected: While AA is active, tap on the video area (not the navbar). Touches should be forwarded to the phone — you should be able to interact with the Android Auto UI (tap buttons, scroll maps, etc.) normally.
result: pass

### 4. Show Navbar During AA Toggle
expected: Go to Settings > Display > Navbar section. There should be a "Show Navbar during Android Auto" toggle. Turn it OFF, then connect to AA. The navbar should be hidden and the AA video should fill the full screen. Turn it back ON and reconnect — navbar should reappear.
result: skipped
reason: Depends on navbar visibility fix — navbar currently not visible during AA so toggle effect can't be verified

### 5. 3-Finger Gesture Overlay During AA
expected: While AA is active, tap with 3 fingers simultaneously. The GestureOverlay should appear. While the overlay is visible, touches should NOT be forwarded to the phone (no phantom taps on AA). Dismiss the overlay — AA touch should resume working.
result: issue
reported: "popup is visible, no touches work when popup is on the screen, seemingly no matter where I press"
severity: major

### 6. Sidebar Fully Removed
expected: There should be no sidebar anywhere in the app. During AA, no sidebar appears on any screen edge. In Settings, there should be no "Sidebar" settings section in Video Settings or AA Settings pages.
result: pass

## Summary

total: 6
passed: 2
issues: 3
pending: 0
skipped: 1

## Gaps

- truth: "Navbar should be visible on screen during AA with clear boundary from video"
  status: resolved
  reason: "User reported: It's not visible in aa"
  severity: major
  test: 1
  root_cause: "AndroidAutoPlugin::wantsFullscreen() unconditionally returns true. Navbar.qml binds visible to !PluginModel.activePluginFullscreen, so navbar is hidden when AA activates. The show_during_aa config only configures evdev touch zones, never affects wantsFullscreen()."
  artifacts:
    - path: "src/plugins/android_auto/AndroidAutoPlugin.hpp"
      issue: "wantsFullscreen() hardcoded to return true, ignores show_during_aa config"
    - path: "qml/components/Navbar.qml"
      issue: "visible bound to !PluginModel.activePluginFullscreen — correct binding, wrong input"
  missing:
    - "wantsFullscreen() should consult navbar.show_during_aa config — return false when navbar should show"
  debug_session: ""

- truth: "Navbar should be visually rendered and interactive during AA"
  status: resolved
  reason: "User reported: Navbar responds to touches even though it's not visible. Can long hold and pops come up, even though I can't see the bar."
  severity: major
  test: 2
  root_cause: "Same root cause as gap 1 — navbar evdev zones are registered (touch works) but QML visible=false hides the visual. Fix gap 1 and this resolves automatically."
  artifacts:
    - path: "src/plugins/android_auto/AndroidAutoPlugin.hpp"
      issue: "wantsFullscreen() returns true, hiding navbar visually while evdev zones remain active"
  missing:
    - "Fix wantsFullscreen() per gap 1"
  debug_session: ""

- truth: "GestureOverlay controls should be interactive during AA — overlay should block AA touch forwarding but its own controls (sliders, buttons) should respond"
  status: resolved
  reason: "User reported: popup is visible, no touches work when popup is on the screen, seemingly no matter where I press"
  severity: major
  test: 5
  root_cause: "Two interlocking problems: (1) checkGesture() early-return in processSync() bypasses zone dispatch while gestureActive_ is true — no zones fire until all fingers lift. (2) The overlay zone callback is an explicit no-op (Q_UNUSED on all params) — even when reached, it consumes touches without action. (3) EVIOCGRAB prevents Qt/Wayland from receiving touch events, so QML MouseAreas/Sliders are dead during AA. Need synthetic Qt touch injection or direct evdev-to-service bridging."
  artifacts:
    - path: "src/core/aa/EvdevTouchReader.cpp"
      issue: "checkGesture() early-return blocks zone dispatch while gestureActive_ is true"
    - path: "src/main.cpp"
      issue: "Overlay zone callback is no-op — Q_UNUSED on x, y, event, all services"
  missing:
    - "Inject synthetic QTouchEvents from evdev zone callback so QML overlay controls work during EVIOCGRAB"
    - "Or: implement direct evdev-to-service bridging (hit-test overlay regions, call audioService/displayService directly)"
  debug_session: ""
