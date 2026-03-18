---
phase: 20-simple-widgets
plan: 01
subsystem: ui
tags: [qt-qml, widgets, theme-service, battery, action-registry]

# Dependency graph
requires:
  - phase: 19-widget-instance-config
    provides: WidgetDescriptor registration, WidgetInstanceContext, widget picker
provides:
  - ThemeService.currentThemeId Q_PROPERTY with NOTIFY signal
  - Theme cycle widget (org.openauto.theme-cycle)
  - Battery widget (org.openauto.battery)
  - AA focus actions (aa.requestFocus, aa.exitToCar) in ActionRegistry
affects: [20-simple-widgets plan 02, 21-clock-weather-widgets]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Null-safe QML companion bindings (CompanionService ? CompanionService.prop : default)"
    - "Widget scaleFactor pattern for proportional sizing at larger spans"

key-files:
  created:
    - qml/widgets/ThemeCycleWidget.qml
    - qml/widgets/BatteryWidget.qml
  modified:
    - src/core/services/ThemeService.hpp
    - src/core/services/ThemeService.cpp
    - src/main.cpp
    - src/CMakeLists.txt

key-decisions:
  - "currentThemeIdChanged emitted from both setTheme() and companion import reload path for complete coverage"
  - "Battery disconnected state uses battery outline + X overlay rather than battery_alert icon"
  - "AA focus actions registered in Plan 01 alongside descriptors so Plan 02 can use them immediately"

patterns-established:
  - "Null-safe companion widget bindings: ternary guard against null CompanionService"
  - "Widget scaleFactor: Math.min(colSpan, rowSpan, 3) for proportional icon/text sizing"

requirements-completed: [TC-01, TC-02, BW-01]

# Metrics
duration: 7min
completed: 2026-03-17
---

# Phase 20 Plan 01: Simple Widgets - Theme Cycle and Battery Summary

**ThemeService currentThemeId Q_PROPERTY, theme cycle widget with tap-to-advance, battery widget with null-safe CompanionService bindings, and AA focus action registrations**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-17T01:11:39Z
- **Completed:** 2026-03-17T01:18:26Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- ThemeService extended with QML-bindable currentThemeId property (emits from all theme-change paths)
- Theme cycle widget cycles through available themes on tap with palette icon tinted by primary color
- Battery widget shows phone battery level from CompanionService with disconnected X overlay state
- AA focus actions (requestFocus, exitToCar) registered for Plan 02's AA focus toggle widget

## Task Commits

Each task was committed atomically:

1. **Task 1: C++ API extensions and widget infrastructure** - `163e1cc` (feat)
2. **Task 2: Theme Cycle and Battery widget QML** - `da25bbf` (feat)

## Files Created/Modified
- `src/core/services/ThemeService.hpp` - Added Q_PROPERTY(QString currentThemeId) + currentThemeIdChanged signal
- `src/core/services/ThemeService.cpp` - Emit currentThemeIdChanged from setTheme() and companion import reload
- `src/main.cpp` - Register theme-cycle + battery descriptors, aa.requestFocus + aa.exitToCar actions
- `src/CMakeLists.txt` - Added ThemeCycleWidget.qml and BatteryWidget.qml to source properties + QML_FILES
- `qml/widgets/ThemeCycleWidget.qml` - Tap-to-cycle theme widget with palette icon and theme name
- `qml/widgets/BatteryWidget.qml` - Battery level widget with null-safe CompanionService bindings

## Decisions Made
- currentThemeIdChanged emitted from both setTheme() and the companion theme import reload path (themeId_ == slug branch) to ensure QML bindings update in all cases
- Battery disconnected state uses battery outline + X overlay (close icon) per CONTEXT spec, not battery_alert
- AA focus actions registered in Plan 01 so Plan 02 can reference them from the AA focus toggle widget

## Deviations from Plan

None - plan executed exactly as written.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Plan 02 can add CompanionStatusWidget.qml and AAFocusToggleWidget.qml with their descriptors + CMake entries
- AA focus actions already available in ActionRegistry for Plan 02 consumption
- All 87 tests pass, no regressions

---
*Phase: 20-simple-widgets*
*Completed: 2026-03-17*
