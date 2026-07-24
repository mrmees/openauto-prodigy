#include <QtTest>
#include "core/services/EqualizerService.hpp"
#include "core/audio/EqualizerEngine.hpp"
#include "core/audio/EqualizerPresets.hpp"
#include "core/YamlConfig.hpp"
#include <QDir>
#include <QFile>
#include <cmath>
#include <memory>

using oap::StreamId;

class TestEqualizerService : public QObject {
    Q_OBJECT

private slots:
    void testDefaultPresets()
    {
        oap::EqualizerService svc;
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Flat"));
        QCOMPARE(svc.activePreset(StreamId::Navigation), QString("Voice"));
        QCOMPARE(svc.activePreset(StreamId::System), QString("Voice"));
    }

    void testInvalidStreamInputs_data()
    {
        QTest::addColumn<int>("streamValue");
        QTest::newRow("negative") << -1;
        QTest::newRow("past-end") << 3;
    }

    void testInvalidStreamInputs()
    {
        QFETCH(int, streamValue);
        const auto invalid = static_cast<StreamId>(streamValue);
        oap::EqualizerService svc;
        const auto mediaGains = svc.gainsForStream(StreamId::Media);
        const auto userPresets = svc.userPresetNames();
        QSignalSpy gainsSpy(&svc, &oap::EqualizerService::gainsChanged);
        QSignalSpy bypassSpy(&svc, &oap::EqualizerService::bypassedChanged);
        QSignalSpy presetSpy(&svc, &oap::EqualizerService::presetListChanged);

        QCOMPARE(svc.activePreset(invalid), QString());
        QCOMPARE(svc.gain(invalid, 0), 0.0f);
        const auto invalidGains = svc.gainsForStream(invalid);
        for (float gain : invalidGains) QCOMPARE(gain, 0.0f);
        QVERIFY(!svc.isBypassed(invalid));

        svc.applyPreset(invalid, "Rock");
        svc.setGain(invalid, 0, 6.0f);
        svc.setBypassed(invalid, true);
        QVERIFY(svc.saveUserPreset(invalid, "Invalid source").isEmpty());
        QVERIFY(svc.acquireEngine(invalid, 48000.0f, 2) == nullptr);

        QCOMPARE(svc.gainsForStream(StreamId::Media), mediaGains);
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Flat"));
        QVERIFY(!svc.isBypassed(StreamId::Media));
        QCOMPARE(svc.userPresetNames(), userPresets);
        QCOMPARE(gainsSpy.count(), 0);
        QCOMPARE(bypassSpy.count(), 0);
        QCOMPARE(presetSpy.count(), 0);
    }

    void testInvalidQmlStreamIndexesRemainGuarded()
    {
        oap::EqualizerService svc;
        for (int invalid : {-1, 3}) {
            QVERIFY(svc.gainsAsList(invalid).isEmpty());
            QVERIFY(!svc.isBypassedForStream(invalid));
            QVERIFY(svc.activePresetForStream(invalid).isEmpty());
            svc.setGainForStream(invalid, 0, 6.0f);
            svc.setBypassedForStream(invalid, true);
            svc.applyPresetForStream(invalid, "Rock");
        }
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Flat"));
        QCOMPARE(svc.gain(StreamId::Media, 0), 0.0f);
        QVERIFY(!svc.isBypassed(StreamId::Media));
    }

