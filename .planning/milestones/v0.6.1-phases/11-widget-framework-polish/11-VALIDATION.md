---
phase: 11
slug: widget-framework-polish
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-14
---

# Phase 11 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | QtTest 6.8.2 |
| **Config file** | `tests/CMakeLists.txt` (uses `oap_add_test()` helper) |
| **Quick run command** | `cd build && ctest --output-on-failure -R widget` |
| **Full suite command** | `cd build && ctest --output-on-failure -E test_navigation_data_bridge` |
| **Estimated runtime** | ~15 seconds |
| **Baseline exclusion** | `test_navigation_data_bridge` — pre-existing imperial unit failures, out of Phase 11 scope |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest --output-on-failure -R "widget|test_widget"` (widget-related tests)
- **After every plan wave:** Run `cd build && ctest --output-on-failure -E test_navigation_data_bridge`
- **Before `/gsd:verify-work`:** Full suite must be green (excluding baseline exclusion)
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 11-01-01 | 01 | 1 | WF-02 | unit | `ctest --output-on-failure -R test_widget_grid_model` | Partial — need below-min test | ⬜ pending |
| 11-01-01 | 01 | 1 | WF-03 | unit | `ctest --output-on-failure -R test_widget_instance_context` | Partial — need span + isCurrentPage tests | ⬜ pending |
| 11-01-01 | 01 | 1 | WF-03 | integration | `ctest --output-on-failure -R test_widget_contract_qml` | ❌ New file (Plan 01 Task 1 creates it) | ⬜ pending |
| 11-01-02 | 01 | 1 | WF-02, WF-03 | build | `ctest --output-on-failure -E test_navigation_data_bridge` | N/A | ⬜ pending |
| 11-02-01 | 02 | 2 | WF-04 | grep audit | `grep -c "width >=" qml/widgets/*.qml` + `grep -c "height >=" qml/widgets/*.qml` — both must return 0 | N/A | ⬜ pending |
| 11-02-02 | 02 | 2 | WF-04 | grep audit | `grep -l "widgetContext" qml/widgets/*.qml` — must return 6 files | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_widget_grid_model.cpp` — add test for resize below minCols/minRows being rejected
- [ ] `tests/test_widget_instance_context.cpp` — add tests for colSpan/rowSpan properties, isCurrentPage flag, and NOTIFY signals
- [ ] `tests/test_widget_contract_qml.cpp` — **new file** — QML integration test for context injection + live binding propagation

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Span-based layout reflow | WF-04 | Visual verification on actual display | Resize widgets on Pi, confirm layout adapts via span thresholds |
| Placeholder-state rendering | WF-04 | Visual check for muted icon + text at 40% opacity | Disconnect BT on Pi, verify Now Playing / Navigation show placeholder |
| HomeMenu Loader injection | WF-03 | Automated test uses Loader but not full HomeMenu context | On Pi: verify widget displays update when switching pages (isCurrentPage) and resizing (colSpan/rowSpan) |
| pressAndHold forwarding | WF-04 | Grep confirms string presence but not runtime host forwarding | On Pi: 1. Long-press on Now Playing control area (play/prev/next) — must trigger context menu, not playback action. 2. Long-press on AA Status widget — must trigger context menu. 3. Long-press on Clock widget background — must trigger context menu. |
| Cross-build + Pi sanity | All | Binary must run on aarch64 target | `./cross-build.sh` succeeds; rsync to Pi; service starts; widgets render correctly |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
