# Project Research Summary

**Project:** OpenAuto Prodigy — Audio Equalizer (v0.4.1)
**Domain:** Real-time 10-band graphic EQ for PipeWire-based Android Auto head unit
**Researched:** 2026-03-01
**Confidence:** HIGH

## Executive Summary

The audio equalizer for OpenAuto Prodigy is a well-scoped, self-contained feature with no new dependencies. All four research streams converged on the same architecture: **inline biquad DSP processing inside the existing PipeWire `onPlaybackProcess` callback**. This approach was evaluated alongside two PipeWire-native alternatives (filter-chain modules and `pw_filter` nodes), both rejected due to graph latency, broken rate matching, and complex link lifecycle. The biquad approach slots cleanly into the existing ring buffer → callback → PW buffer pipeline with approximately 15 new lines of integration code.

The recommended implementation uses the Robert Bristow-Johnson Audio EQ Cookbook (peaking EQ formula), which is the W3C-hosted industry standard. The entire DSP core is approximately 150 lines of C++ using only `<cmath>` and `<atomic>` — nothing new to link, nothing to install. The feature scope is well-defined: 10 bands at ISO octave frequencies (31Hz–16kHz), ±12dB range, 8 bundled presets, per-stream profiles (media/navigation/phone), YAML persistence, QML touch UI, and web config extension. Total estimated new code is approximately 1,070 lines across 9 files.

The primary risks are in the real-time audio domain: mutex use in the callback (priority inversion), denormal floats during quiet audio (CPU spikes on Cortex-A72), and coefficient snap-in on preset change (audible clicks). All three have well-documented prevention strategies — double-buffered atomic coefficients, FTZ bit on the audio thread, and coefficient interpolation over 5–10ms. These are not theoretical risks; they are the specific failure modes that every IIR EQ implementation on real-time audio hardware encounters. Getting them right from the start is essential — retrofitting them is painful.

## Key Findings

### Recommended Stack

No new dependencies. The entire EQ implementation builds on the existing stack. DSP math uses `<cmath>`. Thread safety uses `<atomic>`. The existing PipeWire `pw_stream` API, yaml-cpp config system, Qt Quick Controls, and Flask/IPC web panel all extend naturally.

**Core technologies:**
- **`<cmath>` + `<atomic>` + `<array>`**: DSP math and lock-free coefficient swap — standard C++17, zero new dependency
- **Existing `onPlaybackProcess` callback**: Integration point for inline biquad processing — already owns the audio data at the right moment in the right thread
- **Existing yaml-cpp (YamlConfig)**: Preset and profile persistence — follows `setValueByPath`/`initDefaults` pattern already in use
- **Existing Qt Quick Controls (Slider, ComboBox, Repeater)**: EQ touch UI — available in both Qt 6.4 (dev VM) and Qt 6.8 (Pi)
- **Existing IpcServer + Flask**: Web config panel extension — additive, follows established `handleGetX`/`handleSetX` pattern

**Version compatibility:** No issues. Inline processing uses only `pw_stream` API (no filter-chain modules), so the PipeWire version gap (1.0.5 dev VM vs 1.4.2 Pi) is irrelevant. Qt 6.4 and 6.8 both cover the basic QML controls needed.

### Expected Features

**Must have (table stakes):**
- 10-band graphic EQ sliders (31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16kHz) — standard for any head unit EQ
- Bundled presets (8): Flat, Rock, Pop, Jazz, Bass Boost, Treble Boost, Voice, Car Cabin — one-tap sound profiles are expected
- Bypass/flat toggle — always-visible, prominent; users need to hear the before/after difference
- Real-time slider response — no "apply" button; coefficients update within one audio quantum (~21ms)
- Visual frequency response curve — polyline connecting slider tops; trivial to add, significant UX improvement
- YAML config persistence — survives reboot; non-negotiable for a car head unit

**Should have (differentiators):**
- Per-stream EQ profiles — different curves for media vs navigation vs phone; no commercial head unit does this
- User-created presets — save custom curves with a name; targets power users
- Web config panel EQ — adjust from phone browser with better precision than touchscreen
- Per-stream preset assignment in web panel — configure all three streams from one screen

