---
phase: 25-selection-model-interaction-foundation
plan: 01
subsystem: ui
tags: [qml, widget-grid, selection-model, long-press, drag-drop, touch]

# Dependency graph
requires:
  - phase: 07-edit-mode
    provides: "Global editMode, drag/resize, FABs, grid lines, SwipeView locking"
  - phase: 09-widget-descriptor-grid-foundation
    provides: "WidgetGridModel, remap deferral, grid math"
provides:
  - "Per-widget selectedInstanceId property replacing global editMode"
  - "selectWidget()/deselectWidget() functions with full cleanup"
  - "WidgetGridModel.setWidgetSelected(bool) remap gate"
  - "selectionTapInterceptor MouseArea on all delegates"
  - "Lift animation (scale + shadow) during press-hold"
  - "C++ navigation deselect paths (6 actions in main.cpp)"
  - "WidgetConfigSheet closeConfig()/isOpen API"
  - "_addingPage guard for page-change deselect safety"
affects: [26-navbar-resize-controls, 27-context-menu-picker]

# Tech tracking
tech-stack:
  added: []
  patterns: [per-widget-selection-model, tap-interceptor-overlay, addingPage-guard]

key-files:
  created: []
  modified:
    - src/ui/WidgetGridModel.hpp
    - src/ui/WidgetGridModel.cpp
    - src/main.cpp
    - tests/test_widget_grid_remap.cpp
    - qml/components/WidgetConfigSheet.qml
    - qml/applications/home/HomeMenu.qml

key-decisions:
  - "Scale animation on innerContent not delegateItem (Pitfall 13 prevention)"
  - "selectionTapInterceptor covers ALL delegates including selected (Issue 2 fix)"
  - "liftResetTimer 300ms for interactive widget forwarded long-press (Issue 3 fix)"
  - "SwipeView locked during selection, not just drag (user decision from CONTEXT.md)"

patterns-established:
  - "Per-widget selection: selectedInstanceId drives all interaction gating"
  - "Tap interceptor: z:15 MouseArea overlay during selection prevents widget action leakage"
  - "_addingPage guard: boolean flag wrapping page mutations to prevent onCurrentIndexChanged side effects"

requirements-completed: [SEL-01, SEL-02, SEL-03, SEL-04, CLN-01, CLN-04]

# Metrics
duration: 8min
completed: 2026-03-21
---

# Phase 25 Plan 01: Selection Model & Interaction Foundation Summary

**Per-widget selectedInstanceId replacing global editMode with lift animation, tap interception, and 6-path C++ deselect wiring**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-21T18:15:32Z
- **Completed:** 2026-03-21T18:23:53Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Replaced global editMode boolean with per-widget selectedInstanceId string throughout HomeMenu.qml and WidgetGridModel
- Wired all 6 navigation actions (4 navbar + 2 launcher widget) to call setWidgetSelected(false) in C++
- Added press-hold lift animation (scale 1.05 + drop shadow on innerContent) with Behavior animation
- Added selectionTapInterceptor MouseArea at z:15 covering all delegates during selection to prevent launcher widget navigation escape
- Added _addingPage guard to prevent add-page FAB from triggering page-change deselect self-destruction

## Task Commits

Each task was committed atomically:

1. **Task 1: Rename editMode gate, wire navigation deselect, add WidgetConfigSheet API** - `7afb6d3` (feat)
2. **Task 2: Replace global editMode with per-widget selectedInstanceId** - `c4487ac` (feat)

## Files Created/Modified
- `src/ui/WidgetGridModel.hpp` - Renamed setEditMode/editMode_ to setWidgetSelected/widgetSelected_
- `src/ui/WidgetGridModel.cpp` - Renamed method and member, identical remap deferral logic
- `src/main.cpp` - Added widgetGridModel->setWidgetSelected(false) to 6 navigation actions
- `tests/test_widget_grid_remap.cpp` - Updated test calls from setEditMode to setWidgetSelected
- `qml/components/WidgetConfigSheet.qml` - Added isOpen readonly property and closeConfig() method
- `qml/applications/home/HomeMenu.qml` - Core refactor: selectedInstanceId, selectWidget, deselectWidget, selectionTimer, selectionTapInterceptor, lift animation, _addingPage guard

## Decisions Made
- Applied scale to innerContent child, not delegateItem itself, to prevent position math breakage (Pitfall 13)
- Used simple Rectangle shadow (#40000000) instead of MultiEffect to avoid Pi GPU issues
- selectionTapInterceptor covers ALL delegates including the selected one, preventing launcher widget actions from firing during selection
- Used 300ms Timer for lift reset on interactive widgets rather than Qt.callLater (which fires same-frame before finger-up)
- SwipeView locked during entire selection (interactive: selectedInstanceId === ""), not just during drag, per CONTEXT.md decision

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- selectedInstanceId signal is the foundation for Phase 26 (navbar transformation) and Phase 27 (context menu/picker)
- FABs and badges remain temporarily gated on selection state (removed in Phases 26-27)
- All 88 tests pass, build compiles clean

---
*Phase: 25-selection-model-interaction-foundation*
*Completed: 2026-03-21*
