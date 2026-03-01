# Roadmap: OpenAuto Prodigy v1.0

## Overview

The core AA experience works end-to-end. This roadmap closes the remaining gaps between "it works" and a tagged v1.0 release that others can install and daily-drive. Four phases: logging foundations first (makes debugging everything else easier), then visible polish (themes, brightness), then the critical audio features (HFP independence, EQ), and finally release hardening to make the whole thing bulletproof.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3, 4): Planned milestone work
- Decimal phases (e.g., 2.1): Urgent insertions (marked with INSERTED)

- [ ] **Phase 1: Logging Cleanup** - Replace debug spam with categorized, quiet-by-default logging (UAT gap closure in progress)
- [ ] **Phase 2: Theme & Display** - User-selectable color palettes, wallpapers, and brightness control

## Phase Details

### Phase 1: Logging Cleanup
**Goal**: Production-quality log output that is quiet by default and detailed on demand
**Depends on**: Nothing (first phase)
**Requirements**: LOG-01, LOG-02, LOG-03
**Success Criteria** (what must be TRUE):
  1. Running the app with default settings produces no debug-level output in journalctl
  2. Launching with --verbose (or setting a config flag) enables full debug output per component
  3. Log lines include component category tags (AA, BT, Audio, Plugin, etc.) when verbose
**Plans**: 3 plans

Plans:
- [x] 01-01-PLAN.md — Logging infrastructure (categories, message handler, CLI parsing, tests)
- [x] 01-02-PLAN.md — Migrate all log calls + IPC/web panel runtime toggle
- [ ] 01-03-PLAN.md — UAT gap closure: fix log severity triage + library tag filter

### Phase 2: Theme & Display
**Goal**: Users can personalize the head unit appearance and control screen brightness
**Depends on**: Phase 1
**Requirements**: DISP-01, DISP-02, DISP-03, DISP-04, DISP-05
**Success Criteria** (what must be TRUE):
  1. User can open settings and switch between at least 3 color palettes, with the UI updating live
  2. User can select a wallpaper from available options, visible on the home/launcher screen
  3. Selected theme and wallpaper survive a full reboot
  4. User can adjust screen brightness from the settings slider and gesture overlay, and the physical backlight changes on Pi
**Plans**: 6 plans

Plans:
- [x] 02-01-PLAN.md — ThemeService multi-theme scanning, switching, wallpaper source + bundled theme YAMLs
- [x] 02-02-PLAN.md — DisplayService implementation (3-tier brightness: sysfs > ddcutil > software overlay)
- [x] 02-03-PLAN.md — UI wiring: theme picker, wallpaper, brightness sliders, dimming overlay + Pi verification
- [x] 02-04-PLAN.md — Gap closure: wallpaper images, enumeration, picker UI, persistence
- [x] 02-05-PLAN.md — Gap closure: wallpaper display in Shell.qml + gesture overlay wiring
- [ ] 02-06-PLAN.md — Gap closure: wallpaper config persistence + dynamic wallpaper refresh

### Phase 3: equalizer

**Goal:** [To be planned]
**Requirements**: TBD
**Depends on:** Phase 3
**Plans:** 0 plans

Plans:
- [ ] TBD (run /gsd:plan-phase 4 to break down)
