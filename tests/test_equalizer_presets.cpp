#include <QtTest>
#include "core/audio/EqualizerPresets.hpp"

class TestEqualizerPresets : public QObject {
    Q_OBJECT

private slots:
    void testBundledPresetCount()
    {
        QCOMPARE(oap::kBundledPresetCount, 9);
    }

    void testBundledPresetNames()
    {
        const char* expected[] = {
            "Flat", "Rock", "Pop", "Jazz", "Classical",
            "Bass Boost", "Treble Boost", "Vocal", "Voice"
        };
        for (int i = 0; i < 9; ++i) {
            QCOMPARE(QString(oap::kBundledPresets[i].name), QString(expected[i]));
        }
    }

    void testEachPresetHas10Gains()
    {
        for (int i = 0; i < oap::kBundledPresetCount; ++i) {
            QCOMPARE(static_cast<int>(oap::kBundledPresets[i].gains.size()), oap::kNumBands);
        }
    }

    void testAllGainsWithinRange()
    {
        for (int i = 0; i < oap::kBundledPresetCount; ++i) {
            for (int b = 0; b < oap::kNumBands; ++b) {
                float g = oap::kBundledPresets[i].gains[b];
                QVERIFY2(g >= -12.0f && g <= 12.0f,
                    qPrintable(QString("Preset %1 band %2 gain %3 out of range")
                        .arg(oap::kBundledPresets[i].name).arg(b).arg(g)));
            }
        }
    }

    void testFlatPresetAllZeros()
    {
        const auto* flat = oap::findBundledPreset("Flat");
        QVERIFY(flat != nullptr);
        for (int b = 0; b < oap::kNumBands; ++b) {
            QCOMPARE(flat->gains[b], 0.0f);
        }
    }

    void testFindBundledPresetExists()
    {
        const auto* rock = oap::findBundledPreset("Rock");
        QVERIFY(rock != nullptr);
        QCOMPARE(QString(rock->name), QString("Rock"));
    }

    void testFindBundledPresetNotFound()
    {
        const auto* nope = oap::findBundledPreset("Nonexistent");
        QVERIFY(nope == nullptr);
    }

    void testVoicePresetShape()
    {
        const auto* voice = oap::findBundledPreset("Voice");
        QVERIFY(voice != nullptr);

        // Voice should cut low end (bands 0-1 negative)
        QVERIFY(voice->gains[0] < 0.0f);
        QVERIFY(voice->gains[1] < 0.0f);

        // Voice should boost 1-4kHz range (bands 5-7 positive)
        QVERIFY(voice->gains[5] > 0.0f);
        QVERIFY(voice->gains[6] > 0.0f);
        QVERIFY(voice->gains[7] > 0.0f);
    }

    void testDefaultPresetIndices()
    {
        QCOMPARE(oap::kDefaultMediaPresetIndex, 0);
        QCOMPARE(oap::kDefaultVoicePresetIndex, 8);
        QCOMPARE(QString(oap::kBundledPresets[oap::kDefaultMediaPresetIndex].name), QString("Flat"));
        QCOMPARE(QString(oap::kBundledPresets[oap::kDefaultVoicePresetIndex].name), QString("Voice"));
    }
};

QTEST_MAIN(TestEqualizerPresets)
#include "test_equalizer_presets.moc"
