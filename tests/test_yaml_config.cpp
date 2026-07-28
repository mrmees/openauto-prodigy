#include <QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "core/YamlConfig.hpp"

class TestYamlConfig : public QObject {
    Q_OBJECT
private slots:
    void testLoadDefaults();
    void testLoadFromFile();
    void testSaveAndReload();
    void testPluginScoping();
    void testPluginValueListRoundTrip();
    void testIdentityDefaults();
    void testIdentityFromFile();
    void testVideoDpi();
    void testVideoCodecDefaults();
    void testSensorsDefaults();
    void testSensorsFromFile();
    void testMicDefaults();
    void testMicFromFile();
    void testMicrophoneDevicePathWrite();
    void testLoggingDefaultsAndScalarPathWrite();
    void testLoggingDebugCategoriesRoundTrip();
    void testValueByPath();
    void testValueByPathNested();
    void testValueByPathMissing();
    void testSetValueByPath();
    void testSetValueByPathRejectsUnknown();
    // testSidebarDefaults removed — sidebar config keys no longer exist
    void testProtocolCaptureDefaults();
    void testProtocolCaptureSetValueByPath();
    void testGalVersionDefaultAndRoundTrip();
    void testEqStreamPresetDefaults();
    void testPhoneToSystemMigration();
    void testPhoneToSystemBothPresentKeepsSystem();
    void testEqStreamPresetSetAndGet();
    void testEqStreamPresetSaveReload();
    void testEqUserPresetsEmpty();
    void testEqUserPresetsSaveReload();
    void testEqGainsRoundTripToDisk();
    void testEqGainsValidationRejectsMalformed();
    void testUiScaleDefaults();
    void testUiTokenDefaults();
    void testUiScaleSetAndGet();
    void testUiTokenSetAndGet();
    void testUiFontFloorDefault();
    void testUiNewTokenDefaults();
    void testNavbarShowDuringAADefault();
    void testSidebarDefaultsRemoved();
    void testDisplayScreenSizeDefault();
    void testGridSavedDimsDefaults();
    void testGridSavedDimsRoundTrip();
    void testDashboardDefaultsNextInstanceIdZero();
    void testDashboardsPlacementPageRoundTrip();
    void testV4DefaultsOneHomeDashboard();
    void testDashboardsRoundTrip();
    void testV3MigratesToV4();
    void testV2FlatShapeMigrates();
    void testNestedMergeRetainsUnknownKeys();
    void testLoadCorruptFileFallsBackToDefaults();
    void testLoadMappingShapeMismatchFallsBackToDefaults();
    void testLoadCorruptOverwritesPreviousCorruptAside();
    void testLoadMissingFileYieldsDefaults();
    void testSaveAtomicReplacesExisting();
    void testSaveFailureReturnsFalse();
};

void TestYamlConfig::testLoadDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.hardwareProfile(), QString("rpi4"));
    QCOMPARE(config.displayBrightness(), 80);
    QCOMPARE(config.wifiSsid(), QString("OpenAutoProdigy"));
    QCOMPARE(config.tcpPort(), static_cast<uint16_t>(5277));
    QCOMPARE(config.videoFps(), 30);
    QCOMPARE(config.autoConnectAA(), true);
    QCOMPARE(config.valueByPath("connection.gal_version").toString(),
             QString("6.0"));
    QCOMPARE(config.masterVolume(), 80);
    QCOMPARE(config.audioBufferMs("media"), 500);
    QCOMPARE(config.audioBufferMs("speech"), 500);
    QCOMPARE(config.audioBufferMs("system"), 500);
}

void TestYamlConfig::testLoadFromFile()
{
    oap::YamlConfig config;
    config.load(QString(TEST_DATA_DIR) + "/test_config.yaml");

    QCOMPARE(config.wifiSsid(), QString("TestSSID"));
    QCOMPARE(config.wifiPassword(), QString("TestPassword"));
    QCOMPARE(config.masterVolume(), 75);
    QCOMPARE(config.displayBrightness(), 80);

    auto enabled = config.enabledPlugins();
    QCOMPARE(enabled.size(), 2);
    QCOMPARE(enabled[0], QString("org.openauto.android-auto"));
}

void TestYamlConfig::testVideoCodecDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.videoCodecs(),
             QStringList({QStringLiteral("h265"), QStringLiteral("h264")}));
}

void TestYamlConfig::testSaveAndReload()
{
    oap::YamlConfig config;
    config.setWifiSsid("NewSSID");
    config.setMasterVolume(50);

    QString tmpPath = QDir::tempPath() + "/oap_test_config.yaml";
    config.save(tmpPath);

    oap::YamlConfig loaded;
    loaded.load(tmpPath);
    QCOMPARE(loaded.wifiSsid(), QString("NewSSID"));
    QCOMPARE(loaded.masterVolume(), 50);

    QFile::remove(tmpPath);
}

