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

## v0.6.5 Widget Refinement (In Progress)

**Milestone Goal:** Polish all existing widgets via live preview iteration, split date into its own widget, simplify clock to time-only, and verify on Pi hardware.

## Phases

- [x] **Phase 22: Date Widget & Clock Cleanup** - Create standalone date widget, strip date from clock widget (completed 2026-03-21)
- [x] **Phase 23: Widget Visual Refinement** - Live preview review of all widgets across both target displays (completed 2026-03-21)
- [x] **Phase 24: Hardware Verification** - Deploy and verify all widgets on Pi hardware (completed 2026-03-21)

## Phase Details

### Phase 22: Date Widget & Clock Cleanup
**Goal**: Users have a dedicated date widget and a simplified time-only clock widget
**Depends on**: Nothing (first phase of milestone)
**Requirements**: DT-01, DT-02, DT-03, CL-01, CL-02
**Success Criteria** (what must be TRUE):
  1. A standalone date widget appears in the widget picker with appropriate icon and metadata
  2. The date widget shows day-of-week and date, with compact layout at 1x1 and more detail at larger spans
  3. The clock widget (digital, analog, minimal styles) shows only time at every span -- no date or day-of-week
  4. Clock widget layouts fill available space with time display -- no empty gaps where date used to be
**Plans**: 2 plans

Plans:
- [ ] 22-01-PLAN.md — Create date widget with breakpoints, config, and picker registration
- [ ] 22-02-PLAN.md — Strip date/day from clock widget, simplify to time-only

### Phase 23: Widget Visual Refinement
**Goal**: Every widget looks correct and legible at every valid span on both target displays
**Depends on**: Phase 22
**Requirements**: WR-01, WR-02, WR-03, WR-04
**Success Criteria** (what must be TRUE):
  1. Every widget renders without clipping, overflow, or misalignment at all valid span combinations on 1024x600
  2. Every widget renders without clipping, overflow, or misalignment at all valid span combinations on 800x480
  3. Widget text and icons are legible at 1x1 minimum span on both displays
  4. All user-identified visual issues from the preview session are resolved
**Plans**: TBD

Plans:
- [ ] 23-01: TBD

### Phase 24: Hardware Verification
**Goal**: All widget changes verified working on real Pi hardware
**Depends on**: Phase 23
**Requirements**: HW-01
**Success Criteria** (what must be TRUE):
  1. All widgets display correctly on Pi 4 with DFRobot 1024x600 touchscreen
  2. Widget touch interactions (config sheets, edit mode drag/resize, tap actions) still work after QML changes
**Plans**: TBD

Plans:
- [ ] 24-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 22 -> 23 -> 24

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 22. Date Widget & Clock Cleanup | 2/2 | Complete    | 2026-03-21 |
| 23. Widget Visual Refinement | 0/1 | Complete    | 2026-03-21 |
| 24. Hardware Verification | 0/1 | Complete    | 2026-03-21 |