    void testApplyPresetChangesOnlyTargetStream()
    {
        oap::EqualizerService svc;
        svc.applyPreset(StreamId::Media, "Rock");
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Rock"));
        // Others unchanged
        QCOMPARE(svc.activePreset(StreamId::Navigation), QString("Voice"));
        QCOMPARE(svc.activePreset(StreamId::System), QString("Voice"));

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

    void testSaveUserPresetRejectsWhitespaceName()
    {
        oap::EqualizerService svc;
        QVERIFY(svc.saveUserPreset(StreamId::Media, "  \t").isEmpty());
        QVERIFY(svc.userPresetNames().isEmpty());
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

    void testRenameRejectsEmptyWhitespaceAndDuplicateNames()
    {
        oap::EqualizerService svc;
        svc.saveUserPreset(StreamId::Media, "First");
        svc.saveUserPreset(StreamId::Media, "Second");
        svc.applyPreset(StreamId::Media, "First");

        QVERIFY(!svc.renameUserPreset("First", ""));
        QVERIFY(!svc.renameUserPreset("First", "  \t"));
        QVERIFY(!svc.renameUserPreset("First", "Second"));
        QCOMPARE(svc.userPresetNames(), QStringList({"First", "Second"}));
        QCOMPARE(svc.activePreset(StreamId::Media), QString("First"));
    }

    void testRenameExactNameIsSuccessfulNoOp()
    {
        oap::EqualizerService svc;
        svc.saveUserPreset(StreamId::Media, "Same");
        svc.applyPreset(StreamId::Media, "Same");
        QSignalSpy listSpy(&svc, &oap::EqualizerService::presetListChanged);
        QSignalSpy mediaSpy(&svc, &oap::EqualizerService::mediaPresetChanged);

        QVERIFY(svc.renameUserPreset("Same", "Same"));
        QCOMPARE(svc.userPresetNames(), QStringList({"Same"}));
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Same"));
        QCOMPARE(listSpy.count(), 0);
        QCOMPARE(mediaSpy.count(), 0);
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

    void testAcquireReturnsDistinctInitializedInstances()
    {
        oap::EqualizerService svc;
        svc.setGain(StreamId::Media, 0, 5.0f);
        auto* e1 = svc.acquireEngine(StreamId::Media, 48000.0f, 2);
        auto* e2 = svc.acquireEngine(StreamId::Media, 48000.0f, 2);
        QVERIFY(e1 && e2 && e1 != e2);
        QCOMPARE(e1->getGain(0), 5.0f);
        QCOMPARE(e2->getGain(0), 5.0f);
        svc.releaseEngine(e1); svc.releaseEngine(e2);
    }

    void testFanOutPropagatesToAllInstances()
    {
        oap::EqualizerService svc;
        auto* e1 = svc.acquireEngine(StreamId::Media, 48000.0f, 2);
        auto* e2 = svc.acquireEngine(StreamId::Media, 44100.0f, 2);
        svc.applyPreset(StreamId::Media, "Rock");
        const auto* rock = oap::findBundledPreset("Rock");
        QCOMPARE(e1->getGain(0), rock->gains[0]);
        QCOMPARE(e2->getGain(0), rock->gains[0]);
        svc.setBypassed(StreamId::Media, true);
        QVERIFY(e1->isBypassed() && e2->isBypassed());
        svc.releaseEngine(e2);
        svc.setGain(StreamId::Media, 1, -3.0f);
        QCOMPARE(e1->getGain(1), -3.0f);        // still fans out to live engine
        svc.releaseEngine(e1);
        svc.setGain(StreamId::Media, 2, 4.0f);  // no engines — must not crash
    }

    void testBypassAuthoritativeWithoutEngines()
    {
        oap::EqualizerService svc;
        svc.setBypassed(StreamId::Media, true);
        QVERIFY(svc.isBypassed(StreamId::Media));              // no engine involved
        auto* e = svc.acquireEngine(StreamId::Media, 48000.0f, 2);
        QVERIFY(e->isBypassed());                              // inherited at acquire
        svc.releaseEngine(e);
    }

    void testNonFiniteGainRejected()
    {
        oap::EqualizerService svc;
        svc.setGain(StreamId::Media, 0, 5.0f);
        svc.setGain(StreamId::Media, 0, std::nanf(""));
        QCOMPARE(svc.gain(StreamId::Media, 0), 5.0f);          // unchanged
    }

    // setGain clamps to the engine's +-12 dB range at the service boundary, so
    // the stored/getter value agrees with what the engine applies (round-2 F6:
    // service used to store the raw out-of-range value).
    void testSetGainClampsToEngineRange()
    {
        oap::EqualizerService svc;
        svc.setGain(StreamId::Media, 0, 20.0f);
        QCOMPARE(svc.gain(StreamId::Media, 0), 12.0f);   // clamped to +12
        svc.setGain(StreamId::Media, 1, -30.0f);
        QCOMPARE(svc.gain(StreamId::Media, 1), -12.0f);  // clamped to -12

        // Fan-out also sees the clamped value, not the raw one.
        auto* e = svc.acquireEngine(StreamId::Media, 48000.0f, 2);
        svc.setGain(StreamId::Media, 2, 99.0f);
        QCOMPARE(svc.gain(StreamId::Media, 2), 12.0f);
        QCOMPARE(e->getGain(2), 12.0f);
        svc.releaseEngine(e);
    }

    // A clamped gain persists as the clamped value and reloads unchanged — no
    // restart "snap" from a stored 20 dB down to the engine's 12 dB.
    void testClampedGainPersistsClamped()
    {
        const QString path = QDir::temp().filePath("eqclamp-test.yaml");
        QFile::remove(path);
        auto cfg = std::make_unique<oap::YamlConfig>(); cfg->load(path);
        {
            oap::EqualizerService svc(cfg.get());
            svc.setFlushHook([&]{ return cfg->save(path); });
            svc.setGain(StreamId::Media, 0, 20.0f);   // clamps to 12
            svc.saveNow();
        }
        auto cfg2 = std::make_unique<oap::YamlConfig>(); cfg2->load(path);
        oap::EqualizerService svc2(cfg2.get());
        QCOMPARE(svc2.gain(StreamId::Media, 0), 12.0f);   // reloads at 12, no snap
        QFile::remove(path);
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

    // --- Config-aware tests ---

    void testConstructorWithNullConfigStillWorks()
    {
        // nullptr config = no persistence, should not crash
        oap::YamlConfig* noConfig = nullptr;
        oap::EqualizerService svc(noConfig);
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Flat"));
        QCOMPARE(svc.activePreset(StreamId::Navigation), QString("Voice"));
    }

    void testConfigLoadsPresetAssignments()
    {
        oap::YamlConfig config;
        config.setEqStreamPreset("media", "Rock");
        config.setEqStreamPreset("navigation", "Bass Boost");

        oap::EqualizerService svc(&config);
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Rock"));
        QCOMPARE(svc.activePreset(StreamId::Navigation), QString("Bass Boost"));
        QCOMPARE(svc.activePreset(StreamId::System), QString("Voice")); // default
    }

    void testConfigMissingPresetFallsBackToFlat()
    {
        oap::YamlConfig config;
        config.setEqStreamPreset("media", "NonExistentPreset");

        oap::EqualizerService svc(&config);
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Flat"));
    }

    void testLegacyPhoneKeyRestoresSystemStream()
    {
        const QString path = QDir::temp().filePath("eqmig3-test.yaml");
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("audio:\n  equalizer:\n    streams:\n"
                "      phone: { preset: Rock }\n");
        f.close();
        oap::YamlConfig cfg; cfg.load(path);
        oap::EqualizerService svc(&cfg);
        QCOMPARE(svc.activePreset(StreamId::System), QString("Rock"));
        QFile::remove(path);
    }

    void testApplyPresetTriggersScheduleSave()
    {
        oap::YamlConfig config;
        oap::EqualizerService svc(&config);

        svc.applyPreset(StreamId::Media, "Rock");

        // Force flush
        svc.saveNow();

        QCOMPARE(config.eqStreamPreset("media"), QString("Rock"));
    }

    void testConstructionAndRestoreDoNotFlushUntilUserMutation()
    {
        oap::YamlConfig config;
        config.setEqStreamPreset("media", "Rock");
        oap::EqualizerService svc(&config);
        int flushes = 0;
        svc.setFlushHook([&]{ ++flushes; return true; });

        QTest::qWait(2200);
        QCOMPARE(flushes, 0);
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Rock"));

        svc.setGain(StreamId::Media, 0, 3.0f);
        QTRY_COMPARE_WITH_TIMEOUT(flushes, 1, 5000);
    }

    void testMalformedRestoreDoesNotScheduleDestructiveRewrite()
    {
        const QString path = QDir::temp().filePath("eq-no-boot-rewrite-test.yaml");
        QFile::remove(path);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("audio:\n  equalizer:\n    streams:\n"
                   "      media: { preset: Flat }\n"
                   "    user_presets:\n"
                   "      - { name: Repairable, gains: [1, 2, 3] }\n");
        file.close();

        oap::YamlConfig config;
        config.load(path);
        oap::EqualizerService svc(&config);
        int flushes = 0;
        svc.setFlushHook([&]{ ++flushes; return true; });

        QVERIFY(config.eqUserPresets().isEmpty());
        QTest::qWait(2200);
        QCOMPARE(flushes, 0);
        svc.saveNow();
        QCOMPARE(flushes, 0);
        QFile verify(path);
        QVERIFY(verify.open(QIODevice::ReadOnly));
        QVERIFY(verify.readAll().contains("Repairable"));
        QFile::remove(path);
    }

    void testRestoreFiltersAmbiguousPresetNamesWithoutDirtyingConfig()
    {
        oap::YamlConfig config;
        QList<oap::YamlConfig::EqUserPreset> presets;
        auto append = [&](const QString& name, float gain) {
            oap::YamlConfig::EqUserPreset preset;
            preset.name = name;
            preset.gains.fill(gain);
            presets.append(preset);
        };
        append("  ", 1.0f);
        append("Rock", 2.0f);
        append("Valid", 3.0f);
        append("Valid", 4.0f);
        config.setEqUserPresets(presets);

        oap::EqualizerService svc(&config);
        QCOMPARE(svc.userPresetNames(), QStringList{"Valid"});
        int flushes = 0;
        svc.setFlushHook([&]{ ++flushes; return true; });
        svc.saveNow();
        QCOMPARE(flushes, 0);
        QCOMPARE(config.eqUserPresets().size(), 4);
    }

    void testSaveUserPresetPersistsToConfig()
    {
        oap::YamlConfig config;
        oap::EqualizerService svc(&config);

        svc.setGain(StreamId::Media, 0, 6.0f);
        svc.saveUserPreset(StreamId::Media, "MyCustom");
        svc.saveNow();

        auto presets = config.eqUserPresets();
        QCOMPARE(presets.size(), 1);
        QCOMPARE(presets[0].name, QString("MyCustom"));
        QCOMPARE(presets[0].gains[0], 6.0f);
    }

    void testDeleteUserPresetRemovesFromConfig()
    {
        oap::YamlConfig config;
        oap::EqualizerService svc(&config);

        svc.saveUserPreset(StreamId::Media, "ToDelete");
        svc.saveNow();
        QCOMPARE(config.eqUserPresets().size(), 1);

        svc.deleteUserPreset("ToDelete");
        svc.saveNow();
        QCOMPARE(config.eqUserPresets().size(), 0);
    }

    void testConfigLoadsUserPresets()
    {
        oap::YamlConfig config;
        QList<oap::YamlConfig::EqUserPreset> presets;
        oap::YamlConfig::EqUserPreset p;
        p.name = "Saved Custom";
        p.gains = {1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        presets.append(p);
        config.setEqUserPresets(presets);

        oap::EqualizerService svc(&config);
        QVERIFY(svc.userPresetNames().contains("Saved Custom"));

        // Can apply the loaded user preset
        svc.applyPreset(StreamId::Media, "Saved Custom");
        QCOMPARE(svc.activePreset(StreamId::Media), QString("Saved Custom"));
        QCOMPARE(svc.gain(StreamId::Media, 0), 1.0f);
    }

    // --- Durable persistence (Task 4) ---

    void testUnsavedGainsAndBypassSurviveRestart()
    {
        const QString path = QDir::temp().filePath("eqsvc-test.yaml");
        QFile::remove(path);
        auto cfg = std::make_unique<oap::YamlConfig>(); cfg->load(path);
        {
            oap::EqualizerService svc(cfg.get());
            svc.setFlushHook([&]{ return cfg->save(path); });
            svc.setGain(StreamId::Media, 0, 7.5f);   // "Custom" — no preset saved
            svc.setBypassed(StreamId::Navigation, true);
            svc.saveNow();
        }
        auto cfg2 = std::make_unique<oap::YamlConfig>(); cfg2->load(path);
        oap::EqualizerService svc2(cfg2.get());
        QCOMPARE(svc2.gain(StreamId::Media, 0), 7.5f);
        QCOMPARE(svc2.activePreset(StreamId::Media), QString(""));  // still Custom
        QVERIFY(svc2.isBypassed(StreamId::Navigation));
        QFile::remove(path);
    }

    void testFlushFailureRearmsDebounce()
    {
        oap::YamlConfig cfg;
        oap::EqualizerService svc(&cfg);
        int calls = 0;
        svc.setFlushHook([&]{ ++calls; return false; });
        svc.setGain(StreamId::Media, 0, 1.0f);
        svc.saveNow();
        QCOMPARE(calls, 1);
        // debounce re-armed on failure: saveTimer_ active again
        QTRY_VERIFY_WITH_TIMEOUT(calls >= 2, 5000);
    }
};

QTEST_MAIN(TestEqualizerService)
#include "test_equalizer_service.moc"
