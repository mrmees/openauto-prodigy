# Domain Pitfalls

**Domain:** Real-time audio equalizer (10-band graphic EQ) added to existing PipeWire stream pipeline on Raspberry Pi 4
**Researched:** 2026-03-01
**Confidence:** MEDIUM-HIGH (PipeWire RT constraints from official docs, DSP pitfalls from verified audio engineering sources, Pi 4 performance from community benchmarks + codebase analysis)

## Critical Pitfalls

### Pitfall 1: EQ Processing in PipeWire RT Callback Violates Real-Time Safety

**What goes wrong:** You add biquad filter processing directly inside `onPlaybackProcess()` -- the existing PipeWire RT callback. It works fine initially, but under load (phone navigation + media + call audio simultaneously), you get audio glitches, xruns, or dropouts. The RT thread stalls because your EQ code does something non-RT-safe: allocates memory for coefficient arrays, locks a mutex to read user-changed preset values, or triggers a `qCDebug()` log call that blocks on journald.

**Why it happens:** The existing `onPlaybackProcess()` is already carefully written to be RT-safe -- no allocations, no locks, no logging (just `fprintf` for periodic diagnostics, which is borderline). Adding EQ processing is tempting to do inline here because it's the natural insertion point between ring buffer read and `pw_stream_queue_buffer`. But EQ introduces new shared state: filter coefficients that the UI thread writes and the RT callback reads. The instinct is to protect that with a mutex, which is the single worst thing you can do in an RT callback.

**Consequences:** Audio dropouts during preset switches. Intermittent glitches that only appear under load. Priority inversion if the UI thread holds the coefficient mutex while the RT thread waits. Worst case: the RT thread misses its deadline, PipeWire marks the stream as failed, and audio stops entirely until stream recreation.

**Prevention:**
1. **Double-buffered coefficients:** Maintain two coefficient arrays. UI thread writes to the "staging" array, then atomically swaps a pointer (`std::atomic<int>` index). RT callback reads from the "active" array. Zero locking, zero contention.
2. **Never allocate in the callback.** Pre-allocate all filter state (delay lines, coefficient arrays) at stream creation time. A 10-band stereo biquad needs exactly 10 x 2 channels x 4 state variables = 80 floats of persistent state -- trivially small, allocate once.
3. **No logging in the RT path.** Use the existing pattern from `onPlaybackProcess()`: atomic counters incremented in RT, polled and logged from a Qt timer on the main thread.
4. **No mutex, no `QMutexLocker`, no `std::lock_guard` in the callback.** Period.

**Detection:** Enable PipeWire's xrun reporting (`PIPEWIRE_DEBUG=3`). Monitor `pw-top` for xrun counts on your streams. Any xrun during a preset change = coefficient access is not lock-free.

---

### Pitfall 2: Denormal Floating-Point Numbers Tank Performance on Idle/Quiet Audio

**What goes wrong:** Your EQ works great during loud music playback. But when audio goes quiet (silence between tracks, paused playback, quiet navigation prompt decaying), CPU usage on the audio thread spikes dramatically -- sometimes 100x slower per sample. On Pi 4, this can push the RT callback past its deadline, causing xruns.

**Why it happens:** IIR biquad filters have internal state (delay line / z^-1 and z^-2 values). When input drops to near-zero, these state values decay toward extremely small floating-point numbers called "denormals" (subnormals). Most CPUs, including the Cortex-A72 in Pi 4, process denormalized floats dramatically slower than normal floats because they require microcode fallback instead of hardware FPU paths. A 10-band cascaded biquad has 10 x 2 = 20 state variables per channel, all decaying in parallel -- 40 potential denormal hotspots for stereo.

**Consequences:** Audio glitches during quiet passages or between tracks. CPU spikes visible in `htop` that correlate with silence rather than load. The existing adaptive buffer growth (`checkAdaptiveBuffers()`) may misdiagnose this as underruns and grow buffers unnecessarily.

