#include <QtTest>
#include "core/YamlConfig.hpp"
#include "core/services/ConfigService.hpp"

/// Verifies that every config key used in the codebase
/// is readable through ConfigService with a valid default value.
class TestConfigKeyCoverage : public QObject {
    Q_OBJECT
private slots:
    void testAllInstallerKeys();
    void testAllRuntimeKeys();
    void testPluginConsumedKeys();
};

void TestConfigKeyCoverage::testAllInstallerKeys()
{
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_coverage.yaml");

    QVERIFY(svc.value("connection.wifi_ap.interface").isValid());
    QVERIFY(svc.value("connection.wifi_ap.ssid").isValid());
    QVERIFY(svc.value("connection.wifi_ap.password").isValid());
    QVERIFY(svc.value("connection.tcp_port").isValid());
    QVERIFY(svc.value("video.fps").isValid());
    QVERIFY(svc.value("video.resolution").isValid());
    QVERIFY(svc.value("display.brightness").isValid());
}

void TestConfigKeyCoverage::testAllRuntimeKeys()
{
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_coverage.yaml");

    static const QStringList keys = {
        "hardware_profile",
        "display.brightness",
        "display.theme",
        "display.orientation",
        "display.width",
        "display.height",
        "connection.auto_connect_aa",
        "connection.bt_discoverable",
        "connection.wifi_ap.ssid",
        "connection.wifi_ap.password",
        "connection.wifi_ap.channel",
        "connection.wifi_ap.band",
        "connection.tcp_port",
        "audio.master_volume",
        "audio.output_device",
        "audio.buffer_ms.media",
        "audio.buffer_ms.speech",
        "audio.buffer_ms.system",
        "audio.microphone.device",
        "audio.microphone.gain",
        "video.fps",
        "video.resolution",
        "video.dpi",
        "identity.head_unit_name",
        "identity.manufacturer",
        "identity.model",
        "identity.sw_version",
        "identity.car_model",
        "identity.car_year",
        "identity.left_hand_drive",
        "sensors.night_mode.source",
        "sensors.night_mode.day_start",
        "sensors.night_mode.night_start",
        "sensors.night_mode.gpio_pin",
        "sensors.night_mode.gpio_active_high",
        "sensors.gps.enabled",
        "sensors.gps.source",
        "video.sidebar.enabled",
        "video.sidebar.width",
        "video.sidebar.position",
        "nav_strip.show_labels",
        "touch.device",
    };

    for (const auto& key : keys) {
        QVERIFY2(svc.value(key).isValid(),
                 qPrintable(QString("Key '%1' returned invalid QVariant").arg(key)));
    }
}

void TestConfigKeyCoverage::testPluginConsumedKeys()
{
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_coverage.yaml");

    QVERIFY(svc.value("touch.device").isValid());
    QVERIFY(svc.value("display.width").isValid());
    QVERIFY(svc.value("display.height").isValid());

    QCOMPARE(svc.value("display.width").toInt(), 1024);
    QCOMPARE(svc.value("display.height").toInt(), 600);
    QCOMPARE(svc.value("touch.device").toString(), QString(""));
}

QTEST_MAIN(TestConfigKeyCoverage)
#include "test_config_key_coverage.moc"
