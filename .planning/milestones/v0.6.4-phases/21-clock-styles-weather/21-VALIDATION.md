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
| 21-01-01 | 01 | 1 | CK-01, CK-02 | unit | `ctest -R "widget_config\|widget_instance"` | Existing (extend) | ⬜ pending |
| 21-02-01 | 02 | 1 | WX-01, WX-02 | unit | `ctest -R test_weather_service` | ❌ W0 (TDD task creates it) | ⬜ pending |
| 21-02-01 | 02 | 1 | WX-04 | unit | `ctest -R test_weather_service` | ❌ W0 (TDD task creates it) | ⬜ pending |
| 21-02-02 | 02 | 1 | WX-02, WX-04 | unit | `ctest --output-on-failure` | Created by 21-02-01 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

### Test Coverage Detail (Plan 21-02, Task 1 — TDD)

| Test | Requirement | What It Verifies |
|------|-------------|------------------|
| testWeatherDataDefaults | WX-02 | Default state before any fetch |
| testParseValidResponse | WX-01, WX-02 | JSON → WeatherData field mapping |
| testParseMalformedJson | WX-01 | Graceful error on bad input |
| testParseMissingCurrentKey | WX-01 | Graceful error on missing data |
| testRoundCoordinate | WX-01 | GPS cache key stability (~1km) |
| testGetWeatherDataCaching | WX-01 | Same location → same object |
| testBuildUrl | WX-01 | Correct Open-Meteo URL format |
| testSubscribeIncrementsCount | WX-04 | Subscriber tracking lifecycle |
| testRefreshTimerSkipsUnsubscribed | WX-04 | Page-hidden pause (0 subscribers → no fetch) |
| testSubscribeStaleTriggersImmediateFetch | WX-04 | Resume + immediate fetch on stale data |
| testCleanupNeverEvictsSubscribed | WX-01 | Cache eviction safety (no live object deletion) |
| testRefreshIntervalRespected | WX-04 | Per-location interval controls actual fetch cadence |
| testIntervalRecomputesOnUnsubscribe | WX-04 | Effective interval recalculates when subscriber leaves |

---

## Wave 0 Requirements

- [ ] `tests/test_weather_service.cpp` — created by Plan 21-02 Task 1 (TDD task writes tests + implementation together). Covers WX-01, WX-02, WX-04 including subscriber lifecycle, interval tracking, and cache safety.

*No separate fixture file — tests use inline JSON. Existing infrastructure covers CK-01, CK-02 via existing widget config tests.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Analog clock face renders correctly | CK-01 | Canvas2D visual output | Deploy to Pi, set clock style to "analog" at 2x2 size, verify face/hands/ticks render with theme colors |
| Minimal clock style fills widget | CK-01 | Visual layout | Deploy to Pi, set clock style to "minimal", verify large thin font at multiple sizes |
| Analog resize hint at 1x1 | CK-01 | Visual fallback | Place clock at 1x1, switch to analog, verify resize hint icon (not squashed face) |
| Weather icon matches conditions | WX-02 | Requires live API data | Connect companion, verify weather icon matches actual conditions |
| Weather "--°" on GPS loss | WX-02 | Requires companion disconnect | Disconnect companion, verify weather shows "--°" with location-off icon |
| Weather refresh pauses on hidden page | WX-04 | Requires page navigation + timing | Place weather widget, switch to another page for >5min, verify no fetches in logs while hidden |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