**Prevention:**
1. **Set FTZ (Flush-To-Zero) on the audio thread.** On AArch64 (Pi 4), set bit 24 of FPCR at thread start:
   ```cpp
   // Call once when PipeWire thread starts (or at top of first RT callback)
   uint64_t fpcr;
   __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
   fpcr |= (1 << 24); // FZ bit
   __asm__ __volatile__("msr fpcr, %0" :: "r"(fpcr));
   ```
   This makes ALL denormals flush to zero automatically. On x86 dev VM, use `_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)` from `<xmmintrin.h>`.
2. **Alternative: add a tiny DC bias.** Add 1e-18f to each filter's input before processing. Inaudible (-360 dB), prevents state variables from reaching denormal range. Less elegant than FTZ but portable.
3. **Do NOT rely on compiler flags alone.** `-ffast-math` implies `-ffinite-math-only` which does NOT set FTZ at runtime on ARM. You need the explicit FPCR write.

**Detection:** Profile the `onPlaybackProcess` callback duration during silence vs playback. If silence is slower, you have a denormal problem. `perf stat -e fp_denorm_input` can count denormal operations on some ARM cores.

---

### Pitfall 3: Preset Switching Causes Audible Click/Pop Artifacts

**What goes wrong:** User taps a preset button (switches from "Rock" to "Jazz"). For one buffer frame, the audio passes through a set of filter coefficients that are mathematically inconsistent -- some bands have the old coefficients, some have the new ones, or the filter state (delay lines) is inconsistent with the new coefficients. Result: a loud click, pop, or transient artifact that's very noticeable in a car environment.

**Why it happens:** Biquad IIR filters have internal state (the z^-1 and z^-2 delay values) that are the result of processing history with the OLD coefficients. When you slam in NEW coefficients, the filter's transfer function changes discontinuously. The delay line values, which were appropriate for the old filter shape, become inappropriate for the new shape, producing a transient output spike. This is NOT just about atomic swaps -- even a perfectly atomic coefficient swap causes this artifact because the state is stale.

**Consequences:** Audible click on every preset change. Users will avoid changing presets while music is playing, defeating the purpose of an EQ.

**Prevention:**
1. **Coefficient interpolation (recommended):** Smoothly interpolate between old and new coefficients over 5-10ms (240-480 samples at 48kHz). Per sample: `coeff = old + alpha * (new - old)`, advancing `alpha` from 0 to 1. This spreads the transient over many samples, making it inaudible. Recalculate the target coefficients once per block; interpolate per sample.
2. **Crossfade (alternative):** Run both old and new filter instances simultaneously during transition. Crossfade output over ~10ms. Uses 2x CPU briefly but guarantees glitch-free. More complex to implement.
3. **Reset delay lines on preset change (worst option):** Zeroing the delay lines eliminates the stale-state problem but introduces its own transient as the filter "rings up" from zero. Less bad than a coefficient slam but still audible on material with sustained bass.
4. **DO NOT just atomically swap coefficients and call it done.** The atomic swap prevents data races but does NOT prevent audio artifacts.

**Detection:** Automate rapid preset cycling while playing a sustained sine wave. Record output and look for spikes in a waveform editor. If you see any transient larger than the sine amplitude, you have a glitch.

---

### Pitfall 4: EQ Applied to Navigation/Phone Audio Causes Latency or Intelligibility Problems

**What goes wrong:** You apply the same EQ curve to all three PipeWire streams (media, navigation, phone). The "Bass Boost" preset that sounds great for music makes navigation prompts sound muddy and hard to understand over road noise. Or the filter processing adds enough latency that "turn right in 200 feet" arrives perceptibly late relative to the visual map prompt.

