---
phase: 07-edit-mode
plan: 03
subsystem: ui
tags: [qt, qml, edit-mode, drag, resize, touch, widget-grid]

requires:
  - phase: 07-edit-mode
    provides: Edit mode state machine, visual overlays, FAB, X badges, resize handle placeholders
  - phase: 05-grid-rendering
    provides: HomeMenu.qml grid rendering with delegates, cellWidth/cellHeight, WidgetGridModel bindings
provides:
  - Drag-to-reposition interaction (hold-then-drag with snap-to-grid)
  - Drag-to-resize via corner handle with ghost rectangle preview
  - Drop highlight with valid/invalid color feedback
  - Atomic model writes on release only (EDIT-09)
  - Drag overlay MouseArea for consistent behavior across widget types
affects: [08-multi-page]

tech-stack:
  added: []
  patterns: [hold-then-drag with mapToItem coordinate tracking, ghost rectangle resize preview, drag overlay MouseArea pattern]

key-files:
  created: []
  modified:
    - qml/applications/home/HomeMenu.qml

key-decisions:
  - "Drag overlay MouseArea added for consistent drag behavior across all widget types (interactive and non-interactive)"
  - "Legacy context menu overlay removed -- replaced by edit mode interactions"
  - "All widgets require long-press to select before dragging (prevents accidental drags)"

patterns-established:
  - "Drag overlay pattern: z:25 MouseArea over widget content prevents interactive widgets from stealing press events during edit mode"
  - "Atomic writes: model mutations (moveWidget/resizeWidget) only on touch release, never during drag"

requirements-completed: [EDIT-03, EDIT-04, EDIT-09]

duration: ~30min
completed: 2026-03-13
---

# Phase 07 Plan 03: Drag Interactions Summary

**Drag-to-reposition and drag-to-resize touch interactions with snap-to-grid, ghost rectangle preview, valid/invalid drop feedback, and atomic model commits on release**

## Performance

- **Duration:** ~30 min (across multiple sessions including Pi verification and fixes)
- **Tasks:** 3
- **Files modified:** 1

## Accomplishments
- Drag-to-reposition: 200ms hold activates drag, widget follows finger at 50% opacity, snaps to valid grid cell or animates back
- Drag-to-resize: corner handle drag shows ghost rectangle preview, clamped to min/max constraints, atomic resize on release
- Drop highlight with accent color for valid targets, red for invalid/occupied cells
- Drag overlay MouseArea ensures consistent behavior for both interactive (Now Playing) and static widgets
- Legacy context menu overlay removed -- edit mode is the sole customization entry point
- All model writes atomic (on release only), satisfying EDIT-09

## Task Commits

Each task was committed atomically:

1. **Task 1: Drag-to-reposition interaction** - `a2f53ec` (feat)
2. **Task 2: Drag-to-resize via corner handle** - `82cea46` (feat)
3. **Task 3: Pi touchscreen verification** - `adaf422`, `7245e0b` (fix)

## Files Created/Modified
- `qml/applications/home/HomeMenu.qml` - Drag-to-reposition, drag-to-resize, drop highlight, resize ghost, drag overlay MouseArea, context menu removal (328 insertions, 148 deletions)

## Decisions Made
- Added drag overlay MouseArea (z:25) to handle long-press and drag uniformly -- interactive widgets like Now Playing were stealing press events from the underlying edit-mode MouseArea
- Removed legacy context menu overlay entirely -- edit mode replaces all previous widget customization flows
- Long-press required to select widget before drag starts -- prevents accidental repositioning

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Interactive widget long-press not routing to drag in edit mode**
- **Found during:** Task 3 (Pi verification)
- **Issue:** Widgets with interactive content (Now Playing controls, navigation tap) consumed press events, preventing the edit-mode long-press from initiating drag
- **Fix:** Added drag overlay MouseArea at z:25 that sits above widget content during edit mode, capturing long-press and drag events before interactive content can steal them
- **Files modified:** qml/applications/home/HomeMenu.qml
- **Committed in:** adaf422

**2. [Rule 1 - Bug] Inconsistent drag behavior and stale context menu code**
- **Found during:** Task 3 (Pi verification)
- **Issue:** Drag behavior differed between widget types; legacy context menu overlay from pre-edit-mode era was still present and could interfere
- **Fix:** Unified drag initiation through the overlay MouseArea for all widgets; removed legacy context menu overlay (replaced by edit mode)
- **Files modified:** qml/applications/home/HomeMenu.qml
- **Committed in:** 7245e0b

---

**Total deviations:** 2 auto-fixed (2 bugs found during Pi touch verification)
**Impact on plan:** Both fixes necessary for correct touch behavior on actual hardware. No scope creep.

## Issues Encountered
- Interactive widgets (Now Playing with playback controls) required a different touch routing strategy than static widgets -- resolved with the drag overlay approach

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Edit mode is fully complete: enter, drag, resize, add, remove, exit -- all Pi-verified
- Phase 07 (Edit Mode) is done -- all 10 EDIT requirements satisfied
- Ready for Phase 08 (Multi-Page) which depends on Phase 07 for swipe-disable coupling during edit mode

## Self-Check: PASSED

All files and commits verified:
- HomeMenu.qml: FOUND
- 07-03-SUMMARY.md: FOUND
- Commits a2f53ec, 82cea46, adaf422, 7245e0b: ALL FOUND

---
*Phase: 07-edit-mode*
*Completed: 2026-03-13*
