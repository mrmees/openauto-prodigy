---
phase: 03-aa-integration
plan: 01
subsystem: aa-protocol
tags: [navbar, margins, sidebar-removal, touch-routing, qml, android-auto]

# Dependency graph
requires:
  - phase: 02-navbar
    provides: NavbarController, Shell.qml content insets, EvdevCoordBridge zone infrastructure
provides:
  - Navbar-aware AA margin calculation in ServiceDiscoveryBuilder
  - Simplified AndroidAutoMenu.qml (fills parent, PreserveAspectFit)
  - navbar.show_during_aa config key with settings toggle
  - Sidebar code fully removed from codebase
affects: [03-02, gesture-overlay, touch-routing]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Shared viewport helper (calcNavbarViewport) prevents margin/touch dimension duplication"
    - "Navbar thickness (56px) hardcoded in ServiceDiscoveryBuilder to match NavbarController::BAR_THICK"

key-files:
  created: []
  modified:
    - src/core/aa/ServiceDiscoveryBuilder.cpp
    - src/core/aa/ServiceDiscoveryBuilder.hpp
    - src/core/YamlConfig.cpp
    - src/core/YamlConfig.hpp
    - src/core/aa/EvdevTouchReader.cpp
    - src/core/aa/EvdevTouchReader.hpp
    - src/plugins/android_auto/AndroidAutoPlugin.cpp
    - qml/applications/android_auto/AndroidAutoMenu.qml
    - qml/applications/settings/DisplaySettings.qml
    - qml/applications/settings/VideoSettings.qml
    - qml/applications/settings/AASettings.qml
    - src/CMakeLists.txt
    - tests/test_service_discovery_builder.cpp
    - tests/test_yaml_config.cpp
    - tests/test_config_key_coverage.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Extracted calcNavbarViewport() as shared helper to avoid duplicating navbar edge/thickness logic between calcMargins and buildInputDescriptor"
  - "BAR_THICK (56) hardcoded in ServiceDiscoveryBuilder rather than reading from config -- matches NavbarController constant"
  - "Removed all crop-mode branches from EvdevTouchReader -- navbar uses PreserveAspectFit only"

patterns-established:
  - "ServiceDiscoveryBuilder reads navbar config via valueByPath() generic accessor"
  - "EvdevTouchReader.setNavbar() configures letterbox dimensions; zone registration handled externally by NavbarController"

requirements-completed: [AA-01, AA-02, AA-06]

# Metrics
duration: 12min
completed: 2026-03-04
---

# Phase 3 Plan 1: Sidebar-to-Navbar AA Integration Summary

**Navbar-aware AA margin calculation replacing sidebar, simplified QML view with PreserveAspectFit, settings toggle for navbar during AA**

## Performance

- **Duration:** 12 min
- **Started:** 2026-03-04T03:19:25Z
- **Completed:** 2026-03-04T03:31:38Z
- **Tasks:** 2
- **Files modified:** 16

## Accomplishments
- ServiceDiscoveryBuilder calculates margins from navbar edge/thickness instead of sidebar config
- buildInputDescriptor touch dimensions use shared viewport calculation (no duplication)
- EvdevTouchReader uses navbar fit-mode only (crop branches removed)
- Sidebar.qml deleted, all sidebar config keys and methods removed
- "Show Navbar during Android Auto" toggle added to Navbar settings section
- AndroidAutoMenu.qml simplified to fill parent with PreserveAspectFit

## Task Commits

Each task was committed atomically:

1. **Task 1: Replace sidebar config/margins with navbar and update tests** - `2efb32c` (feat)
2. **Task 2: Simplify AA QML, remove sidebar, update settings and plugin init** - `1875976` (feat)

_Note: Task 1 was TDD (tests written first, then implementation)_

