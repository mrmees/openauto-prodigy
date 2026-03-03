---
phase: 01-touch-routing
plan: 01
subsystem: touch-input
tags: [evdev, touch-routing, zone-dispatch, coordinate-conversion, tdd]

# Dependency graph
requires: []
provides:
  - TouchRouter: pure C++ zone-based touch dispatch with priority and finger stickiness
  - EvdevCoordBridge: QObject pixel-to-evdev coordinate conversion and zone management
  - test_touch_router: 10 unit tests covering dispatch, stickiness, dynamic zones, boundaries
  - test_evdev_coord_bridge: 7 unit tests covering conversion, zone lifecycle, display mapping
affects: [01-touch-routing plan 02, 02-gesture-engine, 03-sidebar-rebuild]

# Tech tracking
tech-stack:
  added: []
  patterns: [zone-based touch dispatch, mutex-guarded copy-on-read, pixel-to-evdev bridge]

key-files:
  created:
    - src/core/aa/TouchRouter.hpp
    - src/core/aa/TouchRouter.cpp
    - src/core/aa/EvdevCoordBridge.hpp
    - src/core/aa/EvdevCoordBridge.cpp
    - tests/test_touch_router.cpp
    - tests/test_evdev_coord_bridge.cpp
  modified:
    - src/CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Used std::mutex with copy-on-read instead of atomic shared_ptr -- simpler, debuggable, zone updates are rare"
  - "TouchRouter has zero Qt dependencies -- pure C++ with std::function callbacks, testable without QApplication"
  - "Inclusive boundary checking on zone edges (x0/y0 and x1/y1 both inside)"

patterns-established:
  - "Zone dispatch: priority-sorted vector, highest priority wins on overlap"
  - "Finger stickiness: per-slot claim map, DOWN claims, MOVE/UP follow, UP releases"
  - "EvdevCoordBridge: single source of truth for pixel-to-evdev conversion"

requirements-completed: [TOUCH-01, TOUCH-02, TOUCH-03, TOUCH-04]

# Metrics
duration: 5min
completed: 2026-03-03
---

# Phase 1 Plan 1: Touch Routing Foundation Summary

**TouchRouter with priority zone dispatch and finger stickiness, plus EvdevCoordBridge for pixel-to-evdev coordinate conversion -- 17 new unit tests, full TDD**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-03T22:57:07Z
- **Completed:** 2026-03-03T23:02:30Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- TouchRouter dispatches touches to highest-priority zone with per-slot finger stickiness
- EvdevCoordBridge converts pixel rectangles to evdev coordinates and pushes zone snapshots to TouchRouter
- 17 new unit tests (10 TouchRouter + 7 EvdevCoordBridge), full suite green at 62 tests
- Both classes integrated into openauto-core static library

## Task Commits

Each task was committed atomically (TDD: RED then GREEN):

1. **Task 1: TouchRouter RED** - `779a7b9` (test) - failing tests for zone dispatch
2. **Task 1: TouchRouter GREEN** - `d094ead` (feat) - implementation passes all 10 tests
3. **Task 2: EvdevCoordBridge RED** - `5b587d4` (test) - failing tests for coordinate conversion
4. **Task 2: EvdevCoordBridge GREEN** - `044e6e9` (feat) - implementation passes all 7 tests

## Files Created/Modified
- `src/core/aa/TouchRouter.hpp` - Zone struct, dispatch API, finger stickiness tracking (pure C++)
- `src/core/aa/TouchRouter.cpp` - Priority dispatch, claim tracking, zone snapshot swap (75 lines)
- `src/core/aa/EvdevCoordBridge.hpp` - QObject bridge for QML geometry to evdev coordinates
- `src/core/aa/EvdevCoordBridge.cpp` - Pixel-to-evdev conversion, zone list management, push to router (80 lines)
- `tests/test_touch_router.cpp` - 10 unit tests for dispatch, stickiness, priority, dynamic zones, boundaries
- `tests/test_evdev_coord_bridge.cpp` - 7 unit tests for coordinate conversion and zone registration
- `src/CMakeLists.txt` - Added TouchRouter.cpp and EvdevCoordBridge.cpp to openauto-core
- `tests/CMakeLists.txt` - Added test_touch_router and test_evdev_coord_bridge targets

## Decisions Made
- Used `std::mutex` with copy-on-read instead of `std::atomic<std::shared_ptr>` -- simpler to reason about, zone updates are rare (settings changes, not per-frame), lock hold time is microseconds
- TouchRouter has zero Qt dependencies -- pure C++ with `std::function` callbacks, testable without `QApplication`
- Zone boundaries are inclusive on all edges (x0/y0 and x1/y1) -- matches the "touch at edge is inside" expectation
- EvdevCoordBridge stores pixel coordinates internally, converts to evdev on push -- allows display mapping changes without re-registering all zones

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed boundary test expectation**
- **Found during:** Task 1 (TouchRouter GREEN phase)
- **Issue:** Test expected hitCount=2 for two DOWN+UP pairs, but UP events also invoke the callback (correct behavior). Actual hitCount=4.
- **Fix:** Corrected test expectation from 2 to 4 with clarifying comment
- **Files modified:** tests/test_touch_router.cpp
- **Verification:** All 10 tests pass
- **Committed in:** d094ead (Task 1 GREEN commit)

---

**Total deviations:** 1 auto-fixed (1 bug in test expectation)
**Impact on plan:** Test logic error, no impact on implementation.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- TouchRouter and EvdevCoordBridge are ready for Plan 02 (EvdevTouchReader integration)
- Plan 02 will wire TouchRouter into EvdevTouchReader::processSync() replacing the ~70 lines of hardcoded sidebar hit-testing
- No blockers

---
*Phase: 01-touch-routing*
*Completed: 2026-03-03*