**Why it happens:** The current `AudioService` creates separate streams for media, navigation, and phone (confirmed in codebase and `CLAUDE.md`). The temptation is to apply EQ uniformly for simplicity. But speech intelligibility and music enjoyment have opposite EQ requirements: speech benefits from mid-frequency boost (2-4 kHz clarity range), while music presets often cut mids and boost bass/treble ("smiley face" EQ). Additionally, even inline biquad processing adds measurable latency at the sample level (each biquad stage adds 2 samples of group delay for a peaking filter).

**Consequences:** "Turn right" is unintelligible because bass boost masks consonant frequencies. Phone calls sound hollow or muffled. Users get lost because they can't understand navigation prompts. If using PipeWire filter-chain instead of inline processing, the extra graph node adds a full quantum of latency (~21ms at 1024/48000) per stream.

**Prevention:**
1. **Per-stream EQ profiles are the correct design (already planned).** Media gets the user's chosen preset. Navigation gets a fixed "speech clarity" preset (or flat). Phone gets a fixed "voice" preset (or flat).
2. **Default to EQ OFF for navigation and phone streams.** Let users opt in to EQ on speech streams, but never make it the default.
3. **Use inline biquad processing, not PipeWire filter-chain.** The current architecture writes audio into a ring buffer (`writeAudio()`), then the RT callback reads and outputs it. Insert biquad processing in the RT callback AFTER the ring buffer read, BEFORE writing to the PipeWire buffer. This adds zero graph-level latency -- just the ~2 sample group delay per biquad stage (~0.4ms total for 10 bands at 48kHz, imperceptible).
4. **Test with actual road noise.** Play a road noise recording through the car speakers while testing navigation audio with EQ. What sounds fine in a quiet room may be unintelligible at 60mph.

**Detection:** Measure time between visual navigation prompt change and audio onset with and without EQ. If delta exceeds 50ms, the EQ path is adding unacceptable latency.

---

## Moderate Pitfalls

### Pitfall 5: Biquad Coefficient Calculation Uses Wrong Formula or Wrong Q Convention

**What goes wrong:** Your 10-band EQ produces unexpected frequency response -- bands interact in weird ways, boosting one band affects adjacent bands more than expected, or the overall level shifts up/down when only one band is adjusted.

**Prevention:**
1. Use the Robert Bristow-Johnson Audio EQ Cookbook (the canonical reference: `https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html`). Specifically use the "peakingEQ" filter type for graphic EQ bands.
2. **Q convention matters.** The cookbook's `Q` for peakingEQ controls bandwidth. For a graphic EQ, typical Q is 1.4-4.3 depending on band spacing. For 10 bands (roughly 1-octave spacing from 31Hz to 16kHz), use Q ~1.4 (1-octave bandwidth). Higher Q = narrower band = less interaction but less musical.
3. **Gain is in dB, not linear.** A +6dB boost is `10^(6/20) = 1.995x`, not `6x`.
4. **Compute coefficients on the UI thread, not the RT thread.** Trig functions (`sin()`, `cos()`, `pow()`) are expensive and not deterministic in timing -- never call them in the RT callback.

---

### Pitfall 6: Ring Buffer + EQ Interaction Breaks Rate Matching PI Controller

**What goes wrong:** The existing PI controller for adaptive rate matching (`pw_stream_set_rate()`) is tuned for the current ring buffer behavior. Adding EQ processing in the RT callback changes the timing characteristics -- the callback takes slightly longer, the fill level dynamics change, and the PI controller starts oscillating or over-correcting. Audio pitch wobbles or stutters.

**Prevention:**
1. **EQ processing time is deterministic and tiny** -- 10 biquads x 2 channels x 5 multiply-adds = 100 FLOPS per sample. At 48kHz, that's 4.8 MFLOPS, well within Pi 4's ~10 GFLOPS capability. The callback timing change should be negligible (<1% of quantum duration).
2. **Do NOT change the PI controller gains** when adding EQ. If the controller was stable before, it should remain stable -- EQ doesn't change the ring buffer fill dynamics, only the processing time per callback.
3. **If you observe rate wobble after adding EQ, the problem is elsewhere** -- likely denormals (Pitfall 2) causing variable callback duration, not the EQ math itself.
4. **Verify:** Log `filteredFill` and `correction` values with and without EQ enabled. They should be statistically identical.

