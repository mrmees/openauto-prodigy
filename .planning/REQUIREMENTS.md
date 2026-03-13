# Requirements: OpenAuto Prodigy v0.5.3

**Defined:** 2026-03-12
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## v0.5.3 Requirements

Requirements for Widget Grid & Content Widgets milestone. Each maps to roadmap phases.

### Grid Layout

- [x] **GRID-01**: Home screen uses a cell-based grid for widget placement (replaces fixed 3-pane layout)
- [x] **GRID-02**: Grid density (columns/rows) is auto-derived from display size via UiMetrics
- [x] **GRID-03**: Widgets render at integer column/row coordinates with column/row span values
- [x] **GRID-04**: Widgets declare min/max grid spans (replaces Main/Sub size enum)
- [x] **GRID-05**: Grid layout (positions, sizes, opacity) persists in YAML config across restarts
- [x] **GRID-06**: Fresh install ships a sensible default widget layout
- [x] **GRID-07**: Existing 3-pane config auto-migrates to grid coordinates on first launch
- [x] **GRID-08**: Widget layouts survive resolution changes — widgets that no longer fit are clamped/reflowed to valid positions

### Edit Mode

- [x] **EDIT-01**: Long-press on empty grid area or widget background enters screen-wide edit mode
- [x] **EDIT-02**: Edit mode shows visual feedback on all widgets (accent borders + resize handles)
- [x] **EDIT-03**: User can drag a widget to reposition it on the grid (snaps to cells on drop, blocked if target occupied)
- [x] **EDIT-04**: User can drag corner handles to resize a widget within its min/max span constraints
- [x] **EDIT-05**: "+" button (FAB) in edit mode opens widget catalog to add a new widget
- [x] **EDIT-06**: "X" badge on each widget in edit mode removes it from the grid
- [x] **EDIT-07**: Edit mode exits on tap outside any widget or after 10-second timeout (safety)
- [x] **EDIT-08**: Edit mode auto-exits when AA fullscreen activates (EVIOCGRAB steals touch)
- [x] **EDIT-09**: Layout writes are atomic — persist on drop/commit, not during drag
- [x] **EDIT-10**: Adding a widget when no space is available shows clear feedback (no silent failure)

### Multi-Page

- [x] **PAGE-01**: Home screen supports multiple widget pages with horizontal swipe navigation
- [x] **PAGE-02**: Page indicator dots show current page position and total count
- [x] **PAGE-03**: Launcher dock remains fixed across all pages
- [x] **PAGE-04**: Page swipe is disabled during edit mode (prevents gesture conflict with drag)
- [x] **PAGE-05**: Maximum 5 pages supported
- [x] **PAGE-06**: Explicit page creation and removal (add page when needed, remove empty pages)
- [x] **PAGE-07**: Page navigation available during edit mode via non-swipe mechanism (e.g. tap page dots)
- [x] **PAGE-08**: Non-visible pages are lazily instantiated (Pi 4 memory/performance)
- [x] **PAGE-09**: Page assignments persist in YAML config across restarts

### Navigation Widget

- [ ] **NAV-01**: AA navigation turn-by-turn widget shows maneuver icon, road name, and distance to next turn
- [ ] **NAV-02**: Maneuver icon renders phone-provided PNG (via QQuickImageProvider), falls back to Material icon
- [ ] **NAV-03**: Widget shows "No active navigation" with muted icon when nav is inactive
- [ ] **NAV-04**: Tapping the navigation widget activates AA fullscreen view
- [ ] **NAV-05**: Navigation data exposed to QML via C++ NavigationDataBridge with Q_PROPERTYs

### Unified Media

- [ ] **MEDIA-01**: Unified Now Playing widget shows track title, artist, and playback controls
- [ ] **MEDIA-02**: Widget displays AA media metadata when AA is connected, BT A2DP metadata as fallback
- [ ] **MEDIA-03**: Source indicator label shows "via Android Auto" or "via Bluetooth"
- [ ] **MEDIA-04**: Playback controls (prev/play-pause/next) map to appropriate source (AVRCP for BT, InputEvent for AA)
- [ ] **MEDIA-05**: AA media metadata exposed to QML via C++ MediaDataBridge (currently parsed but not surfaced)
- [ ] **MEDIA-06**: Replaces current BT-only NowPlayingWidget

### Widget Revision

