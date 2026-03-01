# Feature Research: Audio Equalizer

**Domain:** 10-band graphic EQ for car head unit (PipeWire-based, touchscreen + web UI)
**Researched:** 2026-03-01
**Confidence:** HIGH

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist in any audio EQ implementation. Missing any of these and it feels half-baked.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| 10-band graphic EQ sliders | Standard car audio EQ count. OAP had 15 bands (overkill for 1024x600). 10 is the sweet spot -- enough control, fits the screen. | MEDIUM | Standard ISO frequencies: 31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16kHz. Each band +/-12dB range. |
| Bundled genre presets | Every head unit ships with presets. Users expect one-tap sound profiles. | LOW | Rock, Pop, Jazz, Classical, Bass Boost, Treble Boost, Vocal, Flat. 8 presets is standard. |
| Flat/bypass toggle | Users need a quick way to hear the difference. "Is EQ actually doing anything?" | LOW | Single button that zeros all bands or bypasses processing entirely. Bypass is better (avoids "now I have to re-enter my settings"). |
| Real-time preview | Adjusting a slider must change the sound immediately, not on save. | MEDIUM | Biquad coefficient updates must propagate to DSP path within one audio quantum (~5ms). No "apply" button. |
| Preset selection dropdown/picker | Quick access to presets without entering a full settings screen. | LOW | Reuse existing `FullScreenPicker` pattern from AudioSettings.qml. |
| Persist EQ settings across restarts | Settings must survive reboot. Car users turn the car off and expect the same sound next time. | LOW | Already have YAML config persistence. Store band gains + active preset name under `audio.equalizer.*`. |
| Visual frequency response curve | Users expect to see the EQ shape, not just isolated sliders. A curve connecting slider tops shows overall tonal shape at a glance. | LOW | Simple polyline/spline in QML connecting slider handle positions. No FFT needed -- purely visual. |

### Differentiators (Competitive Advantage)

Features that go beyond what basic car head units offer.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Per-stream EQ profiles | Apply different EQ to media vs navigation vs phone. Boost voice clarity on nav/phone while keeping music EQ separate. No consumer head unit does this -- they EQ the master output only. | MEDIUM | Three independent biquad chains (one per AA audio stream). Config stores `audio.equalizer.profiles.media`, `.navigation`, `.phone`. Each profile references a preset name or custom band values. |
| User-created presets | Save custom EQ curves with a name. Power users tune for their specific car/speakers. | LOW | Save to `audio.equalizer.user_presets.[name]` in YAML. Need a save dialog (name input + confirm). |
| Web config panel EQ | Adjust EQ from phone browser while sitting in the car. Better precision than fat-finger touchscreen sliders. Existing web config pattern makes this straightforward. | MEDIUM | Flask route + HTML/JS EQ UI. IPC commands: `eq.get_state`, `eq.set_band`, `eq.set_preset`, `eq.save_preset`. Same underlying EQ service, different UI. |
| Per-stream preset assignment in web panel | Web UI shows all three streams with their assigned presets. Easier to configure than navigating nested touchscreen menus. | LOW | Builds on web config EQ. Three dropdown selectors + band visualization per stream. |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Parametric EQ (adjustable Q/frequency per band) | "More control" -- audiophile users want to tune individual band width and center frequency. | Massively increases UI complexity. 1024x600 cannot fit Q knobs + frequency selectors + gain sliders for 10 bands. Makes presets non-portable (different Q values break preset sharing). | Stick with graphic EQ (fixed frequencies, fixed Q). Users who need parametric can use PipeWire's built-in parametric-equalizer module externally. |
| Real-time spectrum analyzer / FFT display | "Show me what the music looks like." Looks cool, adds perceived value. | Continuous FFT on RT audio thread costs CPU budget on Pi 4 that should go to video decode. Visual distraction while driving. | Static frequency response curve derived from slider positions. If someone wants a visualizer, that's a separate plugin. |
| Auto-EQ / room correction | "Measure my car and auto-tune." REW + measurement mic workflow. | Requires calibrated mic, test signal playback, complex DSP. Way beyond scope. | Provide good presets and easy manual tuning. Link to external tools (REW, AutoEQ) in docs. |
| Separate left/right channel EQ | "My car's acoustics favor one side." | Doubles controls (20 sliders). UI nightmare on a small touchscreen. | Single EQ applied to both channels. PipeWire filter-chain can do per-channel externally. |
| EQ applied to BT audio plugin | "I want EQ on Bluetooth music too." | Scope creep. BT audio is a separate PipeWire A2DP sink stream. Inserting EQ there means intercepting PipeWire's A2DP sink output (fragile) or running system-wide EQ (conflicts with per-stream). | v0.4.1 scope is AA streams only. System-wide EQ is a future consideration using PipeWire filter-chain at the sink level. |

## Standard Preset Definitions

