---
phase: 14
slug: color-audit-m3-expressive-tokens
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-15
---

# Phase 14 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | QTest (Qt 6.8.2) |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd build && ctest --output-on-failure -R theme` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest --output-on-failure -R theme`
- **After every plan wave:** Run `cd build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 14-01-01 | 01 | 1 | CA-01 | unit | `ctest -R theme --output-on-failure` | ✅ extend tests/test_theme_service.cpp | ⬜ pending |
| 14-01-02 | 01 | 1 | CA-01 | unit | `ctest -R theme --output-on-failure` | ✅ extend tests/test_theme_service.cpp | ⬜ pending |
| 14-01-03 | 01 | 1 | CA-01/CA-03 | unit | `ctest -R theme --output-on-failure` | ✅ extend tests/test_theme_service.cpp | ⬜ pending |
| 14-01-04 | 01 | 1 | CA-03 | unit | `ctest -R theme --output-on-failure` | ✅ extend tests/test_theme_service.cpp | ⬜ pending |
| 14-01-05 | 01 | 2 | CA-02 | manual | Visual inspection on Pi | N/A — QML visual | ⬜ pending |
| 14-01-06 | 01 | 2 | CA-01 | manual | Visual inspection on Pi | N/A — QML visual | ⬜ pending |
| 14-01-07 | 01 | 2 | CA-03 | manual | Visual inspection on Pi | N/A — QML visual | ⬜ pending |
| 14-01-08 | 01 | 3 | CA-03 | manual | Visual inspection on Pi | N/A — document | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Extend `tests/test_theme_service.cpp` — tests for surfaceTintHigh/Highest computation, night guardrail saturation clamping, warning/onWarning tokens

*Existing test_theme_service.cpp covers ThemeService — extend, no new file needed.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| NavbarControl active/pressed state visually distinct | CA-02 | QML visual state requires Pi display | Trigger hold on navbar controls; verify primaryContainer fill + onPrimaryContainer icon color |
| Token pairing legibility day+night | CA-01 | Requires visual inspection on Pi | Open all screens in day mode and night mode; verify no white-on-light or dark-on-dark text |
| Accent boldness vs pre-milestone | CA-03 | Subjective visual comparison | Compare widget cards, settings tiles, controls against neutral-only baseline |
| Night mode comfort at arm's length | CA-01/CA-03 | Requires dark environment + Pi | View Pi at arm's length in dark room; primary should not be a glare source |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
