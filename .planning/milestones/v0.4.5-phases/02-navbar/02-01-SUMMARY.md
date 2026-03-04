---
phase: 02-navbar
plan: 01
subsystem: ui
tags: [qt, gesture, touch, navbar, tdd, qobject, qtimer]

# Dependency graph
requires:
  - phase: 01-touch-routing
    provides: TouchRouter, EvdevCoordBridge zone registration and pixel-to-evdev conversion
provides:
  - NavbarController gesture state machine (tap/short-hold/long-hold)
  - Zone registration for 4-edge navbar (top/bottom/left/right)
  - LHD/RHD control role mapping
  - Popup zone management (dismiss + slider zones)
  - Action dispatch via ActionRegistry (navbar.{role}.{gesture} IDs)
  - Config defaults for navbar.edge and gesture thresholds
affects: [02-navbar-plan-02, 03-aa-integration]

# Tech tracking
tech-stack:
  added: []
  patterns: [dual-input gesture controller, per-control QTimer state machine, zone auto-re-registration on edge change]

key-files:
  created:
    - src/ui/NavbarController.hpp
    - src/ui/NavbarController.cpp
    - tests/test_navbar_controller.cpp
  modified:
    - src/CMakeLists.txt
    - src/core/YamlConfig.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "7-second popup auto-dismiss timeout (middle of 5-10s range)"
  - "Zone layout: driver 1/4, center 1/2, passenger 1/4 of bar length"
  - "Action IDs: navbar.{role}.{gesture} pattern (e.g. navbar.volume.tap)"
  - "Progress timer at 16ms (~60fps) for smooth hold feedback"
  - "BAR_THICK = 56px constant matching UiMetrics touchMin"

patterns-established:
  - "Dual-input gesture controller: QML handlePress/Release + evdev onZoneTouch feeding same state machine"
  - "Zone auto-re-registration: setEdge() re-registers zones if previously registered"
  - "Popup zone pattern: screen-wide dismiss (priority 40) + slider (priority 60) + control zones (priority 50)"

requirements-completed: [NAV-01, NAV-02, NAV-03, NAV-04, NAV-05, NAV-06, NAV-07, EDGE-01, EDGE-03]

# Metrics
duration: 9min
completed: 2026-03-04
---

# Phase 2 Plan 01: NavbarController Summary

**TDD gesture state machine with tap/short-hold/long-hold detection, 4-edge zone registration via EvdevCoordBridge, and ActionRegistry dispatch**

## Performance

- **Duration:** 9 min
- **Started:** 2026-03-04T00:03:44Z
- **Completed:** 2026-03-04T00:13:00Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- NavbarController with full gesture timing (tap <200ms, short-hold 200-600ms, long-hold fires at 600ms)
- Zone registration for all 4 edges with LHD/RHD driver/passenger swapping
- Popup zone management: dismiss zone, slider zone, auto-dismiss timer
- Action dispatch mapping gesture signals to navbar.{role}.{gesture} action IDs
- 30 tests covering all gesture, zone, popup, and action behavior

## Task Commits

Each task was committed atomically (TDD: test + feat):

1. **Task 1: NavbarController gesture state machine (RED)** - `07ea785` (test)
2. **Task 1: NavbarController gesture state machine (GREEN)** - `39e2be3` (feat)
3. **Task 2: Zone registration and action wiring (RED)** - `155f24d` (test)
4. **Task 2: Zone registration and action wiring (GREEN)** - `f7b41a2` (feat)

## Files Created/Modified
- `src/ui/NavbarController.hpp` - Gesture state machine, zone registration, Q_PROPERTYs for QML binding
- `src/ui/NavbarController.cpp` - Implementation: gesture timing, zone callbacks, popup state, action dispatch
- `tests/test_navbar_controller.cpp` - 30 tests covering gesture detection, LHD/RHD, edge change, popup dismiss, action dispatch
- `src/CMakeLists.txt` - Added NavbarController.cpp to openauto-core library
- `src/core/YamlConfig.cpp` - Default config for navbar.edge, gesture.tap_max_ms, gesture.short_hold_max_ms, navbarThick token
- `tests/CMakeLists.txt` - Registered test_navbar_controller

## Decisions Made
- 7-second popup auto-dismiss (middle of 5-10s range per CONTEXT.md)
- Zone layout: driver=1/4, center=1/2, passenger=1/4 of bar axis
- Action ID format: `navbar.{role}.{gesture}` (e.g., `navbar.volume.tap`, `navbar.clock.longHold`)
- Hold progress at ~60fps (16ms timer) for smooth visual feedback
- BAR_THICK = 56px matching existing touchMin token
- Popup slider zone at priority 60 > navbar controls at 50 > dismiss at 40

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- NavbarController ready for QML binding (Plan 02)
- All Q_PROPERTYs and Q_INVOKABLEs exposed for QML usage
- Action IDs defined but handlers not yet registered (Plan 02 wires handlers)
- Zone registration tested with EvdevCoordBridge integration

## Self-Check: PASSED

- All 3 created files exist
- All 4 task commits verified
- NavbarController.cpp: 442 lines (min 150)
- test_navbar_controller.cpp: 527 lines (min 100)
- Full test suite: 63/63 pass

---
*Phase: 02-navbar*
*Completed: 2026-03-04*
