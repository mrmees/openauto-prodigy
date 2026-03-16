---
phase: 17-status-reporting-hardening
plan: 01
subsystem: system-service
tags: [asyncio, iptables, proxy, health-check, ipc]

requires:
  - phase: 16-routing-correctness-and-idempotency
    provides: flush/recreate iptables model, owner-based exemptions, ProxyManager enable/disable
provides:
  - ProxyManager with injectable verification (listener, routing, upstream)
  - Lock-guarded proxy operations preventing race conditions
  - Honest status reporting (ACTIVE requires verified local pipeline)
  - Full status response shape (state/checks/error_code/error/verified_at/live_check)
  - Redesigned 10s health loop with immediate local failure detection
affects: [18-hardware-validation, companion-app]

tech-stack:
  added: []
  patterns: [_enable_locked/_disable_locked deadlock-avoidance, injectable verification seam, TOCTOU-acceptable lock skip]

key-files:
  created: []
  modified:
    - system-service/proxy_manager.py
    - system-service/openauto_system.py
    - system-service/tests/test_proxy_manager.py
    - system-service/tests/test_openauto_system.py

key-decisions:
  - "TCP connect is the listener truth gate, not PID"
  - "Local failure = immediate FAILED (no threshold), upstream failure = DEGRADED"
  - "_enable_locked/_disable_locked pattern avoids self-deadlock on enable->disable rollback"
  - "Exception error field takes priority over status error field via spread ordering"

patterns-established:
  - "_make_verify_all_mock() helper for testing verification-dependent code"
  - "Handler extraction pattern via register_method spy for IPC handler tests"

requirements-completed: [SR-01, SR-02, SR-03]

duration: 6min
completed: 2026-03-16
---

# Phase 17 Plan 01: Status Reporting Hardening Summary

**ProxyManager with verified ACTIVE gate, lock-guarded operations, 10s health loop, and full status response shape through IPC**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-16T04:35:48Z
- **Completed:** 2026-03-16T04:41:26Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- ACTIVE state now requires verified local pipeline (TCP listener check + iptables routing check)
- Single asyncio.Lock guards all state-mutating operations with _enable_locked/_disable_locked deadlock avoidance
- Health loop redesigned: 10s interval, immediate FAILED on local failure, DEGRADED on upstream-only failure, recovery detection
- IPC handlers return full response shape (state/checks/error_code/error/verified_at/live_check) on all paths including errors

## Task Commits

Each task was committed atomically:

1. **Task 1: Add verification logic, lock, and redesigned health loop** - `9012c5a` (feat)
2. **Task 2: Update IPC handler to return full status response** - `0d67e0e` (feat)

## Files Created/Modified
- `system-service/proxy_manager.py` - Added _verify_listener, _verify_routing, _verify_upstream, _verify_all, get_status(), asyncio.Lock, redesigned _health_loop
- `system-service/openauto_system.py` - Updated handle_get_proxy_status and handle_set_proxy_route to return full status shape
- `system-service/tests/test_proxy_manager.py` - 51 tests: added verification, status shape, health transition, lock tests; adapted old tests for removed _probe_once/_fail_count
- `system-service/tests/test_openauto_system.py` - 24 tests: added 10 IPC handler tests with handler extraction pattern

## Decisions Made
- TCP connect as listener truth gate (not PID) — per CONTEXT.md decision
- _enable_locked/_disable_locked pattern to avoid self-deadlock when enable() calls disable() for rollback/reconfigure
- Exception error field takes priority over status error field by placing `"error": str(e)` after `**status` spread
- TOCTOU-acceptable lock skip in health loop — worst case is a brief block, not incorrect state

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed dict spread ordering in exception handler**
- **Found during:** Task 2 (IPC handler update)
- **Issue:** `{"ok": False, "error": str(e), **status}` let status's null error overwrite the exception message
- **Fix:** Changed to `{**status, "ok": False, "error": str(e)}` so exception error takes priority
- **Files modified:** system-service/openauto_system.py
- **Verification:** test_set_proxy_route_exception_returns_full_shape passes
- **Committed in:** 0d67e0e (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Essential fix for correct error reporting. No scope creep.

## Issues Encountered
None beyond the dict spread ordering issue (caught by tests).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Status reporting hardened — ACTIVE means verified local pipeline
- Full response shape available to C++ client (additive, backward-compatible)
- Ready for Phase 18 hardware validation

---
*Phase: 17-status-reporting-hardening*
*Completed: 2026-03-16*
