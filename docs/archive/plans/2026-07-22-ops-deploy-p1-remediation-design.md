# Operations Deployment Reliability Remediation — Design

Status: COMPLETED 2026-07-22

**Date:** 2026-07-22
**Grounded against:** `origin/main` at `2366cf88`
**Publication:** one standalone branch and draft pull request

## 1. Outcome

Make source and prebuilt installations provide the same required BlueZ SDP
compatibility, keep the application usable when its WiFi AP is unavailable or
recovering, and make every restart preserve one systemd-owned application
process and one IPC-socket owner.

This is an operations-reliability batch. It changes installer assets, systemd
relationships, the packaged restart helper, and the IPC listener's ownership
check. It does not change Android Auto protocol behavior or public APIs.

## 2. Decisions Locked

- **Canonical systemd assets.** BlueZ and hostapd integration fragments live
  under `config/systemd/`, are shipped by the existing complete-config payload,
  and are consumed by both installers. Installer-specific inline copies are not
  authoritative.
- **Bluetooth setup is independent of WiFi setup.** Both install modes enable
  `bluetoothd --compat` and establish group-writable access to `/var/run/sdp`
  even when no AP interface was selected.
- **Hostapd is optional to the shell.** An AP-configured install requests and
  orders hostapd through an application drop-in, but hostapd failure never stops
  the application. A no-AP install carries no hostapd dependency.
- **Hostapd owns its recovery.** Its project drop-in uses
  `Restart=on-failure`; application stop/restart is not propagated back to
  hostapd.
- **systemd is the only production process owner.** The restart helper invokes
  the installed service and never kills by executable pattern or launches a
  detached application.
- **The IPC socket is the single-instance boundary.** A live listener is never
  unlinked. A second process fails before initializing Bluetooth, audio,
  projection, or input resources. A genuinely stale socket remains recoverable.
- **Current behavior documentation changes with its owner.** Prebuilt caveats
  are removed only when the corresponding installer regression is green.

## 3. Acceptance Matrix

| Concern | Static/package proof | Dynamic proof | Pi requirement |
|---|---|---|---|
| BlueZ SDP parity | Both installers consume the same packaged override; archive contains it | Installer contract materializes and validates the drop-in | Post-deploy inspection of `bluetooth.service` and `/var/run/sdp`; no Bluetooth restart without separate approval |
| Hostapd lifetime | No `BindsTo` or reverse `PartOf`; optional AP drop-in only | Real user-systemd harness proves AP crash recovery, application survival, AP stability across app restart, and no-AP startup | Non-disruptive merged-unit inspection; real hostapd stop/restart only with separate approval |
| Restart/single instance | Helper contains no detached-launch or process-pattern kill path | IPC regression proves live-owner refusal and stale recovery | Required after separately approved deploy: normal and forced helper paths leave one process and one responding socket |

## 4. Explicitly Out of Scope

- AA night-mode initial-state delivery
- Bluetooth AVRCP duration/position units
- Logging configuration registration or persistence
- RFCOMM/SDP registration retry redesign
- Prebuilt Wayland/compositor startup ordering
- Prebuilt WiFi band, channel, country-code, or adapter-capability selection
- AP password rotation or system-service network policy
- USB Android Auto, HFP role changes, ofono, telephony routing, codecs, or
  protocol/schema changes
- Public API, JS shim, dashboard, overlay, or frozen numeric changes
- Tagging, release publication, or an unapproved Pi deployment

## 5. Verification Policy

Each implementation task runs its focused regression first. Before closure the
branch must pass shell syntax checks, installer/package tests, systemd unit
validation, the real local systemd lifecycle harness, the IPC singleton test,
the full local build, the explicit `openauto-prodigy` target, the full CTest
suite, documentation links, `git diff --check`, and the repository review gate.

Pi validation stops at an explicit approval boundary. The restart/socket row is
required before the batch can be called complete; operations that restart
Bluetooth or hostapd remain separately opt-in because they disrupt connectivity.