void TestYamlConfig::testPluginScoping()
{
    oap::YamlConfig config;
    // Plugin configs are scoped by plugin ID
    config.setPluginValue("org.openauto.android-auto", "auto_connect", true);
    config.setPluginValue("org.openauto.android-auto", "video_fps", 60);

    QCOMPARE(config.pluginValue("org.openauto.android-auto", "auto_connect").toBool(), true);
    QCOMPARE(config.pluginValue("org.openauto.android-auto", "video_fps").toInt(), 60);
    // Different plugin returns invalid
    QVERIFY(!config.pluginValue("org.openauto.bt-audio", "auto_connect").isValid());
}

void TestYamlConfig::testPluginValueListRoundTrip()
{
    // Media player persists its play queue as a QStringList; qint64 track
    // position rides along. Both must survive a save + reload through real
    // YAML — bench 2026-07-09 caught last_queue silently becoming "".
    oap::YamlConfig config;
    const QStringList queue{QStringLiteral("/home/matt/Music/a.mp3"),
                            QStringLiteral("/home/matt/Music/b — with dash.mp3"),
                            QStringLiteral("/home/matt/Music/c'quote.flac")};
    config.setPluginValue("org.openauto.media-player", "last_queue", queue);
    config.setPluginValue("org.openauto.media-player", "last_position_ms",
                          QVariant(qint64(21681)));

    // In-memory read-back
    QCOMPARE(config.pluginValue("org.openauto.media-player", "last_queue").toStringList(),
             queue);

    // Round-trip through the YAML emitter and parser
    QString tmpPath = QDir::tempPath() + "/oap_test_plugin_list.yaml";
    config.save(tmpPath);
    oap::YamlConfig loaded;
    loaded.load(tmpPath);
    QCOMPARE(loaded.pluginValue("org.openauto.media-player", "last_queue").toStringList(),
             queue);
    QCOMPARE(loaded.pluginValue("org.openauto.media-player", "last_position_ms").toLongLong(),
             qint64(21681));
    QFile::remove(tmpPath);
}

void TestYamlConfig::testIdentityDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.headUnitName(), QString("OpenAuto Prodigy"));
    QCOMPARE(config.manufacturer(), QString("OpenAuto Project"));
    QCOMPARE(config.model(), QString("Raspberry Pi 4"));
    // identity.sw_version was removed 2026-07-09 (version is compiled in —
    // OAP_VERSION). setValueByPath validates against the defaults schema,
    // so writes to the dead key must now be rejected.
    QVERIFY(!config.setValueByPath(QStringLiteral("identity.sw_version"),
                                   QStringLiteral("9.9.9")));
    QCOMPARE(config.carModel(), QString(""));
    QCOMPARE(config.carYear(), QString(""));
    QCOMPARE(config.leftHandDrive(), true);
}

void TestYamlConfig::testIdentityFromFile()
{
    oap::YamlConfig config;
    config.load(QString(TEST_DATA_DIR) + "/test_config.yaml");

    QCOMPARE(config.headUnitName(), QString("Test Unit"));
    QCOMPARE(config.manufacturer(), QString("Test Manufacturer"));
    QCOMPARE(config.model(), QString("Test Model X"));
    QCOMPARE(config.carModel(), QString("Miata"));
    QCOMPARE(config.carYear(), QString("2000"));
    QCOMPARE(config.leftHandDrive(), false);
}

void TestYamlConfig::testVideoDpi()
{
    oap::YamlConfig config;
    QCOMPARE(config.videoDpi(), 140);

    config.load(QString(TEST_DATA_DIR) + "/test_config.yaml");
    QCOMPARE(config.videoDpi(), 160);

    config.setVideoDpi(200);
    QCOMPARE(config.videoDpi(), 200);
}

void TestYamlConfig::testSensorsDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.nightModeSource(), QString("time"));
    QCOMPARE(config.nightModeDayStart(), QString("07:00"));
    QCOMPARE(config.nightModeNightStart(), QString("19:00"));
    QCOMPARE(config.nightModeGpioPin(), 17);
    QCOMPARE(config.nightModeGpioActiveHigh(), true);
    QCOMPARE(config.gpsEnabled(), true);
    QCOMPARE(config.gpsSource(), QString("none"));
}

void TestYamlConfig::testSensorsFromFile()
{
    oap::YamlConfig config;
    config.load(QString(TEST_DATA_DIR) + "/test_config.yaml");

    QCOMPARE(config.nightModeSource(), QString("gpio"));
    QCOMPARE(config.nightModeDayStart(), QString("06:30"));
    QCOMPARE(config.nightModeNightStart(), QString("20:00"));
    QCOMPARE(config.nightModeGpioPin(), 22);
    QCOMPARE(config.nightModeGpioActiveHigh(), false);
    QCOMPARE(config.gpsEnabled(), false);
    QCOMPARE(config.gpsSource(), QString("gpsd"));
}

void TestYamlConfig::testMicDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.microphoneDevice(), QString("auto"));
    QCOMPARE(config.microphoneGain(), 1.0);
}

void TestYamlConfig::testMicFromFile()
{
    oap::YamlConfig config;
    config.load(QString(TEST_DATA_DIR) + "/test_config.yaml");

    QCOMPARE(config.microphoneDevice(), QString("hw:1,0"));
    QCOMPARE(config.microphoneGain(), 1.5);

    config.setMicrophoneDevice("pulse");
    config.setMicrophoneGain(2.0);
    QCOMPARE(config.microphoneDevice(), QString("pulse"));
    QCOMPARE(config.microphoneGain(), 2.0);
}

