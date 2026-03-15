---
gsd_state_version: 1.0
milestone: v0.6
milestone_name: milestone
status: verifying
stopped_at: Phase 14 context gathered
last_updated: "2026-03-15T20:17:42.021Z"
last_activity: 2026-03-15 — Theme persistence fix + wallpaper toggle UX
progress:
  total_phases: 5
  completed_phases: 3
  total_plans: 3
  completed_plans: 3
  percent: 60
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-15)

**Core value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.
**Current focus:** v0.6.2 — Phase 13.2: Fix Saving of Themes Received by Companion App (COMPLETE, awaiting Pi verification)

## Current Position

Phase: 13.2 of 15 (Fix Saving of Themes Received by Companion App)
Plan: 01 complete (1 of 1)
Status: Phase 13.2 complete, binary cross-built, awaiting Pi deployment + manual verification
Last activity: 2026-03-15 — Theme persistence fix + wallpaper toggle UX

Progress: [██████░░░░] 60%

## Accumulated Context

### Open Items (carried forward)

- YamlConfig default port mismatch: `YamlConfig.cpp` defaults tcp_port to 5277, `Configuration.hpp` defaults to 5288
- NavigationTurnLabel UTF-8 parse errors spamming logs

### Roadmap Evolution

- Phase 13.1 inserted after Phase 13: Quick Bugfix for Companion App (URGENT)
- Phase 13.2 inserted after Phase 13.1: Fix saving of themes received by companion app (URGENT)

### Blockers/Concerns

- Phase 14 night palette must be verified on Pi in a dark environment before finalizing — M3 Expressive bold primaries can be a glare hazard in-car

### Decisions

- Wallpaper sourceSize: no subpixel margin -- not worth complexity
- Wallpaper z-order: implicit z:0 as first child, no explicit z-index
- clearClientSession() disconnects signals before abort() to prevent re-entrant signal emission
- Always-replace on new companion connection -- newest connection always wins
- Inactivity timer resets only on MAC-verified status messages
- stop() delegates to clearClientSession() instead of duplicating cleanup
- ConfigService creation moved before theme loading in main.cpp for persistence wiring
- Custom Wallpaper toggle derives state from config value (empty=OFF, non-empty=ON)
- Theme Default removed from wallpaper picker (toggle OFF serves that purpose)

## Session Continuity

Last session: 2026-03-15T20:17:42.019Z
Stopped at: Phase 14 context gathered
