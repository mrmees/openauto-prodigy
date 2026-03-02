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

## v0.4.3 Interface Polish & Settings Reorganization

### Overview

Transform the existing functional UI into an automotive-minimal interface. Restyle all controls first (so new pages inherit polish), restructure settings into 6 logical categories, then wire the EQ dual-access shortcut as the integration capstone. Purely QML-side work with two minor ThemeService C++ additions.

### Phases

- [ ] **Phase 1: Visual Foundation** - Restyle all controls with press feedback, icons, animations, and automotive-minimal aesthetic
- [ ] **Phase 2: Settings Restructuring** - Replace flat settings list with 6-category navigation hierarchy
- [ ] **Phase 3: EQ Dual-Access & Shell Polish** - Wire NavStrip EQ shortcut, restyle launcher/TopBar, modal dismiss, BT forget

### Phase Details

### Phase 1: Visual Foundation
**Goal**: Every control in the app looks and feels like an automotive interface — dark, high-contrast, with immediate press feedback
**Depends on**: Nothing (first phase)
**Requirements**: VIS-01, VIS-02, VIS-03, VIS-04, VIS-05, VIS-06, ICON-01, ICON-02
**Success Criteria** (what must be TRUE):
  1. Tapping any interactive control (tile, button, toggle, slider, list item) produces visible feedback within 80ms
  2. Settings page transitions animate smoothly (slide + fade) without dropping below 30fps on Pi 4
  3. Switching between day and night themes animates colors smoothly (no instant flash)
  4. All settings rows display a contextual icon to the left of the label
  5. MaterialIcon component renders variable-weight icons on Qt 6.8 and falls back gracefully on Qt 6.4
**Plans:** 3 plans

Plans:
- [x] 01-01-PLAN.md — ThemeService + UiMetrics + MaterialIcon foundation properties
- [x] 01-02-PLAN.md — Control restyling: press feedback, icons, automotive-minimal aesthetic
- [x] 01-03-PLAN.md — StackView transitions + day/night theme color animation

### Phase 2: Settings Restructuring
**Goal**: Users navigate settings through 6 intuitive category tiles that match their mental model (AA, Display, Audio, Connectivity, Companion, System)
**Depends on**: Phase 1
**Requirements**: SET-01, SET-02, SET-03, SET-04, SET-05, SET-06, SET-07, SET-08, SET-09, UX-03
**Success Criteria** (what must be TRUE):
  1. Settings top-level shows a 6-tile grid; tapping any tile drills into its category page
  2. Category tiles display live status subtitles reflecting current configuration state
  3. Every existing setting is reachable in the new hierarchy — no setting lost or orphaned
  4. All config paths are unchanged — existing config.yaml files work without migration
  5. Read-only fields are visually distinct from editable controls (no confusion about interactivity)
**Plans:** 2 plans

Plans:
- [ ] 02-01-PLAN.md — Tile subtitle + ReadOnlyField UX + tile grid landing page in SettingsMenu
- [ ] 02-02-PLAN.md — Category page creation/restructuring + SettingsMenu wiring + checkpoint

### Phase 3: EQ Dual-Access & Shell Polish
**Goal**: The entire shell (NavStrip, TopBar, launcher, modals) matches the automotive-minimal aesthetic, and EQ is accessible from both Audio settings and a NavStrip shortcut
**Depends on**: Phase 2
**Requirements**: UX-01, UX-02, UX-04, ICON-03, ICON-04, NAV-01, NAV-02, NAV-03
**Success Criteria** (what must be TRUE):
  1. EQ opens from both the Audio settings subcategory and the NavStrip shortcut icon, showing identical state
  2. NavStrip buttons have consistent sizing, icons, and press feedback matching the automotive-minimal style
  3. Launcher grid tiles are restyled with the automotive-minimal aesthetic and show press feedback on tap
  4. Modal pickers dismiss when tapping outside the modal area
  5. BT device forget action has a clearly visible, adequately-sized touch target
**Plans**: TBD

Plans:
- [ ] 03-01: TBD
- [ ] 03-02: TBD

### Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Visual Foundation | 3/3 | Complete | 2026-03-02 |
| 2. Settings Restructuring | 0/? | Not started | - |
| 3. EQ Dual-Access & Shell Polish | 0/? | Not started | - |

---
*Last updated: 2026-03-02*
