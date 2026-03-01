# Phase 3: Equalizer - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Add a parametric equalizer to the audio pipeline so music sounds good through car speakers. Applies to music-type streams only (AA media, BT audio). Includes built-in presets, user-saveable presets, and a settings UI with vertical band sliders. Does not include per-source EQ profiles or advanced DSP (crossover, compressor, etc.).

</domain>

<decisions>
## Implementation Decisions

### EQ Scope & Streams
- EQ applies to AA media stream and BT audio stream (music sources only)
- Speech streams (AA speech, AA system, phone calls, nav prompts) are excluded from EQ processing
- EQ stays active whether music is coming from AA or standalone BT — same curve for both
- EQ settings persist across restarts via YAML config — once dialed in, it stays

### Band Design
- 5-band graphic EQ with fixed frequencies: 60Hz, 230Hz, 910Hz, 3.6kHz, 14kHz
- Fixed band frequencies — user adjusts gain (dB) only, no frequency or Q adjustment
- Gain range: +/- 12 dB per band

### Presets
- Ship with built-in presets: Flat, Rock, Pop, Bass Boost, Vocal, Car
- Users can save their own named presets
- Selecting a preset loads its gains into the sliders; tweaking a preset shows it as "Custom" or modified

### Settings UI
- EQ controls live inside AudioSettings.qml (keeps all audio controls together)
- Vertical sliders in a row — classic graphic EQ look, labeled with frequency
- dB values shown numerically next to each slider
- Preset selector is a dropdown/ComboBox at the top (matches existing device selection pattern)
- Bypass toggle to quickly A/B with and without EQ
- No EQ indicator outside settings — it just works in the background

### Clipping Protection
- Auto gain reduction when bands are boosted — prevents clipping without user intervention

### Real-time Behavior
- Filter coefficients update live as sliders move — hear changes in real time
- No apply button or deferred update

### Claude's Discretion
- Biquad filter implementation details (direct form, transposed, etc.)
- Coefficient calculation approach
- How to intercept the audio pipeline (ring buffer tap, PipeWire filter node, or inline in process callback)
- Built-in preset gain curves (exact dB values for Rock, Pop, etc.)
- "Modified preset" indicator behavior
- Save preset dialog design

</decisions>

<specifics>
## Specific Ideas

- Classic car stereo EQ aesthetic — vertical bars, familiar to anyone who's used an aftermarket head unit
- 1024x600 touchscreen with fat-finger use while driving — keep touch targets big
- Should feel like a built-in feature, not a bolted-on addon

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `AudioService` (PipeWire): creates streams, manages ring buffers, has `onPlaybackProcess()` callback where samples are pulled — natural EQ insertion point
- `AudioStreamHandle`: per-stream state with sample rate, channels, ring buffer — EQ state could live here
- `YamlConfig`: deep-merge YAML config already handles nested structures — EQ bands/presets fit naturally
- `AudioSettings.qml`: existing settings page for audio (volume, device selection) — extend with EQ section
- `IAudioService` interface: defines stream API — may need EQ-related additions

### Established Patterns
- PipeWire threading: `onPlaybackProcess()` runs on PW RT thread, must be lock-free. EQ processing must happen here with pre-computed coefficients
- Config persistence: `ConfigService` with `setValueByPath()` / `valueByPath()` — same pattern as wallpaper/theme persistence
- QML settings: Vertical slider controls exist (brightness), ComboBox for selection (devices) — reuse patterns
- Q_PROPERTY + Q_INVOKABLE: Standard pattern for exposing C++ state to QML

### Integration Points
- `AudioService::onPlaybackProcess()` — where samples flow from ring buffer to PipeWire output. EQ filter goes here
- `AudioSettings.qml` — add EQ section below existing audio controls
- `YamlConfig` defaults (`initDefaults()`) — must register EQ config keys (learned from wallpaper: config defaults gate)
- `HostContext` — if EQ service is separate from AudioService, needs registration here

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-equalizer*
*Context gathered: 2026-03-01*
