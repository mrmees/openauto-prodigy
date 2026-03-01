---
phase: 02-theme-display
verified: 2026-03-01T22:00:00Z
status: human_needed
score: 9/9 must-haves verified
re_verification:
  previous_status: passed
  previous_score: 7/7
  gaps_closed:
    - "Wallpaper image visible on home screen (Wallpaper {} instantiated in Shell.qml as first child of pluginContentHost)"
    - "3-finger gesture overlay signal chain wired end-to-end (gestureDetected -> gestureTriggered -> GestureOverlay.show())"
    - "Gesture detection runs even when ungrabbed (home screen, not just during AA)"
  gaps_remaining: []
  regressions: []
human_verification:
  - test: "Pi hardware: verify wallpaper image renders behind launcher after selecting Default/Ocean/Ember"
    expected: "Gradient wallpaper visible behind tile grid on home screen; None shows solid theme color"
    why_human: "UAT test 3 previously failed (picker shown, no background change). Wallpaper {} instantiation fix applied in plan 05. Requires on-device confirmation."
  - test: "Pi hardware: verify 3-finger gesture overlay opens on home screen (AA not connected)"
    expected: "3-finger tap opens quick controls overlay with brightness slider, even without AA active"
    why_human: "UAT test 7 previously failed (overlay never opened). gestureDetected signal wired and ungrabbed event loop restructured in plan 05. Requires on-device confirmation."
  - test: "Pi hardware: verify 3-finger gesture overlay opens during AA session"
    expected: "3-finger tap during Android Auto opens the overlay; tap or timeout dismisses it"
    why_human: "grabbed_ = true path routes through processSync -> checkGesture. Requires on-device confirmation with AA connected."
  - test: "Pi hardware: verify wallpaper picker independence (different wallpaper from active theme)"
    expected: "Ocean theme with Ember wallpaper: blue/teal colors, amber gradient background"
    why_human: "UAT test 5 was skipped (wallpaper display was broken). Now unblocked. Requires on-device verification."
---

# Phase 2: Theme & Display Verification Report

**Phase Goal:** Users can personalize the head unit appearance and control screen brightness
**Verified:** 2026-03-01T22:00:00Z
**Status:** human_needed — all automated checks pass; plan 05 UAT gap fixes verified in code; on-device re-tests required
**Re-verification:** Yes — after plan 05 gap closure (UAT-driven fixes)

## Re-verification Context

The previous VERIFICATION.md (plan 04) had `status: passed` based on code inspection alone. UAT on Pi then revealed two real runtime failures:

- **UAT Test 3 (Wallpaper Display):** "Picker has shown up, but no changes to the background on the home screen" — `Wallpaper.qml` was correct but orphaned; never instantiated in `Shell.qml`. This is exactly the Level 3 (wiring) failure type code inspection missed.
- **UAT Test 7 (Gesture Overlay):** "Gesture overlay does not work at all and has not worked for a long time" — `gestureDetected` signal emitted but never connected; and gesture detection unreachable when `grabbed_ = false` (home screen state).

Plan 05 addressed both. This re-verification confirms all fixes are in place, the build is clean, and tests pass. On-device UAT re-runs are required for final goal confirmation.

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | User can switch between 4 color themes with live UI update | VERIFIED | DisplaySettings.qml lines 31-39: FullScreenPicker wired to ThemeService.setTheme(). 4 theme YAMLs confirmed. UAT tests 1+2 passed on Pi. |
| 2 | Wallpaper image renders behind launcher on home screen | VERIFIED (code) | Shell.qml lines 31-36: Wallpaper {} is first child of pluginContentHost, visible matches LauncherMenu condition. Wallpaper.qml line 13: source bound to ThemeService.wallpaperSource. Needs Pi re-test. |
| 3 | Wallpaper picker shows None + theme wallpapers; AMOLED excluded | VERIFIED | DisplaySettings.qml lines 41-49: FullScreenPicker with ThemeService.availableWallpapers/availableWallpaperNames. buildWallpaperList() prepends "None", enumerates themes with wallpaper.jpg. AMOLED dir has no wallpaper.jpg. UAT test 4 skipped — needs Pi re-test. |
| 4 | Wallpaper selection is independent of theme selection | VERIFIED (code) | ThemeService has separate setTheme() and setWallpaper(). main.cpp loads display.theme then display.wallpaper independently. UAT test 5 was skipped — needs Pi re-test. |
| 5 | Theme and wallpaper survive full reboot | VERIFIED | FullScreenPicker configPath saves to YAML. main.cpp loads savedTheme + savedWallpaper at startup. UAT test 9 passed on Pi. |
| 6 | Brightness slider in Settings dims screen via software overlay | VERIFIED | DisplaySettings.qml: SettingsSlider onMoved -> DisplayService.setBrightness(). Shell.qml lines 73-81: dimming Rectangle, opacity bound to DisplayService.dimOverlayOpacity, z:998. UAT test 6 passed on Pi. |
| 7 | Brightness slider in gesture overlay controls screen dimming | VERIFIED (code) | GestureOverlay.qml has brightness slider wired to DisplayService. Signal chain now wired: gestureDetected -> gestureTriggered -> main.cpp findChild -> GestureOverlay.show(). UAT test 7 previously failed — needs Pi re-test. |
| 8 | 3-finger gesture opens on home screen (ungrabbed) | VERIFIED (code) | EvdevTouchReader.cpp lines 252-289: ungrabbed branch calls checkGesture() without AA forwarding. Needs Pi re-test. |
| 9 | 3-finger gesture opens during AA session (grabbed) | VERIFIED (code) | processSync() calls checkGesture() when grabbed. gestureTriggered -> GestureOverlay.show() wired in main.cpp lines 332-340. Needs Pi re-test. |

