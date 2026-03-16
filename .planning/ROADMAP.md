# Roadmap: OpenAuto Prodigy

## Milestones

<details>
<summary>v0.4 Logging & Theming (Phases 1-2) - SHIPPED 2015-03-01</summary>

See .planning/milestones/v0.4/ for archived details.

</details>

<details>
<summary>v0.4.1 Audio Equalizer (Phases 1-3) - SHIPPED 2015-03-02</summary>

See .planning/milestones/v0.4.1/ for archived details.

</details>

<details>
<summary>v0.4.2 Service Hardening (Phases 1-3) - SHIPPED 2015-03-02</summary>

See .planning/milestones/v0.4.2/ for archived details.

</details>

<details>
<summary>v0.4.3 Interface Polish & Settings Reorganization (Phases 1-4) - SHIPPED 2015-03-03</summary>

See .planning/milestones/v0.4.3/ for archived details.

</details>

<details>
<summary>v0.4.4 Scalable UI (Phases 1-5) - SHIPPED 2015-03-03</summary>

See .planning/milestones/v0.4.4/ for archived details.

</details>

<details>
<summary>v0.4.5 Navbar Rework (Phases 1-4) - SHIPPED 2015-03-05</summary>

See .planning/milestones/v0.4.5/ for archived details.

</details>

<details>
<summary>v0.5.0 Protocol Compliance (Phases 1-4) - SHIPPED 2015-03-08</summary>

See .planning/milestones/v0.5.0-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.1 DPI Sizing & UI Polish (Phases 1-4 + insertions) - SHIPPED 2015-03-10</summary>

See .planning/milestones/v0.5.1-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.2 Widget System & UI Polish (Phases 1-3) - SHIPPED 2015-03-11</summary>

See .planning/milestones/v0.5.2-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.5.3 Widget Grid & Content Widgets (Phases 04-08) - SHIPPED 2015-03-13</summary>

See .planning/milestones/v0.5.3-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.1 Widget Framework & Layout Refinement (Phases 09-12) - SHIPPED 2015-03-15</summary>

See .planning/milestones/v0.6-ROADMAP.md for archived details.

</details>

<details>
<summary>v0.6.2 Theme Expression & Wallpaper Scaling (Phases 13-14.1) - SHIPPED 2026-03-16</summary>

See .planning/milestones/v0.6.2-ROADMAP.md for archived details.

</details>

## v0.6.3 Proxy Routing Fix (In Progress)

**Milestone Goal:** Fix the system service privilege model, IPC security, and transparent proxy routing so `set_proxy_route` actually works on hardware without privilege errors or self-interception.

### Phases

- [x] **Phase 15: Privilege Model & IPC Lockdown** - System daemon runs privileged with restricted IPC socket (completed 2026-03-16)
- [x] **Phase 16: Routing Correctness & Idempotency** - Proxy rules apply cleanly with self-exemption and no duplication (completed 2026-03-16)
- [x] **Phase 17: Status Reporting Hardening** - Proxy state reports reflect actual pipeline health (completed 2026-03-16)
- [ ] **Phase 18: Hardware Validation** - End-to-end proxy routing verified on real Pi hardware

### Phase Details

### Phase 15: Privilege Model & IPC Lockdown
**Goal**: The system daemon has sufficient privilege for all system-level operations and the IPC socket is locked down so only intended clients can invoke privileged methods
**Depends on**: Nothing (first phase of v0.6.3)
**Requirements**: PR-01, PR-02, PR-03, PR-04, TS-05
**Success Criteria** (what must be TRUE):
  1. `openauto-system.service` runs with privilege sufficient for iptables, systemctl restart, rfkill/sysfs writes, and SDP permission fixes — no `Permission denied` errors
  2. The IPC socket at `/run/openauto/system.sock` is not world-writable — restricted to owner/group access only
  3. The Qt client (`SystemServiceClient`) connects and exchanges messages successfully after socket permissions are tightened
  4. The installed systemd unit on a fresh `install.sh` run matches the checked-in reference unit in the repo
  5. Tests confirm that an unauthorized caller is rejected and the intended client is accepted
**Plans**: 2 plans

Plans:
- [x] 15-01-PLAN.md — Daemon privilege model + IPC peer credential lockdown
- [x] 15-02-PLAN.md — Installer migration and template rendering

### Phase 16: Routing Correctness & Idempotency
**Goal**: Proxy routing applies clean iptables rules with correct exemptions, and repeated enable/disable cycles leave no stale state
**Depends on**: Phase 15
**Requirements**: PX-01, PX-02, PX-03, PX-04, IC-01, IC-02, IC-03, IC-04, TS-01, TS-02, TS-03, TS-04
**Success Criteria** (what must be TRUE):
  1. `set_proxy_route(active=true)` installs the expected `OPENAUTO_PROXY` chain and OUTPUT jump without privilege errors
  2. The upstream SOCKS destination IP/port is explicitly exempt from redirect rules (no self-interception loop)
  3. Redsocks and control-plane traffic (local AP, LAN) are exempt from redirect
  4. Calling enable 5 times in a row produces exactly one OUTPUT jump and one set of redirect rules — no duplicates
  5. Disable removes all routing state completely (chain, jump, DNS mutations) and abnormal restart self-heals on next startup
**Plans**: 4 plans

Plans:
- [x] 16-01-PLAN.md — Phase 15 hardware verification gate
- [ ] 16-02-PLAN.md — ProxyManager routing rewrite with flush/recreate, exemptions, and cleanup honesty
- [ ] 16-03-PLAN.md — Installer redsocks system user creation
- [ ] 16-04-PLAN.md — Routing correctness and idempotency tests

### Phase 17: Status Reporting Hardening
**Goal**: Proxy status reflects actual local pipeline health, not optimistic assumptions
**Depends on**: Phase 16
**Requirements**: SR-01, SR-02, SR-03
**Success Criteria** (what must be TRUE):
  1. `get_proxy_status` returns `ACTIVE` only after verifying the local redirect listener (redsocks) is alive and iptables rules are in place
  2. Startup failures, apply failures, and cleanup failures each produce distinct `FAILED` or `DEGRADED` states with actionable error detail
  3. Health checks probe the local redirect pipeline, not just upstream SOCKS reachability
**Plans**: 2 plans

Plans:
- [ ] 17-01-PLAN.md — Verification logic, lock-guarded operations, and redesigned health loop
- [ ] 17-02-PLAN.md — Status reporting verification and health check tests

### Phase 18: Hardware Validation
**Goal**: End-to-end proof that representative TCP traffic traverses the companion SOCKS bridge on real Pi hardware
**Depends on**: Phase 17
**Requirements**: PX-05
**Success Criteria** (what must be TRUE):
  1. On the Pi with a connected phone, `set_proxy_route(active=true)` succeeds and `get_proxy_status` reports `ACTIVE`
  2. Representative outbound TCP traffic (e.g., curl to an external host) is observed traversing the companion SOCKS bridge, not egressing directly via eth0/wlan0
**Plans**: TBD

Plans:
- [ ] 18-01: TBD

## Progress

**Execution Order:** 15 -> 16 -> 17 -> 18

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 15. Privilege Model & IPC Lockdown | 2/2 | Complete    | 2026-03-16 |
| 16. Routing Correctness & Idempotency | 4/4 | Complete    | 2026-03-16 |
| 17. Status Reporting Hardening | 2/2 | Complete   | 2026-03-16 |
| 18. Hardware Validation | 0/? | Not started | - |
