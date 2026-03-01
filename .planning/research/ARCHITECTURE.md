# Architecture Patterns

**Domain:** 10-band graphic equalizer for PipeWire-based Android Auto head unit
**Researched:** 2026-03-01

## Recommended Architecture

**In-process biquad DSP applied inside the existing `onPlaybackProcess` callback.** No external PipeWire filter-chain modules, no `pw_filter` nodes, no graph rewiring.

### Why In-Process, Not Filter-Chain

There are three possible approaches. Only one makes sense for this project.

| Approach | How It Works | Verdict |
|----------|-------------|---------|
| **PipeWire filter-chain module** | External JSON/conf config, spawns separate graph nodes, auto-wired between stream and sink | REJECTED -- requires managing external PW config files, module loading, graph link management. Per-stream control means 3 separate filter-chain instances. No clean API to update coefficients at runtime without reloading the module. Adds system-level complexity for a feature that should be app-internal. |
| **`pw_filter` DSP node** | Create in-process `pw_filter` objects that sit between streams and sink, processing float32 samples | REJECTED -- `pw_filter` requires manual port linking via `pw-link` or graph manager. Adds 3 extra PW nodes to the graph. Does not integrate with existing ring buffer architecture. Would require converting S16LE to float32 at node boundaries. Architectural mismatch. |
| **Inline biquad in `onPlaybackProcess`** | Apply biquad cascade directly to PCM samples in the existing PW stream callback, between ring buffer read and buffer queue | RECOMMENDED -- zero architectural disruption. Per-stream EQ is trivial (each stream handle carries its own filter state). No extra PW nodes, no graph complexity, no format conversion overhead. Runs on the PW RT thread where the audio already lives. |

**Confidence: HIGH** -- The existing `onPlaybackProcess` already reads samples from the ring buffer and writes them to the PW buffer. Inserting DSP between those two steps is a textbook integration point. The PW RT thread is designed for this kind of processing.

### Data Flow: Current vs. With EQ

**Current flow:**
```
ASIO thread -> ringBuffer.write() -> [PW RT callback] -> ringBuffer.read() -> silence-fill -> pw_stream_queue_buffer()
```

**With EQ:**
```
ASIO thread -> ringBuffer.write() -> [PW RT callback] -> ringBuffer.read() -> EQ biquad cascade -> silence-fill -> pw_stream_queue_buffer()
```

The EQ processing happens in the PW RT callback (`onPlaybackProcess`), after reading from the ring buffer and before writing to the PW buffer. This is the same thread context, same timing constraints, same buffer lifecycle.

**Critical detail:** Audio is currently S16LE (16-bit signed integer). Biquad filters need floating-point math. The process callback must:
1. Read S16LE samples from ring buffer (existing)
2. Convert to float32 (NEW)
3. Apply biquad cascade per channel (NEW)
4. Convert back to S16LE with clamping (NEW)
5. Silence-fill remainder (existing)
6. Queue buffer (existing)

The int16-to-float conversion is cheap (~0.5ns per sample on Pi 4 ARM). For stereo 48kHz at 1024-sample quantum, that's ~2048 samples = ~1us. Negligible.

## Component Boundaries

### New Components

| Component | File(s) | Responsibility | Thread Safety |
|-----------|---------|---------------|---------------|
| **BiquadFilter** | `src/core/audio/BiquadFilter.hpp` | Single biquad stage: holds coefficients + state (z1, z2). Processes float samples in-place. Header-only, no allocations. | Single-thread only (one instance per stream per channel) |
| **EqualizerProcessor** | `src/core/audio/EqualizerProcessor.hpp/cpp` | 10-band cascade of biquads per channel. Owns coefficient recalculation. Provides `process(int16_t*, uint32_t nFrames, int channels)`. | Created on main thread, `process()` called on PW RT thread. Coefficient updates use atomic swap of a shared_ptr to immutable coefficient set. |
| **EqualizerService** | `src/core/services/EqualizerService.hpp/cpp` | QObject singleton. Holds per-stream EQ profiles, preset library, config persistence. Exposes Q_PROPERTYs for QML. Communicates with EqualizerProcessors owned by each stream. | Main thread for state/UI. Pushes coefficient updates to processors via atomic pointer swap. |

### Modified Components

