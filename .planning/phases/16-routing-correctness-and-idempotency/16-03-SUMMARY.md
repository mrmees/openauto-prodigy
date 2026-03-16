---
phase: 16-routing-correctness-and-idempotency
plan: 03
subsystem: infra
tags: [installer, redsocks, iptables, system-user, idempotent]

requires:
  - phase: 16-01
    provides: hardware verification gate confirming Pi environment
provides:
  - redsocks system user created by both installers
  - prerequisite for owner-based iptables exemption in ProxyManager
affects: [16-02]

tech-stack:
  added: []
  patterns: [idempotent-system-user-creation]

key-files:
  created: []
  modified: [install.sh, install-prebuilt.sh]

key-decisions:
  - "Identical user creation block in both installers per Phase 15 duplication decision"

patterns-established:
  - "System user creation: id -u check before useradd for idempotency"

requirements-completed: [PX-01]

duration: 1min
completed: 2026-03-16
---

# Phase 16 Plan 03: Redsocks System User Summary

**Idempotent redsocks system user creation in both installers for owner-based iptables proxy exemption**

## Performance

- **Duration:** 1 min
- **Started:** 2026-03-16T03:22:16Z
- **Completed:** 2026-03-16T03:23:00Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Both installers create `redsocks` system user with --system --no-create-home --shell /usr/sbin/nologin
- Idempotent creation via `id -u redsocks` check before `useradd`
- Placed logically after openauto group management in both scripts

## Task Commits

Each task was committed atomically:

1. **Task 1: Add redsocks system user to install.sh** - `244de12` (feat)
2. **Task 2: Add redsocks system user to install-prebuilt.sh** - `6009920` (feat)

## Files Created/Modified
- `install.sh` - Added redsocks system user creation after openauto group setup
- `install-prebuilt.sh` - Added identical redsocks system user creation block

## Decisions Made
- Duplicated identical code block in both installers per Phase 15 decision (standalone scripts, no shared library)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- redsocks user prerequisite satisfied for Plan 16-02 ProxyManager owner-based exemption
- Both installers ready for Pi deployment

---
*Phase: 16-routing-correctness-and-idempotency*
*Completed: 2026-03-16*
