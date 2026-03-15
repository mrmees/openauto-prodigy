---
phase: 04
slug: grid-data-model
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-12
---

# Phase 04 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | QtTest (Qt 6.4+) |
| **Config file** | tests/CMakeLists.txt (oap_add_test helper) |
| **Quick run command** | `cd build && ctest -R "test_widget_grid\|test_grid_dimensions\|test_yaml_config\|test_widget" -j4 --output-on-failure` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && cmake --build . -j$(nproc) && ctest -R "test_widget_grid\|test_grid_dimensions\|test_yaml_config\|test_widget" -j4 --output-on-failure`
- **After every plan wave:** Run `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 04-01-01 | 01 | 0 | GRID-01 | unit | `ctest -R test_widget_grid_model --output-on-failure` | ❌ W0 | ⬜ pending |
| 04-01-02 | 01 | 0 | GRID-02 | unit | `ctest -R test_grid_dimensions --output-on-failure` | ❌ W0 | ⬜ pending |
| 04-01-03 | 01 | 1 | GRID-04 | unit | `ctest -R test_widget_types --output-on-failure` | ✅ needs update | ⬜ pending |
| 04-01-04 | 01 | 1 | GRID-01 | unit | `ctest -R test_widget_grid_model --output-on-failure` | ❌ W0 | ⬜ pending |
| 04-01-05 | 01 | 1 | GRID-08 | unit | `ctest -R test_widget_grid_model --output-on-failure` | ❌ W0 | ⬜ pending |
| 04-01-06 | 01 | 2 | GRID-05 | unit | `ctest -R test_yaml_config --output-on-failure` | ✅ needs update | ⬜ pending |
| 04-01-07 | 01 | 2 | GRID-07 | unit | `ctest -R test_yaml_config --output-on-failure` | ✅ needs update | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_widget_grid_model.cpp` — new test file covering GRID-01 (placement, occupancy, collision) and GRID-08 (clamping/reflow)
- [ ] `tests/test_grid_dimensions.cpp` — new test file covering GRID-02 (grid density formula for known display sizes)
- [ ] Update `tests/test_widget_types.cpp` — remove WidgetSize enum tests, add min/max/default span constraint tests
- [ ] Update `tests/test_widget_registry.cpp` — replace widgetsForSize tests with space-based filtering
- [ ] Update `tests/test_widget_config.cpp` — update for new widget_grid YAML schema
- [ ] Update `tests/test_widget_instance_context.cpp` — replace paneSize with grid dimensions

*Existing infrastructure covers test framework and CMake helpers.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Grid density looks correct on 1024x600 Pi display | GRID-02 | Visual verification of cell sizing on target hardware | Launch on Pi, count visible grid cells, verify 6x4 |
| Default layout renders sensibly on fresh install | GRID-05 | Visual — need to see Clock, AA Status, Now Playing in correct positions | Delete config, launch, verify widget positions |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