| Component | Change | Risk |
|-----------|--------|------|
| **AudioStreamHandle** | Add `std::unique_ptr<EqualizerProcessor> eqProcessor` field. Initialized when stream is created. | LOW -- struct addition, no behavioral change to existing fields |
| **AudioService::onPlaybackProcess** | Insert EQ processing between ring buffer read and buffer queue. ~15 lines of code. | LOW -- isolated insertion point, no change to ring buffer or rate matching logic |
| **AudioService::createStream** | Initialize EQ processor from EqualizerService profile for the stream. | LOW -- additive, after existing stream setup |
| **IpcServer** | Add `get_eq_config` / `set_eq_config` / `get_eq_presets` handlers. Follow exact pattern of existing `handleGetAudioConfig`. | LOW -- additive, follows established pattern |
| **YamlConfig** | Add `audio.equalizer` config section with defaults. | LOW -- follows existing config defaults gate pattern |
| **web-config/server.py** | Add `/equalizer` route + `/api/eq/*` endpoints. | LOW -- additive |

### Unchanged Components

These are explicitly NOT modified:
- **AudioRingBuffer** -- EQ happens after the read, not inside the buffer
- **PipeWireDeviceRegistry** -- device enumeration is orthogonal to EQ
- **Rate matching logic** -- EQ does not change timing, buffer levels, or rate control
- **Audio focus / ducking** -- volume ducking is applied via PW stream controls, independent of EQ

## Detailed Design

### BiquadFilter (Header-Only)

```cpp
// src/core/audio/BiquadFilter.hpp
struct BiquadCoeffs {
    float b0, b1, b2, a1, a2;  // a0 normalized to 1.0
};

class BiquadFilter {
public:
    void setCoeffs(const BiquadCoeffs& c) { c_ = c; }

    float process(float in) {
        // Transposed Direct Form II -- fewer quantization errors
        float out = c_.b0 * in + z1_;
        z1_ = c_.b1 * in - c_.a1 * out + z2_;
        z2_ = c_.b2 * in - c_.a2 * out;
        return out;
    }

    void reset() { z1_ = z2_ = 0.0f; }

private:
    BiquadCoeffs c_{1.0f, 0.0f, 0.0f, 0.0f, 0.0f};  // unity passthrough
    float z1_ = 0.0f, z2_ = 0.0f;
};
```

Uses Transposed Direct Form II per the Audio EQ Cookbook. This is the standard for floating-point biquad implementation -- fewer multiply-accumulate operations and better numerical stability than Direct Form I.

### Coefficient Computation (RBJ Cookbook peakingEQ)

Each band uses the peakingEQ biquad type from the Audio EQ Cookbook:

```
A  = 10^(dBgain/40)
w0 = 2*pi*f0/Fs
alpha = sin(w0)/(2*Q)

b0 =   1 + alpha*A
b1 =  -2*cos(w0)
b2 =   1 - alpha*A
a0 =   1 + alpha/A
a1 =  -2*cos(w0)
a2 =   1 - alpha/A
```

All coefficients are normalized by dividing by a0. When dBgain = 0, the filter reduces to unity (passthrough), so flat bands add zero processing artifact.

### Standard 10-Band Frequencies

ISO octave-spaced center frequencies:

| Band | Frequency | Typical Use |
|------|-----------|-------------|
| 1 | 31 Hz | Sub-bass rumble |
| 2 | 62 Hz | Bass kick |
| 3 | 125 Hz | Bass body |
| 4 | 250 Hz | Low mids, warmth |
| 5 | 500 Hz | Mid body |
| 6 | 1 kHz | Mid presence |
| 7 | 2 kHz | Upper mids, clarity |
| 8 | 4 kHz | Presence, attack |
| 9 | 8 kHz | Brilliance |
| 10 | 16 kHz | Air, sparkle |

Q factor = 1.41 (approximately 1 octave bandwidth) for all bands. This is the standard for graphic EQs -- wide enough that adjacent bands overlap smoothly, narrow enough to provide independent control.

### EqualizerProcessor

