---
phase: 10
slug: launcher-widget-dock-removal
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-14
---

# Phase 10 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + Google Test |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd build && ctest --output-on-failure -R "widget_grid\|widget_picker\|widget_types\|display_info\|yaml_config"` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick run command (widget/grid/config tests)
- **After every plan wave:** Run full suite
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 10-01-01 | 01 | 1 | NAV-01 | unit | `ctest -R widget_types` | ❌ W0 | ⬜ pending |
| 10-01-02 | 01 | 1 | NAV-01 | unit | `ctest -R widget_picker` | ✅ | ⬜ pending |
| 10-02-01 | 02 | 1 | NAV-02,NAV-03 | unit+build | `ctest -R widget_grid` | ✅ | ⬜ pending |
| 10-02-02 | 02 | 1 | NAV-02 | build | `cmake --build build` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_widget_types.cpp` — add singleton flag tests
- [ ] `tests/test_widget_grid_model.cpp` — add reserved page / singleton removal guard tests

*Existing test infrastructure covers remaining requirements.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Settings widget tap opens settings view | NAV-01 | Requires Qt event loop + QML | Tap settings widget on Pi, verify settings screen appears |
| AA widget tap activates AA plugin | NAV-01 | Requires plugin system + QML | Tap AA widget on Pi, verify AA view activates |
| Reserved page survives page add/delete | NAV-01 | Multi-page interaction on device | Add pages, delete pages, verify reserved page always last |
| All dock views reachable without dock | NAV-02 | End-to-end reachability QA | Navigate to every view (settings, AA) without dock; verify via touch only |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
