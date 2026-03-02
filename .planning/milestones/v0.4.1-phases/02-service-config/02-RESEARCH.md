# Phase 2: Service & Config - Research

**Researched:** 2026-03-02
**Domain:** Qt/C++ service layer, YAML config persistence, PipeWire audio pipeline integration
**Confidence:** HIGH

## Summary

Phase 2 wires the Phase 1 `EqualizerEngine` into the live audio pipeline and wraps it with a Qt service layer for preset management, per-stream EQ profiles, and YAML persistence. The codebase already has well-established patterns for all of this: QObject-based services with `Q_PROPERTY`/`Q_INVOKABLE`, interface-driven architecture (`IConfigService`, `IAudioService`), YAML config with deep-merge defaults, and PipeWire process callbacks where EQ processing slots in naturally.

The integration point is clear: `AudioService::onPlaybackProcess()` reads from the ring buffer, then silence-fills. EQ processing goes between those two steps. Each `AudioStreamHandle` needs a pointer to its assigned `EqualizerEngine` instance. The three AA streams (media/speech/system) have different sample rates and channel counts, all supported by `EqualizerEngine`'s constructor parameters.

**Primary recommendation:** Create an `EqualizerService` (QObject) that owns 3 `EqualizerEngine` instances, manages presets as simple `std::array<float, 10>` named entries, persists everything to YAML under `audio.equalizer.*`, and exposes Q_INVOKABLE/Q_PROPERTY for the Phase 3 QML UI and future web panel. Keep the RT-path integration minimal -- just add an `EqualizerEngine*` pointer to `AudioStreamHandle` and one `engine->process()` call in the callback.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions
- 8 bundled presets: Flat, Rock, Pop, Jazz, Classical, Bass Boost, Treble Boost, Vocal
- Mixed aggressiveness: bold curves for Bass Boost/Rock/Pop, subtle for Jazz/Classical, neutral for Flat
- A preset is exactly 10 float values (band gains in dB). No additional metadata
- Bundled presets are immutable -- users cannot modify them in place
- If user adjusts sliders while on a bundled preset, they must explicitly "Save As"
- One EqualizerEngine instance per audio stream (media, navigation, phone)
- Streams are always independent -- no "link all streams" option
- Media stream defaults to Flat on first use
- Navigation and phone streams default to Voice preset
- "Voice" is a 9th bundled preset specifically tuned for speech intelligibility in car environments
- EQ kicks in immediately -- EqualizerEngine's existing coefficient interpolation handles smooth transitions
- Save with auto-generated name ("Custom 1", "Custom 2") plus option to rename later
- No hard limit on user preset count
- Deleting a preset assigned to a stream: revert that stream to Flat
- No import/export -- out of scope
- EQ config lives under `audio.equalizer.*`
- Per-stream storage: preset name reference (e.g., `audio.equalizer.streams.media.preset: "Rock"`)
- Fallback logic: if stored preset name doesn't exist, revert to Flat

### Claude's Discretion
- Exact Voice preset curve values (within "mid-range clarity" direction)
- Config write timing (immediate vs debounced)
- Signal surface design -- full vs minimal Qt signals for state changes
- EqualizerService class design and API surface
- How to integrate EqualizerEngine into AudioService's PipeWire callback
- Auto-name generation logic (sequential numbering, gap-filling, etc.)

### Deferred Ideas (OUT OF SCOPE)
- Import/export presets (sharing .json files)
- Web config panel EQ controls (WEB-01/02/03)
- Per-channel EQ (left/right)

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PRST-01 | User can select from 8+1 bundled presets | Bundled presets as static const data, EqualizerService applies via `setAllGains()` |
| PRST-02 | User can apply different EQ profiles per stream independently | 3 EqualizerEngine instances, per-stream preset tracking in EqualizerService |
| PRST-03 | User can save custom EQ settings as a named preset | User preset storage in YAML `audio.equalizer.user_presets`, auto-naming |
| PRST-04 | User can load and delete user-created presets | Load applies gains to active stream's engine, delete with Flat fallback |
| CFG-01 | EQ settings persist across app restarts via YAML config | Extend YamlConfig with `audio.equalizer.*` defaults, load/save in EqualizerService |

</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6 (Core/Gui) | 6.4+ | QObject, Q_PROPERTY, Q_INVOKABLE, signals/slots | Already the project's UI/service framework |
| yaml-cpp | system | YAML config persistence | Already used by YamlConfig |
| PipeWire | 1.0+ | Audio pipeline (process callbacks) | Already used by AudioService |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| EqualizerEngine (Phase 1) | in-tree | RT-safe DSP processing | Each audio stream gets one instance |
| BiquadFilter (Phase 1) | in-tree | Coefficient calculation | Used by EqualizerEngine internally |

