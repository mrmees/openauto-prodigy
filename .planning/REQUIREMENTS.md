# Requirements — v0.6.5 Widget Refinement

**Defined:** 2026-03-20
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## Widget Refinement (WR)

- [ ] **WR-01**: Every widget renders correctly at all valid span combinations on the DFRobot 7" (1024x600) display
- [ ] **WR-02**: Every widget renders correctly at all valid span combinations on the Pi Official 7" (800x480) display
- [ ] **WR-03**: Widget text and icons are legible at 1x1 minimum span on both target displays
- [ ] **WR-04**: All user-requested visual refinements from the preview review session are applied

## Date Widget (DT)

- [ ] **DT-01**: Standalone date widget shows day-of-week and date
- [ ] **DT-02**: Date widget has responsive breakpoints (1x1 compact → larger spans show more detail)
- [ ] **DT-03**: Date widget is available in the widget picker with appropriate metadata

## Clock Cleanup (CL)

- [ ] **CL-01**: Clock widget (all 3 styles) no longer displays date or day-of-week at any span
- [ ] **CL-02**: Clock widget layout is adjusted for time-only display (no wasted space where date was)

## Hardware Verification (HW)

- [ ] **HW-01**: All widgets verified on Pi hardware after refinement pass

## Out of Scope

| Feature | Reason |
|---------|--------|
| New widget types beyond date | Can add via /gsd:insert-phase if needed |
| Widget preview tool improvements | Dev tooling, not product scope |
| Config schema changes | No new config fields needed for visual refinements |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| WR-01 | TBD | Pending |
| WR-02 | TBD | Pending |
| WR-03 | TBD | Pending |
| WR-04 | TBD | Pending |
| DT-01 | TBD | Pending |
| DT-02 | TBD | Pending |
| DT-03 | TBD | Pending |
| CL-01 | TBD | Pending |
| CL-02 | TBD | Pending |
| HW-01 | TBD | Pending |

**Coverage:**
- v0.6.5 requirements: 10 total
- Mapped to phases: 0
- Unmapped: 10

---
*Requirements defined: 2026-03-20*
