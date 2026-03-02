# Phase 2: Service & Config - Context

**Gathered:** 2026-03-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Wire the EqualizerEngine (Phase 1) into the live audio pipeline, add preset management (8 bundled + user-created), per-stream EQ profiles (media/nav/phone), and YAML persistence. The service layer exposes Qt signals and Q_INVOKABLE methods so Phase 3 (QML UI) and future web config panel can both drive it.

</domain>

<decisions>
## Implementation Decisions

### Bundled Presets
- 8 presets as specified: Flat, Rock, Pop, Jazz, Classical, Bass Boost, Treble Boost, Vocal
- Mixed aggressiveness approach: bold curves for Bass Boost/Rock/Pop, subtle for Jazz/Classical, neutral for Flat
- A preset is exactly 10 float values (band gains in dB). No additional metadata (limiter threshold, bypass state, etc.)
- Bundled presets are immutable — users cannot modify them in place
- If user adjusts sliders while on a bundled preset, they must explicitly "Save As" to create a user preset

### Per-Stream Behavior
- One EqualizerEngine instance per audio stream (media, navigation, phone) — fits the PipeWire callback model where each stream has its own process callback
- Streams are always independent — no "link all streams" option
- Media stream defaults to Flat on first use
- Navigation and phone streams default to Voice preset (mid-range clarity: boost 1-4kHz, slight low cut to reduce road rumble competing with speech)
- "Voice" is a 9th bundled preset specifically tuned for speech intelligibility in car environments
- EQ kicks in immediately — EqualizerEngine's existing coefficient interpolation handles smooth transitions, no extra fade-in needed

### User Preset Workflow
- Save with auto-generated name ("Custom 1", "Custom 2") plus option to rename later — minimizes on-screen keyboard friction in car
- No hard limit on user preset count — config file stays small (10 floats + name per preset)
- Deleting a preset that's currently assigned to a stream: revert that stream to Flat
- No import/export — out of scope for this phase

### Config Structure
- EQ config lives under `audio.equalizer.*` (follows existing `audio.*` pattern)
- Per-stream storage: preset name reference (e.g., `audio.equalizer.streams.media.preset: "Rock"`) — human-readable, web-panel-friendly
- Fallback logic: if stored preset name doesn't exist (deleted/corrupted), revert to Flat
- Config write timing: Claude's discretion (immediate vs debounced based on existing YamlConfig patterns)

### Claude's Discretion
- Exact Voice preset curve values (within the "mid-range clarity" direction)
- Config write timing (immediate vs debounced)
- Signal surface design — full vs minimal Qt signals for state changes
- EqualizerService class design and API surface
- How to integrate EqualizerEngine into AudioService's PipeWire callback (in-place processing in onPlaybackProcess, or separate step)
- Auto-name generation logic (sequential numbering, gap-filling, etc.)

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `EqualizerEngine` (`src/core/audio/EqualizerEngine.hpp/.cpp`): RT-safe, processes int16_t in-place, `setAllGains()` for preset switch, `setGain()` for individual band, `setBypassed()` for bypass toggle. Already handles coefficient interpolation (2304 samples) and soft limiting.
- `AudioService` (`src/core/services/AudioService.hpp/.cpp`): PipeWire stream management with `onPlaybackProcess()` static callback. Ring buffer read → silence fill → rate matching. EQ processing would slot in after ring buffer read and before silence fill.
- `YamlConfig` (`src/core/YamlConfig.cpp`): Deep-merge YAML with defaults. Already has `audio.*` section with volume, device, buffer settings. EQ config extends this naturally.
- `IHostContext` / `HostContext`: Shared service access pattern. EqualizerService would be registered here for plugin access.

### Established Patterns
- Services are QObject-based with Q_PROPERTY for QML binding and Q_INVOKABLE for QML method calls
- IAudioService interface pattern — new IEqualizerService interface for testability
- PipeWire callbacks use raw pointers (`AudioStreamHandle*`) with static functions
- Config defaults defined in `YamlConfig::initDefaults()`

### Integration Points
- `AudioService::onPlaybackProcess()` — EQ processing inserts here, between ring buffer read and silence fill
- `AudioStreamHandle` — needs pointer to its assigned EqualizerEngine instance
- `YamlConfig::initDefaults()` — add `audio.equalizer.*` defaults
- `main.cpp` — register EqualizerService with HostContext
- `IHostContext` — expose EqualizerService to plugins

</code_context>

<specifics>
## Specific Ideas

- Voice preset should be tuned for car cabin: boost speech frequencies (1-4kHz), slight low cut to reduce road noise masking
- Auto-naming for user presets reduces touchscreen friction — entering text while driving is bad UX
- The 3-stream model (media/nav/phone) maps directly to the 3 AA audio channels the app already manages

</specifics>

<deferred>
## Deferred Ideas

- Import/export presets (sharing .json files) — future phase
- Web config panel EQ controls (WEB-01/02/03) — separate milestone requirement
- Per-channel EQ (left/right) — explicitly out of scope per REQUIREMENTS.md

</deferred>

---

*Phase: 02-service-config*
*Context gathered: 2026-03-02*