No new dependencies needed. Everything builds on existing project libraries.

## Architecture Patterns

### Recommended Project Structure
```
src/core/audio/
    EqualizerEngine.hpp/.cpp      # [EXISTS] RT-safe DSP (Phase 1)
    EqualizerPresets.hpp           # [NEW] Static bundled preset data
    BiquadFilter.hpp              # [EXISTS] Biquad coefficients
    SoftLimiter.hpp/.cpp          # [EXISTS] Limiter

src/core/services/
    IEqualizerService.hpp         # [NEW] Interface for testability
    EqualizerService.hpp/.cpp     # [NEW] QObject service layer

tests/
    test_equalizer_service.cpp    # [NEW] Service layer tests
    test_equalizer_presets.cpp    # [NEW] Preset data validation tests
```

### Pattern 1: Service Interface + Concrete Implementation

**What:** Follow the existing `IAudioService`/`AudioService` pattern. Create `IEqualizerService` interface and `EqualizerService` concrete class.
**When to use:** Always -- this is the project's established pattern for testable services.
**Example:**
```cpp
// IEqualizerService.hpp -- pure virtual interface
class IEqualizerService {
public:
    virtual ~IEqualizerService() = default;

    // Stream identifiers matching AA audio channels
    enum class StreamId { Media, Navigation, Phone };

    virtual QString activePreset(StreamId stream) const = 0;
    virtual void applyPreset(StreamId stream, const QString& presetName) = 0;
    virtual void setGain(StreamId stream, int band, float dB) = 0;
    virtual float gain(StreamId stream, int band) const = 0;
    virtual void setBypassed(StreamId stream, bool bypassed) = 0;

    virtual QStringList bundledPresetNames() const = 0;
    virtual QStringList userPresetNames() const = 0;
    virtual QString saveUserPreset(const QString& name = {}) = 0;
    virtual bool deleteUserPreset(const QString& name) = 0;
    virtual bool renameUserPreset(const QString& oldName, const QString& newName) = 0;
};
```

### Pattern 2: EqualizerEngine Pointer in AudioStreamHandle

**What:** Add an `EqualizerEngine*` field to `AudioStreamHandle` (the PipeWire stream wrapper). The RT callback calls `engine->process()` on the buffer data after ring buffer read, before silence fill.
**When to use:** This is the only RT-safe way to inject processing into the PipeWire callback.
**Example:**
```cpp
// In AudioStreamHandle (AudioService.hpp)
struct AudioStreamHandle {
    // ... existing fields ...
    EqualizerEngine* eqEngine = nullptr;  // non-owning, set by EqualizerService
};

// In AudioService::onPlaybackProcess() -- 2 lines added
uint32_t bytesRead = handle->ringBuffer->read(...);

// EQ processing (RT-safe, in-place, before silence fill)
if (handle->eqEngine && bytesRead > 0) {
    int frames = bytesRead / handle->bytesPerFrame;
    handle->eqEngine->process(
        reinterpret_cast<int16_t*>(static_cast<uint8_t*>(d.data)),
        frames);
}

// Silence-fill any gap (existing code)
if (bytesRead < wantBytes)
    std::memset(...);
```

### Pattern 3: Bundled Presets as Static Const Data

**What:** Define all 9 bundled presets (including Voice) as `static const` data in a header. No runtime loading, no file parsing.
**When to use:** Bundled presets are immutable and known at compile time.
**Example:**
```cpp
// EqualizerPresets.hpp
struct EqPreset {
    const char* name;
    std::array<float, kNumBands> gains; // dB per band
};

// ISO bands: 31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz
static constexpr EqPreset kBundledPresets[] = {
    {"Flat",         { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"Rock",         { 4, 3, -1, -2, 0, 2, 4, 5, 4, 3}},
    {"Pop",          {-1, 1, 3, 4, 3, 0, -1, -1, 1, 2}},
    {"Jazz",         { 2, 1, 0, 1, -1, -1, 0, 1, 2, 2}},
    {"Classical",    { 0, 0, 0, 0, 0, 0, -1, -2, -1, 1}},
    {"Bass Boost",   { 6, 5, 4, 2, 0, 0, 0, 0, 0, 0}},
    {"Treble Boost", { 0, 0, 0, 0, 0, 1, 2, 4, 5, 6}},
    {"Vocal",        {-2, -1, 0, 1, 3, 3, 2, 1, 0, -1}},
    {"Voice",        {-3, -2, 0, 1, 2, 3, 4, 2, 0, -1}},
};
```