void TestYamlConfig::testMicrophoneDevicePathWrite()
{
    oap::YamlConfig cfg;
    // Canonical key: schema-valid, round-trips, feeds the startup getter.
    QVERIFY(cfg.setValueByPath("audio.microphone.device", "alsa_input.usb-mic"));
    QCOMPARE(cfg.valueByPath("audio.microphone.device").toString(),
             QString("alsa_input.usb-mic"));
    QCOMPARE(cfg.microphoneDevice(), QString("alsa_input.usb-mic"));
    // Regression pin for the 2026-07-15 root cause: the dead key the UI used
    // to write is NOT in the defaults schema and must stay rejected.
    QVERIFY(!cfg.setValueByPath("audio.input_device", "anything"));
}

void TestYamlConfig::testLoggingDefaultsAndScalarPathWrite()
{
    oap::YamlConfig config;
    QCOMPARE(config.loggingVerbose(), false);
    QVERIFY(config.loggingDebugCategories().isEmpty());
    QCOMPARE(config.valueByPath("logging.verbose").toBool(), false);
    QVERIFY(!config.valueByPath("logging.debug_categories").isValid());

    QVERIFY(config.setValueByPath("logging.verbose", true));
    QCOMPARE(config.loggingVerbose(), true);
    QVERIFY(!config.setValueByPath("logging.debug_categories",
                                   QStringList{QStringLiteral("core")}));

    const QString path = QDir::temp().filePath("oap_test_logging_verbose.yaml");
    QVERIFY(config.save(path));
    oap::YamlConfig loaded;
    loaded.load(path);
    QCOMPARE(loaded.loggingVerbose(), true);
    QFile::remove(path);
}

void TestYamlConfig::testLoggingDebugCategoriesRoundTrip()
{
    const QStringList categories{QStringLiteral("core"),
                                 QStringLiteral("aa")};
    oap::YamlConfig config;
    config.setLoggingDebugCategories(categories);
    QCOMPARE(config.loggingDebugCategories(), categories);
    QVERIFY(!config.valueByPath("logging.debug_categories").isValid());

    const QString path = QDir::temp().filePath("oap_test_logging_categories.yaml");
    QVERIFY(config.save(path));
    oap::YamlConfig loaded;
    loaded.load(path);
    QCOMPARE(loaded.loggingDebugCategories(), categories);
    QFile::remove(path);
}

void TestYamlConfig::testValueByPath()
{
    oap::YamlConfig config;
    QCOMPARE(config.valueByPath("hardware_profile").toString(), QString("rpi4"));
    QCOMPARE(config.valueByPath("connection.tcp_port").toInt(), 5277);
    QCOMPARE(config.valueByPath("video.fps").toInt(), 30);
}

void TestYamlConfig::testValueByPathNested()
{
    oap::YamlConfig config;
    QCOMPARE(config.valueByPath("connection.wifi_ap.ssid").toString(), QString("OpenAutoProdigy"));
    QCOMPARE(config.valueByPath("connection.wifi_ap.password").toString(), QString("prodigy"));
    QCOMPARE(config.valueByPath("sensors.night_mode.source").toString(), QString("time"));
    QCOMPARE(config.valueByPath("sensors.gps.enabled").toBool(), true);
}

void TestYamlConfig::testValueByPathMissing()
{
    oap::YamlConfig config;
    QVERIFY(!config.valueByPath("nonexistent").isValid());
    QVERIFY(!config.valueByPath("connection.nonexistent").isValid());
    QVERIFY(!config.valueByPath("").isValid());
}

void TestYamlConfig::testSetValueByPath()
{
    oap::YamlConfig config;
    QVERIFY(config.setValueByPath("connection.tcp_port", 9999));
    QCOMPARE(config.tcpPort(), static_cast<uint16_t>(9999));
    QCOMPARE(config.valueByPath("connection.tcp_port").toInt(), 9999);

    QVERIFY(config.setValueByPath("connection.wifi_ap.ssid", QString("NewSSID")));
    QCOMPARE(config.wifiSsid(), QString("NewSSID"));
}

void TestYamlConfig::testSetValueByPathRejectsUnknown()
{
    oap::YamlConfig config;
    // Unknown paths rejected
    QVERIFY(!config.setValueByPath("bogus.key", 42));
    QVERIFY(!config.setValueByPath("connection.bogus", 42));
    QVERIFY(!config.valueByPath("bogus.key").isValid());
    // Non-leaf (map) paths rejected — prevents overwriting subtrees
    QVERIFY(!config.setValueByPath("audio", QString("x")));
    QVERIFY(!config.setValueByPath("connection", QString("x")));
    QVERIFY(!config.setValueByPath("connection.wifi_ap", QString("x")));
}

// testSidebarDefaults removed — sidebar config keys no longer exist

