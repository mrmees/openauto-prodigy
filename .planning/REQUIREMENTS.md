# Requirements — v0.6.4 Widget Work

**Defined:** 2026-03-16
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## Widget Contract (WC)

- [x] **WC-01**: Widget placements support per-instance configuration (key-value map persisted in YAML)
- [x] **WC-02**: Widget config is editable via a host-rendered config sheet opened from a gear icon in edit mode
- [x] **WC-03**: Widget config changes persist across restarts and survive grid remap

## Theme Cycle Widget (TC)

- [x] **TC-01**: 1x1 widget that advances to the next theme on tap
- [x] **TC-02**: Widget displays current theme name or color preview

## Weather Widget (WX)

- [ ] **WX-01**: Pi fetches current weather conditions via HTTP API using companion GPS location
- [ ] **WX-02**: Weather widget displays temperature, condition icon, and location name
- [ ] **WX-03**: Weather widget displays active weather alerts when present
- [ ] **WX-04**: Weather data refreshes on a configurable interval (default 30 min)
- [ ] **WX-05**: API key is configured via YAML config (not hardcoded)

## Battery Widget (BW)

- [x] **BW-01**: 1x1 widget displays phone battery level and charging state from companion data

## Clock Styles (CK)

- [ ] **CK-01**: Clock widget supports at least 3 visual styles (digital, analog, minimal)
- [ ] **CK-02**: Clock style is selectable per widget instance via the widget config UI

## Companion Status Widget (CS)

- [x] **CS-01**: 1x1 widget shows companion connected/disconnected indicator
- [x] **CS-02**: At larger sizes, widget shows GPS, battery, proxy status detail

## AA Focus Toggle (AF)

- [x] **AF-01**: Widget sends VideoFocusRequest(NATIVE) to pause AA projection on tap
- [x] **AF-02**: Widget sends VideoFocusRequest(PROJECTED) to resume AA on tap
- [x] **AF-03**: Widget reflects current focus state (native vs projected)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Weather forecast (multi-day) | Current conditions + alerts is sufficient for v0.6.4 |
| Widget drag-to-configure | Config UI via context menu is simpler and consistent with existing edit mode |
| Per-widget theme override | Theme is global; per-widget theming is excessive complexity |
| Companion app SOCKS logging | Documented gap from v0.6.3, separate repo/timeline |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| WC-01 | Phase 19 | Complete |
| WC-02 | Phase 19 | Complete |
| WC-03 | Phase 19 | Complete |
| TC-01 | Phase 20 | Complete |
| TC-02 | Phase 20 | Complete |
| BW-01 | Phase 20 | Complete |
| CS-01 | Phase 20 | Complete |
| CS-02 | Phase 20 | Complete |
| AF-01 | Phase 20 | Complete |
| AF-02 | Phase 20 | Complete |
| AF-03 | Phase 20 | Complete |
| CK-01 | Phase 21 | Pending |
| CK-02 | Phase 21 | Pending |
| WX-01 | Phase 21 | Pending |
| WX-02 | Phase 21 | Pending |
| WX-03 | Phase 21 | Pending |
| WX-04 | Phase 21 | Pending |
| WX-05 | Phase 21 | Pending |

**Coverage:**
- v0.6.4 requirements: 18 total
- Mapped to phases: 18
- Unmapped: 0

---
*Requirements defined: 2026-03-16*