### Pattern 4: YAML Config Extension

**What:** Extend `YamlConfig::initDefaults()` with `audio.equalizer.*` section. EQ config uses the existing dot-path access (`valueByPath`/`setValueByPath`) but also needs sequence/map support for user presets, which the current `setValueByPath` does NOT support (rejects non-scalar paths).
**When to use:** Always -- config persistence is a requirement.

**Critical design note:** The existing `setValueByPath()` validates against the defaults tree and rejects writes to non-existent or non-scalar paths. User presets are dynamic (variable count, user-defined names), so they CANNOT use the generic dot-path API. Options:
1. **Add dedicated EQ config methods** to `YamlConfig` (like `launcherTiles()` -- custom serialization for complex types)
2. Store EQ config directly on `root_["audio"]["equalizer"]` via dedicated methods

Option 1 matches the existing pattern (launcherTiles, enabledPlugins all have dedicated methods). Use this.

```yaml
# Config structure in YAML
audio:
  equalizer:
    streams:
      media:
        preset: "Rock"
      navigation:
        preset: "Voice"
      phone:
        preset: "Voice"
    user_presets:
      - name: "My Custom"
        gains: [2, 1, 0, -1, 0, 1, 3, 2, 1, 0]
      - name: "Night Drive"
        gains: [-1, 0, 1, 2, 3, 3, 2, 1, -1, -2]
```

### Anti-Patterns to Avoid
- **Allocating EqualizerEngine on the RT thread:** Engines must be pre-created and assigned to stream handles before PipeWire starts processing.
- **Locking a mutex in onPlaybackProcess:** The EQ call must be lock-free. `EqualizerEngine::process()` is already RT-safe (uses atomic double-buffer swap).
- **Storing EQ state in two places:** The EqualizerService is the single source of truth. Don't duplicate state in AudioService or the config.
- **Using `setValueByPath` for user presets:** The generic API rejects non-scalar writes. Use dedicated YamlConfig methods.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| RT-safe coefficient updates | Custom lock-free queue | EqualizerEngine's atomic double-buffer | Already built, tested, handles interpolation |
| Audio processing pipeline | Manual buffer management | PipeWire callback + `process()` | One-liner integration into existing callback |
| YAML serialization | Custom file format | yaml-cpp via YamlConfig | Project standard, deep-merge with defaults |
| Coefficient calculation | DSP math from scratch | `calcPeakingEQ()` in BiquadFilter.hpp | Phase 1 already has this |

**Key insight:** Phase 1 did the hard DSP work. Phase 2 is pure plumbing -- wrapping existing functionality in a service layer and persisting state. No new DSP code needed.

## Common Pitfalls

### Pitfall 1: System Stream Sample Rate
**What goes wrong:** The AA System stream is 16000 Hz, 1 channel. At 16kHz, the Nyquist frequency is 8kHz. The top two EQ bands (8kHz and 16kHz) are at or above Nyquist and will produce unstable/unusable filter coefficients.
**Why it happens:** `calcPeakingEQ()` doesn't guard against center frequencies at/above Nyquist.
**How to avoid:** Create the System stream's EqualizerEngine with `sampleRate=16000`. The `calcPeakingEQ` function will compute coefficients for 8kHz and 16kHz bands but they'll be numerically degenerate (Q approaches infinity near Nyquist). However, since the system stream is for notification sounds and the Voice preset doesn't boost those bands anyway, this is low-risk. The gains for those bands should remain at 0 dB. Alternatively, the EqualizerEngine could skip bands whose center frequency >= sampleRate/2.
**Warning signs:** Squealing or artifacts on the system audio stream.

### Pitfall 2: Engine Lifetime vs Stream Lifetime
**What goes wrong:** AA streams are created/destroyed per session (connect/disconnect). EqualizerEngine instances must outlive the streams they're assigned to.
**Why it happens:** If engines are destroyed while PipeWire is still calling `process()`, it's a use-after-free.
**How to avoid:** EqualizerService owns the 3 engines for the app's lifetime. When AA creates streams, EqualizerService assigns engine pointers to the handles. When AA destroys streams, the pointers become stale but engines live on.
**Warning signs:** Crash on AA disconnect.

### Pitfall 3: Config Write During Driving
**What goes wrong:** Frequent config saves (on every slider move) thrash the SD card and risk corruption on power loss.
**Why it happens:** Eager save-on-change without debouncing.
**How to avoid:** Debounce config saves with a QTimer (e.g., 2-3 second delay). The existing `ConfigService::save()` writes the full YAML file synchronously, so batching changes is important. Also save on app shutdown.
**Warning signs:** SD card wear, config corruption after power loss.

