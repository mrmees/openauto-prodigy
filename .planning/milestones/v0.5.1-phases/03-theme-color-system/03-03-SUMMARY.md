---
phase: 03-theme-color-system
plan: 03
subsystem: ui
tags: [qt, qml, theme, aa-protocol, material-you, theming-tokens]

# Dependency graph
requires:
  - phase: 03-01
    provides: 16 base AA wire token Q_PROPERTYs on ThemeService
provides:
  - Connected Device theme (5th bundled theme) with live AA palette token ingestion
  - applyAATokens method on ThemeService for ARGB token updates
  - persistConnectedDeviceTheme for YAML persistence of received colors
  - UiConfigRequest (0x8011) sending on AA session start
  - UpdateHuUiConfigResponse (0x8012) parsing and logging
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns: [AA UiConfigRequest theming token exchange, connected-device theme as persistent cache]

key-files:
  created:
    - config/themes/connected-device/theme.yaml
  modified:
    - src/core/services/ThemeService.hpp
    - src/core/services/ThemeService.cpp
    - src/core/aa/AndroidAutoOrchestrator.hpp
    - src/core/aa/AndroidAutoOrchestrator.cpp
    - src/plugins/android_auto/AndroidAutoPlugin.cpp
    - libs/prodigy-oaa-protocol/src/HU/Handlers/VideoChannelHandler.cpp
    - tests/test_theme_service.cpp

key-decisions:
  - "AA tokens applied to both day and night color maps (phone sends one palette, not mode-specific)"
  - "YAML persistence uses HexArgb format for lossless alpha preservation"
  - "UiConfigRequest sent immediately on session Active state (after service discovery)"
  - "VideoChannelHandler parses 0x8012 response inline -- no signal/callback needed since status is informational"

patterns-established:
  - "Connected Device theme YAML doubles as persistent cache of last-received phone colors"
  - "OVERLAY_SESSION_UPDATE (0x8011) is the message ID for UiConfigRequest on the video channel"

requirements-completed: [THM-01, THM-02]

# Metrics
duration: 7min
completed: 2026-03-08
---

# Phase 3 Plan 3: Connected Device Theme + UiConfigRequest Summary

**Connected Device theme with live AA palette token ingestion via applyAATokens, plus UiConfigRequest (0x8011) sending on session start**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-08T21:06:58Z
- **Completed:** 2026-03-08T21:13:45Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- Created Connected Device theme as 5th bundled theme with default fallback palette
- Implemented applyAATokens() on ThemeService: guards on theme ID, converts ARGB, persists to YAML, emits colorsChanged
- Wired UiConfigRequest (0x8011) sending on AA session Active with 16 base token colors
- Added proper UpdateHuUiConfigResponse (0x8012) parsing with ThemingTokensStatus logging
- 4 new passing tests for token ingestion (64 total tests pass)

## Task Commits

Each task was committed atomically:

1. **Task 1: Connected Device theme + applyAATokens method (TDD)**
   - `1a87e5f` test(03-03): add failing tests for Connected Device theme + applyAATokens
   - `0e6a8e0` feat(03-03): add Connected Device theme + applyAATokens method
2. **Task 2: Wire UiConfigRequest sending on AA session start** - `3ab6b2d`

## Files Created/Modified
- `config/themes/connected-device/theme.yaml` - 5th bundled theme, persistent cache for phone colors
- `src/core/services/ThemeService.hpp` - applyAATokens public method + persistConnectedDeviceTheme private
- `src/core/services/ThemeService.cpp` - Token ingestion, YAML persistence, known-key filtering
- `src/core/aa/AndroidAutoOrchestrator.hpp` - sendUiConfigRequest method + themeService_ member
- `src/core/aa/AndroidAutoOrchestrator.cpp` - UiConfigRequest construction and sending on Active state
- `src/plugins/android_auto/AndroidAutoPlugin.cpp` - Pass ThemeService to orchestrator
- `libs/prodigy-oaa-protocol/src/HU/Handlers/VideoChannelHandler.cpp` - Parse 0x8012 response
- `tests/test_theme_service.cpp` - 4 new tests for applyAATokens

## Decisions Made
- AA tokens applied to both day and night color maps -- the phone sends one unified palette, not mode-specific colors
- YAML persistence uses HexArgb format (`#aarrggbb`) for lossless alpha channel preservation
- UiConfigRequest fires immediately on session Active (not after video channel setup) -- this matches the protocol flow where the HU pushes its theme during early session
- VideoChannelHandler handles 0x8012 response inline with logging only -- no callback needed since status is purely informational

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Modified VideoChannelHandler for response parsing**
- **Found during:** Task 2
- **Issue:** Plan only listed AndroidAutoOrchestrator files, but 0x8012 response arrives on the video channel and needs parsing in VideoChannelHandler
- **Fix:** Added UpdateHuUiConfigResponse parsing in VideoChannelHandler.cpp's onMessage switch
- **Files modified:** libs/prodigy-oaa-protocol/src/HU/Handlers/VideoChannelHandler.cpp
- **Verification:** Build passes, response logging confirmed
- **Committed in:** 3ab6b2d (Task 2 commit)

**2. [Rule 3 - Blocking] Modified AndroidAutoPlugin to pass ThemeService**
- **Found during:** Task 2
- **Issue:** Orchestrator needs ThemeService but doesn't receive it in constructor; plugin creates orchestrator
- **Fix:** Added setThemeService() call in AndroidAutoPlugin.initialize() after orchestrator creation
- **Files modified:** src/plugins/android_auto/AndroidAutoPlugin.cpp
- **Verification:** Build passes, ThemeService accessible in orchestrator
- **Committed in:** 3ab6b2d (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both auto-fixes necessary for the feature to work. No scope creep.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 3 (Theme Color System) fully complete -- all 3 plans executed
- Ready for Phase 4 (final phase of v0.5.1)
- Connected Device theme ready to receive phone colors when the phone->HU color message is discovered

---
*Phase: 03-theme-color-system*
*Completed: 2026-03-08*