**Defer (v2+):**
- System-wide EQ covering BT audio — different architecture (sink-level filter-chain), out of v0.4.1 scope
- Parametric EQ (adjustable Q/frequency) — UI nightmare on 1024x600; users who need it can use PipeWire externally
- Real-time spectrum analyzer — FFT on the RT thread competes with video decode CPU budget on Pi 4
- Auto-EQ / room correction — requires calibrated mic, test signal, far beyond scope
- Separate left/right channel EQ — doubles controls, UI nightmare on small touchscreen

### Architecture Approach

The architecture is a clean **Service + Processor split**: `EqualizerService` (QObject, main thread, UI bindings, config, presets) owns `EqualizerProcessor` instances (RT thread, stateless except biquad filter state). They communicate only through atomic pointer swap — the service computes new coefficients on the main thread and stores them atomically; the processor checks and applies them at the top of each RT callback. This mirrors how ThemeService and AudioService already work. The integration into `AudioService` is minimal: `AudioStreamHandle` gets an `eqProcessor` field, `createStream()` initializes it, and `onPlaybackProcess()` gets approximately 15 lines between the ring buffer read and the silence fill.

**Major components:**
1. **`BiquadFilter` (header-only)** — single biquad stage; coefficients + z1/z2 state; Transposed Direct Form II
2. **`EqualizerProcessor`** — 10-band cascade per channel; atomic coefficient swap; RT-safe `process(int16_t*, nFrames, channels)`
3. **`EqualizerService`** (QObject) — per-stream profiles, preset library (bundled + user), config persistence, Q_PROPERTYs for QML
4. **`EqualizerSettings.qml`** — vertical sliders, stream selector tabs, preset picker, frequency curve; integrates with Settings view
5. **IPC extension** — 5 new JSON commands over existing Unix socket; web panel reads/writes via these, never direct YAML access

**Suggested build order:** BiquadFilter → EqualizerProcessor → AudioService integration → EqualizerService → config schema → IPC handlers → QML UI → web config panel. Bottom-up: testable DSP core first, UI surfaces last.

### Critical Pitfalls

1. **Mutex in the RT callback** — any lock in `onPlaybackProcess` causes priority inversion under load. Use double-buffered atomic coefficient swap. No exceptions. An atomic index swap (`std::atomic<int>`) between two coefficient arrays is wait-free and correct.

2. **Denormal floats during quiet audio** — IIR biquad state variables decay to subnormals when audio goes quiet; Cortex-A72 processes these ~100x slower, blowing the RT deadline. Set FTZ bit on FPCR at PipeWire thread start: `fpcr |= (1 << 24)`. One assembly line prevents the entire class of problem.

3. **Coefficient snap causes click artifacts on preset change** — atomically swapping biquad coefficients produces an audible transient because filter delay line state is inconsistent with the new transfer function. Interpolate coefficients smoothly over 5–10ms (240–480 samples at 48kHz). **This is not optional polish — implement it from day one.**

4. **EQ on navigation/phone audio breaks intelligibility** — bass boost preset applied to navigation voice prompts masks consonants over road noise. Default nav and phone streams to flat or Voice preset; never apply user music EQ to speech streams automatically. The three-stream separation in AudioService already enables clean per-stream control.

5. **Config defaults gate** — `setValueByPath` silently fails without an `initDefaults()` entry (known project issue). Register all `audio.equalizer.*` keys in `initDefaults()` before any read/write operation.

## Implications for Roadmap

Based on research, the natural build order is bottom-up: testable DSP core first, service layer second, UI surfaces last. This minimizes integration risk and enables unit testing at each layer before touching the next.

### Phase 1: DSP Core
**Rationale:** Everything else depends on the biquad math being correct and RT-safe. This layer has zero Qt/PipeWire dependencies and is fully unit-testable on the dev VM without a PipeWire daemon. Getting coefficient accuracy and thread safety right before touching any other system avoids debugging RT problems through layers of UI.
**Delivers:** `BiquadFilter` (header-only), `EqualizerProcessor` with atomic coefficient swap, RBJ peaking EQ coefficient calculator, coefficient interpolation, FTZ setup
**Addresses:** 10-band EQ (underlying engine), bypass toggle (atomic `enabled_` flag), real-time response (lock-free updates)
**Avoids:** Pitfall 1 (mutex in RT callback), Pitfall 2 (denormals — FTZ set here), Pitfall 3 (click artifacts — interpolation implemented here, not deferred)

