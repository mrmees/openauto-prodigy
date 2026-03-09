# Requirements: OpenAuto Prodigy

**Defined:** 2026-03-08
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.

## v0.5.1 Requirements

Requirements for DPI sizing and UI polish milestone. Each maps to roadmap phases.

### Screen & DPI

- [x] **DPI-01**: Installer probes EDID for physical screen dimensions and prompts user to confirm or enter screen size
- [x] **DPI-02**: Default physical screen size is 7" (double DIN) when EDID unavailable and user skips entry
- [x] **DPI-03**: UiMetrics computes baseline scale from real DPI (resolution / physical size) instead of pure resolution ratio
- [x] **DPI-04**: Physical screen size is persisted in YAML config and shown (read-only) in Display settings; editable via YAML for advanced users
- [x] **DPI-05**: User can adjust UI scale via stepper control (+-0.1 increments) in Display settings, applied as multiplier on DPI baseline

### Clock

- [x] **CLK-01**: Clock display is larger and more readable at automotive glance distance
- [x] **CLK-02**: AM/PM designation removed from clock display (12h format shows time only)
- [x] **CLK-03**: User can toggle between 12h and 24h clock format in settings

### Theme & Colors

- [x] **THM-01**: Theme color palette is fully fleshed out with named semantic roles (surface, primary, secondary, accent, error, etc.)
- [x] **THM-02**: Theme colors align with Material Design color system conventions for consistency and familiarity
- [x] **THM-03**: All UI elements consistently use semantic theme colors (no hardcoded color values in QML)

### Companion Theme Import

- [x] **CTI-01**: ThemeService exposes all 34 Material Design 3 color roles as Q_PROPERTYs for QML binding
- [x] **CTI-02**: All bundled themes (default, AMOLED, Ocean, Ember, connected-device) specify all 34 M3 roles in both day and night maps
- [ ] **CTI-03**: Companion app can send M3 color palettes + wallpaper JPEG over TCP 9876, and HU receives, applies, and acknowledges
- [ ] **CTI-04**: AA wire token path (0x8011/0x8012) is disabled; companion app is sole source of dynamic themes
- [x] **CTI-05**: Companion themes appear as named entries in theme picker; users can delete companion themes but not bundled themes
- [ ] **CTI-06**: hello_ack contains display resolution for companion wallpaper cropping

### Styling

- [ ] **STY-01**: Buttons have subtle 3D depth effect (shadow, bevel, or gradient) that responds to press state
- [ ] **STY-02**: Taskbar/navbar has subtle depth treatment distinguishing it from content area

## Future Requirements

### Display

- **DISP-01**: User-facing preset sizes (Small / Medium / Large) as shortcuts for scale values
- **DISP-02**: Dynamic DPI recalculation on display hot-plug

### Theme

- **THM-04**: Dark/light mode auto-switching based on ambient light sensor
- **THM-05**: User-created custom color themes

## Out of Scope

| Feature | Reason |
|---------|--------|
| Full Material Design component library | Overkill for automotive -- adopt color conventions only |
| Animated theme transitions beyond existing | Color animation already shipped in v0.4.3 |
| Font selection/customization | Single font is fine for v1.0 |
| Screen rotation/orientation | Auto-detection handles this, no user control needed |
| Per-plugin theme overrides | Global theme is sufficient for v1.0 |
| Theme export (HU to companion) | Deferred -- reverse direction not needed yet |
| Theme animation on switch | Deferred -- cross-fade between old and new colors |
| Multiple wallpaper support per theme | Deferred -- slideshow/rotation not needed yet |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| DPI-01 | Phase 1 | Complete |
| DPI-02 | Phase 1 | Complete |
| DPI-03 | Phase 1 | Complete |
| DPI-04 | Phase 1 | Complete |
| DPI-05 | Phase 2 | Complete |
| CLK-01 | Phase 2 | Complete |
| CLK-02 | Phase 2 | Complete |
| CLK-03 | Phase 2 | Complete |
| THM-01 | Phase 3 | Complete |
| THM-02 | Phase 3 | Complete |
| THM-03 | Phase 3 | Complete |
| CTI-01 | Phase 3.2 | Complete |
| CTI-02 | Phase 3.2 | Complete |
| CTI-03 | Phase 3.2 | Pending |
| CTI-04 | Phase 3.2 | Pending |
| CTI-05 | Phase 3.2 | Complete |
| CTI-06 | Phase 3.2 | Pending |
| STY-01 | Phase 4 | Pending |
| STY-02 | Phase 4 | Pending |

**Coverage:**
- v0.5.1 requirements: 19 total
- Mapped to phases: 19
- Unmapped: 0

---
*Requirements defined: 2026-03-08*
*Last updated: 2026-03-09 after Plan 03.2-01 complete*
