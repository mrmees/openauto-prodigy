#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "core/YamlConfig.hpp"
#include "core/system/HostapdConfig.hpp"

class TestHostapdConfig : public QObject {
    Q_OBJECT

private slots:
    void parseWifiCredentialsExtractsSsidAndPassphrase()
    {
        const QByteArray config =
            "interface=wlan0\n"
            "driver=nl80211\n"
            "ssid=Prodigy_ab12\n"
            "channel=36\n"
            "wpa=2\n"
            "wpa_passphrase=s3cretPass123\n";

        auto creds = oap::parseHostapdWifiCredentials(config);
        QVERIFY(creds.has_value());
        QCOMPARE(creds->ssid, QString("Prodigy_ab12"));
        QCOMPARE(creds->passphrase, QString("s3cretPass123"));
    }

    void syncWifiCredentialsUpdatesBothSsidAndPassword()
    {
        oap::YamlConfig config;
        config.setWifiSsid("OpenAutoProdigy");
        config.setWifiPassword("prodigy");

        const QByteArray hostapdConfig =
            "ssid=Prodigy_cd34\n"
            "wpa_passphrase=CorrectHorseBatteryStaple\n";

        const bool changed = oap::syncWifiCredentialsFromHostapd(config, hostapdConfig);

        QVERIFY(changed);
        QCOMPARE(config.wifiSsid(), QString("Prodigy_cd34"));
        QCOMPARE(config.wifiPassword(), QString("CorrectHorseBatteryStaple"));
    }

    void loadWifiCredentialsReadsHostapdFile()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString hostapdPath = tempDir.path() + "/hostapd.conf";
        QFile hostapdFile(hostapdPath);
        QVERIFY(hostapdFile.open(QIODevice::WriteOnly | QIODevice::Text));
        hostapdFile.write(
            "interface=wlan0\n"
            "ssid=Prodigy_file99\n"
            "wpa_passphrase=FilePassword456\n");
        hostapdFile.close();

        auto creds = oap::loadHostapdWifiCredentials(hostapdPath);

        QVERIFY(creds.has_value());
        QCOMPARE(creds->ssid, QString("Prodigy_file99"));
        QCOMPARE(creds->passphrase, QString("FilePassword456"));
    }

    void syncWifiCredentialsIgnoresIncompleteHostapdData()
    {
        oap::YamlConfig config;
        config.setWifiSsid("Prodigy_live");
        config.setWifiPassword("CorrectPassword123");

        const QByteArray hostapdConfig = "ssid=Prodigy_partial\n";

        const bool changed = oap::syncWifiCredentialsFromHostapd(config, hostapdConfig);

        QVERIFY(!changed);
        QCOMPARE(config.wifiSsid(), QString("Prodigy_live"));
        QCOMPARE(config.wifiPassword(), QString("CorrectPassword123"));
    }
};

QTEST_GUILESS_MAIN(TestHostapdConfig)

#include "test_hostapd_config.moc"
