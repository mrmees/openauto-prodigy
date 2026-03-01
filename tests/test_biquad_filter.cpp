#include <QtTest>
#include <cmath>
#include "core/audio/BiquadFilter.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class TestBiquadFilter : public QObject {
    Q_OBJECT

private:
    static constexpr float kSampleRate = 48000.0f;
    static constexpr int kSettleSamples = 4096;
    static constexpr int kMeasureSamples = 1024;
    static constexpr int kTotalSamples = kSettleSamples + kMeasureSamples;

    // Generate sine wave, process through filter, return gain in dB
    float measureGainDb(const oap::BiquadCoeffs& coeffs, float freqHz)
    {
        oap::BiquadState state;

        // Compute input and output RMS over measurement window
        double inSumSq = 0.0;
        double outSumSq = 0.0;

        for (int n = 0; n < kTotalSamples; ++n) {
            float x = std::sin(2.0f * static_cast<float>(M_PI) * freqHz * n / kSampleRate);
            float y = oap::processSample(x, coeffs, state);

            if (n >= kSettleSamples) {
                inSumSq += static_cast<double>(x) * x;
                outSumSq += static_cast<double>(y) * y;
            }
        }

        double inRms = std::sqrt(inSumSq / kMeasureSamples);
        double outRms = std::sqrt(outSumSq / kMeasureSamples);

        if (inRms < 1e-10) return 0.0f;
        return static_cast<float>(20.0 * std::log10(outRms / inRms));
    }

private slots:
    void testNonTrivialCoefficients()
    {
        auto c = oap::calcPeakingEQ(1000.0f, 6.0f, oap::kGraphicEqQ, kSampleRate);
        // Non-trivial: b0 should not be 1, a1 and a2 should not be 0
        QVERIFY(std::fabs(c.b0 - 1.0f) > 0.001f);
        QVERIFY(std::fabs(c.a1) > 0.001f);
        QVERIFY(std::fabs(c.a2) > 0.001f);
    }

    void testZeroGainIsPassthrough()
    {
        auto c = oap::calcPeakingEQ(1000.0f, 0.0f, oap::kGraphicEqQ, kSampleRate);
        // At 0 dB gain, peaking EQ should be identity
        QVERIFY(std::fabs(c.b0 - 1.0f) < 0.001f);
        QVERIFY(std::fabs(c.b1 - c.a1) < 0.001f);
        QVERIFY(std::fabs(c.b2 - c.a2) < 0.001f);
    }

    void testBoostAtCenterFrequency()
    {
        // +12dB peaking at 1kHz, measure at 1kHz
        auto c = oap::calcPeakingEQ(1000.0f, 12.0f, oap::kGraphicEqQ, kSampleRate);
        float gain = measureGainDb(c, 1000.0f);
        // Should be within 1dB of +12
        QVERIFY2(std::fabs(gain - 12.0f) < 1.0f,
                 qPrintable(QString("Expected ~12dB, got %1dB").arg(gain)));
    }

    void testUnityAwayFromCenter()
    {
        // +12dB peaking at 100Hz, measure at 1kHz (far away)
        auto c = oap::calcPeakingEQ(100.0f, 12.0f, oap::kGraphicEqQ, kSampleRate);
        float gain = measureGainDb(c, 1000.0f);
        // Should be approximately 0dB (unity) -- within 1dB
        QVERIFY2(std::fabs(gain) < 1.0f,
                 qPrintable(QString("Expected ~0dB, got %1dB").arg(gain)));
    }

    void testBoostAtLowFrequency()
    {
        // +12dB peaking at 100Hz, measure at 100Hz
        auto c = oap::calcPeakingEQ(100.0f, 12.0f, oap::kGraphicEqQ, kSampleRate);
        float gain = measureGainDb(c, 100.0f);
        // Should be within 1dB of +12
        QVERIFY2(std::fabs(gain - 12.0f) < 1.0f,
                 qPrintable(QString("Expected ~12dB, got %1dB").arg(gain)));
    }

    void testStereoIndependence()
    {
        auto c = oap::calcPeakingEQ(1000.0f, 6.0f, oap::kGraphicEqQ, kSampleRate);
        oap::BiquadState stateL;
        oap::BiquadState stateR;

        // Process identical input through two independent states
        for (int n = 0; n < 1000; ++n) {
            float x = std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * n / kSampleRate);
            float yL = oap::processSample(x, c, stateL);
            float yR = oap::processSample(x, c, stateR);
            QCOMPARE(yL, yR);
        }
    }

    void testStatePersistence()
    {
        auto c = oap::calcPeakingEQ(1000.0f, 6.0f, oap::kGraphicEqQ, kSampleRate);
        oap::BiquadState state;

        // Process some samples
        for (int n = 0; n < 100; ++n) {
            float x = std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * n / kSampleRate);
            oap::processSample(x, c, state);
        }

        // State should be non-zero after processing
        QVERIFY(state.z1 != 0.0f || state.z2 != 0.0f);
    }

    void testResetState()
    {
        auto c = oap::calcPeakingEQ(1000.0f, 6.0f, oap::kGraphicEqQ, kSampleRate);
        oap::BiquadState state;

        // Process some samples to populate state
        for (int n = 0; n < 100; ++n) {
            oap::processSample(0.5f, c, state);
        }

        state.reset();
        QCOMPARE(state.z1, 0.0f);
        QCOMPARE(state.z2, 0.0f);
    }

    void testUnityCoeffsFactory()
    {
        auto c = oap::BiquadCoeffs::unity();
        QCOMPARE(c.b0, 1.0f);
        QCOMPARE(c.b1, 0.0f);
        QCOMPARE(c.b2, 0.0f);
        QCOMPARE(c.a1, 0.0f);
        QCOMPARE(c.a2, 0.0f);
    }

    void testUnityPassthrough()
    {
        // Unity coefficients should pass signal through unchanged
        auto c = oap::BiquadCoeffs::unity();
        oap::BiquadState state;

        for (int n = 0; n < 100; ++n) {
            float x = std::sin(2.0f * static_cast<float>(M_PI) * 440.0f * n / kSampleRate);
            float y = oap::processSample(x, c, state);
            QVERIFY(std::fabs(y - x) < 1e-6f);
        }
    }

    void testCenterFrequenciesArray()
    {
        QCOMPARE(oap::kNumBands, 10);
        QCOMPARE(oap::kCenterFreqs[0], 31.0f);
        QCOMPARE(oap::kCenterFreqs[9], 16000.0f);
    }
};

QTEST_MAIN(TestBiquadFilter)
#include "test_biquad_filter.moc"
