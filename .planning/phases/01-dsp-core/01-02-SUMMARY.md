---
phase: 01-dsp-core
plan: 02
subsystem: audio
tags: [dsp, equalizer, biquad, interpolation, bypass, limiter, audio-processing]

# Dependency graph
requires:
  - "01-dsp-core/plan-01: BiquadFilter + SoftLimiter primitives"
provides:
  - "EqualizerEngine: RT-safe 10-band graphic EQ with atomic coefficient swap, per-sample interpolation, bypass crossfade, soft limiting"
affects: [02-service-config]

# Tech tracking
tech-stack:
  added: []
  patterns: [atomic-double-buffer, generation-counter-aba, per-sample-coefficient-interpolation]

key-files:
  created:
    - src/core/audio/EqualizerEngine.hpp
    - src/core/audio/EqualizerEngine.cpp
    - tests/test_equalizer_engine.cpp
  modified:
    - src/CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Generation counter for double-buffer swap detection (avoids ABA problem with pointer comparison)"
  - "Bypass fast-path snaps coefficients immediately (no interpolation while fully bypassed)"
  - "Bypass crossfade uses same 2304-sample ramp as coefficient interpolation (consistent behavior)"

patterns-established:
  - "Atomic double-buffer with generation counter: non-RT writes to inactive buffer, swaps pointer + increments generation; RT checks generation to detect changes"
  - "Bypass snap: coefficients set during full bypass take effect immediately (no interpolation ramp) so they're ready when un-bypassed"

requirements-completed: [DSP-01, DSP-02, DSP-03]

# Metrics
duration: 5min
completed: 2026-03-01
---

# Phase 1 Plan 2: Equalizer Engine Summary

**10-band graphic EQ engine composing biquad cascade + soft limiter with atomic coefficient swap, per-sample interpolation, and smooth bypass crossfade**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-01T23:46:55Z
- **Completed:** 2026-03-01T23:51:51Z
- **Tasks:** 1
- **Files modified:** 5

## Accomplishments
- EqualizerEngine class: 10-band biquad cascade processing stereo interleaved int16_t at 48kHz
- Atomic double-buffer coefficient swap with generation counter (ABA-safe) for lock-free RT/non-RT communication
- Per-sample coefficient interpolation over 2304 samples (~48ms) for click-free transitions
- Wet/dry bypass crossfade with same ramp duration; coefficients snap during full bypass for instant un-bypass
- Integrated soft limiter prevents clipping even with all bands at +12dB on full-scale input
- 17 comprehensive tests covering frequency response, interpolation smoothness, bypass behavior, clipping prevention, stereo independence

## Task Commits

Each task was committed atomically:

1. **Task 1: EqualizerEngine -- 10-band cascade with interpolation, bypass, and limiter** - `bd38a6a` (feat)

_TDD flow: RED (compilation failure -- missing header) -> GREEN (implementation + ABA fix) -> no refactor needed._

## Files Created/Modified
- `src/core/audio/EqualizerEngine.hpp` - Public API: setGain, setAllGains, setBypassed, process, kInterpolationSamples
- `src/core/audio/EqualizerEngine.cpp` - Implementation: atomic swap, interpolation, bypass crossfade, RT-safe process loop
- `tests/test_equalizer_engine.cpp` - 17 tests: boost/isolation/preset, interpolation smoothness/completion/retrigger, bypass passthrough/applies-changes/crossfade, clipping, zero-frames, stereo, accessors, clamping
- `src/CMakeLists.txt` - Added EqualizerEngine.cpp to openauto-core library
- `tests/CMakeLists.txt` - Added test_equalizer_engine target

## Decisions Made
- **Generation counter over pointer comparison:** The double-buffer swap uses two buffers that alternate as active/write targets. With two rapid swaps (e.g., setGain then setBypassed without process() between), the active pointer can return to the same address (ABA problem). A monotonic generation counter detects every swap regardless of pointer identity.
- **Bypass coefficient snap:** When fully bypassed, new coefficients are applied immediately (no interpolation ramp). This ensures changes made during bypass are ready the instant bypass is turned off, matching the user decision that "changes stored during bypass, applied on un-bypass."

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed ABA problem in double-buffer swap detection**
- **Found during:** Task 1 (GREEN phase -- testBypassAppliesChanges failing)
- **Issue:** Pointer-based swap detection (`current != lastSeenCoeffs_`) missed coefficient updates when two swaps happened without an intervening process() call (active pointer returned to same address)
- **Fix:** Added `std::atomic<uint32_t> generation_` counter, incremented on each swap. RT side compares generation instead of pointer identity.
- **Files modified:** src/core/audio/EqualizerEngine.hpp, src/core/audio/EqualizerEngine.cpp
- **Verification:** testBypassAppliesChanges passes; all 17 tests pass
- **Committed in:** bd38a6a (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** ABA fix is a correctness requirement for the double-buffer pattern. No scope creep.

## Issues Encountered
- Pre-existing test_event_bus failure (unrelated to DSP work) -- same as Plan 01, logged but not fixed per scope boundary rules.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 1 (DSP Core) is complete: BiquadFilter, SoftLimiter, and EqualizerEngine are all implemented and tested
- EqualizerEngine is compiled into the openauto-core static library, ready for Phase 2 integration into AudioService's PipeWire callback
- No blockers

---
*Phase: 01-dsp-core*
*Completed: 2026-03-01*
