---
phase: 08
slug: multi-page
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-13
---

# Phase 08 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt Test 6.4+ (CTest runner) |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd build && ctest -R test_widget_grid --output-on-failure` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && cmake --build . -j$(nproc) && ctest -R test_widget_grid --output-on-failure`
- **After every plan wave:** Run `cd build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 08-01-xx | 01 | 1 | PAGE-05 | unit | `ctest -R test_widget_grid` | ❌ W0 | ⬜ pending |
| 08-01-xx | 01 | 1 | PAGE-06 | unit | `ctest -R test_widget_grid` | ❌ W0 | ⬜ pending |
| 08-01-xx | 01 | 1 | PAGE-09 | unit | `ctest -R test_yaml_config` | ❌ W0 | ⬜ pending |
| 08-02-xx | 02 | 2 | PAGE-01 | manual | Pi touchscreen test | N/A | ⬜ pending |
| 08-02-xx | 02 | 2 | PAGE-02 | manual | Visual verification | N/A | ⬜ pending |
| 08-02-xx | 02 | 2 | PAGE-03 | manual | Visual verification | N/A | ⬜ pending |
| 08-02-xx | 02 | 2 | PAGE-04 | manual | Pi touchscreen test | N/A | ⬜ pending |
| 08-02-xx | 02 | 2 | PAGE-07 | manual | Pi touchscreen test | N/A | ⬜ pending |
| 08-02-xx | 02 | 2 | PAGE-08 | manual | Performance check on Pi | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_widget_grid_model.cpp` — extend with page-scoped tests: canPlace per page, addPage, removePage, page count, page-filtered occupancy (PAGE-05, PAGE-06)
- [ ] `tests/test_yaml_config.cpp` — add page field serialization roundtrip test (PAGE-09)

*Existing test infrastructure covers framework setup. Only new test cases needed.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Horizontal swipe between pages | PAGE-01 | Requires Pi touchscreen + visual | Swipe left/right on home screen, verify smooth page transition |
| Page indicator dots | PAGE-02 | Visual verification | Check dots visible, active dot highlighted, count matches pages |
| Dock fixed across pages | PAGE-03 | Visual verification | Swipe between pages, verify dock stays in place |
| Swipe disabled in edit mode | PAGE-04 | Requires Pi touch + edit mode | Long-press to edit, try to swipe — should not change page |
| Dot tap navigation in edit mode | PAGE-07 | Requires Pi touch interaction | In edit mode, tap dots to change page |
| Lazy instantiation performance | PAGE-08 | Requires Pi memory/performance monitoring | Create 5 pages, check memory usage stays reasonable |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