```cpp
// src/core/audio/EqualizerProcessor.hpp
class EqualizerProcessor {
public:
    static constexpr int kNumBands = 10;
    static constexpr float kFrequencies[kNumBands] = {
        31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
        1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
    };
    static constexpr float kDefaultQ = 1.41f;  // ~1 octave bandwidth

    explicit EqualizerProcessor(int sampleRate, int channels);

    /// Process interleaved S16LE samples in-place.
    /// Called from PW RT thread -- must be RT-safe.
    void process(int16_t* samples, uint32_t nFrames, int channels);

    /// Update band gains. Called from main thread.
    /// Internally swaps an atomic pointer to new coefficients.
    void setGains(const std::array<float, kNumBands>& gainsDb);

    /// Bypass toggle (atomic bool).
    void setEnabled(bool enabled);
    bool isEnabled() const;

private:
    // Per-channel, per-band filter state -- NOT shared across channels
    // filters_[channel][band]
    std::vector<std::array<BiquadFilter, kNumBands>> filters_;

    // Coefficient update: main thread writes new coeffs, RT thread reads
    struct CoeffSet {
        std::array<BiquadCoeffs, kNumBands> bands;
    };
    std::atomic<CoeffSet*> pendingCoeffs_{nullptr};
    std::unique_ptr<CoeffSet> activeCoeffs_;

    std::atomic<bool> enabled_{true};
    int sampleRate_;
};
```

**Thread safety approach:** The main thread computes new biquad coefficients (from the RBJ cookbook formulas) and stores them behind an atomic pointer. The RT callback checks the atomic, and if non-null, swaps in the new coefficients. This avoids locks on the RT thread entirely. The coefficient computation (trig functions) happens on the main thread where it does not matter.

**RT-safe `process()` implementation outline:**
1. Check `enabled_` atomic -- if false, return immediately (passthrough)
2. Check `pendingCoeffs_` atomic -- if non-null, swap into active and update all filter instances
3. Convert S16LE buffer to float32 in a stack-allocated scratch buffer
4. For each channel, run samples through 10 biquad stages in series
5. Convert float32 back to S16LE with clamping to [-32768, 32767]

The scratch buffer for float conversion can be stack-allocated up to reasonable sizes. At 1024 frames x 2 channels = 8KB of float32, well within RT stack limits.

### EqualizerService (QObject)

```cpp
class EqualizerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString activeStream READ activeStream WRITE setActiveStream NOTIFY activeStreamChanged)
    Q_PROPERTY(QVariantList bandGains READ bandGains NOTIFY bandsChanged)
    Q_PROPERTY(QStringList presetNames READ presetNames NOTIFY presetsChanged)
    Q_PROPERTY(QString activePreset READ activePreset NOTIFY presetChanged)

public:
    // Per-stream profile management
    Q_INVOKABLE QVariantList bandGainsForStream(const QString& stream) const;
    Q_INVOKABLE void setBandGain(const QString& stream, int band, float gainDb);
    Q_INVOKABLE void loadPreset(const QString& stream, const QString& presetName);
    Q_INVOKABLE void saveUserPreset(const QString& name);
    Q_INVOKABLE void deleteUserPreset(const QString& name);

    // Called by AudioService when creating/destroying a stream
    EqualizerProcessor* createProcessor(const QString& streamName,
                                         int sampleRate, int channels);

    // Config persistence
    void loadFromConfig(YamlConfig* config);
    void saveToConfig(YamlConfig* config, const QString& configPath);

signals:
    void enabledChanged();
    void activeStreamChanged();
    void bandsChanged(const QString& stream);
    void presetChanged();
    void presetsChanged();
};
```

Follows the ThemeService pattern: QObject singleton with Q_PROPERTYs exposed to QML context, Q_INVOKABLE methods for UI interaction, and signal emission for reactive updates.

### Integration Point: onPlaybackProcess

The change to the existing process callback is minimal. Approximately 15 lines inserted between the ring buffer read and the buffer queue:

```cpp
// In AudioService::onPlaybackProcess, after ringBuffer.read():

uint32_t bytesRead = handle->ringBuffer->read(
    static_cast<uint8_t*>(d.data), wantBytes);

// --- NEW: Apply EQ if processor exists and is enabled ---
if (bytesRead > 0 && handle->eqProcessor &&
    handle->eqProcessor->isEnabled()) {
    uint32_t nFrames = bytesRead / handle->bytesPerFrame;
    handle->eqProcessor->process(
        static_cast<int16_t*>(d.data), nFrames, handle->channels);
}

// Existing silence-fill and queue logic unchanged below
if (bytesRead < wantBytes)
    std::memset(static_cast<uint8_t*>(d.data) + bytesRead, 0,
                wantBytes - bytesRead);
```

The EQ only processes bytes that were actually read (not the silence fill). When the processor is null or disabled, there is zero overhead -- a single pointer check and atomic load.

