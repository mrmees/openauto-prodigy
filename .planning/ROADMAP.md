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

---

## v0.6.2 Theme Expression & Wallpaper Scaling (Phases 13-15)

**Milestone Goal:** Fix confirmed Wallpaper.qml memory and rendering bugs, apply M3 Expressive token corrections to the existing color audit, and add a color boldness slider that amplifies accent saturation without touching neutral/surface roles.

### Phases

- [x] **Phase 13: Wallpaper Hardening** - Cap texture memory, eliminate paint-outside-bounds, prevent transition flicker — always cover-fit (completed 2015-03-15)
- [x] **Phase 13.1: Quick Bugfix for Companion App** - Fix stale client blocking reconnects, add socket error handling and inactivity timeout (INSERTED) (completed 2015-03-15)
- [x] **Phase 13.2: Fix Theme Persistence & Wallpaper UX** - Persist theme selection to config, gate wallpaper override behind toggle, relocate delete button (INSERTED) (completed 2015-03-15)
- [x] **Phase 14: Color Audit & M3 Expressive Tokens** - Fix confirmed on-* token pairings, apply NavbarControl active state fix, update interactive controls to use bolder accent colors (completed 2015-03-15)

## Phase Details

### Phase 13: Wallpaper Hardening
**Goal**: Wallpaper.qml is memory-safe, renders without bleeding into adjacent regions, and never flickers during theme or day/night transitions
**Depends on**: Nothing (independent QML fixes)
**Requirements**: WP-01
**Success Criteria** (what must be TRUE):
  1. A phone-camera wallpaper (8+ MP JPEG) does not cause a visible memory spike or video decode degradation during AA session
  2. Switching themes or toggling day/night mode does not produce a solid-color flash — the previous wallpaper stays visible until the new one is loaded
  3. A portrait-orientation wallpaper does not paint outside the wallpaper container into the navbar or widget area
  4. Every wallpaper fills the display edge-to-edge with no letterbox bars regardless of the image's aspect ratio
**Plans**: 1 plan

Plans:
- [x] 13-01-PLAN.md — Harden Wallpaper.qml + move to root shell layer

### Phase 13.1: Quick Bugfix for Companion App (INSERTED)

**Goal:** Fix stale CompanionListenerService client blocking all future companion reconnects via idempotent cleanup, always-replace, socket error handling, and inactivity timeout
**Requirements**: BF-01, BF-02, BF-03, BF-04
**Depends on:** Phase 13
**Plans:** 1/1 plans complete

Plans:
- [x] 13.1-01-PLAN.md — Harden companion reconnect with clearClientSession, always-replace, errorOccurred, and 30s inactivity timeout

### Phase 13.2: Fix Theme Persistence & Wallpaper UX (INSERTED)

**Goal:** ThemeService::setTheme() persists theme ID to config so companion-imported and manually-selected themes survive restart; wallpaper override picker gated behind toggle to prevent accidental masking; delete button relocated to reduce mis-taps
**Requirements**: TP-01, TP-02, TP-03
**Depends on:** Phase 13.1
**Plans:** 1/1 plans complete

Plans:
- [ ] 13.2-01-PLAN.md — Wire IConfigService into ThemeService, restructure ThemeSettings.qml

### Phase 14: Color Audit & M3 Expressive Tokens
**Goal**: All interactive surfaces with accent-colored backgrounds have correct on-* foreground tokens, NavbarControl active state uses the right M3 pair, and widget/tile/control colors use bolder accent values from the M3 Expressive palette
**Depends on**: Phase 13
**Requirements**: CA-01, CA-02, CA-03
**Success Criteria** (what must be TRUE):
  1. Text on every primary/secondary/tertiary-colored surface is legible in both day and night modes — no white-on-light or dark-on-dark combinations visible on the Pi
  2. The active/pressed navbar control shows a distinctly colored fill (primaryContainer) with appropriately contrasting foreground (onPrimaryContainer), visually distinct from inactive controls
  3. Widget cards, settings tiles, and interactive controls show noticeably bolder accent colors compared to pre-milestone neutral-only surfaces
  4. Night mode primary color is visually comfortable at arm's length in a dark environment — not a glare source
**Plans**: 2 plans

Plans:
- [x] 14-01-PLAN.md — ThemeService C++ infrastructure: tint properties, warning tokens, night guardrail
- [x] 14-02-PLAN.md — QML color audit fixes, token pairings, state matrix document

### Phase 14.1: Define new default themes (INSERTED)

**Goal:** Promote 9 visually-approved companion-created themes from the Pi into bundled `config/themes/`, replacing the 4 old defaults. Prodigy theme becomes the new `default`.
**Requirements**: TM-01, TM-02
**Depends on:** Phase 14
**Plans:** 1 plan

Plans:
- [ ] 14.1-01-PLAN.md — Pull 9 themes from Pi, replace old defaults, rename prodigy to default
