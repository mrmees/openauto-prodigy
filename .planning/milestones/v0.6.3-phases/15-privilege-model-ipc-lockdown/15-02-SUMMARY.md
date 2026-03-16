---
phase: 15-privilege-model-ipc-lockdown
plan: 02
subsystem: infra
tags: [installer, systemd, migration, group-management, iptables-cleanup]

requires: [15-01]
provides:
  - Installer integration for privilege model (group creation, template rendering, migration)
affects: []

tech-stack:
  added: []
  patterns: [sed-template-rendering, auto-migration-with-notice, openauto-group-management]

key-files:
  created: []
  modified:
    - install.sh
    - install-prebuilt.sh

key-decisions:
  - "Inline heredoc kept as fallback when template file not found -- defense-in-depth for edge cases"
  - "Migration is auto with notice, no interactive prompt -- consistent with installer's non-interactive philosophy"
  - "Duplicate migration logic in both installers rather than shared library -- standalone scripts, small code, template is the shared truth"

duration: 2min
completed: 2026-03-16
---

# Phase 15 Plan 02: Installer Integration Summary

**Both installers updated with openauto group management, old-to-root service migration with stale iptables cleanup, and sed-based template rendering from the Plan 01 .service.in file**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-16T01:46:40Z
- **Completed:** 2026-03-16T01:48:54Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Both install.sh and install-prebuilt.sh create the `openauto` group and add the installing user
- Old `User=$USER` units are detected and auto-migrated to `User=root` with informational notice
- Stale iptables OPENAUTO_PROXY chains are explicitly cleaned during migration (not reliant on Phase 16 self-heal)
- Systemd unit rendered from `system-service/openauto-system.service.in` template via sed substitution
- Session refresh warning printed honestly when group membership changes (no false claim of immediate connectivity)
- No `/run/openauto` directory prep in either installer -- daemon + systemd own runtime state
- Inline heredoc fallback with User=root and hardening directives retained for template-not-found edge case

## Task Commits

Each task was committed atomically:

1. **Task 1: Update install.sh with migration logic and template rendering** - `2b3fb9a` (feat)
2. **Task 2: Update install-prebuilt.sh with identical migration logic** - `aed3b76` (feat)

## Files Created/Modified
- `install.sh` - create_system_service() rewritten: group management, migration, template rendering, post-migration notices
- `install-prebuilt.sh` - Identical logic with adjusted template path resolution ($PAYLOAD_DIR for prebuilt layout)

## Decisions Made
- Inline heredoc kept as fallback when template file not found -- defense-in-depth for unusual deployment scenarios
- Migration is auto with notice, no interactive prompt -- consistent with installer's non-interactive philosophy
- Duplicate migration logic in both installers rather than shared library -- both are standalone scripts, the template file is the single source of truth for unit content

## Deviations from Plan

None - plan executed exactly as written.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Both installers produce the correct privileged service configuration
- End-to-end client connectivity verification requires manual post-install check (logout/login for group membership)
- Phase 15 complete -- ready for Phase 16 (routing correctness) or Phase 17 (status hardening)

---
*Phase: 15-privilege-model-ipc-lockdown*
*Completed: 2026-03-16*
