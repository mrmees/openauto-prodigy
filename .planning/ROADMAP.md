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

---

## v0.5.1 DPI Sizing & UI Polish

**Milestone Goal:** Make UI sizing physically meaningful via DPI-aware scaling with user control, and polish visual presentation (clock, theme colors, element depth).

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [ ] **Phase 1: DPI Foundation** - Installer EDID probe, DPI-based UiMetrics, config persistence with settings UI
- [ ] **Phase 2: Clock & Scale Control** - Clock readability and 24h toggle, user-facing scale stepper
- [ ] **Phase 3: Theme Color System** - Full semantic color palette aligned with Material Design conventions
- [ ] **Phase 4: Visual Depth** - 3D depth effects on buttons and navbar

## Phase Details

### Phase 1: DPI Foundation
**Goal**: UI sizing is derived from the physical screen size, not just pixel resolution
**Depends on**: Nothing (first phase)
**Requirements**: DPI-01, DPI-02, DPI-03, DPI-04
**Success Criteria** (what must be TRUE):
  1. Running the installer on a Pi with EDID-capable display shows detected screen size and lets the user confirm or override it
  2. A fresh install with no EDID and user skipping entry defaults to 7" and produces correctly scaled UI
  3. UiMetrics-driven element sizes change when the configured screen size changes (e.g., 7" vs 10" produces visibly different control sizes at the same resolution)
  4. User can view and change the physical screen size value in Display settings, and the change persists across restarts
**Plans**: TBD

### Phase 2: Clock & Scale Control
**Goal**: Clock is readable at glance distance, and user can fine-tune overall UI scale
**Depends on**: Phase 1
**Requirements**: CLK-01, CLK-02, CLK-03, DPI-05
**Success Criteria** (what must be TRUE):
  1. Clock text in the navbar is noticeably larger than current and readable from arm's length in a car
  2. Clock shows bare time without AM/PM suffix in 12h mode
  3. User can toggle between 12h and 24h format in settings, and the navbar clock updates immediately
  4. User can adjust UI scale via stepper in Display settings (increments of 0.1), and all UI elements resize live or on confirm
**Plans**: TBD

### Phase 3: Theme Color System
**Goal**: All UI elements draw from a complete, consistent semantic color palette
**Depends on**: Phase 1
**Requirements**: THM-01, THM-02, THM-03
**Success Criteria** (what must be TRUE):
  1. ThemeService exposes a full set of named semantic color roles (surface, primary, secondary, accent, error, on-surface, on-primary, etc.) as Q_PROPERTYs
  2. Switching between day and night themes produces a coherent, non-clashing color scheme across all screens (launcher, settings, AA overlay, BT audio, phone)
  3. No hardcoded color hex values remain in any QML file -- all colors reference ThemeService properties
**Plans**: TBD

### Phase 4: Visual Depth
**Goal**: Buttons and navbar have subtle physical depth that makes the UI feel polished
**Depends on**: Phase 3
**Requirements**: STY-01, STY-02
**Success Criteria** (what must be TRUE):
  1. Buttons across the app (settings tiles, launcher tiles, dialog actions) have a visible but subtle 3D effect (shadow, gradient, or bevel) in their resting state
  2. Button press state visibly changes the depth effect (pressed-in appearance) in addition to existing scale+opacity feedback
  3. Navbar has a depth treatment (drop shadow, gradient border, or elevation) that visually separates it from the content area
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. DPI Foundation | 0/? | Not started | - |
| 2. Clock & Scale Control | 0/? | Not started | - |
| 3. Theme Color System | 0/? | Not started | - |
| 4. Visual Depth | 0/? | Not started | - |

---
*Last updated: 2026-03-08 -- v0.5.1 roadmap created*
