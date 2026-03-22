---
phase: 12
slug: documentation
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-03-15
---

# Phase 12 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + Qt Test (via CMake) |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cd build && ctest --output-on-failure` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~11 seconds |

---

## Sampling Rate

- **After every task commit:** Verify doc links resolve, code examples match actual source
- **After every plan wave:** Read-through for accuracy and completeness
- **Before `/gsd:verify-work`:** Success criteria checklist review
- **Max feedback latency:** N/A (documentation phase — no code changes)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 12-01-01 | 01 | 1 | DOC-01 | manual-only | N/A (prose review) | N/A | ⬜ pending |
| 12-01-02 | 01 | 1 | DOC-02 | manual-only | N/A (prose review) | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. This phase produces documentation only — no test infrastructure changes needed.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Developer guide covers manifest, registration, lifecycle, sizing, categories, CMake | DOC-01 | Documentation correctness is verified by reading, not automated tests | Read `docs/widget-developer-guide.md`, verify all sections present, code examples compile |
| ADRs document v0.6-v0.6.1 design choices | DOC-02 | ADR completeness is a judgment call | Read `docs/design-decisions.md`, verify decisions from STATE.md are captured |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < N/A (docs only)
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
