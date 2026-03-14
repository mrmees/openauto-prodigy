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

- [ ] **Phase 09: Widget Descriptor & Grid Foundation** - Enrich WidgetDescriptor with manifest metadata; replace fixed-pixel grid math with DPI-based physical sizing; add YAML grid migration infrastructure
- [ ] **Phase 10: Launcher Widget & Dock Removal** - Create LauncherWidget as navigation replacement; remove LauncherDock and LauncherModel after verification
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
- [ ] 09-01-PLAN.md -- Widget descriptor metadata (category/description) + category-grouped picker
- [ ] 09-02-PLAN.md -- DPI-based cell sizing (cellSide) + dock height removal + QML grid frame
- [ ] 09-03-PLAN.md -- YAML grid dimension persistence + proportional remap algorithm

### Phase 10: Launcher Widget & Dock Removal
**Goal**: Users navigate to all views (AA, BT Audio, Phone, Settings) via a home screen widget instead of a fixed bottom bar
**Depends on**: Phase 09 (grid foundation must be stable; dock height deduction already removed)
**Requirements**: NAV-01, NAV-02, NAV-03
**Success Criteria** (what must be TRUE):
  1. LauncherWidget provides quick-launch tiles for all plugin views and settings, rendered as a standard grid widget
  2. LauncherDock bottom bar is gone from the shell -- vertical space reclaimed for widgets
  3. Every view reachable from the dock is reachable without it (QA gate: reach every view without SSH on Pi)
  4. LauncherModel is deleted -- LauncherWidget reads PluginModel directly as single source of truth
**Plans**: TBD

Plans:
- [ ] 10-01: TBD
- [ ] 10-02: TBD

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
Phases execute in numeric order: 09 -> 10 -> 11 -> 12

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 09. Widget Descriptor & Grid Foundation | 2/3 | In Progress|  |
| 10. Launcher Widget & Dock Removal | 0/2 | Not started | - |
| 11. Widget Framework Polish | 0/2 | Not started | - |
| 12. Documentation | 0/1 | Not started | - |

---
*Last updated: 2026-03-14 -- Phase 09 plans created*
