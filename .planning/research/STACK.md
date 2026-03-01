# Technology Stack: Audio Equalizer

**Project:** OpenAuto Prodigy v0.4.1
**Researched:** 2026-03-01
**Focus:** 10-band graphic EQ — what to add to the existing stack

## Recommendation: Inline Biquad Processing, No New Dependencies

The EQ should be implemented as **inline biquad filter processing inside the existing `onPlaybackProcess` PipeWire callback** — not as a separate PipeWire filter node, not with an external DSP library. The math is ~15 lines of code per filter stage. Adding a library for this would be over-engineering.

**Zero new packages. Zero new build dependencies. ~150 lines of DSP code using `<cmath>`.**

## What's Needed (New Code)

### EQ DSP Core
| Component | What | Why |
|-----------|------|-----|
| `BiquadFilter` class | ~50 lines C++ — coefficients + `process(float)` | One biquad = one EQ band. Transposed Direct Form II. |
| `EqProcessor` class | 10 `BiquadFilter` instances per channel, processes sample buffers | Wraps the chain, provides `apply(float* samples, int count, int channels)` |
| RBJ coefficient calculator | Static function: `(frequency, gain_dB, Q, sampleRate) -> coefficients` | Robert Bristow-Johnson's Audio EQ Cookbook peaking filter formula |

**No new libraries needed.** The entire DSP core is ~150 lines of C++ using only `<cmath>`.

### EqService — Config + State Management
| Component | What | Why |
|-----------|------|-----|
| `EqService` class | Owns per-stream EqProcessor instances, manages presets, exposes Q_PROPERTYs for QML | Follows existing service pattern (ThemeService, AudioService) |
| YAML preset format | Band gains stored in config, preset library as bundled + user-created | Matches existing YAML config pattern with deep merge |

### QML UI
| Component | What | Why |
|-----------|------|-----|
| EQ slider panel | 10 vertical sliders (-12dB to +12dB) with band labels | Head unit touch interface — must work at 1024x600 |
| Preset selector | ComboBox or tile grid for preset selection | Quick switching while driving |
| Stream selector | Tab bar or similar for media/navigation/phone | Per-stream profile switching |

### Web Config Extension
| Component | What | Why |
|-----------|------|-----|
| EQ panel page | HTML/JS sliders + preset management | Detailed adjustment from phone/laptop |
| IPC messages | `eq/get-profile`, `eq/set-bands`, `eq/list-presets`, `eq/save-preset` | Extends existing Unix socket JSON protocol |

## Why Inline Processing (Not PipeWire Filter Nodes)

I evaluated three approaches. The previous research (v1.0 STACK.md) recommended PipeWire filter-chain. After examining the actual AudioService code, **that recommendation was wrong for this project.** Here's why:

### Option A: Inline in `onPlaybackProcess` callback -- RECOMMENDED

**How:** After reading from ring buffer, convert S16->float, apply biquad chain, convert float->S16, write to PW buffer.

**Pros:**
- Zero additional latency — processing happens in the same callback that already runs
- No PipeWire graph changes — no link management, no WirePlumber interference
- Per-stream EQ is trivial — each stream's callback has its own EqProcessor
- No new PipeWire API surface — stays within `pw_stream` which is already battle-tested here
- Works on dev VM (unit tests run DSP math without PipeWire daemon)
- Doesn't disrupt the existing adaptive rate matching (PI controller in callback)
- Simple bypass — set a flag, skip the processing loop

**Cons:**
- Slightly more CPU in the RT callback (~0.01ms for 10 biquads on Pi 4 at 48kHz — negligible vs the 21ms quantum)
- Format conversion overhead (S16 <-> float) — ~2 operations per sample, also negligible

### Option B: PipeWire `pw_filter` nodes -- REJECTED

**How:** Create separate filter nodes per stream, link between app streams and the sink.

**Why not:**
- Requires programmatic link management (`pw_link` API) — WirePlumber may fight with manual links or reconnect them wrong
- Adds graph latency (extra node = extra quantum of buffering = +21ms at 48kHz/1024)
- Per-stream filtering requires 3 separate filter nodes with link lifecycle tracking
- **Critically: the app's adaptive rate matching PI controller lives in `onPlaybackProcess`.** Inserting a filter node between stream and sink means the rate controller can't see actual sink buffer state — it breaks clock drift compensation.