Based on car audio conventions and genre characteristics. All values in dB, range +/-12.

| Preset | 31Hz | 62Hz | 125Hz | 250Hz | 500Hz | 1kHz | 2kHz | 4kHz | 8kHz | 16kHz | Character |
|--------|------|------|-------|-------|-------|------|------|------|------|-------|-----------|
| Flat | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | No coloration |
| Rock | +4 | +3 | +1 | 0 | -1 | +1 | +3 | +4 | +3 | +2 | Scooped mids, strong guitar/drums |
| Pop | -1 | +1 | +3 | +4 | +3 | +1 | -1 | -1 | +1 | +2 | Forward mids, clear vocals |
| Jazz | +3 | +2 | +1 | 0 | -1 | -1 | 0 | +1 | +3 | +4 | Warm bass, crisp highs |
| Classical | 0 | 0 | 0 | 0 | 0 | 0 | -1 | -1 | +1 | +2 | Subtle high lift, natural |
| Bass Boost | +6 | +5 | +4 | +2 | 0 | 0 | 0 | 0 | 0 | 0 | Heavy low end |
| Treble Boost | 0 | 0 | 0 | 0 | 0 | +1 | +2 | +4 | +5 | +6 | Bright, airy top end |
| Vocal | -2 | -1 | 0 | +2 | +4 | +4 | +3 | +1 | 0 | -1 | Midrange emphasis for speech clarity |

## Feature Dependencies

```
[Biquad DSP engine]
    └──requires──> [AudioService stream access]  (EXISTING)
                       └──requires──> [PipeWire thread loop]  (EXISTING)

[Per-stream EQ profiles]
    └──requires──> [Biquad DSP engine]
    └──requires──> [Profile config schema]
                       └──requires──> [YAML config]  (EXISTING)

[Bundled presets]
    └──requires──> [Biquad DSP engine]

[User-created presets]
    └──requires──> [Bundled presets]  (same data model)
    └──requires──> [Save dialog UI]

[Head unit touch UI]
    └──requires──> [Biquad DSP engine]
    └──requires──> [EQ service (QML-exposed)]

[Web config EQ]
    └──requires──> [EQ service]
    └──requires──> [IPC commands for EQ]
                       └──requires──> [IpcServer]  (EXISTING)

[Visual frequency curve]
    └──enhances──> [Head unit touch UI]
    └──enhances──> [Web config EQ]
```

### Dependency Notes

- **Biquad DSP engine requires AudioService stream access:** EQ processing happens in `onPlaybackProcess` callback where we already access the audio buffer. Biquad filters process samples inline between ring buffer read and PipeWire buffer write -- no separate PipeWire filter node needed.
- **Per-stream EQ profiles require profile config schema:** Each of the three AA streams (media, navigation, phone) needs its own set of 10 band gains in YAML. Schema supports both preset references (`preset: "Rock"`) and custom values (`bands: [4, 3, 1, ...]`).
- **User presets require bundled presets:** Same data model (name + 10 band values). User presets stored alongside bundled but flagged as user-created (bundled are read-only).
- **Web config EQ requires IPC commands:** New IPC command set for Flask panel to read/write EQ state. Follows existing `get_config`/`set_config` pattern but with EQ-specific commands for real-time band updates.

## MVP Definition

### Launch With (v0.4.1)

Minimum viable EQ that's actually useful in the car.

- [ ] **10-band biquad DSP in playback callback** -- the core processing engine
- [ ] **Bundled presets (8)** -- one-tap sound profiles (most users pick a preset and never touch individual bands)
- [ ] **Per-stream EQ profiles** -- the differentiator. Even if the UI only lets you pick presets per stream, it's valuable.
- [ ] **Head unit touch UI with sliders** -- the primary interface. Vertical sliders, preset picker, bypass toggle.
- [ ] **Visual frequency curve** -- connects slider tops with a smooth line. Trivial to implement, big UX win.
- [ ] **YAML config persistence** -- survives reboot. Non-negotiable.

### Add After Validation (v0.4.1 polish)

- [ ] **User-created presets** -- needs name input dialog. Low complexity but not essential for initial validation.
- [ ] **Web config panel EQ** -- duplicate UI in Flask/HTML/JS. Worth doing but only after head unit UI is validated.
- [ ] **Per-stream preset assignment in web panel** -- nice for configuration, not needed for driving.

### Future Consideration (v2+)

