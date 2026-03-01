#ifndef OAP_EQUALIZER_ENGINE_HPP
#define OAP_EQUALIZER_ENGINE_HPP

#include <array>
#include <atomic>
#include <cstdint>
#include "core/audio/BiquadFilter.hpp"
#include "core/audio/SoftLimiter.hpp"

namespace oap {

/// RT-safe 10-band graphic equalizer engine.
///
/// Processes stereo interleaved int16_t audio at 48kHz (or configured sample rate).
/// Atomic coefficient swap for lock-free RT/non-RT communication.
/// Per-sample coefficient interpolation for smooth transitions.
/// Wet/dry crossfade for bypass toggle.
/// Integrated soft limiter prevents clipping.
///
/// Thread safety:
///   - setGain/setAllGains/setBypassed: call from any (non-RT) thread
///   - process: call from RT thread only (no allocation, no mutex, no logging)
class EqualizerEngine {
public:
    /// Number of interpolation samples for coefficient transitions (~48ms at 48kHz)
    static constexpr int kInterpolationSamples = 2304;

    /// @param sampleRate  Audio sample rate in Hz
    /// @param channels    Number of audio channels (1 or 2, default stereo)
    explicit EqualizerEngine(float sampleRate = 48000.0f, int channels = 2);

    /// Set gain for a single EQ band.
    /// @param band  Band index (0-9)
    /// @param dB    Gain in dB (clamped to +-12dB)
    void setGain(int band, float dB);

    /// Set all 10 band gains at once (preset switch).
    /// Single atomic swap for all bands.
    void setAllGains(const std::array<float, kNumBands>& gainsDb);

    /// Get current gain for a band.
    /// @param band  Band index (0-9)
    /// @return Gain in dB
    float getGain(int band) const;

    /// Enable/disable bypass (smooth crossfade).
    void setBypassed(bool bypassed);

    /// @return Current bypass state
    bool isBypassed() const;

    /// Process stereo interleaved int16_t audio in-place (RT-safe).
    /// @param data       Pointer to interleaved samples (may be nullptr if frameCount == 0)
    /// @param frameCount Number of frames (each frame = channels samples)
    void process(int16_t* data, int frameCount);

private:
    /// All coefficients for the 10-band EQ + bypass flag, atomically swapped
    struct EngineCoeffs {
        std::array<BiquadCoeffs, kNumBands> bands;
        bool bypass = false;

        EngineCoeffs()
        {
            for (auto& b : bands) b = BiquadCoeffs::unity();
        }
    };

    void recomputeCoeffs();
    void swapCoeffs(const EngineCoeffs& newCoeffs);

    float sampleRate_;
    int channels_;

    // Gain storage (non-RT side)
    std::array<float, kNumBands> gains_;
    std::atomic<bool> bypassed_{false};

    // Double-buffer for atomic coefficient swap (generation counter avoids ABA problem)
    EngineCoeffs bufferA_;
    EngineCoeffs bufferB_;
    std::atomic<EngineCoeffs*> activeCoeffs_;
    EngineCoeffs* writeCoeffs_; // non-RT side writes here
    std::atomic<uint32_t> generation_{0};  // incremented on each swap
    uint32_t lastSeenGeneration_ = 0;      // RT-side tracks last seen

    // RT-side state
    std::array<BiquadCoeffs, kNumBands> oldCoeffs_;
    std::array<BiquadCoeffs, kNumBands> newCoeffs_;
    int interpSamplesRemaining_ = 0;

    // Bypass crossfade state
    float bypassMix_ = 0.0f; // 0.0 = EQ active, 1.0 = bypass
    float bypassTarget_ = 0.0f;
    int bypassRampRemaining_ = 0;

    // Per-channel filter state: [channel][band]
    std::array<std::array<BiquadState, kNumBands>, 2> states_;

    // Per-channel soft limiter
    std::array<SoftLimiter, 2> limiters_;
};

} // namespace oap

#endif // OAP_EQUALIZER_ENGINE_HPP
