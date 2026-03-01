---
status: diagnosed
phase: 02-theme-display
source: [02-01-SUMMARY.md, 02-02-SUMMARY.md, 02-03-SUMMARY.md, 02-04-SUMMARY.md]
started: 2026-03-01T20:35:00Z
updated: 2026-03-01T20:45:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Theme Picker Available
expected: In Settings > Display, there's a theme picker showing 4 themes: Default, Ocean, Ember, AMOLED.
result: pass

### 2. Theme Switching
expected: Selecting a different theme (e.g., Ocean) instantly changes all UI colors — top bar, nav strip, backgrounds, text — to the new palette.
result: pass

### 3. Wallpaper Display
expected: With a theme that has a wallpaper (Default, Ocean, or Ember), a gradient wallpaper image is visible behind the UI on the home/launcher screen.
result: issue
reported: "Picker has shown up, but no changes to the background on the home screen"
severity: major

### 4. Wallpaper Picker
expected: In Settings > Display, there's a wallpaper picker with options including "None" plus wallpapers from each theme that has one (Default, Ocean, Ember). AMOLED should NOT have a wallpaper option.
result: skipped
reason: Wallpaper display broken, can't verify picker behavior including AMOLED exclusion

### 5. Wallpaper Independent of Theme
expected: You can select a wallpaper from a different theme than the active one (e.g., Ocean theme with Ember wallpaper). The wallpaper changes without affecting the color theme.
result: skipped
reason: Wallpaper display broken

### 6. Brightness/Dimming Slider in Settings
expected: In Settings > Display, there's a brightness or "Screen Dimming" slider. Moving it visibly darkens/lightens the screen via a software overlay.
result: pass

### 7. Brightness Slider in Gesture Overlay
expected: 3-finger tap opens the quick controls overlay. There's a brightness slider that also controls screen dimming, matching the value from Settings.
result: issue
reported: "Gesture overlay does not work at all and has not worked for a long time"
severity: major

### 8. AMOLED Theme
expected: Selecting the AMOLED theme shows a pure black background with light blue accents. No wallpaper. Day and night modes look identical.
result: pass

### 9. Persistence After Restart
expected: After selecting a non-default theme, wallpaper, and brightness level, restart the app. All three choices are restored on relaunch.
result: pass

## Summary

total: 9
passed: 5
issues: 2
pending: 0
skipped: 2

## Gaps

- truth: "Wallpaper image visible behind UI on home/launcher screen"
  status: failed
  reason: "User reported: Picker has shown up, but no changes to the background on the home screen"
  severity: major
  test: 3
  root_cause: "Wallpaper.qml is defined and compiled but never instantiated in the QML scene graph — Shell.qml has no Wallpaper {} element"
  artifacts:
    - path: "qml/components/Shell.qml"
      issue: "Missing Wallpaper {} instantiation"
    - path: "qml/components/Wallpaper.qml"
      issue: "Component is correct but orphaned — never used anywhere"
  missing:
    - "Add Wallpaper {} inside pluginContentHost in Shell.qml, before LauncherMenu, with anchors.fill and visible tied to launcher visibility"
  debug_session: ".planning/debug/wallpaper-not-displayed.md"

- truth: "3-finger gesture overlay opens with brightness slider"
  status: failed
  reason: "User reported: Gesture overlay does not work at all and has not worked for a long time"
  severity: major
  test: 7
  root_cause: "gestureDetected signal emitted but never connected to anything; also events discarded when ungrabbed (AA not connected) so gesture detection unreachable on home screen"
  artifacts:
    - path: "src/plugins/android_auto/AndroidAutoPlugin.cpp"
      issue: "gestureDetected signal never connected (sidebar signals were, this one was missed)"
    - path: "src/core/aa/EvdevTouchReader.cpp"
      issue: "Line 253: events discarded when grabbed_=false, gesture detection impossible outside AA"
  missing:
    - "Connect gestureDetected signal to invoke GestureOverlay.show() (C++ to QML bridge)"
    - "Restructure EvdevTouchReader event loop so gesture detection runs even when ungrabbed, while AA touch forwarding remains suppressed"
  debug_session: ".planning/debug/gesture-overlay-broken.md"
