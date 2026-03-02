# Phase 1: DSP Core - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

A correct, RT-safe 10-band biquad equalizer engine with coefficient interpolation, bypass, and unit tests. Pure DSP math library — no UI, no config persistence, no service layer. Self-contained and unit-testable with no external dependencies beyond the audio sample format.

</domain>

<decisions>
## Implementation Decisions

### Gain range per band
- ±12dB range (standard car audio)
- 1dB step increments (25 positions per band: -12 to +12)
- Soft output limiter to prevent clipping when multiple bands are boosted — engine auto-attenuates, user never hears digital crackle
- Default all bands to 0dB (flat). The "Flat" preset is literally flat.

### Transition feel
- ~50ms crossfade for coefficient interpolation — fast enough to feel instant, slow enough to avoid clicks
- Real-time audio updates while dragging sliders (engine handles smooth interpolation between rapid gain changes)
- Same transition speed for both preset switches and single-band adjustments — consistent behavior everywhere
- Direct crossfade only — no volume dip during transitions. Audio never mutes or ducks.

### Bypass behavior
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

</decisions>

<specifics>
## Specific Ideas

- Engine should feel like good car audio gear — transitions are seamless, never jarring
- The 10 ISO center frequencies are: 31Hz, 62Hz, 125Hz, 250Hz, 500Hz, 1kHz, 2kHz, 4kHz, 8kHz, 16kHz
- Current audio pipeline uses int16_t samples at 48kHz stereo, bridged through a lock-free SPSC ring buffer (AudioRingBuffer) from ASIO threads to PipeWire RT callback

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `AudioRingBuffer` (src/core/audio/AudioRingBuffer.hpp): Lock-free SPSC ring buffer using spa_ringbuffer. EQ processing will likely sit between ring buffer read and PipeWire output.
- `AudioStreamHandle` (src/core/services/AudioService.hpp): Per-stream state including sampleRate, channels, bytesPerFrame. EQ engine instances can be attached here.

### Established Patterns
- **Lock-free RT path**: PipeWire process callback is RT — no allocations, no mutexes. Atomic coefficient swap fits this pattern.
- **int16_t sample format**: All audio is 16-bit signed integer, stereo interleaved, 48kHz. Engine must accept this format (internal precision is Claude's discretion).
- **Per-stream architecture**: Three separate audio streams (media, nav, phone) each with their own AudioStreamHandle. Engine needs to support multiple independent instances.

### Integration Points
- `AudioService::onPlaybackProcess()` — PipeWire RT callback where audio flows from ring buffer to output. EQ processing inserts here.
- `AudioStreamHandle` — likely gains an `EqualizerEngine*` member for per-stream EQ instance (Phase 2 wires this up).
- No existing DSP code — this is greenfield.

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 01-dsp-core*
*Context gathered: 2026-03-01*