### Phase 2: AudioService Integration
**Rationale:** The DSP core is useless without being wired into the audio pipeline. This phase does the minimal integration: add `eqProcessor` to `AudioStreamHandle`, call `process()` in `onPlaybackProcess()`, and verify the PI rate controller is unaffected. Approximately 15 lines of production code but the most risk-sensitive step because it touches the RT callback.
**Delivers:** Working EQ on all three AA audio streams, per-stream processor instances, verified rate matching stability
**Addresses:** Per-stream EQ profiles (foundation), audio quality improvement
**Avoids:** Pitfall 4 (speech stream intelligibility — default nav/phone to flat here), Pitfall 6 (rate matching disruption — verify PI controller unchanged with EQ enabled)

### Phase 3: EqualizerService + Config
**Rationale:** With processors wired in, the service layer provides the Qt-facing API: per-stream profiles, preset library (8 bundled presets compiled into binary), config persistence, and QML Q_PROPERTY bindings. This is where the per-stream differentiator gets built.
**Delivers:** `EqualizerService` QObject, 8 bundled presets (code-defined), per-stream profiles in YAML, `initDefaults()` registration, user preset save/delete
**Addresses:** Bundled presets, user-created presets, YAML persistence, per-stream profiles
**Avoids:** Pitfall 5 (config defaults gate — register all keys before first access), Pitfall 7 (correct YAML schema up front)

### Phase 4: Head Unit Touch UI
**Rationale:** Primary user-facing surface. Built after the service layer is stable so QML bindings are reliable. Preset-first design (prominent preset picker, sliders behind "Custom" tap) reduces touch precision requirements and matches how most users interact with EQ.
**Delivers:** `EqualizerSettings.qml` with 10 vertical sliders, stream selector tabs, preset picker (reuses FullScreenPicker), frequency curve polyline, bypass toggle, "Reset to Flat" button
**Addresses:** All P1 features — sliders, presets, curve, bypass, stream switching
**Avoids:** Pitfall 9 (touch target sizing — preset-first design, sliders behind advanced tap; snap to 1dB increments)

### Phase 5: Web Config Panel
**Rationale:** Second UI surface, lower priority than touch UI. Depends on Phase 3 IPC commands. Delivers power-user configuration from phone/laptop — better precision for fine tuning than fat-finger touchscreen sliders.
**Delivers:** Flask `/equalizer` route, HTML/JS EQ panel with sliders, per-stream preset dropdowns, save-as-preset form, real-time IPC commands
**Addresses:** Web config EQ (P2), per-stream web panel assignment (P2), all P2 features
**Avoids:** Pitfall 8 (web/head-unit state sync — IPC socket is single source of truth, web panel polls for state rather than reading YAML directly)

### Phase Ordering Rationale

- **DSP before service:** Biquad math is dependency-free and fully unit-testable. Bugs found here cost nothing. Bugs found after Qt wiring cost more.
- **Integration before service:** The RT callback is the most constrained environment. Verify the integration point works in isolation before adding Qt machinery on top.
- **Service before UI:** QML bindings require stable Q_PROPERTYs and Q_INVOKABLEs to bind against.
- **Touch UI before web:** Primary interface first; web panel is an enhancement.
- **Coefficient interpolation in Phase 1, not Phase 4:** Click artifacts are a DSP problem, not a UI problem. Deferring them to "UI polish" is a trap — by then the audio callback is wired into the app and testing is harder.

### Research Flags

Phases with well-documented patterns (skip `/gsd:research-phase`):
- **Phase 1 (DSP Core):** RBJ cookbook is the canonical W3C reference; Transposed Direct Form II is textbook; no research needed.
- **Phase 2 (AudioService integration):** Codebase was read directly during research; integration point is fully characterized.
- **Phase 3 (Service + Config):** Follows existing ThemeService and YamlConfig patterns exactly.
- **Phase 4 (Touch UI):** Standard Qt Quick Controls; UiMetrics scaling established in project.
- **Phase 5 (Web Config):** Follows existing Flask + IPC patterns. Only open question is polling vs push for state sync — recommend polling at ~1s interval for v0.4.1.

