---
phase: 3
slug: theme-color-system
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Qt Test (QTest) 6.4+ |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd build && ctest -R test_theme_service --output-on-failure` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest -R test_theme_service --output-on-failure`
- **After every plan wave:** Run `cd build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 1 | THM-01 | unit | `ctest -R test_theme_service --output-on-failure` | Exists (needs update) | ⬜ pending |
| 03-01-02 | 01 | 1 | THM-01 | unit | `ctest -R test_theme_service --output-on-failure` | Wave 0 | ⬜ pending |
| 03-02-01 | 02 | 1 | THM-02 | unit | `ctest -R test_theme_service --output-on-failure` | Wave 0 | ⬜ pending |
| 03-02-02 | 02 | 1 | THM-02 | unit | `ctest -R test_theme_service --output-on-failure` | Exists (needs update) | ⬜ pending |
| 03-03-01 | 03 | 2 | THM-03 | smoke | `grep -rn '#[0-9a-fA-F]\{6,8\}"' qml/ --include='*.qml' \| grep -v '// debug' \| wc -l` | Wave 0 | ⬜ pending |
| 03-03-02 | 03 | 2 | THM-03 | unit | `ctest -R test_ipc --output-on-failure` | Wave 0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Update `tests/data/themes/default/theme.yaml` (and other test fixtures) to use new AA token key names
- [ ] Update `tests/test_theme_service.cpp` -- rename accessor calls (`backgroundColor()` -> `background()`, etc.)
- [ ] Add test cases for derived colors (scrim, pressed, success return expected values)
- [ ] Add test case verifying all 16 base tokens present in each bundled theme

*Existing test infrastructure covers framework needs. Wave 0 updates test data and assertions for renamed properties.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Day/night theme switch looks visually coherent across all screens | THM-02 | Visual appearance judgment | Switch themes on Pi, check launcher, settings, AA overlay, BT audio, phone screens |
| Connected Device theme applies phone colors when AA connects | THM-02 | Requires live phone connection | Connect phone via AA, verify HU chrome adopts Material You palette |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
