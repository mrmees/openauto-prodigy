---
phase: 3
slug: commands-authentication
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-06
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt6::Test (QTest) |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd build && ctest -R "test_(media_status\|bluetooth\|input)_channel_handler\|test_control" --output-on-failure` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest -R "test_(media_status|bluetooth|input)_channel_handler|test_control" --output-on-failure`
- **After every plan wave:** Run `cd build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 1 | MED-01 | unit | `ctest -R test_media_status_channel_handler --output-on-failure` | No — W0 | pending |
| 03-01-02 | 01 | 1 | MED-02 | unit | `ctest -R test_control_channel --output-on-failure` | No — W0 | pending |
| 03-02-01 | 02 | 1 | BT-01 | unit | `ctest -R test_bluetooth_channel_handler --output-on-failure` | Yes — extend | pending |
| 03-02-02 | 02 | 1 | INP-01 | unit | `ctest -R test_input_channel_handler --output-on-failure` | Yes — extend | pending |

*Status: pending / green / red / flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_media_status_channel_handler.cpp` — stubs for MED-01 (new file, follows existing handler test pattern)
- [ ] Extend `tests/test_bluetooth_channel_handler.cpp` — covers BT-01 (AUTH_DATA + AUTH_RESULT cases)
- [ ] Extend `tests/test_input_channel_handler.cpp` — covers INP-01 (BINDING_NOTIFICATION case)
- [ ] Voice session test — MED-02 via ControlChannel send method (verify serialization in existing test infrastructure or new test)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Media pause/resume affects phone playback | MED-01 | Requires connected phone + active media | Connect phone, play music, press pause in HU, verify phone pauses |
| Voice button activates Google Assistant | MED-02 | Requires connected phone | Press voice button in HU, verify GA activates on phone; release, verify GA stops |
| BT auth exchange completes without errors | BT-01 | Requires phone pairing flow | Pair phone, check logs for AUTH_DATA/AUTH_RESULT without errors |
| Haptic feedback requests logged | INP-01 | Requires phone sending InputBindingNotification | Navigate AA UI, check logs for haptic feedback entries |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