### Pitfall 4: Stream Name Mapping
**What goes wrong:** The AA streams are named "AA Media", "AA Speech", "AA System" but the EQ service uses StreamId::Media/Navigation/Phone. The mapping between "Speech" and "Navigation" (and "System" and "Phone") must be consistent.
**Why it happens:** AA protocol uses "Speech" for navigation prompts and "System" for phone ringtones/alerts, but from the user's perspective they're "Navigation" and "Phone".
**How to avoid:** Document the mapping clearly:
- `StreamId::Media` -> "AA Media" (48kHz, stereo)
- `StreamId::Navigation` -> "AA Speech" (48kHz, mono)
- `StreamId::Phone` -> "AA System" (16kHz, mono)

### Pitfall 5: Preset Name Collision
**What goes wrong:** User creates a preset named "Rock" or "Flat" -- same as a bundled preset.
**Why it happens:** No validation on user preset names.
**How to avoid:** Check against bundled preset names when saving. Either reject the name or prefix user presets internally.

## Code Examples

### EQ Integration in PipeWire Callback
```cpp
// AudioService::onPlaybackProcess() -- modified
void AudioService::onPlaybackProcess(void* userdata)
{
    auto* handle = static_cast<AudioStreamHandle*>(userdata);
    if (!handle || !handle->stream || !handle->ringBuffer)
        return;

    struct pw_buffer* buf = pw_stream_dequeue_buffer(handle->stream);
    if (!buf) return;

    struct spa_data& d = buf->buffer->datas[0];
    int stride = handle->bytesPerFrame;
    uint32_t n_frames = d.maxsize / stride;
    if (buf->requested > 0 && buf->requested < n_frames)
        n_frames = buf->requested;
    uint32_t wantBytes = n_frames * stride;

    uint32_t bytesRead = handle->ringBuffer->read(
        static_cast<uint8_t*>(d.data), wantBytes);

    // --- EQ processing (RT-safe, in-place) ---
    if (handle->eqEngine && bytesRead > 0) {
        int frames = static_cast<int>(bytesRead / stride);
        handle->eqEngine->process(
            reinterpret_cast<int16_t*>(d.data), frames);
    }

    if (bytesRead == 0)
        handle->underrunCount.fetch_add(1, std::memory_order_relaxed);

    if (bytesRead < wantBytes)
        std::memset(static_cast<uint8_t*>(d.data) + bytesRead,
                    0, wantBytes - bytesRead);

    // ... rest of callback unchanged ...
}
```

### EqualizerService Skeleton
```cpp
class EqualizerService : public QObject, public IEqualizerService {
    Q_OBJECT
    Q_PROPERTY(QString mediaPreset READ mediaPreset NOTIFY mediaPresetChanged)
    Q_PROPERTY(QString navigationPreset READ navigationPreset NOTIFY navigationPresetChanged)
    Q_PROPERTY(QString phonePreset READ phonePreset NOTIFY phonePresetChanged)

public:
    explicit EqualizerService(YamlConfig* config, QObject* parent = nullptr);

    // Engine access -- for AudioStreamHandle assignment
    EqualizerEngine* engineForStream(StreamId stream);

    // IEqualizerService implementation
    Q_INVOKABLE QString activePreset(StreamId stream) const override;
    Q_INVOKABLE void applyPreset(StreamId stream, const QString& name) override;
    Q_INVOKABLE void setGain(StreamId stream, int band, float dB) override;
    Q_INVOKABLE float gain(StreamId stream, int band) const override;
    Q_INVOKABLE void setBypassed(StreamId stream, bool bypassed) override;

    Q_INVOKABLE QStringList bundledPresetNames() const override;
    Q_INVOKABLE QStringList userPresetNames() const override;
    Q_INVOKABLE QString saveUserPreset(const QString& name = {}) override;
    Q_INVOKABLE bool deleteUserPreset(const QString& name) override;
    Q_INVOKABLE bool renameUserPreset(const QString& old, const QString& newN) override;

signals:
    void mediaPresetChanged();
    void navigationPresetChanged();
    void phonePresetChanged();
    void presetListChanged();  // user presets added/removed/renamed
    void gainsChanged(int streamId);  // individual band changed

private:
    struct StreamState {
        EqualizerEngine engine;
        QString activePreset;
        StreamState(float sr, int ch) : engine(sr, ch) {}
    };

    StreamState media_{48000.0f, 2};
    StreamState navigation_{48000.0f, 1};
    StreamState phone_{16000.0f, 1};

    struct UserPreset {
        QString name;
        std::array<float, kNumBands> gains;
    };
    QList<UserPreset> userPresets_;

    YamlConfig* config_;
    QTimer saveTimer_;  // debounced config persistence

    void loadFromConfig();
    void scheduleSave();
    StreamState& streamState(StreamId id);
    QString generateAutoName() const;
};
```

