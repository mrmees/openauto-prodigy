# Phase 1: DSP Core - Research

**Researched:** 2026-03-01
**Domain:** Real-time audio DSP — biquad equalizer engine (C++17)
**Confidence:** HIGH

## Summary

This phase implements a 10-band graphic equalizer engine as a pure DSP math library. The core is well-understood territory: biquad peaking EQ filters using the Robert Bristow-Johnson Audio EQ Cookbook formulas, cascaded across 10 ISO octave-spaced center frequencies. The existing codebase processes `int16_t` stereo audio at 48kHz through lock-free SPSC ring buffers in a PipeWire RT callback — the EQ engine must fit this pattern exactly.

The three main technical challenges are: (1) correct biquad coefficient calculation for peaking EQ, (2) click-free coefficient interpolation during parameter changes, and (3) a lock-free mechanism for the non-RT thread to publish new coefficients to the RT thread. All three are well-solved problems in audio DSP with established, proven approaches.

**Primary recommendation:** Use Direct Form II Transposed biquad topology with per-sample linear coefficient interpolation over ~2304 samples (48ms at 48kHz). Coefficient swap via atomic pointer exchange (double-buffering). Soft limiter via simple envelope follower with fast attack / slow release.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- ±12dB gain range per band, 1dB step increments (25 positions per band: -12 to +12)
- Soft output limiter to prevent clipping when multiple bands are boosted — engine auto-attenuates, user never hears digital crackle
- Default all bands to 0dB (flat). The "Flat" preset is literally flat.
- ~50ms crossfade for coefficient interpolation — fast enough to feel instant, slow enough to avoid clicks
- Real-time audio updates while dragging sliders (engine handles smooth interpolation between rapid gain changes)
- Same transition speed for both preset switches and single-band adjustments — consistent behavior everywhere
- Direct crossfade only — no volume dip during transitions. Audio never mutes or ducks.
- Smooth fade (~50ms) when toggling bypass — same crossfade as everything else, consistent engine behavior
- Global bypass only — one switch kills all EQ processing. Per-stream control is Phase 2's domain.
- Changes are silently applied during bypass — stored and ready. When bypass turns off, new settings take effect immediately. Allows "blind" tweaking then reveal.
- No audio cue for bypass toggle — the audible difference is the feedback. UI state indicator is Phase 3's job.

### Claude's Discretion
- Biquad filter topology and coefficient calculation approach
- Internal processing precision (float32 vs float64)
- Lock-free coefficient swap mechanism
- Unit test structure and coverage strategy
- Soft limiter algorithm (peak vs RMS, attack/release characteristics)

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DSP-01 | User can hear EQ-processed audio through a 10-band graphic equalizer (31Hz-16kHz ISO frequencies) | Biquad peaking EQ cookbook formulas, Direct Form II Transposed topology, Q=1.414 for 1-octave bandwidth, int16_t→float→process→float→int16_t pipeline |
| DSP-02 | User experiences smooth audio transitions when switching EQ presets (coefficient interpolation) | Per-sample linear interpolation over ~2304 samples (48ms), atomic double-buffer coefficient swap, no volume dip crossfade |
| DSP-03 | User can bypass EQ processing entirely with a single toggle | Atomic bypass flag, wet/dry crossfade over same 48ms ramp, changes stored during bypass and applied on un-bypass |
</phase_requirements>

## Standard Stack

### Core
| Component | Details | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| RBJ Audio EQ Cookbook | Peaking EQ formulas | Biquad coefficient calculation | Industry standard, used by Web Audio API, JUCE, virtually all EQ implementations |
| Direct Form II Transposed | Biquad topology | Filter processing | Best numerical accuracy for floating-point, recommended by ARM CMSIS-DSP, minimal state variables |
| `std::atomic<T*>` | C++17 | Lock-free coefficient swap | Platform-guaranteed lock-free for pointer types on all modern architectures |

