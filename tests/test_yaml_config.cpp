#include <QtTest>
#include "core/YamlConfig.hpp"

class TestYamlConfig : public QObject {
    Q_OBJECT
private slots:
    void testLoadDefaults();
    void testLoadFromFile();
    void testSaveAndReload();
    void testPluginScoping();
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

QTEST_MAIN(TestYamlConfig)
#include "test_yaml_config.moc"
