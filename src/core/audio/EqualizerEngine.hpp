#ifndef OAP_EQUALIZER_ENGINE_HPP
#define OAP_EQUALIZER_ENGINE_HPP

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include "core/audio/BiquadFilter.hpp"
#include "core/audio/SoftLimiter.hpp"

class TestEqualizerEngine;

namespace oap {

/// RT-safe 10-band graphic equalizer engine.
///
/// Processes stereo interleaved int16_t audio at 48kHz (or configured sample rate).
/// Atomic coefficient snapshots for lock-free RT/non-RT communication.
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
    /// Single coherent publication for all bands.
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
    friend class ::TestEqualizerEngine;

    /// All coefficients for the 10-band EQ + bypass flag.
    struct EngineCoeffs {
        std::array<BiquadCoeffs, kNumBands> bands;
        bool bypass = false;

        EngineCoeffs()
        {
            for (auto& b : bands) b = BiquadCoeffs::unity();
        }
    };

    struct AtomicBiquadCoeffs {
        std::atomic<uint64_t> b0{pack(0, 1.0f)};
        std::atomic<uint64_t> b1{pack(0, 0.0f)};
        std::atomic<uint64_t> b2{pack(0, 0.0f)};
        std::atomic<uint64_t> a1{pack(0, 0.0f)};
        std::atomic<uint64_t> a2{pack(0, 0.0f)};

        static uint64_t pack(uint32_t generation, float value)
        {
            uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            return (static_cast<uint64_t>(generation) << 32)
                   | bits;
        }

        static bool unpack(uint64_t tagged, uint32_t expectedGeneration,
                           float& value)
        {
            if (static_cast<uint32_t>(tagged >> 32) != expectedGeneration)
                return false;
            const uint32_t bits = static_cast<uint32_t>(tagged);
            std::memcpy(&value, &bits, sizeof(value));
            return true;
        }

        void store(uint32_t generation, const BiquadCoeffs& coeffs)
        {
            b0.store(pack(generation, coeffs.b0), std::memory_order_relaxed);
            b1.store(pack(generation, coeffs.b1), std::memory_order_relaxed);
            b2.store(pack(generation, coeffs.b2), std::memory_order_relaxed);
            a1.store(pack(generation, coeffs.a1), std::memory_order_relaxed);
            a2.store(pack(generation, coeffs.a2), std::memory_order_relaxed);
        }

        bool load(uint32_t generation, BiquadCoeffs& result) const
        {
            return unpack(b0.load(std::memory_order_relaxed), generation, result.b0)
                   && unpack(b1.load(std::memory_order_relaxed), generation, result.b1)
                   && unpack(b2.load(std::memory_order_relaxed), generation, result.b2)
                   && unpack(a1.load(std::memory_order_relaxed), generation, result.a1)
                   && unpack(a2.load(std::memory_order_relaxed), generation, result.a2);
        }
    };

    static constexpr uint64_t packBypass(uint32_t generation, bool bypass)
    {
        return (static_cast<uint64_t>(generation) << 32)
               | static_cast<uint64_t>(bypass);
    }

    void recomputeCoeffsLocked();
    void publishCoeffsLocked(const EngineCoeffs& newCoeffs);
    // One bounded RT snapshot attempt. A concurrent publication returns false;
    // process() defers it to the next callback instead of spinning.
    bool tryLoadPublishedCoeffs(EngineCoeffs& result, uint32_t& generation) const;

    float sampleRate_;
    int channels_;

    // Control state. Writers/getters are non-RT and serialized so the public
    // "any non-RT thread" contract is real rather than Qt-thread accidental.
    mutable std::mutex controlMutex_;
    std::array<float, kNumBands> gains_;
    bool bypassed_ = false;

    // Atomic coefficient mailbox. The control writer brackets tagged atomic
    // fields with an odd/even generation. The RT reader accepts only fields
    // tagged with the same stable even generation, so a partially visible
    // publication cannot be mistaken for a coherent snapshot.
    std::array<AtomicBiquadCoeffs, kNumBands> publishedBands_;
    std::atomic<uint64_t> publishedBypass_{packBypass(0, false)};
    std::atomic<uint32_t> publishedGeneration_{0};
    uint32_t lastSeenGeneration_ = 0;      // RT-side only

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