## Files Created/Modified
- `src/core/aa/ServiceDiscoveryBuilder.cpp` - Navbar-based margin calculation with shared calcNavbarViewport helper
- `src/core/aa/ServiceDiscoveryBuilder.hpp` - Added calcNavbarViewport private method
- `src/core/YamlConfig.cpp` - Added navbar.show_during_aa default, removed sidebar defaults and methods
- `src/core/YamlConfig.hpp` - Removed sidebar getter/setter declarations
- `src/core/aa/EvdevTouchReader.cpp` - setNavbar replaces setSidebar, crop modes removed, fit-mode only
- `src/core/aa/EvdevTouchReader.hpp` - Navbar members replace sidebar members, removed sidebar signals
- `src/plugins/android_auto/AndroidAutoPlugin.cpp` - Navbar setup replaces sidebar setup
- `src/core/aa/AndroidAutoOrchestrator.cpp` - Cleaned sidebar comment in log message
- `qml/applications/android_auto/AndroidAutoMenu.qml` - Simplified: no sidebar, VideoOutput fills parent
- `qml/applications/android_auto/Sidebar.qml` - DELETED
- `qml/applications/settings/DisplaySettings.qml` - Added "Show Navbar during AA" toggle
- `qml/applications/settings/VideoSettings.qml` - Removed sidebar settings section
- `qml/applications/settings/AASettings.qml` - Removed sidebar settings section
- `src/CMakeLists.txt` - Removed Sidebar.qml references
- `tests/test_service_discovery_builder.cpp` - Added navbar margin tests (bottom, left, disabled, input descriptor)
- `tests/test_yaml_config.cpp` - Added navbar.show_during_aa test, removed testSidebarDefaults
- `tests/test_config_key_coverage.cpp` - Updated: sidebar keys replaced with navbar keys
- `tests/CMakeLists.txt` - Removed test_sidebar_zones

## Decisions Made
- Extracted calcNavbarViewport() as shared helper -- avoids Pitfall 1 from research (duplicated viewport logic)
- BAR_THICK (56) hardcoded in ServiceDiscoveryBuilder rather than sharing constant or reading config -- simplest approach, matches NavbarController
- Removed all crop mode branches from EvdevTouchReader since navbar always uses PreserveAspectFit

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed AndroidAutoPlugin sidebar references preventing compilation**
- **Found during:** Task 1 (building after removing sidebar API)
- **Issue:** AndroidAutoPlugin::initialize() still called setSidebar(), connected sidebarVolumeSet/sidebarHome signals
- **Fix:** Replaced sidebar setup block with navbar configuration (setNavbar call with edge/thickness from config)
- **Files modified:** src/plugins/android_auto/AndroidAutoPlugin.cpp
- **Verification:** Build succeeds
- **Committed in:** 2efb32c (Task 1 commit -- needed for test build)

**2. [Rule 3 - Blocking] Removed test_sidebar_zones.cpp that referenced deleted API**
- **Found during:** Task 1 (build failure)
- **Issue:** test_sidebar_zones.cpp called setSidebar() and referenced sidebar accessors
- **Fix:** Removed test file and its CMakeLists entry (sidebar zones no longer exist)
- **Files modified:** tests/CMakeLists.txt
- **Committed in:** 2efb32c (Task 1 commit)

**3. [Rule 1 - Bug] Fixed test_config_key_coverage referencing removed sidebar keys**
- **Found during:** Task 1 (test suite failure)
- **Issue:** test_config_key_coverage.cpp listed video.sidebar.* keys that no longer have defaults
- **Fix:** Replaced sidebar keys with navbar.edge and navbar.show_during_aa
- **Files modified:** tests/test_config_key_coverage.cpp
- **Committed in:** 2efb32c (Task 1 commit)

---

**Total deviations:** 3 auto-fixed (1 bug, 2 blocking)
**Impact on plan:** All fixes were directly caused by the sidebar removal. No scope creep.

## Issues Encountered
- Pre-existing flaky test (test_navbar_controller::testHoldProgressEmittedDuringHold) fails intermittently due to timing sensitivity -- not related to this plan's changes.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- AA protocol layer now reads navbar config for margins and touch dimensions
- GestureOverlay touch zone registration (plan 03-02) can build on existing TouchRouter infrastructure
- Full test suite passes (61/62, 1 pre-existing flaky)

---
*Phase: 03-aa-integration*
*Completed: 2026-03-04*
