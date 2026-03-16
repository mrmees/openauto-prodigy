---
phase: 16-routing-correctness-and-idempotency
plan: 04
subsystem: testing
tags: [pytest, iptables, proxy, idempotent, mocking]

# Dependency graph
requires:
  - phase: 16-02
    provides: "Flush/recreate iptables model, owner+destination exemptions, cleanup honesty"
provides:
  - "28 proxy manager tests covering routing correctness, idempotency, exemptions, cleanup honesty"
  - "15 system tests covering request parsing defaults and startup self-heal ordering"
affects: [17-status-hardening]

# Tech tracking
tech-stack:
  added: []
  patterns: ["flush-aware mock pattern for while-loop iptables delete", "call-order tracking for startup sequencing"]

key-files:
  created: []
  modified:
    - system-service/tests/test_proxy_manager.py
    - system-service/tests/test_openauto_system.py

key-decisions:
  - "Flush-aware mock helper (_make_flush_aware_mock) as reusable pattern for all tests touching disable/enable"
  - "Call-order tracking via plain list for startup sequencing test (simpler than mock.call_args_list)"

patterns-established:
  - "Flush-aware mocking: side_effect that returns (1, ...) for -D OUTPUT after N successes to break while loop"
  - "Exemption ordering assertion: extract -A OPENAUTO_PROXY rules, verify index ordering"

requirements-completed: [TS-01, TS-02, TS-03, TS-04]

# Metrics
duration: 4min
completed: 2026-03-16
---

# Phase 16 Plan 04: Test Coverage for Routing Correctness Summary

**43 tests covering flush/recreate idempotency, owner/destination exemptions, cleanup failure honesty, and startup self-heal ordering**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-16T03:41:07Z
- **Completed:** 2026-03-16T03:44:59Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Fixed all 14 existing proxy manager tests that hung due to infinite while-loop in flush/recreate model (flush-aware mock pattern)
- Added 14 new proxy manager tests: TS-01 permission-denied, TS-02 idempotency (pre-existing chains + 5x repeated enable), TS-03 exemptions (owner, destination, networks, interfaces, ordering), TS-04 cleanup honesty, cleanup_stale_state coverage
- Updated 3 system route validation tests for removed skip_tcp_ports and new default networks/interfaces
- Added startup self-heal ordering test verifying cleanup_stale_state() before ipc.start()

## Task Commits

Each task was committed atomically:

1. **Task 1: Update existing tests and add routing correctness + idempotency tests** - `c82b97e` (test)
2. **Task 2: Add startup self-heal ordering test and update request parsing test** - `cd09ee8` (test)

## Files Created/Modified
- `system-service/tests/test_proxy_manager.py` - 28 tests: routing correctness, idempotency, exemptions, cleanup honesty, stale state cleanup
- `system-service/tests/test_openauto_system.py` - 15 tests: request parsing with updated defaults, startup ordering, shutdown timeouts

## Decisions Made
- Created `_make_flush_aware_mock()` helper to handle the while-loop delete pattern across all tests
- Used plain list call-order tracking for startup sequencing test instead of complex mock introspection

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- Existing tests all hung due to `_run_cmd = AsyncMock(return_value=(0, ""))` causing infinite while-loop in `_iptables_flush()` -- resolved by flush-aware mock pattern that returns failure for -D OUTPUT after N successes

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All TS-* requirement tests pass, routing correctness fully verified
- Phase 16 complete (all 4 plans done)

---
*Phase: 16-routing-correctness-and-idempotency*
*Completed: 2026-03-16*

## Self-Check: PASSED
