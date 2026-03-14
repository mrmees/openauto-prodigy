---
phase: 09
slug: widget-descriptor-grid-foundation
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-14
---

# Phase 09 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt Test (QTest) 6.8.2 |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd build && ctest -R 'test_display_info\|test_widget_grid_model\|test_grid_dimensions\|test_widget_types\|test_yaml_config' --output-on-failure` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick run command (phase-relevant tests)
- **After every plan wave:** Run full suite command
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 09-01-01 | 01 | 1 | WF-01 | unit | `ctest -R test_widget_types --output-on-failure` | Yes (update) | pending |
| 09-01-02 | 01 | 1 | WF-01 | unit | `ctest -R test_widget_picker --output-on-failure` | No -- W0 | pending |
| 09-02-01 | 02 | 1 | GL-01 | unit | `ctest -R test_display_info --output-on-failure` | Yes (rewrite) | pending |
| 09-02-02 | 02 | 1 | GL-01 | unit | `ctest -R test_grid_dimensions --output-on-failure` | Yes (rewrite) | pending |
| 09-02-03 | 02 | 1 | GL-02 | unit | `ctest -R test_display_info --output-on-failure` | Yes (update) | pending |
| 09-03-01 | 03 | 2 | GL-03 | unit | `ctest -R test_yaml_config --output-on-failure` | Yes (new cases) | pending |
| 09-03-02 | 03 | 2 | GL-03 | unit | `ctest -R test_widget_grid_remap --output-on-failure` | No -- W0 | pending |
| 09-03-03 | 03 | 2 | GL-03 | unit | `ctest -R test_widget_grid_remap --output-on-failure` | No -- W0 | pending |

*Status: pending · green · red · flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_widget_picker_model.cpp` — stubs for WF-01 (CategoryRole, DescriptionRole, category grouping)
- [ ] `tests/test_widget_grid_remap.cpp` — stubs for GL-03 (proportional remap, nudge, page spill, base snapshot)
- [ ] Update `tests/test_display_info.cpp` — rewrite assertions for cellSide instead of gridColumns/gridRows
- [ ] Update `tests/test_grid_dimensions.cpp` — rewrite assertions for new divisor-based grid dims
- [ ] Update `tests/test_widget_grid_model.cpp` — update assertions depending on old grid dim defaults

*Existing infrastructure covers test framework and CMake setup.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Grid visually consistent across displays | GL-01 | Requires physical display comparison | Run with --geometry 800x480, 1024x600, 1920x1080 and visually compare cell sizes |
| Dock overlaps grid correctly | GL-02 | Visual z-order check | Verify dock renders on top of grid bottom row |
| Picker grouped layout is glanceable | WF-01 | UX/visual check | Open picker in edit mode, verify category headers and card layout |
| Boot sequence no flicker | GL-01 | Timing-dependent visual | Cold start app, watch for grid flash/resize on initial render |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