### Config Schema

```yaml
audio:
  equalizer:
    enabled: true
    profiles:
      media:
        preset: "flat"
        gains: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]  # dB per band
      navigation:
        preset: "voice"
        gains: [0, 0, 0, 2, 4, 4, 2, 0, 0, 0]
      phone:
        preset: "voice"
        gains: [0, 0, 0, 2, 4, 4, 2, 0, 0, 0]
    user_presets:
      my_car_tune:
        gains: [3, 2, 0, -1, 0, 1, 3, 4, 3, 1]
```

Per-stream profiles allow different EQ curves for music vs. navigation voice vs. phone calls. Config persistence uses the existing `setValueByPath` / `valueByPath` pattern with the config defaults gate.

### Bundled Presets (Code-Defined)

These are compiled into the binary, not stored in YAML. User presets are stored in YAML under `audio.equalizer.user_presets`.

| Preset | 31 | 62 | 125 | 250 | 500 | 1k | 2k | 4k | 8k | 16k | Notes |
|--------|----|----|-----|-----|-----|----|----|----|----|----|-------|
| Flat | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | Unity passthrough |
| Rock | 4 | 3 | 0 | -2 | -1 | 1 | 3 | 4 | 4 | 3 | V-shaped |
| Pop | -1 | 1 | 3 | 4 | 3 | 0 | -1 | 0 | 1 | 2 | Mid-forward |
| Jazz | 3 | 2 | 0 | 1 | -1 | -1 | 0 | 1 | 2 | 3 | Warm + airy |
| Bass Boost | 6 | 5 | 3 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | Low end emphasis |
| Treble Boost | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 3 | 5 | 6 | High end emphasis |
| Voice | -2 | -1 | 0 | 2 | 4 | 4 | 2 | 0 | -1 | -2 | Speech clarity |
| Car Cabin | 3 | 2 | 0 | -1 | -2 | 0 | 1 | 2 | 3 | 2 | Compensate road noise |

Gain values are in dB, range [-12, +12].

### IPC Protocol Extensions

New commands follow the exact JSON request/response pattern used by all existing IPC handlers:

```json
// Get full EQ state
{"command": "get_eq_config"}
// Response:
{
  "enabled": true,
  "profiles": {
    "media": {"preset": "flat", "gains": [0,0,0,0,0,0,0,0,0,0]},
    "navigation": {"preset": "voice", "gains": [0,0,0,2,4,4,2,0,0,0]},
    "phone": {"preset": "voice", "gains": [0,0,0,2,4,4,2,0,0,0]}
  },
  "presets": ["flat","rock","pop","jazz","bass_boost","treble_boost","voice","car_cabin"],
  "user_presets": ["my_car_tune"]
}

// Set single band gain
{"command": "set_eq_band", "data": {"stream": "media", "band": 3, "gain": 4.5}}
// Response: {"ok": true}

// Load preset for a stream
{"command": "set_eq_preset", "data": {"stream": "media", "preset": "rock"}}
// Response: {"ok": true}

// Save current gains as user preset
{"command": "save_eq_preset", "data": {"stream": "media", "name": "my_tune"}}
// Response: {"ok": true}

// Toggle EQ enabled/disabled
{"command": "set_eq_enabled", "data": {"enabled": false}}
// Response: {"ok": true}
```

### QML UI Integration

New settings sub-view `EqualizerSettings.qml` accessible from the Settings menu. Contains:

- 10 vertical sliders (one per band), labeled with frequency
- Stream selector tabs (Media / Navigation / Phone)
- Preset picker (FullScreenPicker, follows existing pattern)
- Enable/disable toggle
- Save/delete custom preset buttons

The sliders bind to `EqualizerService.bandGainsForStream(currentStream)` and call `EqualizerService.setBandGain()` on value change. The reactive signal `bandsChanged` updates all sliders when a preset is loaded.

**Touch considerations for car use:** Vertical sliders need to be wide enough for finger targeting (at least 44px per band on 1024x600 = 440px for 10 bands). Remaining horizontal space accommodates labels and preset controls. Slider height should be at least 200px for meaningful dB resolution at glance speed.

## Data Flow Diagram

