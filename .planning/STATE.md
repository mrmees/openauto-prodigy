---
gsd_state_version: 1.0
milestone: v0.5
milestone_name: milestone
status: in-progress
stopped_at: Plan 03.2-01 complete
last_updated: "2026-03-09T17:06:39Z"
last_activity: 2026-03-09 -- Plan 03.2-01 executed (ThemeService 34 M3 roles + import/delete)
progress:
  total_phases: 6
  completed_phases: 4
  total_plans: 13
  completed_plans: 11
  percent: 85
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-08)

**Core value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.
**Current focus:** v0.5.1 DPI Sizing & UI Polish -- Phase 03.2 in progress (Companion Theme Import)

## Current Position

Phase: 3.2 of 5 (Companion Theme Import)
Plan: 1 of 3 complete
Status: Plan 03.2-01 complete -- ThemeService expanded to 34 M3 roles with import/delete
Last activity: 2026-03-09 -- Plan 03.2-01 executed (ThemeService 34 M3 roles + import/delete)

Progress: [████████░░] 85%

## Performance Metrics

**Velocity:**
- Total plans completed: 11
- Average duration: 5 min
- Total execution time: 0.91 hours

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.

- (01-03) Pixel clock check at bytes 54-55 to distinguish timing vs monitor descriptors before reading EDID physical size
- (01-01) computedDpi NOTIFY bound to windowSizeChanged for dual-trigger QML recalc
- (01-01) qFuzzyCompare for screen size equality to avoid float precision issues
- (01-02) 160 DPI (Android mdpi) as reference baseline for scale computation
- (01-02) Screen info is read-only in settings per user decision
- (02-01) Clock fills 75% of control height with DemiBold weight for glanceability
- (02-01) 12h format omits AM/PM -- in a car you know if it's day or night
- (02-01) UiMetrics Connections on ConfigService.configChanged for reactive config updates
- (02-02) Safety revert triggers on every scale change, not just extreme values -- Windows display pattern
- (02-02) No animation on scale changes -- instant re-layout avoids visual chaos
- (02-02) Overlay at Flickable bottom with z:10 to float over scrollable content
- (03-01) Derived colors (scrim, pressed, barShadow, success, onSuccess) are computed constants, not stored in theme YAML
- (03-01) Old property names completely removed -- clean break, no backwards compat shim
- (03-02) Qt.rgba() with ThemeService color components for alpha-tinted panel backgrounds
- (03-02) ThemeService.onRed/onSuccess for text on colored buttons instead of hardcoded 'white'
- (03-02) ThemeService.outline for panel borders (was hardcoded #0f3460)
- [Phase 03]: AA tokens applied to both day and night color maps (phone sends one palette)
- [Phase 03]: UiConfigRequest sent on session Active state, response is informational-only logging
- (03.1-01) Hyphenated keys in YAML and C++ to match AA wire token format
- (03.1-01) applyAATokens always caches to connected-device YAML even when another theme is active
- (03.1-01) Swap-and-restore pattern for persistence when connected-device is not active theme
- (03.1-02) Phone pushes tokens inbound (0x8011), HU no longer sends outbound UiConfigRequest
- (03.1-02) ACCEPTED ack sent immediately after parsing, before ThemeService processing
- (03.2-01) scrim promoted from hardcoded QColor(0,0,0,180) to YAML-stored opaque black
- (03.2-01) importCompanionTheme writes to first search path (user themes dir) -- bundled protected
- (03.2-01) error/onError are new M3 keys separate from red/onRed -- both coexist in YAML
- (03.2-01) allThemeKeys = knownM3Keys | knownAATokenKeys for connected-device YAML persistence

### Pending Todos

None yet.

### Roadmap Evolution

- Phase 03.1 inserted after Phase 03: get AA theme information working from stream (URGENT)
- Phase 03.2 inserted after Phase 03: Companion Theme Import (URGENT)

### Blockers/Concerns

- Submodule is read-only -- any proto corrections must go to the open-android-auto repo, not this one.

## Session Continuity

Last session: 2026-03-09T17:06:39Z
Stopped at: Completed 03.2-01-PLAN.md
