# Requirements: OpenAuto Prodigy v0.4.4

**Defined:** 2026-03-03
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.

## v1 Requirements

Requirements for v0.4.4 Scalable UI. Each maps to roadmap phases.

### Config Overrides

- [x] **CFG-01**: YAML config supports override for global UI scale factor (e.g. `ui.scale: 1.2`)
- [x] **CFG-02**: YAML config supports override for font scale factor independent of layout scale (e.g. `ui.fontScale: 1.1`)
- [x] **CFG-03**: YAML config supports override for individual UiMetrics tokens (e.g. `ui.tokens.rowH: 48`)
- [x] **CFG-04**: UiMetrics applies config overrides after auto-derived values, so user always wins

### UiMetrics Foundation

- [x] **SCALE-01**: UiMetrics scale factor is unclamped -- derives freely from actual screen dimensions
- [x] **SCALE-02**: UiMetrics uses dual-axis scaling -- `min(scaleH, scaleV)` for layout, geometric mean for fonts
- [x] **SCALE-03**: UiMetrics derives scale from window dimensions (not Screen.* which is unreliable at Wayland init)
- [x] **SCALE-04**: Font tokens have pixel floors (min 12px for smallest) to guarantee automotive legibility
- [x] **SCALE-05**: New UiMetrics tokens added for currently-missing dimensions (trackThick, trackThin, knobSize)

### QML Audit

- [ ] **AUDIT-01**: NormalText and SpecialText use UiMetrics font tokens instead of hardcoded pixelSize
- [x] **AUDIT-02**: TopBar and NavStrip margins/spacing/radius use UiMetrics tokens
- [ ] **AUDIT-03**: Sidebar icon sizes, font sizes, thumb dimensions, and margins use UiMetrics tokens
- [ ] **AUDIT-04**: GestureOverlay font sizes, spacing, and dimensions use UiMetrics tokens
- [ ] **AUDIT-05**: PhoneView font sizes, button dimensions, and spacing use UiMetrics tokens
- [ ] **AUDIT-06**: IncomingCallOverlay font sizes, spacing, and button dimensions use UiMetrics tokens
- [ ] **AUDIT-07**: BtAudioView font sizes, album art dimensions, and spacing use UiMetrics tokens
- [ ] **AUDIT-08**: HomeMenu font sizes use UiMetrics tokens
- [ ] **AUDIT-09**: Tile and PairingDialog radius and dimensions use UiMetrics tokens
- [ ] **AUDIT-10**: Zero hardcoded pixel values remain in QML files (excluding intentional dev debug overlays)

### Layout Adaptation

- [ ] **LAYOUT-01**: LauncherMenu tile grid derives width from available container space (no overflow at 800px)
- [ ] **LAYOUT-02**: Settings tile grid derives dimensions from available width (no clipping at 800x480)
- [ ] **LAYOUT-03**: EQ band sliders have minimum height guard at small resolutions

### Touch Pipeline

- [x] **TOUCH-01**: EvdevTouchReader sidebar hit zones are derived from display config (not hardcoded 1024x600 magic values)
- [x] **TOUCH-02**: YamlConfig display dimensions auto-detected from actual screen or validated against it

### Runtime Adaptation

- [ ] **ADAPT-01**: App auto-detects display resolution on startup and adjusts UiMetrics accordingly (no manual config required)
- [ ] **ADAPT-02**: App responds dynamically if window dimensions change at runtime (e.g. compositor resize)
- [ ] **ADAPT-03**: Portrait/landscape display orientation setting removed — app derives orientation from detected dimensions
- [ ] **ADAPT-04**: Brightness detection adapts to display hardware (existing 3-tier sysfs > ddcutil > software)

## v2 Requirements

Deferred to future milestone. Tracked but not in current roadmap.

### User Scale Options

- **USCALE-01**: User can select UI scale preset (Small / Medium / Large) from Display settings
- **USCALE-02**: Scale preset overrides auto-derived scale factor with user preference
- **USCALE-03**: Scale change applies immediately without restart

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Multi-display support | Single display target; just different resolutions on that display |
| System resolution control from app | User owns the display config; app adapts to whatever they set |
| Manual display resolution/orientation settings | App detects these automatically -- no user config needed |
| Fractional DPI / HiDPI scaling | Pi displays are all 1x DPR; HiDPI is a different problem |
| Custom icon sets per resolution | Material Symbols scale fine via font metrics |
| Separate hscale/vscale consumer API | Adds complexity; min(h,v) is sufficient for v0.4.4 -- ultrawide handled by geometric mean for fonts |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CFG-01 | Phase 1 | Complete |
| CFG-02 | Phase 1 | Complete |
| CFG-03 | Phase 1 | Complete |
| CFG-04 | Phase 1 | Complete |
| SCALE-01 | Phase 2 | Complete |
| SCALE-02 | Phase 2 | Complete |
| SCALE-03 | Phase 2 | Complete |
| SCALE-04 | Phase 2 | Complete |
| SCALE-05 | Phase 2 | Complete |
| TOUCH-01 | Phase 2 | Complete |
| TOUCH-02 | Phase 2 | Complete |
| AUDIT-01 | Phase 3 | Pending |
| AUDIT-02 | Phase 3 | Complete |
| AUDIT-03 | Phase 3 | Pending |
| AUDIT-04 | Phase 3 | Pending |
| AUDIT-05 | Phase 3 | Pending |
| AUDIT-06 | Phase 3 | Pending |
| AUDIT-07 | Phase 3 | Pending |
| AUDIT-08 | Phase 3 | Pending |
| AUDIT-09 | Phase 3 | Pending |
| AUDIT-10 | Phase 3 | Pending |
| LAYOUT-01 | Phase 4 | Pending |
| LAYOUT-02 | Phase 4 | Pending |
| LAYOUT-03 | Phase 4 | Pending |
| ADAPT-01 | Phase 5 | Pending |
| ADAPT-02 | Phase 5 | Pending |
| ADAPT-03 | Phase 5 | Pending |
| ADAPT-04 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 28 total
- Mapped to phases: 28
- Unmapped: 0

---
*Requirements defined: 2026-03-03*
*Last updated: 2026-03-03 after roadmap creation*
