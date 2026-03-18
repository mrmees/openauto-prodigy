---
phase: 20-simple-widgets
plan: 02
subsystem: ui
tags: [qt-qml, widgets, companion-service, projection-status, action-registry]

# Dependency graph
requires:
  - phase: 20-simple-widgets
    plan: 01
    provides: Widget descriptor registration pattern, AA focus actions, BatteryWidget null-safe pattern
provides:
  - Companion Status widget (org.openauto.companion-status) with 1x1 compact and 2x1+ expanded views
  - AA Focus Toggle widget (org.openauto.aa-focus) with projection state switching
affects: [21-clock-weather-widgets]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Expanded widget layout: RowLayout with icon left + detail ColumnLayout right at colSpan >= 2"
    - "Projection state toggle: ActionRegistry dispatch for aa.requestFocus/aa.exitToCar"

key-files:
  created:
    - qml/widgets/CompanionStatusWidget.qml
    - qml/widgets/AAFocusToggleWidget.qml
  modified:
    - src/main.cpp
    - src/CMakeLists.txt

key-decisions:
  - "Companion status shows disconnected state at any span when CompanionService null or disconnected"
  - "AA focus widget uses root-level opacity 0.4 for not-connected state rather than per-element dimming"

patterns-established:
  - "Expanded widget detail rows: icon + text RowLayout items inside a ColumnLayout, shown at colSpan >= 2"
  - "State-dependent toggle: MouseArea with click handler that checks state before dispatching action"

requirements-completed: [CS-01, CS-02, AF-01, AF-02, AF-03]

# Metrics
duration: 6min
completed: 2026-03-17
---

# Phase 20 Plan 02: Simple Widgets - Companion Status and AA Focus Toggle Summary

**Companion status widget with GPS/battery/proxy detail rows and AA focus toggle dispatching projection state via ActionRegistry**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-17T01:20:36Z
- **Completed:** 2026-03-17T01:27:31Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Companion Status widget shows smartphone icon + colored dot at 1x1, expands to GPS/battery/proxy detail rows at 2x1+
- AA Focus Toggle widget switches between phone/AA (projected) and car (backgrounded) states via ActionRegistry
- All CompanionService bindings null-safe with ternary guards
- AA focus widget disabled/greyed at 0.4 opacity when AA not connected
- All 4 Phase 20 widget QML files now in place (ThemeCycle, Battery, CompanionStatus, AAFocusToggle)

## Task Commits

Each task was committed atomically:

1. **Task 1: Companion Status widget QML + descriptor/CMake registration** - `09c421d` (feat)
2. **Task 2: AA Focus Toggle widget QML + CMake registration** - `4f0d963` (feat)

## Files Created/Modified
- `qml/widgets/CompanionStatusWidget.qml` - Companion connection status with compact/expanded views
- `qml/widgets/AAFocusToggleWidget.qml` - AA projection toggle with state-dependent icon/label/color
- `src/main.cpp` - Register companion-status and aa-focus widget descriptors
- `src/CMakeLists.txt` - Add both new widget QML files to source properties and QML_FILES

## Decisions Made
- Companion status shows disconnected state regardless of span when CompanionService is null or disconnected (expanded layout only shown when connected AND colSpan >= 2)
- AA focus widget uses root-level opacity 0.4 for not-connected state for simplicity rather than per-element color adjustments
- AA focus widget uses a single MouseArea for both click (toggle) and pressAndHold (context menu) matching AAStatusWidget pattern

## Deviations from Plan

None - plan executed exactly as written.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- All 4 Phase 20 widgets complete and registered in widget picker
- Phase 20 fully complete, ready for Phase 21 (clock styles + weather widgets)
- 87 tests pass, no regressions

---
*Phase: 20-simple-widgets*
*Completed: 2026-03-17*
