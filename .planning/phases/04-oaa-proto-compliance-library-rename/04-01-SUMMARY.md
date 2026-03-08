---
phase: 04-oaa-proto-compliance-library-rename
plan: 01
subsystem: protocol
tags: [cleanup, dead-code, wifi, bssid, test-fix, protobuf]

requires:
  - phase: 03-commands-authentication
    provides: handler stubs and orchestrator signal wiring for retracted protos
provides:
  - Clean handler code with no retracted stubs or dead methods
  - Correct WiFi BSSID semantics (MAC address, not SSID)
  - 64/64 test pass rate with reliable navbar hold progress test
affects: [04-02, library-rename]

tech-stack:
  added: []
  patterns:
    - "Retracted message IDs fall through to default/unknownMessage handler (Phase 1 logger catches them)"
    - "WiFi BSSID obtained from QNetworkInterface::hardwareAddress() on wlan0"

key-files:
  created: []
  modified:
    - libs/open-androidauto/src/HU/Handlers/AudioChannelHandler.cpp
    - libs/open-androidauto/include/oaa/HU/Handlers/AudioChannelHandler.hpp
    - libs/open-androidauto/src/HU/Handlers/NavigationChannelHandler.cpp
    - libs/open-androidauto/include/oaa/HU/Handlers/NavigationChannelHandler.hpp
    - libs/open-androidauto/include/oaa/Channel/MessageIds.hpp
    - libs/open-androidauto/src/Messenger/ProtocolLogger.cpp
    - src/core/aa/AndroidAutoOrchestrator.cpp
    - src/core/aa/ServiceDiscoveryBuilder.cpp
    - src/core/aa/ServiceDiscoveryBuilder.hpp
    - tests/test_audio_channel_handler.cpp
    - tests/test_service_discovery_builder.cpp
    - tests/test_navbar_controller.cpp

key-decisions:
  - "Retracted message IDs (0x8021, 0x8022, 0x8005) fall to default handler — Phase 1 unhandled logger catches them"
  - "WiFi BSSID passed as separate constructor parameter with empty string fallback"

patterns-established:
  - "QTRY_VERIFY_WITH_TIMEOUT for QTimer-based signal assertions in tests"

requirements-completed: [CLN-01, CLN-02, TST-01]

duration: 6min
completed: 2026-03-08
---

# Phase 4 Plan 1: Dead Code Cleanup Summary

**Removed all retracted proto handler stubs, fixed WiFi BSSID semantic bug (was sending SSID as BSSID), and fixed flaky navbar hold progress test — 64/64 tests green**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-08T04:08:34Z
- **Completed:** 2026-03-08T04:15:01Z
- **Tasks:** 2
- **Files modified:** 12

## Accomplishments
- Removed all retracted handler stubs, dead method declarations, dead includes, and RETRACTED comments from handler code, MessageIds, ProtocolLogger, orchestrator, and SDP builder
- Fixed WiFi BSSID bug: now sends wlan0 MAC address via QNetworkInterface instead of SSID string
- Fixed flaky testHoldProgressEmittedDuringHold: root cause was resetControlState emitting holdProgress(0.0) on release, which the spy captured as the "last" signal

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove retracted handler stubs, dead code, and fix bssid bug** - `2d5dc05` (fix)
2. **Task 2: Fix navbar controller hold progress test** - `922cfdd` (fix)

## Files Created/Modified
- `libs/open-androidauto/src/HU/Handlers/AudioChannelHandler.cpp` - Removed retracted 0x8021/0x8022 switch cases and dead handler methods
- `libs/open-androidauto/include/oaa/HU/Handlers/AudioChannelHandler.hpp` - Removed dead method declarations and retracted comment
- `libs/open-androidauto/src/HU/Handlers/NavigationChannelHandler.cpp` - Removed 0x8005 case, dead include, retracted comment
- `libs/open-androidauto/include/oaa/HU/Handlers/NavigationChannelHandler.hpp` - Removed retracted comment
- `libs/open-androidauto/include/oaa/Channel/MessageIds.hpp` - Removed AUDIO_FOCUS_STATE/AUDIO_STREAM_TYPE constants and retracted NAV comment
- `libs/open-androidauto/src/Messenger/ProtocolLogger.cpp` - Removed NAV_0x8005_RETRACTED entry
- `src/core/aa/AndroidAutoOrchestrator.cpp` - Removed retracted comments, added wlan0 MAC lookup for BSSID
- `src/core/aa/ServiceDiscoveryBuilder.cpp` - Changed set_bssid to use wifiBssid_ (MAC) instead of wifiSsid_
- `src/core/aa/ServiceDiscoveryBuilder.hpp` - Added wifiBssid_ member and constructor parameter
- `tests/test_audio_channel_handler.cpp` - Updated test to verify retracted msgs emit unknownMessage
- `tests/test_service_discovery_builder.cpp` - Updated WiFi test to verify MAC address as BSSID
- `tests/test_navbar_controller.cpp` - Fixed hold progress test with QTRY_VERIFY and pre-release capture

## Decisions Made
- Retracted message IDs (0x8021, 0x8022, 0x8005) now fall to default handler — the Phase 1 unhandled-message logger catches them if they ever arrive
- WiFi BSSID is passed as a separate constructor parameter to ServiceDiscoveryBuilder, with empty string fallback if wlan0 is not found
- Used QTRY_VERIFY_WITH_TIMEOUT instead of QTest::qWait for deterministic timer-based test assertions

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Test referenced removed MessageIds constants**
- **Found during:** Task 1
- **Issue:** test_audio_channel_handler.cpp used `oaa::AVMessageId::AUDIO_FOCUS_STATE` and `AUDIO_STREAM_TYPE` which were just deleted
- **Fix:** Changed test to use raw hex values (0x8021, 0x8022) and verify they emit unknownMessage signal
- **Files modified:** tests/test_audio_channel_handler.cpp
- **Committed in:** 2d5dc05

**2. [Rule 1 - Bug] SDP builder test expected SSID as BSSID**
- **Found during:** Task 2 (full test suite run)
- **Issue:** testWifiChannelHasSsid expected bssid() to return "TestSSID" — now it correctly uses wifiBssid_
- **Fix:** Updated test to pass MAC address as wifiBssid parameter and verify it
- **Files modified:** tests/test_service_discovery_builder.cpp
- **Committed in:** 922cfdd

---

**Total deviations:** 2 auto-fixed (2 bugs in tests referencing removed/changed code)
**Impact on plan:** Both auto-fixes necessary for test correctness. No scope creep.

## Issues Encountered
- Hold progress test root cause identified: `resetControlState` emits `holdProgress(0.0)` on release, so spy.last() was always 0.0. Fixed by capturing progress values before calling handleRelease.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Codebase is clean of retracted proto artifacts
- All 64 tests pass reliably
- Ready for Plan 02 (library rename)

---
*Phase: 04-oaa-proto-compliance-library-rename*
*Completed: 2026-03-08*