### Supporting
| Component | Details | Purpose | When to Use |
|-----------|---------|---------|-------------|
| `float` (32-bit) | Internal precision | Sample processing | Sufficient for 10-band graphic EQ at 16-bit input; double precision unnecessary for this application |
| Qt6::Test | Existing framework | Unit testing | Already used by all 47 existing tests in the project |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Direct Form II Transposed | State Variable Filter (SVF/TPT) | SVF is more modulation-stable but overkill for graphic EQ where params change infrequently; DF2T is simpler and well-tested |
| float32 | float64 (double) | Double gives more headroom but wastes cache lines; 10-band peaking EQ at ±12dB doesn't approach float32 precision limits |
| Linear coefficient interpolation | Parameter-space interpolation (interpolate dB gain, recompute coefficients) | Parameter interpolation is more correct but requires trig per sample; linear coeff interpolation is standard practice for smooth graphic EQ changes |

## Architecture Patterns

### Recommended Project Structure
```
src/core/audio/
├── BiquadFilter.hpp          # Single biquad stage (DF2T, per-sample process)
├── EqualizerEngine.hpp       # 10-band cascade + interpolation + bypass + limiter
├── EqualizerEngine.cpp       # Implementation
├── SoftLimiter.hpp           # Envelope-follower peak limiter
└── EqPresets.hpp             # (Phase 2 — NOT this phase)

tests/
├── test_biquad_filter.cpp    # Single-band frequency response verification
├── test_equalizer_engine.cpp # 10-band processing, interpolation, bypass
└── test_soft_limiter.cpp     # Limiter behavior (clipping prevention, transparency)
```

### Pattern 1: Biquad Coefficient Calculation (Peaking EQ)
**What:** RBJ Cookbook peaking EQ formula — compute normalized b0/b1/b2/a1/a2 from center frequency, gain dB, and Q
**When to use:** Every time a band's gain changes (called from non-RT thread)
**Example:**
```cpp
// Source: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
struct BiquadCoeffs {
    float b0, b1, b2, a1, a2; // normalized (divided by a0)
};

BiquadCoeffs calcPeakingEQ(float freqHz, float gainDb, float Q, float sampleRate) {
    float A     = std::pow(10.0f, gainDb / 40.0f);
    float w0    = 2.0f * M_PI * freqHz / sampleRate;
    float cosw0 = std::cos(w0);
    float sinw0 = std::sin(w0);
    float alpha = sinw0 / (2.0f * Q);

    float a0 = 1.0f + alpha / A;

    return {
        .b0 = (1.0f + alpha * A) / a0,
        .b1 = (-2.0f * cosw0)    / a0,
        .b2 = (1.0f - alpha * A) / a0,
        .a1 = (-2.0f * cosw0)    / a0,
        .a2 = (1.0f - alpha / A) / a0,
    };
}
```

### Pattern 2: Direct Form II Transposed Processing
**What:** Process one sample through a biquad stage using 2 state variables
**When to use:** Per-sample in the RT callback
**Example:**
```cpp
// Source: https://en.wikipedia.org/wiki/Digital_biquad_filter
// DF2T: y[n] = b0*x[n] + z1
//        z1  = b1*x[n] - a1*y[n] + z2
//        z2  = b2*x[n] - a2*y[n]
float processSample(float x, const BiquadCoeffs& c, float& z1, float& z2) {
    float y = c.b0 * x + z1;
    z1 = c.b1 * x - c.a1 * y + z2;
    z2 = c.b2 * x - c.a2 * y;
    return y;
}
```

### Pattern 3: Atomic Double-Buffer Coefficient Swap
**What:** Non-RT thread prepares new coefficients, RT thread picks them up atomically
**When to use:** When user changes any EQ parameter
**Example:**
```cpp
// Two coefficient sets — RT thread reads 'active', non-RT thread writes 'staged'
struct EngineCoeffs {
    BiquadCoeffs bands[10];
    bool bypass;
};

// Non-RT thread: compute new coefficients, store, then swap pointer
std::atomic<EngineCoeffs*> coeffs_;  // points to active set
EngineCoeffs buffers_[2];            // double buffer
int writeIdx_ = 0;                   // non-RT thread only

void setGain(int band, float dB) {   // called from Qt main thread
    auto& buf = buffers_[writeIdx_];
    buf = *coeffs_.load(std::memory_order_acquire);  // copy current
    buf.bands[band] = calcPeakingEQ(centerFreqs[band], dB, Q, sampleRate);
    coeffs_.store(&buf, std::memory_order_release);
    writeIdx_ ^= 1;
}
```

