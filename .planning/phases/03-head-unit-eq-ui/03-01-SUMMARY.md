---
phase: 03-head-unit-eq-ui
plan: 01
subsystem: ui
tags: [qml, equalizer, slider, qt6, pipewire]

requires:
  - phase: 02-service-config
    provides: EqualizerService with per-stream gains, presets, bypass, config persistence

provides:
  - EqBandSlider custom vertical slider control
  - EqSettings page with 10 sliders, stream selector, bypass toggle
  - QML-friendly EqualizerService helpers (int-param overloads, gainsAsList, bandLabel)

affects: [03-02 preset picker, web-config EQ panel]

tech-stack:
  added: []
  patterns: [int-parameter Q_INVOKABLE overloads for QML interop with C++ enums]

key-files:
  created:
    - qml/controls/EqBandSlider.qml
    - qml/applications/settings/EqSettings.qml
  modified:
    - src/core/services/EqualizerService.hpp
    - src/core/services/EqualizerService.cpp
    - src/CMakeLists.txt

key-decisions:
  - "int-parameter overloads for QML (StreamId enum not QML-registered)"
  - "Dual signal emission: gainsChanged(StreamId) + gainsChangedForStream(int) for C++ and QML consumers"
  - "SegmentedButton with empty configPath for transient stream selection (no config persistence)"
  - "0.5 dB snap granularity for touch-friendly slider interaction"

patterns-established:
  - "QML enum interop: add int-parameter Q_INVOKABLE overloads that delegate to StreamId-based methods"
  - "EqBandSlider: reusable vertical slider with gain-to-Y mapping, floating label, bypass dimming"

requirements-completed: [UI-01, UI-03]

duration: 5min
completed: 2026-03-02
---

# Phase 03 Plan 01: Core EQ UI Summary

**10-band EQ slider page with per-stream switching, bypass toggle, and QML-friendly EqualizerService helpers**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-02T04:03:47Z
- **Completed:** 2026-03-02T04:08:56Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- QML-friendly EqualizerService helpers: gainsAsList, bandCount, bandLabel, int-parameter overloads for setGain/setBypassed/isBypassed/activePreset/applyPreset
- EqBandSlider custom control with vertical drag, 0.5dB snap, floating dB label, 0dB reference line, bypass dimming, double-tap reset
- EqSettings page with 10 sliders, SegmentedButton stream selector (Media/Nav/Phone), bypass badge, live Connections for external updates

## Task Commits

Each task was committed atomically:

1. **Task 1: Add QML-friendly helpers to EqualizerService** - `63fbca2` (feat)
2. **Task 2: Create EqBandSlider control and EqSettings page** - `a715b28` (feat)

## Files Created/Modified
- `src/core/services/EqualizerService.hpp` - Added QML helper declarations, bypassedChanged/gainsChangedForStream signals
- `src/core/services/EqualizerService.cpp` - Implemented gainsAsList, bandCount, bandLabel, int-param overloads, signal emissions
- `qml/controls/EqBandSlider.qml` - Custom vertical EQ slider with touch handling and visual feedback
- `qml/applications/settings/EqSettings.qml` - Full EQ page with 10 sliders, stream selector, bypass badge
- `src/CMakeLists.txt` - Registered both new QML files in module

## Decisions Made
- Used int-parameter overloads (not Q_ENUM registration) for StreamId QML interop -- simpler, no enum registration boilerplate
- Dual signal emission (gainsChanged + gainsChangedForStream) preserves C++ API while enabling QML Connections
- SegmentedButton with configPath="" for transient stream selection (verified empty-string skip in source)
- 0.5 dB snap granularity balances precision with touch usability on a car display

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- Pre-existing test_event_bus failure (unrelated to EQ, out of scope) -- 57/58 tests pass

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- EqSettings page exists but has no navigation entry yet (Plan 02 adds the "Equalizer >" item in AudioSettings and preset picker)
- Preset label shown as non-interactive text ("Flat", "Custom") -- Plan 02 makes it clickable

---
*Phase: 03-head-unit-eq-ui*
*Completed: 2026-03-02*