**Score: 9/9 truths verified in code**

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `qml/components/Shell.qml` | Wallpaper {} inside pluginContentHost as first child | VERIFIED | Lines 31-36: Wallpaper {} with anchors.fill, visible matching LauncherMenu condition. Added by plan 05. |
| `qml/components/Shell.qml` | GestureOverlay has objectName: "gestureOverlay" | VERIFIED | Line 86: objectName: "gestureOverlay" present. Required for C++ findChild access. |
| `qml/components/Wallpaper.qml` | Image bound to ThemeService.wallpaperSource | VERIFIED (regression) | Line 13: source: ThemeService.wallpaperSource. Unchanged from plan 03. |
| `src/plugins/android_auto/AndroidAutoPlugin.hpp` | gestureTriggered() signal declared | VERIFIED | Line 77: void gestureTriggered(); in signals block. |
| `src/plugins/android_auto/AndroidAutoPlugin.cpp` | gestureDetected connected to gestureTriggered | VERIFIED | Lines 100-102: connect with Qt::QueuedConnection. |
| `src/core/aa/EvdevTouchReader.cpp` | Event loop: always track slots, gesture detection when ungrabbed | VERIFIED | Lines 252-289: isGrabbed flag; ungrabbed branch calls checkGesture() + clears dirty flags without AA forwarding. |
| `src/main.cpp` | gestureTriggered connected to GestureOverlay.show() via findChild | VERIFIED | Lines 332-340: QObject::connect on gestureTriggered, findChild<QObject*>("gestureOverlay"), invokeMethod "show". |
| `config/themes/default/wallpaper.jpg` | Default theme wallpaper | VERIFIED (regression) | Exists, 5763 bytes. Unchanged from plan 04. |
| `config/themes/ocean/wallpaper.jpg` | Ocean theme wallpaper | VERIFIED (regression) | Exists, 5717 bytes. Unchanged from plan 04. |
| `config/themes/ember/wallpaper.jpg` | Ember theme wallpaper | VERIFIED (regression) | Exists, 5323 bytes. Unchanged from plan 04. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| Shell.qml pluginContentHost | ThemeService.wallpaperSource | Wallpaper {} first child | WIRED | Wallpaper {} at line 32; Wallpaper.qml Image.source bound to ThemeService.wallpaperSource |
| EvdevTouchReader::gestureDetected | AndroidAutoPlugin::gestureTriggered | Qt signal-signal connect | WIRED | AndroidAutoPlugin.cpp lines 100-102: connect with Qt::QueuedConnection |
| AndroidAutoPlugin::gestureTriggered | GestureOverlay.show() | main.cpp lambda + findChild | WIRED | main.cpp lines 333-340: findChild<QObject*>("gestureOverlay") + invokeMethod "show" |
| EvdevTouchReader event loop | checkGesture() when ungrabbed | isGrabbed branch | WIRED | EvdevTouchReader.cpp lines 281-287: else branch calls checkGesture() when !isGrabbed |
| DisplaySettings.qml brightness slider | Shell.qml dimming overlay | DisplayService.dimOverlayOpacity | WIRED (regression) | Shell.qml line 77: opacity bound to DisplayService.dimOverlayOpacity. Unchanged. |
| DisplaySettings.qml theme picker | ThemeService.setTheme() | FullScreenPicker onActivated | WIRED (regression) | Lines 36-38: onActivated calls ThemeService.setTheme(). Unchanged. |
| DisplaySettings.qml wallpaper picker | ThemeService.setWallpaper() | FullScreenPicker onActivated | WIRED (regression) | Lines 46-48: onActivated calls ThemeService.setWallpaper(). Unchanged. |
| main.cpp | themeService->setWallpaper() | display.wallpaper config key | WIRED (regression) | Lines 131-134: override chain correct. Unchanged. |

### Build and Test Status

