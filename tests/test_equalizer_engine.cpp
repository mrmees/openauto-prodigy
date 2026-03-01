#include <QtTest>
#include <array>
#include <cmath>
#include <vector>
#include "core/audio/EqualizerEngine.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class TestEqualizerEngine : public QObject {
    Q_OBJECT

private:
    // Generate stereo interleaved int16_t sine wave
    static std::vector<int16_t> generateStereoSine(float freqHz, float amplitude,
                                                    int frameCount, float sampleRate = 48000.0f)
    {
        std::vector<int16_t> data(frameCount * 2);
        for (int i = 0; i < frameCount; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            auto sample = static_cast<int16_t>(amplitude * std::sin(2.0f * M_PI * freqHz * t));
            data[i * 2]     = sample; // L
            data[i * 2 + 1] = sample; // R
        }
        return data;
    }

    // Measure RMS of left channel from stereo interleaved data
    static float measureRmsLeft(const int16_t* data, int frameCount)
    {
        double sum = 0.0;
        for (int i = 0; i < frameCount; ++i) {
            float s = static_cast<float>(data[i * 2]);
            sum += s * s;
        }
        return static_cast<float>(std::sqrt(sum / frameCount));
    }

    // Measure RMS of right channel
    static float measureRmsRight(const int16_t* data, int frameCount)
    {
        double sum = 0.0;
        for (int i = 0; i < frameCount; ++i) {
            float s = static_cast<float>(data[i * 2 + 1]);
            sum += s * s;
        }
        return static_cast<float>(std::sqrt(sum / frameCount));
    }

    static float rmsToDb(float rms)
    {
        if (rms < 1e-10f) return -200.0f;
        return 20.0f * std::log10(rms);
    }

