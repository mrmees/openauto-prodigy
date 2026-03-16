---
gsd_state_version: 1.0
milestone: v0.6.3
milestone_name: Proxy Routing Fix
status: defining_requirements
stopped_at: null
last_updated: "2026-03-16"
last_activity: 2026-03-16 — Milestone v0.6.3 started
progress:
  total_phases: 0
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-16)

**Core value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.
**Current focus:** Defining requirements for v0.6.3 Proxy Routing Fix

## Current Position

Phase: Not started (defining requirements)
Plan: —
Status: Defining requirements
Last activity: 2026-03-16 — Milestone v0.6.3 started

Progress: [░░░░░░░░░░] 0%

## Accumulated Context

### Open Items (carried forward)

- YamlConfig default port mismatch: `YamlConfig.cpp` defaults tcp_port to 5277, `Configuration.hpp` defaults to 5288
- NavigationTurnLabel UTF-8 parse errors spamming logs

### Roadmap Evolution

- Phase 13.1 inserted after Phase 13: Quick Bugfix for Companion App (URGENT)
- Phase 13.2 inserted after Phase 13.1: Fix saving of themes received by companion app (URGENT)
- Phase 14.1 inserted after Phase 14: Define new default themes (URGENT)

### Blockers/Concerns

- (RESOLVED) Phase 14 night palette verified on Pi -- approved

### Decisions

- HSL saturation clamp at 0.55 for night comfort guardrail (simpler than HCT chroma, Qt-native)
- Guardrail applies inside activeColor() so tint blends use clamped primary automatically
- Accent roles: primary, primary-container, secondary, secondary-container, tertiary, tertiary-container
- Solid primaryContainer fills for pressed/hold states (not opacity overlays) for correct M3 contrast
- Material.accent override on all Switch controls for consistent primaryContainer track color
- Semantic success/warning tokens always win over theme accent for status indicators
- Wallpaper sourceSize: no subpixel margin -- not worth complexity
- Wallpaper z-order: implicit z:0 as first child, no explicit z-index
- clearClientSession() disconnects signals before abort() to prevent re-entrant signal emission
- Always-replace on new companion connection -- newest connection always wins
- Inactivity timer resets only on MAC-verified status messages
- stop() delegates to clearClientSession() instead of duplicating cleanup
- ConfigService creation moved before theme loading in main.cpp for persistence wiring
- Custom Wallpaper toggle derives state from config value (empty=OFF, non-empty=ON)
- Theme Default removed from wallpaper picker (toggle OFF serves that purpose)
- Prodigy theme renamed to default (id: default, name: Prodigy) for first-install identity
- Old hand-crafted themes (ember, ocean, amoled) retired in favor of companion-generated M3 palettes
- Wallpapers capped at 1440x1440 for Pi GPU/memory constraints
- FullScreenPicker delegate shadows removed to prevent GPU freeze with 9+ items

## Session Continuity

Last session: 2026-03-16T00:22:33Z
Stopped at: Completed 14.1-01-PLAN.md
