---
phase: 07
slug: edit-mode
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-12
---

# Phase 07 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt Test (QtTest) |
| **Config file** | tests/CMakeLists.txt (`oap_add_test()` helper) |
| **Quick run command** | `cd build && ctest -R widget_grid -j$(nproc) --output-on-failure` |
| **Full suite command** | `cd build && ctest -j$(nproc) --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest -R widget_grid -j$(nproc) --output-on-failure`
- **After every plan wave:** Run `cd build && ctest -j$(nproc) --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 07-01-XX | 01 | 1 | EDIT-01 | manual-only | N/A — QML interaction | N/A | ⬜ pending |
| 07-01-XX | 01 | 1 | EDIT-02 | manual-only | N/A — QML visual | N/A | ⬜ pending |
| 07-02-XX | 02 | 1 | EDIT-03 | unit | `ctest -R widget_grid --output-on-failure` | ✅ moveWidget tests | ⬜ pending |
| 07-02-XX | 02 | 1 | EDIT-04 | unit | `ctest -R widget_grid --output-on-failure` | ✅ resizeWidget tests | ⬜ pending |
| 07-03-XX | 03 | 1 | EDIT-05 | unit | `ctest -R widget_grid --output-on-failure` | ❌ W0 findFirstAvailableCell | ⬜ pending |
| 07-03-XX | 03 | 1 | EDIT-06 | unit | `ctest -R widget_grid --output-on-failure` | ✅ removeWidget tests | ⬜ pending |
| 07-01-XX | 01 | 1 | EDIT-07 | manual-only | N/A — QML timer | N/A | ⬜ pending |
| 07-01-XX | 01 | 1 | EDIT-08 | manual-only | N/A — signal/slot, Pi-only | N/A | ⬜ pending |
| 07-02-XX | 02 | 1 | EDIT-09 | unit | `ctest -R widget_grid --output-on-failure` | ✅ placementsChanged tests | ⬜ pending |
| 07-03-XX | 03 | 1 | EDIT-10 | unit | `ctest -R widget_grid --output-on-failure` | ❌ W0 canPlace-full-grid | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_widget_grid_model.cpp` — add `testFindFirstAvailableCell` for auto-place (EDIT-05)
- [ ] `tests/test_widget_grid_model.cpp` — add `testMinMaxConstraintRoles` for constraint data roles (EDIT-04)
- [ ] `tests/test_widget_grid_model.cpp` — add `testCanPlaceFullGrid` for no-space detection (EDIT-10)

*Existing infrastructure covers EDIT-03, EDIT-06, EDIT-09 with 20 existing tests.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Long-press enters edit mode | EDIT-01 | QML touch interaction, no C++ state to test | Long-press empty grid area → verify accent borders + handles appear on all widgets |
| Visual feedback (borders, handles, grid dots) | EDIT-02 | Visual-only, no logic to unit test | Verify accent border, resize handle, X badge, grid dots visible in edit mode |
| Exit on tap outside | EDIT-07 | QML interaction test | Tap empty grid area in edit mode → verify edit mode exits |
| 10s inactivity timeout | EDIT-07 | Timer-based, no touch for 10s | Enter edit mode, wait 10s → verify edit mode exits |
| AA fullscreen auto-exit | EDIT-08 | Requires AA connection on Pi | Enter edit mode, connect phone → verify edit mode exits when AA activates |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
