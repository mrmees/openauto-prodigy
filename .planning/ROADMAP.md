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

<details>
<summary>v0.5.3 Widget Grid & Content Widgets (Phases 04-08) - SHIPPED 2026-03-13</summary>

See .planning/milestones/v0.5.3-ROADMAP.md for archived details.

</details>

---

## v0.6.1 Widget Framework & Layout Refinement (In Progress)

**Milestone Goal:** Formalize widget framework conventions, replace the quick-launch dock with a widget-based launcher, refine DPI-based grid layout, and document all plugin/widget contracts for third-party development.

## Phases

**Phase Numbering:**
- Integer phases (09, 10, 11, 12): Planned milestone work
- Decimal phases (09.1, 10.1): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 09: Widget Descriptor & Grid Foundation** - Enrich WidgetDescriptor with manifest metadata; replace fixed-pixel grid math with DPI-based physical sizing; add YAML grid migration infrastructure (completed 2026-03-14)
- [x] **Phase 10: Launcher Widget & Dock Removal** - Create LauncherWidget as navigation replacement; remove LauncherDock and LauncherModel after verification (completed 2026-03-14)
- [ ] **Phase 11: Widget Framework Polish** - Enforce size constraints from descriptors; add widget lifecycle signals; migrate pixel breakpoints to span-based thresholds
- [ ] **Phase 12: Documentation** - Developer guide covering widget manifest, registration, lifecycle, and sizing conventions; architecture decision records

## Phase Details

### Phase 09: Widget Descriptor & Grid Foundation
**Goal**: Widget metadata is fully specified and grid layout produces physically consistent cell sizes across all target displays
**Depends on**: Nothing (first phase of v0.6.1)
**Requirements**: WF-01, GL-01, GL-02, GL-03
**Success Criteria** (what must be TRUE):
  1. WidgetDescriptor exposes category, description, and icon fields, and the widget picker displays them
  2. Grid cell dimensions are derived from physical mm targets via DPI -- visually consistent touch targets on 800x480, 1024x600, and 1080p displays
  3. Grid math does not deduct a hardcoded dock height -- full vertical space is available to the grid
  4. Changing grid dimension constants triggers a YAML migration that preserves existing widget placements (no "widgets vanished after update")
**Plans**: 3 plans

Plans:
- [x] 09-01-PLAN.md -- Widget descriptor metadata (category/description) + category-grouped picker
- [x] 09-02-PLAN.md -- DPI-based cell sizing (cellSide) + dock height removal + QML grid frame
- [x] 09-03-PLAN.md -- YAML grid dimension persistence + proportional remap algorithm

### Phase 10: Launcher Widget & Dock Removal
**Goal**: Singleton system widgets on a reserved utility page replace the fixed LauncherDock bottom bar for navigating to Settings and Android Auto
**Depends on**: Phase 09 (grid foundation must be stable; dock height deduction already removed)
**Requirements**: NAV-01, NAV-02, NAV-03
**Success Criteria** (what must be TRUE):
  1. Settings and AA launcher singleton widgets are seeded on a protected reserved page (last page, undeletable)
  2. LauncherDock bottom bar is gone from the shell -- vertical space reclaimed for widgets
  3. Every view reachable from the dock is reachable without it (Settings via widget + navbar holds, AA via widget)
  4. LauncherModel and LauncherMenu are deleted -- singleton widgets use PluginModel directly
**Plans**: 2 plans

Plans:
- [x] 10-01-PLAN.md -- Singleton widget infrastructure + Settings/AA widgets + reserved page logic + default seeding
- [x] 10-02-PLAN.md -- Delete LauncherDock, LauncherModel, LauncherMenu + reference cleanup

### Phase 10.1: Adjust grid spacing and page indicator location (INSERTED)

**Goal:** Grid recovers wasted gutter space via auto-snap threshold, page indicator dots move to navbar flanking the clock, and card padding tightens for a more cohesive layout
**Requirements**: GRID-SNAP, CARD-PADDING, PAGE-DOTS-NAVBAR, PAGE-INDICATOR-REMOVAL
**Depends on:** Phase 10
**Success Criteria** (what must be TRUE):
  1. Grid auto-snap threshold (60%) adds rows/cols when waste exceeds threshold -- 1024x600 goes from 7x3 to 7x4
  2. Page dots flank the clock in the navbar; clock serves as active page indicator
  3. Dots hidden when not on home screen, when plugin is active, or when only 1 page exists
  4. PageIndicator deleted from HomeMenu -- vertical space reclaimed for the grid
  5. Card padding tightened for less visual gap between widgets
**Plans**: 2 plans

Plans:
- [ ] 10.1-01-PLAN.md -- Auto-snap grid threshold + tighter card padding
- [ ] 10.1-02-PLAN.md -- Page dots in navbar + PageIndicator removal from HomeMenu

### Phase 11: Widget Framework Polish
**Goal**: Widgets behave predictably under resize, report lifecycle transitions, and scale their content based on grid span rather than absolute pixels
**Depends on**: Phase 09 (descriptor size constraints and grid dimensions must be finalized)
**Requirements**: WF-02, WF-03, WF-04
**Success Criteria** (what must be TRUE):
  1. Resizing a widget via drag is clamped to its declared min/max size -- WidgetGridModel enforces WidgetDescriptor constraints as single source of truth
  2. WidgetHost emits lifecycle signals (created, resized, destroying) that widget QML can observe and react to
  3. All 4 existing widgets (Clock, AA Status, Now Playing, Navigation) use grid-span or UiMetrics-based thresholds instead of hardcoded pixel breakpoints
**Plans**: TBD

Plans:
- [ ] 11-01: TBD
- [ ] 11-02: TBD

### Phase 12: Documentation
**Goal**: A third-party developer can create a widget plugin by reading the documentation alone
**Depends on**: Phase 11 (documents the finalized contracts, not a moving target)
**Requirements**: DOC-01, DOC-02
**Success Criteria** (what must be TRUE):
  1. Plugin-widget developer guide covers: manifest spec (WidgetDescriptor fields), registration API, QML lifecycle hooks, sizing conventions, category taxonomy, and CMake setup (QT_QML_SKIP_CACHEGEN requirement)
  2. Architecture decision records document the key design choices made during v0.6-v0.6.1 for future contributors
**Plans**: TBD

Plans:
- [ ] 12-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 09 -> 10 -> 10.1 -> 11 -> 12

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 09. Widget Descriptor & Grid Foundation | 3/3 | Complete    | 2026-03-14 |
| 10. Launcher Widget & Dock Removal | 2/2 | Complete    | 2026-03-14 |
| 10.1. Grid Spacing & Page Indicators | 2/2 | Complete    | 2026-03-14 |
| 11. Widget Framework Polish | 0/2 | Not started | - |
| 12. Documentation | 0/1 | Not started | - |

---
*Last updated: 2026-03-14 -- Phase 10.1 planned*
