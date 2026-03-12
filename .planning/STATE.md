---
gsd_state_version: 1.0
milestone: v0.5
milestone_name: milestone
status: completed
stopped_at: Completed 05-02-PLAN.md
last_updated: "2026-03-12T19:24:39Z"
last_activity: 2026-03-12 -- Phase 05 Plan 02 complete (Widget revision with pixel breakpoints)
progress:
  total_phases: 5
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
  percent: 25
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-12)

**Core value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.
**Current focus:** v0.5.3 Widget Grid & Content Widgets -- Phase 05 in progress

## Current Position

Phase: 05 of 08 (Static Grid Rendering & Widget Revision)
Plan: 02 complete (phase complete)
Status: Phase 05 done -- all widgets revised with pixel breakpoints
Last activity: 2026-03-12 -- Phase 05 Plan 02 complete (Widget revision with pixel breakpoints)

Progress: [##########] 100%

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.

- Roadmap: WidgetPlacementModel is a clean REPLACEMENT, not extension (pane model can't represent grid coords)
- Roadmap: Content widgets (Phase 06) before edit mode (Phase 07) -- delivers user value earlier
- Roadmap: Multi-page (Phase 08) last -- depends on edit mode for swipe disable coupling
- Roadmap: Phase 08 is the natural cut line -- ship 04-07 if schedule tightens
- Codex review: Phase 04 split into model/persistence (04) and rendering/revision (05) -- original was overloaded
- Codex review: GRID-05 split -- page persistence moved to PAGE-09 (pages don't exist until Phase 08)
- Codex review: Added GRID-08 (resolution portability), EDIT-08/09/10 (EVIOCGRAB exit, atomic writes, no-space feedback), PAGE-06/07/08/09 (page management, lazy instantiation, page persistence)
- Codex review: REV-01 scoped to "shipped v0.5.3 widgets" not future plugins
- Codex review: Resolve MouseArea+Drag vs DragHandler before Phase 07 implementation (Qt 6.4 compat)
- Codex review: Do not ship dense grid without Pi touch validation
- 04-01: Grid density constants = 150px cellW, 125px cellH (produces 6x4 for 1024x600, 5x3 for 800x480)
- 04-01: 1280x720 maps to 8x4 grid (not 7x4 as research estimated)
- 04-01: Legacy WidgetPlacement/PageDescriptor kept temporarily -- Plan 02 removes them
- 04-01: WidgetPickerModel.filterForSize -> filterByAvailableSpace
- 04-02: Occupancy grid stores instanceId per cell (not bool) for collision resolution during clamping
- 04-02: YAML schema v2 uses snake_case keys (instance_id, col_span) for consistency
- 04-02: QML WidgetPlacementModel context property removed -- Phase 05 handles QML migration
- 04-02: Phase 06 stubs registered with empty qmlComponent, excluded from picker
- 05-01: Inline delegate in HomeMenu (not separate GridCell.qml) -- simple enough, avoids SKIP_CACHEGEN
- 05-01: Blank canvas on fresh install per user decision -- GRID-06 deferred to post Phase 06/07
- 05-02: Clock date format "MMMM d" (no year) -- irrelevant on car display
- 05-02: AAStatusWidget ColumnLayout for both tiers (icon size changes, not layout type)
- 05-02: NowPlaying compact strip omits artist entirely -- too tight at 2x1

### Pending Todos

None yet.

### Blockers/Concerns

- Nav turn events are logged but not wired to EventBus -- must fix before nav widget (Phase 06)
- AA media metadata exists in orchestrator but is discarded -- must surface via MediaDataBridge (Phase 06)
- New widget QML files MUST set QT_QML_SKIP_CACHEGEN (known gotcha from v0.5.2)
- Use ghost rectangle for resize preview in edit mode -- animating width/height is janky on Pi
- Grid density formula needs real Pi touch validation before shipping

## Session Continuity

Last session: 2026-03-12T19:24:39Z
Stopped at: Completed 05-02-PLAN.md
