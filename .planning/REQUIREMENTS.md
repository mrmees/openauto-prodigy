# Requirements: OpenAuto Prodigy v0.4.5

**Defined:** 2026-03-03
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## v0.4.5 Requirements

Requirements for navbar rework milestone. Each maps to roadmap phases.

### Touch Routing

- [x] **TOUCH-01**: Touch routing layer dispatches evdev touches to registered zones by priority
- [x] **TOUCH-02**: Finger routing is sticky — once a finger claims a zone on DOWN, all MOVE/UP stay with that zone
- [x] **TOUCH-03**: Popup overlays can register/deregister transient touch zones dynamically
- [x] **TOUCH-04**: QML components report their geometry to C++ touch zones (no duplicated layout math)
- [x] **TOUCH-05**: Dual input path — QML MouseArea works in launcher, evdev zones work during AA

### Navbar

- [ ] **NAV-01**: Navbar displays 3 controls: volume (driver side), clock/home (center), brightness (passenger side)
- [ ] **NAV-02**: Each control supports tap, short-hold-release, and long-hold gestures
- [ ] **NAV-03**: Volume tap shows popup slider; short-hold opens EQ; long-hold mutes
- [ ] **NAV-04**: Clock tap goes home; short-hold opens settings; long-hold shows power/minimize menu
- [ ] **NAV-05**: Brightness tap shows popup slider; short-hold opens display settings; long-hold toggles night mode
- [ ] **NAV-06**: LHD/RHD config swaps driver/passenger side placement
- [ ] **NAV-07**: Popup sliders dismiss on outside tap, consistent with existing modal pattern

### Edge Positioning

- [ ] **EDGE-01**: User can configure navbar edge position (top/bottom/left/right) in settings
- [ ] **EDGE-02**: Navbar renders horizontally on top/bottom edges, vertically on left/right edges
- [ ] **EDGE-03**: Touch zones update automatically when navbar edge position changes

### AA Integration

- [ ] **AA-01**: "Show Navbar during AA" toggle in settings
- [ ] **AA-02**: When navbar is visible during AA, viewport margins reserve space for navbar
- [ ] **AA-03**: Touch in navbar zone during AA is consumed (not forwarded to phone)
- [ ] **AA-04**: Touch outside navbar zone during AA forwards to phone normally
- [ ] **AA-05**: Gesture overlay touch passthrough bug is fixed (overlay controls respond to touch during AA)
- [ ] **AA-06**: Sidebar overlay is replaced by navbar — no separate sidebar component

### Cleanup

- [ ] **CLEAN-01**: TopBar component removed from shell
- [ ] **CLEAN-02**: Old NavStrip (plugin icon bar) removed from shell
- [ ] **CLEAN-03**: Sidebar overlay component and its evdev hit zones removed
- [ ] **CLEAN-04**: Clock display integrated into navbar (no separate clock component needed)

## Future Requirements

Deferred to future milestones. Tracked but not in current roadmap.

### Navbar Enhancements

- **NAVX-01**: Navbar auto-hide during AA (overlay on margin area, reveal on edge tap)
- **NAVX-02**: Configurable gesture actions (user remaps tap/short-hold/long-hold per control)
- **NAVX-03**: Navbar animation on show/hide (slide in/out)

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Plugin icons in navbar | Launcher tiles are the sole plugin navigation — navbar is controls only |
| Double-tap gesture | Adds 300ms delay to single-tap; replaced by short-hold-release |
| Navbar auto-hide during AA | Adds complexity; deferred unless users request it |
| Custom icon sets for navbar | Material Symbols icons sufficient for 3 controls |
| Swipe gestures on navbar | Touch routing complexity; tap/hold covers all needed actions |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| TOUCH-01 | Phase 1 | Complete |
| TOUCH-02 | Phase 1 | Complete |
| TOUCH-03 | Phase 1 | Complete |
| TOUCH-04 | Phase 1 | Complete |
| TOUCH-05 | Phase 1 | Complete |
| NAV-01 | Phase 2 | Pending |
| NAV-02 | Phase 2 | Pending |
| NAV-03 | Phase 2 | Pending |
| NAV-04 | Phase 2 | Pending |
| NAV-05 | Phase 2 | Pending |
| NAV-06 | Phase 2 | Pending |
| NAV-07 | Phase 2 | Pending |
| EDGE-01 | Phase 2 | Pending |
| EDGE-02 | Phase 2 | Pending |
| EDGE-03 | Phase 2 | Pending |
| AA-01 | Phase 3 | Pending |
| AA-02 | Phase 3 | Pending |
| AA-03 | Phase 3 | Pending |
| AA-04 | Phase 3 | Pending |
| AA-05 | Phase 3 | Pending |
| AA-06 | Phase 3 | Pending |
| CLEAN-01 | Phase 4 | Pending |
| CLEAN-02 | Phase 4 | Pending |
| CLEAN-03 | Phase 4 | Pending |
| CLEAN-04 | Phase 4 | Pending |

**Coverage:**
- v0.4.5 requirements: 25 total
- Mapped to phases: 25
- Unmapped: 0

---
*Requirements defined: 2026-03-03*
*Last updated: 2026-03-03 -- TOUCH-01 through TOUCH-05 complete (Phase 1)*
