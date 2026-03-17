---
phase: 21
slug: clock-styles-weather
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-17
---

# Phase 21 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt6::Test (QTest) + CTest |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd build && ctest --output-on-failure -R "weather\|widget_config\|widget_instance"` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest --output-on-failure -R "weather\|widget_config\|widget_instance"`
- **After every plan wave:** Run `cd build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 21-01-01 | 01 | 1 | CK-01, CK-02 | unit | `ctest -R test_widget_config` | Existing (extend) | ⬜ pending |
| 21-01-02 | 01 | 1 | CK-01 | unit | `ctest -R test_widget_config` | Existing (extend) | ⬜ pending |
| 21-01-03 | 01 | 1 | CK-01 | manual | Visual inspection on Pi | N/A | ⬜ pending |
| 21-02-01 | 02 | 1 | WX-01, WX-02, WX-04 | unit | `ctest -R test_weather_service` | ❌ W0 | ⬜ pending |
| 21-02-02 | 02 | 1 | WX-02 | unit | `ctest -R test_weather_service` | ❌ W0 | ⬜ pending |
| 21-02-03 | 02 | 1 | WX-04 | unit | `ctest -R test_weather_service` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_weather_service.cpp` — stubs for WX-01, WX-02, WX-04 (WeatherData parsing, coordinate rounding, refresh logic)
- [ ] `tests/data/open_meteo_response.json` — test fixture for API response parsing

*Existing infrastructure covers CK-01, CK-02 — extend existing widget config tests.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Analog clock face renders correctly | CK-01 | Canvas2D visual output | Deploy to Pi, set clock style to "analog" at 2x2 size, verify face/hands/ticks render with theme colors |
| Minimal clock style fills widget | CK-01 | Visual layout | Deploy to Pi, set clock style to "minimal", verify large thin font at multiple sizes |
| Weather icon matches conditions | WX-02 | Requires live API data | Connect companion, verify weather icon matches actual conditions |
| Weather "--°" on GPS loss | WX-02 | Requires companion disconnect | Disconnect companion, verify weather shows "--°" with location-off icon |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