- [x] **REV-01**: All shipped v0.5.3 widgets use pixel-based breakpoints for responsive layout (replaces isMainPane boolean)
- [x] **REV-02**: Clock widget adapts content across grid sizes (1x1: time only, 2x1: time+date, 2x2+: full)
- [x] **REV-03**: AA Status widget adapts content across grid sizes (1x1: icon only, 2x1+: icon+text)

## Future Requirements

Deferred to subsequent milestones.

### Lane Guidance
- **LANE-01**: Navigation widget shows lane guidance arrows when available
  - Reason: Lane arrow bitmap format in proto is uncharacterized. Needs investigation.

### Widget Polish
- **POLISH-01**: Widget catalog shows live scaled-down previews of each widget
- **POLISH-02**: AA album art displayed in Now Playing widget (if protocol provides image bytes)
- **POLISH-03**: Page reordering via drag on page indicator dots

### Grid Enhancements
- **GRIDENH-01**: Grid density user choice in Display settings (sparse/dense override)
- **GRIDENH-02**: Auto-arrange finds first available space when target cells are occupied

## Out of Scope

| Feature | Reason |
|---------|--------|
| Freeform pixel placement (non-grid) | Looks terrible on small screens, alignment becomes user's problem |
| Widget overlap/stacking | Z-order confusion and tap ambiguity, violates automotive glanceability |
| Widget-specific settings dialogs | Over-engineering for built-in widgets — YAML config sufficient |
| Infinite pages | Memory bloat, navigation confusion — capped at 5 |
| Horizontal scrolling within widgets | Conflicts with page swipe gesture |
| App shortcut widgets | Launcher dock handles app launching |
| Real-time map in nav widget | GPU-expensive on Pi, useless at glance distance — AA fullscreen IS the map |
| Widget-to-widget communication | Coupling nightmare for solo maintainer |
| Drag widgets between pages | Complex gesture, easy to trigger accidentally — use remove+add instead |
| Animated widget transitions while driving | Distracting — NHTSA/Google Design for Driving guidance |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| GRID-01 | Phase 04 | Complete |
| GRID-02 | Phase 04 | Complete |
| GRID-03 | Phase 05 | Complete |
| GRID-04 | Phase 04 | Complete |
| GRID-05 | Phase 04 | Complete |
| GRID-06 | Phase 05 | Complete |
| GRID-07 | Phase 04 | Complete |
| GRID-08 | Phase 04 | Complete |
| EDIT-01 | Phase 07 | Complete |
| EDIT-02 | Phase 07 | Complete |
| EDIT-03 | Phase 07 | Complete |
| EDIT-04 | Phase 07 | Complete |
| EDIT-05 | Phase 07 | Complete |
| EDIT-06 | Phase 07 | Complete |
| EDIT-07 | Phase 07 | Complete |
| EDIT-08 | Phase 07 | Complete |
| EDIT-09 | Phase 07 | Complete |
| EDIT-10 | Phase 07 | Complete |
| PAGE-01 | Phase 08 | Complete |
| PAGE-02 | Phase 08 | Complete |
| PAGE-03 | Phase 08 | Complete |
| PAGE-04 | Phase 08 | Complete |
| PAGE-05 | Phase 08 | Complete |
| PAGE-06 | Phase 08 | Complete |
| PAGE-07 | Phase 08 | Complete |
| PAGE-08 | Phase 08 | Complete |
| PAGE-09 | Phase 08 | Complete |
| NAV-01 | Phase 06 | Pending |
| NAV-02 | Phase 06 | Pending |
| NAV-03 | Phase 06 | Pending |
| NAV-04 | Phase 06 | Pending |
| NAV-05 | Phase 06 | Pending |
| MEDIA-01 | Phase 06 | Pending |
| MEDIA-02 | Phase 06 | Pending |
| MEDIA-03 | Phase 06 | Pending |
| MEDIA-04 | Phase 06 | Pending |
| MEDIA-05 | Phase 06 | Pending |
| MEDIA-06 | Phase 06 | Pending |
| REV-01 | Phase 05 | Complete |
| REV-02 | Phase 05 | Complete |
| REV-03 | Phase 05 | Complete |

**Coverage:**
- v0.5.3 requirements: 42 total
- Mapped to phases: 42
- Unmapped: 0 ✓

---
*Requirements defined: 2026-03-12*
*Last updated: 2026-03-12 after Codex review — added GRID-08, EDIT-08/09/10, PAGE-06/07/08/09; split Phase 04; renumbered phases*
