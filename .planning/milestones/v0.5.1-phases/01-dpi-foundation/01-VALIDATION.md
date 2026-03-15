---
phase: 1
slug: dpi-foundation
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt Test (QTest) |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd build && ctest --output-on-failure -R "test_display_info\|test_yaml_config"` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **After every plan wave:** Run `cd build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| TBD | 01 | 0 | DPI-02 | unit | `ctest -R test_yaml_config` | ❌ W0 | ⬜ pending |
| TBD | 01 | 0 | DPI-03 | unit | `ctest -R test_display_info` | ❌ W0 | ⬜ pending |
| TBD | 01 | 0 | DPI-04 | unit | `ctest -R test_config_service` | ❌ W0 | ⬜ pending |
| TBD | 01 | 1 | DPI-01 | manual-only | N/A | N/A | ⬜ pending |
| TBD | 01 | 1 | DPI-03 | manual-only | N/A | N/A | ⬜ pending |
| TBD | 01 | 1 | DPI-04 | manual-only | N/A | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_display_info.cpp` — add tests for `screenSizeInches`, `computedDpi`, `setScreenSizeInches`
- [ ] `tests/test_yaml_config.cpp` — add test for `display.screen_size` default value (7.0)
- [ ] `tests/test_config_service.cpp` — add test for `display.screen_size` read/write via ConfigService

*Existing test infrastructure (CMake, QTest) covers framework needs — no new framework install required.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| EDID probe returns valid screen size | DPI-01 | Requires Pi with physical HDMI display | Run installer on Pi, verify detected size matches display specs |
| UiMetrics uses DPI-based scale | DPI-03 | QML singleton, visual verification | Change `display.screen_size` in config, verify UI element sizes change |
| Display settings shows screen info | DPI-04 | QML UI visual verification | Open Display settings, verify screen size and DPI shown |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
