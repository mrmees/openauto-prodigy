# Requirements: OpenAuto Prodigy

**Defined:** 2026-03-21
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## v0.6.6 Requirements

Requirements for Homescreen Layout & Widget Settings Rework. Each maps to roadmap phases.

### Widget Selection

- [x] **SEL-01**: User can long-press a widget to lift it (scale up + shadow visual feedback)
- [x] **SEL-02**: User can drag a lifted widget to reposition it on the grid with snap-to-cell
- [x] **SEL-03**: User can release a long-pressed widget without dragging to enter selected state (border/glow indicator)
- [x] **SEL-04**: User can tap empty space or clock-home navbar button to deselect a widget and revert all UI

### Navbar Transformation

- [x] **NAV-01**: Volume navbar control transforms to settings gear icon when a widget is selected
- [x] **NAV-02**: Brightness navbar control transforms to delete/trash icon when a widget is selected
- [x] **NAV-03**: Tapping settings navbar button opens config sheet for the selected widget
- [x] **NAV-04**: Tapping delete navbar button removes the selected widget
- [x] **NAV-05**: Navbar automatically reverts to normal controls on deselect, drag start, or auto-deselect timeout

### Resize

- [x] **RSZ-01**: Selected widget shows visible drag handles on all 4 edges
- [x] **RSZ-02**: User can drag any edge handle to resize the widget in that direction
- [x] **RSZ-03**: Resize is clamped to widget descriptor min/max constraints with visual feedback at limits
- [x] **RSZ-04**: Resize is blocked when it would overlap another widget, with visual collision indicator

### Widget Picker

- [x] **PKR-01**: Bottom sheet slides up with scrollable categorized list of available widgets
- [x] **PKR-02**: Tapping a widget in the picker auto-places it at first available grid cell
- [x] **PKR-03**: Picker filters to widgets that fit available grid space

### Page Management

- [x] **PGM-01**: Long-press on empty grid space shows menu with "Add Widget" and "Add Page" options
- [x] **PGM-02**: "Add Widget" opens the bottom sheet widget picker
- [x] **PGM-03**: "Add Page" creates a new page and navigates to it
- [x] **PGM-04**: Empty pages are automatically deleted when no widgets remain on them

### Cleanup

- [x] **CLN-01**: Global edit mode removed (no editMode flag, no all-widgets-edit-simultaneously)
- [x] **CLN-02**: All FABs removed (add widget, add page, delete page)
- [x] **CLN-03**: All tiny badge buttons removed (X delete, gear config, corner resize handle)
- [x] **CLN-04**: Replace global edit mode inactivity timer with per-widget selection auto-deselect timeout

## Future Requirements

### Deferred

- **RSZ-F01**: Tap-to-resize +/- buttons (Android 16 QPR3 pattern) — evaluate after edge handles ship
- **SEL-F01**: Widget name shown in navbar center during selection — evaluate if visual lift is insufficient feedback

## Out of Scope

| Feature | Reason |
|---------|--------|
| C++ interaction state machine | Start with QML state, escalate only if gesture races emerge |
| Drag-to-remove zone at top of screen | Navbar delete button replaces this — no floating remove target needed |
| Widget search in picker | Small widget count doesn't justify search UI |
| Animated navbar transitions | Instant swap preferred per pitfalls research — animation risks zone timing issues |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| SEL-01 | Phase 25 | Complete |
| SEL-02 | Phase 25 | Complete |
| SEL-03 | Phase 25 | Complete |
| SEL-04 | Phase 25 | Complete |
| NAV-01 | Phase 26 | Complete |
| NAV-02 | Phase 26 | Complete |
| NAV-03 | Phase 26 | Complete |
| NAV-04 | Phase 26 | Complete |
| NAV-05 | Phase 26 | Complete |
| RSZ-01 | Phase 26 | Complete |
| RSZ-02 | Phase 26 | Complete |
| RSZ-03 | Phase 26 | Complete |
| RSZ-04 | Phase 26 | Complete |
| PKR-01 | Phase 27 | Complete |
| PKR-02 | Phase 27 | Complete |
| PKR-03 | Phase 27 | Complete |
| PGM-01 | Phase 27 | Complete |
| PGM-02 | Phase 27 | Complete |
| PGM-03 | Phase 27 | Complete |
| PGM-04 | Phase 26 | Complete |
| CLN-01 | Phase 25 | Complete |
| CLN-02 | Phase 27 | Complete |
| CLN-03 | Phase 26 | Complete |
| CLN-04 | Phase 25 | Complete |

**Coverage:**
- v0.6.6 requirements: 24 total
- Mapped to phases: 24
- Unmapped: 0

---
*Requirements defined: 2026-03-21*
*Last updated: 2026-03-21 after roadmap revision*
