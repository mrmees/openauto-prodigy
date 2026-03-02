---
phase: 01-dsp-core
plan: 01
subsystem: audio
tags: [dsp, biquad, eq, limiter, audio-processing]

# Dependency graph
requires: []
provides:
  - "BiquadFilter: RBJ peaking EQ coefficient calculation + DF2T processing"
  - "SoftLimiter: envelope-follower peak limiter with fast attack / slow release"
  - "Constants: 10-band center frequencies, graphic EQ Q value"
affects: [01-dsp-core/plan-02]

# Tech tracking
tech-stack:
  added: []
  patterns: [header-only-dsp, namespace-oap, rbj-cookbook]

key-files:
  created:
    - src/core/audio/BiquadFilter.hpp
    - src/core/audio/SoftLimiter.hpp
    - tests/test_biquad_filter.cpp
    - tests/test_soft_limiter.cpp
  modified:
    - tests/CMakeLists.txt

key-decisions:
  - "float32 precision for all DSP (sufficient for 10-band graphic EQ at 16-bit input)"
  - "Direct Form II Transposed topology for biquad (fewer delay elements, good numerical behavior)"
  - "Per-channel limiter (not stereo-linked) for simplicity and zero crosstalk"

patterns-established:
  - "Header-only DSP: pure computation in .hpp, no Qt dependencies, no allocation"
  - "DSP test pattern: steady-state sine wave measurement with settle period, RMS-to-dB comparison"
  - "DSP tests section in tests/CMakeLists.txt"

requirements-completed: [DSP-01]

# Metrics
duration: 4min
completed: 2026-03-01
---

# Phase 1 Plan 1: DSP Primitives Summary

**RBJ peaking EQ biquad filter and envelope-follower soft limiter -- header-only, zero-dependency DSP building blocks**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-01T23:40:09Z
- **Completed:** 2026-03-01T23:44:42Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- BiquadFilter with correct RBJ Audio EQ Cookbook peaking EQ coefficients and DF2T processing
- SoftLimiter with fast attack (~0.5ms) and slow release (~50ms) envelope follower
- 22 total tests covering frequency response, stereo independence, transparency, limiting, attack/release timing, polarity preservation
- No regressions in existing test suite (54/55 pass; 1 pre-existing failure in test_event_bus)

## Task Commits

Each task was committed atomically:

1. **Task 1: BiquadFilter** - `fb9a861` (feat)
2. **Task 2: SoftLimiter** - `4945017` (feat)

_Both tasks followed TDD: RED (compilation failure) -> GREEN (implementation passes) -> no refactor needed._

## Files Created/Modified
- `src/core/audio/BiquadFilter.hpp` - BiquadCoeffs, BiquadState, calcPeakingEQ, processSample, constants
- `src/core/audio/SoftLimiter.hpp` - SoftLimiter class with envelope-follower peak limiting
- `tests/test_biquad_filter.cpp` - 13 tests: coefficients, frequency response, stereo, state
- `tests/test_soft_limiter.cpp` - 9 tests: transparency, limiting, attack, release, polarity, independence
- `tests/CMakeLists.txt` - Added DSP tests section with both test targets

## Decisions Made
- float32 precision throughout (not double) -- sufficient for 10-band graphic EQ at 16-bit audio input
- Direct Form II Transposed for biquad processing -- fewer state variables, good numerical stability
- Per-channel limiter design (not stereo-linked) -- simpler, no crosstalk between channels
- Envelope follower approach for limiter (not brickwall) -- smoother, avoids audible artifacts

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed SoftLimiter test steady-state measurement**
- **Found during:** Task 2 (SoftLimiter GREEN phase)
- **Issue:** testLimitsAboveThreshold measured peak from sample 0, but envelope starts at 0 so first sample passes through at full amplitude before attack engages
- **Fix:** Added 100ms settle period before measurement (consistent with other limiter tests)
- **Files modified:** tests/test_soft_limiter.cpp
- **Verification:** All 9 limiter tests pass
- **Committed in:** 4945017 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug in test)
**Impact on plan:** Test measurement window corrected to match envelope-follower behavior. No scope creep.

## Issues Encountered
- Pre-existing test_event_bus failure (unrelated to DSP work) -- logged but not fixed per scope boundary rules.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- BiquadFilter and SoftLimiter are ready for composition into EqualizerEngine (Plan 02)
- Plan 02 will compose 10 BiquadFilters + 2 SoftLimiters into a stereo processing chain
- No blockers

---
*Phase: 01-dsp-core*
*Completed: 2026-03-01*
