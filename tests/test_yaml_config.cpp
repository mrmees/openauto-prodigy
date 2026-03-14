#include <QtTest>
#include "core/YamlConfig.hpp"

class TestYamlConfig : public QObject {
    Q_OBJECT
private slots:
    void testLoadDefaults();
    void testLoadFromFile();
    void testSaveAndReload();
    void testPluginScoping();
    void testIdentityDefaults();
    void testIdentityFromFile();
    void testVideoDpi();
    void testSensorsDefaults();
    void testSensorsFromFile();
    void testMicDefaults();
    void testMicFromFile();
    void testValueByPath();
    void testValueByPathNested();
    void testValueByPathMissing();
    void testSetValueByPath();
    void testSetValueByPathRejectsUnknown();
    // testSidebarDefaults removed — sidebar config keys no longer exist
    void testProtocolCaptureDefaults();
    void testProtocolCaptureSetValueByPath();
    void testEqStreamPresetDefaults();
    void testEqStreamPresetSetAndGet();
    void testEqStreamPresetSaveReload();
    void testEqUserPresetsEmpty();
    void testEqUserPresetsSaveReload();
    void testUiScaleDefaults();
    void testUiTokenDefaults();
    void testUiScaleSetAndGet();
    void testUiTokenSetAndGet();
    void testUiFontFloorDefault();
    void testUiNewTokenDefaults();
    void testNavbarShowDuringAADefault();
    void testSidebarDefaultsRemoved();
    void testDisplayScreenSizeDefault();
    void testWidgetGridDefaults();
    void testGridPlacementPageRoundTrip();
    void testGridPageCountRoundTrip();
    void testGridPageCountDefault();
    void testGridSavedDimsDefaults();
    void testGridSavedDimsRoundTrip();
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
    QCOMPARE(config.masterVolume(), 80);
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

void TestYamlConfig::testIdentityDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.headUnitName(), QString("OpenAuto Prodigy"));
    QCOMPARE(config.manufacturer(), QString("OpenAuto Project"));
    QCOMPARE(config.model(), QString("Raspberry Pi 4"));
    QCOMPARE(config.swVersion(), QString("0.3.0"));
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
    QCOMPARE(config.swVersion(), QString("9.9.9"));
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

void TestYamlConfig::testEqStreamPresetDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.eqStreamPreset("media"), QString("Flat"));
    QCOMPARE(config.eqStreamPreset("navigation"), QString("Voice"));
    QCOMPARE(config.eqStreamPreset("phone"), QString("Voice"));
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

void TestYamlConfig::testWidgetGridDefaults()
{
    oap::YamlConfig config;
    // Default grid config should have version 2 and empty placements
    auto placements = config.gridPlacements();
    QVERIFY(placements.isEmpty());
    QCOMPARE(config.gridNextInstanceId(), 0);
}

void TestYamlConfig::testGridPlacementPageRoundTrip()
{
    oap::YamlConfig config;

    QList<oap::GridPlacement> placements;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0";
        p.widgetId = "org.openauto.clock";
        p.col = 0; p.row = 0;
        p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25;
        p.page = 0;
        placements.append(p);
    }
    {
        oap::GridPlacement p;
        p.instanceId = "status-1";
        p.widgetId = "org.openauto.aa-status";
        p.col = 0; p.row = 0;
        p.colSpan = 2; p.rowSpan = 1;
        p.opacity = 0.5;
        p.page = 1;
        placements.append(p);
    }

    config.setGridPlacements(placements);

    QString tmpPath = QDir::tempPath() + "/oap_test_grid_page.yaml";
    config.save(tmpPath);

    oap::YamlConfig loaded;
    loaded.load(tmpPath);
    auto loadedPlacements = loaded.gridPlacements();

    QCOMPARE(loadedPlacements.size(), 2);
    QCOMPARE(loadedPlacements[0].page, 0);
    QCOMPARE(loadedPlacements[1].page, 1);
    QCOMPARE(loadedPlacements[1].instanceId, QString("status-1"));

    QFile::remove(tmpPath);
}

void TestYamlConfig::testGridPageCountRoundTrip()
{
    oap::YamlConfig config;
    config.setGridPageCount(3);

    QString tmpPath = QDir::tempPath() + "/oap_test_grid_pagecount.yaml";
    config.save(tmpPath);

    oap::YamlConfig loaded;
    loaded.load(tmpPath);
    QCOMPARE(loaded.gridPageCount(), 3);

    QFile::remove(tmpPath);
}

void TestYamlConfig::testGridPageCountDefault()
{
    oap::YamlConfig config;
    QCOMPARE(config.gridPageCount(), 2);
}

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

QTEST_MAIN(TestYamlConfig)
#include "test_yaml_config.moc"
