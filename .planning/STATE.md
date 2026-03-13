---
gsd_state_version: 1.0
milestone: v0.5
milestone_name: milestone
status: completed
stopped_at: Phase 08 context gathered
last_updated: "2026-03-13T01:49:40.951Z"
last_activity: 2026-03-13 - Drag-to-reposition and drag-to-resize with Pi touchscreen verification
progress:
  total_phases: 5
  completed_phases: 2
  total_plans: 6
  completed_plans: 6
  percent: 73
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-12)

**Core value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.
**Current focus:** v0.5.3 Widget Grid & Content Widgets -- Phase 06 complete, Phase 07 next

## Current Position

Phase: 07 of 8 (Edit Mode) — Plan 3 of 3 complete (Phase 07 DONE)
Status: Phase 07 complete. Phase 08 (Multi-Page) next.
Last activity: 2026-03-13 - Drag-to-reposition and drag-to-resize with Pi touchscreen verification

Progress: [########--] 73%

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
- 06-03: AA 16.2 dropped NavigationTurnEvent (0x8004) -- bridge uses NavigationNotification (0x8006) + NavigationNextTurnDistanceEvent (0x8007) for modern phones
- 06-03: AA Distance.displayUnit: 0=unknown, 1=m, 2/3=km, 4/5=mi, 6=ft, 7=yd (P1 variants 3,5 = one decimal place)
- 06-03: Phone's display_text preferred over computed distance -- always correct for locale
- [Phase 07]: findFirstAvailableCell uses row-major scan for top-left placement bias
- [Phase 07]: WidgetPickerModel extended with defaultCols/defaultRows for auto-placement sizing
- [Phase 07]: Edit mode picker uses auto-place; legacy targeted placement preserved for context menu Change Widget
- [Phase 07]: Drag overlay MouseArea (z:25) for consistent drag behavior across interactive and static widgets
- [Phase 07]: Legacy context menu overlay removed -- replaced by edit mode interactions
- [Phase 07]: All widgets require long-press to select before dragging (prevents accidental drags)

### Pending Todos

None yet.

### Roadmap Evolution

- Phase 05.1 inserted after Phase 05: Fix AA wireless connection failure (URGENT) — resolved

### Blockers/Concerns

- Use ghost rectangle for resize preview in edit mode -- animating width/height is janky on Pi
- Grid density formula needs real Pi touch validation before shipping
- Resolve MouseArea+Drag vs DragHandler before Phase 07 implementation (Qt 6.4 compat)

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 1 | Fix AA wireless connection failure | 2026-03-12 | config-only | [1-fix-the-aa-wireless-connection-failure](./quick/1-fix-the-aa-wireless-connection-failure/) |

## Session Continuity

Last session: 2026-03-13T01:49:40.947Z
Stopped at: Phase 08 context gathered

### Session Handoff (2026-03-12)

**Phase 06 completed:**
- Plans 06-01 through 06-03 all complete and Pi-verified
- Nav widget showing correct distance with miles (AA 16.2 modern message path)
- Now Playing widget with unified AA+BT metadata, source indicator, controls
- Old BT Now Playing widget removed from picker
- Long-press widget picker working on touchscreen

**Bug fixes during verification:**
- NavigationDataBridge wired to modern AA 16.2 messages (0x8006 + 0x8007)
- Distance unit mapping corrected from AA APK source analysis (community contribution)

**Open items carried forward:**
- YamlConfig default port mismatch: `YamlConfig.cpp` defaults tcp_port to 5277, `Configuration.hpp` defaults to 5288. Should align — 5288 is what works.
- NavigationTurnLabel UTF-8 parse errors spamming logs (proto field may need `bytes` type)

**Next:** Phase 07 — Edit Mode (long-press to enter, drag-to-reposition, drag-to-resize, add/remove widgets)