```
                    Main Thread                          PW RT Thread
                    ~~~~~~~~~~~                          ~~~~~~~~~~~~

User adjusts slider
        |
        v
EqualizerService.setBandGain()
        |
        v
Compute biquad coefficients
(RBJ cookbook, trig math)
        |
        v
Store in atomic pointer on
EqualizerProcessor
        |
        :
        :                    onPlaybackProcess()
        :                           |
        :                           v
        :                    ringBuffer.read(S16LE)
        :                           |
        :                           v
        :....................> Check atomic for new coeffs
                                    |  (if non-null, swap in)
                                    v
                             process(S16LE data):
                               S16 -> float32 (stack buf)
                               10-stage biquad cascade
                               float32 -> S16 (clamp)
                                    |
                                    v
                             Silence-fill remainder
                                    |
                                    v
                             Rate matching (unchanged)
                                    |
                                    v
                             pw_stream_queue_buffer()
```

## Performance Budget

| Operation | Cost per callback (1024 frames, stereo) | Notes |
|-----------|----------------------------------------|-------|
| S16LE -> float32 | ~2us | 2048 samples, simple multiply |
| 10 biquads x 2 channels | ~15us | 5 FP ops per sample per biquad x 10 x 2048 |
| float32 -> S16LE | ~2us | 2048 samples, clamp + cast |
| Coefficient check | ~0ns | Single atomic load |
| **Total EQ overhead** | **~19us** | |
| PW quantum at 48kHz/1024 | 21,333us | Available budget per callback |
| **EQ as % of quantum** | **~0.09%** | Negligible |

**Confidence: HIGH** -- Pi 4 Cortex-A72 handles floating-point natively with NEON SIMD. Even without SIMD optimization, 10 biquad bands at 48kHz stereo is well within budget. The existing rate matching and ring buffer operations already consume <1% of the quantum.

## Anti-Patterns to Avoid

### Anti-Pattern 1: External PipeWire Filter-Chain
**What:** Using `libpipewire-module-filter-chain` or `libpipewire-module-parametric-equalizer` to handle EQ externally.
**Why bad:** Requires managing PW config files outside the app, complicates per-stream control (need 3 separate filter instances with manual graph linking), runtime coefficient updates require module reload, adds failure modes when PW graph changes, makes the app dependent on PW module availability.
**Instead:** In-process biquad DSP in the existing callback.

### Anti-Pattern 2: Separate Float32 PW Stream Format
**What:** Changing the PW stream format from S16LE to F32LE to avoid the int-to-float conversion.
**Why bad:** Breaks the existing ring buffer which is byte-oriented S16LE. Would require changing the protocol audio output path, the ring buffer, and every consumer. The format conversion cost is negligible (<5us) and not worth the architectural disruption.
**Instead:** Convert locally in the process callback using a stack buffer.

### Anti-Pattern 3: Mutex-Protected Coefficient Updates
**What:** Using a QMutex to protect coefficient reads/writes between main and RT threads.
**Why bad:** Priority inversion. If the main thread holds the mutex when the RT callback fires, the audio callback blocks waiting for the main thread -- causing audio glitches. This is the classic RT audio anti-pattern.
**Instead:** Atomic pointer swap. Main thread allocates new coefficients, stores pointer atomically. RT thread reads atomically, swaps in new set. Old coefficients freed on main thread via QTimer::singleShot.

### Anti-Pattern 4: Per-Callback Signal Emission
**What:** Emitting Qt signals from the EQ processor to update QML on every audio callback.
**Why bad:** The PW callback fires ~47 times/second at 1024/48000. Flooding the Qt event loop with cross-thread signal emissions causes marshaling overhead and UI jank. Also violates RT-safety (signal emission allocates).
**Instead:** EQ state flows one-way: UI -> EqualizerService -> EqualizerProcessor. The processor never signals back to the UI.

## Patterns to Follow

