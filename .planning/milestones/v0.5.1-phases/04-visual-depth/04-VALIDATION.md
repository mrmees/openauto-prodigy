---
phase: 4
slug: visual-depth
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-10
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + Qt Test (C++) |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd build && ctest --output-on-failure -R theme` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest --output-on-failure`
- **After every plan wave:** Run `cd build && ctest --output-on-failure` + visual check on Pi
- **Before `/gsd:verify-work`:** Full suite must be green + Pi visual verification
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| TBD | 01 | 0 | STY-01, STY-02 | build | `cd build && cmake .. && make -j$(nproc)` | ✅ | ⬜ pending |
| TBD | 01 | 1 | STY-01 | manual-only | Visual: buttons have shadow on Pi | N/A | ⬜ pending |
| TBD | 01 | 1 | STY-01 | manual-only | Visual: press changes depth on Pi | N/A | ⬜ pending |
| TBD | 01 | 1 | STY-02 | manual-only | Visual: navbar has depth treatment on Pi | N/A | ⬜ pending |
| TBD | 01 | 1 | STY-01, STY-02 | unit | `cd build && ctest --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Qt 6.8 installed on dev VM (prerequisite for MultiEffect)
- [ ] `import QtQuick.Effects` verified working in both dev VM and Pi builds
- [ ] Existing 65 tests pass after Qt 6.8 upgrade on dev VM

*Qt 6.8 upgrade is a hard prerequisite — no depth work can begin without it.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Buttons have visible 3D shadow in resting state | STY-01 | Visual rendering depends on GPU output; no programmatic assertion for "subtle depth" | Deploy to Pi, inspect Tile, SettingsListItem, dialog buttons for shadow presence |
| Button press visibly changes depth effect | STY-01 | Animation timing and shadow reduction must be evaluated by eye | Tap buttons on Pi, verify shadow shrinks/disappears and button appears to sink |
| Navbar has gradient fade separation | STY-02 | Gradient blending quality is GPU-dependent and subjective | View launcher and settings views on Pi, verify navbar edge has gradient fade |
| Gradient skipped during AA | STY-02 | Requires active AA session on hardware | Connect phone, verify navbar blends with AA status bar |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
