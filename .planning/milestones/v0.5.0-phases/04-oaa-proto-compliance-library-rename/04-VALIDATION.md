---
phase: 04
slug: oaa-proto-compliance-library-rename
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 04 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt Test (QTest) via Qt 6.4/6.8 |
| **Config file** | `tests/CMakeLists.txt` with `oap_add_test()` helper |
| **Quick run command** | `ctest --test-dir build --output-on-failure` |
| **Full suite command** | `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build --output-on-failure`
- **After every plan wave:** Run `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 04-01-01 | 01 | 1 | CLN-01 | unit | `ctest --test-dir build -R test_audio_channel_handler --output-on-failure` | ✅ (update needed) | ⬜ pending |
| 04-01-01 | 01 | 1 | CLN-01 | unit | `ctest --test-dir build -R test_navigation_channel_handler --output-on-failure` | ✅ | ⬜ pending |
| 04-01-02 | 01 | 1 | CLN-02 | unit | `ctest --test-dir build -R test_service_discovery_builder --output-on-failure` | ✅ (add bssid test) | ⬜ pending |
| 04-02-01 | 02 | 2 | REN-01 | build | `cd build && cmake --build . -j$(nproc)` | N/A | ⬜ pending |
| 04-02-02 | 02 | 2 | TST-01 | full suite | `ctest --test-dir build --output-on-failure` | ✅ | ⬜ pending |
| 04-02-02 | 02 | 2 | TST-01 | unit | `ctest --test-dir build -R test_navbar_controller --output-on-failure` | ✅ (fix needed) | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. Only modifications to existing tests needed:

- [ ] Update `test_audio_channel_handler.cpp` — verify retracted message IDs (0x8021, 0x8022) now emit `unknownMessage` instead of being handled
- [ ] Add bssid test to `test_service_discovery_builder.cpp` — verify WiFi descriptor contains MAC format
- [ ] Fix `test_navbar_controller.cpp::testHoldProgressEmittedDuringHold` — timing issue

*No new test files or framework installs required.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| WiFi AP BSSID visible in phone AA connection | CLN-02 | Requires physical phone + Pi | Connect phone to Pi AP, verify AA connects successfully |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
