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

## v0.6.4 Widget Work (In Progress)

**Milestone Goal:** Expand the widget library with 6 new widgets and extend the widget contract to support per-instance configuration.

### Phases

- [x] **Phase 19: Widget Instance Config** - Per-placement config storage, config UI, and persistence (completed 2026-03-17)
- [x] **Phase 20: Simple Widgets** - Theme cycle, battery, companion status, and AA focus toggle (completed 2026-03-17)
- [ ] **Phase 21: Clock Styles & Weather** - Clock visual variants and weather widget with API integration

### Phase Details

#### Phase 19: Widget Instance Config
**Goal**: Any widget can declare configuration options and users can set them per-placement
**Depends on**: Nothing (first phase in milestone)
**Requirements**: WC-01, WC-02, WC-03
**Success Criteria** (what must be TRUE):
  1. A widget placement stores key-value config that is independent from other placements of the same widget
  2. User can open a config sheet from a gear icon in edit mode and change settings
  3. Widget config survives app restart and grid remap without data loss
**Plans**: 2 plans

Plans:
- [ ] 19-01: Per-instance config storage and YAML persistence
- [ ] 19-02: Widget config sheet UI (gear in edit mode + bottom sheet)

#### Phase 20: Simple Widgets
**Goal**: Four new utility widgets are available in the widget picker and functional on the home screen
**Depends on**: Nothing (these widgets do not require per-instance config)
**Requirements**: TC-01, TC-02, BW-01, CS-01, CS-02, AF-01, AF-02, AF-03
**Success Criteria** (what must be TRUE):
  1. Tapping the theme cycle widget advances to the next theme and the UI updates immediately
  2. Battery widget displays the phone's current battery level and charging state from companion data
  3. Companion status widget shows connected/disconnected at 1x1 and GPS/battery/proxy detail at larger sizes
  4. AA focus toggle widget pauses and resumes AA projection on tap, reflecting current focus state
**Plans**: 2 plans

Plans:
- [ ] 20-01-PLAN.md — C++ API extensions + theme cycle + battery widgets
- [ ] 20-02-PLAN.md — Companion status + AA focus toggle widgets

#### Phase 21: Clock Styles & Weather
**Goal**: Clock widget supports multiple visual styles and a new weather widget displays live conditions via API
**Depends on**: Phase 19 (both use per-instance config for style/settings selection)
**Requirements**: CK-01, CK-02, WX-01, WX-02, WX-03, WX-04, WX-05
**Success Criteria** (what must be TRUE):
  1. Clock widget offers at least 3 distinct visual styles (digital, analog, minimal) selectable per instance via config UI
  2. Weather widget displays current temperature, condition icon, and location name using companion GPS coordinates
  3. Weather widget shows active weather alerts when present
  4. Weather data refreshes automatically on a configurable interval and the API key is set in YAML config
**Plans**: TBD

Plans:
- [ ] 21-01: Clock style variants with per-instance selection
- [ ] 21-02: WeatherService and weather widget

## Progress

**Execution Order:**
Phases execute in numeric order: 19 -> 20 -> 21

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 19. Widget Instance Config | 2/2 | Complete    | 2026-03-17 |
| 20. Simple Widgets | 2/2 | Complete   | 2026-03-17 |
| 21. Clock Styles & Weather | 0/2 | Not started | - |
