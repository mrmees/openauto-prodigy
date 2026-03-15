---
phase: 2
slug: clock-scale-control
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt Test (QTest) |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd build && ctest --output-on-failure -R "test_config\|test_yaml"` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure -R "test_config|test_yaml"`
- **After every plan wave:** Run `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 2-01-01 | 01 | 1 | CLK-03 | unit | `ctest -R test_config_key_coverage` | Needs key added | ⬜ pending |
| 2-01-02 | 01 | 1 | DPI-05 | unit | `ctest -R test_config_service` | ✅ | ⬜ pending |
| 2-01-03 | 01 | 1 | CLK-01 | manual-only | Visual inspection on Pi | N/A | ⬜ pending |
| 2-01-04 | 01 | 1 | CLK-02 | manual-only | Visual inspection on Pi | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Add `"display.clock_24h"` to `test_config_key_coverage.cpp` `testAllRuntimeKeys()` key list
- [ ] Add `display.clock_24h` default to `YamlConfig::initDefaults()`

*These are prerequisites for automated verification of CLK-03.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Clock font is larger than fontBody and readable at arm's length | CLK-01 | Visual/perceptual — requires human judgment on real display | Launch on Pi, check clock text in navbar is significantly larger than body text |
| No AM/PM suffix shown in 12h mode | CLK-02 | Visual — need to confirm format string renders correctly | Set `display.clock_24h: false`, check navbar clock shows bare time (e.g., "2:30" not "2:30 PM") |
| Scale stepper resizes all UI elements | DPI-05 | Visual — live resize behavior on real hardware | Adjust scale stepper in Display settings, verify all elements resize proportionally |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
