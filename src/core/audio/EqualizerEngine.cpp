#include "core/audio/EqualizerEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace oap {

EqualizerEngine::EqualizerEngine(float sampleRate, int channels)
    : sampleRate_(sampleRate)
    , channels_(std::clamp(channels, 1, 2))
    , limiters_{SoftLimiter(sampleRate), SoftLimiter(sampleRate)}
{
    gains_.fill(0.0f);

    // Initialize both buffers to unity (flat)
    // bufferA_ and bufferB_ default-construct to unity
    activeCoeffs_.store(&bufferA_, std::memory_order_relaxed);
    writeCoeffs_ = &bufferB_;
    generation_.store(0, std::memory_order_relaxed);
    lastSeenGeneration_ = 0;

    // Initialize RT-side interpolation state to unity
    for (int i = 0; i < kNumBands; ++i) {
        oldCoeffs_[i] = BiquadCoeffs::unity();
        newCoeffs_[i] = BiquadCoeffs::unity();
    }

    // Zero all filter states
    for (auto& ch : states_) {
        for (auto& s : ch) {
            s.reset();
        }
    }
}

void EqualizerEngine::setGain(int band, float dB)
{
    if (band < 0 || band >= kNumBands) return;

    dB = std::clamp(dB, -12.0f, 12.0f);
    gains_[band] = dB;

    recomputeCoeffs();
}

void EqualizerEngine::setAllGains(const std::array<float, kNumBands>& gainsDb)
{
    for (int i = 0; i < kNumBands; ++i) {
        gains_[i] = std::clamp(gainsDb[i], -12.0f, 12.0f);
    }

    recomputeCoeffs();
}

float EqualizerEngine::getGain(int band) const
{
    if (band < 0 || band >= kNumBands) return 0.0f;
    return gains_[band];
}

void EqualizerEngine::setBypassed(bool bypassed)
{
    bypassed_.store(bypassed, std::memory_order_relaxed);

    // Also update the coefficient buffer's bypass flag so the RT side picks it up
    recomputeCoeffs();
}

bool EqualizerEngine::isBypassed() const
{
    return bypassed_.load(std::memory_order_relaxed);
}

void EqualizerEngine::recomputeCoeffs()
{
    EngineCoeffs newCoeffs;
    for (int i = 0; i < kNumBands; ++i) {
        if (std::fabs(gains_[i]) < 0.001f) {
            newCoeffs.bands[i] = BiquadCoeffs::unity();
        } else {
            newCoeffs.bands[i] = calcPeakingEQ(
                kCenterFreqs[i], gains_[i], kGraphicEqQ, sampleRate_);
        }
    }
    newCoeffs.bypass = bypassed_.load(std::memory_order_relaxed);

    swapCoeffs(newCoeffs);
}

void EqualizerEngine::swapCoeffs(const EngineCoeffs& newCoeffs)
{
    // Write to inactive buffer, then swap pointer
    *writeCoeffs_ = newCoeffs;

    // Swap: make writeCoeffs_ the active one, old active becomes write target
    EngineCoeffs* prev = activeCoeffs_.exchange(writeCoeffs_, std::memory_order_release);
    writeCoeffs_ = prev;
    generation_.fetch_add(1, std::memory_order_release);
}