void TestYamlConfig::testProtocolCaptureDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.valueByPath("connection.protocol_capture.enabled").toBool(), false);
    QCOMPARE(config.valueByPath("connection.protocol_capture.format").toString(), QString("jsonl"));
    QCOMPARE(config.valueByPath("connection.protocol_capture.include_media").toBool(), false);
    QCOMPARE(config.valueByPath("connection.protocol_capture.path").toString(),
             QString("/tmp/oaa-protocol-capture.jsonl"));
}

void TestYamlConfig::testProtocolCaptureSetValueByPath()
{
    oap::YamlConfig config;
    QVERIFY(config.setValueByPath("connection.protocol_capture.enabled", true));
    QVERIFY(config.setValueByPath("connection.protocol_capture.format", QString("tsv")));
    QVERIFY(config.setValueByPath("connection.protocol_capture.include_media", true));
    QVERIFY(config.setValueByPath("connection.protocol_capture.path",
                                  QString("/tmp/custom-capture.jsonl")));

    QCOMPARE(config.valueByPath("connection.protocol_capture.enabled").toBool(), true);
    QCOMPARE(config.valueByPath("connection.protocol_capture.format").toString(), QString("tsv"));
    QCOMPARE(config.valueByPath("connection.protocol_capture.include_media").toBool(), true);
    QCOMPARE(config.valueByPath("connection.protocol_capture.path").toString(),
             QString("/tmp/custom-capture.jsonl"));
}

void TestYamlConfig::testGalVersionDefaultAndRoundTrip()
{
    oap::YamlConfig config;
    QCOMPARE(config.valueByPath("connection.gal_version").toString(),
             QString("6.0"));
    QVERIFY(config.setValueByPath("connection.gal_version", QString("6.0")));
    QCOMPARE(config.valueByPath("connection.gal_version").toString(),
             QString("6.0"));

    const QString path = QDir::tempPath() + "/oap_test_gal_version.yaml";
    QVERIFY(config.save(path));
    oap::YamlConfig loaded;
    loaded.load(path);
    QCOMPARE(loaded.valueByPath("connection.gal_version").toString(),
             QString("6.0"));
    QFile::remove(path);
}

void TestYamlConfig::testEqStreamPresetDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.eqStreamPreset("media"), QString("Flat"));
    QCOMPARE(config.eqStreamPreset("navigation"), QString("Voice"));
    QCOMPARE(config.eqStreamPreset("system"), QString("Voice"));
}

void TestYamlConfig::testPhoneToSystemMigration()
{
    const QString path = QDir::temp().filePath("eqmig-test.yaml");
    QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("audio:\n  equalizer:\n    streams:\n"
            "      phone: { preset: Rock }\n");
    f.close();
    oap::YamlConfig cfg; cfg.load(path);
    QCOMPARE(cfg.eqStreamPreset("system"), QString("Rock"));  // migrated
    QVERIFY(cfg.save(path));
    QFile rf(path); QVERIFY(rf.open(QIODevice::ReadOnly));
    const QByteArray out = rf.readAll();
    // Scope the key-presence checks to the EQ streams block. The unrelated
    // top-level telephony `phone:` default (reject_sco_during_aa /
    // settle_grace_ms, design §6) legitimately survives and must NOT be
    // migrated (design §4.6) — a whole-file contains("phone:") would collide
    // with it. streams: always precedes its sibling user_presets: under
    // equalizer, so that window isolates the migrated stream keys.
    const int sIdx = out.indexOf("streams:");
    const int eIdx = out.indexOf("user_presets:");
    QVERIFY(sIdx >= 0 && eIdx > sIdx);
    const QByteArray streamsBlock = out.mid(sIdx, eIdx - sIdx);
    QVERIFY(!streamsBlock.contains("phone:"));                 // old EQ key gone
    QVERIFY(streamsBlock.contains("system:"));
    QFile::remove(path);
}

void TestYamlConfig::testPhoneToSystemBothPresentKeepsSystem()
{
    const QString path = QDir::temp().filePath("eqmig2-test.yaml");
    QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("audio:\n  equalizer:\n    streams:\n"
            "      phone: { preset: Rock }\n"
            "      system: { preset: Jazz }\n");
    f.close();
    oap::YamlConfig cfg; cfg.load(path);
    QCOMPARE(cfg.eqStreamPreset("system"), QString("Jazz"));
    QFile::remove(path);
}

void TestYamlConfig::testEqStreamPresetSetAndGet()
{
    oap::YamlConfig config;
    config.setEqStreamPreset("media", "Rock");
    QCOMPARE(config.eqStreamPreset("media"), QString("Rock"));
    // Others unchanged
    QCOMPARE(config.eqStreamPreset("navigation"), QString("Voice"));
}

void TestYamlConfig::testEqStreamPresetSaveReload()
{
    oap::YamlConfig config;
    config.setEqStreamPreset("media", "Rock");

    QString tmpPath = QDir::tempPath() + "/oap_test_eq_config.yaml";
    config.save(tmpPath);

    oap::YamlConfig loaded;
    loaded.load(tmpPath);
    QCOMPARE(loaded.eqStreamPreset("media"), QString("Rock"));
    QCOMPARE(loaded.eqStreamPreset("navigation"), QString("Voice"));

    QFile::remove(tmpPath);
}

