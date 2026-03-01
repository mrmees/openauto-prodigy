#ifndef OAP_BIQUAD_FILTER_HPP
#define OAP_BIQUAD_FILTER_HPP

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace oap {

/// Number of bands in a standard graphic EQ
constexpr int kNumBands = 10;

/// ISO standard center frequencies for 10-band graphic EQ (Hz)
constexpr float kCenterFreqs[kNumBands] = {
    31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

/// Default Q for graphic EQ bands (sqrt(2))
constexpr float kGraphicEqQ = 1.4142f;

/// Normalized biquad filter coefficients (a0 already divided out)
struct BiquadCoeffs {
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;

    /// Return unity (passthrough) coefficients
    static BiquadCoeffs unity()
    {
        return {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    }
};

/// Per-channel biquad filter state (Direct Form II Transposed)
struct BiquadState {
    float z1 = 0.0f;
    float z2 = 0.0f;

    void reset()
    {
        z1 = 0.0f;
        z2 = 0.0f;
    }
};

/// Compute RBJ Audio EQ Cookbook peaking EQ coefficients
///
/// @param freqHz  Center frequency in Hz
/// @param gainDb  Gain at center frequency in dB (positive = boost, negative = cut)
/// @param Q       Quality factor (bandwidth control)
/// @param sampleRate  Sample rate in Hz
/// @return Normalized biquad coefficients
inline BiquadCoeffs calcPeakingEQ(float freqHz, float gainDb, float Q, float sampleRate)
{
    float A = std::pow(10.0f, gainDb / 40.0f);
    float w0 = 2.0f * static_cast<float>(M_PI) * freqHz / sampleRate;
    float sinW0 = std::sin(w0);
    float cosW0 = std::cos(w0);
    float alpha = sinW0 / (2.0f * Q);

    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cosW0;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cosW0;
    float a2 = 1.0f - alpha / A;

    // Normalize by a0
    float invA0 = 1.0f / a0;
    return {
        b0 * invA0,
        b1 * invA0,
        b2 * invA0,
        a1 * invA0,
        a2 * invA0
    };
}

/// Process a single sample through a biquad filter (Direct Form II Transposed)
///
/// @param x  Input sample
/// @param c  Filter coefficients
/// @param s  Filter state (modified in place)
/// @return Filtered output sample
inline float processSample(float x, const BiquadCoeffs& c, BiquadState& s)
{
    float y = c.b0 * x + s.z1;
    s.z1 = c.b1 * x - c.a1 * y + s.z2;
    s.z2 = c.b2 * x - c.a2 * y;
    return y;
}

} // namespace oap

#endif // OAP_BIQUAD_FILTER_HPP
