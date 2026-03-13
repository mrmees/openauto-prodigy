# Roadmap: OpenAuto Prodigy

## Milestones

<details>
<summary>v0.4 Logging & Theming (Phases 1-2) - SHIPPED 2026-03-01</summary>

See .planning/milestones/v0.4/ for archived details.

</details>

<details>
<summary>v0.4.1 Audio Equalizer (Phases 1-3) - SHIPPED 2026-03-02</summary>

See .planning/milestones/v0.4.1/ for archived details.

</details>

<details>
<summary>v0.4.2 Service Hardening (Phases 1-3) - SHIPPED 2026-03-02</summary>

See .planning/milestones/v0.4.2/ for archived details.

</details>

<details>
<summary>v0.4.3 Interface Polish & Settings Reorganization (Phases 1-4) - SHIPPED 2026-03-03</summary>

See .planning/milestones/v0.4.3/ for archived details.

</details>

<details>
<summary>v0.4.4 Scalable UI (Phases 1-5) - SHIPPED 2026-03-03</summary>

See .planning/milestones/v0.4.4/ for archived details.

</details>

<details>
<summary>v0.4.5 Navbar Rework (Phases 1-4) - SHIPPED 2026-03-05</summary>

See .planning/milestones/v0.4.5/ for archived details.

</details>

<details>
<summary>v0.5.0 Protocol Compliance (Phases 1-4) - SHIPPED 2026-03-08</summary>

See .planning/milestones/v0.5.0-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.1 DPI Sizing & UI Polish (Phases 1-4 + insertions) - SHIPPED 2026-03-10</summary>

See .planning/milestones/v0.5.1-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.2 Widget System & UI Polish (Phases 1-3) - SHIPPED 2026-03-11</summary>

See .planning/milestones/v0.5.2-ROADMAP.md for archived details.

</details>

---

## v0.5.3 Widget Grid & Content Widgets

Replace the fixed 3-pane home screen with an Android-style freeform grid supporting cell-based widget placement, drag-to-reposition, drag-to-resize, multi-page swipe navigation, and two new content widgets (AA navigation turn-by-turn and unified now playing). The grid data model is a clean replacement of the pane-based WidgetPlacementModel — not an extension.

**Natural cut line:** Phase 08 (Multi-Page) can be deferred if schedule gets tight. Phases 04-07 deliver a complete single-page grid with content widgets and full edit mode.

## Phases

**Phase Numbering:**
- Integer phases (4-8): Planned milestone work (continuing from v0.5.2 phase 03)
- Decimal phases (e.g. 5.1): Urgent insertions (marked with INSERTED)

- [x] **Phase 04: Grid Data Model & Persistence** - Replace pane-based model with cell-based grid model, YAML persistence, config migration (completed 2026-03-12)
- [x] **Phase 05: Static Grid Rendering & Widget Revision** - Render widgets on grid in QML, revise existing widgets for variable grid sizing (completed 2026-03-12)
- [x] **Phase 06: Content Widgets** - Wire AA protocol data to QML, build navigation turn-by-turn and unified now playing widgets (completed 2026-03-12)
- [x] **Phase 07: Edit Mode** - Long-press to enter edit state, drag-to-reposition, drag-to-resize, add/remove widgets, safety exits (completed 2026-03-13)
- [x] **Phase 08: Multi-Page** - SwipeView page navigation, page indicator, page management, lazy instantiation, edit mode swipe disable (completed 2026-03-13)

## Phase Details

### Phase 04: Grid Data Model & Persistence
**Goal**: Cell-based grid model replaces pane-based WidgetPlacementModel with occupancy tracking, YAML persistence, and migration from v0.5.2 config
**Depends on**: Nothing (first phase of v0.5.3)
**Requirements**: GRID-01, GRID-02, GRID-04, GRID-05, GRID-07, GRID-08
**Success Criteria** (what must be TRUE):
  1. WidgetGridModel stores widget placements as (col, row, colSpan, rowSpan) with occupancy tracking and collision detection
  2. Grid dimensions (columns/rows) are derived from display size via UiMetrics — different values for 800x480 vs 1024x600
  3. WidgetDescriptor uses minCols/minRows/maxCols/maxRows instead of Main/Sub size enum
  4. Grid state round-trips through YAML config (save on commit, restore on launch)
  5. v0.5.2 pane-based config auto-migrates to grid coordinates on first launch
  6. Layouts clamp/reflow gracefully when display resolution changes
**Plans**: 2 plans

Plans:
- [x] 04-01-PLAN.md — Grid types, DisplayInfo grid density, test updates
- [x] 04-02-PLAN.md — WidgetGridModel, YAML persistence, app wiring

### Phase 05: Static Grid Rendering & Widget Revision
**Goal**: Home screen renders widgets positioned on the grid with correct sizing, and existing widgets adapt layout to variable grid cell dimensions
**Depends on**: Phase 04
**Requirements**: GRID-03, GRID-06, REV-01, REV-02, REV-03
**Success Criteria** (what must be TRUE):
  1. HomeMenu.qml renders widgets at grid-computed pixel positions using Repeater + manual positioning (not GridView/GridLayout)
  2. Widgets visually snap to grid cells matching their col/row/span from the model
  3. Clock widget shows time-only at 1x1, time+date at 2x1, full at 2x2+
  4. AA Status widget shows icon-only at 1x1, icon+text at 2x1+
  5. Fresh install displays blank canvas (default layout deferred to post Phase 06/07)
**Plans**: 2 plans