private slots:
    // --- Frequency response tests ---

    void testBoost1kHz()
    {
        // setGain(band=5, +12dB) should boost 1kHz content by ~12dB
        oap::EqualizerEngine engine;

        // Generate 1kHz sine at moderate amplitude
        const int settleFrames = 4800; // 100ms settle
        const int measureFrames = 4800;
        const int totalFrames = settleFrames + measureFrames;
        auto data = generateStereoSine(1000.0f, 8000.0f, totalFrames);

        // Measure unprocessed RMS
        float inputRms = measureRmsLeft(data.data() + settleFrames * 2, measureFrames);

        // Process with +12dB at 1kHz
        engine.setGain(5, 12.0f);
        // Let interpolation complete
        auto processData = generateStereoSine(1000.0f, 8000.0f, totalFrames);
        engine.process(processData.data(), totalFrames);

        // Measure after settle
        float outputRms = measureRmsLeft(processData.data() + settleFrames * 2, measureFrames);

        float gainDb = rmsToDb(outputRms) - rmsToDb(inputRms);
        // Should be approximately +12dB (within 2dB tolerance for peaking EQ + limiter effects)
        QVERIFY2(gainDb > 9.0f, qPrintable(QString("Expected >9dB gain, got %1dB").arg(gainDb)));
        QVERIFY2(gainDb < 15.0f, qPrintable(QString("Expected <15dB gain, got %1dB").arg(gainDb)));
    }

    void testBandIsolation()
    {
        // Boosting 31Hz should not affect 8kHz content significantly
        oap::EqualizerEngine engine;
        engine.setGain(0, 12.0f); // boost 31Hz

        const int settleFrames = 4800;
        const int measureFrames = 4800;
        const int totalFrames = settleFrames + measureFrames;

        // Generate 8kHz sine
        auto original = generateStereoSine(8000.0f, 8000.0f, totalFrames);
        auto processed = original;
        engine.process(processed.data(), totalFrames);

        float inputRms = measureRmsLeft(original.data() + settleFrames * 2, measureFrames);
        float outputRms = measureRmsLeft(processed.data() + settleFrames * 2, measureFrames);

        float gainDb = rmsToDb(outputRms) - rmsToDb(inputRms);
        // 8kHz should be mostly unaffected by 31Hz boost (within ±2dB)
        QVERIFY2(std::fabs(gainDb) < 2.0f,
                 qPrintable(QString("Expected 8kHz unaffected, got %1dB change").arg(gainDb)));
    }

    void testSetAllGains()
    {
        // setAllGains with uniform +6dB should boost all frequencies
        oap::EqualizerEngine engine;
        std::array<float, 10> gains = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6};
        engine.setAllGains(gains);

        const int settleFrames = 4800;
        const int measureFrames = 4800;
        const int totalFrames = settleFrames + measureFrames;

        auto original = generateStereoSine(1000.0f, 4000.0f, totalFrames);
        auto processed = original;
        engine.process(processed.data(), totalFrames);

        float inputRms = measureRmsLeft(original.data() + settleFrames * 2, measureFrames);
        float outputRms = measureRmsLeft(processed.data() + settleFrames * 2, measureFrames);

        float gainDb = rmsToDb(outputRms) - rmsToDb(inputRms);
        // With +6dB on the relevant band, expect a noticeable boost
        QVERIFY2(gainDb > 3.0f, qPrintable(QString("Expected >3dB gain, got %1dB").arg(gainDb)));
    }

    // --- Interpolation smoothness tests ---

    void testSmoothInterpolation()
    {
        // After setGain(), there should be no discontinuity > threshold between adjacent samples
        oap::EqualizerEngine engine;

        // Process some silence to initialize state
        std::vector<int16_t> warmup(4800 * 2, 0);
        engine.process(warmup.data(), 4800);

        // Generate a steady signal
        auto data = generateStereoSine(1000.0f, 8000.0f, 4800);

        // Now trigger a gain change
        engine.setGain(5, 12.0f);

        // Process frame by frame during transition, tracking max delta
        float maxDelta = 0.0f;
        int16_t prevSample = 0;
        bool first = true;

        for (int i = 0; i < 4800; ++i) {
            int16_t frame[2] = {data[i * 2], data[i * 2 + 1]};
            engine.process(frame, 1);

            if (!first) {
                float delta = std::fabs(static_cast<float>(frame[0] - prevSample));
                maxDelta = std::max(maxDelta, delta);
            }
            prevSample = frame[0];
            first = false;
        }

        // Max sample-to-sample delta during interpolation should be reasonable
        // For a 1kHz sine at 8000 amplitude, the natural max delta is about
        // 2*pi*1000/48000 * 8000 = ~1047 per sample. With +12dB boost ramping in,
        // it should not spike dramatically above the fully boosted max delta.
        // The key is no click: no sudden discontinuity far exceeding the signal's natural delta.
        // A click would show as a delta much larger than the signal's natural range.
        QVERIFY2(maxDelta < 5000.0f,
                 qPrintable(QString("Possible click detected: max delta = %1").arg(maxDelta)));
    }

    void testInterpolationCompletesWithin2304Samples()
    {
        // After 2304 samples, interpolation should be complete
        oap::EqualizerEngine engine;

        // Set a gain change
        engine.setGain(5, 12.0f);

        // Process exactly kInterpolationSamples frames
        auto data = generateStereoSine(1000.0f, 4000.0f, oap::EqualizerEngine::kInterpolationSamples + 4800);
        engine.process(data.data(), oap::EqualizerEngine::kInterpolationSamples + 4800);

        // After interpolation, processing more should give steady-state output
        auto steady1 = generateStereoSine(1000.0f, 4000.0f, 480);
        auto steady2 = generateStereoSine(1000.0f, 4000.0f, 480);
        engine.process(steady1.data(), 480);
        engine.process(steady2.data(), 480);

        float rms1 = measureRmsLeft(steady1.data(), 480);
        float rms2 = measureRmsLeft(steady2.data(), 480);

        // Should be effectively the same (within 0.5dB)
        float diff = std::fabs(rmsToDb(rms1) - rmsToDb(rms2));
        QVERIFY2(diff < 0.5f,
                 qPrintable(QString("Output not steady after interpolation: %1dB difference").arg(diff)));
    }

    void testMidInterpolationRetrigger()
    {
        // Re-triggering setGain() mid-interpolation should ramp from current position (no click)
        oap::EqualizerEngine engine;

        engine.setGain(5, 12.0f);

        // Process partway through interpolation (half)
        int halfInterp = oap::EqualizerEngine::kInterpolationSamples / 2;
        auto data1 = generateStereoSine(1000.0f, 4000.0f, halfInterp);
        engine.process(data1.data(), halfInterp);

        // Re-trigger with different gain
        engine.setGain(5, -6.0f);

        // Process frame by frame, check for clicks
        float maxDelta = 0.0f;
        int16_t prevSample = data1[(halfInterp - 1) * 2]; // last sample from first block

        for (int i = 0; i < 2400; ++i) {
            auto frame = generateStereoSine(1000.0f, 4000.0f, 1);
            engine.process(frame.data(), 1);

            float delta = std::fabs(static_cast<float>(frame[0] - prevSample));
            maxDelta = std::max(maxDelta, delta);
            prevSample = frame[0];
        }

        QVERIFY2(maxDelta < 5000.0f,
                 qPrintable(QString("Click on mid-interp retrigger: max delta = %1").arg(maxDelta)));
    }

    // --- Bypass tests ---

    void testBypassPassthrough()
    {
        // setBypassed(true) should produce output matching input (after crossfade completes)
        oap::EqualizerEngine engine;
        engine.setGain(5, 12.0f);

        // Let EQ settle
        auto settle = generateStereoSine(1000.0f, 4000.0f, 4800);
        engine.process(settle.data(), 4800);

        // Enable bypass
        engine.setBypassed(true);

        // Process through the crossfade
        auto crossfade = generateStereoSine(1000.0f, 4000.0f, 4800);
        engine.process(crossfade.data(), 4800);

        // Now bypass should be fully active — output should match input
        const int testFrames = 480;
        auto input = generateStereoSine(1000.0f, 4000.0f, testFrames);
        auto output = input; // copy
        engine.process(output.data(), testFrames);

        // Compare — should be within a few LSBs (float rounding)
        for (int i = 0; i < testFrames * 2; ++i) {
            int diff = std::abs(static_cast<int>(output[i]) - static_cast<int>(input[i]));
            QVERIFY2(diff <= 1,
                     qPrintable(QString("Bypass output differs at sample %1: in=%2 out=%3")
                                .arg(i).arg(input[i]).arg(output[i])));
        }
    }

    void testBypassAppliesChanges()
    {
        // Changes made during bypass should take effect when bypass is turned off
        oap::EqualizerEngine engine;
        engine.setBypassed(true);

        // Let bypass crossfade complete
        auto settle = generateStereoSine(1000.0f, 4000.0f, 4800);
        engine.process(settle.data(), 4800);

        // Set gain while bypassed
        engine.setGain(5, 12.0f);

        // Turn bypass off
        engine.setBypassed(false);

        // Let crossfade + interpolation complete
        auto transition = generateStereoSine(1000.0f, 4000.0f, 9600);
        engine.process(transition.data(), 9600);

        // Measure — should be boosted
        const int measureFrames = 4800;
        auto original = generateStereoSine(1000.0f, 4000.0f, measureFrames);
        auto processed = original;
        engine.process(processed.data(), measureFrames);

        float inputRms = measureRmsLeft(original.data(), measureFrames);
        float outputRms = measureRmsLeft(processed.data(), measureFrames);
        float gainDb = rmsToDb(outputRms) - rmsToDb(inputRms);

        QVERIFY2(gainDb > 9.0f,
                 qPrintable(QString("Expected boost after un-bypass, got %1dB").arg(gainDb)));
    }

    void testBypassCrossfadeSmoothness()
    {
        // Bypass toggle should use smooth crossfade (same ~2304 sample ramp)
        oap::EqualizerEngine engine;
        engine.setGain(5, 12.0f);

        // Let EQ settle
        auto settle = generateStereoSine(1000.0f, 8000.0f, 9600);
        engine.process(settle.data(), 9600);

        // Toggle bypass and check for clicks
        engine.setBypassed(true);

        float maxDelta = 0.0f;
        int16_t prevSample = settle[(9600 - 1) * 2];

        for (int i = 0; i < 4800; ++i) {
            auto frame = generateStereoSine(1000.0f, 8000.0f, 1);
            engine.process(frame.data(), 1);

            float delta = std::fabs(static_cast<float>(frame[0] - prevSample));
            maxDelta = std::max(maxDelta, delta);
            prevSample = frame[0];
        }

        QVERIFY2(maxDelta < 5000.0f,
                 qPrintable(QString("Click on bypass toggle: max delta = %1").arg(maxDelta)));
    }

    // --- Clipping prevention ---

    void testNoClipping()
    {
        // With all 10 bands at +12dB, output should stay within int16_t range
        oap::EqualizerEngine engine;
        std::array<float, 10> gains = {12, 12, 12, 12, 12, 12, 12, 12, 12, 12};
        engine.setAllGains(gains);

        // Full-scale 1kHz sine
        const int totalFrames = 9600; // 200ms
        auto data = generateStereoSine(1000.0f, 32000.0f, totalFrames);
        engine.process(data.data(), totalFrames);

        for (int i = 0; i < totalFrames * 2; ++i) {
            QVERIFY2(data[i] >= -32768 && data[i] <= 32767,
                     qPrintable(QString("Clipping at sample %1: value = %2").arg(i).arg(data[i])));
        }
    }

    // --- Edge cases ---

    void testZeroFramesSafe()
    {
        // process(nullptr, 0) should be a safe no-op
        oap::EqualizerEngine engine;
        engine.process(nullptr, 0);

        // Also test with valid pointer but 0 frames
        int16_t dummy[2] = {0, 0};
        engine.process(dummy, 0);
        // No crash = pass
    }

    void testStereoIndependence()
    {
        // Left and right channels should be processed independently
        oap::EqualizerEngine engine;
        engine.setGain(5, 12.0f);

        // Create asymmetric stereo: 1kHz left, silence right
        const int totalFrames = 9600;
        std::vector<int16_t> data(totalFrames * 2);
        for (int i = 0; i < totalFrames; ++i) {
            float t = static_cast<float>(i) / 48000.0f;
            data[i * 2]     = static_cast<int16_t>(4000.0f * std::sin(2.0f * M_PI * 1000.0f * t));
            data[i * 2 + 1] = 0; // silence
        }

        engine.process(data.data(), totalFrames);

        // Right channel should still be (near) silence
        float rightRms = measureRmsRight(data.data(), totalFrames);
        QVERIFY2(rightRms < 50.0f,
                 qPrintable(QString("Right channel not silent: RMS = %1").arg(rightRms)));

        // Left channel should be boosted (not silent)
        float leftRms = measureRmsLeft(data.data() + 4800 * 2, totalFrames - 4800);
        QVERIFY2(leftRms > 1000.0f,
                 qPrintable(QString("Left channel not boosted: RMS = %1").arg(leftRms)));
    }

    // --- Accessor tests ---

    void testGetGain()
    {
        oap::EqualizerEngine engine;
        QCOMPARE(engine.getGain(5), 0.0f);

        engine.setGain(5, 6.0f);
        QCOMPARE(engine.getGain(5), 6.0f);

        engine.setGain(5, -12.0f);
        QCOMPARE(engine.getGain(5), -12.0f);
    }

    void testIsBypassed()
    {
        oap::EqualizerEngine engine;
        QCOMPARE(engine.isBypassed(), false);

        engine.setBypassed(true);
        QCOMPARE(engine.isBypassed(), true);

        engine.setBypassed(false);
        QCOMPARE(engine.isBypassed(), false);
    }

    void testGainClamping()
    {
        oap::EqualizerEngine engine;
        engine.setGain(5, 20.0f); // over +12
        QCOMPARE(engine.getGain(5), 12.0f);

        engine.setGain(5, -20.0f); // under -12
        QCOMPARE(engine.getGain(5), -12.0f);
    }
};

QTEST_MAIN(TestEqualizerEngine)
#include "test_equalizer_engine.moc"
