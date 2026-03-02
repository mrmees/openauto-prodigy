---
phase: 01-service-foundation
plan: 02
subsystem: infra
tags: [systemd, sd_notify, watchdog, service-lifecycle, cmake]

# Dependency graph
requires:
  - phase: 01-service-foundation/01
    provides: "systemd service unit with Type=notify and WatchdogSec=30"
provides:
  - "sd_notify(READY=1) after full app initialization"
  - "sd_notify(WATCHDOG=1) periodic heartbeat on Qt main thread"
  - "sd_notify(STOPPING=1) on clean shutdown"
  - "Conditional libsystemd linkage via pkg-config"
affects: [03-app-lifecycle]

# Tech tracking
tech-stack:
  added: [libsystemd, sd-daemon.h]
  patterns: [ifdef-guarded-platform-feature, pkg-config-optional-dep]

key-files:
  created: []
  modified: [src/main.cpp, CMakeLists.txt, src/CMakeLists.txt]

key-decisions:
  - "libsystemd is optional (QUIET) — builds without it on non-systemd systems"
  - "Watchdog interval dynamically read from sd_watchdog_enabled with 15s fallback"
  - "All sd_notify code uses #ifdef HAS_SYSTEMD, matching existing HAS_BLUETOOTH pattern"

patterns-established:
  - "HAS_SYSTEMD compile definition: same pattern as HAS_BLUETOOTH for platform-optional features"

requirements-completed: [SVC-01, SVC-02]

# Metrics
duration: 3min
completed: 2026-03-02
---

# Phase 1 Plan 2: sd_notify Integration Summary

**systemd Type=notify readiness signal and watchdog heartbeat via libsystemd, conditionally compiled for dual-platform builds**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-02T16:16:54Z
- **Completed:** 2026-03-02T16:19:27Z
- **Tasks:** 1
- **Files modified:** 3

## Accomplishments
- App sends READY=1 after QML engine load and plugin initialization — systemd knows when app is truly ready
- Periodic WATCHDOG=1 on Qt main thread timer — if event loop freezes (deadlock), systemd detects and restarts
- STOPPING=1 on aboutToQuit — clean lifecycle signaling for dependent services
- Conditional build: libsystemd detected via pkg-config, HAS_SYSTEMD compile definition gates all code

## Task Commits

Each task was committed atomically:

1. **Task 1: Add libsystemd dependency and sd_notify integration** - `5fc1a42` (feat)

**Plan metadata:** TBD (docs: complete plan)

## Files Created/Modified
- `CMakeLists.txt` - Added pkg_check_modules for libsystemd (optional QUIET)
- `src/CMakeLists.txt` - Conditional HAS_SYSTEMD definition and PkgConfig::SYSTEMD linkage for openauto-core
- `src/main.cpp` - sd_notify(READY=1), watchdog QTimer with dynamic interval, STOPPING=1 on aboutToQuit

## Decisions Made
- libsystemd is optional (QUIET flag) so the project still builds on non-systemd dev environments
- Watchdog interval dynamically reads from sd_watchdog_enabled() — adapts if service unit changes WatchdogSec
- Follows existing HAS_BLUETOOTH ifdef pattern for consistency

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- libsystemd-dev was not installed on dev VM — installed via apt (not a code issue, just dev environment setup)
- Pre-existing test_event_bus failure unrelated to changes (EventBus testSubscribeAndPublish assertion)

## User Setup Required

None - no external service configuration required. libsystemd-dev must be available at build time (already in install.sh deps).

## Next Phase Readiness
- sd_notify integration complete, pairs with the Type=notify service unit from Plan 01
- Watchdog heartbeat ensures systemd can detect and recover from app hangs
- Ready for Phase 2 (Bluetooth Hardening) or Phase 3 (App Lifecycle)

---
*Phase: 01-service-foundation*
*Completed: 2026-03-02*