Plans:
- [ ] 05-01-PLAN.md — HomeMenu grid Repeater rewrite, WidgetHost cleanup, blank canvas fresh install
- [ ] 05-02-PLAN.md — Widget pixel breakpoint revision (Clock, AA Status, Now Playing)

### Phase 06: Content Widgets
**Goal**: Users see live AA navigation and media information on the home screen without switching to fullscreen AA
**Depends on**: Phase 05
**Requirements**: NAV-01, NAV-02, NAV-03, NAV-04, NAV-05, MEDIA-01, MEDIA-02, MEDIA-03, MEDIA-04, MEDIA-05, MEDIA-06
**Success Criteria** (what must be TRUE):
  1. Navigation widget shows the next turn maneuver icon, road name, and distance when AA navigation is active
  2. Maneuver icon renders phone-provided PNG via QQuickImageProvider; falls back to Material icon when no PNG available
  3. Navigation widget shows muted "No active navigation" state when nav inactive; tapping it opens AA fullscreen
  4. Unified Now Playing widget shows track title, artist, and playback controls — sourced from AA media when connected, BT A2DP when not
  5. Now Playing widget displays source indicator ("via Android Auto" / "via Bluetooth") and routes controls to correct source
**Plans**: 3 plans

Plans:
- [x] 06-01-PLAN.md — NavigationDataBridge, ManeuverIconProvider, orchestrator accessors, tests
- [x] 06-02-PLAN.md — MediaDataBridge with source priority and control delegation, tests
- [x] 06-03-PLAN.md — QML widgets (NavigationWidget, unified NowPlayingWidget), main.cpp wiring, BT widget removal

### Phase 07: Edit Mode
**Goal**: Users can customize their home screen layout by repositioning, resizing, adding, and removing widgets through a touch-driven edit mode
**Depends on**: Phase 05
**Requirements**: EDIT-01, EDIT-02, EDIT-03, EDIT-04, EDIT-05, EDIT-06, EDIT-07, EDIT-08, EDIT-09, EDIT-10
**Success Criteria** (what must be TRUE):
  1. Long-pressing on empty grid space or widget background enters edit mode with visible accent borders and resize handles on all widgets
  2. User can drag a widget to a new grid position (snaps to cells on drop, rejects occupied positions)
  3. User can drag corner handles to resize a widget within its declared min/max span constraints
  4. "+" FAB opens widget catalog to place new widgets; "X" badge removes existing widgets; "no space" gets clear feedback
  5. Edit mode exits automatically after 10s inactivity, on tap outside, or when AA fullscreen activates (EVIOCGRAB). Layout writes are atomic (on commit, not during drag).
**Plans**: 3 plans

Plans:
- [x] 07-01-PLAN.md — C++ backend extensions (constraint roles, auto-place) + Toast.qml
- [x] 07-02-PLAN.md — Edit mode visual state, entry/exit, FAB add, X remove
- [x] 07-03-PLAN.md — Drag-to-reposition + drag-to-resize interactions

### Phase 08: Multi-Page
**Goal**: Users can organize widgets across multiple swipeable home screen pages
**Depends on**: Phase 05, Phase 07
**Requirements**: PAGE-01, PAGE-02, PAGE-03, PAGE-04, PAGE-05, PAGE-06, PAGE-07, PAGE-08, PAGE-09
**Success Criteria** (what must be TRUE):
  1. User can swipe horizontally between widget pages with page indicator dots showing position and count
  2. Launcher dock remains visible and fixed across all pages
  3. Page swipe gesture is disabled during edit mode; page dots remain tappable for navigation
  4. Pages can be explicitly created and removed; empty pages are auto-cleaned
  5. No maximum page limit; page assignments persist across restarts; non-visible pages lazily instantiated
**Plans**: 2 plans

Plans:
- [x] 08-01-PLAN.md — Page-aware WidgetGridModel, schema v3 YAML persistence, page-scoped operations
- [x] 08-02-PLAN.md — SwipeView multi-page grid, PageIndicator, page management FABs, Pi verification

## Progress

**Execution Order:**
Phases execute in numeric order: 04 -> 05 -> 06 + 07 (parallel-capable) -> 08
(Phases 06 and 07 both depend on 05 only; Phase 08 depends on 05 and 07)

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 04. Grid Data Model & Persistence | 2/2 | Complete    | 2026-03-12 |
| 05. Static Grid Rendering & Widget Revision | 2/2 | Complete    | 2026-03-12 |
| 06. Content Widgets | 3/3 | Complete    | 2026-03-12 |
| 07. Edit Mode | 3/3 | Complete    | 2026-03-13 |
| 08. Multi-Page | 2/2 | Complete   | 2026-03-13 |

## Key Research Flags

- **Phase 04**: Grid density formula needs Pi hardware tuning; config migration edge cases with different column counts across resolutions
- **Phase 05**: Use `QT_QML_SKIP_CACHEGEN` on all new widget QML files; verify on Pi with real touch
- **Phase 06**: Nav turn events must be wired to EventBus (currently logged only); AA media metadata currently discarded in orchestrator; resolve dual BT/AA metadata flickering with priority logic
- **Phase 07**: MouseArea+drag.target resolved (not DragHandler); ghost rectangle for resize preview (not animate width/height — janky on Pi); EVIOCGRAB/edit-mode interaction explicit via exitEditMode()
- **Phase 08**: SwipeView.interactive must be disabled during edit mode; lazy page instantiation critical for Pi 4 memory; no max page limit per user decision (overrides PAGE-05)

---
*Last updated: 2026-03-13 -- v0.5.3 milestone complete (all 5 phases, 13 plans)*
