# Requirements: OpenAuto Prodigy v0.4.3

**Defined:** 2026-03-02
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## v0.4.3 Requirements

### Visual Foundation (VIS)

- [ ] **VIS-01**: All interactive controls (tiles, buttons, toggles, sliders, list items) show visible press feedback (scale or opacity change)
- [ ] **VIS-02**: Settings page transitions use smooth slide + fade animations (render-thread Animators, ≤150ms)
- [ ] **VIS-03**: Automotive-minimal aesthetic applied globally — dark, high-contrast, full-width rows, subtle section dividers, generous spacing
- [ ] **VIS-04**: Day/night theme transitions animate smoothly (color interpolation, no instant flash)
- [ ] **VIS-05**: ThemeService provides `dividerColor` and `pressedColor` properties for consistent styling
- [ ] **VIS-06**: UiMetrics extended with animation duration constants and category tile sizing

### Icons (ICON)

- [ ] **ICON-01**: All settings rows and category tiles display contextual Material Symbols icons with minimal whitespace
- [ ] **ICON-02**: MaterialIcon component supports weight and optical size properties (variable font axes on Qt 6.8, graceful fallback on 6.4)
- [ ] **ICON-03**: NavStrip buttons use consistent icon sizing and press feedback
- [ ] **ICON-04**: Launcher tiles use appropriately-sized icons with the automotive-minimal visual style

### Settings Reorganization (SET)

- [ ] **SET-01**: Settings top-level displays 6 category tiles in a grid: Android Auto, Display, Audio, Connectivity, Companion, System/About
- [ ] **SET-02**: Category tiles show live status subtitles (e.g., Connectivity: "BT: Connected", Audio: "EQ: Rock", AA: "720p 60fps")
- [ ] **SET-03**: Android Auto category contains: resolution, FPS, DPI, codec enable/disable, decoder selection, auto-connect, sidebar config, protocol capture
- [ ] **SET-04**: Display category contains: brightness, theme picker, wallpaper picker, orientation, day/night mode source
- [ ] **SET-05**: Audio category contains: master volume, output device, mic gain, input device, and EQ subsection (stream selector, presets, band sliders)
- [ ] **SET-06**: Connectivity category contains: WiFi AP channel/band, Bluetooth device name, pairable toggle, paired devices list with connect/forget actions
- [ ] **SET-07**: Companion category contains: connection status, GPS coordinates, platform info
- [ ] **SET-08**: System/About category contains: app version, build info, system diagnostics
- [ ] **SET-09**: All existing config paths preserved — reorganization is purely presentational, no config migration needed

### Touch UX (UX)

- [ ] **UX-01**: BT device forget action has a clearly visible, adequately-sized touch target (not tiny text)
- [ ] **UX-02**: Modal pickers can be dismissed by tapping outside the modal area (background dismiss)
- [ ] **UX-03**: Read-only fields display clearly as informational (no "edit via web panel" confusion)
- [ ] **UX-04**: EQ accessible both via Audio settings subsection and NavStrip shortcut icon (dual-access with consistent state)

### Shell & Navigation (NAV)

- [ ] **NAV-01**: NavStrip buttons have consistent automotive-minimal styling with press feedback
- [ ] **NAV-02**: TopBar styling updated to match automotive-minimal aesthetic
- [ ] **NAV-03**: Launcher grid tiles restyled with automotive-minimal aesthetic and press feedback

## Future Requirements

### Deferred

- **UX-F01**: Swipe-to-dismiss modal pickers (touch gesture recognition complexity)
- **UX-F02**: Haptic feedback on touch (requires Pi hardware support investigation)
- **VIS-F01**: Custom transition directions based on navigation hierarchy (up/down vs left/right)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Font size increase (34-38px body) | Research suggests larger, but needs layout audit — risk of settings pages requiring excessive scrolling on 600px height. Evaluate after v0.4.3 layout is finalized. |
| Status bar redesign | TopBar is functional; full redesign is scope creep for a settings-focused milestone |
| New settings features (new config options) | This milestone reorganizes existing settings, not adding new configurable behaviors |
| Web config panel visual refresh | Separate codebase (Python/HTML), different design surface |
| `layer.enabled` effects (drop shadows, blur) | Banned per pitfalls research — GPU competition with video decode on Pi 4 |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| VIS-01 | — | Pending |
| VIS-02 | — | Pending |
| VIS-03 | — | Pending |
| VIS-04 | — | Pending |
| VIS-05 | — | Pending |
| VIS-06 | — | Pending |
| ICON-01 | — | Pending |
| ICON-02 | — | Pending |
| ICON-03 | — | Pending |
| ICON-04 | — | Pending |
| SET-01 | — | Pending |
| SET-02 | — | Pending |
| SET-03 | — | Pending |
| SET-04 | — | Pending |
| SET-05 | — | Pending |
| SET-06 | — | Pending |
| SET-07 | — | Pending |
| SET-08 | — | Pending |
| SET-09 | — | Pending |
| UX-01 | — | Pending |
| UX-02 | — | Pending |
| UX-03 | — | Pending |
| UX-04 | — | Pending |
| NAV-01 | — | Pending |
| NAV-02 | — | Pending |
| NAV-03 | — | Pending |

**Coverage:**
- v0.4.3 requirements: 26 total
- Mapped to phases: 0
- Unmapped: 26 ⚠️

---
*Requirements defined: 2026-03-02*
*Last updated: 2026-03-02 after initial definition*