- [ ] **System-wide EQ (BT audio, local media)** -- requires PipeWire sink-level filter-chain, different architecture
- [ ] **EQ preset import/export** -- share presets between installations (YAML copy is manual but works)
- [ ] **Spectrum analyzer plugin** -- separate from EQ, visual-only, opt-in CPU cost

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| 10-band biquad DSP engine | HIGH | MEDIUM | P1 |
| Bundled presets (8) | HIGH | LOW | P1 |
| Head unit touch UI (sliders + curve) | HIGH | MEDIUM | P1 |
| Per-stream EQ profiles | HIGH | MEDIUM | P1 |
| Bypass toggle | HIGH | LOW | P1 |
| YAML config persistence | HIGH | LOW | P1 |
| Preset picker | MEDIUM | LOW | P1 |
| Visual frequency response curve | MEDIUM | LOW | P1 |
| User-created presets | MEDIUM | LOW | P2 |
| Web config panel EQ | MEDIUM | MEDIUM | P2 |
| Per-stream web panel assignment | LOW | LOW | P2 |

**Priority key:**
- P1: Must have for v0.4.1 launch
- P2: Should have, add during v0.4.1 development
- P3: Nice to have, future consideration

## Competitor Feature Analysis

| Feature | OpenAuto Pro (defunct) | Kenwood DMX1058XR | Pioneer DMH-W4660NEX | Prodigy Approach |
|---------|----------------------|-------------------|----------------------|------------------|
| Band count | 15-band graphic | 13-band parametric | 13-band graphic | 10-band graphic (fits 1024x600) |
| Presets | Flat, Rock, Custom | Multiple built-in | Multiple built-in | 8 built-in + user-created |
| Per-source EQ | No | Source-dependent curves (JVC) | No | Yes -- per AA stream (media/nav/phone) |
| Parametric Q control | No | Yes (Basic/Advanced modes) | Yes | No -- complexity vs. screen size tradeoff |
| Remote config | No | No | No | Yes -- web config panel from phone |
| EQ implementation | LADSPA plugin (PulseAudio) | Hardware DSP | Hardware DSP | Inline biquad in PipeWire callback |
| Config storage | INI file | NVRAM | NVRAM | YAML (human-readable, git-friendly) |

## UX Patterns for Touchscreen EQ

### Slider Design (1024x600 constraint)

- **Vertical sliders**, not horizontal. Frequency on X-axis, gain on Y-axis -- matches the mental model of a frequency response curve.
- **10 sliders in ~900px usable width** (after margins) = ~90px per slider. Comfortable touch target.
- **Slider height**: At least 200px for +/-12dB range. On 600px display minus top/bottom bars (~80px each), ~440px content area. 200px sliders + labels + preset picker fits.
- **Frequency labels below**: "31" "62" "125" "250" "500" "1k" "2k" "4k" "8k" "16k" -- abbreviated, no "Hz".
- **dB readout above active slider**: Show current value while dragging. Disappears on release.
- **Snap to 0dB**: Slight detent at 0dB center for easy flat reset per band.

### Interaction Patterns

- **Tap preset name** to open preset picker (reuse FullScreenPicker).
- **Drag sliders** for individual band adjustment. Switching to custom auto-names "Custom" (or keeps preset name with modified indicator).
- **Long-press/double-tap a slider** to reset that band to 0dB.
- **Bypass toggle** in section header -- prominent, always visible.
- **Stream selector** (if per-stream exposed in touch UI): tab-like buttons -- "Media" | "Nav" | "Phone". Most users only care about media.

### Web Config UX

- **HTML5 range inputs** (vertical) or a canvas-drawn EQ widget.
- **Immediate IPC updates** on slider change (debounced ~50ms).
- **Dropdown per stream** for preset assignment.
- **"Save as Preset" button** with text input for name.

## Sources

- [Crutchfield: How to adjust your car's equalizer settings](https://www.crutchfield.com/learn/how-to-adjust-the-equalizer-in-your-car.html)
- [SoundCertified: Best Equalizer Settings for Car Audio](https://soundcertified.com/best-equalizer-settings-for-car-audio/)
- [Sorena Car Audio: Comprehensive Guide to Car Audio Equalizers](https://sorenacaraudio.com/comprehensive-guide-to-car-audio-equalizers-elevate-your-sound-quality/)
- [PipeWire: Filter-Chain module](https://docs.pipewire.org/page_module_filter_chain.html)
- [PipeWire: Tutorial 7 - Creating an Audio DSP Filter](https://docs.pipewire.org/devel/page_tutorial7.html)
- [PipeWire: Parametric Equalizer module](https://docs.pipewire.org/page_module_parametric_equalizer.html)
- [BlueWave Studio forum: OAP 5.0 equalizer](https://bluewavestudio.io/community/archive/index.php?thread-1286.html)
- [Headphones Addict: Best Equalizer Settings for Music Genres](https://headphonesaddict.com/best-equalizer-settings-for-music-genres/)
- [Digital Trends: How to master your equalizer settings](https://www.digitaltrends.com/home-theater/eq-explainer/)
- OpenAuto Prodigy reverse-engineering notes (in-repo)

---
*Feature research for: Audio Equalizer (v0.4.1 milestone)*
*Researched: 2026-03-01*
