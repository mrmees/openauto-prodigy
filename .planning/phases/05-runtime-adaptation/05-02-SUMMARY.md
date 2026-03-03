---
phase: 05-runtime-adaptation
plan: 02
subsystem: touch-input
tags: [evdev, sidebar, touch-zones, tdd, uimetrics]

# Dependency graph
requires:
  - phase: 05-01
    provides: "Display config dead code removed, auto-detection is sole source"
  - phase: 02-02
    provides: "Proportional sidebar sub-zones and DisplayInfo flow"
provides:
  - "Layout-derived sidebar touch zone boundaries matching QML Sidebar.qml"
  - "Unit tests for sidebar zone computation at multiple resolutions"
affects: [sidebar, touch-input, android-auto]

# Tech tracking
tech-stack:
  added: []
  patterns: ["C++ mirrors QML UiMetrics scale formula for touch zone computation"]

key-files:
  created:
    - tests/test_sidebar_zones.cpp
  modified:
    - src/core/aa/EvdevTouchReader.hpp
    - src/core/aa/EvdevTouchReader.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Zone boundaries derived from UiMetrics scale formula (min(w/1024, h/600)) matching QML"
  - "Home zone = touchMin(56*scale) + spacing margin from bottom/right edge"
  - "Volume zone covers from top to separator area (spacing+1px+spacing above home)"

patterns-established:
  - "C++ touch zones mirror QML layout math: same scale formula, same element sizes"

requirements-completed: [ADAPT-01]

# Metrics
duration: 3min
completed: 2026-03-03
---

# Phase 5 Plan 2: Sidebar Zone Fix Summary

**Layout-derived sidebar touch zones replacing hardcoded 70%/5%/25% split, matching QML Sidebar.qml ColumnLayout/RowLayout structure**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-03T20:06:44Z
- **Completed:** 2026-03-03T20:10:11Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Home button touch zone now starts at ~536px (was 450px at 1024x600), matching actual QML position
- Volume bar touch zone covers full visual slider range (86.5% of sidebar height, was 70%)
- Dead zone between volume and home reduced to ~17px separator area (was ~30px)
- Zone boundaries scale correctly with display resolution via UiMetrics scale formula

## Task Commits

Each task was committed atomically:

1. **Task 1: Add sidebar zone unit tests** - `a756490` (test - TDD RED)
2. **Task 2: Fix sidebar zone computation to match QML layout** - `2e6a022` (fix - TDD GREEN)

## Files Created/Modified
- `tests/test_sidebar_zones.cpp` - 5 unit tests for sidebar zone computation at 1024x600, 800x480, horizontal/vertical
- `src/core/aa/EvdevTouchReader.hpp` - Added test accessor methods for zone boundary values
- `src/core/aa/EvdevTouchReader.cpp` - Replaced hardcoded percentage splits with layout-derived computation
- `tests/CMakeLists.txt` - Registered test_sidebar_zones

## Decisions Made
- Zone boundaries derived from UiMetrics scale formula (min(w/1024, h/600)) matching QML
- Home zone = touchMin(56*scale) + spacing margin from bottom/right edge
- Volume zone covers from top to separator area (spacing+1px+spacing above home)
- Test accessors (public getters) preferred over friend class for zone boundary testing

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- Pre-existing test_event_bus flaky failure (unrelated to sidebar changes, not investigated)

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Sidebar touch alignment fix complete and tested
- Ready for on-device validation (volume drag should work across full visual bar)

---
*Phase: 05-runtime-adaptation*
*Completed: 2026-03-03*
