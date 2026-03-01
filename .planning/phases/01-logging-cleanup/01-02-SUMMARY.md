---
phase: 01-logging-cleanup
plan: 02
subsystem: infra
tags: [qt-logging, QLoggingCategory, ipc, web-panel, logging-migration]

requires:
  - phase: 01-logging-cleanup/01
    provides: "6 QLoggingCategory categories, custom message handler, CLI flags, oap::setVerbose/setDebugCategories"
provides:
  - "All ~250 raw log calls migrated to QLoggingCategory macros across 25 source files"
  - "IPC get_logging/set_logging commands for runtime logging control"
  - "Web panel logging toggle with verbose switch and per-category debug checkboxes"
  - "Logging settings persisted to config.yaml"
affects: [02-brightness, 03-hfp, 04-eq]

tech-stack:
  added: []
  patterns: [categorized-logging-everywhere, ipc-runtime-config]

key-files:
  created: []
  modified:
    - src/core/aa/AndroidAutoOrchestrator.cpp
    - src/core/aa/BluetoothDiscoveryService.cpp
    - src/core/aa/VideoDecoder.cpp
    - src/core/aa/EvdevTouchReader.cpp
    - src/core/aa/ServiceDiscoveryBuilder.cpp
    - src/core/aa/ThemeNightMode.cpp
    - src/core/aa/TimedNightMode.cpp
    - src/core/aa/GpioNightMode.cpp
    - src/core/aa/CodecCapability.cpp
    - src/core/aa/DmaBufVideoBuffer.cpp
    - src/core/aa/TouchHandler.hpp
    - src/core/services/BluetoothManager.cpp
    - src/core/services/AudioService.cpp
    - src/core/services/CompanionListenerService.cpp
    - src/core/services/SystemServiceClient.cpp
    - src/core/services/IpcServer.cpp
    - src/core/services/IpcServer.hpp
    - src/core/services/ActionRegistry.cpp
    - src/core/plugin/PluginManager.cpp
    - src/core/plugin/PluginLoader.cpp
    - src/core/plugin/PluginDiscovery.cpp
    - src/core/plugin/PluginManifest.cpp
    - src/core/plugin/HostContext.cpp
    - src/plugins/android_auto/AndroidAutoPlugin.cpp
    - src/ui/PluginRuntimeContext.cpp
    - src/ui/PluginViewHost.cpp
    - web-config/server.py
    - web-config/templates/settings.html

key-decisions:
  - "BtAudioPlugin and PhonePlugin use hostContext_->log() already -- no migration needed for those two files"
  - "Logging web controls are live (no form submit) -- changes take effect immediately via IPC"
  - "Per-category debug and verbose are mutually exclusive in the UI -- selecting categories clears verbose, enabling verbose clears categories"

patterns-established:
  - "All app logging uses qCDebug/qCInfo/qCWarning/qCCritical with one of 6 categories"
  - "IPC command pair pattern: get_X returns state, set_X applies + persists"

requirements-completed: [LOG-01, LOG-02]

duration: 25min
completed: 2026-03-01
---

# Phase 1 Plan 2: Log Migration & Runtime Toggle Summary

**Migrated ~250 raw log calls to QLoggingCategory macros across 25 files, added IPC get_logging/set_logging commands, and web panel with verbose toggle and per-category debug checkboxes**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-03-01
- **Completed:** 2026-03-01
- **Tasks:** 2
- **Files modified:** 28

## Accomplishments
- Zero raw qDebug()/qInfo()/qWarning()/qCritical() calls remain in src/ -- all use categorized macros
- IPC server handles get_logging (returns verbose state + categories) and set_logging (applies changes immediately)
- Web panel settings page has logging card with verbose toggle and aa/bt/audio/plugin/ui/core checkboxes
- Changes persist to config.yaml and take effect without app restart

## Task Commits

Each task was committed atomically:

1. **Task 1: Migrate all 25 source files to QLoggingCategory macros** - `2a16a4f` (feat)
2. **Task 2: Add IPC logging commands and web panel toggle** - `65f1592` (feat)

## Files Created/Modified
- `src/core/aa/AndroidAutoOrchestrator.cpp` - Largest migration (~30 calls), lcAA category
- `src/core/aa/VideoDecoder.cpp` - Video decoder logging, lcAA category
- `src/core/services/BluetoothManager.cpp` - BT manager + agent logging, lcBT category
- `src/core/services/AudioService.cpp` - Audio service logging, lcAudio category
- `src/core/plugin/PluginManager.cpp` - Plugin lifecycle logging, lcPlugin category
- `src/core/services/IpcServer.hpp` - Added handleGetLogging/handleSetLogging declarations
- `src/core/services/IpcServer.cpp` - Implemented get_logging/set_logging handlers + dispatch
- `web-config/server.py` - Added GET/POST /api/logging routes
- `web-config/templates/settings.html` - Added logging card with verbose toggle + category checkboxes
- (+ 19 other source files with log call migrations)

## Decisions Made
- BtAudioPlugin and PhonePlugin were not modified -- they use `hostContext_->log()` which routes through the already-migrated HostContext.cpp
- Web logging controls are live (immediate IPC calls on toggle change) rather than form-submit based
- Per-category debug and verbose are treated as mutually exclusive in the UI

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed double << operator in VideoDecoder.cpp**
- **Found during:** Task 1 (Build verification)
- **Issue:** sed migration of `qWarning() << "[VideoDecoder]" << selectedCodec->name` produced `qCWarning(lcAA) << << selectedCodec->name` (double `<<`)
- **Fix:** Removed the extra `<<` operator
- **Files modified:** src/core/aa/VideoDecoder.cpp
- **Verification:** Build succeeds
- **Committed in:** 2a16a4f (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Minor sed artifact, fixed inline. No scope creep.

## Issues Encountered
- Pre-existing test_event_bus failure (Qt 6.4 timing issue) -- not related to logging changes. 51/52 tests pass.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 1 (Logging Cleanup) is now fully complete
- All source files use categorized logging -- quiet by default, verbose via CLI/config/web panel
- Ready for Phase 2 (Brightness) and Phase 3 (HFP) which can proceed in parallel

---
*Phase: 01-logging-cleanup*
*Completed: 2026-03-01*

## Self-Check: PASSED
- All key files exist on disk
- Both task commits (2a16a4f, 65f1592) found in git log
- Zero raw qDebug()/qInfo()/qWarning()/qCritical() calls in src/
