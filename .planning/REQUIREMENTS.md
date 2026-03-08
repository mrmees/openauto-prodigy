# Requirements: OpenAuto Prodigy

**Defined:** 2026-03-08
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## v0.5.1 Requirements

Requirements for DPI sizing and UI polish milestone. Each maps to roadmap phases.

### Screen & DPI

- [x] **DPI-01**: Installer probes EDID for physical screen dimensions and prompts user to confirm or enter screen size
- [x] **DPI-02**: Default physical screen size is 7" (double DIN) when EDID unavailable and user skips entry
- [x] **DPI-03**: UiMetrics computes baseline scale from real DPI (resolution ÷ physical size) instead of pure resolution ratio
- [x] **DPI-04**: Physical screen size is persisted in YAML config and shown (read-only) in Display settings; editable via YAML for advanced users
- [x] **DPI-05**: User can adjust UI scale via stepper control (±0.1 increments) in Display settings, applied as multiplier on DPI baseline

### Clock

- [x] **CLK-01**: Clock display is larger and more readable at automotive glance distance
- [x] **CLK-02**: AM/PM designation removed from clock display (12h format shows time only)
- [x] **CLK-03**: User can toggle between 12h and 24h clock format in settings

### Theme & Colors

- [x] **THM-01**: Theme color palette is fully fleshed out with named semantic roles (surface, primary, secondary, accent, error, etc.)
- [x] **THM-02**: Theme colors align with Material Design color system conventions for consistency and familiarity
- [x] **THM-03**: All UI elements consistently use semantic theme colors (no hardcoded color values in QML)

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
| Full Material Design component library | Overkill for automotive — adopt color conventions only |
| Animated theme transitions beyond existing | Color animation already shipped in v0.4.3 |
| Font selection/customization | Single font is fine for v1.0 |
| Screen rotation/orientation | Auto-detection handles this, no user control needed |
| Per-plugin theme overrides | Global theme is sufficient for v1.0 |

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
| STY-01 | Phase 4 | Pending |
| STY-02 | Phase 4 | Pending |

**Coverage:**
- v0.5.1 requirements: 13 total
- Mapped to phases: 13
- Unmapped: 0

---
*Requirements defined: 2026-03-08*
*Last updated: 2026-03-08 after roadmap creation*
