# Roadmap: OpenAuto Prodigy v0.4.2 — Service Hardening

## Overview

Every lifecycle transition (install, first boot, reboot, app restart, crash recovery) must result in a fully working system with no manual intervention. This milestone hardens the service stack from the bottom up: first make the system services (WiFi AP, DHCP, systemd ordering) bulletproof, then harden Bluetooth/SDP initialization, then make the app itself a good systemd citizen with clean startup/shutdown and a final health check to verify everything works together.

## Milestones

<details>
<summary>v0.4 Logging & Theming (Phases 1-2) - SHIPPED 2026-03-01</summary>

See .planning/milestones/v0.4/ for archived details.

</details>

<details>
<summary>v0.4.1 Audio Equalizer (Phases 1-3) - SHIPPED 2026-03-02</summary>

See .planning/milestones/v0.4.1/ for archived details.

</details>

### v0.4.2 Service Hardening (In Progress)

**Milestone Goal:** Every lifecycle transition (install, first boot, reboot, app restart, crash recovery) results in a fully working system with no manual intervention.

## Phases

- [ ] **Phase 1: Service Foundation** - Reliable WiFi AP, systemd ordering, and installer service definitions
- [ ] **Phase 2: Bluetooth Hardening** - Robust BlueZ/SDP initialization that survives restarts and upgrades
- [ ] **Phase 3: App Lifecycle** - PipeWire readiness, clean shutdown, and post-install health verification

## Phase Details

### Phase 1: Service Foundation
**Goal**: WiFi AP and system services start reliably on every boot with correct ordering and pre-conditions
**Depends on**: Nothing (first phase)
**Requirements**: WIFI-01, WIFI-02, WIFI-03, WIFI-04, SVC-01, SVC-02, INST-01
**Success Criteria** (what must be TRUE):
  1. After a cold boot, the WiFi AP is up and accepting client connections without manual intervention
  2. Killing and restarting the Prodigy app does not take down the WiFi AP (phone stays connected to WiFi)
  3. The Prodigy systemd service starts only after Wayland, hostapd, BlueZ, and PipeWire are ready
  4. If the Prodigy app crashes, systemd restarts it automatically (with rate limiting to prevent restart loops)
  5. A fresh `install.sh` run produces service files with ExecStartPre checks for rfkill and socket readiness
**Plans**: 2 plans (1 wave)

Plans:
- [ ] 01-01-PLAN.md — Pre-flight script and systemd service hardening (install.sh)
- [ ] 01-02-PLAN.md — sd_notify integration and watchdog heartbeat (C++ app)

### Phase 2: Bluetooth Hardening
**Goal**: SDP registration and BlueZ compatibility work reliably across boots, daemon restarts, and package updates
**Depends on**: Phase 1
**Requirements**: BT-01, BT-02, BT-03
**Success Criteria** (what must be TRUE):
  1. After any boot, `/var/run/sdp` has correct group permissions (bluetooth group, group-writable) without manual fixup
  2. The BlueZ `--compat` override persists through `apt upgrade` of bluez and `systemctl daemon-reload`
  3. App logs show SDP service registration success within 10 seconds of startup (with clear retry/success messages)
**Plans**: TBD

Plans:
- [ ] 02-01: TBD

### Phase 3: App Lifecycle
**Goal**: The app starts cleanly (waiting for PipeWire), shuts down cleanly (no orphaned resources), and the installer verifies everything works
**Depends on**: Phase 1, Phase 2
**Requirements**: AUD-01, AUD-02, SVC-03, INST-02
**Success Criteria** (what must be TRUE):
  1. App waits for PipeWire to be ready before creating audio streams (no "PipeWire not available" errors in logs)
  2. After stopping the app with SIGTERM, `pw-cli list-objects` shows no orphaned Prodigy audio nodes
  3. SIGTERM triggers orderly shutdown: config saved, AA connection closed, PipeWire streams destroyed, exit code 0
  4. Running `install.sh` on a fresh system ends with a health check confirming all services (hostapd, BlueZ, PipeWire, Prodigy) are active
**Plans**: TBD

Plans:
- [ ] 03-01: TBD
- [ ] 03-02: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Service Foundation | 0/? | Not started | - |
| 2. Bluetooth Hardening | 0/? | Not started | - |
| 3. App Lifecycle | 0/? | Not started | - |

---
*Roadmap created: 2026-03-02*
*Last updated: 2026-03-02*
