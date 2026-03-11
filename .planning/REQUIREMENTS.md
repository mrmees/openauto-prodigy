# Requirements: OpenAuto Prodigy

**Defined:** 2026-03-10
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.

## v0.5.2 Requirements

Requirements for widget system and UI polish milestone.

### Widget System

- [x] **WDG-01**: Home screen displays 3-pane widget layout (landscape: 60/40 main+subs, portrait: top+bottom)
- [x] **WDG-02**: LauncherDock provides quick-access app icons at bottom of home screen
- [x] **WDG-03**: Long-press on widget pane opens context menu (Change/Opacity/Clear)
- [x] **WDG-04**: Widget picker allows selecting from available widgets or clearing a pane
- [x] **WDG-05**: Glass card WidgetHost with semi-transparent background and per-pane adjustable opacity
- [x] **WDG-06**: Three built-in widgets: Clock, AA Status, Now Playing

### Settings Restructure

- [x] **SET-01**: Settings reorganized into 9 top-level categories: AA, Display, Audio, Bluetooth, Theme, Companion, System, Information, Debug
- [x] **SET-02**: AA page shows Resolution, DPI, and Auto-connect only
- [x] **SET-03**: Display page contains screen size info (size/PPI/resolution), UI scale stepper, screen dimming, navbar settings
- [x] **SET-04**: Theme promoted to top-level category with theme picker, wallpaper, dark mode toggle
- [x] **SET-05**: System contains left-hand drive, clock 24h toggle, day/night mode settings, software version
- [x] **SET-06**: Information page shows identity fields and hardware profile (read-only)
- [x] **SET-07**: Debug page (visible, last in list) contains protocol capture toggle, codec/decoder selection, TCP port override, verbose logging toggle
- [x] **SET-08**: FPS setting removed from UI (YAML-only, 30fps default)
- [x] **SET-09**: About OpenAutoProdigy section removed entirely
- [x] **SET-10**: Debug AA protocol buttons removed from System page

### Touch Normalization

- [x] **TCH-01**: All settings sub-pages use consistent alternating-row list style matching main settings menu
- [x] **TCH-02**: All interactive controls have touch-friendly sizing via UiMetrics (DPI-scaled, no hardcoded pixels)
- [x] **TCH-03**: Text readable at arm's length on target display sizes

## Workflow Constraints

- Use `frontend-design` skill for UI work
- All sizing via UiMetrics/DPI system — target real physical sizes, not pixel values
- Use Codex MCP for review/production (opposite role of Claude on each task)

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
| FPS selection in UI | Changing from 30fps causes bandwidth issues — YAML-only for power users |
| Full Material Design component library | Overkill for automotive — adopt color conventions only |
| Font selection/customization | Single font is fine for v1.0 |
| Screen rotation/orientation | Auto-detection handles this, no user control needed |
| Per-plugin theme overrides | Global theme is sufficient for v1.0 |
| Theme export (HU to companion) | Deferred — reverse direction not needed yet |
| About OpenAutoProdigy section | Removed — software version shown in System, no need for separate About |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| WDG-01 | Phase 1 | Complete |
| WDG-02 | Phase 1 | Complete |
| WDG-03 | Phase 1 | Complete |
| WDG-04 | Phase 1 | Complete |
| WDG-05 | Phase 1 | Complete |
| WDG-06 | Phase 1 | Complete |
| SET-01 | Phase 2 | Complete |
| SET-02 | Phase 2 | Complete |
| SET-03 | Phase 2 | Complete |
| SET-04 | Phase 2 | Complete |
| SET-05 | Phase 2 | Complete |
| SET-06 | Phase 2 | Complete |
| SET-07 | Phase 2 | Complete |
| SET-08 | Phase 2 | Complete |
| SET-09 | Phase 2 | Complete |
| SET-10 | Phase 2 | Complete |
| TCH-01 | Phase 3 | Complete |
| TCH-02 | Phase 3 | Complete |
| TCH-03 | Phase 3 | Complete |

**Coverage:**
- v0.5.2 requirements: 19 total
- Mapped to phases: 19/19
- Orphaned: 0

---
*Requirements defined: 2026-03-10*
*Last updated: 2026-03-10 after roadmap creation*
