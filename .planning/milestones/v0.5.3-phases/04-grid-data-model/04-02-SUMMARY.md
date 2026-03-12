---
phase: 04-grid-data-model
plan: 02
subsystem: ui
tags: [qt6, grid, widget-model, yaml-persistence, occupancy]

requires:
  - 04-grid-data-model-plan-01
provides:
  - WidgetGridModel QAbstractListModel with occupancy tracking and collision detection
  - YAML widget_grid schema v2 with gridPlacements()/setGridPlacements()
  - Phase 06 widget stubs (nav-turn, unified now-playing) pre-registered
  - Fresh install default layout (Clock + NowPlaying + AAStatus)
  - WidgetPlacementModel replaced in main.cpp wiring
affects: [05-qml-grid-renderer, 06-content-widgets, 07-edit-mode]

tech-stack:
  added: []
  patterns:
    - "Occupancy grid: flat QVector<QString> storing instanceId per cell for O(1) collision checks"
    - "Grid clamping: insertion-order priority, first-placed wins, late-comers hidden"
    - "YAML schema v2: widget_grid.placements with snake_case keys"
    - "Monotonic instance ID counter persisted in widget_grid.next_instance_id"

key-files:
  created:
    - src/ui/WidgetGridModel.hpp
    - src/ui/WidgetGridModel.cpp
  modified:
    - src/core/YamlConfig.hpp
    - src/core/YamlConfig.cpp
    - src/main.cpp
    - src/CMakeLists.txt
    - tests/test_widget_grid_model.cpp
    - tests/test_widget_config.cpp
    - tests/test_yaml_config.cpp
    - tests/data/test_widget_config.yaml

key-decisions:
  - "WidgetGridModel stores instanceId in occupancy cells (not just bool) for collision resolution"
  - "Grid clamping enforces widget min constraints -- widgets that can't fit get hidden"
  - "YAML uses snake_case (instance_id, col_span) matching project config convention"
  - "QML WidgetPlacementModel context property removed -- Phase 05 handles QML migration"
  - "Phase 06 stubs registered with empty qmlComponent -- WidgetPickerModel excludes them"

patterns-established:
  - "WidgetGridModel as QAbstractListModel with grid coordinate roles for QML Repeater"
  - "canPlace(col, row, colSpan, rowSpan, excludeInstanceId) for self-aware collision checks"
  - "Auto-save pattern: placementsChanged -> setGridPlacements + setGridNextInstanceId + save"

requirements-completed: [GRID-01, GRID-05, GRID-07, GRID-08]

duration: 14min
completed: 2026-03-12
---

# Phase 04 Plan 02: WidgetGridModel & Persistence Summary

**WidgetGridModel with occupancy-tracked collision detection, YAML v2 persistence, grid clamping, Phase 06 stubs, and default fresh-install layout**

## Performance

- **Duration:** 14 min
- **Started:** 2026-03-12T17:12:25Z
- **Completed:** 2026-03-12T17:26:59Z
- **Tasks:** 2
- **Files modified:** 10

## Accomplishments
- Built WidgetGridModel as a QAbstractListModel with 9 roles (widgetId, instanceId, column, row, colSpan, rowSpan, opacity, qmlComponent, visible)
- Occupancy grid tracks cell ownership by instanceId for O(1) collision detection
- placeWidget/moveWidget/resizeWidget/removeWidget with full bounds and overlap validation
- setGridDimensions clamps widgets when grid shrinks (first-placed wins, overlapping late-comers hidden)
- YAML widget_grid schema v2 round-trips all placement data including opacity
- Fresh install gets default layout: Clock(0,0,2x2) + NowPlaying(2,0,3x2) + AAStatus(0,2,2x1)
- Phase 06 widget stubs registered: nav-turn and unified now-playing (empty qmlComponent)
- WidgetGridModel wired in main.cpp with auto-save and DisplayInfo dimension binding
- All 74 tests pass (20 new grid model tests + 7 new config tests + 1 new yaml default test)

## Task Commits

Each task was committed atomically:

1. **Task 1: WidgetGridModel with occupancy tracking** - `47d8ff9` (feat)
2. **Task 2: YAML persistence, stubs, app wiring** - `803b5e2` (feat)

## Files Created/Modified
- `src/ui/WidgetGridModel.hpp` - QAbstractListModel with grid coordinate roles, occupancy tracking
- `src/ui/WidgetGridModel.cpp` - Implementation: place/move/resize/remove with collision detection, clamping
- `src/core/YamlConfig.hpp/cpp` - gridPlacements(), setGridPlacements(), gridNextInstanceId() methods
- `src/main.cpp` - WidgetGridModel replaces WidgetPlacementModel, Phase 06 stubs, default layout
- `src/CMakeLists.txt` - WidgetGridModel.cpp added to openauto-core
- `tests/test_widget_grid_model.cpp` - 20 tests: placement, collision, move, resize, remove, clamping, opacity, signals
- `tests/test_widget_config.cpp` - 7 tests: YAML round-trip, missing key, old config ignored
- `tests/test_yaml_config.cpp` - 1 new test: widget_grid defaults
- `tests/data/test_widget_config.yaml` - Updated to widget_grid v2 schema

## Decisions Made
- Occupancy grid stores instanceId per cell (not just bool) to answer "who occupies this cell?" during clamping
- Grid clamping enforces widget minCols/minRows constraints -- widgets that literally cannot fit in the smaller grid are hidden
- YAML schema uses snake_case keys (instance_id, col_span) consistent with the rest of the config file
- QML WidgetPlacementModel context property removed from main.cpp -- QML will show warnings until Phase 05 migrates to grid
- Phase 06 stubs registered with empty qmlComponent -- WidgetPickerModel.filterByAvailableSpace correctly excludes them

## Deviations from Plan

None -- plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None -- no external service configuration required.

## Next Phase Readiness
- WidgetGridModel is a QAbstractListModel ready for QML Repeater consumption (Phase 05)
- Grid coordinate roles exposed: column, row, colSpan, rowSpan, qmlComponent, visible, opacity
- DisplayInfo gridDimensionsChanged wired to WidgetGridModel.setGridDimensions
- Phase 06 stubs pre-registered with size constraints, awaiting QML implementation
- Old WidgetPlacementModel still compiles but is no longer wired in main.cpp
- QML migration from pane-based to grid-based rendering is Phase 05's responsibility

## Self-Check: PASSED