void EqualizerEngine::process(int16_t* data, int frameCount)
{
    if (frameCount <= 0 || data == nullptr) return;

    // Check for new coefficients (generation counter avoids ABA problem)
    uint32_t gen = generation_.load(std::memory_order_acquire);
    EngineCoeffs* current = activeCoeffs_.load(std::memory_order_acquire);
    if (gen != lastSeenGeneration_) {
        // New coefficients arrived — start interpolation

        // If already interpolating, snapshot current interpolated position as "old"
        if (interpSamplesRemaining_ > 0) {
            float t = 1.0f - static_cast<float>(interpSamplesRemaining_)
                             / static_cast<float>(kInterpolationSamples);
            for (int b = 0; b < kNumBands; ++b) {
                auto& o = oldCoeffs_[b];
                auto& n = newCoeffs_[b];
                o.b0 = o.b0 + t * (n.b0 - o.b0);
                o.b1 = o.b1 + t * (n.b1 - o.b1);
                o.b2 = o.b2 + t * (n.b2 - o.b2);
                o.a1 = o.a1 + t * (n.a1 - o.a1);
                o.a2 = o.a2 + t * (n.a2 - o.a2);
            }
        } else {
            // Copy current newCoeffs_ as old (they were what we were using)
            oldCoeffs_ = newCoeffs_;
        }

        newCoeffs_ = current->bands;

        // Handle bypass state change
        float newBypassTarget = current->bypass ? 1.0f : 0.0f;
        if (newBypassTarget != bypassTarget_) {
            bypassTarget_ = newBypassTarget;
            bypassRampRemaining_ = kInterpolationSamples;
        }

        // If fully bypassed with no bypass ramp pending, skip interpolation —
        // just snap to new coefficients so they're ready when un-bypassed.
        if (bypassMix_ >= 1.0f && bypassRampRemaining_ <= 0) {
            oldCoeffs_ = current->bands;
            interpSamplesRemaining_ = 0;
        } else {
            interpSamplesRemaining_ = kInterpolationSamples;
        }

        lastSeenGeneration_ = gen;
    }

    // Fully bypassed and no ramp in progress — fast path
    if (bypassMix_ >= 1.0f && bypassRampRemaining_ <= 0 && interpSamplesRemaining_ <= 0) {
        return; // data passes through unmodified
    }

    for (int f = 0; f < frameCount; ++f) {
        // Compute current interpolated coefficients if ramping
        std::array<BiquadCoeffs, kNumBands> coeffs;
        if (interpSamplesRemaining_ > 0) {
            float t = 1.0f - static_cast<float>(interpSamplesRemaining_)
                             / static_cast<float>(kInterpolationSamples);
            for (int b = 0; b < kNumBands; ++b) {
                coeffs[b].b0 = oldCoeffs_[b].b0 + t * (newCoeffs_[b].b0 - oldCoeffs_[b].b0);
                coeffs[b].b1 = oldCoeffs_[b].b1 + t * (newCoeffs_[b].b1 - oldCoeffs_[b].b1);
                coeffs[b].b2 = oldCoeffs_[b].b2 + t * (newCoeffs_[b].b2 - oldCoeffs_[b].b2);
                coeffs[b].a1 = oldCoeffs_[b].a1 + t * (newCoeffs_[b].a1 - oldCoeffs_[b].a1);
                coeffs[b].a2 = oldCoeffs_[b].a2 + t * (newCoeffs_[b].a2 - oldCoeffs_[b].a2);
            }
            --interpSamplesRemaining_;
        } else {
            coeffs = newCoeffs_;
        }

        // Update bypass crossfade mix
        if (bypassRampRemaining_ > 0) {
            float step = 1.0f / static_cast<float>(kInterpolationSamples);
            if (bypassTarget_ > bypassMix_) {
                bypassMix_ = std::min(bypassMix_ + step, 1.0f);
            } else {
                bypassMix_ = std::max(bypassMix_ - step, 0.0f);
            }
            --bypassRampRemaining_;
        }

        for (int ch = 0; ch < channels_; ++ch) {
            int idx = f * channels_ + ch;
            float dry = static_cast<float>(data[idx]) / 32768.0f;
            float wet = dry;

            // Process through 10-band biquad cascade
            for (int b = 0; b < kNumBands; ++b) {
                wet = processSample(wet, coeffs[b], states_[ch][b]);
            }

            // Soft limiter
            wet = limiters_[ch].process(wet);

            // Bypass crossfade: mix = 0 means full wet (EQ active), 1 means full dry (bypass)
            float out;
            if (bypassMix_ <= 0.0f) {
                out = wet;
            } else if (bypassMix_ >= 1.0f) {
                out = dry;
            } else {
                out = wet * (1.0f - bypassMix_) + dry * bypassMix_;
            }

            // Convert back to int16_t with clamp
            int32_t sample = static_cast<int32_t>(out * 32768.0f);
            sample = std::clamp(sample, static_cast<int32_t>(-32768), static_cast<int32_t>(32767));
            data[idx] = static_cast<int16_t>(sample);
        }
    }
}

} // namespace oap
