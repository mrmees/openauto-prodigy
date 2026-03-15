---
phase: 04-visual-depth
plan: 04
subsystem: ui
tags: [qml, gradient, navbar, m3-buttons, theme-service]

# Dependency graph
requires:
  - phase: 04-02
    provides: M3 button components (ElevatedButton) for power menu migration
provides:
  - Gradient fade navbar separation replacing 1px barShadow border
  - Power menu buttons migrated to M3 ElevatedButton components
  - barShadow derived color removed from ThemeService
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - QML Gradient with edge-adaptive orientation for depth separation
    - outlineVariant borders as dark-theme depth fallback

key-files:
  created: []
  modified:
    - qml/components/Navbar.qml
    - src/core/services/ThemeService.cpp
    - src/core/services/ThemeService.hpp

key-decisions:
  - "Gradient fade (6px scaled) replaces 1px barShadow border for softer depth separation"
  - "Gradient hidden during AA -- black navbar blends with phone status bar"
  - "barShadow removed entirely from ThemeService (no longer needed)"
  - "Dark theme depth boosted: outlineVariant borders, higher shadow opacity, 12% surface tint, wider gradient"

patterns-established:
  - "Edge-adaptive gradient: orientation and anchor positions switch based on navbar edge config"
  - "Dark theme depth requires stronger visual cues than light theme -- borders + boosted opacity"

requirements-completed: ["STY-02"]

# Metrics
duration: 5min
completed: 2026-03-10
---

# Phase 4 Plan 4: Navbar Gradient & Power Menu Summary

**Navbar 1px barShadow border replaced with edge-adaptive gradient fade, power menu buttons migrated to M3 ElevatedButton, dark theme depth boosted with borders and stronger shadows**

## Performance

- **Duration:** ~5 min (continuation -- tasks 1-2 completed by prior agent, task 3 checkpoint approved)
- **Started:** 2026-03-10T15:50:50Z
- **Completed:** 2026-03-10T15:51:00Z
- **Tasks:** 3
- **Files modified:** 3

## Accomplishments
- Navbar separation changed from hard 1px barShadow border to soft gradient fade (6px scaled)
- Gradient adapts to all 4 navbar edge positions (top/bottom/left/right) with correct orientation
- Gradient hidden during Android Auto (black navbar blends with phone status bar)
- Power menu buttons migrated from custom Rectangles to M3 ElevatedButton components
- barShadow derived color completely removed from ThemeService
- Dark theme depth visibility improved: outlineVariant borders, boosted shadow opacity, 12% surface tint, wider gradient

## Task Commits

Each task was committed atomically:

1. **Task 1: Replace navbar barShadow border with gradient fade and migrate power menu buttons** - `76376dd` (feat)
2. **Task 2: Remove barShadow derived color from ThemeService** - `a1799c9` (refactor)
3. **Task 3: Visual verification on Pi hardware** - checkpoint approved (no commit)

**Dark theme depth fix:** `996fcf6` (fix) -- post-verification adjustment

## Files Created/Modified
- `qml/components/Navbar.qml` - Gradient fade separation, M3 power menu buttons, edge-adaptive gradient positioning
- `src/core/services/ThemeService.cpp` - barShadow derived color computation removed
- `src/core/services/ThemeService.hpp` - barShadow Q_PROPERTY and getter removed

## Decisions Made
- Gradient fade (6px scaled) replaces 1px barShadow border for softer depth separation
- Gradient hidden during AA -- black navbar blends with phone status bar
- barShadow removed entirely from ThemeService (no longer needed)
- Dark theme depth boosted: outlineVariant borders, higher shadow opacity, 12% surface tint, wider gradient

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Dark theme depth invisible on Pi**
- **Found during:** Task 3 (visual verification)
- **Issue:** Shadows and gradient were effectively invisible on dark themes -- insufficient contrast between dark surface colors and shadow/gradient
- **Fix:** Added outlineVariant borders to elevated surfaces, increased shadow opacity, boosted surface tint from 7% to 12%, widened navbar gradient
- **Files modified:** Multiple QML files (navbar, buttons, tiles, dialogs)
- **Verification:** User confirmed improved visibility on Pi hardware
- **Committed in:** `996fcf6`

---

**Total deviations:** 1 auto-fixed (1 bug fix)
**Impact on plan:** Dark theme depth fix was necessary for correctness on target hardware. No scope creep.

## Issues Encountered
None beyond the dark theme depth adjustment documented above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Visual depth system complete across all UI elements
- All navbar, tile, button, dialog, and popup surfaces have appropriate M3 elevation
- Dark and light themes both verified on Pi hardware

## Self-Check: PASSED

All commits verified (76376dd, a1799c9, 996fcf6). All key files exist. SUMMARY.md created.

---
*Phase: 04-visual-depth*
*Completed: 2026-03-10*
