---
phase: 11-widget-framework-polish
plan: 02
subsystem: ui
tags: [qml, widgets, widget-contract, action-registry, span-breakpoints]

requires:
  - phase: 11-widget-framework-polish (plan 01)
    provides: WidgetInstanceContext with colSpan/rowSpan/isCurrentPage/providers, ActionRegistry actions
provides:
  - 6 reference-implementation widgets fully compliant with formalized widget contract
  - Span-based responsive breakpoints (no pixel checks)
  - Provider injection via widgetContext (no root context globals)
  - ActionRegistry command egress (no raw PluginModel/ApplicationController)
  - Standardized placeholder states and pressAndHold forwarding
affects: [third-party-plugins, widget-documentation]

tech-stack:
  added: []
  patterns:
    - "widgetContext property pattern for context injection"
    - "colSpan/rowSpan breakpoints replacing pixel width/height checks"
    - "ActionRegistry.dispatch for app-level navigation commands"
    - "pressAndHold forwarding on all MouseAreas (z:-1 background + interactive controls)"
    - "Provider null-safety: widgetContext && widgetContext.provider ? ... : fallback"

key-files:
  created: []
  modified:
    - qml/widgets/ClockWidget.qml
    - qml/widgets/AAStatusWidget.qml
    - qml/widgets/NowPlayingWidget.qml
    - qml/widgets/NavigationWidget.qml
    - qml/widgets/SettingsLauncherWidget.qml
    - qml/widgets/AALauncherWidget.qml

key-decisions:
  - "NowPlayingWidget media transport controls stay as typed provider methods (widgetContext.mediaStatus.previous/playPause/next) -- correct per contract, not ActionRegistry candidates"
  - "Launcher widgets keep proportional icon sizing (Math.min * 0.6) rather than UiMetrics.iconSize -- appropriate for single-icon container widgets"
  - "All control MouseAreas on NowPlayingWidget forward pressAndHold in addition to background z:-1 area"

patterns-established:
  - "Widget contract: property QtObject widgetContext: null + span convenience props + provider access"
  - "Placeholder state: 40% opacity, centered icon + optional text, span-gated visibility"
  - "Command egress: ActionRegistry.dispatch for app navigation, provider methods for domain actions"

requirements-completed: [WF-04]

duration: 5min
completed: 2026-03-15
---

# Phase 11 Plan 02: Widget QML Rewrite Summary

**All 6 widget QML files rewritten to formalized contract: span-based breakpoints, widgetContext providers, ActionRegistry egress, standardized placeholders, universal pressAndHold forwarding**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-15T01:16:05Z
- **Completed:** 2026-03-15T01:21:00Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Eliminated all pixel breakpoints (width >= / height >=) from widget QML -- replaced with colSpan/rowSpan checks
- Eliminated all root context global access (typeof ProjectionStatus/NavigationProvider/MediaStatus) -- replaced with widgetContext.provider pattern
- Eliminated all raw PluginModel.setActivePlugin / ApplicationController.navigateTo calls -- replaced with ActionRegistry.dispatch
- Added pressAndHold forwarding to all MouseAreas including NowPlayingWidget's 6 control buttons
- All 6 widgets now serve as reference implementations for third-party widget developers

## Task Commits

Each task was committed atomically:

1. **Task 1: Rewrite 4 content widgets to span-based contract + ActionRegistry egress** - `68001a6` (feat)
2. **Task 2: Update 2 launcher widgets for contract compliance + ActionRegistry egress** - `0942030` (feat)

## Files Created/Modified
- `qml/widgets/ClockWidget.qml` - Span breakpoints, widgetContext, isCurrentPage, pressAndHold
- `qml/widgets/AAStatusWidget.qml` - widgetContext.projectionStatus, ActionRegistry dispatch
- `qml/widgets/NowPlayingWidget.qml` - widgetContext.mediaStatus, span breakpoints, control pressAndHold
- `qml/widgets/NavigationWidget.qml` - widgetContext.navigationProvider, ActionRegistry dispatch
- `qml/widgets/SettingsLauncherWidget.qml` - widgetContext, ActionRegistry("app.openSettings")
- `qml/widgets/AALauncherWidget.qml` - widgetContext, ActionRegistry("app.launchPlugin", ...)

## Decisions Made
- NowPlayingWidget media transport controls kept as typed provider methods (widgetContext.mediaStatus.previous/playPause/next) -- these are domain-specific provider methods, not ActionRegistry candidates per CONTEXT.md
- Launcher widgets retain proportional icon sizing (Math.min * 0.6) rather than switching to UiMetrics.iconSize -- appropriate for container-proportional rendering
- All 6 control MouseAreas on NowPlayingWidget (3 full layout + 3 compact layout) now forward pressAndHold

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All 6 widgets fully compliant with the formalized widget contract
- Cross-build verification (item 10 in plan verification) deferred to Matt's Pi testing
- Pi sanity pass (item 11) requires Matt to test widget rendering, resize reflow, page switching
- Phase 11 complete pending Pi hardware verification

---
*Phase: 11-widget-framework-polish*
*Completed: 2026-03-15*