### Option C: PipeWire `filter-chain` module with config files -- REJECTED

**How:** Generate PipeWire filter-chain config files, load as modules.

**Why not:**
- Config is file-based — live coefficient updates require module reload (audible glitch)
- Per-stream control is awkward — filter-chain operates on PipeWire graph nodes, not app-internal streams. Would need to identify nodes by name and route manually.
- Requires `libpipewire-module-filter-chain` to be present (likely is on Trixie, but adds a runtime dependency to verify)
- On the dev VM (PipeWire 1.0.5), filter-chain module may not have the same features as 1.4.2
- **Previous STACK.md recommended this approach.** After reading the actual AudioService code, it's clear this is the wrong fit — the app's audio architecture (ring buffer -> callback -> PW buffer) doesn't need an external graph node.

**Verdict:** Option A wins decisively. The app already owns the audio callback. Inserting 10 biquads there is the simplest, lowest-latency, most maintainable approach.

## Biquad Implementation Details

### RBJ Peaking EQ Formula (Audio EQ Cookbook)

For a 10-band graphic EQ, each band uses a **peaking EQ** biquad:

```
w0 = 2 * pi * f0 / sampleRate
alpha = sin(w0) / (2 * Q)
A = 10^(dBgain / 40)   // note: /40 not /20 — this is amplitude, not power

b0 =  1 + alpha * A
b1 = -2 * cos(w0)
b2 =  1 - alpha * A
a0 =  1 + alpha / A
a1 = -2 * cos(w0)
a2 =  1 - alpha / A
```

Normalize all coefficients by dividing by `a0`.

**Confidence:** HIGH — This is the W3C-hosted Audio EQ Cookbook, the definitive reference used by virtually every software EQ implementation.

### Standard 10-Band Frequencies (Octave Spacing)

| Band | Frequency (Hz) |
|------|----------------|
| 1 | 31 |
| 2 | 62 |
| 3 | 125 |
| 4 | 250 |
| 5 | 500 |
| 6 | 1000 |
| 7 | 2000 |
| 8 | 4000 |
| 9 | 8000 |
| 10 | 16000 |

Q = 1.414 (sqrt(2)) for graphic EQ gives ~1 octave bandwidth per band with smooth overlap between adjacent bands. This is the standard value used by hardware graphic EQs.

### Transposed Direct Form II (process loop)

```cpp
float BiquadFilter::process(float in) {
    float out = b0_ * in + z1_;
    z1_ = b1_ * in - a1_ * out + z2_;
    z2_ = b2_ * in - a2_ * out;
    return out;
}
```

Two state variables per filter, 5 multiply-adds per sample per band. For 10 bands, stereo, 48kHz: ~1.92M multiply-adds/sec. The Pi 4's Cortex-A72 does billions per second. This is free.

### Integration Point: `onPlaybackProcess`

Current flow:
```
ring buffer -> read S16 bytes -> silence fill -> write to PW buffer
```

New flow:
```
ring buffer -> read S16 bytes -> silence fill -> S16->float -> EQ process -> float->S16 -> write to PW buffer
```

The conversion is cheap and fits naturally after the existing silence-fill:
```cpp
// S16 interleaved to float (in-place with temp buffer on stack or handle)
for (uint32_t i = 0; i < totalSamples; i++)
    floatBuf[i] = static_cast<float>(s16Buf[i]) / 32768.0f;

// Apply 10-band EQ cascade
eqProcessor->apply(floatBuf, numFrames, channels);

// Float to S16 with clamp
for (uint32_t i = 0; i < totalSamples; i++)
    s16Buf[i] = static_cast<int16_t>(
        std::clamp(floatBuf[i] * 32768.0f, -32768.0f, 32767.0f));
```

**Float buffer allocation:** Pre-allocate a float buffer in `AudioStreamHandle` (same size as max PW buffer). No allocation in the RT callback.

### Thread Safety: Double-Buffered Coefficients

The RT callback reads coefficients. The Qt main thread writes them when the user adjusts a slider.

