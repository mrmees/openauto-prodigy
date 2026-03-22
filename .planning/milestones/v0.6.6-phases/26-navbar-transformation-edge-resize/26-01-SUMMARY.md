---
phase: 26-navbar-transformation-edge-resize
plan: 01
subsystem: ui
tags: [navbar, widget-selection, qt6, qml, gesture, action-registry]

# Dependency graph
requires:
  - phase: 25-selection-model-interaction-foundation
    provides: per-widget selection model, selectedInstanceId, selectionTapInterceptor
provides:
  - NavbarController widgetInteractionMode property with gear/trash role mapping
  - NavbarController widgetConfigRequested/widgetDeleteRequested signals
  - WidgetGridModel widgetMeta() Q_INVOKABLE for widget metadata lookup
  - ActionRegistry navbar.gear.tap/trash.tap handlers
  - QML navbar icon swap (gear/trash replaces volume/brightness during selection)
  - Widget display name in center navbar control during selection
  - Gear/trash dimming for no-config/singleton widgets
  - PGM-04 empty page auto-delete on widget removal via trash
  - CLN-03 badge button removal (X delete, gear config, corner resize handle)
affects: [26-02-edge-resize-handles, 27-long-press-picker]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "NavbarController mode-aware controlRole dispatch"
    - "Binding element for QML-to-C++ property sync"
    - "_skipPageCleanup flag to prevent deferred cleanup race"

key-files:
  created: []
  modified:
    - src/ui/NavbarController.hpp
    - src/ui/NavbarController.cpp
    - src/ui/WidgetGridModel.hpp
    - src/ui/WidgetGridModel.cpp
    - src/main.cpp
    - qml/components/Navbar.qml
    - qml/components/NavbarControl.qml
    - qml/applications/home/HomeMenu.qml
    - tests/test_navbar_controller.cpp
    - tests/test_widget_grid_model.cpp

key-decisions:
  - "widgetInteractionMode bound from QML via Binding element (selectedInstanceId !== '' && draggingInstanceId === '')"
  - "Side controls are tap-only in widget mode -- handlePress suppresses hold timers, handleRelease forces Tap gesture"
  - "Gear/trash dimming via widgetDisplayName/selectedWidgetHasConfig/selectedWidgetIsSingleton Q_PROPERTYs on NavbarController"
  - "PGM-04 uses _skipPageCleanup flag to prevent deselectWidget deferred cleanup from iterating shifted page indices"
  - "Corner resize handle removed entirely (will be replaced by edge handles in Plan 02)"

patterns-established:
  - "NavbarController mode-aware dispatch: controlRole returns context-dependent role strings, dispatchAction builds action IDs from them"
  - "_skipPageCleanup flag pattern: set before deselectWidget, cleared after, prevents deferred page cleanup race"

requirements-completed: [NAV-01, NAV-02, NAV-03, NAV-04, NAV-05, CLN-03, PGM-04]

# Metrics
duration: 9min
completed: 2026-03-21
---

# Phase 26 Plan 01: Navbar Transformation Summary

**Navbar transforms to gear/trash controls during widget selection with tap-only enforcement, dimming for edge cases, center widget name display, all badge buttons removed, and PGM-04 empty page auto-delete via trash**

## Performance

- **Duration:** 9 min
- **Started:** 2026-03-21T20:15:49Z
- **Completed:** 2026-03-21T20:24:51Z
- **Tasks:** 2 (Task 1 TDD, Task 2 standard)
- **Files modified:** 10

## Accomplishments
- NavbarController widgetInteractionMode property with gear/trash role mapping, hold timer suppression, and tap-only gesture enforcement for side controls
- WidgetGridModel widgetMeta() returning widget metadata for navbar display and dimming
- QML navbar icon swap (gear/trash replaces volume/brightness), widget name in center, controlEnabled dimming
- Badge buttons fully removed (CLN-03): X delete, gear config, corner resize handle
- PGM-04: trash button on last widget auto-deletes the page with shifted-index race prevention

## Task Commits

Each task was committed atomically:

1. **Task 1: NavbarController widgetInteractionMode + widgetMeta + action wiring (TDD)**
   - `3f00f7f` test(26-01): add failing tests for navbar widget interaction mode and widgetMeta
   - `3e9c2e6` feat(26-01): implement navbar widget interaction mode, widgetMeta, action wiring

2. **Task 2: QML navbar icon swap + badge removal + action handling + PGM-04**
   - `6f6bbdb` feat(26-01): QML navbar icon swap, badge removal, action handling, PGM-04

## Files Created/Modified
- `src/ui/NavbarController.hpp` - Added widgetInteractionMode, widgetDisplayName, selectedWidgetHasConfig, selectedWidgetIsSingleton Q_PROPERTYs; widgetConfigRequested/widgetDeleteRequested signals
- `src/ui/NavbarController.cpp` - Mode-aware controlRole(), handlePress hold timer suppression, handleRelease tap-only enforcement, property getters/setters
- `src/ui/WidgetGridModel.hpp` - Added widgetMeta() Q_INVOKABLE declaration
- `src/ui/WidgetGridModel.cpp` - widgetMeta() implementation returning widget metadata from placement + registry
- `src/main.cpp` - ActionRegistry handlers for navbar.gear.tap/shortHold/longHold and navbar.trash.tap/shortHold/longHold
- `qml/components/Navbar.qml` - Conditional gear/trash icons, dimming properties, controlEnabled on side controls
- `qml/components/NavbarControl.qml` - Hold progress suppression, controlEnabled opacity, widget name display (horizontal + vertical)
- `qml/applications/home/HomeMenu.qml` - Binding for widgetInteractionMode, Connections for config/delete signals, _skipPageCleanup flag, badge removal, PGM-04 page auto-delete
- `tests/test_navbar_controller.cpp` - 7 new tests for widget interaction mode
- `tests/test_widget_grid_model.cpp` - 2 new tests for widgetMeta

## Decisions Made
- widgetInteractionMode bound from QML via Binding element rather than set imperatively -- cleaner reactivity
- Side controls always emit Tap in widget mode regardless of press duration -- automotive touchscreens in bumpy cars produce >200ms presses for intended taps
- Defensive no-op ActionRegistry handlers for gear/trash shortHold/longHold to prevent unregistered action warnings
- PGM-04 page deletion happens before deselectWidget, with _skipPageCleanup flag to prevent the deferred empty-page cleanup from iterating over shifted indices

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Navbar transformation complete and verified (all 88 tests pass)
- Ready for Plan 02: 4-edge resize handles (gated on this plan's completion)
- resizeGhost infrastructure preserved in HomeMenu.qml for reuse by edge handles

## Self-Check: PASSED

All 10 modified files exist. All 3 task commits (3f00f7f, 3e9c2e6, 6f6bbdb) verified in git log.

---
*Phase: 26-navbar-transformation-edge-resize*
*Completed: 2026-03-21*
