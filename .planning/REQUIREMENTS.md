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
- [x] **CTI-03**: Companion app can send M3 color palettes + wallpaper JPEG over TCP 9876, and HU receives, applies, and acknowledges
- [x] **CTI-04**: AA wire token path (0x8011/0x8012) is disabled; companion app is sole source of dynamic themes
- [x] **CTI-05**: Companion themes appear as named entries in theme picker; users can delete companion themes but not bundled themes
- [x] **CTI-06**: hello_ack contains display resolution for companion wallpaper cropping

### M3 Color Role Mapping

- [x] **M3R-01**: All custom/legacy AA token Q_PROPERTYs (textPrimary, textSecondary, red, onRed, yellow, onYellow) removed from ThemeService and replaced with M3 equivalents in QML
- [x] **M3R-02**: Deprecated custom token keys removed from bundled theme YAMLs and AA token persistence sets
- [x] **M3R-03**: Surface container hierarchy applied -- UI elements use appropriate container levels (Lowest/Low/Container/High/Highest) based on visual elevation
- [x] **M3R-04**: Button pressed states use primaryContainer/onPrimaryContainer (M3 contained button pattern) instead of raw primary
- [x] **M3R-05**: Toggle/slider active states use secondaryContainer (M3 selected state convention) instead of raw primary

### AA Rendering

- [ ] **FIX-01**: AA video content is fully visible with no cropping when navbar is shown (PreserveAspectFit letterboxing)
- [ ] **FIX-02**: Touch coordinates align with visible AA content after fillMode change (tapping a button hits that button)
- [ ] **FIX-03**: Existing SDP margin calculation unit tests continue to pass after rendering changes

### Navbar Status Indicators

- [x] **NAV-01**: Phone battery level is extracted from AA protocol (BatteryStatusNotification msg 0x0017 on control channel) and exposed as Q_PROPERTY
- [x] **NAV-02**: Phone signal strength is extracted from AA protocol (PhoneStatusUpdate.signal_strength on phone status channel) and exposed as Q_PROPERTY
- [x] **NAV-03**: Battery and signal properties reset to sentinel values on AA disconnect (no stale data)
- [x] **NAV-04**: SDP VideoConfig includes hidden_ui_elements (clock, battery, signal) when navbar is visible during AA; omits them when navbar is hidden
- [ ] **NAV-05**: Battery and signal icons appear flanking the clock in the navbar center zone during AA connection, with zone proportions updated to 15/70/15
- [ ] **NAV-06**: Battery icon turns red below 20% charge; indicators work in both horizontal and vertical navbar orientations

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
| CTI-03 | Phase 3.2 | Complete |
| CTI-04 | Phase 3.2 | Complete |
| CTI-05 | Phase 3.2 | Complete |
| CTI-06 | Phase 3.2 | Complete |
| M3R-01 | Phase 3.3 | Complete |
| M3R-02 | Phase 3.3 | Complete |
| M3R-03 | Phase 3.3 | Complete |
| M3R-04 | Phase 3.3 | Complete |
| M3R-05 | Phase 3.3 | Complete |
| FIX-01 | Phase 3.4 | Pending |
| FIX-02 | Phase 3.4 | Pending |
| FIX-03 | Phase 3.4 | Pending |
| NAV-01 | Phase 3.5 | Complete |
| NAV-02 | Phase 3.5 | Complete |
| NAV-03 | Phase 3.5 | Complete |
| NAV-04 | Phase 3.5 | Complete |
| NAV-05 | Phase 3.5 | Pending |
| NAV-06 | Phase 3.5 | Pending |
| STY-01 | Phase 4 | Pending |
| STY-02 | Phase 4 | Pending |

**Coverage:**
- v0.5.1 requirements: 33 total
- Mapped to phases: 33
- Unmapped: 0

---
*Requirements defined: 2026-03-08*
*Last updated: 2026-03-09 after Phase 03.5 planning*