```cpp
struct EqCoefficients {
    std::array<BiquadCoeffs, 10> bands;
    bool enabled = true;
};

// In EqProcessor:
std::atomic<int> activeIndex_{0};
EqCoefficients buffers_[2];

// Qt thread writes:
void updateBand(int band, float gainDb) {
    int inactive = 1 - activeIndex_.load(std::memory_order_acquire);
    buffers_[inactive] = buffers_[activeIndex_];  // copy current
    buffers_[inactive].bands[band] = computeCoeffs(freq, gainDb, Q);
    activeIndex_.store(inactive, std::memory_order_release);
}

// RT thread reads:
const EqCoefficients& active() const {
    return buffers_[activeIndex_.load(std::memory_order_acquire)];
}
```

No locks in the RT path. Atomic index swap is wait-free.

## Per-Stream EQ Architecture

Each `AudioStreamHandle` gets an `EqProcessor*` member. The `EqService` creates and owns per-stream processors, and updates coefficients when gains change.

**Stream-to-profile mapping:**
- `media` stream -> media EQ profile (music, podcasts)
- `navigation` stream -> navigation EQ profile (voice clarity)
- `phone` stream -> phone EQ profile (voice clarity)

Different profiles make sense: you want bass boost for music but voice clarity for navigation.

## Preset Storage (YAML)

Integrated into existing config, not separate files:

```yaml
equalizer:
  enabled: true
  profiles:
    media:
      preset: "Rock"
      bands: [3, 5, -1, -2, 0, 2, 4, 5, 3, 1]  # dB gains per band
    navigation:
      preset: "Voice Boost"
      bands: [0, 0, 0, 2, 4, 6, 4, 2, 0, 0]
    phone:
      preset: "Voice Boost"
      bands: [0, 0, 0, 2, 4, 6, 4, 2, 0, 0]
  user_presets:
    "My Custom":
      bands: [-2, 0, 1, 3, 2, 0, -1, 1, 3, 2]
```

**Bundled presets** (compiled into the binary, not files):
- Flat (all 0dB — bypass equivalent)
- Rock (+3, +5, -1, -2, 0, +2, +4, +5, +3, +1)
- Pop (-1, +2, +4, +5, +4, +2, 0, -1, -1, -1)
- Jazz (+3, +2, 0, +2, -2, -2, 0, +2, +3, +3)
- Bass Boost (+6, +5, +4, +2, 0, 0, 0, 0, 0, 0)
- Treble Boost (0, 0, 0, 0, 0, +2, +3, +4, +5, +5)
- Voice Boost (0, 0, 0, +2, +4, +6, +4, +2, 0, 0)
- Bass Cut (-4, -3, -2, -1, 0, 0, 0, 0, 0, 0)

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| DSP approach | Inline biquad in callback | PipeWire filter-chain | Can't do live coefficient updates without glitchy module reload. Doesn't integrate with per-stream model. Breaks rate matching. |
| DSP approach | Inline biquad in callback | `pw_filter` nodes | Link management nightmare, +21ms latency per node, breaks rate matching PI controller. |
| DSP library | Hand-rolled ~150 LOC | KFR framework | 50MB+ dependency for 150 lines of math. Absurd. |
| DSP library | Hand-rolled ~150 LOC | FFTW + overlap-add FIR | FIR EQ is overkill for 10 bands. Biquad cascade is the standard, simpler, lower latency. |
| Coefficient formula | RBJ Audio EQ Cookbook | Custom derivation | RBJ is the industry standard, now hosted by W3C. No reason to deviate. |
| Thread safety | Double-buffered coefficients | Mutex in RT callback | Mutex in RT path = priority inversion risk. Never lock in audio callbacks. |
| Thread safety | Double-buffered coefficients | `std::atomic<float>` per coeff | 5 coefficients per band x 10 bands = 50 atomic loads per sample. Double-buffer is one atomic load per callback. |
| Preset storage | YAML in existing config | Separate JSON files | Existing config system works. Don't add a second format for one feature. |
| Float buffer | Pre-allocated in AudioStreamHandle | Stack allocation | PW buffer can be up to 4096 frames x 2ch = 32KB of floats. Too large for stack on some configs. |

