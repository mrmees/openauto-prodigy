---
phase: 05-static-grid-rendering-widget-revision
plan: 02
subsystem: ui
tags: [qt6, qml, widgets, responsive-layout, breakpoints]

requires:
  - phase: 05-static-grid-rendering-widget-revision
    provides: HomeMenu.qml grid renderer with Repeater over WidgetGridModel
provides:
  - Clock widget with 3-tier responsive layout (time / time+date / time+date+day)
  - AA Status widget with 2-tier responsive layout (icon-only / icon+text)
  - Now Playing widget with 2-tier responsive layout (compact strip / full media card)
affects: [06-content-widgets, 07-edit-mode]

tech-stack:
  added: []
  patterns:
    - "Pixel-based breakpoints (width/height) for widget responsive layout instead of isMainPane boolean"
    - "showDate/showDay/showText/isFullLayout boolean properties derived from widget dimensions"

key-files:
  created: []
  modified:
    - qml/widgets/ClockWidget.qml
    - qml/widgets/AAStatusWidget.qml
    - qml/widgets/NowPlayingWidget.qml

key-decisions:
  - "Clock date format simplified to 'MMMM d' (no year) -- year is irrelevant on a car display"
  - "AAStatusWidget uses ColumnLayout for both tiers (icon above text at 2x1+, icon only at 1x1)"
  - "NowPlaying compact strip omits artist text entirely -- title+play/pause only at 2x1"

patterns-established:
  - "Widget breakpoint pattern: readonly property bool showX: width >= N"
  - "Font weight varies by tier: bold at smallest size for glanceability, light at larger sizes"

requirements-completed: [REV-01, REV-02, REV-03]

duration: 2min
completed: 2026-03-12
---

# Phase 05 Plan 02: Widget Revision Summary

**All three shipped widgets revised with pixel-based width/height breakpoints replacing legacy isMainPane boolean for grid-responsive layout**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-12T19:22:47Z
- **Completed:** 2026-03-12T19:24:39Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- ClockWidget: 3-tier layout (1x1 bold time only, 2x1 time+date, 2x2+ time+date+day) using width>=250 and height>=200 breakpoints
- AAStatusWidget: 2-tier layout (1x1 large centered icon, 2x1+ icon+text) using width>=250 breakpoint; removed "Android Auto" subtitle
- NowPlayingWidget: 2-tier layout (compact horizontal strip vs full media card) using width>=400 && height>=200 breakpoint
- Zero references to isMainPane, widgetContext, or WidgetPlacementModel in any widget QML file
- All font sizes use UiMetrics tokens -- no hardcoded pixel values
- All 74 tests pass

## Task Commits

Each task was committed atomically:

1. **Task 1: Revise ClockWidget.qml with pixel breakpoints** - `49c724e` (feat)
2. **Task 2: Revise AAStatusWidget.qml and NowPlayingWidget.qml with pixel breakpoints** - `4143ea7` (feat)

## Files Created/Modified
- `qml/widgets/ClockWidget.qml` - 3-tier responsive clock: time-only (1x1), time+date (2x1), time+date+day (2x2+)
- `qml/widgets/AAStatusWidget.qml` - 2-tier responsive AA status: icon-only (1x1), icon+text (2x1+)
- `qml/widgets/NowPlayingWidget.qml` - 2-tier responsive now playing: compact strip (2x1), full media card (3x2+)

## Decisions Made
- Clock date format simplified to "MMMM d" (no year) -- year is irrelevant on a car display, saves horizontal space
- AAStatusWidget uses ColumnLayout for both tiers with icon size changing (2x at 1x1, 1.5x at 2x1+) rather than switching between Row/Column layouts
- NowPlaying compact strip omits artist entirely (just icon + title + play/pause) -- too tight at 2x1 to be readable
- Removed transparent Rectangle wrappers from all three widgets -- grid's glass card background handles this now
- MouseArea in AAStatusWidget moved outside ColumnLayout (direct child of root Item) for full-area tap coverage

## Deviations from Plan

None -- plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None -- no external service configuration required.

## Next Phase Readiness
- All three shipped widgets use pixel-based breakpoints and render correctly at any grid cell size
- Ready for Phase 06 (content widgets) -- new widgets should follow the same breakpoint pattern
- Ready for Phase 07 (edit mode) -- widgets adapt automatically when resized via drag handles
- WidgetPlacementModel C++ class still exists but is completely unused from QML

## Self-Check: PASSED

All files verified present, all commits verified in git log.

---
*Phase: 05-static-grid-rendering-widget-revision*
*Completed: 2026-03-12*
