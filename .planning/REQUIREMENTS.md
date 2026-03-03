# Requirements: OpenAuto Prodigy v0.4.3

**Defined:** 2026-03-02
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## v0.4.3 Requirements

### Visual Foundation (VIS)

- [x] **VIS-01**: All interactive controls (tiles, buttons, toggles, sliders, list items) show visible press feedback (scale or opacity change)
- [x] **VIS-02**: Settings page transitions use smooth slide + fade animations (render-thread Animators, ≤150ms)
- [x] **VIS-03**: Automotive-minimal aesthetic applied globally — dark, high-contrast, full-width rows, subtle section dividers, generous spacing
- [x] **VIS-04**: Day/night theme transitions animate smoothly (color interpolation, no instant flash)
- [x] **VIS-05**: ThemeService provides `dividerColor` and `pressedColor` properties for consistent styling
- [x] **VIS-06**: UiMetrics extended with animation duration constants and category tile sizing

### Icons (ICON)

- [x] **ICON-01**: All settings rows and category tiles display contextual Material Symbols icons with minimal whitespace
- [x] **ICON-02**: MaterialIcon component supports weight and optical size properties (variable font axes on Qt 6.8, graceful fallback on 6.4)
- [x] **ICON-03**: NavStrip buttons use consistent icon sizing and press feedback
- [x] **ICON-04**: Launcher tiles use appropriately-sized icons with the automotive-minimal visual style

### Settings Reorganization (SET)

- [x] **SET-01**: Settings top-level displays 6 category tiles in a grid: Android Auto, Display, Audio, Bluetooth, Companion, System
- [x] **SET-02**: ~~Category tiles show live status subtitles~~ Descoped — subtitles too small for automotive display at 1024x600. Deferred to future milestone.
- [x] **SET-03**: Android Auto category contains: resolution, FPS, DPI, codec enable/disable, decoder selection, auto-connect, sidebar config, protocol capture
- [x] **SET-04**: Display category contains: brightness, theme picker, wallpaper picker, orientation, day/night mode source
- [x] **SET-05**: Audio category contains: master volume, output device, mic gain, input device, and EQ entry point
- [x] **SET-06**: ~~Connectivity category contains WiFi AP~~ Descoped — WiFi AP is set-once at install, no runtime UI needed. Renamed to Bluetooth: device name, pairable toggle, paired devices with forget.
- [x] **SET-07**: Companion category contains: enabled toggle, connection status, GPS coordinates, pairing code generation with QR popup
- [x] **SET-08**: System category contains: identity, hardware info, app version, Close App button (About merged in)
- [x] **SET-09**: All existing config paths preserved — reorganization is purely presentational, no config migration needed

### Touch UX (UX)

- [x] **UX-01**: BT device forget action has a clearly visible, adequately-sized touch target (not tiny text)
- [x] **UX-02**: Modal pickers can be dismissed by tapping outside the modal area (background dismiss)
- [x] **UX-03**: Read-only fields display clearly as informational (no "edit via web panel" confusion)
- [x] **UX-04**: EQ accessible both via Audio settings subsection and NavStrip shortcut icon (dual-access with consistent state)

### Shell & Navigation (NAV)

- [x] **NAV-01**: NavStrip buttons have consistent automotive-minimal styling with press feedback
- [x] **NAV-02**: TopBar styling updated to match automotive-minimal aesthetic
- [x] **NAV-03**: Launcher grid tiles restyled with automotive-minimal aesthetic and press feedback

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
| VIS-01 | Phase 1 | Complete |
| VIS-02 | Phase 1 | Complete |
| VIS-03 | Phase 1 | Complete |
| VIS-04 | Phase 1 | Complete |
| VIS-05 | Phase 1 | Complete |
| VIS-06 | Phase 1 | Complete |
| ICON-01 | Phase 1 | Complete |
| ICON-02 | Phase 1 | Complete |
| ICON-03 | Phase 3 | Complete |
| ICON-04 | Phase 3 | Complete |
| SET-01 | Phase 2 | Complete |
| SET-02 | Phase 2 | Complete |
| SET-03 | Phase 2 | Complete |
| SET-04 | Phase 2 | Complete |
| SET-05 | Phase 2 | Complete |
| SET-06 | Phase 2 | Complete |
| SET-07 | Phase 2 | Complete |
| SET-08 | Phase 2 | Complete |
| SET-09 | Phase 2 | Complete |
| UX-01 | Phase 3 | Complete |
| UX-02 | Phase 3 | Complete |
| UX-03 | Phase 2 | Complete |
| UX-04 | Phase 3 | Complete |
| NAV-01 | Phase 3 | Complete |
| NAV-02 | Phase 3 | Complete |
| NAV-03 | Phase 3 | Complete |

**Coverage:**
- v0.4.3 requirements: 26 total
- Mapped to phases: 26
- Unmapped: 0

---
*Requirements defined: 2026-03-02*
*Last updated: 2026-03-02 after roadmap creation*
