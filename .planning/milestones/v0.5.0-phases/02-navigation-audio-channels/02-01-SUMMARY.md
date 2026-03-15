---
phase: 02-navigation-audio-channels
plan: 01
subsystem: aa-protocol
tags: [protobuf, navigation, turn-event, focus-indication, qt-signals]

# Dependency graph
requires:
  - phase: 01-proto-foundation
    provides: "Updated proto submodule v1.0 with NavigationTurnEvent, NavigationFocusIndication protos"
provides:
  - "NavigationTurnEvent (0x8004) parsing with all 6 fields via Qt signal"
  - "Enhanced NavigationNotification with multi-step, lane guidance, and destination data"
  - "NavigationFocusIndication handling with state tracking"
  - "NAV_TURN_EVENT and NAV_FOCUS_INDICATION message ID constants"
affects: [ui-nav-card, aa-protocol-compliance]

# Tech tracking
tech-stack:
  added: []
  patterns: ["TDD for protocol handler extensions", "Multi-signal handler pattern with backward-compat"]

key-files:
  created:
    - tests/test_navigation_channel_handler.cpp
  modified:
    - libs/open-androidauto/include/oaa/Channel/MessageIds.hpp
    - libs/open-androidauto/include/oaa/HU/Handlers/NavigationChannelHandler.hpp
    - libs/open-androidauto/src/HU/Handlers/NavigationChannelHandler.cpp
    - libs/open-androidauto/src/Messenger/ProtocolLogger.cpp
    - src/core/aa/AndroidAutoOrchestrator.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "NAV_FOCUS_INDICATION assigned provisional ID 0x8005 (gap between 0x8003 and 0x8006)"
  - "Enhanced handleNavStep emits both original navigationStepChanged (backward compat) and new navigationNotificationReceived"
  - "Focus indication always sets navFocusActive_=true (no explicit false from phone expected)"

patterns-established:
  - "TDD: write failing tests first, then implement handler code"
  - "Handler enhancement: keep original signals for backward compat, add new signals for richer data"

requirements-completed: [NAV-01, NAV-02]

# Metrics
duration: 6min
completed: 2026-03-06
---

# Phase 2 Plan 1: Navigation Channel Handler Extensions Summary

**NavigationTurnEvent (0x8004) parsed with all 6 fields, NavigationNotification enhanced with multi-step lane guidance, NavigationFocusIndication handled with state tracking**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-06T17:58:28Z
- **Completed:** 2026-03-06T18:04:28Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- NavigationTurnEvent (0x8004) parsing with road_name, maneuver_type, turn_direction, turn_icon (PNG bytes), distance_meters, distance_unit
- Enhanced NavigationNotification to iterate all steps with lane guidance (shape + is_recommended) and road info
- NavigationFocusIndication handling with opaque focus_data hex logging and navFocusActive_ state tracking
- Orchestrator debug logging for all three new navigation signals
- 7 unit tests covering full/partial/invalid payloads, multi-step lanes, and focus indication

## Task Commits

Each task was committed atomically:

1. **Task 1 (RED): Failing tests** - `a8659d0` (test)
2. **Task 1 (GREEN): Handler implementation** - `7b0c8e2` (feat)
3. **Task 2: Orchestrator wiring** - `2dccf2d` (feat)

## Files Created/Modified
- `libs/open-androidauto/include/oaa/Channel/MessageIds.hpp` - Added NAV_TURN_EVENT (0x8004) and NAV_FOCUS_INDICATION (0x8005) constants
- `libs/open-androidauto/include/oaa/HU/Handlers/NavigationChannelHandler.hpp` - New signals, private handlers, navFocusActive_ state
- `libs/open-androidauto/src/HU/Handlers/NavigationChannelHandler.cpp` - handleTurnEvent, handleFocusIndication, enhanced handleNavStep
- `libs/open-androidauto/src/Messenger/ProtocolLogger.cpp` - Named entries for new nav message IDs
- `src/core/aa/AndroidAutoOrchestrator.cpp` - Debug logging connections for 3 new nav signals
- `tests/test_navigation_channel_handler.cpp` - 7 test cases for nav handler
- `tests/CMakeLists.txt` - Registered new test target

## Decisions Made
- NAV_FOCUS_INDICATION assigned provisional ID 0x8005 -- the gap between NAV_STATE (0x8003) and NAV_STEP (0x8006). Comment notes this is provisional since the actual ID is unknown from protocol captures.
- Enhanced handleNavStep emits both the original `navigationStepChanged` signal (backward compatibility with existing event bus consumers) and the new `navigationNotificationReceived` signal with richer data.
- Focus indication always sets `navFocusActive_=true` -- there's no explicit "focus lost" message from the phone. Focus resets on channel close.

## Deviations from Plan

None -- plan executed exactly as written.

## Issues Encountered
- Pre-existing flaky test `test_navbar_controller::testHoldProgressEmittedDuringHold` fails intermittently (timing-sensitive). Unrelated to this plan's changes.

## User Setup Required
None -- no external service configuration required.

## Next Phase Readiness
- Navigation channel handler now exposes all guidance data via Qt signals
- Ready for future turn-card UI to consume navigationTurnEvent signal
- Audio channel handler extensions (Plan 02-02) can follow the same pattern

---
*Phase: 02-navigation-audio-channels*
*Completed: 2026-03-06*
