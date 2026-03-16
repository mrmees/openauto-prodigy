---
phase: 16-routing-correctness-and-idempotency
plan: 01
subsystem: infra
tags: [privilege-model, ipc, hardware-verification, systemd]

# Dependency graph
requires:
  - phase: 15-privilege-model-ipc-lockdown
    provides: root daemon, 0660 socket, peer credential auth
provides:
  - Confirmed Phase 15 privilege model works on real Pi hardware
  - ProtectHome regression identified and fixed
affects: [16-02, 16-03, 16-04]

# Tech tracking
tech-stack:
  added: []
  patterns: []

key-files:
  created: []
  modified:
    - web-config/openauto-system.service.template
    - install.sh

key-decisions:
  - "ProtectHome=yes removed from service template — blocks /home access needed by the daemon"

patterns-established: []

requirements-completed: []

# Metrics
duration: checkpoint (human-verified)
completed: 2026-03-16
---

# Phase 16 Plan 01: Phase 15 Hardware Verification Gate Summary

**Phase 15 privilege model confirmed working on Pi hardware: root daemon, 0660 socket, positive/negative IPC auth all verified; one regression (ProtectHome=yes) found and fixed**

## Performance

- **Duration:** Checkpoint (human-verified on hardware)
- **Tasks:** 1 (hardware verification gate)
- **Files modified:** 2 (regression fix)

## Accomplishments
- Verified openauto-system running as root on Pi hardware
- Confirmed socket permissions root:openauto 660
- Confirmed no permission-denied errors in journal
- Positive IPC auth verified: matt (openauto group) gets health response
- Negative IPC auth verified: nobody user gets Permission denied at filesystem level
- Qt client connected cleanly
- Found and fixed Phase 15 regression: ProtectHome=yes blocking /home access

## Task Commits

1. **Task 1: Phase 15 hardware verification gate** - checkpoint (human-verified)
   - Regression fix commits: `f4aba3f` (remove ProtectHome from template), `6cd79d2` (remove from installer fallback units)

## Files Created/Modified
- `web-config/openauto-system.service.template` - Removed ProtectHome=yes (f4aba3f)
- `install.sh` - Removed ProtectHome=yes from fallback unit (6cd79d2)

## Decisions Made
- ProtectHome=yes must be removed from service template — it blocks the daemon from accessing /home where user config and the app itself reside

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] ProtectHome=yes in service template blocked /home access**
- **Found during:** Task 1 (hardware verification)
- **Issue:** Phase 15 service template included `ProtectHome=yes` which prevents the daemon from accessing /home, breaking normal operation
- **Fix:** Removed ProtectHome=yes from both the service template and installer fallback unit
- **Files modified:** web-config/openauto-system.service.template, install.sh
- **Verification:** Service starts and operates normally after fix
- **Committed in:** f4aba3f, 6cd79d2

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Essential fix for correct operation. No scope creep.

## Issues Encountered
None beyond the ProtectHome regression documented above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 15 privilege model confirmed working on hardware
- Ready to proceed with Phase 16 Plan 02: ProxyManager routing rewrite
- All success criteria met: root daemon, restricted socket, positive/negative auth, clean Qt client connection

---
*Phase: 16-routing-correctness-and-idempotency*
*Completed: 2026-03-16*
