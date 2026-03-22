---
phase: 06
slug: content-widgets
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-12
---

# Phase 06 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | QTest (Qt Test) |
| **Config file** | tests/CMakeLists.txt with `oap_add_test()` helper |
| **Quick run command** | `cd build && ctest -R "test_navigation_data_bridge\|test_media_data_bridge\|test_maneuver_icon" --output-on-failure` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest -R "test_navigation_data_bridge\|test_media_data_bridge\|test_maneuver_icon" --output-on-failure`
- **After every plan wave:** Run `cd build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 06-01-xx | 01 | 1 | NAV-05 | unit | `ctest -R test_navigation_data_bridge` | ❌ W0 | ⬜ pending |
| 06-01-xx | 01 | 1 | NAV-01 | unit | `ctest -R test_navigation_data_bridge` | ❌ W0 | ⬜ pending |
| 06-01-xx | 01 | 1 | NAV-02 | unit | `ctest -R test_maneuver_icon_provider` | ❌ W0 | ⬜ pending |
| 06-01-xx | 01 | 1 | NAV-03 | unit | `ctest -R test_navigation_data_bridge` | ❌ W0 | ⬜ pending |
| 06-01-xx | 01 | 1 | NAV-04 | manual | Pi touch test | N/A | ⬜ pending |
| 06-02-xx | 02 | 1 | MEDIA-05 | unit | `ctest -R test_media_data_bridge` | ❌ W0 | ⬜ pending |
| 06-02-xx | 02 | 1 | MEDIA-02 | unit | `ctest -R test_media_data_bridge` | ❌ W0 | ⬜ pending |
| 06-02-xx | 02 | 1 | MEDIA-04 | unit | `ctest -R test_media_data_bridge` | ❌ W0 | ⬜ pending |
| 06-02-xx | 02 | 1 | MEDIA-03 | unit | `ctest -R test_media_data_bridge` | ❌ W0 | ⬜ pending |
| 06-02-xx | 02 | 1 | MEDIA-01 | manual | Pi visual test | N/A | ⬜ pending |
| 06-02-xx | 02 | 1 | MEDIA-06 | unit | `ctest -R test_widget_plugin_integration` | ✅ update | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_navigation_data_bridge.cpp` — stubs for NAV-01, NAV-03, NAV-05 (distance formatting, state changes, property updates)
- [ ] `tests/test_media_data_bridge.cpp` — stubs for MEDIA-02, MEDIA-03, MEDIA-04, MEDIA-05 (source switching, control delegation, metadata)
- [ ] `tests/test_maneuver_icon_provider.cpp` — stubs for NAV-02 (PNG serving, cache busting)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Nav widget tap opens AA fullscreen | NAV-04 | Requires Pi touch + QML plugin navigation stack | Tap nav widget on home screen, verify AA fullscreen activates |
| Unified widget shows title/artist/controls | MEDIA-01 | Visual layout verification on 1024x600 display | Connect BT audio + play track, verify title/artist/controls visible; connect AA, verify switch |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
