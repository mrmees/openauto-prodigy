#pragma once

#include <algorithm>
#include <cstdint>

namespace oap {

/// Per-frame gain ramp step for focus transitions: a full 0.0 ↔ 1.0 swing
/// completes in 960 frames (~20 ms at 48 kHz) — fast enough that a nav
/// prompt ducks immediately, slow enough that there is no audible click.
inline constexpr float kFocusGainRampPerFrame = 1.0f / 960.0f;

/// Apply a smoothed focus gain to interleaved int16 PCM in place.
///
/// RT-safe (no locks, no allocation): called from the PipeWire process
/// callback with the stream's persistent gain state. Ramps `currentGain`
/// toward `targetGain` by kFocusGainRampPerFrame each frame, scaling the
/// samples as it goes; frames at unity gain pass through bit-exact.
///
/// Returns the gain after the final frame — the caller stores it back and
/// passes it in again on the next callback so ramps continue seamlessly
/// across period boundaries.
inline float applyFocusGain(int16_t* samples, int frames, int channels,
                            float currentGain, float targetGain)
{
    float g = currentGain;
    for (int f = 0; f < frames; ++f) {
        if (g < targetGain)
            g = std::min(g + kFocusGainRampPerFrame, targetGain);
        else if (g > targetGain)
            g = std::max(g - kFocusGainRampPerFrame, targetGain);
        if (g < 1.0f) {
            int16_t* frame = samples + f * channels;
            for (int c = 0; c < channels; ++c)
                frame[c] = static_cast<int16_t>(frame[c] * g);
        }
    }
    return g;
}

} // namespace oap