### Pattern 4: Per-Sample Coefficient Interpolation (Crossfade)
**What:** Linearly interpolate between old and new coefficients over N samples to avoid clicks
**When to use:** Every process() call when coefficients have changed
**Example:**
```cpp
// In RT process loop:
if (interpSamplesRemaining_ > 0) {
    float t = 1.0f - (float(interpSamplesRemaining_) / float(interpLength_));
    BiquadCoeffs interp;
    interp.b0 = oldCoeffs_.b0 + t * (newCoeffs_.b0 - oldCoeffs_.b0);
    // ... same for b1, b2, a1, a2
    float y = processSample(x, interp, z1, z2);
    --interpSamplesRemaining_;
} else {
    float y = processSample(x, currentCoeffs_, z1, z2);
}
```

### Pattern 5: Soft Limiter (Envelope Follower)
**What:** Peak envelope follower with fast attack, slow release, applied as gain reduction
**When to use:** After all 10 bands have processed, before int16_t output conversion
**Example:**
```cpp
// Source: https://www.musicdsp.org/en/latest/Filters/265-output-limiter-using-envelope-follower-in-c.html
class SoftLimiter {
    float envelope_ = 0.0f;
    float attackCoeff_;   // fast: ~0.5ms → pow(0.01, 1.0/(0.0005*48000))
    float releaseCoeff_;  // slow: ~50ms → pow(0.01, 1.0/(0.05*48000))
    float threshold_ = 1.0f; // 0dBFS

    float process(float x) {
        float absX = std::fabs(x);
        if (absX > envelope_)
            envelope_ += attackCoeff_ * (absX - envelope_);
        else
            envelope_ += releaseCoeff_ * (absX - envelope_);

        if (envelope_ > threshold_)
            return x * (threshold_ / envelope_);
        return x;
    }
};
```

### Anti-Patterns to Avoid
- **Interpolating in parameter space per sample:** Recomputing biquad coefficients (with trig) per sample is CPU-wasteful. Interpolate the coefficients directly — for peaking EQ with moderate gain changes, linear coefficient interpolation is stable and click-free.
- **Using mutex/lock in RT callback:** The PipeWire process callback is RT-safe. Any blocking call (mutex, memory allocation, logging) violates RT guarantees. Use atomic pointer swap only.
- **Reusing filter state across coefficient changes without interpolation:** Abrupt coefficient changes cause transient discontinuities that manifest as audible clicks/pops. Always interpolate.
- **Processing in int16_t:** Integer arithmetic accumulates rounding error through 10 cascaded biquad stages. Convert to float at input, process in float, convert back at output.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Biquad coefficient formulas | Custom derivation from s-domain | RBJ Audio EQ Cookbook formulas | Battle-tested, used everywhere, handles BLT frequency warping correctly |
| Lock-free parameter passing | Custom lock-free queue | `std::atomic<T*>` double buffer | Simplest correct solution for "latest value" semantics (not queued) |
| Frequency response verification | Manual dB calculation in tests | Sine wave sweep + FFT or single-frequency steady-state measurement | Avoids reimplementing DSP math in tests — test the actual filter output |

**Key insight:** The DSP math here is textbook — the value is in correct implementation, not novel algorithms. Every component (biquad, interpolation, limiter) has canonical implementations. The risk is subtle bugs (wrong coefficient normalization, forgetting to reset filter state, interpolation off-by-one), not architectural decisions.

## Common Pitfalls

### Pitfall 1: Forgetting to Normalize Coefficients by a0
**What goes wrong:** Filter produces wildly wrong output or oscillates
**Why it happens:** The RBJ cookbook gives 6 coefficients (b0-b2, a0-a2). Implementation must divide all by a0 to get the 5 normalized coefficients used in DF2T.
**How to avoid:** `calcPeakingEQ()` returns pre-normalized coefficients (divided by a0 at computation time)
**Warning signs:** Any band boost/cut produces extreme gain or instability

