# Requirements — v0.6.3 Proxy Routing Fix

**Defined:** 2026-03-16
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.
**Source:** `docs/plans/2026-03-15-proxy-routing-fix-change-request.md`

## Privilege Model (PR)

- [x] **PR-01**: `openauto-system` uses a privilege model that is sufficient for all system-level operations it performs, including proxy routing, service restarts, rfkill/sysfs writes, and required permission fixes
- [x] **PR-02**: The IPC socket is restricted from world-writable access and only intended local clients can call privileged methods
- [x] **PR-03**: The intended Qt client can still connect successfully after IPC permissions are tightened
- [x] **PR-04**: Installers and the checked-in reference unit are updated consistently so deployed behavior matches repo state

## Proxy Routing Correctness (PX)

- [x] **PX-01**: `set_proxy_route(active=true)` succeeds without privilege errors on real hardware
- [x] **PX-02**: The expected redirect rules are installed for the transparent proxy path
- [x] **PX-03**: The upstream SOCKS destination is exempt from redirect
- [x] **PX-04**: Redsocks/control-plane traffic is exempt from redirect so the proxy path does not self-intercept or loop
- [ ] **PX-05**: Representative outbound TCP traffic traverses the companion SOCKS bridge on hardware once routing is enabled (hardware/UAT acceptance)

## Idempotency & Cleanup (IC)

- [x] **IC-01**: Repeated enable/disable cycles do not accumulate duplicate redirect rules or duplicate output jumps
- [x] **IC-02**: Disable removes routing state completely, including redirect rules and any DNS mutations introduced by enable
- [x] **IC-03**: Abnormal stop/restart does not leave persistent routing drift, or startup self-heals that state before reporting success
- [x] **IC-04**: Apply and cleanup failures surface honestly in returned state/error; the daemon must not report `disabled` or `active` when teardown/setup actually failed

## Status Reporting (SR)

- [x] **SR-01**: `get_proxy_status` reports `ACTIVE` only after the local redirect pipeline is verified alive
- [x] **SR-02**: `FAILED` and `DEGRADED` are used meaningfully for startup, apply, runtime, and cleanup failures
- [x] **SR-03**: Health/status checks reflect local proxy-pipeline health, not merely upstream SOCKS reachability

## Testing (TS)

- [x] **TS-01**: Tests cover permission-denied proxy-route setup failures
- [x] **TS-02**: Tests cover pre-existing chains/jumps and repeated enable without duplication
- [x] **TS-03**: Tests cover upstream destination exemption and redsocks/control-plane self-exemption
- [x] **TS-04**: Tests cover cleanup failure truthfulness
- [x] **TS-05**: Tests cover restricted IPC permissions while preserving access for the intended client

## Out of Scope

| Feature | Reason |
|---------|--------|
| Companion-side changes | Phone side verified clean per change request |
| UI/state string cosmetics | Fix the actual routing, not the labels |
| Broader local privilege expansion | Tighten, don't broaden |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| PR-01 | Phase 15 | Complete |
| PR-02 | Phase 15 | Complete |
| PR-03 | Phase 15 | Complete |
| PR-04 | Phase 15 | Complete |
| PX-01 | Phase 16 | Complete |
| PX-02 | Phase 16 | Complete |
| PX-03 | Phase 16 | Complete |
| PX-04 | Phase 16 | Complete |
| PX-05 | Phase 18 | Pending |
| IC-01 | Phase 16 | Complete |
| IC-02 | Phase 16 | Complete |
| IC-03 | Phase 16 | Complete |
| IC-04 | Phase 16 | Complete |
| SR-01 | Phase 17 | Complete |
| SR-02 | Phase 17 | Complete |
| SR-03 | Phase 17 | Complete |
| TS-01 | Phase 16 | Complete |
| TS-02 | Phase 16 | Complete |
| TS-03 | Phase 16 | Complete |
| TS-04 | Phase 16 | Complete |
| TS-05 | Phase 15 | Complete |

**Coverage:**
- v0.6.3 requirements: 21 total
- Mapped to phases: 21
- Unmapped: 0

---
*Requirements defined: 2026-03-16*
