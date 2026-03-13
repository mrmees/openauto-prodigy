---
phase: 07-edit-mode
plan: 02
subsystem: ui
tags: [qt, qml, edit-mode, widget-grid, fab, toast]

requires:
  - phase: 07-edit-mode
    provides: WidgetGridModel constraint roles, findFirstAvailableCell, Toast.qml
  - phase: 05-grid-rendering
    provides: HomeMenu.qml grid rendering with delegates and context menu
provides:
  - Edit mode state machine (editMode property, entry/exit paths)
  - Visual overlays (accent borders, X badges, resize handles, dotted grid lines)
  - FAB widget-add flow with auto-placement
  - Widget removal via X badge
  - Inactivity timeout and AA fullscreen auto-exit
affects: [07-03]

tech-stack:
  added: []
  patterns: [edit-mode state binding on delegates, FAB auto-place pattern]

key-files:
  created: []
  modified:
    - qml/applications/home/HomeMenu.qml
    - src/ui/WidgetPickerModel.hpp
    - src/ui/WidgetPickerModel.cpp

key-decisions:
  - "WidgetPickerModel extended with defaultCols/defaultRows roles for auto-placement sizing"
  - "Edit mode picker uses auto-place; legacy targeted placement preserved for context menu Change Widget"
  - "FAB positioned above LauncherDock with bottom margin offset"

patterns-established:
  - "Edit mode visual overlays: z:10 border, z:20 badges/handles, controlled by editMode binding"
  - "exitEditMode() centralizes all cleanup for consistent exit across paths"

requirements-completed: [EDIT-01, EDIT-02, EDIT-05, EDIT-06, EDIT-07, EDIT-08, EDIT-10]

duration: 6min
completed: 2026-03-13
---

# Phase 07 Plan 02: Edit Mode Visual State & Interactions Summary

**Edit mode state machine with long-press entry, visual overlays on all widgets, FAB auto-placement, X-badge removal, and three exit paths (tap/timeout/AA)**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-13T00:26:27Z
- **Completed:** 2026-03-13T00:32:30Z
- **Tasks:** 1
- **Files modified:** 3

## Accomplishments
- Full edit mode state machine: long-press enters, tap-outside/10s-timeout/AA-fullscreen exits
- All widgets show accent borders, X remove badges, and resize handle placeholders in edit mode
- Dotted grid lines overlay showing cell boundaries
- FAB opens picker with auto-placement at first available cell using widget's default size
- X badge removes widget immediately without confirmation, with toast on full grid
- WidgetPickerModel extended with defaultCols/defaultRows for proper auto-placement sizing

## Task Commits

Each task was committed atomically:

1. **Task 1: Edit mode state, visual overlays, entry/exit, and FAB/remove interactions** - `0f6dda5` (feat)

## Files Created/Modified
- `qml/applications/home/HomeMenu.qml` - Edit mode state machine, visual overlays, FAB, modified picker for auto-place
- `src/ui/WidgetPickerModel.hpp` - Added DefaultColsRole and DefaultRowsRole
- `src/ui/WidgetPickerModel.cpp` - Expose defaultCols/defaultRows in data() and roleNames()

## Decisions Made
- Extended WidgetPickerModel with defaultCols/defaultRows roles rather than using hardcoded 2x2 fallback -- enables proper per-widget sizing during auto-placement
- Kept legacy targeted placement path in picker for context menu "Change Widget" flow (removes old widget, places new at same position)
- FAB anchored to bottom-right with offset above LauncherDock area
- Edit mode picker filters by full grid dimensions (not cell-specific space) since auto-place finds the best spot

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added defaultCols/defaultRows roles to WidgetPickerModel**
- **Found during:** Task 1 (picker auto-placement)
- **Issue:** Picker needed widget default sizes for auto-placement but only exposed widgetId/displayName/iconName
- **Fix:** Added DefaultColsRole and DefaultRowsRole to WidgetPickerModel enum, data(), and roleNames()
- **Files modified:** src/ui/WidgetPickerModel.hpp, src/ui/WidgetPickerModel.cpp
- **Verification:** Build succeeds, all 76/77 tests pass (1 pre-existing failure unrelated)
- **Committed in:** 0f6dda5 (task commit)

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Essential for correct auto-placement sizing. No scope creep.

## Issues Encountered
- Pre-existing test_navigation_data_bridge failure (distance unit formatting) -- unrelated to this plan, not addressed.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Edit mode visual state complete, ready for drag-to-reposition and drag-to-resize (Plan 03)
- Resize handles are visual-only placeholders -- Plan 03 adds MouseArea drag behavior
- pressAndHold in edit mode is intentionally a no-op -- Plan 03 will add drag initiation

---
*Phase: 07-edit-mode*
*Completed: 2026-03-13*
