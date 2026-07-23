#include <QTest>
#include <QSignalSpy>
#include <QTimer>
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

class ScriptedBluetoothDiscoveryService
    : public oap::aa::BluetoothDiscoveryService {
public:
    explicit ScriptedBluetoothDiscoveryService(StubConfigService* config)
        : BluetoothDiscoveryService(config)
    {
    }

    QList<quint16> listenerResults;
    QList<bool> sdpResults;
    QList<quint16> registeredPorts;
    int listenerAttempts = 0;

protected:
    quint16 listenForRfcomm() override
    {
        ++listenerAttempts;
        return listenerResults.isEmpty() ? 0 : listenerResults.takeFirst();
    }

    bool registerSdpRecord(uint8_t rfcommChannel) override
    {
        registeredPorts.append(rfcommChannel);
        return sdpResults.isEmpty() ? false : sdpResults.takeFirst();
    }
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

    void testListenerFailureSchedulesRetryBeforeSdp() {
        StubConfigService cfg;
        ScriptedBluetoothDiscoveryService svc(&cfg);
        svc.listenerResults = {0};

        svc.start();

        QCOMPARE(svc.listenerAttempts, 1);
        QVERIFY(svc.registeredPorts.isEmpty());
        auto* listenerRetry = svc.findChild<QTimer*>("rfcommListenerRetryTimer");
        auto* sdpRetry = svc.findChild<QTimer*>("sdpRetryTimer");
        QVERIFY(listenerRetry);
        QVERIFY(sdpRetry);
        QVERIFY(listenerRetry->isActive());
        QVERIFY(!sdpRetry->isActive());
    }

    void testListenerRetrySuccessRegistersSdpOnce() {
        StubConfigService cfg;
        ScriptedBluetoothDiscoveryService svc(&cfg);
        svc.listenerResults = {0, 17};
        svc.sdpResults = {true};
        svc.start();

        auto* listenerRetry = svc.findChild<QTimer*>("rfcommListenerRetryTimer");
        QVERIFY(listenerRetry);
        QVERIFY(listenerRetry->isActive());
        QVERIFY(QMetaObject::invokeMethod(
            &svc, "attemptListenerStart", Qt::DirectConnection));

        QCOMPARE(svc.listenerAttempts, 2);
        QCOMPARE(svc.registeredPorts, QList<quint16>{17});
        QVERIFY(!listenerRetry->isActive());
        QVERIFY(!svc.findChild<QTimer*>("sdpRetryTimer")->isActive());

        // A late listener timeout cannot register SDP a second time.
        QVERIFY(QMetaObject::invokeMethod(
            &svc, "attemptListenerStart", Qt::DirectConnection));
        QCOMPARE(svc.listenerAttempts, 2);
        QCOMPARE(svc.registeredPorts, QList<quint16>{17});
    }

    void testListenerRetryExhaustionEmitsOneTerminalErrorAndRestartsFresh() {
        StubConfigService cfg;
        ScriptedBluetoothDiscoveryService svc(&cfg);
        QSignalSpy errorSpy(&svc, &oap::aa::BluetoothDiscoveryService::error);

        svc.start();
        for (int attempt = 1; attempt < 30; ++attempt) {
            QVERIFY(QMetaObject::invokeMethod(
                &svc, "attemptListenerStart", Qt::DirectConnection));
        }

        QCOMPARE(svc.listenerAttempts, 30);
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(svc.registeredPorts.isEmpty());
        QVERIFY(!svc.findChild<QTimer*>("rfcommListenerRetryTimer")->isActive());

        QVERIFY(QMetaObject::invokeMethod(
            &svc, "attemptListenerStart", Qt::DirectConnection));
        QCOMPARE(svc.listenerAttempts, 30);
        QCOMPARE(errorSpy.count(), 1);

        svc.stop();
        svc.listenerResults = {9};
        svc.sdpResults = {true};
        svc.start();
        QCOMPARE(svc.listenerAttempts, 31);
        QCOMPARE(svc.registeredPorts, QList<quint16>{9});
        QCOMPARE(errorSpy.count(), 1);
    }

    void testStopCancelsListenerAndSdpRetries() {
        StubConfigService cfg;

        ScriptedBluetoothDiscoveryService listenerPending(&cfg);
        listenerPending.start();
        auto* listenerRetry =
            listenerPending.findChild<QTimer*>("rfcommListenerRetryTimer");
        QVERIFY(listenerRetry->isActive());
        listenerPending.stop();
        QVERIFY(!listenerRetry->isActive());
        QVERIFY(QMetaObject::invokeMethod(
            &listenerPending, "attemptListenerStart", Qt::DirectConnection));
        QCOMPARE(listenerPending.listenerAttempts, 1);

        ScriptedBluetoothDiscoveryService sdpPending(&cfg);
        sdpPending.listenerResults = {12};
        sdpPending.sdpResults = {false};
        sdpPending.start();
        auto* sdpRetry = sdpPending.findChild<QTimer*>("sdpRetryTimer");
        QVERIFY(sdpRetry->isActive());
        QCOMPARE(sdpPending.registeredPorts, QList<quint16>{12});
        sdpPending.stop();
        QVERIFY(!sdpRetry->isActive());
        QVERIFY(QMetaObject::invokeMethod(
            &sdpPending, "attemptSdpRegistration", Qt::DirectConnection));
        QCOMPARE(sdpPending.registeredPorts, QList<quint16>{12});
    }
};

QTEST_GUILESS_MAIN(TestBtDiscoveryService)
#include "test_bt_discovery_service.moc"
