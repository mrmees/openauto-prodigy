#include <QtTest>
#include <cmath>
#include "core/audio/SoftLimiter.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class TestSoftLimiter : public QObject {
    Q_OBJECT

private:
    static constexpr float kSampleRate = 48000.0f;

private slots:
    void testTransparentBelowThreshold()
    {
        oap::SoftLimiter limiter(kSampleRate);

        // -6dBFS = amplitude 0.5
        const float amplitude = 0.5f;
        const int settle = 4800; // 100ms settle
        const int measure = 4800;

        // Settle the envelope
        for (int n = 0; n < settle; ++n) {
            float x = amplitude * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * n / kSampleRate);
            limiter.process(x);
        }

        // Measure: output should be approximately equal to input
        float maxError = 0.0f;
        for (int n = settle; n < settle + measure; ++n) {
            float x = amplitude * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * n / kSampleRate);
            float y = limiter.process(x);
            float error = std::fabs(y - x);
            if (error > maxError) maxError = error;
        }

        QVERIFY2(maxError < 0.01f,
                 qPrintable(QString("Max error %1, expected < 0.01").arg(maxError)));
    }

    void testLimitsAboveThreshold()
    {
        oap::SoftLimiter limiter(kSampleRate);

        // +6dBFS = amplitude 2.0
        const float amplitude = 2.0f;
        const int settle = 4800; // 100ms for envelope to settle
        const int measure = 4800;

        // Let the envelope settle on the loud signal
        for (int n = 0; n < settle; ++n) {
            float x = amplitude * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * n / kSampleRate);
            limiter.process(x);
        }

        // After settling, measure peak output
        float maxOutput = 0.0f;
        for (int n = settle; n < settle + measure; ++n) {
            float x = amplitude * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * n / kSampleRate);
            float y = limiter.process(x);
            float absY = std::fabs(y);
            if (absY > maxOutput) maxOutput = absY;
        }

        // After envelope settles, peak should be limited to ~1.0
        // Allow small overshoot from envelope follower (up to 1.05)
        QVERIFY2(maxOutput <= 1.05f,
                 qPrintable(QString("Max output %1, expected <= 1.05").arg(maxOutput)));
    }

    void testFastAttack()
    {
        oap::SoftLimiter limiter(kSampleRate);

        // Feed silence, then a sudden +12dB spike (amplitude 4.0)
        for (int n = 0; n < 480; ++n) {
            limiter.process(0.0f);
        }

        // Now send loud signal — check that limiting kicks in within ~1ms (48 samples)
        float firstLoudOutput = 0.0f;
        bool limitingActive = false;
        int samplesUntilLimiting = 0;

        for (int n = 0; n < 480; ++n) {
            float x = 4.0f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * n / kSampleRate);
            float y = limiter.process(x);

            if (n == 0) firstLoudOutput = std::fabs(y);

            // Check if output is being limited (output < input magnitude)
            if (std::fabs(x) > 1.0f && std::fabs(y) < std::fabs(x) * 0.9f) {
                if (!limitingActive) {
                    limitingActive = true;
                    samplesUntilLimiting = n;
                }
            }
        }

        // First sample may pass through (envelope starts at 0)
        // But within 48 samples (1ms), limiting should be active
        QVERIFY2(limitingActive,
                 "Limiting never became active");
        QVERIFY2(samplesUntilLimiting < 48,
                 qPrintable(QString("Took %1 samples, expected < 48").arg(samplesUntilLimiting)));
    }

    void testReleaseRecovery()
    {
        oap::SoftLimiter limiter(kSampleRate);

        // Feed loud signal to engage limiter
        for (int n = 0; n < 4800; ++n) { // 100ms loud
            float x = 2.0f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * n / kSampleRate);
            limiter.process(x);
        }

        // Now feed quiet signal and check recovery
        const float quietAmp = 0.3f;
        bool recovered = false;
        int recoverySamples = 0;

        for (int n = 0; n < 9600; ++n) { // 200ms quiet
            float x = quietAmp * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * n / kSampleRate);
            float y = limiter.process(x);

            // Check if gain has returned to near unity (output ~= input)
            if (std::fabs(x) > 0.1f && std::fabs(y / x - 1.0f) < 0.05f) {
                if (!recovered) {
                    recovered = true;
                    recoverySamples = n;
                }
            }
        }

        QVERIFY2(recovered, "Limiter never recovered to unity gain");
        // Should recover within ~100ms (4800 samples)
        QVERIFY2(recoverySamples < 4800,
                 qPrintable(QString("Recovery took %1 samples, expected < 4800").arg(recoverySamples)));
    }

    void testReset()
    {
        oap::SoftLimiter limiter(kSampleRate);

        // Engage the limiter
        for (int n = 0; n < 4800; ++n) {
            limiter.process(3.0f);
        }

        limiter.reset();

        // After reset, a quiet signal should pass through unchanged
        float x = 0.5f;
        float y = limiter.process(x);
        QVERIFY(std::fabs(y - x) < 0.01f);
    }

    void testPolarityPreservation()
    {
        oap::SoftLimiter limiter(kSampleRate);

        // Engage limiter with loud signal
        for (int n = 0; n < 4800; ++n) {
            float x = 2.0f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * n / kSampleRate);
            limiter.process(x);
        }

        // Check that positive input gives positive output and vice versa
        float yPos = limiter.process(2.0f);
        float yNeg = limiter.process(-2.0f);

        QVERIFY2(yPos > 0.0f, qPrintable(QString("Positive input gave %1").arg(yPos)));
        QVERIFY2(yNeg < 0.0f, qPrintable(QString("Negative input gave %1").arg(yNeg)));
    }

    void testPerChannelIndependence()
    {
        oap::SoftLimiter limiterL(kSampleRate);
        oap::SoftLimiter limiterR(kSampleRate);

        // Feed loud signal only to L
        for (int n = 0; n < 4800; ++n) {
            limiterL.process(3.0f);
            limiterR.process(0.1f);
        }

        // L should be limiting, R should not
        float yL = limiterL.process(2.0f);
        float yR = limiterR.process(0.1f);

        // L is being limited (output < input)
        QVERIFY2(yL < 2.0f,
                 qPrintable(QString("L output %1, expected < 2.0").arg(yL)));
        // R is not being limited (output ~= input)
        QVERIFY2(std::fabs(yR - 0.1f) < 0.01f,
                 qPrintable(QString("R output %1, expected ~0.1").arg(yR)));
    }
};

QTEST_MAIN(TestSoftLimiter)
#include "test_soft_limiter.moc"