---

### Pitfall 7: EQ State Not Persisted Correctly -- Config Defaults Gate Pattern Bites Again

**What goes wrong:** User configures a custom preset, saves it, restarts the app. The preset is gone, or worse, the app crashes because `ConfigService::getValueByPath()` returns an unexpected type for the EQ config subtree.

**Prevention:**
1. The project already has a known issue with config defaults gating: `"setValueByPath silently fails without initDefaults() entry"` (from PROJECT.md Key Decisions). EQ config keys MUST be registered in `initDefaults()` before any read/write.
2. **YAML schema for EQ config:** Define the schema up front:
   ```yaml
   audio:
     equalizer:
       enabled: false
       active_preset: "flat"
       per_stream:
         media: "flat"
         navigation: "flat"
         phone: "flat"
       presets:
         flat:
           bands: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
         rock:
           bands: [5, 4, -1, -2, 1, 3, 5, 6, 6, 5]
   ```
3. **User presets in a separate file** (`~/.openauto/eq-presets.yaml`), not inline in the main config. Main config references preset names; preset definitions live separately. This prevents config bloat and makes preset sharing possible.
4. **Validate on load.** A preset with != 10 bands, or gain values outside [-12, +12] dB, should be rejected with a warning and fall back to flat.

---

### Pitfall 8: Web Config Panel EQ UI Out of Sync with Head Unit

**What goes wrong:** User adjusts EQ via the web panel. The change appears saved in the web UI, but the head unit doesn't reflect it until restart. Or worse, the head unit has a different preset active, and the web panel overwrites it on next save.

**Prevention:**
1. **Use the existing IPC socket (`/tmp/openauto-prodigy.sock`) for real-time EQ commands**, not just config file writes. The web panel should send JSON commands like `{"type": "eq_set_preset", "stream": "media", "preset": "rock"}` over the Unix socket. The Qt app applies the change immediately AND persists to config.
2. **IPC must be bidirectional for EQ state.** When the head unit EQ changes (user adjusts via touch), the IPC server should push the update to connected web clients. The existing IpcServer supports request/response but may not support server-initiated push -- this may need WebSocket upgrade or polling.
3. **Single source of truth:** The Qt app's in-memory EQ state is authoritative. Both UIs (touch and web) read from and write to it via defined APIs. Neither UI reads the YAML file directly for current state.

---

## Minor Pitfalls

### Pitfall 9: Slider UI Not Suitable for Car Touchscreen

**What goes wrong:** You build a nice 10-band EQ with vertical sliders. On the 1024x600 DFRobot touchscreen, the sliders are too narrow to hit accurately while driving. Users accidentally adjust adjacent bands. The slider throw (vertical range) is too short for fine adjustment.

**Prevention:**
1. **Wide touch targets.** Each band slider should be at least 60px wide (with the existing `UiMetrics` scale factor, this means ~55-80dp). For 10 bands across 1024px, that's ~100px per band including spacing -- achievable.
2. **Consider a preset-first UI** instead of raw sliders. Show preset buttons prominently; hide per-band sliders behind a "Custom" or "Advanced" tap. Most users will use presets, not individual band adjustment.
3. **Snap to dB values.** Snap to 1dB increments, not continuous. Reduces the precision needed for touch targeting.
4. **Provide a "Reset to Flat" button** that's always visible. Users who accidentally mess up their EQ need a quick escape.

---

### Pitfall 10: BT Audio Plugin EQ Expectations

**What goes wrong:** User expects the EQ to also apply to Bluetooth A2DP audio (BtAudioPlugin), not just AA media. BT audio uses a completely separate PipeWire stream path.

