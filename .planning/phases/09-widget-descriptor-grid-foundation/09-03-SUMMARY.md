---
phase: 09-widget-descriptor-grid-foundation
plan: 03
subsystem: ui
tags: [yaml, grid, remap, widget, proportional-scaling, base-snapshot]

# Dependency graph
requires:
  - phase: 09-02
    provides: "DisplayInfo.cellSide, QML grid frame computation"
provides:
  - "YAML grid_cols/grid_rows persistence"
  - "Proportional remap algorithm with spiral nudge and page spill"
  - "Base snapshot architecture (basePlacements_ + livePlacements_)"
  - "Boot readiness guard for safe startup sequencing"
  - "Edit mode deferral of remap"
affects: [10-launcher-dock-migration, 11-framework-polish]

# Tech tracking
tech-stack:
  added: []
  patterns: [base-snapshot-remap, spiral-nudge-placement, boot-readiness-guard]

key-files:
  created:
    - tests/test_widget_grid_remap.cpp
  modified:
    - src/core/YamlConfig.hpp
    - src/core/YamlConfig.cpp
    - src/ui/WidgetGridModel.hpp
    - src/ui/WidgetGridModel.cpp
    - src/main.cpp
    - qml/applications/home/HomeMenu.qml
    - tests/test_yaml_config.cpp
    - tests/test_widget_grid_model.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Remap clamps oversized spans to fit grid (then nudges/spills) rather than hiding -- preserves widget visibility whenever possible"
  - "promoteToBase() called on every user edit (place/move/resize/remove) -- ensures live always reflects latest user intent as base"
  - "Existing test_widget_grid_model tests updated: destructive hide-on-overlap replaced with nudge behavior"

patterns-established:
  - "Base snapshot pattern: basePlacements_ is source of truth from YAML, livePlacements_ is runtime; remap always derives from base"
  - "Boot readiness guard: setGridDimensions defers until both setPlacements and setSavedDimensions have been called"
  - "Spiral nudge: expanding-ring search from target position for non-overlapping placement"

requirements-completed: [GL-03]

# Metrics
duration: 9min
completed: 2026-03-14
---

# Phase 09 Plan 03: Grid Remap Algorithm Summary

**YAML grid_cols/grid_rows persistence with proportional remap, spiral nudge, page spill, and base snapshot drift prevention**

## Performance

- **Duration:** 9 min
- **Started:** 2026-03-14T16:34:17Z
- **Completed:** 2026-03-14T16:43:44Z
- **Tasks:** 1 (TDD: test + implement)
- **Files modified:** 10

## Accomplishments
- YAML widget_grid section stores grid_cols and grid_rows; round-trips correctly through save/load
- Proportional remap algorithm handles grow and shrink with span clamping, spiral nudge for overlaps, and page spill for overflow
- Base snapshot architecture prevents drift on resize cycles (small->large->small = same as direct small)
- Boot readiness guard ensures setGridDimensions is safe to call before data is loaded (no crash, no empty grid)
- Edit mode defers remap until user exits editing
- 12 dedicated remap tests + 2 YAML tests, all passing

## Task Commits

Each task was committed atomically:

1. **Task 1 (RED): Failing tests** - `d7308d9` (test)
2. **Task 1 (GREEN): Implementation** - `0677ee3` (feat)

## Files Created/Modified
- `src/core/YamlConfig.hpp` - Added gridSavedCols(), gridSavedRows(), setGridSavedDims() declarations
- `src/core/YamlConfig.cpp` - YAML read/write for widget_grid.grid_cols/grid_rows
- `src/ui/WidgetGridModel.hpp` - Base snapshot members, remap methods, editMode, setSavedDimensions
- `src/ui/WidgetGridModel.cpp` - Full proportional remap algorithm replacing destructive clamp-and-hide
- `src/main.cpp` - Wire setSavedDimensions at load, setGridSavedDims in save lambda
- `qml/applications/home/HomeMenu.qml` - Wire editMode to WidgetGridModel.setEditMode
- `tests/test_widget_grid_remap.cpp` - 12 remap algorithm tests
- `tests/test_yaml_config.cpp` - 2 grid dim persistence tests
- `tests/test_widget_grid_model.cpp` - Updated 3 tests for nudge-instead-of-hide behavior
- `tests/CMakeLists.txt` - Register test_widget_grid_remap

## Decisions Made
- Remap clamps oversized spans rather than hiding widgets -- maximizes visibility. Only min-span-exceeds-grid triggers hidden.
- promoteToBase() on every user edit mutation ensures the base snapshot always reflects the latest user-committed layout.
- Updated existing test_widget_grid_model tests to expect nudge behavior instead of hide behavior (behavioral change is correct per spec).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Added span clamping in remap algorithm**
- **Found during:** Task 1 (GREEN implementation)
- **Issue:** Plan spec said "spans stay original size" but didn't account for spans larger than the new grid dimensions (e.g., colSpan=4 on a 3-col grid)
- **Fix:** Added span clamping step before proportional position calculation: clamp colSpan/rowSpan to newCols/newRows, respecting minCols/minRows
- **Files modified:** src/ui/WidgetGridModel.cpp
- **Verification:** testSetGridDimensionsClampsSpan passes
- **Committed in:** 0677ee3

**2. [Rule 1 - Bug] Updated existing tests for new nudge-over-hide behavior**
- **Found during:** Task 1 (GREEN implementation)
- **Issue:** Three existing tests (testSetGridDimensionsHidesOverlapping, testHiddenWidgetsNotInOccupancy, testSetGridDimensionsClampsSpan) assumed old destructive hide-on-overlap behavior
- **Fix:** Updated tests to expect nudge behavior and renamed test methods to reflect new semantics
- **Files modified:** tests/test_widget_grid_model.cpp
- **Verification:** All 84 tests pass (1 pre-existing nav distance failure)
- **Committed in:** 0677ee3

---

**Total deviations:** 2 auto-fixed (2 bug fixes)
**Impact on plan:** Both fixes necessary for correctness with new remap algorithm. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Grid foundation (Phase 09) is now complete: DisplayInfo with cellSide, QML grid frame, and proportional remap
- Ready for Phase 10 (launcher creation + dock migration) which builds on the grid system
- Ready for Phase 11 (framework polish) which is independent of Phase 10

## Self-Check: PASSED

All 7 key files found. Both commit hashes (d7308d9, 0677ee3) verified in git log.

---
*Phase: 09-widget-descriptor-grid-foundation*
*Completed: 2026-03-14*
