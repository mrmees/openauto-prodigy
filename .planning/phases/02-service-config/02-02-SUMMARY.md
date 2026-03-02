---
phase: 02-service-config
plan: 02
subsystem: audio
tags: [equalizer, persistence, yaml-config, pipewire, host-context, qml-context]

requires:
  - phase: 02-service-config
    plan: 01
    provides: "EqualizerService with 3 per-stream engines and preset management"
provides:
  - "YAML config persistence for EQ stream presets and user presets"
  - "Debounced save with shutdown flush via saveNow()"
  - "EQ processing in PipeWire playback callback (RT-safe)"
  - "EqualizerService registered in IHostContext for plugin access"
  - "EqualizerService registered in QML context for Phase 3 UI"
affects: [03-head-unit-ui]

tech-stack:
  added: []
  patterns: [debounced-config-save, eq-pipeline-integration, hostcontext-service-registration]

key-files:
  created: []
  modified:
    - src/core/YamlConfig.hpp
    - src/core/YamlConfig.cpp
    - src/core/services/EqualizerService.hpp
    - src/core/services/EqualizerService.cpp
    - src/core/services/AudioService.hpp
    - src/core/services/AudioService.cpp
    - src/core/plugin/IHostContext.hpp
    - src/core/plugin/HostContext.hpp
    - src/core/aa/AndroidAutoOrchestrator.hpp
    - src/core/aa/AndroidAutoOrchestrator.cpp
    - src/plugins/android_auto/AndroidAutoPlugin.cpp
    - src/main.cpp
    - tests/test_yaml_config.cpp
    - tests/test_equalizer_service.cpp
    - tests/test_plugin_manager.cpp
    - tests/test_plugin_model.cpp

key-decisions:
  - "2-second debounce timer for config saves (avoids disk thrash during rapid EQ adjustments)"
  - "saveNow() on aboutToQuit for clean shutdown flush"
  - "EQ defaults: Media=Flat, Navigation=Voice, Phone=Voice"
  - "eqEngine as non-owning raw pointer in AudioStreamHandle (ownership stays with EqualizerService)"

patterns-established:
  - "Debounced config persistence via QTimer::singleShot pattern"
  - "Service registration flow: create in main.cpp, set in HostContext, pass to orchestrator, expose to QML"

requirements-completed: [CFG-01, PRST-01, PRST-02]

duration: 10min
completed: 2026-03-02
---

# Phase 02 Plan 02: Config Persistence & Pipeline Integration Summary

**YAML-persisted EQ presets with debounced save, RT-safe EQ processing in PipeWire callback, and HostContext/QML registration**

## Performance

- **Duration:** 10 min
- **Started:** 2026-03-02T00:39:39Z
- **Completed:** 2026-03-02T00:49:17Z
- **Tasks:** 2
- **Files modified:** 16

## Accomplishments
- YamlConfig gains EQ config methods: per-stream preset storage and user preset CRUD with save/load round-trip
- EqualizerService loads presets from config on startup, debounce-saves on every mutation, flushes on shutdown
- EQ engine pointer in AudioStreamHandle, process() called in PipeWire RT callback for all 3 AA streams
- EqualizerService registered in IHostContext (for plugins) and QML context (for Phase 3 UI)
- 12 new tests (5 YamlConfig + 7 EqualizerService config-aware), all passing

## Task Commits

Each task was committed atomically:

1. **Task 1: YamlConfig EQ persistence and EqualizerService config integration** - `24a39a6` (feat, TDD)
2. **Task 2: AudioService pipeline integration and HostContext wiring** - `1ddd97c` (feat)

## Files Created/Modified
- `src/core/YamlConfig.hpp` - Added EqUserPreset struct, eqStreamPreset/eqUserPresets methods
- `src/core/YamlConfig.cpp` - EQ defaults in initDefaults, YAML read/write for EQ config
- `src/core/services/EqualizerService.hpp` - Added YamlConfig* constructor, saveNow, save timer, config persistence
- `src/core/services/EqualizerService.cpp` - loadFromConfig, scheduleSave, writeToConfig, saveNow implementations
- `src/core/services/AudioService.hpp` - eqEngine pointer in AudioStreamHandle
- `src/core/services/AudioService.cpp` - EQ process() call in onPlaybackProcess callback
- `src/core/plugin/IHostContext.hpp` - equalizerService() pure virtual
- `src/core/plugin/HostContext.hpp` - setEqualizerService/equalizerService implementation
- `src/core/aa/AndroidAutoOrchestrator.hpp` - EqualizerService* member and constructor param
- `src/core/aa/AndroidAutoOrchestrator.cpp` - EQ engine assignment to stream handles on session start
- `src/plugins/android_auto/AndroidAutoPlugin.cpp` - Pass eqService from HostContext to orchestrator
- `src/main.cpp` - Create EqualizerService, register in HostContext + QML, connect aboutToQuit
- `tests/test_yaml_config.cpp` - 5 new EQ config tests
- `tests/test_equalizer_service.cpp` - 7 new config-aware tests
- `tests/test_plugin_manager.cpp` - Mock IHostContext updated with equalizerService()
- `tests/test_plugin_model.cpp` - Mock IHostContext updated with equalizerService()

## Decisions Made
- 2-second debounce timer for config saves to avoid disk thrash during rapid slider adjustments
- saveNow() connected to aboutToQuit for guaranteed shutdown persistence
- EQ defaults: Media=Flat, Navigation=Voice, Phone=Voice (matches Plan 01 defaults)
- eqEngine is a non-owning raw pointer in AudioStreamHandle; EqualizerService owns the engines

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed ambiguous constructor call with nullptr**
- **Found during:** Task 1 (test compilation)
- **Issue:** `EqualizerService(nullptr)` ambiguous between `QObject*` and `YamlConfig*` constructors
- **Fix:** Test uses typed null pointer `YamlConfig* noConfig = nullptr; EqualizerService svc(noConfig);`
- **Files modified:** tests/test_equalizer_service.cpp
- **Committed in:** 24a39a6

**2. [Rule 1 - Bug] Fixed test using non-existent preset name "Bass"**
- **Found during:** Task 1 (test execution)
- **Issue:** Test expected preset "Bass" but bundled presets have "Bass Boost"
- **Fix:** Changed test to use "Bass Boost"
- **Files modified:** tests/test_equalizer_service.cpp
- **Committed in:** 24a39a6

**3. [Rule 3 - Blocking] Added AudioService.hpp include in AndroidAutoOrchestrator.cpp**
- **Found during:** Task 2 (compilation)
- **Issue:** AudioStreamHandle was forward-declared but eqEngine member access needs full definition
- **Fix:** Added `#include "../../core/services/AudioService.hpp"`
- **Files modified:** src/core/aa/AndroidAutoOrchestrator.cpp
- **Committed in:** 1ddd97c

---

**Total deviations:** 3 auto-fixed (2 bugs, 1 blocking)
**Impact on plan:** All fixes necessary for correctness. No scope creep.

## Issues Encountered
- Pre-existing `test_event_bus` failure (unrelated to EQ changes) - not in scope

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- EqualizerService fully wired: persists, processes audio, accessible from plugins and QML
- Phase 3 (Head Unit UI) can bind to EqualizerService QML context property directly
- Q_PROPERTY/Q_INVOKABLE API ready for QML slider controls

---
*Phase: 02-service-config*
*Completed: 2026-03-02*
