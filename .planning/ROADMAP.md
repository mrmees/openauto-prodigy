# Roadmap: OpenAuto Prodigy

## Milestones

<details>
<summary>v0.4 Logging & Theming (Phases 1-2) - SHIPPED 2015-03-01</summary>

See .planning/milestones/v0.4/ for archived details.

</details>

<details>
<summary>v0.4.1 Audio Equalizer (Phases 1-3) - SHIPPED 2015-03-02</summary>

See .planning/milestones/v0.4.1/ for archived details.

</details>

<details>
<summary>v0.4.2 Service Hardening (Phases 1-3) - SHIPPED 2015-03-02</summary>

See .planning/milestones/v0.4.2/ for archived details.

</details>

<details>
<summary>v0.4.3 Interface Polish & Settings Reorganization (Phases 1-4) - SHIPPED 2015-03-03</summary>

See .planning/milestones/v0.4.3/ for archived details.

</details>

<details>
<summary>v0.4.4 Scalable UI (Phases 1-5) - SHIPPED 2015-03-03</summary>

See .planning/milestones/v0.4.4/ for archived details.

</details>

<details>
<summary>v0.4.5 Navbar Rework (Phases 1-4) - SHIPPED 2015-03-05</summary>

See .planning/milestones/v0.4.5/ for archived details.

</details>

<details>
<summary>v0.5.0 Protocol Compliance (Phases 1-4) - SHIPPED 2015-03-08</summary>

See .planning/milestones/v0.5.0-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.1 DPI Sizing & UI Polish (Phases 1-4 + insertions) - SHIPPED 2015-03-10</summary>

See .planning/milestones/v0.5.1-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.2 Widget System & UI Polish (Phases 1-3) - SHIPPED 2015-03-11</summary>

See .planning/milestones/v0.5.2-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.3 Widget Grid & Content Widgets (Phases 04-08) - SHIPPED 2015-03-13</summary>

See .planning/milestones/v0.5.3-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.1 Widget Framework & Layout Refinement (Phases 09-12) - SHIPPED 2015-03-15</summary>

See .planning/milestones/v0.6-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.2 Theme Expression & Wallpaper Scaling (Phases 13-14.1) - SHIPPED 2026-03-16</summary>

See .planning/milestones/v0.6.2-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.3 Proxy Routing Fix (Phases 15-18) - SHIPPED 2026-03-16</summary>

- [x] Phase 15: Privilege Model & IPC Lockdown (2/2 plans) -- completed 2026-03-16
- [x] Phase 16: Routing Correctness & Idempotency (4/4 plans) -- completed 2026-03-16
- [x] Phase 17: Status Reporting Hardening (2/2 plans) -- completed 2026-03-16
- [x] Phase 18: Hardware Validation (1/1 plan) -- completed 2026-03-16

See .planning/milestones/v0.6.3-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.4 Widget Work (Phases 19-21) - SHIPPED 2026-03-17</summary>

- [x] Phase 19: Widget Instance Config (2/2 plans) -- completed 2026-03-17
- [x] Phase 20: Simple Widgets (2/2 plans) -- completed 2026-03-17
- [x] Phase 21: Clock Styles & Weather (2/2 plans) -- completed 2026-03-17

See .planning/milestones/v0.6.4-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.5 Widget Refinement (Phases 22-24) - SHIPPED 2026-03-21</summary>

- [x] Phase 22: Date Widget & Clock Cleanup (2/2 plans) -- completed 2026-03-21
- [x] Phase 23: Widget Visual Refinement (interactive review) -- completed 2026-03-21
- [x] Phase 24: Hardware Verification -- completed 2026-03-21

See .planning/milestones/v0.6.5-ROADMAP.md for archived details.

</details>

## v0.6.6 Homescreen Layout & Widget Settings Rework (In Progress)

**Milestone Goal:** Replace global edit mode with Android-style per-widget interactions -- long-press to lift/drag, navbar-hosted settings/delete, edge resize handles, bottom-sheet widget picker, and long-press-empty for page/widget management.

## Phases

