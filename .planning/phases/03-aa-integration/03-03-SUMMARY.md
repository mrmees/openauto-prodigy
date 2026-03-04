---
phase: 03-aa-integration
plan: 03
subsystem: ui, aa-protocol
tags: [evdev, touch-routing, gesture-overlay, navbar, fullscreen, pipewire]

# Dependency graph
requires:
  - phase: 03-01
    provides: navbar-to-AA margin integration, calcNavbarViewport helper
  - phase: 03-02
    provides: GestureOverlay zone registration, EvdevCoordBridge, TouchRouter priority system
provides:
  - Config-aware wantsFullscreen() for navbar visibility during AA
  - Evdev-to-service bridging for GestureOverlay controls during EVIOCGRAB
  - Zone dispatch during gesture suppression (overlay receives touches while AA forwarding blocked)
affects: [04-uat-validation, gesture-overlay, navbar]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Evdev touch-to-service bridging via QMetaObject::invokeMethod from reader thread"
    - "Gesture blocking separates zone dispatch from AA forwarding"

key-files:
  created: []
  modified:
    - src/plugins/android_auto/AndroidAutoPlugin.hpp
    - src/plugins/android_auto/AndroidAutoPlugin.cpp
    - src/core/aa/EvdevTouchReader.cpp
    - src/main.cpp

key-decisions:
  - "wantsFullscreen() defaults to false (navbar visible) when config unset, matching show_during_aa=true default"
  - "Zone dispatch runs before gesture-blocking return so overlay zone can claim touches"
  - "Overlay touch bridging uses approximate panel geometry regions (title/volume/brightness/buttons)"

patterns-established:
  - "Evdev-to-service pattern: reader thread dispatches to zones, zone callbacks marshal to Qt thread"

requirements-completed: [AA-01, AA-02, AA-05]

# Metrics
duration: 4min
completed: 2026-03-04
---

# Phase 3 Plan 3: Gap Closure Summary

**Config-aware navbar visibility during AA + evdev-to-service GestureOverlay bridging for volume/brightness/buttons**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-04T04:15:42Z
- **Completed:** 2026-03-04T04:19:53Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Navbar now appears during AA when show_during_aa is true (default) via config-driven wantsFullscreen()
- GestureOverlay controls (volume, brightness, home, theme toggle, close) work during AA via evdev touch bridging
- Zone dispatch continues during gesture suppression, enabling overlay to receive touches while blocking AA forwarding

## Task Commits

Each task was committed atomically:

1. **Task 1: Make wantsFullscreen() config-aware** - `dd0f263` (feat)
2. **Task 2: Fix GestureOverlay touch during AA** - `9e474c7` (feat)

## Files Created/Modified
- `src/plugins/android_auto/AndroidAutoPlugin.hpp` - Declaration change: wantsFullscreen() no longer inline
- `src/plugins/android_auto/AndroidAutoPlugin.cpp` - Config-driven wantsFullscreen() implementation reading navbar.show_during_aa
- `src/core/aa/EvdevTouchReader.cpp` - Moved zone dispatch above gesture-blocking return in processSync()
- `src/main.cpp` - Replaced no-op overlay callback with volume/brightness/button evdev bridging

## Decisions Made
- wantsFullscreen() defaults to false (navbar visible) when navbar.show_during_aa config is unset, matching the default expectation
- Overlay touch regions use approximate percentages of panel geometry rather than querying QML for exact coordinates (QML state not accessible from evdev thread)
- QML slider visuals won't update during EVIOCGRAB, but service values change -- acceptable tradeoff documented in plan

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- Pre-existing flaky test `test_navbar_controller::testHoldProgressEmittedDuringHold` fails intermittently due to timing sensitivity -- not related to this plan's changes.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- All three UAT failures from Phase 3 validation are addressed
- Ready for on-device verification (navbar visibility + overlay controls during AA session)

---
*Phase: 03-aa-integration*
*Completed: 2026-03-04*
