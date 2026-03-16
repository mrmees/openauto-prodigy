---
phase: 15-privilege-model-ipc-lockdown
plan: 01
subsystem: infra
tags: [systemd, unix-socket, peer-credentials, SO_PEERCRED, ipc, security]

requires: []
provides:
  - Service template (openauto-system.service.in) with User=root and hardening directives
  - IPC socket lockdown (0660 root:openauto) with injectable peer credential authorization
  - Authorization policy function (check_peer_authorized) with DI seam for testing
affects: [15-02-installer-integration]

tech-stack:
  added: []
  patterns: [injectable-authorizer-callable, SO_PEERCRED-peer-credential-validation, service-template-with-placeholders]

key-files:
  created:
    - system-service/openauto-system.service.in
    - system-service/tests/test_ipc_auth.py
  modified:
    - system-service/ipc_server.py
    - system-service/tests/test_ipc_server.py

key-decisions:
  - "Injectable authorizer callable instead of auth_enabled toggle — no 'disable security' flag in production"
  - "Module-level check_peer_authorized() for direct testability via mock patching"
  - "PR-03 proven at policy/permission-model level; end-to-end client connectivity deferred to manual post-install verification"

patterns-established:
  - "Authorizer DI: IpcServer accepts authorizer callable, tests inject stubs, production uses SO_PEERCRED"
  - "Service template: .service.in with @@PLACEHOLDER@@ syntax, install.sh does substitution"

requirements-completed: [PR-01, PR-02, PR-03, TS-05]

duration: 2min
completed: 2026-03-16
---

# Phase 15 Plan 01: Privilege Model & IPC Lockdown Summary

**Root-privileged systemd service template with SO_PEERCRED socket lockdown and injectable authorization policy proven by 10 mocked-identity tests**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-16T01:41:46Z
- **Completed:** 2026-03-16T01:43:50Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Service template with User=root, minimal hardening (PrivateTmp, ProtectHome, etc.), and @@PLACEHOLDER@@ syntax for install-time substitution
- IPC socket permissions tightened from 0666 to 0660 with root:openauto ownership
- Peer credential validation via SO_PEERCRED with injectable authorizer callable (no auth_enabled toggle)
- 10 authorization tests proving the complete policy via mocked identities: root, openauto member, primary GID match, non-member, missing group

## Task Commits

Each task was committed atomically:

1. **Task 1: Create service template, delete old .service file, and add injectable peer credential auth** - `57a2498` (feat)
2. **Task 2: Write IPC authorization tests proving the policy via dependency injection** - `796324a` (test)

## Files Created/Modified
- `system-service/openauto-system.service.in` - Canonical service template with User=root, hardening, placeholders
- `system-service/ipc_server.py` - Added check_peer_authorized(), injectable authorizer, 0660 socket permissions
- `system-service/tests/test_ipc_auth.py` - 10 tests covering socket permissions, authorization integration, and policy unit tests
- `system-service/tests/test_ipc_server.py` - Updated to use permissive authorizer injection, 0660 assertion

## Decisions Made
- Injectable authorizer callable instead of auth_enabled toggle — avoids "disable security" flag in production code
- Module-level check_peer_authorized() function rather than method — enables direct mock patching in tests
- PR-03 proven at policy/permission-model level; end-to-end client connectivity (Qt client after usermod + session refresh) deferred to manual post-install verification

## Deviations from Plan

None - plan executed exactly as written.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Service template ready for install.sh integration (Plan 15-02)
- IPC server ready for privileged method registration (iptables, rfkill, systemctl)
- Authorization policy proven — subsequent plans can add privileged methods knowing the security foundation is solid

---
*Phase: 15-privilege-model-ipc-lockdown*
*Completed: 2026-03-16*
