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

---

## v0.5.2 Widget System & UI Polish

**Milestone Goal:** Add a widget-based home screen and clean up settings UI for touch-friendly daily use. Demote power-user settings to YAML-only, reorganize into logical categories, and normalize all pages for consistent touch interaction.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [x] **Phase 1: Widget System** - Home screen with 3-pane widget layout, launcher dock, and built-in widgets (completed 2026-03-10)
- [x] **Phase 2: Settings Restructure** - Reorganize settings into 9 categories, move/remove/demote power-user and debug controls (completed 2026-03-11)
- [ ] **Phase 3: Touch Normalization** - Consistent list styling, touch-friendly controls, and readability across all settings pages

## Phase Details

### Phase 1: Widget System
**Goal**: Users have a useful, customizable home screen with glanceable information
**Depends on**: Nothing (first phase)
**Requirements**: WDG-01, WDG-02, WDG-03, WDG-04, WDG-05, WDG-06
**Success Criteria** (what must be TRUE):
  1. Home screen shows 3 widget panes in landscape (60/40 split) and adapts to portrait
  2. Launcher dock at bottom provides quick access to installed plugins
  3. Long-pressing a widget pane opens a context menu to change widget, adjust opacity, or clear
  4. Clock, AA Status, and Now Playing widgets display real-time information in their panes
**Plans**: 3/3 plans complete

Plans:
- [x] 01-01-PLAN.md -- Widget architecture (WidgetDescriptor, WidgetPlacement, WidgetInstanceContext)
- [x] 01-02-PLAN.md -- Home screen layout, LauncherDock, glass card WidgetHost
- [x] 01-03-PLAN.md -- Built-in widgets (Clock, AA Status, Now Playing) and widget picker

### Phase 2: Settings Restructure
**Goal**: Settings are reorganized into clear, focused categories with power-user controls demoted
**Depends on**: Phase 1
**Requirements**: SET-01, SET-02, SET-03, SET-04, SET-05, SET-06, SET-07, SET-08, SET-09, SET-10
**Success Criteria** (what must be TRUE):
  1. Settings top-level shows 9 category entries (AA, Display, Audio, Bluetooth, Theme, Companion, System, Information, Debug) and each opens to a focused sub-page
  2. Each sub-page contains only its designated controls -- no duplicates across pages, no power-user controls outside Debug
  3. FPS setting removed from UI (YAML-only), About section removed, debug protocol buttons moved to Debug page
  4. Theme has its own top-level entry with picker, wallpaper, and dark mode toggle
**Plans**: 2 plans

Plans:
- [x] 02-01-PLAN.md -- Create new pages (Theme, Information, Debug) and update menu routing
- [x] 02-02-PLAN.md -- Restructure existing pages, remove relocated controls, delete dead files

### Phase 3: Touch Normalization
**Goal**: Every settings page is comfortable to use by touch in a car
**Depends on**: Phase 2
**Requirements**: TCH-01, TCH-02, TCH-03
**Success Criteria** (what must be TRUE):
  1. All settings sub-pages use consistent alternating-row list styling matching the main settings menu
  2. All interactive controls (toggles, pickers, sliders) have touch-friendly sizing via UiMetrics (DPI-scaled)
  3. Text is readable at arm's length across all target display sizes (no hardcoded pixel values)
**Plans**: 3 plans

Plans:
- [x] 03-01-PLAN.md -- Create SettingsRow component, update controls for flat mode, apply to 5 simpler pages
- [x] 03-02-PLAN.md -- Apply SettingsRow to 4 complex pages (Display, Audio, Bluetooth, Debug)
- [ ] 03-03-PLAN.md -- Gap closure: increase font base sizes for automotive readability

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Widget System | 3/3 | Complete | 2026-03-10 |
| 2. Settings Restructure | 2/2 | Complete   | 2026-03-11 |
| 3. Touch Normalization | 2/3 | In progress (gap closure) | - |

---
*Last updated: 2026-03-11 -- Gap closure plan 03-03 created for font size readability*
