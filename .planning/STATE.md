---
gsd_state_version: 1.0
milestone: v0.4.2
milestone_name: Service Hardening
status: defining_requirements
last_updated: "2026-03-02T04:13:26Z"
progress:
  total_phases: 3
  completed_phases: 3
  total_plans: 6
  completed_plans: 6
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-01)

**Core value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.
**Current focus:** v0.4.2 Service Hardening — reliable boot and lifecycle transitions.

## Current Position

Phase: Not started (defining requirements)
Plan: —
Status: Defining requirements for v0.4.2
Last activity: 2026-03-02 — Milestone v0.4.2 started

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 6
- Average duration: 5min
- Total execution time: 31min

| Phase | Plan | Duration | Tasks | Files |
|-------|------|----------|-------|-------|
| 01-dsp-core | 01 | 4min | 2 | 5 |
| 01-dsp-core | 02 | 5min | 1 | 5 |
| 02-service-config | 01 | 5min | 2 | 8 |
| 02-service-config | 02 | 10min | 2 | 16 |
| 03-head-unit-eq-ui | 01 | 5min | 2 | 5 |
| 03-head-unit-eq-ui | 02 | 2min | 2 | 3 |

*Updated after each plan completion*

## Accumulated Context

### Decisions

- [01-01] float32 precision for all DSP (sufficient for 10-band graphic EQ at 16-bit input)
- [01-01] Direct Form II Transposed topology for biquad (fewer delay elements, good numerical behavior)
- [01-01] Per-channel limiter (not stereo-linked) for simplicity and zero crosstalk
- [01-02] Generation counter for double-buffer swap detection (avoids ABA problem with pointer comparison)
- [01-02] Bypass fast-path snaps coefficients immediately (no interpolation while fully bypassed)
- [01-02] Bypass crossfade uses same 2304-sample ramp as coefficient interpolation
- [02-01] saveUserPreset takes StreamId parameter (caller specifies source stream)
- [02-01] setGain clears activePreset to "" (indicates manual adjustment from preset)
- [02-01] User presets in-memory only; config persistence deferred to Plan 02
- [02-02] 2-second debounce timer for EQ config saves (avoids disk thrash)
- [02-02] saveNow() on aboutToQuit for guaranteed shutdown persistence
- [02-02] EQ defaults: Media=Flat, Navigation=Voice, Phone=Voice
- [02-02] eqEngine as non-owning raw pointer in AudioStreamHandle

- [03-01] int-parameter Q_INVOKABLE overloads for QML (StreamId enum not registered)
- [03-01] Dual signal emission: gainsChanged(StreamId) + gainsChangedForStream(int) for C++ and QML
- [03-01] SegmentedButton with empty configPath for transient stream selection
- [03-01] 0.5 dB snap granularity for touch-friendly slider interaction
- [03-02] Preset picker as bottom sheet Dialog following FullScreenPicker pattern
- [03-02] Swipe-to-delete via drag.target on content Rectangle over red background
- [03-02] Depth-aware back handler checks settingsStack.depth after pop for title

### Roadmap Evolution

- v0.4: Originally scoped as v1.0 with 4+ phases. Rescoped to logging + theming only.
- v0.4.1: Audio equalizer -- 3 phases (DSP Core -> Service & Config -> Head Unit UI). Web config panel EQ deferred to future milestone.

### Pending Todos

None.

### Blockers/Concerns

- PipeWire SCO behavior needs wire verification on Pi before finalizing PhonePlugin architecture
- Research flagged: QML slider layout at 1024x600 (10 vertical sliders in ~900px) needs visual prototyping in Phase 3

## Session Continuity

Last session: 2026-03-02
Stopped at: Defining requirements for v0.4.2 Service Hardening
Resume file: None
