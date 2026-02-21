#include <QtTest>
#include <QDir>
#include "core/YamlConfig.hpp"
#include "core/services/ConfigService.hpp"

class TestConfigService : public QObject {
    Q_OBJECT
private slots:
    void testReadTopLevelValues();
    void testWriteTopLevelValues();
    void testPluginScopedConfig();
    void testPluginIsolation();
    void testSaveAndReload();
    void testPreviouslyUnmappedKeys();
};

void TestConfigService::testReadTopLevelValues()
{
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_cs.yaml");

    QCOMPARE(svc.value("display.brightness").toInt(), 80);
    QCOMPARE(svc.value("audio.master_volume").toInt(), 80);
    QCOMPARE(svc.value("connection.wifi_ap.ssid").toString(), QString("OpenAutoProdigy"));
    QCOMPARE(svc.value("video.fps").toInt(), 60);
    QCOMPARE(svc.value("hardware_profile").toString(), QString("rpi4"));
    // Unknown key returns invalid
    QVERIFY(!svc.value("nonexistent.key").isValid());
}

void TestConfigService::testWriteTopLevelValues()
{
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_cs.yaml");

    svc.setValue("display.brightness", 50);
    QCOMPARE(svc.value("display.brightness").toInt(), 50);

    svc.setValue("audio.master_volume", 30);
    QCOMPARE(svc.value("audio.master_volume").toInt(), 30);
}

void TestConfigService::testPluginScopedConfig()
{
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_cs.yaml");

    svc.setPluginValue("org.openauto.android-auto", "auto_connect", true);
    svc.setPluginValue("org.openauto.android-auto", "video_fps", 60);

    QCOMPARE(svc.pluginValue("org.openauto.android-auto", "auto_connect").toBool(), true);
    QCOMPARE(svc.pluginValue("org.openauto.android-auto", "video_fps").toInt(), 60);
}

void TestConfigService::testPluginIsolation()
{
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_cs.yaml");

    svc.setPluginValue("org.openauto.android-auto", "some_key", 42);
    // Different plugin can't see it
    QVERIFY(!svc.pluginValue("org.openauto.bt-audio", "some_key").isValid());
}

void TestConfigService::testSaveAndReload()
{
    QString path = QDir::tempPath() + "/oap_test_config_svc.yaml";

    {
        oap::YamlConfig yaml;
        oap::ConfigService svc(&yaml, path);
        svc.setValue("audio.master_volume", 42);
        svc.setPluginValue("org.test", "foo", QString("bar"));
        svc.save();
    }

    {
        oap::YamlConfig yaml;
        yaml.load(path);
        oap::ConfigService svc(&yaml, path);
        QCOMPARE(svc.value("audio.master_volume").toInt(), 42);
        QCOMPARE(svc.pluginValue("org.test", "foo").toString(), QString("bar"));
    }

    QFile::remove(path);
}

void TestConfigService::testPreviouslyUnmappedKeys()
{
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_cs.yaml");

    // These keys were previously silently ignored by ConfigService
    QCOMPARE(svc.value("display.brightness").toInt(), 80);
    QCOMPARE(svc.value("connection.wifi_ap.channel").toInt(), 36);
    QCOMPARE(svc.value("sensors.night_mode.source").toString(), QString("time"));
    QCOMPARE(svc.value("identity.head_unit_name").toString(), QString("OpenAuto Prodigy"));
    QCOMPARE(svc.value("video.resolution").toString(), QString("720p"));
    QCOMPARE(svc.value("video.dpi").toInt(), 140);
}

QTEST_MAIN(TestConfigService)
#include "test_config_service.moc"
