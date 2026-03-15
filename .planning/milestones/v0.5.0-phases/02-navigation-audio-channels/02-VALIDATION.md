---
phase: 2
slug: navigation-audio-channels
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-06
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt Test (QTest) |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd build && ctest -R "test_audio_channel\|test_navigation_channel" --output-on-failure` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~10 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest -R "test_audio_channel|test_navigation_channel" --output-on-failure`
- **After every plan wave:** Run `cd build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | NAV-01 | unit | `ctest -R test_navigation_channel` | ❌ W0 | ⬜ pending |
| 02-01-02 | 01 | 1 | NAV-02 | unit | `ctest -R test_navigation_channel` | ❌ W0 | ⬜ pending |
| 02-02-01 | 02 | 1 | AUD-01 | unit | `ctest -R test_audio_channel` | ✅ (extend) | ⬜ pending |
| 02-02-02 | 02 | 1 | AUD-02 | unit | `ctest -R test_audio_channel` | ✅ (extend) | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_navigation_channel_handler.cpp` — stubs for NAV-01, NAV-02 (new file needed)
- [ ] Extend `tests/test_audio_channel_handler.cpp` — add test cases for AUD-01, AUD-02

*Existing test infrastructure covers framework setup.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| NavigationTurnEvent logged at ~1Hz during navigation | NAV-01 | Requires live phone + active Google Maps | Start navigation on phone, check HU logs for ~1Hz turn events |
| NavigationFocusIndication received and logged | NAV-02 | Message ID unknown, requires protocol capture | Start/stop navigation, check logs for unknown nav channel messages |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