**Prevention:**
1. **Decide scope early.** If EQ applies to BT audio too, the EQ engine must be integrated at the AudioService level (shared by all plugins), not inside the AndroidAutoPlugin.
2. **Recommended: EQ at AudioService level.** The `onPlaybackProcess()` callback is already per-stream. Add EQ state to `AudioStreamHandle`. Each stream gets its own filter instances. BT audio, AA media, and any future audio source all benefit automatically.
3. **Expose per-stream enable/disable** so users can EQ media but not phone calls, regardless of source plugin.

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Biquad DSP engine | Denormals on quiet audio (Pitfall 2) | Set FTZ on audio thread at startup; test with silence between tracks |
| Biquad DSP engine | Coefficient interpolation missing (Pitfall 3) | Implement 5-10ms smoothing from day one; don't defer as "polish" |
| Integration into AudioService | RT safety violations (Pitfall 1) | Double-buffered coefficients, zero allocations, no logging in callback |
| Integration into AudioService | Rate matching disruption (Pitfall 6) | Verify PI controller stability with EQ enabled; expect no change |
| Per-stream profiles | Navigation intelligibility (Pitfall 4) | Default nav/phone to flat; test with road noise |
| Preset management | Config defaults gate (Pitfall 7) | Register all EQ keys in `initDefaults()` before first access |
| Preset switching | Click artifacts (Pitfall 3) | Coefficient interpolation is mandatory, not optional |
| Head unit touch UI | Touch target sizing (Pitfall 9) | Preset-first design; sliders behind "Custom" |
| Web config panel | State sync (Pitfall 8) | Real-time IPC commands, single source of truth in Qt app |
| BT Audio integration | Scope confusion (Pitfall 10) | EQ engine at AudioService level, not plugin level |

## Sources

- [PipeWire DSP Filter Tutorial](https://docs.pipewire.org/devel/page_tutorial7.html) -- HIGH confidence (official docs, RT safety constraints)
- [PipeWire Filter API](https://docs.pipewire.org/group__pw__filter.html) -- HIGH confidence (official docs)
- [PipeWire Filter-Chain Module](https://docs.pipewire.org/page_module_filter_chain.html) -- HIGH confidence (official docs)
- [PipeWire Parametric Equalizer Module](https://docs.pipewire.org/1.2/page_module_parametric_equalizer.html) -- HIGH confidence (official docs)
- [Audio EQ Cookbook (Robert Bristow-Johnson)](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html) -- HIGH confidence (canonical DSP reference)
- [EarLevel Engineering: Denormalization in Audio](https://www.earlevel.com/main/2012/12/03/a-note-about-de-normalization/) -- HIGH confidence (established audio engineering reference)
- [EarLevel Engineering: Floating Point Caveats](https://www.earlevel.com/main/2019/04/19/floating-point-caveats/) -- HIGH confidence
- [DSP Parameter Smoothing (Dark Palace Studio)](https://darkpalace.studio/2025/04/18/dsp-smoothing.html) -- MEDIUM confidence (practical guide, verified against multiple sources)
- [Pi 4 Real-Time DSP Discussion (RPi Forums)](https://forums.raspberrypi.com/viewtopic.php?t=248857) -- MEDIUM confidence (community, but hardware-specific)
- [ARM NEON Denormal Handling](https://community.arm.com/support-forums/f/ai-and-ml-forum/47731/cmsis-dsp---64-bit-dsp-filtering-on-neon-raspberry-pi) -- MEDIUM confidence (ARM community forum)
- Project codebase: `AudioService.cpp`, `AudioService.hpp`, `AudioRingBuffer.hpp` -- HIGH confidence (primary source, read directly)
- Project memory: PipeWire 1.4.2 on Pi, ring buffer architecture, RT callback patterns -- HIGH confidence (tested)

---
*Pitfalls research for: OpenAuto Prodigy v0.4.1 Audio Equalizer milestone*
*Researched: 2026-03-01*