void TestYamlConfig::testEqUserPresetsEmpty()
{
    oap::YamlConfig config;
    QVERIFY(config.eqUserPresets().isEmpty());
}

void TestYamlConfig::testEqUserPresetsSaveReload()
{
    oap::YamlConfig config;

    QList<oap::YamlConfig::EqUserPreset> presets;
    oap::YamlConfig::EqUserPreset p1;
    p1.name = "My Preset";
    p1.gains = {1.0f, 2.0f, 3.0f, -1.0f, -2.0f, 0.0f, 4.0f, 5.0f, -3.0f, 0.5f};
    presets.append(p1);

    oap::YamlConfig::EqUserPreset p2;
    p2.name = "Bass Boost Custom";
    p2.gains = {6.0f, 5.0f, 4.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    presets.append(p2);

    config.setEqUserPresets(presets);

    QString tmpPath = QDir::tempPath() + "/oap_test_eq_user_presets.yaml";
    config.save(tmpPath);

    oap::YamlConfig loaded;
    loaded.load(tmpPath);
    auto loadedPresets = loaded.eqUserPresets();
    QCOMPARE(loadedPresets.size(), 2);
    QCOMPARE(loadedPresets[0].name, QString("My Preset"));
    QCOMPARE(loadedPresets[1].name, QString("Bass Boost Custom"));
    for (int i = 0; i < 10; ++i) {
        QCOMPARE(loadedPresets[0].gains[i], p1.gains[i]);
        QCOMPARE(loadedPresets[1].gains[i], p2.gains[i]);
    }

    QFile::remove(tmpPath);
}

void TestYamlConfig::testEqGainsRoundTripToDisk()
{
    const QString path = QDir::temp().filePath("eqgains-test.yaml");
    QFile::remove(path);
    {
        oap::YamlConfig cfg; cfg.load(path);
        QList<float> g; for (int i = 0; i < 10; ++i) g << float(i) - 4.5f;
        cfg.setEqStreamGains("media", g);
        cfg.setEqStreamBypassed("media", true);
        QVERIFY(cfg.save(path));
    }
    oap::YamlConfig fresh; fresh.load(path);          // brand-new object
    auto g2 = fresh.eqStreamGains("media");
    QCOMPARE(g2.size(), 10);
    QCOMPARE(g2[0], -4.5f);
    QVERIFY(fresh.eqStreamBypassed("media"));
    QFile::remove(path);
}

void TestYamlConfig::testEqGainsValidationRejectsMalformed()
{
    const QString path = QDir::temp().filePath("eqbad-test.yaml");
    QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("audio:\n  equalizer:\n    streams:\n"
            "      media: { gains: [1, 2, .nan, 4, 5, 6, 7, 8, 9, 10] }\n"
            "      navigation: { gains: [1, 2, 3] }\n"
            "    user_presets:\n"
            "      - { name: Bad, gains: [.inf, 2, 3, 4, 5, 6, 7, 8, 9, 10] }\n"
            "      - { name: Good, gains: [1, 1, 1, 1, 1, 1, 1, 1, 1, 1] }\n");
    f.close();
    oap::YamlConfig cfg; cfg.load(path);
    QVERIFY(cfg.eqStreamGains("media").isEmpty());        // NaN ⇒ invalid
    QVERIFY(cfg.eqStreamGains("navigation").isEmpty());   // short ⇒ invalid
    auto presets = cfg.eqUserPresets();
    QCOMPARE(presets.size(), 1);                          // "Bad" dropped
    QCOMPARE(presets[0].name, QString("Good"));
    QFile::remove(path);
}

void TestYamlConfig::testUiScaleDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.valueByPath("ui.scale").toInt(), 0);
    QCOMPARE(config.valueByPath("ui.fontScale").toInt(), 0);
}

void TestYamlConfig::testUiTokenDefaults()
{
    oap::YamlConfig config;
    QStringList tokens = {"rowH", "touchMin", "fontTitle", "fontBody", "fontSmall",
                          "fontHeading", "headerH", "iconSize", "radius", "tileW", "tileH"};
    for (const auto& tok : tokens) {
        QVariant v = config.valueByPath("ui.tokens." + tok);
        QVERIFY2(v.isValid(), qPrintable("ui.tokens." + tok + " should be registered"));
        QCOMPARE(v.toInt(), 0);
    }
}

void TestYamlConfig::testUiScaleSetAndGet()
{
    oap::YamlConfig config;
    QVERIFY(config.setValueByPath("ui.scale", 1.5));
    QCOMPARE(config.valueByPath("ui.scale").toDouble(), 1.5);
}

void TestYamlConfig::testUiTokenSetAndGet()
{
    oap::YamlConfig config;
    QVERIFY(config.setValueByPath("ui.tokens.fontBody", 24));
    QCOMPARE(config.valueByPath("ui.tokens.fontBody").toInt(), 24);
}

void TestYamlConfig::testUiFontFloorDefault()
{
    oap::YamlConfig config;
    QVariant v = config.valueByPath("ui.fontFloor");
    QVERIFY2(v.isValid(), "ui.fontFloor should be registered");
    QCOMPARE(v.toInt(), 0);
}