### Pitfall 2: Filter State Accumulation Across Coefficient Changes
**What goes wrong:** Clicks or thumps when changing EQ settings
**Why it happens:** DF2T state variables (z1, z2) contain energy from old coefficients. Abrupt coefficient change with retained state causes discontinuity.
**How to avoid:** Per-sample linear interpolation smoothly transitions the coefficients, which naturally handles the state transition. Do NOT zero state variables on coefficient change — that causes its own click.
**Warning signs:** Audible artifacts only when changing settings, not during steady-state

### Pitfall 3: Denormalized Floats in Idle Filter
**What goes wrong:** CPU usage spikes when audio goes silent
**Why it happens:** Very small float values (denormals) in biquad state variables trigger slow microcode path on x86
**How to avoid:** Add tiny DC offset (1e-25f) to input, or use `-ffast-math` / `_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)` — but for ARM (Pi 4) this is not an issue as ARM flushes denormals by default
**Warning signs:** CPU usage stays high after audio stops

### Pitfall 4: int16_t Overflow on Output Conversion
**What goes wrong:** Harsh digital distortion when EQ boosts signal
**Why it happens:** Multiple bands boosted can push signal above int16_t max (32767). Simple cast wraps around.
**How to avoid:** The soft limiter catches this before conversion. Additionally, clamp to [-32768, 32767] after limiter as safety net.
**Warning signs:** Distortion only when multiple bands are boosted significantly

### Pitfall 5: Stereo Channel Crosstalk
**What goes wrong:** Subtle stereo image smearing
**Why it happens:** Using shared biquad state variables for both left and right channels
**How to avoid:** Each biquad band needs independent state (z1, z2) per channel. For stereo: 10 bands × 2 channels = 20 state pairs.
**Warning signs:** Mono content processed through EQ no longer sums to zero when L-R differenced

### Pitfall 6: Interpolation Re-trigger During Active Ramp
**What goes wrong:** Coefficient trajectory becomes non-linear or clicks occur
**Why it happens:** User drags slider rapidly, triggering new interpolation while previous is still in progress
**How to avoid:** When new coefficients arrive mid-interpolation, start new ramp from current interpolated position (not from old target). Capture the in-progress interpolated coefficients as the new "old" coefficients.
**Warning signs:** Artifacts only during rapid parameter changes

## Code Examples

### Complete Per-Sample Processing Pipeline
```cpp
// In RT callback, for each frame (L+R pair):
void processFrame(int16_t* frame, int channels) {
    for (int ch = 0; ch < channels; ++ch) {
        // 1. Convert int16_t → float [-1.0, 1.0]
        float sample = frame[ch] / 32768.0f;

        // 2. Apply 10 biquad stages in series
        for (int band = 0; band < 10; ++band) {
            sample = processSample(sample, coeffs[band], state[ch][band].z1, state[ch][band].z2);
        }

        // 3. Soft limit (prevent clipping)
        sample = limiter_[ch].process(sample);

        // 4. Convert float → int16_t with clamp
        int32_t out = static_cast<int32_t>(sample * 32768.0f);
        if (out > 32767) out = 32767;
        if (out < -32768) out = -32768;
        frame[ch] = static_cast<int16_t>(out);
    }
}
```

### Q Value for 1-Octave Bandwidth
```cpp
// For a 10-band graphic EQ with octave spacing:
// Q = 1 / (2 * sinh(ln(2)/2 * BW)) where BW = 1 octave
// Q = 1 / (2 * sinh(ln(2)/2)) ≈ 1.4142 ≈ sqrt(2)
static constexpr float kGraphicEqQ = 1.4142f;
```

### 10 ISO Center Frequencies
```cpp
static constexpr float kCenterFreqs[10] = {
    31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};
```

