#include <QtTest>
#include "core/services/EqualizerService.hpp"
#include "core/audio/EqualizerPresets.hpp"

using oap::StreamId;

class TestEqualizerService : public QObject {
    Q_OBJECT

private slots:
    void testDefaultPresets()
    {
        oap::EqualizerService svc;
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Flat"));
        QCOMPARE(svc.activePreset(StreamId::Navigation), QString("Voice"));
        QCOMPARE(svc.activePreset(StreamId::Phone), QString("Voice"));
    }

    void testApplyPresetChangesOnlyTargetStream()
    {
        oap::EqualizerService svc;
        svc.applyPreset(StreamId::Media, "Rock");
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Rock"));
        // Others unchanged
        QCOMPARE(svc.activePreset(StreamId::Navigation), QString("Voice"));
        QCOMPARE(svc.activePreset(StreamId::Phone), QString("Voice"));

        // Verify gains match Rock preset
        const auto* rock = oap::findBundledPreset("Rock");
        auto gains = svc.gainsForStream(StreamId::Media);
        for (int b = 0; b < oap::kNumBands; ++b) {
            QCOMPARE(gains[b], rock->gains[b]);
        }
    }

    void testApplyNonexistentPresetFallsBackToFlat()
    {
        oap::EqualizerService svc;
        svc.applyPreset(StreamId::Media, "Rock");
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Rock"));

        svc.applyPreset(StreamId::Media, "DoesNotExist");
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Flat"));

        auto gains = svc.gainsForStream(StreamId::Media);
        for (int b = 0; b < oap::kNumBands; ++b) {
            QCOMPARE(gains[b], 0.0f);
        }
    }

    void testSetGainSingleBand()
    {
        oap::EqualizerService svc;
        svc.setGain(StreamId::Media, 3, 5.0f);
        QCOMPARE(svc.gain(StreamId::Media, 3), 5.0f);

        // Other streams unaffected
        QCOMPARE(svc.gain(StreamId::Navigation, 3), 0.0f);  // Voice preset band 3 = 0
    }

    void testSetGainClearsPresetName()
    {
        oap::EqualizerService svc;
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Flat"));
        svc.setGain(StreamId::Media, 3, 5.0f);
        QCOMPARE(svc.activePreset(StreamId::Media), QString(""));
    }

    void testSaveUserPresetAutoName()
    {
        oap::EqualizerService svc;
        QString name1 = svc.saveUserPreset(StreamId::Media);
        QCOMPARE(name1, QString("Custom 1"));

        QString name2 = svc.saveUserPreset(StreamId::Media);
        QCOMPARE(name2, QString("Custom 2"));
    }

    void testSaveUserPresetExplicitName()
    {
        oap::EqualizerService svc;
        QString name = svc.saveUserPreset(StreamId::Media, "My EQ");
        QCOMPARE(name, QString("My EQ"));

        QStringList userPresets = svc.userPresetNames();
        QVERIFY(userPresets.contains("My EQ"));
    }

    void testSaveUserPresetRejectsBundledName()
    {
        oap::EqualizerService svc;
        QString result = svc.saveUserPreset(StreamId::Media, "Rock");
        QVERIFY(result.isEmpty());
    }

    void testDeleteUserPreset()
    {
        oap::EqualizerService svc;
        svc.saveUserPreset(StreamId::Media, "ToDelete");
        QVERIFY(svc.userPresetNames().contains("ToDelete"));

        bool ok = svc.deleteUserPreset("ToDelete");
        QVERIFY(ok);
        QVERIFY(!svc.userPresetNames().contains("ToDelete"));
    }

    void testDeleteActivePresetRevertsToFlat()
    {
        oap::EqualizerService svc;
        // Set some gains and save
        svc.setGain(StreamId::Media, 0, 6.0f);
        svc.saveUserPreset(StreamId::Media, "Temp");
        svc.applyPreset(StreamId::Media, "Temp");
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Temp"));

        svc.deleteUserPreset("Temp");
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Flat"));

        // Gains should be flat (all zeros)
        auto gains = svc.gainsForStream(StreamId::Media);
        for (int b = 0; b < oap::kNumBands; ++b) {
            QCOMPARE(gains[b], 0.0f);
        }
    }

    void testDeleteNonexistentPreset()
    {
        oap::EqualizerService svc;
        QVERIFY(!svc.deleteUserPreset("Nope"));
    }

    void testRenameUserPreset()
    {
        oap::EqualizerService svc;
        svc.saveUserPreset(StreamId::Media, "OldName");
        bool ok = svc.renameUserPreset("OldName", "NewName");
        QVERIFY(ok);
        QVERIFY(!svc.userPresetNames().contains("OldName"));
        QVERIFY(svc.userPresetNames().contains("NewName"));
    }

    void testRenameRejectsBundledName()
    {
        oap::EqualizerService svc;
        svc.saveUserPreset(StreamId::Media, "MyPreset");
        bool ok = svc.renameUserPreset("MyPreset", "Rock");
        QVERIFY(!ok);
        // Original should still exist
        QVERIFY(svc.userPresetNames().contains("MyPreset"));
    }

    void testRenameUpdatesActivePreset()
    {
        oap::EqualizerService svc;
        svc.saveUserPreset(StreamId::Media, "Active");
        svc.applyPreset(StreamId::Media, "Active");
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Active"));

        svc.renameUserPreset("Active", "Renamed");
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Renamed"));
    }

    void testBundledPresetNames()
    {
        oap::EqualizerService svc;
        QStringList names = svc.bundledPresetNames();
        QCOMPARE(names.size(), 9);
        QVERIFY(names.contains("Flat"));
        QVERIFY(names.contains("Rock"));
        QVERIFY(names.contains("Voice"));
    }

    void testUserPresetNamesEmpty()
    {
        oap::EqualizerService svc;
        QVERIFY(svc.userPresetNames().isEmpty());
    }

    void testEngineForStreamDistinct()
    {
        oap::EqualizerService svc;
        auto* media = svc.engineForStream(StreamId::Media);
        auto* nav = svc.engineForStream(StreamId::Navigation);
        auto* phone = svc.engineForStream(StreamId::Phone);

        QVERIFY(media != nullptr);
        QVERIFY(nav != nullptr);
        QVERIFY(phone != nullptr);
        QVERIFY(media != nav);
        QVERIFY(media != phone);
        QVERIFY(nav != phone);
    }

    void testBypassPerStream()
    {
        oap::EqualizerService svc;
        QVERIFY(!svc.isBypassed(StreamId::Media));
        QVERIFY(!svc.isBypassed(StreamId::Navigation));

        svc.setBypassed(StreamId::Media, true);
        QVERIFY(svc.isBypassed(StreamId::Media));
        QVERIFY(!svc.isBypassed(StreamId::Navigation));
    }

    void testGainsForStream()
    {
        oap::EqualizerService svc;
        // Media defaults to Flat
        auto gains = svc.gainsForStream(StreamId::Media);
        for (int b = 0; b < oap::kNumBands; ++b) {
            QCOMPARE(gains[b], 0.0f);
        }

        // Nav defaults to Voice
        auto navGains = svc.gainsForStream(StreamId::Navigation);
        const auto* voice = oap::findBundledPreset("Voice");
        for (int b = 0; b < oap::kNumBands; ++b) {
            QCOMPARE(navGains[b], voice->gains[b]);
        }
    }
};

QTEST_MAIN(TestEqualizerService)
#include "test_equalizer_service.moc"