| Check | Result | Details |
|-------|--------|---------|
| cmake --build | PASS | Clean build, no errors |
| ctest (53 tests) | 52/53 PASS | test_event_bus (#28) fails — pre-existing issue, unrelated to phase 2 |

### Requirements Coverage

| Requirement | Source Plans | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| DISP-01 | 02-01, 02-03 | User can select from multiple color palettes in settings | SATISFIED | 4 themes in DisplaySettings; setTheme() live update. UAT tests 1+2 passed on Pi. |
| DISP-02 | 02-03, 02-04, 02-05 | User can select a wallpaper from available options in settings | SATISFIED (code) | Wallpaper picker in DisplaySettings; Wallpaper {} now instantiated in Shell.qml. UAT tests 3+4 need Pi re-test. |
| DISP-03 | 02-01, 02-03, 02-04 | Selected theme and wallpaper persist across restarts | SATISFIED | FullScreenPicker configPath + main.cpp load at startup. UAT test 9 passed on Pi. |
| DISP-04 | 02-02, 02-03, 02-05 | Screen brightness controllable from settings slider and gesture overlay | SATISFIED (code) | Settings slider confirmed (UAT test 6 passed). Gesture overlay signal chain now wired. UAT test 7 needs Pi re-test. |
| DISP-05 | 02-02, 02-03 | Brightness control writes to actual display backlight hardware on Pi | SATISFIED (needs human) | DisplayService sysfs/ddcutil paths implemented. Software overlay confirmed working (UAT test 6). Pi hardware backlight path verified per 02-03-SUMMARY. |

All 5 DISP requirements accounted for. No orphaned requirements.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| AndroidAutoPlugin.cpp | 240 | return {} | INFO | IPlugin::qmlComponent() — intentional empty URL when QRC path unavailable. Not a stub. |
| AndroidAutoPlugin.hpp | 62, 65 | return {} | INFO | IPlugin optional interface methods (no settings component, no required services). By design. |

No blockers. No warnings.

### Human Verification Required

#### 1. Pi Hardware — Wallpaper Display (UAT Test 3 re-run)

**Test:** Pull latest from main, restart app. Open Settings > Display, select "Default" wallpaper, navigate back to the home/launcher screen.
**Expected:** Gradient wallpaper image visible behind the tile grid. Selecting "None" reverts to solid theme background color.
**Why human:** UAT test 3 previously failed with "no changes to the background." The fix (Wallpaper {} instantiated in Shell.qml) is verified in code but on-device rendering requires physical observation.

#### 2. Pi Hardware — Gesture Overlay on Home Screen (UAT Test 7 re-run)

**Test:** With AA disconnected, touch the Pi screen with 3 fingers simultaneously within ~200ms.
**Expected:** Quick controls overlay slides in / appears with brightness slider.
**Why human:** UAT test 7 previously failed completely. Two fixes applied in plan 05: gestureDetected signal wired and EvdevTouchReader ungrabbed path restructured. Requires physical touchscreen confirmation.

#### 3. Pi Hardware — Gesture Overlay During AA

**Test:** With Android Auto connected and video displaying, touch screen with 3 fingers simultaneously.
**Expected:** Quick controls overlay appears over the AA video. Single tap or timeout dismisses it, returning to AA.
**Why human:** grabbed_ = true path (processSync -> checkGesture) is a separate code path from the ungrabbed path. Requires on-device confirmation with AA connected.

#### 4. Pi Hardware — Wallpaper Picker Independence (UAT Test 5, previously skipped)

**Test:** Select "Ocean" theme. Then use the Wallpaper picker to select "Ember" wallpaper. Navigate to home screen. Restart app.
**Expected:** Color palette stays Ocean (blue/teal tones); background image shows Ember amber gradient. Both choices persist after restart.
**Why human:** UAT test 5 was skipped because wallpaper display was broken. Now unblocked. Requires on-device verification.

---

## Gap Closure Summary

Plan 05 addressed the two UAT failures that slipped through code-only verification:

**Gap 1 — Wallpaper not displayed (UAT Test 3):**
Root cause: `Wallpaper.qml` was a correct, fully-wired component that was never instantiated in the scene graph — a classic orphaned component (Level 3 wiring failure). The previous verification confirmed Wallpaper.qml's internal wiring but missed that Shell.qml never placed it anywhere. Fix: added `Wallpaper {}` as the first child of `pluginContentHost` in Shell.qml, with visibility matching the LauncherMenu condition so it shows only on the home screen.

**Gap 2 — Gesture overlay non-functional (UAT Test 7):**
Two root causes: (a) `gestureDetected` emitted by EvdevTouchReader but connected to nothing in AndroidAutoPlugin — the sidebar signals (`sidebarVolumeSet`, `sidebarHome`) were connected but `gestureDetected` was missed; (b) all touch events discarded when `grabbed_ = false`, making gesture detection impossible on the home screen. Fixes: (a) added `gestureTriggered()` relay signal to AndroidAutoPlugin, connected to `gestureDetected`, wired in main.cpp via `findChild<QObject*>("gestureOverlay")` + `invokeMethod("show")`; (b) restructured EvdevTouchReader event loop so EV_ABS slot tracking always runs, SYN_REPORT branches on grab state — ungrabbed path calls `checkGesture()` without AA forwarding.

**Lesson:** Code-only verification missed both of these because the components existed and their internal logic was correct. The failures were wiring failures at the integration level (QML scene graph instantiation, signal connection). UAT caught what code inspection did not.

---

_Verified: 2026-03-01T22:00:00Z_
_Verifier: Claude (gsd-verifier)_
