#include <QTest>
#include "core/audio/FocusGain.hpp"

#include <vector>

/// Tests for the RT-safe focus gain helper — the sample-level half of audio
/// focus arbitration. applyDucking() computes per-stream target gains; the
/// playback process callback applies them here with a click-free ramp.
class TestFocusGain : public QObject {
    Q_OBJECT

    static std::vector<int16_t> constantFrames(int frames, int channels, int16_t value)
    {
        return std::vector<int16_t>(static_cast<size_t>(frames * channels), value);
    }

private slots:
    void unityGainLeavesSamplesUntouched()
    {
        auto samples = constantFrames(64, 2, 10000);
        const float out = oap::applyFocusGain(samples.data(), 64, 2, 1.0f, 1.0f);
        QCOMPARE(out, 1.0f);
        for (int16_t s : samples)
            QCOMPARE(s, int16_t(10000));
    }

    void steadyDuckScalesSamples()
    {
        auto samples = constantFrames(64, 2, 10000);
        const float out = oap::applyFocusGain(samples.data(), 64, 2, 0.2f, 0.2f);
        QCOMPARE(out, 0.2f);
        for (int16_t s : samples)
            QCOMPARE(s, int16_t(2000));
    }

    void steadyMuteSilencesSamples()
    {
        auto samples = constantFrames(64, 2, 10000);
        const float out = oap::applyFocusGain(samples.data(), 64, 2, 0.0f, 0.0f);
        QCOMPARE(out, 0.0f);
        for (int16_t s : samples)
            QCOMPARE(s, int16_t(0));
    }

    void reachesTargetWithinRampWindow()
    {
        // Full-scale swing must complete within the ramp window (~20ms @ 48k).
        // +2: float truncation in the frame count plus the final clamp step.
        const int rampFrames = static_cast<int>(1.0f / oap::kFocusGainRampPerFrame) + 2;
        auto samples = constantFrames(rampFrames, 2, 10000);
        const float out = oap::applyFocusGain(samples.data(), rampFrames, 2, 1.0f, 0.0f);
        QCOMPARE(out, 0.0f);
        // Last frame is fully muted, first frame is barely attenuated (no click).
        QCOMPARE(samples[static_cast<size_t>((rampFrames - 1) * 2)], int16_t(0));
        QVERIFY(samples[0] > 9000);
    }

    void rampIsMonotonicOnTheWayDown()
    {
        auto samples = constantFrames(480, 1, 10000);
        oap::applyFocusGain(samples.data(), 480, 1, 1.0f, 0.2f);
        for (size_t i = 1; i < samples.size(); ++i)
            QVERIFY2(samples[i] <= samples[i - 1], "duck ramp must not overshoot upward");
        // 480 frames at 1/960 per frame moves gain 0.5 down: 1.0 -> 0.5.
        QVERIFY(samples.back() < 6000);
        QVERIFY(samples.back() > 4000);
    }

    void rampsBackUpToUnityAndStopsScaling()
    {
        const int rampFrames = static_cast<int>(1.0f / oap::kFocusGainRampPerFrame) + 2;
        auto samples = constantFrames(rampFrames, 2, 10000);
        const float out = oap::applyFocusGain(samples.data(), rampFrames, 2, 0.0f, 1.0f);
        QCOMPARE(out, 1.0f);
        // Final frame back at unity — bit-exact original value.
        QCOMPARE(samples[static_cast<size_t>((rampFrames - 1) * 2)], int16_t(10000));
        // First frame still ~muted (came out of a mute, no click).
        QVERIFY(samples[0] < 1000);
    }

    void gainStatePersistsAcrossCalls()
    {
        // Two consecutive callbacks: the second continues where the first left off.
        auto first = constantFrames(240, 2, 10000);
        const float mid = oap::applyFocusGain(first.data(), 240, 2, 1.0f, 0.0f);
        QVERIFY(mid > 0.0f);
        QVERIFY(mid < 1.0f);

        auto second = constantFrames(960, 2, 10000);
        const float done = oap::applyFocusGain(second.data(), 960, 2, mid, 0.0f);
        QCOMPARE(done, 0.0f);
        QCOMPARE(second[second.size() - 1], int16_t(0));
    }
};

QTEST_MAIN(TestFocusGain)
#include "test_focus_gain.moc"
