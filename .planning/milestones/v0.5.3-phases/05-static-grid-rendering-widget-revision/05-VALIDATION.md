---
phase: 05
slug: static-grid-rendering-widget-revision
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-12
---

# Phase 05 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt Test (QTest) |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd build && ctest --output-on-failure -R "widget_grid_model\|display_info\|grid_dimensions"` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure -R "widget_grid\|display_info\|grid_dim"`
- **After every plan wave:** Run `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 05-01-01 | 01 | 1 | GRID-03 | manual | Visual inspection on dev VM + Pi | N/A | ⬜ pending |
| 05-01-02 | 01 | 1 | GRID-06 | unit | `ctest -R widget_grid_model` | ✅ | ⬜ pending |
| 05-02-01 | 02 | 1 | REV-01 | manual | Visual inspection at multiple cell sizes | N/A | ⬜ pending |
| 05-02-02 | 02 | 1 | REV-02 | manual | Visual inspection: `--geometry 800x480` vs `--geometry 1024x600` | N/A | ⬜ pending |
| 05-02-03 | 02 | 1 | REV-03 | manual | Visual inspection with geometry flag | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

*Existing infrastructure covers all phase requirements.*

Existing `widget_grid_model`, `display_info`, and `grid_dimensions` tests cover the data model layer. Widget revision requirements (REV-01/02/03) are visual-only QML changes that require manual verification on the target display.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Widgets render at grid positions | GRID-03 | QML visual layout — no automated pixel verification | Run app on dev VM, verify widgets appear at correct grid positions |
| Clock breakpoints | REV-02 | Visual layout adaptation — needs human judgment | Run with `--geometry 800x480` and `--geometry 1024x600`, verify time-only at 1x1, time+date at 2x1, full at 2x2+ |
| AA Status breakpoints | REV-03 | Visual layout adaptation — needs human judgment | Run with multiple geometries, verify icon-only at 1x1, icon+text at 2x1+ |
| Pixel breakpoints replace isMainPane | REV-01 | Visual — confirm all widgets use pixel dimensions not pane identity | Resize window, verify widgets adapt based on cell pixel size |
| Default layout on fresh install | GRID-06 | Visual confirmation + unit test | Delete config, launch app, verify sensible widget arrangement |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
