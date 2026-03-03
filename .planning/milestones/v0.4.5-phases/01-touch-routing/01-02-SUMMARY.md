---
phase: 01-touch-routing
plan: 02
subsystem: touch-input
tags: [evdev, touch-router, sidebar, zone-dispatch, refactor]

# Dependency graph
requires:
  - phase: 01-touch-routing/01
    provides: TouchRouter dispatch engine and EvdevCoordBridge pixel-to-evdev converter
provides:
  - TouchRouter integrated into EvdevTouchReader processSync() for all touch dispatch
  - Sidebar zones registered via TouchRouter with priority 10
  - Unclaimed touches fall through to AA forwarding
  - Zone callbacks emit sidebarVolumeSet/sidebarHome signals
affects: [02-navbar-zone, 03-gesture-actions, 04-cleanup]

# Tech tracking
tech-stack:
  added: []
  patterns: [zone-callback-dispatch, router-dispatch-before-AA-forwarding, dirty-flag-consumption]

key-files:
  created: []
  modified:
    - src/core/aa/EvdevTouchReader.hpp
    - src/core/aa/EvdevTouchReader.cpp
    - tests/test_sidebar_zones.cpp

key-decisions:
  - "Zone callbacks capture sidebar boundary members by reference via lambda -- safe because reader owns both router and boundaries"
  - "Volume zone callback tracks drag slot internally, same as old inline code"
  - "Sidebar disable calls router_.setZones({}) to clear all zones"

patterns-established:
  - "Zone registration pattern: compute pixel boundaries, create TouchZone with callback lambda, push via router_.setZones()"
  - "Dispatch integration: router dispatch runs after gesture check, before AA forwarding; consumed slots have dirty=false"

requirements-completed: [TOUCH-01, TOUCH-02, TOUCH-03, TOUCH-04, TOUCH-05]

# Metrics
duration: 3min
completed: 2026-03-03
---

# Phase 1 Plan 2: EvdevTouchReader Integration Summary

**Replaced ~70 lines of inline sidebar hit-testing with TouchRouter dispatch loop, enabling any component to register touch zones without modifying EvdevTouchReader**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-03T23:04:41Z
- **Completed:** 2026-03-03T23:08:00Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- processSync() sidebar dispatch block replaced by 35-line TouchRouter dispatch loop
- Volume drag and home tap emit identical signals as before (verified via QSignalSpy tests)
- Touches outside registered zones fall through to AA forwarding unchanged
- 3-finger gesture detection untouched (runs before dispatch)
- 5 new dispatch tests added (volume claim, home signal, drag signal, outside-zone passthrough, disable-clears-zones)

## Task Commits

Each task was committed atomically:

1. **Task 1: Integrate TouchRouter into EvdevTouchReader** - `aa4473e` (feat)
2. **Task 2: Update sidebar zone tests and verify full suite** - `7b7adaf` (test)

## Files Created/Modified
- `src/core/aa/EvdevTouchReader.hpp` - Added TouchRouter member, EvdevCoordBridge forward decl, setCoordBridge() setter, router() accessor
- `src/core/aa/EvdevTouchReader.cpp` - Replaced sidebar dispatch block with router dispatch loop; added zone registration in setSidebar(); resetClaims() in ungrab()
- `tests/test_sidebar_zones.cpp` - Added 5 new tests exercising TouchRouter dispatch path

## Decisions Made
- Zone callbacks use lambdas capturing `this` pointer for signal emission -- same thread (reader thread) so no cross-thread concerns
- Volume zone handles drag tracking inside callback (Down sets slot, Move updates volume, Up clears slot)
- Home zone only fires on Down event (matches previous behavior)
- Sidebar disable pushes empty zone vector to clear router state

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Touch routing foundation complete -- TouchRouter is live in EvdevTouchReader
- Any component can now register zones via EvdevCoordBridge (QML) or directly via TouchRouter
- Ready for Phase 2 (navbar zone registration) which will add navbar touch zones through the same mechanism
- All 62 tests passing

---
*Phase: 01-touch-routing*
*Completed: 2026-03-03*
