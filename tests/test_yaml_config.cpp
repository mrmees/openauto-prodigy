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
};

void TestYamlConfig::testLoadDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.hardwareProfile(), QString("rpi4"));
    QCOMPARE(config.displayBrightness(), 80);
    QCOMPARE(config.wifiSsid(), QString("OpenAutoProdigy"));
    QCOMPARE(config.tcpPort(), static_cast<uint16_t>(5288));
    QCOMPARE(config.videoFps(), 60);
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

QTEST_MAIN(TestYamlConfig)
#include "test_yaml_config.moc"