### Pattern 1: Atomic Coefficient Swap (RT-Safe Parameter Update)
**What:** Compute DSP parameters on UI thread, swap into RT-accessible struct atomically.
**When:** Any time DSP parameters change from a non-RT context.
**Example:**
```cpp
// UI thread: compute and swap
void EqualizerProcessor::setGains(const std::array<float, 10>& gainsDb) {
    auto* newCoeffs = new CoeffSet();
    for (int i = 0; i < kNumBands; i++) {
        newCoeffs->bands[i] = computePeakingEQ(
            kFrequencies[i], gainsDb[i], kDefaultQ, sampleRate_);
    }
    auto* old = pendingCoeffs_.exchange(newCoeffs, std::memory_order_acq_rel);
    // Schedule old for deletion on main thread (not RT thread)
    if (old) QTimer::singleShot(100, [old]() { delete old; });
}

// PW RT thread: check and apply
void EqualizerProcessor::process(int16_t* samples, uint32_t nFrames, int channels) {
    if (!enabled_.load(std::memory_order_relaxed)) return;
    auto* pending = pendingCoeffs_.exchange(nullptr, std::memory_order_acq_rel);
    if (pending) {
        activeCoeffs_.reset(pending);
        for (auto& ch : filters_)
            for (int b = 0; b < kNumBands; b++)
                ch[b].setCoeffs(activeCoeffs_->bands[b]);
    }
    // ... process samples through cascade ...
}
```

### Pattern 2: Service + Processor Split
**What:** Separate the QObject service (main thread, UI bindings, config) from the DSP processor (RT thread, stateless except filter memory).
**When:** Any audio processing that needs both UI control and RT execution.
**Why:** Keeps the RT path clean of Qt machinery. The service owns the user-facing API; the processor owns the math. They communicate only through atomic pointers.

### Pattern 3: IPC Handler Following Existing Convention
**What:** New IPC commands follow the exact `handleGetX` / `handleSetX` pattern.
**When:** Adding any new feature accessible from the web config panel.
**Example:** `handleGetEqConfig()` returns JSON with all EQ state. `handleSetEqBand()` takes stream + band + gain, forwards to EqualizerService, persists to config, returns `{"ok":true}`.

## Build Order (Suggested)

This ordering minimizes integration risk by building testable pieces first:

1. **BiquadFilter + coefficient math** -- Pure DSP, no Qt dependency, fully unit-testable. Write tests that verify frequency response of known filter configurations.

2. **EqualizerProcessor** -- Wraps biquad cascade with atomic coefficient swap. Unit-testable with synthetic S16LE buffers. Verify: passthrough when gains are all 0, correct boost/cut at center frequencies, no clicks on coefficient swap.

3. **Integration into onPlaybackProcess** -- Add `eqProcessor` to `AudioStreamHandle`, insert process call in callback. Minimal change (~15 lines). Testable on dev VM with PipeWire running.

4. **EqualizerService** -- QObject with config persistence, preset management, per-stream profiles. Wire into `IHostContext`. Depends on steps 1-3 for processor creation.

5. **Config schema + defaults** -- Add `audio.equalizer` section to `initDefaults()`. Must happen before or alongside step 4.

6. **IPC handlers** -- Wire `get_eq_config`, `set_eq_band`, `set_eq_preset`, etc. into IpcServer. Depends on step 4.

7. **QML EqualizerSettings UI** -- Vertical sliders, stream tabs, preset picker. Depends on step 4 for EqualizerService bindings.

8. **Web config panel** -- Flask route + HTML/JS equalizer UI with sliders. Depends on step 6 for IPC endpoints.

9. **Preset library** -- Curate and tune bundled presets (Rock, Pop, Jazz, etc.) with real audio testing on the Pi. Can be refined at any point after step 4.

Steps 1-3 are the core DSP pipeline. Steps 4-5 are the service layer. Steps 6-8 are the two UI surfaces. Step 9 is polish.

## Sources

- [PipeWire pw_filter API](https://docs.pipewire.org/group__pw__filter.html) -- filter node documentation (REJECTED approach)
- [PipeWire Tutorial 7: Audio DSP Filter](https://docs.pipewire.org/devel/page_tutorial7.html) -- pw_filter callback structure reference
- [PipeWire Filter-Chain Module](https://docs.pipewire.org/page_module_filter_chain.html) -- external EQ approach (REJECTED)
- [PipeWire Parametric Equalizer Module](https://docs.pipewire.org/page_module_parametric_equalizer.html) -- external EQ approach (REJECTED)
- [Audio EQ Cookbook (Robert Bristow-Johnson)](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html) -- biquad coefficient formulas, peakingEQ type
- [PipeWire audio-dsp-filter.c example](https://docs.pipewire.org/audio-dsp-filter_8c-example.html) -- reference for pw_filter usage
- Existing codebase: `AudioService.cpp` (process callback), `AudioService.hpp` (stream handle), `AudioRingBuffer.hpp`, `IpcServer.cpp` (IPC pattern), `ThemeService.hpp` (service pattern), `YamlConfig.hpp` (config pattern)
