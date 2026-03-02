# Requirements: OpenAuto Prodigy v0.4.2

**Defined:** 2026-03-02
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## v0.4.2 Requirements

Requirements for service hardening milestone. Each maps to roadmap phases.

### WiFi

- [ ] **WIFI-01**: WiFi rfkill is unblocked before hostapd starts on every boot
- [ ] **WIFI-02**: hostapd starts reliably and recovers from transient failures
- [ ] **WIFI-03**: WiFi AP is independent of Prodigy app lifecycle (survives app restart)
- [ ] **WIFI-04**: DHCP server (systemd-networkd) is ready before hostapd accepts clients

### Bluetooth

- [ ] **BT-01**: SDP socket (`/var/run/sdp`) always has correct group permissions after BlueZ starts
- [ ] **BT-02**: BlueZ `--compat` override is robust (survives package updates, daemon-reload)
- [ ] **BT-03**: App registers SDP service within 10 seconds of startup (clear logging on retry/success)

### Audio

- [ ] **AUD-01**: App detects PipeWire readiness before creating audio streams
- [ ] **AUD-02**: Audio streams are properly cleaned up on app shutdown (no orphaned PipeWire nodes)

### Service Stack

- [ ] **SVC-01**: Prodigy systemd service has correct After/Wants dependencies (Wayland, hostapd, BlueZ, PipeWire)
- [ ] **SVC-02**: Prodigy restarts automatically on crash with rate limiting
- [ ] **SVC-03**: SIGTERM triggers clean shutdown (config save, connection close, resource release)

### Installer

- [ ] **INST-01**: Installer creates service definitions with ExecStartPre pre-conditions (rfkill, socket checks)
- [ ] **INST-02**: Installer runs post-install health check verifying all services are functional

## Future Requirements

### Deferred

- **PERF-01**: Boot-to-AA-ready time under 30 seconds
- **FRX-01**: First-run experience guides user through phone pairing and WiFi verification
- **HFP-01**: HFP call audio persists across AA connection state changes
- **WEB-01**: User can adjust EQ bands from the web config panel
- **WEB-02**: User can manage presets from the web config panel
- **WEB-03**: User can assign presets to streams from the web config panel

## Out of Scope

| Feature | Reason |
|---------|--------|
| WiFi password rotation per connection | Post-v1.0 security enhancement |
| Systemd watchdog with custom health protocol | Restart=on-failure is sufficient for v0.4.2 |
| NetworkManager instead of hostapd/systemd-networkd | Current stack works, just needs hardening |
| Boot speed optimization | Reliability first; speed is a future milestone |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| WIFI-01 | — | Pending |
| WIFI-02 | — | Pending |
| WIFI-03 | — | Pending |
| WIFI-04 | — | Pending |
| BT-01 | — | Pending |
| BT-02 | — | Pending |
| BT-03 | — | Pending |
| AUD-01 | — | Pending |
| AUD-02 | — | Pending |
| SVC-01 | — | Pending |
| SVC-02 | — | Pending |
| SVC-03 | — | Pending |
| INST-01 | — | Pending |
| INST-02 | — | Pending |

**Coverage:**
- v0.4.2 requirements: 14 total
- Mapped to phases: 0
- Unmapped: 14 ⚠️

---
*Requirements defined: 2026-03-02*
*Last updated: 2026-03-02 after initial definition*
