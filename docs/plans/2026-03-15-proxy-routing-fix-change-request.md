# Prodigy Change Request: Fix Transparent Proxy Routing

Use this file as the prompt/context for a fresh Claude session in the Prodigy repo running GSD.

## Issue

Fix Prodigy transparent proxy routing so `set_proxy_route` actually works on hardware, does not self-intercept, and does not open a local privilege hole.

## Background

On March 15, 2026, a real hardware bridge validation run from the companion side proved the phone app is doing the right thing:

- `BRDG-01` passed: the companion persisted `socks5_enabled` per vehicle and reapplied it correctly across reconnects.
- The remaining failure is in Prodigy: `set_proxy_route` stays disabled and returns `iptables ... Permission denied (you must be root)`.
- On the Pi, `get_proxy_status` stayed `{"state":"disabled"}`, NAT rules stayed empty, and traffic continued to egress over `eth0` instead of the phone bridge.

This is now a Prodigy-side routing/privilege bug, not a companion bug.

## Primary Goal

Make Prodigy's `set_proxy_route` path actually apply and maintain transparent SOCKS routing on the Pi for representative local TCP traffic, while keeping the service secure and its reported state truthful.

## Observed Failure

Current hardware result:

- `set_proxy_route` reports disabled with an iptables privilege error
- no NAT rules are installed
- Pi traffic stays on normal `eth0` egress
- companion-side SOCKS server is up and credentials are valid

## Relevant Files

- `system-service/openauto_system.py`
- `system-service/proxy_manager.py`
- `system-service/ipc_server.py`
- `system-service/openauto-system.service`
- `install.sh`
- `install-prebuilt.sh`
- `system-service/health_monitor.py`
- `src/core/services/SystemServiceClient.cpp`

## Root Problems To Fix

### 1. The system daemon is underprivileged for the work it performs.

- The generated service runs as `User=$USER` / `matt`.
- That same daemon invokes `iptables`, `systemctl restart`, rfkill sysfs writes, Bluetooth bring-up, and SDP permission changes.
- This is fundamentally inconsistent. The `iptables Permission denied` error is the direct consequence.

### 2. If we simply run the daemon as root, the IPC socket becomes dangerous.

- `/run/openauto/system.sock` is created world-writable (`0666`).
- That socket exposes privileged methods including `set_proxy_route` and service restart/config actions.
- If the daemon becomes privileged without tightening socket access, any local user can drive privileged system actions.

### 3. Proxy routing is not safely scoped.

- The current rules redirect broad local `OUTPUT` TCP traffic.
- There is no explicit exemption for the upstream SOCKS endpoint.
- There is no exemption for the `redsocks` process/user itself.
- This risks self-interception or redirect loops.

### 4. Rule application/cleanup is not idempotent or truthful.

- Existing `OPENAUTO_PROXY` chains are tolerated, then rules are appended again.
- Duplicate `OUTPUT -> OPENAUTO_PROXY` jumps can accumulate.
- Flush/delete failures are only logged.
- The daemon can still report `disabled` or a benign state while stale rules remain.

### 5. Health/status reporting is too optimistic.

- `ACTIVE` is set after a short sleep, not after proving the local redirect path is actually working.
- The health probe checks upstream reachability, not end-to-end local redirect correctness.
- A broken local pipeline can still look healthy.

## Required Changes

### A. Fix the privilege model

- Make `openauto-system.service` the privileged system daemon, or split privileged operations into a dedicated root helper.
- Do not leave the current design as "app user service that shells out to root-only tools."
- If keeping one daemon, make the unit explicitly privileged enough for:
  - iptables/nftables changes
  - service restarts
  - rfkill/sysfs writes
  - Bluetooth recovery actions
  - SDP permission adjustments
- Update both installers and the checked-in reference unit consistently.

### B. Lock down IPC before or together with privilege changes

- Change `/run/openauto/system.sock` from `0666` to a restricted mode like `0660`.
- Use a dedicated group if needed so the UI/client process can still connect.
- Prefer validating peer credentials on the Unix socket before allowing privileged methods.
- Ensure the Qt client still connects successfully after permissions are tightened.

### C. Make proxy application idempotent

- Rework proxy rule install so repeated `set_proxy_route(active=true)` calls do not stack duplicate rules.
- On enable:
  - remove or verify existing `OUTPUT` jump before inserting
  - flush/recreate the custom chain cleanly, or use checked insertion logic
- On disable:
  - remove all matching `OUTPUT` jumps, not just one
  - flush/delete the chain fully
  - restore any DNS changes too, not just iptables state
- If cleanup fails, surface that honestly in returned state/error.

### D. Prevent self-interception

- Always exempt the upstream SOCKS destination from redirect.
- Also exempt redsocks traffic by owner/cgroup or another reliable mechanism.
- Review default exclusions so local AP/LAN/control-plane traffic is not accidentally redirected.
- The current defaults are too weak for safe transparent proxying.

### E. Make reported state reflect reality

- Do not mark route `ACTIVE` just because redsocks started and a sleep elapsed.
- Verify the local redirect listener/process is alive.
- Prefer an end-to-end probe of the actual local redirection path.
- Use `FAILED`/`DEGRADED` meaningfully for startup, apply, or cleanup failures.

### F. Add tests for the real failure modes

Add or extend tests for:

- permission-denied iptables failure
- existing `OPENAUTO_PROXY` chain before enable
- repeated enable without duplicate `OUTPUT` jumps
- cleanup failure keeping state/error truthful
- upstream proxy destination exemption
- redsocks self-exemption
- privileged daemon + restricted socket permissions still allowing intended client access

## Concrete Implementation Expectations

### 1. Service/installer

Update:

- `system-service/openauto-system.service`
- `install.sh`
- `install-prebuilt.sh`

Ensure the installed unit and repo reference file do not drift.

### 2. IPC security

Update:

- `system-service/ipc_server.py`

Restrict socket permissions and add caller validation if practical.

### 3. Proxy pipeline

Update:

- `system-service/proxy_manager.py`
- `system-service/openauto_system.py`

Make apply/disable/idempotency/state reporting robust.

### 4. Health/recovery

Review and adjust:

- `system-service/health_monitor.py`

Keep runtime permissions policy consistent with the new service model.

## Acceptance Criteria

1. On real hardware, `set_proxy_route(active=true, ...)` succeeds without privilege errors.
2. `get_proxy_status` reports a truthful active state only when routing is actually applied.
3. The Pi installs the expected redirect rules and representative local TCP traffic traverses the companion SOCKS bridge.
4. Repeated enable/disable cycles do not accumulate duplicate rules.
5. Disable fully removes routing state and restores DNS/other side effects.
6. The upstream SOCKS connection is not intercepted by Prodigy's own redirect rules.
7. The system socket is no longer world-writable if the daemon is privileged.
8. Unit/integration tests cover the permission, idempotency, and self-interception cases.

## Non-Goals

- Do not change companion behavior in the phone repo for this fix.
- Do not paper over the failure by changing UI/state strings only.
- Do not broaden local privilege exposure just to make iptables work.

## Suggested Validation After Implementation

- Run the Python test suite for `system-service`
- Reinstall/restart `openauto-system`
- Call `set_proxy_route` manually and confirm:
  - no permission error
  - expected nat rules exist
  - `get_proxy_status` is accurate
- Run the same real hardware bridge validation again with active Android Auto session and confirm the Pi actually routes through the phone bridge

## Deliverable

Please implement this as a Prodigy-side routing/security fix, not as a companion workaround. The companion evidence already says the phone side is clean for this phase.
