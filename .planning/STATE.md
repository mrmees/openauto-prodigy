---
gsd_state_version: 1.0
milestone: v0.6
milestone_name: milestone
status: verifying
stopped_at: Completed 14-01-PLAN.md
last_updated: "2026-03-15T21:01:11Z"
last_activity: 2026-03-15 — ThemeService tint properties, warning tokens, night guardrail
progress:
  total_phases: 5
  completed_phases: 3
  total_plans: 4
  completed_plans: 4
  percent: 65
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-15)

**Core value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.
**Current focus:** Phase 14: Color Audit & M3 Expressive Tokens — Plan 01 complete

## Current Position

Phase: 14 of 15 (Color Audit & M3 Expressive Tokens)
Plan: 01 complete (1 of 2)
Status: Plan 01 (ThemeService infrastructure) complete; Plan 02 (QML audit) next
Last activity: 2026-03-15 — ThemeService tint properties, warning tokens, night guardrail

Progress: [██████▌░░░] 65%

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

- HSL saturation clamp at 0.55 for night comfort guardrail (simpler than HCT chroma, Qt-native)
- Guardrail applies inside activeColor() so tint blends use clamped primary automatically
- Accent roles: primary, primary-container, secondary, secondary-container, tertiary, tertiary-container
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

Last session: 2026-03-15T21:01:11Z
Stopped at: Completed 14-01-PLAN.md
