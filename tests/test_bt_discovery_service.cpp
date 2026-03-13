#include <QTest>
#include "core/aa/BluetoothDiscoveryService.hpp"
#include "core/services/IConfigService.hpp"

// Stub IConfigService for testing
class StubConfigService : public oap::IConfigService {
public:
    QVariantMap values;

    QVariant value(const QString& key) const override {
        return values.value(key);
    }
    void setValue(const QString& key, const QVariant& v) override {
        values[key] = v;
    }
    QVariant pluginValue(const QString&, const QString&) const override { return {}; }
    void setPluginValue(const QString&, const QString&, const QVariant&) override {}
    void save() override {}
};

class TestBtDiscoveryService : public QObject {
    Q_OBJECT
private slots:
    void testBuildWifiStartRequest() {
        // buildWifiStartRequest should produce a protobuf with the given IP and port
        auto msg = oap::aa::BluetoothDiscoveryService::buildWifiStartRequest("10.0.0.1", 5277);
        QCOMPARE(msg.ip_address(), std::string("10.0.0.1"));
        QCOMPARE(msg.port(), 5277u);
    }

    void testBuildWifiStartRequestCustomPort() {
        auto msg = oap::aa::BluetoothDiscoveryService::buildWifiStartRequest("192.168.1.152", 9999);
        QCOMPARE(msg.ip_address(), std::string("192.168.1.152"));
        QCOMPARE(msg.port(), 9999u);
    }

    void testBuildWifiCredentialResponse() {
        auto msg = oap::aa::BluetoothDiscoveryService::buildWifiCredentialResponse(
            "OpenAutoProdigy", "prodigy", "AA:BB:CC:DD:EE:FF");
        QCOMPARE(msg.ssid(), std::string("OpenAutoProdigy"));
        QCOMPARE(msg.key(), std::string("prodigy"));
        QCOMPARE(msg.bssid(), std::string("AA:BB:CC:DD:EE:FF"));
        QCOMPARE(msg.security_mode(),
                 oaa::proto::messages::WifiSecurityResponse_SecurityMode_WPA2_PERSONAL);
        QCOMPARE(msg.access_point_type(),
                 oaa::proto::messages::WifiSecurityResponse_AccessPointType_DYNAMIC);
    }

    void testBuildWifiCredentialResponseCustomValues() {
        auto msg = oap::aa::BluetoothDiscoveryService::buildWifiCredentialResponse(
            "MyNetwork", "s3cretP4ss", "11:22:33:44:55:66");
        QCOMPARE(msg.ssid(), std::string("MyNetwork"));
        QCOMPARE(msg.key(), std::string("s3cretP4ss"));
        QCOMPARE(msg.bssid(), std::string("11:22:33:44:55:66"));
    }

    void testConstructionWithConfigService() {
        // Verify BluetoothDiscoveryService can be constructed with IConfigService*
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 5277;
        cfg.values["connection.wifi_ap.ssid"] = "TestSSID";
        cfg.values["connection.wifi_ap.password"] = "TestPass";

        // This should compile and not crash — we can't start() without real BT hardware
        oap::aa::BluetoothDiscoveryService svc(&cfg, "wlan0");
        Q_UNUSED(svc);
    }
};

QTEST_GUILESS_MAIN(TestBtDiscoveryService)
#include "test_bt_discovery_service.moc"