### Config Persistence Methods for YamlConfig
```cpp
// New methods in YamlConfig
struct EqStreamConfig {
    QString preset;
};

struct EqUserPreset {
    QString name;
    std::array<float, 10> gains;
};

// Getters
EqStreamConfig eqStreamConfig(const QString& streamName) const;
QList<EqUserPreset> eqUserPresets() const;

// Setters
void setEqStreamPreset(const QString& streamName, const QString& presetName);
void setEqUserPresets(const QList<EqUserPreset>& presets);
```

### Voice Preset Design
```cpp
// Voice preset: optimized for speech intelligibility in car cabin
// - Boost 1-4kHz (speech formants, consonant clarity)
// - Slight cut below 125Hz (reduce road rumble masking)
// - Gentle rolloff above 8kHz (reduce sibilance, tire noise)
//
// ISO bands: 31   62   125  250  500  1k   2k   4k   8k   16k
{"Voice",   {-3,  -2,  -1,   0,   1,   3,   4,   3,   0,  -1}},
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| EQ as PipeWire filter-chain | In-app DSP processing | Project decision | Lower latency, full control, no PipeWire API dependency for params |
| Plugin-owned audio processing | Service-layer EQ with engine pointers | Phase 2 design | Clean separation, engines outlive streams |

## Open Questions

1. **System stream EQ at 16kHz**
   - What we know: The EqualizerEngine will compute coefficients for all 10 bands regardless of sample rate. Bands 9 (8kHz) and 10 (16kHz) are at/above Nyquist for a 16kHz stream.
   - What's unclear: Whether `calcPeakingEQ()` produces NaN or just degenerate coefficients at Nyquist. The Voice preset doesn't boost those bands, so it may be moot.
   - Recommendation: Test during implementation. If problematic, clamp band gains to 0 for bands >= sampleRate/2 in the service layer. Low risk since phone audio is narrowband speech.

2. **EqualizerService registration in HostContext**
   - What we know: HostContext has setters for each service. Adding equalizerService means modifying IHostContext, HostContext, and all test mocks.
   - What's unclear: Whether this is the right place, or whether EqualizerService should be accessed through AudioService.
   - Recommendation: Add to IHostContext/HostContext following the existing pattern. It's a first-class service that the QML UI needs direct access to. Having it on AudioService would create unnecessary coupling.

3. **Stream handle assignment timing**
   - What we know: AA streams are created in `AndroidAutoOrchestrator::onSessionStarted()`. EqualizerService engines exist at app startup.
   - What's unclear: Who assigns the engine pointer to the stream handle -- the orchestrator, AudioService, or EqualizerService?
   - Recommendation: EqualizerService provides `engineForStream(StreamId)`. The orchestrator (or AudioService via a hook) assigns it after `createStream()`. Simplest approach: orchestrator calls `eqService->engineForStream()` and sets `handle->eqEngine` directly.

## Sources

### Primary (HIGH confidence)
- `src/core/audio/EqualizerEngine.hpp/.cpp` -- Phase 1 implementation, verified API surface
- `src/core/services/AudioService.hpp/.cpp` -- PipeWire callback structure, stream handle layout
- `src/core/YamlConfig.hpp/.cpp` -- Config patterns, dot-path API limitations
- `src/core/services/ConfigService.hpp/.cpp` -- Service config pattern
- `src/core/plugin/IHostContext.hpp` -- Service registration pattern
- `src/core/aa/AndroidAutoOrchestrator.cpp` -- Stream creation (names, sample rates, channels)

### Secondary (MEDIUM confidence)
- Bundled preset gain values -- based on standard audio engineering EQ curves for car environments. May need tuning by ear on actual hardware.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies, everything is in-tree
- Architecture: HIGH -- follows established project patterns exactly
- Integration: HIGH -- onPlaybackProcess callback point is clear, engine API is known
- Preset curves: MEDIUM -- gain values are reasonable estimates but subjective; may need tuning
- Pitfalls: HIGH -- identified from direct code inspection

**Research date:** 2026-03-02
**Valid until:** Indefinite (all findings are from in-tree code, no external dependencies)
