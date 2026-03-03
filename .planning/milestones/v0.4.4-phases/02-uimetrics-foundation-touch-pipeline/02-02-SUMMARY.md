---
phase: 02-uimetrics-foundation-touch-pipeline
plan: 02
subsystem: touch
tags: [evdev, touch-pipeline, display-detection, sidebar, android-auto]

# Dependency graph
requires:
  - phase: 02-01
    provides: DisplayInfo QObject with window dimension detection and signals
provides:
  - Proportional sidebar hit zones in EvdevTouchReader (no hardcoded pixel values)
  - Dynamic display dimension flow from window geometry to touch pipeline
  - ServiceDiscoveryBuilder using detected display dimensions for AA margin calculations
affects: [android-auto, touch, video-margins]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Atomic pending update pattern for cross-thread display dimension propagation"
    - "DisplayInfo as single source of truth for display dimensions across subsystems"

key-files:
  created: []
  modified:
    - src/core/aa/EvdevTouchReader.hpp
    - src/core/aa/EvdevTouchReader.cpp
    - src/plugins/android_auto/AndroidAutoPlugin.hpp
    - src/plugins/android_auto/AndroidAutoPlugin.cpp
    - src/core/aa/ServiceDiscoveryBuilder.hpp
    - src/core/aa/ServiceDiscoveryBuilder.cpp
    - src/core/aa/AndroidAutoOrchestrator.hpp
    - src/core/aa/AndroidAutoOrchestrator.cpp
    - src/main.cpp

key-decisions:
  - "Proportional sidebar sub-zones derived from sidebarPixelWidth_ instead of hardcoded 100px/80px"
  - "DisplayInfo flows through AndroidAutoPlugin to both EvdevTouchReader and Orchestrator"
  - "ServiceDiscoveryBuilder override dimensions stored as members (0 = use config fallback)"

patterns-established:
  - "DisplayInfo wiring pattern: setDisplayInfo() before initializeAll(), connect windowSizeChanged for dynamic updates"

requirements-completed: [TOUCH-01, TOUCH-02]

# Metrics
duration: 4min
completed: 2026-03-03
---

# Phase 02 Plan 02: Touch Pipeline Display Dimensions Summary

**Proportional sidebar hit zones and dynamic display dimension flow from window geometry through AA plugin to EvdevTouchReader and ServiceDiscoveryBuilder**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-03T06:24:35Z
- **Completed:** 2026-03-03T06:28:59Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments
- EvdevTouchReader sidebar zones now scale proportionally from sidebarPixelWidth_ instead of hardcoded 100px/80px magic values
- Dynamic display dimension flow: window geometry -> DisplayInfo -> AndroidAutoPlugin -> EvdevTouchReader (atomic update) + Orchestrator -> ServiceDiscoveryBuilder
- Config display.width/height is fallback only; detected values never written to config.yaml

## Task Commits

Each task was committed atomically:

1. **Task 1: Add setDisplayDimensions() to EvdevTouchReader and fix hardcoded sidebar zones** - `c819e1b` (feat)
2. **Task 2: Wire DisplayInfo through AndroidAutoPlugin and ServiceDiscoveryBuilder** - `f081d04` (feat)

## Files Created/Modified
- `src/core/aa/EvdevTouchReader.hpp` - Added setDisplayDimensions() + pending atomics
- `src/core/aa/EvdevTouchReader.cpp` - Proportional sidebar zones, display dimension consumption in processSync()
- `src/plugins/android_auto/AndroidAutoPlugin.hpp` - Added setDisplayInfo() + displayInfo_ member
- `src/plugins/android_auto/AndroidAutoPlugin.cpp` - DisplayInfo-driven dimension flow, dynamic signal wiring
- `src/core/aa/ServiceDiscoveryBuilder.hpp` - Added setDisplayDimensions() + override members
- `src/core/aa/ServiceDiscoveryBuilder.cpp` - Override dimensions in calcMargins() and buildInputDescriptor()
- `src/core/aa/AndroidAutoOrchestrator.hpp` - Added setDisplayDimensions() + displayW_/displayH_
- `src/core/aa/AndroidAutoOrchestrator.cpp` - Pass display dimensions to builder per-session
- `src/main.cpp` - Wire displayInfo to aaPlugin before initializeAll()

## Decisions Made
- Sidebar horizontal sub-zones use sidebarPixelWidth_ directly (home zone = full sidebar width, volume extends to sidebar width from right edge) -- keeps proportions correct at any display width
- Vertical sub-zones (70%/75% Y ratios) left unchanged as they were already proportional
- ServiceDiscoveryBuilder uses simple member override pattern (0 = use yamlConfig fallback) rather than a separate DisplayInfo dependency
- Orchestrator stores display dimensions and passes to builder at session creation time

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 2 complete -- both plans (DisplayInfo + UiMetrics, Touch Pipeline Dimensions) executed successfully
- Touch pipeline now scales correctly for any display resolution
- Ready for Phase 3 and beyond

---
*Phase: 02-uimetrics-foundation-touch-pipeline*
*Completed: 2026-03-03*
