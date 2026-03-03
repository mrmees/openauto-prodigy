---
phase: 01-visual-foundation
plan: 01
subsystem: ui
tags: [qt, qml, theme, animation, material-icons, variable-font]

# Dependency graph
requires: []
provides:
  - "ThemeService.dividerColor and pressedColor Q_PROPERTYs with fallback defaults"
  - "UiMetrics animation duration constants (150ms, 80ms)"
  - "UiMetrics tile sizing properties (tileW, tileH, tileIconSize)"
  - "MaterialIcon weight and opticalSize properties with Qt 6.4 fallback"
  - "divider and pressed color keys in all theme YAMLs"
affects: [01-02, 01-03]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "ThemeService fallback defaults for new color properties (check Qt::transparent from activeColor)"
    - "Qt version-safe QML property access via undefined check on font.variableAxes"

key-files:
  created: []
  modified:
    - src/core/services/ThemeService.hpp
    - src/core/services/ThemeService.cpp
    - config/themes/default/theme.yaml
    - config/themes/amoled/theme.yaml
    - config/themes/ember/theme.yaml
    - config/themes/ocean/theme.yaml
    - tests/data/themes/default/theme.yaml
    - qml/controls/UiMetrics.qml
    - qml/controls/MaterialIcon.qml

key-decisions:
  - "Used 8-digit hex (#RRGGBBAA) in theme YAMLs for alpha colors - QColor handles it natively"
  - "Fallback checks Qt::transparent (not QColor()) since activeColor() returns transparent for missing keys"
  - "Ember uses warm-tinted dividers (#f0e0d0xx), ocean uses cool blue-tinted (#d0e0f0xx) to complement palettes"

patterns-established:
  - "ThemeService color fallbacks: check against Qt::transparent, not invalid/black"
  - "Qt 6.4 variable font fallback: guard font.variableAxes with undefined check"

requirements-completed: [VIS-05, VIS-06, ICON-02]

# Metrics
duration: 4min
completed: 2026-03-02
---

# Phase 1 Plan 01: Visual Foundation Properties Summary

**ThemeService divider/pressed color Q_PROPERTYs, UiMetrics animation/tile constants, and MaterialIcon variable font axis support with Qt 6.4 fallback**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-02T23:02:37Z
- **Completed:** 2026-03-02T23:06:08Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments
- ThemeService exposes dividerColor and pressedColor with fallback defaults for older theme YAMLs
- All 4 config themes + 1 test theme have divider and pressed color keys with palette-appropriate tints
- UiMetrics provides animDuration (150ms), animDurationFast (80ms), and tile sizing properties
- MaterialIcon supports weight/opticalSize variable font axes, safely guarded for Qt 6.4

## Task Commits

Each task was committed atomically:

1. **Task 1: Add ThemeService C++ properties and update all theme YAMLs** - `e14527d` (feat)
2. **Task 2: Add UiMetrics constants and MaterialIcon variable font support** - `1a8f1f3` (feat)

## Files Created/Modified
- `src/core/services/ThemeService.hpp` - Added dividerColor and pressedColor Q_PROPERTYs and getter declarations
- `src/core/services/ThemeService.cpp` - Getter implementations with Qt::transparent fallback check
- `config/themes/default/theme.yaml` - Added divider/pressed keys (white alpha)
- `config/themes/amoled/theme.yaml` - Added divider/pressed keys (white alpha, same day/night)
- `config/themes/ember/theme.yaml` - Added divider/pressed keys (warm-tinted alpha)
- `config/themes/ocean/theme.yaml` - Added divider/pressed keys (cool blue-tinted alpha)
- `tests/data/themes/default/theme.yaml` - Added divider/pressed keys for test data
- `qml/controls/UiMetrics.qml` - Added animation durations and tile sizing constants
- `qml/controls/MaterialIcon.qml` - Added weight/opticalSize props with variableAxes guard

## Decisions Made
- Used 8-digit hex (#RRGGBBAA) in theme YAMLs since QColor handles it natively
- Fallback in ThemeService checks Qt::transparent (activeColor's actual missing-key return) not QColor()
- Ember dividers use warm tint (#f0e0d0), ocean uses cool blue (#d0e0f0) to match palettes

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected fallback check from QColor() to Qt::transparent**
- **Found during:** Task 1 (ThemeService getters)
- **Issue:** Plan specified checking `c == QColor()` but activeColor() returns `QColor(Qt::transparent)` for missing keys, not default-constructed QColor (which is black)
- **Fix:** Changed fallback check to `c == QColor(Qt::transparent)`
- **Files modified:** src/core/services/ThemeService.cpp
- **Verification:** Build succeeds, theme test passes
- **Committed in:** e14527d (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Essential correctness fix. Without it, fallbacks would never trigger.

## Issues Encountered
- Pre-existing test_event_bus failure (testSubscribeAndPublish expects 42, gets 0) - unrelated to our changes, not addressed

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- All foundation properties in place for Plans 02 (control restyling) and 03 (animation/transitions)
- dividerColor, pressedColor, animDuration, animDurationFast, tile sizes all available in QML

---
*Phase: 01-visual-foundation*
*Completed: 2026-03-02*