- [ ] **Phase 25: Selection Model & Interaction Foundation** - Per-widget long-press select/drag/deselect replaces global edit mode; auto-deselect timeout replaces global inactivity timer
- [ ] **Phase 26: Navbar Transformation & Edge Resize** - Navbar morphs to settings/delete during widget selection; 4-edge resize handles with constraint enforcement; widget deletion with empty page auto-cleanup
- [ ] **Phase 27: Widget Picker & Page Management** - Bottom-sheet categorized picker with auto-placement; long-press empty space menu; FAB removal

## Phase Details

### Phase 25: Selection Model & Interaction Foundation
**Goal**: Users interact with individual widgets via long-press rather than entering a global edit mode
**Depends on**: Nothing (first phase of v0.6.6)
**Requirements**: SEL-01, SEL-02, SEL-03, SEL-04, CLN-01, CLN-04
**Success Criteria** (what must be TRUE):
  1. User can long-press a single widget to see it visually lift (scale + border feedback) without affecting other widgets
  2. User can drag a lifted widget to a new grid position with snap-to-cell behavior
  3. User can release a long-pressed widget without dragging to see a selected-state indicator, then tap empty space or clock-home to deselect
  4. Selection automatically clears after inactivity timeout (auto-deselect, replacing the old global edit mode timer)
  5. SwipeView is locked (non-interactive) while any widget is selected
  6. Long-press still works on interactive widgets (widgets with their own touch handling, e.g. Now Playing controls, AA Focus toggle)
  7. Tapping a different widget while one is selected only deselects — does NOT fire the tapped widget's action
  8. AA fullscreen activation force-deselects AND dismisses any open overlays (config sheet, picker)
  9. No global edit mode flag exists in UI or code
**Plans:** 1 plan
Plans:
- [ ] 25-01-PLAN.md — Selection model core: WidgetGridModel rename + HomeMenu.qml full refactor (editMode -> selectedInstanceId)

### Phase 26: Navbar Transformation & Edge Resize
**Goal**: Users manage widget settings, deletion, and sizing through automotive-sized controls instead of tiny overlays
**Depends on**: Phase 25
**Requirements**: NAV-01, NAV-02, NAV-03, NAV-04, NAV-05, RSZ-01, RSZ-02, RSZ-03, RSZ-04, CLN-03, PGM-04
**Gated substep**: Navbar transformation must be verified working (select widget -> navbar shows gear/trash -> tap each -> correct action) BEFORE resize handle implementation begins.
**Success Criteria** (what must be TRUE):
  1. When a widget is selected, volume control shows a settings gear and brightness control shows a trash icon; tapping gear opens the widget config sheet
  2. Tapping the trash icon removes the selected widget and reverts navbar to normal controls
  3. Navbar automatically reverts to volume/brightness on deselect, drag start, or auto-deselect timeout
  4. All tiny badge buttons removed (X delete, gear config, corner resize handle) -- replaced by navbar controls
  5. Selected widget shows drag handles on all 4 edges; dragging any edge resizes the widget in that direction
  6. Resize is clamped to widget descriptor min/max constraints with visual feedback at limits (e.g. handle color change or bounce)
  7. Resize is blocked when it would overlap another widget, with a distinct collision indicator (e.g. red flash or shake)
  8. Empty pages are automatically deleted when their last widget is removed via navbar trash
**Plans**: TBD

### Phase 27: Widget Picker & Page Management
**Goal**: Users can add widgets and manage pages through discoverable long-press interactions on empty space
**Depends on**: Phase 26
**Requirements**: PKR-01, PKR-02, PKR-03, PGM-01, PGM-02, PGM-03, CLN-02
**Success Criteria** (what must be TRUE):
  1. Long-pressing empty grid space shows a menu with "Add Widget" and "Add Page" options
  2. "Add Widget" opens a bottom sheet with categorized scrollable widget list; tapping a widget auto-places it at the first available cell
  3. Picker only shows widgets that fit the available grid space
  4. "Add Page" creates a new page and navigates to it
  5. All FABs removed (add widget, add page, delete page) -- replaced by long-press empty menu and picker
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 25 -> 26 -> 27

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 25. Selection Model & Interaction Foundation | 0/1 | Not started | - |
| 26. Navbar Transformation & Edge Resize | 0/TBD | Not started | - |
| 27. Widget Picker & Page Management | 0/TBD | Not started | - |
