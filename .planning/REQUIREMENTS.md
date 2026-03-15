# Requirements — v0.6.2 Theme Expression & Wallpaper Scaling

## Wallpaper (WP)

- [x] **WP-01**: Wallpaper Image caps decoded texture to display dimensions via sourceSize, clips overflow, retains previous image during transitions, and always fills the screen without letterboxing

## Companion Bugfix (BF)

- [x] **BF-01**: New companion connection replaces stale client instead of rejecting, with diagnostic logging
- [x] **BF-02**: Socket errorOccurred triggers full session cleanup via shared idempotent path
- [x] **BF-03**: 30-second inactivity timeout (from last valid status message) triggers session teardown
- [x] **BF-04**: clearClientSession() is idempotent — double-call safe, no double-emit, no crash

## Theme Persistence Bugfix (TP)

- [x] **TP-01**: ThemeService::setTheme() persists theme ID to config.yaml via IConfigService so theme survives restart
- [x] **TP-02**: Wallpaper override picker gated behind "Custom Wallpaper" toggle in ThemeSettings.qml — OFF hides picker and clears override, ON reveals picker
- [x] **TP-03**: Delete Theme button relocated to bottom of theme settings list to prevent accidental taps

## Color Audit (CA)

- [ ] **CA-01**: All QML surfaces using accent-colored backgrounds have matching `on-*` foreground tokens (e.g., primary background uses onPrimary text, not onSurface)
- [ ] **CA-02**: NavbarControl active/pressed state uses primaryContainer fill with onPrimaryContainer foreground
- [ ] **CA-03**: Widget cards, settings tiles, and interactive controls use bolder accent colors from M3 Expressive palette instead of neutral-only surfaces

## Color Boldness (CB)

- [ ] **CB-01**: ThemeService exposes a colorBoldness property (0-1) that amplifies accent role saturation without affecting neutral/surface roles
- [ ] **CB-02**: User can adjust color boldness via slider in Display settings, persisted across restarts

## Out of Scope

| Feature | Reason |
|---------|--------|
| Wallpaper fill mode selector | Always cover-fit, no user choice — prevents letterboxing |
| Bundled theme YAML changes | Themes built via companion app and imported, not shipped in binary |
| HCT color math (material-color-utilities port) | HSL saturation multiplier is sufficient; companion app handles proper HCT |
| Stretch fill mode | Distorts images — anti-feature |
| Color boldness on neutral/surface roles | Would break glass widget card aesthetic |
| Theme import fix | Separate investigation — may be companion-side or protocol mismatch |
| Manual "clear companion session" UI | Not needed with always-replace logic |
| Companion protocol versioning | Future resilience, not urgent |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| WP-01 | Phase 13 | Complete |
| BF-01 | Phase 13.1 | Complete |
| BF-02 | Phase 13.1 | Complete |
| BF-03 | Phase 13.1 | Complete |
| BF-04 | Phase 13.1 | Complete |
| TP-01 | Phase 13.2 | Complete |
| TP-02 | Phase 13.2 | Complete |
| TP-03 | Phase 13.2 | Complete |
| CA-01 | Phase 14 | Pending |
| CA-02 | Phase 14 | Pending |
| CA-03 | Phase 14 | Pending |
| CB-01 | Phase 15 | Pending |
| CB-02 | Phase 15 | Pending |

**Coverage:**
- v0.6.2 requirements: 13 total
- Mapped to phases: 13
- Unmapped: 0

---
*Requirements defined: 2026-03-15*
*Last updated: 2026-03-15 — TP requirements added for Phase 13.2*
