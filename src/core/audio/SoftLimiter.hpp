#ifndef OAP_SOFT_LIMITER_HPP
#define OAP_SOFT_LIMITER_HPP

#include <cmath>

namespace oap {

/// Envelope-follower peak limiter with fast attack / slow release.
///
/// Prevents output from exceeding 0dBFS when signal is boosted by EQ.
/// Transparent (unity gain) when signal is below threshold.
/// Per-channel — instantiate one per audio channel.
class SoftLimiter {
public:
    /// @param sampleRate  Audio sample rate in Hz (default 48kHz)
    explicit SoftLimiter(float sampleRate = 48000.0f)
        : envelope_(0.0f)
        , threshold_(1.0f)
    {
        // Attack: ~0.5ms — fast enough to catch transients
        attackCoeff_ = 1.0f - std::exp(-1.0f / (0.0005f * sampleRate));
        // Release: ~50ms — slow enough to avoid pumping
        releaseCoeff_ = 1.0f - std::exp(-1.0f / (0.05f * sampleRate));
    }

    /// Process a single sample through the limiter.
    ///
    /// @param x  Input sample (any amplitude)
    /// @return   Limited output sample (magnitude <= threshold when envelope has settled)
    float process(float x)
    {
        float absX = std::fabs(x);

        // Envelope follower: fast attack, slow release
        if (absX > envelope_) {
            envelope_ += attackCoeff_ * (absX - envelope_);
        } else {
            envelope_ += releaseCoeff_ * (absX - envelope_);
        }

        // Apply gain reduction when envelope exceeds threshold
        if (envelope_ > threshold_) {
            return x * (threshold_ / envelope_);
        }
        return x;
    }

    /// Reset the limiter state (envelope to zero).
    void reset()
    {
        envelope_ = 0.0f;
    }

private:
    float envelope_;
    float attackCoeff_;
    float releaseCoeff_;
    float threshold_;
};

} // namespace oap

#endif // OAP_SOFT_LIMITER_HPP
