---
phase: 04-grid-data-model
plan: 01
subsystem: ui
tags: [qt6, grid, widget-types, displayinfo, qproperty]

requires: []
provides:
  - GridPlacement struct with col/row/colSpan/rowSpan/opacity/visible
  - WidgetDescriptor with minCols/minRows/maxCols/maxRows/defaultCols/defaultRows
  - WidgetRegistry::widgetsFittingSpace() grid-aware filter
  - WidgetInstanceContext cellWidth/cellHeight for grid cell sizing
  - DisplayInfo gridColumns/gridRows Q_PROPERTYs with density formula
  - test_widget_grid_model.cpp scaffold for Plan 02
  - test_grid_dimensions.cpp formula verification
affects: [04-grid-data-model-plan-02, 05-qml-grid-renderer]

tech-stack:
  added: []
  patterns:
    - "Grid density formula: usableW/150 cols, usableH/125 rows, clamped [3-8] x [2-6]"
    - "Legacy WidgetPlacement/PageDescriptor kept for backward compat until Plan 02"

key-files:
  created:
    - tests/test_grid_dimensions.cpp
    - tests/test_widget_grid_model.cpp
  modified:
    - src/core/widget/WidgetTypes.hpp
    - src/core/widget/WidgetRegistry.hpp
    - src/core/widget/WidgetRegistry.cpp
    - src/ui/WidgetInstanceContext.hpp
    - src/ui/WidgetInstanceContext.cpp
    - src/ui/WidgetPickerModel.hpp
    - src/ui/WidgetPickerModel.cpp
    - src/ui/DisplayInfo.hpp
    - src/ui/DisplayInfo.cpp
    - src/main.cpp
    - src/plugins/bt_audio/BtAudioPlugin.cpp
    - tests/test_widget_types.cpp
    - tests/test_widget_registry.cpp
    - tests/test_widget_instance_context.cpp
    - tests/test_widget_placement_model.cpp
    - tests/test_widget_plugin_integration.cpp
    - tests/test_display_info.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Grid density constants: 150px target cell width, 125px target cell height"
  - "Legacy WidgetPlacement and PageDescriptor kept temporarily for backward compat"
  - "WidgetPickerModel.filterForSize() renamed to filterByAvailableSpace()"
  - "1280x720 maps to 8x4 grid (not 7x4 as research estimated)"

patterns-established:
  - "Grid span constraints on WidgetDescriptor: min/max/default cols and rows"
  - "GridPlacement as the canonical placement struct (col/row/colSpan/rowSpan)"

requirements-completed: [GRID-02, GRID-04]

duration: 20min
completed: 2026-03-12
---

# Phase 04 Plan 01: Grid Data Model Types Summary

**GridPlacement struct, 6-field WidgetDescriptor spans, grid density formula in DisplayInfo (1024x600 -> 6x4, 800x480 -> 5x3)**

## Performance

- **Duration:** 20 min
- **Started:** 2026-03-12T16:44:19Z
- **Completed:** 2026-03-12T17:05:17Z
- **Tasks:** 2
- **Files modified:** 19

## Accomplishments
- Replaced WidgetSize enum with 6 grid span fields (minCols/minRows/maxCols/maxRows/defaultCols/defaultRows)
- Added GridPlacement struct with col/row/colSpan/rowSpan/opacity/visible
- DisplayInfo computes gridColumns and gridRows from window dimensions (user-specified densities verified)
- All 74 tests pass (72 existing updated + 2 new)

## Task Commits

Each task was committed atomically:

1. **Task 1: Replace pane-based types with grid types** - `d853750` (feat)
2. **Task 2: Add grid density computation to DisplayInfo** - `ff8cba5` (feat)

