---
phase: 02-theme-display
plan: 03
subsystem: ui
tags: [qt6, qml, themes, brightness, display, wallpaper]

# Dependency graph
requires:
  - phase: 02-theme-display/01
    provides: "ThemeService multi-theme scanning, availableThemes/availableThemeNames properties, wallpaperSource"
  - phase: 02-theme-display/02
    provides: "DisplayService brightness control with sysfs/overlay backends, dimOverlayOpacity"
provides:
  - "Theme picker UI in DisplaySettings (FullScreenPicker with 4 themes)"
  - "Brightness sliders wired in DisplaySettings and GestureOverlay"
  - "Software dimming overlay in Shell.qml"
  - "Wallpaper image layer in Wallpaper.qml"
  - "DisplayService registered as QML context property"
  - "Theme scanning at startup with config-driven theme load"
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "FullScreenPicker pattern for theme selection with live apply callback"
    - "Software dimming overlay (z:998) below GestureOverlay (z:999) with enabled:false"
    - "ConfigService persistence from GestureOverlay for brightness"

key-files:
  created: []
  modified:
    - src/main.cpp
    - qml/applications/settings/DisplaySettings.qml
    - qml/components/Wallpaper.qml
    - qml/components/GestureOverlay.qml
    - qml/components/Shell.qml
    - src/core/services/DisplayService.hpp
    - src/core/services/DisplayService.cpp
    - src/core/services/ThemeService.hpp

key-decisions:
  - "setBrightness and setTheme marked Q_INVOKABLE for QML access (discovered during Pi verification)"
  - "Software dimming slider labeled 'Screen Dimming' with contrast icon when no hardware backend"
  - "Brightness persistence handled by SettingsSlider configPath and explicit ConfigService.save() in GestureOverlay -- no C++ persistence signal needed"

patterns-established:
  - "Q_INVOKABLE required for QML to call C++ slots via context properties"
  - "GestureOverlay brightness syncs value on show() from DisplayService"

requirements-completed: [DISP-01, DISP-02, DISP-03, DISP-04, DISP-05]

# Metrics
duration: ~30min
completed: 2026-03-01
---

# Phase 2 Plan 3: UI Integration Summary

**Theme picker with 4 palettes, dual brightness sliders (settings + gesture overlay), wallpaper layer, and software dimming overlay -- all verified on Pi hardware**

## Performance

- **Duration:** ~30 min (including Pi verification cycle)
- **Started:** 2026-03-01T19:27:30Z
- **Completed:** 2026-03-01T19:59:17Z
- **Tasks:** 3 (2 auto + 1 human-verify)
- **Files modified:** 8

## Accomplishments

- Theme picker in DisplaySettings with 4 themes (Default, Ocean, Ember, AMOLED) -- selection changes all UI colors instantly
- Brightness control from both Settings slider and 3-finger gesture overlay, with persistence
- Software dimming overlay in Shell.qml for displays without hardware backlight control
- Wallpaper.qml supports theme wallpaper images behind solid color background
- All 12 verification steps passed on Pi hardware (DFRobot 1024x600 display)

## Task Commits

Each task was committed atomically:

1. **Task 1: Register DisplayService and theme scanning in main.cpp** - `254ccd0` (feat)
2. **Task 2: Wire theme picker, wallpaper, brightness sliders, and dimming overlay in QML** - `0c56c27` (feat)
3. **Task 3: Verify theme and brightness on Pi hardware** - n/a (human-verify checkpoint, approved)

## Verification Fix Commits

Issues discovered during Pi hardware verification (Task 3):

- `e44d79b` - fix: mark ThemeService::setTheme Q_INVOKABLE for QML access
- `786b350` - fix: mark DisplayService::setBrightness Q_INVOKABLE for QML access
- `a898173` - feat: show "Screen Dimming" label when no hardware brightness backend
- `59ed6dd` - fix: use brightness icon for dimming slider
- `68d2e9e` - feat: use contrast icon for software dimming slider

## Files Created/Modified

- `src/main.cpp` - DisplayService creation, theme scanning, config-driven theme load, QML context property registration
- `qml/applications/settings/DisplaySettings.qml` - FullScreenPicker for theme selection, brightness slider wired to DisplayService
- `qml/components/Wallpaper.qml` - Image layer for theme wallpaper behind solid color
- `qml/components/GestureOverlay.qml` - Brightness slider wired to DisplayService with ConfigService persistence
- `qml/components/Shell.qml` - Software dimming overlay (z:998, enabled:false)
- `src/core/services/DisplayService.hpp` - setBrightness marked Q_INVOKABLE
- `src/core/services/DisplayService.cpp` - Q_INVOKABLE implementation
- `src/core/services/ThemeService.hpp` - setTheme marked Q_INVOKABLE

## Decisions Made

- **Q_INVOKABLE for QML access:** Both `setTheme` and `setBrightness` needed Q_INVOKABLE to be callable from QML via context properties. Q_PROPERTY WRITE alone is insufficient for explicit method calls from QML.
- **"Screen Dimming" label:** When no hardware brightness backend is detected (e.g., DFRobot display with physical brightness pot), the label changes to "Screen Dimming" with a contrast icon to avoid user confusion.
- **No C++ persistence signal:** Brightness persistence is handled by SettingsSlider's configPath (in settings) and explicit ConfigService.save() (in gesture overlay), avoiding feedback loop concerns.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] ThemeService::setTheme not Q_INVOKABLE**
- **Found during:** Task 3 (Pi verification)
- **Issue:** Theme picker saved to config but didn't apply live -- QML couldn't call setTheme()
- **Fix:** Added Q_INVOKABLE to setTheme declaration in ThemeService.hpp
- **Files modified:** src/core/services/ThemeService.hpp
- **Committed in:** e44d79b

**2. [Rule 1 - Bug] DisplayService::setBrightness not Q_INVOKABLE**
- **Found during:** Task 3 (Pi verification)
- **Issue:** Brightness slider didn't affect display -- QML couldn't call setBrightness()
- **Fix:** Added Q_INVOKABLE to setBrightness declaration and override in DisplayService.hpp/.cpp
- **Files modified:** src/core/services/DisplayService.hpp, src/core/services/DisplayService.cpp
- **Committed in:** 786b350

**3. [Rule 2 - Missing Critical] Software dimming label clarity**
- **Found during:** Task 3 (Pi verification)
- **Issue:** "Brightness" label misleading on displays with physical brightness control
- **Fix:** Label changes to "Screen Dimming" with contrast icon when no hardware backend
- **Files modified:** qml/applications/settings/DisplaySettings.qml
- **Committed in:** a898173, 59ed6dd, 68d2e9e

---

**Total deviations:** 3 auto-fixed (2 bugs, 1 missing critical UX)
**Impact on plan:** All fixes necessary for correct QML-C++ integration and user clarity. No scope creep.

## Issues Encountered

- Q_PROPERTY WRITE is not sufficient for QML to call methods on context properties -- Q_INVOKABLE is required. This is a recurring Qt/QML integration gotcha that should be remembered for future plans.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- All DISP requirements (DISP-01 through DISP-05) satisfied and verified on Pi hardware
- Phase 2 (Theme & Display) is now complete
- Phase 3 (HFP) can proceed independently

## Self-Check: PASSED

All 8 modified files verified on disk. All 7 commit hashes verified in git log.

---
*Phase: 02-theme-display*
*Completed: 2026-03-01*
