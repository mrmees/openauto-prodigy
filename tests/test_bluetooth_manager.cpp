#include <QtTest>
#include <QSignalSpy>
#include "core/services/BluetoothManager.hpp"
#include "core/services/IConfigService.hpp"

// Minimal mock ConfigService for tests
class MockConfigService : public oap::IConfigService {
public:
    QVariant value(const QString& path) const override {
        return values_.value(path);
    }
    void setValue(const QString& path, const QVariant& value) override {
        values_[path] = value;
    }
    void save() override {}
    QVariant pluginValue(const QString&, const QString&) const override { return {}; }
    void setPluginValue(const QString&, const QString&, const QVariant&) override {}

    QMap<QString, QVariant> values_;
};

class TestBluetoothManager : public QObject {
    Q_OBJECT

private slots:
    void testInitialState();
    void testPairableToggle();
    void testPairingConfirmReject();
    void testConnectedDeviceState();
    void testAutoConnectLifecycle();
};

void TestBluetoothManager::testInitialState()
{
    MockConfigService config;
    config.values_["connection.bt_name"] = "TestProdigy";
    oap::BluetoothManager mgr(&config);

    QVERIFY(mgr.adapterAddress().isEmpty());  // No adapter in test env
    QVERIFY(!mgr.isPairable());
    QVERIFY(!mgr.isPairingActive());
    QVERIFY(mgr.connectedDeviceName().isEmpty());
}

void TestBluetoothManager::testPairableToggle()
{
    MockConfigService config;
    oap::BluetoothManager mgr(&config);

    QSignalSpy spy(&mgr, &oap::BluetoothManager::pairableChanged);
    QVERIFY(!mgr.isPairable());

    mgr.setPairable(true);
    QVERIFY(mgr.isPairable());
    QCOMPARE(spy.count(), 1);

    // Setting same value doesn't emit
    mgr.setPairable(true);
    QCOMPARE(spy.count(), 1);

    mgr.setPairable(false);
    QVERIFY(!mgr.isPairable());
    QCOMPARE(spy.count(), 2);
}

void TestBluetoothManager::testPairingConfirmReject()
{
    MockConfigService config;
    oap::BluetoothManager mgr(&config);

    // These should not crash when no pending request
    mgr.confirmPairing();
    mgr.rejectPairing();
    QVERIFY(!mgr.isPairingActive());
}

void TestBluetoothManager::testConnectedDeviceState()
{
    MockConfigService config;
    oap::BluetoothManager mgr(&config);

    QVERIFY(mgr.connectedDeviceName().isEmpty());
    QVERIFY(mgr.connectedDeviceAddress().isEmpty());
}

void TestBluetoothManager::testAutoConnectLifecycle()
{
    MockConfigService config;
    oap::BluetoothManager mgr(&config);

    // startAutoConnect() with no adapter should be a no-op (no crash)
    mgr.startAutoConnect();

    // cancelAutoConnect() should be safe to call anytime
    mgr.cancelAutoConnect();

    // Double cancel should also be safe
    mgr.cancelAutoConnect();

    // startAutoConnect after cancel should also be safe
    mgr.startAutoConnect();

    // With auto-connect disabled in config, should return early
    config.values_["connection.auto_connect_aa"] = false;
    mgr.startAutoConnect();
}

QTEST_MAIN(TestBluetoothManager)
#include "test_bluetooth_manager.moc"