void TestYamlConfig::testUiNewTokenDefaults()
{
    oap::YamlConfig config;
    QStringList newTokens = {"trackThick", "trackThin", "knobSize", "knobSizeSmall",
                              "radiusSmall", "radiusLarge", "albumArt", "callBtnSize",
                              "overlayBtnW", "overlayBtnH", "fontTiny"};
    for (const auto& tok : newTokens) {
        QVariant v = config.valueByPath("ui.tokens." + tok);
        QVERIFY2(v.isValid(), qPrintable("ui.tokens." + tok + " should be registered"));
        QCOMPARE(v.toInt(), 0);
    }
}

void TestYamlConfig::testNavbarShowDuringAADefault()
{
    oap::YamlConfig config;
    QVariant v = config.valueByPath("navbar.show_during_aa");
    QVERIFY2(v.isValid(), "navbar.show_during_aa should be registered");
    QCOMPARE(v.toBool(), true);
}

void TestYamlConfig::testSidebarDefaultsRemoved()
{
    oap::YamlConfig config;
    // Sidebar config keys should no longer have defaults
    QVERIFY2(!config.valueByPath("video.sidebar.enabled").isValid(),
             "video.sidebar.enabled should not be in defaults");
    QVERIFY2(!config.valueByPath("video.sidebar.width").isValid(),
             "video.sidebar.width should not be in defaults");
    QVERIFY2(!config.valueByPath("video.sidebar.position").isValid(),
             "video.sidebar.position should not be in defaults");
}

void TestYamlConfig::testDisplayScreenSizeDefault()
{
    oap::YamlConfig config;
    QVariant v = config.valueByPath("display.screen_size");
    QVERIFY2(v.isValid(), "display.screen_size should be registered");
    QCOMPARE(v.toDouble(), 7.0);
}

void TestYamlConfig::testDashboardDefaultsNextInstanceIdZero()
{
    // Ported from testWidgetGridDefaults (flat gridNextInstanceId()==0 on fresh
    // config) — the empty-placements half of the original assertion is already
    // covered by testV4DefaultsOneHomeDashboard.
    oap::YamlConfig config;
    auto ds = config.dashboards();
    QCOMPARE(ds.size(), 1);
    QVERIFY(ds[0].placements.isEmpty());
    QCOMPARE(ds[0].nextInstanceId, 0);
}

void TestYamlConfig::testDashboardsPlacementPageRoundTrip()
{
    // Ported from testGridPlacementPageRoundTrip — two placements on different
    // pages within a single dashboard, round-tripped through save/load.
    oap::YamlConfig config;

    oap::DashboardConfig home;
    home.id = "home";
    home.name = "Home";
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0";
        p.widgetId = "org.openauto.clock";
        p.col = 0; p.row = 0;
        p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25;
        p.page = 0;
        home.placements.append(p);
    }
    {
        oap::GridPlacement p;
        p.instanceId = "status-1";
        p.widgetId = "org.openauto.aa-status";
        p.col = 0; p.row = 0;
        p.colSpan = 2; p.rowSpan = 1;
        p.opacity = 0.5;
        p.page = 1;
        home.placements.append(p);
    }

    config.setDashboards({home});

    QString tmpPath = QDir::tempPath() + "/oap_test_grid_page.yaml";
    config.save(tmpPath);

    oap::YamlConfig loaded;
    loaded.load(tmpPath);
    auto loadedPlacements = loaded.dashboards()[0].placements;

    QCOMPARE(loadedPlacements.size(), 2);
    QCOMPARE(loadedPlacements[0].page, 0);
    QCOMPARE(loadedPlacements[1].page, 1);
    QCOMPARE(loadedPlacements[1].instanceId, QString("status-1"));

    QFile::remove(tmpPath);
}

// testGridPageCountRoundTrip dropped — pageCount save/reload fidelity is
// exactly exercised by testDashboardsRoundTrip (ds[1].pageCount == 1).
// testGridPageCountDefault dropped — exact duplicate of
// testV4DefaultsOneHomeDashboard's QCOMPARE(ds[0].pageCount, 2).

void TestYamlConfig::testGridSavedDimsDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.gridSavedCols(), 0);
    QCOMPARE(config.gridSavedRows(), 0);
}

void TestYamlConfig::testGridSavedDimsRoundTrip()
{
    oap::YamlConfig config;
    config.setGridSavedDims(8, 5);

    QCOMPARE(config.gridSavedCols(), 8);
    QCOMPARE(config.gridSavedRows(), 5);

    QString tmpPath = QDir::tempPath() + "/oap_test_grid_dims.yaml";
    config.save(tmpPath);

    oap::YamlConfig loaded;
    loaded.load(tmpPath);
    QCOMPARE(loaded.gridSavedCols(), 8);
    QCOMPARE(loaded.gridSavedRows(), 5);

    QFile::remove(tmpPath);
}

void TestYamlConfig::testV4DefaultsOneHomeDashboard()
{
    oap::YamlConfig cfg;
    auto ds = cfg.dashboards();
    QCOMPARE(ds.size(), 1);
    QCOMPARE(ds[0].id, QString("home"));
    QCOMPARE(ds[0].name, QString("Home"));
    QCOMPARE(ds[0].pageCount, 2);
    QVERIFY(ds[0].placements.isEmpty());
    QCOMPARE(cfg.activeDashboardId(), QString("home"));
}

