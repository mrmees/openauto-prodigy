# Requirements: OpenAuto Prodigy v1.0

**Defined:** 2026-03-01
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## v1 Requirements

Requirements for v1.0 release. Each maps to roadmap phases.

### Audio

- [ ] **AUD-01**: Phone calls via HFP continue playing through head unit speakers even when AA session disconnects or crashes
- [ ] **AUD-02**: Audio focus management ducks AA media when HFP call is active
- [ ] **AUD-03**: User can select from built-in EQ presets (flat, bass boost, vocal, etc.) from head unit settings
- [ ] **AUD-04**: User can create and save custom EQ profiles with per-band gain adjustment
- [ ] **AUD-05**: EQ settings are editable from the web config panel for precise adjustment
- [ ] **AUD-06**: EQ applies to media/music audio only (not navigation or phone call audio)

### Display & Theming

- [x] **DISP-01**: User can select from multiple color palettes in settings (dark, light, blue, etc.)
- [ ] **DISP-02**: User can select a wallpaper from available options in settings
- [x] **DISP-03**: Selected theme and wallpaper persist across restarts
- [x] **DISP-04**: Screen brightness is controllable from the settings slider and gesture overlay
- [x] **DISP-05**: Brightness control writes to actual display backlight hardware on the Pi

### Logging

- [x] **LOG-01**: Default log output is clean — no debug spam in journalctl during normal operation
- [x] **LOG-02**: Debug-level logging is available via --verbose flag or config option
- [x] **LOG-03**: Log categories are defined per component (AA, BT, Audio, Plugin, etc.)

### Release Quality

- [ ] **REL-01**: First-run experience guides user through phone pairing and WiFi verification
- [ ] **REL-02**: Shutdown and reboot from ExitDialog actually trigger systemd poweroff/reboot
- [ ] **REL-03**: Systemd watchdog monitors the process and restarts on hang
- [ ] **REL-04**: Application handles clean shutdown on SIGTERM (saves config, closes connections)
- [ ] **REL-05**: Application is stable as a daily driver — no crashes requiring SSH intervention over a week of use

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### AA Protocol

- **AAPROT-01**: Dynamic sidebar reconfiguration during active AA session via UpdateHuUiConfigRequest (0x8012)
- **AAPROT-02**: Sidebar position changes (left/right/top/bottom) trigger video renegotiation without reconnect

### Security

- **SEC-01**: Per-connection WiFi password rotation via hostapd_cli

### Ecosystem

- **ECO-01**: CI automation for builds and tests
- **ECO-02**: Community contribution workflow (issue templates, PR guidelines)
- **ECO-03**: Example community plugins (OBD-II, backup camera, GPIO control)

### Display (Extended)

- **DISP-06**: Custom icon sets and full theme engine
- **DISP-07**: Multi-display / resolution support beyond 1024x600

## Out of Scope

| Feature | Reason |
|---------|--------|
| USB Android Auto | Wireless-only by design — every modern phone supports wireless AA |
| CarPlay / non-AA protocols | Single protocol focus for quality |
| Hardware beyond Pi 4 | Target hardware only for v1.0 |
| Cloud services, accounts, telemetry | Privacy-first, offline-only |
| Companion app features | Separate repo, separate timeline |
| Built-in media player (Kodi) | BT A2DP covers music playback without AA |
| Screen mirroring | Niche feature, not daily driver need |
| BMW iDrive / IBus / MMI controllers | Niche hardware, community plugin post-v1.0 |
| Hardware sensors (TSL2561, DS18B20) | Niche, GPIO day/night already exists |
| External application launcher | Plugin system replaces this pattern |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| AUD-01 | Phase 3 | Pending |
| AUD-02 | Phase 3 | Pending |
| AUD-03 | Phase 3 | Pending |
| AUD-04 | Phase 3 | Pending |
| AUD-05 | Phase 3 | Pending |
| AUD-06 | Phase 3 | Pending |
| DISP-01 | Phase 2 | Complete |
| DISP-02 | Phase 2 | Pending |
| DISP-03 | Phase 2 | Complete |
| DISP-04 | Phase 2 | Complete |
| DISP-05 | Phase 2 | Complete |
| LOG-01 | Phase 1 | Complete |
| LOG-02 | Phase 1 | Complete |
| LOG-03 | Phase 1 | Complete |
| REL-01 | Phase 4 | Pending |
| REL-02 | Phase 4 | Pending |
| REL-03 | Phase 4 | Pending |
| REL-04 | Phase 4 | Pending |
| REL-05 | Phase 4 | Pending |

**Coverage:**
- v1 requirements: 19 total
- Mapped to phases: 19
- Unmapped: 0

---
*Requirements defined: 2026-03-01*
*Last updated: 2026-03-01 after roadmap creation*
