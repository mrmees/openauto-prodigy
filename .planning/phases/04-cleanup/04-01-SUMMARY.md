---
phase: 04-cleanup
plan: 01
subsystem: ui
tags: [qml, cleanup, dead-code, configuration, plugin-manifest]

# Dependency graph
requires:
  - phase: 02-navbar
    provides: Navbar as replacement for TopBar/NavStrip/Clock
  - phase: 03-aa-integration
    provides: Sidebar removal during AA integration
provides:
  - Single navigation system (Navbar only) with no dead QML components
  - Clean Configuration without showTopBar/showClockInAndroidAuto
  - Clean PluginManifest without navStripOrder/navStripVisible
  - Clean YamlConfig without nav_strip defaults
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns: []

key-files:
  created: []
  modified:
    - qml/components/Shell.qml
    - src/CMakeLists.txt
    - src/core/Configuration.hpp
    - src/core/Configuration.cpp
    - src/core/YamlConfig.cpp
    - src/core/plugin/PluginManifest.hpp
    - src/core/plugin/PluginManifest.cpp
    - config/openauto_system.ini
    - tests/test_configuration.cpp
    - tests/test_plugin_manifest.cpp
    - tests/test_config_key_coverage.cpp
    - tests/data/test_config.ini
    - tests/data/test_config.yaml
    - tests/data/test_plugin.yaml
    - CLAUDE.md

key-decisions:
  - "No decisions needed -- straightforward dead code removal"

patterns-established: []

requirements-completed: [CLEAN-01, CLEAN-02, CLEAN-03, CLEAN-04]

# Metrics
duration: 9min
completed: 2026-03-05
---

# Phase 04 Plan 01: Dead Navigation UI Cleanup Summary

**Removed all dead navigation components (NavStrip, Clock, BottomBar, TopBar) and their config/manifest/YAML/test references, leaving Navbar as the sole navigation system**

## Performance

- **Duration:** 9 min
- **Started:** 2026-03-05T01:05:21Z
- **Completed:** 2026-03-05T01:14:05Z
- **Tasks:** 2
- **Files modified:** 18 (including 3 deleted)

## Accomplishments
- Deleted 4 dead QML files (NavStrip.qml, Clock.qml, BottomBar.qml, TopBar.qml) totaling ~300 lines
- Removed showTopBar/showClockInAndroidAuto from Configuration (C++ and ini files)
- Removed navStripOrder/navStripVisible from PluginManifest and nav_strip defaults from YamlConfig
- Cleaned all test files and test data of orphaned references
- Grep sweep confirms zero remaining references in src/, qml/, config/, tests/

## Task Commits

Each task was committed atomically:

1. **Task 1: Delete dead QML files and clean Shell/CMake/config references** - `425a281` (feat)
2. **Task 2: Remove dead C++ config, YAML defaults, manifest fields, and update tests/docs** - `a3c7117` (feat)

## Files Created/Modified
- `qml/components/NavStrip.qml` - Deleted (219 lines, replaced by Navbar)
- `qml/components/Clock.qml` - Deleted (16 lines, clock in NavbarControl.qml)
- `qml/components/BottomBar.qml` - Deleted (66 lines, dead code)
- `qml/components/Shell.qml` - Removed NavStrip block and updated comment
- `src/CMakeLists.txt` - Removed set_source_files_properties and QML_FILES entries
- `src/core/Configuration.hpp` - Removed showClockInAndroidAuto/showTopBar getters/setters/members
- `src/core/Configuration.cpp` - Removed load/save of dead config keys
- `src/core/YamlConfig.cpp` - Removed nav_strip default initialization
- `src/core/plugin/PluginManifest.hpp` - Removed navStripOrder/navStripVisible fields
- `src/core/plugin/PluginManifest.cpp` - Removed nav_strip YAML parsing
- `config/openauto_system.ini` - Removed show_clock_in_android_auto and show_top_bar
- `tests/test_configuration.cpp` - Removed dead config assertions
- `tests/test_plugin_manifest.cpp` - Removed testNavStripConfig test
- `tests/test_config_key_coverage.cpp` - Removed nav_strip.show_labels key
- `tests/data/test_config.ini` - Removed dead config keys
- `tests/data/test_config.yaml` - Removed nav_strip section
- `tests/data/test_plugin.yaml` - Removed nav_strip section
- `CLAUDE.md` - Updated qml/components description

## Decisions Made
None - followed plan as specified.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Cleaned orphaned nav_strip references in test data**
- **Found during:** Task 2 grep sweep
- **Issue:** test_config_key_coverage.cpp and tests/data/test_config.yaml still had nav_strip references not mentioned in plan
- **Fix:** Removed nav_strip.show_labels from coverage test and nav_strip section from test_config.yaml
- **Files modified:** tests/test_config_key_coverage.cpp, tests/data/test_config.yaml
- **Verification:** Grep sweep returns zero hits
- **Committed in:** a3c7117 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Essential for completeness. The plan's grep sweep verification would have failed without this fix.

## Issues Encountered
- test_navbar_controller has a pre-existing flaky test (testHoldProgressEmittedDuringHold - timing sensitive). Not related to changes in this plan. Out of scope.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Codebase has a single navigation system (Navbar)
- All dead component references eliminated
- Ready for any subsequent cleanup or feature work

---
*Phase: 04-cleanup*
*Completed: 2026-03-05*