void TestYamlConfig::testDashboardsRoundTrip()
{
    oap::YamlConfig cfg;
    oap::DashboardConfig home{ "home", "Home", 5, 2, {} };
    oap::GridPlacement p;
    p.instanceId = "org.openauto.clock-0"; p.widgetId = "org.openauto.clock";
    p.col = 1; p.row = 2; p.colSpan = 2; p.rowSpan = 1; p.opacity = 0.5; p.page = 0;
    p.config["style"] = QString("analog");
    home.placements.append(p);
    oap::DashboardConfig work{ "work", "Work", 0, 1, {} };
    cfg.setDashboards({home, work});
    cfg.setActiveDashboardId("work");

    const QString path = QDir::temp().filePath("oap_test_dash_v4.yaml");
    cfg.save(path);
    oap::YamlConfig loaded;
    loaded.load(path);
    auto ds = loaded.dashboards();
    QCOMPARE(ds.size(), 2);
    QCOMPARE(ds[0].nextInstanceId, 5);
    QCOMPARE(ds[0].placements.size(), 1);
    QCOMPARE(ds[0].placements[0].colSpan, 2);
    QCOMPARE(ds[0].placements[0].config.value("style").toString(), QString("analog"));
    QCOMPARE(ds[1].id, QString("work"));
    QCOMPARE(ds[1].pageCount, 1);
    QCOMPARE(loaded.activeDashboardId(), QString("work"));
}

void TestYamlConfig::testV3MigratesToV4()
{
    // Literal v3 file — exactly what a pre-migration install has on disk.
    const QString path = QDir::temp().filePath("oap_test_dash_v3.yaml");
    {
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("widget_grid:\n"
                "  version: 3\n"
                "  next_instance_id: 7\n"
                "  page_count: 3\n"
                "  grid_cols: 8\n"
                "  grid_rows: 4\n"
                "  placements:\n"
                "    - instance_id: org.openauto.clock-4\n"
                "      widget_id: org.openauto.clock\n"
                "      col: 0\n      row: 0\n      col_span: 2\n      row_span: 2\n"
                "      opacity: 0.25\n      page: 1\n");
    }
    oap::YamlConfig cfg;
    cfg.load(path);
    auto ds = cfg.dashboards();
    QCOMPARE(ds.size(), 1);
    QCOMPARE(ds[0].id, QString("home"));
    QCOMPARE(ds[0].nextInstanceId, 7);
    QCOMPARE(ds[0].pageCount, 3);
    QCOMPARE(ds[0].placements.size(), 1);
    QCOMPARE(ds[0].placements[0].page, 1);
    QCOMPARE(cfg.activeDashboardId(), QString("home"));
    QCOMPARE(cfg.gridSavedCols(), 8);            // global key untouched
    // Idempotence: save then reload — still one dashboard, still v4 shape.
    cfg.save(path);
    oap::YamlConfig again; again.load(path);
    QCOMPARE(again.dashboards().size(), 1);
    QCOMPARE(again.dashboards()[0].nextInstanceId, 7);
}

void TestYamlConfig::testV2FlatShapeMigrates()
{
    // Literal v2 (flat, pre-dashboards) file — the migration gate keyed
    // strictly on version==3 skips this shape entirely, dropping the
    // user's placements on load. version==2 is on-disk-plausible (older
    // installs); a missing version key is also possible pre-versioning.
    const QString path = QDir::temp().filePath("oap_test_dash_v2.yaml");
    {
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("widget_grid:\n"
                "  version: 2\n"
                "  next_instance_id: 7\n"
                "  page_count: 3\n"
                "  grid_cols: 8\n"
                "  grid_rows: 4\n"
                "  placements:\n"
                "    - instance_id: org.openauto.clock-4\n"
                "      widget_id: org.openauto.clock\n"
                "      col: 0\n      row: 0\n      col_span: 2\n      row_span: 2\n"
                "      opacity: 0.25\n      page: 1\n");
    }
    oap::YamlConfig cfg;
    cfg.load(path);
    auto ds = cfg.dashboards();
    QCOMPARE(ds.size(), 1);
    QCOMPARE(ds[0].id, QString("home"));
    QCOMPARE(ds[0].nextInstanceId, 7);
    QCOMPARE(ds[0].pageCount, 3);
    QCOMPARE(ds[0].placements.size(), 1);
    QCOMPARE(ds[0].placements[0].page, 1);
    QCOMPARE(cfg.activeDashboardId(), QString("home"));
    QCOMPARE(cfg.gridSavedCols(), 8);            // global key untouched
    // Idempotence: save then reload — still one dashboard, still v4 shape.
    cfg.save(path);
    oap::YamlConfig again; again.load(path);
    QCOMPARE(again.dashboards().size(), 1);
    QCOMPARE(again.dashboards()[0].nextInstanceId, 7);
}

