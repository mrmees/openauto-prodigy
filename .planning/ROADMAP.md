# Roadmap: OpenAuto Prodigy v1.0

## Overview

The core AA experience works end-to-end. This roadmap closes the remaining gaps between "it works" and a tagged v1.0 release that others can install and daily-drive. Four phases: logging foundations first (makes debugging everything else easier), then visible polish (themes, brightness), then the critical audio features (HFP independence, EQ), and finally release hardening to make the whole thing bulletproof.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3, 4): Planned milestone work
- Decimal phases (e.g., 2.1): Urgent insertions (marked with INSERTED)

- [ ] **Phase 1: Logging Cleanup** - Replace debug spam with categorized, quiet-by-default logging (UAT gap closure in progress)
- [ ] **Phase 2: Theme & Display** - User-selectable color palettes, wallpapers, and brightness control
- [ ] **Phase 3: Audio Features** - HFP call audio independence from AA session + parametric EQ with presets
- [ ] **Phase 4: Release Hardening** - First-run experience, watchdog, clean shutdown, daily-driver stability

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
**Plans**: 5 plans

Plans:
- [x] 02-01-PLAN.md — ThemeService multi-theme scanning, switching, wallpaper source + bundled theme YAMLs
- [x] 02-02-PLAN.md — DisplayService implementation (3-tier brightness: sysfs > ddcutil > software overlay)
- [x] 02-03-PLAN.md — UI wiring: theme picker, wallpaper, brightness sliders, dimming overlay + Pi verification
- [x] 02-04-PLAN.md — Gap closure: wallpaper images, enumeration, picker UI, persistence
- [ ] 02-05-PLAN.md — Gap closure: wallpaper display in Shell.qml + gesture overlay wiring

### Phase 3: Audio Features
**Goal**: Phone calls never drop due to AA state, and music sounds good through car speakers with EQ
**Depends on**: Phase 1
**Requirements**: AUD-01, AUD-02, AUD-03, AUD-04, AUD-05, AUD-06
**Success Criteria** (what must be TRUE):
  1. An active HFP phone call continues with audio when the AA session disconnects or crashes
  2. AA media audio ducks (reduces volume) automatically when an HFP call is active
  3. User can select an EQ preset (flat, bass boost, vocal, etc.) from head unit settings and hear the difference immediately
  4. User can create a custom EQ profile with per-band gain sliders, save it, and recall it later
  5. EQ is editable from the web config panel and applies only to music/media (not navigation or call audio)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

### Phase 4: Release Hardening
**Goal**: The app is trustworthy enough to be someone's daily-driver head unit without SSH access
**Depends on**: Phase 2, Phase 3
**Requirements**: REL-01, REL-02, REL-03, REL-04, REL-05
**Success Criteria** (what must be TRUE):
  1. First launch after install presents a guided flow that walks the user through phone pairing and WiFi verification
  2. Selecting shutdown or reboot from the ExitDialog actually powers off or reboots the Pi
  3. If the app crashes or hangs, systemd automatically restarts it within 10 seconds
  4. Killing the app with SIGTERM results in a clean exit (config saved, BT ungrabbed, connections closed, no orphan processes)
  5. The app runs as a daily driver for a week without crashes requiring manual intervention
**Plans**: TBD

Plans:
- [ ] 04-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Logging Cleanup | 2/3 | UAT gap closure | - |
| 2. Theme & Display | 4/5 | UAT gap closure | - |
| 3. Audio Features | 0/? | Not started | - |
| 4. Release Hardening | 0/? | Not started | - |