No phases need `/gsd:research-phase`. The domain is extremely well-documented from multiple high-confidence sources, and the codebase was read directly during research.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All technologies already in use; no new dependencies; version compatibility verified for both Qt 6.4/6.8 and PipeWire 1.0.5/1.4.2 |
| Features | HIGH | Standard car audio EQ domain; OAP reference available; competitor analysis complete; feature count and UX patterns well-established |
| Architecture | HIGH | Codebase read directly; existing `onPlaybackProcess` callback fully characterized; integration point confirmed; all three alternative approaches evaluated and rejected with specific reasons |
| Pitfalls | HIGH | RT audio pitfalls from official PipeWire docs + established EarLevel Engineering references; Pi 4 ARM-specific pitfalls from ARM community + codebase analysis |

**Overall confidence:** HIGH

### Gaps to Address

- **QML slider layout at 1024x600:** 10 vertical sliders in ~900px usable width needs visual prototyping to confirm touch targets are comfortable at driving precision levels. STACK.md flagged this as MEDIUM. Mitigation: preset-first design means most users never touch individual sliders.
- **Coefficient interpolation implementation detail:** Research recommends 5–10ms linear interpolation but the exact per-sample `alpha` increment loop in the RT callback needs care. If the quantum is shorter than the interpolation window, carry `alpha` across multiple callbacks. If alpha reaches 1.0 mid-callback, stop interpolating. Not complex but needs explicit handling.
- **Web config state sync direction:** Current IpcServer is request/response only — no server-initiated push. If the user adjusts EQ via the head unit touchscreen while the web panel is open, the web panel won't update until next poll. Options: poll every ~1s (simple, acceptable for v0.4.1), or upgrade to WebSocket (deferred). Recommend polling for now.

## Sources

### Primary (HIGH confidence)
- [W3C Audio EQ Cookbook (RBJ)](https://www.w3.org/TR/audio-eq-cookbook/) — biquad peaking EQ coefficient formulas, the definitive reference
- [PipeWire Tutorial 7: Audio DSP Filter](https://docs.pipewire.org/devel/page_tutorial7.html) — RT callback structure; pw_filter API (evaluated, rejected)
- [PipeWire Filter-Chain Module](https://docs.pipewire.org/page_module_filter_chain.html) — external EQ approach (evaluated, rejected)
- [PipeWire Parametric Equalizer Module](https://docs.pipewire.org/page_module_parametric_equalizer.html) — external EQ approach (evaluated, rejected)
- [PipeWire Filter API](https://docs.pipewire.org/group__pw__filter.html) — pw_filter reference (evaluated, rejected)
- Existing codebase: `AudioService.cpp`, `AudioService.hpp`, `AudioRingBuffer.hpp`, `IpcServer.cpp`, `ThemeService.hpp`, `YamlConfig.hpp` — primary source, read directly during research

### Secondary (MEDIUM confidence)
- [EarLevel Engineering: Biquad C++ Source](https://www.earlevel.com/main/2012/11/26/biquad-c-source-code/) — Transposed Direct Form II reference implementation
- [EarLevel Engineering: Denormalization in Audio](https://www.earlevel.com/main/2012/12/03/a-note-about-de-normalization/) — FTZ/DAZ mitigation strategies and ARM specifics
- [DSP Parameter Smoothing (Dark Palace Studio)](https://darkpalace.studio/2025/04/18/dsp-smoothing.html) — coefficient interpolation patterns for RT audio
- [Pi 4 Real-Time DSP Discussion (RPi Forums)](https://forums.raspberrypi.com/viewtopic.php?t=248857) — ARM Cortex-A72 performance validation community data
- [Crutchfield: Car EQ Guide](https://www.crutchfield.com/learn/how-to-adjust-the-equalizer-in-your-car.html) — preset values and UX conventions for car audio EQ
- [SoundCertified: Best Equalizer Settings for Car Audio](https://soundcertified.com/best-equalizer-settings-for-car-audio/) — preset genre definitions

### Tertiary (LOW confidence)
- [DSP-Cpp-filters (GitHub)](https://github.com/dimtass/DSP-Cpp-filters) — reference IIR implementations; useful for cross-checking only, not authoritative

---
*Research completed: 2026-03-01*
*Ready for roadmap: yes*
