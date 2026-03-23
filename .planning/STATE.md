---
gsd_state_version: 1.0
milestone: v0.7
milestone_name: milestone
status: Phase 30 complete, ready for Phase 31
last_updated: "2026-03-22T23:59:00Z"
last_activity: 2026-03-22 — Completed Phase 30 Plan 01
progress:
  total_phases: 5
  completed_phases: 3
  total_plans: 3
  completed_plans: 3
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-22)

**Core value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.
**Current focus:** v0.7.0 Kiosk Session & Boot Experience — Phase 30 complete, Phase 31 next

## Current Position

Phase: 30 of 32 — RPi Boot Splash (COMPLETE)
Plan: 1 of 1 (complete)
Status: Phase 30 complete, ready for Phase 31
Last activity: 2026-03-22 — Completed 30-01 pre-converted TGA + boot splash docs

## Accumulated Context

### Open Items (carried forward)

- YamlConfig default port mismatch: `YamlConfig.cpp` defaults tcp_port to 5277, `Configuration.hpp` defaults to 5288
- NavigationTurnLabel UTF-8 parse errors spamming logs

### Decisions

- [28-01] Used `<labwc_config>` root element for kiosk rc.xml (canonical schema, not backward-compat `<openbox_config>`)
- [28-01] Wildcard `identifier="openauto*"` in windowRule avoids app_id mismatch without main.cpp changes
- [28-01] One service file for both modes with zero panel management -- kiosk provides clean canvas
- [29-01] 500ms delay after frameSwapped before pkill swaybg -- ensures compositor presentation
- [29-01] pkill -f "swaybg.*splash" pattern targets only kiosk splash instance
- [29-01] Heap-allocated QMetaObject::Connection for one-shot disconnect pattern

### Blockers/Concerns

None yet.