## Files Created/Modified
- `src/core/widget/WidgetTypes.hpp` - GridPlacement struct, WidgetDescriptor grid spans, legacy compat types
- `src/core/widget/WidgetRegistry.hpp/cpp` - widgetsFittingSpace() replaces widgetsForSize()
- `src/ui/WidgetInstanceContext.hpp/cpp` - cellWidth/cellHeight replacing paneSize
- `src/ui/WidgetPickerModel.hpp/cpp` - filterByAvailableSpace() replaces filterForSize()
- `src/ui/DisplayInfo.hpp/cpp` - gridColumns/gridRows Q_PROPERTYs with density formula
- `src/main.cpp` - Widget registrations use grid span fields
- `src/plugins/bt_audio/BtAudioPlugin.cpp` - Now Playing widget uses grid spans
- `tests/test_grid_dimensions.cpp` - Grid density formula verification for 4 display sizes
- `tests/test_widget_grid_model.cpp` - Plan 02 placeholder scaffold (7 QSKIP tests)
- `tests/test_widget_types.cpp` - GridPlacement and WidgetDescriptor grid span tests
- `tests/test_widget_registry.cpp` - widgetsFittingSpace() tests with stub filtering
- `tests/test_widget_instance_context.cpp` - cellWidth/cellHeight tests
- `tests/test_widget_placement_model.cpp` - Updated for removed supportedSizes field
- `tests/test_widget_plugin_integration.cpp` - Updated mock plugin descriptors
- `tests/test_display_info.cpp` - Grid property and signal tests

## Decisions Made
- Grid density formula uses targetCellW=150, targetCellH=125 with margins 40px + dock 56px subtracted. This hits user-specified 6x4 for 1024x600 and 5x3 for 800x480.
- 1280x720 resolves to 8x4 (not 7x4 as research estimated) -- proportional scaling is correct.
- Kept legacy WidgetPlacement and PageDescriptor structs marked deprecated for backward compat. WidgetPlacementModel and YamlConfig still use them. Plan 02 will remove both when it replaces the model.
- WidgetPickerModel method renamed from filterForSize(int) to filterByAvailableSpace(int, int).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Kept legacy WidgetPlacement and PageDescriptor**
- **Found during:** Task 1
- **Issue:** Removing WidgetPlacement entirely broke WidgetPlacementModel, YamlConfig, and main.cpp which still depend on the pane-based types
- **Fix:** Added legacy structs back as deprecated, marked for removal in Plan 02
- **Files modified:** src/core/widget/WidgetTypes.hpp
- **Verification:** Full test suite passes (74/74)
- **Committed in:** d853750

**2. [Rule 3 - Blocking] Updated WidgetPickerModel to remove WidgetSize dependency**
- **Found during:** Task 1
- **Issue:** WidgetPickerModel::filterForSize() used WidgetSize enum which was removed
- **Fix:** Replaced with filterByAvailableSpace(int availCols, int availRows) using widgetsFittingSpace()
- **Files modified:** src/ui/WidgetPickerModel.hpp, src/ui/WidgetPickerModel.cpp
- **Verification:** Build succeeds, no WidgetSize references remain in non-legacy code
- **Committed in:** d853750

**3. [Rule 1 - Bug] Tuned grid density constants**
- **Found during:** Task 2
- **Issue:** Research-suggested constants (155x145) produced wrong grid densities (1024x600 -> 6x3, 800x480 -> 4x2)
- **Fix:** Adjusted to 150x125 which correctly produces 6x4 and 5x3 for the user-specified displays
- **Files modified:** src/ui/DisplayInfo.cpp
- **Verification:** test_grid_dimensions passes for all 4 display sizes
- **Committed in:** ff8cba5

---

**Total deviations:** 3 auto-fixed (1 bug, 2 blocking)
**Impact on plan:** All fixes necessary for compilation and correctness. No scope creep.

## Issues Encountered
None beyond the auto-fixed deviations above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- GridPlacement and updated WidgetDescriptor ready for WidgetGridModel (Plan 02)
- DisplayInfo gridColumns/gridRows ready for grid dimension binding
- test_widget_grid_model.cpp scaffold in place for Plan 02 to fill in
- Legacy types still present -- Plan 02 must remove WidgetPlacementModel and legacy structs

---
*Phase: 04-grid-data-model*
*Completed: 2026-03-12*