## Version Compatibility

| Technology | Dev VM (Ubuntu 24.04) | Pi (RPi OS Trixie) | Impact |
|------------|----------------------|---------------------|--------|
| PipeWire | 1.0.5 | 1.4.2 | No impact — inline processing uses `pw_stream` API only, no filter-chain modules |
| Qt | 6.4 | 6.8 | No impact — EQ UI uses basic QML controls (Slider, ComboBox, Repeater) available in both |
| C++ | C++17 | C++17 | `std::clamp`, `<cmath>`, `<array>`, `<atomic>` — all available |
| `<cmath>` | standard | standard | `sin()`, `cos()`, `pow()`, `tan()`, `M_PI` — no platform concerns |

## New Files to Create

| File | Purpose | Lines (est.) |
|------|---------|-------------|
| `src/core/audio/BiquadFilter.hpp` | Coefficient calc + process (header-only) | ~80 |
| `src/core/audio/EqProcessor.hpp` | 10-band chain with double-buffered coefficients | ~60 |
| `src/core/audio/EqProcessor.cpp` | Implementation | ~120 |
| `src/core/services/EqService.hpp` | Service interface: per-stream profiles, presets, QML props | ~80 |
| `src/core/services/EqService.cpp` | Implementation | ~200 |
| `qml/applications/equalizer/EqView.qml` | Main EQ UI (sliders + presets + stream tabs) | ~150 |
| `web-config/templates/equalizer.html` | Web config EQ page | ~200 |
| `tests/test_biquad.cpp` | Coefficient calculation + filtering accuracy tests | ~100 |
| `tests/test_eq_service.cpp` | Preset management, per-stream profiles, config persistence | ~80 |

**Estimated total new code:** ~1,070 lines across 9 files. Small feature, well-scoped.

## Installation

```bash
# No new packages needed.
# The entire EQ implementation uses only:
#   - <cmath> (sin, cos, pow, M_PI)
#   - <atomic> (double-buffer swap)
#   - <array> (coefficient storage)
#   - <algorithm> (std::clamp)
#   - Existing PipeWire headers (already linked)
#   - Existing yaml-cpp (already linked)
#   - Existing Qt Quick Controls (already linked)
```

## Sources

- [W3C Audio EQ Cookbook (RBJ)](https://www.w3.org/TR/audio-eq-cookbook/) — Biquad coefficient formulas, the definitive reference (HIGH confidence)
- [WebAudio EQ Cookbook (original text)](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html) — Same content, original hosting (HIGH confidence)
- [EarLevel Engineering Biquad C++ Source](https://www.earlevel.com/main/2012/11/26/biquad-c-source-code/) — Clean implementation reference (HIGH confidence)
- [PipeWire Tutorial 7: Audio DSP Filter](https://docs.pipewire.org/devel/page_tutorial7.html) — pw_filter API pattern (HIGH confidence, evaluated and rejected for this use case)
- [PipeWire Filter API](https://docs.pipewire.org/group__pw__filter.html) — pw_filter reference (HIGH confidence)
- [PipeWire Filter-Chain Module](https://docs.pipewire.org/page_module_filter_chain.html) — Alternative approach evaluated and rejected (HIGH confidence)
- [Debian Trixie PipeWire Package](https://packages.debian.org/trixie/pipewire) — Version 1.4.2 confirmed (HIGH confidence)
- [DSP-Cpp-filters](https://github.com/dimtass/DSP-Cpp-filters) — Reference implementations of various IIR filters in C++ (MEDIUM confidence)

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Biquad math / RBJ formulas | HIGH | W3C standard, used everywhere, thoroughly verified |
| Inline processing approach | HIGH | Standard technique; existing callback already does volume, silence fill, rate matching |
| Double-buffer thread safety | HIGH | Well-known lock-free pattern for RT audio |
| Per-stream EQ profiles | HIGH | Natural extension — each AudioStreamHandle is independent |
| Pi 4 performance | HIGH | 10 biquads x 2 channels x 48kHz = trivial for Cortex-A72 |
| QML slider UI at 1024x600 | MEDIUM | Need to verify 10 sliders fit comfortably on the small display — may need horizontal scroll or compact layout |