### Bypass with Crossfade
```cpp
// During bypass transition (48ms ramp):
// Process BOTH paths, crossfade between them
float wet = processAllBands(sample);  // EQ'd signal
float dry = sample;                   // original

float t = bypassRampPosition_ / float(kRampLength);  // 0→1 = engaging bypass
float output = dry * t + wet * (1.0f - t);

// When bypass fully engaged: skip processing entirely (save CPU)
// When bypass fully disengaged: skip crossfade (save multiply)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Direct Form I biquad | Direct Form II Transposed | Always preferred for float | Better numerical accuracy, 2 state vars instead of 4 |
| Lock + copy coefficients | Atomic pointer swap | C++11 atomics (2011) | Zero RT-thread blocking, trivial to implement |
| Block-rate coefficient update | Per-sample interpolation | Common since ~2005 | Eliminates all audible zipper noise |
| Hard clip limiter | Envelope-follower soft limiter | Standard practice | Transparent at moderate levels, graceful at extremes |

**Deprecated/outdated:**
- Direct Form I for floating-point: wastes state variables, worse numerical properties
- Coefficient interpolation at block boundaries only: audible stepping with fast parameter changes

## Open Questions

1. **Limiter attack/release tuning**
   - What we know: Fast attack (~0.5ms), slow release (~50ms) is the standard starting point
   - What's unclear: Exact values that sound best with car audio + 10 bands of ±12dB
   - Recommendation: Start with attack=0.5ms, release=50ms, tune by ear on Pi if needed. These are simple constants to adjust.

2. **Per-channel vs stereo-linked limiter**
   - What we know: Per-channel limiting preserves stereo image but can cause pumping on asymmetric content
   - What's unclear: Whether car audio (typically mono-summed at low frequencies) would benefit from stereo linking
   - Recommendation: Start with per-channel (simpler, no crosstalk). Add stereo linking later if pumping is audible — it probably won't be with this modest gain range.

3. **Interpolation length in samples**
   - What we know: User wants ~50ms. At 48kHz, that's 2400 samples. PipeWire quantum is typically 1024 samples (~21ms).
   - What's unclear: Whether to round to an exact quantum boundary
   - Recommendation: Use 2304 samples (48ms, divisible by common buffer sizes). Close enough to 50ms, and clean division simplifies the ramp math.

## Sources

### Primary (HIGH confidence)
- [RBJ Audio EQ Cookbook (W3C)](https://www.w3.org/TR/audio-eq-cookbook/) — Peaking EQ biquad coefficient formulas, Q definition, bandwidth relationship
- [RBJ Audio EQ Cookbook (WebAudio)](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html) — Same formulas, canonical source
- [Digital biquad filter - Wikipedia](https://en.wikipedia.org/wiki/Digital_biquad_filter) — DF2T topology description and equations
- [ARM CMSIS-DSP DF2T](https://arm-software.github.io/CMSIS_5/DSP/html/group__BiquadCascadeDF2T.html) — ARM's recommendation of DF2T for floating-point

### Secondary (MEDIUM confidence)
- [Musicdsp.org Limiter](https://www.musicdsp.org/en/latest/Filters/265-output-limiter-using-envelope-follower-in-c.html) — Envelope follower limiter algorithm and recommended settings
- [EarLevel Engineering - Biquads](https://www.earlevel.com/main/2003/02/28/biquads/) — DF1 vs DF2T comparison, Q for octave bandwidth
- [Signalsmith Audio - Limiter Design](https://signalsmith-audio.co.uk/writing/2022/limiter/) — Limiter architecture patterns
- [KVR Audio Forums](https://www.kvraudio.com/forum/viewtopic.php?t=528591) — Community consensus on coefficient interpolation approaches
- [timur.audio - Lock-free RT audio](https://timur.audio/using-locks-in-real-time-audio-processing-safely) — Atomic patterns for RT audio parameter updates

### Tertiary (LOW confidence)
- None — all findings verified against multiple authoritative sources

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — biquad EQ is textbook DSP, RBJ cookbook is the universal standard, DF2T is well-documented
- Architecture: HIGH — atomic double-buffer and per-sample interpolation are established patterns in RT audio
- Pitfalls: HIGH — these are well-known issues documented across multiple DSP resources and forums

**Research date:** 2026-03-01
**Valid until:** Indefinite — this is fundamental DSP math that doesn't change