void TestYamlConfig::testNestedMergeRetainsUnknownKeys()
{
    const QString path = QDir::temp().filePath("oap_test_nested_merge.yaml");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("connection:\n"
               "  wifi_ap:\n"
               "    ssid: MergedSSID\n"
               "    experimental_option: retained\n"
               "audio:\n"
               "  adaptive: true\n"
               "custom_section:\n"
               "  enabled: true\n");
    file.close();

    oap::YamlConfig config;
    config.load(path);
    QCOMPARE(config.wifiSsid(), QString("MergedSSID"));
    QCOMPARE(config.wifiPassword(), QString("prodigy"));
    config.setMasterVolume(55);
    QVERIFY(config.save(path));

    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray saved = file.readAll();
    QVERIFY(saved.contains("experimental_option"));
    QVERIFY(saved.contains("custom_section"));
    const YAML::Node emitted = YAML::Load(saved.toStdString());
    QVERIFY(emitted["connection"]["wifi_ap"]["experimental_option"]);
    QVERIFY(!emitted["experimental_option"]);
    QVERIFY(emitted["audio"]["adaptive"]);
    QVERIFY(!emitted["adaptive"]);
    QFile::remove(path);
}

// Task C: atomic save + corrupt-load fallback. A corrupt config on disk must
// not crash the app at startup -- load() degrades to defaults and moves the
// bad file aside for post-mortem inspection instead of throwing.
void TestYamlConfig::testLoadCorruptFileFallsBackToDefaults()
{
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    const QByteArray garbage = "widgets: [unclosed\n\t: {{";
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(garbage);
    }

    oap::YamlConfig config;
    config.load(path);  // must not throw

    // Known default survives (mirrors testLoadDefaults).
    QCOMPARE(config.displayBrightness(), 80);
    QCOMPARE(config.hardwareProfile(), QString("rpi4"));

    // Original path no longer exists; garbage moved to <path>.corrupt.
    QVERIFY(!QFile::exists(path));
    const QString corruptPath = path + ".corrupt";
    QVERIFY(QFile::exists(corruptPath));
    {
        QFile f(corruptPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), garbage);
    }

    QFile::remove(corruptPath);
}

void TestYamlConfig::testLoadMappingShapeMismatchFallsBackToDefaults()
{
    const QString path = QDir::temp().filePath("oap_test_mapping_mismatch.yaml");
    const QList<QByteArray> invalidOverlays{
        "connection: invalid\n",
        "connection:\n  - invalid\n",
    };

    for (const QByteArray& invalidOverlay : invalidOverlays) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(invalidOverlay);
        file.close();

        oap::YamlConfig config;
        config.load(path);

        QCOMPARE(config.hardwareProfile(), QString("rpi4"));
        QCOMPARE(config.wifiSsid(), QString("OpenAutoProdigy"));
        QVERIFY(!QFile::exists(path));
        const QString corruptPath = path + ".corrupt";
        QVERIFY(QFile::exists(corruptPath));
        QFile corruptFile(corruptPath);
        QVERIFY(corruptFile.open(QIODevice::ReadOnly));
        QCOMPARE(corruptFile.readAll(), invalidOverlay);
        QFile::remove(corruptPath);
    }
}

void TestYamlConfig::testLoadCorruptOverwritesPreviousCorruptAside()
{
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.close();

    const QString corruptPath = path + ".corrupt";
    {
        QFile f(corruptPath);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("old junk from a previous crash");
    }

    const QByteArray newGarbage = "widgets: [also-unclosed\n\t: }}";
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(newGarbage);
    }

    oap::YamlConfig config;
    config.load(path);  // must not throw

    QVERIFY(QFile::exists(corruptPath));
    {
        QFile f(corruptPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), newGarbage);
    }

    QFile::remove(corruptPath);
}

void TestYamlConfig::testLoadMissingFileYieldsDefaults()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString path = tmpDir.filePath("nonexistent.yaml");
    QVERIFY(!QFile::exists(path));

    oap::YamlConfig config;
    config.load(path);  // must not throw

    QCOMPARE(config.displayBrightness(), 80);
    QCOMPARE(config.hardwareProfile(), QString("rpi4"));
    QVERIFY(!QFile::exists(path + ".corrupt"));
}

void TestYamlConfig::testSaveAtomicReplacesExisting()
{
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    const QString path = tmpFile.fileName();
    tmpFile.write("hardware_profile: old-stale-content\n");
    tmpFile.close();

    oap::YamlConfig config;
    config.setWifiSsid("AtomicSaveSSID");

    QVERIFY(config.save(path));
    QVERIFY(!QFile::exists(path + ".tmp"));

    oap::YamlConfig loaded;
    loaded.load(path);
    QCOMPARE(loaded.wifiSsid(), QString("AtomicSaveSSID"));

    QFile::remove(path);
}

void TestYamlConfig::testSaveFailureReturnsFalse()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString path = tmpDir.filePath("no-such-dir/config.yaml");

    oap::YamlConfig config;
    QVERIFY(!config.save(path));  // must not throw
    QVERIFY(!QFile::exists(path));
    QVERIFY(!QFile::exists(path + ".tmp"));
}

QTEST_MAIN(TestYamlConfig)
#include "test_yaml_config.moc"
