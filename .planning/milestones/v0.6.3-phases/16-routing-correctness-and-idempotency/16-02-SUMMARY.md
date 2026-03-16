---
phase: 16-routing-correctness-and-idempotency
plan: 02
subsystem: infra
tags: [iptables, redsocks, proxy, idempotent, systemd]

# Dependency graph
requires:
  - phase: 16-01
    provides: "Hardware-verified Phase 15 privilege model (root daemon, 0660 socket)"
provides:
  - "Idempotent flush/recreate iptables routing with owner+destination exemptions"
  - "Startup self-heal via cleanup_stale_state() before IPC"
  - "Honest disable() that reports FAILED on partial cleanup failure"
  - "Dedicated redsocks user spawning via sudo -u"
affects: [16-03, 16-04, 17-status-hardening]

# Tech tracking
tech-stack:
  added: []
  patterns: ["flush/recreate iptables chain model", "owner-based UID exemption", "cleanup failure honesty"]

key-files:
  created: []
  modified:
    - system-service/proxy_manager.py
    - system-service/openauto_system.py

key-decisions:
  - "Redsocks spawned under dedicated user via sudo -u for owner-based iptables exemption"
  - "Flush/recreate model always rebuilds chain (never check-and-reuse)"
  - "disable() sets FAILED on any cleanup step failure, DISABLED only on full success"

patterns-established:
  - "Flush/recreate: loop-delete all OUTPUT jumps, flush+delete chain, create fresh, populate, insert one jump"
  - "Exemption order: owner -> upstream dest (host+port) -> networks -> interfaces -> REDIRECT"
  - "Startup self-heal: cleanup_stale_state() runs before ipc.start() unconditionally"

requirements-completed: [PX-01, PX-02, PX-03, PX-04, IC-01, IC-02, IC-03, IC-04]

# Metrics
duration: 2min
completed: 2026-03-16
---

# Phase 16 Plan 02: Routing Correctness Summary

**Idempotent flush/recreate iptables routing with owner+destination exemptions, cleanup failure honesty, and startup self-heal**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-16T03:22:03Z
- **Completed:** 2026-03-16T03:24:21Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- ProxyManager rewritten with deterministic flush/recreate iptables chain model (never check-and-reuse)
- Owner-based exemption (-m owner --uid-owner redsocks) and upstream SOCKS destination exemption (host+port)
- disable() honestly reports FAILED on partial cleanup failure instead of lying with DISABLED
- cleanup_stale_state() wired into main() before ipc.start() for startup self-heal
- Removed port-based exemptions (superseded by proper owner/destination exemptions)

## Task Commits

Each task was committed atomically:

1. **Task 1: Rewrite ProxyManager with flush/recreate routing, exemptions, and cleanup honesty** - `6db7bbd` (feat)
2. **Task 2: Wire startup self-heal and update IPC handler response** - `f826094` (feat)

## Files Created/Modified
- `system-service/proxy_manager.py` - Flush/recreate iptables, owner+destination exemptions, cleanup honesty, startup self-heal method
- `system-service/openauto_system.py` - Startup cleanup call, removed skip_ports, state-aware IPC handler

## Decisions Made
- Redsocks spawned via `sudo -u redsocks` (cleanest approach for dedicated user without password)
- Exemption rule order: owner first, then upstream destination, then networks, then interfaces, then REDIRECT
- skip_ports removed entirely from both ProxyManager and IPC request parsing

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Routing correctness complete, ready for Plan 03 (installer creates redsocks user) and Plan 04 (test updates)
- Existing tests in test_proxy_manager.py will need updating for removed skip_ports and new exemption rules (Plan 04)

---
*Phase: 16-routing-correctness-and-idempotency*
*Completed: 2026-03-16*

## Self-Check: PASSED
